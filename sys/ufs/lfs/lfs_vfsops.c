/*	$NetBSD: lfs_vfsops.c,v 1.64 2001/01/26 07:59:23 itohy Exp $	*/

/*-
 * Copyright (c) 1999, 2000 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Konrad E. Schroder <perseant@hhhh.org>.
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
 *      This product includes software developed by the NetBSD
 *      Foundation, Inc. and its contributors.
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
/*-
 * Copyright (c) 1989, 1991, 1993, 1994
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
 *	@(#)lfs_vfsops.c	8.20 (Berkeley) 6/10/95
 */

#if defined(_KERNEL) && !defined(_LKM)
#include "opt_quota.h"
#endif

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/namei.h>
#include <sys/proc.h>
#include <sys/kernel.h>
#include <sys/vnode.h>
#include <sys/mount.h>
#include <sys/buf.h>
#include <sys/device.h>
#include <sys/mbuf.h>
#include <sys/file.h>
#include <sys/disklabel.h>
#include <sys/ioctl.h>
#include <sys/errno.h>
#include <sys/malloc.h>
#include <sys/pool.h>
#include <sys/socket.h>
#include <uvm/uvm_extern.h>
#include <sys/sysctl.h>

#include <miscfs/specfs/specdev.h>

#include <ufs/ufs/quota.h>
#include <ufs/ufs/inode.h>
#include <ufs/ufs/ufsmount.h>
#include <ufs/ufs/ufs_extern.h>

#include <ufs/lfs/lfs.h>
#include <ufs/lfs/lfs_extern.h>

int lfs_mountfs __P((struct vnode *, struct mount *, struct proc *));

extern const struct vnodeopv_desc lfs_vnodeop_opv_desc;
extern const struct vnodeopv_desc lfs_specop_opv_desc;
extern const struct vnodeopv_desc lfs_fifoop_opv_desc;

const struct vnodeopv_desc * const lfs_vnodeopv_descs[] = {
	&lfs_vnodeop_opv_desc,
	&lfs_specop_opv_desc,
	&lfs_fifoop_opv_desc,
	NULL,
};

struct vfsops lfs_vfsops = {
	MOUNT_LFS,
	lfs_mount,
	ufs_start,
	lfs_unmount,
	ufs_root,
	ufs_quotactl,
	lfs_statfs,
	lfs_sync,
	lfs_vget,
	lfs_fhtovp,
	lfs_vptofh,
	lfs_init,
	lfs_done,
	lfs_sysctl,
	lfs_mountroot,
	ufs_check_export,
	lfs_vnodeopv_descs,
};

struct pool lfs_inode_pool;

extern int locked_queue_count;
extern long locked_queue_bytes;

/*
 * Initialize the filesystem, most work done by ufs_init.
 */
void
lfs_init()
{
	ufs_init();

	/*
	 * XXX Same structure as FFS inodes?  Should we share a common pool?
	 */
	pool_init(&lfs_inode_pool, sizeof(struct inode), 0, 0, 0,
		  "lfsinopl", 0, pool_page_alloc_nointr, pool_page_free_nointr,
		  M_LFSNODE);
}

void
lfs_done()
{
	ufs_done();
	pool_destroy(&lfs_inode_pool);
}

/*
 * Called by main() when ufs is going to be mounted as root.
 */
int
lfs_mountroot()
{
	extern struct vnode *rootvp;
	struct mount *mp;
	struct proc *p = curproc;	/* XXX */
	int error;
	
	if (root_device->dv_class != DV_DISK)
		return (ENODEV);

	if (rootdev == NODEV)
	  	return (ENODEV);
	/*
	 * Get vnodes for swapdev and rootdev.
	 */
	if ((error = bdevvp(rootdev, &rootvp))) {
		printf("lfs_mountroot: can't setup bdevvp's");
		return (error);
	}
	if ((error = vfs_rootmountalloc(MOUNT_LFS, "root_device", &mp))) {
		vrele(rootvp);
		return (error);
	}
	if ((error = lfs_mountfs(rootvp, mp, p))) {
		mp->mnt_op->vfs_refcount--;
		vfs_unbusy(mp);
		free(mp, M_MOUNT);
		vrele(rootvp);
		return (error);
	}
	simple_lock(&mountlist_slock);
	CIRCLEQ_INSERT_TAIL(&mountlist, mp, mnt_list);
	simple_unlock(&mountlist_slock);
	(void)lfs_statfs(mp, &mp->mnt_stat, p);
	vfs_unbusy(mp);
	inittodr(VFSTOUFS(mp)->um_lfs->lfs_tstamp);
	return (0);
}

/*
 * VFS Operations.
 *
 * mount system call
 */
int
lfs_mount(mp, path, data, ndp, p)
	struct mount *mp;
	const char *path;
	void *data;
	struct nameidata *ndp;
	struct proc *p;
{
	struct vnode *devvp;
	struct ufs_args args;
	struct ufsmount *ump = NULL;
	struct lfs *fs = NULL;				/* LFS */
	size_t size;
	int error;
	mode_t accessmode;

	error = copyin(data, (caddr_t)&args, sizeof (struct ufs_args));
	if (error)
		return (error);

#if 0
	/* Until LFS can do NFS right.		XXX */
	if (args.export.ex_flags & MNT_EXPORTED)
		return (EINVAL);
#endif

	/*
	 * If updating, check whether changing from read-only to
	 * read/write; if there is no device name, that's all we do.
	 */
	if (mp->mnt_flag & MNT_UPDATE) {
		ump = VFSTOUFS(mp);
		fs = ump->um_lfs;
		if (fs->lfs_ronly && (mp->mnt_flag & MNT_WANTRDWR)) {
			/*
			 * If upgrade to read-write by non-root, then verify
			 * that user has necessary permissions on the device.
			 */
			if (p->p_ucred->cr_uid != 0) {
				vn_lock(ump->um_devvp, LK_EXCLUSIVE | LK_RETRY);
				error = VOP_ACCESS(ump->um_devvp, VREAD|VWRITE,
						   p->p_ucred, p);
				VOP_UNLOCK(ump->um_devvp, 0);
				if (error)
					return (error);
			}
			fs->lfs_ronly = 0;
		}
		if (args.fspec == 0) {
			/*
			 * Process export requests.
			 */
			return (vfs_export(mp, &ump->um_export, &args.export));
		}
	}
	/*
	 * Not an update, or updating the name: look up the name
	 * and verify that it refers to a sensible block device.
	 */
	NDINIT(ndp, LOOKUP, FOLLOW, UIO_USERSPACE, args.fspec, p);
	if ((error = namei(ndp)) != 0)
		return (error);
	devvp = ndp->ni_vp;
	if (devvp->v_type != VBLK) {
		vrele(devvp);
		return (ENOTBLK);
	}
	if (major(devvp->v_rdev) >= nblkdev) {
		vrele(devvp);
		return (ENXIO);
	}
	/*
	 * If mount by non-root, then verify that user has necessary
	 * permissions on the device.
	 */
	if (p->p_ucred->cr_uid != 0) {
		accessmode = VREAD;
		if ((mp->mnt_flag & MNT_RDONLY) == 0)
			accessmode |= VWRITE;
		vn_lock(devvp, LK_EXCLUSIVE | LK_RETRY);
		error = VOP_ACCESS(devvp, accessmode, p->p_ucred, p);
		if (error) {
			vput(devvp);
			return (error);
		}
		VOP_UNLOCK(devvp, 0);
	}
	if ((mp->mnt_flag & MNT_UPDATE) == 0)
		error = lfs_mountfs(devvp, mp, p);		/* LFS */
	else {
		if (devvp != ump->um_devvp)
			error = EINVAL;	/* needs translation */
		else
			vrele(devvp);
	}
	if (error) {
		vrele(devvp);
		return (error);
	}
	ump = VFSTOUFS(mp);
	fs = ump->um_lfs;					/* LFS */
	(void)copyinstr(path, fs->lfs_fsmnt, sizeof(fs->lfs_fsmnt) - 1, &size);
	bzero(fs->lfs_fsmnt + size, sizeof(fs->lfs_fsmnt) - size);
	bcopy(fs->lfs_fsmnt, mp->mnt_stat.f_mntonname, MNAMELEN);
	(void) copyinstr(args.fspec, mp->mnt_stat.f_mntfromname, MNAMELEN - 1,
			 &size);
	bzero(mp->mnt_stat.f_mntfromname + size, MNAMELEN - size);
	return (0);
}

#ifdef LFS_DO_ROLLFORWARD
/*
 * Roll-forward code.
 */

/*
 * Load the appropriate indirect block, and change the appropriate pointer.
 * Mark the block dirty.  Do segment and avail accounting.
 */
static int
update_meta(struct lfs *fs, ino_t ino, int version, ufs_daddr_t lbn,
	    daddr_t ndaddr, size_t size, struct proc *p)
{
	int error;
	struct vnode *vp;
	struct inode *ip;
	daddr_t odaddr, ooff;
	struct indir a[NIADDR], *ap;
	struct buf *bp;
	SEGUSE *sup;
	int num;

	if ((error = lfs_rf_valloc(fs, ino, version, p, &vp)) != 0) {
		printf("update_meta: ino %d: lfs_rf_valloc returned %d\n", ino,
		       error);
		return error;
	}

	if ((error = VOP_BALLOC(vp, (lbn << fs->lfs_bshift), size,
				NOCRED, 0, &bp)) != 0) {
		vput(vp);
		return (error);
	}
	/* No need to write, the block is already on disk */
	if (bp->b_flags & B_DELWRI) {
		LFS_UNLOCK_BUF(bp);
		fs->lfs_avail += btodb(bp->b_bcount);
	}
	bp->b_flags |= B_INVAL;
	brelse(bp);

	/*
	 * Extend the file, if it is not large enough already.
	 * XXX this is not exactly right, we don't know how much of the
	 * XXX last block is actually used.  We hope that an inode will
	 * XXX appear later to give the correct size.
	 */
	ip = VTOI(vp);
	if (ip->i_ffs_size <= (lbn << fs->lfs_bshift)) {
		if (lbn < NDADDR)
			ip->i_ffs_size = (lbn << fs->lfs_bshift) +
				(size - fs->lfs_fsize) + 1;
		else
			ip->i_ffs_size = (lbn << fs->lfs_bshift) + 1;
	}

	error = ufs_bmaparray(vp, lbn, &odaddr, &a[0], &num, NULL);
	if (error) {
		printf("update_meta: ufs_bmaparray returned %d\n", error);
		vput(vp);
		return error;
	}
	switch (num) {
	    case 0:
		ooff = ip->i_ffs_db[lbn];
		if (ooff == UNWRITTEN)
			ip->i_ffs_blocks += btodb(size);
		ip->i_ffs_db[lbn] = ndaddr;
		break;
	    case 1:
		ooff = ip->i_ffs_ib[a[0].in_off];
		if (ooff == UNWRITTEN)
			ip->i_ffs_blocks += btodb(size);
		ip->i_ffs_ib[a[0].in_off] = ndaddr;
		break;
	    default:
		ap = &a[num - 1];
		if (bread(vp, ap->in_lbn, fs->lfs_bsize, NOCRED, &bp))
			panic("update_meta: bread bno %d", ap->in_lbn);
		
		ooff = ((ufs_daddr_t *)bp->b_data)[ap->in_off];
		if (ooff == UNWRITTEN)
			ip->i_ffs_blocks += btodb(size);
		((ufs_daddr_t *)bp->b_data)[ap->in_off] = ndaddr;
		(void) VOP_BWRITE(bp);
	}
	LFS_SET_UINO(ip, IN_CHANGE | IN_MODIFIED | IN_UPDATE);

	/* Update segment usage information. */
	if (odaddr > 0) {
		LFS_SEGENTRY(sup, fs, datosn(fs, odaddr), bp);
#ifdef DIAGNOSTIC
		if (sup->su_nbytes < size) {
			panic("update_meta: negative bytes "
			      "(segment %d short by %ld)\n",
			      datosn(fs, odaddr), (long)size - sup->su_nbytes);
			sup->su_nbytes = size;
		}
#endif
		sup->su_nbytes -= size;
		VOP_BWRITE(bp);
	}
	LFS_SEGENTRY(sup, fs, datosn(fs, ndaddr), bp);
	sup->su_nbytes += size;
	VOP_BWRITE(bp);

	/* Fix this so it can be released */
	/* ip->i_lfs_effnblks = ip->i_ffs_blocks; */

	/* Now look again to make sure it worked */
	ufs_bmaparray(vp, lbn, &odaddr, &a[0], &num, NULL );
	if (odaddr != ndaddr)
		printf("update_meta: failed setting ino %d lbn %d to %x\n",
		       ino, lbn, ndaddr);

	vput(vp);
	return 0;
}

static int
update_inoblk(struct lfs *fs, daddr_t offset, struct ucred *cred,
	      struct proc *p)
{
	struct vnode *devvp, *vp;
	struct inode *ip;
	struct dinode *dip;
	struct buf *dbp, *ibp;
	int error;
	daddr_t daddr;
	IFILE *ifp;
	SEGUSE *sup;

	devvp = VTOI(fs->lfs_ivnode)->i_devvp;

	/*
	 * Get the inode, update times and perms.
	 * DO NOT update disk blocks, we do that separately.
	 */
	error = bread(devvp, offset, fs->lfs_bsize, cred, &dbp);
	if (error) {
		printf("update_inoblk: bread returned %d\n", error);
		return error;
	}
	dip = ((struct dinode *)(dbp->b_data)) + INOPB(fs);
	while(--dip >= (struct dinode *)dbp->b_data) {
		if(dip->di_inumber > LFS_IFILE_INUM) {
			/* printf("ino %d version %d\n", dip->di_inumber,
			       dip->di_gen); */
			error = lfs_rf_valloc(fs, dip->di_inumber, dip->di_gen,
					      p, &vp);
			if (error) {
				printf("update_inoblk: lfs_rf_valloc returned %d\n", error);
				continue;
			}
			ip = VTOI(vp);
			if (dip->di_size != ip->i_ffs_size)
				VOP_TRUNCATE(vp, dip->di_size, 0, NOCRED, p);
			/* Get mode, link count, size, and times */
			memcpy(&ip->i_din.ffs_din, dip, 
			       offsetof(struct dinode, di_db[0]));

			/* Then the rest, except di_blocks */
			ip->i_ffs_flags = dip->di_flags;
			ip->i_ffs_gen = dip->di_gen;
			ip->i_ffs_uid = dip->di_uid;
			ip->i_ffs_gid = dip->di_gid;

			ip->i_ffs_effnlink = dip->di_nlink;

			LFS_SET_UINO(ip, IN_CHANGE | IN_MODIFIED | IN_UPDATE);

			/* Re-initialize to get type right */
			ufs_vinit(vp->v_mount, lfs_specop_p, lfs_fifoop_p,
				  &vp);
			vput(vp);

			/* Record change in location */
			LFS_IENTRY(ifp, fs, dip->di_inumber, ibp);
			daddr = ifp->if_daddr;
			ifp->if_daddr = dbp->b_blkno;
			error = VOP_BWRITE(ibp); /* Ifile */
			/* And do segment accounting */
			if (datosn(fs, daddr) != datosn(fs, dbp->b_blkno)) {
				if (daddr > 0) {
					LFS_SEGENTRY(sup, fs, datosn(fs, daddr),
						     ibp);
					sup->su_nbytes -= DINODE_SIZE;
					VOP_BWRITE(ibp);
				}
				LFS_SEGENTRY(sup, fs, datosn(fs, dbp->b_blkno),
					     ibp);
				sup->su_nbytes += DINODE_SIZE;
				VOP_BWRITE(ibp);
			}
		}
	}
	dbp->b_flags |= B_AGE;
	brelse(dbp);

	return 0;
}

#define CHECK_CKSUM   0x0001  /* Check the checksum to make sure it's valid */
#define CHECK_UPDATE  0x0002  /* Update Ifile for new data blocks / inodes */

static daddr_t
check_segsum(struct lfs *fs, daddr_t offset,
	     struct ucred *cred, int flags, int *pseg_flags, struct proc *p)
{
	struct vnode *devvp;
	struct buf *bp, *dbp;
	int error, nblocks, ninos, i, j;
	SEGSUM *ssp;
	u_long *dp, *datap; /* XXX u_int32_t */
	daddr_t *iaddr, oldoffset;
	FINFO *fip;
	SEGUSE *sup;
	size_t size;

	devvp = VTOI(fs->lfs_ivnode)->i_devvp;
	/*
	 * If the segment has a superblock and we're at the top
	 * of the segment, skip the superblock.
	 */
	if(sntoda(fs, datosn(fs, offset)) == offset) {
       		LFS_SEGENTRY(sup, fs, datosn(fs, offset), bp); 
       		if(sup->su_flags & SEGUSE_SUPERBLOCK)
			offset += btodb(LFS_SBPAD);
       		brelse(bp);
	}

	/* Read in the segment summary */
	error = bread(devvp, offset, LFS_SUMMARY_SIZE, cred, &bp);
	if(error)
		return -1;
	
	/* Check summary checksum */
	ssp = (SEGSUM *)bp->b_data;
	if(flags & CHECK_CKSUM) {
		if(ssp->ss_sumsum != cksum(&ssp->ss_datasum,
					   LFS_SUMMARY_SIZE -
					   sizeof(ssp->ss_sumsum))) {
#ifdef DEBUG_LFS_RFW
			printf("Sumsum error at 0x%x\n", offset);
#endif
			offset = -1;
			goto err1;
		}
		if (ssp->ss_nfinfo == 0 && ssp->ss_ninos == 0) {
#ifdef DEBUG_LFS_RFW
			printf("Empty pseg at 0x%x\n", offset);
#endif
			offset = -1;
			goto err1;
		}
		if (ssp->ss_create < fs->lfs_tstamp) {
#ifdef DEBUG_LFS_RFW
			printf("Old data at 0x%x\n", offset);
#endif
			offset = -1;
			goto err1;
		}
	}
	if(pseg_flags)
		*pseg_flags = ssp->ss_flags;
	oldoffset = offset;
	offset += btodb(LFS_SUMMARY_SIZE);

	ninos = howmany(ssp->ss_ninos, INOPB(fs));
	iaddr = (daddr_t *)(bp->b_data + LFS_SUMMARY_SIZE - sizeof(daddr_t));
	if(flags & CHECK_CKSUM) {
		/* Count blocks */
		nblocks = 0;
		fip = (FINFO *)(bp->b_data + sizeof(SEGSUM));
		for(i = 0; i < ssp->ss_nfinfo; ++i) {
			nblocks += fip->fi_nblocks;
			if(fip->fi_nblocks <= 0)
				break;
			fip = (FINFO *)(((char *)fip) + sizeof(FINFO) +
					(fip->fi_nblocks - 1) *
					sizeof(ufs_daddr_t));
		}
		nblocks += ninos;
		/* Create the sum array */
		datap = dp = (u_long *)malloc(nblocks * sizeof(u_long),
					      M_SEGMENT, M_WAITOK);
	}

	/* Handle individual blocks */
	fip = (FINFO *)(bp->b_data + sizeof(SEGSUM));
	for(i = 0; i < ssp->ss_nfinfo || ninos; ++i) {
		/* Inode block? */
		if(ninos && *iaddr == offset) {
			if(flags & CHECK_CKSUM) {
				/* Read in the head and add to the buffer */
				error = bread(devvp, offset, fs->lfs_bsize,
					      cred, &dbp);
				if(error) {
					offset = -1;
					goto err2;
				}
				(*dp++) = ((u_long *)(dbp->b_data))[0];
				dbp->b_flags |= B_AGE;
				brelse(dbp);
			}
			if(flags & CHECK_UPDATE) {
				if ((error = update_inoblk(fs, offset, cred, p))
				    != 0) {
					offset = -1;
					goto err2;
				}
			}
			offset += fsbtodb(fs,1);
			--iaddr;
			--ninos;
			--i; /* compensate */
			continue;
		}
		/* printf("check: blocks from ino %d version %d\n",
		       fip->fi_ino, fip->fi_version); */
		size = fs->lfs_bsize;
		for(j = 0; j < fip->fi_nblocks; ++j) {
			if (j == fip->fi_nblocks - 1)
				size = fip->fi_lastlength;
			if(flags & CHECK_CKSUM) {
				error = bread(devvp, offset, size, cred, &dbp);
				if(error) {
					offset = -1;
					goto err2;
				}
				(*dp++) = ((u_long *)(dbp->b_data))[0];
				dbp->b_flags |= B_AGE;
				brelse(dbp);
			}
			/* Account for and update any direct blocks */
			if((flags & CHECK_UPDATE) &&
			   fip->fi_ino > LFS_IFILE_INUM &&
			   fip->fi_blocks[j] >= 0) {
				update_meta(fs, fip->fi_ino, fip->fi_version,
					    fip->fi_blocks[j], offset, size, p);
			}
			offset += btodb(size);
		}
		fip = (FINFO *)(((char *)fip) + sizeof(FINFO)
				+ (fip->fi_nblocks - 1) * sizeof(ufs_daddr_t));
	}
	/* Checksum the array, compare */
	if((flags & CHECK_CKSUM) &&
	   ssp->ss_datasum != cksum(datap, nblocks * sizeof(u_long)))
	{
		printf("Datasum error at 0x%x (wanted %x got %x)\n", offset,
		       ssp->ss_datasum, cksum(datap, nblocks *
					      sizeof(u_long)));
		offset = -1;
		goto err2;
	}

	/* If we're at the end of the segment, move to the next */
	if(datosn(fs, offset + btodb(LFS_SUMMARY_SIZE + fs->lfs_bsize)) !=
	   datosn(fs, offset)) {
		if (datosn(fs, offset) == datosn(fs, ssp->ss_next)) {
			offset = -1;
			goto err2;
		}
		offset = ssp->ss_next;
#ifdef DEBUG_LFS_RFW
		printf("LFS roll forward: moving on to offset 0x%x "
		       " -> segment %d\n", offset, datosn(fs,offset));
#endif
	}

	if (flags & CHECK_UPDATE) {
		fs->lfs_avail -= (offset - oldoffset);
		/* Don't clog the buffer queue */
		if (locked_queue_count > LFS_MAX_BUFS ||
		    locked_queue_bytes > LFS_MAX_BYTES) {
			++fs->lfs_writer;
			lfs_flush(fs, SEGM_CKP);
			if(--fs->lfs_writer==0)
				wakeup(&fs->lfs_dirops);
		}
	}

    err2:
	if(flags & CHECK_CKSUM)
		free(datap, M_SEGMENT);
    err1:
	bp->b_flags |= B_AGE;
	brelse(bp);

	return offset;
}
#endif /* LFS_DO_ROLLFORWARD */

/*
 * Common code for mount and mountroot
 * LFS specific
 */
int
lfs_mountfs(devvp, mp, p)
	struct vnode *devvp;
	struct mount *mp;
	struct proc *p;
{
	extern struct vnode *rootvp;
	struct dlfs *tdfs, *dfs, *adfs;
	struct lfs *fs;
	struct ufsmount *ump;
	struct vnode *vp;
	struct buf *bp, *abp;
	struct partinfo dpart;
	dev_t dev;
	int error, i, ronly, size;
	struct ucred *cred;
	CLEANERINFO *cip;
        SEGUSE *sup;
#ifdef LFS_DO_ROLLFORWARD
	int flags, dirty;
	daddr_t offset, oldoffset, lastgoodpseg;
	int sn, curseg;
#endif

	cred = p ? p->p_ucred : NOCRED;
	/*
	 * Disallow multiple mounts of the same device.
	 * Disallow mounting of a device that is currently in use
	 * (except for root, which might share swap device for miniroot).
	 * Flush out any old buffers remaining from a previous use.
	 */
	if ((error = vfs_mountedon(devvp)) != 0)
		return (error);
	if (vcount(devvp) > 1 && devvp != rootvp)
		return (EBUSY);
	if ((error = vinvalbuf(devvp, V_SAVE, cred, p, 0, 0)) != 0)
		return (error);

	ronly = (mp->mnt_flag & MNT_RDONLY) != 0;
	error = VOP_OPEN(devvp, ronly ? FREAD : FREAD|FWRITE, FSCRED, p);
	if (error)
		return (error);
	if (VOP_IOCTL(devvp, DIOCGPART, (caddr_t)&dpart, FREAD, cred, p) != 0)
		size = DEV_BSIZE;
	else
		size = dpart.disklab->d_secsize;

	/* Don't free random space on error. */
	bp = NULL;
	abp = NULL;
	ump = NULL;

	/* Read in the superblock. */
	error = bread(devvp, LFS_LABELPAD / size, LFS_SBPAD, cred, &bp);
	if (error)
		goto out;
	dfs = (struct dlfs *)bp->b_data;

	/* Check the basics. */
	if (dfs->dlfs_magic != LFS_MAGIC || dfs->dlfs_bsize > MAXBSIZE ||
	    dfs->dlfs_version > LFS_VERSION ||
	    dfs->dlfs_bsize < sizeof(struct dlfs)) {
		error = EINVAL;		/* XXX needs translation */
		goto out;
	}

	/*
	 * Check the second superblock to see which is newer; then mount
	 * using the older of the two.  This is necessary to ensure that
	 * the filesystem is valid if it was not unmounted cleanly.
	 */

	if (dfs->dlfs_sboffs[1] &&
	    dfs->dlfs_sboffs[1]-(LFS_LABELPAD/size) > LFS_SBPAD/size)
	{
		error = bread(devvp, dfs->dlfs_sboffs[1], LFS_SBPAD, cred, &abp);
		if (error)
			goto out;
		adfs = (struct dlfs *)abp->b_data;

		if (adfs->dlfs_tstamp < dfs->dlfs_tstamp) /* XXX 1s? */
			tdfs = adfs;
		else
			tdfs = dfs;

		/* Check the basics. */
		if (tdfs->dlfs_magic != LFS_MAGIC ||
		    tdfs->dlfs_bsize > MAXBSIZE ||
	    	    tdfs->dlfs_version > LFS_VERSION ||
	    	    tdfs->dlfs_bsize < sizeof(struct dlfs)) {
			error = EINVAL;		/* XXX needs translation */
			goto out;
		}
	} else {
		printf("lfs_mountfs: invalid alt superblock daddr=0x%x\n",
			dfs->dlfs_sboffs[1]);
		error = EINVAL;
		goto out;
	}

	/* Allocate the mount structure, copy the superblock into it. */
	fs = malloc(sizeof(struct lfs), M_UFSMNT, M_WAITOK);
	memcpy(&fs->lfs_dlfs, tdfs, sizeof(struct dlfs));

#ifdef LFS_DO_ROLLFORWARD
	/* Before rolling forward, lock so vget will sleep for other procs */
	fs->lfs_flags = LFS_NOTYET;
	fs->lfs_rfpid = p->p_pid;
#else
	fs->lfs_flags = 0;
#endif

	ump = malloc(sizeof *ump, M_UFSMNT, M_WAITOK);
	memset((caddr_t)ump, 0, sizeof *ump);
	ump->um_lfs = fs;
	if (sizeof(struct lfs) < LFS_SBPAD) {			/* XXX why? */
		bp->b_flags |= B_INVAL;
		abp->b_flags |= B_INVAL;
	}
	brelse(bp);
	bp = NULL;
	brelse(abp);
	abp = NULL;

	/* Set up the I/O information */
	fs->lfs_iocount = 0;
	fs->lfs_diropwait = 0;
	fs->lfs_activesb = 0;
	fs->lfs_uinodes = 0;
	fs->lfs_ravail = 0;
#ifdef LFS_CANNOT_ROLLFW
	fs->lfs_sbactive = 0;
#endif
#ifdef LFS_TRACK_IOS
	for (i=0;i<LFS_THROTTLE;i++)
		fs->lfs_pending[i] = LFS_UNUSED_DADDR;
#endif

	/* Set up the ifile and lock aflags */
	fs->lfs_doifile = 0;
	fs->lfs_writer = 0;
	fs->lfs_dirops = 0;
	fs->lfs_nadirop = 0;
	fs->lfs_seglock = 0;
	lockinit(&fs->lfs_freelock, PINOD, "lfs_freelock", 0, 0);

	/* Set the file system readonly/modify bits. */
	fs->lfs_ronly = ronly;
	if (ronly == 0)
		fs->lfs_fmod = 1;

	/* Initialize the mount structure. */
	dev = devvp->v_rdev;
	mp->mnt_data = (qaddr_t)ump;
	mp->mnt_stat.f_fsid.val[0] = (long)dev;
	mp->mnt_stat.f_fsid.val[1] = makefstype(MOUNT_LFS);
	mp->mnt_stat.f_iosize = fs->lfs_bsize;
	mp->mnt_maxsymlinklen = fs->lfs_maxsymlinklen;
	mp->mnt_flag |= MNT_LOCAL;
	ump->um_flags = 0;
	ump->um_mountp = mp;
	ump->um_dev = dev;
	ump->um_devvp = devvp;
	ump->um_bptrtodb = 0;
	ump->um_seqinc = 1 << fs->lfs_fsbtodb;
	ump->um_nindir = fs->lfs_nindir;
	ump->um_lognindir = ffs(fs->lfs_nindir) - 1;
	for (i = 0; i < MAXQUOTAS; i++)
		ump->um_quotas[i] = NULLVP;
	devvp->v_specmountpoint = mp;

	/*
	 * We use the ifile vnode for almost every operation.  Instead of
	 * retrieving it from the hash table each time we retrieve it here,
	 * artificially increment the reference count and keep a pointer
	 * to it in the incore copy of the superblock.
	 */
	if ((error = VFS_VGET(mp, LFS_IFILE_INUM, &vp)) != 0)
		goto out;
	fs->lfs_ivnode = vp;
	VREF(vp);
	vput(vp);

#ifdef LFS_DO_ROLLFORWARD
	/*
	 * Roll forward.
	 */
	/*
	 * Phase I:
	 * Find the address of the last good partial segment that was written
	 * after the checkpoint.  Mark the segments in question dirty, so
	 * they won't be reallocated.
	 */
	lastgoodpseg = oldoffset = offset = fs->lfs_offset;
	flags = 0x0;
#ifdef DEBUG_LFS_RFW
	printf("LFS roll forward phase 1: starting at offset 0x%x\n", offset);
#endif
	LFS_SEGENTRY(sup, fs, datosn(fs, offset), bp);
	if (!(sup->su_flags & SEGUSE_DIRTY))
		--fs->lfs_nclean;
	sup->su_flags |= SEGUSE_DIRTY;
	(void) VOP_BWRITE(bp);
	while ((offset = check_segsum(fs, offset, cred, CHECK_CKSUM, &flags,
				      p)) > 0) {
		if(sntoda(fs, oldoffset) != sntoda(fs, offset)) {
        		LFS_SEGENTRY(sup, fs, datosn(fs, oldoffset), bp); 
			if (!(sup->su_flags & SEGUSE_DIRTY))
				--fs->lfs_nclean;
        		sup->su_flags |= SEGUSE_DIRTY;
        		(void) VOP_BWRITE(bp);
		}

#ifdef DEBUG_LFS_RFW
		printf("LFS roll forward phase 1: offset=0x%x\n", offset);
		if(flags & SS_DIROP) {
			printf("lfs_mountfs: dirops at 0x%x\n", oldoffset);
			if(!(flags & SS_CONT))
				printf("lfs_mountfs: dirops end at 0x%x\n",
				       oldoffset);
		}
#endif
		if(!(flags & SS_CONT))
			lastgoodpseg = offset;
		oldoffset = offset;
	}
#ifdef DEBUG_LFS_RFW
	if (flags & SS_CONT) {
		printf("LFS roll forward: warning: incomplete dirops discarded\n");
	}
	printf("LFS roll forward phase 1: completed: lastgoodpseg=0x%x\n",
	       lastgoodpseg);
#endif

	/* Don't accidentally overwrite what we're trying to preserve */
	offset = fs->lfs_offset;
	fs->lfs_offset = lastgoodpseg;
	fs->lfs_curseg = sntoda(fs, datosn(fs, fs->lfs_offset));
	for (sn = curseg = datosn(fs, fs->lfs_curseg);;) {
		sn = (sn + 1) % fs->lfs_nseg;
		if (sn == curseg)
			panic("lfs_mountfs: no clean segments");
		LFS_SEGENTRY(sup, fs, sn, bp);
		dirty = (sup->su_flags & SEGUSE_DIRTY);
		brelse(bp);
		if (!dirty)
			break;
	}
	fs->lfs_nextseg = sntoda(fs, sn);

	/*
	 * Phase II: Roll forward from the first superblock.
	 */
	while (offset != lastgoodpseg) {
#ifdef DEBUG_LFS_RFW
		printf("LFS roll forward phase 2: 0x%x\n", offset);
#endif
		oldoffset = offset;
		offset = check_segsum(fs, offset, cred, CHECK_UPDATE, NULL, p);
	}

	/*
	 * Finish: flush our changes to disk.
	 */
	lfs_segwrite(fs->lfs_ivnode->v_mount, SEGM_CKP | SEGM_SYNC);

#ifdef DEBUG_LFS_RFW
	printf("LFS roll forward complete\n");
#endif
	/* Allow vget now that roll-forward is complete */
	fs->lfs_flags &= ~(LFS_NOTYET);
	wakeup(&fs->lfs_flags);
#endif /* LFS_DO_ROLLFORWARD */

	/*
	 * Initialize the ifile cleaner info with information from 
	 * the superblock.
	 */ 
	LFS_CLEANERINFO(cip, fs, bp);
	cip->clean = fs->lfs_nclean;
	cip->dirty = fs->lfs_nseg - fs->lfs_nclean;
	cip->avail = fs->lfs_avail;
	cip->bfree = fs->lfs_bfree;
	(void) VOP_BWRITE(bp); /* Ifile */

	/*
	 * Mark the current segment as ACTIVE, since we're going to 
	 * be writing to it.
	 */
        LFS_SEGENTRY(sup, fs, datosn(fs, fs->lfs_offset), bp); 
        sup->su_flags |= SEGUSE_DIRTY | SEGUSE_ACTIVE;
        (void) VOP_BWRITE(bp); /* Ifile */

	return (0);
out:
	if (bp)
		brelse(bp);
	if (abp)
		brelse(abp);
	vn_lock(devvp, LK_EXCLUSIVE | LK_RETRY);
	(void)VOP_CLOSE(devvp, ronly ? FREAD : FREAD|FWRITE, cred, p);
	VOP_UNLOCK(devvp, 0);
	if (ump) {
		free(ump->um_lfs, M_UFSMNT);
		free(ump, M_UFSMNT);
		mp->mnt_data = (qaddr_t)0;
	}
	return (error);
}

/*
 * unmount system call
 */
int
lfs_unmount(mp, mntflags, p)
	struct mount *mp;
	int mntflags;
	struct proc *p;
{
	struct ufsmount *ump;
	struct lfs *fs;
	int error, flags, ronly;
	extern int lfs_allclean_wakeup;

	flags = 0;
	if (mntflags & MNT_FORCE)
		flags |= FORCECLOSE;

	ump = VFSTOUFS(mp);
	fs = ump->um_lfs;
#ifdef QUOTA
	if (mp->mnt_flag & MNT_QUOTA) {
		int i;
		error = vflush(mp, fs->lfs_ivnode, SKIPSYSTEM|flags);
		if (error)
			return (error);
		for (i = 0; i < MAXQUOTAS; i++) {
			if (ump->um_quotas[i] == NULLVP)
				continue;
			quotaoff(p, mp, i);
		}
		/*
		 * Here we fall through to vflush again to ensure
		 * that we have gotten rid of all the system vnodes.
		 */
	}
#endif
	if ((error = vflush(mp, fs->lfs_ivnode, flags)) != 0)
		return (error);
	fs->lfs_clean = 1;
	if ((error = VFS_SYNC(mp, 1, p->p_ucred, p)) != 0)
		return (error);
	if (fs->lfs_ivnode->v_dirtyblkhd.lh_first)
		panic("lfs_unmount: still dirty blocks on ifile vnode\n");
	vrele(fs->lfs_ivnode);
	vgone(fs->lfs_ivnode);

	ronly = !fs->lfs_ronly;
	if (ump->um_devvp->v_type != VBAD)
		ump->um_devvp->v_specmountpoint = NULL;
	vn_lock(ump->um_devvp, LK_EXCLUSIVE | LK_RETRY);
	error = VOP_CLOSE(ump->um_devvp,
	    ronly ? FREAD : FREAD|FWRITE, NOCRED, p);
	vput(ump->um_devvp);

	/* XXX KS - wake up the cleaner so it can die */
	wakeup(&fs->lfs_nextseg);
	wakeup(&lfs_allclean_wakeup);

	free(fs, M_UFSMNT);
	free(ump, M_UFSMNT);
	mp->mnt_data = (qaddr_t)0;
	mp->mnt_flag &= ~MNT_LOCAL;
	return (error);
}

/*
 * Get file system statistics.
 */
int
lfs_statfs(mp, sbp, p)
	struct mount *mp;
	struct statfs *sbp;
	struct proc *p;
{
	struct lfs *fs;
	struct ufsmount *ump;

	ump = VFSTOUFS(mp);
	fs = ump->um_lfs;
	if (fs->lfs_magic != LFS_MAGIC)
		panic("lfs_statfs: magic");

	sbp->f_type = 0;
	sbp->f_bsize = fs->lfs_fsize;
	sbp->f_iosize = fs->lfs_bsize;
	sbp->f_blocks = dbtofrags(fs, LFS_EST_NONMETA(fs));
	sbp->f_bfree = dbtofrags(fs, LFS_EST_BFREE(fs));
	sbp->f_bavail = dbtofrags(fs, (long)LFS_EST_BFREE(fs) -
				  (long)LFS_EST_RSVD(fs));
	sbp->f_files = dbtofsb(fs,fs->lfs_bfree) * INOPB(fs);
	sbp->f_ffree = sbp->f_files - fs->lfs_nfiles;
	if (sbp != &mp->mnt_stat) {
		bcopy(mp->mnt_stat.f_mntonname, sbp->f_mntonname, MNAMELEN);
		bcopy(mp->mnt_stat.f_mntfromname, sbp->f_mntfromname, MNAMELEN);
	}
	strncpy(sbp->f_fstypename, mp->mnt_op->vfs_name, MFSNAMELEN);
	return (0);
}

/*
 * Go through the disk queues to initiate sandbagged IO;
 * go through the inodes to write those that have been modified;
 * initiate the writing of the super block if it has been modified.
 *
 * Note: we are always called with the filesystem marked `MPBUSY'.
 */
int
lfs_sync(mp, waitfor, cred, p)
	struct mount *mp;
	int waitfor;
	struct ucred *cred;
	struct proc *p;
{
	int error;
	struct lfs *fs;

	fs = ((struct ufsmount *)mp->mnt_data)->ufsmount_u.lfs;
	if (fs->lfs_ronly)
		return 0;
	while(fs->lfs_dirops)
		error = tsleep(&fs->lfs_dirops, PRIBIO + 1, "lfs_dirops", 0);
	fs->lfs_writer++;

	/* All syncs must be checkpoints until roll-forward is implemented. */
	error = lfs_segwrite(mp, SEGM_CKP | (waitfor ? SEGM_SYNC : 0));
	if(--fs->lfs_writer==0)
		wakeup(&fs->lfs_dirops);
#ifdef QUOTA
	qsync(mp);
#endif
	return (error);
}

extern struct lock ufs_hashlock;

/*
 * Look up an LFS dinode number to find its incore vnode.  If not already
 * in core, read it in from the specified device.  Return the inode locked.
 * Detection and handling of mount points must be done by the calling routine.
 */
int
lfs_vget(mp, ino, vpp)
	struct mount *mp;
	ino_t ino;
	struct vnode **vpp;
{
	struct lfs *fs;
	struct inode *ip;
	struct buf *bp;
	struct ifile *ifp;
	struct vnode *vp;
	struct ufsmount *ump;
	ufs_daddr_t daddr;
	dev_t dev;
	int error;
#ifdef LFS_ATIME_IFILE
	struct timespec ts;
#endif

	ump = VFSTOUFS(mp);
	dev = ump->um_dev;
	fs = ump->um_lfs;

	/*
	 * If the filesystem is not completely mounted yet, suspend
	 * any access requests (wait for roll-forward to complete).
	 */
	while((fs->lfs_flags & LFS_NOTYET) && curproc->p_pid != fs->lfs_rfpid)
		tsleep(&fs->lfs_flags, PRIBIO+1, "lfs_notyet", 0);

	if ((*vpp = ufs_ihashget(dev, ino, LK_EXCLUSIVE)) != NULL)
		return (0);

	if ((error = getnewvnode(VT_LFS, mp, lfs_vnodeop_p, &vp)) != 0) {
		*vpp = NULL;
		 return (error);
	}

	do {
		if ((*vpp = ufs_ihashget(dev, ino, LK_EXCLUSIVE)) != NULL) {
			ungetnewvnode(vp);
			return (0);
		}
	} while (lockmgr(&ufs_hashlock, LK_EXCLUSIVE|LK_SLEEPFAIL, 0));

	/* Translate the inode number to a disk address. */
	if (ino == LFS_IFILE_INUM)
		daddr = fs->lfs_idaddr;
	else {
		/* XXX bounds-check this too */
		LFS_IENTRY(ifp, fs, ino, bp);
		daddr = ifp->if_daddr;
#ifdef LFS_ATIME_IFILE
		ts = ifp->if_atime; /* structure copy */
#endif
		brelse(bp);
		if (daddr == LFS_UNUSED_DADDR) {
			*vpp = NULLVP;
			ungetnewvnode(vp);
			lockmgr(&ufs_hashlock, LK_RELEASE, 0);
			return (ENOENT);
		}
	}

	/* Allocate/init new vnode/inode. */
	lfs_vcreate(mp, ino, vp);

	/*
	 * Put it onto its hash chain and lock it so that other requests for
	 * this inode will block if they arrive while we are sleeping waiting
	 * for old data structures to be purged or for the contents of the
	 * disk portion of this inode to be read.
	 */
	ip = VTOI(vp);
	ufs_ihashins(ip);
	lockmgr(&ufs_hashlock, LK_RELEASE, 0);

	/*
	 * XXX
	 * This may not need to be here, logically it should go down with
	 * the i_devvp initialization.
	 * Ask Kirk.
	 */
	ip->i_lfs = ump->um_lfs;

	/* Read in the disk contents for the inode, copy into the inode. */
	error = bread(ump->um_devvp, daddr, (int)fs->lfs_bsize, NOCRED, &bp);
	if (error) {
		/*
		 * The inode does not contain anything useful, so it would
		 * be misleading to leave it on its hash chain. With mode
		 * still zero, it will be unlinked and returned to the free
		 * list by vput().
		 */
		vput(vp);
		brelse(bp);
		*vpp = NULL;
		return (error);
	}
	ip->i_din.ffs_din = *lfs_ifind(fs, ino, bp);
	ip->i_ffs_effnlink = ip->i_ffs_nlink;
	ip->i_lfs_effnblks = ip->i_ffs_blocks;
#ifdef LFS_ATIME_IFILE
	ip->i_ffs_atime = ts.tv_sec;
	ip->i_ffs_atimensec = ts.tv_nsec;
#endif
	brelse(bp);

	/*
	 * Initialize the vnode from the inode, check for aliases.  In all
	 * cases re-init ip, the underlying vnode/inode may have changed.
	 */
	error = ufs_vinit(mp, lfs_specop_p, lfs_fifoop_p, &vp);
	if (error) {
		vput(vp);
		*vpp = NULL;
		return (error);
	}
#ifdef DIAGNOSTIC
	if(vp->v_type == VNON) {
		panic("lfs_vget: ino %d is type VNON! (ifmt %o)\n",
		       ip->i_number, (ip->i_ffs_mode & IFMT) >> 12);
	}
#endif
	/*
	 * Finish inode initialization now that aliasing has been resolved.
	 */
	ip->i_devvp = ump->um_devvp;
	VREF(ip->i_devvp);
	*vpp = vp;

	uvm_vnp_setsize(vp, ip->i_ffs_size);

	return (0);
}

/*
 * File handle to vnode
 *
 * Have to be really careful about stale file handles:
 * - check that the inode number is valid
 * - call lfs_vget() to get the locked inode
 * - check for an unallocated inode (i_mode == 0)
 *
 * XXX
 * use ifile to see if inode is allocated instead of reading off disk
 * what is the relationship between my generational number and the NFS
 * generational number.
 */
int
lfs_fhtovp(mp, fhp, vpp)
	struct mount *mp;
	struct fid *fhp;
	struct vnode **vpp;
{
	struct ufid *ufhp;

	ufhp = (struct ufid *)fhp;
	if (ufhp->ufid_ino < ROOTINO)
		return (ESTALE);
	return (ufs_fhtovp(mp, ufhp, vpp));
}

/*
 * Vnode pointer to File handle
 */
/* ARGSUSED */
int
lfs_vptofh(vp, fhp)
	struct vnode *vp;
	struct fid *fhp;
{
	struct inode *ip;
	struct ufid *ufhp;

	ip = VTOI(vp);
	ufhp = (struct ufid *)fhp;
	ufhp->ufid_len = sizeof(struct ufid);
	ufhp->ufid_ino = ip->i_number;
	ufhp->ufid_gen = ip->i_ffs_gen;
	return (0);
}

int
lfs_sysctl(name, namelen, oldp, oldlenp, newp, newlen, p)
	int *name;
	u_int namelen;
	void *oldp;
	size_t *oldlenp;
	void *newp;
	size_t newlen;
	struct proc *p;
{
	extern int lfs_writeindir, lfs_dostats, lfs_clean_vnhead;
	extern struct lfs_stats lfs_stats;
	int error;

	/* all sysctl names at this level are terminal */
	if (namelen != 1)
		return (ENOTDIR);

	switch (name[0]) {
	case LFS_WRITEINDIR:
		return (sysctl_int(oldp, oldlenp, newp, newlen,
				   &lfs_writeindir));
	case LFS_CLEAN_VNHEAD:
		return (sysctl_int(oldp, oldlenp, newp, newlen,
				   &lfs_clean_vnhead));
	case LFS_DOSTATS:
		if((error = sysctl_int(oldp, oldlenp, newp, newlen,
				       &lfs_dostats)))
			return error;
		if(lfs_dostats == 0)
			memset(&lfs_stats,0,sizeof(lfs_stats));
		return 0;
	case LFS_STATS:
		return (sysctl_rdstruct(oldp, oldlenp, newp,
					&lfs_stats, sizeof(lfs_stats)));
	default:
		return (EOPNOTSUPP);
	}
	/* NOTREACHED */
}
