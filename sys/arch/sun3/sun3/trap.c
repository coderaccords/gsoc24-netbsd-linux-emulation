/*	$NetBSD: trap.c,v 1.100 2001/05/30 15:24:39 lukem Exp $	*/

/*
 * Copyright (c) 1994 Gordon W. Ross
 * Copyright (c) 1993 Adam Glass
 * Copyright (c) 1988 University of Utah.
 * Copyright (c) 1982, 1986, 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * the Systems Programming Group of the University of Utah Computer
 * Science Department.
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
 *	from: Utah Hdr: trap.c 1.37 92/12/20
 *	from: @(#)trap.c	8.5 (Berkeley) 1/4/94
 */

#include "opt_ddb.h"
#include "opt_execfmt.h"
#include "opt_kgdb.h"
#include "opt_compat_aout_m68k.h"
#include "opt_compat_sunos.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/acct.h>
#include <sys/kernel.h>
#include <sys/signalvar.h>
#include <sys/resourcevar.h>
#include <sys/syscall.h>
#include <sys/syslog.h>
#include <sys/user.h>
#ifdef	KGDB
#include <sys/kgdb.h>
#endif

#include <uvm/uvm_extern.h>

#include <machine/cpu.h>
#include <machine/db_machdep.h>
#include <machine/endian.h>
#include <machine/psl.h>
#include <machine/trap.h>
#include <machine/reg.h>

#include <sun3/sun3/fc.h>
#include <sun3/sun3/machdep.h>

#ifdef COMPAT_SUNOS
#include <compat/sunos/sunos_syscall.h>
extern struct emul emul_sunos;
#endif

#ifdef COMPAT_AOUT_M68K
extern struct emul emul_netbsd_aoutm68k;
#endif

/*
 * The sun3 wants faults to go through the pmap code, but
 * the sun3x just goes directly to the common VM code.
 */
#ifdef	_SUN3X_
# define _pmap_fault(map, va, ftype) \
	uvm_fault(map, va, 0, ftype)
#endif	/* SUN3X */

/* Special labels in m68k/copy.s */
extern char fubail[], subail[];

/* These are called from locore.s */
void trap __P((int type, u_int code, u_int v, struct trapframe));
void trap_kdebug __P((int type, struct trapframe tf));
int _nodb_trap __P((int type, struct trapframe *));
void straytrap __P((struct trapframe));

static void userret __P((struct proc *, struct trapframe *, u_quad_t));

int astpending;
int want_resched;

char	*trap_type[] = {
	"Bus error",
	"Address error",
	"Illegal instruction",
	"Zero divide",
	"CHK instruction",
	"TRAPV instruction",
	"Privilege violation",
	"Trace trap",
	"MMU fault",
	"SSIR trap",
	"Format error",
	"68881 exception",
	"Coprocessor violation",
	"Async system trap",
	"Unused? (14)",
	"Breakpoint",
	"FPU instruction",
	"FPU data format",
};
u_int trap_types = sizeof(trap_type) / sizeof(trap_type[0]);

/*
 * Size of various exception stack frames (minus the standard 8 bytes)
 */
short	exframesize[] = {
	FMT0SIZE,	/* type 0 - normal (68020/030/040/060) */
	FMT1SIZE,	/* type 1 - throwaway (68020/030/040) */
	FMT2SIZE,	/* type 2 - normal 6-word (68020/030/040/060) */
	FMT3SIZE,	/* type 3 - FP post-instruction (68040/060) */
	FMT4SIZE,	/* type 4 - access error/fp disabled (68060) */
	-1, -1, 	/* type 5-6 - undefined */
	FMT7SIZE,	/* type 7 - access error (68040) */
	58,		/* type 8 - bus fault (68010) */
	FMT9SIZE,	/* type 9 - coprocessor mid-instruction (68020/030) */
	FMTASIZE,	/* type A - short bus fault (68020/030) */
	FMTBSIZE,	/* type B - long bus fault (68020/030) */
	-1, -1, -1, -1	/* type C-F - undefined */
};

#define KDFAULT(c)	(((c) & (SSW_DF|SSW_FCMASK)) == (SSW_DF|FC_SUPERD))
#define WRFAULT(c)	(((c) & (SSW_DF|SSW_RW)) == SSW_DF)

/* #define	DEBUG XXX */

#ifdef DEBUG
int mmudebug = 0;
int mmupid = -1;
#define MDB_ISPID(p)	((p) == mmupid)
#define MDB_FOLLOW	1
#define MDB_WBFOLLOW	2
#define MDB_WBFAILED	4
#define MDB_CPFAULT 	8
#endif

/*
 * trap and syscall both need the following work done before
 * returning to user mode.
 */
static void
userret(p, tf, oticks)
	register struct proc *p;
	register struct trapframe *tf;
	u_quad_t oticks;
{
	int sig;

	/* take pending signals */
	while ((sig = CURSIG(p)) != 0)
		postsig(sig);

	p->p_priority = p->p_usrpri;

	if (want_resched) {
		/*
		 * We are being preempted.
		 */
		preempt(NULL);
		while ((sig = CURSIG(p)) != 0)
			postsig(sig);
	}

	/*
	 * If profiling, charge system time to the trapped pc.
	 */
	if (p->p_flag & P_PROFIL) {
		extern int psratio;
		addupc_task(p, tf->tf_pc,
		            (int)(p->p_sticks - oticks) * psratio);
	}

	curcpu()->ci_schedstate.spc_curpriority = p->p_priority;
}

/*
 * Used by the common m68k syscall() and child_return() functions.
 * XXX: Temporary until all m68k ports share common trap()/userret() code.
 */
void machine_userret(struct proc *, struct frame *, u_quad_t);

void
machine_userret(p, f, t)
	struct proc *p;
	struct frame *f;
	u_quad_t t;
{

	userret(p, &f->F_t, t);
}

/*
 * Trap is called from locore to handle most types of processor traps,
 * including events such as simulated software interrupts/AST's.
 * System calls are broken out for efficiency.
 */
/*ARGSUSED*/
void
trap(type, code, v, tf)
	int type;
	u_int code, v;
	struct trapframe tf;
{
	register struct proc *p;
	register int sig, tmp;
	u_int ucode;
	u_quad_t sticks;

	uvmexp.traps++;
	p = curproc;
	ucode = 0;
	sig = 0;

	/* I have verified that this DOES happen! -gwr */
	if (p == NULL)
		p = &proc0;
#ifdef	DIAGNOSTIC
	if (p->p_addr == NULL)
		panic("trap: no pcb");
#endif

	if (USERMODE(tf.tf_sr)) {
		type |= T_USER;
		sticks = p->p_sticks;
		p->p_md.md_regs = tf.tf_regs;
	} else {
		sticks = 0;
		/* XXX: Detect trap recursion? */
	}

	switch (type) {
	default:
	dopanic:
		printf("trap type=0x%x, code=0x%x, v=0x%x\n", type, code, v);
		/*
		 * Let the kernel debugger see the trap frame that
		 * caused us to panic.  This is a convenience so
		 * one can see registers at the point of failure.
		 */
		tmp = splhigh();
#ifdef KGDB
		/* If connected, step or cont returns 1 */
		if (kgdb_trap(type, &tf))
			goto kgdb_cont;
#endif
#ifdef	DDB
		(void) kdb_trap(type, (db_regs_t *) &tf);
#endif
#ifdef KGDB
	kgdb_cont:
#endif
		splx(tmp);
		if (panicstr) {
			/*
			 * Note: panic is smart enough to do:
			 *   boot(RB_AUTOBOOT | RB_NOSYNC, NULL)
			 * if we call it again.
			 */
			panic("trap during panic!");
		}
		regdump(&tf, 128);
		type &= ~T_USER;
		if ((u_int)type < trap_types)
			panic(trap_type[type]);
		panic("trap type 0x%x", type);

	case T_BUSERR:		/* kernel bus error */
		if (p->p_addr->u_pcb.pcb_onfault == NULL)
			goto dopanic;
		/*FALLTHROUGH*/

	copyfault:
		/*
		 * If we have arranged to catch this fault in any of the
		 * copy to/from user space routines, set PC to return to
		 * indicated location and set flag informing buserror code
		 * that it may need to clean up stack frame.
		 */
		tf.tf_stackadj = exframesize[tf.tf_format];
		tf.tf_format = tf.tf_vector = 0;
		tf.tf_pc = (int) p->p_addr->u_pcb.pcb_onfault;
		goto done;

	case T_BUSERR|T_USER:	/* bus error */
	case T_ADDRERR|T_USER:	/* address error */
		ucode = v;
		sig = SIGBUS;
		break;

	case T_COPERR:		/* kernel coprocessor violation */
	case T_FMTERR|T_USER:	/* do all RTE errors come in as T_USER? */
	case T_FMTERR:		/* ...just in case... */
		/*
		 * The user has most likely trashed the RTE or FP state info
		 * in the stack frame of a signal handler.
		 */
		printf("pid %d: kernel %s exception\n", p->p_pid,
		       type==T_COPERR ? "coprocessor" : "format");
		type |= T_USER;
		SIGACTION(p, SIGILL).sa_handler = SIG_DFL;
		sigdelset(&p->p_sigctx.ps_sigignore, SIGILL);
		sigdelset(&p->p_sigctx.ps_sigcatch, SIGILL);
		sigdelset(&p->p_sigctx.ps_sigmask, SIGILL);
		sig = SIGILL;
		ucode = tf.tf_format;
		break;

	case T_COPERR|T_USER:	/* user coprocessor violation */
		/* What is a proper response here? */
		ucode = 0;
		sig = SIGFPE;
		break;

	case T_FPERR|T_USER:	/* 68881 exceptions */
		/*
		 * We pass along the 68881 status register which locore stashed
		 * in code for us.  Note that there is a possibility that the
		 * bit pattern of this register will conflict with one of the
		 * FPE_* codes defined in signal.h.  Fortunately for us, the
		 * only such codes we use are all in the range 1-7 and the low
		 * 3 bits of the status register are defined as 0 so there is
		 * no clash.
		 */
		ucode = code;
		sig = SIGFPE;
		break;

	case T_FPEMULI:		/* FPU faults in supervisor mode */
	case T_FPEMULD:
		if (nofault)	/* Doing FPU probe? */
			longjmp(nofault);
		goto dopanic;

	case T_FPEMULI|T_USER:	/* unimplemented FP instuction */
	case T_FPEMULD|T_USER:	/* unimplemented FP data type */
#ifdef	FPU_EMULATE
		sig = fpu_emulate(&tf, &p->p_addr->u_pcb.pcb_fpregs);
		/* XXX - Deal with tracing? (tf.tf_sr & PSL_T) */
#else
		uprintf("pid %d killed: no floating point support\n", p->p_pid);
		sig = SIGILL;
#endif
		break;

	case T_ILLINST|T_USER:	/* illegal instruction fault */
	case T_PRIVINST|T_USER:	/* privileged instruction fault */
		ucode = tf.tf_format;
		sig = SIGILL;
		break;

	case T_ZERODIV|T_USER:	/* Divide by zero */
	case T_CHKINST|T_USER:	/* CHK instruction trap */
	case T_TRAPVINST|T_USER:	/* TRAPV instruction trap */
		ucode = tf.tf_format;
		sig = SIGFPE;
		break;

	/*
	 * XXX: Trace traps are a nightmare.
	 *
	 *	HP-UX uses trap #1 for breakpoints,
	 *	NetBSD/m68k uses trap #2,
	 *	SUN 3.x uses trap #15,
	 *	DDB and KGDB uses trap #15 (for kernel breakpoints;
	 *	handled elsewhere).
	 *
	 * NetBSD and HP-UX traps both get mapped by locore.s into T_TRACE.
	 * SUN 3.x traps get passed through as T_TRAP15 and are not really
	 * supported yet.
	 *
	 * XXX: We should never get kernel-mode T_TRAP15
	 * XXX: because locore.s now gives them special treatment.
	 */
	case T_TRAP15:		/* kernel breakpoint */
		tf.tf_sr &= ~PSL_T;
		goto done;

	case T_TRACE|T_USER:	/* user trace trap */
#ifdef COMPAT_SUNOS
		/*
		 * SunOS uses Trap #2 for a "CPU cache flush"
		 * Just flush the on-chip caches and return.
		 * XXX - Too bad NetBSD uses trap 2...
		 */
		if (p->p_emul == &emul_sunos) {
			ICIA();
			DCIU();
			/* get out fast */
			goto done;
		}
#endif
		/* FALLTHROUGH */
	case T_TRACE:		/* tracing a trap instruction */
	case T_TRAP15|T_USER:	/* SUN user trace trap */
		tf.tf_sr &= ~PSL_T;
		sig = SIGTRAP;
		break;

	case T_ASTFLT:		/* system async trap, cannot happen */
		goto dopanic;

	case T_ASTFLT|T_USER:	/* user async trap */
		astpending = 0;
		/* T_SSIR is not used on a Sun3. */
		if (p->p_flag & P_OWEUPC) {
			p->p_flag &= ~P_OWEUPC;
			ADDUPROF(p);
		}
		goto douret;

	case T_MMUFLT:		/* kernel mode page fault */
		/* Hacks to avoid calling VM code from debugger. */
#ifdef	DDB
		if (db_recover != 0)
			goto dopanic;
#endif
#ifdef	KGDB
		if (kgdb_recover != 0)
			goto dopanic;
#endif
		/*
		 * If we were doing profiling ticks or other user mode
		 * stuff from interrupt code, Just Say No.
		 */
		if (p->p_addr->u_pcb.pcb_onfault == (caddr_t)fubail ||
		    p->p_addr->u_pcb.pcb_onfault == (caddr_t)subail)
		{
#ifdef	DEBUG
			if (mmudebug & MDB_CPFAULT) {
				printf("trap: copyfault fu/su bail\n");
				Debugger();
			}
#endif
			goto copyfault;
		}
		/*FALLTHROUGH*/

	case T_MMUFLT|T_USER: { 	/* page fault */
		register vm_offset_t va;
		register struct vmspace *vm = p->p_vmspace;
		register vm_map_t map;
		int rv;
		vm_prot_t ftype;
		extern vm_map_t kernel_map;

#ifdef DEBUG
		if ((mmudebug & MDB_WBFOLLOW) || MDB_ISPID(p->p_pid))
		printf("trap: T_MMUFLT pid=%d, code=0x%x, v=0x%x, pc=0x%x, sr=0x%x\n",
		       p->p_pid, code, v, tf.tf_pc, tf.tf_sr);
#endif

		/*
		 * It is only a kernel address space fault iff:
		 * 	1. (type & T_USER) == 0  and: (2 or 3)
		 * 	2. pcb_onfault not set or
		 *	3. pcb_onfault set but supervisor space data fault
		 * The last can occur during an exec() copyin where the
		 * argument space is lazy-allocated.
		 */
		map = &vm->vm_map;
		if ((type & T_USER) == 0) {
			/* supervisor mode fault */
			if ((p->p_addr->u_pcb.pcb_onfault == NULL) || KDFAULT(code))
				map = kernel_map;
		}

		if (WRFAULT(code))
			ftype = VM_PROT_READ | VM_PROT_WRITE;
		else
			ftype = VM_PROT_READ;
		va = m68k_trunc_page((vm_offset_t)v);

		/*
		 * Need to resolve the fault.
		 *
		 * We give the pmap code a chance to resolve faults by
		 * reloading translations that it was forced to unload.
		 * This function does that, and calls vm_fault if it
		 * could not resolve the fault by reloading the MMU.
		 * This function may also, for example, disallow any
		 * faults in the kernel text segment, etc.
		 */
		rv = _pmap_fault(map, va, ftype);

#ifdef	DEBUG
		if (rv && MDB_ISPID(p->p_pid)) {
			printf("vm_fault(%p, 0x%lx, 0x%x, 0) -> 0x%x\n",
			       map, va, ftype, rv);
			if (mmudebug & MDB_WBFAILED)
				Debugger();
		}
#endif	/* DEBUG */

		/*
		 * If this was a stack access we keep track of the maximum
		 * accessed stack size.  Also, if vm_fault gets a protection
		 * failure it is due to accessing the stack region outside
		 * the current limit and we need to reflect that as an access
		 * error.
		 */
		if ((map != kernel_map) && ((caddr_t)va >= vm->vm_maxsaddr)) {
			if (rv == 0) {
				unsigned nss;

				nss = btoc((u_int)(USRSTACK-va));
				if (nss > vm->vm_ssize)
					vm->vm_ssize = nss;
			} else if (rv == EACCES)
				rv = EFAULT;
		}
		if (rv == 0)
			goto finish;

		if ((type & T_USER) == 0) {
			/* supervisor mode fault */
			if (p->p_addr->u_pcb.pcb_onfault) {
#ifdef	DEBUG
				if (mmudebug & MDB_CPFAULT) {
					printf("trap: copyfault pcb_onfault\n");
					Debugger();
				}
#endif
				goto copyfault;
			}
			printf("vm_fault(%p, 0x%lx, 0x%x, 0) -> 0x%x\n",
			       map, va, ftype, rv);
			goto dopanic;
		}
		ucode = v;
		if (rv == ENOMEM) {
			printf("UVM: pid %d (%s), uid %d killed: out of swap\n",
			       p->p_pid, p->p_comm,
			       p->p_cred && p->p_ucred ?
			       p->p_ucred->cr_uid : -1);
			sig = SIGKILL;
		} else {
			sig = SIGSEGV;
		}
		break;
		} /* T_MMUFLT */
	} /* switch */

finish:
	/* If trap was from supervisor mode, just return. */
	if ((type & T_USER) == 0)
		goto done;
	/* Post a signal if necessary. */
	if (sig != 0)
		trapsignal(p, sig, ucode);
douret:
	userret(p, &tf, sticks);

done:;
	/* XXX: Detect trap recursion? */
}

/*
 * This is used if we hit a kernel breakpoint or trace trap
 * when there is no debugger installed (or not attached).
 * Drop into the PROM temporarily...
 */
int
_nodb_trap(type, tf)
	int type;
	struct trapframe *tf;
{

	printf("\r\nKernel ");
	if ((0 <= type) && (type < trap_types))
		printf("%s", trap_type[type]);
	else
		printf("trap 0x%x", type);
	printf(", frame=%p\r\n", tf);
	printf("No debugger; doing PROM abort.\r\n");
	printf("To continue, type: c <RETURN>\r\n");
	sunmon_abort();
	/* OK then, just resume... */
	tf->tf_sr &= ~PSL_T;
	return(1);
}

/*
 * This is called by locore for supervisor-mode trace and
 * breakpoint traps.  This is separate from trap() above
 * so that breakpoints in trap() will work.
 *
 * If we have both DDB and KGDB, let KGDB see it first,
 * because KGDB will just return 0 if not connected.
 */
void
trap_kdebug(type, tf)
	int type;
	struct trapframe tf;
{

#ifdef	KGDB
	/* Let KGDB handle it (if connected) */
	if (kgdb_trap(type, &tf))
		return;
#endif
#ifdef	DDB
	/* Let DDB handle it. */
	if (kdb_trap(type, &tf))
		return;
#endif

	/* Drop into the PROM temporarily... */
	(void)_nodb_trap(type, &tf);
}

/*
 * Called by locore.s for an unexpected interrupt.
 * XXX - Almost identical to trap_kdebug...
 */
void
straytrap(tf)
	struct trapframe tf;
{
	int type = -1;

	printf("unexpected trap; vector=0x%x at pc=0x%x\n",
		tf.tf_vector, tf.tf_pc);

#ifdef	KGDB
	/* Let KGDB handle it (if connected) */
	if (kgdb_trap(type, &tf))
		return;
#endif
#ifdef	DDB
	/* Let DDB handle it. */
	if (kdb_trap(type, &tf))
		return;
#endif

	/* Drop into the PROM temporarily... */
	(void)_nodb_trap(type, &tf);
}
