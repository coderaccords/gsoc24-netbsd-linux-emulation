/*	$NetBSD: ptmx.c,v 1.1 2004/05/25 20:32:32 christos Exp $	*/

/*-
 * Copyright (c) 1994 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Christos Zoulas.
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
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
#include <sys/cdefs.h>
__RCSID("$NetBSD: ptmx.c,v 1.1 2004/05/25 20:32:32 christos Exp $");

#include <stdlib.h>
#include <unistd.h>
#include <grp.h>
#include <fcntl.h>
#include <sys/stat.h>

int
main(int argc, char *argv[])
{
	struct stat stm, sts;
	char *pty;
	int fdm, fds;
	struct group *gp;

	if ((fdm = posix_openpt(O_RDWR|O_NOCTTY)) == -1)
		err(1, "open master");

	if (fstat(fdm, &stm) == -1)
		err(1, "fstat master");

	if (stat("/dev/ptyp0", &sts) == -1)
		err(1, "stat example");

	if (major(stm.st_rdev) != major(sts.st_rdev))
		errx(1, "bad master major number %d", major(stm.st_rdev));

	if (grantpt(fdm) == -1)
		err(1, "grantpt");

	if ((pty = ptsname(fdm)) == NULL)
		err(1, "ptsname");

	if ((fds = open(pty, O_RDWR|O_NOCTTY)) == -1)
		err(1, "open slave");

	if (fstat(fds, &sts) == -1)
		err(1, "fstat slave");

	if (minor(stm.st_rdev) != minor(sts.st_rdev))
		errx(1, "bad slave minor number %d", major(stm.st_rdev));

	if (sts.st_uid != getuid())
		errx(1, "bad slave uid %lu != %lu", (unsigned long)stm.st_uid,
		    getuid());

	if ((gp = getgrnam("tty")) == NULL)
		errx(1, "cannot find `tty' group");

	if (sts.st_gid != gp->gr_gid)
		errx(1, "bad slave gid %lu != %lu", (unsigned long)stm.st_gid,
		    gp->gr_gid);
	return 0;
}
