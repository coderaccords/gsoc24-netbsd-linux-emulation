/*	$NetBSD: fullname.c,v 1.11 2003/08/07 16:44:21 agc Exp $	*/

/*
 * Copyright (c) 1981, 1993
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
 */

#include <sys/cdefs.h>
#ifndef lint
#if 0
static char sccsid[] = "@(#)fullname.c	8.1 (Berkeley) 6/4/93";
#else
__RCSID("$NetBSD: fullname.c,v 1.11 2003/08/07 16:44:21 agc Exp $");
#endif
#endif				/* not lint */

#include "curses.h"

/*
 * fullname --
 *	This routine fills in "def" with the full name of the terminal.
 *	This is assumed to be the last name in the list of aliases.
 */
char   *
fullname(const char *bp, char *def)
{
	char   *cp;

	*def = '\0';		/* In case no name. */

	while (*bp && *bp != ':') {
		cp = def;	/* Start of answer. */
		while (*bp && *bp != ':' && *bp != '|')
			*cp++ = *bp++;	/* Copy name over. */
		*cp = '\0';	/* Zero end of name. */
		if (*bp == '|')
			bp++;	/* Skip over '|' if that is case. */
	}
	return (def);
}
