/*	$NetBSD: sh3_machdep.c,v 1.39 2002/04/29 09:33:30 uch Exp $	*/

/*-
 * Copyright (c) 1996, 1997, 1998, 2002 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Charles M. Hannum and by Jason R. Thorpe of the Numerical Aerospace
 * Simulation Facility, NASA Ames Research Center.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the NetBSD
 *	Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*-
 * Copyright (c) 1982, 1987, 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * William Jolitz.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)machdep.c	7.4 (Berkeley) 6/3/91
 */

#include "opt_kgdb.h"
#include "opt_memsize.h"
#include "opt_compat_netbsd.h"
#include "opt_kstack_debug.h"

#include <sys/param.h>
#include <sys/systm.h>

#include <sys/buf.h>
#include <sys/exec.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/mount.h>
#include <sys/proc.h>
#include <sys/signalvar.h>
#include <sys/syscallargs.h>
#include <sys/user.h>

#ifdef KGDB
#include <sys/kgdb.h>
#ifndef KGDB_DEVNAME
#define	KGDB_DEVNAME "nodev"
#endif
const char kgdb_devname[] = KGDB_DEVNAME;
#endif /* KGDB */

#include <uvm/uvm_extern.h>

#include <sh3/cache.h>
#include <sh3/clock.h>
#include <sh3/exception.h>
#include <sh3/locore.h>
#include <sh3/mmu.h>
#include <sh3/intr.h>

/* Our exported CPU info; we can have only one. */
struct cpu_info cpu_info_store;
int cpu_arch;
int cpu_product;
char cpu_model[120];

struct vm_map *exec_map;
struct vm_map *mb_map;
struct vm_map *phys_map;

int physmem;
struct user *proc0paddr;	/* init_main.c use this. */
struct pcb *curpcb;
struct md_upte *curupte;	/* SH3 wired u-area hack */

#ifndef IOM_RAM_BEGIN
#error "define IOM_RAM_BEGIN"
#endif
#define	VBR	(u_int8_t *)IOM_RAM_BEGIN
vaddr_t ram_start = IOM_RAM_BEGIN;
/* exception handler holder (sh3/sh3/exception_vector.S) */
extern char sh_vector_generic[], sh_vector_generic_end[];
extern char sh_vector_interrupt[], sh_vector_interrupt_end[];
#ifdef SH3
extern char sh3_vector_tlbmiss[], sh3_vector_tlbmiss_end[];
#endif
#ifdef SH4
extern char sh4_vector_tlbmiss[], sh4_vector_tlbmiss_end[];
#endif
/*
 * These variables are needed by /sbin/savecore
 */
u_int32_t dumpmag = 0x8fca0101;	/* magic number */
int dumpsize;			/* pages */
long dumplo;	 		/* blocks */

void
sh_cpu_init(int arch, int product)
{
	/* CPU type */
	cpu_arch = arch;
	cpu_product = product;

#if defined(SH3) && defined(SH4)
	/* Set register addresses */
	sh_devreg_init();
#endif
	/* Cache access ops. */
	sh_cache_init();

	/* MMU access ops. */
	sh_mmu_init();

	/* Hardclock, RTC initialize. */
	machine_clock_init();

	/* ICU initiailze. */
	intc_init();

	/* Exception vector. */
	memcpy(VBR + 0x100, sh_vector_generic,
	    sh_vector_generic_end - sh_vector_generic);
#ifdef SH3
	if (CPU_IS_SH3)
		memcpy(VBR + 0x400, sh3_vector_tlbmiss,
		    sh3_vector_tlbmiss_end - sh3_vector_tlbmiss);
#endif
#ifdef SH4
	if (CPU_IS_SH4)
		memcpy(VBR + 0x400, sh4_vector_tlbmiss,
		    sh4_vector_tlbmiss_end - sh4_vector_tlbmiss);
#endif
	memcpy(VBR + 0x600, sh_vector_interrupt,
	    sh_vector_interrupt_end - sh_vector_interrupt);

	sh_icache_sync_all();	/* for I/D separated cache */

	__asm__ __volatile__ ("ldc	%0, vbr" :: "r"(VBR));

	/* kernel stack setup */
	__sh_switch_resume = CPU_IS_SH3 ? sh3_switch_resume : sh4_switch_resume;
}

/*
 * vsize_t sh_proc0_init(vaddr_t kernend, paddr_t pstart, paddr_t pend)
 *
 *	kernend ... P1 address.
 *	pstart  ... physical address of RAM start address.
 *	pend    ... physical address of the last RAM address
 *
 *      Returns size of stealed memory.
 *
 *	Memory map
 *	....|  proc0 stack  | Page Dir |    Page Table    |
 *	    *    USPACE        NBPG     (1+nkpde)*NBPG
 *       kernend
 */
vsize_t
sh_proc0_init(vaddr_t kernend, paddr_t pstart, paddr_t pend)
{
	pd_entry_t *pagedir, *pagetab, pte;
	vsize_t sz;
	vaddr_t p0;
	int i;

	/* Set default page size (4KB) */
	uvm_setpagesize();

	/* # of pdes maps whole physical memory area. */
	nkpde = sh3_btod(((pend - pstart + 1) + PDOFSET) & ~PDOFSET);

	/* Steal page dir area, process0 stack, page table area */
	sz = USPACE + NBPG + (1 + nkpde) * NBPG;
	p0 = round_page(kernend);
	memset((void *)p0, 0, sz);

	/* Build initial page tables */
	pagedir = (pt_entry_t *)(p0 + USPACE);
	pagetab = (pt_entry_t *)(p0 + USPACE + NBPG);

	/* Construct a page table directory */
	pte = (pt_entry_t)pagetab;
	pte |= PG_KW | PG_V | PG_4K | PG_M | PG_N;
	pagedir[(SH3_PHYS_TO_P1SEG(pstart)) >> PDSHIFT] = pte;

	/* Map whole physical memory space from VM_MIN_KERNEL_ADDRESS */
	pte += NBPG;
	for (i = 0; i < nkpde; i++, pte += NBPG)
		pagedir[(VM_MIN_KERNEL_ADDRESS >> PDSHIFT) + i] = pte;

	/* Install a PDE recursively mapping page directory as a page table. */
	pte = (pt_entry_t)pagedir;
	pte |= PG_V | PG_4K | PG_KW | PG_M | PG_N;
	pagedir[PDSLOT_PTE] = pte;	/* 0xcfc00000 */

	/* Set page directory base */
	SH_MMU_TTB_WRITE((u_int32_t)pagedir);

	/* Setup proc0 */
	proc0paddr = (struct user *)p0;
	proc0.p_addr = proc0paddr;
	curpcb = &proc0.p_addr->u_pcb;
	curpcb->pageDirReg = (pt_entry_t)pagedir;
	/*
	 * u-area map:
	 * |user| .... | ............... |
	 * |      NBPG +  USPACE - NBPG  +
         *           pcb_fp(P1)        pcb_sp
	 * pcb_fp and pcb_sp are stored into r6_bank, r7_bank
	 * when context switching.
	 */

	curpcb->pcb_sp = p0 + USPACE;
	curpcb->pcb_fp = p0 + NBPG;
	curpcb->pcb_sf.sf_r6_bank = curpcb->pcb_fp;
	curpcb->pcb_sf.sf_r15 = curpcb->pcb_sp;
	__asm__ __volatile__("ldc %0, r6_bank" :: "r"(curpcb->pcb_fp));
	__asm__ __volatile__("ldc %0, r7_bank" :: "r"(curpcb->pcb_sp));
	curupte = proc0.p_md.md_upte;

	/* trap frame */
	proc0.p_md.md_regs = (struct trapframe *)curpcb->pcb_fp - 1;
#ifdef KSTACK_DEBUG
	memset((char *)(p0 + sizeof(struct user)), 0x5a,
	    NBPG - sizeof(struct user));
	memset((char *)(p0 + NBPG), 0xa5, USPACE - NBPG);
#endif

	/* Enable MMU */
	sh_mmu_start();

	/* Mask all interrupt */
	_cpu_intr_suspend();

	/* Enable exception for P3 access */
	_cpu_exception_resume(0);

	return (p0 + sz - kernend);
}

void
sh_startup()
{
	caddr_t v;
	int i, sz, base, residual;
	vaddr_t minaddr, maxaddr;
	vsize_t size;
	char pbuf[9];

	printf(version);
	if (*cpu_model != '\0')
		printf("%s", cpu_model);
#ifdef DEBUG
	printf("general exception handler:\t%d byte\n",
	       sh_vector_generic_end - sh_vector_generic);
	printf("TLB miss exception handler:\t%d byte\n",
#if defined(SH3) && defined(SH4)
	    CPU_IS_SH3 ? sh3_vector_tlbmiss_end - sh3_vector_tlbmiss :
	    sh4_vector_tlbmiss_end - sh4_vector_tlbmiss
#elif defined(SH3)
	    sh3_vector_tlbmiss_end - sh3_vector_tlbmiss
#elif defined(SH4)
	    sh4_vector_tlbmiss_end - sh4_vector_tlbmiss
#endif
	    );
	printf("interrupt exception handler:\t%d byte\n",
	       sh_vector_interrupt_end - sh_vector_interrupt);
#endif /* DEBUG */

	format_bytes(pbuf, sizeof(pbuf), ctob(physmem));
	printf("total memory = %s\n", pbuf);

	/*
	 * Find out how much space we need, allocate it,
	 * and then give everything true virtual addresses.
	 */
	sz = (int)allocsys(NULL, NULL);
	if ((v = (caddr_t)uvm_km_zalloc(kernel_map, round_page(sz))) == 0)
		panic("startup: no room for tables");
	if (allocsys(v, NULL) - v != sz)
		panic("startup: table size inconsistency");

	/*
	 * Now allocate buffers proper.  They are different than the above
	 * in that they usually occupy more virtual memory than physical.
	 */
	size = MAXBSIZE * nbuf;
	buffers = 0;
	if (uvm_map(kernel_map, (vaddr_t *) &buffers, round_page(size),
		    NULL, UVM_UNKNOWN_OFFSET, 0,
		    UVM_MAPFLAG(UVM_PROT_NONE, UVM_PROT_NONE, UVM_INH_NONE,
				UVM_ADV_NORMAL, 0)) != 0)
		panic("sh3_startup: cannot allocate VM for buffers");
	minaddr = (vaddr_t)buffers;
	if ((bufpages / nbuf) >= btoc(MAXBSIZE)) {
		/* don't want to alloc more physical mem than needed */
		bufpages = btoc(MAXBSIZE) * nbuf;
	}

	base = bufpages / nbuf;
	residual = bufpages % nbuf;
	for (i = 0; i < nbuf; i++) {
		vsize_t curbufsize;
		vaddr_t curbuf;
		struct vm_page *pg;

		/*
		 * Each buffer has MAXBSIZE bytes of VM space allocated.  Of
		 * that MAXBSIZE space, we allocate and map (base+1) pages
		 * for the first "residual" buffers, and then we allocate
		 * "base" pages for the rest.
		 */
		curbuf = (vaddr_t) buffers + (i * MAXBSIZE);
		curbufsize = NBPG * ((i < residual) ? (base+1) : base);

		while (curbufsize) {
			pg = uvm_pagealloc(NULL, 0, NULL, 0);
			if (pg == NULL)
				panic("sh3_startup: not enough memory for "
				    "buffer cache");
			pmap_kenter_pa(curbuf, VM_PAGE_TO_PHYS(pg),
					VM_PROT_READ|VM_PROT_WRITE);
			curbuf += PAGE_SIZE;
			curbufsize -= PAGE_SIZE;
		}
	}
	pmap_update(pmap_kernel());

	/*
	 * Allocate a submap for exec arguments.  This map effectively
	 * limits the number of processes exec'ing at any time.
	 */
	exec_map = uvm_km_suballoc(kernel_map, &minaddr, &maxaddr,
				   16*NCARGS, VM_MAP_PAGEABLE, FALSE, NULL);

	/*
	 * Allocate a submap for physio
	 */
	phys_map = uvm_km_suballoc(kernel_map, &minaddr, &maxaddr,
				   VM_PHYS_SIZE, 0, FALSE, NULL);

	format_bytes(pbuf, sizeof(pbuf), ptoa(uvmexp.free));
	printf("avail memory = %s\n", pbuf);
	format_bytes(pbuf, sizeof(pbuf), bufpages * NBPG);
	printf("using %d buffers containing %s of memory\n", nbuf, pbuf);

	/*
	 * Set up buffers, so they can be used to read disk labels.
	 */
	bufinit();

}

/*
 * This is called by main to set dumplo and dumpsize.
 * Dumps always skip the first CLBYTES of disk space
 * in case there might be a disk label stored there.
 * If there is extra space, put dump at the end to
 * reduce the chance that swapping trashes it.
 */
void
cpu_dumpconf()
{
}

/*
 * Doadump comes here after turning off memory management and
 * getting on the dump stack, either when called above, or by
 * the auto-restart code.
 */
#define	BYTES_PER_DUMP  NBPG	/* must be a multiple of pagesize XXX small */
static vaddr_t dumpspace;

vaddr_t
reserve_dumppages(p)
	vaddr_t p;
{

	dumpspace = p;
	return (p + BYTES_PER_DUMP);
}

void
dumpsys()
{
}

/*
 * Send an interrupt to process.
 *
 * Stack is set up to allow sigcode stored
 * in u. to call routine, followed by kcall
 * to sigreturn routine below.  After sigreturn
 * resets the signal mask, the stack, and the
 * frame pointer, it returns to the user
 * specified pc, psl.
 */
void
sendsig(sig_t catcher, int sig, sigset_t *mask, u_long code)
{
	struct proc *p = curproc;
	struct trapframe *tf;
	struct sigframe *fp, frame;
	int onstack;

	tf = p->p_md.md_regs;

	/* Do we need to jump onto the signal stack? */
	onstack =
	    (p->p_sigctx.ps_sigstk.ss_flags & (SS_DISABLE | SS_ONSTACK)) == 0 &&
	    (SIGACTION(p, sig).sa_flags & SA_ONSTACK) != 0;

	/* Allocate space for the signal handler context. */
	if (onstack)
		fp = (struct sigframe *)((caddr_t)p->p_sigctx.ps_sigstk.ss_sp +
						p->p_sigctx.ps_sigstk.ss_size);
	else
		fp = (struct sigframe *)tf->tf_r15;
	fp--;

	/* Build stack frame for signal trampoline. */
	frame.sf_signum = sig;
	frame.sf_code = code;
	frame.sf_scp = &fp->sf_sc;
	frame.sf_handler = catcher;

	/* Save register context. */
	frame.sf_sc.sc_ssr = tf->tf_ssr;
	frame.sf_sc.sc_spc = tf->tf_spc;
	frame.sf_sc.sc_pr = tf->tf_pr;
	frame.sf_sc.sc_r15 = tf->tf_r15;
	frame.sf_sc.sc_r14 = tf->tf_r14;
	frame.sf_sc.sc_r13 = tf->tf_r13;
	frame.sf_sc.sc_r12 = tf->tf_r12;
	frame.sf_sc.sc_r11 = tf->tf_r11;
	frame.sf_sc.sc_r10 = tf->tf_r10;
	frame.sf_sc.sc_r9 = tf->tf_r9;
	frame.sf_sc.sc_r8 = tf->tf_r8;
	frame.sf_sc.sc_r7 = tf->tf_r7;
	frame.sf_sc.sc_r6 = tf->tf_r6;
	frame.sf_sc.sc_r5 = tf->tf_r5;
	frame.sf_sc.sc_r4 = tf->tf_r4;
	frame.sf_sc.sc_r3 = tf->tf_r3;
	frame.sf_sc.sc_r2 = tf->tf_r2;
	frame.sf_sc.sc_r1 = tf->tf_r1;
	frame.sf_sc.sc_r0 = tf->tf_r0;
	frame.sf_sc.sc_trapno = tf->tf_trapno;

	/* Save signal stack. */
	frame.sf_sc.sc_onstack = p->p_sigctx.ps_sigstk.ss_flags & SS_ONSTACK;

	/* Save signal mask. */
	frame.sf_sc.sc_mask = *mask;

#ifdef COMPAT_13
	/*
	 * XXX We always have to save an old style signal mask because
	 * XXX we might be delivering a signal to a process which will
	 * XXX escape from the signal in a non-standard way and invoke
	 * XXX sigreturn() directly.
	 */
	native_sigset_to_sigset13(mask, &frame.sf_sc.__sc_mask13);
#endif

	if (copyout(&frame, fp, sizeof(frame)) != 0) {
		/*
		 * Process has trashed its stack; give it an illegal
		 * instruction to halt it in its tracks.
		 */
		sigexit(p, SIGILL);
		/* NOTREACHED */
	}

	/*
	 * Build context to run handler in.
	 */
	tf->tf_spc = (int)p->p_sigctx.ps_sigcode;
	tf->tf_r15 = (int)fp;

	/* Remember that we're now on the signal stack. */
	if (onstack)
		p->p_sigctx.ps_sigstk.ss_flags |= SS_ONSTACK;
}

/*
 * System call to cleanup state after a signal
 * has been taken.  Reset signal mask and
 * stack state from context left by sendsig (above).
 * Return to previous pc and psl as specified by
 * context left by sendsig. Check carefully to
 * make sure that the user has not modified the
 * psl to gain improper privileges or to cause
 * a machine fault.
 */
int
sys___sigreturn14(struct proc *p, void *v, register_t *retval)
{
	struct sys___sigreturn14_args /* {
		syscallarg(struct sigcontext *) sigcntxp;
	} */ *uap = v;
	struct sigcontext *scp, context;
	struct trapframe *tf;

	/*
	 * The trampoline code hands us the context.
	 * It is unsafe to keep track of it ourselves, in the event that a
	 * program jumps out of a signal handler.
	 */
	scp = SCARG(uap, sigcntxp);
	if (copyin((caddr_t)scp, &context, sizeof(*scp)) != 0)
		return (EFAULT);

	/* Restore signal context. */
	tf = p->p_md.md_regs;

	/* Check for security violations. */
	if (((context.sc_ssr ^ tf->tf_ssr) & PSL_USERSTATIC) != 0)
		return (EINVAL);

	tf->tf_ssr = context.sc_ssr;

	tf->tf_r0 = context.sc_r0;
	tf->tf_r1 = context.sc_r1;
	tf->tf_r2 = context.sc_r2;
	tf->tf_r3 = context.sc_r3;
	tf->tf_r4 = context.sc_r4;
	tf->tf_r5 = context.sc_r5;
	tf->tf_r6 = context.sc_r6;
	tf->tf_r7 = context.sc_r7;
	tf->tf_r8 = context.sc_r8;
	tf->tf_r9 = context.sc_r9;
	tf->tf_r10 = context.sc_r10;
	tf->tf_r11 = context.sc_r11;
	tf->tf_r12 = context.sc_r12;
	tf->tf_r13 = context.sc_r13;
	tf->tf_r14 = context.sc_r14;
	tf->tf_spc = context.sc_spc;
	tf->tf_r15 = context.sc_r15;
	tf->tf_pr = context.sc_pr;

	/* Restore signal stack. */
	if (context.sc_onstack & SS_ONSTACK)
		p->p_sigctx.ps_sigstk.ss_flags |= SS_ONSTACK;
	else
		p->p_sigctx.ps_sigstk.ss_flags &= ~SS_ONSTACK;
	/* Restore signal mask. */
	(void) sigprocmask1(p, SIG_SETMASK, &context.sc_mask, 0);

	return (EJUSTRETURN);
}

/*
 * Clear registers on exec
 */
void
setregs(struct proc *p, struct exec_package *pack, u_long stack)
{
	register struct pcb *pcb = &p->p_addr->u_pcb;
	register struct trapframe *tf;

	p->p_md.md_flags &= ~MDP_USEDFPU;
	pcb->pcb_flags = 0;

	tf = p->p_md.md_regs;

	tf->tf_r0 = 0;
	tf->tf_r1 = 0;
	tf->tf_r2 = 0;
	tf->tf_r3 = 0;
	tf->tf_r4 = *(int *)stack;	/* argc */
	tf->tf_r5 = stack+4;		/* argv */
	tf->tf_r6 = stack+4*tf->tf_r4 + 8; /* envp */
	tf->tf_r7 = 0;
	tf->tf_r8 = 0;
	tf->tf_r9 = (int)p->p_psstr;
	tf->tf_r10 = 0;
	tf->tf_r11 = 0;
	tf->tf_r12 = 0;
	tf->tf_r13 = 0;
	tf->tf_r14 = 0;
	tf->tf_spc = pack->ep_entry;
	tf->tf_ssr = PSL_USERSET;
	tf->tf_r15 = stack;
}

/*
 * Jump to reset vector.
 */
void
cpu_reset()
{

	_cpu_exception_suspend();
	_reg_write_4(SH_(EXPEVT), 0x020);	/* manual reset */

	goto *(u_int32_t *)0xa0000000;
	/* NOTREACHED */
	while (1)
		;
}
