/*	$NetBSD: ext2fs_subr.c,v 1.18 2005/11/02 12:39:00 yamt Exp $	*/

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
 *	@(#)ffs_subr.c	8.2 (Berkeley) 9/21/93
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
 *	@(#)ffs_subr.c	8.2 (Berkeley) 9/21/93
 * Modified for ext2fs by Manuel Bouyer.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: ext2fs_subr.c,v 1.18 2005/11/02 12:39:00 yamt Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/vnode.h>
#include <sys/buf.h>
#include <sys/inttypes.h>
#include <ufs/ufs/inode.h>
#include <ufs/ext2fs/ext2fs.h>
#include <ufs/ext2fs/ext2fs_extern.h>

/*
 * Return buffer with the contents of block "offset" from the beginning of
 * directory "ip".  If "res" is non-zero, fill it in with a pointer to the
 * remaining space in the directory.
 */
int
ext2fs_blkatoff(struct vnode *vp, off_t offset, char **res, struct buf **bpp)
{
	struct inode *ip;
	struct m_ext2fs *fs;
	struct buf *bp;
	daddr_t lbn;
	int error;

	ip = VTOI(vp);
	fs = ip->i_e2fs;
	lbn = lblkno(fs, offset);

	*bpp = NULL;
	if ((error = bread(vp, lbn, fs->e2fs_bsize, NOCRED, &bp)) != 0) {
		brelse(bp);
		return (error);
	}
	if (res)
		*res = (char *)bp->b_data + blkoff(fs, offset);
	*bpp = bp;
	return (0);
}

void
ext2fs_itimes(struct inode *ip, const struct timespec *acc,
    const struct timespec *mod, const struct timespec *cre)
{
	struct timespec *ts = NULL, tsb;

	if (!(ip->i_flag & (IN_ACCESS | IN_CHANGE | IN_UPDATE | IN_MODIFY))) {
		return;
	}

	if (ip->i_flag & IN_ACCESS) {
		if (acc == NULL)
			acc = ts == NULL ? (ts = nanotime(&tsb)) : ts;
		ip->i_e2fs_atime = acc->tv_sec;
	}
	if (ip->i_flag & (IN_UPDATE | IN_MODIFY)) {
		if (mod == NULL)
			mod = ts == NULL ? (ts = nanotime(&tsb)) : ts;
		ip->i_e2fs_mtime = mod->tv_sec;
		ip->i_modrev++;
	}
	if (ip->i_flag & (IN_CHANGE | IN_MODIFY)) {
		if (cre == NULL)
			cre = ts == NULL ? (ts = nanotime(&tsb)) : ts;
		ip->i_e2fs_ctime = cre->tv_sec;
	}
	if (ip->i_flag & (IN_ACCESS | IN_MODIFY))
		ip->i_flag |= IN_ACCESSED;
	if (ip->i_flag & (IN_UPDATE | IN_CHANGE))
		ip->i_flag |= IN_MODIFIED;
	ip->i_flag &= ~(IN_ACCESS | IN_CHANGE | IN_UPDATE | IN_MODIFY);
}

#ifdef DIAGNOSTIC
void
ext2fs_checkoverlap(struct buf *bp, struct inode *ip)
{
#if 0
	struct buf *ebp, *ep;
	daddr_t start, last;
	struct vnode *vp;

	ebp = &buf[nbuf];
	start = bp->b_blkno;
	last = start + btodb(bp->b_bcount) - 1;
	for (ep = buf; ep < ebp; ep++) {
		if (ep == bp || (ep->b_flags & B_INVAL) ||
			ep->b_vp == NULLVP)
			continue;
		if (VOP_BMAP(ep->b_vp, (daddr_t)0, &vp, (daddr_t)0, NULL))
			continue;
		if (vp != ip->i_devvp)
			continue;
		/* look for overlap */
		if (ep->b_bcount == 0 || ep->b_blkno > last ||
			ep->b_blkno + btodb(ep->b_bcount) <= start)
			continue;
		vprint("Disk overlap", vp);
		printf("\tstart %" PRId64 ", end %" PRId64 " overlap start "
			"%" PRId64 ", end %" PRId64 "\n",
			start, last, ep->b_blkno,
			ep->b_blkno + btodb(ep->b_bcount) - 1);
		panic("Disk buffer overlap");
	}
#else
	printf("ext2fs_checkoverlap disabled due to buffer cache implementation changes\n");
#endif
}
#endif
