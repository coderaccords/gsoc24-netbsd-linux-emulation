/*	$NetBSD: ip_frag.h,v 1.2 2005/12/11 12:24:21 christos Exp $	*/

/*
 * Copyright (C) 1993-2001 by Darren Reed.
 *
 * See the IPFILTER.LICENCE file for details on licencing.
 *
 * @(#)ip_frag.h	1.5 3/24/96
 * Id: ip_frag.h,v 2.23.2.1 2004/03/29 16:21:56 darrenr Exp
 */

#ifndef _NETINET_IP_FRAG_H_
#define _NETINET_IP_FRAG_H_

#define	IPFT_SIZE	257

typedef	struct	ipfr	{
	struct	ipfr	*ipfr_hnext, **ipfr_hprev;
	struct	ipfr	*ipfr_next, **ipfr_prev;
	void	*ipfr_data;
	void	*ipfr_ifp;
	struct	in_addr	ipfr_src;
	struct	in_addr	ipfr_dst;
	u_32_t	ipfr_optmsk;
	u_short	ipfr_secmsk;
	u_short	ipfr_auth;
	u_short	ipfr_id;
	u_char	ipfr_p;
	u_char	ipfr_tos;
	u_32_t	ipfr_pass;
	u_short	ipfr_off;
	u_char	ipfr_ttl;
	u_char	ipfr_seen0;
	frentry_t *ipfr_rule;
} ipfr_t;


typedef	struct	ipfrstat {
	u_long	ifs_exists;	/* add & already exists */
	u_long	ifs_nomem;
	u_long	ifs_new;
	u_long	ifs_hits;
	u_long	ifs_expire;
	u_long	ifs_inuse;
	u_long	ifs_retrans0;
	u_long	ifs_short;
	struct	ipfr	**ifs_table;
	struct	ipfr	**ifs_nattab;
} ipfrstat_t;

#define	IPFR_CMPSZ	(offsetof(ipfr_t, ipfr_pass) - \
			 offsetof(ipfr_t, ipfr_ifp))

extern	int	ipfr_size;
extern	int	fr_ipfrttl;
extern	int	fr_frag_lock;
extern	int	fr_fraginit __P((void));
extern	void	fr_fragunload __P((void));
extern	ipfrstat_t	*fr_fragstats __P((void));

extern	int	fr_newfrag __P((fr_info_t *, u_32_t));
extern	frentry_t *fr_knownfrag __P((fr_info_t *, u_32_t *));

extern	int	fr_nat_newfrag __P((fr_info_t *, u_32_t, struct nat *));
extern	nat_t	*fr_nat_knownfrag __P((fr_info_t *));

extern	int	fr_ipid_newfrag __P((fr_info_t *, u_32_t));
extern	u_32_t	fr_ipid_knownfrag __P((fr_info_t *));

extern	void	fr_forget __P((void *));
extern	void	fr_forgetnat __P((void *));
extern	void	fr_fragclear __P((void));
extern	void	fr_fragexpire __P((void));

#if     defined(_KERNEL) && ((BSD >= 199306) || SOLARIS || defined(__sgi) \
	        || defined(__osf__) || (defined(__sgi) && (IRIX >= 60500)))
# if defined(SOLARIS2) && (SOLARIS2 < 7)
extern	void	fr_slowtimer __P((void));
# else
extern	void	fr_slowtimer __P((void *));
# endif
#else
extern	int	fr_slowtimer __P((void));
#endif

#endif /* _NETINET_IP_FRAG_H_ */
