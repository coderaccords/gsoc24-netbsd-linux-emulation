/*	$NetBSD: eject.c,v 1.10 1999/02/18 20:02:43 tron Exp $	*/

/*-
 * Copyright (c) 1999 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Chris Jones.
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
 *	This product includes software developed by the NetBSD
 *	Foundation, Inc. and its contributors.
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
#ifndef lint
__COPYRIGHT("@(#) Copyright (c) 1999 The NetBSD Foundation, Inc.\n\
	All rights reserved.\n");
#endif /* not lint */

#ifndef lint
__RCSID("$NetBSD: eject.c,v 1.10 1999/02/18 20:02:43 tron Exp $");
#endif /* not lint */

#include <ctype.h>
#include <err.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/cdio.h>
#include <sys/disklabel.h>
#include <sys/ioctl.h>
#include <sys/param.h>
#include <sys/ucred.h>
#include <sys/mount.h>
#include <sys/mtio.h>

struct nicknames_s {
    char *name;			/* The name given on the command line. */
    char *devname;		/* The base name of the device */
    int type;			/* The type of device, for determining
				   what ioctl to use. */
#define TAPE 0x10
#define DISK 0x20
    /* OR one of the above with one of the below: */
#define NOTLOADABLE 0x00
#define LOADABLE 0x01
#define TYPEMASK ((int)~0x01)
} nicknames[] = {
    { "diskette", "fd", DISK | NOTLOADABLE },
    { "floppy", "fd", DISK | NOTLOADABLE },
    { "fd", "fd", DISK | NOTLOADABLE },
    { "cdrom", "cd", DISK | LOADABLE },
    { "cd", "cd", DISK | LOADABLE },
    { "mcd", "mcd", DISK | LOADABLE }, /* XXX Is this true? */
    { "tape", "st", TAPE | NOTLOADABLE },
    { "st", "st", TAPE | NOTLOADABLE },
    { "dat", "st", TAPE | NOTLOADABLE },
    { "exabyte", "st", TAPE | NOTLOADABLE },
};
#define MAXNICKLEN 12		/* at least enough room for the
				   longest nickname */
#define MAXDEVLEN (MAXNICKLEN + 7) /* "/dev/r" ... "a" */

struct devtypes_s {
    char *name;
    int type;
} devtypes[] = {
    { "diskette", DISK | NOTLOADABLE },
    { "floppy", DISK | NOTLOADABLE },
    { "cdrom", DISK | LOADABLE },
    { "disk", DISK | NOTLOADABLE },
    { "tape", TAPE | NOTLOADABLE },
};

int verbose_f = 0;
int umount_f = 1;
int load_f = 0;

int main __P((int, char *[]));
void usage __P((void));
char *nick2dev __P((char *));
char *nick2rdev __P((char *));
int guess_devtype __P((char *));
char *guess_nickname __P((char *));
void eject_tape __P((char *));
void eject_disk __P((char *));
void unmount_dev __P((char *));

int
main(int argc,
     char *argv[])
{
    int ch;
    int devtype = -1;
    int n, i;
    char *devname = NULL;

    while((ch = getopt(argc, argv, "d:flnt:v")) != -1) {
	switch(ch) {
	case 'd':
	    devname = optarg;
	    break;
	case 'f':
	    umount_f = 0;
	    break;
	case 'l':
	    load_f = 1;
	    break;
	case 'n':
	    for(n = 0; n < sizeof(nicknames) / sizeof(nicknames[0]);
		n++) {
		struct nicknames_s *np = &nicknames[n];

		printf("%s -> %s\n", np->name, nick2dev(np->name));
	    }
	    return(0);
	case 't':
	    for(i = 0; i < sizeof(devtypes) / sizeof(devtypes[0]);
		i++) {
		if(strcasecmp(devtypes[i].name, optarg) == 0) {
		    devtype = devtypes[i].type;
		    break;
		}
	    }
	    if(devtype == -1) {
		errx(1, "%s: unknown device type\n", optarg);
	    }
	    break;
	case 'v':
	    verbose_f = 1;
	    break;
	default:
	    warnx("-%c: unknown switch", ch);
	    usage();
	    /* NOTREACHED */
	}
    }
    argc -= optind;
    argv += optind;

    if(devname == NULL) {
	if(argc == 0) {
	    usage();
	    /* NOTREACHED */
	} else
	    devname = argv[0];
    }


    if(devtype == -1) {
	devtype = guess_devtype(devname);
    }
    if(devtype == -1) {
	errx(1, "%s: unable to determine type of device",
	     devname);
    }
    if(verbose_f) {
	printf("device type == ");
	if((devtype & TYPEMASK) == TAPE)
	    printf("tape\n");
	else
	    printf("disk, floppy, or cdrom\n");
    }

    if(umount_f)
	unmount_dev(devname);

    /* XXX Tapes and disks have different ioctl's: */
    if((devtype & TYPEMASK) == TAPE)
	eject_tape(devname);
    else
	eject_disk(devname);

    if(verbose_f)
	printf("done.\n");

    return(0);
}

void
usage(void)
{
    errx(1, "Usage: eject [-n][-f][-v][-l][-t type][-d] device | nickname");
}

int
guess_devtype(char *devname)
{
    int n;

    /* Nickname match: */
    for(n = 0; n < sizeof(nicknames) / sizeof(nicknames[0]);
	n++) {
	if(strncasecmp(nicknames[n].name, devname,
		       strlen(nicknames[n].name)) == 0)
	    return(nicknames[n].type);
    }

    /*
     * If we still don't know it, then try to compare vs. dev
     * and rdev names that we know.
     */
    /* dev first: */
    for(n = 0; n < sizeof(nicknames) / sizeof(nicknames[0]); n++) {
	char *name;
	name = nick2dev(nicknames[n].name);
	/*
	 * Assume that the part of the name that distinguishes the
	 * instance of this device begins with a 0.
	 */
	*(strchr(name, '0')) = '\0';
	if(strncmp(name, devname, strlen(name)) == 0)
	    return(nicknames[n].type);
    }

    /* Now rdev: */
    for(n = 0; n < sizeof(nicknames) / sizeof(nicknames[0]); n++) {
	char *name = nick2rdev(nicknames[n].name);
	*(strchr(name, '0')) = '\0';
	if(strncmp(name, devname, strlen(name)) == 0)
	    return(nicknames[n].type);
    }

    /* Not found. */
    return(-1);
}

/* "floppy5" -> "/dev/fd5a".  Yep, this uses a static buffer. */
char *
nick2dev(char *nn)
{
    int n;
    static char devname[MAXDEVLEN];
    int devnum = 0;

    for(n = 0; n < sizeof(nicknames) / sizeof(nicknames[0]); n++) {
	if(strncasecmp(nicknames[n].name, nn,
		       strlen(nicknames[n].name)) == 0) {
	    sscanf(nn, "%*[^0-9]%d", &devnum);
	    sprintf(devname, "/dev/%s%d", nicknames[n].devname,
		    devnum);
	    if((nicknames[n].type & TYPEMASK) != TAPE)
		strcat(devname, "a");
	    return(devname);
	}
    }

    return(NULL);
}

/* "floppy5" -> "/dev/rfd5c".  Static buffer. */
char *
nick2rdev(char *nn)
{
    int n;
    static char devname[MAXDEVLEN];
    int devnum = 0;

    for(n = 0; n < sizeof(nicknames) / sizeof(nicknames[0]); n++) {
	if(strncasecmp(nicknames[n].name, nn,
		       strlen(nicknames[n].name)) == 0) {
	    sscanf(nn, "%*[^0-9]%d", &devnum);
	    sprintf(devname, "/dev/r%s%d", nicknames[n].devname,
		    devnum);
	    if((nicknames[n].type & TYPEMASK) != TAPE) {
		strcat(devname, "a");
		devname[strlen(devname) - 1] += RAW_PART;
	    }
	    return(devname);
	}
    }

    return(NULL);
}

/* Unmount all filesystems attached to dev. */
void
unmount_dev(char *name)
{
    struct statfs *mounts;
    int i, nmnts, len;
    char *dn;

    nmnts = getmntinfo(&mounts, MNT_NOWAIT);
    if(nmnts == 0) {
	err(1, "getmntinfo");
    }

    /* Make sure we have a device name: */
    dn = nick2dev(name);
    if(dn == NULL)
	dn = name;

    /* Set len to strip off the partition name: */
    len = strlen(dn);
    if(!isdigit(dn[len - 1]))
	len--;
    if(!isdigit(dn[len - 1])) {
	errx(1, "Can't figure out base name for dev name %s", dn);
    }

    for(i = 0; i < nmnts; i++) {
	if(strncmp(mounts[i].f_mntfromname, dn, len) == 0) {
	    if(verbose_f)
		printf("Unmounting %s from %s...\n",
		       mounts[i].f_mntfromname,
		       mounts[i].f_mntonname);

	    if(unmount(mounts[i].f_mntonname, 0) == -1) {
		err(1, "unmount: %s", mounts[i].f_mntonname);
	    }
	}
    }

    return;
}

void
eject_tape(char *name)
{
    struct mtop m;
    int fd;
    char *dn;

    dn = nick2rdev(name);
    if(dn == NULL) {
	dn = name;		/* Hope for the best. */
    }

    fd = open(dn, O_RDONLY);
    if(fd == -1) {
	err(1, "open: %s", dn);
    }

    if(verbose_f)
	printf("Ejecting %s...\n", dn);

    m.mt_op = MTOFFL;
    m.mt_count = 0;
    if(ioctl(fd, MTIOCTOP, &m) == -1) {
	err(1, "ioctl: MTIOCTOP: %s", dn);
    }

    close(fd);
    return;
}

void
eject_disk(char *name)
{
    int fd;
    char *dn;
    int arg;

    dn = nick2rdev(name);
    if(dn == NULL) {
	dn = name;		/* Hope for the best. */
    }

    fd = open(dn, O_RDONLY);
    if(fd == -1) {
	err(1, "open: %s", dn);
    }

    if(load_f) {
	if(verbose_f)
	    printf("Closing %s...\n", dn);

	if(ioctl(fd, CDIOCCLOSE, NULL) == -1) {
	    err(1, "ioctl: CDIOCCLOSE: %s", dn);
	}
    } else {
	if(verbose_f)
	    printf("Ejecting %s...\n", dn);

	arg = 0;
	if(ioctl(fd, DIOCLOCK, (char *)&arg) == -1)
	    err(1, "ioctl: DIOCLOCK: %s", dn);
	if(ioctl(fd, DIOCEJECT, &arg) == -1)
	    err(1, "ioctl: DIOCEJECT: %s", dn);
    }

    close(fd);
    return;
}
