/*	$NetBSD: trap.c,v 1.41 1995/03/01 05:10:36 gwr Exp $	*/

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
#ifdef KTRACE
#include <sys/ktrace.h>
#endif

#include <vm/vm.h>
#include <vm/pmap.h>

#include <machine/cpu.h>
#include <machine/endian.h>
#include <machine/psl.h>
#include <machine/trap.h>
#include <machine/reg.h>

#ifdef COMPAT_SUNOS
#include <compat/sunos/sunos_syscall.h>
extern struct	sysent	sunos_sysent[];
extern int	nsunos_sysent;
#endif


/*
 * TODO:
 *        Chris's new syscall debug stuff 
 */

extern struct	sysent	sysent[];
extern int	nsysent;
extern int fubail(), subail();
extern int *nofault;

/* XXX - put these in some header file? */
extern vm_offset_t virtual_avail;
extern int pmap_fault_reload(pmap_t, vm_offset_t, vm_prot_t);

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
	FMT0SIZE,	/* type 0 - normal (68020/030/040) */
	FMT1SIZE,	/* type 1 - throwaway (68020/030/040) */
	FMT2SIZE,	/* type 2 - normal 6-word (68020/030/040) */
	-1,		/* type 3 - FP post-instruction (68040) */
	-1, -1, -1,	/* type 4-6 - undefined */
	-1,		/* type 7 - access error (68040) */
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
userret(p, fp, oticks)
	register struct proc *p;
	register struct frame *fp;
	u_quad_t oticks;
{
	int sig, s;

	/* take pending signals */
	while ((sig = CURSIG(p)) != 0)
		postsig(sig);

	p->p_priority = p->p_usrpri;

	if (want_resched) {
		/*
		 * Since we are curproc, clock will normally just change
		 * our priority without moving us from one queue to another
		 * (since the running process is not on a queue.)
		 * If that happened after we put ourselves on the run queue
		 * but before we mi_switch()'ed, we might not be on the queue
		 * indicated by our priority.
		 */
		s = splstatclock();
		setrunqueue(p);
		p->p_stats->p_ru.ru_nivcsw++;
		mi_switch();
		splx(s);
		while ((sig = CURSIG(p)) != 0)
			postsig(sig);
	}

	/*
	 * If profiling, charge system time to the trapped pc.
	 */
	if (p->p_flag & P_PROFIL) {
		extern int psratio;
		addupc_task(p, fp->f_pc,
					(int)(p->p_sticks - oticks) * psratio);
	}

	curpriority = p->p_priority;
}

/*
 * Trap is called from locore to handle most types of processor traps,
 * including events such as simulated software interrupts/AST's.
 * System calls are broken out for efficiency.
 */
/*ARGSUSED*/
trap(type, code, v, frame)
	int type;
	u_int code, v;
	struct frame frame;
{
	register struct proc *p;
	register int sig;
	u_int ucode;
	u_quad_t sticks;

	cnt.v_trap++;
	p = curproc;
	ucode = 0;
	sig = 0;

	/* I have verified that this DOES happen! -gwr */
	if (p == NULL)
		p = &proc0;

	if (USERMODE(frame.f_sr)) {
		type |= T_USER;
		sticks = p->p_sticks;
		p->p_md.md_regs = frame.f_regs;
	}

#ifdef DDB
	if (type == T_TRACE || type == T_BREAKPOINT) {
		(void)splhigh();	/* XXX - return will restore it */
		if (kdb_trap(type, &frame))
			return;
	}
#endif

	switch (type) {
	default:
	dopanic:
		type &= ~T_USER;
		printf("trap type=%d, code=0x%x, v=0x%x\n", type, code, v);
#ifdef	DDB
		if (kdb_trap(type, &frame))
			return;
#endif
		regdump(&frame, 128);
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
		frame.f_stackadj = exframesize[frame.f_format];
		frame.f_format = frame.f_vector = 0;
		frame.f_pc = (int) p->p_addr->u_pcb.pcb_onfault;
		return;

	case T_BUSERR|T_USER:	/* bus error */
	case T_ADDRERR|T_USER:	/* address error */
		ucode = v;
		sig = SIGBUS;
		break;

#ifdef	FPCOPROC
	case T_COPERR:		/* kernel coprocessor violation */
#endif	/* FPCOPROC */
	case T_FMTERR|T_USER:	/* do all RTE errors come in as T_USER? */
	case T_FMTERR:		/* ...just in case... */
		/*
		 * The user has most likely trashed the RTE or FP state info
		 * in the stack frame of a signal handler.
		 */
		printf("pid %d: kernel %s exception\n", p->p_pid,
		       type==T_COPERR ? "coprocessor" : "format");
		type |= T_USER;
		p->p_sigacts->ps_sigact[SIGILL] = SIG_DFL;
		/* temporary use of sig as mask */
		sig = sigmask(SIGILL);
		p->p_sigignore &= ~sig;
		p->p_sigcatch  &= ~sig;
		p->p_sigmask   &= ~sig;
		sig = SIGILL;	/* back to normal */
		ucode = frame.f_format;
		break;

#ifdef	FPCOPROC
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
#endif	/* FPCOPROC */

	case T_FPEMULI:		/* FPU faults in supervisor mode */
	case T_FPEMULD:
		if (nofault)	/* Doing FPU probe? */
			longjmp(nofault);
		goto dopanic;

	case T_FPEMULI|T_USER:	/* unimplemented FP instuction */
	case T_FPEMULD|T_USER:	/* unimplemented FP data type */
#ifdef	FPU_EMULATE
		sig = fpu_emulate(&frame, &p->p_addr->u_pcb.pcb_fpregs);
		/* XXX - Deal with tracing? (frame.f_sr & PSL_T) */
#else
		uprintf("pid %d killed: no floating point support\n", p->p_pid);
		sig = SIGILL;
#endif
		break;

	case T_ILLINST|T_USER:	/* illegal instruction fault */
	case T_PRIVINST|T_USER:	/* privileged instruction fault */
		ucode = frame.f_format;
		sig = SIGILL;
		break;

	case T_ZERODIV|T_USER:	/* Divide by zero */
	case T_CHKINST|T_USER:	/* CHK instruction trap */
	case T_TRAPVINST|T_USER:	/* TRAPV instruction trap */
		ucode = frame.f_format;
		sig = SIGFPE;
		break;

	/*
	 * XXX: Trace traps are a nightmare.
	 *
	 *	HP-UX uses trap #1 for breakpoints,
	 *	NetBSD/m68k uses trap #2,
	 *	SUN 3.x uses trap #15,
	 *	KGDB uses trap #15 (for kernel breakpoints; handled elsewhere).
	 *
	 * HPBSD and HP-UX traps both get mapped by locore.s into T_TRACE.
	 * SUN 3.x traps get passed through as T_TRAP15 and are not really
	 * supported yet.
	 */
	case T_TRACE:		/* kernel trace trap */
	case T_TRAP15:		/* SUN trace trap */
		frame.f_sr &= ~PSL_T;
		sig = SIGTRAP;
		break;

	case T_TRACE|T_USER:	/* user trace trap */
	case T_TRAP15|T_USER:	/* SUN user trace trap */
#ifdef COMPAT_SUNOS
		/*
		 * SunOS seems to use Trap #2 for some obscure fpu operations.
		 * So far, just ignore it, but DONT trap on it...
		 * (i.e. do not deliver a signal for it)
		 */
		if (p->p_emul == EMUL_SUNOS)
		    break;
#endif
		frame.f_sr &= ~PSL_T;
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
		printf("trap: T_MMUFLT pid=%d, code=%x, v=%x, pc=%x, sr=%x\n",
		       p->p_pid, code, v, frame.f_pc, frame.f_sr);
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
		va = trunc_page((vm_offset_t)v);

		/*
		 * Need to resolve the fault.  For user maps, we
		 * can often resolve the fault with a shortcut
		 * into the pmap module to just reload a PMEG.
		 * If that does not prodce a valid mapping,
		 * call the VM code as usual.
		 */
		if (map == kernel_map) {
			/* Do not allow faults outside the "managed" space. */
			if (va < virtual_avail) {
				if (p->p_addr->u_pcb.pcb_onfault) {
#ifdef	DEBUG
					if (mmudebug & MDB_CPFAULT) {
						printf("trap: copyfault kernel_map va < avail\n");
						Debugger();
					}
#endif
					goto copyfault;
				}
				goto dopanic;
			}
		} else {
			/* User map.  Try shortcut. */
			if (pmap_fault_reload(&vm->vm_pmap, va, ftype))
				goto finish;
		}

		/* OK, let the VM code handle the fault. */
		rv = vm_fault(map, va, ftype, FALSE);
#ifdef	DEBUG
		if (rv && MDB_ISPID(p->p_pid)) {
			printf("vm_fault(%x, %x, %x, 0) -> %x\n",
			       map, va, ftype, rv);
#ifdef	DDB
			if (mmudebug & MDB_WBFAILED)
				Debugger();
#endif	/* DDB */
		}
#endif	/* DEBUG */
#ifdef VMFAULT_TRACE
		printf("vm_fault(%x, %x, %x, 0) -> %x\n",
		       map, va, ftype, rv);
		printf("  type=%x, code=%x, pc=%x\n",
		       type, code, ((int *) frame.f_regs)[PC]);
#endif
		/*
		 * If this was a stack access we keep track of the maximum
		 * accessed stack size.  Also, if vm_fault gets a protection
		 * failure it is due to accessing the stack region outside
		 * the current limit and we need to reflect that as an access
		 * error.
		 */
		if ((caddr_t)va >= vm->vm_maxsaddr && map != kernel_map) {
			if (rv == KERN_SUCCESS) {
				unsigned nss;

				nss = clrnd(btoc(USRSTACK-(unsigned)va));
				if (nss > vm->vm_ssize)
					vm->vm_ssize = nss;
			} else if (rv == KERN_PROTECTION_FAILURE)
				rv = KERN_INVALID_ADDRESS;
		}
		if (rv == KERN_SUCCESS)
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
			printf("vm_fault(%x, %x, %x, 0) -> %x\n",
			       map, va, ftype, rv);
			goto dopanic;
		}
		ucode = v;
		sig = (rv == KERN_PROTECTION_FAILURE) ? SIGBUS : SIGSEGV;
		break;
	} /* T_MMUFLT */
	} /* switch */

finish:
	/* If trap was from supervisor mode, just return. */
	if ((type & T_USER) == 0)
		return;
	/* Post a signal if necessary. */
	if (sig != 0)
		trapsignal(p, sig, ucode);
douret:
	userret(p, &frame, sticks);
}

/*
 * Process a system call.
 */
syscall(code, frame)
	u_int code;
	struct frame frame;
{
	register caddr_t params;
	register struct sysent *callp;
	register struct proc *p;
	int error, opc, numsys;
	u_int argsize;
	u_quad_t sticks;
	int args[8];
	int rval[2];
	struct timeval syst;
	struct sysent *systab;

	if (!USERMODE(frame.f_sr))
		panic("syscall");

	cnt.v_syscall++;
	p = curproc;
	sticks = p->p_sticks;

	p->p_md.md_regs = frame.f_regs;
	p->p_md.md_flags &= ~MDP_STACKADJ;
	opc = frame.f_pc - 2;
	error = 0;

	switch (p->p_emul) {
#ifdef COMPAT_SUNOS
	case EMUL_SUNOS:
		systab = sunos_sysent;
		numsys = nsunos_sysent;
		/*
		 * SunOS passes the syscall-number on the stack, whereas
		 * BSD passes it in D0. So, we have to get the real "code"
		 * from the stack, and clean up the stack, as SunOS glue
		 * code assumes the kernel pops the syscall argument the
		 * glue pushed on the stack. Sigh...
		 */
		code = fuword ((caddr_t) frame.f_regs[SP]);

		/*
		 * XXX don't do this for sun_sigreturn, as there's no
		 * XXX stored pc on the stack to skip, the argument follows
		 * XXX the syscall number without a gap.
		 */
		if (code != SUNOS_SYS_sigreturn) {
			frame.f_regs[SP] += sizeof (int);
			/*
			 * remember that we adjusted the SP, might have to
			 * undo this if the system call returns ERESTART.
			 */
			p->p_md.md_flags |= MDP_STACKADJ;
		}
		break;
#endif
	case EMUL_NETBSD:
		systab = sysent;
		numsys = nsysent;
		break;
	default:
	    panic("syscall: bad syscall emulation type");
	}

	params = (caddr_t)frame.f_regs[SP] + sizeof(int);

	switch (code) {

	case SYS_syscall:
		/*
		 * Code is first argument, followed by actual args.
		 */
		code = fuword(params);
		params += sizeof(int);
		/*
		 * XXX sigreturn requires special stack manipulation
		 * that is only done if entered via the sigreturn
		 * trap.  Cannot allow it here so make sure we fail.
		 */
		if (code == SYS_sigreturn)
			code = numsys;
		break;

	case SYS___syscall:
		/*
		 * Like syscall, but code is a quad, so as to maintain
		 * quad alignment for the rest of the arguments.
		 */
		if (systab != sysent)
			break;
		code = fuword(params + _QUAD_LOWWORD * sizeof(int));
		params += sizeof(quad_t);
		break;

	default:
	}

	callp = systab;
	if (code < numsys)
		callp += code;
	else
		callp += SYS_syscall;		/* => nosys */

	argsize = callp->sy_argsize;
	if (argsize != 0)
		error = copyin(params, (caddr_t)args, argsize);

#ifdef KTRACE
	if (KTRPOINT(p, KTR_SYSCALL))
		ktrsyscall(p->p_tracep, code, callp->sy_narg, argsize, args);
#endif

	if (error)
		goto bad;

	rval[0] = 0;
	rval[1] = frame.f_regs[D1];

	/* OK, actualy do the system call... */
	error = (*callp->sy_call)(p, &args, rval);

	switch (error) {

	case 0:
		/*
		 * Reinitialize proc pointer `p' as it may be different
		 * if this is a child returning from fork syscall.
		 */
		p = curproc;
		frame.f_regs[D0] = rval[0];
		frame.f_regs[D1] = rval[1];
		frame.f_sr &= ~PSL_C;
		break;

	case ERESTART:
		/* The opc already points at the trap instruction. */
		frame.f_pc = opc;
		break;

	case EJUSTRETURN:
		break;

	default:
	bad:
#ifdef COMPAT_HPUX
		if (p->p_emul == EMUL_HPUX)
			error = bsdtohpuxerrno(error);
#endif
		frame.f_regs[D0] = error;
		frame.f_sr |= PSL_C;	/* carry bit */
		break;
	}

#ifdef COMPAT_SUNOS
	/* need new p-value for this */
	if (error == ERESTART && (p->p_md.md_flags & MDP_STACKADJ)) {
		frame.f_regs[SP] -= sizeof (int);
		p->p_md.md_flags &= ~MDP_STACKADJ;
	}
#endif

	userret(p, &frame, sticks, (u_int)0, 0);
#ifdef KTRACE
	if (KTRPOINT(p, KTR_SYSRET))
		ktrsysret(p->p_tracep, code, error, rval[0]);
#endif
}
