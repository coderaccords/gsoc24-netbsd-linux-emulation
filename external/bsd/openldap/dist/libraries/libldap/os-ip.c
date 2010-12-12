/*	$NetBSD: os-ip.c,v 1.4 2010/12/12 15:46:33 adam Exp $	*/

/* os-ip.c -- platform-specific TCP & UDP related code */
/* OpenLDAP: pkg/ldap/libraries/libldap/os-ip.c,v 1.118.2.20 2010/04/13 20:22:58 kurt Exp */
/* This work is part of OpenLDAP Software <http://www.openldap.org/>.
 *
 * Copyright 1998-2010 The OpenLDAP Foundation.
 * Portions Copyright 1999 Lars Uffmann.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted only as authorized by the OpenLDAP
 * Public License.
 *
 * A copy of this license is available in the file LICENSE in the
 * top-level directory of the distribution or, alternatively, at
 * <http://www.OpenLDAP.org/license.html>.
 */
/* Portions Copyright (c) 1995 Regents of the University of Michigan.
 * All rights reserved.
 */
/* Significant additional contributors include:
 *    Lars Uffman
 */

#include "portable.h"

#include <stdio.h>

#include <ac/stdlib.h>

#include <ac/errno.h>
#include <ac/socket.h>
#include <ac/string.h>
#include <ac/time.h>
#include <ac/unistd.h>

#ifdef HAVE_IO_H
#include <io.h>
#endif /* HAVE_IO_H */
#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif

#include "ldap-int.h"

#if defined( HAVE_GETADDRINFO ) && defined( HAVE_INET_NTOP )
#  ifdef LDAP_PF_INET6
int ldap_int_inet4or6 = AF_UNSPEC;
#  else
int ldap_int_inet4or6 = AF_INET;
#  endif
#endif

#ifdef LDAP_DEBUG

#define osip_debug(ld,fmt,arg1,arg2,arg3) \
do { \
	ldap_log_printf(NULL, LDAP_DEBUG_TRACE, fmt, arg1, arg2, arg3); \
} while(0)

#else

#define osip_debug(ld,fmt,arg1,arg2,arg3) ((void)0)

#endif /* LDAP_DEBUG */

static void
ldap_pvt_set_errno(int err)
{
	sock_errset(err);
}

int
ldap_int_timeval_dup( struct timeval **dest, const struct timeval *src )
{
	struct timeval *new;

	assert( dest != NULL );

	if (src == NULL) {
		*dest = NULL;
		return 0;
	}

	new = (struct timeval *) LDAP_MALLOC(sizeof(struct timeval));

	if( new == NULL ) {
		*dest = NULL;
		return 1;
	}

	AC_MEMCPY( (char *) new, (const char *) src, sizeof(struct timeval));

	*dest = new;
	return 0;
}

static int
ldap_pvt_ndelay_on(LDAP *ld, int fd)
{
	osip_debug(ld, "ldap_ndelay_on: %d\n",fd,0,0);
	return ber_pvt_socket_set_nonblock( fd, 1 );
}
   
static int
ldap_pvt_ndelay_off(LDAP *ld, int fd)
{
	osip_debug(ld, "ldap_ndelay_off: %d\n",fd,0,0);
	return ber_pvt_socket_set_nonblock( fd, 0 );
}

static ber_socket_t
ldap_int_socket(LDAP *ld, int family, int type )
{
	ber_socket_t s = socket(family, type, 0);
	osip_debug(ld, "ldap_new_socket: %d\n",s,0,0);
#ifdef FD_CLOEXEC
	fcntl(s, F_SETFD, FD_CLOEXEC);
#endif
	return ( s );
}

static int
ldap_pvt_close_socket(LDAP *ld, int s)
{
	osip_debug(ld, "ldap_close_socket: %d\n",s,0,0);
	return tcp_close(s);
}

static int
ldap_int_prepare_socket(LDAP *ld, int s, int proto )
{
	osip_debug( ld, "ldap_prepare_socket: %d\n", s, 0, 0 );

#if defined( SO_KEEPALIVE ) || defined( TCP_NODELAY )
	if ( proto == LDAP_PROTO_TCP ) {
		int dummy = 1;
#ifdef SO_KEEPALIVE
		if ( setsockopt( s, SOL_SOCKET, SO_KEEPALIVE,
			(char*) &dummy, sizeof(dummy) ) == AC_SOCKET_ERROR )
		{
			osip_debug( ld, "ldap_prepare_socket: "
				"setsockopt(%d, SO_KEEPALIVE) failed (ignored).\n",
				s, 0, 0 );
		}
		if ( ld->ld_options.ldo_keepalive_idle > 0 )
		{
#ifdef TCP_KEEPIDLE
			if ( setsockopt( s, IPPROTO_TCP, TCP_KEEPIDLE,
					(void*) &ld->ld_options.ldo_keepalive_idle,
					sizeof(ld->ld_options.ldo_keepalive_idle) ) == AC_SOCKET_ERROR )
			{
				osip_debug( ld, "ldap_prepare_socket: "
					"setsockopt(%d, TCP_KEEPIDLE) failed (ignored).\n",
					s, 0, 0 );
			}
#else
			osip_debug( ld, "ldap_prepare_socket: "
					"sockopt TCP_KEEPIDLE not supported on this system.\n", 
					0, 0, 0 );
#endif /* TCP_KEEPIDLE */
		}
		if ( ld->ld_options.ldo_keepalive_probes > 0 )
		{
#ifdef TCP_KEEPCNT
			if ( setsockopt( s, IPPROTO_TCP, TCP_KEEPCNT,
					(void*) &ld->ld_options.ldo_keepalive_probes,
					sizeof(ld->ld_options.ldo_keepalive_probes) ) == AC_SOCKET_ERROR )
			{
				osip_debug( ld, "ldap_prepare_socket: "
					"setsockopt(%d, TCP_KEEPCNT) failed (ignored).\n",
					s, 0, 0 );
			}
#else
			osip_debug( ld, "ldap_prepare_socket: "
					"sockopt TCP_KEEPCNT not supported on this system.\n", 
					0, 0, 0 );
#endif /* TCP_KEEPCNT */
		}
		if ( ld->ld_options.ldo_keepalive_interval > 0 )
		{
#ifdef TCP_KEEPINTVL
			if ( setsockopt( s, IPPROTO_TCP, TCP_KEEPINTVL,
					(void*) &ld->ld_options.ldo_keepalive_interval,
					sizeof(ld->ld_options.ldo_keepalive_interval) ) == AC_SOCKET_ERROR )
			{
				osip_debug( ld, "ldap_prepare_socket: "
					"setsockopt(%d, TCP_KEEPINTVL) failed (ignored).\n",
					s, 0, 0 );
			} 
#else
			osip_debug( ld, "ldap_prepare_socket: "
					"sockopt TCP_KEEPINTVL not supported on this system.\n", 
					0, 0, 0 );
#endif /* TCP_KEEPINTVL */
		}
#endif /* SO_KEEPALIVE */
#ifdef TCP_NODELAY
		if ( setsockopt( s, IPPROTO_TCP, TCP_NODELAY,
			(char*) &dummy, sizeof(dummy) ) == AC_SOCKET_ERROR )
		{
			osip_debug( ld, "ldap_prepare_socket: "
				"setsockopt(%d, TCP_NODELAY) failed (ignored).\n",
				s, 0, 0 );
		}
#endif /* TCP_NODELAY */
	}
#endif /* SO_KEEPALIVE || TCP_NODELAY */

	return 0;
}

#ifndef HAVE_WINSOCK

#undef TRACE
#define TRACE do { \
	osip_debug(ld, \
		"ldap_is_socket_ready: error on socket %d: errno: %d (%s)\n", \
		s, \
		errno, \
		sock_errstr(errno) ); \
} while( 0 )

/*
 * check the socket for errors after select returned.
 */
static int
ldap_pvt_is_socket_ready(LDAP *ld, int s)
{
	osip_debug(ld, "ldap_is_sock_ready: %d\n",s,0,0);

#if defined( notyet ) /* && defined( SO_ERROR ) */
{
	int so_errno;
	ber_socklen_t dummy = sizeof(so_errno);
	if ( getsockopt( s, SOL_SOCKET, SO_ERROR, &so_errno, &dummy )
		== AC_SOCKET_ERROR )
	{
		return -1;
	}
	if ( so_errno ) {
		ldap_pvt_set_errno(so_errno);
		TRACE;
		return -1;
	}
	return 0;
}
#else
{
	/* error slippery */
#ifdef LDAP_PF_INET6
	struct sockaddr_storage sin;
#else
	struct sockaddr_in sin;
#endif
	char ch;
	ber_socklen_t dummy = sizeof(sin);
	if ( getpeername( s, (struct sockaddr *) &sin, &dummy )
		== AC_SOCKET_ERROR )
	{
		/* XXX: needs to be replace with ber_stream_read() */
		(void)read(s, &ch, 1);
		TRACE;
		return -1;
	}
	return 0;
}
#endif
	return -1;
}
#undef TRACE

#endif /* HAVE_WINSOCK */

/* NOTE: this is identical to analogous code in os-local.c */
int
ldap_int_poll(
	LDAP *ld,
	ber_socket_t s,
	struct timeval *tvp )
{
	int		rc;
		

	osip_debug(ld, "ldap_int_poll: fd: %d tm: %ld\n",
		s, tvp ? tvp->tv_sec : -1L, 0);

#ifdef HAVE_POLL
	{
		struct pollfd fd;
		int timeout = INFTIM;

		fd.fd = s;
		fd.events = POLL_WRITE;

		if ( tvp != NULL ) {
			timeout = TV2MILLISEC( tvp );
		}
		do {
			fd.revents = 0;
			rc = poll( &fd, 1, timeout );
		
		} while ( rc == AC_SOCKET_ERROR && errno == EINTR &&
			LDAP_BOOL_GET( &ld->ld_options, LDAP_BOOL_RESTART ) );

		if ( rc == AC_SOCKET_ERROR ) {
			return rc;
		}

		if ( timeout == 0 && rc == 0 ) {
			return -2;
		}

		if ( fd.revents & POLL_WRITE ) {
			if ( ldap_pvt_is_socket_ready( ld, s ) == -1 ) {
				return -1;
			}

			if ( ldap_pvt_ndelay_off( ld, s ) == -1 ) {
				return -1;
			}
			return 0;
		}
	}
#else
	{
		fd_set		wfds, *z = NULL;
#ifdef HAVE_WINSOCK
		fd_set		efds;
#endif
		struct timeval	tv = { 0 };

#if defined( FD_SETSIZE ) && !defined( HAVE_WINSOCK )
		if ( s >= FD_SETSIZE ) {
			rc = AC_SOCKET_ERROR;
			tcp_close( s );
			ldap_pvt_set_errno( EMFILE );
			return rc;
		}
#endif

		if ( tvp != NULL ) {
			tv = *tvp;
		}

		do {
			FD_ZERO(&wfds);
			FD_SET(s, &wfds );

#ifdef HAVE_WINSOCK
			FD_ZERO(&efds);
			FD_SET(s, &efds );
#endif

			rc = select( ldap_int_tblsize, z, &wfds,
#ifdef HAVE_WINSOCK
				&efds,
#else
				z,
#endif
				tvp ? &tv : NULL );
		} while ( rc == AC_SOCKET_ERROR && errno == EINTR &&
			LDAP_BOOL_GET( &ld->ld_options, LDAP_BOOL_RESTART ) );

		if ( rc == AC_SOCKET_ERROR ) {
			return rc;
		}

		if ( rc == 0 && tvp && tvp->tv_sec == 0 && tvp->tv_usec == 0 ) {
			return -2;
		}

#ifdef HAVE_WINSOCK
		/* This means the connection failed */
		if ( FD_ISSET(s, &efds) ) {
			int so_errno;
			ber_socklen_t dummy = sizeof(so_errno);
			if ( getsockopt( s, SOL_SOCKET, SO_ERROR,
				(char *) &so_errno, &dummy ) == AC_SOCKET_ERROR || !so_errno )
			{
				/* impossible */
				so_errno = WSAGetLastError();
			}
			ldap_pvt_set_errno( so_errno );
			osip_debug(ld, "ldap_int_poll: error on socket %d: "
			       "errno: %d (%s)\n", s, errno, sock_errstr( errno ));
			return -1;
		}
#endif
		if ( FD_ISSET(s, &wfds) ) {
#ifndef HAVE_WINSOCK
			if ( ldap_pvt_is_socket_ready( ld, s ) == -1 ) {
				return -1;
			}
#endif
			if ( ldap_pvt_ndelay_off(ld, s) == -1 ) {
				return -1;
			}
			return 0;
		}
	}
#endif

	osip_debug(ld, "ldap_int_poll: timed out\n",0,0,0);
	ldap_pvt_set_errno( ETIMEDOUT );
	return -1;
}

static int
ldap_pvt_connect(LDAP *ld, ber_socket_t s,
	struct sockaddr *sin, ber_socklen_t addrlen,
	int async)
{
	int rc, err;
	struct timeval	tv, *opt_tv = NULL;

#ifdef LDAP_CONNECTIONLESS
	/* We could do a connect() but that would interfere with
	 * attempts to poll a broadcast address
	 */
	if (LDAP_IS_UDP(ld)) {
		if (ld->ld_options.ldo_peer)
			ldap_memfree(ld->ld_options.ldo_peer);
		ld->ld_options.ldo_peer=ldap_memalloc(sizeof(struct sockaddr));
		AC_MEMCPY(ld->ld_options.ldo_peer,sin,sizeof(struct sockaddr));
		return ( 0 );
	}
#endif
	if ( ld->ld_options.ldo_tm_net.tv_sec >= 0 ) {
		tv = ld->ld_options.ldo_tm_net;
		opt_tv = &tv;
	}

	osip_debug(ld, "ldap_pvt_connect: fd: %d tm: %ld async: %d\n",
			s, opt_tv ? tv.tv_sec : -1L, async);

	if ( opt_tv && ldap_pvt_ndelay_on(ld, s) == -1 )
		return ( -1 );

	if ( connect(s, sin, addrlen) != AC_SOCKET_ERROR ) {
		if ( opt_tv && ldap_pvt_ndelay_off(ld, s) == -1 )
			return ( -1 );
		return ( 0 );
	}

	err = sock_errno();
	if ( err != EINPROGRESS && err != EWOULDBLOCK ) {
		return ( -1 );
	}
	
	if ( async ) {
		/* caller will call ldap_int_poll() as appropriate? */
		return ( -2 );
	}

	rc = ldap_int_poll( ld, s, opt_tv );

	osip_debug(ld, "ldap_pvt_connect: %d\n", rc, 0, 0);

	return rc;
}

#ifndef HAVE_INET_ATON
int
ldap_pvt_inet_aton( const char *host, struct in_addr *in)
{
	unsigned long u = inet_addr( host );

#ifdef INADDR_NONE
	if ( u == INADDR_NONE ) return 0;
#endif
	if ( u == 0xffffffffUL || u == (unsigned long) -1L ) return 0;

	in->s_addr = u;
	return 1;
}
#endif

int
ldap_int_connect_cbs(LDAP *ld, Sockbuf *sb, ber_socket_t *s, LDAPURLDesc *srv, struct sockaddr *addr)
{
	struct ldapoptions *lo;
	ldaplist *ll;
	ldap_conncb *cb;
	int rc;

	ber_sockbuf_ctrl( sb, LBER_SB_OPT_SET_FD, s );

	/* Invoke all handle-specific callbacks first */
	lo = &ld->ld_options;
	for (ll = lo->ldo_conn_cbs; ll; ll = ll->ll_next) {
		cb = ll->ll_data;
		rc = cb->lc_add( ld, sb, srv, addr, cb );
		/* on any failure, call the teardown functions for anything
		 * that previously succeeded
		 */
		if ( rc ) {
			ldaplist *l2;
			for (l2 = lo->ldo_conn_cbs; l2 != ll; l2 = l2->ll_next) {
				cb = l2->ll_data;
				cb->lc_del( ld, sb, cb );
			}
			/* a failure might have implicitly closed the fd */
			ber_sockbuf_ctrl( sb, LBER_SB_OPT_GET_FD, s );
			return rc;
		}
	}
	lo = LDAP_INT_GLOBAL_OPT();
	for (ll = lo->ldo_conn_cbs; ll; ll = ll->ll_next) {
		cb = ll->ll_data;
		rc = cb->lc_add( ld, sb, srv, addr, cb );
		if ( rc ) {
			ldaplist *l2;
			for (l2 = lo->ldo_conn_cbs; l2 != ll; l2 = l2->ll_next) {
				cb = l2->ll_data;
				cb->lc_del( ld, sb, cb );
			}
			lo = &ld->ld_options;
			for (l2 = lo->ldo_conn_cbs; l2; l2 = l2->ll_next) {
				cb = l2->ll_data;
				cb->lc_del( ld, sb, cb );
			}
			ber_sockbuf_ctrl( sb, LBER_SB_OPT_GET_FD, s );
			return rc;
		}
	}
	return 0;
}

int
ldap_connect_to_host(LDAP *ld, Sockbuf *sb,
	int proto, LDAPURLDesc *srv,
	int async )
{
	int	rc;
	int	socktype, port;
	ber_socket_t		s = AC_SOCKET_INVALID;
	char *host;

#if defined( HAVE_GETADDRINFO ) && defined( HAVE_INET_NTOP )
	char serv[7];
	int err;
	struct addrinfo hints, *res, *sai;
#else
	int i;
	int use_hp = 0;
	struct hostent *hp = NULL;
	struct hostent he_buf;
	struct in_addr in;
	char *ha_buf=NULL;
#endif

	if ( srv->lud_host == NULL || *srv->lud_host == 0 ) {
		host = "localhost";
	} else {
		host = srv->lud_host;
	}

	port = srv->lud_port;

	if( !port ) {
		if( strcmp(srv->lud_scheme, "ldaps") == 0 ) {
			port = LDAPS_PORT;
		} else {
			port = LDAP_PORT;
		}
	}

	switch(proto) {
	case LDAP_PROTO_TCP: socktype = SOCK_STREAM;
		osip_debug( ld,
			"ldap_connect_to_host: TCP %s:%d\n",
			host, port, 0);
		break;
	case LDAP_PROTO_UDP: socktype = SOCK_DGRAM;
		osip_debug( ld,
			"ldap_connect_to_host: UDP %s:%d\n",
			host, port, 0);
		break;
	default:
		osip_debug( ld, "ldap_connect_to_host: unknown proto: %d\n",
			proto, 0, 0 );
		return -1;
	}

#if defined( HAVE_GETADDRINFO ) && defined( HAVE_INET_NTOP )
	memset( &hints, '\0', sizeof(hints) );
#ifdef USE_AI_ADDRCONFIG /* FIXME: configure test needed */
	/* Use AI_ADDRCONFIG only on systems where its known to be needed. */
	hints.ai_flags = AI_ADDRCONFIG;
#endif
	hints.ai_family = ldap_int_inet4or6;
	hints.ai_socktype = socktype;
	snprintf(serv, sizeof serv, "%d", port );

#ifdef LDAP_R_COMPILE
	/* most getaddrinfo(3) use non-threadsafe resolver libraries */
	ldap_pvt_thread_mutex_lock(&ldap_int_resolv_mutex);
#endif

	err = getaddrinfo( host, serv, &hints, &res );

#ifdef LDAP_R_COMPILE
	ldap_pvt_thread_mutex_unlock(&ldap_int_resolv_mutex);
#endif

	if ( err != 0 ) {
		osip_debug(ld, "ldap_connect_to_host: getaddrinfo failed: %s\n",
			AC_GAI_STRERROR(err), 0, 0);
		return -1;
	}
	rc = -1;

	for( sai=res; sai != NULL; sai=sai->ai_next) {
		if( sai->ai_addr == NULL ) {
			osip_debug(ld, "ldap_connect_to_host: getaddrinfo "
				"ai_addr is NULL?\n", 0, 0, 0);
			continue;
		}

		/* we assume AF_x and PF_x are equal for all x */
		s = ldap_int_socket( ld, sai->ai_family, socktype );
		if ( s == AC_SOCKET_INVALID ) {
			continue;
		}

		if ( ldap_int_prepare_socket(ld, s, proto ) == -1 ) {
			ldap_pvt_close_socket(ld, s);
			break;
		}

		switch (sai->ai_family) {
#ifdef LDAP_PF_INET6
			case AF_INET6: {
				char addr[INET6_ADDRSTRLEN];
				inet_ntop( AF_INET6,
					&((struct sockaddr_in6 *)sai->ai_addr)->sin6_addr,
					addr, sizeof addr);
				osip_debug(ld, "ldap_connect_to_host: Trying %s %s\n", 
					addr, serv, 0);
			} break;
#endif
			case AF_INET: {
				char addr[INET_ADDRSTRLEN];
				inet_ntop( AF_INET,
					&((struct sockaddr_in *)sai->ai_addr)->sin_addr,
					addr, sizeof addr);
				osip_debug(ld, "ldap_connect_to_host: Trying %s:%s\n", 
					addr, serv, 0);
			} break;
		}

		rc = ldap_pvt_connect( ld, s,
			sai->ai_addr, sai->ai_addrlen, async );
		if ( rc == 0 || rc == -2 ) {
			err = ldap_int_connect_cbs( ld, sb, &s, srv, sai->ai_addr );
			if ( err )
				rc = err;
			else
				break;
		}
		ldap_pvt_close_socket(ld, s);
	}
	freeaddrinfo(res);

#else
	if (! inet_aton( host, &in ) ) {
		int local_h_errno;
		rc = ldap_pvt_gethostbyname_a( host, &he_buf, &ha_buf,
			&hp, &local_h_errno );

		if ( (rc < 0) || (hp == NULL) ) {
#ifdef HAVE_WINSOCK
			ldap_pvt_set_errno( WSAGetLastError() );
#else
			/* not exactly right, but... */
			ldap_pvt_set_errno( EHOSTUNREACH );
#endif
			if (ha_buf) LDAP_FREE(ha_buf);
			return -1;
		}

		use_hp = 1;
	}

	rc = s = -1;
	for ( i = 0; !use_hp || (hp->h_addr_list[i] != 0); ++i, rc = -1 ) {
		struct sockaddr_in	sin;

		s = ldap_int_socket( ld, PF_INET, socktype );
		if ( s == AC_SOCKET_INVALID ) {
			/* use_hp ? continue : break; */
			break;
		}
	   
		if ( ldap_int_prepare_socket( ld, s, proto ) == -1 ) {
			ldap_pvt_close_socket(ld, s);
			break;
		}

		(void)memset((char *)&sin, '\0', sizeof sin);
		sin.sin_family = AF_INET;
		sin.sin_port = htons((unsigned short) port);

		if( use_hp ) {
			AC_MEMCPY( &sin.sin_addr, hp->h_addr_list[i],
				sizeof(sin.sin_addr) );
		} else {
			AC_MEMCPY( &sin.sin_addr, &in.s_addr,
				sizeof(sin.sin_addr) );
		}

#ifdef HAVE_INET_NTOA_B
		{
			/* for VxWorks */
			char address[INET_ADDR_LEN];
			inet_ntoa_b(sin.sin_address, address);
			osip_debug(ld, "ldap_connect_to_host: Trying %s:%d\n", 
				address, port, 0);
		}
#else
		osip_debug(ld, "ldap_connect_to_host: Trying %s:%d\n", 
			inet_ntoa(sin.sin_addr), port, 0);
#endif

		rc = ldap_pvt_connect(ld, s,
			(struct sockaddr *)&sin, sizeof(sin),
			async);
   
		if ( (rc == 0) || (rc == -2) ) {
			i = ldap_int_connect_cbs( ld, sb, &s, srv, (struct sockaddr *)&sin );
			if ( i )
				rc = i;
			else
				break;
		}

		ldap_pvt_close_socket(ld, s);

		if (!use_hp) break;
	}
	if (ha_buf) LDAP_FREE(ha_buf);
#endif

	return rc;
}

#if defined( HAVE_CYRUS_SASL )
char *
ldap_host_connected_to( Sockbuf *sb, const char *host )
{
	ber_socklen_t	len;
#ifdef LDAP_PF_INET6
	struct sockaddr_storage sabuf;
#else
	struct sockaddr sabuf;
#endif
	struct sockaddr	*sa = (struct sockaddr *) &sabuf;
	ber_socket_t	sd;

	(void)memset( (char *)sa, '\0', sizeof sabuf );
	len = sizeof sabuf;

	ber_sockbuf_ctrl( sb, LBER_SB_OPT_GET_FD, &sd );
	if ( getpeername( sd, sa, &len ) == -1 ) {
		return( NULL );
	}

	/*
	 * do a reverse lookup on the addr to get the official hostname.
	 * this is necessary for kerberos to work right, since the official
	 * hostname is used as the kerberos instance.
	 */

	switch (sa->sa_family) {
#ifdef LDAP_PF_LOCAL
	case AF_LOCAL:
		return LDAP_STRDUP( ldap_int_hostname );
#endif
#ifdef LDAP_PF_INET6
	case AF_INET6:
		{
			struct in6_addr localhost = IN6ADDR_LOOPBACK_INIT;
			if( memcmp ( &((struct sockaddr_in6 *)sa)->sin6_addr,
				&localhost, sizeof(localhost)) == 0 )
			{
				return LDAP_STRDUP( ldap_int_hostname );
			}
		}
		break;
#endif
	case AF_INET:
		{
			struct in_addr localhost;
			localhost.s_addr = htonl( INADDR_ANY );

			if( memcmp ( &((struct sockaddr_in *)sa)->sin_addr,
				&localhost, sizeof(localhost) ) == 0 )
			{
				return LDAP_STRDUP( ldap_int_hostname );
			}

#ifdef INADDR_LOOPBACK
			localhost.s_addr = htonl( INADDR_LOOPBACK );

			if( memcmp ( &((struct sockaddr_in *)sa)->sin_addr,
				&localhost, sizeof(localhost) ) == 0 )
			{
				return LDAP_STRDUP( ldap_int_hostname );
			}
#endif
		}
		break;

	default:
		return( NULL );
		break;
	}

	{
		char *herr;
#ifdef NI_MAXHOST
		char hbuf[NI_MAXHOST];
#elif defined( MAXHOSTNAMELEN )
		char hbuf[MAXHOSTNAMELEN];
#else
		char hbuf[256];
#endif
		hbuf[0] = 0;

		if (ldap_pvt_get_hname( sa, len, hbuf, sizeof(hbuf), &herr ) == 0
			&& hbuf[0] ) 
		{
			return LDAP_STRDUP( hbuf );   
		}
	}

	return host ? LDAP_STRDUP( host ) : NULL;
}
#endif


struct selectinfo {
#ifdef HAVE_POLL
	/* for UNIX poll(2) */
	int si_maxfd;
	struct pollfd si_fds[FD_SETSIZE];
#else
	/* for UNIX select(2) */
	fd_set	si_readfds;
	fd_set	si_writefds;
	fd_set	si_use_readfds;
	fd_set	si_use_writefds;
#endif
};

void
ldap_mark_select_write( LDAP *ld, Sockbuf *sb )
{
	struct selectinfo	*sip;
	ber_socket_t		sd;

	sip = (struct selectinfo *)ld->ld_selectinfo;
	
	ber_sockbuf_ctrl( sb, LBER_SB_OPT_GET_FD, &sd );

#ifdef HAVE_POLL
	/* for UNIX poll(2) */
	{
		int empty=-1;
		int i;
		for(i=0; i < sip->si_maxfd; i++) {
			if( sip->si_fds[i].fd == sd ) {
				sip->si_fds[i].events |= POLL_WRITE;
				return;
			}
			if( empty==-1 && sip->si_fds[i].fd == -1 ) {
				empty=i;
			}
		}

		if( empty == -1 ) {
			if( sip->si_maxfd >= FD_SETSIZE ) {
				/* FIXME */
				return;
			}
			empty = sip->si_maxfd++;
		}

		sip->si_fds[empty].fd = sd;
		sip->si_fds[empty].events = POLL_WRITE;
	}
#else
	/* for UNIX select(2) */
	if ( !FD_ISSET( sd, &sip->si_writefds )) {
		FD_SET( sd, &sip->si_writefds );
	}
#endif
}


void
ldap_mark_select_read( LDAP *ld, Sockbuf *sb )
{
	struct selectinfo	*sip;
	ber_socket_t		sd;

	sip = (struct selectinfo *)ld->ld_selectinfo;

	ber_sockbuf_ctrl( sb, LBER_SB_OPT_GET_FD, &sd );

#ifdef HAVE_POLL
	/* for UNIX poll(2) */
	{
		int empty=-1;
		int i;
		for(i=0; i < sip->si_maxfd; i++) {
			if( sip->si_fds[i].fd == sd ) {
				sip->si_fds[i].events |= POLL_READ;
				return;
			}
			if( empty==-1 && sip->si_fds[i].fd == -1 ) {
				empty=i;
			}
		}

		if( empty == -1 ) {
			if( sip->si_maxfd >= FD_SETSIZE ) {
				/* FIXME */
				return;
			}
			empty = sip->si_maxfd++;
		}

		sip->si_fds[empty].fd = sd;
		sip->si_fds[empty].events = POLL_READ;
	}
#else
	/* for UNIX select(2) */
	if ( !FD_ISSET( sd, &sip->si_readfds )) {
		FD_SET( sd, &sip->si_readfds );
	}
#endif
}


void
ldap_mark_select_clear( LDAP *ld, Sockbuf *sb )
{
	struct selectinfo	*sip;
	ber_socket_t		sd;

	sip = (struct selectinfo *)ld->ld_selectinfo;

	ber_sockbuf_ctrl( sb, LBER_SB_OPT_GET_FD, &sd );

#ifdef HAVE_POLL
	/* for UNIX poll(2) */
	{
		int i;
		for(i=0; i < sip->si_maxfd; i++) {
			if( sip->si_fds[i].fd == sd ) {
				sip->si_fds[i].fd = -1;
			}
		}
	}
#else
	/* for UNIX select(2) */
	FD_CLR( sd, &sip->si_writefds );
	FD_CLR( sd, &sip->si_readfds );
#endif
}


int
ldap_is_write_ready( LDAP *ld, Sockbuf *sb )
{
	struct selectinfo	*sip;
	ber_socket_t		sd;

	sip = (struct selectinfo *)ld->ld_selectinfo;

	ber_sockbuf_ctrl( sb, LBER_SB_OPT_GET_FD, &sd );

#ifdef HAVE_POLL
	/* for UNIX poll(2) */
	{
		int i;
		for(i=0; i < sip->si_maxfd; i++) {
			if( sip->si_fds[i].fd == sd ) {
				return sip->si_fds[i].revents & POLL_WRITE;
			}
		}

		return 0;
	}
#else
	/* for UNIX select(2) */
	return( FD_ISSET( sd, &sip->si_use_writefds ));
#endif
}


int
ldap_is_read_ready( LDAP *ld, Sockbuf *sb )
{
	struct selectinfo	*sip;
	ber_socket_t		sd;

	sip = (struct selectinfo *)ld->ld_selectinfo;

	if (ber_sockbuf_ctrl( sb, LBER_SB_OPT_DATA_READY, NULL ))
		return 1;

	ber_sockbuf_ctrl( sb, LBER_SB_OPT_GET_FD, &sd );

#ifdef HAVE_POLL
	/* for UNIX poll(2) */
	{
		int i;
		for(i=0; i < sip->si_maxfd; i++) {
			if( sip->si_fds[i].fd == sd ) {
				return sip->si_fds[i].revents & POLL_READ;
			}
		}

		return 0;
	}
#else
	/* for UNIX select(2) */
	return( FD_ISSET( sd, &sip->si_use_readfds ));
#endif
}


void *
ldap_new_select_info( void )
{
	struct selectinfo	*sip;

	sip = (struct selectinfo *)LDAP_CALLOC( 1, sizeof( struct selectinfo ));

	if ( sip == NULL ) return NULL;

#ifdef HAVE_POLL
	/* for UNIX poll(2) */
	/* sip->si_maxfd=0 */
#else
	/* for UNIX select(2) */
	FD_ZERO( &sip->si_readfds );
	FD_ZERO( &sip->si_writefds );
#endif

	return( (void *)sip );
}


void
ldap_free_select_info( void *sip )
{
	LDAP_FREE( sip );
}


#ifndef HAVE_POLL
int ldap_int_tblsize = 0;

void
ldap_int_ip_init( void )
{
#if defined( HAVE_SYSCONF )
	long tblsize = sysconf( _SC_OPEN_MAX );
	if( tblsize > INT_MAX ) tblsize = INT_MAX;

#elif defined( HAVE_GETDTABLESIZE )
	int tblsize = getdtablesize();
#else
	int tblsize = FD_SETSIZE;
#endif /* !USE_SYSCONF */

#ifdef FD_SETSIZE
	if( tblsize > FD_SETSIZE ) tblsize = FD_SETSIZE;
#endif	/* FD_SETSIZE */

	ldap_int_tblsize = tblsize;
}
#endif


int
ldap_int_select( LDAP *ld, struct timeval *timeout )
{
	int rc;
	struct selectinfo	*sip;

	Debug( LDAP_DEBUG_TRACE, "ldap_int_select\n", 0, 0, 0 );

#ifndef HAVE_POLL
	if ( ldap_int_tblsize == 0 ) ldap_int_ip_init();
#endif

	sip = (struct selectinfo *)ld->ld_selectinfo;
	assert( sip != NULL );

#ifdef HAVE_POLL
	{
		int to = timeout ? TV2MILLISEC( timeout ) : INFTIM;
		rc = poll( sip->si_fds, sip->si_maxfd, to );
	}
#else
	sip->si_use_readfds = sip->si_readfds;
	sip->si_use_writefds = sip->si_writefds;
	
	rc = select( ldap_int_tblsize,
		&sip->si_use_readfds, &sip->si_use_writefds,
		NULL, timeout );
#endif

	return rc;
}
