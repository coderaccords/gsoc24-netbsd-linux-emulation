/*	$NetBSD: linux_machdep.c,v 1.6 2001/03/18 11:31:44 manu Exp $ */

/*-
 * Copyright (c) 1995, 2000, 2001 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Frank van der Linden and Emmanuel Dreyfus.
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/signalvar.h>
#include <sys/kernel.h>
#include <sys/map.h>
#include <sys/proc.h>
#include <sys/user.h>
#include <sys/buf.h>
#include <sys/reboot.h>
#include <sys/conf.h>
#include <sys/exec.h>
#include <sys/file.h>
#include <sys/callout.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/msgbuf.h>
#include <sys/mount.h>
#include <sys/vnode.h>
#include <sys/device.h>
#include <sys/syscallargs.h>
#include <sys/filedesc.h>
#include <sys/exec_elf.h>
#include <sys/disklabel.h>
#include <sys/ioctl.h>
#include <miscfs/specfs/specdev.h>

#include <compat/linux/common/linux_types.h>
#include <compat/linux/common/linux_signal.h>
#include <compat/linux/common/linux_util.h>
#include <compat/linux/common/linux_ioctl.h>
#include <compat/linux/common/linux_hdio.h>
#include <compat/linux/common/linux_exec.h>
#include <compat/linux/common/linux_machdep.h>

#include <compat/linux/linux_syscallargs.h>

#include <machine/cpu.h>
#include <machine/psl.h>
#include <machine/reg.h>
#include <machine/vmparam.h>

/*
 * To see whether wscons is configured (for virtual console ioctl calls).
 */
#if defined(_KERNEL) && !defined(_LKM)
#include "wsdisplay.h"
#endif
#if (NWSDISPLAY > 0)
#include <dev/wscons/wsconsio.h>
#include <dev/wscons/wsdisplay_usl_io.h>
#if defined(_KERNEL) && !defined(_LKM)
#endif
#endif

/* 
 * Set set up registers on exec.
 * XXX not used at the moment since in sys/kern/exec_conf, LINUX_COMPAT
 * entry uses NetBSD's native setregs instead of linux_setregs
 */
void
linux_setregs(p, pack, stack) 
	struct proc *p;
	struct exec_package *pack;
	u_long stack;
{	
	setregs(p, pack, stack);
}

/*
 * Send an interrupt to process.
 *
 * Adapted from arch/powerpc/powerpc/sig_machdep.c:sendsig and
 * compat/linux/arch/i386/linux_machdep.c:linux_sendsig
 *
 * XXX Does not work well yet with RT signals
 *
 */

void
linux_sendsig(catcher, sig, mask, code)  /* XXX Check me */
	sig_t catcher;
	int sig;
	sigset_t *mask;
	u_long code;
{
	struct proc *p = curproc;
	struct trapframe *tf;
	struct linux_sigregs *fp, frame;
	struct linux_pt_regs linux_regs;
	struct linux_sigcontext sc;
	int onstack;
	int i;

	tf = trapframe(p);
 
	/* 
	 * Do we need to jump onto the signal stack? 
	 */
	onstack =
	    (p->p_sigctx.ps_sigstk.ss_flags & (SS_DISABLE | SS_ONSTACK)) == 0 &&
	    (SIGACTION(p, sig).sa_flags & SA_ONSTACK) != 0;

	/*
	 * Signal stack is broken (see at the end of linux_sigreturn), so we do
	 * not use it yet. XXX fix this.
	 */
	onstack=0;

	/* 
	 * Allocate space for the signal handler context. 
	 */
	if (onstack) {
		fp = (struct linux_sigregs *)
		    ((caddr_t)p->p_sigctx.ps_sigstk.ss_sp +
		    p->p_sigctx.ps_sigstk.ss_size);
	} else {
		fp = (struct linux_sigregs *)tf->fixreg[1];
	}
	fp = (struct linux_sigregs *)((int)(fp - 1) & ~0xf); 

	/* 
	 * Prepare a sigcontext for later.
	 */
	sc.lsignal = (int)native_to_linux_sig[sig];
	sc.lhandler = (unsigned long)catcher;
	native_to_linux_old_sigset(mask, &sc.lmask);
	sc.lregs = (struct linux_pt_regs*)fp;

	/*
	 * Setup the signal stack frame as Linux does it in 
	 * arch/ppc/kernel/signal.c:setup_frame()
	 *
	 * Save register context. 
	 */
	for (i = 0; i < 32; i++) 
		linux_regs.lgpr[i] = tf->fixreg[i];
	linux_regs.lnip = tf->srr0;	
	linux_regs.lmsr = tf->srr1;
	linux_regs.lorig_gpr3 = tf->fixreg[3]; /* XXX Is that right? */
	linux_regs.lctr = tf->ctr;
	linux_regs.llink = tf->lr;
	linux_regs.lxer = tf->xer;
	linux_regs.lccr = tf->cr; 
	linux_regs.lmq = 0;  			/* Unused, 601 only */
	linux_regs.ltrap = 0; 	/* XXX What is ltrap counterpart in NetBSD ? */
	linux_regs.ldar = tf->dar; 
	linux_regs.ldsisr = tf->dsisr; 
	linux_regs.lresult = tf->exc; 
	memcpy(&frame.lgp_regs, &linux_regs, sizeof(frame.lgp_regs));

	/* 
	 * NetBSD does not uses the FPU in the kernel, so there is no
	 * need to save floating point register. However, Linux expects
	 * them to be saved on the stack. Therefore we just keep a 
	 * gap of zero'ed data where the FP registers should be stored
	 */
	memset(&frame.lfp_regs, 0, sizeof (frame.lfp_regs));

	/*
	 * Copy Linux's signal trampoline on the user stack It should not
	 * be used, but Linux binaries might expect it to be there.
	 */
	frame.ltramp[0] = 0x38997777; /* li r0, 0x7777 */
	frame.ltramp[1] = 0x44000002; /* sc */
	
	/*
	 * Move it to the user stack
	 * There is a little trick here, about the LINUX_ABIGAP: the 
	 * linux_sigreg structure has a 56 int gap to support rs6000/xcoff
	 * binaries. But the Linux kernel seems to do without it, and it
	 * just skip it when building the stack frame. Hence the LINUX_ABIGAP.
	 */
	if (copyout(&frame, fp, sizeof (frame) - LINUX_ABIGAP) != 0) { 
		/*
		 * Process has trashed its stack; give it an illegal
		 * instructoin to halt it in its tracks.
		 */
		sigexit(p, SIGILL);
		/* NOTREACHED */
	}

	/*
	 * adjust stack pointer after the previous data copy
	 */
	fp = (struct linux_sigregs *)
	    ((unsigned long)fp - (sizeof (frame) - LINUX_ABIGAP));

	/*
	 * "Mind the gap" Linux expects a gap here.
	 */
	fp = (struct linux_sigregs *)
	    ((unsigned long)fp - LINUX__SIGNAL_FRAMESIZE);

	/*
	 * Add a sigcontext on the stack
	 */
	if (copyout(&sc, fp, sizeof (struct linux_sigcontext)) != 0) {
		/*
		 * Process has trashed its stack; give it an illegal
		 * instructoin to halt it in its tracks.
		 */
		sigexit(p, SIGILL);
		/* NOTREACHED */
	}

	/* 
	 * Here, I expected to need a stack pointer adjust after the copy.
	 * Something like this: (unsigned long)fp-=sizeof(struct sigcontext)
	 * But if we do it, the signal handler does not get its arguments as
	 * expected. 
	 */
	 
	/*
	 * Set the registers according to how the Linux process expects them
	 */
	tf->fixreg[1] = (int)fp;
	tf->lr = (int)catcher;
	tf->fixreg[3] = (int)sig;
	tf->fixreg[4] = (int)&fp->lgp_regs;
	tf->srr0 = (int)p->p_sigctx.ps_sigcode;

	/* 
	 * Remember that we're now on the signal stack. 
	 */
	if (onstack)
		p->p_sigctx.ps_sigstk.ss_flags |= SS_ONSTACK;
#ifdef DEBUG_LINUX
	printf("linux_sendsig: exitting. fp=0x%lx\n",(long)fp);
#endif
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
 *
 * XXX not tested
 */
int
linux_sys_rt_sigreturn(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct linux_sys_rt_sigreturn_args /* {
		syscallarg(struct linux_rt_sigframe *) sfp;
	} */ *uap = v;
	struct linux_rt_sigframe *scp, sigframe;
	struct trapframe *tf;
	sigset_t mask;
	int i;

	/*
	 * The trampoline code hands us the context.
	 * It is unsafe to keep track of it ourselves, in the event that a
	 * program jumps out of a signal handler.
	 */
	scp = SCARG(uap, sfp);

	/*
	 * It seems we need a 16 bytes alignement here (it just works with it,
	 * don't ask me why
	 */
	scp = (struct linux_rt_sigframe *)((unsigned long)scp & ~0xfUL); 

	/*
	 * Get the context from user stack
	 */
	if (copyin((caddr_t)scp, &sigframe, sizeof(*scp)) != 0)
		return (EFAULT);

	/*
	 * Grab the signal mask
	 */
	linux_to_native_sigset(&sigframe.luc.luc_sigmask, &mask);

	/*
	 *  Restore register context. XXX need security review 
	 */
	tf = trapframe(p);
#ifdef DEBUG_LINUX
	printf("linux_sys_sigreturn: trapframe=0x%lx scp=0x%lx\n",
	    (unsigned long)tf, (unsigned long)scp);
#endif

	if ((sigframe.luc.luc_context.lregs->lmsr & PSL_USERSTATIC) != 
	    (tf->srr1 & PSL_USERSTATIC))
		return (EINVAL);  

	for (i = 0; i < 32; i++) 
		tf->fixreg[i] = sigframe.luc.luc_context.lregs->lgpr[i];
	tf->lr = sigframe.luc.luc_context.lregs->llink;
	tf->cr = sigframe.luc.luc_context.lregs->lccr;
	tf->xer = sigframe.luc.luc_context.lregs->lxer;
	tf->ctr = sigframe.luc.luc_context.lregs->lctr;
	tf->srr0 = sigframe.luc.luc_context.lregs->lnip;
	tf->srr1 = sigframe.luc.luc_context.lregs->lmsr;
	tf->dar = sigframe.luc.luc_context.lregs->ldar;
	tf->dsisr = sigframe.luc.luc_context.lregs->ldsisr;
	tf->exc = sigframe.luc.luc_context.lregs->lresult;

	/* 
	 * Restore signal stack. 
	 * 
	 * XXX cannot find the onstack information in Linux sig context. 
	 * Is signal stack really supported on Linux?
	 * 
	 * It seems to be supported in libc6...
	 */
	/* if (sc.sc_onstack & SS_ONSTACK) 
		p->p_sigctx.ps_sigstk.ss_flags |= SS_ONSTACK;
	else */
		p->p_sigctx.ps_sigstk.ss_flags &= ~SS_ONSTACK;

	return (EJUSTRETURN);
}


/*
 * The following needs code review for potential security issues
 */
int
linux_sys_sigreturn(p, v, retval)  
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct linux_sys_sigreturn_args /* {
		syscallarg(struct linux_sigcontext *) scp;
	} */ *uap = v;
	struct linux_sigcontext *scp, context;
	struct trapframe *tf;
	sigset_t mask;
	int i;

	/*
	 * The trampoline code hands us the context.
	 * It is unsafe to keep track of it ourselves, in the event that a
	 * program jumps out of a signal handler.
	 */
	scp = SCARG(uap, scp);

	/*
	 * It seems we need a 16 bytes alignement here (it just works with it,
	 * don't ask me why
	 */
	(unsigned long)scp = (unsigned long) scp & ~0xfUL; 

	/*
	 * Get the context from user stack
	 */
	if (copyin(scp, &context, sizeof(struct linux_sigcontext)) != 0)
		return (EFAULT);

	/*
	 *  Restore register context. XXX need security review 
	 */
	tf = trapframe(p);
#ifdef DEBUG_LINUX
	printf("linux_sys_sigreturn: trapframe=0x%lx scp=0x%lx\n",
	    (unsigned long)tf, (unsigned long)scp);
#endif

	if ((context.lregs->lmsr & PSL_USERSTATIC) !=
	    (tf->srr1 & PSL_USERSTATIC))
		return (EINVAL);  

	for (i = 0; i < 32; i++) 
		tf->fixreg[i] = context.lregs->lgpr[i];
	tf->lr = context.lregs->llink;
	tf->cr = context.lregs->lccr;
	tf->xer = context.lregs->lxer;
	tf->ctr = context.lregs->lctr;
	tf->srr0 = context.lregs->lnip;
	tf->srr1 = context.lregs->lmsr;
	tf->dar = context.lregs->ldar;
	tf->dsisr = context.lregs->ldsisr;
	tf->exc = context.lregs->lresult;

	/* 
	 * Restore signal stack. 
	 * 
	 * XXX cannot find the onstack information in Linux sig context. 
	 * Is signal stack really supported on Linux?
	 */
#if 0
	if (sc.sc_onstack & SS_ONSTACK) 
		p->p_sigctx.ps_sigstk.ss_flags |= SS_ONSTACK;
	else
#endif
		p->p_sigctx.ps_sigstk.ss_flags &= ~SS_ONSTACK;

	/* Restore signal mask. */
	linux_old_to_native_sigset(&context.lmask, &mask); 
	(void) sigprocmask1(p, SIG_SETMASK, &mask, 0);

	return (EJUSTRETURN);
}


int
linux_sys_modify_ldt(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	/* 
	 * This syscall is not implemented in Linux/PowerPC: we should not
	 * be here
	 */
#ifdef DEBUG_LINUX
	printf("linux_sys_modify_ldt: should not be here.\n");	 
#endif
  return 0;
}

/* 
 * major device numbers remapping
 */
dev_t
linux_fakedev(dev)
	dev_t dev;
{
  /* XXX write me */
  return dev;
}

/*
 * We come here in a last attempt to satisfy a Linux ioctl() call
 */
int
linux_machdepioctl(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{ 
	struct linux_sys_ioctl_args /* {
		syscallarg(int) fd;
		syscallarg(u_long) com;
		syscallarg(caddr_t) data;
	} */ *uap = v;
	struct sys_ioctl_args bia;
	u_long com;
	
	SCARG(&bia, fd) = SCARG(uap, fd);
	SCARG(&bia, data) = SCARG(uap, data);
	com = SCARG(uap, com);
	
	switch (com) {
	default:
		printf("linux_machdepioctl: invalid ioctl %08lx\n", com);
		return EINVAL;
	}
	SCARG(&bia, com) = com;
	return sys_ioctl(p, &bia, retval);
}
/*
 * Set I/O permissions for a process. Just set the maximum level
 * right away (ignoring the argument), otherwise we would have
 * to rely on I/O permission maps, which are not implemented.
 */
int
linux_sys_iopl(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	/* 
	 * This syscall is not implemented in Linux/PowerPC: we should not be here
	 */
#ifdef DEBUG_LINUX
	printf("linux_sys_iopl: should not be here.\n");
#endif
	return 0;
}

/*
 * See above. If a root process tries to set access to an I/O port,
 * just let it have the whole range.
 */
int
linux_sys_ioperm(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	/* 
	 * This syscall is not implemented in Linux/PowerPC: we should not be here
	 */
#ifdef DEBUG_LINUX
	printf("linux_sys_ioperm: should not be here.\n");	 
#endif
	return 0;
}

/*
 * wrapper linux_sys_new_uname() -> linux_sys_uname() 
 */
int	
linux_sys_new_uname(p, v, retval) 
	struct proc *p;
	void *v;
	register_t *retval;
{
	return linux_sys_uname(p, v, retval); 
}

/*
 * wrapper linux_sys_new_select() -> linux_sys_select() 
 */
int	
linux_sys_new_select(p, v, retval) 
	struct proc *p;
	void *v;
	register_t *retval;
{
	return linux_sys_select(p, v, retval); 
}
