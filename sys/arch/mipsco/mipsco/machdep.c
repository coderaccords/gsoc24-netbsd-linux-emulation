/*	$NetBSD: machdep.c,v 1.3 2000/08/16 21:54:44 wdk Exp $	*/

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

__KERNEL_RCSID(0, "$NetBSD: machdep.c,v 1.3 2000/08/16 21:54:44 wdk Exp $");

/* from: Utah Hdr: machdep.c 1.63 91/04/24 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/signalvar.h>
#include <sys/kernel.h>
#include <sys/map.h>
#include <sys/proc.h>
#include <sys/buf.h>
#include <sys/reboot.h>
#include <sys/conf.h>
#include <sys/file.h>
#include <sys/callout.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/msgbuf.h>
#include <sys/ioctl.h>
#include <sys/device.h>
#include <sys/user.h>
#include <sys/exec.h>
#include <sys/sysctl.h>
#include <sys/mount.h>
#include <sys/syscallargs.h>
#include <sys/kcore.h>

#include <uvm/uvm_extern.h>

#include <ufs/mfs/mfs_extern.h>		/* mfs_initminiroot() */

#include <machine/cpu.h>
#include <machine/reg.h>
#include <machine/psl.h>
#include <machine/pte.h>

#ifdef DDB
#include <machine/db_machdep.h>
#endif

#include <machine/intr.h>
#include <machine/mainboard.h>
#include <machine/sysconf.h>
#include <machine/autoconf.h>
#include <dev/clock_subr.h>
#include <dev/cons.h>

#include "fs_mfs.h"
#include "opt_ddb.h"
#include "opt_execfmt.h"

/* the following is used externally (sysctl_hw) */
char  machine[] = MACHINE;	/* from <machine/param.h> */
char  machine_arch[] = MACHINE_ARCH;
char  cpu_model[40];
unsigned ssir;

/* Our exported CPU info; we can have only one. */  
struct cpu_info cpu_info_store;

/* maps for VM objects */

vm_map_t exec_map = NULL;
vm_map_t mb_map = NULL;
vm_map_t phys_map = NULL;

int maxmem;			/* max memory per process */
int physmem;			/* max supported memory, changes to actual */
int physmem_max = 32*1024*1024 / NBPG;		/* XXX: Pages */

phys_ram_seg_t mem_clusters[VM_PHYSSEG_MAX];
int mem_cluster_cnt;

void to_monitor __P((int)) __attribute__((__noreturn__));
void prom_halt __P((int)) __attribute__((__noreturn__));

struct evcnt soft_evcnt[IPL_NSOFT];

/*
 *  Local functions.
 */
int initcpu __P((void));
void configure __P((void));

void mach_init __P((int, char *[], char*[]));
void softintr_init __P((void));

#ifdef DEBUG
/* stacktrace code violates prototypes to get callee's registers */
extern void stacktrace __P((void)); /*XXX*/
#endif

/*
 * safepri is a safe priority for sleep to set for a spin-wait
 * during autoconfiguration or after a panic.  Used as an argument to splx().
 * XXX disables interrupt 5 to disable mips3 on-chip clock, which also
 * disables mips1 FPU interrupts.
 */
int	safepri = MIPS3_PSL_LOWIPL;	/* XXX */
extern struct user *proc0paddr;

/* locore callback-vector setup */
extern void mips_vector_init  __P((void));

void pizazz_init __P((void));

/* platform-specific initialization vector */
static void	unimpl_cons_init __P((void));
static void	unimpl_iointr __P((unsigned, unsigned, unsigned, unsigned));
static int	unimpl_memsize __P((caddr_t));
static unsigned	unimpl_clkread __P((void));
static void	unimpl_todr __P((struct clock_ymdhms *));
static void	unimpl_intr_establish __P((int, int (*)__P((void *)), void *));

struct platform platform = {
	"iobus not set",
	unimpl_cons_init,
	unimpl_iointr,
	unimpl_memsize,
	unimpl_clkread,
	unimpl_todr,
	unimpl_todr,
	unimpl_intr_establish,
};

struct consdev *cn_tab = NULL;
extern struct consdev consdev_prom;
extern struct consdev consdev_zs;

int tty00_is_console = 0;

static void null_cnprobe __P((struct consdev *));
static void prom_cninit __P((struct consdev *));
static int  prom_cngetc __P((dev_t));
static void prom_cnputc __P((dev_t, int));
static void null_cnpollc __P((dev_t, int));

void prom_printf();
void prom_putchar(int);
int prom_getchar();

struct consdev consdev_prom = {
        null_cnprobe,
	prom_cninit,
	prom_cngetc,
	prom_cnputc,
        null_cnpollc,
};


/*
 * Do all the stuff that locore normally does before calling main().
 * Process arguments passed to us by the prom monitor.
 * Return the first page address following the system.
 */
void
mach_init(argc, argv, envp)
	int    argc;
	char   *argv[];
	char   *envp[];
{
	u_long first, last;
	caddr_t kernend, v;
	vsize_t size;
	char *cp;
	int i;
	extern char edata[], end[];

	/* clear the BSS segment */
	kernend = (caddr_t)mips_round_page(end);
	bzero(edata, kernend - edata);

	/*
	 * Set the VM page size.
	 */
	uvm_setpagesize();

	consinit();

	/*
         * The MIPS Rc3230 series machine have a really ugly memory
         * alias that is difficult to detect.
	 *
	 * For now we limit the scan to 32MB which is the maximum
	 * onboard memory allowed when using 1MB SIMM's
	 */

	/*
	 * Non destructive scan of memory to determine the size
	 */
	physmem = btoc((vaddr_t)kernend - MIPS_KSEG0_START);
	cp = (char *)MIPS_PHYS_TO_KSEG1(physmem << PGSHIFT);
	while (cp < (char *)MIPS_MAX_MEM_ADDR) {
	  	int i, j;
		i = *(int *)cp;
		j = ((int *)cp)[4];
		*(int *)cp = 0xa5a5a5a5;
		/*
		 * Data will persist on the bus if we read it right away.
		 * Have to be tricky here.
		 */
		((int *)cp)[4] = 0x5a5a5a5a;
		wbflush();
		if (*(int *)cp != 0xa5a5a5a5)
			break;
		*(int *)cp = i;
		((int *)cp)[4] = j;
		cp += NBPG;
		if (physmem >= physmem_max)
		        break;
		physmem++;
	}

	maxmem = physmem;

	/*
	 * Now that we know how much memory we have, initialize the
	 * mem cluster array.
	 */
	mem_clusters[0].start = 0;		/* XXX is this correct? */
	mem_clusters[0].size  = ctob(physmem);
	mem_cluster_cnt = 1;

	/*
	 * Copy exception-dispatch code down to exception vector.
	 * Initialize locore-function vector.
	 * Clear out the I and D caches.
	 */
	mips_vector_init();

	/* Look at argv[0] and compute bootdev */
	makebootdev(argv[0]);

	/*
	 * Look at arguments passed to us and compute boothowto.
	 */
	boothowto = RB_SINGLE;
	for (i = 1; i < argc; i++) {
		for (cp = argv[i]; *cp; cp++) {
			switch (*cp) {
			case 'a': /* autoboot */
			case 'A':
				boothowto &= ~RB_SINGLE;
				break;

#if defined(KGDB) || defined(DDB)
			case 'd': /* break into the kernel debugger ASAP */
			case 'D':
				boothowto |= RB_KDB;
				break;
#endif

			case 'm': /* mini root present in memory */
			case 'M':
				boothowto |= RB_MINIROOT;
				break;

			case 'n': /* ask for names */
				boothowto |= RB_ASKNAME;
				break;

			case 'N': /* don't ask for names */
				boothowto &= ~RB_ASKNAME;
				break;

			case 's': /* single-user (default) */
			case 'S':
				boothowto |= RB_SINGLE;
				break;

			case '-': /* Ignore superfluous '-' */
				break;

			default:
				printf("bootflag '%c' not recognised", *cp);
			}
		}
	}


#ifdef DDB
	/*
	 * Initialize machine-dependent DDB commands, in case of early panic.
	 */
	db_machine_init();

	if (boothowto & RB_KDB)
		Debugger();
#endif

#ifdef MFS
	/*
	 * Check to see if a mini-root was loaded into memory. It resides
	 * at the start of the next page just after the end of BSS.
	 */
	if (boothowto & RB_MINIROOT) {
		boothowto |= RB_DFLTROOT;
		kernend += round_page(mfs_initminiroot(kernend));
	}
#endif

	/*
	 * Alloc u pages for proc0 stealing KSEG0 memory.
	 */
	proc0.p_addr = proc0paddr = (struct user *)kernend;
	proc0.p_md.md_regs = (struct frame *)(kernend + USPACE) - 1;
	memset(proc0.p_addr, 0, USPACE);
	curpcb = &proc0.p_addr->u_pcb;
	curpcb->pcb_context[11] = MIPS_INT_MASK | MIPS_SR_INT_IE; /* SR */

	kernend += USPACE;

	/*
	 * Load the rest of the available pages into the VM system.
	 */
	first = round_page(MIPS_KSEG0_TO_PHYS(kernend));
	last = mem_clusters[0].start + mem_clusters[0].size;
	uvm_page_physload(atop(first), atop(last), atop(first), atop(last),
	    VM_FREELIST_DEFAULT);

	/*
	 * Initialize error message buffer (at end of core).
	 */
	mips_init_msgbuf();

	/*
	 * Allocate space for system data structures.  These data structures
	 * are allocated here instead of cpu_startup() because physical
	 * memory is directly addressable.  We don't have to map these into
	 * virtual address space.
	 */
	size = (vsize_t)allocsys(NULL, NULL);
	v = (caddr_t)pmap_steal_memory(size, NULL, NULL); 
	if ((allocsys(v, NULL) - v) != size)
		panic("mach_init: table size inconsistency");
	/*
	 * Set up interrupt handling and I/O addresses.
	 */

	pizazz_init();

	/*
	 * Initialize the virtual memory system.
	 */
	pmap_bootstrap();
}



/*
 * cpu_startup: allocate memory for variable-sized tables,
 * initialize cpu, and do autoconfiguration.
 */
void
cpu_startup()
{
	register unsigned i;
	int base, residual;
	vaddr_t minaddr, maxaddr;
	vsize_t size;
#ifdef DEBUG
	extern int pmapdebug;
	int opmapdebug = pmapdebug;

	pmapdebug = 0;
#endif

	/*
	 * Good {morning,afternoon,evening,night}.
	 */
	printf(version);
	printf("%s\n", cpu_model);
	printf("total memory = %d\n", ctob(physmem));

	/*
	 * Allocate virtual address space for file I/O buffers.
	 * Note they are different than the array of headers, 'buf',
	 * and usually occupy more virtual memory than physical.
	 */
	size = MAXBSIZE * nbuf;
	if (uvm_map(kernel_map, (vaddr_t *)&buffers, round_page(size),
		    NULL, UVM_UNKNOWN_OFFSET,
		    UVM_MAPFLAG(UVM_PROT_NONE, UVM_PROT_NONE, UVM_INH_NONE,
				UVM_ADV_NORMAL, 0)) != KERN_SUCCESS)
		panic("startup: cannot allocate VM for buffers");
	minaddr = (vaddr_t)buffers;
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
				panic("cpu_startup: not enough memory for "
				    "buffer cache");
			pmap_kenter_pa(curbuf, VM_PAGE_TO_PHYS(pg),
				       VM_PROT_READ|VM_PROT_WRITE);
			curbuf += PAGE_SIZE;
			curbufsize -= PAGE_SIZE;
		}
	}
	/*
	 * Allocate a submap for exec arguments.  This map effectively
	 * limits the number of processes exec'ing at any time.
	 */
	exec_map = uvm_km_suballoc(kernel_map, &minaddr, &maxaddr,
				   16 * NCARGS, TRUE, FALSE, NULL);
	/*
	 * Allocate a submap for physio
	 */
	phys_map = uvm_km_suballoc(kernel_map, &minaddr, &maxaddr,
				   VM_PHYS_SIZE, TRUE, FALSE, NULL);

	/*
	 * No need to allocate an mbuf cluster submap.  Mbuf clusters
	 * are allocated via the pool allocator, and we use KSEG to
	 * map those pages.
	 */

#ifdef DEBUG
	pmapdebug = opmapdebug;
#endif
	printf("avail mem = %ld\n", ptoa(uvmexp.free));
	printf("using %d buffers containing %d bytes of memory\n",
		nbuf, bufpages * NBPG);

	/*
	 * Set up buffers, so they can be used to read disk labels.
	 */
	bufinit();
}

void
softintr_init()
{	
    int i;
    static const char *intr_names[] = IPL_SOFTNAMES;

    for (i=0; i < IPL_NSOFT; i++) {
	evcnt_attach_dynamic(&soft_evcnt[i], EVCNT_TYPE_INTR, NULL,
		"soft", intr_names[i]);
    }
}

/*
 * machine dependent system variables.
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
	default:
		return (EOPNOTSUPP);
	}
	/* NOTREACHED */
}

int	waittime = -1;

/*
 * call PROM to halt or reboot.
 */
void
prom_halt(howto)
	int howto;
{
	to_monitor(howto);
}

void
cpu_reboot(howto, bootstr)
	volatile int howto;
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
	if ((howto & RB_NOSYNC) == 0 && waittime < 0) {
		/*
		 * Synchronize the disks....
		 */
		waittime = 0;
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

	if ((howto & RB_POWERDOWN) == RB_POWERDOWN)
		prom_halt(0x80);	/* rom monitor RB_PWOFF */

	/* Finally, halt/reboot the system. */
	printf("%s\n\n", howto & RB_HALT ? "halted." : "rebooting...");
	prom_halt(howto & RB_HALT);
	/*NOTREACHED*/
}

/*
 * Return the best possible estimate of the time in the timeval
 * to which tvp points.  Unfortunately, we can't read the hardware registers.
 * We guarantee that the time will be greater than the value obtained by a
 * previous call.
 */
void
microtime(tvp)
	register struct timeval *tvp;
{
	static struct timeval lasttime;
	int s = splclock();
	long rambo_getutime();

	*tvp = time;

	tvp->tv_usec += (*platform.clkread)();

	if (tvp->tv_usec >= 1000000) {
		tvp->tv_usec -= 1000000;
		tvp->tv_sec++;
	}

	if (tvp->tv_sec == lasttime.tv_sec &&
	    tvp->tv_usec <= lasttime.tv_usec &&
	    (tvp->tv_usec = lasttime.tv_usec + 1) > 1000000) {
		tvp->tv_sec++;
		tvp->tv_usec -= 1000000;
	}
	lasttime = *tvp;
	splx(s);
}

int
initcpu()
{
        softintr_init();
	spl0();		/* safe to turn interrupts on now */
	return 0;
}

static void
unimpl_cons_init()
{

	panic("sysconf.init didn't set cons_init");
}

static void
unimpl_iointr(mask, pc, statusreg, causereg)
	u_int mask;
	u_int pc;
	u_int statusreg;
	u_int causereg;
{

	panic("sysconf.init didn't set intr");
}

static int
unimpl_memsize(first)
caddr_t first;
{

	panic("sysconf.init didn't set memsize");
}

static unsigned
unimpl_clkread()
{
	panic("sysconf.init didn't set clkread");
}

static void
unimpl_todr(dt)
	struct clock_ymdhms *dt;
{
	panic("sysconf.init didn't init TOD");
}

void
unimpl_intr_establish(level, func, arg)
	int level;
	int (*func) __P((void *));
	void *arg;
{
	panic("sysconf.init didn't init intr_establish\n");
}

void
delay(n)
	int n;
{
	DELAY(n);
}

void
cpu_intr(status, cause, pc, ipending)
	u_int32_t status;
	u_int32_t cause;
	u_int32_t pc;
	u_int32_t ipending;
{
	uvmexp.intrs++;

	/* device interrupts */
	(*platform.iointr)(status, cause, pc, ipending);

	/* software simulated interrupt */
	if ((ipending & MIPS_SOFT_INT_MASK_1) ||
	    (ssir && (status & MIPS_SOFT_INT_MASK_1))) {

#define DO_SIR(bit, fn, ev)			       		\
	do {							\
		if (n & (bit)) {				\
			uvmexp.softs++;				\
			soft_evcnt[ev].ev_count++;		\
			fn;					\
		}						\
	} while (0)

		unsigned n;
		n = ssir; ssir = 0;
		_clrsoftintr(MIPS_SOFT_INT_MASK_1);

#if NZSC > 0
		DO_SIR(SIR_SERIAL, zssoft(), IPL_SOFTSERIAL);
#endif
		DO_SIR(SIR_NET, netintr(), IPL_SOFTNET);
#undef DO_SIR
	}

	/* 'softclock' interrupt */
	if (ipending & MIPS_SOFT_INT_MASK_0) {
		_clrsoftintr(MIPS_SOFT_INT_MASK_0);
		uvmexp.softs++;
		soft_evcnt[IPL_SOFTCLOCK].ev_count++;
		softclock();
	}
}

#ifdef EXEC_ECOFF
#include <sys/exec_ecoff.h>

int
cpu_exec_ecoff_hook(p, epp)
	struct proc *p;
	struct exec_package *epp;
{
	extern struct emul emul_netbsd;

	epp->ep_emul = &emul_netbsd;

	return 0;
}
#endif

/*
 * Console initialization: called early on from main,
 * before vm init or startup.  Do enough configuration
 * to choose and initialize a console.
 */

static void
null_cnprobe(cn)
     struct consdev *cn;
{
}

static void
prom_cninit(cn)
	struct consdev *cn;
{
	cn->cn_dev = makedev(0, 0);
	cn->cn_pri = CN_REMOTE;
}

static int
prom_cngetc(dev)
	dev_t dev;
{
	return prom_getchar();
}

static void
prom_cnputc(dev, c)
	dev_t dev;
	int c;
{
	prom_putchar(c);
}

static void
null_cnpollc(dev, on)
	dev_t dev;
	int on;
{
}

void
consinit()
{
  int zs_unit = 0;

  tty00_is_console = 1;

  zs_unit = 0;
  cn_tab = &consdev_zs;

  (*cn_tab->cn_init)(cn_tab);
}
