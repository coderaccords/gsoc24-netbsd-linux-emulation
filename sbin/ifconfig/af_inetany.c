/*	$NetBSD: af_inetany.c,v 1.5 2008/05/08 07:13:20 dyoung Exp $	*/

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
__RCSID("$NetBSD: af_inetany.c,v 1.5 2008/05/08 07:13:20 dyoung Exp $");
#endif /* not lint */

#include <sys/param.h> 
#include <sys/ioctl.h> 
#include <sys/socket.h>

#include <net/if.h> 
#include <netinet/in.h>
#include <netinet/in_var.h>
#include <netinet6/nd6.h>

#include <arpa/inet.h>

#include <assert.h>
#include <err.h>
#include <errno.h>
#include <ifaddrs.h>
#include <netdb.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <util.h>

#include "env.h"
#include "extern.h"
#include "af_inet.h"
#include "af_inet6.h"
#include "af_inetany.h"

static void *
loadbuf(struct apbuf *b, const struct paddr_prefix *pfx)
{
	return memcpy(b->buf, &pfx->pfx_addr,
	              MIN(b->buflen, pfx->pfx_addr.sa_len));
}

void
commit_address(prop_dictionary_t env, prop_dictionary_t oenv,
    struct afparam *param)
{
	const char *ifname;
	int af, rc, s;
	bool alias, delete, replace;
	prop_data_t d;
	const struct paddr_prefix *addr, *brd, *dst, *mask;
	unsigned short flags;

	if ((af = getaf(env)) == -1)
		af = AF_INET;

	if ((s = getsock(af)) == -1)
		err(EXIT_FAILURE, "%s: getsock", __func__);

	if ((ifname = getifinfo(env, oenv, &flags)) == NULL)
		return;

	if ((d = (prop_data_t)prop_dictionary_get(env, "address")) != NULL)
		addr = prop_data_data_nocopy(d);
	else
		return;

	if ((d = (prop_data_t)prop_dictionary_get(env, "dst")) != NULL)
		dst = prop_data_data_nocopy(d);
	else
		dst = NULL;

	if ((d = (prop_data_t)prop_dictionary_get(env, "netmask")) != NULL)
		mask = prop_data_data_nocopy(d);
	else
		mask = NULL;

	if ((d = (prop_data_t)prop_dictionary_get(env, "broadcast")) != NULL)
		brd = prop_data_data_nocopy(d);
	else
		brd = NULL;

	if (!prop_dictionary_get_bool(env, "alias", &alias)) {
		delete = false;
		replace = (param->gifaddr.cmd != 0);
	} else {
		replace = false;
		delete = !alias;
	}

	memset(param->req.buf, 0, param->req.buflen);
	memset(param->dgreq.buf, 0, param->dgreq.buflen);

	strlcpy(param->name[0].buf, ifname, param->name[0].buflen);
	strlcpy(param->name[1].buf, ifname, param->name[1].buflen);

	loadbuf(&param->addr, addr);

	/* TBD: read matching ifaddr from kernel, use the netmask as default
	 * TBD: handle preference
	 */
	switch (flags & (IFF_BROADCAST|IFF_POINTOPOINT)) {
	case 0:
		break;
	case IFF_BROADCAST:
		if (mask != NULL)
			loadbuf(&param->mask, mask);
		else if (param->defmask.buf != NULL) {
			memcpy(param->mask.buf, param->defmask.buf,
			    MIN(param->mask.buflen, param->defmask.buflen));
		}
		if (brd != NULL)
			loadbuf(&param->brd, brd);
		break;
	case IFF_POINTOPOINT:
		if (dst == NULL) {
			errx(EXIT_FAILURE, "no point-to-point "
			     "destination address");
		}
		if (brd != NULL) {
			errx(EXIT_FAILURE, "%s is not a broadcast interface",
			    ifname);
		}
		loadbuf(&param->dst, dst);
		break;
	case IFF_BROADCAST|IFF_POINTOPOINT:
		errx(EXIT_FAILURE, "unsupported interface flags");
	}
	if (replace) {
		if (ioctl(s, param->gifaddr.cmd, param->dgreq.buf) == 0) {
			rc = ioctl(s, param->difaddr.cmd, param->dgreq.buf);
			if (rc == -1)
				err(EXIT_FAILURE, param->difaddr.desc);
		} else if (errno == EADDRNOTAVAIL)
			;	/* No address was assigned yet. */
		else
			err(EXIT_FAILURE, param->gifaddr.desc);
	} else if (delete) {
		loadbuf(&param->dgaddr, addr);
		if (ioctl(s, param->difaddr.cmd, param->dgreq.buf) == -1)
			err(EXIT_FAILURE, param->difaddr.desc);
		return;
	}
	if (param->pre_aifaddr != NULL &&
	    (*param->pre_aifaddr)(env, param) == -1)
		err(EXIT_FAILURE, "pre-%s", param->aifaddr.desc);
	if (ioctl(s, param->aifaddr.cmd, param->req.buf) == -1)
		err(EXIT_FAILURE, param->aifaddr.desc);
}
