/*	$NetBSD: isnanl_ieee754.c,v 1.2 2003/08/07 16:42:52 agc Exp $	*/

/*
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This software was developed by the Computer Systems Engineering group
 * at Lawrence Berkeley Laboratory under DARPA contract BG 91-66 and
 * contributed to Berkeley.
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
 * from: Header: isinf.c,v 1.1 91/07/08 19:03:34 torek Exp
 */

#include <sys/cdefs.h>
#if defined(LIBC_SCCS) && !defined(lint)
#if 0
static char sccsid[] = "@(#)isinf.c	8.1 (Berkeley) 6/4/93";
#else
__RCSID("$NetBSD54_isnan.c,v 1.1 2002/02/19 13:08:13 simonb Exp $");
#endif
#endif /* LIBC_SCCS and not lint */

#include "namespace.h"
#include <sys/types.h>
#include <machine/ieee.h>
#include <math.h>

#if !defined(EXT_EXP_INFNAN)
#include <assert.h>
#endif

#if 0	/* XXX Currently limited to internal use. */
#ifdef __weak_alias
__weak_alias(isnanl,_isnanl)
#endif
#endif

int
isnanl(long double ld)
{
#if defined(EXT_EXP_INFNAN)
	union {
		long double ld;
		struct ieee_ext ldbl;
	} u;

	u.ld = ld;
	return (u.ldbl.ext_exp == DBL_EXP_INFNAN &&
	    (u.ldbl.ext_frach != 0 || u.ldbl.ext_frachm != 0 ||
	     u.ldbl.ext_fraclm != 0 || u.ldbl.ext_fracl != 0));
#else
	_DIAGASSERT(sizeof(double) == sizeof(long double));
	return (isnan((double) ld));
#endif /* EXT_EXP_INFNAN */
}
