/*	$NetBSD: yp_order.c,v 1.7 1997/07/21 14:09:30 jtc Exp $	 */

/*
 * Copyright (c) 1992, 1993 Theo de Raadt <deraadt@fsa.ca>
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
 *	This product includes software developed by Theo de Raadt.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
#if defined(LIBC_SCCS) && !defined(lint)
__RCSID("$NetBSD: yp_order.c,v 1.7 1997/07/21 14:09:30 jtc Exp $");
#endif

#include "namespace.h"
#include <string.h>
#include <rpc/rpc.h>
#include <rpcsvc/yp_prot.h>
#include <rpcsvc/ypclnt.h>
#include "local.h"

extern struct timeval _yplib_timeout;
extern int _yplib_nerrs;

#ifdef __weak_alias
__weak_alias(yp_order,_yp_order);
#endif

int
yp_order(indomain, inmap, outorder)
	const char     *indomain;
	const char     *inmap;
	int            *outorder;
{
	struct dom_binding *ysd;
	struct ypresp_order ypro;
	struct ypreq_nokey yprnk;
	int r, nerrs = 0;

	if (_yp_invalid_domain(indomain))
		return YPERR_BADARGS;
	if (inmap == NULL || *inmap == '\0'
	    || strlen(inmap) > YPMAXMAP)
		return YPERR_BADARGS;
	if (outorder == NULL)
		return YPERR_BADARGS;

again:
	if (_yp_dobind(indomain, &ysd) != 0)
		return YPERR_DOMAIN;

	yprnk.domain = indomain;
	yprnk.map = inmap;

	(void)memset(&ypro, 0, sizeof ypro);

	r = clnt_call(ysd->dom_client, YPPROC_ORDER,
		      xdr_ypreq_nokey, &yprnk, xdr_ypresp_order, &ypro, 
		      _yplib_timeout);
	if (r != RPC_SUCCESS) {
		if (++nerrs == _yplib_nerrs) {
			clnt_perror(ysd->dom_client, "yp_order: clnt_call");
			nerrs = 0;
		}
	        if (r == RPC_PROCUNAVAIL) {
			/* Case of NIS+ server in NIS compat mode */
			r = YPERR_YPERR;
			goto bail;
	        } 
		ysd->dom_vers = -1;
		goto again;
	}
	*outorder = ypro.ordernum;
	xdr_free(xdr_ypresp_order, (char *) &ypro);
	r = ypprot_err(ypro.status);
bail:
	__yp_unbind(ysd);
	return r;
}
