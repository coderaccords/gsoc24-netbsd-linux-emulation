/*	$NetBSD: genfs_vnops.c,v 1.38 2001/09/21 07:52:25 chs Exp $	*/

/*
 * Copyright (c) 1982, 1986, 1989, 1993
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
 *
 */

#include "opt_nfsserver.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/kernel.h>
#include <sys/mount.h>
#include <sys/namei.h>
#include <sys/vnode.h>
#include <sys/fcntl.h>
#include <sys/malloc.h>
#include <sys/poll.h>
#include <sys/mman.h>

#include <miscfs/genfs/genfs.h>
#include <miscfs/genfs/genfs_node.h>
#include <miscfs/specfs/specdev.h>

#include <uvm/uvm.h>
#include <uvm/uvm_pager.h>

#ifdef NFSSERVER
#include <nfs/rpcv2.h>
#include <nfs/nfsproto.h>
#include <nfs/nfs.h>
#include <nfs/nqnfs.h>
#include <nfs/nfs_var.h>
#endif

int
genfs_poll(v)
	void *v;
{
	struct vop_poll_args /* {
		struct vnode *a_vp;
		int a_events;
		struct proc *a_p;
	} */ *ap = v;

	return (ap->a_events & (POLLIN | POLLOUT | POLLRDNORM | POLLWRNORM));
}

int
genfs_fsync(v)
	void *v;
{
	struct vop_fsync_args /* {
		struct vnode *a_vp;
		struct ucred *a_cred;
		int a_flags;
		off_t offlo;
		off_t offhi;
		struct proc *a_p;
	} */ *ap = v;
	struct vnode *vp = ap->a_vp;
	int wait;

	wait = (ap->a_flags & FSYNC_WAIT) != 0;
	vflushbuf(vp, wait);
	if ((ap->a_flags & FSYNC_DATAONLY) != 0)
		return (0);
	else
		return (VOP_UPDATE(vp, NULL, NULL, wait ? UPDATE_WAIT : 0));
}

int
genfs_seek(v)
	void *v;
{
	struct vop_seek_args /* {
		struct vnode *a_vp;
		off_t a_oldoff;
		off_t a_newoff;
		struct ucred *a_ucred;
	} */ *ap = v;

	if (ap->a_newoff < 0)
		return (EINVAL);

	return (0);
}

int
genfs_abortop(v)
	void *v;
{
	struct vop_abortop_args /* {
		struct vnode *a_dvp;
		struct componentname *a_cnp;
	} */ *ap = v;
 
	if ((ap->a_cnp->cn_flags & (HASBUF | SAVESTART)) == HASBUF)
		PNBUF_PUT(ap->a_cnp->cn_pnbuf);
	return (0);
}

int
genfs_fcntl(v)
	void *v;
{
	struct vop_fcntl_args /* {
		struct vnode *a_vp;
		u_int a_command;
		caddr_t a_data;
		int a_fflag;
		struct ucred *a_cred;
		struct proc *a_p;
	} */ *ap = v;

	if (ap->a_command == F_SETFL)
		return (0);
	else
		return (EOPNOTSUPP);
}

/*ARGSUSED*/
int
genfs_badop(v)
	void *v;
{

	panic("genfs: bad op");
}

/*ARGSUSED*/
int
genfs_nullop(v)
	void *v;
{

	return (0);
}

/*ARGSUSED*/
int
genfs_einval(v)
	void *v;
{

	return (EINVAL);
}

/*ARGSUSED*/
int
genfs_eopnotsupp(v)
	void *v;
{

	return (EOPNOTSUPP);
}

/*
 * Called when an fs doesn't support a particular vop but the vop needs to
 * vrele, vput, or vunlock passed in vnodes.
 */
int
genfs_eopnotsupp_rele(v)
	void *v;
{
	struct vop_generic_args /*
		struct vnodeop_desc *a_desc;
		/ * other random data follows, presumably * / 
	} */ *ap = v;
	struct vnodeop_desc *desc = ap->a_desc;
	struct vnode *vp;
	int flags, i, j, offset;

	flags = desc->vdesc_flags;
	for (i = 0; i < VDESC_MAX_VPS; flags >>=1, i++) {
		if ((offset = desc->vdesc_vp_offsets[i]) == VDESC_NO_OFFSET)
			break;	/* stop at end of list */
		if ((j = flags & VDESC_VP0_WILLPUT)) {
			vp = *VOPARG_OFFSETTO(struct vnode**,offset,ap);
			switch (j) {
			case VDESC_VP0_WILLPUT:
				vput(vp);
				break;
			case VDESC_VP0_WILLUNLOCK:
				VOP_UNLOCK(vp, 0);
				break;
			case VDESC_VP0_WILLRELE:
				vrele(vp);
				break;
			}
		}
	}

	return (EOPNOTSUPP);
}

/*ARGSUSED*/
int
genfs_ebadf(v)
	void *v;
{

	return (EBADF);
}

/* ARGSUSED */
int
genfs_enoioctl(v)
	void *v;
{

	return (ENOTTY);
}


/*
 * Eliminate all activity associated with the requested vnode
 * and with all vnodes aliased to the requested vnode.
 */
int
genfs_revoke(v)
	void *v;
{
	struct vop_revoke_args /* {
		struct vnode *a_vp;
		int a_flags;
	} */ *ap = v;
	struct vnode *vp, *vq;
	struct proc *p = curproc;	/* XXX */

#ifdef DIAGNOSTIC
	if ((ap->a_flags & REVOKEALL) == 0)
		panic("genfs_revoke: not revokeall");
#endif

	vp = ap->a_vp;
	simple_lock(&vp->v_interlock);

	if (vp->v_flag & VALIASED) {
		/*
		 * If a vgone (or vclean) is already in progress,
		 * wait until it is done and return.
		 */
		if (vp->v_flag & VXLOCK) {
			vp->v_flag |= VXWANT;
			simple_unlock(&vp->v_interlock);
			tsleep((caddr_t)vp, PINOD, "vop_revokeall", 0);
			return (0);
		}
		/*
		 * Ensure that vp will not be vgone'd while we
		 * are eliminating its aliases.
		 */
		vp->v_flag |= VXLOCK;
		simple_unlock(&vp->v_interlock);
		while (vp->v_flag & VALIASED) {
			simple_lock(&spechash_slock);
			for (vq = *vp->v_hashchain; vq; vq = vq->v_specnext) {
				if (vq->v_rdev != vp->v_rdev ||
				    vq->v_type != vp->v_type || vp == vq)
					continue;
				simple_unlock(&spechash_slock);
				vgone(vq);
				break;
			}
			if (vq == NULLVP)
				simple_unlock(&spechash_slock);
		}
		/*
		 * Remove the lock so that vgone below will
		 * really eliminate the vnode after which time
		 * vgone will awaken any sleepers.
		 */
		simple_lock(&vp->v_interlock);
		vp->v_flag &= ~VXLOCK;
	}
	vgonel(vp, p);
	return (0);
}

/*
 * Lock the node.
 */
int
genfs_lock(v)
	void *v;
{
	struct vop_lock_args /* {
		struct vnode *a_vp;
		int a_flags;
	} */ *ap = v;
	struct vnode *vp = ap->a_vp;

	return (lockmgr(&vp->v_lock, ap->a_flags, &vp->v_interlock));
}

/*
 * Unlock the node.
 */
int
genfs_unlock(v)
	void *v;
{
	struct vop_unlock_args /* {
		struct vnode *a_vp;
		int a_flags;
	} */ *ap = v;
	struct vnode *vp = ap->a_vp;

	return (lockmgr(&vp->v_lock, ap->a_flags | LK_RELEASE,
		&vp->v_interlock));
}

/*
 * Return whether or not the node is locked.
 */
int
genfs_islocked(v)
	void *v;
{
	struct vop_islocked_args /* {
		struct vnode *a_vp;
	} */ *ap = v;
	struct vnode *vp = ap->a_vp;

	return (lockstatus(&vp->v_lock));
}

/*
 * Stubs to use when there is no locking to be done on the underlying object.
 */
int
genfs_nolock(v)
	void *v;
{
	struct vop_lock_args /* {
		struct vnode *a_vp;
		int a_flags;
		struct proc *a_p;
	} */ *ap = v;

	/*
	 * Since we are not using the lock manager, we must clear
	 * the interlock here.
	 */
	if (ap->a_flags & LK_INTERLOCK)
		simple_unlock(&ap->a_vp->v_interlock);
	return (0);
}

int
genfs_nounlock(v)
	void *v;
{
	return (0);
}

int
genfs_noislocked(v)
	void *v;
{
	return (0);
}

/*
 * Local lease check for NFS servers.  Just set up args and let
 * nqsrv_getlease() do the rest.  If NFSSERVER is not in the kernel,
 * this is a null operation.
 */
int
genfs_lease_check(v)
	void *v;
{
#ifdef NFSSERVER
	struct vop_lease_args /* {
		struct vnode *a_vp;
		struct proc *a_p;
		struct ucred *a_cred;
		int a_flag;
	} */ *ap = v;
	u_int32_t duration = 0;
	int cache;
	u_quad_t frev;

	(void) nqsrv_getlease(ap->a_vp, &duration, ND_CHECK | ap->a_flag,
	    NQLOCALSLP, ap->a_p, (struct mbuf *)0, &cache, &frev, ap->a_cred);
	return (0);
#else
	return (0);
#endif /* NFSSERVER */
}

int
genfs_mmap(v)
	void *v;
{
	return 0;
}

/*
 * generic VM getpages routine.
 * Return PG_BUSY pages for the given range,
 * reading from backing store if necessary.
 */

int
genfs_getpages(v)
	void *v;
{
	struct vop_getpages_args /* {
		struct vnode *a_vp;
		voff_t a_offset;
		struct vm_page **a_m;
		int *a_count;
		int a_centeridx;
		vm_prot_t a_access_type;
		int a_advice;
		int a_flags;
	} */ *ap = v;

	off_t newsize, diskeof, memeof;
	off_t offset, origoffset, startoffset, endoffset, raoffset;
	daddr_t lbn, blkno;
	int s, i, error, npages, orignpages, npgs, run, ridx, pidx, pcount;
	int fs_bshift, fs_bsize, dev_bshift;
	int flags = ap->a_flags;
	size_t bytes, iobytes, tailbytes, totalbytes, skipbytes;
	vaddr_t kva;
	struct buf *bp, *mbp;
	struct vnode *vp = ap->a_vp;
	struct vnode *devvp;
	struct genfs_node *gp = VTOG(vp);
	struct uvm_object *uobj = &vp->v_uobj;
	struct vm_page *pg, *pgs[16];			/* XXXUBC 16 */
	struct ucred *cred = curproc->p_ucred;		/* XXXUBC curproc */
	boolean_t async = (flags & PGO_SYNCIO) == 0;
	boolean_t write = (ap->a_access_type & VM_PROT_WRITE) != 0;
	boolean_t sawhole = FALSE;
	boolean_t overwrite = (flags & PGO_OVERWRITE) != 0;
	UVMHIST_FUNC("genfs_getpages"); UVMHIST_CALLED(ubchist);

	UVMHIST_LOG(ubchist, "vp %p off 0x%x/%x count %d",
		    vp, ap->a_offset >> 32, ap->a_offset, *ap->a_count);

	/* XXXUBC temp limit */
	if (*ap->a_count > 16) {
		panic("genfs_getpages: too many pages");
	}

	error = 0;
	origoffset = ap->a_offset;
	orignpages = *ap->a_count;
	GOP_SIZE(vp, vp->v_size, &diskeof);
	if (flags & PGO_PASTEOF) {
		newsize = MAX(vp->v_size,
			      origoffset + (orignpages << PAGE_SHIFT));
		GOP_SIZE(vp, newsize, &memeof);
	} else {
		memeof = diskeof;
	}
	KASSERT(ap->a_centeridx >= 0 || ap->a_centeridx <= orignpages);
	KASSERT((origoffset & (PAGE_SIZE - 1)) == 0 && origoffset >= 0);
	KASSERT(orignpages > 0);

	/*
	 * Bounds-check the request.
	 */

	if (origoffset + (ap->a_centeridx << PAGE_SHIFT) >= memeof) {
		if ((flags & PGO_LOCKED) == 0) {
			simple_unlock(&uobj->vmobjlock);
		}
		UVMHIST_LOG(ubchist, "off 0x%x count %d goes past EOF 0x%x",
			    origoffset, *ap->a_count, memeof,0);
		return EINVAL;
	}

	/*
	 * For PGO_LOCKED requests, just return whatever's in memory.
	 */

	if (flags & PGO_LOCKED) {
		uvn_findpages(uobj, origoffset, ap->a_count, ap->a_m,
			      UFP_NOWAIT|UFP_NOALLOC|UFP_NORDONLY);

		return ap->a_m[ap->a_centeridx] == NULL ? EBUSY : 0;
	}

	/* vnode is VOP_LOCKed, uobj is locked */

	if (write && (vp->v_flag & VONWORKLST) == 0) {
		vn_syncer_add_to_worklist(vp, filedelay);
	}

	/*
	 * find the requested pages and make some simple checks.
	 * leave space in the page array for a whole block.
	 */

	if (vp->v_type == VREG) {
		fs_bshift = vp->v_mount->mnt_fs_bshift;
		dev_bshift = vp->v_mount->mnt_dev_bshift;
	} else {
		fs_bshift = DEV_BSHIFT;
		dev_bshift = DEV_BSHIFT;
	}
	fs_bsize = 1 << fs_bshift;

	orignpages = MIN(orignpages,
	    round_page(memeof - origoffset) >> PAGE_SHIFT);
	npages = orignpages;
	startoffset = origoffset & ~(fs_bsize - 1);
	endoffset = round_page((origoffset + (npages << PAGE_SHIFT)
				+ fs_bsize - 1) & ~(fs_bsize - 1));
	endoffset = MIN(endoffset, round_page(memeof));
	ridx = (origoffset - startoffset) >> PAGE_SHIFT;

	memset(pgs, 0, sizeof(pgs));
	uvn_findpages(uobj, origoffset, &npages, &pgs[ridx], UFP_ALL);

	/*
	 * if the pages are already resident, just return them.
	 */

	for (i = 0; i < npages; i++) {
		struct vm_page *pg = pgs[ridx + i];

		if ((pg->flags & PG_FAKE) ||
		    (write && (pg->flags & PG_RDONLY))) {
			break;
		}
	}
	if (i == npages) {
		UVMHIST_LOG(ubchist, "returning cached pages", 0,0,0,0);
		raoffset = origoffset + (orignpages << PAGE_SHIFT);
		npages += ridx;
		goto raout;
	}

	/*
	 * if PGO_OVERWRITE is set, don't bother reading the pages.
	 */

	if (flags & PGO_OVERWRITE) {
		UVMHIST_LOG(ubchist, "PGO_OVERWRITE",0,0,0,0);

		for (i = 0; i < npages; i++) {
			struct vm_page *pg = pgs[ridx + i];

			pg->flags &= ~(PG_RDONLY|PG_CLEAN);
		}
		npages += ridx;
		goto out;
	}

	/*
	 * the page wasn't resident and we're not overwriting,
	 * so we're going to have to do some i/o.
	 * find any additional pages needed to cover the expanded range.
	 */

	npages = (endoffset - startoffset) >> PAGE_SHIFT;
	if (startoffset != origoffset || npages != orignpages) {

		/*
		 * we need to avoid deadlocks caused by locking
		 * additional pages at lower offsets than pages we
		 * already have locked.  unlock them all and start over.
		 */

		for (i = 0; i < orignpages; i++) {
			struct vm_page *pg = pgs[ridx + i];

			if (pg->flags & PG_FAKE) {
				pg->flags |= PG_RELEASED;
			}
		}
		uvm_page_unbusy(&pgs[ridx], orignpages);
		memset(pgs, 0, sizeof(pgs));

		UVMHIST_LOG(ubchist, "reset npages start 0x%x end 0x%x",
			    startoffset, endoffset, 0,0);
		npgs = npages;
		uvn_findpages(uobj, startoffset, &npgs, pgs, UFP_ALL);
	}
	simple_unlock(&uobj->vmobjlock);

	/*
	 * read the desired page(s).
	 */

	totalbytes = npages << PAGE_SHIFT;
	bytes = MIN(totalbytes, MAX(diskeof - startoffset, 0));
	tailbytes = totalbytes - bytes;
	skipbytes = 0;

	kva = uvm_pagermapin(pgs, npages, UVMPAGER_MAPIN_WAITOK |
			     UVMPAGER_MAPIN_READ);

	s = splbio();
	mbp = pool_get(&bufpool, PR_WAITOK);
	splx(s);
	mbp->b_bufsize = totalbytes;
	mbp->b_data = (void *)kva;
	mbp->b_resid = mbp->b_bcount = bytes;
	mbp->b_flags = B_BUSY|B_READ| (async ? B_CALL : 0);
	mbp->b_iodone = (async ? uvm_aio_biodone : 0);
	mbp->b_vp = vp;
	LIST_INIT(&mbp->b_dep);

	/*
	 * if EOF is in the middle of the range, zero the part past EOF.
	 * if the page including EOF is not PG_FAKE, skip over it since
	 * in that case it has valid data that we need to preserve.
	 */

	if (tailbytes > 0) {
		size_t tailstart = bytes;

		if ((pgs[bytes >> PAGE_SHIFT]->flags & PG_FAKE) == 0) {
			tailstart = round_page(tailstart);
			tailbytes -= tailstart - bytes;
		}
		UVMHIST_LOG(ubchist, "tailbytes %p 0x%x 0x%x",
			    kva, tailstart, tailbytes,0);
		memset((void *)(kva + tailstart), 0, tailbytes);
	}

	/*
	 * now loop over the pages, reading as needed.
	 */

	if (write) {
		lockmgr(&gp->g_glock, LK_EXCLUSIVE, NULL);
	} else {
		lockmgr(&gp->g_glock, LK_SHARED, NULL);
	}

	bp = NULL;
	for (offset = startoffset;
	     bytes > 0;
	     offset += iobytes, bytes -= iobytes) {

		/*
		 * skip pages which don't need to be read.
		 */

		pidx = (offset - startoffset) >> PAGE_SHIFT;
		while ((pgs[pidx]->flags & (PG_FAKE|PG_RDONLY)) == 0) {
			size_t b;

			KASSERT((offset & (PAGE_SIZE - 1)) == 0);
			b = MIN(PAGE_SIZE, bytes);
			offset += b;
			bytes -= b;
			skipbytes += b;
			pidx++;
			UVMHIST_LOG(ubchist, "skipping, new offset 0x%x",
				    offset, 0,0,0);
			if (bytes == 0) {
				goto loopdone;
			}
		}

		/*
		 * bmap the file to find out the blkno to read from and
		 * how much we can read in one i/o.  if bmap returns an error,
		 * skip the rest of the top-level i/o.
		 */

		lbn = offset >> fs_bshift;
		error = VOP_BMAP(vp, lbn, &devvp, &blkno, &run);
		if (error) {
			UVMHIST_LOG(ubchist, "VOP_BMAP lbn 0x%x -> %d\n",
				    lbn, error,0,0);
			skipbytes += bytes;
			goto loopdone;
		}

		/*
		 * see how many pages can be read with this i/o.
		 * reduce the i/o size if necessary to avoid
		 * overwriting pages with valid data.
		 */

		iobytes = MIN((((off_t)lbn + 1 + run) << fs_bshift) - offset,
		    bytes);
		if (offset + iobytes > round_page(offset)) {
			pcount = 1;
			while (pidx + pcount < npages &&
			       pgs[pidx + pcount]->flags & PG_FAKE) {
				pcount++;
			}
			iobytes = MIN(iobytes, (pcount << PAGE_SHIFT) -
				      (offset - trunc_page(offset)));
		}

		/*
		 * if this block isn't allocated, zero it instead of reading it.
		 * if this is a read access, mark the pages we zeroed PG_RDONLY.
		 */

		if (blkno < 0) {
			int holepages = (round_page(offset + iobytes) - 
					 trunc_page(offset)) >> PAGE_SHIFT;
			UVMHIST_LOG(ubchist, "lbn 0x%x -> HOLE", lbn,0,0,0);

			sawhole = TRUE;
			memset((char *)kva + (offset - startoffset), 0,
			       iobytes);
			skipbytes += iobytes;

			for (i = 0; i < holepages; i++) {
				if (write) {
					pgs[pidx + i]->flags &= ~PG_CLEAN;
				} else {
					pgs[pidx + i]->flags |= PG_RDONLY;
				}
			}
			continue;
		}

		/*
		 * allocate a sub-buf for this piece of the i/o
		 * (or just use mbp if there's only 1 piece),
		 * and start it going.
		 */

		if (offset == startoffset && iobytes == bytes) {
			bp = mbp;
		} else {
			s = splbio();
			bp = pool_get(&bufpool, PR_WAITOK);
			splx(s);
			bp->b_data = (char *)kva + offset - startoffset;
			bp->b_resid = bp->b_bcount = iobytes;
			bp->b_flags = B_BUSY|B_READ|B_CALL;
			bp->b_iodone = uvm_aio_biodone1;
			bp->b_vp = vp;
			bp->b_proc = NULL;
			LIST_INIT(&bp->b_dep);
		}
		bp->b_lblkno = 0;
		bp->b_private = mbp;
		if (devvp->v_type == VBLK) {
			bp->b_dev = devvp->v_rdev;
		}

		/* adjust physical blkno for partial blocks */
		bp->b_blkno = blkno + ((offset - ((off_t)lbn << fs_bshift)) >>
				       dev_bshift);

		UVMHIST_LOG(ubchist, "bp %p offset 0x%x bcount 0x%x blkno 0x%x",
			    bp, offset, iobytes, bp->b_blkno);

		VOP_STRATEGY(bp);
	}

loopdone:
	if (skipbytes) {
		s = splbio();
		if (error) {
			mbp->b_flags |= B_ERROR;
			mbp->b_error = error;
		}
		mbp->b_resid -= skipbytes;
		if (mbp->b_resid == 0) {
			biodone(mbp);
		}
		splx(s);
	}

	if (async) {
		UVMHIST_LOG(ubchist, "returning 0 (async)",0,0,0,0);
		lockmgr(&gp->g_glock, LK_RELEASE, NULL);
		return 0;
	}
	if (bp != NULL) {
		error = biowait(mbp);
	}
	s = splbio();
	pool_put(&bufpool, mbp);
	splx(s);
	uvm_pagermapout(kva, npages);
	raoffset = startoffset + totalbytes;

	/*
	 * if this we encountered a hole then we have to do a little more work.
	 * for read faults, we marked the page PG_RDONLY so that future
	 * write accesses to the page will fault again.
	 * for write faults, we must make sure that the backing store for
	 * the page is completely allocated while the pages are locked.
	 */

	if (!error && sawhole && write) {
		for (i = 0; i < npages; i++) {
			if (pgs[i] == NULL) {
				continue;
			}
			pgs[i]->flags &= ~PG_CLEAN;
			UVMHIST_LOG(ubchist, "mark dirty pg %p", pgs[i],0,0,0);
		}
		error = GOP_ALLOC(vp, startoffset, npages << PAGE_SHIFT, 0,
				  cred);
		UVMHIST_LOG(ubchist, "gop_alloc off 0x%x/0x%x -> %d",
		    startoffset, npages << PAGE_SHIFT, error,0);
	}
	lockmgr(&gp->g_glock, LK_RELEASE, NULL);
	simple_lock(&uobj->vmobjlock);

	/*
	 * see if we want to start any readahead.
	 * XXXUBC for now, just read the next 128k on 64k boundaries.
	 * this is pretty nonsensical, but it is 50% faster than reading
	 * just the next 64k.
	 */

raout:
	if (!error && !async && !write && ((int)raoffset & 0xffff) == 0 &&
	    PAGE_SHIFT <= 16) {
		int racount;

		racount = 1 << (16 - PAGE_SHIFT);
		(void) VOP_GETPAGES(vp, raoffset, NULL, &racount, 0,
				    VM_PROT_READ, 0, 0);
		simple_lock(&uobj->vmobjlock);

		racount = 1 << (16 - PAGE_SHIFT);
		(void) VOP_GETPAGES(vp, raoffset + 0x10000, NULL, &racount, 0,
				    VM_PROT_READ, 0, 0);
		simple_lock(&uobj->vmobjlock);
	}

	/*
	 * we're almost done!  release the pages...
	 * for errors, we free the pages.
	 * otherwise we activate them and mark them as valid and clean.
	 * also, unbusy pages that were not actually requested.
	 */

	if (error) {
		for (i = 0; i < npages; i++) {
			if (pgs[i] == NULL) {
				continue;
			}
			UVMHIST_LOG(ubchist, "examining pg %p flags 0x%x",
				    pgs[i], pgs[i]->flags, 0,0);
			if (pgs[i]->flags & PG_FAKE) {
				pgs[i]->flags |= PG_RELEASED;
			}
		}
		uvm_lock_pageq();
		uvm_page_unbusy(pgs, npages);
		uvm_unlock_pageq();
		simple_unlock(&uobj->vmobjlock);
		UVMHIST_LOG(ubchist, "returning error %d", error,0,0,0);
		return error;
	}

out:
	UVMHIST_LOG(ubchist, "succeeding, npages %d", npages,0,0,0);
	uvm_lock_pageq();
	for (i = 0; i < npages; i++) {
		pg = pgs[i];
		if (pg == NULL) {
			continue;
		}
		UVMHIST_LOG(ubchist, "examining pg %p flags 0x%x",
			    pg, pg->flags, 0,0);
		if (pg->flags & PG_FAKE && !overwrite) {
			pg->flags &= ~(PG_FAKE);
			pmap_clear_modify(pgs[i]);
		}
		if (write) {
			pg->flags &= ~(PG_RDONLY);
		}
		if (i < ridx || i >= ridx + orignpages || async) {
			UVMHIST_LOG(ubchist, "unbusy pg %p offset 0x%x",
				    pg, pg->offset,0,0);
			if (pg->flags & PG_WANTED) {
				wakeup(pg);
			}
			if (pg->flags & PG_FAKE) {
				KASSERT(overwrite);
				uvm_pagezero(pg);
			}
			if (pg->flags & PG_RELEASED) {
				uvm_pagefree(pg);
				continue;
			}
			uvm_pageactivate(pg);
			pg->flags &= ~(PG_WANTED|PG_BUSY|PG_FAKE);
			UVM_PAGE_OWN(pg, NULL);
		}
	}
	uvm_unlock_pageq();
	simple_unlock(&uobj->vmobjlock);
	if (ap->a_m != NULL) {
		memcpy(ap->a_m, &pgs[ridx],
		       orignpages * sizeof(struct vm_page *));
	}
	return 0;
}

/*
 * generic VM putpages routine.
 * Write the given range of pages to backing store.
 *
 * => "offhi == 0" means flush all pages at or after "offlo".
 * => object should be locked by caller.   we may _unlock_ the object
 *	if (and only if) we need to clean a page (PGO_CLEANIT), or
 *	if PGO_SYNCIO is set and there are pages busy.
 *	we return with the object locked.
 * => if PGO_CLEANIT or PGO_SYNCIO is set, we may block (due to I/O).
 *	thus, a caller might want to unlock higher level resources
 *	(e.g. vm_map) before calling flush.
 * => if neither PGO_CLEANIT nor PGO_SYNCIO is set, then we will neither
 *	unlock the object nor block.
 * => if PGO_ALLPAGES is set, then all pages in the object will be processed.
 * => NOTE: we rely on the fact that the object's memq is a TAILQ and
 *	that new pages are inserted on the tail end of the list.   thus,
 *	we can make a complete pass through the object in one go by starting
 *	at the head and working towards the tail (new pages are put in
 *	front of us).
 * => NOTE: we are allowed to lock the page queues, so the caller
 *	must not be holding the page queue lock.
 *
 * note on "cleaning" object and PG_BUSY pages:
 *	this routine is holding the lock on the object.   the only time
 *	that it can run into a PG_BUSY page that it does not own is if
 *	some other process has started I/O on the page (e.g. either
 *	a pagein, or a pageout).    if the PG_BUSY page is being paged
 *	in, then it can not be dirty (!PG_CLEAN) because no one has
 *	had a chance to modify it yet.    if the PG_BUSY page is being
 *	paged out then it means that someone else has already started
 *	cleaning the page for us (how nice!).    in this case, if we 
 *	have syncio specified, then after we make our pass through the
 *	object we need to wait for the other PG_BUSY pages to clear 
 *	off (i.e. we need to do an iosync).   also note that once a
 *	page is PG_BUSY it must stay in its object until it is un-busyed.
 *
 * note on page traversal:
 *	we can traverse the pages in an object either by going down the
 *	linked list in "uobj->memq", or we can go over the address range
 *	by page doing hash table lookups for each address.    depending
 *	on how many pages are in the object it may be cheaper to do one 
 *	or the other.   we set "by_list" to true if we are using memq.
 *	if the cost of a hash lookup was equal to the cost of the list
 *	traversal we could compare the number of pages in the start->stop
 *	range to the total number of pages in the object.   however, it
 *	seems that a hash table lookup is more expensive than the linked
 *	list traversal, so we multiply the number of pages in the 
 *	range by an estimate of the relatively higher cost of the hash lookup.
 */

int
genfs_putpages(v)
	void *v;
{
	struct vop_putpages_args /* {
		struct vnode *a_vp;
		voff_t a_offlo;
		voff_t a_offhi;
		int a_flags;
	} */ *ap = v;
	struct vnode *vp = ap->a_vp;
	struct uvm_object *uobj = &vp->v_uobj;
	off_t startoff = ap->a_offlo;
	off_t endoff = ap->a_offhi;
	off_t off;
	int flags = ap->a_flags;
	int n = MAXBSIZE >> PAGE_SHIFT;
	int i, s, error, npages, nback;
	int freeflag;
	struct vm_page *pgs[n], *pg, *nextpg, *tpg, curmp, endmp;
	boolean_t wasclean, by_list, needs_clean;
	boolean_t async = (flags & PGO_SYNCIO) == 0;
	UVMHIST_FUNC("genfs_putpages"); UVMHIST_CALLED(ubchist);

	KASSERT(flags & (PGO_CLEANIT|PGO_FREE|PGO_DEACTIVATE));
	KASSERT((startoff & PAGE_MASK) == 0 && (endoff & PAGE_MASK) == 0);
	KASSERT(startoff < endoff || endoff == 0);

	UVMHIST_LOG(ubchist, "vp %p pages %d off 0x%x len 0x%x",
	    vp, uobj->uo_npages, startoff, endoff - startoff);
	if (uobj->uo_npages == 0) {
		if (LIST_FIRST(&vp->v_dirtyblkhd) == NULL &&
		    (vp->v_flag & VONWORKLST)) {
			vp->v_flag &= ~VONWORKLST;
			LIST_REMOVE(vp, v_synclist);
		}
		simple_unlock(&uobj->vmobjlock);
		return 0;
	}

	/*
	 * the vnode has pages, set up to process the request.
	 */

	error = 0;
	wasclean = TRUE;
	off = startoff;
	if (endoff == 0 || flags & PGO_ALLPAGES) {
		endoff = trunc_page(LLONG_MAX);
	}
	by_list = (uobj->uo_npages <=
	    ((endoff - startoff) >> PAGE_SHIFT) * UVM_PAGE_HASH_PENALTY);

	/*
	 * start the loop.  when scanning by list, hold the last page
	 * in the list before we start.  pages allocated after we start
	 * will be added to the end of the list, so we can stop at the
	 * current last page.
	 */

	freeflag = (curproc == uvm.pagedaemon_proc) ? PG_PAGEOUT : PG_RELEASED;
	curmp.uobject = uobj;
	curmp.offset = (voff_t)-1;
	curmp.flags = PG_BUSY;
	endmp.uobject = uobj;
	endmp.offset = (voff_t)-1;
	endmp.flags = PG_BUSY;
	if (by_list) {
		pg = TAILQ_FIRST(&uobj->memq);
		TAILQ_INSERT_TAIL(&uobj->memq, &endmp, listq);
		PHOLD(curproc);
	} else {
		pg = uvm_pagelookup(uobj, off);
	}
	nextpg = NULL;
	while (by_list || off < endoff) {

		/*
		 * if the current page is not interesting, move on to the next.
		 */

		KASSERT(pg == NULL || pg->uobject == uobj);
		KASSERT(pg == NULL ||
			(pg->flags & (PG_RELEASED|PG_PAGEOUT)) == 0 ||
			(pg->flags & PG_BUSY) != 0);
		if (by_list) {
			if (pg == &endmp) {
				break;
			}
			if (pg->offset < startoff || pg->offset >= endoff ||
			    pg->flags & (PG_RELEASED|PG_PAGEOUT)) {
				pg = TAILQ_NEXT(pg, listq);
				continue;
			}
			off = pg->offset;
		} else if (pg == NULL || pg->flags & (PG_RELEASED|PG_PAGEOUT)) {
			off += PAGE_SIZE;
			if (off < endoff) {
				pg = uvm_pagelookup(uobj, off);
			}
			continue;
		}

		/*
		 * if the current page needs to be cleaned and it's busy,
		 * wait for it to become unbusy.
		 */

		if (flags & PGO_FREE) {
			pmap_page_protect(pg, VM_PROT_NONE);
		}
		if (flags & PGO_CLEANIT) {
			needs_clean = pmap_clear_modify(pg) ||
				(pg->flags & PG_CLEAN) == 0;
			pg->flags |= PG_CLEAN;
		} else {
			needs_clean = FALSE;
		}
		if (needs_clean && pg->flags & PG_BUSY) {
			KASSERT(curproc != uvm.pagedaemon_proc);
			UVMHIST_LOG(ubchist, "busy %p", pg,0,0,0);
			if (by_list) {
				TAILQ_INSERT_BEFORE(pg, &curmp, listq);
				UVMHIST_LOG(ubchist, "curmp next %p",
					    TAILQ_NEXT(&curmp, listq), 0,0,0);
			}
			pg->flags |= PG_WANTED;
			pg->flags &= ~PG_CLEAN;
			UVM_UNLOCK_AND_WAIT(pg, &uobj->vmobjlock, 0,
			    "genput", 0);
			simple_lock(&uobj->vmobjlock);
			if (by_list) {
				UVMHIST_LOG(ubchist, "after next %p",
					    TAILQ_NEXT(&curmp, listq), 0,0,0);
				pg = TAILQ_NEXT(&curmp, listq);
				TAILQ_REMOVE(&uobj->memq, &curmp, listq);
			} else {
				pg = uvm_pagelookup(uobj, off);
			}
			continue;
		}

		/*
		 * if we're cleaning, build a cluster.
		 * the cluster will consist of pages which are currently dirty,
		 * but they will be returned to us marked clean.
		 * if not cleaning, just operate on the one page.
		 */

		if (needs_clean) {
			wasclean = FALSE;
			memset(pgs, 0, sizeof(pgs));
			pg->flags |= PG_BUSY;
			UVM_PAGE_OWN(pg, "genfs_putpages");

			/*
			 * first look backward.
			 */

			npages = MIN(n >> 1, off >> PAGE_SHIFT);
			nback = npages;
			uvn_findpages(uobj, off - PAGE_SIZE, &nback, &pgs[0],
			    UFP_NOWAIT|UFP_NOALLOC|UFP_DIRTYONLY|UFP_BACKWARD);
			if (nback) {
				memmove(&pgs[0], &pgs[npages - nback],
				    nback * sizeof(pgs[0]));
			}
			n -= nback;

			/*
			 * then plug in our page of interest.
			 */

			pgs[nback] = pg;

			/*
			 * then look forward to fill in the remaining space in
			 * the array of pages.
			 */

			npages = MIN(n, (endoff - off) >> PAGE_SHIFT) - 1;
			uvn_findpages(uobj, off + PAGE_SIZE, &npages,
			    &pgs[nback + 1],
			    UFP_NOWAIT|UFP_NOALLOC|UFP_DIRTYONLY);
			npages += nback + 1;
		} else {
			pgs[0] = pg;
			npages = 1;
		}

		/*
		 * apply FREE or DEACTIVATE options if requested.
		 */

		if (flags & (PGO_DEACTIVATE|PGO_FREE)) {
			uvm_lock_pageq();
		}
		for (i = 0; i < npages; i++) {
			tpg = pgs[i];
			KASSERT(tpg->uobject == uobj);
			if (flags & PGO_DEACTIVATE &&
			    (tpg->pqflags & PQ_INACTIVE) == 0 &&
			    tpg->wire_count == 0) {
				(void) pmap_clear_reference(tpg);
				uvm_pagedeactivate(tpg);
			} else if (flags & PGO_FREE) {
				pmap_page_protect(tpg, VM_PROT_NONE);
				if (tpg->flags & PG_BUSY) {
					tpg->flags |= freeflag;
					if (freeflag == PG_PAGEOUT) {
						uvmexp.paging++;
						uvm_pagedequeue(tpg);
					}
				} else {
					nextpg = TAILQ_NEXT(tpg, listq);
					uvm_pagefree(tpg);
				}
			}
		}
		if (flags & (PGO_DEACTIVATE|PGO_FREE)) {
			uvm_unlock_pageq();
		}
		if (needs_clean) {

			/*
			 * start the i/o.  if we're traversing by list,
			 * keep our place in the list with a marker page.
			 */

			if (by_list) {
				TAILQ_INSERT_AFTER(&uobj->memq, pg, &curmp,
				    listq);
			}
			simple_unlock(&uobj->vmobjlock);
			error = GOP_WRITE(vp, pgs, npages, flags);
			simple_lock(&uobj->vmobjlock);
			if (by_list) {
				pg = TAILQ_NEXT(&curmp, listq);
				TAILQ_REMOVE(&uobj->memq, &curmp, listq);
			}
			if (error == ENOMEM) {
				for (i = 0; i < npages; i++) {
					tpg = pgs[i];
					if (tpg->flags & PG_PAGEOUT) {
						tpg->flags &= ~PG_PAGEOUT;
						uvmexp.paging--;
					}
					tpg->flags &= ~PG_CLEAN;
					uvm_pageactivate(tpg);
				}
				uvm_page_unbusy(pgs, npages);
			}
			if (error) {
				break;
			}
			if (by_list) {
				continue;
			}
		}

		/*
		 * find the next page and continue if there was no error.
		 */

		if (by_list) {
			if (nextpg) {
				pg = nextpg;
				nextpg = NULL;
			} else {
				pg = TAILQ_NEXT(pg, listq);
			}
		} else {
			off += PAGE_SIZE;
			if (off < endoff) {
				pg = uvm_pagelookup(uobj, off);
			}
		}
	}
	if (by_list) {
		TAILQ_REMOVE(&uobj->memq, &endmp, listq);
		PRELE(curproc);
	}

	/*
	 * if we're cleaning and there was nothing to clean,
	 * take us off the syncer list.  if we started any i/o
	 * and we're doing sync i/o, wait for all writes to finish.
	 */

	if ((flags & PGO_CLEANIT) && wasclean &&
	    startoff == 0 && endoff == trunc_page(LLONG_MAX) &&
	    LIST_FIRST(&vp->v_dirtyblkhd) == NULL &&
	    (vp->v_flag & VONWORKLST)) {
		vp->v_flag &= ~VONWORKLST;
		LIST_REMOVE(vp, v_synclist);
	}
	if (!wasclean && !async) {
		s = splbio();
		while (vp->v_numoutput != 0) {
			vp->v_flag |= VBWAIT;
			UVM_UNLOCK_AND_WAIT(&vp->v_numoutput, &uobj->vmobjlock,
					    FALSE, "genput2",0);
			simple_lock(&uobj->vmobjlock);
		}
		splx(s);
	}
	simple_unlock(&uobj->vmobjlock);
	return error;
}

int
genfs_gop_write(struct vnode *vp, struct vm_page **pgs, int npages, int flags)
{
	int s, error, run;
	int fs_bshift, dev_bshift;
	vaddr_t kva;
	off_t eof, offset, startoffset;
	size_t bytes, iobytes, skipbytes;
	daddr_t lbn, blkno;
	struct vm_page *pg;
	struct buf *mbp, *bp;
	struct vnode *devvp;
	boolean_t async = (flags & PGO_SYNCIO) == 0;
	UVMHIST_FUNC("genfs_do_putpages"); UVMHIST_CALLED(ubchist);

	UVMHIST_LOG(ubchist, "vp %p pgs %p npages %d flags 0x%x",
	    vp, pgs, npages, flags);

	GOP_SIZE(vp, vp->v_size, &eof);
	if (vp->v_type == VREG) {
		fs_bshift = vp->v_mount->mnt_fs_bshift;
		dev_bshift = vp->v_mount->mnt_dev_bshift;
	} else {
		fs_bshift = DEV_BSHIFT;
		dev_bshift = DEV_BSHIFT;
	}
	error = 0;
	pg = pgs[0];
	startoffset = pg->offset;
	bytes = MIN(npages << PAGE_SHIFT, eof - startoffset);
	skipbytes = 0;
	KASSERT(bytes != 0);

	kva = uvm_pagermapin(pgs, npages, UVMPAGER_MAPIN_WRITE |
			     UVMPAGER_MAPIN_WAITOK);

	s = splbio();
	vp->v_numoutput += 2;
	mbp = pool_get(&bufpool, PR_WAITOK);
	UVMHIST_LOG(ubchist, "vp %p mbp %p num now %d bytes 0x%x",
		    vp, mbp, vp->v_numoutput, bytes);
	splx(s);
	mbp->b_bufsize = npages << PAGE_SHIFT;
	mbp->b_data = (void *)kva;
	mbp->b_resid = mbp->b_bcount = bytes;
	mbp->b_flags = B_BUSY|B_WRITE|B_AGE| (async ? B_CALL : 0);
	mbp->b_iodone = uvm_aio_biodone;
	mbp->b_vp = vp;
	LIST_INIT(&mbp->b_dep);

	bp = NULL;
	for (offset = startoffset;
	     bytes > 0;
	     offset += iobytes, bytes -= iobytes) {
		lbn = offset >> fs_bshift;
		error = VOP_BMAP(vp, lbn, &devvp, &blkno, &run);
		if (error) {
			UVMHIST_LOG(ubchist, "VOP_BMAP() -> %d", error,0,0,0);
			skipbytes += bytes;
			bytes = 0;
			break;
		}

		iobytes = MIN((((off_t)lbn + 1 + run) << fs_bshift) - offset,
		    bytes);
		if (blkno == (daddr_t)-1) {
			skipbytes += iobytes;
			continue;
		}

		/* if it's really one i/o, don't make a second buf */
		if (offset == startoffset && iobytes == bytes) {
			bp = mbp;
		} else {
			s = splbio();
			vp->v_numoutput++;
			bp = pool_get(&bufpool, PR_WAITOK);
			UVMHIST_LOG(ubchist, "vp %p bp %p num now %d",
				    vp, bp, vp->v_numoutput, 0);
			splx(s);
			bp->b_data = (char *)kva +
				(vaddr_t)(offset - pg->offset);
			bp->b_resid = bp->b_bcount = iobytes;
			bp->b_flags = B_BUSY|B_WRITE|B_CALL;
			bp->b_iodone = uvm_aio_biodone1;
			bp->b_vp = vp;
			LIST_INIT(&bp->b_dep);
		}
		bp->b_lblkno = 0;
		bp->b_private = mbp;
		if (devvp->v_type == VBLK) {
			bp->b_dev = devvp->v_rdev;
		}

		/* adjust physical blkno for partial blocks */
		bp->b_blkno = blkno + ((offset - ((off_t)lbn << fs_bshift)) >>
				       dev_bshift);
		UVMHIST_LOG(ubchist, "vp %p offset 0x%x bcount 0x%x blkno 0x%x",
			    vp, offset, bp->b_bcount, bp->b_blkno);
		VOP_STRATEGY(bp);
	}
	if (skipbytes) {
		UVMHIST_LOG(ubchist, "skipbytes %d", skipbytes, 0,0,0);
		s = splbio();
		if (error) {
			mbp->b_flags |= B_ERROR;
			mbp->b_error = error;
		}
		mbp->b_resid -= skipbytes;
		if (mbp->b_resid == 0) {
			biodone(mbp);
		}
		splx(s);
	}
	if (async) {
		UVMHIST_LOG(ubchist, "returning 0 (async)", 0,0,0,0);
		return 0;
	}
	UVMHIST_LOG(ubchist, "waiting for mbp %p", mbp,0,0,0);
	error = biowait(mbp);
	uvm_aio_aiodone(mbp);
	UVMHIST_LOG(ubchist, "returning, error %d", error,0,0,0);
	return error;
}

void
genfs_node_init(struct vnode *vp, struct genfs_ops *ops)
{
	struct genfs_node *gp = VTOG(vp);

	lockinit(&gp->g_glock, PINOD, "glock", 0, 0);
	gp->g_op = ops;
}

void
genfs_size(struct vnode *vp, off_t size, off_t *eobp)
{
	int bsize;

	bsize = 1 << vp->v_mount->mnt_fs_bshift;
	*eobp = (size + bsize - 1) & ~(bsize - 1);
}
