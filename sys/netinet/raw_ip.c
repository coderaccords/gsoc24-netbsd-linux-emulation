/*	$NetBSD: raw_ip.c,v 1.27 1996/05/23 16:12:15 mycroft Exp $	*/

/*
 * Copyright (c) 1982, 1986, 1988, 1993
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
 *	@(#)raw_ip.c	8.2 (Berkeley) 1/4/94
 */

#include <sys/param.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/protosw.h>
#include <sys/socketvar.h>
#include <sys/errno.h>
#include <sys/systm.h>
#include <sys/proc.h>

#include <net/if.h>
#include <net/route.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>
#include <netinet/ip_mroute.h>
#include <netinet/in_pcb.h>
#include <netinet/in_var.h>

#include <machine/stdarg.h>

struct inpcbtable rawcbtable;

/*
 * Nominal space allocated to a raw ip socket.
 */
#define	RIPSNDQ		8192
#define	RIPRCVQ		8192

/*
 * Raw interface to IP protocol.
 */

/*
 * Initialize raw connection block q.
 */
void
rip_init()
{

	in_pcbinit(&rawcbtable, 1);
}

struct	sockaddr_in ripsrc = { sizeof(ripsrc), AF_INET };
/*
 * Setup generic address and protocol structures
 * for raw_input routine, then pass them along with
 * mbuf chain.
 */
void
#if __STDC__
rip_input(struct mbuf *m, ...)
#else
rip_input(m, va_alist)
	struct mbuf *m;
	va_dcl
#endif
{
	register struct ip *ip = mtod(m, struct ip *);
	register struct inpcb *inp;
	struct socket *last = 0;

	ripsrc.sin_addr = ip->ip_src;
	for (inp = rawcbtable.inpt_queue.cqh_first;
	    inp != (struct inpcb *)&rawcbtable.inpt_queue;
	    inp = inp->inp_queue.cqe_next) {
		if (inp->inp_ip.ip_p && inp->inp_ip.ip_p != ip->ip_p)
			continue;
		if (inp->inp_laddr.s_addr != INADDR_ANY &&
		    inp->inp_laddr.s_addr != ip->ip_dst.s_addr)
			continue;
		if (inp->inp_faddr.s_addr != INADDR_ANY &&
		    inp->inp_faddr.s_addr != ip->ip_src.s_addr)
			continue;
		if (last) {
			struct mbuf *n;
			if ((n = m_copy(m, 0, (int)M_COPYALL)) != NULL) {
				if (sbappendaddr(&last->so_rcv,
				    sintosa(&ripsrc), n,
				    (struct mbuf *)0) == 0)
					/* should notify about lost packet */
					m_freem(n);
				else
					sorwakeup(last);
			}
		}
		last = inp->inp_socket;
	}
	if (last) {
		if (sbappendaddr(&last->so_rcv, sintosa(&ripsrc), m,
		    (struct mbuf *)0) == 0)
			m_freem(m);
		else
			sorwakeup(last);
	} else {
		m_freem(m);
		ipstat.ips_noproto++;
		ipstat.ips_delivered--;
	}
}

/*
 * Generate IP header and pass packet to ip_output.
 * Tack on options user may have setup with control call.
 */
int
#if __STDC__
rip_output(struct mbuf *m, ...)
#else
rip_output(m, va_alist)
	struct mbuf *m;
	va_dcl
#endif
{
	register struct inpcb *inp;
	register struct ip *ip;
	struct mbuf *opts;
	int flags;
	va_list ap;

	va_start(ap, m);
	inp = va_arg(ap, struct inpcb *);
	va_end(ap);

	flags =
	    (inp->inp_socket->so_options & SO_DONTROUTE) | IP_ALLOWBROADCAST;

	/*
	 * If the user handed us a complete IP packet, use it.
	 * Otherwise, allocate an mbuf for a header and fill it in.
	 */
	if ((inp->inp_flags & INP_HDRINCL) == 0) {
		M_PREPEND(m, sizeof(struct ip), M_WAIT);
		ip = mtod(m, struct ip *);
		ip->ip_tos = 0;
		ip->ip_off = 0;
		ip->ip_p = inp->inp_ip.ip_p;
		ip->ip_len = m->m_pkthdr.len;
		ip->ip_src = inp->inp_laddr;
		ip->ip_dst = inp->inp_faddr;
		ip->ip_ttl = MAXTTL;
		opts = inp->inp_options;
	} else {
		ip = mtod(m, struct ip *);
		if (ip->ip_id == 0)
			ip->ip_id = htons(ip_id++);
		opts = NULL;
		/* XXX prevent ip_output from overwriting header fields */
		flags |= IP_RAWOUTPUT;
		ipstat.ips_rawout++;
	}
	return (ip_output(m, opts, &inp->inp_route, flags, inp->inp_moptions));
}

/*
 * Raw IP socket option processing.
 */
int
rip_ctloutput(op, so, level, optname, m)
	int op;
	struct socket *so;
	int level, optname;
	struct mbuf **m;
{
	register struct inpcb *inp = sotoinpcb(so);
#ifdef MROUTING
	int error;
#endif

	if (level != IPPROTO_IP) {
		if (m != 0 && *m != 0)
			(void)m_free(*m);
		return (EINVAL);
	}

	switch (optname) {

	case IP_HDRINCL:
		if (op == PRCO_SETOPT || op == PRCO_GETOPT) {
			if (m == 0 || *m == 0 || (*m)->m_len < sizeof (int))
				return (EINVAL);
			if (op == PRCO_SETOPT) {
				if (*mtod(*m, int *))
					inp->inp_flags |= INP_HDRINCL;
				else
					inp->inp_flags &= ~INP_HDRINCL;
				(void)m_free(*m);
			} else {
				(*m)->m_len = sizeof (int);
				*mtod(*m, int *) = inp->inp_flags & INP_HDRINCL;
			}
			return (0);
		}
		break;

	case MRT_INIT:
	case MRT_DONE:
	case MRT_ADD_VIF:
	case MRT_DEL_VIF:
	case MRT_ADD_MFC:
	case MRT_DEL_MFC:
	case MRT_VERSION:
	case MRT_ASSERT:
#ifdef MROUTING
		switch (op) {
		case PRCO_SETOPT:
			error = ip_mrouter_set(optname, so, m);
			break;
		case PRCO_GETOPT:
			error = ip_mrouter_get(optname, so, m);
			break;
		default:
			error = EINVAL;
			break;
		}
		return (error);
#else
		if (op == PRCO_SETOPT && *m)
			m_free(*m);
		return (EOPNOTSUPP);
#endif
	}
	return (ip_ctloutput(op, so, level, optname, m));
}

int
rip_connect(inp, nam)
	struct inpcb *inp;
	struct mbuf *nam;
{
	struct sockaddr_in *addr = mtod(nam, struct sockaddr_in *);

	if (nam->m_len != sizeof(*addr))
		return (EINVAL);
	if (ifnet.tqh_first == 0)
		return (EADDRNOTAVAIL);
	if (addr->sin_family != AF_INET &&
	    addr->sin_family != AF_IMPLINK)
		return (EAFNOSUPPORT);
	inp->inp_faddr = addr->sin_addr;
	return (0);
}

void
rip_disconnect(inp)
	struct inpcb *inp;
{

	inp->inp_faddr.s_addr = INADDR_ANY;
}

u_long	rip_sendspace = RIPSNDQ;
u_long	rip_recvspace = RIPRCVQ;

/*ARGSUSED*/
int
rip_usrreq(so, req, m, nam, control, p)
	register struct socket *so;
	int req;
	struct mbuf *m, *nam, *control;
	struct proc *p;
{
	register struct inpcb *inp;
	int s;
	register int error = 0;
#ifdef MROUTING
	extern struct socket *ip_mrouter;
#endif

	if (req == PRU_CONTROL)
		return (in_control(so, (long)m, (caddr_t)nam,
		    (struct ifnet *)control, p));

	s = splsoftnet();
	inp = sotoinpcb(so);
	if (control && control->m_len) {
		m_freem(control);
		error = EINVAL;
		goto release;
	}
	if (inp == 0 && req != PRU_ATTACH) {
		error = EINVAL;
		goto release;
	}

	switch (req) {

	case PRU_ATTACH:
		if (inp != 0) {
			error = EISCONN;
			break;
		}
		if (p == 0 || (error = suser(p->p_ucred, &p->p_acflag))) {
			error = EACCES;
			break;
		}
		if (so->so_snd.sb_hiwat == 0 || so->so_rcv.sb_hiwat == 0) {
			error = soreserve(so, rip_sendspace, rip_recvspace);
			if (error)
				break;
		}
		error = in_pcballoc(so, &rawcbtable);
		if (error)
			break;
		inp = sotoinpcb(so);
		inp->inp_ip.ip_p = (long)nam;
		break;

	case PRU_DETACH:
#ifdef MROUTING
		if (so == ip_mrouter)
			ip_mrouter_done();
#endif
		in_pcbdetach(inp);
		break;

	case PRU_BIND:
	    {
		struct sockaddr_in *addr = mtod(nam, struct sockaddr_in *);

		if (nam->m_len != sizeof(*addr)) {
			error = EINVAL;
			break;
		}
		if (ifnet.tqh_first == 0) {
			error = EADDRNOTAVAIL;
			break;
		}
		if (addr->sin_family != AF_INET &&
		    addr->sin_family != AF_IMPLINK) {
			error = EAFNOSUPPORT;
			break;
		}
		if (addr->sin_addr.s_addr != INADDR_ANY &&
		    ifa_ifwithaddr(sintosa(addr)) == 0) {
			error = EADDRNOTAVAIL;
			break;
		}
		inp->inp_laddr = addr->sin_addr;
		break;
	    }

	case PRU_LISTEN:
		error = EOPNOTSUPP;
		break;

	case PRU_CONNECT:
		error = rip_connect(inp, nam);
		if (error)
			break;
		soisconnected(so);
		break;

	case PRU_CONNECT2:
		error = EOPNOTSUPP;
		break;

	case PRU_DISCONNECT:
		soisdisconnected(so);
		rip_disconnect(inp);
		break;

	/*
	 * Mark the connection as being incapable of further input.
	 */
	case PRU_SHUTDOWN:
		socantsendmore(so);
		break;

	case PRU_RCVD:
		error = EOPNOTSUPP;
		break;

	/*
	 * Ship a packet out.  The appropriate raw output
	 * routine handles any massaging necessary.
	 */
	case PRU_SEND:
	    {
		struct in_addr faddr;

		if (nam) {
			if ((so->so_state & SS_ISCONNECTED) != 0) {
				m_freem(m);
				error = EISCONN;
				break;
			}
			error = rip_connect(inp, nam);
			if (error) {
				m_freem(m);
				break;
			}
		} else {
			if ((so->so_state & SS_ISCONNECTED) == 0) {
				m_freem(m);
				error = ENOTCONN;
				break;
			}
		}
		error = rip_output(m, inp);
		if (nam)
			rip_disconnect(inp);
		break;
	    }

	case PRU_SENSE:
		/*
		 * stat: don't bother with a blocksize.
		 */
		splx(s);
		return (0);

	case PRU_RCVOOB:
		error = EOPNOTSUPP;
		break;

	case PRU_SENDOOB:
		m_freem(m);
		error = EOPNOTSUPP;
		break;

	case PRU_SOCKADDR:
		in_setsockaddr(inp, nam);
		break;

	case PRU_PEERADDR:
		in_setpeeraddr(inp, nam);
		break;

	default:
		panic("rip_usrreq");
	}

release:
	splx(s);
	return (error);
}
