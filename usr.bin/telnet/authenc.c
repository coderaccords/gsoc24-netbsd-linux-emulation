/*	$NetBSD: authenc.c,v 1.9 2000/06/22 06:47:48 thorpej Exp $	*/

/*-
 * Copyright (c) 1991, 1993
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
#ifndef lint
#if 0
static char sccsid[] = "@(#)authenc.c	8.1 (Berkeley) 6/6/93";
#else
__RCSID("$NetBSD: authenc.c,v 1.9 2000/06/22 06:47:48 thorpej Exp $");
#endif
#endif /* not lint */

#if	defined(AUTHENTICATION) || defined(ENCRYPTION)
#include <unistd.h>
#include <sys/types.h>
#include <arpa/telnet.h>
#include <libtelnet/encrypt.h>
#include <libtelnet/misc.h>

#include "general.h"
#include "ring.h"
#include "externs.h"
#include "defines.h"
#include "types.h"

	int
telnet_net_write(str, len)
	unsigned char *str;
	int len;
{
	if (NETROOM() > len) {
		ring_supply_data(&netoring, str, len);
		if (str[0] == IAC && str[1] == SE)
			printsub('>', &str[2], len-2);
		return(len);
	}
	return(0);
}

	void
net_encrypt()
{
#ifdef	ENCRYPTION
	if (encrypt_output)
		ring_encrypt(&netoring, encrypt_output);
	else
		ring_clearto(&netoring);
#endif	/* ENCRYPTION */
}

	int
telnet_spin()
{
	return(-1);
}

	char *
telnet_getenv(val)
	char *val;
{
	return((char *)env_getvalue((unsigned char *)val));
}

	char *
telnet_gets(prompt, result, length, echo)
	char *prompt;
	char *result;
	int length;
	int echo;
{
	extern int globalmode;
	int om = globalmode;
	char *res;

	TerminalNewMode(-1);
	if (echo) {
		printf("%s", prompt);
		res = fgets(result, length, stdin);
	} else if ((res = getpass(prompt)) != NULL) {
		strncpy(result, res, length);
		res = result;
	}
	TerminalNewMode(om);
	return(res);
}
#endif	/* defined(AUTHENTICATION) || defined(ENCRYPTION) */
