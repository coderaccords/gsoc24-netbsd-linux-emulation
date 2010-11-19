/*	$NetBSD: ccd.c,v 1.137 2010/11/19 09:11:45 dholland Exp $	*/

/*-
 * Copyright (c) 1996, 1997, 1998, 1999, 2007, 2009 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe, and by Andrew Doran.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
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

/*
 * Copyright (c) 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * the Systems Programming Group of the University of Utah Computer
 * Science Department.
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
 *
 * from: Utah $Hdr: cd.c 1.6 90/11/28$
 *
 *	@(#)cd.c	8.2 (Berkeley) 11/16/93
 */

/*
 * Copyright (c) 1988 University of Utah.
 *
 * This code is derived from software contributed to Berkeley by
 * the Systems Programming Group of the University of Utah Computer
 * Science Department.
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
 * from: Utah $Hdr: cd.c 1.6 90/11/28$
 *
 *	@(#)cd.c	8.2 (Berkeley) 11/16/93
 */

/*
 * "Concatenated" disk driver.
 *
 * Notes on concurrency:
 *
 * => sc_dvlock serializes access to the device nodes, excluding block I/O.
 *
 * => sc_iolock serializes access to (sc_flags & CCDF_INITED), disk stats,
 *    sc_stop, sc_bufq and b_resid from master buffers.
 *
 * => a combination of CCDF_INITED, sc_inflight, and sc_iolock is used to
 *    serialize I/O and configuration changes.
 *
 * => the in-core disk label does not change while the device is open.
 *
 * On memory consumption: ccd fans out I/O requests and so needs to
 * allocate memory.  If the system is desperately low on memory, we
 * single thread I/O.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: ccd.c,v 1.137 2010/11/19 09:11:45 dholland Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/errno.h>
#include <sys/buf.h>
#include <sys/kmem.h>
#include <sys/pool.h>
#include <sys/namei.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/disklabel.h>
#include <sys/device.h>
#include <sys/disk.h>
#include <sys/syslog.h>
#include <sys/fcntl.h>
#include <sys/vnode.h>
#include <sys/conf.h>
#include <sys/mutex.h>
#include <sys/queue.h>
#include <sys/kauth.h>
#include <sys/kthread.h>
#include <sys/bufq.h>

#include <uvm/uvm_extern.h>

#include <dev/ccdvar.h>
#include <dev/dkvar.h>

#if defined(CCDDEBUG) && !defined(DEBUG)
#define DEBUG
#endif

#ifdef DEBUG
#define CCDB_FOLLOW	0x01
#define CCDB_INIT	0x02
#define CCDB_IO		0x04
#define CCDB_LABEL	0x08
#define CCDB_VNODE	0x10
int ccddebug = 0x00;
#endif

#define	ccdunit(x)	DISKUNIT(x)

struct ccdbuf {
	struct buf	cb_buf;		/* new I/O buf */
	struct buf	*cb_obp;	/* ptr. to original I/O buf */
	struct ccd_softc *cb_sc;	/* pointer to ccd softc */
	int		cb_comp;	/* target component */
	SIMPLEQ_ENTRY(ccdbuf) cb_q;	/* fifo of component buffers */
};

/* component buffer pool */
static pool_cache_t ccd_cache;

#define	CCD_GETBUF()		pool_cache_get(ccd_cache, PR_WAITOK)
#define	CCD_PUTBUF(cbp)		pool_cache_put(ccd_cache, cbp)

#define CCDLABELDEV(dev)	\
	(MAKEDISKDEV(major((dev)), ccdunit((dev)), RAW_PART))

/* called by main() at boot time */
void	ccdattach(int);

/* called by biodone() at interrupt time */
static void	ccdiodone(struct buf *);

static void	ccdinterleave(struct ccd_softc *);
static int	ccdinit(struct ccd_softc *, char **, struct vnode **,
		    struct lwp *);
static struct ccdbuf *ccdbuffer(struct ccd_softc *, struct buf *,
		    daddr_t, void *, long);
static void	ccdgetdefaultlabel(struct ccd_softc *, struct disklabel *);
static void	ccdgetdisklabel(dev_t);
static void	ccdmakedisklabel(struct ccd_softc *);
static void	ccdstart(struct ccd_softc *);
static void	ccdthread(void *);

static dev_type_open(ccdopen);
static dev_type_close(ccdclose);
static dev_type_read(ccdread);
static dev_type_write(ccdwrite);
static dev_type_ioctl(ccdioctl);
static dev_type_strategy(ccdstrategy);
static dev_type_size(ccdsize);

const struct bdevsw ccd_bdevsw = {
	.d_open = ccdopen,
	.d_close = ccdclose,
	.d_strategy = ccdstrategy,
	.d_ioctl = ccdioctl,
	.d_dump = nodump,
	.d_psize = ccdsize,
	.d_flag = D_DISK | D_MPSAFE
};

const struct cdevsw ccd_cdevsw = {
	.d_open = ccdopen,
	.d_close = ccdclose,
	.d_read = ccdread,
	.d_write = ccdwrite,
	.d_ioctl = ccdioctl,
	.d_stop = nostop,
	.d_tty = notty,
	.d_poll = nopoll,
	.d_mmap = nommap,
	.d_kqfilter = nokqfilter,
	.d_flag = D_DISK | D_MPSAFE
};

#ifdef DEBUG
static	void printiinfo(struct ccdiinfo *);
#endif

/* Publically visible for the benefit of libkvm and ccdconfig(8). */
struct ccd_softc 	*ccd_softc;
const int		ccd_softc_elemsize = sizeof(struct ccd_softc);
int			numccd = 0;

/*
 * Called by main() during pseudo-device attachment.  All we need
 * to do is allocate enough space for devices to be configured later.
 */
void
ccdattach(int num)
{
	struct ccd_softc *cs;
	int i;

	if (num <= 0) {
#ifdef DIAGNOSTIC
		panic("ccdattach: count <= 0");
#endif
		return;
	}

	ccd_softc = kmem_zalloc(num * ccd_softc_elemsize, KM_SLEEP);
	if (ccd_softc == NULL) {
		printf("WARNING: no memory for concatenated disks\n");
		return;
	}
	numccd = num;

	/* Initialize the component buffer pool. */
	ccd_cache = pool_cache_init(sizeof(struct ccdbuf), 0,
	    0, 0, "ccdbuf", NULL, IPL_BIO, NULL, NULL, NULL);

	/* Initialize per-softc structures. */
	for (i = 0; i < num; i++) {
		cs = &ccd_softc[i];
		snprintf(cs->sc_xname, sizeof(cs->sc_xname), "ccd%d", i);
		mutex_init(&cs->sc_dvlock, MUTEX_DEFAULT, IPL_NONE);
		cs->sc_iolock = mutex_obj_alloc(MUTEX_DEFAULT, IPL_NONE);
		cv_init(&cs->sc_stop, "ccdstop");
		cv_init(&cs->sc_push, "ccdthr");
		disk_init(&cs->sc_dkdev, cs->sc_xname, NULL); /* XXX */
	}
}

static int
ccdinit(struct ccd_softc *cs, char **cpaths, struct vnode **vpp,
    struct lwp *l)
{
	struct ccdcinfo *ci = NULL;
	size_t size;
	int ix;
	struct vattr va;
	size_t minsize;
	int maxsecsize;
	struct partinfo dpart;
	struct ccdgeom *ccg = &cs->sc_geom;
	char *tmppath;
	int error, path_alloced;

#ifdef DEBUG
	if (ccddebug & (CCDB_FOLLOW|CCDB_INIT))
		printf("%s: ccdinit\n", cs->sc_xname);
#endif

	/* Allocate space for the component info. */
	cs->sc_cinfo = kmem_alloc(cs->sc_nccdisks * sizeof(*cs->sc_cinfo),
	    KM_SLEEP);
	tmppath = kmem_alloc(MAXPATHLEN, KM_SLEEP);

	cs->sc_size = 0;

	/*
	 * Verify that each component piece exists and record
	 * relevant information about it.
	 */
	maxsecsize = 0;
	minsize = 0;
	for (ix = 0, path_alloced = 0; ix < cs->sc_nccdisks; ix++) {
		ci = &cs->sc_cinfo[ix];
		ci->ci_vp = vpp[ix];

		/*
		 * Copy in the pathname of the component.
		 */
		memset(tmppath, 0, sizeof(tmppath));	/* sanity */
		error = copyinstr(cpaths[ix], tmppath,
		    MAXPATHLEN, &ci->ci_pathlen);
		if (ci->ci_pathlen == 0)
			error = EINVAL;
		if (error) {
#ifdef DEBUG
			if (ccddebug & (CCDB_FOLLOW|CCDB_INIT))
				printf("%s: can't copy path, error = %d\n",
				    cs->sc_xname, error);
#endif
			goto out;
		}
		ci->ci_path = kmem_alloc(ci->ci_pathlen, KM_SLEEP);
		memcpy(ci->ci_path, tmppath, ci->ci_pathlen);
		path_alloced++;

		/*
		 * XXX: Cache the component's dev_t.
		 */
		if ((error = VOP_GETATTR(vpp[ix], &va, l->l_cred)) != 0) {
#ifdef DEBUG
			if (ccddebug & (CCDB_FOLLOW|CCDB_INIT))
				printf("%s: %s: getattr failed %s = %d\n",
				    cs->sc_xname, ci->ci_path,
				    "error", error);
#endif
			goto out;
		}
		ci->ci_dev = va.va_rdev;

		/*
		 * Get partition information for the component.
		 */
		error = VOP_IOCTL(vpp[ix], DIOCGPART, &dpart,
		    FREAD, l->l_cred);
		if (error) {
#ifdef DEBUG
			if (ccddebug & (CCDB_FOLLOW|CCDB_INIT))
				 printf("%s: %s: ioctl failed, error = %d\n",
				     cs->sc_xname, ci->ci_path, error);
#endif
			goto out;
		}

/*
 * This diagnostic test is disabled (for now?) since not all port supports
 * on-disk BSD disklabel.
 */
#if 0 /* def DIAGNOSTIC */
		/* Check fstype field of component. */
		if (dpart.part->p_fstype != FS_CCD)
			printf("%s: WARNING: %s: fstype %d != FS_CCD\n",
			    cs->sc_xname, ci->ci_path, dpart.part->p_fstype);
#endif

		/*
		 * Calculate the size, truncating to an interleave
		 * boundary if necessary.
		 */
		maxsecsize =
		    ((dpart.disklab->d_secsize > maxsecsize) ?
		    dpart.disklab->d_secsize : maxsecsize);
		size = dpart.part->p_size;
		if (cs->sc_ileave > 1)
			size -= size % cs->sc_ileave;

		if (size == 0) {
#ifdef DEBUG
			if (ccddebug & (CCDB_FOLLOW|CCDB_INIT))
				printf("%s: %s: size == 0\n",
				    cs->sc_xname, ci->ci_path);
#endif
			error = ENODEV;
			goto out;
		}

		if (minsize == 0 || size < minsize)
			minsize = size;
		ci->ci_size = size;
		cs->sc_size += size;
	}

	/*
	 * Don't allow the interleave to be smaller than
	 * the biggest component sector.
	 */
	if ((cs->sc_ileave > 0) &&
	    (cs->sc_ileave < (maxsecsize / DEV_BSIZE))) {
#ifdef DEBUG
		if (ccddebug & (CCDB_FOLLOW|CCDB_INIT))
			printf("%s: interleave must be at least %d\n",
			    cs->sc_xname, (maxsecsize / DEV_BSIZE));
#endif
		error = EINVAL;
		goto out;
	}

	/*
	 * If uniform interleave is desired set all sizes to that of
	 * the smallest component.
	 */
	if (cs->sc_flags & CCDF_UNIFORM) {
		for (ci = cs->sc_cinfo;
		     ci < &cs->sc_cinfo[cs->sc_nccdisks]; ci++)
			ci->ci_size = minsize;

		cs->sc_size = cs->sc_nccdisks * minsize;
	}

	/*
	 * Construct the interleave table.
	 */
	ccdinterleave(cs);

	/*
	 * Create pseudo-geometry based on 1MB cylinders.  It's
	 * pretty close.
	 */
	ccg->ccg_secsize = DEV_BSIZE;
	ccg->ccg_ntracks = 1;
	ccg->ccg_nsectors = 1024 * (1024 / ccg->ccg_secsize);
	ccg->ccg_ncylinders = cs->sc_size / ccg->ccg_nsectors;

	/*
	 * Create thread to handle deferred I/O.
	 */
	cs->sc_zap = false;
	error = kthread_create(PRI_BIO, KTHREAD_MPSAFE, NULL, ccdthread,
	    cs, &cs->sc_thread, "%s", cs->sc_xname);
	if (error) {
		printf("ccdinit: can't create thread: %d\n", error);
		goto out;
	}

	/*
	 * Only now that everything is set up can we enable the device.
	 */
	mutex_enter(cs->sc_iolock);
	cs->sc_flags |= CCDF_INITED;
	mutex_exit(cs->sc_iolock);
	kmem_free(tmppath, MAXPATHLEN);
	return (0);

 out:
	for (ix = 0; ix < path_alloced; ix++) {
		kmem_free(cs->sc_cinfo[ix].ci_path,
		    cs->sc_cinfo[ix].ci_pathlen);
	}
	kmem_free(cs->sc_cinfo, cs->sc_nccdisks * sizeof(struct ccdcinfo));
	kmem_free(tmppath, MAXPATHLEN);
	return (error);
}

static void
ccdinterleave(struct ccd_softc *cs)
{
	struct ccdcinfo *ci, *smallci;
	struct ccdiinfo *ii;
	daddr_t bn, lbn;
	int ix;
	u_long size;

#ifdef DEBUG
	if (ccddebug & CCDB_INIT)
		printf("ccdinterleave(%p): ileave %d\n", cs, cs->sc_ileave);
#endif
	/*
	 * Allocate an interleave table.
	 * Chances are this is too big, but we don't care.
	 */
	size = (cs->sc_nccdisks + 1) * sizeof(struct ccdiinfo);
	cs->sc_itable = kmem_zalloc(size, KM_SLEEP);

	/*
	 * Trivial case: no interleave (actually interleave of disk size).
	 * Each table entry represents a single component in its entirety.
	 */
	if (cs->sc_ileave == 0) {
		bn = 0;
		ii = cs->sc_itable;

		for (ix = 0; ix < cs->sc_nccdisks; ix++) {
			/* Allocate space for ii_index. */
			ii->ii_indexsz = sizeof(int);
			ii->ii_index = kmem_alloc(ii->ii_indexsz, KM_SLEEP);
			ii->ii_ndisk = 1;
			ii->ii_startblk = bn;
			ii->ii_startoff = 0;
			ii->ii_index[0] = ix;
			bn += cs->sc_cinfo[ix].ci_size;
			ii++;
		}
		ii->ii_ndisk = 0;
#ifdef DEBUG
		if (ccddebug & CCDB_INIT)
			printiinfo(cs->sc_itable);
#endif
		return;
	}

	/*
	 * The following isn't fast or pretty; it doesn't have to be.
	 */
	size = 0;
	bn = lbn = 0;
	for (ii = cs->sc_itable; ; ii++) {
		/* Allocate space for ii_index. */
		ii->ii_indexsz = sizeof(int) * cs->sc_nccdisks;
		ii->ii_index = kmem_alloc(ii->ii_indexsz, KM_SLEEP);

		/*
		 * Locate the smallest of the remaining components
		 */
		smallci = NULL;
		for (ci = cs->sc_cinfo;
		     ci < &cs->sc_cinfo[cs->sc_nccdisks]; ci++)
			if (ci->ci_size > size &&
			    (smallci == NULL ||
			     ci->ci_size < smallci->ci_size))
				smallci = ci;

		/*
		 * Nobody left, all done
		 */
		if (smallci == NULL) {
			ii->ii_ndisk = 0;
			break;
		}

		/*
		 * Record starting logical block and component offset
		 */
		ii->ii_startblk = bn / cs->sc_ileave;
		ii->ii_startoff = lbn;

		/*
		 * Determine how many disks take part in this interleave
		 * and record their indices.
		 */
		ix = 0;
		for (ci = cs->sc_cinfo;
		     ci < &cs->sc_cinfo[cs->sc_nccdisks]; ci++)
			if (ci->ci_size >= smallci->ci_size)
				ii->ii_index[ix++] = ci - cs->sc_cinfo;
		ii->ii_ndisk = ix;
		bn += ix * (smallci->ci_size - size);
		lbn = smallci->ci_size / cs->sc_ileave;
		size = smallci->ci_size;
	}
#ifdef DEBUG
	if (ccddebug & CCDB_INIT)
		printiinfo(cs->sc_itable);
#endif
}

/* ARGSUSED */
static int
ccdopen(dev_t dev, int flags, int fmt, struct lwp *l)
{
	int unit = ccdunit(dev);
	struct ccd_softc *cs;
	struct disklabel *lp;
	int error = 0, part, pmask;

#ifdef DEBUG
	if (ccddebug & CCDB_FOLLOW)
		printf("ccdopen(0x%"PRIx64", 0x%x)\n", dev, flags);
#endif
	if (unit >= numccd)
		return (ENXIO);
	cs = &ccd_softc[unit];

	mutex_enter(&cs->sc_dvlock);

	lp = cs->sc_dkdev.dk_label;

	part = DISKPART(dev);
	pmask = (1 << part);

	/*
	 * If we're initialized, check to see if there are any other
	 * open partitions.  If not, then it's safe to update
	 * the in-core disklabel.  Only read the disklabel if it is
	 * not already valid.
	 */
	if ((cs->sc_flags & (CCDF_INITED|CCDF_VLABEL)) == CCDF_INITED &&
	    cs->sc_dkdev.dk_openmask == 0)
		ccdgetdisklabel(dev);

	/* Check that the partition exists. */
	if (part != RAW_PART) {
		if (((cs->sc_flags & CCDF_INITED) == 0) ||
		    ((part >= lp->d_npartitions) ||
		     (lp->d_partitions[part].p_fstype == FS_UNUSED))) {
			error = ENXIO;
			goto done;
		}
	}

	/* Prevent our unit from being unconfigured while open. */
	switch (fmt) {
	case S_IFCHR:
		cs->sc_dkdev.dk_copenmask |= pmask;
		break;

	case S_IFBLK:
		cs->sc_dkdev.dk_bopenmask |= pmask;
		break;
	}
	cs->sc_dkdev.dk_openmask =
	    cs->sc_dkdev.dk_copenmask | cs->sc_dkdev.dk_bopenmask;

 done:
	mutex_exit(&cs->sc_dvlock);
	return (error);
}

/* ARGSUSED */
static int
ccdclose(dev_t dev, int flags, int fmt, struct lwp *l)
{
	int unit = ccdunit(dev);
	struct ccd_softc *cs;
	int part;

#ifdef DEBUG
	if (ccddebug & CCDB_FOLLOW)
		printf("ccdclose(0x%"PRIx64", 0x%x)\n", dev, flags);
#endif

	if (unit >= numccd)
		return (ENXIO);
	cs = &ccd_softc[unit];

	mutex_enter(&cs->sc_dvlock);

	part = DISKPART(dev);

	/* ...that much closer to allowing unconfiguration... */
	switch (fmt) {
	case S_IFCHR:
		cs->sc_dkdev.dk_copenmask &= ~(1 << part);
		break;

	case S_IFBLK:
		cs->sc_dkdev.dk_bopenmask &= ~(1 << part);
		break;
	}
	cs->sc_dkdev.dk_openmask =
	    cs->sc_dkdev.dk_copenmask | cs->sc_dkdev.dk_bopenmask;

	if (cs->sc_dkdev.dk_openmask == 0) {
		if ((cs->sc_flags & CCDF_KLABEL) == 0)
			cs->sc_flags &= ~CCDF_VLABEL;
	}

	mutex_exit(&cs->sc_dvlock);
	return (0);
}

static bool
ccdbackoff(struct ccd_softc *cs)
{

	/* XXX Arbitrary, should be a uvm call. */
	return uvmexp.free < (uvmexp.freemin >> 1) &&
	    disk_isbusy(&cs->sc_dkdev);
}

static void
ccdthread(void *cookie)
{
	struct ccd_softc *cs;

	cs = cookie;

#ifdef DEBUG
 	if (ccddebug & CCDB_FOLLOW)
 		printf("ccdthread: hello\n");
#endif

	mutex_enter(cs->sc_iolock);
	while (__predict_true(!cs->sc_zap)) {
		if (bufq_peek(cs->sc_bufq) == NULL) {
			/* Nothing to do. */
			cv_wait(&cs->sc_push, cs->sc_iolock);
			continue;
		}
		if (ccdbackoff(cs)) {
			/* Wait for memory to become available. */
			(void)cv_timedwait(&cs->sc_push, cs->sc_iolock, 1);
			continue;
		}
#ifdef DEBUG
 		if (ccddebug & CCDB_FOLLOW)
 			printf("ccdthread: dispatching I/O\n");
#endif
		ccdstart(cs);
		mutex_enter(cs->sc_iolock);
	}
	cs->sc_thread = NULL;
	mutex_exit(cs->sc_iolock);
#ifdef DEBUG
 	if (ccddebug & CCDB_FOLLOW)
 		printf("ccdthread: goodbye\n");
#endif
	kthread_exit(0);
}

static void
ccdstrategy(struct buf *bp)
{
	int unit = ccdunit(bp->b_dev);
	struct ccd_softc *cs = &ccd_softc[unit];

	/* Must be open or reading label. */
	KASSERT(cs->sc_dkdev.dk_openmask != 0 ||
	    (cs->sc_flags & CCDF_RLABEL) != 0);

	mutex_enter(cs->sc_iolock);
	/* Synchronize with device init/uninit. */
	if (__predict_false((cs->sc_flags & CCDF_INITED) == 0)) {
		mutex_exit(cs->sc_iolock);
#ifdef DEBUG
 		if (ccddebug & CCDB_FOLLOW)
 			printf("ccdstrategy: unit %d: not inited\n", unit);
#endif
 		bp->b_error = ENXIO;
 		bp->b_resid = bp->b_bcount;
 		biodone(bp);
		return;
	}

	/* Defer to thread if system is low on memory. */
	bufq_put(cs->sc_bufq, bp);
	if (__predict_false(ccdbackoff(cs))) {
		mutex_exit(cs->sc_iolock);
#ifdef DEBUG
 		if (ccddebug & CCDB_FOLLOW)
 			printf("ccdstrategy: holding off on I/O\n");
#endif
		return;
	}
	ccdstart(cs);
}

static void
ccdstart(struct ccd_softc *cs)
{
	daddr_t blkno;
	int wlabel;
	struct disklabel *lp;
	long bcount, rcount;
	struct ccdbuf *cbp;
	char *addr;
	daddr_t bn;
	vnode_t *vp;
	buf_t *bp;

	KASSERT(mutex_owned(cs->sc_iolock));

	disk_busy(&cs->sc_dkdev);
	bp = bufq_get(cs->sc_bufq);
	KASSERT(bp != NULL);

#ifdef DEBUG
	if (ccddebug & CCDB_FOLLOW)
		printf("ccdstart(%s, %p)\n", cs->sc_xname, bp);
#endif

	/* If it's a nil transfer, wake up the top half now. */
	if (bp->b_bcount == 0)
		goto done;

	lp = cs->sc_dkdev.dk_label;

	/*
	 * Do bounds checking and adjust transfer.  If there's an
	 * error, the bounds check will flag that for us.  Convert
	 * the partition relative block number to an absolute.
	 */
	blkno = bp->b_blkno;
	wlabel = cs->sc_flags & (CCDF_WLABEL|CCDF_LABELLING);
	if (DISKPART(bp->b_dev) != RAW_PART) {
		if (bounds_check_with_label(&cs->sc_dkdev, bp, wlabel) <= 0)
			goto done;
		blkno += lp->d_partitions[DISKPART(bp->b_dev)].p_offset;
	}
	mutex_exit(cs->sc_iolock);
	bp->b_rawblkno = blkno;

	/* Allocate the component buffers and start I/O! */
	bp->b_resid = bp->b_bcount;
	bn = bp->b_rawblkno;
	addr = bp->b_data;
	for (bcount = bp->b_bcount; bcount > 0; bcount -= rcount) {
		cbp = ccdbuffer(cs, bp, bn, addr, bcount);
		rcount = cbp->cb_buf.b_bcount;
		bn += btodb(rcount);
		addr += rcount;
		vp = cbp->cb_buf.b_vp;
		if ((cbp->cb_buf.b_flags & B_READ) == 0) {
			mutex_enter(&vp->v_interlock);
			vp->v_numoutput++;
			mutex_exit(&vp->v_interlock);
		}
		(void)VOP_STRATEGY(vp, &cbp->cb_buf);
	}
	return;

 done:
	disk_unbusy(&cs->sc_dkdev, 0, 0);
	cv_broadcast(&cs->sc_stop);
	cv_broadcast(&cs->sc_push);
	mutex_exit(cs->sc_iolock);
	bp->b_resid = bp->b_bcount;
	biodone(bp);
}

/*
 * Build a component buffer header.
 */
static struct ccdbuf *
ccdbuffer(struct ccd_softc *cs, struct buf *bp, daddr_t bn, void *addr,
    long bcount)
{
	struct ccdcinfo *ci;
	struct ccdbuf *cbp;
	daddr_t cbn, cboff;
	u_int64_t cbc;
	int ccdisk;

#ifdef DEBUG
	if (ccddebug & CCDB_IO)
		printf("ccdbuffer(%p, %p, %" PRId64 ", %p, %ld)\n",
		       cs, bp, bn, addr, bcount);
#endif
	/*
	 * Determine which component bn falls in.
	 */
	cbn = bn;
	cboff = 0;

	/*
	 * Serially concatenated
	 */
	if (cs->sc_ileave == 0) {
		daddr_t sblk;

		sblk = 0;
		for (ccdisk = 0, ci = &cs->sc_cinfo[ccdisk];
		    cbn >= sblk + ci->ci_size;
		    ccdisk++, ci = &cs->sc_cinfo[ccdisk])
			sblk += ci->ci_size;
		cbn -= sblk;
	}
	/*
	 * Interleaved
	 */
	else {
		struct ccdiinfo *ii;
		int off;

		cboff = cbn % cs->sc_ileave;
		cbn /= cs->sc_ileave;
		for (ii = cs->sc_itable; ii->ii_ndisk; ii++)
			if (ii->ii_startblk > cbn)
				break;
		ii--;
		off = cbn - ii->ii_startblk;
		if (ii->ii_ndisk == 1) {
			ccdisk = ii->ii_index[0];
			cbn = ii->ii_startoff + off;
		} else {
			ccdisk = ii->ii_index[off % ii->ii_ndisk];
			cbn = ii->ii_startoff + off / ii->ii_ndisk;
		}
		cbn *= cs->sc_ileave;
		ci = &cs->sc_cinfo[ccdisk];
	}

	/*
	 * Fill in the component buf structure.
	 */
	cbp = CCD_GETBUF();
	KASSERT(cbp != NULL);
	buf_init(&cbp->cb_buf);
	cbp->cb_buf.b_flags = bp->b_flags;
	cbp->cb_buf.b_oflags = bp->b_oflags;
	cbp->cb_buf.b_cflags = bp->b_cflags;
	cbp->cb_buf.b_iodone = ccdiodone;
	cbp->cb_buf.b_proc = bp->b_proc;
	cbp->cb_buf.b_dev = ci->ci_dev;
	cbp->cb_buf.b_blkno = cbn + cboff;
	cbp->cb_buf.b_data = addr;
	cbp->cb_buf.b_vp = ci->ci_vp;
	cbp->cb_buf.b_objlock = &ci->ci_vp->v_interlock;
	if (cs->sc_ileave == 0)
		cbc = dbtob((u_int64_t)(ci->ci_size - cbn));
	else
		cbc = dbtob((u_int64_t)(cs->sc_ileave - cboff));
	cbp->cb_buf.b_bcount = cbc < bcount ? cbc : bcount;

	/*
	 * context for ccdiodone
	 */
	cbp->cb_obp = bp;
	cbp->cb_sc = cs;
	cbp->cb_comp = ccdisk;

	BIO_COPYPRIO(&cbp->cb_buf, bp);

#ifdef DEBUG
	if (ccddebug & CCDB_IO)
		printf(" dev 0x%"PRIx64"(u%lu): cbp %p bn %" PRId64 " addr %p"
		       " bcnt %d\n",
		    ci->ci_dev, (unsigned long) (ci-cs->sc_cinfo), cbp,
		    cbp->cb_buf.b_blkno, cbp->cb_buf.b_data,
		    cbp->cb_buf.b_bcount);
#endif

	return (cbp);
}

/*
 * Called at interrupt time.
 * Mark the component as done and if all components are done,
 * take a ccd interrupt.
 */
static void
ccdiodone(struct buf *vbp)
{
	struct ccdbuf *cbp = (struct ccdbuf *) vbp;
	struct buf *bp = cbp->cb_obp;
	struct ccd_softc *cs = cbp->cb_sc;
	int count;

#ifdef DEBUG
	if (ccddebug & CCDB_FOLLOW)
		printf("ccdiodone(%p)\n", cbp);
	if (ccddebug & CCDB_IO) {
		printf("ccdiodone: bp %p bcount %d resid %d\n",
		       bp, bp->b_bcount, bp->b_resid);
		printf(" dev 0x%"PRIx64"(u%d), cbp %p bn %" PRId64 " addr %p"
		       " bcnt %d\n",
		       cbp->cb_buf.b_dev, cbp->cb_comp, cbp,
		       cbp->cb_buf.b_blkno, cbp->cb_buf.b_data,
		       cbp->cb_buf.b_bcount);
	}
#endif

	if (cbp->cb_buf.b_error != 0) {
		bp->b_error = cbp->cb_buf.b_error;
		printf("%s: error %d on component %d\n",
		       cs->sc_xname, bp->b_error, cbp->cb_comp);
	}
	count = cbp->cb_buf.b_bcount;
	buf_destroy(&cbp->cb_buf);
	CCD_PUTBUF(cbp);

	/*
	 * If all done, "interrupt".
	 */
	mutex_enter(cs->sc_iolock);
	bp->b_resid -= count;
	if (bp->b_resid < 0)
		panic("ccdiodone: count");
	if (bp->b_resid == 0) {
		/*
		 * Request is done for better or worse, wakeup the top half.
		 */
		if (bp->b_error != 0)
			bp->b_resid = bp->b_bcount;
		disk_unbusy(&cs->sc_dkdev, (bp->b_bcount - bp->b_resid),
		    (bp->b_flags & B_READ));
		if (!disk_isbusy(&cs->sc_dkdev)) {
			if (bufq_peek(cs->sc_bufq) != NULL) {
				cv_broadcast(&cs->sc_push);
			}
			cv_broadcast(&cs->sc_stop);
		}
		mutex_exit(cs->sc_iolock);
		biodone(bp);
	} else
		mutex_exit(cs->sc_iolock);
}

/* ARGSUSED */
static int
ccdread(dev_t dev, struct uio *uio, int flags)
{
	int unit = ccdunit(dev);
	struct ccd_softc *cs;

#ifdef DEBUG
	if (ccddebug & CCDB_FOLLOW)
		printf("ccdread(0x%"PRIx64", %p)\n", dev, uio);
#endif
	if (unit >= numccd)
		return (ENXIO);
	cs = &ccd_softc[unit];

	/* Unlocked advisory check, ccdstrategy check is synchronous. */
	if ((cs->sc_flags & CCDF_INITED) == 0)
		return (ENXIO);

	return (physio(ccdstrategy, NULL, dev, B_READ, minphys, uio));
}

/* ARGSUSED */
static int
ccdwrite(dev_t dev, struct uio *uio, int flags)
{
	int unit = ccdunit(dev);
	struct ccd_softc *cs;

#ifdef DEBUG
	if (ccddebug & CCDB_FOLLOW)
		printf("ccdwrite(0x%"PRIx64", %p)\n", dev, uio);
#endif
	if (unit >= numccd)
		return (ENXIO);
	cs = &ccd_softc[unit];

	/* Unlocked advisory check, ccdstrategy check is synchronous. */
	if ((cs->sc_flags & CCDF_INITED) == 0)
		return (ENXIO);

	return (physio(ccdstrategy, NULL, dev, B_WRITE, minphys, uio));
}

static int
ccdioctl(dev_t dev, u_long cmd, void *data, int flag, struct lwp *l)
{
	int unit = ccdunit(dev);
	int i, j, lookedup = 0, error = 0;
	int part, pmask;
	struct ccd_softc *cs;
	struct ccd_ioctl *ccio = (struct ccd_ioctl *)data;
	kauth_cred_t uc;
	char **cpp;
	struct pathbuf *pb;
	struct vnode **vpp;
#ifdef __HAVE_OLD_DISKLABEL
	struct disklabel newlabel;
#endif

	if (unit >= numccd)
		return (ENXIO);
	cs = &ccd_softc[unit];
	uc = kauth_cred_get();

	/* Must be open for writes for these commands... */
	switch (cmd) {
	case CCDIOCSET:
	case CCDIOCCLR:
	case DIOCSDINFO:
	case DIOCWDINFO:
#ifdef __HAVE_OLD_DISKLABEL
	case ODIOCSDINFO:
	case ODIOCWDINFO:
#endif
	case DIOCKLABEL:
	case DIOCWLABEL:
		if ((flag & FWRITE) == 0)
			return (EBADF);
	}

	mutex_enter(&cs->sc_dvlock);

	/* Must be initialized for these... */
	switch (cmd) {
	case CCDIOCCLR:
	case DIOCGDINFO:
	case DIOCCACHESYNC:
	case DIOCSDINFO:
	case DIOCWDINFO:
	case DIOCGPART:
	case DIOCWLABEL:
	case DIOCKLABEL:
	case DIOCGDEFLABEL:
#ifdef __HAVE_OLD_DISKLABEL
	case ODIOCGDINFO:
	case ODIOCSDINFO:
	case ODIOCWDINFO:
	case ODIOCGDEFLABEL:
#endif
		if ((cs->sc_flags & CCDF_INITED) == 0) {
			error = ENXIO;
			goto out;
		}
	}

	switch (cmd) {
	case CCDIOCSET:
		if (cs->sc_flags & CCDF_INITED) {
			error = EBUSY;
			goto out;
		}

		/* Validate the flags. */
		if ((ccio->ccio_flags & CCDF_USERMASK) != ccio->ccio_flags) {
			error = EINVAL;
			goto out;
		}

		if (ccio->ccio_ndisks > CCD_MAXNDISKS ||
		    ccio->ccio_ndisks == 0) {
			error = EINVAL;
			goto out;
		}

		/* Fill in some important bits. */
		cs->sc_ileave = ccio->ccio_ileave;
		cs->sc_nccdisks = ccio->ccio_ndisks;
		cs->sc_flags = ccio->ccio_flags & CCDF_USERMASK;

		/*
		 * Allocate space for and copy in the array of
		 * componet pathnames and device numbers.
		 */
		cpp = kmem_alloc(ccio->ccio_ndisks * sizeof(*cpp), KM_SLEEP);
		vpp = kmem_alloc(ccio->ccio_ndisks * sizeof(*vpp), KM_SLEEP);
		error = copyin(ccio->ccio_disks, cpp,
		    ccio->ccio_ndisks * sizeof(*cpp));
		if (error) {
			kmem_free(vpp, ccio->ccio_ndisks * sizeof(*vpp));
			kmem_free(cpp, ccio->ccio_ndisks * sizeof(*cpp));
			goto out;
		}

#ifdef DEBUG
		if (ccddebug & CCDB_INIT)
			for (i = 0; i < ccio->ccio_ndisks; ++i)
				printf("ccdioctl: component %d: %p\n",
				    i, cpp[i]);
#endif

		for (i = 0; i < ccio->ccio_ndisks; ++i) {
#ifdef DEBUG
			if (ccddebug & CCDB_INIT)
				printf("ccdioctl: lookedup = %d\n", lookedup);
#endif
			error = pathbuf_copyin(cpp[i], &pb);
			if (error == 0) {
				error = dk_lookup(pb, l, &vpp[i]);
			}
			pathbuf_destroy(pb);
			if (error != 0) {
				for (j = 0; j < lookedup; ++j)
					(void)vn_close(vpp[j], FREAD|FWRITE,
					    uc);
				kmem_free(vpp, ccio->ccio_ndisks *
				    sizeof(*vpp));
				kmem_free(cpp, ccio->ccio_ndisks *
				    sizeof(*cpp));
				goto out;
			}
			++lookedup;
		}

		/* Attach the disk. */
		disk_attach(&cs->sc_dkdev);
		bufq_alloc(&cs->sc_bufq, "fcfs", 0);

		/*
		 * Initialize the ccd.  Fills in the softc for us.
		 */
		if ((error = ccdinit(cs, cpp, vpp, l)) != 0) {
			for (j = 0; j < lookedup; ++j)
				(void)vn_close(vpp[j], FREAD|FWRITE,
				    uc);
			kmem_free(vpp, ccio->ccio_ndisks * sizeof(*vpp));
			kmem_free(cpp, ccio->ccio_ndisks * sizeof(*cpp));
			disk_detach(&cs->sc_dkdev);
			bufq_free(cs->sc_bufq);
			goto out;
		}

		/* We can free the temporary variables now. */
		kmem_free(vpp, ccio->ccio_ndisks * sizeof(*vpp));
		kmem_free(cpp, ccio->ccio_ndisks * sizeof(*cpp));

		/*
		 * The ccd has been successfully initialized, so
		 * we can place it into the array.  Don't try to
		 * read the disklabel until the disk has been attached,
		 * because space for the disklabel is allocated
		 * in disk_attach();
		 */
		ccio->ccio_unit = unit;
		ccio->ccio_size = cs->sc_size;

		/* Try and read the disklabel. */
		ccdgetdisklabel(dev);
		break;

	case CCDIOCCLR:
		/*
		 * Don't unconfigure if any other partitions are open
		 * or if both the character and block flavors of this
		 * partition are open.
		 */
		part = DISKPART(dev);
		pmask = (1 << part);
		if ((cs->sc_dkdev.dk_openmask & ~pmask) ||
		    ((cs->sc_dkdev.dk_bopenmask & pmask) &&
		    (cs->sc_dkdev.dk_copenmask & pmask))) {
			error = EBUSY;
			goto out;
		}

		/* Stop new I/O, wait for in-flight I/O to complete. */
		mutex_enter(cs->sc_iolock);
		cs->sc_flags &= ~(CCDF_INITED|CCDF_VLABEL);
		cs->sc_zap = true;
		while (disk_isbusy(&cs->sc_dkdev) ||
		    bufq_peek(cs->sc_bufq) != NULL ||
		    cs->sc_thread != NULL) {
			cv_broadcast(&cs->sc_push);
			(void)cv_timedwait(&cs->sc_stop, cs->sc_iolock, hz);
		}
		mutex_exit(cs->sc_iolock);

		/*
		 * Free ccd_softc information and clear entry.
		 */

		/* Close the components and free their pathnames. */
		for (i = 0; i < cs->sc_nccdisks; ++i) {
			/*
			 * XXX: this close could potentially fail and
			 * cause Bad Things.  Maybe we need to force
			 * the close to happen?
			 */
#ifdef DEBUG
			if (ccddebug & CCDB_VNODE)
				vprint("CCDIOCCLR: vnode info",
				    cs->sc_cinfo[i].ci_vp);
#endif
			(void)vn_close(cs->sc_cinfo[i].ci_vp, FREAD|FWRITE,
			    uc);
			kmem_free(cs->sc_cinfo[i].ci_path,
			    cs->sc_cinfo[i].ci_pathlen);
		}

		/* Free interleave index. */
		for (i = 0; cs->sc_itable[i].ii_ndisk; ++i) {
			kmem_free(cs->sc_itable[i].ii_index,
			    cs->sc_itable[i].ii_indexsz);
		}

		/* Free component info and interleave table. */
		kmem_free(cs->sc_cinfo, cs->sc_nccdisks *
		    sizeof(struct ccdcinfo));
		kmem_free(cs->sc_itable, (cs->sc_nccdisks + 1) *
		    sizeof(struct ccdiinfo));

		/* Detatch the disk. */
		disk_detach(&cs->sc_dkdev);
		bufq_free(cs->sc_bufq);
		break;

	case DIOCGDINFO:
		*(struct disklabel *)data = *(cs->sc_dkdev.dk_label);
		break;

#ifdef __HAVE_OLD_DISKLABEL
	case ODIOCGDINFO:
		newlabel = *(cs->sc_dkdev.dk_label);
		if (newlabel.d_npartitions > OLDMAXPARTITIONS)
			return ENOTTY;
		memcpy(data, &newlabel, sizeof (struct olddisklabel));
		break;
#endif

	case DIOCGPART:
		((struct partinfo *)data)->disklab = cs->sc_dkdev.dk_label;
		((struct partinfo *)data)->part =
		    &cs->sc_dkdev.dk_label->d_partitions[DISKPART(dev)];
		break;

	case DIOCCACHESYNC:
		/*
		 * XXX Do we really need to care about having a writable
		 * file descriptor here?
		 */
		if ((flag & FWRITE) == 0)
			return (EBADF);

		/*
		 * We pass this call down to all components and report
		 * the first error we encounter.
		 */
		for (error = 0, i = 0; i < cs->sc_nccdisks; i++) {
			j = VOP_IOCTL(cs->sc_cinfo[i].ci_vp, cmd, data,
				      flag, uc);
			if (j != 0 && error == 0)
				error = j;
		}
		break;

	case DIOCWDINFO:
	case DIOCSDINFO:
#ifdef __HAVE_OLD_DISKLABEL
	case ODIOCWDINFO:
	case ODIOCSDINFO:
#endif
	{
		struct disklabel *lp;
#ifdef __HAVE_OLD_DISKLABEL
		if (cmd == ODIOCSDINFO || cmd == ODIOCWDINFO) {
			memset(&newlabel, 0, sizeof newlabel);
			memcpy(&newlabel, data, sizeof (struct olddisklabel));
			lp = &newlabel;
		} else
#endif
		lp = (struct disklabel *)data;

		cs->sc_flags |= CCDF_LABELLING;

		error = setdisklabel(cs->sc_dkdev.dk_label,
		    lp, 0, cs->sc_dkdev.dk_cpulabel);
		if (error == 0) {
			if (cmd == DIOCWDINFO
#ifdef __HAVE_OLD_DISKLABEL
			    || cmd == ODIOCWDINFO
#endif
			   )
				error = writedisklabel(CCDLABELDEV(dev),
				    ccdstrategy, cs->sc_dkdev.dk_label,
				    cs->sc_dkdev.dk_cpulabel);
		}

		cs->sc_flags &= ~CCDF_LABELLING;
		break;
	}

	case DIOCKLABEL:
		if (*(int *)data != 0)
			cs->sc_flags |= CCDF_KLABEL;
		else
			cs->sc_flags &= ~CCDF_KLABEL;
		break;

	case DIOCWLABEL:
		if (*(int *)data != 0)
			cs->sc_flags |= CCDF_WLABEL;
		else
			cs->sc_flags &= ~CCDF_WLABEL;
		break;

	case DIOCGDEFLABEL:
		ccdgetdefaultlabel(cs, (struct disklabel *)data);
		break;

#ifdef __HAVE_OLD_DISKLABEL
	case ODIOCGDEFLABEL:
		ccdgetdefaultlabel(cs, &newlabel);
		if (newlabel.d_npartitions > OLDMAXPARTITIONS)
			return ENOTTY;
		memcpy(data, &newlabel, sizeof (struct olddisklabel));
		break;
#endif

	default:
		error = ENOTTY;
	}

 out:
	mutex_exit(&cs->sc_dvlock);
	return (error);
}

static int
ccdsize(dev_t dev)
{
	struct ccd_softc *cs;
	struct disklabel *lp;
	int part, unit, omask, size;

	unit = ccdunit(dev);
	if (unit >= numccd)
		return (-1);
	cs = &ccd_softc[unit];

	if ((cs->sc_flags & CCDF_INITED) == 0)
		return (-1);

	part = DISKPART(dev);
	omask = cs->sc_dkdev.dk_openmask & (1 << part);
	lp = cs->sc_dkdev.dk_label;

	if (omask == 0 && ccdopen(dev, 0, S_IFBLK, curlwp))
		return (-1);

	if (lp->d_partitions[part].p_fstype != FS_SWAP)
		size = -1;
	else
		size = lp->d_partitions[part].p_size *
		    (lp->d_secsize / DEV_BSIZE);

	if (omask == 0 && ccdclose(dev, 0, S_IFBLK, curlwp))
		return (-1);

	return (size);
}

static void
ccdgetdefaultlabel(struct ccd_softc *cs, struct disklabel *lp)
{
	struct ccdgeom *ccg = &cs->sc_geom;

	memset(lp, 0, sizeof(*lp));

	lp->d_secperunit = cs->sc_size;
	lp->d_secsize = ccg->ccg_secsize;
	lp->d_nsectors = ccg->ccg_nsectors;
	lp->d_ntracks = ccg->ccg_ntracks;
	lp->d_ncylinders = ccg->ccg_ncylinders;
	lp->d_secpercyl = lp->d_ntracks * lp->d_nsectors;

	strncpy(lp->d_typename, "ccd", sizeof(lp->d_typename));
	lp->d_type = DTYPE_CCD;
	strncpy(lp->d_packname, "fictitious", sizeof(lp->d_packname));
	lp->d_rpm = 3600;
	lp->d_interleave = 1;
	lp->d_flags = 0;

	lp->d_partitions[RAW_PART].p_offset = 0;
	lp->d_partitions[RAW_PART].p_size = cs->sc_size;
	lp->d_partitions[RAW_PART].p_fstype = FS_UNUSED;
	lp->d_npartitions = RAW_PART + 1;

	lp->d_magic = DISKMAGIC;
	lp->d_magic2 = DISKMAGIC;
	lp->d_checksum = dkcksum(cs->sc_dkdev.dk_label);
}

/*
 * Read the disklabel from the ccd.  If one is not present, fake one
 * up.
 */
static void
ccdgetdisklabel(dev_t dev)
{
	int unit = ccdunit(dev);
	struct ccd_softc *cs = &ccd_softc[unit];
	const char *errstring;
	struct disklabel *lp = cs->sc_dkdev.dk_label;
	struct cpu_disklabel *clp = cs->sc_dkdev.dk_cpulabel;

	KASSERT(mutex_owned(&cs->sc_dvlock));

	memset(clp, 0, sizeof(*clp));

	ccdgetdefaultlabel(cs, lp);

	/*
	 * Call the generic disklabel extraction routine.
	 */
	cs->sc_flags |= CCDF_RLABEL;
	if ((cs->sc_flags & CCDF_NOLABEL) != 0)
		errstring = "CCDF_NOLABEL set; ignoring on-disk label";
	else
		errstring = readdisklabel(CCDLABELDEV(dev), ccdstrategy,
		    cs->sc_dkdev.dk_label, cs->sc_dkdev.dk_cpulabel);
	if (errstring)
		ccdmakedisklabel(cs);
	else {
		int i;
		struct partition *pp;

		/*
		 * Sanity check whether the found disklabel is valid.
		 *
		 * This is necessary since total size of ccd may vary
		 * when an interleave is changed even though exactly
		 * same componets are used, and old disklabel may used
		 * if that is found.
		 */
		if (lp->d_secperunit != cs->sc_size)
			printf("WARNING: %s: "
			    "total sector size in disklabel (%d) != "
			    "the size of ccd (%lu)\n", cs->sc_xname,
			    lp->d_secperunit, (u_long)cs->sc_size);
		for (i = 0; i < lp->d_npartitions; i++) {
			pp = &lp->d_partitions[i];
			if (pp->p_offset + pp->p_size > cs->sc_size)
				printf("WARNING: %s: end of partition `%c' "
				    "exceeds the size of ccd (%lu)\n",
				    cs->sc_xname, 'a' + i, (u_long)cs->sc_size);
		}
	}

#ifdef DEBUG
	/* It's actually extremely common to have unlabeled ccds. */
	if (ccddebug & CCDB_LABEL)
		if (errstring != NULL)
			printf("%s: %s\n", cs->sc_xname, errstring);
#endif

	/* In-core label now valid. */
	cs->sc_flags = (cs->sc_flags | CCDF_VLABEL) & ~CCDF_RLABEL;
}

/*
 * Take care of things one might want to take care of in the event
 * that a disklabel isn't present.
 */
static void
ccdmakedisklabel(struct ccd_softc *cs)
{
	struct disklabel *lp = cs->sc_dkdev.dk_label;

	/*
	 * For historical reasons, if there's no disklabel present
	 * the raw partition must be marked FS_BSDFFS.
	 */
	lp->d_partitions[RAW_PART].p_fstype = FS_BSDFFS;

	strncpy(lp->d_packname, "default label", sizeof(lp->d_packname));

	lp->d_checksum = dkcksum(lp);
}

#ifdef DEBUG
static void
printiinfo(struct ccdiinfo *ii)
{
	int ix, i;

	for (ix = 0; ii->ii_ndisk; ix++, ii++) {
		printf(" itab[%d]: #dk %d sblk %" PRId64 " soff %" PRId64,
		    ix, ii->ii_ndisk, ii->ii_startblk, ii->ii_startoff);
		for (i = 0; i < ii->ii_ndisk; i++)
			printf(" %d", ii->ii_index[i]);
		printf("\n");
	}
}
#endif

#ifdef _MODULE

#include <sys/module.h>

MODULE(MODULE_CLASS_DRIVER, ccd, NULL);

static int
ccd_modcmd(modcmd_t cmd, void *arg)
{
	int bmajor = -1, cmajor = -1,  error = 0;
	
	switch (cmd) {
	case MODULE_CMD_INIT:
		ccdattach(4);
		
		return devsw_attach("ccd", &ccd_bdevsw, &bmajor,
		    &ccd_cdevsw, &cmajor);
		break;

	case MODULE_CMD_FINI:
		return devsw_detach(&ccd_bdevsw, &ccd_cdevsw);
		break;

	case MODULE_CMD_STAT:
		return ENOTTY;

	default:
		return ENOTTY;
	}

	return error;
}

#endif
