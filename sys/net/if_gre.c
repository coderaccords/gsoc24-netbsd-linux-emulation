/*	$NetBSD: if_gre.c,v 1.115 2007/10/06 03:35:14 dyoung Exp $ */

/*
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Heiko W.Rupp <hwr@pilhuhn.de>
 *
 * IPv6-over-GRE contributed by Gert Doering <gert@greenie.muc.de>
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

/*
 * Encapsulate L3 protocols into IP
 * See RFC 1701 and 1702 for more details.
 * If_gre is compatible with Cisco GRE tunnels, so you can
 * have a NetBSD box as the other end of a tunnel interface of a Cisco
 * router. See gre(4) for more details.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: if_gre.c,v 1.115 2007/10/06 03:35:14 dyoung Exp $");

#include "opt_gre.h"
#include "opt_inet.h"
#include "bpfilter.h"

#ifdef INET
#include <sys/param.h>
#include <sys/file.h>
#include <sys/filedesc.h>
#include <sys/malloc.h>
#include <sys/mallocvar.h>
#include <sys/mbuf.h>
#include <sys/proc.h>
#include <sys/domain.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/ioctl.h>
#include <sys/queue.h>
#if __NetBSD__
#include <sys/systm.h>
#include <sys/sysctl.h>
#include <sys/kauth.h>
#endif

#include <sys/kernel.h>
#include <sys/mutex.h>
#include <sys/condvar.h>
#include <sys/kthread.h>

#include <machine/cpu.h>

#include <net/ethertypes.h>
#include <net/if.h>
#include <net/if_types.h>
#include <net/netisr.h>
#include <net/route.h>

#ifdef INET
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>
#else
#error "Huh? if_gre without inet?"
#endif


#ifdef NETATALK
#include <netatalk/at.h>
#include <netatalk/at_var.h>
#include <netatalk/at_extern.h>
#endif

#if NBPFILTER > 0
#include <sys/time.h>
#include <net/bpf.h>
#endif

#include <net/if_gre.h>

#include <compat/sys/socket.h>
#include <compat/sys/sockio.h>
/*
 * It is not easy to calculate the right value for a GRE MTU.
 * We leave this task to the admin and use the same default that
 * other vendors use.
 */
#define GREMTU 1476

#ifdef GRE_DEBUG
int gre_debug = 0;
#define	GRE_DPRINTF(__sc, __fmt, ...)				\
	do {							\
		if (__predict_false(gre_debug ||		\
		    ((__sc)->sc_if.if_flags & IFF_DEBUG) != 0))	\
			printf(__fmt, __VA_ARGS__);		\
	} while (/*CONSTCOND*/0)
#else
#define	GRE_DPRINTF(__sc, __fmt, ...)	do { } while (/*CONSTCOND*/0)
#endif /* GRE_DEBUG */

int ip_gre_ttl = GRE_TTL;
MALLOC_DEFINE(M_GRE_BUFQ, "gre_bufq", "gre mbuf queue");

static int gre_clone_create(struct if_clone *, int);
static int gre_clone_destroy(struct ifnet *);

static struct if_clone gre_cloner =
    IF_CLONE_INITIALIZER("gre", gre_clone_create, gre_clone_destroy);

static int gre_input(struct gre_softc *, struct mbuf *, int,
    const struct gre_h *);
static bool gre_is_nullconf(const struct gre_soparm *);
static int gre_output(struct ifnet *, struct mbuf *,
			   const struct sockaddr *, struct rtentry *);
static int gre_ioctl(struct ifnet *, u_long, void *);
static void gre_closef(struct file **, struct lwp *);
static int gre_getsockname(struct socket *, struct mbuf *, struct lwp *);
static int gre_getpeername(struct socket *, struct mbuf *, struct lwp *);
static int gre_getnames(struct socket *, struct lwp *,
    struct sockaddr_storage *, struct sockaddr_storage *);
static void gre_clearconf(struct gre_soparm *, bool);
static int gre_soreceive(struct socket *, struct mbuf **);
static int gre_sosend(struct socket *, struct mbuf *, struct lwp *);
static struct socket *gre_reconf(struct gre_softc *, struct socket *, lwp_t *,
    const struct gre_soparm *);

static int
nearest_pow2(size_t len0)
{
	size_t len, mid;

	if (len0 == 0)
		return 1;

	for (len = len0; (len & (len - 1)) != 0; len &= len - 1)
		;

	mid = len | (len >> 1);

	/* avoid overflow */
	if ((len << 1) < len)
		return len;
	if (len0 >= mid)
		return len << 1;
	return len;
}

static struct gre_bufq *
gre_bufq_init(struct gre_bufq *bq, size_t len0)
{
	size_t len;

	len = nearest_pow2(len0);

	memset(bq, 0, sizeof(*bq));
	bq->bq_buf = malloc(len * sizeof(struct mbuf *), M_GRE_BUFQ, M_WAITOK);
	bq->bq_len = len;
	bq->bq_lenmask = len - 1;

	return bq;
}

static bool
gre_bufq_empty(struct gre_bufq *bq)
{
	return bq->bq_prodidx == bq->bq_considx;
}

static struct mbuf *
gre_bufq_dequeue(struct gre_bufq *bq)
{
	struct mbuf *m;

	if (gre_bufq_empty(bq))
		return NULL;

	m = bq->bq_buf[bq->bq_considx];
	bq->bq_considx = (bq->bq_considx + 1) & bq->bq_lenmask;

	return m;
}

static void
gre_bufq_purge(struct gre_bufq *bq)
{
	struct mbuf *m;

	while ((m = gre_bufq_dequeue(bq)) != NULL)
		m_freem(m);
}

static int
gre_bufq_enqueue(struct gre_bufq *bq, struct mbuf *m)
{
	int next;

	next = (bq->bq_prodidx + 1) & bq->bq_lenmask;

	if (next == bq->bq_considx) {
		bq->bq_drops++;
		return ENOBUFS;
	}

	bq->bq_buf[bq->bq_prodidx] = m;
	bq->bq_prodidx = next;
	return 0;
}

static void
greintr(void *arg)
{
	struct gre_softc *sc = (struct gre_softc *)arg;
	struct socket *so = sc->sc_so;
	lwp_t *l = curlwp;
	int rc;
	struct mbuf *m;

	KASSERT(sc->sc_so != NULL);

	sc->sc_send_ev.ev_count++;
	GRE_DPRINTF(sc, "%s: enter\n", __func__);
	while ((m = gre_bufq_dequeue(&sc->sc_snd)) != NULL) {
		/* XXX handle ENOBUFS? */
		if ((rc = gre_sosend(so, m, l)) != 0) {
			GRE_DPRINTF(sc, "%s: gre_sosend failed %d\n", __func__,
			    rc);
		}
	}
}

/* Caller must hold sc->sc_mtx. */
static void
gre_wait(struct gre_softc *sc)
{
	sc->sc_waiters++;
	cv_wait(&sc->sc_condvar, &sc->sc_mtx);
	sc->sc_waiters--;
}

/* Caller must hold sc->sc_mtx. */
static void
gre_join(struct gre_softc *sc)
{
	while (sc->sc_waiters > 0)
		cv_wait(&sc->sc_condvar, &sc->sc_mtx);
}

static void
gre_evcnt_detach(struct gre_softc *sc)
{
	evcnt_detach(&sc->sc_unsupp_ev);
	evcnt_detach(&sc->sc_pullup_ev);
	evcnt_detach(&sc->sc_error_ev);
	evcnt_detach(&sc->sc_block_ev);
	evcnt_detach(&sc->sc_recv_ev);

	evcnt_detach(&sc->sc_oflow_ev);
	evcnt_detach(&sc->sc_send_ev);
}

static void
gre_evcnt_attach(struct gre_softc *sc)
{
	evcnt_attach_dynamic(&sc->sc_recv_ev, EVCNT_TYPE_MISC,
	    NULL, sc->sc_if.if_xname, "recv");
	evcnt_attach_dynamic(&sc->sc_block_ev, EVCNT_TYPE_MISC,
	    &sc->sc_recv_ev, sc->sc_if.if_xname, "would block");
	evcnt_attach_dynamic(&sc->sc_error_ev, EVCNT_TYPE_MISC,
	    &sc->sc_recv_ev, sc->sc_if.if_xname, "error");
	evcnt_attach_dynamic(&sc->sc_pullup_ev, EVCNT_TYPE_MISC,
	    &sc->sc_recv_ev, sc->sc_if.if_xname, "pullup failed");
	evcnt_attach_dynamic(&sc->sc_unsupp_ev, EVCNT_TYPE_MISC,
	    &sc->sc_recv_ev, sc->sc_if.if_xname, "unsupported");

	evcnt_attach_dynamic(&sc->sc_send_ev, EVCNT_TYPE_MISC,
	    NULL, sc->sc_if.if_xname, "send");
	evcnt_attach_dynamic(&sc->sc_oflow_ev, EVCNT_TYPE_MISC,
	    &sc->sc_send_ev, sc->sc_if.if_xname, "overflow");
}

static int
gre_clone_create(struct if_clone *ifc, int unit)
{
	struct gre_soparm sp;
	struct gre_softc *sc;

	sc = malloc(sizeof(struct gre_softc), M_DEVBUF, M_WAITOK|M_ZERO);
	mutex_init(&sc->sc_mtx, MUTEX_DRIVER, IPL_SOFTNET);
	cv_init(&sc->sc_condvar, "gre wait");

	snprintf(sc->sc_if.if_xname, sizeof(sc->sc_if.if_xname), "%s%d",
	    ifc->ifc_name, unit);
	sc->sc_if.if_softc = sc;
	sc->sc_if.if_type = IFT_TUNNEL;
	sc->sc_if.if_addrlen = 0;
	sc->sc_if.if_hdrlen = sizeof(struct ip) + sizeof(struct gre_h);
	sc->sc_if.if_dlt = DLT_NULL;
	sc->sc_if.if_mtu = GREMTU;
	sc->sc_if.if_flags = IFF_POINTOPOINT|IFF_MULTICAST;
	sc->sc_if.if_output = gre_output;
	sc->sc_if.if_ioctl = gre_ioctl;
	sockaddr_copy(sstosa(&sp.sp_dst), sizeof(sp.sp_dst), sintocsa(&in_any));
	sockaddr_copy(sstosa(&sp.sp_src), sizeof(sp.sp_src), sintocsa(&in_any));
	sp.sp_proto = IPPROTO_GRE;
	sp.sp_type = SOCK_RAW;
	sp.sp_bysock = 0;
	sp.sp_fd = -1;
	sc->sc_soparm = sp;

	gre_evcnt_attach(sc);

	gre_bufq_init(&sc->sc_snd, 17);
	sc->sc_if.if_flags |= IFF_LINK0;
	if_attach(&sc->sc_if);
	if_alloc_sadl(&sc->sc_if);
#if NBPFILTER > 0
	bpfattach(&sc->sc_if, DLT_NULL, sizeof(u_int32_t));
#endif
	sc->sc_lwp = &lwp0;
	sc->sc_state = GRE_S_IDLE;
	return 0;
}

static int
gre_clone_destroy(struct ifnet *ifp)
{
	struct gre_softc *sc = ifp->if_softc;

	GRE_DPRINTF(sc, "%s: l.%d\n", __func__, __LINE__);

#if NBPFILTER > 0
	bpfdetach(ifp);
#endif
	if_detach(ifp);

	mutex_enter(&sc->sc_mtx);
	GRE_DPRINTF(sc, "%s: l.%d\n", __func__, __LINE__);
	while (sc->sc_state != GRE_S_IDLE)
		gre_wait(sc);
	GRE_DPRINTF(sc, "%s: l.%d\n", __func__, __LINE__);
	sc->sc_state = GRE_S_DIE;
	cv_broadcast(&sc->sc_condvar);
	gre_join(sc);
	sc->sc_so = gre_reconf(sc, sc->sc_so, sc->sc_lwp, NULL);
	mutex_exit(&sc->sc_mtx);
	GRE_DPRINTF(sc, "%s: l.%d\n", __func__, __LINE__);

	cv_destroy(&sc->sc_condvar);
	mutex_destroy(&sc->sc_mtx);
	gre_evcnt_detach(sc);
	free(sc, M_DEVBUF);

	return 0;
}

static void
gre_receive(struct socket *so, void *arg, int waitflag)
{
	struct gre_softc *sc = (struct gre_softc *)arg;
	int rc;
	const struct gre_h *gh;
	struct mbuf *m;

	GRE_DPRINTF(sc, "%s: enter\n", __func__);

	sc->sc_recv_ev.ev_count++;

	rc = gre_soreceive(so, &m);
	/* TBD Back off if ECONNREFUSED (indicates
	 * ICMP Port Unreachable)?
	 */
	if (rc == EWOULDBLOCK) {
		GRE_DPRINTF(sc, "%s: EWOULDBLOCK\n", __func__);
		sc->sc_block_ev.ev_count++;
		return;
	} else if (rc != 0 || m == NULL) {
		GRE_DPRINTF(sc, "%s: rc %d m %p\n",
		    sc->sc_if.if_xname, rc, (void *)m);
		sc->sc_error_ev.ev_count++;
		return;
	}
	if (m->m_len < sizeof(*gh) &&
	    (m = m_pullup(m, sizeof(*gh))) == NULL) {
		GRE_DPRINTF(sc, "%s: m_pullup failed\n", __func__);
		sc->sc_pullup_ev.ev_count++;
		return;
	}
	gh = mtod(m, const struct gre_h *);

	if (gre_input(sc, m, 0, gh) == 0) {
		sc->sc_unsupp_ev.ev_count++;
		GRE_DPRINTF(sc, "%s: dropping unsupported\n", __func__);
		m_freem(m);
	}
}

static void
gre_upcall_add(struct socket *so, void *arg)
{
	/* XXX What if the kernel already set an upcall? */
	KASSERT((so->so_rcv.sb_flags & SB_UPCALL) == 0);
	so->so_upcallarg = arg;
	so->so_upcall = gre_receive;
	so->so_rcv.sb_flags |= SB_UPCALL;
}

static void
gre_upcall_remove(struct socket *so)
{
	so->so_rcv.sb_flags &= ~SB_UPCALL;
	so->so_upcallarg = NULL;
	so->so_upcall = NULL;
}

static int
gre_socreate(struct gre_softc *sc, struct lwp *l,
    struct gre_soparm *sp, int *fdout)
{
	const struct protosw *pr;
	int fd, rc;
	struct mbuf *m;
	struct sockaddr *sa;
	sa_family_t af;
	struct socket *so;

	GRE_DPRINTF(sc, "%s: enter\n", __func__);

	af = sp->sp_src.ss_family;
	rc = fsocreate(af, &so, sp->sp_type, sp->sp_proto, l, &fd);
	if (rc != 0) {
		GRE_DPRINTF(sc, "%s: fsocreate failed\n", __func__);
		return rc;
	}

	if ((m = getsombuf(so)) == NULL) {
		rc = ENOBUFS;
		goto out;
	}
	sa = mtod(m, struct sockaddr *);
	sockaddr_copy(sa, MIN(MLEN, sizeof(sp->sp_src)), sstosa(&sp->sp_src));
	m->m_len = sp->sp_src.ss_len;

#if 0
	/* XXX */
	GRE_DPRINTF(sc, "%s: bind 0x%08" PRIx32 " port %d\n", __func__,
	    sin->sin_addr.s_addr, ntohs(sin->sin_port));
#endif
	if ((rc = sobind(so, m, l)) != 0) {
		GRE_DPRINTF(sc, "%s: sobind failed\n", __func__);
		goto out;
	}

	if ((rc = gre_getsockname(so, m, l)) != 0) {
		GRE_DPRINTF(sc, "%s: gre_getsockname\n", __func__);
		goto out;
	}
	sockaddr_copy(sstosa(&sp->sp_src), sizeof(sp->sp_src), sa);

	sockaddr_copy(sa, MIN(MLEN, sizeof(sp->sp_dst)), sstosa(&sp->sp_dst));
	m->m_len = sp->sp_dst.ss_len;

	if ((rc = soconnect(so, m, l)) != 0) {
		GRE_DPRINTF(sc, "%s: soconnect failed\n", __func__);
		goto out;
	}

	/* XXX convert to a (new) SOL_SOCKET call */
	*mtod(m, int *) = ip_gre_ttl;
	m->m_len = sizeof(int);
	pr = so->so_proto;
	KASSERT(pr != NULL);
	rc = sosetopt(so, IPPROTO_IP, IP_TTL, m);
	m = NULL;
	if (rc != 0) {
		GRE_DPRINTF(sc, "%s: sosetopt ttl failed\n", __func__);
		rc = 0;
	}
	rc = sosetopt(so, SOL_SOCKET, SO_NOHEADER, m_intopt(so, 1));
	if (rc != 0) {
		GRE_DPRINTF(sc, "%s: sosetopt SO_NOHEADER failed\n", __func__);
		rc = 0;
	}
out:
	m_freem(m);

	if (rc != 0)
		fdrelease(l, fd);
	else 
		*fdout = fd;

	return rc;
}

static int
gre_sosend(struct socket *so, struct mbuf *top, struct lwp *l)
{
	struct mbuf	**mp;
	struct proc	*p;
	long		space, resid;
	int		error, s;

	p = l->l_proc;

	resid = top->m_pkthdr.len;
	if (p)
		p->p_stats->p_ru.ru_msgsnd++;
#define	snderr(errno)	{ error = errno; splx(s); goto release; }

	if ((error = sblock(&so->so_snd, M_NOWAIT)) != 0)
		goto out;
	s = splsoftnet();
	if (so->so_state & SS_CANTSENDMORE)
		snderr(EPIPE);
	if (so->so_error) {
		error = so->so_error;
		so->so_error = 0;
		splx(s);
		goto release;
	}
	if ((so->so_state & SS_ISCONNECTED) == 0) {
		if (so->so_proto->pr_flags & PR_CONNREQUIRED) {
			if ((so->so_state & SS_ISCONFIRMING) == 0)
				snderr(ENOTCONN);
		} else
			snderr(EDESTADDRREQ);
	}
	space = sbspace(&so->so_snd);
	if (resid > so->so_snd.sb_hiwat)
		snderr(EMSGSIZE);
	if (space < resid)
		snderr(EWOULDBLOCK);
	splx(s);
	mp = &top;
	/*
	 * Data is prepackaged in "top".
	 */
	s = splsoftnet();

	if (so->so_state & SS_CANTSENDMORE)
		snderr(EPIPE);

	error = (*so->so_proto->pr_usrreq)(so, PRU_SEND, top, NULL, NULL, l);
	splx(s);

	top = NULL;
	mp = &top;
	if (error != 0)
		goto release;

 release:
	sbunlock(&so->so_snd);
 out:
	if (top != NULL)
		m_freem(top);
	return error;
}

/* This is a stripped-down version of soreceive() that will never
 * block.  It will support SOCK_DGRAM sockets.  It may also support
 * SOCK_SEQPACKET sockets.
 */
static int
gre_soreceive(struct socket *so, struct mbuf **mp0)
{
	struct lwp *l = curlwp;
	struct mbuf *m, **mp;
	int flags, len, error, s, type;
	const struct protosw	*pr;
	struct mbuf *nextrecord;

	KASSERT(mp0 != NULL);

	flags = MSG_DONTWAIT;
	pr = so->so_proto;
	mp = mp0;
	type = 0;

	*mp = NULL;

	KASSERT(pr->pr_flags & PR_ATOMIC);

	if (so->so_state & SS_ISCONFIRMING)
		(*pr->pr_usrreq)(so, PRU_RCVD, NULL, NULL, NULL, l);

 restart:
	if ((error = sblock(&so->so_rcv, M_NOWAIT)) != 0)
		return error;
	s = splsoftnet();

	m = so->so_rcv.sb_mb;
	/*
	 * If we have less data than requested, do not block awaiting more.
	 */
	if (m == NULL) {
#ifdef DIAGNOSTIC
		if (so->so_rcv.sb_cc)
			panic("receive 1");
#endif
		if (so->so_error) {
			error = so->so_error;
			so->so_error = 0;
		} else if (so->so_state & SS_CANTRCVMORE)
			;
		else if ((so->so_state & (SS_ISCONNECTED|SS_ISCONNECTING)) == 0
		      && (so->so_proto->pr_flags & PR_CONNREQUIRED))
			error = ENOTCONN;
		else
			error = EWOULDBLOCK;
		goto release;
	}
	/*
	 * On entry here, m points to the first record of the socket buffer.
	 * While we process the initial mbufs containing address and control
	 * info, we save a copy of m->m_nextpkt into nextrecord.
	 */
	if (l != NULL)
		l->l_proc->p_stats->p_ru.ru_msgrcv++;
	KASSERT(m == so->so_rcv.sb_mb);
	SBLASTRECORDCHK(&so->so_rcv, "soreceive 1");
	SBLASTMBUFCHK(&so->so_rcv, "soreceive 1");
	nextrecord = m->m_nextpkt;
	if (pr->pr_flags & PR_ADDR) {
#ifdef DIAGNOSTIC
		if (m->m_type != MT_SONAME)
			panic("receive 1a");
#endif
		sbfree(&so->so_rcv, m);
		MFREE(m, so->so_rcv.sb_mb);
		m = so->so_rcv.sb_mb;
	}
	while (m != NULL && m->m_type == MT_CONTROL && error == 0) {
		sbfree(&so->so_rcv, m);
		/*
		 * Dispose of any SCM_RIGHTS message that went
		 * through the read path rather than recv.
		 */
		if (pr->pr_domain->dom_dispose &&
		    mtod(m, struct cmsghdr *)->cmsg_type == SCM_RIGHTS)
			(*pr->pr_domain->dom_dispose)(m);
		MFREE(m, so->so_rcv.sb_mb);
		m = so->so_rcv.sb_mb;
	}

	/*
	 * If m is non-NULL, we have some data to read.  From now on,
	 * make sure to keep sb_lastrecord consistent when working on
	 * the last packet on the chain (nextrecord == NULL) and we
	 * change m->m_nextpkt.
	 */
	if (m != NULL) {
		m->m_nextpkt = nextrecord;
		/*
		 * If nextrecord == NULL (this is a single chain),
		 * then sb_lastrecord may not be valid here if m
		 * was changed earlier.
		 */
		if (nextrecord == NULL) {
			KASSERT(so->so_rcv.sb_mb == m);
			so->so_rcv.sb_lastrecord = m;
		}
		type = m->m_type;
		if (type == MT_OOBDATA)
			flags |= MSG_OOB;
	} else {
		KASSERT(so->so_rcv.sb_mb == m);
		so->so_rcv.sb_mb = nextrecord;
		SB_EMPTY_FIXUP(&so->so_rcv);
	}
	SBLASTRECORDCHK(&so->so_rcv, "soreceive 2");
	SBLASTMBUFCHK(&so->so_rcv, "soreceive 2");

	while (m != NULL) {
		if (m->m_type == MT_OOBDATA) {
			if (type != MT_OOBDATA)
				break;
		} else if (type == MT_OOBDATA)
			break;
#ifdef DIAGNOSTIC
		else if (m->m_type != MT_DATA && m->m_type != MT_HEADER)
			panic("receive 3");
#endif
		so->so_state &= ~SS_RCVATMARK;
		if (so->so_oobmark != 0 && so->so_oobmark < m->m_len)
			break;
		len = m->m_len;
		/*
		 * mp is set, just pass back the mbufs.
		 * Sockbuf must be consistent here (points to current mbuf,
		 * it points to next record) when we drop priority;
		 * we must note any additions to the sockbuf when we
		 * block interrupts again.
		 */
		if (m->m_flags & M_EOR)
			flags |= MSG_EOR;
		nextrecord = m->m_nextpkt;
		sbfree(&so->so_rcv, m);
		*mp = m;
		mp = &m->m_next;
		so->so_rcv.sb_mb = m = m->m_next;
		*mp = NULL;
		/*
		 * If m != NULL, we also know that
		 * so->so_rcv.sb_mb != NULL.
		 */
		KASSERT(so->so_rcv.sb_mb == m);
		if (m) {
			m->m_nextpkt = nextrecord;
			if (nextrecord == NULL)
				so->so_rcv.sb_lastrecord = m;
		} else {
			so->so_rcv.sb_mb = nextrecord;
			SB_EMPTY_FIXUP(&so->so_rcv);
		}
		SBLASTRECORDCHK(&so->so_rcv, "soreceive 3");
		SBLASTMBUFCHK(&so->so_rcv, "soreceive 3");
		if (so->so_oobmark) {
			so->so_oobmark -= len;
			if (so->so_oobmark == 0) {
				so->so_state |= SS_RCVATMARK;
				break;
			}
		}
		if (flags & MSG_EOR)
			break;
	}

	if (m != NULL) {
		m_freem(*mp);
		*mp = NULL;
		error = ENOMEM;
		(void) sbdroprecord(&so->so_rcv);
	} else {
		/*
		 * First part is an inline SB_EMPTY_FIXUP().  Second
		 * part makes sure sb_lastrecord is up-to-date if
		 * there is still data in the socket buffer.
		 */
		so->so_rcv.sb_mb = nextrecord;
		if (so->so_rcv.sb_mb == NULL) {
			so->so_rcv.sb_mbtail = NULL;
			so->so_rcv.sb_lastrecord = NULL;
		} else if (nextrecord->m_nextpkt == NULL)
			so->so_rcv.sb_lastrecord = nextrecord;
	}
	SBLASTRECORDCHK(&so->so_rcv, "soreceive 4");
	SBLASTMBUFCHK(&so->so_rcv, "soreceive 4");
	if (pr->pr_flags & PR_WANTRCVD && so->so_pcb)
		(*pr->pr_usrreq)(so, PRU_RCVD, NULL,
		    (struct mbuf *)(long)flags, NULL, l);
	if (*mp0 == NULL && (flags & MSG_EOR) == 0 &&
	    (so->so_state & SS_CANTRCVMORE) == 0) {
		sbunlock(&so->so_rcv);
		splx(s);
		goto restart;
	}

 release:
	sbunlock(&so->so_rcv);
	splx(s);
	return error;
}

static struct socket *
gre_reconf(struct gre_softc *sc, struct socket *so, lwp_t *l,
    const struct gre_soparm *newsoparm)
{
	int rc;
	struct file *fp;
	struct ifnet *ifp = &sc->sc_if;

	GRE_DPRINTF(sc, "%s: enter\n", __func__);

shutdown:
	if (sc->sc_soparm.sp_fd != -1) {
		GRE_DPRINTF(sc, "%s: l.%d\n", __func__, __LINE__);
		gre_upcall_remove(so);
		softintr_disestablish(sc->sc_si);
		sc->sc_si = NULL;
		mutex_exit(&sc->sc_mtx);
		fdrelease(l, sc->sc_soparm.sp_fd);
		mutex_enter(&sc->sc_mtx);
		gre_clearconf(&sc->sc_soparm, false);
		sc->sc_soparm.sp_fd = -1;
		so = NULL;
	}

	if (newsoparm != NULL) {
		GRE_DPRINTF(sc, "%s: l.%d\n", __func__, __LINE__);
		sc->sc_soparm = *newsoparm;
	}

	if (sc->sc_soparm.sp_fd != -1) {
		GRE_DPRINTF(sc, "%s: l.%d\n", __func__, __LINE__);
		rc = getsock(l->l_proc->p_fd, sc->sc_soparm.sp_fd, &fp);
		if (rc != 0)
			goto shutdown;
		GRE_DPRINTF(sc, "%s: l.%d\n", __func__, __LINE__);
		FILE_UNUSE(fp, NULL);
		so = (struct socket *)fp->f_data;
		sc->sc_si = softintr_establish(IPL_SOFTNET, greintr, sc);
		gre_upcall_add(so, sc);
		if ((ifp->if_flags & IFF_UP) == 0) {
			GRE_DPRINTF(sc, "%s: down\n", __func__);
			goto shutdown;
		}
	}

	GRE_DPRINTF(sc, "%s: l.%d\n", __func__, __LINE__);
	if (so != NULL)
		sc->sc_if.if_flags |= IFF_RUNNING;
	else {
		gre_bufq_purge(&sc->sc_snd);
		sc->sc_if.if_flags &= ~IFF_RUNNING;
	}
	return so;
}

static int
gre_input(struct gre_softc *sc, struct mbuf *m, int hlen,
    const struct gre_h *gh)
{
	u_int16_t flags;
	u_int32_t af;		/* af passed to BPF tap */
	int isr, s;
	struct ifqueue *ifq;

	sc->sc_if.if_ipackets++;
	sc->sc_if.if_ibytes += m->m_pkthdr.len;

	hlen += sizeof(struct gre_h);

	/* process GRE flags as packet can be of variable len */
	flags = ntohs(gh->flags);

	/* Checksum & Offset are present */
	if ((flags & GRE_CP) | (flags & GRE_RP))
		hlen += 4;
	/* We don't support routing fields (variable length) */
	if (flags & GRE_RP) {
		sc->sc_if.if_ierrors++;
		return 0;
	}
	if (flags & GRE_KP)
		hlen += 4;
	if (flags & GRE_SP)
		hlen += 4;

	switch (ntohs(gh->ptype)) { /* ethertypes */
	case ETHERTYPE_IP:
		ifq = &ipintrq;
		isr = NETISR_IP;
		af = AF_INET;
		break;
#ifdef NETATALK
	case ETHERTYPE_ATALK:
		ifq = &atintrq1;
		isr = NETISR_ATALK;
		af = AF_APPLETALK;
		break;
#endif
#ifdef INET6
	case ETHERTYPE_IPV6:
		ifq = &ip6intrq;
		isr = NETISR_IPV6;
		af = AF_INET6;
		break;
#endif
	default:	   /* others not yet supported */
		GRE_DPRINTF(sc, "%s: unhandled ethertype 0x%04x\n", __func__,
		    ntohs(gh->ptype));
		sc->sc_if.if_noproto++;
		return 0;
	}

	if (hlen > m->m_pkthdr.len) {
		m_freem(m);
		sc->sc_if.if_ierrors++;
		return EINVAL;
	}
	m_adj(m, hlen);

#if NBPFILTER > 0
	if (sc->sc_if.if_bpf != NULL)
		bpf_mtap_af(sc->sc_if.if_bpf, af, m);
#endif /*NBPFILTER > 0*/

	m->m_pkthdr.rcvif = &sc->sc_if;

	s = splnet();
	if (IF_QFULL(ifq)) {
		IF_DROP(ifq);
		m_freem(m);
	} else {
		IF_ENQUEUE(ifq, m);
	}
	/* we need schednetisr since the address family may change */
	schednetisr(isr);
	splx(s);

	return 1;	/* packet is done, no further processing needed */
}

/*
 * The output routine. Takes a packet and encapsulates it in the protocol
 * given by sc->sc_soparm.sp_proto. See also RFC 1701 and RFC 2004
 */
static int
gre_output(struct ifnet *ifp, struct mbuf *m, const struct sockaddr *dst,
	   struct rtentry *rt)
{
	int error = 0;
	struct gre_softc *sc = ifp->if_softc;
	struct gre_h *gh;
	struct ip *ip;
	u_int8_t ip_tos = 0;
	u_int16_t etype = 0;

	if ((ifp->if_flags & (IFF_UP|IFF_RUNNING)) != (IFF_UP|IFF_RUNNING)) {
		m_freem(m);
		error = ENETDOWN;
		goto end;
	}

#if NBPFILTER > 0
	if (ifp->if_bpf != NULL)
		bpf_mtap_af(ifp->if_bpf, dst->sa_family, m);
#endif

	m->m_flags &= ~(M_BCAST|M_MCAST);

	GRE_DPRINTF(sc, "%s: dst->sa_family=%d\n", __func__, dst->sa_family);
	switch (dst->sa_family) {
	case AF_INET:
		ip = mtod(m, struct ip *);
		ip_tos = ip->ip_tos;
		etype = htons(ETHERTYPE_IP);
		break;
#ifdef NETATALK
	case AF_APPLETALK:
		etype = htons(ETHERTYPE_ATALK);
		break;
#endif
#ifdef INET6
	case AF_INET6:
		etype = htons(ETHERTYPE_IPV6);
		break;
#endif
	default:
		IF_DROP(&ifp->if_snd);
		m_freem(m);
		error = EAFNOSUPPORT;
		goto end;
	}

	M_PREPEND(m, sizeof(*gh), M_DONTWAIT);

	if (m == NULL) {
		IF_DROP(&ifp->if_snd);
		error = ENOBUFS;
		goto end;
	}

	gh = mtod(m, struct gre_h *);
	gh->flags = 0;
	gh->ptype = etype;
	/* XXX Need to handle IP ToS.  Look at how I handle IP TTL. */

	ifp->if_opackets++;
	ifp->if_obytes += m->m_pkthdr.len;

	/* send it off */
	if ((error = gre_bufq_enqueue(&sc->sc_snd, m)) != 0) {
		sc->sc_oflow_ev.ev_count++;
		m_freem(m);
	} else
		softintr_schedule(sc->sc_si);
  end:
	if (error)
		ifp->if_oerrors++;
	return error;
}

/* Caller must hold sc->sc_mtx. */
static int
gre_getname(struct socket *so, int req, struct mbuf *nam, struct lwp *l)
{
	return (*so->so_proto->pr_usrreq)(so, req, NULL, nam, NULL, l);
}

/* Caller must hold sc->sc_mtx. */
static int
gre_getsockname(struct socket *so, struct mbuf *nam, struct lwp *l)
{
	return gre_getname(so, PRU_SOCKADDR, nam, l);
}

/* Caller must hold sc->sc_mtx. */
static int
gre_getpeername(struct socket *so, struct mbuf *nam, struct lwp *l)
{
	return gre_getname(so, PRU_PEERADDR, nam, l);
}

/* Caller must hold sc->sc_mtx. */
static int
gre_getnames(struct socket *so, struct lwp *l, struct sockaddr_storage *src,
    struct sockaddr_storage *dst)
{
	struct mbuf *m;
	struct sockaddr_storage *ss;
	int rc;

	if ((m = getsombuf(so)) == NULL)
		return ENOBUFS;

	ss = mtod(m, struct sockaddr_storage *);

	if ((rc = gre_getsockname(so, m, l)) != 0)
		goto out;
	*src = *ss;

	if ((rc = gre_getpeername(so, m, l)) != 0)
		goto out;
	*dst = *ss;

out:
	m_freem(m);
	return rc;
}

static void
gre_closef(struct file **fpp, struct lwp *l)
{
	struct file *fp = *fpp;

	simple_lock(&fp->f_slock);
	FILE_USE(fp);
	closef(fp, l);
	*fpp = NULL;
}

/* Caller must hold sc->sc_mtx. */
static int
gre_ssock(struct ifnet *ifp, struct gre_soparm *sp, int fd)
{
	int error, kfd;
	const struct protosw *pr;
	struct file *fp;
	struct filedesc	*fdp;
	struct gre_softc *sc = ifp->if_softc;
	struct lwp *l = curlwp;
	struct proc *kp, *p = curproc;
	struct socket *so;
	struct sockaddr_storage dst, src;

	/* getsock() will FILE_USE() and unlock the descriptor for us */
	if ((error = getsock(p->p_fd, fd, &fp)) != 0) {
		GRE_DPRINTF(sc, "%s: l.%d\n", __func__, __LINE__);
		return EINVAL;
	}

	/* Increase reference count.  Now that our reference to
	 * the file descriptor is counted, this thread can release
	 * our "use" of the descriptor, but it will not be destroyed
	 * by some other thread's action.  This thread needs to
	 * release its use, too, because one and only one thread
	 * can have use of the descriptor at once.  The kernel
	 * thread will pick up the use if it needs it.
	 */
	fp->f_count++;
	GRE_DPRINTF(sc, "%s: l.%d f_count %d\n", __func__, __LINE__,
	    fp->f_count);
	FILE_UNUSE(fp, NULL);

	kp = sc->sc_lwp->l_proc;
	while ((error = fdalloc(kp, 0, &kfd)) != 0 && error == ENOSPC)
		fdexpand(kp);
	if (error != 0)
		goto closef;
	fdp = kp->p_fd;
	simple_lock(&fdp->fd_slock);
	fdp->fd_ofiles[kfd] = fp;
	simple_unlock(&fdp->fd_slock);

	GRE_DPRINTF(sc, "%s: l.%d\n", __func__, __LINE__);

	so = (struct socket *)fp->f_data;
	pr = so->so_proto;
	if ((pr->pr_flags & PR_ATOMIC) == 0 ||
	    (sp->sp_type != 0 && pr->pr_type != sp->sp_type) ||
	    (sp->sp_proto != 0 && pr->pr_protocol != 0 &&
	     pr->pr_protocol != sp->sp_proto)) {
		GRE_DPRINTF(sc, "%s: l.%d, type %d, proto %d\n", __func__,
		    __LINE__, pr->pr_type, pr->pr_protocol);
		error = EINVAL;
		goto release;
	}

	GRE_DPRINTF(sc, "%s: l.%d\n", __func__, __LINE__);

	/* check address */
	if ((error = gre_getnames(so, l, &src, &dst)) != 0)
		goto release;

	GRE_DPRINTF(sc, "%s: l.%d\n", __func__, __LINE__);

	if (error != 0)
		goto release;

	GRE_DPRINTF(sc, "%s: l.%d\n", __func__, __LINE__);

	sp->sp_src = src;
	sp->sp_dst = dst;
	/* fp does not any longer belong to this thread. */
	sp->sp_fd = kfd;

	/* XXX print src & dst */

	return 0;
release:
	GRE_DPRINTF(sc, "%s: l.%d\n", __func__, __LINE__);
	fdrelease(sc->sc_lwp, kfd);
	return error;
closef:
	GRE_DPRINTF(sc, "%s: l.%d\n", __func__, __LINE__);
	gre_closef(&fp, l);
	return error;
}

static bool
sockaddr_is_anyaddr(const struct sockaddr *sa)
{
	socklen_t anylen, salen;
	const void *anyaddr, *addr;

	if ((anyaddr = sockaddr_anyaddr(sa, &anylen)) == NULL ||
	    (addr = sockaddr_const_addr(sa, &salen)) == NULL)
		return false;

	if (salen > anylen)
		return false;

	return memcmp(anyaddr, addr, MIN(anylen, salen)) == 0;
}

static bool
gre_is_nullconf(const struct gre_soparm *sp)
{
	return sockaddr_is_anyaddr(sstocsa(&sp->sp_src)) ||
	       sockaddr_is_anyaddr(sstocsa(&sp->sp_dst));
}

static void
gre_clearconf(struct gre_soparm *sp, bool force)
{
	if (sp->sp_bysock || force) {
		sockaddr_copy(sstosa(&sp->sp_src), sizeof(sp->sp_src),
		    sockaddr_any(sstosa(&sp->sp_src)));
		sockaddr_copy(sstosa(&sp->sp_dst), sizeof(sp->sp_dst),
		    sockaddr_any(sstosa(&sp->sp_dst)));
		sp->sp_bysock = 0;
	}
	sp->sp_fd = -1;
}

static int
gre_ioctl(struct ifnet *ifp, const u_long cmd, void *data)
{
	struct lwp *l = curlwp;
	struct ifreq *ifr;
	struct if_laddrreq *lifr = (struct if_laddrreq *)data;
	struct gre_softc *sc = ifp->if_softc;
	struct gre_soparm *sp;
	int fd, error = 0, oproto, otype;
	struct gre_soparm sp0;

	ifr = data;

	GRE_DPRINTF(sc, "%s: l.%d, cmd %lu\n", __func__, __LINE__, cmd);

	switch (cmd) {
	case SIOCSIFFLAGS:
	case SIOCSIFMTU:
	case GRESPROTO:
	case GRESADDRD:
	case GRESADDRS:
	case GRESSOCK:
	case GREDSOCK:
	case SIOCSLIFPHYADDR:
	case SIOCDIFPHYADDR:
		if (kauth_authorize_network(l->l_cred, KAUTH_NETWORK_INTERFACE,
		    KAUTH_REQ_NETWORK_INTERFACE_SETPRIV, ifp, (void *)cmd,
		    NULL) != 0)
			return EPERM;
		break;
	default:
		break;
	}

	mutex_enter(&sc->sc_mtx);

	while (sc->sc_state == GRE_S_IOCTL)
		gre_wait(sc);

	if (sc->sc_state != GRE_S_IDLE) {
		GRE_DPRINTF(sc, "%s: l.%d\n", __func__, __LINE__);
		error = ENXIO;
		goto out;
	}

	sc->sc_state = GRE_S_IOCTL;
	sp0 = sc->sc_soparm;
	sp0.sp_fd = -1;
	sp = &sp0;

	GRE_DPRINTF(sc, "%s: l.%d\n", __func__, __LINE__);

	switch (cmd) {
	case SIOCSIFADDR:
		GRE_DPRINTF(sc, "%s: l.%d\n", __func__, __LINE__);
		if ((ifp->if_flags & IFF_UP) != 0)
			break;
		gre_clearconf(sp, false);
		ifp->if_flags |= IFF_UP;
		goto mksocket;
	case SIOCSIFDSTADDR:
		break;
	case SIOCSIFFLAGS:
		oproto = sp->sp_proto;
		otype = sp->sp_type;
		switch (ifr->ifr_flags & (IFF_LINK0|IFF_LINK2)) {
		case IFF_LINK0|IFF_LINK2:
			sp->sp_proto = IPPROTO_UDP;
			sp->sp_type = SOCK_DGRAM;
			break;
		case IFF_LINK2:
			sp->sp_proto = 0;
			sp->sp_type = 0;
			break;
		case IFF_LINK0:
			sp->sp_proto = IPPROTO_GRE;
			sp->sp_type = SOCK_RAW;
			break;
		default:
			GRE_DPRINTF(sc, "%s: l.%d\n", __func__, __LINE__);
			error = EINVAL;
			goto out;
		}
		GRE_DPRINTF(sc, "%s: l.%d\n", __func__, __LINE__);
		gre_clearconf(sp, false);
		if ((ifp->if_flags & (IFF_UP|IFF_RUNNING)) ==
		    (IFF_UP|IFF_RUNNING) &&
		    (oproto == sp->sp_proto || sp->sp_proto == 0) &&
		    (otype == sp->sp_type || sp->sp_type == 0))
			break;
		switch (sp->sp_proto) {
		case IPPROTO_UDP:
		case IPPROTO_GRE:
			goto mksocket;
		default:
			break;
		}
		break;
	case SIOCSIFMTU:
		/* XXX determine MTU automatically by probing w/
		 * XXX do-not-fragment packets?
		 */
		if (ifr->ifr_mtu < 576) {
			error = EINVAL;
			break;
		}
		ifp->if_mtu = ifr->ifr_mtu;
		break;
	case SIOCGIFMTU:
		ifr->ifr_mtu = sc->sc_if.if_mtu;
		break;
	case SIOCADDMULTI:
	case SIOCDELMULTI:
		if (ifr == NULL) {
			error = EAFNOSUPPORT;
			break;
		}
		switch (ifreq_getaddr(cmd, ifr)->sa_family) {
#ifdef INET
		case AF_INET:
			break;
#endif
#ifdef INET6
		case AF_INET6:
			break;
#endif
		default:
			error = EAFNOSUPPORT;
			break;
		}
		break;
	case GRESPROTO:
		gre_clearconf(sp, false);
		oproto = sp->sp_proto;
		otype = sp->sp_type;
		sp->sp_proto = ifr->ifr_flags;
		switch (sp->sp_proto) {
		case IPPROTO_UDP:
			ifp->if_flags |= IFF_LINK0|IFF_LINK2;
			sp->sp_type = SOCK_DGRAM;
			break;
		case IPPROTO_GRE:
			ifp->if_flags |= IFF_LINK0;
			ifp->if_flags &= ~IFF_LINK2;
			sp->sp_type = SOCK_RAW;
			break;
		case 0:
			ifp->if_flags &= ~IFF_LINK0;
			ifp->if_flags |= IFF_LINK2;
			sp->sp_type = 0;
			break;
		default:
			error = EPROTONOSUPPORT;
			break;
		}
		if ((oproto == sp->sp_proto || sp->sp_proto == 0) &&
		    (otype == sp->sp_type || sp->sp_type == 0))
			break;
		switch (sp->sp_proto) {
		case IPPROTO_UDP:
		case IPPROTO_GRE:
			goto mksocket;
		default:
			break;
		}
		break;
	case GREGPROTO:
		ifr->ifr_flags = sp->sp_proto;
		break;
	case GRESADDRS:
	case GRESADDRD:
		gre_clearconf(sp, false);
		/*
		 * set tunnel endpoints, compute a less specific route
		 * to the remote end and mark if as up
		 */
		switch (cmd) {
		case GRESADDRS:
			sockaddr_copy(sstosa(&sp->sp_src),
			    sizeof(sp->sp_src), ifreq_getaddr(cmd, ifr));
			break;
		case GRESADDRD:
			sockaddr_copy(sstosa(&sp->sp_dst),
			    sizeof(sp->sp_dst), ifreq_getaddr(cmd, ifr));
			break;
		}
	checkaddr:
		if (sockaddr_any(sstosa(&sp->sp_src)) == NULL ||
		    sockaddr_any(sstosa(&sp->sp_dst)) == NULL) {
			error = EINVAL;
			break;
		}
		/* let gre_socreate() check the rest */
	mksocket:
		GRE_DPRINTF(sc, "%s: l.%d\n", __func__, __LINE__);
		/* If we're administratively down, or the configuration
		 * is empty, there's no use creating a socket.
		 */
		if ((ifp->if_flags & IFF_UP) == 0 || gre_is_nullconf(sp))
			goto sendconf;

		GRE_DPRINTF(sc, "%s: l.%d\n", __func__, __LINE__);
		mutex_exit(&sc->sc_mtx);
		error = gre_socreate(sc, l, sp, &fd);
		mutex_enter(&sc->sc_mtx);

		if (error != 0)
			break;

	setsock:
		GRE_DPRINTF(sc, "%s: l.%d\n", __func__, __LINE__);

		mutex_exit(&sc->sc_mtx);
		error = gre_ssock(ifp, sp, fd);

		if (cmd != GRESSOCK) {
			GRE_DPRINTF(sc, "%s: l.%d\n", __func__, __LINE__);
			fdrelease(l, fd);
		}

		mutex_enter(&sc->sc_mtx);

		if (error == 0) {
	sendconf:
			GRE_DPRINTF(sc, "%s: l.%d\n", __func__, __LINE__);
			ifp->if_flags &= ~IFF_RUNNING;
			sc->sc_so = gre_reconf(sc, sc->sc_so, sc->sc_lwp, sp);
		}

		break;
	case GREGADDRS:
		ifreq_setaddr(cmd, ifr, sstosa(&sp->sp_src));
		break;
	case GREGADDRD:
		ifreq_setaddr(cmd, ifr, sstosa(&sp->sp_dst));
		break;
	case GREDSOCK:
		GRE_DPRINTF(sc, "%s: l.%d\n", __func__, __LINE__);
		if (sp->sp_bysock)
			ifp->if_flags &= ~IFF_UP;
		gre_clearconf(sp, false);
		goto mksocket;
	case GRESSOCK:
		GRE_DPRINTF(sc, "%s: l.%d\n", __func__, __LINE__);
		gre_clearconf(sp, true);
		fd = (int)ifr->ifr_value;
		sp->sp_bysock = 1;
		ifp->if_flags |= IFF_UP;
		goto setsock;
	case SIOCSLIFPHYADDR:
		GRE_DPRINTF(sc, "%s: l.%d\n", __func__, __LINE__);
		if (lifr->addr.ss_family != lifr->dstaddr.ss_family) {
			error = EAFNOSUPPORT;
			break;
		}
		sockaddr_copy(sstosa(&sp->sp_src), sizeof(sp->sp_src),
		    sstosa(&lifr->addr));
		sockaddr_copy(sstosa(&sp->sp_dst), sizeof(sp->sp_dst),
		    sstosa(&lifr->dstaddr));
		GRE_DPRINTF(sc, "%s: l.%d\n", __func__, __LINE__);
		goto checkaddr;
	case SIOCDIFPHYADDR:
		GRE_DPRINTF(sc, "%s: l.%d\n", __func__, __LINE__);
		gre_clearconf(sp, true);
		ifp->if_flags &= ~IFF_UP;
		goto mksocket;
	case SIOCGLIFPHYADDR:
		GRE_DPRINTF(sc, "%s: l.%d\n", __func__, __LINE__);
		if (gre_is_nullconf(sp)) {
			error = EADDRNOTAVAIL;
			break;
		}
		sockaddr_copy(sstosa(&lifr->addr), sizeof(lifr->addr),
		    sstosa(&sp->sp_src));
		sockaddr_copy(sstosa(&lifr->dstaddr), sizeof(lifr->dstaddr),
		    sstosa(&sp->sp_dst));
		GRE_DPRINTF(sc, "%s: l.%d\n", __func__, __LINE__);
		break;
	default:
		error = EINVAL;
		break;
	}
out:
	GRE_DPRINTF(sc, "%s: l.%d\n", __func__, __LINE__);
	if (sc->sc_state == GRE_S_IOCTL) {
		GRE_DPRINTF(sc, "%s: l.%d\n", __func__, __LINE__);
		sc->sc_state = GRE_S_IDLE;
	}
	cv_signal(&sc->sc_condvar);
	mutex_exit(&sc->sc_mtx);
	return error;
}

#endif

void	greattach(int);

/* ARGSUSED */
void
greattach(int count)
{
#ifdef INET
	if_clone_attach(&gre_cloner);
#endif
}
