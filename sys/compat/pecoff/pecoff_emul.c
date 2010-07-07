/*	$NetBSD: pecoff_emul.c,v 1.24 2010/07/07 01:30:36 chs Exp $	*/

/*
 * Copyright (c) 2000 Masaru OKI
 * Copyright (c) 1994, 1995, 1998 Scott Bartram
 * Copyright (c) 1994 Adam Glass
 * Copyright (c) 1993, 1994 Christopher G. Demetriou
 * All rights reserved.
 *
 * from compat/ibcs2/ibcs2_exec.c
 * originally from kern/exec_ecoff.c
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
 *      This product includes software developed by Scott Bartram.
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

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: pecoff_emul.c,v 1.24 2010/07/07 01:30:36 chs Exp $");

/*#define DEBUG_PECOFF*/

#ifdef _KERNEL_OPT
#include "opt_syscall_debug.h"
#endif

#include <sys/param.h>
#include <sys/proc.h>
#include <sys/malloc.h>
#include <sys/namei.h>
#include <sys/vnode.h>
#include <sys/mount.h>
#include <sys/exec.h>
#include <sys/syscall.h>
#include <sys/syscallvar.h>
#include <sys/signalvar.h>
#include <sys/resourcevar.h>
#include <sys/stat.h>

#include <uvm/uvm_extern.h>

#include <sys/exec_coff.h>
#include <machine/coff_machdep.h>

#include <compat/pecoff/pecoff_exec.h>
#include <compat/pecoff/pecoff_util.h>

extern struct sysent pecoff_sysent[];

#ifdef COMPAT_16
extern char sigcode[], esigcode[];
struct uvm_object *emul_pecoff_object;
#endif

#ifndef __HAVE_SYSCALL_INTERN
void	syscall(void);
#endif

struct emul emul_pecoff = {
	.e_name =		"pecoff",
	.e_path =		"/emul/pecoff",
#ifndef __HAVE_MINIMAL_EMUL
	.e_flags =		EMUL_HAS_SYS___syscall,
	.e_errno =		NULL
	.e_nosys =		SYS_syscall,
	.e_nsysent =		SYS_NSYSENT,
#endif
	.e_sysent =		sysent,
#ifdef SYSCALL_DEBUG
	.e_syscallnames =	syscallnames,
#endif
	.e_sendsig =		sendsig,
	.e_trapsignal =		trapsignal,
	.e_tracesig =		NULL,
#ifdef COMPAT_16
	.e_sigcode =		sigcode,
	.e_esigcode =		esigcode,
	.e_sigobject =		&emul_pecoff_object,
#endif
	.e_setregs =		setregs,
	.e_proc_exec =		NULL,
	.e_proc_fork =		NULL,
	.e_proc_exit =		NULL,
	.e_lwp_fork =		NULL,
	.e_lwp_exit =		NULL,
#ifdef __HAVE_SYSCALL_INTERN
	.e_syscall_intern =	syscall_intern,
#else
	.e_syscall_intern =	syscall,
#endif
	.e_sysctlovly =		NULL,
	.e_fault =		NULL,
	.e_vm_default_addr =	uvm_default_mapaddr,
	.e_usertrap =		NULL,
	.e_sa =			NULL,
	.e_ucsize =		sizeof(ucontext_t),
	.e_startlwp =		NULL
};
