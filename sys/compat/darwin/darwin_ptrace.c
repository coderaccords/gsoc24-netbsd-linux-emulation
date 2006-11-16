/*	$NetBSD: darwin_ptrace.c,v 1.10 2006/11/16 01:32:42 christos Exp $ */

/*-
 * Copyright (c) 2003 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Emmanuel Dreyfus.
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
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
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

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: darwin_ptrace.c,v 1.10 2006/11/16 01:32:42 christos Exp $");

#include "opt_ptrace.h"

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mount.h>
#include <sys/proc.h>
#include <sys/ptrace.h>
#include <sys/sa.h>

#include <sys/syscall.h>
#include <sys/syscallargs.h>

#include <compat/sys/signal.h>

#include <compat/mach/mach_types.h>
#include <compat/mach/mach_vm.h>

#include <compat/darwin/darwin_exec.h>
#include <compat/darwin/darwin_audit.h>
#include <compat/darwin/darwin_ptrace.h>
#include <compat/darwin/darwin_syscallargs.h>

#define ISSET(t, f)     ((t) & (f))

int
darwin_sys_ptrace(l, v, retval)
	struct lwp *l;
	void *v;
	register_t *retval;
{
#if defined(PTRACE) || defined(_LKM)
	struct darwin_sys_ptrace_args /* {
		syscallarg(int) req;
		syscallarg(pid_t) pid;
		syscallarg(caddr_t) addr;
		syscallarg(int) data;
	} */ *uap = v;
	int req = SCARG(uap, req);
	struct proc *p = l->l_proc;
	struct darwin_emuldata *ded = NULL;
	struct proc *t;			/* target process */
	int error;

#ifdef _LKM
#define sys_ptrace (*sysent[SYS_ptrace].sy_call)
	if (sys_ptrace == sys_nosys)
		return ENOSYS;
#endif

	ded = (struct darwin_emuldata *)p->p_emuldata;

	switch (req) {
	case DARWIN_PT_ATTACHEXC:
		if ((t = pfind(SCARG(uap, pid))) == NULL)
			return ESRCH;

		if (t->p_emul != &emul_darwin)
			return ESRCH;
		ded = t->p_emuldata;

		if (ded->ded_flags & DARWIN_DED_SIGEXC)
			return EBUSY;

		ded->ded_flags |= DARWIN_DED_SIGEXC;

		SCARG(uap, req) = PT_ATTACH;
		if ((error = sys_ptrace(l, v, retval)) != 0)
			 ded->ded_flags &= ~DARWIN_DED_SIGEXC;

		return error;
		break;

	case DARWIN_PT_SIGEXC:
		if ((p->p_flag & P_TRACED) == 0)
			return EBUSY;

		ded->ded_flags |= DARWIN_DED_SIGEXC;
		break;

	case DARWIN_PT_DETACH: {
		int had_sigexc = 0;

		if ((t = pfind(SCARG(uap, pid))) == NULL)
			return (ESRCH);

		if ((t->p_emul == &emul_darwin) &&
		    (t->p_flag & P_TRACED) &&
		    (t->p_pptr == p)) {
			ded = t->p_emuldata;
			if (ded->ded_flags & DARWIN_DED_SIGEXC) {
				had_sigexc = 1;
				ded->ded_flags &= ~DARWIN_DED_SIGEXC;
			}
		}

		/*
		 * If the process is not marked as stopped,
		 * sys_ptrace sanity checks will return EBUSY.
		 */
		proc_stop(t, 0);

		if ((error = sys_ptrace(l, v, retval)) != 0) {
			proc_unstop(t);
			if (had_sigexc)
				ded->ded_flags |= DARWIN_DED_SIGEXC;
		}

		break;
	}

	case DARWIN_PT_THUPDATE: {
		int signo = SCARG(uap, data);

		if ((t = pfind(SCARG(uap, pid))) == NULL)
			return ESRCH;

		/* Checks from native ptrace */
		if (!ISSET(t->p_flag, P_TRACED))
			return EPERM;

		if (ISSET(t->p_flag, P_FSTRACE))
			return EBUSY;

		if (t->p_pptr != p)
			return EBUSY;

#if 0
		if (t->p_stat != SSTOP || !ISSET(t->p_flag, P_WAITED))
			return EBUSY;
#endif
		if ((signo < 0) || (signo > NSIG))
			return EINVAL;

		t->p_xstat = signo;
		if (signo != 0)
			sigaddset(&p->p_sigctx.ps_siglist, signo);

		break;
	}

	case DARWIN_PT_READ_U:
	case DARWIN_PT_WRITE_U:
	case DARWIN_PT_STEP:
	case DARWIN_PT_FORCEQUOTA:
	case DARWIN_PT_DENY_ATTACH:
		printf("darwin_sys_ptrace: unimplemented command %d\n", req);
		break;

	/* The other ptrace commands are the same on NetBSD */
	default:
		return sys_ptrace(l, v, retval);
		break;
	}

	return 0;
#else
	return ENOSYS;
#endif /* PTRACE || _LKM */
}

int
darwin_sys_kdebug_trace(struct lwp *l, void *v,
    register_t *retval)
{
	struct darwin_sys_kdebug_trace_args /* {
		syscallarg(int) debugid;
		syscallarg(int) arg1;
		syscallarg(int) arg2;
		syscallarg(int) arg3;
		syscallarg(int) arg4;
		syscallarg(int) arg5;
	} */ *uap = v;
	int args[4];
	char *str;

	args[0] = SCARG(uap, arg1);
	args[1] = SCARG(uap, arg2);
	args[2] = SCARG(uap, arg3);
	args[3] = 0;
	str = (char*)args;

#ifdef DEBUG_DARWIN
	printf("darwin_sys_kdebug_trace(%x, (%x %x %x)/\"%s\", %x, %x)\n",
	    SCARG(uap, debugid), SCARG(uap, arg1), SCARG(uap, arg2),
	    SCARG(uap, arg3), str, SCARG(uap, arg4), SCARG(uap, arg5));
#endif
	return 0;
}
