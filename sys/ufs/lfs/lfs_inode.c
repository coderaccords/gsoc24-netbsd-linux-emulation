/*	$NetBSD: lfs_inode.c,v 1.67 2003/02/28 07:37:56 perseant Exp $	*/

/*-
 * Copyright (c) 1999, 2000, 2001, 2002, 2003 The NetBSD Foundation, Inc.
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
/*
 * Copyright (c) 1986, 1989, 1991, 1993
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
 *	@(#)lfs_inode.c	8.9 (Berkeley) 5/8/95
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: lfs_inode.c,v 1.67 2003/02/28 07:37:56 perseant Exp $");

#if defined(_KERNEL_OPT)
#include "opt_quota.h"
#endif

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

#include <ufs/ufs/quota.h>
#include <ufs/ufs/inode.h>
#include <ufs/ufs/ufsmount.h>
#include <ufs/ufs/ufs_extern.h>

#include <ufs/lfs/lfs.h>
#include <ufs/lfs/lfs_extern.h>

extern int locked_queue_count;
extern long locked_queue_bytes;

static int lfs_update_seguse(struct lfs *, long, size_t);
static int lfs_indirtrunc (struct inode *, daddr_t, daddr_t,
			   daddr_t, int, long *, long *, long *, size_t *,
			   struct proc *);
static int lfs_blkfree (struct lfs *, daddr_t, size_t, long *, size_t *);
static int lfs_vtruncbuf(struct vnode *, daddr_t, int, int);

/* Search a block for a specific dinode. */
struct dinode *
lfs_ifind(struct lfs *fs, ino_t ino, struct buf *bp)
{
	struct dinode *dip = (struct dinode *)bp->b_data;
	struct dinode *ldip, *fin;
	
#ifdef LFS_IFILE_FRAG_ADDRESSING
	if (fs->lfs_version == 1)
		fin = dip + INOPB(fs);
	else
		fin = dip + INOPF(fs);
#else
	fin = dip + INOPB(fs);
#endif

	/*
	 * Read the inode block backwards, since later versions of the
	 * inode will supercede earlier ones.  Though it is unlikely, it is
	 * possible that the same inode will appear in the same inode block.
	 */
	for (ldip = fin - 1; ldip >= dip; --ldip)
		if (ldip->di_inumber == ino)
			return (ldip);

	printf("searched %d entries\n", (int)(fin - dip));
	printf("offset is 0x%x (seg %d)\n", fs->lfs_offset,
	       dtosn(fs, fs->lfs_offset));
	printf("block is 0x%llx (seg %lld)\n",
	       (unsigned long long)dbtofsb(fs, bp->b_blkno),
	       (long long)dtosn(fs, dbtofsb(fs, bp->b_blkno)));

	return NULL;
}

int
lfs_update(void *v)
{
	struct vop_update_args /* {
				  struct vnode *a_vp;
				  struct timespec *a_access;
				  struct timespec *a_modify;
				  int a_flags;
				  } */ *ap = v;
	struct inode *ip;
	struct vnode *vp = ap->a_vp;
	struct timespec ts;
	struct lfs *fs = VFSTOUFS(vp->v_mount)->um_lfs;
	int s;
	
	if (vp->v_mount->mnt_flag & MNT_RDONLY)
		return (0);
	ip = VTOI(vp);

	/*
	 * If we are called from vinvalbuf, and the file's blocks have
	 * already been scheduled for writing, but the writes have not
	 * yet completed, lfs_vflush will not be called, and vinvalbuf
	 * will cause a panic.	So, we must wait until any pending write
	 * for our inode completes, if we are called with UPDATE_WAIT set.
	 */
	s = splbio();
	while ((ap->a_flags & (UPDATE_WAIT|UPDATE_DIROP)) == UPDATE_WAIT &&
	    WRITEINPROG(vp)) {
#ifdef DEBUG_LFS
		printf("lfs_update: sleeping on inode %d (in-progress)\n",
		       ip->i_number);
#endif
		tsleep(vp, (PRIBIO+1), "lfs_update", 0);
	}
	splx(s);
	TIMEVAL_TO_TIMESPEC(&time, &ts);
	LFS_ITIMES(ip,
		   ap->a_access ? ap->a_access : &ts,
		   ap->a_modify ? ap->a_modify : &ts, &ts);
	if ((ip->i_flag & (IN_MODIFIED | IN_ACCESSED | IN_CLEANING)) == 0) {
		return (0);
	}
	
	/* If sync, push back the vnode and any dirty blocks it may have. */
	if ((ap->a_flags & (UPDATE_WAIT|UPDATE_DIROP)) == UPDATE_WAIT) {
		/* Avoid flushing VDIROP. */
		++fs->lfs_diropwait;
		while (vp->v_flag & VDIROP) {
#ifdef DEBUG_LFS
			printf("lfs_update: sleeping on inode %d (dirops)\n",
			       ip->i_number);
			printf("lfs_update: vflags 0x%x, iflags 0x%x\n",
				vp->v_flag, ip->i_flag);
#endif
			if (fs->lfs_dirops == 0)
				lfs_flush_fs(fs, SEGM_SYNC);
			else
				tsleep(&fs->lfs_writer, PRIBIO+1, "lfs_fsync",
				       0);
			/* XXX KS - by falling out here, are we writing the vn
			twice? */
		}
		--fs->lfs_diropwait;
		return lfs_vflush(vp);
	}
	return 0;
}

#define	SINGLE	0	/* index of single indirect block */
#define	DOUBLE	1	/* index of double indirect block */
#define	TRIPLE	2	/* index of triple indirect block */
/*
 * Truncate the inode oip to at most length size, freeing the
 * disk blocks.
 */
/* VOP_BWRITE 1 + NIADDR + VOP_BALLOC == 2 + 2*NIADDR times */

int
lfs_truncate(void *v)
{
	struct vop_truncate_args /* {
		struct vnode *a_vp;
		off_t a_length;
		int a_flags;
		struct ucred *a_cred;
		struct proc *a_p;
	} */ *ap = v;
	struct vnode *ovp = ap->a_vp;
#ifdef LFS_UBC
	struct genfs_node *gp = VTOG(ovp);
#endif
	daddr_t lastblock;
	struct inode *oip;
	daddr_t bn, lbn, lastiblock[NIADDR], indir_lbn[NIADDR];
	/* XXX ondisk32 */
	int32_t newblks[NDADDR + NIADDR];
	off_t length = ap->a_length;
	struct lfs *fs;
	struct buf *bp;
	int offset, size, level;
	long count, rcount, nblocks, blocksreleased = 0, real_released = 0;
	int i;
	int aflags, error, allerror = 0;
	off_t osize;
	long lastseg;
	size_t bc;
	int obufsize, odb;
	int usepc, needunlock;

	if (length < 0)
		return (EINVAL);
	oip = VTOI(ovp);

	/*
	 * Just return and not update modification times.
	 */
	if (oip->i_ffs_size == length)
		return (0);

	if (ovp->v_type == VLNK &&
	    (oip->i_ffs_size < ovp->v_mount->mnt_maxsymlinklen ||
	     (ovp->v_mount->mnt_maxsymlinklen == 0 &&
	      oip->i_din.ffs_din.di_blocks == 0))) {
#ifdef DIAGNOSTIC
		if (length != 0)
			panic("lfs_truncate: partial truncate of symlink");
#endif
		memset((char *)&oip->i_ffs_shortlink, 0, (u_int)oip->i_ffs_size);
		oip->i_ffs_size = 0;
		oip->i_flag |= IN_CHANGE | IN_UPDATE;
		return (VOP_UPDATE(ovp, NULL, NULL, 0));
	}
	if (oip->i_ffs_size == length) {
		oip->i_flag |= IN_CHANGE | IN_UPDATE;
		return (VOP_UPDATE(ovp, NULL, NULL, 0));
	}
#ifdef QUOTA
	if ((error = getinoquota(oip)) != 0)
		return (error);
#endif
	fs = oip->i_lfs;
	lfs_imtime(fs);
	osize = oip->i_ffs_size;
	needunlock = usepc = 0;
#ifdef LFS_UBC
	usepc = (ovp->v_type == VREG && ovp != fs->lfs_ivnode);
#endif

	/*
	 * Lengthen the size of the file. We must ensure that the
	 * last byte of the file is allocated. Since the smallest
	 * value of osize is 0, length will be at least 1.
	 */
	if (osize < length) {
		if (length > fs->lfs_maxfilesize)
			return (EFBIG);
		aflags = B_CLRBUF;
		if (ap->a_flags & IO_SYNC)
			aflags |= B_SYNC;
#ifdef LFS_UBC
		if (usepc) {
			if (lblkno(fs, osize) < NDADDR &&
			    lblkno(fs, osize) != lblkno(fs, length) &&
			    blkroundup(fs, osize) != osize) {
				error = ufs_balloc_range(ovp, osize,
							 blkroundup(fs, osize) -
							 osize, ap->a_cred,
							 aflags);
				if (error) {
					return error;
				}
				if (ap->a_flags & IO_SYNC) {
					ovp->v_size = blkroundup(fs, osize);
					simple_lock(&ovp->v_interlock);
					VOP_PUTPAGES(ovp,
					    trunc_page(osize & fs->lfs_bmask),
					    round_page(ovp->v_size),
					    PGO_CLEANIT | PGO_SYNCIO);
				}
			}
			error = ufs_balloc_range(ovp, length - 1, 1, ap->a_cred,
						 aflags);
			if (error) {
				(void) VOP_TRUNCATE(ovp, osize,
						    ap->a_flags & IO_SYNC,
				    		    ap->a_cred, ap->a_p);
				return error;
			}
			uvm_vnp_setsize(ovp, length);
			oip->i_flag |= IN_CHANGE | IN_UPDATE;
			KASSERT(ovp->v_size == oip->i_ffs_size);
			return (VOP_UPDATE(ovp, NULL, NULL, 0));
		} else {
#endif /* !LFS_UBC */
			error = lfs_reserve(fs, ovp, NULL,
			    btofsb(fs, (NIADDR + 2) << fs->lfs_bshift));
			if (error)
				return (error);
			error = VOP_BALLOC(ovp, length - 1, 1, ap->a_cred,
					   aflags, &bp);
			lfs_reserve(fs, ovp, NULL,
			    -btofsb(fs, (NIADDR + 2) << fs->lfs_bshift));
			if (error)
				return (error);
			oip->i_ffs_size = length;
			uvm_vnp_setsize(ovp, length);
			(void) VOP_BWRITE(bp);
			oip->i_flag |= IN_CHANGE | IN_UPDATE;
			return (VOP_UPDATE(ovp, NULL, NULL, 0));
#ifdef LFS_UBC
		}
#endif
	}

	if ((error = lfs_reserve(fs, ovp, NULL,
	    btofsb(fs, (2 * NIADDR + 3) << fs->lfs_bshift))) != 0)
		return (error);

	/*
	 * Shorten the size of the file. If the file is not being
	 * truncated to a block boundary, the contents of the
	 * partial block following the end of the file must be
	 * zero'ed in case it ever becomes accessible again because
	 * of subsequent file growth. Directories however are not
	 * zero'ed as they should grow back initialized to empty.
	 */
	offset = blkoff(fs, length);
	lastseg = -1;
	bc = 0;
	if (offset == 0) {
		oip->i_ffs_size = length;
	} else
#ifdef LFS_UBC
	if (!usepc)
#endif
	{
		lockmgr(&fs->lfs_fraglock, LK_SHARED, 0);
		lbn = lblkno(fs, length);
		aflags = B_CLRBUF;
		if (ap->a_flags & IO_SYNC)
			aflags |= B_SYNC;
		error = VOP_BALLOC(ovp, length - 1, 1, ap->a_cred, aflags, &bp);
		if (error) {
			lfs_reserve(fs, ovp, NULL,
			    -btofsb(fs, (2 * NIADDR + 3) << fs->lfs_bshift));
			lockmgr(&fs->lfs_fraglock, LK_RELEASE, 0);
			return (error);
		}
		obufsize = bp->b_bufsize;
		odb = btofsb(fs, bp->b_bcount);
		oip->i_ffs_size = length;
		size = blksize(fs, oip, lbn);
		if (ovp->v_type != VDIR)
			memset((char *)bp->b_data + offset, 0,
			       (u_int)(size - offset));
		allocbuf(bp, size);
		if ((bp->b_flags & (B_LOCKED | B_CALL)) == B_LOCKED)
			locked_queue_bytes -= obufsize - bp->b_bufsize;
		if (bp->b_flags & B_DELWRI)
			fs->lfs_avail += odb - btofsb(fs, size);
		(void) VOP_BWRITE(bp);
		lockmgr(&fs->lfs_fraglock, LK_RELEASE, 0);
	}
#ifdef LFS_UBC
	/*
	 * When truncating a regular file down to a non-block-aligned size,
	 * we must zero the part of last block which is past the new EOF.
	 * We must synchronously flush the zeroed pages to disk
	 * since the new pages will be invalidated as soon as we
	 * inform the VM system of the new, smaller size.
	 * We must do this before acquiring the GLOCK, since fetching
	 * the pages will acquire the GLOCK internally.
	 * So there is a window where another thread could see a whole
	 * zeroed page past EOF, but that's life.
	 */

	else { /* vp->v_type == VREG && length < osize && offset != 0 */
		voff_t eoz;

		aflags = ap->a_flags & IO_SYNC ? B_SYNC : 0;
		error = ufs_balloc_range(ovp, length - 1, 1, ap->a_cred,
			aflags);
		if (error) {
			return error;
		}
		size = blksize(fs, oip, lblkno(fs, length));
		eoz = MIN(lblktosize(fs, lblkno(fs, length)) + size, osize);
		uvm_vnp_zerorange(ovp, length, eoz - length);
		simple_lock(&ovp->v_interlock);
		error = VOP_PUTPAGES(ovp, trunc_page(length), round_page(eoz),
		    PGO_CLEANIT | PGO_DEACTIVATE | PGO_SYNCIO);
		if (error) {
			return error;
		}
	}

	lockmgr(&gp->g_glock, LK_EXCLUSIVE, NULL);
#endif

	oip->i_ffs_size = length;
	uvm_vnp_setsize(ovp, length);
	/*
	 * Calculate index into inode's block list of
	 * last direct and indirect blocks (if any)
	 * which we want to keep.  Lastblock is -1 when
	 * the file is truncated to 0.
	 */
	lastblock = lblkno(fs, length + fs->lfs_bsize - 1) - 1;
	lastiblock[SINGLE] = lastblock - NDADDR;
	lastiblock[DOUBLE] = lastiblock[SINGLE] - NINDIR(fs);
	lastiblock[TRIPLE] = lastiblock[DOUBLE] - NINDIR(fs) * NINDIR(fs);
	nblocks = btofsb(fs, fs->lfs_bsize);
	/*
	 * Record changed file and block pointers before we start
	 * freeing blocks.  lastiblock values are also normalized to -1
	 * for calls to lfs_indirtrunc below.
	 */
	memcpy((caddr_t)newblks, (caddr_t)&oip->i_ffs_db[0], sizeof newblks);
	for (level = TRIPLE; level >= SINGLE; level--)
		if (lastiblock[level] < 0) {
			newblks[NDADDR+level] = 0;
			lastiblock[level] = -1;
		}
	for (i = NDADDR - 1; i > lastblock; i--)
		newblks[i] = 0;

	oip->i_ffs_size = osize;
	error = lfs_vtruncbuf(ovp, lastblock + 1, 0, 0);
	if (error && !allerror)
		allerror = error;

	/*
	 * Indirect blocks first.
	 */
	indir_lbn[SINGLE] = -NDADDR;
	indir_lbn[DOUBLE] = indir_lbn[SINGLE] - NINDIR(fs) - 1;
	indir_lbn[TRIPLE] = indir_lbn[DOUBLE] - NINDIR(fs) * NINDIR(fs) - 1;
	for (level = TRIPLE; level >= SINGLE; level--) {
		bn = oip->i_ffs_ib[level];
		if (bn != 0) {
			error = lfs_indirtrunc(oip, indir_lbn[level],
					       bn, lastiblock[level],
					       level, &count, &rcount,
					       &lastseg, &bc, ap->a_p);
			if (error)
				allerror = error;
			real_released += rcount;
			blocksreleased += count;
			if (lastiblock[level] < 0) {
				if (oip->i_ffs_ib[level] > 0)
					real_released += nblocks;
				blocksreleased += nblocks;
				oip->i_ffs_ib[level] = 0;
				lfs_blkfree(fs, bn, fs->lfs_bsize, &lastseg, &bc);
			}
		}
		if (lastiblock[level] >= 0)
			goto done;
	}

	if (!usepc) {
		lockmgr(&fs->lfs_fraglock, LK_SHARED, 0);
		needunlock = 1;
	}
	/*
	 * All whole direct blocks or frags.
	 */
	for (i = NDADDR - 1; i > lastblock; i--) {
		long bsize, obsize;

		bn = oip->i_ffs_db[i];
		if (bn == 0)
			continue;
		bsize = blksize(fs, oip, i);
		if (oip->i_ffs_db[i] > 0) {
			/* Check for fragment size changes */
			obsize = oip->i_lfs_fragsize[i];
			real_released += btofsb(fs, obsize);
			oip->i_lfs_fragsize[i] = 0;
		} else
			obsize = 0;
		blocksreleased += btofsb(fs, bsize);
		oip->i_ffs_db[i] = 0;
		lfs_blkfree(fs, bn, obsize, &lastseg, &bc);
	}
	if (lastblock < 0)
		goto done;

	/*
	 * Finally, look for a change in size of the
	 * last direct block; release any frags.
	 */
	bn = oip->i_ffs_db[lastblock];
	if (bn != 0) {
		long oldspace, newspace, olddspace;

		/*
		 * Calculate amount of space we're giving
		 * back as old block size minus new block size.
		 */
		oldspace = blksize(fs, oip, lastblock);
		olddspace = oip->i_lfs_fragsize[lastblock];

		oip->i_ffs_size = length;
		newspace = blksize(fs, oip, lastblock);
		if (newspace == 0)
			panic("itrunc: newspace");
		if (oldspace - newspace > 0) {
			blocksreleased += btofsb(fs, oldspace - newspace);
		}
#if 0
		if (bn > 0 && olddspace - newspace > 0) {
			/* No segment accounting here, just vnode */
			real_released += btofsb(fs, olddspace - newspace);
		}
#endif
	}

done:
	/* Finish segment accounting corrections */
	lfs_update_seguse(fs, lastseg, bc);
#ifdef DIAGNOSTIC
	for (level = SINGLE; level <= TRIPLE; level++)
		if ((newblks[NDADDR + level] == 0) !=
		    (oip->i_ffs_ib[level]) == 0) {
			panic("lfs itrunc1");
		}
	for (i = 0; i < NDADDR; i++)
		if ((newblks[i] == 0) != (oip->i_ffs_db[i] == 0)) {
			panic("lfs itrunc2");
		}
	if (length == 0 &&
	    (!LIST_EMPTY(&ovp->v_cleanblkhd) || !LIST_EMPTY(&ovp->v_dirtyblkhd)))
		panic("lfs itrunc3");
#endif /* DIAGNOSTIC */
	/*
	 * Put back the real size.
	 */
	oip->i_ffs_size = length;
	oip->i_lfs_effnblks -= blocksreleased;
	oip->i_ffs_blocks -= real_released;
	fs->lfs_bfree += blocksreleased;
#ifdef DIAGNOSTIC
	if (oip->i_ffs_size == 0 && oip->i_ffs_blocks != 0) {
		printf("lfs_truncate: truncate to 0 but %d blocks on inode\n",
		       oip->i_ffs_blocks);
		panic("lfs_truncate: persistent blocks");
	}
#endif
	oip->i_flag |= IN_CHANGE;
#ifdef QUOTA
	(void) chkdq(oip, -blocksreleased, NOCRED, 0);
#endif
	lfs_reserve(fs, ovp, NULL,
	    -btofsb(fs, (2 * NIADDR + 3) << fs->lfs_bshift));
	if (needunlock)
		lockmgr(&fs->lfs_fraglock, LK_RELEASE, 0);
#ifdef LFS_UBC
	lockmgr(&gp->g_glock, LK_RELEASE, NULL);
#endif
	return (allerror);
}

/* Update segment usage information when removing a block. */
static int
lfs_blkfree(struct lfs *fs, daddr_t daddr, size_t bsize, long *lastseg,
	    size_t *num)
{
	long seg;
	int error = 0;

	bsize = fragroundup(fs, bsize);
	if (daddr > 0) {
		if (*lastseg != (seg = dtosn(fs, daddr))) {
			error = lfs_update_seguse(fs, *lastseg, *num);
			*num = bsize;
			*lastseg = seg;
		} else
			*num += bsize;
	}
	return error;
}

/* Finish the accounting updates for a segment. */
static int
lfs_update_seguse(struct lfs *fs, long lastseg, size_t num)
{
	SEGUSE *sup;
	struct buf *bp;

	if (lastseg < 0 || num == 0)
		return 0;
	
	LFS_SEGENTRY(sup, fs, lastseg, bp);
	if (num > sup->su_nbytes) {
		printf("lfs_truncate: segment %ld short by %ld\n",
		       lastseg, (long)num - sup->su_nbytes);
		panic("lfs_truncate: negative bytes");
		sup->su_nbytes = num;
	}
	sup->su_nbytes -= num;
	LFS_WRITESEGENTRY(sup, fs, lastseg, bp);

	return 0;
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
lfs_indirtrunc(struct inode *ip, daddr_t lbn, daddr_t dbn,
	       daddr_t lastbn, int level, long *countp,
	       long *rcountp, long *lastsegp, size_t *bcp, struct proc *p)
{
	int i;
	struct buf *bp;
	struct lfs *fs = ip->i_lfs;
	int32_t *bap;	/* XXX ondisk32 */
	struct vnode *vp;
	daddr_t nb, nlbn, last;
	int32_t *copy = NULL;	/* XXX ondisk32 */
	long blkcount, rblkcount, factor;
	int nblocks, blocksreleased = 0, real_released = 0;
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
	nblocks = btofsb(fs, fs->lfs_bsize);
	/*
	 * Get buffer of block pointers, zero those entries corresponding
	 * to blocks to be free'd, and update on disk copy first.  Since
	 * double(triple) indirect before single(double) indirect, calls
	 * to bmap on these blocks will fail.  However, we already have
	 * the on disk address, so we have to set the b_blkno field
	 * explicitly instead of letting bread do everything for us.
	 */
	vp = ITOV(ip);
	bp = getblk(vp, lbn, (int)fs->lfs_bsize, 0, 0);
	if (bp->b_flags & (B_DONE | B_DELWRI)) {
		/* Braces must be here in case trace evaluates to nothing. */
		trace(TR_BREADHIT, pack(vp, fs->lfs_bsize), lbn);
	} else {
		trace(TR_BREADMISS, pack(vp, fs->lfs_bsize), lbn);
		p->p_stats->p_ru.ru_inblock++;	/* pay for read */
		bp->b_flags |= B_READ;
		if (bp->b_bcount > bp->b_bufsize)
			panic("lfs_indirtrunc: bad buffer size");
		bp->b_blkno = fsbtodb(fs, dbn);
		VOP_STRATEGY(bp);
		error = biowait(bp);
	}
	if (error) {
		brelse(bp);
		*countp = *rcountp = 0;
		return (error);
	}

	bap = (int32_t *)bp->b_data;	/* XXX ondisk32 */
	if (lastbn >= 0) {
		MALLOC(copy, int32_t *, fs->lfs_bsize, M_TEMP, M_WAITOK);
		memcpy((caddr_t)copy, (caddr_t)bap, (u_int)fs->lfs_bsize);
		memset((caddr_t)&bap[last + 1], 0,
		/* XXX ondisk32 */
		  (u_int)(NINDIR(fs) - (last + 1)) * sizeof (int32_t));
		error = VOP_BWRITE(bp);
		if (error)
			allerror = error;
		bap = copy;
	}

	/*
	 * Recursively free totally unused blocks.
	 */
	for (i = NINDIR(fs) - 1, nlbn = lbn + 1 - i * factor; i > last;
	    i--, nlbn += factor) {
		nb = bap[i];
		if (nb == 0)
			continue;
		if (level > SINGLE) {
			error = lfs_indirtrunc(ip, nlbn, nb,
					       (daddr_t)-1, level - 1,
					       &blkcount, &rblkcount,
					       lastsegp, bcp, p);
			if (error)
				allerror = error;
			blocksreleased += blkcount;
			real_released += rblkcount;
		}
		lfs_blkfree(fs, nb, fs->lfs_bsize, lastsegp, bcp);
		if (bap[i] > 0)
			real_released += nblocks;
		blocksreleased += nblocks;
	}

	/*
	 * Recursively free last partial block.
	 */
	if (level > SINGLE && lastbn >= 0) {
		last = lastbn % factor;
		nb = bap[i];
		if (nb != 0) {
			error = lfs_indirtrunc(ip, nlbn, nb,
					       last, level - 1, &blkcount,
					       &rblkcount, lastsegp, bcp, p);
			if (error)
				allerror = error;
			real_released += rblkcount;
			blocksreleased += blkcount;
		}
	}

	if (copy != NULL) {
		FREE(copy, M_TEMP);
	} else {
		if (bp->b_flags & B_DELWRI) {
			LFS_UNLOCK_BUF(bp);
			fs->lfs_avail += btofsb(fs, bp->b_bcount);
			wakeup(&fs->lfs_avail);
		}
		bp->b_flags |= B_INVAL;
		brelse(bp);
	}

	*countp = blocksreleased;
	*rcountp = real_released;
	return (allerror);
}

/*
 * Destroy any in core blocks past the truncation length.
 * Inlined from vtruncbuf, so that lfs_avail could be updated.
 * We take the fraglock to prevent cleaning from occurring while we are
 * invalidating blocks.
 */
static int
lfs_vtruncbuf(struct vnode *vp, daddr_t lbn, int slpflag, int slptimeo)
{
	struct buf *bp, *nbp;
	int s, error;
	struct lfs *fs;
	voff_t off;

	off = round_page((voff_t)lbn << vp->v_mount->mnt_fs_bshift);
	simple_lock(&vp->v_interlock);
	error = VOP_PUTPAGES(vp, off, 0, PGO_FREE | PGO_SYNCIO);
	if (error) {
		return error;
	} 

	fs = VTOI(vp)->i_lfs;
	s = splbio();

	lockmgr(&fs->lfs_fraglock, LK_SHARED, 0);
restart:
	for (bp = LIST_FIRST(&vp->v_cleanblkhd); bp; bp = nbp) {
		nbp = LIST_NEXT(bp, b_vnbufs);
		if (bp->b_lblkno < lbn)
			continue;
		if (bp->b_flags & B_BUSY) {
			bp->b_flags |= B_WANTED;
			error = tsleep((caddr_t)bp, slpflag | (PRIBIO + 1),
			    "lfs_vtruncbuf", slptimeo);
			if (error) {
				splx(s);
				lockmgr(&fs->lfs_fraglock, LK_RELEASE, 0);
				return (error);
			}
			goto restart;
		}
		bp->b_flags |= B_BUSY | B_INVAL | B_VFLUSH;
		if (bp->b_flags & B_DELWRI) {
			bp->b_flags &= ~B_DELWRI;
			fs->lfs_avail += btofsb(fs, bp->b_bcount);
			wakeup(&fs->lfs_avail);
		}
		LFS_UNLOCK_BUF(bp);
		brelse(bp);
	}

	for (bp = LIST_FIRST(&vp->v_dirtyblkhd); bp; bp = nbp) {
		nbp = LIST_NEXT(bp, b_vnbufs);
		if (bp->b_lblkno < lbn)
			continue;
		if (bp->b_flags & B_BUSY) {
			bp->b_flags |= B_WANTED;
			error = tsleep((caddr_t)bp, slpflag | (PRIBIO + 1),
			    "lfs_vtruncbuf", slptimeo);
			if (error) {
				splx(s);
				lockmgr(&fs->lfs_fraglock, LK_RELEASE, 0);
				return (error);
			}
			goto restart;
		}
		bp->b_flags |= B_BUSY | B_INVAL | B_VFLUSH;
		if (bp->b_flags & B_DELWRI) {
			bp->b_flags &= ~B_DELWRI;
			fs->lfs_avail += btofsb(fs, bp->b_bcount);
			wakeup(&fs->lfs_avail);
		}
		LFS_UNLOCK_BUF(bp);
		brelse(bp);
	}

	splx(s);
	lockmgr(&fs->lfs_fraglock, LK_RELEASE, 0);

	return (0);
}

