/*	$NetBSD: uipc_syscalls.c,v 1.129 2008/04/24 11:38:36 ad Exp $	*/

/*-
 * Copyright (c) 2008 The NetBSD Foundation, Inc.
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

/*
 * Copyright (c) 1982, 1986, 1989, 1990, 1993
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
 *	@(#)uipc_syscalls.c	8.6 (Berkeley) 2/14/95
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: uipc_syscalls.c,v 1.129 2008/04/24 11:38:36 ad Exp $");

#include "opt_pipe.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/filedesc.h>
#include <sys/proc.h>
#include <sys/file.h>
#include <sys/buf.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/signalvar.h>
#include <sys/un.h>
#include <sys/ktrace.h>
#include <sys/event.h>

#include <sys/mount.h>
#include <sys/syscallargs.h>

#include <uvm/uvm_extern.h>

/*
 * System call interface to the socket abstraction.
 */
extern const struct fileops socketops;

int
sys___socket30(struct lwp *l, const struct sys___socket30_args *uap, register_t *retval)
{
	/* {
		syscallarg(int)	domain;
		syscallarg(int)	type;
		syscallarg(int)	protocol;
	} */
	int		fd, error;

	error = fsocreate(SCARG(uap, domain), NULL, SCARG(uap, type),
			 SCARG(uap, protocol), l, &fd);
	if (error == 0)
		*retval = fd;
	return error;
}

/* ARGSUSED */
int
sys_bind(struct lwp *l, const struct sys_bind_args *uap, register_t *retval)
{
	/* {
		syscallarg(int)				s;
		syscallarg(const struct sockaddr *)	name;
		syscallarg(unsigned int)		namelen;
	} */
	struct mbuf	*nam;
	int		error;

	error = sockargs(&nam, SCARG(uap, name), SCARG(uap, namelen),
	    MT_SONAME);
	if (error)
		return error;

	return do_sys_bind(l, SCARG(uap, s), nam);
}

int
do_sys_bind(struct lwp *l, int fd, struct mbuf *nam)
{
	struct socket	*so;
	int		error;

	if ((error = fd_getsock(fd, &so)) != 0) {
		m_freem(nam);
		return (error);
	}
	MCLAIM(nam, so->so_mowner);
	error = sobind(so, nam, l);
	m_freem(nam);
	fd_putfile(fd);
	return error;
}

/* ARGSUSED */
int
sys_listen(struct lwp *l, const struct sys_listen_args *uap, register_t *retval)
{
	/* {
		syscallarg(int)	s;
		syscallarg(int)	backlog;
	} */
	struct socket	*so;
	int		error;

	if ((error = fd_getsock(SCARG(uap, s), &so)) != 0)
		return (error);
	error = solisten(so, SCARG(uap, backlog), l);
	fd_putfile(SCARG(uap, s));
	return error;
}

int
do_sys_accept(struct lwp *l, int sock, struct mbuf **name, register_t *new_sock)
{
	file_t		*fp, *fp2;
	struct mbuf	*nam;
	int		error, fd;
	struct socket	*so, *so2;

	if ((fp = fd_getfile(sock)) == NULL)
		return (EBADF);
	if (fp->f_type != DTYPE_SOCKET)
		return (ENOTSOCK);
	if ((error = fd_allocfile(&fp2, &fd)) != 0)
		return (error);
	nam = m_get(M_WAIT, MT_SONAME);
	*new_sock = fd;
	so = fp->f_data;
	solock(so);
	if (!(so->so_proto->pr_flags & PR_LISTEN)) {
		error = EOPNOTSUPP;
		goto bad;
	}
	if ((so->so_options & SO_ACCEPTCONN) == 0) {
		error = EINVAL;
		goto bad;
	}
	if (so->so_nbio && so->so_qlen == 0) {
		error = EWOULDBLOCK;
		goto bad;
	}
	while (so->so_qlen == 0 && so->so_error == 0) {
		if (so->so_state & SS_CANTRCVMORE) {
			so->so_error = ECONNABORTED;
			break;
		}
		error = sowait(so, 0);
		if (error) {
			goto bad;
		}
	}
	if (so->so_error) {
		error = so->so_error;
		so->so_error = 0;
		goto bad;
	}
	/* connection has been removed from the listen queue */
	KNOTE(&so->so_rcv.sb_sel.sel_klist, NOTE_SUBMIT);
	so2 = TAILQ_FIRST(&so->so_q);
	if (soqremque(so2, 1) == 0)
		panic("accept");
	fp2->f_type = DTYPE_SOCKET;
	fp2->f_flag = fp->f_flag;
	fp2->f_ops = &socketops;
	fp2->f_data = so2;
	error = soaccept(so2, nam);
	sounlock(so);
	if (error) {
		/* an error occurred, free the file descriptor and mbuf */
		m_freem(nam);
		mutex_enter(&fp2->f_lock);
		fp2->f_count++;
		mutex_exit(&fp2->f_lock);
		closef(fp2);
		fd_abort(curproc, NULL, fd);
	} else {
		fd_affix(curproc, fp2, fd);
		*name = nam;
	}
	fd_putfile(sock);
	return (error);
 bad:
 	sounlock(so);
 	m_freem(nam);
	fd_putfile(sock);
 	fd_abort(curproc, fp2, fd);
 	return (error);
}

int
sys_accept(struct lwp *l, const struct sys_accept_args *uap, register_t *retval)
{
	/* {
		syscallarg(int)			s;
		syscallarg(struct sockaddr *)	name;
		syscallarg(unsigned int *)	anamelen;
	} */
	int error, fd;
	struct mbuf *name;

	error = do_sys_accept(l, SCARG(uap, s), &name, retval);
	if (error != 0)
		return error;
	error = copyout_sockname(SCARG(uap, name), SCARG(uap, anamelen),
	    MSG_LENUSRSPACE, name);
	if (name != NULL)
		m_free(name);
	if (error != 0) {
		fd = (int)*retval;
		if (fd_getfile(fd) != NULL)
			(void)fd_close(fd);
	}
	return error;
}

/* ARGSUSED */
int
sys_connect(struct lwp *l, const struct sys_connect_args *uap, register_t *retval)
{
	/* {
		syscallarg(int)				s;
		syscallarg(const struct sockaddr *)	name;
		syscallarg(unsigned int)		namelen;
	} */
	int		error;
	struct mbuf	*nam;

	error = sockargs(&nam, SCARG(uap, name), SCARG(uap, namelen),
	    MT_SONAME);
	if (error)
		return error;
	return do_sys_connect(l,  SCARG(uap, s), nam);
}

int
do_sys_connect(struct lwp *l, int fd, struct mbuf *nam)
{
	struct socket	*so;
	int		error;
	int		interrupted = 0;

	if ((error = fd_getsock(fd, &so)) != 0) {
		m_freem(nam);
		return (error);
	}
	solock(so);
	MCLAIM(nam, so->so_mowner);
	if (so->so_state & SS_ISCONNECTING) {
		error = EALREADY;
		goto out;
	}

	error = soconnect(so, nam, l);
	if (error)
		goto bad;
	if (so->so_nbio && (so->so_state & SS_ISCONNECTING)) {
		error = EINPROGRESS;
		goto out;
	}
	while ((so->so_state & SS_ISCONNECTING) && so->so_error == 0) {
		error = sowait(so, 0);
		if (error) {
			if (error == EINTR || error == ERESTART)
				interrupted = 1;
			break;
		}
	}
	if (error == 0) {
		error = so->so_error;
		so->so_error = 0;
	}
 bad:
	if (!interrupted)
		so->so_state &= ~SS_ISCONNECTING;
	if (error == ERESTART)
		error = EINTR;
 out:
 	sounlock(so);
 	fd_putfile(fd);
	m_freem(nam);
	return (error);
}

int
sys_socketpair(struct lwp *l, const struct sys_socketpair_args *uap, register_t *retval)
{
	/* {
		syscallarg(int)		domain;
		syscallarg(int)		type;
		syscallarg(int)		protocol;
		syscallarg(int *)	rsv;
	} */
	file_t		*fp1, *fp2;
	struct socket	*so1, *so2;
	int		fd, error, sv[2];
	proc_t		*p;

	p = curproc;
	error = socreate(SCARG(uap, domain), &so1, SCARG(uap, type),
	    SCARG(uap, protocol), l, NULL);
	if (error)
		return (error);
	error = socreate(SCARG(uap, domain), &so2, SCARG(uap, type),
	    SCARG(uap, protocol), l, so1);
	if (error)
		goto free1;
	if ((error = fd_allocfile(&fp1, &fd)) != 0)
		goto free2;
	sv[0] = fd;
	fp1->f_flag = FREAD|FWRITE;
	fp1->f_type = DTYPE_SOCKET;
	fp1->f_ops = &socketops;
	fp1->f_data = so1;
	if ((error = fd_allocfile(&fp2, &fd)) != 0)
		goto free3;
	fp2->f_flag = FREAD|FWRITE;
	fp2->f_type = DTYPE_SOCKET;
	fp2->f_ops = &socketops;
	fp2->f_data = so2;
	sv[1] = fd;
	solock(so1);
	error = soconnect2(so1, so2);
	if (error == 0 && SCARG(uap, type) == SOCK_DGRAM) {
		/*
		 * Datagram socket connection is asymmetric.
		 */
		error = soconnect2(so2, so1);
	}
	sounlock(so1);
	if (error == 0)
		error = copyout(sv, SCARG(uap, rsv), 2 * sizeof(int));
	if (error == 0) {
		fd_affix(p, fp2, sv[1]);
		fd_affix(p, fp1, sv[0]);
		return (0);
	}
	fd_abort(p, fp2, sv[1]);
 free3:
	fd_abort(p, fp1, sv[0]);
 free2:
	(void)soclose(so2);
 free1:
	(void)soclose(so1);
	return (error);
}

int
sys_sendto(struct lwp *l, const struct sys_sendto_args *uap, register_t *retval)
{
	/* {
		syscallarg(int)				s;
		syscallarg(const void *)		buf;
		syscallarg(size_t)			len;
		syscallarg(int)				flags;
		syscallarg(const struct sockaddr *)	to;
		syscallarg(unsigned int)		tolen;
	} */
	struct msghdr	msg;
	struct iovec	aiov;

	msg.msg_name = __UNCONST(SCARG(uap, to)); /* XXXUNCONST kills const */
	msg.msg_namelen = SCARG(uap, tolen);
	msg.msg_iov = &aiov;
	msg.msg_iovlen = 1;
	msg.msg_control = NULL;
	msg.msg_flags = 0;
	aiov.iov_base = __UNCONST(SCARG(uap, buf)); /* XXXUNCONST kills const */
	aiov.iov_len = SCARG(uap, len);
	return do_sys_sendmsg(l, SCARG(uap, s), &msg, SCARG(uap, flags), retval);
}

int
sys_sendmsg(struct lwp *l, const struct sys_sendmsg_args *uap, register_t *retval)
{
	/* {
		syscallarg(int)				s;
		syscallarg(const struct msghdr *)	msg;
		syscallarg(int)				flags;
	} */
	struct msghdr	msg;
	int		error;

	error = copyin(SCARG(uap, msg), &msg, sizeof(msg));
	if (error)
		return (error);

	msg.msg_flags = MSG_IOVUSRSPACE;
	return do_sys_sendmsg(l, SCARG(uap, s), &msg, SCARG(uap, flags), retval);
}

int
do_sys_sendmsg(struct lwp *l, int s, struct msghdr *mp, int flags,
		register_t *retsize)
{
	struct uio	auio;
	int		i, len, error, iovlen;
	struct mbuf	*to, *control;
	struct socket	*so;
	struct iovec	*tiov;
	struct iovec	aiov[UIO_SMALLIOV], *iov = aiov;
	struct iovec	*ktriov = NULL;

	ktrkuser("msghdr", mp, sizeof *mp);

	/* If the caller passed us stuff in mbufs, we must free them */
	if (mp->msg_flags & MSG_NAMEMBUF)
		to = mp->msg_name;
	else
		to = NULL;

	if (mp->msg_flags & MSG_CONTROLMBUF)
		control = mp->msg_control;
	else
		control = NULL;

	if (mp->msg_flags & MSG_IOVUSRSPACE) {
		if ((unsigned int)mp->msg_iovlen > UIO_SMALLIOV) {
			if ((unsigned int)mp->msg_iovlen > IOV_MAX) {
				error = EMSGSIZE;
				goto bad;
			}
			iov = malloc(sizeof(struct iovec) * mp->msg_iovlen,
			    M_IOV, M_WAITOK);
		}
		if (mp->msg_iovlen != 0) {
			error = copyin(mp->msg_iov, iov,
			    (size_t)(mp->msg_iovlen * sizeof(struct iovec)));
			if (error)
				goto bad;
		}
		mp->msg_iov = iov;
	}

	auio.uio_iov = mp->msg_iov;
	auio.uio_iovcnt = mp->msg_iovlen;
	auio.uio_rw = UIO_WRITE;
	auio.uio_offset = 0;			/* XXX */
	auio.uio_resid = 0;
	KASSERT(l == curlwp);
	auio.uio_vmspace = l->l_proc->p_vmspace;

	for (i = 0, tiov = mp->msg_iov; i < mp->msg_iovlen; i++, tiov++) {
#if 0
		/* cannot happen; iov_len is unsigned */
		if (tiov->iov_len < 0) {
			error = EINVAL;
			goto bad;
		}
#endif
		/*
		 * Writes return ssize_t because -1 is returned on error.
		 * Therefore, we must restrict the length to SSIZE_MAX to
		 * avoid garbage return values.
		 */
		auio.uio_resid += tiov->iov_len;
		if (tiov->iov_len > SSIZE_MAX || auio.uio_resid > SSIZE_MAX) {
			error = EINVAL;
			goto bad;
		}
	}

	if (mp->msg_name && to == NULL) {
		error = sockargs(&to, mp->msg_name, mp->msg_namelen,
		    MT_SONAME);
		if (error)
			goto bad;
	}

	if (mp->msg_control) {
		if (mp->msg_controllen < CMSG_ALIGN(sizeof(struct cmsghdr))) {
			error = EINVAL;
			goto bad;
		}
		if (control == NULL) {
			error = sockargs(&control, mp->msg_control,
			    mp->msg_controllen, MT_CONTROL);
			if (error)
				goto bad;
		}
	}

	if (ktrpoint(KTR_GENIO)) {
		iovlen = auio.uio_iovcnt * sizeof(struct iovec);
		ktriov = malloc(iovlen, M_TEMP, M_WAITOK);
		memcpy(ktriov, auio.uio_iov, iovlen);
	}

	if ((error = fd_getsock(s, &so)) != 0)
		goto bad;

	if (mp->msg_name)
		MCLAIM(to, so->so_mowner);
	if (mp->msg_control)
		MCLAIM(control, so->so_mowner);

	len = auio.uio_resid;
	error = (*so->so_send)(so, to, &auio, NULL, control, flags, l);
	/* Protocol is responsible for freeing 'control' */
	control = NULL;

	fd_putfile(s);

	if (error) {
		if (auio.uio_resid != len && (error == ERESTART ||
		    error == EINTR || error == EWOULDBLOCK))
			error = 0;
		if (error == EPIPE && (flags & MSG_NOSIGNAL) == 0) {
			mutex_enter(&proclist_mutex);
			psignal(l->l_proc, SIGPIPE);
			mutex_exit(&proclist_mutex);
		}
	}
	if (error == 0)
		*retsize = len - auio.uio_resid;

bad:
	if (ktriov != NULL) {
		ktrgeniov(s, UIO_WRITE, ktriov, *retsize, error);
		free(ktriov, M_TEMP);
	}

 	if (iov != aiov)
		free(iov, M_IOV);
	if (to)
		m_freem(to);
	if (control)
		m_freem(control);

	return (error);
}

int
sys_recvfrom(struct lwp *l, const struct sys_recvfrom_args *uap, register_t *retval)
{
	/* {
		syscallarg(int)			s;
		syscallarg(void *)		buf;
		syscallarg(size_t)		len;
		syscallarg(int)			flags;
		syscallarg(struct sockaddr *)	from;
		syscallarg(unsigned int *)	fromlenaddr;
	} */
	struct msghdr	msg;
	struct iovec	aiov;
	int		error;
	struct mbuf	*from;

	msg.msg_name = NULL;
	msg.msg_iov = &aiov;
	msg.msg_iovlen = 1;
	aiov.iov_base = SCARG(uap, buf);
	aiov.iov_len = SCARG(uap, len);
	msg.msg_control = NULL;
	msg.msg_flags = SCARG(uap, flags) & MSG_USERFLAGS;

	error = do_sys_recvmsg(l, SCARG(uap, s), &msg, &from, NULL, retval);
	if (error != 0)
		return error;

	error = copyout_sockname(SCARG(uap, from), SCARG(uap, fromlenaddr),
	    MSG_LENUSRSPACE, from);
	if (from != NULL)
		m_free(from);
	return error;
}

int
sys_recvmsg(struct lwp *l, const struct sys_recvmsg_args *uap, register_t *retval)
{
	/* {
		syscallarg(int)			s;
		syscallarg(struct msghdr *)	msg;
		syscallarg(int)			flags;
	} */
	struct msghdr	msg;
	int		error;
	struct mbuf	*from, *control;

	error = copyin(SCARG(uap, msg), &msg, sizeof(msg));
	if (error)
		return (error);

	msg.msg_flags = (SCARG(uap, flags) & MSG_USERFLAGS) | MSG_IOVUSRSPACE;

	error = do_sys_recvmsg(l, SCARG(uap, s), &msg, &from,
	    msg.msg_control != NULL ? &control : NULL, retval);
	if (error != 0)
		return error;

	if (msg.msg_control != NULL)
		error = copyout_msg_control(l, &msg, control);

	if (error == 0)
		error = copyout_sockname(msg.msg_name, &msg.msg_namelen, 0,
			from);
	if (from != NULL)
		m_free(from);
	if (error == 0) {
		ktrkuser("msghdr", &msg, sizeof msg);
		error = copyout(&msg, SCARG(uap, msg), sizeof(msg));
	}

	return (error);
}

/*
 * Adjust for a truncated SCM_RIGHTS control message.
 *  This means closing any file descriptors that aren't present
 *  in the returned buffer.
 *  m is the mbuf holding the (already externalized) SCM_RIGHTS message.
 */
static void
free_rights(struct mbuf *m)
{
	int nfd;
	int i;
	int *fdv;

	nfd = m->m_len < CMSG_SPACE(sizeof(int)) ? 0
	    : (m->m_len - CMSG_SPACE(sizeof(int))) / sizeof(int) + 1;
	fdv = (int *) CMSG_DATA(mtod(m,struct cmsghdr *));
	for (i = 0; i < nfd; i++) {
		if (fd_getfile(fdv[i]) != NULL)
			(void)fd_close(fdv[i]);
	}
}

void
free_control_mbuf(struct lwp *l, struct mbuf *control, struct mbuf *uncopied)
{
	struct mbuf *next;
	struct cmsghdr *cmsg;
	bool do_free_rights = false;

	while (control != NULL) {
		cmsg = mtod(control, struct cmsghdr *);
		if (control == uncopied)
			do_free_rights = true;
		if (do_free_rights && cmsg->cmsg_level == SOL_SOCKET
		    && cmsg->cmsg_type == SCM_RIGHTS)
			free_rights(control);
		next = control->m_next;
		m_free(control);
		control = next;
	}
}

/* Copy socket control/CMSG data to user buffer, frees the mbuf */
int
copyout_msg_control(struct lwp *l, struct msghdr *mp, struct mbuf *control)
{
	int i, len, error = 0;
	struct cmsghdr *cmsg;
	struct mbuf *m;
	char *q;

	len = mp->msg_controllen;
	if (len <= 0 || control == 0) {
		mp->msg_controllen = 0;
		free_control_mbuf(l, control, control);
		return 0;
	}

	q = (char *)mp->msg_control;

	for (m = control; m != NULL; ) {
		cmsg = mtod(m, struct cmsghdr *);
		i = m->m_len;
		if (len < i) {
			mp->msg_flags |= MSG_CTRUNC;
			if (cmsg->cmsg_level == SOL_SOCKET
			    && cmsg->cmsg_type == SCM_RIGHTS)
				/* Do not truncate me ... */
				break;
			i = len;
		}
		error = copyout(mtod(m, void *), q, i);
		ktrkuser("msgcontrol", mtod(m, void *), i);
		if (error != 0) {
			/* We must free all the SCM_RIGHTS */
			m = control;
			break;
		}
		m = m->m_next;
		if (m)
			i = ALIGN(i);
		q += i;
		len -= i;
		if (len <= 0)
			break;
	}

	free_control_mbuf(l, control, m);

	mp->msg_controllen = q - (char *)mp->msg_control;
	return error;
}

int
do_sys_recvmsg(struct lwp *l, int s, struct msghdr *mp, struct mbuf **from,
    struct mbuf **control, register_t *retsize)
{
	struct uio	auio;
	struct iovec	aiov[UIO_SMALLIOV], *iov = aiov;
	struct iovec	*tiov;
	int		i, len, error, iovlen;
	struct socket	*so;
	struct iovec	*ktriov;

	ktrkuser("msghdr", mp, sizeof *mp);

	*from = NULL;
	if (control != NULL)
		*control = NULL;

	if ((error = fd_getsock(s, &so)) != 0)
		return (error);

	if (mp->msg_flags & MSG_IOVUSRSPACE) {
		if ((unsigned int)mp->msg_iovlen > UIO_SMALLIOV) {
			if ((unsigned int)mp->msg_iovlen > IOV_MAX) {
				error = EMSGSIZE;
				goto out;
			}
			iov = malloc(sizeof(struct iovec) * mp->msg_iovlen,
			    M_IOV, M_WAITOK);
		}
		if (mp->msg_iovlen != 0) {
			error = copyin(mp->msg_iov, iov,
			    (size_t)(mp->msg_iovlen * sizeof(struct iovec)));
			if (error)
				goto out;
		}
		auio.uio_iov = iov;
	} else
		auio.uio_iov = mp->msg_iov;
	auio.uio_iovcnt = mp->msg_iovlen;
	auio.uio_rw = UIO_READ;
	auio.uio_offset = 0;			/* XXX */
	auio.uio_resid = 0;
	KASSERT(l == curlwp);
	auio.uio_vmspace = l->l_proc->p_vmspace;

	tiov = auio.uio_iov;
	for (i = 0; i < mp->msg_iovlen; i++, tiov++) {
#if 0
		/* cannot happen iov_len is unsigned */
		if (tiov->iov_len < 0) {
			error = EINVAL;
			goto out;
		}
#endif
		/*
		 * Reads return ssize_t because -1 is returned on error.
		 * Therefore we must restrict the length to SSIZE_MAX to
		 * avoid garbage return values.
		 */
		auio.uio_resid += tiov->iov_len;
		if (tiov->iov_len > SSIZE_MAX || auio.uio_resid > SSIZE_MAX) {
			error = EINVAL;
			goto out;
		}
	}

	ktriov = NULL;
	if (ktrpoint(KTR_GENIO)) {
		iovlen = auio.uio_iovcnt * sizeof(struct iovec);
		ktriov = malloc(iovlen, M_TEMP, M_WAITOK);
		memcpy(ktriov, auio.uio_iov, iovlen);
	}

	len = auio.uio_resid;
	mp->msg_flags &= MSG_USERFLAGS;
	error = (*so->so_receive)(so, from, &auio, NULL, control,
	    &mp->msg_flags);
	len -= auio.uio_resid;
	*retsize = len;
	if (error != 0 && len != 0
	    && (error == ERESTART || error == EINTR || error == EWOULDBLOCK))
		/* Some data transferred */
		error = 0;

	if (ktriov != NULL) {
		ktrgeniov(s, UIO_READ, ktriov, len, error);
		free(ktriov, M_TEMP);
	}

	if (error != 0) {
		m_freem(*from);
		*from = NULL;
		if (control != NULL) {
			free_control_mbuf(l, *control, *control);
			*control = NULL;
		}
	}
 out:
	if (iov != aiov)
		free(iov, M_TEMP);
	fd_putfile(s);
	return (error);
}


/* ARGSUSED */
int
sys_shutdown(struct lwp *l, const struct sys_shutdown_args *uap, register_t *retval)
{
	/* {
		syscallarg(int)	s;
		syscallarg(int)	how;
	} */
	struct socket	*so;
	int		error;

	if ((error = fd_getsock(SCARG(uap, s), &so)) != 0)
		return (error);
	solock(so);
	error = soshutdown(so, SCARG(uap, how));
	sounlock(so);
	fd_putfile(SCARG(uap, s));
	return (error);
}

/* ARGSUSED */
int
sys_setsockopt(struct lwp *l, const struct sys_setsockopt_args *uap, register_t *retval)
{
	/* {
		syscallarg(int)			s;
		syscallarg(int)			level;
		syscallarg(int)			name;
		syscallarg(const void *)	val;
		syscallarg(unsigned int)	valsize;
	} */
	struct proc	*p;
	struct mbuf	*m;
	struct socket	*so;
	int		error;
	unsigned int	len;

	p = l->l_proc;
	m = NULL;
	if ((error = fd_getsock(SCARG(uap, s), &so)) != 0)
		return (error);
	len = SCARG(uap, valsize);
	if (len > MCLBYTES) {
		error = EINVAL;
		goto out;
	}
	if (SCARG(uap, val)) {
		m = getsombuf(so, MT_SOOPTS);
		if (len > MLEN)
			m_clget(m, M_WAIT);
		error = copyin(SCARG(uap, val), mtod(m, void *), len);
		if (error) {
			(void) m_free(m);
			goto out;
		}
		m->m_len = SCARG(uap, valsize);
	}
	error = sosetopt(so, SCARG(uap, level), SCARG(uap, name), m);
 out:
 	fd_putfile(SCARG(uap, s));
	return (error);
}

/* ARGSUSED */
int
sys_getsockopt(struct lwp *l, const struct sys_getsockopt_args *uap, register_t *retval)
{
	/* {
		syscallarg(int)			s;
		syscallarg(int)			level;
		syscallarg(int)			name;
		syscallarg(void *)		val;
		syscallarg(unsigned int *)	avalsize;
	} */
	struct socket	*so;
	struct mbuf	*m;
	unsigned int	op, i, valsize;
	int		error;
	char *val = SCARG(uap, val);

	m = NULL;
	if ((error = fd_getsock(SCARG(uap, s), &so)) != 0)
		return (error);
	if (val != NULL) {
		error = copyin(SCARG(uap, avalsize),
			       &valsize, sizeof(valsize));
		if (error)
			goto out;
	} else
		valsize = 0;
	error = sogetopt(so, SCARG(uap, level), SCARG(uap, name), &m);
	if (error == 0 && val != NULL && valsize && m != NULL) {
		op = 0;
		while (m && !error && op < valsize) {
			i = min(m->m_len, (valsize - op));
			error = copyout(mtod(m, void *), val, i);
			op += i;
			val += i;
			m = m_free(m);
		}
		valsize = op;
		if (error == 0)
			error = copyout(&valsize,
					SCARG(uap, avalsize), sizeof(valsize));
	}
	if (m != NULL)
		(void) m_freem(m);
 out:
 	fd_putfile(SCARG(uap, s));
	return (error);
}

#ifdef PIPE_SOCKETPAIR
/* ARGSUSED */
int
sys_pipe(struct lwp *l, const void *v, register_t *retval)
{
	file_t		*rf, *wf;
	struct socket	*rso, *wso;
	int		fd, error;
	proc_t		*p;

	p = curproc;
	if ((error = socreate(AF_LOCAL, &rso, SOCK_STREAM, 0, l, NULL)) != 0)
		return (error);
	if ((error = socreate(AF_LOCAL, &wso, SOCK_STREAM, 0, l, rso)) != 0)
		goto free1;
	/* remember this socket pair implements a pipe */
	wso->so_state |= SS_ISAPIPE;
	rso->so_state |= SS_ISAPIPE;
	if ((error = fd_allocfile(&rf, &fd)) != 0)
		goto free2;
	retval[0] = fd;
	rf->f_flag = FREAD;
	rf->f_type = DTYPE_SOCKET;
	rf->f_ops = &socketops;
	rf->f_data = rso;
	if ((error = fd_allocfile(&wf, &fd)) != 0)
		goto free3;
	wf->f_flag = FWRITE;
	wf->f_type = DTYPE_SOCKET;
	wf->f_ops = &socketops;
	wf->f_data = wso;
	retval[1] = fd;
	solock(wso);
	error = unp_connect2(wso, rso, PRU_CONNECT2);
	sounlock(wso);
	if (error != 0)
		goto free4;
	fd_affix(p, wf, (int)retval[1]);
	fd_affix(p, rf, (int)retval[0]);
	return (0);
 free4:
	fd_abort(p, wf, (int)retval[1]);
 free3:
	fd_abort(p, rf, (int)retval[0]);
 free2:
	(void)soclose(wso);
 free1:
	(void)soclose(rso);
	return (error);
}
#endif /* PIPE_SOCKETPAIR */

/*
 * Get socket name.
 */
/* ARGSUSED */
int
do_sys_getsockname(struct lwp *l, int fd, int which, struct mbuf **nam)
{
	struct socket	*so;
	struct mbuf	*m;
	int		error;

	if ((error = fd_getsock(fd, &so)) != 0)
		return error;

	m = m_getclr(M_WAIT, MT_SONAME);
	MCLAIM(m, so->so_mowner);

	solock(so);
	if (which == PRU_PEERADDR
	    && (so->so_state & (SS_ISCONNECTED | SS_ISCONFIRMING)) == 0) {
		error = ENOTCONN;
	} else {
		*nam = m;
		error = (*so->so_proto->pr_usrreq)(so, which, NULL, m, NULL,
		    NULL);
	}
 	sounlock(so);
	if (error != 0)
		m_free(m);
 	fd_putfile(fd);
	return error;
}

int
copyout_sockname(struct sockaddr *asa, unsigned int *alen, int flags,
    struct mbuf *addr)
{
	int len;
	int error;

	if (asa == NULL)
		/* Assume application not interested */
		return 0;

	if (flags & MSG_LENUSRSPACE) {
		error = copyin(alen, &len, sizeof(len));
		if (error)
			return error;
	} else
		len = *alen;
	if (len < 0)
		return EINVAL;

	if (addr == NULL) {
		len = 0;
		error = 0;
	} else {
		if (len > addr->m_len)
			len = addr->m_len;
		/* Maybe this ought to copy a chain ? */
		ktrkuser("sockname", mtod(addr, void *), len);
		error = copyout(mtod(addr, void *), asa, len);
	}

	if (error == 0) {
		if (flags & MSG_LENUSRSPACE)
			error = copyout(&len, alen, sizeof(len));
		else
			*alen = len;
	}

	return error;
}

/*
 * Get socket name.
 */
/* ARGSUSED */
int
sys_getsockname(struct lwp *l, const struct sys_getsockname_args *uap, register_t *retval)
{
	/* {
		syscallarg(int)			fdes;
		syscallarg(struct sockaddr *)	asa;
		syscallarg(unsigned int *)	alen;
	} */
	struct mbuf	*m;
	int		error;

	error = do_sys_getsockname(l, SCARG(uap, fdes), PRU_SOCKADDR, &m);
	if (error != 0)
		return error;

	error = copyout_sockname(SCARG(uap, asa), SCARG(uap, alen),
	    MSG_LENUSRSPACE, m);
	if (m != NULL)
		m_free(m);
	return error;
}

/*
 * Get name of peer for connected socket.
 */
/* ARGSUSED */
int
sys_getpeername(struct lwp *l, const struct sys_getpeername_args *uap, register_t *retval)
{
	/* {
		syscallarg(int)			fdes;
		syscallarg(struct sockaddr *)	asa;
		syscallarg(unsigned int *)	alen;
	} */
	struct mbuf	*m;
	int		error;

	error = do_sys_getsockname(l, SCARG(uap, fdes), PRU_PEERADDR, &m);
	if (error != 0)
		return error;

	error = copyout_sockname(SCARG(uap, asa), SCARG(uap, alen),
	    MSG_LENUSRSPACE, m);
	if (m != NULL)
		m_free(m);
	return error;
}

/*
 * XXX In a perfect world, we wouldn't pass around socket control
 * XXX arguments in mbufs, and this could go away.
 */
int
sockargs(struct mbuf **mp, const void *bf, size_t buflen, int type)
{
	struct sockaddr	*sa;
	struct mbuf	*m;
	int		error;

	/*
	 * We can't allow socket names > UCHAR_MAX in length, since that
	 * will overflow sa_len.  Control data more than a page size in
	 * length is just too much.
	 */
	if (buflen > (type == MT_SONAME ? UCHAR_MAX : PAGE_SIZE))
		return (EINVAL);

	/* Allocate an mbuf to hold the arguments. */
	m = m_get(M_WAIT, type);
	/* can't claim.  don't who to assign it to. */
	if (buflen > MLEN) {
		/*
		 * Won't fit into a regular mbuf, so we allocate just
		 * enough external storage to hold the argument.
		 */
		MEXTMALLOC(m, buflen, M_WAITOK);
	}
	m->m_len = buflen;
	error = copyin(bf, mtod(m, void *), buflen);
	if (error) {
		(void) m_free(m);
		return (error);
	}
	ktrkuser("sockargs", mtod(m, void *), buflen);
	*mp = m;
	if (type == MT_SONAME) {
		sa = mtod(m, struct sockaddr *);
#if BYTE_ORDER != BIG_ENDIAN
		/*
		 * 4.3BSD compat thing - need to stay, since bind(2),
		 * connect(2), sendto(2) were not versioned for COMPAT_43.
		 */
		if (sa->sa_family == 0 && sa->sa_len < AF_MAX)
			sa->sa_family = sa->sa_len;
#endif
		sa->sa_len = buflen;
	}
	return (0);
}

int
getsock(int fdes, struct file **fpp)
{
	file_t		*fp;

	if ((fp = fd_getfile(fdes)) == NULL)
		return (EBADF);

	if (fp->f_type != DTYPE_SOCKET) {
		fd_putfile(fdes);
		return (ENOTSOCK);
	}
	*fpp = fp;
	return (0);
}
