/*	$NetBSD: __strerror.c,v 1.12 1998/02/16 11:27:16 lukem Exp $	*/

/*
 * Copyright (c) 1988 Regents of the University of California.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
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
static char *sccsid = "@(#)strerror.c	5.6 (Berkeley) 5/4/91";
#else
__RCSID("$NetBSD: __strerror.c,v 1.12 1998/02/16 11:27:16 lukem Exp $");
#endif
#endif /* LIBC_SCCS and not lint */

#include "namespace.h"
#ifdef NLS
#include <limits.h>
#include <nl_types.h>
#endif

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include "extern.h"

/*
 * Since perror() is not allowed to change the contents of strerror()'s
 * static buffer, both functions supply their own buffers to the
 * internal function __strerror().
 */

char *
__strerror(num, buf, buflen)
	int num;
	char *buf;
	int buflen;
{
#define	UPREFIX	"Unknown error: %u"
	unsigned int errnum;

#ifdef NLS
	nl_catd catd ;
	catd = catopen("libc", 0);
#endif

	errnum = num;				/* convert to unsigned */
	if (errnum < sys_nerr) {
#ifdef NLS
		strncpy(buf, catgets(catd, 1, errnum,
		    (char *)sys_errlist[errnum]), buflen); 
		buf[buflen - 1] = '\0';
#else
		return(sys_errlist[errnum]);
#endif
	} else {
#ifdef NLS
		snprintf(buf, buflen, 
		    catgets(catd, 1, 0xffff, UPREFIX), errnum);
#else
		snprintf(buf, buflen, UPREFIX, errnum);
#endif
	}

#ifdef NLS
	catclose(catd);
#endif

	return buf;
}
