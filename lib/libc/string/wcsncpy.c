/*	$NetBSD: wcsncpy.c,v 1.3 2005/10/13 21:13:05 tnozaki Exp $	*/

/*-
 * Copyright (c)1999 Citrus Project,
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	citrus Id: wcsncpy.c,v 1.1 1999/12/29 21:47:45 tshiozak Exp
 */

#include <sys/cdefs.h>
#if defined(LIBC_SCCS) && !defined(lint)
__RCSID("$NetBSD: wcsncpy.c,v 1.3 2005/10/13 21:13:05 tnozaki Exp $");
#endif /* LIBC_SCCS and not lint */

#include <assert.h>
#include <wchar.h>

wchar_t *
wcsncpy(s1, s2, n)
	wchar_t *s1;
	const wchar_t *s2;
	size_t n;
{
	wchar_t *p;

	_DIAGASSERT(s1 != NULL);
	_DIAGASSERT(s2 != NULL);

	p = s1;
	while (n && *s2) {
		*p++ = *s2++;
		n--;
	}
	while (n) {
		*p++ = L'\0';
		n--;
	}

	return s1;
}
