/*	$NetBSD: conf.c,v 1.8 1995/04/10 08:06:06 mycroft Exp $	*/

/*-
 * Copyright (c) 1991 The Regents of the University of California.
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
 *
 *      @(#)conf.c	7.9 (Berkeley) 5/28/91
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/buf.h>
#include <sys/ioctl.h>
#include <sys/tty.h>
#include <sys/conf.h>
#include <sys/vnode.h>

int	rawread		__P((dev_t, struct uio *, int));
int	rawwrite	__P((dev_t, struct uio *, int));
void	swstrategy	__P((struct buf *));
int	ttselect	__P((dev_t, int, struct proc *));

#include "sd.h"
bdev_decl(sd);
#include "wd.h"
bdev_decl(wd);
#include "id.h"
bdev_decl(id);
#include "vnd.h"
bdev_decl(vnd);

struct bdevsw	bdevsw[] =
{
	bdev_notdef(),			/* 0 */
	bdev_disk_init(NSD,sd),		/* 1: SCSI disk */
	bdev_disk_init(NWD,wd),		/* 2: WD winchester/floppy ctrler */
	bdev_swap_init(),		/* 3: swap pseudo-device */
	bdev_disk_init(NID,id),		/* 4: IDE disk */
	bdev_notdef(),			/* 5 */
	bdev_disk_init(NVND,vnd),	/* 6: vnode disk driver */
	bdev_notdef(),			/* 7 */
};
int	nblkdev = sizeof(bdevsw) / sizeof(bdevsw[0]);

/* open, close, read, write, ioctl, stop, tty, mmap */
#define	cdev_gsp_init(c,n) { \
	dev_init(c,n,open), dev_init(c,n,close), dev_init(c,n,read), \
	dev_init(c,n,write), dev_init(c,n,ioctl), dev_init(c,n,stop), \
	(dev_type_reset((*))) nullop, dev_tty_init(c,n), ttselect, \
	dev_init(c,n,map), 0 }

cdev_decl(cn);
cdev_decl(ctty);
#define	mmread	mmrw
#define	mmwrite	mmrw
#include "pty.h"
#define	pts_tty		pt_tty
#define	ptsioctl	ptyioctl
cdev_decl(pts);
#define	ptc_tty		pt_tty
#define	ptcioctl	ptyioctl
cdev_decl(ptc);
cdev_decl(log);
cdev_decl(sd);
cdev_decl(wd);
cdev_decl(id);
#include "zs.h"
cdev_decl(zs);
#include "gsp.h"
cdev_decl(gsp);
cdev_decl(vnd);
cdev_decl(fd);
#include "bpfilter.h"
cdev_decl(bpf);

struct cdevsw	cdevsw[] =
{
	cdev_cn_init(1,cn),		/* 0: virtual console */
	cdev_ctty_init(1,ctty),		/* 1: controlling terminal */
	cdev_mm_init(1,mm),		/* 2: /dev/{null,mem,kmem,...} */
	cdev_swap_init(1,sw),		/* 3: /dev/drum (swap pseudo-device) */
	cdev_tty_init(NPTY,pts),	/* 4: pseudo-tty slave */
	cdev_ptc_init(NPTY,ptc),	/* 5: pseudo-tty master */
	cdev_log_init(1,log),		/* 6: /dev/klog */
	cdev_notdef(),			/* 7 */
	cdev_disk_init(NSD,sd),		/* 8: SCSI disk */
	cdev_disk_init(NWD,wd),		/* 9: winchester/floppy disk */
	cdev_disk_init(NID,id),		/* 10: IDE disk */
	cdev_notdef(),			/* 11: printer/plotter interface */
	cdev_tty_init(NZS,zs),		/* 12: SCC serial ports */
	cdev_gsp_init(NGSP,gsp),	/* 13: console terminal emulator */
	cdev_notdef(),			/* 14: human interface loop */
	cdev_notdef(),			/* 15: 4-port serial */
	cdev_notdef(),			/* 16 */
	cdev_notdef(),			/* 17: concatenated disk */
	cdev_notdef(),			/* 18: mapped clock */
	cdev_disk_init(NVND,vnd),	/* 19: vnode disk */
	cdev_notdef(),			/* 20: exabyte tape */
	cdev_fd_init(1,fd),		/* 21: file descriptor pseudo-dev */
	cdev_bpftun_init(NBPFILTER,bpf),/* 22: berkeley packet filter */
};
int	nchrdev = sizeof(cdevsw) / sizeof(cdevsw[0]);

int	mem_no = 2; 	/* major device number of memory special file */

/*
 * Swapdev is a fake device implemented
 * in sw.c used only internally to get to swstrategy.
 * It cannot be provided to the users, because the
 * swstrategy routine munches the b_dev and b_blkno entries
 * before calling the appropriate driver.  This would horribly
 * confuse, e.g. the hashing routines. Instead, /dev/drum is
 * provided as a character (raw) device.
 */
dev_t	swapdev = makedev(3, 0);

/*
 * Returns true if dev is /dev/mem or /dev/kmem.
 */
iskmemdev(dev)
	dev_t dev;
{

	return (major(dev) == mem_no && minor(dev) < 2);
}

/*
 * Returns true if dev is /dev/zero.
 */
iszerodev(dev)
	dev_t dev;
{

	return (major(dev) == mem_no && minor(dev) == 12);
}

static int chrtoblktbl[] = {
	/* XXXX This needs to be dynamic for LKMs. */
	/*VCHR*/	/*VBLK*/
	/*  0 */	NODEV,
	/*  1 */	NODEV,
	/*  2 */	NODEV,
	/*  3 */	NODEV,
	/*  4 */	NODEV,
	/*  5 */	NODEV,
	/*  6 */	NODEV,
	/*  7 */	NODEV,
	/*  8 */	1,
	/*  9 */	2,
	/* 10 */	4,
	/* 11 */	NODEV,
	/* 12 */	NODEV,
	/* 13 */	NODEV,
	/* 14 */	NODEV,
	/* 15 */	NODEV,
	/* 16 */	NODEV,
	/* 17 */	NODEV,
	/* 18 */	NODEV,
	/* 19 */	6,
	/* 20 */	NODEV,
	/* 21 */	NODEV,
	/* 22 */	NODEV,
};

/*
 * Convert a character device number to a block device number.
 */
chrtoblk(dev)
	dev_t dev;
{
	int blkmaj;

	if (major(dev) >= nchrdev)
		return (NODEV);
	blkmaj = chrtoblktbl[major(dev)];
	if (blkmaj == NODEV)
		return (NODEV);
	return (makedev(blkmaj, minor(dev)));
}

/*
 * This entire table could be autoconfig()ed but that would mean that
 * the kernel's idea of the console would be out of sync with that of
 * the standalone boot.  I think it best that they both use the same
 * known algorithm unless we see a pressing need otherwise.
 */
#include <dev/cons.h>

cons_decl(gsp);
cons_decl(zs);

struct	consdev constab[] = {
#if NGSP > 0
	cons_init(gsp),
#endif
#if NZS > 0
	cons_init(zs),
#endif
	{ 0 },
};
