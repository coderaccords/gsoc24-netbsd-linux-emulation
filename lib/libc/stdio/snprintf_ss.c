/*	$NetBSD: snprintf_ss.c,v 1.2 2006/10/29 16:22:17 christos Exp $	*/

/*-
 * Copyright (c) 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Chris Torek.
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
static char sccsid[] = "@(#)snprintf.c	8.1 (Berkeley) 6/4/93";
#else
__RCSID("$NetBSD: snprintf_ss.c,v 1.2 2006/10/29 16:22:17 christos Exp $");
#endif
#endif /* LIBC_SCCS and not lint */

#include "namespace.h"

#include <assert.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>

#include "reentrant.h"
#include "local.h"

#ifdef __weak_alias
__weak_alias(snprintf_ss,_snprintf_ss)
#endif

int
snprintf_ss(char *str, size_t n, char const *fmt, ...)
{
	int ret;
	va_list ap;
	FILE f;
	struct __sfileext fext;
	unsigned char dummy[1];

	_DIAGASSERT(n == 0 || str != NULL);
	_DIAGASSERT(fmt != NULL);

	if ((int)n < 0) {
		errno = EINVAL;
		return (-1);
	}
	va_start(ap, fmt);
	_FILEEXT_SETUP(&f, &fext);
	f._file = -1;
	f._flags = __SWR | __SSTR | __SAFE;
	if (n == 0) {
		f._bf._base = f._p = dummy;
		f._bf._size = f._w = 0;
	} else {
		f._bf._base = f._p = (unsigned char *)str;
		f._bf._size = f._w = n - 1;
	}
	ret = __vfprintf_unlocked(&f, fmt, ap);
	*f._p = 0;
	va_end(ap);
	return (ret);
}
