/*	$NetBSD: af_inet6.c,v 1.13 2008/05/08 07:13:20 dyoung Exp $	*/

/*
 * Copyright (c) 1983, 1993
 *      The Regents of the University of California.  All rights reserved.
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
 */

#include <sys/cdefs.h>
#ifndef lint
__RCSID("$NetBSD: af_inet6.c,v 1.13 2008/05/08 07:13:20 dyoung Exp $");
#endif /* not lint */

#include <sys/param.h> 
#include <sys/ioctl.h> 
#include <sys/socket.h>

#include <net/if.h> 
#include <netinet/in.h>
#include <netinet/in_var.h>
#include <netinet6/nd6.h>

#include <err.h>
#include <errno.h>
#include <ifaddrs.h>
#include <netdb.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <util.h>

#include "env.h"
#include "parse.h"
#include "extern.h"
#include "af_inet6.h"
#include "af_inetany.h"

struct in6_ifreq    in6_ridreq = {
	.ifr_addr = {
		.sin6_family = AF_INET6,
		.sin6_addr = {
			.s6_addr =
			    {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff}
		}
	}
};

struct in6_aliasreq in6_addreq = {
	.ifra_prefixmask = {
		.sin6_len = sizeof(in6_addreq.ifra_prefixmask),
		.sin6_addr = {
			.s6_addr =
			    {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff}}},
	.ifra_lifetime = {
		  .ia6t_pltime = ND6_INFINITE_LIFETIME
		, .ia6t_vltime = ND6_INFINITE_LIFETIME
	}
};

static const struct kwinst ia6flagskw[] = {
	  IFKW("anycast",	IN6_IFF_ANYCAST)
	, IFKW("tentative",	IN6_IFF_TENTATIVE)
	, IFKW("deprecated",	IN6_IFF_DEPRECATED)
};

static struct pinteger parse_pltime = PINTEGER_INITIALIZER(&parse_pltime,
    "pltime", 0, setia6pltime, "pltime", &command_root.pb_parser);

static struct pinteger parse_vltime = PINTEGER_INITIALIZER(&parse_vltime,
    "vltime", 0, setia6vltime, "vltime", &command_root.pb_parser);

static const struct kwinst inet6kw[] = {
	  {.k_word = "pltime", .k_nextparser = &parse_pltime.pi_parser}
	, {.k_word = "vltime", .k_nextparser = &parse_vltime.pi_parser}
	, {.k_word = "eui64", .k_exec = setia6eui64,
	   .k_nextparser = &command_root.pb_parser}
};

struct pkw ia6flags = PKW_INITIALIZER(&ia6flags, "ia6flags", setia6flags,
    "ia6flag", ia6flagskw, __arraycount(ia6flagskw), &command_root.pb_parser);
struct pkw inet6 = PKW_INITIALIZER(&inet6, "IPv6 keywords", NULL,
    NULL, inet6kw, __arraycount(inet6kw), NULL);

static int setia6lifetime(prop_dictionary_t, int64_t, time_t *, uint32_t *);
static void in6_alias(const char *, prop_dictionary_t, prop_dictionary_t,
    struct in6_ifreq *);

static char *
sec2str(time_t total)
{
	static char result[256];
	int days, hours, mins, secs;
	int first = 1;
	char *p = result;
	char *end = &result[sizeof(result)];
	int n;

	if (0) {	/*XXX*/
		days = total / 3600 / 24;
		hours = (total / 3600) % 24;
		mins = (total / 60) % 60;
		secs = total % 60;

		if (days) {
			first = 0;
			n = snprintf(p, end - p, "%dd", days);
			if (n < 0 || n >= end - p)
				return(result);
			p += n;
		}
		if (!first || hours) {
			first = 0;
			n = snprintf(p, end - p, "%dh", hours);
			if (n < 0 || n >= end - p)
				return(result);
			p += n;
		}
		if (!first || mins) {
			first = 0;
			n = snprintf(p, end - p, "%dm", mins);
			if (n < 0 || n >= end - p)
				return(result);
			p += n;
		}
		snprintf(p, end - p, "%ds", secs);
	} else
		snprintf(p, end - p, "%lu", (u_long)total);

	return(result);
}

static int
prefix(void *val, int size)
{
	u_char *pname = (u_char *)val;
	int byte, bit, plen = 0;

	for (byte = 0; byte < size; byte++, plen += 8)
		if (pname[byte] != 0xff)
			break;
	if (byte == size)
		return (plen);
	for (bit = 7; bit != 0; bit--, plen++)
		if (!(pname[byte] & (1 << bit)))
			break;
	for (; bit != 0; bit--)
		if (pname[byte] & (1 << bit))
			return(0);
	byte++;
	for (; byte < size; byte++)
		if (pname[byte])
			return(0);
	return (plen);
}

int
setia6flags_impl(prop_dictionary_t env, struct in6_aliasreq *ifra)
{
	int64_t ia6flag;

	if (!prop_dictionary_get_int64(env, "ia6flag", &ia6flag)) {
		errno = ENOENT;
		return -1;
	}

	if (ia6flag < 0) {
		ia6flag = -ia6flag;
		ifra->ifra_flags &= ~ia6flag;
	} else
		ifra->ifra_flags |= ia6flag;
	return 0;
}

int
setia6flags(prop_dictionary_t env, prop_dictionary_t xenv)
{
	return setia6flags_impl(env, &in6_addreq);
}

int
setia6pltime_impl(prop_dictionary_t env, struct in6_aliasreq *ifra)
{
	int64_t pltime;

	if (!prop_dictionary_get_int64(env, "pltime", &pltime)) {
		errno = ENOENT;
		return -1;
	}

	return setia6lifetime(env, pltime,
	    &ifra->ifra_lifetime.ia6t_preferred,
	    &ifra->ifra_lifetime.ia6t_pltime);
}

int
setia6pltime(prop_dictionary_t env, prop_dictionary_t xenv)
{
	return setia6pltime_impl(env, &in6_addreq);
}

int
setia6vltime_impl(prop_dictionary_t env, struct in6_aliasreq *ifra)
{
	int64_t vltime;

	if (!prop_dictionary_get_int64(env, "vltime", &vltime)) {
		errno = ENOENT;
		return -1;
	}

	return setia6lifetime(env, vltime,
		&ifra->ifra_lifetime.ia6t_expire,
		&ifra->ifra_lifetime.ia6t_vltime);
}

int
setia6vltime(prop_dictionary_t env, prop_dictionary_t xenv)
{
	return setia6vltime_impl(env, &in6_addreq);
}

static int
setia6lifetime(prop_dictionary_t env, int64_t val, time_t *timep,
    uint32_t *ivalp)
{
	time_t t;
	int af;

	if ((af = getaf(env)) == -1 || af != AF_INET6) {
		errx(EXIT_FAILURE,
		    "inet6 address lifetime not allowed for the AF");
	}

	t = time(NULL);
	*timep = t + val;
	*ivalp = val;
	return 0;
}

int
setia6eui64_impl(prop_dictionary_t env, struct in6_aliasreq *ifra)
{
	struct ifaddrs *ifap, *ifa;
	const struct sockaddr_in6 *sin6 = NULL;
	const struct in6_addr *lladdr = NULL;
	struct in6_addr *in6;
	const char *ifname;
	int af;

	if ((ifname = getifname(env)) == NULL)
		return -1;

	af = getaf(env);
	if (af != AF_INET6) {
		errx(EXIT_FAILURE,
		    "eui64 address modifier not allowed for the AF");
	}
 	in6 = &ifra->ifra_addr.sin6_addr;
	if (memcmp(&in6addr_any.s6_addr[8], &in6->s6_addr[8], 8) != 0)
		errx(EXIT_FAILURE, "interface index is already filled");
	if (getifaddrs(&ifap) != 0)
		err(EXIT_FAILURE, "getifaddrs");
	for (ifa = ifap; ifa; ifa = ifa->ifa_next) {
		if (ifa->ifa_addr->sa_family == AF_INET6 &&
		    strcmp(ifa->ifa_name, ifname) == 0) {
			sin6 = (const struct sockaddr_in6 *)ifa->ifa_addr;
			if (IN6_IS_ADDR_LINKLOCAL(&sin6->sin6_addr)) {
				lladdr = &sin6->sin6_addr;
				break;
			}
		}
	}
	if (!lladdr)
		errx(EXIT_FAILURE, "could not determine link local address"); 

 	memcpy(&in6->s6_addr[8], &lladdr->s6_addr[8], 8);

	freeifaddrs(ifap);
	return 0;
}

int
setia6eui64(prop_dictionary_t env, prop_dictionary_t xenv)
{
	return setia6eui64_impl(env, &in6_addreq);
}

void
in6_fillscopeid(struct sockaddr_in6 *sin6)
{
	/* KAME idiosyncrasy */
	if (IN6_IS_ADDR_LINKLOCAL(&sin6->sin6_addr)) {
		sin6->sin6_scope_id =
			ntohs(*(u_int16_t *)&sin6->sin6_addr.s6_addr[2]);
		sin6->sin6_addr.s6_addr[2] = sin6->sin6_addr.s6_addr[3] = 0;
	}
}

/* XXX not really an alias */
void
in6_alias(const char *ifname, prop_dictionary_t env, prop_dictionary_t oenv,
    struct in6_ifreq *creq)
{
	struct in6_ifreq ifr6;
	struct sockaddr_in6 *sin6;
	char hbuf[NI_MAXHOST];
	u_int32_t scopeid;
	int s;
	const int niflag = NI_NUMERICHOST;
	unsigned short flags;

	/* Get the non-alias address for this interface. */
	if ((s = getsock(AF_INET6)) == -1) {
		if (errno == EAFNOSUPPORT)
			return;
		err(EXIT_FAILURE, "socket");
	}

	sin6 = (struct sockaddr_in6 *)&creq->ifr_addr;

	in6_fillscopeid(sin6);
	scopeid = sin6->sin6_scope_id;
	if (getnameinfo((struct sockaddr *)sin6, sin6->sin6_len,
			hbuf, sizeof(hbuf), NULL, 0, niflag))
		strlcpy(hbuf, "", sizeof(hbuf));	/* some message? */
	printf("\tinet6 %s", hbuf);

	if (getifflags(env, oenv, &flags) == -1)
		err(EXIT_FAILURE, "%s: getifflags", __func__);

	if (flags & IFF_POINTOPOINT) {
		memset(&ifr6, 0, sizeof(ifr6));
		estrlcpy(ifr6.ifr_name, ifname, sizeof(ifr6.ifr_name));
		ifr6.ifr_addr = creq->ifr_addr;
		if (ioctl(s, SIOCGIFDSTADDR_IN6, &ifr6) == -1) {
			if (errno != EADDRNOTAVAIL)
				warn("SIOCGIFDSTADDR_IN6");
			memset(&ifr6.ifr_addr, 0, sizeof(ifr6.ifr_addr));
			ifr6.ifr_addr.sin6_family = AF_INET6;
			ifr6.ifr_addr.sin6_len = sizeof(struct sockaddr_in6);
		}
		sin6 = (struct sockaddr_in6 *)&ifr6.ifr_addr;
		in6_fillscopeid(sin6);
		hbuf[0] = '\0';
		if (getnameinfo((struct sockaddr *)sin6, sin6->sin6_len,
				hbuf, sizeof(hbuf), NULL, 0, niflag))
			strlcpy(hbuf, "", sizeof(hbuf)); /* some message? */
		printf(" -> %s", hbuf);
	}

	memset(&ifr6, 0, sizeof(ifr6));
	estrlcpy(ifr6.ifr_name, ifname, sizeof(ifr6.ifr_name));
	ifr6.ifr_addr = creq->ifr_addr;
	if (ioctl(s, SIOCGIFNETMASK_IN6, &ifr6) == -1) {
		if (errno != EADDRNOTAVAIL)
			warn("SIOCGIFNETMASK_IN6");
	} else {
		sin6 = (struct sockaddr_in6 *)&ifr6.ifr_addr;
		printf(" prefixlen %d", prefix(&sin6->sin6_addr,
					       sizeof(struct in6_addr)));
	}

	memset(&ifr6, 0, sizeof(ifr6));
	estrlcpy(ifr6.ifr_name, ifname, sizeof(ifr6.ifr_name));
	ifr6.ifr_addr = creq->ifr_addr;
	if (ioctl(s, SIOCGIFAFLAG_IN6, &ifr6) == -1) {
		if (errno != EADDRNOTAVAIL)
			warn("SIOCGIFAFLAG_IN6");
	} else {
		if (ifr6.ifr_ifru.ifru_flags6 & IN6_IFF_ANYCAST)
			printf(" anycast");
		if (ifr6.ifr_ifru.ifru_flags6 & IN6_IFF_TENTATIVE)
			printf(" tentative");
		if (ifr6.ifr_ifru.ifru_flags6 & IN6_IFF_DUPLICATED)
			printf(" duplicated");
		if (ifr6.ifr_ifru.ifru_flags6 & IN6_IFF_DETACHED)
			printf(" detached");
		if (ifr6.ifr_ifru.ifru_flags6 & IN6_IFF_DEPRECATED)
			printf(" deprecated");
	}

	if (scopeid)
		printf(" scopeid 0x%x", scopeid);

	if (Lflag) {
		struct in6_addrlifetime *lifetime;
		memset(&ifr6, 0, sizeof(ifr6));
		estrlcpy(ifr6.ifr_name, ifname, sizeof(ifr6.ifr_name));
		ifr6.ifr_addr = creq->ifr_addr;
		lifetime = &ifr6.ifr_ifru.ifru_lifetime;
		if (ioctl(s, SIOCGIFALIFETIME_IN6, &ifr6) == -1) {
			if (errno != EADDRNOTAVAIL)
				warn("SIOCGIFALIFETIME_IN6");
		} else if (lifetime->ia6t_preferred || lifetime->ia6t_expire) {
			time_t t = time(NULL);
			printf(" pltime ");
			if (lifetime->ia6t_preferred) {
				printf("%s", lifetime->ia6t_preferred < t
					? "0"
					: sec2str(lifetime->ia6t_preferred - t));
			} else
				printf("infty");

			printf(" vltime ");
			if (lifetime->ia6t_expire) {
				printf("%s", lifetime->ia6t_expire < t
					? "0"
					: sec2str(lifetime->ia6t_expire - t));
			} else
				printf("infty");
		}
	}

	printf("\n");
}

void
in6_status(prop_dictionary_t env, prop_dictionary_t oenv, bool force)
{
	struct ifaddrs *ifap, *ifa;
	struct in6_ifreq ifr;
	const char *ifname;

	if ((ifname = getifname(env)) == NULL)
		err(EXIT_FAILURE, "%s: getifname", __func__);

	if (getifaddrs(&ifap) != 0)
		err(EXIT_FAILURE, "getifaddrs");
	for (ifa = ifap; ifa; ifa = ifa->ifa_next) {
		if (strcmp(ifname, ifa->ifa_name) != 0)
			continue;
		if (ifa->ifa_addr->sa_family != AF_INET6)
			continue;
		if (sizeof(ifr.ifr_addr) < ifa->ifa_addr->sa_len)
			continue;

		memset(&ifr, 0, sizeof(ifr));
		estrlcpy(ifr.ifr_name, ifa->ifa_name, sizeof(ifr.ifr_name));
		memcpy(&ifr.ifr_addr, ifa->ifa_addr, ifa->ifa_addr->sa_len);
		in6_alias(ifname, env, oenv, &ifr);
	}
	freeifaddrs(ifap);
}

#define SIN6(x) ((struct sockaddr_in6 *) &(x))
struct sockaddr_in6 *sin6tab[] = {
    SIN6(in6_ridreq.ifr_addr), SIN6(in6_addreq.ifra_addr),
    SIN6(in6_addreq.ifra_prefixmask), SIN6(in6_addreq.ifra_dstaddr)};

void
in6_getaddr(const struct paddr_prefix *pfx, int which)
{
	struct sockaddr_in6 *sin6 = sin6tab[which];

	if (pfx->pfx_addr.sa_family != AF_INET6)
		errx(EXIT_FAILURE, "%s: address family mismatch", __func__);

	if (which == ADDR && pfx->pfx_len >= 0)
		in6_getprefix(pfx->pfx_len, MASK);

	memcpy(sin6, &pfx->pfx_addr, MIN(sizeof(*sin6), pfx->pfx_addr.sa_len));

	/* KAME idiosyncrasy */
	if (IN6_IS_ADDR_LINKLOCAL(&sin6->sin6_addr) && sin6->sin6_scope_id) {
		*(u_int16_t *)&sin6->sin6_addr.s6_addr[2] =
			htons(sin6->sin6_scope_id);
		sin6->sin6_scope_id = 0;
	}
}

void
in6_getprefix(int len, int which)
{
	struct sockaddr_in6 *gpsin = sin6tab[which];
	u_char *cp;

	if (len < 0 || len > 128)
		errx(EXIT_FAILURE, "%d: bad value", len);
	gpsin->sin6_len = sizeof(*gpsin);
	if (which != MASK)
		gpsin->sin6_family = AF_INET6;
	if (len == 0 || len == 128) {
		memset(&gpsin->sin6_addr, 0xff, sizeof(struct in6_addr));
		return;
	}
	memset((void *)&gpsin->sin6_addr, 0x00, sizeof(gpsin->sin6_addr));
	for (cp = (u_char *)&gpsin->sin6_addr; len > 7; len -= 8)
		*cp++ = 0xff;
	if (len)
		*cp = 0xff << (8 - len);
}

static int
in6_pre_aifaddr(prop_dictionary_t env, void *arg)
{
	struct in6_aliasreq *ifra = arg;

	setia6eui64_impl(env, ifra);
	setia6vltime_impl(env, ifra);
	setia6pltime_impl(env, ifra);
	setia6flags_impl(env, ifra);

	return 0;
}

void
in6_commit_address(prop_dictionary_t env, prop_dictionary_t oenv)
{
	struct in6_ifreq in6_ifr;
#if 0
	 = {
		.ifr_addr = {
			.sin6_family = AF_INET6,
			.sin6_addr = {
				.s6_addr =
				    {0xff, 0xff, 0xff, 0xff,
				     0xff, 0xff, 0xff, 0xff}
			}
		}
	};
#endif
	static struct sockaddr_in6 in6_defmask = {
		.sin6_addr = {
			.s6_addr = {0xff, 0xff, 0xff, 0xff,
			            0xff, 0xff, 0xff, 0xff}
		}
	};

	struct in6_aliasreq in6_ifra;
#if 0
	 = {
		.ifra_prefixmask = {
			.sin6_addr = {
				.s6_addr =
				    {0xff, 0xff, 0xff, 0xff,
				     0xff, 0xff, 0xff, 0xff}}},
		.ifra_lifetime = {
			  .ia6t_pltime = ND6_INFINITE_LIFETIME
			, .ia6t_vltime = ND6_INFINITE_LIFETIME
		}
	};
#endif
	struct afparam in6param = {
		  .req = BUFPARAM(in6_ifra)
		, .dgreq = BUFPARAM(in6_ifr)
		, .name = {
			{.buf = in6_ifr.ifr_name,
			 .buflen = sizeof(in6_ifr.ifr_name)},
			{.buf = in6_ifra.ifra_name,
			 .buflen = sizeof(in6_ifra.ifra_name)}
		  }
		, .dgaddr = BUFPARAM(in6_ifr.ifr_addr)
		, .addr = BUFPARAM(in6_ifra.ifra_addr)
		, .dst = BUFPARAM(in6_ifra.ifra_dstaddr)
		, .brd = BUFPARAM(in6_ifra.ifra_broadaddr)
		, .mask = BUFPARAM(in6_ifra.ifra_prefixmask)
		, .aifaddr = IFADDR_PARAM(SIOCAIFADDR_IN6)
		, .difaddr = IFADDR_PARAM(SIOCDIFADDR_IN6)
		, .gifaddr = IFADDR_PARAM(SIOCGIFADDR_IN6)
		, .defmask = BUFPARAM(in6_defmask)
		, .pre_aifaddr = in6_pre_aifaddr
		, .pre_aifaddr_arg = BUFPARAM(in6_ifra)
	};
	commit_address(env, oenv, &in6param);
}
