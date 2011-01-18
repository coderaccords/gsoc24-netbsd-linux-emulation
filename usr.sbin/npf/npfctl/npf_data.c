/*	$NetBSD: npf_data.c,v 1.6 2011/01/18 20:33:45 rmind Exp $	*/

/*-
 * Copyright (c) 2009-2011 The NetBSD Foundation, Inc.
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
 * NPF proplib(9) dictionary producer.
 *
 * XXX: Needs some clean-up.
 */

#include <sys/cdefs.h>
__RCSID("$NetBSD: npf_data.c,v 1.6 2011/01/18 20:33:45 rmind Exp $");

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <netinet/tcp.h>

#include <arpa/inet.h>
#include <prop/proplib.h>

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <err.h>
#include <ifaddrs.h>
#include <netdb.h>
#include <assert.h>

#include "npfctl.h"

static struct ifaddrs *		ifs_list = NULL;

static prop_dictionary_t	npf_dict, settings_dict;
static prop_array_t		nat_arr, tables_arr, rproc_arr, rules_arr;

static pri_t			gr_prio_counter = 1;
static pri_t			rl_prio_counter = 1;
static pri_t			nat_prio_counter = 1;
static u_int			rproc_id_counter = 1;

void
npfctl_init_data(void)
{

	if (getifaddrs(&ifs_list) == -1)
		err(EXIT_FAILURE, "getifaddrs");

	npf_dict = prop_dictionary_create();

	nat_arr = prop_array_create();
	prop_dictionary_set(npf_dict, "translation", nat_arr);

	settings_dict = prop_dictionary_create();
	prop_dictionary_set(npf_dict, "settings", settings_dict);

	tables_arr = prop_array_create();
	prop_dictionary_set(npf_dict, "tables", tables_arr);

	rproc_arr = prop_array_create();
	prop_dictionary_set(npf_dict, "rprocs", rproc_arr);

	rules_arr = prop_array_create();
	prop_dictionary_set(npf_dict, "rules", rules_arr);
}

int
npfctl_ioctl_send(int fd)
{
	int ret = 0, errval;

#ifdef _NPF_TESTING
	prop_dictionary_externalize_to_file(npf_dict, "./npf.plist");
#else
	errval = prop_dictionary_send_ioctl(npf_dict, fd, IOC_NPF_RELOAD);
	if (errval) {
		errx(EXIT_FAILURE, "npfctl_ioctl_send: %s\n", strerror(errval));
	}
#endif
	prop_object_release(npf_dict);
	return ret;
}

int
npfctl_ioctl_flushse(int fd)
{
	prop_dictionary_t sesdict;
	prop_array_t selist;
	int errval;

	sesdict = prop_dictionary_create();
	selist = prop_array_create();
	prop_dictionary_set(sesdict, "session-list", selist);
	errval = prop_dictionary_send_ioctl(sesdict, fd, IOC_NPF_SESSIONS_LOAD);
	if (errval) {
		errx(EXIT_FAILURE, "npfctl_ioctl_flushse: %s\n",
		    strerror(errval));
	}
	prop_object_release(sesdict);
	return errval;
}

int
npfctl_ioctl_sendse(int fd)
{
	prop_dictionary_t sesdict;
	int error;

	sesdict = prop_dictionary_internalize_from_file(NPF_SESSDB_PATH);
	if (sesdict == NULL) {
		errx(EXIT_FAILURE, "npfctl: no sessions saved "
		    "('%s' does not exist)", NPF_SESSDB_PATH);
	}
	error = prop_dictionary_send_ioctl(sesdict, fd, IOC_NPF_SESSIONS_LOAD);
	prop_object_release(sesdict);
	if (error) {
		err(EXIT_FAILURE, "npfctl_ioctl_sendse");
	}
	return 0;
}

int
npfctl_ioctl_recvse(int fd)
{
	prop_dictionary_t sesdict;
	int error;

	error = prop_dictionary_recv_ioctl(fd, IOC_NPF_SESSIONS_SAVE, &sesdict);
	if (error) {
		err(EXIT_FAILURE, "prop_array_recv_ioctl");
	}
	if (!prop_dictionary_externalize_to_file(sesdict, NPF_SESSDB_PATH)) {
		errx(EXIT_FAILURE, "could not save to '%s'", NPF_SESSDB_PATH);
	}
	prop_object_release(sesdict);
	return 0;
}

/*
 * Helper routines:
 *
 *	npfctl_getif() - get interface addresses and index number from name.
 *	npfctl_parse_v4mask() - parse address/mask integers from CIDR block.
 *	npfctl_parse_port() - parse port number (which may be a service name).
 *	npfctl_parse_tcpfl() - parse TCP flags.
 */

struct ifaddrs *
npfctl_getif(char *ifname, unsigned int *if_idx, bool reqaddr)
{
	struct ifaddrs *ifent;
	struct sockaddr_in *sin;

	for (ifent = ifs_list; ifent != NULL; ifent = ifent->ifa_next) {
		sin = (struct sockaddr_in *)ifent->ifa_addr;
		if (sin->sin_family != AF_INET && reqaddr)
			continue;
		if (strcmp(ifent->ifa_name, ifname) == 0)
			break;
	}
	if (ifent) {
		*if_idx = if_nametoindex(ifname);
	}
	return ifent;
}

bool
npfctl_parse_v4mask(char *ostr, in_addr_t *addr, in_addr_t *mask)
{
	char *str = xstrdup(ostr);
	char *p = strchr(str, '/');
	u_int bits;
	bool ret;

	/* In network byte order. */
	if (p) {
		*p++ = '\0';
		bits = (u_int)atoi(p);
		*mask = bits ? htonl(0xffffffff << (32 - bits)) : 0;
	} else {
		*mask = 0xffffffff;
	}
	ret = inet_aton(str, (struct in_addr *)addr) != 0;
	free(str);
	return ret;
}

static bool
npfctl_parse_port(char *ostr, bool *range, in_port_t *fport, in_port_t *tport)
{
	char *str = xstrdup(ostr), *sep;

	*range = false;
	if ((sep = strchr(str, ':')) != NULL) {
		/* Port range (only numeric). */
		*range = true;
		*sep = '\0';

	} else if (isalpha((unsigned char)*str)) {
		struct servent *se;

		se = getservbyname(str, NULL);
		if (se == NULL) {
			free(str);
			return false;
		}
		*fport = se->s_port;
	} else {
		*fport = htons(atoi(str));
	}
	*tport = sep ? htons(atoi(sep + 1)) : *fport;
	free(str);
	return true;
}

static void
npfctl_parse_cidr(char *str, in_addr_t *addr, in_addr_t *mask)
{

	if (strcmp(str, "any") == 0) {
		*addr = 0x0;
		*mask = 0x0;

	} else if (isalpha((unsigned char)*str)) {
		struct ifaddrs *ifa;
		struct sockaddr_in *sin;
		u_int idx;

		if ((ifa = npfctl_getif(str, &idx, true)) == NULL) {
			errx(EXIT_FAILURE, "invalid interface '%s'", str);
		}
		/* Interface address. */
		sin = (struct sockaddr_in *)ifa->ifa_addr;
		*addr = sin->sin_addr.s_addr;
		*mask = 0xffffffff;

	} else if (!npfctl_parse_v4mask(str, addr, mask)) {
		errx(EXIT_FAILURE, "invalid CIDR '%s'\n", str);
	}
}

static bool
npfctl_parse_tcpfl(char *s, uint8_t *tfl, uint8_t *tfl_mask)
{
	uint8_t tcpfl = 0;
	bool mask = false;

	while (*s) {
		switch (*s) {
		case 'F': tcpfl |= TH_FIN; break;
		case 'S': tcpfl |= TH_SYN; break;
		case 'R': tcpfl |= TH_RST; break;
		case 'P': tcpfl |= TH_PUSH; break;
		case 'A': tcpfl |= TH_ACK; break;
		case 'U': tcpfl |= TH_URG; break;
		case 'E': tcpfl |= TH_ECE; break;
		case 'W': tcpfl |= TH_CWR; break;
		case '/':
			*s = '\0';
			*tfl = tcpfl;
			tcpfl = 0;
			mask = true;
			break;
		default:
			return false;
		}
		s++;
	}
	if (!mask) {
		*tfl = tcpfl;
	}
	*tfl_mask = tcpfl;
	return true;
}

/*
 * NPF table creation and construction routines.
 */

prop_dictionary_t
npfctl_lookup_table(char *tidstr)
{
	prop_dictionary_t tl;
	prop_object_iterator_t it;
	prop_object_t obj;
	u_int tid;

	tid = atoi(tidstr);
	it = prop_array_iterator(tables_arr);
	while ((tl = prop_object_iterator_next(it)) != NULL) {
		obj = prop_dictionary_get(tl, "id");
		if (tid == prop_number_integer_value(obj))
			break;
	}
	return tl;
}

prop_dictionary_t
npfctl_construct_table(int id, int type)
{
	prop_dictionary_t tl;

	tl = prop_dictionary_create();
	/* TODO: 1. check ID range 2. check if not a duplicate */
	prop_dictionary_set(tl, "id", prop_number_create_integer(id));
	prop_dictionary_set(tl, "type", prop_number_create_integer(type));
	prop_dictionary_set(tl, "entries", prop_array_create());
	prop_array_add(tables_arr, tl);
	return tl;
}

void
npfctl_fill_table(prop_dictionary_t tl, char *fname)
{
	prop_dictionary_t entdict;
	prop_array_t tblents;
	char *buf;
	FILE *fp;
	size_t n;
	int l;

	tblents = prop_dictionary_get(tl, "entries");
	assert(tblents != NULL);

	fp = fopen(fname, "r");
	if (fp == NULL) {
		err(EXIT_FAILURE, "open '%s'", fname);
	}
	l = 1;
	buf = NULL;
	while (getline(&buf, &n, fp) != -1) {
		in_addr_t addr, mask;

		if (*buf == '\n' || *buf == '#')
			continue;

		/* IPv4 CIDR: a.b.c.d/mask */
		if (!npfctl_parse_v4mask(buf, &addr, &mask))
			errx(EXIT_FAILURE, "invalid table entry at line %d", l);

		/* Create and add table entry. */
		entdict = prop_dictionary_create();
		prop_dictionary_set(entdict, "addr",
		    prop_number_create_integer(addr));
		prop_dictionary_set(entdict, "mask",
		    prop_number_create_integer(mask));
		prop_array_add(tblents, entdict);
		l++;
	}
	if (buf != NULL) {
		free(buf);
	}
}

/*
 * npfctl_mk_rule: create a rule (or group) dictionary.
 *
 * Note: group is a rule containing subrules.  It has no n-code, however.
 */
prop_dictionary_t
npfctl_mk_rule(bool group, prop_dictionary_t parent)
{
	prop_dictionary_t rl;
	prop_array_t subrl, rlset;
	pri_t pri;

	rl = prop_dictionary_create();
	if (group) {
		subrl = prop_array_create();
		prop_dictionary_set(rl, "subrules", subrl);
		/* Give new priority, reset rule priority counter. */
		pri = gr_prio_counter++;
		rl_prio_counter = 1;
	} else {
		pri = rl_prio_counter++;
	}
	prop_dictionary_set(rl, "priority", prop_number_create_integer(pri));

	if (parent) {
		rlset = prop_dictionary_get(parent, "subrules");
		assert(rlset != NULL);
	} else {
		rlset = rules_arr;
	}
	prop_array_add(rlset, rl);
	return rl;
}

void
npfctl_rule_setattr(prop_dictionary_t rl, int attr, u_int iface)
{
	prop_number_t attrnum, ifnum;

	attrnum = prop_number_create_integer(attr);
	prop_dictionary_set(rl, "attributes", attrnum);
	if (iface) {
		ifnum = prop_number_create_integer(iface);
		prop_dictionary_set(rl, "interface", ifnum);
	}
}

/*
 * Main rule generation routines.
 */

static void
npfctl_rulenc_v4cidr(void **nc, int nblocks[], var_t *dat, bool sd)
{
	element_t *el = dat->v_elements;
	int foff;

	/* If table, generate a single table matching block. */
	if (dat->v_type == VAR_TABLE) {
		u_int tid = atoi(el->e_data);

		nblocks[0]--;
		foff = npfctl_failure_offset(nblocks);
		npfctl_gennc_tbl(nc, foff, tid, sd);
		return;
	}

	/* Generate v4 CIDR matching blocks. */
	for (el = dat->v_elements; el != NULL; el = el->e_next) {
		in_addr_t addr, mask;

		npfctl_parse_cidr(el->e_data, &addr, &mask);

		nblocks[1]--;
		foff = npfctl_failure_offset(nblocks);
		npfctl_gennc_v4cidr(nc, foff, addr, mask, sd);
	}
}

static void
npfctl_rulenc_ports(void **nc, int nblocks[], var_t *dat, bool tcpudp,
    bool both, bool sd)
{
	element_t *el = dat->v_elements;
	int foff;

	assert(dat->v_type != VAR_TABLE);

	/* Generate TCP/UDP port matching blocks. */
	for (el = dat->v_elements; el != NULL; el = el->e_next) {
		in_port_t fport, tport;
		bool range;

		if (!npfctl_parse_port(el->e_data, &range, &fport, &tport)) {
			errx(EXIT_FAILURE, "invalid service '%s'", el->e_data);
		}
		nblocks[0]--;
		foff = both ? 0 : npfctl_failure_offset(nblocks);
		npfctl_gennc_ports(nc, foff, fport, tport, tcpudp, sd);
	}
}

static void
npfctl_rulenc_block(void **nc, int nblocks[], var_t *cidr, var_t *ports,
    bool both, bool tcpudp, bool sd)
{

	npfctl_rulenc_v4cidr(nc, nblocks, cidr, sd);
	if (ports == NULL) {
		return;
	}
	npfctl_rulenc_ports(nc, nblocks, ports, tcpudp, both, sd);
	if (!both) {
		return;
	}
	npfctl_rulenc_ports(nc, nblocks, ports, !tcpudp, false, sd);
}

void
npfctl_rule_protodata(prop_dictionary_t rl, char *proto, char *tcp_flags,
    int icmp_type, int icmp_code,
    var_t *from, var_t *fports, var_t *to, var_t *tports)
{
	prop_data_t ncdata;
	bool icmp, tcpudp, both;
	int foff, nblocks[3] = { 0, 0, 0 };
	void *ncptr, *nc;
	size_t sz;

	/*
	 * Default: both TCP and UDP.
	 */
	icmp = false;
	tcpudp = true;
	if (proto == NULL) {
		both = true;
		goto skip_proto;
	}
	both = false;

	if (strcmp(proto, "icmp") == 0) {
		/* ICMP case. */
		fports = NULL;
		tports = NULL;
		icmp = true;

	} else if (strcmp(proto, "tcp") == 0) {
		/* Just TCP. */
		tcpudp = true;

	} else if (strcmp(proto, "udp") == 0) {
		/* Just UDP. */
		tcpudp = false;

	} else {
		/* Default. */
	}
skip_proto:
	if (icmp || icmp_type != -1) {
		assert(tcp_flags == NULL);
		icmp = true;
		nblocks[2] += 1;
	}
	if (tcpudp && tcp_flags) {
		assert(icmp_type == -1 && icmp_code == -1);
		nblocks[2] += 1;
	}

	/* Calculate how blocks to determince n-code. */
	if (from && from->v_count) {
		if (from->v_type == VAR_TABLE)
			nblocks[0] += 1;
		else
			nblocks[1] += from->v_count;
		if (fports && fports->v_count)
			nblocks[0] += fports->v_count * (both ? 2 : 1);
	}
	if (to && to->v_count) {
		if (to->v_type == VAR_TABLE)
			nblocks[0] += 1;
		else
			nblocks[1] += to->v_count;
		if (tports && tports->v_count)
			nblocks[0] += tports->v_count * (both ? 2 : 1);
	}

	/* Any n-code to generate? */
	if (!icmp && (nblocks[0] + nblocks[1] + nblocks[2]) == 0) {
		/* Done, if none. */
		return;
	}

	/* Allocate memory for the n-code. */
	sz = npfctl_calc_ncsize(nblocks);
	ncptr = malloc(sz);
	if (ncptr == NULL) {
		perror("malloc");
		exit(EXIT_FAILURE);
	}
	nc = ncptr;

	/*
	 * Generate v4 CIDR matching blocks and TCP/UDP port matching.
	 */
	if (from) {
		npfctl_rulenc_block(&nc, nblocks, from, fports,
		    both, tcpudp, true);
	}
	if (to) {
		npfctl_rulenc_block(&nc, nblocks, to, tports,
		    both, tcpudp, false);
	}

	if (icmp) {
		/*
		 * ICMP case.
		 */
		nblocks[2]--;
		foff = npfctl_failure_offset(nblocks);
		npfctl_gennc_icmp(&nc, foff, icmp_type, icmp_code);

	} else if (tcpudp && tcp_flags) {
		/*
		 * TCP case, flags.
		 */
		uint8_t tfl = 0, tfl_mask;

		nblocks[2]--;
		foff = npfctl_failure_offset(nblocks);
		if (!npfctl_parse_tcpfl(tcp_flags, &tfl, &tfl_mask)) {
			errx(EXIT_FAILURE, "invalid TCP flags '%s'", tcp_flags);
		}
		npfctl_gennc_tcpfl(&nc, foff, tfl, tfl_mask);
	}
	npfctl_gennc_complete(&nc);

	if ((uintptr_t)nc - (uintptr_t)ncptr != sz) {
		errx(EXIT_FAILURE, "n-code size got wrong (%tu != %zu)",
		    (uintptr_t)nc - (uintptr_t)ncptr, sz);
	}

#ifdef DEBUG
	uint32_t *op = ncptr;
	size_t n = sz;
	do {
		DPRINTF(("\t> |0x%02x|\n", (u_int)*op));
		op++;
		n -= sizeof(*op);
	} while (n);
#endif

	/* Create a final memory block of data, ready to send. */
	ncdata = prop_data_create_data(ncptr, sz);
	if (ncdata == NULL) {
		perror("prop_data_create_data");
		exit(EXIT_FAILURE);
	}
	prop_dictionary_set(rl, "ncode", ncdata);
	free(ncptr);
}

/*
 * Rule procedure construction routines.
 */

prop_dictionary_t
npfctl_mk_rproc(void)
{
	prop_dictionary_t rp;

	rp = prop_dictionary_create();
	prop_dictionary_set(rp, "id",
	    prop_number_create_unsigned_integer(rproc_id_counter++));
	prop_array_add(rproc_arr, rp);
	return rp;
}

bool
npfctl_find_rproc(prop_dictionary_t rl, char *name)
{
	prop_dictionary_t rp;
	prop_object_iterator_t it;
	prop_object_t obj;

	it = prop_array_iterator(rproc_arr);
	while ((rp = prop_object_iterator_next(it)) != NULL) {
		obj = prop_dictionary_get(rp, "name");
		if (strcmp(prop_string_cstring(obj), name) == 0)
			break;
	}
	if (rp == NULL) {
		return false;
	}
	prop_dictionary_set(rl, "rproc-id", prop_dictionary_get(rp, "id"));
	return true;
}

/*
 * NAT policy construction routines.
 */

prop_dictionary_t
npfctl_mk_nat(void)
{
	prop_dictionary_t rl;
	pri_t pri;

	/* NAT policy is rule with extra info. */
	rl = prop_dictionary_create();
	pri = nat_prio_counter++;
	prop_dictionary_set(rl, "priority", prop_number_create_integer(pri));
	prop_array_add(nat_arr, rl);
	return rl;
}

void
npfctl_nat_setup(prop_dictionary_t rl, int type, int flags,
    u_int iface, char *taddr, char *rport)
{
	int attr = NPF_RULE_PASS | NPF_RULE_FINAL;
	in_addr_t addr, _dummy;
	prop_data_t addrdat;

	/* Translation type and flags. */
	prop_dictionary_set(rl, "type",
	    prop_number_create_integer(type));
	prop_dictionary_set(rl, "flags",
	    prop_number_create_integer(flags));

	/* Interface and attributes. */
	attr |= (type == NPF_NATOUT) ? NPF_RULE_OUT : NPF_RULE_IN;
	npfctl_rule_setattr(rl, attr, iface);

	/* Translation IP. */
	npfctl_parse_cidr(taddr, &addr, &_dummy);
	addrdat = prop_data_create_data(&addr, sizeof(in_addr_t));
	if (addrdat == NULL) {
		err(EXIT_FAILURE, "prop_data_create_data");
	}
	prop_dictionary_set(rl, "translation-ip", addrdat);

	/* Translation port (for redirect case). */
	if (rport) {
		in_port_t port;
		bool range;

		if (!npfctl_parse_port(rport, &range, &port, &port)) {
			errx(EXIT_FAILURE, "invalid service '%s'", rport);
		}
		if (range) {
			errx(EXIT_FAILURE, "range is not supported for 'rdr'");
		}
		prop_dictionary_set(rl, "translation-port",
		    prop_number_create_integer(port));
	}
}
