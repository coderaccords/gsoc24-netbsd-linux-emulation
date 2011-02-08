/*	$NetBSD: cacheops_machdep.h,v 1.7 2011/02/08 20:20:20 rmind Exp $	*/

/*
 * Copyright (c) 1988 University of Utah.
 * Copyright (c) 1980, 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * the Systems Programming Group of the University of Utah Computer
 * Science Department.
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

#ifndef _NEWS68K_CACHEOPS_MACHDEP_H_
#define	_NEWS68K_CACHEOPS_MACHDEP_H_

extern void *cache_clr;

static __inline int __attribute__((__unused__))
DCIx_md(void)
{
	volatile uint8_t *p = cache_clr;

	if (ectype != EC_VIRT) {
		return 0;
	}

	*p = 0xff;
	return 1;
}

static __inline int __attribute__((__unused__))
DCIA_md(void)
{
	return DCIx_md();
}

static __inline int __attribute__((__unused__))
DCIS_md(void)
{
	return DCIx_md();
}

static __inline int __attribute__((__unused__))
DCIU_md(void)
{
	return DCIx_md();
}

static __inline int __attribute__((__unused__))
PCIA_md(void)
{
	volatile uint8_t *p = cache_clr;

	if (ectype != EC_PHYS) {
		return 0;
	}

	*p = 0xff;
	return 1;
}
#endif	/* _NEWS68K_CACHEOPS_MACHDEP_H_ */
