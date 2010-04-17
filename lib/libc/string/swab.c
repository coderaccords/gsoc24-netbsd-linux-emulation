/*	$NetBSD: swab.c,v 1.13 2010/04/17 17:57:39 christos Exp $	*/

/*
 * Copyright (c) 1988, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Jeffrey Mogul.
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
#if defined(LIBC_SCCS) && !defined(lint)
#if 0
static char sccsid[] = "@(#)swab.c	8.1 (Berkeley) 6/4/93";
#else
__RCSID("$NetBSD: swab.c,v 1.13 2010/04/17 17:57:39 christos Exp $");
#endif
#endif /* LIBC_SCCS and not lint */

#include <assert.h>
#include <unistd.h>

void
swab(const void * __restrict from, void * __restrict to, ssize_t len)
{
	char temp;
	const char *fp;
	char *tp;

	if (len & 1)
		len--;
	if (len <= 0)
		return;

	_DIAGASSERT(from != NULL);
	_DIAGASSERT(to != NULL);

	len = (len / 2) + 1;
	fp = (const char *)from;
	tp = (char *)to;
#define	STEP	temp = *fp++,*tp++ = *fp++,*tp++ = temp
	/* round to multiple of 8 */
	while ((--len & 07) != 0)
		STEP;
	len >>= 3;
	if (len == 0)
		return;
	while (len-- != 0) {
		STEP; STEP; STEP; STEP;
		STEP; STEP; STEP; STEP;
	}
}
