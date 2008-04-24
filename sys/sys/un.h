/*	$NetBSD: un.h,v 1.43 2008/04/24 11:38:39 ad Exp $	*/

/*
 * Copyright (c) 1982, 1986, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
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
 *	@(#)un.h	8.3 (Berkeley) 2/19/95
 */

#ifndef _SYS_UN_H_
#define _SYS_UN_H_

#include <sys/ansi.h>
#include <sys/featuretest.h>
#include <sys/types.h>

#ifndef sa_family_t
typedef __sa_family_t	sa_family_t;
#define sa_family_t	__sa_family_t
#endif

/*
 * Definitions for UNIX IPC domain.
 */
struct	sockaddr_un {
	uint8_t		sun_len;	/* total sockaddr length */
	sa_family_t	sun_family;	/* AF_LOCAL */
	char		sun_path[104];	/* path name (gag) */
};

/*
 * Socket options for UNIX IPC domain.
 */
#if defined(_NETBSD_SOURCE)
#define	LOCAL_CREDS	0x0001		/* pass credentials to receiver */
#define	LOCAL_CONNWAIT	0x0002		/* connects block until accepted */
#define	LOCAL_PEEREID	0x0003		/* get peer identification */
#endif

/*
 * Data automatically stored inside connect() for use by LOCAL_PEEREID
 */
struct unpcbid {
	pid_t unp_pid;		/* process id */
	uid_t unp_euid;		/* effective user id */
	gid_t unp_egid;		/* effective group id */
};

#ifdef _KERNEL
struct unpcb;
struct socket;

int	uipc_usrreq(struct socket *, int, struct mbuf *,
	    struct mbuf *, struct mbuf *, struct lwp *);
int	uipc_ctloutput(int, struct socket *, int, int, struct mbuf **);
void	uipc_init (void);
kmutex_t *uipc_dgramlock (void);
kmutex_t *uipc_streamlock (void);
kmutex_t *uipc_rawlock (void);

int	unp_attach (struct socket *);
int	unp_bind (struct socket *, struct mbuf *, struct lwp *);
int	unp_connect (struct socket *, struct mbuf *, struct lwp *);
int	unp_connect2 (struct socket *, struct socket *, int);
void	unp_detach (struct unpcb *);
void	unp_discard (struct file *);
void	unp_disconnect (struct unpcb *);
bool	unp_drop (struct unpcb *, int);
void	unp_gc (void);
void	unp_mark (struct file *);
void	unp_scan (struct mbuf *, void (*)(struct file *), int);
void	unp_shutdown (struct unpcb *);
int 	unp_externalize (struct mbuf *, struct lwp *);
int	unp_internalize (struct mbuf **);
void 	unp_dispose (struct mbuf *);
int	unp_output (struct mbuf *, struct mbuf *, struct unpcb *,
	    struct lwp *);
void	unp_setaddr (struct socket *, struct mbuf *, bool);
#else /* !_KERNEL */

/* actual length of an initialized sockaddr_un */
#if defined(_NETBSD_SOURCE)
#define SUN_LEN(su) \
	(sizeof(*(su)) - sizeof((su)->sun_path) + strlen((su)->sun_path))
#endif /* !_NetBSD_SOURCE */
#endif /* _KERNEL */

#endif /* !_SYS_UN_H_ */
