/*	$NetBSD: biosdisk_user.c,v 1.2 1998/11/22 15:44:03 drochner Exp $	*/

/*
 * Copyright (c) 1998
 *	Matthias Drochner.  All rights reserved.
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
 *	This product includes software developed for the NetBSD Project
 *	by Matthias Drochner.
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
 */

#include "sanamespace.h"

#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <err.h>

#include "biosdisk_user.h"

/*
 * Replacement for i386/stand/lib/bios_disk.S.
 * Allows to map BIOS-like device numbers to character
 * device nodes or plain files.
 * The actual mapping is defined in the external table
 * "emuldisktab".
 */

static int currentdev, currentdte;
static int fd = -1;

int
get_diskinfo(dev)
	int dev;
{
	int i;

	if (fd != -1) {
		close(fd);
		fd = -1;
	}

	i = 0;
	for (;;) {
		if (emuldisktab[i].biosdev == -1)
			break;
		if (emuldisktab[i].biosdev == dev)
			goto ok;
		i++;
	}
	warnx("unknown device %x", dev);
	return (0);

ok:
	fd = open(emuldisktab[i].name, O_RDONLY, 0);
	if (fd < 0) {
		warn("open %s", emuldisktab[i].name);
		return (0);
	}

	currentdev = dev;
	currentdte = i;
	return (((emuldisktab[i].heads - 1) << 8)
		+ emuldisktab[i].spt);
}

int
biosread(dev, cyl, head, sec, nsec, buf)
	int dev;
	int cyl, head, sec;
	int nsec;
	char *buf;
{
	if (dev != currentdev) {
		warnx("biosread: unexpected device %x", dev);
		return (-1);
	}

	if (lseek(fd, ((cyl * emuldisktab[currentdte].heads + head)
		       * emuldisktab[currentdte].spt + sec) * 512,
		  SEEK_SET) == -1) {
		warn("lseek");
		return (-1);
	}
	if (read(fd, buf, nsec * 512) != nsec * 512) {
		warn("read");
		return (-1);
	}
	return (0);
}

int
int13_extension(dev)
	int dev;
{
	return (0);
}

struct ext {
	int8_t	size;
	int8_t	resvd;
	int16_t	cnt;
	int16_t	off;
	int16_t	seg;
	int64_t	sec;
};

int
biosextread(dev, ext)
	int dev;
	struct ext *ext;
{
	return (-1);
}
