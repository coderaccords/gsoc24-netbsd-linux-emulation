/*	$NetBSD: ffs_alloc.c,v 1.1.1.1 2001/10/26 06:21:35 lukem Exp $	*/
/* From: NetBSD: ffs_alloc.c,v 1.50 2001/09/06 02:16:01 lukem Exp */

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
 *	@(#)ffs_alloc.c	8.19 (Berkeley) 7/13/95
 */

#include <sys/param.h>
#include <sys/time.h>

#include <err.h>
#include <errno.h>

#include <ufs/ufs/ufs_bswap.h>
#include <ufs/ufs/inode.h>
#include <ufs/ffs/fs.h>

#include "ffs/buf.h"
#include "ffs/ffs_extern.h"


static int scanc(u_int, const u_char *, const u_char *, int);

static ufs_daddr_t ffs_alloccg(struct inode *, int, ufs_daddr_t, int);
static ufs_daddr_t ffs_alloccgblk(struct inode *, struct buf *, ufs_daddr_t);
static u_long ffs_hashalloc(struct inode *, int, long, int,
		     ufs_daddr_t (*)(struct inode *, int, ufs_daddr_t, int));
static ufs_daddr_t ffs_mapsearch(struct fs *, struct cg *, ufs_daddr_t, int);

/* in ffs_tables.c */
extern const int inside[], around[];
extern const u_char * const fragtbl[];

/*
 * Allocate a block in the file system.
 * 
 * The size of the requested block is given, which must be some
 * multiple of fs_fsize and <= fs_bsize.
 * A preference may be optionally specified. If a preference is given
 * the following hierarchy is used to allocate a block:
 *   1) allocate the requested block.
 *   2) allocate a rotationally optimal block in the same cylinder.
 *   3) allocate a block in the same cylinder group.
 *   4) quadradically rehash into other cylinder groups, until an
 *      available block is located.
 * If no block preference is given the following hierarchy is used
 * to allocate a block:
 *   1) allocate a block in the cylinder group that contains the
 *      inode for the file.
 *   2) quadradically rehash into other cylinder groups, until an
 *      available block is located.
 */
int
ffs_alloc(struct inode *ip, ufs_daddr_t lbn, ufs_daddr_t bpref, int size,
    ufs_daddr_t *bnp)
{
	struct fs *fs = ip->i_fs;
	ufs_daddr_t bno;
	int cg;
	
	*bnp = 0;
	if ((u_int)size > fs->fs_bsize || fragoff(fs, size) != 0) {
		errx(1, "ffs_alloc: bad size: bsize %d size %d",
		    fs->fs_bsize, size);
	}
	if (size == fs->fs_bsize && fs->fs_cstotal.cs_nbfree == 0)
		goto nospace;
	if (bpref >= fs->fs_size)
		bpref = 0;
	if (bpref == 0)
		cg = ino_to_cg(fs, ip->i_number);
	else
		cg = dtog(fs, bpref);
	bno = (ufs_daddr_t)ffs_hashalloc(ip, cg, (long)bpref, size,
	    			     ffs_alloccg);
	if (bno > 0) {
		ip->i_ffs_blocks += btodb(size);
		*bnp = bno;
		return (0);
	}
nospace:
	return (ENOSPC);
}

/*
 * Select the desired position for the next block in a file.  The file is
 * logically divided into sections. The first section is composed of the
 * direct blocks. Each additional section contains fs_maxbpg blocks.
 * 
 * If no blocks have been allocated in the first section, the policy is to
 * request a block in the same cylinder group as the inode that describes
 * the file. If no blocks have been allocated in any other section, the
 * policy is to place the section in a cylinder group with a greater than
 * average number of free blocks.  An appropriate cylinder group is found
 * by using a rotor that sweeps the cylinder groups. When a new group of
 * blocks is needed, the sweep begins in the cylinder group following the
 * cylinder group from which the previous allocation was made. The sweep
 * continues until a cylinder group with greater than the average number
 * of free blocks is found. If the allocation is for the first block in an
 * indirect block, the information on the previous allocation is unavailable;
 * here a best guess is made based upon the logical block number being
 * allocated.
 * 
 * If a section is already partially allocated, the policy is to
 * contiguously allocate fs_maxcontig blocks.  The end of one of these
 * contiguous blocks and the beginning of the next is physically separated
 * so that the disk head will be in transit between them for at least
 * fs_rotdelay milliseconds.  This is to allow time for the processor to
 * schedule another I/O transfer.
 */
ufs_daddr_t
ffs_blkpref(struct inode *ip, ufs_daddr_t lbn, int indx, ufs_daddr_t *bap)
{
	struct fs *fs;
	int cg;
	int avgbfree, startcg;
	ufs_daddr_t nextblk;

	fs = ip->i_fs;
	if (indx % fs->fs_maxbpg == 0 || bap[indx - 1] == 0) {
		if (lbn < NDADDR + NINDIR(fs)) {
			cg = ino_to_cg(fs, ip->i_number);
			return (fs->fs_fpg * cg + fs->fs_frag);
		}
		/*
		 * Find a cylinder with greater than average number of
		 * unused data blocks.
		 */
		if (indx == 0 || bap[indx - 1] == 0)
			startcg =
			    ino_to_cg(fs, ip->i_number) + lbn / fs->fs_maxbpg;
		else
			startcg = dtog(fs,
				ufs_rw32(bap[indx - 1], UFS_FSNEEDSWAP(fs)) + 1);
		startcg %= fs->fs_ncg;
		avgbfree = fs->fs_cstotal.cs_nbfree / fs->fs_ncg;
		for (cg = startcg; cg < fs->fs_ncg; cg++)
			if (fs->fs_cs(fs, cg).cs_nbfree >= avgbfree) {
				fs->fs_cgrotor = cg;
				return (fs->fs_fpg * cg + fs->fs_frag);
			}
		for (cg = 0; cg <= startcg; cg++)
			if (fs->fs_cs(fs, cg).cs_nbfree >= avgbfree) {
				fs->fs_cgrotor = cg;
				return (fs->fs_fpg * cg + fs->fs_frag);
			}
		return (0);
	}
	/*
	 * One or more previous blocks have been laid out. If less
	 * than fs_maxcontig previous blocks are contiguous, the
	 * next block is requested contiguously, otherwise it is
	 * requested rotationally delayed by fs_rotdelay milliseconds.
	 */
	nextblk = ufs_rw32(bap[indx - 1], UFS_FSNEEDSWAP(fs)) + fs->fs_frag;
	if (indx < fs->fs_maxcontig ||
		ufs_rw32(bap[indx - fs->fs_maxcontig], UFS_FSNEEDSWAP(fs)) +
	    blkstofrags(fs, fs->fs_maxcontig) != nextblk)
		return (nextblk);
	if (fs->fs_rotdelay != 0)
		/*
		 * Here we convert ms of delay to frags as:
		 * (frags) = (ms) * (rev/sec) * (sect/rev) /
		 *	((sect/frag) * (ms/sec))
		 * then round up to the next block.
		 */
		nextblk += roundup(fs->fs_rotdelay * fs->fs_rps * fs->fs_nsect /
		    (NSPF(fs) * 1000), fs->fs_frag);
	return (nextblk);
}

/*
 * Implement the cylinder overflow algorithm.
 *
 * The policy implemented by this algorithm is:
 *   1) allocate the block in its requested cylinder group.
 *   2) quadradically rehash on the cylinder group number.
 *   3) brute force search for a free block.
 *
 * `size':	size for data blocks, mode for inodes
 */
/*VARARGS5*/
static u_long
ffs_hashalloc(struct inode *ip, int cg, long pref, int size,
    ufs_daddr_t (*allocator)(struct inode *, int, ufs_daddr_t, int))
{
	struct fs *fs;
	long result;
	int i, icg = cg;

	fs = ip->i_fs;
	/*
	 * 1: preferred cylinder group
	 */
	result = (*allocator)(ip, cg, pref, size);
	if (result)
		return (result);
	/*
	 * 2: quadratic rehash
	 */
	for (i = 1; i < fs->fs_ncg; i *= 2) {
		cg += i;
		if (cg >= fs->fs_ncg)
			cg -= fs->fs_ncg;
		result = (*allocator)(ip, cg, 0, size);
		if (result)
			return (result);
	}
	/*
	 * 3: brute force search
	 * Note that we start at i == 2, since 0 was checked initially,
	 * and 1 is always checked in the quadratic rehash.
	 */
	cg = (icg + 2) % fs->fs_ncg;
	for (i = 2; i < fs->fs_ncg; i++) {
		result = (*allocator)(ip, cg, 0, size);
		if (result)
			return (result);
		cg++;
		if (cg == fs->fs_ncg)
			cg = 0;
	}
	return (0);
}

/*
 * Determine whether a block can be allocated.
 *
 * Check to see if a block of the appropriate size is available,
 * and if it is, allocate it.
 */
static ufs_daddr_t
ffs_alloccg(struct inode *ip, int cg, ufs_daddr_t bpref, int size)
{
	struct cg *cgp;
	struct buf *bp;
	ufs_daddr_t bno, blkno;
	int error, frags, allocsiz, i;
	struct fs *fs = ip->i_fs;
	const int needswap = UFS_FSNEEDSWAP(fs);

	if (fs->fs_cs(fs, cg).cs_nbfree == 0 && size == fs->fs_bsize)
		return (0);
	error = bread(ip->i_fd, ip->i_fs, fsbtodb(fs, cgtod(fs, cg)),
		(int)fs->fs_cgsize, &bp);
	if (error) {
		brelse(bp);
		return (0);
	}
	cgp = (struct cg *)bp->b_data;
	if (!cg_chkmagic(cgp, needswap) ||
	    (cgp->cg_cs.cs_nbfree == 0 && size == fs->fs_bsize)) {
		brelse(bp);
		return (0);
	}
	if (size == fs->fs_bsize) {
		bno = ffs_alloccgblk(ip, bp, bpref);
		bdwrite(bp);
		return (bno);
	}
	/*
	 * check to see if any fragments are already available
	 * allocsiz is the size which will be allocated, hacking
	 * it down to a smaller size if necessary
	 */
	frags = numfrags(fs, size);
	for (allocsiz = frags; allocsiz < fs->fs_frag; allocsiz++)
		if (cgp->cg_frsum[allocsiz] != 0)
			break;
	if (allocsiz == fs->fs_frag) {
		/*
		 * no fragments were available, so a block will be 
		 * allocated, and hacked up
		 */
		if (cgp->cg_cs.cs_nbfree == 0) {
			brelse(bp);
			return (0);
		}
		bno = ffs_alloccgblk(ip, bp, bpref);
		bpref = dtogd(fs, bno);
		for (i = frags; i < fs->fs_frag; i++)
			setbit(cg_blksfree(cgp, needswap), bpref + i);
		i = fs->fs_frag - frags;
		ufs_add32(cgp->cg_cs.cs_nffree, i, needswap);
		fs->fs_cstotal.cs_nffree += i;
		fs->fs_cs(fs, cg).cs_nffree += i;
		fs->fs_fmod = 1;
		ufs_add32(cgp->cg_frsum[i], 1, needswap);
		bdwrite(bp);
		return (bno);
	}
	bno = ffs_mapsearch(fs, cgp, bpref, allocsiz);
	for (i = 0; i < frags; i++)
		clrbit(cg_blksfree(cgp, needswap), bno + i);
	ufs_add32(cgp->cg_cs.cs_nffree, -frags, needswap);
	fs->fs_cstotal.cs_nffree -= frags;
	fs->fs_cs(fs, cg).cs_nffree -= frags;
	fs->fs_fmod = 1;
	ufs_add32(cgp->cg_frsum[allocsiz], -1, needswap);
	if (frags != allocsiz)
		ufs_add32(cgp->cg_frsum[allocsiz - frags], 1, needswap);
	blkno = cg * fs->fs_fpg + bno;
	bdwrite(bp);
	return blkno;
}

/*
 * Allocate a block in a cylinder group.
 *
 * This algorithm implements the following policy:
 *   1) allocate the requested block.
 *   2) allocate a rotationally optimal block in the same cylinder.
 *   3) allocate the next available block on the block rotor for the
 *      specified cylinder group.
 * Note that this routine only allocates fs_bsize blocks; these
 * blocks may be fragmented by the routine that allocates them.
 */
static ufs_daddr_t
ffs_alloccgblk(struct inode *ip, struct buf *bp, ufs_daddr_t bpref)
{
	struct cg *cgp;
	ufs_daddr_t bno, blkno;
	int cylno, pos, delta;
	short *cylbp;
	int i;
	struct fs *fs = ip->i_fs;
	const int needswap = UFS_FSNEEDSWAP(fs);

	cgp = (struct cg *)bp->b_data;
	if (bpref == 0 || dtog(fs, bpref) != ufs_rw32(cgp->cg_cgx, needswap)) {
		bpref = ufs_rw32(cgp->cg_rotor, needswap);
		goto norot;
	}
	bpref = blknum(fs, bpref);
	bpref = dtogd(fs, bpref);
	/*
	 * if the requested block is available, use it
	 */
	if (ffs_isblock(fs, cg_blksfree(cgp, needswap),
		fragstoblks(fs, bpref))) {
		bno = bpref;
		goto gotit;
	}
	if (fs->fs_nrpos <= 1 || fs->fs_cpc == 0) {
		/*
		 * Block layout information is not available.
		 * Leaving bpref unchanged means we take the
		 * next available free block following the one 
		 * we just allocated. Hopefully this will at
		 * least hit a track cache on drives of unknown
		 * geometry (e.g. SCSI).
		 */
		goto norot;
	}
	/*
	 * check for a block available on the same cylinder
	 */
	cylno = cbtocylno(fs, bpref);
	if (cg_blktot(cgp, needswap)[cylno] == 0)
		goto norot;
	/*
	 * check the summary information to see if a block is 
	 * available in the requested cylinder starting at the
	 * requested rotational position and proceeding around.
	 */
	cylbp = cg_blks(fs, cgp, cylno, needswap);
	pos = cbtorpos(fs, bpref);
	for (i = pos; i < fs->fs_nrpos; i++)
		if (ufs_rw16(cylbp[i], needswap) > 0)
			break;
	if (i == fs->fs_nrpos)
		for (i = 0; i < pos; i++)
			if (ufs_rw16(cylbp[i], needswap) > 0)
				break;
	if (ufs_rw16(cylbp[i], needswap) > 0) {
		/*
		 * found a rotational position, now find the actual
		 * block. A panic if none is actually there.
		 */
		pos = cylno % fs->fs_cpc;
		bno = (cylno - pos) * fs->fs_spc / NSPB(fs);
		if (fs_postbl(fs, pos)[i] == -1) {
			errx(1,
			    "ffs_alloccgblk: cyl groups corrupted: pos %d i %d",
			    pos, i);
		}
		for (i = fs_postbl(fs, pos)[i];; ) {
			if (ffs_isblock(fs, cg_blksfree(cgp, needswap), bno + i)) {
				bno = blkstofrags(fs, (bno + i));
				goto gotit;
			}
			delta = fs_rotbl(fs)[i];
			if (delta <= 0 ||
			    delta + i > fragstoblks(fs, fs->fs_fpg))
				break;
			i += delta;
		}
		errx(1, "ffs_alloccgblk: can't find blk in cyl: pos %d i %d",
		    pos, i);
	}
norot:
	/*
	 * no blocks in the requested cylinder, so take next
	 * available one in this cylinder group.
	 */
	bno = ffs_mapsearch(fs, cgp, bpref, (int)fs->fs_frag);
	if (bno < 0)
		return (0);
	cgp->cg_rotor = ufs_rw32(bno, needswap);
gotit:
	blkno = fragstoblks(fs, bno);
	ffs_clrblock(fs, cg_blksfree(cgp, needswap), (long)blkno);
	ffs_clusteracct(fs, cgp, blkno, -1);
	ufs_add32(cgp->cg_cs.cs_nbfree, -1, needswap);
	fs->fs_cstotal.cs_nbfree--;
	fs->fs_cs(fs, ufs_rw32(cgp->cg_cgx, needswap)).cs_nbfree--;
	cylno = cbtocylno(fs, bno);
	ufs_add16(cg_blks(fs, cgp, cylno, needswap)[cbtorpos(fs, bno)], -1,
	    needswap);
	ufs_add32(cg_blktot(cgp, needswap)[cylno], -1, needswap);
	fs->fs_fmod = 1;
	blkno = ufs_rw32(cgp->cg_cgx, needswap) * fs->fs_fpg + bno;
	return (blkno);
}

/*
 * Free a block or fragment.
 *
 * The specified block or fragment is placed back in the
 * free map. If a fragment is deallocated, a possible 
 * block reassembly is checked.
 */
void
ffs_blkfree(struct inode *ip, ufs_daddr_t bno, long size)
{
	struct cg *cgp;
	struct buf *bp;
	ufs_daddr_t blkno;
	int i, error, cg, blk, frags, bbase;
	struct fs *fs = ip->i_fs;
	const int needswap = UFS_FSNEEDSWAP(fs);

	if ((u_int)size > fs->fs_bsize || fragoff(fs, size) != 0 ||
	    fragnum(fs, bno) + numfrags(fs, size) > fs->fs_frag) {
		errx(1, "blkfree: bad size: bno %u bsize %d size %ld",
		    bno, fs->fs_bsize, size);
	}
	cg = dtog(fs, bno);
	if ((u_int)bno >= fs->fs_size) {
		warnx("bad block %d, ino %d\n", bno, ip->i_number);
		return;
	}
	error = bread(ip->i_fd, ip->i_fs, fsbtodb(fs, cgtod(fs, cg)),
		(int)fs->fs_cgsize, &bp);
	if (error) {
		brelse(bp);
		return;
	}
	cgp = (struct cg *)bp->b_data;
	if (!cg_chkmagic(cgp, needswap)) {
		brelse(bp);
		return;
	}
	bno = dtogd(fs, bno);
	if (size == fs->fs_bsize) {
		blkno = fragstoblks(fs, bno);
		if (!ffs_isfreeblock(fs, cg_blksfree(cgp, needswap), blkno)) {
			errx(1, "blkfree: freeing free block %d", bno);
		}
		ffs_setblock(fs, cg_blksfree(cgp, needswap), blkno);
		ffs_clusteracct(fs, cgp, blkno, 1);
		ufs_add32(cgp->cg_cs.cs_nbfree, 1, needswap);
		fs->fs_cstotal.cs_nbfree++;
		fs->fs_cs(fs, cg).cs_nbfree++;
		i = cbtocylno(fs, bno);
		ufs_add16(cg_blks(fs, cgp, i, needswap)[cbtorpos(fs, bno)], 1,
		    needswap);
		ufs_add32(cg_blktot(cgp, needswap)[i], 1, needswap);
	} else {
		bbase = bno - fragnum(fs, bno);
		/*
		 * decrement the counts associated with the old frags
		 */
		blk = blkmap(fs, cg_blksfree(cgp, needswap), bbase);
		ffs_fragacct(fs, blk, cgp->cg_frsum, -1, needswap);
		/*
		 * deallocate the fragment
		 */
		frags = numfrags(fs, size);
		for (i = 0; i < frags; i++) {
			if (isset(cg_blksfree(cgp, needswap), bno + i)) {
				errx(1, "blkfree: freeing free frag: block %d",
				    bno + i);
			}
			setbit(cg_blksfree(cgp, needswap), bno + i);
		}
		ufs_add32(cgp->cg_cs.cs_nffree, i, needswap);
		fs->fs_cstotal.cs_nffree += i;
		fs->fs_cs(fs, cg).cs_nffree += i;
		/*
		 * add back in counts associated with the new frags
		 */
		blk = blkmap(fs, cg_blksfree(cgp, needswap), bbase);
		ffs_fragacct(fs, blk, cgp->cg_frsum, 1, needswap);
		/*
		 * if a complete block has been reassembled, account for it
		 */
		blkno = fragstoblks(fs, bbase);
		if (ffs_isblock(fs, cg_blksfree(cgp, needswap), blkno)) {
			ufs_add32(cgp->cg_cs.cs_nffree, -fs->fs_frag, needswap);
			fs->fs_cstotal.cs_nffree -= fs->fs_frag;
			fs->fs_cs(fs, cg).cs_nffree -= fs->fs_frag;
			ffs_clusteracct(fs, cgp, blkno, 1);
			ufs_add32(cgp->cg_cs.cs_nbfree, 1, needswap);
			fs->fs_cstotal.cs_nbfree++;
			fs->fs_cs(fs, cg).cs_nbfree++;
			i = cbtocylno(fs, bbase);
			ufs_add16(cg_blks(fs, cgp, i, needswap)[cbtorpos(fs,
								bbase)], 1,
			    needswap);
			ufs_add32(cg_blktot(cgp, needswap)[i], 1, needswap);
		}
	}
	fs->fs_fmod = 1;
	bdwrite(bp);
}


static int
scanc(u_int size, const u_char *cp, const u_char table[], int mask)
{
	const u_char *end = &cp[size];

	while (cp < end && (table[*cp] & mask) == 0)
		cp++;
	return (end - cp);
}

/*
 * Find a block of the specified size in the specified cylinder group.
 *
 * It is a panic if a request is made to find a block if none are
 * available.
 */
static ufs_daddr_t
ffs_mapsearch(struct fs *fs, struct cg *cgp, ufs_daddr_t bpref, int allocsiz)
{
	ufs_daddr_t bno;
	int start, len, loc, i;
	int blk, field, subfield, pos;
	int ostart, olen;
	const int needswap = UFS_FSNEEDSWAP(fs);

	/*
	 * find the fragment by searching through the free block
	 * map for an appropriate bit pattern
	 */
	if (bpref)
		start = dtogd(fs, bpref) / NBBY;
	else
		start = ufs_rw32(cgp->cg_frotor, needswap) / NBBY;
	len = howmany(fs->fs_fpg, NBBY) - start;
	ostart = start;
	olen = len;
	loc = scanc((u_int)len,
		(const u_char *)&cg_blksfree(cgp, needswap)[start],
		(const u_char *)fragtbl[fs->fs_frag],
		(1 << (allocsiz - 1 + (fs->fs_frag % NBBY))));
	if (loc == 0) {
		len = start + 1;
		start = 0;
		loc = scanc((u_int)len,
			(const u_char *)&cg_blksfree(cgp, needswap)[0],
			(const u_char *)fragtbl[fs->fs_frag],
			(1 << (allocsiz - 1 + (fs->fs_frag % NBBY))));
		if (loc == 0) {
			errx(1,
    "ffs_alloccg: map corrupted: start %d len %d offset %d %ld",
				ostart, olen,
				ufs_rw32(cgp->cg_freeoff, needswap),
				(long)cg_blksfree(cgp, needswap) - (long)cgp);
			/* NOTREACHED */
		}
	}
	bno = (start + len - loc) * NBBY;
	cgp->cg_frotor = ufs_rw32(bno, needswap);
	/*
	 * found the byte in the map
	 * sift through the bits to find the selected frag
	 */
	for (i = bno + NBBY; bno < i; bno += fs->fs_frag) {
		blk = blkmap(fs, cg_blksfree(cgp, needswap), bno);
		blk <<= 1;
		field = around[allocsiz];
		subfield = inside[allocsiz];
		for (pos = 0; pos <= fs->fs_frag - allocsiz; pos++) {
			if ((blk & field) == subfield)
				return (bno + pos);
			field <<= 1;
			subfield <<= 1;
		}
	}
	errx(1, "ffs_alloccg: block not in map: bno %d", bno);
	return (-1);
}

/*
 * Update the cluster map because of an allocation or free.
 *
 * Cnt == 1 means free; cnt == -1 means allocating.
 */
void
ffs_clusteracct(struct fs *fs, struct cg *cgp, ufs_daddr_t blkno, int cnt)
{
	int32_t *sump;
	int32_t *lp;
	u_char *freemapp, *mapp;
	int i, start, end, forw, back, map, bit;
	const int needswap = UFS_FSNEEDSWAP(fs);

	if (fs->fs_contigsumsize <= 0)
		return;
	freemapp = cg_clustersfree(cgp, needswap);
	sump = cg_clustersum(cgp, needswap);
	/*
	 * Allocate or clear the actual block.
	 */
	if (cnt > 0)
		setbit(freemapp, blkno);
	else
		clrbit(freemapp, blkno);
	/*
	 * Find the size of the cluster going forward.
	 */
	start = blkno + 1;
	end = start + fs->fs_contigsumsize;
	if (end >= ufs_rw32(cgp->cg_nclusterblks, needswap))
		end = ufs_rw32(cgp->cg_nclusterblks, needswap);
	mapp = &freemapp[start / NBBY];
	map = *mapp++;
	bit = 1 << (start % NBBY);
	for (i = start; i < end; i++) {
		if ((map & bit) == 0)
			break;
		if ((i & (NBBY - 1)) != (NBBY - 1)) {
			bit <<= 1;
		} else {
			map = *mapp++;
			bit = 1;
		}
	}
	forw = i - start;
	/*
	 * Find the size of the cluster going backward.
	 */
	start = blkno - 1;
	end = start - fs->fs_contigsumsize;
	if (end < 0)
		end = -1;
	mapp = &freemapp[start / NBBY];
	map = *mapp--;
	bit = 1 << (start % NBBY);
	for (i = start; i > end; i--) {
		if ((map & bit) == 0)
			break;
		if ((i & (NBBY - 1)) != 0) {
			bit >>= 1;
		} else {
			map = *mapp--;
			bit = 1 << (NBBY - 1);
		}
	}
	back = start - i;
	/*
	 * Account for old cluster and the possibly new forward and
	 * back clusters.
	 */
	i = back + forw + 1;
	if (i > fs->fs_contigsumsize)
		i = fs->fs_contigsumsize;
	ufs_add32(sump[i], cnt, needswap);
	if (back > 0)
		ufs_add32(sump[back], -cnt, needswap);
	if (forw > 0)
		ufs_add32(sump[forw], -cnt, needswap);

	/*
	 * Update cluster summary information.
	 */
	lp = &sump[fs->fs_contigsumsize];
	for (i = fs->fs_contigsumsize; i > 0; i--)
		if (ufs_rw32(*lp--, needswap) > 0)
			break;
	fs->fs_maxcluster[ufs_rw32(cgp->cg_cgx, needswap)] = i;
}
