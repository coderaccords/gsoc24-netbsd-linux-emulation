/*	$NetBSD: getttyent.c,v 1.20 2003/08/07 16:42:51 agc Exp $	*/

/*
 * Copyright (c) 1989, 1993
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
#if defined(LIBC_SCCS) && !defined(lint)
#if 0
static char sccsid[] = "@(#)getttyent.c	8.1 (Berkeley) 6/4/93";
#else
__RCSID("$NetBSD: getttyent.c,v 1.20 2003/08/07 16:42:51 agc Exp $");
#endif
#endif /* LIBC_SCCS and not lint */

#include "namespace.h"

#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <ttyent.h>
#include <errno.h>
#include <err.h>
#include <stdlib.h>

#ifdef __weak_alias
__weak_alias(endttyent,_endttyent)
__weak_alias(getttyent,_getttyent)
__weak_alias(getttynam,_getttynam)
__weak_alias(setttyent,_setttyent)
#endif

static char zapchar;
static FILE *tf;
static size_t lineno = 0;
static char *skip __P((char *));
static char *value __P((char *));

struct ttyent *
getttynam(tty)
	const char *tty;
{
	struct ttyent *t;

	_DIAGASSERT(tty != NULL);

	setttyent();
	while ((t = getttyent()) != NULL)
		if (!strcmp(tty, t->ty_name))
			break;
	endttyent();
	return (t);
}

struct ttyent *
getttyent()
{
	static struct ttyent tty;
	int c;
	char *p;
	size_t len;
	static char *line = NULL;

	if (!tf && !setttyent())
		return (NULL);
	if (line)
		free(line);
	for (;;) {
		errno = 0;
		line = fparseln(tf, &len, &lineno, NULL, FPARSELN_UNESCALL);
		if (line == NULL) {
			if (errno != 0)
				warn("gettyent");
			return NULL;
		}
		for (p = line; *p && isspace((unsigned char) *p); p++)
			continue;
		if (*p && *p != '#')
			break;
		free(line);
	}

	zapchar = 0;
	tty.ty_name = p;
	p = skip(p);
	if (!*(tty.ty_getty = p))
		tty.ty_getty = tty.ty_type = NULL;
	else {
		p = skip(p);
		if (!*(tty.ty_type = p))
			tty.ty_type = NULL;
		else
			p = skip(p);
	}
	tty.ty_status = 0;
	tty.ty_window = NULL;

#define	scmp(e)	!strncmp(p, e, sizeof(e) - 1) && (isspace((unsigned char) p[sizeof(e) - 1]) || p[sizeof(e) - 1] == '\0')
#define	vcmp(e)	!strncmp(p, e, sizeof(e) - 1) && p[sizeof(e) - 1] == '='
	for (; *p; p = skip(p)) {
		if (scmp(_TTYS_OFF))
			tty.ty_status &= ~TTY_ON;
		else if (scmp(_TTYS_ON))
			tty.ty_status |= TTY_ON;
		else if (scmp(_TTYS_SECURE))
			tty.ty_status |= TTY_SECURE;
		else if (scmp(_TTYS_LOCAL))
			tty.ty_status |= TTY_LOCAL;
		else if (scmp(_TTYS_RTSCTS))
			tty.ty_status |= TTY_RTSCTS;
		else if (scmp(_TTYS_DTRCTS))
			tty.ty_status |= TTY_DTRCTS;
		else if (scmp(_TTYS_SOFTCAR))
			tty.ty_status |= TTY_SOFTCAR;
		else if (scmp(_TTYS_MDMBUF))
			tty.ty_status |= TTY_MDMBUF;
		else if (vcmp(_TTYS_WINDOW))
			tty.ty_window = value(p);
		else if (vcmp(_TTYS_CLASS))
			tty.ty_class = value(p);
		else
			warnx("gettyent: %s, %lu: unknown option `%s'",
			    _PATH_TTYS, (unsigned long)lineno, p);
	}

	if (zapchar == '#' || *p == '#')
		while ((c = *++p) == ' ' || c == '\t')
			;
	tty.ty_comment = p;
	if (*p == 0)
		tty.ty_comment = 0;
	if ((p = strchr(p, '\n')) != NULL)
		*p = '\0';
	return (&tty);
}

#define	QUOTED	1

/*
 * Skip over the current field, removing quotes, and return a pointer to
 * the next field.
 */
static char *
skip(p)
	char *p;
{
	char *t;
	int c, q;

	_DIAGASSERT(p != NULL);

	for (q = 0, t = p; (c = *p) != '\0'; p++) {
		if (c == '"') {
			q ^= QUOTED;	/* obscure, but nice */
			continue;
		}
		if (q == QUOTED && *p == '\\' && *(p+1) == '"')
			p++;
		*t++ = *p;
		if (q == QUOTED)
			continue;
		if (c == '#') {
			zapchar = c;
			*p = 0;
			break;
		}
		if (c == '\t' || c == ' ' || c == '\n') {
			zapchar = c;
			*p++ = 0;
			while ((c = *p) == '\t' || c == ' ' || c == '\n')
				p++;
			break;
		}
	}
	*--t = '\0';
	return (p);
}

static char *
value(p)
	char *p;
{

	_DIAGASSERT(p != NULL);

	return ((p = strchr(p, '=')) ? ++p : NULL);
}

int
setttyent()
{
	lineno = 0;
	if (tf) {
		rewind(tf);
		return (1);
	} else if ((tf = fopen(_PATH_TTYS, "r")) != NULL)
		return (1);
	return (0);
}

int
endttyent()
{
	int rval;

	if (tf) {
		rval = !(fclose(tf) == EOF);
		tf = NULL;
		return (rval);
	}
	return (1);
}
