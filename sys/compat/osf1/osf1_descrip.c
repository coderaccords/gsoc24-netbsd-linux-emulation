/* $NetBSD: osf1_descrip.c,v 1.1 1999/05/01 05:34:59 cgd Exp $ */

/*
 * Copyright (c) 1999 Christopher G. Demetriou.  All rights reserved.
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
 *      This product includes software developed by Christopher G. Demetriou
 *	for the NetBSD Project.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Copyright (c) 1994, 1995 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: Chris G. Demetriou
 * 
 * Permission to use, copy, modify and distribute this software and
 * its documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS" 
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND 
 * FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 * 
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/namei.h>
#include <sys/proc.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/filedesc.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/mman.h>
#include <sys/mount.h>
#include <sys/signal.h>
#include <sys/signalvar.h>
#include <sys/reboot.h>
#include <sys/syscallargs.h>
#include <sys/exec.h>
#include <sys/vnode.h>
#include <sys/socketvar.h>
#include <sys/resource.h>
#include <sys/resourcevar.h>
#include <sys/wait.h>
#include <vm/vm.h>

#include <compat/osf1/osf1.h>
#include <compat/osf1/osf1_syscallargs.h>
#include <compat/osf1/osf1_cvt.h>

/*
 * Return status information about a file descriptor.
 */
int
osf1_sys_fstat(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct osf1_sys_fstat_args *uap = v;
	struct filedesc *fdp = p->p_fd;
	struct file *fp;
	struct stat ub;
	struct osf1_stat oub;
	int error;

	if ((unsigned)SCARG(uap, fd) >= fdp->fd_nfiles ||
	    (fp = fdp->fd_ofiles[SCARG(uap, fd)]) == NULL)
		return (EBADF);
	switch (fp->f_type) {

	case DTYPE_VNODE:
		error = vn_stat((struct vnode *)fp->f_data, &ub, p);
		break;

	case DTYPE_SOCKET:
		error = soo_stat((struct socket *)fp->f_data, &ub);
		break;

	default:
		panic("ofstat");
		/*NOTREACHED*/
	}
	osf1_cvt_stat_from_native(&ub, &oub);
	if (error == 0)
		error = copyout((caddr_t)&oub, (caddr_t)SCARG(uap, sb),
		    sizeof (oub));
	return (error);
}

int
osf1_sys_fcntl(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct osf1_sys_fcntl_args *uap = v;
	struct sys_fcntl_args a;
	unsigned long xfl, leftovers;
	int error;

	SCARG(&a, fd) = SCARG(uap, fd);

	leftovers = 0;
	switch (SCARG(uap, cmd)) {
	case OSF1_F_DUPFD:
		SCARG(&a, cmd) = F_DUPFD;
		SCARG(&a, arg) = SCARG(uap, arg);
		break;

	case OSF1_F_GETFD:
		SCARG(&a, cmd) = F_GETFD;
		SCARG(&a, arg) = 0;		/* ignored */
		break;

	case OSF1_F_SETFD:
		SCARG(&a, cmd) = F_SETFD;
		SCARG(&a, arg) = (void *)emul_flags_translate(
		    osf1_fcntl_getsetfd_flags_xtab,
		    (unsigned long)SCARG(uap, arg), &leftovers);
		break;

	case OSF1_F_GETFL:
		SCARG(&a, cmd) = F_GETFL;
		SCARG(&a, arg) = 0;		/* ignored */
		break;

	case OSF1_F_SETFL:
		SCARG(&a, cmd) = F_SETFL;
		xfl = emul_flags_translate(osf1_open_flags_xtab,
		    (unsigned long)SCARG(uap, arg), &leftovers);
		xfl |= emul_flags_translate(osf1_fcntl_getsetfl_flags_xtab,
		    leftovers, &leftovers);
		SCARG(&a, arg) = (void *)xfl;
		break;

	case OSF1_F_GETOWN:		/* XXX not yet supported */
	case OSF1_F_SETOWN:		/* XXX not yet supported */
	case OSF1_F_GETLK:		/* XXX not yet supported */
	case OSF1_F_SETLK:		/* XXX not yet supported */
	case OSF1_F_SETLKW:		/* XXX not yet supported */
		/* XXX translate. */
		return (EINVAL);
		
	case OSF1_F_RGETLK:		/* [lock mgr op] XXX not supported */
	case OSF1_F_RSETLK:		/* [lock mgr op] XXX not supported */
	case OSF1_F_CNVT:		/* [lock mgr op] XXX not supported */
	case OSF1_F_RSETLKW:		/* [lock mgr op] XXX not supported */
	case OSF1_F_PURGEFS:		/* [lock mgr op] XXX not supported */
	case OSF1_F_PURGENFS:		/* [DECsafe op] XXX not supported */
	default:
		/* XXX syslog? */
		return (EINVAL);
	}
	if (leftovers != 0)
		return (EINVAL);

	error = sys_fcntl(p, &a, retval);

	if (error)
		return error;

	switch (SCARG(uap, cmd)) {
	case OSF1_F_GETFD:
		retval[0] = emul_flags_translate(
		    osf1_fcntl_getsetfd_flags_rxtab, retval[0], NULL);
		break;

	case OSF1_F_GETFL:
		xfl = emul_flags_translate(osf1_open_flags_rxtab,
		    retval[0], &leftovers);
		xfl |= emul_flags_translate(osf1_fcntl_getsetfl_flags_rxtab,
		    leftovers, NULL);
		retval[0] = xfl;
		break;
	}

	return error;
}

int
osf1_sys_lseek(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct osf1_sys_lseek_args *uap = v;
	struct sys_lseek_args a;

	SCARG(&a, fd) = SCARG(uap, fd);
	SCARG(&a, pad) = 0;
	SCARG(&a, offset) = SCARG(uap, offset);
	SCARG(&a, whence) = SCARG(uap, whence);

	return sys_lseek(p, &a, retval);
}

int
osf1_sys_ftruncate(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct osf1_sys_ftruncate_args *uap = v;
	struct sys_ftruncate_args a;

	SCARG(&a, fd) = SCARG(uap, fd);
	SCARG(&a, pad) = 0;
	SCARG(&a, length) = SCARG(uap, length);

	return sys_ftruncate(p, &a, retval);
}
