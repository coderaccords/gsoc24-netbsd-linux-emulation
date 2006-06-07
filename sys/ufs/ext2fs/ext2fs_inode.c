/*	$NetBSD: ext2fs_inode.c,v 1.55 2006/06/07 22:34:18 kardel Exp $	*/

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
 *	@(#)ffs_inode.c	8.8 (Berkeley) 10/19/94
 * Modified for ext2fs by Manuel Bouyer.
 */

/*
 * Copyright (c) 1997 Manuel Bouyer.
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
 *	This product includes software developed by Manuel Bouyer.
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
 *	@(#)ffs_inode.c	8.8 (Berkeley) 10/19/94
 * Modified for ext2fs by Manuel Bouyer.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: ext2fs_inode.c,v 1.55 2006/06/07 22:34:18 kardel Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mount.h>
#include <sys/proc.h>
#include <sys/file.h>
#include <sys/buf.h>
#include <sys/vnode.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/trace.h>
#include <sys/resourcevar.h>
#include <sys/kauth.h>

#include <ufs/ufs/inode.h>
#include <ufs/ufs/ufsmount.h>
#include <ufs/ufs/ufs_extern.h>

#include <ufs/ext2fs/ext2fs.h>
#include <ufs/ext2fs/ext2fs_extern.h>

extern int prtactive;

static int ext2fs_indirtrunc(struct inode *, daddr_t, daddr_t,
				  daddr_t, int, long *);

/*
 * Get the size of an inode.
 */
u_int64_t
ext2fs_size(struct inode *ip)
{
	u_int64_t size = ip->i_e2fs_size;

	if ((ip->i_e2fs_mode & IFMT) == IFREG)
		size |= (u_int64_t)ip->i_e2fs_dacl << 32;
	return size;
}

int
ext2fs_setsize(struct inode *ip, u_int64_t size)
{
	if ((ip->i_e2fs_mode & IFMT) == IFREG ||
	    ip->i_e2fs_mode == 0) {
		ip->i_e2fs_dacl = size >> 32;
		if (size >= 0x80000000U) {
			struct m_ext2fs *fs = ip->i_e2fs;

			if (fs->e2fs.e2fs_rev <= E2FS_REV0) {
				/* Linux automagically upgrades to REV1 here! */
				return EFBIG;
			}
			if (!(fs->e2fs.e2fs_features_rocompat
			    & EXT2F_ROCOMPAT_LARGEFILE)) {
				fs->e2fs.e2fs_features_rocompat |=
				    EXT2F_ROCOMPAT_LARGEFILE;
				fs->e2fs_fmod = 1;
			}
		}
	} else if (size >= 0x80000000U)
		return EFBIG;

	ip->i_e2fs_size = size;

	return 0;
}

/*
 * Last reference to an inode.  If necessary, write or delete it.
 */
int
ext2fs_inactive(void *v)
{
	struct vop_inactive_args /* {
		struct vnode *a_vp;
		struct lwp *a_l;
	} */ *ap = v;
	struct vnode *vp = ap->a_vp;
	struct inode *ip = VTOI(vp);
	struct mount *mp;
	struct lwp *l = ap->a_l;
	int error = 0;

	if (prtactive && vp->v_usecount != 0)
		vprint("ext2fs_inactive: pushing active", vp);
	/* Get rid of inodes related to stale file handles. */
	if (ip->i_e2fs_mode == 0 || ip->i_e2fs_dtime != 0)
		goto out;

	error = 0;
	if (ip->i_e2fs_nlink == 0 && (vp->v_mount->mnt_flag & MNT_RDONLY) == 0) {
		vn_start_write(vp, &mp, V_WAIT | V_LOWER);
		if (ext2fs_size(ip) != 0) {
			error = ext2fs_truncate(vp, (off_t)0, 0, NOCRED, NULL);
		}
		ip->i_e2fs_dtime = time_uptime;
		ip->i_flag |= IN_CHANGE | IN_UPDATE;
		ext2fs_vfree(vp, ip->i_number, ip->i_e2fs_mode);
		vn_finished_write(mp, V_LOWER);
	}
	if (ip->i_flag & (IN_CHANGE | IN_UPDATE | IN_MODIFIED)) {
		vn_start_write(vp, &mp, V_WAIT | V_LOWER);
		ext2fs_update(vp, NULL, NULL, 0);
		vn_finished_write(mp, V_LOWER);
	}
out:
	VOP_UNLOCK(vp, 0);
	/*
	 * If we are done with the inode, reclaim it
	 * so that it can be reused immediately.
	 */
	if (ip->i_e2fs_dtime != 0)
		vrecycle(vp, NULL, l);
	return (error);
}


/*
 * Update the access, modified, and inode change times as specified by the
 * IACCESS, IUPDATE, and ICHANGE flags respectively. The IMODIFIED flag is
 * used to specify that the inode needs to be updated but that the times have
 * already been set. The access and modified times are taken from the second
 * and third parameters; the inode change time is always taken from the current
 * time. If UPDATE_WAIT or UPDATE_DIROP is set, then wait for the disk
 * write of the inode to complete.
 */
int
ext2fs_update(struct vnode *vp, const struct timespec *acc,
    const struct timespec *mod, int updflags)
{
	struct m_ext2fs *fs;
	struct buf *bp;
	struct inode *ip;
	int error;
	caddr_t cp;
	int flags;

	if (vp->v_mount->mnt_flag & MNT_RDONLY)
		return (0);
	ip = VTOI(vp);
	EXT2FS_ITIMES(ip, acc, mod, NULL);
	if (updflags & UPDATE_CLOSE)
		flags = ip->i_flag & (IN_MODIFIED | IN_ACCESSED);
	else
		flags = ip->i_flag & IN_MODIFIED;
	if (flags == 0)
		return (0);
	fs = ip->i_e2fs;

	error = bread(ip->i_devvp,
			  fsbtodb(fs, ino_to_fsba(fs, ip->i_number)),
			  (int)fs->e2fs_bsize, NOCRED, &bp);
	if (error) {
		brelse(bp);
		return (error);
	}
	ip->i_flag &= ~(IN_MODIFIED | IN_ACCESSED);
	cp = (caddr_t)bp->b_data +
	    (ino_to_fsbo(fs, ip->i_number) * EXT2_DINODE_SIZE);
	e2fs_isave(ip->i_din.e2fs_din, (struct ext2fs_dinode *)cp);
	if ((updflags & (UPDATE_WAIT|UPDATE_DIROP)) != 0 &&
	    (flags & IN_MODIFIED) != 0 &&
	    (vp->v_mount->mnt_flag & MNT_ASYNC) == 0)
		return (bwrite(bp));
	else {
		bdwrite(bp);
		return (0);
	}
}

#define	SINGLE	0	/* index of single indirect block */
#define	DOUBLE	1	/* index of double indirect block */
#define	TRIPLE	2	/* index of triple indirect block */
/*
 * Truncate the inode oip to at most length size, freeing the
 * disk blocks.
 */
int
ext2fs_truncate(struct vnode *ovp, off_t length, int ioflag,
    kauth_cred_t cred, struct proc *p)
{
	daddr_t lastblock;
	struct inode *oip = VTOI(ovp);
	daddr_t bn, lastiblock[NIADDR], indir_lbn[NIADDR];
	/* XXX ondisk32 */
	int32_t oldblks[NDADDR + NIADDR], newblks[NDADDR + NIADDR];
	struct m_ext2fs *fs;
	int offset, size, level;
	long count, blocksreleased = 0;
	int i, nblocks;
	int error, allerror = 0;
	off_t osize;
	int sync;
	struct ufsmount *ump = oip->i_ump;

	if (ovp->v_type == VCHR || ovp->v_type == VBLK ||
	    ovp->v_type == VFIFO || ovp->v_type == VSOCK) {
		return 0;
	}

	if (length < 0)
		return (EINVAL);

	if (ovp->v_type == VLNK &&
	    (ext2fs_size(oip) < ump->um_maxsymlinklen ||
	     (ump->um_maxsymlinklen == 0 && oip->i_e2fs_nblock == 0))) {
		KDASSERT(length == 0);
		memset((char *)&oip->i_din.e2fs_din->e2di_shortlink, 0,
			(u_int)ext2fs_size(oip));
		(void)ext2fs_setsize(oip, 0);
		oip->i_flag |= IN_CHANGE | IN_UPDATE;
		return (ext2fs_update(ovp, NULL, NULL, 0));
	}
	if (ext2fs_size(oip) == length) {
		oip->i_flag |= IN_CHANGE | IN_UPDATE;
		return (ext2fs_update(ovp, NULL, NULL, 0));
	}
	fs = oip->i_e2fs;
	if (length > ump->um_maxfilesize)
		return (EFBIG);

	osize = ext2fs_size(oip);

	/*
	 * Lengthen the size of the file. We must ensure that the
	 * last byte of the file is allocated. Since the smallest
	 * value of osize is 0, length will be at least 1.
	 */
	if (osize < length) {
		error = ufs_balloc_range(ovp, length - 1, 1, cred,
		    ioflag & IO_SYNC ? B_SYNC : 0);
		if (error) {
			(void) ext2fs_truncate(ovp, osize, ioflag & IO_SYNC,
			    cred, p);
			return (error);
		}
		uvm_vnp_setsize(ovp, length);
		oip->i_flag |= IN_CHANGE | IN_UPDATE;
		KASSERT(error  || ovp->v_size == ext2fs_size(oip));
		return (ext2fs_update(ovp, NULL, NULL, 0));
	}
	/*
	 * Shorten the size of the file. If the file is not being
	 * truncated to a block boundry, the contents of the
	 * partial block following the end of the file must be
	 * zero'ed in case it ever become accessible again because
	 * of subsequent file growth.
	 */
	offset = blkoff(fs, length);
	if (offset != 0) {
		size = fs->e2fs_bsize;

		/* XXXUBC we should handle more than just VREG */
		uvm_vnp_zerorange(ovp, length, size - offset);
	}
	(void)ext2fs_setsize(oip, length);
	uvm_vnp_setsize(ovp, length);
	/*
	 * Calculate index into inode's block list of
	 * last direct and indirect blocks (if any)
	 * which we want to keep.  Lastblock is -1 when
	 * the file is truncated to 0.
	 */
	lastblock = lblkno(fs, length + fs->e2fs_bsize - 1) - 1;
	lastiblock[SINGLE] = lastblock - NDADDR;
	lastiblock[DOUBLE] = lastiblock[SINGLE] - NINDIR(fs);
	lastiblock[TRIPLE] = lastiblock[DOUBLE] - NINDIR(fs) * NINDIR(fs);
	nblocks = btodb(fs->e2fs_bsize);
	/*
	 * Update file and block pointers on disk before we start freeing
	 * blocks.  If we crash before free'ing blocks below, the blocks
	 * will be returned to the free list.  lastiblock values are also
	 * normalized to -1 for calls to ext2fs_indirtrunc below.
	 */
	memcpy((caddr_t)oldblks, (caddr_t)&oip->i_e2fs_blocks[0], sizeof oldblks);
	sync = 0;
	for (level = TRIPLE; level >= SINGLE; level--) {
		if (lastiblock[level] < 0 && oldblks[NDADDR + level] != 0) {
			sync = 1;
			oip->i_e2fs_blocks[NDADDR + level] = 0;
			lastiblock[level] = -1;
		}
	}
	for (i = 0; i < NDADDR; i++) {
		if (i > lastblock && oldblks[i] != 0) {
			sync = 1;
			oip->i_e2fs_blocks[i] = 0;
		}
	}
	oip->i_flag |= IN_CHANGE | IN_UPDATE;
	if (sync) {
		error = ext2fs_update(ovp, NULL, NULL, UPDATE_WAIT);
		if (error && !allerror)
			allerror = error;
	}

	/*
	 * Having written the new inode to disk, save its new configuration
	 * and put back the old block pointers long enough to process them.
	 * Note that we save the new block configuration so we can check it
	 * when we are done.
	 */
	memcpy((caddr_t)newblks, (caddr_t)&oip->i_e2fs_blocks[0], sizeof newblks);
	memcpy((caddr_t)&oip->i_e2fs_blocks[0], (caddr_t)oldblks, sizeof oldblks);

	(void)ext2fs_setsize(oip, osize);
	error = vtruncbuf(ovp, lastblock + 1, 0, 0);
	if (error && !allerror)
		allerror = error;

	/*
	 * Indirect blocks first.
	 */
	indir_lbn[SINGLE] = -NDADDR;
	indir_lbn[DOUBLE] = indir_lbn[SINGLE] - NINDIR(fs) -1;
	indir_lbn[TRIPLE] = indir_lbn[DOUBLE] - NINDIR(fs) * NINDIR(fs) - 1;
	for (level = TRIPLE; level >= SINGLE; level--) {
		/* XXX ondisk32 */
		bn = fs2h32(oip->i_e2fs_blocks[NDADDR + level]);
		if (bn != 0) {
			error = ext2fs_indirtrunc(oip, indir_lbn[level],
			    fsbtodb(fs, bn), lastiblock[level], level, &count);
			if (error)
				allerror = error;
			blocksreleased += count;
			if (lastiblock[level] < 0) {
				oip->i_e2fs_blocks[NDADDR + level] = 0;
				ext2fs_blkfree(oip, bn);
				blocksreleased += nblocks;
			}
		}
		if (lastiblock[level] >= 0)
			goto done;
	}

	/*
	 * All whole direct blocks or frags.
	 */
	for (i = NDADDR - 1; i > lastblock; i--) {
		/* XXX ondisk32 */
		bn = fs2h32(oip->i_e2fs_blocks[i]);
		if (bn == 0)
			continue;
		oip->i_e2fs_blocks[i] = 0;
		ext2fs_blkfree(oip, bn);
		blocksreleased += btodb(fs->e2fs_bsize);
	}

done:
#ifdef DIAGNOSTIC
	for (level = SINGLE; level <= TRIPLE; level++)
		if (newblks[NDADDR + level] !=
		    oip->i_e2fs_blocks[NDADDR + level])
			panic("ext2fs_truncate1");
	for (i = 0; i < NDADDR; i++)
		if (newblks[i] != oip->i_e2fs_blocks[i])
			panic("ext2fs_truncate2");
	if (length == 0 &&
	    (!LIST_EMPTY(&ovp->v_cleanblkhd) ||
	     !LIST_EMPTY(&ovp->v_dirtyblkhd)))
		panic("ext2fs_truncate3");
#endif /* DIAGNOSTIC */
	/*
	 * Put back the real size.
	 */
	(void)ext2fs_setsize(oip, length);
	oip->i_e2fs_nblock -= blocksreleased;
	oip->i_flag |= IN_CHANGE;
	KASSERT(ovp->v_type != VREG || ovp->v_size == ext2fs_size(oip));
	return (allerror);
}

/*
 * Release blocks associated with the inode ip and stored in the indirect
 * block bn.  Blocks are free'd in LIFO order up to (but not including)
 * lastbn.  If level is greater than SINGLE, the block is an indirect block
 * and recursive calls to indirtrunc must be used to cleanse other indirect
 * blocks.
 *
 * NB: triple indirect blocks are untested.
 */
static int
ext2fs_indirtrunc(struct inode *ip, daddr_t lbn, daddr_t dbn, daddr_t lastbn,
		int level, long *countp)
{
	int i;
	struct buf *bp;
	struct m_ext2fs *fs = ip->i_e2fs;
	int32_t *bap;	/* XXX ondisk32 */
	struct vnode *vp;
	daddr_t nb, nlbn, last;
	int32_t *copy = NULL;	/* XXX ondisk32 */
	long blkcount, factor;
	int nblocks, blocksreleased = 0;
	int error = 0, allerror = 0;

	/*
	 * Calculate index in current block of last
	 * block to be kept.  -1 indicates the entire
	 * block so we need not calculate the index.
	 */
	factor = 1;
	for (i = SINGLE; i < level; i++)
		factor *= NINDIR(fs);
	last = lastbn;
	if (lastbn > 0)
		last /= factor;
	nblocks = btodb(fs->e2fs_bsize);
	/*
	 * Get buffer of block pointers, zero those entries corresponding
	 * to blocks to be free'd, and update on disk copy first.  Since
	 * double(triple) indirect before single(double) indirect, calls
	 * to bmap on these blocks will fail.  However, we already have
	 * the on disk address, so we have to set the b_blkno field
	 * explicitly instead of letting bread do everything for us.
	 */
	vp = ITOV(ip);
	bp = getblk(vp, lbn, (int)fs->e2fs_bsize, 0, 0);
	if (bp->b_flags & (B_DONE | B_DELWRI)) {
		/* Braces must be here in case trace evaluates to nothing. */
		trace(TR_BREADHIT, pack(vp, fs->e2fs_bsize), lbn);
	} else {
		trace(TR_BREADMISS, pack(vp, fs->e2fs_bsize), lbn);
		curproc->p_stats->p_ru.ru_inblock++;	/* pay for read */
		bp->b_flags |= B_READ;
		if (bp->b_bcount > bp->b_bufsize)
			panic("ext2fs_indirtrunc: bad buffer size");
		bp->b_blkno = dbn;
		VOP_STRATEGY(vp, bp);
		error = biowait(bp);
	}
	if (error) {
		brelse(bp);
		*countp = 0;
		return (error);
	}

	bap = (int32_t *)bp->b_data;	/* XXX ondisk32 */
	if (lastbn >= 0) {
		/* XXX ondisk32 */
		copy = malloc(fs->e2fs_bsize, M_TEMP, M_WAITOK);
		memcpy((caddr_t)copy, (caddr_t)bap, (u_int)fs->e2fs_bsize);
		memset((caddr_t)&bap[last + 1], 0,
			(u_int)(NINDIR(fs) - (last + 1)) * sizeof (u_int32_t));
		error = bwrite(bp);
		if (error)
			allerror = error;
		bap = copy;
	}

	/*
	 * Recursively free totally unused blocks.
	 */
	for (i = NINDIR(fs) - 1,
		nlbn = lbn + 1 - i * factor; i > last;
		i--, nlbn += factor) {
		/* XXX ondisk32 */
		nb = fs2h32(bap[i]);
		if (nb == 0)
			continue;
		if (level > SINGLE) {
			error = ext2fs_indirtrunc(ip, nlbn, fsbtodb(fs, nb),
						   (daddr_t)-1, level - 1,
						   &blkcount);
			if (error)
				allerror = error;
			blocksreleased += blkcount;
		}
		ext2fs_blkfree(ip, nb);
		blocksreleased += nblocks;
	}

	/*
	 * Recursively free last partial block.
	 */
	if (level > SINGLE && lastbn >= 0) {
		last = lastbn % factor;
		/* XXX ondisk32 */
		nb = fs2h32(bap[i]);
		if (nb != 0) {
			error = ext2fs_indirtrunc(ip, nlbn, fsbtodb(fs, nb),
						   last, level - 1, &blkcount);
			if (error)
				allerror = error;
			blocksreleased += blkcount;
		}
	}

	if (copy != NULL) {
		FREE(copy, M_TEMP);
	} else {
		bp->b_flags |= B_INVAL;
		brelse(bp);
	}

	*countp = blocksreleased;
	return (allerror);
}
