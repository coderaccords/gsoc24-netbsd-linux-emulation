/*	$NetBSD: pwd_gensalt.c,v 1.11 2004/07/02 00:05:23 sjg Exp $	*/

/*
 * Copyright 1997 Niels Provos <provos@physnet.uni-hamburg.de>
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
 *      This product includes software developed by Niels Provos.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * from OpenBSD: pwd_gensalt.c,v 1.9 1998/07/05 21:08:32 provos Exp
 */

#include <sys/cdefs.h>
#ifndef lint
__RCSID("$NetBSD: pwd_gensalt.c,v 1.11 2004/07/02 00:05:23 sjg Exp $");
#endif /* not lint */

#include <sys/syslimits.h>
#include <sys/types.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <err.h>
#include <grp.h>
#include <pwd.h>
#include <util.h>
#include <time.h>
#include <pwd.h>

#include <crypt.h>
#include "extern.h"

int
pwd_gensalt(char *salt, int max, struct passwd *pwd, char type)
{
	char option[LINE_MAX], *next, *now, *cipher, grpkey[LINE_MAX];
	int rounds;
	struct group *grp;

	*salt = '\0';

	switch (type) {
	case 'y':
	        cipher = "ypcipher";
		break;
	case 'l':
	default:
	        cipher = "localcipher";
		break;
	}

	pw_getconf(option, sizeof(option), pwd->pw_name, cipher);

	/* Try to find an entry for the group */
	if (*option == 0) {
		if ((grp = getgrgid(pwd->pw_gid)) != NULL) {
			snprintf(grpkey, sizeof(grpkey), ":%s", grp->gr_name);
			pw_getconf(option, sizeof(option), grpkey, cipher);
		}
		if (*option == 0)
		        pw_getconf(option, sizeof(option), "default", cipher);
	}

	next = option;
	now = strsep(&next, ",");
	if (strcmp(now, "old") == 0) {
		if (max < 3)
			return (0);
		__crypt_to64(&salt[0], arc4random(), 2);
		salt[2] = '\0';
	} else if (strcmp(now, "newsalt") == 0) {
		rounds = atol(next);
		if (max < 10)
			return (0);
		/* Check rounds, 24 bit is max */
		if (rounds < 7250)
			rounds = 7250;
		else if (rounds > 0xffffff)
		        rounds = 0xffffff;
		salt[0] = _PASSWORD_EFMT1;
		__crypt_to64(&salt[1], (u_int32_t)rounds, 4);
		__crypt_to64(&salt[5], arc4random(), 4);
		salt[9] = '\0';
	} else if (strcmp(now, "md5") == 0) {
		if (max < 13)  /* $1$8salt$\0 */
			return (0);
		salt[0] = _PASSWORD_NONDES;
		salt[1] = '1';
		salt[2] = '$';
		__crypt_to64(&salt[3], arc4random(), 4);
		__crypt_to64(&salt[7], arc4random(), 4);
		salt[11] = '$';
		salt[12] = '\0';
	} else if (strcmp(now, "sha1") == 0) {
		int n;

		rounds = atoi(next);	/* a hint only, 0 is ok */
		n = snprintf(salt, max, "%s%u$", SHA1_MAGIC,
			     __crypt_sha1_iterations(rounds));
		/*
		 * The salt can be up to 64 bytes, but 8
		 * is considered enough for now.
		 */
		if (n + 9 >= max)
			return 0;
		__crypt_to64(&salt[n], arc4random(), 4);
		__crypt_to64(&salt[n + 4], arc4random(), 4);
		salt[n + 8] = '$';
		salt[n + 9] = '\0';
	} else if (strcmp(now, "blowfish") == 0) {
		rounds = atoi(next);
		if (rounds < 4)
			rounds = 4;
		strlcpy(salt, bcrypt_gensalt(rounds), max);
	} else {
		strlcpy(salt, ":", max);
		warnx("Unknown option %s.", now);
	}

	return (1);
}
