/*
 * Copyright (c) 1982, 1986, 1989 Regents of the University of California.
 * All rights reserved.
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
 *	from: @(#)kern_xxx.c	7.17 (Berkeley) 4/20/91
 *	$Id: kern_xxx.c,v 1.5 1993/06/27 06:01:49 andrew Exp $
 */

#include "param.h"
#include "systm.h"
#include "kernel.h"
#include "proc.h"
#include "reboot.h"
#include "utsname.h"

/* ARGSUSED */
int
gethostid(p, uap, retval)
	struct proc *p;
	void *uap;
	long *retval;
{

	*retval = hostid;
	return (0);
}

/* ARGSUSED */
int
sethostid(p, uap, retval)
	struct proc *p;
	struct args {
		long	hostid;
	} *uap;
	int *retval;
{
	int error;

	if (error = suser(p->p_ucred, &p->p_acflag))
		return (error);
	hostid = uap->hostid;
	return (0);
}

/* ARGSUSED */
int
gethostname(p, uap, retval)
	struct proc *p;
	struct args {
		char	*hostname;
		u_int	len;
	} *uap;
	int *retval;
{

	if (uap->len > hostnamelen + 1)
		uap->len = hostnamelen + 1;
	return (copyout((caddr_t)hostname, (caddr_t)uap->hostname, uap->len));
}

/* ARGSUSED */
int
sethostname(p, uap, retval)
	struct proc *p;
	register struct args {
		char	*hostname;
		u_int	len;
	} *uap;
	int *retval;
{
	int error;

	if (error = suser(p->p_ucred, &p->p_acflag))
		return (error);
	if (uap->len > sizeof (hostname) - 1)
		return (EINVAL);
	hostnamelen = uap->len;
	error = copyin((caddr_t)uap->hostname, hostname, uap->len);
	hostname[hostnamelen] = 0;
	return (error);
}

/* ARGSUSED */
int
getdomainname(p, uap, retval)
	struct proc *p;
	struct args {
		char	*domainname;
		u_int	len;
	} *uap;
	int *retval;
{
	if (uap->len > domainnamelen + 1)
		uap->len = domainnamelen + 1;
	return (copyout((caddr_t)domainname, (caddr_t)uap->domainname, uap->len));
}

/* ARGSUSED */
int
setdomainname(p, uap, retval)
	struct proc *p;
	struct args {
		char	*domainname;
		u_int	len;
	} *uap;
	int *retval;
{
	int error;

	if (error = suser(p->p_ucred, &p->p_acflag))
		return (error);
	if (uap->len > sizeof (domainname) - 1)
		return EINVAL;
	domainnamelen = uap->len;
	error = copyin((caddr_t)uap->domainname, domainname, uap->len);
	domainname[domainnamelen] = 0;
	return (error);
}

/* ARGSUSED */
int
uname(p, uap, retval)
	struct proc *p;
	struct args {
		struct utsname	*name;
	} *uap;
	int *retval;
{
	bcopy(hostname, utsname.nodename, sizeof(utsname.nodename));
	utsname.nodename[sizeof(utsname.nodename)-1] = '\0';
	return (copyout((caddr_t)&utsname, (caddr_t)uap->name, sizeof(struct utsname)));
}

/* ARGSUSED */
int
reboot(p, uap, retval)
	struct proc *p;
	struct args {
		int	opt;
	} *uap;
	int *retval;
{
	int error;

	if (error = suser(p->p_ucred, &p->p_acflag))
		return (error);
	boot(uap->opt);
	return (0);
}

#ifdef COMPAT_43
int
oquota()
{

	return (ENOSYS);
}
#endif
