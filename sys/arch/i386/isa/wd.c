/*	$NetBSD: wd.c,v 1.120 1994/11/30 02:32:03 mycroft Exp $	*/

/*
 * Copyright (c) 1994 Charles Hannum.  All rights reserved.
 *
 * DMA and multi-sector PIO handling are derived from code contributed by
 * Onno van der Linden.
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
 *	This product includes software developed by Charles Hannum.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * William Jolitz.
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
 *	@(#)wd.c	7.2 (Berkeley) 5/9/91
 */

#define	INSTRUMENT	/* instrumentation stuff by Brad Parker */

#include <sys/param.h>
#include <sys/dkbad.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/conf.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/buf.h>
#include <sys/uio.h>
#include <sys/malloc.h>
#include <sys/device.h>
#include <sys/disklabel.h>
#include <sys/disk.h>
#include <sys/syslog.h>
#ifdef INSTRUMENT
#include <sys/dkstat.h>
#endif

#include <vm/vm.h>

#include <machine/cpu.h>
#include <machine/pio.h>

#include <i386/isa/isavar.h>
#include <i386/isa/wdreg.h>

#define WDCNDELAY	100000	/* delay = 100us; so 10s for a controller state change */
#define WDCDELAY	100

#define	WAITTIME	(4 * hz)	/* time to wait for a completion */
#define	RECOVERYTIME	(hz / 2)	/* time to recover from an error */

#if 0
/* If you enable this, it will report any delays more than 100us * N long. */
#define WDCNDELAY_DEBUG	10
#endif

#define	WDIORETRIES	5	/* number of retries before giving up */

#define	WDUNIT(dev)			DISKUNIT(dev)
#define	WDPART(dev)			DISKPART(dev)
#define	MAKEWDDEV(maj, unit, part)	MAKEDISKDEV(maj, unit, part)

#define	WDLABELDEV(dev)	(MAKEWDDEV(major(dev), WDUNIT(dev), RAW_PART))

#define b_cylin	b_resid		/* cylinder number for doing IO to */
				/* shares an entry in the buf struct */

/*
 * Drive status.
 */
struct wd_softc {
	struct device sc_dev;
	struct dkdevice sc_dk;

	long	sc_bcount;	/* byte count left */
	short	sc_skip;	/* blocks already transferred */
	char	sc_drive;	/* physical unit number */
	char	sc_state;	/* control state */
#define	RECAL		0		/* recalibrate */
#define	RECAL_WAIT	1		/* done recalibrating */
#define	GEOMETRY	2		/* upload geometry */
#define	GEOMETRY_WAIT	3		/* done uploading geometry */
#define	MULTIMODE	4		/* set multiple mode */
#define	MULTIMODE_WAIT	5		/* done setting multiple mode */
#define	OPEN		6		/* done with open */
	char	sc_mode;	/* transfer mode */
#define	WDM_PIOSINGLE	0		/* single-sector PIO */
#define	WDM_PIOMULTI	1		/* multi-sector PIO */
#define	WDM_DMA		2		/* DMA */
	u_char	sc_multiple;	/* multiple for WDM_PIOMULTI */
	u_char	sc_flags;	/* drive characteistics found */
#define	WDF_LOCKED	0x01
#define	WDF_WANTED	0x02
#define	WDF_LOADED	0x04
#define	WDF_BSDLABEL	0x08		/* has a BSD disk label */
#define	WDF_WLABEL	0x10		/* label is writable */
#define	WDF_32BIT	0x20		/* can do 32-bit transfer */
	TAILQ_ENTRY(wd_softc) sc_drivechain;
	struct buf sc_q;
	struct wdparams sc_params; /* ESDI/IDE drive/controller parameters */
	long	sc_badsect[127];	/* 126 plus trailing -1 marker */
};

struct wdc_softc {
	struct device sc_dev;
	struct intrhand sc_ih;

	u_char	sc_flags;
#define	WDCF_ACTIVE	0x01	/* controller is active */
#define	WDCF_SINGLE	0x02	/* sector at a time mode */
#define	WDCF_ERROR	0x04	/* processing a disk error */
	u_char	sc_status;	/* copy of status register */
	u_char	sc_error;	/* copy of error register */
	int	sc_iobase;	/* I/O port base */
	int	sc_drq;		/* DMA channel */
	int	sc_errors;	/* count of errors during current transfer */
	int	sc_nblks;	/* number of blocks currently transferring */
	TAILQ_HEAD(drivehead, wd_softc) sc_drives;
};

int wdcprobe __P((struct device *, void *, void *));
void wdcattach __P((struct device *, struct device *, void *));

struct cfdriver wdccd = {
	NULL, "wdc", wdcprobe, wdcattach, DV_DULL, sizeof(struct wd_softc)
};

int wdprobe __P((struct device *, void *, void *));
void wdattach __P((struct device *, struct device *, void *));

struct cfdriver wdcd = {
	NULL, "wd", wdprobe, wdattach, DV_DISK, sizeof(struct wd_softc)
};

void wdgetdisklabel __P((struct wd_softc *));
int wd_get_parms __P((struct wd_softc *));
void wdstrategy __P((struct buf *));
void wdstart __P((struct wd_softc *));

struct dkdriver wddkdriver = { wdstrategy };

void wdfinish __P((struct wd_softc *, struct buf *));
int wdcintr __P((struct wdc_softc *));
static void wdcstart __P((struct wdc_softc *));
static int wdcommand __P((struct wd_softc *, int, int, int, int, int));
static int wdcommandshort __P((struct wdc_softc *, int, int));
static int wdcontrol __P((struct wd_softc *));
static int wdsetctlr __P((struct wd_softc *));
static void bad144intern __P((struct wd_softc *));
static int wdcreset __P((struct wdc_softc *));
static void wdcrestart __P((void *arg));
static void wdcunwedge __P((struct wdc_softc *));
static void wdctimeout __P((void *arg));
static void wderror __P((void *, struct buf *, char *));
int wdcwait __P((struct wdc_softc *, int));
/* ST506 spec says that if READY or SEEKCMPLT go off, then the read or write
   command is aborted. */
#define	wait_for_drq(d)		wdcwait(d, WDCS_DRDY | WDCS_DSC | WDCS_DRQ)
#define	wait_for_ready(d)	wdcwait(d, WDCS_DRDY | WDCS_DSC)
#define	wait_for_unbusy(d)	wdcwait(d, 0)

/*
 * Probe for controller.
 */
int
wdcprobe(parent, match, aux)
	struct device *parent;
	void *match, *aux;
{
	struct wdc_softc *wdc = match;
	struct isa_attach_args *ia = aux;
	int iobase;

	wdc->sc_iobase = iobase = ia->ia_iobase;

	/* Check if we have registers that work. */
	outb(iobase+wd_error, 0x5a);	/* Error register not writable. */
	outb(iobase+wd_cyl_lo, 0xa5);	/* But all of cyllo are implemented. */
	if (inb(iobase+wd_error) == 0x5a || inb(iobase+wd_cyl_lo) != 0xa5)
		return 0;

	if (wdcreset(wdc) != 0) {
		delay(500000);
		if (wdcreset(wdc) != 0)
			return 0;
	}

	outb(iobase+wd_sdh, WDSD_IBM | 0);

	/* Wait for controller to become ready. */
	if (wait_for_unbusy(wdc) < 0)
		return 0;
    
	/* Send command. */
	outb(iobase+wd_command, WDCC_DIAGNOSE);

	/* Wait for command to complete. */
	if (wait_for_unbusy(wdc) != 0)
		return 0;

	ia->ia_iosize = 8;
	ia->ia_msize = 0;
	return 1;
}

struct wdc_attach_args {
	int wa_drive;
};

int
wdprint(aux, wdc)
	void *aux;
	char *wdc;
{
	struct wdc_attach_args *wa = aux;

	if (!wdc)
		printf(" drive %d", wa->wa_drive);
	return QUIET;
}

void
wdcattach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct wdc_softc *wdc = (void *)self;
	struct isa_attach_args *ia = aux;
	struct wdc_attach_args wa;

	TAILQ_INIT(&wdc->sc_drives);
	wdc->sc_drq = ia->ia_drq;

	printf("\n");

	wdc->sc_ih.ih_fun = wdcintr;
	wdc->sc_ih.ih_arg = wdc;
	wdc->sc_ih.ih_level = IPL_BIO;
	intr_establish(ia->ia_irq, &wdc->sc_ih);

	for (wa.wa_drive = 0; wa.wa_drive < 2; wa.wa_drive++)
		(void)config_found(self, (void *)&wa, wdprint);
}

int
wdprobe(parent, match, aux)
	struct device *parent;
	void *match, *aux;
{
	struct wdc_softc *wdc = (void *)parent;
	struct cfdata *cf = match;
	struct wdc_attach_args *wa = aux;
	int drive = wa->wa_drive;

	if (cf->cf_loc[0] != -1 && cf->cf_loc[0] != drive)
		return 0;
	
	if (wdcommandshort(wdc, drive, WDCC_RECAL) != 0 ||
	    wait_for_ready(wdc) != 0)
		return 0;

	return 1;
}

void
wdattach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct wd_softc *wd = (void *)self;
	struct wdc_softc *wdc = (void *)parent;
	struct wdc_attach_args *wa = aux;
	int i, blank;

	wd->sc_drive = wa->wa_drive;

	wd_get_parms(wd);
	printf(": %dMB, %d cyl, %d head, %d sec, %d bytes/sec <",
	    wd->sc_params.wdp_cylinders *
	    (wd->sc_params.wdp_heads * wd->sc_params.wdp_sectors) /
	    (1048576 / DEV_BSIZE),
	    wd->sc_params.wdp_cylinders,
	    wd->sc_params.wdp_heads,
	    wd->sc_params.wdp_sectors,
	    DEV_BSIZE);
	for (i = blank = 0; i < sizeof(wd->sc_params.wdp_model); i++) {
		char c = wd->sc_params.wdp_model[i];
		if (c == '\0')
			break;
		if (c != ' ') {
			if (blank)
				printf(" %c", c);
			else
				printf("%c", c);
			blank = 0;
		} else
			blank = 1;
	}
	printf(">\n");

	if ((wd->sc_params.wdp_capabilities & WD_CAP_DMA) != 0 &&
	    wdc->sc_drq != DRQUNK) {
		wd->sc_mode = WDM_DMA;
	} else if (wd->sc_params.wdp_maxmulti > 1) {
		wd->sc_mode = WDM_PIOMULTI;
		wd->sc_multiple = min(wd->sc_params.wdp_maxmulti, 16);
	} else {
		wd->sc_mode = WDM_PIOSINGLE;
		wd->sc_multiple = 1;
	}

	printf("%s: using", wd->sc_dev.dv_xname);
	if (wd->sc_mode == WDM_DMA)
		printf(" dma transfers,");
	else
		printf(" %d-sector %d-bit pio transfers,",
		    wd->sc_multiple, (wd->sc_flags & WDF_32BIT) == 0 ? 16 : 32);
	if ((wd->sc_params.wdp_capabilities & WD_CAP_LBA) != 0)
		printf(" lba addressing\n");
	else
		printf(" chs addressing\n");

	wd->sc_dk.dk_driver = &wddkdriver;
}

/*
 * Read/write routine for a buffer.  Finds the proper unit, range checks
 * arguments, and schedules the transfer.  Does not wait for the transfer to
 * complete.  Multi-page transfers are supported.  All I/O requests must be a
 * multiple of a sector in length.
 */
void
wdstrategy(bp)
	struct buf *bp;
{
	struct wd_softc *wd;	/* disk unit to do the IO */
	int unit = WDUNIT(bp->b_dev);
	int s;
    
	/* Valid unit, controller, and request?  */
	if (unit >= wdcd.cd_ndevs ||
	    (wd = wdcd.cd_devs[unit]) == 0 ||
	    bp->b_blkno < 0 ||
	    (bp->b_bcount % DEV_BSIZE) != 0 ||
	    (bp->b_bcount / DEV_BSIZE) >= (1 << NBBY)) {
		bp->b_error = EINVAL;
		goto bad;
	}
    
#if 0
	/* "Soft" write protect check. */
	if ((wd->sc_flags & WDF_WRITEPROT) && (bp->b_flags & B_READ) == 0) {
		bp->b_error = EROFS;
		goto bad;
	}
#endif
    
	/* If it's a null transfer, return immediately. */
	if (bp->b_bcount == 0)
		goto done;

	/* Have partitions and want to use them? */
	if (WDPART(bp->b_dev) != RAW_PART) {
		if ((wd->sc_flags & WDF_BSDLABEL) == 0) {
			bp->b_error = EIO;
			goto bad;
		}
		/*
		 * Do bounds checking, adjust transfer. if error, process.
		 * If end of partition, just return.
		 */
		if (bounds_check_with_label(bp, &wd->sc_dk.dk_label,
		    (wd->sc_flags & WDF_WLABEL) != 0) <= 0)
			goto done;
		/* Otherwise, process transfer request. */
	}
    
	/* Don't bother doing rotational optimization. */
	bp->b_cylin = 0;

	/* Queue transfer on drive, activate drive and controller if idle. */
	s = splbio();
	disksort(&wd->sc_q, bp);
	if (!wd->sc_q.b_active)
		wdstart(wd);		/* Start drive. */
#if 0
	else {
		struct wdc_softc *wdc = (void *)wd->sc_dev.dv_parent;
		if ((wdc->sc_flags & (WDCF_ACTIVE|WDCF_ERROR)) == 0) {
			printf("wdstrategy: controller inactive\n");
			wdcstart(wdc);
		}
	}
#endif
	splx(s);
	return;
    
bad:
	bp->b_flags |= B_ERROR;
done:
	/* Toss transfer; we're done early. */
	biodone(bp);
}

/*
 * Routine to queue a command to the controller.  The unit's request is linked
 * into the active list for the controller.  If the controller is idle, the
 * transfer is started.
 */
void
wdstart(wd)
	struct wd_softc *wd;
{
	struct wdc_softc *wdc = (void *)wd->sc_dev.dv_parent;
	int active = wdc->sc_drives.tqh_first != 0;

	/* Link onto controller queue. */
	wd->sc_q.b_active = 1;
	TAILQ_INSERT_TAIL(&wdc->sc_drives, wd, sc_drivechain);
    
	/* If controller not already active, start it. */
	if (!active)
		wdcstart(wdc);
}

void
wdfinish(wd, bp)
	struct wd_softc *wd;
	struct buf *bp;
{
	struct wdc_softc *wdc = (void *)wd->sc_dev.dv_parent;

#ifdef INSTRUMENT
	dk_busy &= ~(1 << wd->sc_dev.dv_unit);
#endif
	wdc->sc_flags &= ~(WDCF_SINGLE | WDCF_ERROR);
	wdc->sc_errors = 0;
	/*
	 * Move this drive to the end of the queue to give others a `fair'
	 * chance.
	 */
	if (wd->sc_drivechain.tqe_next) {
		TAILQ_REMOVE(&wdc->sc_drives, wd, sc_drivechain);
		if (bp->b_actf) {
			TAILQ_INSERT_TAIL(&wdc->sc_drives, wd,
			    sc_drivechain);
		} else
			wd->sc_q.b_active = 0;
	}
	bp->b_resid = wd->sc_bcount;
	wd->sc_skip = 0;
	wd->sc_q.b_actf = bp->b_actf;
	biodone(bp);
}

/*
 * Controller startup routine.  This does the calculation, and starts a
 * single-sector read or write operation.  Called to start a transfer, or from
 * the interrupt routine to continue a multi-sector transfer.
 * RESTRICTIONS:
 * 1.	The transfer length must be an exact multiple of the sector size.
 */
static void
wdcstart(wdc)
	struct wdc_softc *wdc;
{
	struct wd_softc *wd;	/* disk unit for IO */
	struct buf *bp;
	int nblks;

loop:
	/* Is there a drive for the controller to do a transfer with? */
	wd = wdc->sc_drives.tqh_first;
	if (wd == NULL)
		return;
    
	/* Is there a transfer to this drive?  If not, deactivate drive. */
	bp = wd->sc_q.b_actf;
	if (bp == NULL) {
		TAILQ_REMOVE(&wdc->sc_drives, wd, sc_drivechain);
		wd->sc_q.b_active = 0;
		goto loop;
	}
    
	if (wdc->sc_errors >= WDIORETRIES) {
		wderror(wd, bp, "hard error");
		bp->b_error = EIO;
		bp->b_flags |= B_ERROR;
		wdfinish(wd, bp);
		goto loop;
	}

	/* Do control operations specially. */
	if (wd->sc_state < OPEN) {
		/*
		 * Actually, we want to be careful not to mess with the control
		 * state if the device is currently busy, but we can assume
		 * that we never get to this point if that's the case.
		 */
		if (wdcontrol(wd) == 0) {
			/* The drive is busy.  Wait. */
			return;
		}
	}

	/*
	 * WDCF_ERROR is set by wdcunwedge() and wdcintr() when an error is
	 * encountered.  If we are in multi-sector mode, then we switch to
	 * single-sector mode and retry the operation from the start.
	 */
	if (wdc->sc_flags & WDCF_ERROR) {
		wdc->sc_flags &= ~WDCF_ERROR;
		if ((wdc->sc_flags & WDCF_SINGLE) == 0) {
			wdc->sc_flags |= WDCF_SINGLE;
			wd->sc_skip = 0;
		}
	}

	if (wd->sc_skip == 0) {
#ifdef WDDEBUG
		printf("\n%s: wdcstart %s %d@%d; map ", wd->sc_dev.dv_xname,
		    (bp->b_flags & B_READ) ? "read" : "write", bp->b_bcount,
		    bp->b_blkno);
#endif
		wd->sc_bcount = bp->b_bcount;
#ifdef INSTRUMENT
		dk_busy |= (1 << wd->sc_dev.dv_unit);
		dk_wds[wd->sc_dev.dv_unit] += bp->b_bcount >> 6;
#endif
	} else {
#ifdef WDDEBUG
		printf(" %d)%x", wd->sc_skip, inb(wd->sc_iobase+wd_altsts));
#endif
	}

	/* If starting a multisector transfer, or doing single transfers. */
	if (wd->sc_skip == 0 || (wdc->sc_flags & WDCF_SINGLE) != 0) {
		struct disklabel *lp;
		long blkno;
		long cylin, head, sector;
		int command;

		lp = &wd->sc_dk.dk_label;
		blkno = bp->b_blkno / (lp->d_secsize / DEV_BSIZE) + wd->sc_skip;
		if (WDPART(bp->b_dev) != RAW_PART)
			blkno += lp->d_partitions[WDPART(bp->b_dev)].p_offset;
		if ((wdc->sc_flags & WDCF_SINGLE) != 0)
			nblks = 1;
		else if (wd->sc_mode != WDM_DMA)
			nblks = wd->sc_bcount / DEV_BSIZE;
		else
			nblks = min(wd->sc_bcount / DEV_BSIZE, 8);

		/* Check for bad sectors and adjust transfer, if necessary. */
		if ((lp->d_flags & D_BADSECT) != 0
#ifdef B_FORMAT
		    && (bp->b_flags & B_FORMAT) == 0
#endif
		    ) {
			long blkdiff;
			int i;

			for (i = 0; (blkdiff = wd->sc_badsect[i]) != -1; i++) {
				blkdiff -= blkno;
				if (blkdiff < 0)
					continue;
				if (blkdiff == 0) {
					/* Replace current block of transfer. */
					blkno =
					    lp->d_secperunit - lp->d_nsectors - i - 1;
				}
				if (blkdiff < nblks) {
					/* Bad block inside transfer. */
					wdc->sc_flags |= WDCF_SINGLE;
					nblks = 1;
				}
				break;
			}
			/* Tranfer is okay now. */
		}

		if ((wd->sc_params.wdp_capabilities & WD_CAP_LBA) != 0) {
			sector = (blkno >> 0) & 0xff;
			cylin = (blkno >> 8) & 0xffff;
			head = (blkno >> 24) & 0xf;
			head |= WDSD_LBA;
		} else {
			sector = blkno % lp->d_nsectors;
			sector++;	/* Sectors begin with 1, not 0. */
			blkno /= lp->d_nsectors;
			head = blkno % lp->d_ntracks;
			blkno /= lp->d_ntracks;
			cylin = blkno;
			head |= WDSD_CHS;
		}

#ifdef INSTRUMENT
		++dk_seek[wd->sc_dev.dv_unit];
		++dk_xfer[wd->sc_dev.dv_unit];
#endif

#ifdef B_FORMAT
		if (bp->b_flags & B_FORMAT) {
			sector = lp->d_gap3;
			nblks = lp->d_nsectors;
			command = WDCC_FORMAT;
		} else
#endif
		switch (wd->sc_mode) {
		case WDM_DMA:
			command = (bp->b_flags & B_READ) ?
			    WDCC_READDMA : WDCC_WRITEDMA;
			isa_dmastart(bp->b_flags & B_READ, bp->b_data,
			    nblks * DEV_BSIZE, wdc->sc_drq);
			break;
		case WDM_PIOMULTI:
			command = (bp->b_flags & B_READ) ?
			    WDCC_READMULTI : WDCC_WRITEMULTI;
			break;
		case WDM_PIOSINGLE:
			command = (bp->b_flags & B_READ) ?
			    WDCC_READ : WDCC_WRITE;
			break;
		}
	
		/* Initiate command! */
		if (wdcommand(wd, command, cylin, head, sector, nblks) != 0) {
			wderror(wd, NULL,
			    "wdcstart: timeout waiting for unbusy");
			wdcunwedge(wdc);
			return;
		}
#ifdef WDDEBUG
		printf("sector %d cylin %d head %d addr %x sts %x\n", sector,
		    cylin, head, bp->b_data, inb(wd->sc_iobase+wd_altsts));
#endif
	}

	if (wd->sc_mode == WDM_PIOSINGLE ||
	    (wdc->sc_flags & WDCF_SINGLE) != 0)
		nblks = 1;
	else if (wd->sc_mode != WDM_DMA)
		nblks = min(wd->sc_bcount / DEV_BSIZE, wd->sc_multiple);
	else
		nblks = min(wd->sc_bcount / DEV_BSIZE, 8);
	wdc->sc_nblks = nblks;
    
	/* If this was a write and not using DMA, push the data. */
	if (wd->sc_mode != WDM_DMA &&
	    (bp->b_flags & B_READ) == 0) {
		if (wait_for_drq(wdc) < 0) {
			wderror(wd, NULL, "wdcstart: timeout waiting for drq");
			wdcunwedge(wdc);
			return;
		}

		/* Then send it! */
		if ((wd->sc_flags & WDF_32BIT) == 0)
			outsw(wdc->sc_iobase+wd_data,
			    bp->b_data + wd->sc_skip * DEV_BSIZE,
			    nblks * DEV_BSIZE / sizeof(short));
		else
			outsl(wdc->sc_iobase+wd_data,
			    bp->b_data + wd->sc_skip * DEV_BSIZE,
			    nblks * DEV_BSIZE / sizeof(long));
	}

	wdc->sc_flags |= WDCF_ACTIVE;
	timeout(wdctimeout, wdc, WAITTIME);
}

/*
 * Interrupt routine for the controller.  Acknowledge the interrupt, check for
 * errors on the current operation, mark it done if necessary, and start the
 * next request.  Also check for a partially done transfer, and continue with
 * the next chunk if so.
 */
int
wdcintr(wdc)
	struct wdc_softc *wdc;
{
	struct wd_softc *wd;
	struct buf *bp;
	int nblks;

	if ((wdc->sc_flags & WDCF_ACTIVE) == 0) {
		/* Clear the pending interrupt. */
		(void) inb(wdc->sc_iobase+wd_status);
		return 0;
	}

	wdc->sc_flags &= ~WDCF_ACTIVE;
	untimeout(wdctimeout, wdc);

	wd = wdc->sc_drives.tqh_first;
	bp = wd->sc_q.b_actf;

#ifdef WDDEBUG
	printf("I%d ", ctrlr);
#endif

	if (wait_for_unbusy(wdc) < 0) {
		wderror(wd, NULL, "wdcintr: timeout waiting for unbusy");
		wdc->sc_status |= WDCS_ERR;	/* XXX */
	}
    
	/* Is it not a transfer, but a control operation? */
	if (wd->sc_state < OPEN) {
		if (wdcontrol(wd) == 0) {
			/* The drive is busy.  Wait. */
			return 1;
		}
		wdcstart(wdc);
		return 1;
	}

	nblks = wdc->sc_nblks;
    
	if (wd->sc_mode == WDM_DMA)
		isa_dmadone(bp->b_flags & B_READ, bp->b_data,
		    nblks * DEV_BSIZE, wdc->sc_drq);

	/* Have we an error? */
	if (wdc->sc_status & WDCS_ERR) {
	lose:
#ifdef WDDEBUG
		wderror(wd, NULL, "wdcintr");
#endif
		if ((wdc->sc_flags & WDCF_SINGLE) == 0) {
			wdc->sc_flags |= WDCF_ERROR;
			goto restart;
		}

#ifdef B_FORMAT
		if (bp->b_flags & B_FORMAT)
			goto bad;
#endif
	
		if (++wdc->sc_errors < WDIORETRIES)
			goto restart;
		wderror(wd, bp, "hard error");

	bad:
		bp->b_error = EIO;
		bp->b_flags |= B_ERROR;
		goto done;
	}

	if (wdc->sc_status & WDCS_CORR)
		wderror(wd, bp, "soft ecc");
    
	/* If this was a read and not using DMA, fetch the data. */
	if (wd->sc_mode != WDM_DMA &&
	    (bp->b_flags & B_READ) != 0) {
		if ((wdc->sc_status & (WDCS_DRDY | WDCS_DSC | WDCS_DRQ))
		    != (WDCS_DRDY | WDCS_DSC | WDCS_DRQ)) {
			wderror(wd, NULL, "wdcintr: read intr before drq");
			wdcunwedge(wdc);
			return 1;
		}

		/* Suck in data. */
		if ((wd->sc_flags & WDF_32BIT) == 0)
			insw(wdc->sc_iobase+wd_data,
			    bp->b_data + wd->sc_skip * DEV_BSIZE, 
			    nblks * DEV_BSIZE / sizeof(short));
		else
			insl(wdc->sc_iobase+wd_data,
			    bp->b_data + wd->sc_skip * DEV_BSIZE, 
			    nblks * DEV_BSIZE / sizeof(long));
	}
    
	/* If we encountered any abnormalities, flag it as a soft error. */
	if (wdc->sc_errors) {
		wderror(wd, bp, "soft error");
		wdc->sc_errors = 0;
	}
    
	/* Ready for the next block, if any. */
	wd->sc_skip += nblks;
	wd->sc_bcount -= nblks * DEV_BSIZE;

	/* See if more to transfer. */
	if (wd->sc_bcount > 0)
		goto restart;

done:
	/* Done with this transfer, with or without error. */
	wdfinish(wd, bp);

restart:
	/* Start the next transfer, if any. */
	wdcstart(wdc);

	return 1;
}

/*
 * Initialize a drive.
 */
int
wdopen(dev, flag, fmt)
	dev_t dev;
	int flag, fmt;
{
	int error;
	int unit, part;
	struct wd_softc *wd;
    
	unit = WDUNIT(dev);
	if (unit >= wdcd.cd_ndevs)
		return ENXIO;
	wd = wdcd.cd_devs[unit];
	if (wd == 0)
		return ENXIO;
    
	part = WDPART(dev);

	while ((wd->sc_flags & WDF_LOCKED) != 0) {
		wd->sc_flags |= WDF_WANTED;
		if ((error = tsleep(wd, PRIBIO | PCATCH, "wdopn", 0)) != 0)
			return error;
	}

	if (wd->sc_dk.dk_openmask != 0) {
		/*
		 * If any partition is open, but the disk has been invalidated,
		 * disallow further opens.
		 */
		if ((wd->sc_flags & WDF_LOADED) == 0)
			return ENXIO;
	} else {
		wd->sc_flags |= WDF_LOCKED;

		if ((wd->sc_flags & WDF_LOADED) == 0) {
			wd->sc_flags &= ~WDF_BSDLABEL;
			wd->sc_flags |= WDF_LOADED;

			/* Load the physical device parameters. */
			if (wd_get_parms(wd) != 0) {
				error = ENXIO;
				goto bad2;
			}

			/* Load the partition info if not already loaded. */
			wdgetdisklabel(wd);
		}

		wd->sc_flags &= ~WDF_LOCKED;
		if ((wd->sc_flags & WDF_WANTED) != 0) {
			wd->sc_flags &= ~WDF_WANTED;
			wakeup(wd);
		}
	}

	/* Check that the partition exists. */
	if (part != RAW_PART &&
	    (part >= wd->sc_dk.dk_label.d_npartitions ||
	     wd->sc_dk.dk_label.d_partitions[part].p_fstype == FS_UNUSED)) {
		error = ENXIO;
		goto bad;
	}
    
	/* Insure only one open at a time. */
	switch (fmt) {
	case S_IFCHR:
		wd->sc_dk.dk_copenmask |= (1 << part);
		break;
	case S_IFBLK:
		wd->sc_dk.dk_bopenmask |= (1 << part);
		break;
	}
	wd->sc_dk.dk_openmask = wd->sc_dk.dk_copenmask | wd->sc_dk.dk_bopenmask;

	return 0;

bad2:
	wd->sc_flags &= ~WDF_LOADED;

bad:
	if (wd->sc_dk.dk_openmask == 0) {
		wd->sc_flags &= ~WDF_LOCKED;
		if ((wd->sc_flags & WDF_WANTED) != 0) {
			wd->sc_flags &= ~WDF_WANTED;
			wakeup(wd);
		}
	}

	return error;
}

void
wdgetdisklabel(wd)
	struct wd_softc *wd;
{
	char *errstring;

	if ((wd->sc_flags & WDF_BSDLABEL) != 0)
		return;

	bzero(&wd->sc_dk.dk_label, sizeof(struct disklabel));
	bzero(&wd->sc_dk.dk_cpulabel, sizeof(struct cpu_disklabel));

	wd->sc_dk.dk_label.d_secsize = DEV_BSIZE;
	wd->sc_dk.dk_label.d_ntracks = wd->sc_params.wdp_heads;
	wd->sc_dk.dk_label.d_nsectors = wd->sc_params.wdp_sectors;
	wd->sc_dk.dk_label.d_ncylinders = wd->sc_params.wdp_cylinders;
	wd->sc_dk.dk_label.d_secpercyl =
	    wd->sc_dk.dk_label.d_ntracks * wd->sc_dk.dk_label.d_nsectors;

#if 0
	strncpy(wd->sc_dk.dk_label.d_typename, "ST506 disk", 16);
	wd->sc_dk.dk_label.d_type = DTYPE_ST506;
#endif
	strncpy(wd->sc_dk.dk_label.d_packname, wd->sc_params.wdp_model, 16);
	wd->sc_dk.dk_label.d_secperunit =
	    wd->sc_dk.dk_label.d_secpercyl * wd->sc_dk.dk_label.d_ncylinders;
	wd->sc_dk.dk_label.d_rpm = 3600;
	wd->sc_dk.dk_label.d_interleave = 1;
	wd->sc_dk.dk_label.d_flags = 0;

	wd->sc_dk.dk_label.d_partitions[RAW_PART].p_offset = 0;
	wd->sc_dk.dk_label.d_partitions[RAW_PART].p_size =
	    wd->sc_dk.dk_label.d_secperunit *
	    (wd->sc_dk.dk_label.d_secsize / DEV_BSIZE);
	wd->sc_dk.dk_label.d_partitions[RAW_PART].p_fstype = FS_UNUSED;
	wd->sc_dk.dk_label.d_npartitions = RAW_PART + 1;

	wd->sc_dk.dk_label.d_magic = DISKMAGIC;
	wd->sc_dk.dk_label.d_magic2 = DISKMAGIC;
	wd->sc_dk.dk_label.d_checksum = dkcksum(&wd->sc_dk.dk_label);

	wd->sc_badsect[0] = -1;

	if (wd->sc_state > RECAL)
		wd->sc_state = RECAL;
	errstring = readdisklabel(MAKEWDDEV(0, wd->sc_dev.dv_unit, RAW_PART),
	    wdstrategy, &wd->sc_dk.dk_label, &wd->sc_dk.dk_cpulabel);
	if (errstring) {
		/*
		 * This probably happened because the drive's default
		 * geometry doesn't match the DOS geometry.  We
		 * assume the DOS geometry is now in the label and try
		 * again.  XXX This is a kluge.
		 */
		if (wd->sc_state > GEOMETRY)
			wd->sc_state = GEOMETRY;
		errstring = readdisklabel(MAKEWDDEV(0, wd->sc_dev.dv_unit, RAW_PART),
		    wdstrategy, &wd->sc_dk.dk_label, &wd->sc_dk.dk_cpulabel);
	}
	if (errstring) {
		printf("%s: %s\n", wd->sc_dev.dv_xname, errstring);
		return;
	}

	if (wd->sc_state > GEOMETRY)
		wd->sc_state = GEOMETRY;
	if ((wd->sc_dk.dk_label.d_flags & D_BADSECT) != 0)
		bad144intern(wd);

	wd->sc_flags |= WDF_BSDLABEL;
}

/*
 * Implement operations other than read/write.
 * Called from wdcstart or wdcintr during opens and formats.
 * Uses finite-state-machine to track progress of operation in progress.
 * Returns 0 if operation still in progress, 1 if completed.
 */
static int
wdcontrol(wd)
	struct wd_softc *wd;
{
	struct wdc_softc *wdc = (void *)wd->sc_dev.dv_parent;
    
	switch (wd->sc_state) {
	case RECAL:			/* Set SDH, step rate, do recal. */
		if (wdcommandshort(wdc, wd->sc_drive, WDCC_RECAL) != 0) {
			wderror(wd, NULL, "wdcontrol: recal failed (1)");
			goto bad;
		}
		wd->sc_state = RECAL_WAIT;
		break;

	case RECAL_WAIT:
		if (wdc->sc_status & WDCS_ERR) {
			wderror(wd, NULL, "wdcontrol: recal failed (2)");
			goto bad;
		}
		/* fall through */
	case GEOMETRY:
		if ((wd->sc_params.wdp_capabilities & WD_CAP_LBA) != 0)
			goto multimode;
		if (wdsetctlr(wd) != 0) {
			/* Already printed a message. */
			goto bad;
		}
		wd->sc_state = GEOMETRY_WAIT;
		break;

	case GEOMETRY_WAIT:
		if (wdc->sc_status & WDCS_ERR) {
			wderror(wd, NULL, "wdcontrol: geometry failed");
			goto bad;
		}
		/* fall through */
	case MULTIMODE:
	multimode:
		if (wd->sc_mode != WDM_PIOMULTI)
			goto open;
		outb(wdc->sc_iobase+wd_seccnt, wd->sc_multiple);
		if (wdcommandshort(wdc, wd->sc_drive, WDCC_SETMULTI) != 0) {
			wderror(wd, NULL, "wdcontrol: setmulti failed (1)");
			goto bad;
		}
		wd->sc_state = MULTIMODE_WAIT;
		break;

	case MULTIMODE_WAIT:
		if (wdc->sc_status & WDCS_ERR) {
			wderror(wd, NULL, "wdcontrol: setmulti failed (2)");
			goto bad;
		}
		/* fall through */
	case OPEN:
	open:
		wdc->sc_errors = 0;
		wd->sc_state = OPEN;
		/*
		 * The rest of the initialization can be done by normal means.
		 */
		return 1;

	bad:
		wdcunwedge(wdc);
		return 0;
	}

	wdc->sc_flags |= WDCF_ACTIVE;
	timeout(wdctimeout, wdc, WAITTIME);
	return 0;
}

/*
 * Send a command and wait uninterruptibly until controller is finished.
 * Return -1 if controller busy for too long, otherwise return non-zero if
 * error.  Intended for brief controller commands at critical points.
 * Assumes interrupts are blocked.
 */
static int
wdcommand(wd, command, cylin, head, sector, count)
	struct wd_softc *wd;
	int command;
	int cylin, head, sector, count;
{
	struct wdc_softc *wdc = (void *)wd->sc_dev.dv_parent;
	int iobase = wdc->sc_iobase;
	int stat;
    
	/* Select drive, head, and addressing mode. */
	outb(iobase+wd_sdh, WDSD_IBM | (wd->sc_drive << 4) | head);

	/* Wait for it to become ready to accept a command. */
	if (command == WDCC_IDP)
		stat = wait_for_unbusy(wdc);
	else
		stat = wdcwait(wdc, WDCS_DRDY);
	if (stat < 0)
		return -1;
    
	/* Load parameters. */
	if (wd->sc_dk.dk_label.d_type == DTYPE_ST506)
		outb(iobase+wd_precomp, wd->sc_dk.dk_label.d_precompcyl / 4);
	else
		outb(iobase+wd_features, 0);
	outb(iobase+wd_cyl_lo, cylin);
	outb(iobase+wd_cyl_hi, cylin >> 8);
	outb(iobase+wd_sector, sector);
	outb(iobase+wd_seccnt, count);

	/* Send command. */
	outb(iobase+wd_command, command);

	return 0;
}

int
wdcommandshort(wdc, drive, command)
	struct wdc_softc *wdc;
	int drive;
	int command;
{
	int iobase = wdc->sc_iobase;

	/* Select drive. */
	outb(iobase+wd_sdh, WDSD_IBM | (drive << 4));

	if (wdcwait(wdc, WDCS_DRDY) < 0)
		return -1;

	outb(iobase+wd_command, command);

	return 0;
}

/*
 * Issue IDP to drive to tell it just what geometry it is to be.
 */
static int
wdsetctlr(wd)
	struct wd_softc *wd;
{
	struct wdc_softc *wdc = (void *)wd->sc_dev.dv_parent;

#ifdef WDDEBUG
	printf("wd(%d,%d) C%dH%dS%d\n", wd->sc_dev.dv_unit, wd->sc_drive,
	    wd->sc_dk.dk_label.d_ncylinders, wd->sc_dk.dk_label.d_ntracks,
	    wd->sc_dk.dk_label.d_nsectors);
#endif
    
	if (wdcommand(wd, WDCC_IDP, wd->sc_dk.dk_label.d_ncylinders,
	    wd->sc_dk.dk_label.d_ntracks - 1, 0, wd->sc_dk.dk_label.d_nsectors)
	    != 0) {
		wderror(wd, NULL, "wdsetctlr: geometry upload failed");
		return -1;
	}

	return 0;
}

/*
 * Issue IDENTIFY to drive to ask it what it is.
 */
int
wd_get_parms(wd)
	struct wd_softc *wd;
{
	struct wdc_softc *wdc = (void *)wd->sc_dev.dv_parent;
	int i;
	char tb[DEV_BSIZE];
    
	if (wdcommandshort(wdc, wd->sc_drive, WDCC_IDENTIFY) != 0 ||
	    wait_for_drq(wdc) != 0) {
		/*
		 * We `know' there's a drive here; just assume it's old.
		 */
		strncpy(wd->sc_dk.dk_label.d_typename, "ST506",
		    sizeof wd->sc_dk.dk_label.d_typename);
		wd->sc_dk.dk_label.d_type = DTYPE_ST506;

		strncpy(wd->sc_params.wdp_model, "unknown",
		    sizeof wd->sc_params.wdp_model);
		wd->sc_params.wdp_config = WD_CFG_FIXED;
		wd->sc_params.wdp_cylinders = 1024;
		wd->sc_params.wdp_heads = 8;
		wd->sc_params.wdp_sectors = 17;
		wd->sc_params.wdp_maxmulti = 0;
		wd->sc_params.wdp_usedmovsd = 0;
		wd->sc_params.wdp_capabilities = 0;
	} else {
		/* Obtain parameters. */
		insw(wdc->sc_iobase+wd_data, tb, sizeof(tb) / sizeof(short));
		bcopy(tb, &wd->sc_params, sizeof(struct wdparams));

		/* Shuffle string byte order. */
		for (i = 0; i < sizeof(wd->sc_params.wdp_model); i += 2) {
			u_short *p;
			p = (u_short *)(wd->sc_params.wdp_model + i);
			*p = ntohs(*p);
		}

		strncpy(wd->sc_dk.dk_label.d_typename, "ESDI/IDE",
		    sizeof wd->sc_dk.dk_label.d_typename);
		wd->sc_dk.dk_label.d_type = DTYPE_ESDI;
	}

#if 0
	printf("gc %x cyl %d trk %d sec %d type %d sz %d model %s\n",
	    wp->wdp_config, wp->wdp_cylinders, wp->wdp_heads, wp->wdp_sectors,
	    wp->wdp_buftype, wp->wdp_bufsize, wp->wdp_model);
#endif
    
	/* XXX sometimes possibly needed */
	(void) inb(wdc->sc_iobase+wd_status);

	return 0;
}

int
wdclose(dev, flag, fmt)
	dev_t dev;
	int flag, fmt;
{
	struct wd_softc *wd = wdcd.cd_devs[WDUNIT(dev)];
	int part = WDPART(dev);
	int s;
    
	switch (fmt) {
	case S_IFCHR:
		wd->sc_dk.dk_copenmask &= ~(1 << part);
		break;
	case S_IFBLK:
		wd->sc_dk.dk_bopenmask &= ~(1 << part);
		break;
	}
	wd->sc_dk.dk_openmask = wd->sc_dk.dk_copenmask | wd->sc_dk.dk_bopenmask;

	if (wd->sc_dk.dk_openmask == 0) {
		wd->sc_flags |= WDF_LOCKED;

#if 0
		s = splbio();
		while (...) {
			wd->sc_flags |= WDF_WAITING;
			if ((error = tsleep(wd, PRIBIO | PCATCH, "wdcls", 0)) != 0)
				return error;
		}
		splx(s);
#endif

		wd->sc_flags &= ~WDF_LOCKED;
		if ((wd->sc_flags & WDF_WANTED) != 0) {
			wd->sc_flags &= WDF_WANTED;
			wakeup(wd);
		}
	}

	return 0;
}

int
wdioctl(dev, command, addr, flag, p)
	dev_t dev;
	u_long command;
	caddr_t addr;
	int flag;
	struct proc *p;
{
	struct wd_softc *wd = wdcd.cd_devs[WDUNIT(dev)];
	int error;
    
	if ((wd->sc_flags & WDF_LOADED) == 0)
		return EIO;

	switch (command) {
	case DIOCSBAD:
		if ((flag & FWRITE) == 0)
			return EBADF;
		wd->sc_dk.dk_cpulabel.bad = *(struct dkbad *)addr;
		wd->sc_dk.dk_label.d_flags |= D_BADSECT;
		bad144intern(wd);
		return 0;

	case DIOCGDINFO:
		*(struct disklabel *)addr = wd->sc_dk.dk_label;
		return 0;
	
	case DIOCGPART:
		((struct partinfo *)addr)->disklab = &wd->sc_dk.dk_label;
		((struct partinfo *)addr)->part =
		    &wd->sc_dk.dk_label.d_partitions[WDPART(dev)];
		return 0;
	
	case DIOCSDINFO:
		if ((flag & FWRITE) == 0)
			return EBADF;
		error = setdisklabel(&wd->sc_dk.dk_label,
		    (struct disklabel *)addr,
		    /*(wd->sc_flags & WDF_BSDLABEL) ? wd->sc_dk.dk_openmask : */0,
		    &wd->sc_dk.dk_cpulabel);
		if (error == 0) {
			wd->sc_flags |= WDF_BSDLABEL;
			if (wd->sc_state > GEOMETRY)
				wd->sc_state = GEOMETRY;
		}
		return error;
	
	case DIOCWLABEL:
		if ((flag & FWRITE) == 0)
			return EBADF;
		if (*(int *)addr)
			wd->sc_flags |= WDF_WLABEL;
		else
			wd->sc_flags &= ~WDF_WLABEL;
		return 0;
	
	case DIOCWDINFO:
		if ((flag & FWRITE) == 0)
			return EBADF;
		error = setdisklabel(&wd->sc_dk.dk_label,
		    (struct disklabel *)addr,
		    /*(wd->sc_flags & WDF_BSDLABEL) ? wd->sc_dk.dk_openmask : */0,
		    &wd->sc_dk.dk_cpulabel);
		if (error == 0) {
			wd->sc_flags |= WDF_BSDLABEL;
			if (wd->sc_state > GEOMETRY)
				wd->sc_state = GEOMETRY;
	    
			/* Simulate opening partition 0 so write succeeds. */
			wd->sc_dk.dk_openmask |= (1 << 0);	/* XXX */
			error = writedisklabel(WDLABELDEV(dev), wdstrategy,
			    &wd->sc_dk.dk_label, &wd->sc_dk.dk_cpulabel);
			wd->sc_dk.dk_openmask =
			    wd->sc_dk.dk_copenmask | wd->sc_dk.dk_bopenmask;
		}
		return error;
	
#ifdef notyet
	case DIOCGDINFOP:
		*(struct disklabel **)addr = &wd->sc_dk.dk_label;
		return 0;
	
	case DIOCWFORMAT:
		if ((flag & FWRITE) == 0)
			return EBADF;
	{
		register struct format_op *fop;
		struct iovec aiov;
		struct uio auio;
	    
		fop = (struct format_op *)addr;
		aiov.iov_base = fop->df_buf;
		aiov.iov_len = fop->df_count;
		auio.uio_iov = &aiov;
		auio.uio_iovcnt = 1;
		auio.uio_resid = fop->df_count;
		auio.uio_segflg = 0;
		auio.uio_offset =
		    fop->df_startblk * wd->sc_dk.dk_label.d_secsize;
		auio.uio_procp = p;
		error = physio(wdformat, NULL, dev, B_WRITE, minphys,
		    &auio);
		fop->df_count -= auio.uio_resid;
		fop->df_reg[0] = wdc->sc_status;
		fop->df_reg[1] = wdc->sc_error;
		return error;
	}
#endif
	
	default:
		return ENOTTY;
	}

#ifdef DIAGNOSTIC
	panic("wdioctl: impossible");
#endif
}

#ifdef B_FORMAT
int
wdformat(struct buf *bp)
{

	bp->b_flags |= B_FORMAT;
	return wdstrategy(bp);
}
#endif

int
wdsize(dev)
	dev_t dev;
{
	struct wd_softc *wd;
	int part;
	int size;
    
	if (wdopen(dev, 0, S_IFBLK) != 0)
		return -1;
	wd = wdcd.cd_devs[WDUNIT(dev)];
	part = WDPART(dev);
	if ((wd->sc_flags & WDF_BSDLABEL) == 0 ||
	    wd->sc_dk.dk_label.d_partitions[part].p_fstype != FS_SWAP)
		size = -1;
	else
		size = wd->sc_dk.dk_label.d_partitions[part].p_size;
	if (wdclose(dev, 0, S_IFBLK) != 0)
		return -1;
	return size;
}

/*
 * Dump core after a system crash.
 */
int
wddump(dev)
	dev_t dev;
{
	struct wd_softc *wd;	/* disk unit to do the IO */
	struct wdc_softc *wdc;
	struct disklabel *lp;
	int unit, part;
	long rblkno, nblks;
	char *addr;
	static wddoingadump = 0;
	extern caddr_t CADDR1;
	extern pt_entry_t *CMAP1;
	
	if (wddoingadump)
		return EFAULT;
	wddoingadump = 1;

	unit = WDUNIT(dev);
	/* Check for acceptable drive number. */
	if (unit >= wdcd.cd_ndevs)
		return ENXIO;
	wd = wdcd.cd_devs[unit];
	/* Was it ever initialized? */
	if (wd == 0 || wd->sc_state < OPEN)
		return ENXIO;

	wdc = (void *)wd->sc_dev.dv_parent;
	addr = (char *)0;	/* starting address */
	lp = &wd->sc_dk.dk_label;
	part = WDPART(dev);

	/* Convert to disk sectors. */
	rblkno = lp->d_partitions[part].p_offset + dumplo;
	nblks = min(ctob(physmem) / lp->d_secsize,
		    lp->d_partitions[part].p_size - dumplo);

	/* Check transfer bounds against partition size. */
	if (dumplo < 0 || nblks <= 0)
		return EINVAL;

	/* Recalibrate. */
	if (wdcommandshort(wdc, wd->sc_drive, WDCC_RECAL) != 0 ||
	    wait_for_ready(wdc) != 0 || wdsetctlr(wd) != 0 ||
	    wait_for_ready(wdc) != 0) {
		wderror(wd, NULL, "wddump: recal failed");
		return EIO;
	}
    
	while (nblks > 0) {
		long blkno;
		long cylin, head, sector;

		blkno = rblkno;

		if ((lp->d_flags & D_BADSECT) != 0) {
			long blkdiff;
			int i;

			for (i = 0; (blkdiff = wd->sc_badsect[i]) != -1; i++) {
				blkdiff -= blkno;
				if (blkdiff < 0)
					continue;
				if (blkdiff == 0) {
					/* Replace current block of transfer. */
					blkno =
					    lp->d_secperunit - lp->d_nsectors - i - 1;
				}
				break;
			}
			/* Tranfer is okay now. */
		}

		if ((wd->sc_params.wdp_capabilities & WD_CAP_LBA) != 0) {
			sector = (blkno >> 0) & 0xff;
			cylin = (blkno >> 8) & 0xffff;
			head = (blkno >> 24) & 0xf;
			head |= WDSD_LBA;
		} else {
			sector = blkno % lp->d_nsectors;
			sector++;	/* Sectors begin with 1, not 0. */
			blkno /= lp->d_nsectors;
			head = blkno % lp->d_ntracks;
			blkno /= lp->d_ntracks;
			cylin = blkno;
			head |= WDSD_CHS;
		}
	
#ifdef notdef
		/* Let's just talk about this first. */
		printf("cylin %d, head %d, sector %d, addr 0x%x", cylin, head,
		    sector, addr);
#endif
		if (wdcommand(wd, WDCC_WRITE, cylin, head, sector, 1) != 0 ||
		    wait_for_drq(wdc) != 0) {
			wderror(wd, NULL, "wddump: write failed");
			return EIO;
		}
	
#ifdef notdef	/* Cannot use this since this address was mapped differently. */
		pmap_enter(kernel_pmap, CADDR1, trunc_page(addr), VM_PROT_READ, TRUE);
#else
		*CMAP1 = PG_V | PG_KW | ctob((long)addr);
		tlbflush();
#endif
	
		outsw(wdc->sc_iobase+wd_data, CADDR1 + ((int)addr & PGOFSET),
		    DEV_BSIZE / sizeof(short));
	
		/* Check data request (should be done). */
		if (wait_for_ready(wdc) != 0) {
			wderror(wd, NULL, "wddump: timeout waiting for ready");
			return EIO;
		}
		if (wdc->sc_status & WDCS_DRQ) {
			wderror(wd, NULL, "wddump: extra drq");
			return EIO;
		}
	
		if ((unsigned)addr % 1048576 == 0)
			printf("%d ", nblks / (1048576 / DEV_BSIZE));

		/* Update block count. */
		nblks--;
		rblkno++;
		(int)addr += DEV_BSIZE;
	}

	return 0;
}

/*
 * Internalize the bad sector table.
 */
void
bad144intern(wd)
	struct wd_softc *wd;
{
	struct dkbad *bt = &wd->sc_dk.dk_cpulabel.bad;
	struct disklabel *lp = &wd->sc_dk.dk_label;
	int i = 0;

	for (; i < 126; i++) {
		if (bt->bt_bad[i].bt_cyl == 0xffff)
			break;
		wd->sc_badsect[i] =
		    bt->bt_bad[i].bt_cyl * lp->d_secpercyl +
		    (bt->bt_bad[i].bt_trksec >> 8) * lp->d_nsectors +
		    (bt->bt_bad[i].bt_trksec & 0xff);
	}
	for (; i < 127; i++)
		wd->sc_badsect[i] = -1;
}

static int
wdcreset(wdc)
	struct wdc_softc *wdc;
{
	int iobase = wdc->sc_iobase;

	/* Reset the device. */
	outb(iobase+wd_ctlr, WDCTL_RST | WDCTL_IDS);
	delay(1000);
	outb(iobase+wd_ctlr, WDCTL_IDS);
	delay(1000);
	(void) inb(iobase+wd_error);
	outb(iobase+wd_ctlr, WDCTL_4BIT);

	if (wait_for_unbusy(wdc) < 0) {
		printf("%s: reset failed\n", wdc->sc_dev.dv_xname);
		return 1;
	}

	return 0;
}

static void
wdcrestart(arg)
	void *arg;
{
	struct wdc_softc *wdc = (struct wdc_softc *)arg;
	int s;

	s = splbio();
	wdcstart(wdc);
	splx(s);
}

/*
 * Unwedge the controller after an unexpected error.  We do this by resetting
 * it, marking all drives for recalibration, and stalling the queue for a short
 * period to give the reset time to finish.
 * NOTE: We use a timeout here, so this routine must not be called during
 * autoconfig or dump.
 */
static void
wdcunwedge(wdc)
	struct wdc_softc *wdc;
{
	int unit;

	untimeout(wdctimeout, wdc);
	(void) wdcreset(wdc);

	/* Schedule recalibrate for all drives on this controller. */
	for (unit = 0; unit < wdcd.cd_ndevs; unit++) {
		struct wd_softc *wd = wdcd.cd_devs[unit];
		if (!wd || (void *)wd->sc_dev.dv_parent != wdc)
			continue;
		if (wd->sc_state > RECAL)
			wd->sc_state = RECAL;
	}

	wdc->sc_flags |= WDCF_ERROR;
	++wdc->sc_errors;

	/* Wake up in a little bit and restart the operation. */
	timeout(wdcrestart, wdc, RECOVERYTIME);
}

int
wdcwait(wdc, mask)
	struct wdc_softc *wdc;
	int mask;
{
	int iobase = wdc->sc_iobase;
	int timeout = 0;
	u_char status;
	extern int cold;

	for (;;) {
		wdc->sc_status = status = inb(iobase+wd_status);
		if ((status & WDCS_BSY) == 0 && (status & mask) == mask)
			break;
		if (++timeout > WDCNDELAY)
			return -1;
		delay(WDCDELAY);
	}
	if (status & WDCS_ERR) {
		wdc->sc_error = inb(iobase+wd_error);
		return WDCS_ERR;
	}
#ifdef WDCNDELAY_DEBUG
	/* After autoconfig, there should be no long delays. */
	if (!cold && timeout > WDCNDELAY_DEBUG)
		printf("%s: warning: busy-wait took %dus\n",
		    wdc->sc_dev.dv_xname, WDCDELAY * timeout);
#endif
	return 0;
}

static void
wdctimeout(arg)
	void *arg;
{
	struct wdc_softc *wdc = (struct wdc_softc *)arg;
	int s;

	s = splbio();
	if ((wdc->sc_flags & WDCF_ACTIVE) != 0) {
		wdc->sc_flags &= ~WDCF_ACTIVE;
		wderror(wdc, NULL, "lost interrupt");
		wdcunwedge(wdc);
	} else
		wderror(wdc, NULL, "missing untimeout");
	splx(s);
}

static void
wderror(dev, bp, msg)
	void *dev;
	struct buf *bp;
	char *msg;
{
	struct wd_softc *wd = dev;
	struct wdc_softc *wdc = dev;

	if (bp) {
		diskerr(bp, "wd", msg, LOG_PRINTF, wd->sc_skip,
		    &wd->sc_dk.dk_label);
		printf("\n");
	} else
		printf("%s: %s: status %b error %b\n", wdc->sc_dev.dv_xname,
		    msg, wdc->sc_status, WDCS_BITS, wdc->sc_error, WDERR_BITS);
}
