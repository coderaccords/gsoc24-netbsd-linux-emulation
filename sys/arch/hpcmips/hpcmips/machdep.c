/*	$NetBSD: machdep.c,v 1.57 2001/07/03 02:37:06 enami Exp $	*/

/*-
 * Copyright (c) 1999 Shin Takemura, All rights reserved.
 * Copyright (c) 1999-2001 SATO Kazumi, All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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
 */

/*
 * Copyright (c) 1988 University of Utah.
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * the Systems Programming Group of the University of Utah Computer
 * Science Department, The Mach Operating System project at
 * Carnegie-Mellon University and Ralph Campbell.
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
 *	@(#)machdep.c	8.3 (Berkeley) 1/12/94
 */

#include <sys/cdefs.h>			/* RCS ID & Copyright macro defns */

__KERNEL_RCSID(0, "$NetBSD: machdep.c,v 1.57 2001/07/03 02:37:06 enami Exp $");

/* from: Utah Hdr: machdep.c 1.63 91/04/24 */
#include "opt_vr41xx.h"
#include "opt_tx39xx.h"
#include "opt_boot_standalone.h"
#include "opt_spec_platform.h"
#include "biconsdev.h"
#include "fs_mfs.h"
#include "opt_ddb.h"
#include "opt_kgdb.h"
#include "opt_rtc_offset.h"
#include "fs_nfs.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <sys/map.h>
#include <sys/proc.h>
#include <sys/buf.h>
#include <sys/reboot.h>
#include <sys/conf.h>
#include <sys/file.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/msgbuf.h>
#include <sys/ioctl.h>
#include <sys/user.h>
#include <sys/mount.h>
#include <sys/syscallargs.h>
#include <sys/kcore.h>
#include <sys/boot_flag.h>

#ifdef KGDB
#include <sys/kgdb.h>
#endif

#include <uvm/uvm_extern.h>

#include <sys/sysctl.h>
#include <dev/cons.h>

#include <ufs/mfs/mfs_extern.h>		/* mfs_initminiroot() */

#include <machine/cpu.h>
#include <machine/reg.h>
#include <machine/psl.h>
#include <machine/pte.h>
#include <machine/bus.h>
#include <machine/autoconf.h>
#include <machine/sysconf.h>
#include <machine/bootinfo.h>
#include <machine/platid.h>
#include <machine/platid_mask.h>
#include <machine/locore.h>

#ifdef DDB
#include <sys/exec_aout.h>		/* XXX backwards compatilbity for DDB */
#include <machine/db_machdep.h>
#include <ddb/db_access.h>
#include <ddb/db_sym.h>
#include <ddb/db_extern.h>
#ifndef DB_ELFSIZE
#error Must define DB_ELFSIZE!
#endif
#define ELFSIZE         DB_ELFSIZE
#include <sys/exec_elf.h>
#endif

#if NBICONSDEV > 0
#include <dev/hpc/biconsvar.h>
#include <dev/hpc/bicons.h>
#define biconscnpollc	nullcnpollc
cons_decl(bicons);
static struct consdev bicons = cons_init(bicons);
#define DPRINTF(arg) printf arg
#else
#define DPRINTF(arg)
#endif

#ifdef NFS
extern int nfs_mountroot __P((void));
extern int (*mountroot) __P((void));
#endif
#ifdef MEMORY_DISK_DYNAMIC
void md_root_setconf(caddr_t, size_t);
#endif

/* the following is used externally (sysctl_hw) */
char	machine[] = MACHINE;		/* from <machine/param.h> */
char	machine_arch[] = MACHINE_ARCH;	/* from <machine/param.h> */
char	cpu_model[128];	
char	booted_kernel[128];

char	cpu_name[40];			/* set cpu depend xx_init() */

/* Our exported CPU info; we can have only one. */  
struct cpu_info cpu_info_store;

#define VPRINTF(arg)	if (bootverbose) printf arg;

/* maps for VM objects */
struct vm_map *exec_map = NULL;
struct vm_map *mb_map = NULL;
struct vm_map *phys_map = NULL;

int	systype;		/* mother board type */
int	physmem;		/* max supported memory, changes to actual */
int	mem_cluster_cnt;
phys_ram_seg_t mem_clusters[VM_PHYSSEG_MAX];

/*
 * safepri is a safe priority for sleep to set for a spin-wait
 * during autoconfiguration or after a panic.
 * Used as an argument to splx().
 * XXX disables interrupt 5 to disable mips3 on-chip clock, which also
 * disables mips1 FPU interrupts.
 */
int	safepri = MIPS3_PSL_LOWIPL;	/* XXX */

unsigned ssir;				/* schedules software interrupt */

struct splvec	splvec;			/* XXX will go XXX */

void mach_init __P((int, char *[], struct bootinfo*));

static struct bootinfo bi_copy;
struct bootinfo *bootinfo = NULL;

unsigned (*clkread) __P((void)); /* high resolution timer if available */
unsigned nullclkread __P((void));

int	initcpu __P((void));
void	consinit __P((void));

#ifdef DEBUG
/* stacktrace code violates prototypes to get callee's registers */
extern void stacktrace __P((void)); /*XXX*/
#endif

/* Motherboard or system-specific initialization vector */
void	unimpl_os_init __P((void));
void	unimpl_bus_reset __P((void));
int	unimpl_intr __P((unsigned, unsigned, unsigned, unsigned));
void	unimpl_cons_init __P((void));
void	unimpl_device_register __P((struct device *, void *));
int 	unimpl_iointr __P((u_int32_t, u_int32_t, u_int32_t, u_int32_t));
void	unimpl_clockintr __P ((void *));
void    unimpl_fb_init __P((caddr_t*));
void    unimpl_mem_init __P((paddr_t));
void	unimpl_reboot __P((int howto, char *bootstr));

struct platform platform = {
	"iobus not set",
	unimpl_os_init,
	unimpl_bus_reset,
	unimpl_cons_init,
	unimpl_device_register,
	unimpl_iointr,
	unimpl_clockintr,
	unimpl_fb_init,
	unimpl_mem_init,
	unimpl_reboot,
};

#ifdef VR41XX
extern void	vr_init __P((void));
#endif
#ifdef TX39XX
extern void	tx_init __P((void));
#endif

extern caddr_t esym;
extern struct user *proc0paddr;

/*
 * Do all the stuff that locore normally does before calling main().
 * Process arguments passed to us by the boot loader. 
 * Return the first page address following the system.
 */
void
mach_init(argc, argv, bi)
	int argc;
	char *argv[];
	struct bootinfo *bi;
{
	int i;
	caddr_t kernend, v;
	unsigned size;
	char *cp;
	extern char edata[], end[];

	/* clear the BSS segment */
#ifdef DDB
	size_t symbolsz = 0;
	Elf_Ehdr *eh = (void *)end;
	if (memcmp(eh->e_ident, ELFMAG, SELFMAG) == 0 &&
	    eh->e_ident[EI_CLASS] == ELFCLASS) {
		esym = end;
		if (eh->e_entry != 0) {
			/* pbsdboot */
			symbolsz = eh->e_entry;
		} else {
			/* hpcboot */
			Elf_Shdr *sh = (void *)(end + eh->e_shoff);
			for(i = 0; i < eh->e_shnum; i++, sh++)
				if (sh->sh_offset > 0 &&
				    (sh->sh_offset + sh->sh_size) > symbolsz)
					symbolsz = sh->sh_offset + sh->sh_size;
		}
		esym += symbolsz;
		kernend = (caddr_t)mips_round_page(esym);
		bzero(edata, end - edata);
	} else
#endif /* DDB */
	{
		kernend = (caddr_t)mips_round_page(end);
		memset(edata, 0, kernend - edata);
	}

#if defined(BOOT_STANDALONE)
#if !defined (SPEC_PLATFORM) || SPEC_PLATFORM == 1
#error specify SPEC_PLATFORM=platid_mask_MACH_xxx_yyy in BOOT_STANDALONE case.
#error see platid_mask.c for platid_mask_MACH_xxx_yyy.
#else
	memcpy(&platid, &SPEC_PLATFORM, sizeof(platid));
#endif
#endif /* defined(BOOT_STANDALONE) && defined(SPEC_PLATFORM) */
	/*
	 *  Arguments are set up by boot loader.
	 */
	if (bi && bi->magic == BOOTINFO_MAGIC) {
		memset(&bi_copy, 0, sizeof(struct bootinfo));
		memcpy(&bi_copy, bi, min(bi->length, sizeof(struct bootinfo)));
		bootinfo = &bi_copy;
		if (bootinfo->platid_cpu != 0) {
			platid.dw.dw0 = bootinfo->platid_cpu;
		}
		if (bootinfo->platid_machine != 0) {
			platid.dw.dw1 = bootinfo->platid_machine;
		}
	}
#if defined TX39XX && defined VR41XX
/* XXX: currently, the case defined TX39XX && defined VR41XX don't work */
#error misconfiguration
#endif /* defined TX39XX && defined VR41XX */

	/* Platform Specific Function Hooks */
#ifdef VR41XX
#ifdef TX39XX
	if (platid_match(&platid, &platid_mask_CPU_MIPS_VR_41XX))
#endif /* TX39XX */
	{
		vr_init();
#if NBICONSDEV > 0
	/* bicons don't need actual device initialize. only bootinfo needed. */
		cn_tab = &bicons;
		bicons_init(&bicons);
#endif
	}
#endif /* VR41XX */
#ifdef TX39XX
#ifdef VR41XX
	if (platid_match(&platid, &platid_mask_CPU_MIPS_TX_3900)
	    || platid_match(&platid, &platid_mask_CPU_MIPS_TX_3920))
#endif /* VR41XX */
	{
		tx_init();
	}
#endif /* TX39XX */

	/* Initialize frame buffer (to steal DMA buffer, stay here.) */
	(*platform.fb_init)(&kernend);
	kernend = (caddr_t)mips_round_page(kernend);

	/*
	 * Set the VM page size.
	 */
	uvmexp.pagesize = NBPG; /* Notify the VM system of our page size. */
	uvm_setpagesize();

	/*
	 * Copy exception-dispatch code down to exception vector.
	 * Initialize locore-function vector.
	 * Clear out the I and D caches.
	 */
	mips_vector_init();

	/*
	 * Look at arguments passed to us and compute boothowto.
	 */
	/* XXX, for debugging. */
	if (bootinfo) {
		DPRINTF(("Bootinfo. available, "));
	}
	DPRINTF(("args: "));
	for (i = 0; i < argc; i++) {
		DPRINTF(("%s ", argv[i]));
	}

	DPRINTF(("\n"));
	DPRINTF(("platform ID: %08lx %08lx\n", platid.dw.dw0, platid.dw.dw1));

#ifndef RTC_OFFSET
	/*
	 * rtc_offset from bootinfo.timezone set by pbsdboot.exe
	 */
	if (rtc_offset == 0 && bootinfo
	   && bootinfo->timezone > (-12*60)
	   && bootinfo->timezone <= (12*60))
		rtc_offset = bootinfo->timezone;
#endif /* RTC_OFFSET */

	/* Compute bootdev */
	makebootdev("wd0"); /* default boot device */

	boothowto = 0;
#ifdef KADB
	boothowto |= RB_KDB;
#endif
	strncpy(booted_kernel, argv[0], sizeof(booted_kernel));
	booted_kernel[sizeof(booted_kernel)-1] = 0;
	for (i = 1; i < argc; i++) {
		for (cp = argv[i]; *cp; cp++) {
			switch (*cp) {
			case 'h': /* XXX, serial console */
				bootinfo->bi_cnuse |= BI_CNUSE_SERIAL;
				break;

			case 'b':
				/* boot device: -b=sd0 etc. */
#ifdef NFS
				if (strcmp(cp+2, "nfs") == 0)
					mountroot = nfs_mountroot;
				else
					makebootdev(cp+2);
#else
				makebootdev(cp+2);
#endif
				cp += strlen(cp);
				break;
			default:
				BOOT_FLAG(*cp, boothowto);
				break;
			}
		}
	}
#ifdef MFS
	/*
	 * Check to see if a mini-root was loaded into memory. It resides
	 * at the start of the next page just after the end of BSS.
	 */
	if (boothowto & RB_MINIROOT) {
		size_t fssz;
		fssz = round_page(mfs_initminiroot(kernend));
#ifdef MEMORY_DISK_DYNAMIC
		md_root_setconf((caddr_t)kernend, fssz);
#endif
		kernend += fssz;
	}
#endif

#ifdef DDB
	/* init symbols if present */
	if (esym)
		ddb_init(symbolsz, &end, esym);
#endif
	/*
	 * Alloc u pages for proc0 stealing KSEG0 memory.
	 */
	proc0.p_addr = proc0paddr = (struct user *)kernend;
	proc0.p_md.md_regs =
	    (struct frame *)((caddr_t)kernend + UPAGES * PAGE_SIZE) - 1;
	memset(kernend, 0, UPAGES * PAGE_SIZE);
	curpcb = &proc0.p_addr->u_pcb;
	curpcb->pcb_context[11] = MIPS_INT_MASK | MIPS_SR_INT_IE; /* SR */

	kernend += UPAGES * PAGE_SIZE;

	/* Setup interrupt handler */
	(*platform.os_init)();

	/* Initialize console and KGDB serial port. */
	(*platform.cons_init)();

#if defined(DDB) || defined(KGDB)
	if (boothowto & RB_KDB) {
#ifdef DDB
		Debugger();
#endif
#ifdef KGDB
		kgdb_debug_init = 1;
		kgdb_connect(1);
#endif
	}
#endif

	/* Find physical memory regions. */
	(*platform.mem_init)((paddr_t)kernend - MIPS_KSEG0_START);

	printf("mem_cluster_cnt = %d\n", mem_cluster_cnt);
	physmem = 0;
	for (i = 0; i < mem_cluster_cnt; i++) {
		printf("mem_clusters[%d] = {0x%lx,0x%lx}\n", i,
		    (paddr_t)mem_clusters[i].start,
		    (paddr_t)mem_clusters[i].size);
		physmem += atop(mem_clusters[i].size);
	}

	/* Cluster 0 is always the kernel, which doesn't get loaded. */
	for (i = 1; i < mem_cluster_cnt; i++) {
		paddr_t start, size;

		start = (paddr_t)mem_clusters[i].start;
		size = (paddr_t)mem_clusters[i].size;

		printf("loading 0x%lx,0x%lx\n", start, size);

		memset((void *)MIPS_PHYS_TO_KSEG1(start), 0,
		       size);

		uvm_page_physload(atop(start), atop(start + size),
				  atop(start), atop(start + size),
				  VM_FREELIST_DEFAULT);
	}

	/*
	 * Initialize error message buffer (at end of core).
	 */
	mips_init_msgbuf();

	/*
	 * Compute the size of system data structures.  pmap_bootstrap()
	 * needs some of this information.
	 */
	size = (unsigned)allocsys(NULL, NULL);

	/*
	 * Initialize the virtual memory system.
	 */
	pmap_bootstrap();

	/*
	 * Allocate space for system data structures.  These data structures
	 * are allocated here instead of cpu_startup() because physical
	 * memory is directly addressable.  We don't have to map these into
	 * virtual address space.
	 */
	v = (caddr_t)uvm_pageboot_alloc(size);
	if ((allocsys(v, NULL) - v) != size)
		panic("mach_init: table size inconsistency");
}


/*
 * Machine-dependent startup code.
 * allocate memory for variable-sized tables, initialize cpu.
 */
void
cpu_startup()
{
	unsigned i;
	int base, residual;
	vaddr_t minaddr, maxaddr;
	vsize_t size;
	char pbuf[9];
#ifdef DEBUG
	extern int pmapdebug;
	int opmapdebug = pmapdebug;

	pmapdebug = 0;
#endif

	/*
	 * Good {morning,afternoon,evening,night}.
	 */
	printf(version);
	sprintf(cpu_model, "%s (%s)", platid_name(&platid), cpu_name);
	printf("%s\n", cpu_model);
	format_bytes(pbuf, sizeof(pbuf), ctob(physmem));
	printf("total memory = %s\n", pbuf);
	if (bootverbose) {
		/* show again when verbose mode */
		printf("total memory banks = %d\n", mem_cluster_cnt);
		for (i = 0; i < mem_cluster_cnt; i++) {
			printf("memory bank %d = 0x%08lx %ldKB(0x%08lx)\n", i,
			    (paddr_t)mem_clusters[i].start,
			    (paddr_t)mem_clusters[i].size/1024,
			    (paddr_t)mem_clusters[i].size);
		}
	}

	/*
	 * Allocate virtual address space for file I/O buffers.
	 * Note they are different than the array of headers, 'buf',
	 * and usually occupy more virtual memory than physical.
	 */
	size = MAXBSIZE * nbuf;
	if (uvm_map(kernel_map, (vaddr_t *)&buffers, round_page(size),
		    NULL, UVM_UNKNOWN_OFFSET, 0,
		    UVM_MAPFLAG(UVM_PROT_NONE, UVM_PROT_NONE, UVM_INH_NONE,
				UVM_ADV_NORMAL, 0)) != 0)
		panic("cpu_startup: cannot allocate VM for buffers");

	minaddr = (vaddr_t)buffers;
	if ((bufpages / nbuf) >= btoc(MAXBSIZE)) {
		bufpages = btoc(MAXBSIZE) * nbuf; /* do not overallocate RAM */
	}
	base = bufpages / nbuf;
	residual = bufpages % nbuf;

	/* now allocate RAM for buffers */
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
		curbuf = (vaddr_t)buffers + (i * MAXBSIZE);
		curbufsize = NBPG * ((i < residual) ? (base+1) : base);

		while (curbufsize) {
			pg = uvm_pagealloc(NULL, 0, NULL, 0);
			if (pg == NULL)
				panic("cpu_startup: not enough memory for "
				    "buffer cache");
			pmap_kenter_pa(curbuf, VM_PAGE_TO_PHYS(pg),
				       VM_PROT_READ|VM_PROT_WRITE);
			curbuf += PAGE_SIZE;
			curbufsize -= PAGE_SIZE;
		}
	}
	pmap_update();

	/*
	 * Allocate a submap for exec arguments.  This map effectively
	 * limits the number of processes exec'ing at any time.
	 */
	exec_map = uvm_km_suballoc(kernel_map, &minaddr, &maxaddr,
				   16 * NCARGS, VM_MAP_PAGEABLE, FALSE, NULL);

	/*
	 * Allocate a submap for physio
	 */
	phys_map = uvm_km_suballoc(kernel_map, &minaddr, &maxaddr,
				   VM_PHYS_SIZE, 0, FALSE, NULL);

	/*
	 * No need to allocate an mbuf cluster submap.  Mbuf clusters
	 * are allocated via the pool allocator, and we use KSEG to
	 * map those pages.
	 */

#ifdef DEBUG
	pmapdebug = opmapdebug;
#endif
	format_bytes(pbuf, sizeof(pbuf), ptoa(uvmexp.free));
	printf("avail memory = %s\n", pbuf);
	format_bytes(pbuf, sizeof(pbuf), bufpages * NBPG);
	printf("using %d buffers containing %s of memory\n", nbuf, pbuf);

	/*
	 * Set up buffers, so they can be used to read disk labels.
	 */
	bufinit();

	/*
	 * Set up CPU-specific registers, cache, etc.
	 */
	initcpu();
}


/*
 * Machine dependent system variables.
 */
int
cpu_sysctl(name, namelen, oldp, oldlenp, newp, newlen, p)
	int *name;
	u_int namelen;
	void *oldp;
	size_t *oldlenp;
	void *newp;
	size_t newlen;
	struct proc *p;
{

	/* all sysctl names at this level are terminal */
	if (namelen != 1)
		return (ENOTDIR);		/* overloaded */

	switch (name[0]) {
	case CPU_CONSDEV:
		return (sysctl_rdstruct(oldp, oldlenp, newp, &cn_tab->cn_dev,
		    sizeof cn_tab->cn_dev));
	case CPU_ROOT_DEVICE:
		return (sysctl_rdstring(oldp, oldlenp, newp, 
		    root_device->dv_xname));
	case CPU_BOOTED_KERNEL:
		return (sysctl_rdstring(oldp, oldlenp, newp, 
		    booted_kernel));
	default:
		return (EOPNOTSUPP);
	}
	/* NOTREACHED */
}

void
cpu_reboot(howto, bootstr)
	volatile int howto;	/* XXX volatile to keep gcc happy */
	char *bootstr;
{
	extern int cold;

	/* take a snap shot before clobbering any registers */
	if (curproc)
		savectx((struct user *)curpcb);

#ifdef DEBUG
	if (panicstr)
		stacktrace();
#endif

	/* If system is cold, just halt. */
	if (cold) {
		howto |= RB_HALT;
		goto haltsys;
	}

	/* If "always halt" was specified as a boot flag, obey. */
	if ((boothowto & RB_HALT) != 0)
		howto |= RB_HALT;

	boothowto = howto;
	if ((howto & RB_NOSYNC) == 0) {
		/*
		 * Synchronize the disks....
		 */
		vfs_shutdown();

		/*
		 * If we've been adjusting the clock, the todr
		 * will be out of synch; adjust it now.
		 */
		resettodr();
	}

	/* Disable interrupts. */
	splhigh();

	/* If rebooting and a dump is requested do it. */
#if 0
	if ((howto & (RB_DUMP | RB_HALT)) == RB_DUMP)
#else
	if (howto & RB_DUMP)
#endif
		dumpsys();

haltsys:

	/* run any shutdown hooks */
	doshutdownhooks();

	/* Finally, halt/reboot the system. */
	printf("%s\n\n", howto & RB_HALT ? "halted." : "rebooting...");
	(*platform.reboot)(howto, bootstr);

	while(1)
		;
	/*NOTREACHED*/
}

/*
 * Return the best possible estimate of the time in the timeval to
 * which tvp points.  We guarantee that the time will be greater than
 * the value obtained by a previous call.
 */
void
microtime(tvp)
	struct timeval *tvp;
{
	int s = splclock();
	static struct timeval lasttime;

	*tvp = time;

	if (tvp->tv_usec >= 1000000) {
		tvp->tv_usec -= 1000000;
		tvp->tv_sec++;
	}

	if (tvp->tv_sec == lasttime.tv_sec &&
	    tvp->tv_usec <= lasttime.tv_usec &&
	    (tvp->tv_usec = lasttime.tv_usec + 1) >= 1000000) {
		tvp->tv_sec++;
		tvp->tv_usec -= 1000000;
	}
	lasttime = *tvp;
	splx(s);
}

int
initcpu()
{
	int i = 0;

	/*
	 * reset after autoconfig probe:
	 * clear  any memory errors, reset any pending interrupts.
	 */

	(*platform.bus_reset)();

	return i;
}

void
consinit()
{
	/*
	 *	Nothing to do.
	 *	Console is alredy initialized in platform.cons_init().
	 */

	return;
}

/*
 * Wait "n" microseconds.
 */
void
delay(n)
        int n;
{
        DELAY(n);
}

/*
 *  Ensure all platform vectors are always initialized.
 */
void
unimpl_os_init()
{
	panic("sysconf.init didnt set os_init");
}

void
unimpl_bus_reset()
{
	panic("sysconf.init didnt set bus_reset");
}

void
unimpl_cons_init()
{
	panic("sysconf.init didnt set cons_init");
}

void
unimpl_device_register(sc, arg)
	struct device *sc;
	void *arg;
{
	panic("sysconf.init didnt set device_register");

}

int
unimpl_iointr(arg, arg2, arg3, arg4)
	u_int32_t arg, arg2, arg3, arg4;
{
	panic("sysconf.init didnt set iointr");
}

void
unimpl_clockintr(arg)
	void *arg;
{
	panic("sysconf.init didnt set clockintr");
}

int
unimpl_intr(mask, pc, statusreg, causereg)
	u_int mask;
	u_int pc;
	u_int statusreg;
	u_int causereg;
{
	panic("sysconf.init didnt set intr");
}

void
unimpl_mem_init(kernend)
	paddr_t kernend;
{
	panic("sysconf.init didnt set memory");
}

void
unimpl_fb_init(kernend)
	caddr_t *kernend;
{
	panic("sysconf.init didnt set frame buffer");
}

void
unimpl_reboot(howto, bootstr)
	int howto;
	char *bootstr;
{
	printf("platform depend reboot code is not implemented.\n");
}

unsigned
nullclkread()
{
	return 0;
}	


