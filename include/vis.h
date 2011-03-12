/*	$NetBSD: vis.h,v 1.19 2011/03/12 19:52:45 christos Exp $	*/

/*-
 * Copyright (c) 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
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
 *	@(#)vis.h	8.1 (Berkeley) 6/2/93
 */

#ifndef _VIS_H_
#define	_VIS_H_

#include <sys/types.h>

/*
 * to select alternate encoding format
 */
#define	VIS_OCTAL	0x001	/* use octal \ddd format */
#define	VIS_CSTYLE	0x002	/* use \[nrft0..] where appropiate */

/*
 * to alter set of characters encoded (default is to encode all
 * non-graphic except space, tab, and newline).
 */
#define	VIS_SP		0x004	/* also encode space */
#define	VIS_TAB		0x008	/* also encode tab */
#define	VIS_NL		0x010	/* also encode newline */
#define	VIS_WHITE	(VIS_SP | VIS_TAB | VIS_NL)
#define	VIS_SAFE	0x020	/* only encode "unsafe" characters */

/*
 * other
 */
#define	VIS_NOSLASH	0x040	/* inhibit printing '\' */
#define	VIS_HTTP1808	0x080	/* http-style escape % hex hex */
#define	VIS_HTTPSTYLE	0x080	/* http-style escape % hex hex */
#define	VIS_MIMESTYLE	0x100	/* mime-style escape = HEX HEX */
#define	VIS_HTTP1866	0x200	/* http-style &#num; or &string; */
#define	VIS_NOESCAPE	0x400	/* don't decode `\' */
#define	_VIS_END	0x800	/* for unvis */

/*
 * unvis return codes
 */
#define	UNVIS_VALID	 1	/* character valid */
#define	UNVIS_VALIDPUSH	 2	/* character valid, push back passed char */
#define	UNVIS_NOCHAR	 3	/* valid sequence, no character produced */
#define	UNVIS_SYNBAD	-1	/* unrecognized escape sequence */
#define	UNVIS_ERROR	-2	/* decoder in unknown state (unrecoverable) */

/*
 * unvis flags
 */
#define	UNVIS_END	_VIS_END	/* no more characters */

#include <sys/cdefs.h>

__BEGIN_DECLS
char	*vis(char *, int, int, int);
char	*nvis(char *, size_t, int, int, int);

char	*svis(char *, int, int, int, const char *);
char	*snvis(char *, size_t, int, int, int, const char *);

int	strvis(char *, const char *, int);
int	strnvis(char *, size_t, const char *, int);

int	strsvis(char *, const char *, int, const char *);
int	strsnvis(char *, size_t, const char *, int, const char *);

int	strvisx(char *, const char *, size_t, int);
int	strnvisx(char *, size_t, const char *, size_t, int);

int	strsvisx(char *, const char *, size_t, int, const char *);
int	strsnvisx(char *, size_t, const char *, size_t, int, const char *);

int	strunvis(char *, const char *);
int	strnunvis(char *, size_t, const char *);

int	strunvisx(char *, const char *, int);
int	strnunvisx(char *, size_t, const char *, int);

#ifndef __LIBC12_SOURCE__
int	unvis(char *, int, int *, int) __RENAME(__unvis50);
#endif
__END_DECLS

#endif /* !_VIS_H_ */
