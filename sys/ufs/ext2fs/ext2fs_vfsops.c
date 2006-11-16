/*	$NetBSD: ext2fs_vfsops.c,v 1.105 2006/11/16 01:33:51 christos Exp $	*/

/*
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
 *	@(#)ffs_vfsops.c	8.14 (Berkeley) 11/28/94
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
 *	@(#)ffs_vfsops.c	8.14 (Berkeley) 11/28/94
 * Modified for ext2fs by Manuel Bouyer.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: ext2fs_vfsops.c,v 1.105 2006/11/16 01:33:51 christos Exp $");

#if defined(_KERNEL_OPT)
#include "opt_compat_netbsd.h"
#endif

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/sysctl.h>
#include <sys/namei.h>
#include <sys/proc.h>
#include <sys/kernel.h>
#include <sys/vnode.h>
#include <sys/socket.h>
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
#include <sys/lock.h>
#include <sys/conf.h>
#include <sys/kauth.h>

#include <miscfs/specfs/specdev.h>

#include <ufs/ufs/quota.h>
#include <ufs/ufs/ufsmount.h>
#include <ufs/ufs/inode.h>
#include <ufs/ufs/dir.h>
#include <ufs/ufs/ufs_extern.h>

#include <ufs/ext2fs/ext2fs.h>
#include <ufs/ext2fs/ext2fs_dir.h>
#include <ufs/ext2fs/ext2fs_extern.h>

extern struct lock ufs_hashlock;

int ext2fs_sbupdate(struct ufsmount *, int);
static int ext2fs_checksb(struct ext2fs *, int);

extern const struct vnodeopv_desc ext2fs_vnodeop_opv_desc;
extern const struct vnodeopv_desc ext2fs_specop_opv_desc;
extern const struct vnodeopv_desc ext2fs_fifoop_opv_desc;

const struct vnodeopv_desc * const ext2fs_vnodeopv_descs[] = {
	&ext2fs_vnodeop_opv_desc,
	&ext2fs_specop_opv_desc,
	&ext2fs_fifoop_opv_desc,
	NULL,
};

struct vfsops ext2fs_vfsops = {
	MOUNT_EXT2FS,
	ext2fs_mount,
	ufs_start,
	ext2fs_unmount,
	ufs_root,
	ufs_quotactl,
	ext2fs_statvfs,
	ext2fs_sync,
	ext2fs_vget,
	ext2fs_fhtovp,
	ext2fs_vptofh,
	ext2fs_init,
	ext2fs_reinit,
	ext2fs_done,
	ext2fs_mountroot,
	(int (*)(struct mount *, struct vnode *, struct timespec *)) eopnotsupp,
	vfs_stdextattrctl,
	ext2fs_vnodeopv_descs,
	0,
	{ NULL, NULL },
};
VFS_ATTACH(ext2fs_vfsops);

static const struct genfs_ops ext2fs_genfsops = {
	.gop_size = genfs_size,
	.gop_alloc = ext2fs_gop_alloc,
	.gop_write = genfs_gop_write,
	.gop_markupdate = ufs_gop_markupdate,
};

static const struct ufs_ops ext2fs_ufsops = {
	.uo_itimes = ext2fs_itimes,
	.uo_update = ext2fs_update,
};

/*
 * XXX Same structure as FFS inodes?  Should we share a common pool?
 */
POOL_INIT(ext2fs_inode_pool, sizeof(struct inode), 0, 0, 0, "ext2fsinopl",
    &pool_allocator_nointr);
POOL_INIT(ext2fs_dinode_pool, sizeof(struct ext2fs_dinode), 0, 0, 0,
    "ext2dinopl", &pool_allocator_nointr);

extern u_long ext2gennumber;

void
ext2fs_init(void)
{
#ifdef _LKM
	pool_init(&ext2fs_inode_pool, sizeof(struct inode), 0, 0, 0,
	    "ext2fsinopl", &pool_allocator_nointr);
	pool_init(&ext2fs_dinode_pool, sizeof(struct ext2fs_dinode), 0, 0, 0,
	    "ext2dinopl", &pool_allocator_nointr);
#endif
	ufs_init();
}

void
ext2fs_reinit(void)
{
	ufs_reinit();
}

void
ext2fs_done(void)
{
	ufs_done();
#ifdef _LKM
	pool_destroy(&ext2fs_inode_pool);
	pool_destroy(&ext2fs_dinode_pool);
#endif
}

/*
 * Called by main() when ext2fs is going to be mounted as root.
 *
 * Name is updated by mount(8) after booting.
 */
#define ROOTNAME	"root_device"

int
ext2fs_mountroot(void)
{
	extern struct vnode *rootvp;
	struct m_ext2fs *fs;
	struct mount *mp;
	struct lwp *l = curlwp;		/* XXX */
	struct ufsmount *ump;
	int error;

	if (device_class(root_device) != DV_DISK)
		return (ENODEV);

	if ((error = vfs_rootmountalloc(MOUNT_EXT2FS, "root_device", &mp))) {
		vrele(rootvp);
		return (error);
	}

	if ((error = ext2fs_mountfs(rootvp, mp, l)) != 0) {
		mp->mnt_op->vfs_refcount--;
		vfs_unbusy(mp);
		free(mp, M_MOUNT);
		return (error);
	}
	simple_lock(&mountlist_slock);
	CIRCLEQ_INSERT_TAIL(&mountlist, mp, mnt_list);
	simple_unlock(&mountlist_slock);
	ump = VFSTOUFS(mp);
	fs = ump->um_e2fs;
	memset(fs->e2fs_fsmnt, 0, sizeof(fs->e2fs_fsmnt));
	(void) copystr(mp->mnt_stat.f_mntonname, fs->e2fs_fsmnt,
	    sizeof(fs->e2fs_fsmnt) - 1, 0);
	if (fs->e2fs.e2fs_rev > E2FS_REV0) {
		memset(fs->e2fs.e2fs_fsmnt, 0, sizeof(fs->e2fs.e2fs_fsmnt));
		(void) copystr(mp->mnt_stat.f_mntonname, fs->e2fs.e2fs_fsmnt,
		    sizeof(fs->e2fs.e2fs_fsmnt) - 1, 0);
	}
	(void)ext2fs_statvfs(mp, &mp->mnt_stat, l);
	vfs_unbusy(mp);
	setrootfstime((time_t)fs->e2fs.e2fs_wtime);
	return (0);
}

/*
 * VFS Operations.
 *
 * mount system call
 */
int
ext2fs_mount(struct mount *mp, const char *path, void *data,
	struct nameidata *ndp, struct lwp *l)
{
	struct vnode *devvp;
	struct ufs_args args;
	struct ufsmount *ump = NULL;
	struct m_ext2fs *fs;
	size_t size;
	int error, flags, update;
	mode_t accessmode;

	if (mp->mnt_flag & MNT_GETARGS) {
		ump = VFSTOUFS(mp);
		if (ump == NULL)
			return EIO;
		args.fspec = NULL;
		return copyout(&args, data, sizeof(args));
	}
	error = copyin(data, &args, sizeof (struct ufs_args));
	if (error)
		return (error);

	update = mp->mnt_flag & MNT_UPDATE;

	/* Check arguments */
	if (args.fspec != NULL) {
		/*
		 * Look up the name and verify that it's sane.
		 */
		NDINIT(ndp, LOOKUP, FOLLOW, UIO_USERSPACE, args.fspec, l);
		if ((error = namei(ndp)) != 0)
			return (error);
		devvp = ndp->ni_vp;

		if (!update) {
			/*
			 * Be sure this is a valid block device
			 */
			if (devvp->v_type != VBLK)
				error = ENOTBLK;
			else if (bdevsw_lookup(devvp->v_rdev) == NULL)
				error = ENXIO;
		} else {
		        /*
			 * Be sure we're still naming the same device
			 * used for our initial mount
			 */
			ump = VFSTOUFS(mp);
			if (devvp != ump->um_devvp)
				error = EINVAL;
		}
	} else {
		if (!update) {
			/* New mounts must have a filename for the device */
			return (EINVAL);
		} else {
			ump = VFSTOUFS(mp);
			devvp = ump->um_devvp;
			vref(devvp);
		}
	}

	/*
	 * If mount by non-root, then verify that user has necessary
	 * permissions on the device.
	 */
	if (error == 0 && kauth_cred_geteuid(l->l_cred) != 0) {
		accessmode = VREAD;
		if (update ?
		    (mp->mnt_iflag & IMNT_WANTRDWR) != 0 :
		    (mp->mnt_flag & MNT_RDONLY) == 0)
			accessmode |= VWRITE;
		vn_lock(devvp, LK_EXCLUSIVE | LK_RETRY);
		error = VOP_ACCESS(devvp, accessmode, l->l_cred, l);
		VOP_UNLOCK(devvp, 0);
	}

	if (error) {
		vrele(devvp);
		return (error);
	}

	if (!update) {
		int xflags;

		/*
		 * Disallow multiple mounts of the same device.
		 * Disallow mounting of a device that is currently in use
		 * (except for root, which might share swap device for
		 * miniroot).
		 */
		error = vfs_mountedon(devvp);
		if (error)
			goto fail;
		if (vcount(devvp) > 1 && devvp != rootvp) {
			error = EBUSY;
			goto fail;
		}
		if (mp->mnt_flag & MNT_RDONLY)
			xflags = FREAD;
		else
			xflags = FREAD|FWRITE;
		error = VOP_OPEN(devvp, xflags, FSCRED, l);
		if (error)
			goto fail;
		error = ext2fs_mountfs(devvp, mp, l);
		if (error) {
			vn_lock(devvp, LK_EXCLUSIVE | LK_RETRY);
			(void)VOP_CLOSE(devvp, xflags, NOCRED, l);
			VOP_UNLOCK(devvp, 0);
			goto fail;
		}

		ump = VFSTOUFS(mp);
		fs = ump->um_e2fs;
	} else {
		/*
		 * Update the mount.
		 */

		/*
		 * The initial mount got a reference on this
		 * device, so drop the one obtained via
		 * namei(), above.
		 */
		vrele(devvp);

		ump = VFSTOUFS(mp);
		fs = ump->um_e2fs;
		if (fs->e2fs_ronly == 0 && (mp->mnt_flag & MNT_RDONLY)) {
			/*
			 * Changing from r/w to r/o
			 */
			flags = WRITECLOSE;
			if (mp->mnt_flag & MNT_FORCE)
				flags |= FORCECLOSE;
			error = ext2fs_flushfiles(mp, flags, l);
			if (error == 0 &&
			    ext2fs_cgupdate(ump, MNT_WAIT) == 0 &&
			    (fs->e2fs.e2fs_state & E2FS_ERRORS) == 0) {
				fs->e2fs.e2fs_state = E2FS_ISCLEAN;
				(void) ext2fs_sbupdate(ump, MNT_WAIT);
			}
			if (error)
				return (error);
			fs->e2fs_ronly = 1;
		}

		if (mp->mnt_flag & MNT_RELOAD) {
			error = ext2fs_reload(mp, ndp->ni_cnd.cn_cred, l);
			if (error)
				return (error);
		}

		if (fs->e2fs_ronly && (mp->mnt_iflag & IMNT_WANTRDWR)) {
			/*
			 * Changing from read-only to read/write
			 */
			fs->e2fs_ronly = 0;
			if (fs->e2fs.e2fs_state == E2FS_ISCLEAN)
				fs->e2fs.e2fs_state = 0;
			else
				fs->e2fs.e2fs_state = E2FS_ERRORS;
			fs->e2fs_fmod = 1;
		}
		if (args.fspec == NULL)
			return EINVAL;
	}

	error = set_statvfs_info(path, UIO_USERSPACE, args.fspec,
	    UIO_USERSPACE, mp, l);
	(void) copystr(mp->mnt_stat.f_mntonname, fs->e2fs_fsmnt,
	    sizeof(fs->e2fs_fsmnt) - 1, &size);
	memset(fs->e2fs_fsmnt + size, 0, sizeof(fs->e2fs_fsmnt) - size);
	if (fs->e2fs.e2fs_rev > E2FS_REV0) {
		(void) copystr(mp->mnt_stat.f_mntonname, fs->e2fs.e2fs_fsmnt,
		    sizeof(fs->e2fs.e2fs_fsmnt) - 1, &size);
		memset(fs->e2fs.e2fs_fsmnt, 0,
		    sizeof(fs->e2fs.e2fs_fsmnt) - size);
	}
	if (fs->e2fs_fmod != 0) {	/* XXX */
		fs->e2fs_fmod = 0;
		if (fs->e2fs.e2fs_state == 0)
			fs->e2fs.e2fs_wtime = time_second;
		else
			printf("%s: file system not clean; please fsck(8)\n",
				mp->mnt_stat.f_mntfromname);
		(void) ext2fs_cgupdate(ump, MNT_WAIT);
	}
	return (error);

fail:
	vrele(devvp);
	return (error);
}

/*
 * Reload all incore data for a filesystem (used after running fsck on
 * the root filesystem and finding things to fix). The filesystem must
 * be mounted read-only.
 *
 * Things to do to update the mount:
 *	1) invalidate all cached meta-data.
 *	2) re-read superblock from disk.
 *	3) re-read summary information from disk.
 *	4) invalidate all inactive vnodes.
 *	5) invalidate all cached file data.
 *	6) re-read inode data for all active vnodes.
 */
int
ext2fs_reload(struct mount *mountp, kauth_cred_t cred, struct lwp *l)
{
	struct vnode *vp, *nvp, *devvp;
	struct inode *ip;
	struct buf *bp;
	struct m_ext2fs *fs;
	struct ext2fs *newfs;
	struct partinfo dpart;
	int i, size, error;
	caddr_t cp;

	if ((mountp->mnt_flag & MNT_RDONLY) == 0)
		return (EINVAL);
	/*
	 * Step 1: invalidate all cached meta-data.
	 */
	devvp = VFSTOUFS(mountp)->um_devvp;
	vn_lock(devvp, LK_EXCLUSIVE | LK_RETRY);
	error = vinvalbuf(devvp, 0, cred, l, 0, 0);
	VOP_UNLOCK(devvp, 0);
	if (error)
		panic("ext2fs_reload: dirty1");
	/*
	 * Step 2: re-read superblock from disk.
	 */
	if (VOP_IOCTL(devvp, DIOCGPART, &dpart, FREAD, NOCRED, l) != 0)
		size = DEV_BSIZE;
	else
		size = dpart.disklab->d_secsize;
	error = bread(devvp, (daddr_t)(SBOFF / size), SBSIZE, NOCRED, &bp);
	if (error) {
		brelse(bp);
		return (error);
	}
	newfs = (struct ext2fs *)bp->b_data;
	error = ext2fs_checksb(newfs, (mountp->mnt_flag & MNT_RDONLY) != 0);
	if (error) {
		brelse(bp);
		return (error);
	}

	fs = VFSTOUFS(mountp)->um_e2fs;
	/*
	 * copy in new superblock, and compute in-memory values
	 */
	e2fs_sbload(newfs, &fs->e2fs);
	fs->e2fs_ncg =
	    howmany(fs->e2fs.e2fs_bcount - fs->e2fs.e2fs_first_dblock,
	    fs->e2fs.e2fs_bpg);
	/* XXX assume hw bsize = 512 */
	fs->e2fs_fsbtodb = fs->e2fs.e2fs_log_bsize + 1;
	fs->e2fs_bsize = 1024 << fs->e2fs.e2fs_log_bsize;
	fs->e2fs_bshift = LOG_MINBSIZE + fs->e2fs.e2fs_log_bsize;
	fs->e2fs_qbmask = fs->e2fs_bsize - 1;
	fs->e2fs_bmask = ~fs->e2fs_qbmask;
	fs->e2fs_ngdb = howmany(fs->e2fs_ncg,
			fs->e2fs_bsize / sizeof(struct ext2_gd));
	fs->e2fs_ipb = fs->e2fs_bsize / EXT2_DINODE_SIZE;
	fs->e2fs_itpg = fs->e2fs.e2fs_ipg/fs->e2fs_ipb;

	/*
	 * Step 3: re-read summary information from disk.
	 */

	for (i=0; i < fs->e2fs_ngdb; i++) {
		error = bread(devvp ,
		    fsbtodb(fs, ((fs->e2fs_bsize>1024)? 0 : 1) + i + 1),
		    fs->e2fs_bsize, NOCRED, &bp);
		if (error) {
			brelse(bp);
			return (error);
		}
		e2fs_cgload((struct ext2_gd*)bp->b_data,
		    &fs->e2fs_gd[i* fs->e2fs_bsize / sizeof(struct ext2_gd)],
		    fs->e2fs_bsize);
		brelse(bp);
	}

loop:
	/*
	 * NOTE: not using the TAILQ_FOREACH here since in this loop vgone()
	 * and vclean() can be called indirectly
	 */
	simple_lock(&mntvnode_slock);
	for (vp = TAILQ_FIRST(&mountp->mnt_vnodelist); vp; vp = nvp) {
		if (vp->v_mount != mountp) {
			simple_unlock(&mntvnode_slock);
			goto loop;
		}
		/*
		 * Step 4: invalidate all inactive vnodes.
		 */
		if (vrecycle(vp, &mntvnode_slock, l))
			goto loop;
		/*
		 * Step 5: invalidate all cached file data.
		 */
		simple_lock(&vp->v_interlock);
		nvp = TAILQ_NEXT(vp, v_mntvnodes);
		simple_unlock(&mntvnode_slock);
		if (vget(vp, LK_EXCLUSIVE | LK_INTERLOCK))
			goto loop;
		if (vinvalbuf(vp, 0, cred, l, 0, 0))
			panic("ext2fs_reload: dirty2");
		/*
		 * Step 6: re-read inode data for all active vnodes.
		 */
		ip = VTOI(vp);
		error = bread(devvp, fsbtodb(fs, ino_to_fsba(fs, ip->i_number)),
				  (int)fs->e2fs_bsize, NOCRED, &bp);
		if (error) {
			vput(vp);
			return (error);
		}
		cp = (caddr_t)bp->b_data +
		    (ino_to_fsbo(fs, ip->i_number) * EXT2_DINODE_SIZE);
		e2fs_iload((struct ext2fs_dinode *)cp, ip->i_din.e2fs_din);
		brelse(bp);
		vput(vp);
		simple_lock(&mntvnode_slock);
	}
	simple_unlock(&mntvnode_slock);
	return (0);
}

/*
 * Common code for mount and mountroot
 */
int
ext2fs_mountfs(struct vnode *devvp, struct mount *mp, struct lwp *l)
{
	struct ufsmount *ump;
	struct buf *bp;
	struct ext2fs *fs;
	struct m_ext2fs *m_fs;
	dev_t dev;
	struct partinfo dpart;
	int error, i, size, ronly;
	kauth_cred_t cred;
	struct proc *p;

	dev = devvp->v_rdev;
	p = l ? l->l_proc : NULL;
	cred = l ? l->l_cred : NOCRED;

	/* Flush out any old buffers remaining from a previous use. */
	vn_lock(devvp, LK_EXCLUSIVE | LK_RETRY);
	error = vinvalbuf(devvp, V_SAVE, cred, l, 0, 0);
	VOP_UNLOCK(devvp, 0);
	if (error)
		return (error);

	ronly = (mp->mnt_flag & MNT_RDONLY) != 0;
	if (VOP_IOCTL(devvp, DIOCGPART, &dpart, FREAD, cred, l) != 0)
		size = DEV_BSIZE;
	else
		size = dpart.disklab->d_secsize;

	bp = NULL;
	ump = NULL;

#ifdef DEBUG_EXT2
	printf("sb size: %d ino size %d\n", sizeof(struct ext2fs),
	    EXT2_DINODE_SIZE);
#endif
	error = bread(devvp, (SBOFF / size), SBSIZE, cred, &bp);
	if (error)
		goto out;
	fs = (struct ext2fs *)bp->b_data;
	error = ext2fs_checksb(fs, ronly);
	if (error)
		goto out;
	ump = malloc(sizeof *ump, M_UFSMNT, M_WAITOK);
	memset(ump, 0, sizeof *ump);
	ump->um_fstype = UFS1;
	ump->um_ops = &ext2fs_ufsops;
	ump->um_e2fs = malloc(sizeof(struct m_ext2fs), M_UFSMNT, M_WAITOK);
	memset(ump->um_e2fs, 0, sizeof(struct m_ext2fs));
	e2fs_sbload((struct ext2fs*)bp->b_data, &ump->um_e2fs->e2fs);
	brelse(bp);
	bp = NULL;
	m_fs = ump->um_e2fs;
	m_fs->e2fs_ronly = ronly;
	if (ronly == 0) {
		if (m_fs->e2fs.e2fs_state == E2FS_ISCLEAN)
			m_fs->e2fs.e2fs_state = 0;
		else
			m_fs->e2fs.e2fs_state = E2FS_ERRORS;
		m_fs->e2fs_fmod = 1;
	}

	/* compute dynamic sb infos */
	m_fs->e2fs_ncg =
		howmany(m_fs->e2fs.e2fs_bcount - m_fs->e2fs.e2fs_first_dblock,
		m_fs->e2fs.e2fs_bpg);
	/* XXX assume hw bsize = 512 */
	m_fs->e2fs_fsbtodb = m_fs->e2fs.e2fs_log_bsize + 1;
	m_fs->e2fs_bsize = 1024 << m_fs->e2fs.e2fs_log_bsize;
	m_fs->e2fs_bshift = LOG_MINBSIZE + m_fs->e2fs.e2fs_log_bsize;
	m_fs->e2fs_qbmask = m_fs->e2fs_bsize - 1;
	m_fs->e2fs_bmask = ~m_fs->e2fs_qbmask;
	m_fs->e2fs_ngdb = howmany(m_fs->e2fs_ncg,
		m_fs->e2fs_bsize / sizeof(struct ext2_gd));
	m_fs->e2fs_ipb = m_fs->e2fs_bsize / EXT2_DINODE_SIZE;
	m_fs->e2fs_itpg = m_fs->e2fs.e2fs_ipg/m_fs->e2fs_ipb;

	m_fs->e2fs_gd = malloc(m_fs->e2fs_ngdb * m_fs->e2fs_bsize,
		M_UFSMNT, M_WAITOK);
	for (i=0; i < m_fs->e2fs_ngdb; i++) {
		error = bread(devvp ,
		    fsbtodb(m_fs, ((m_fs->e2fs_bsize>1024)? 0 : 1) + i + 1),
		    m_fs->e2fs_bsize, NOCRED, &bp);
		if (error) {
			free(m_fs->e2fs_gd, M_UFSMNT);
			goto out;
		}
		e2fs_cgload((struct ext2_gd*)bp->b_data,
		    &m_fs->e2fs_gd[
			i * m_fs->e2fs_bsize / sizeof(struct ext2_gd)],
		    m_fs->e2fs_bsize);
		brelse(bp);
		bp = NULL;
	}

	mp->mnt_data = ump;
	mp->mnt_stat.f_fsidx.__fsid_val[0] = (long)dev;
	mp->mnt_stat.f_fsidx.__fsid_val[1] = makefstype(MOUNT_EXT2FS);
	mp->mnt_stat.f_fsid = mp->mnt_stat.f_fsidx.__fsid_val[0];
	mp->mnt_stat.f_namemax = EXT2FS_MAXNAMLEN;
	mp->mnt_flag |= MNT_LOCAL;
	mp->mnt_dev_bshift = DEV_BSHIFT;	/* XXX */
	mp->mnt_fs_bshift = m_fs->e2fs_bshift;
	mp->mnt_iflag |= IMNT_DTYPE;
	ump->um_flags = 0;
	ump->um_mountp = mp;
	ump->um_dev = dev;
	ump->um_devvp = devvp;
	ump->um_nindir = NINDIR(m_fs);
	ump->um_lognindir = ffs(NINDIR(m_fs)) - 1;
	ump->um_bptrtodb = m_fs->e2fs_fsbtodb;
	ump->um_seqinc = 1; /* no frags */
	ump->um_maxsymlinklen = EXT2_MAXSYMLINKLEN;
	ump->um_dirblksiz = m_fs->e2fs_bsize;
	ump->um_maxfilesize = ((u_int64_t)0x80000000 * m_fs->e2fs_bsize - 1);
	devvp->v_specmountpoint = mp;
	return (0);

out:
	KASSERT(bp != NULL);
	brelse(bp);
	if (ump) {
		free(ump->um_e2fs, M_UFSMNT);
		free(ump, M_UFSMNT);
		mp->mnt_data = NULL;
	}
	return (error);
}

/*
 * unmount system call
 */
int
ext2fs_unmount(struct mount *mp, int mntflags, struct lwp *l)
{
	struct ufsmount *ump;
	struct m_ext2fs *fs;
	int error, flags;

	flags = 0;
	if (mntflags & MNT_FORCE)
		flags |= FORCECLOSE;
	if ((error = ext2fs_flushfiles(mp, flags, l)) != 0)
		return (error);
	ump = VFSTOUFS(mp);
	fs = ump->um_e2fs;
	if (fs->e2fs_ronly == 0 &&
		ext2fs_cgupdate(ump, MNT_WAIT) == 0 &&
		(fs->e2fs.e2fs_state & E2FS_ERRORS) == 0) {
		fs->e2fs.e2fs_state = E2FS_ISCLEAN;
		(void) ext2fs_sbupdate(ump, MNT_WAIT);
	}
	if (ump->um_devvp->v_type != VBAD)
		ump->um_devvp->v_specmountpoint = NULL;
	vn_lock(ump->um_devvp, LK_EXCLUSIVE | LK_RETRY);
	error = VOP_CLOSE(ump->um_devvp, fs->e2fs_ronly ? FREAD : FREAD|FWRITE,
		NOCRED, l);
	vput(ump->um_devvp);
	free(fs->e2fs_gd, M_UFSMNT);
	free(fs, M_UFSMNT);
	free(ump, M_UFSMNT);
	mp->mnt_data = NULL;
	mp->mnt_flag &= ~MNT_LOCAL;
	return (error);
}

/*
 * Flush out all the files in a filesystem.
 */
int
ext2fs_flushfiles(struct mount *mp, int flags, struct lwp *l)
{
	extern int doforce;
	int error;

	if (!doforce)
		flags &= ~FORCECLOSE;
	error = vflush(mp, NULLVP, flags);
	return (error);
}

/*
 * Get file system statistics.
 */
int
ext2fs_statvfs(struct mount *mp, struct statvfs *sbp, struct lwp *l)
{
	struct ufsmount *ump;
	struct m_ext2fs *fs;
	u_int32_t overhead, overhead_per_group;
	int i, ngroups;

	ump = VFSTOUFS(mp);
	fs = ump->um_e2fs;
	if (fs->e2fs.e2fs_magic != E2FS_MAGIC)
		panic("ext2fs_statvfs");

	/*
	 * Compute the overhead (FS structures)
	 */
	overhead_per_group = 1 /* block bitmap */ +
				 1 /* inode bitmap */ +
				 fs->e2fs_itpg;
	overhead = fs->e2fs.e2fs_first_dblock +
		   fs->e2fs_ncg * overhead_per_group;
	if (fs->e2fs.e2fs_rev > E2FS_REV0 &&
	    fs->e2fs.e2fs_features_rocompat & EXT2F_ROCOMPAT_SPARSESUPER) {
		for (i = 0, ngroups = 0; i < fs->e2fs_ncg; i++) {
			if (cg_has_sb(i))
				ngroups++;
		}
	} else {
		ngroups = fs->e2fs_ncg;
	}
	overhead += ngroups * (1 + fs->e2fs_ngdb);

	sbp->f_bsize = fs->e2fs_bsize;
	sbp->f_frsize = 1024 << fs->e2fs.e2fs_fsize;
	sbp->f_iosize = fs->e2fs_bsize;
	sbp->f_blocks = fs->e2fs.e2fs_bcount - overhead;
	sbp->f_bfree = fs->e2fs.e2fs_fbcount;
	sbp->f_bresvd = fs->e2fs.e2fs_rbcount;
	if (sbp->f_bfree > sbp->f_bresvd)
		sbp->f_bavail = sbp->f_bfree - sbp->f_bresvd;
	else
		sbp->f_bavail = 0;
	sbp->f_files =  fs->e2fs.e2fs_icount;
	sbp->f_ffree = fs->e2fs.e2fs_ficount;
	sbp->f_favail = fs->e2fs.e2fs_ficount;
	sbp->f_fresvd = 0;
	copy_statvfs_info(sbp, mp);
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
ext2fs_sync(struct mount *mp, int waitfor, kauth_cred_t cred, struct lwp *l)
{
	struct vnode *vp, *nvp;
	struct inode *ip;
	struct ufsmount *ump = VFSTOUFS(mp);
	struct m_ext2fs *fs;
	int error, allerror = 0;

	fs = ump->um_e2fs;
	if (fs->e2fs_fmod != 0 && fs->e2fs_ronly != 0) {	/* XXX */
		printf("fs = %s\n", fs->e2fs_fsmnt);
		panic("update: rofs mod");
	}
	/*
	 * Write back each (modified) inode.
	 */
	simple_lock(&mntvnode_slock);
loop:
	/*
	 * NOTE: not using the TAILQ_FOREACH here since in this loop vgone()
	 * and vclean() can be called indirectly
	 */
	for (vp = TAILQ_FIRST(&mp->mnt_vnodelist); vp; vp = nvp) {
		/*
		 * If the vnode that we are about to sync is no longer
		 * associated with this mount point, start over.
		 */
		if (vp->v_mount != mp)
			goto loop;
		simple_lock(&vp->v_interlock);
		nvp = TAILQ_NEXT(vp, v_mntvnodes);
		ip = VTOI(vp);
		if (vp->v_type == VNON ||
		    ((ip->i_flag &
		      (IN_CHANGE | IN_UPDATE | IN_MODIFIED)) == 0 &&
		     LIST_EMPTY(&vp->v_dirtyblkhd) &&
		     vp->v_uobj.uo_npages == 0))
		{
			simple_unlock(&vp->v_interlock);
			continue;
		}
		simple_unlock(&mntvnode_slock);
		error = vget(vp, LK_EXCLUSIVE | LK_NOWAIT | LK_INTERLOCK);
		if (error) {
			simple_lock(&mntvnode_slock);
			if (error == ENOENT)
				goto loop;
			continue;
		}
		if (vp->v_type == VREG && waitfor == MNT_LAZY)
			error = ext2fs_update(vp, NULL, NULL, 0);
		else
			error = VOP_FSYNC(vp, cred,
			    waitfor == MNT_WAIT ? FSYNC_WAIT : 0, 0, 0, l);
		if (error)
			allerror = error;
		vput(vp);
		simple_lock(&mntvnode_slock);
	}
	simple_unlock(&mntvnode_slock);
	/*
	 * Force stale file system control information to be flushed.
	 */
	if (waitfor != MNT_LAZY) {
		vn_lock(ump->um_devvp, LK_EXCLUSIVE | LK_RETRY);
		if ((error = VOP_FSYNC(ump->um_devvp, cred,
		    waitfor == MNT_WAIT ? FSYNC_WAIT : 0, 0, 0, l)) != 0)
			allerror = error;
		VOP_UNLOCK(ump->um_devvp, 0);
	}
	/*
	 * Write back modified superblock.
	 */
	if (fs->e2fs_fmod != 0) {
		fs->e2fs_fmod = 0;
		fs->e2fs.e2fs_wtime = time_second;
		if ((error = ext2fs_cgupdate(ump, waitfor)))
			allerror = error;
	}
	return (allerror);
}

/*
 * Look up a EXT2FS dinode number to find its incore vnode, otherwise read it
 * in from disk.  If it is in core, wait for the lock bit to clear, then
 * return the inode locked.  Detection and handling of mount points must be
 * done by the calling routine.
 */
int
ext2fs_vget(struct mount *mp, ino_t ino, struct vnode **vpp)
{
	struct m_ext2fs *fs;
	struct inode *ip;
	struct ufsmount *ump;
	struct buf *bp;
	struct vnode *vp;
	dev_t dev;
	int error;
	caddr_t cp;

	ump = VFSTOUFS(mp);
	dev = ump->um_dev;

	if ((*vpp = ufs_ihashget(dev, ino, LK_EXCLUSIVE)) != NULL)
		return (0);

	/* Allocate a new vnode/inode. */
	if ((error = getnewvnode(VT_EXT2FS, mp, ext2fs_vnodeop_p, &vp)) != 0) {
		*vpp = NULL;
		return (error);
	}

	do {
		if ((*vpp = ufs_ihashget(dev, ino, LK_EXCLUSIVE)) != NULL) {
			ungetnewvnode(vp);
			return (0);
		}
	} while (lockmgr(&ufs_hashlock, LK_EXCLUSIVE|LK_SLEEPFAIL, 0));

	vp->v_flag |= VLOCKSWORK;

	ip = pool_get(&ext2fs_inode_pool, PR_WAITOK);
	memset(ip, 0, sizeof(struct inode));
	vp->v_data = ip;
	ip->i_vnode = vp;
	ip->i_ump = ump;
	ip->i_e2fs = fs = ump->um_e2fs;
	ip->i_dev = dev;
	ip->i_number = ino;
	ip->i_e2fs_last_lblk = 0;
	ip->i_e2fs_last_blk = 0;

	/*
	 * Put it onto its hash chain and lock it so that other requests for
	 * this inode will block if they arrive while we are sleeping waiting
	 * for old data structures to be purged or for the contents of the
	 * disk portion of this inode to be read.
	 */

	ufs_ihashins(ip);
	lockmgr(&ufs_hashlock, LK_RELEASE, 0);

	/* Read in the disk contents for the inode, copy into the inode. */
	error = bread(ump->um_devvp, fsbtodb(fs, ino_to_fsba(fs, ino)),
			  (int)fs->e2fs_bsize, NOCRED, &bp);
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
	cp = (caddr_t)bp->b_data +
	    (ino_to_fsbo(fs, ino) * EXT2_DINODE_SIZE);
	ip->i_din.e2fs_din = pool_get(&ext2fs_dinode_pool, PR_WAITOK);
	e2fs_iload((struct ext2fs_dinode *)cp, ip->i_din.e2fs_din);
	brelse(bp);

	/* If the inode was deleted, reset all fields */
	if (ip->i_e2fs_dtime != 0) {
		ip->i_e2fs_mode = ip->i_e2fs_nblock = 0;
		(void)ext2fs_setsize(ip, 0);
		memset(ip->i_e2fs_blocks, 0, sizeof(ip->i_e2fs_blocks));
	}

	/*
	 * Initialize the vnode from the inode, check for aliases.
	 * Note that the underlying vnode may have changed.
	 */

	error = ext2fs_vinit(mp, ext2fs_specop_p, ext2fs_fifoop_p, &vp);
	if (error) {
		vput(vp);
		*vpp = NULL;
		return (error);
	}
	/*
	 * Finish inode initialization now that aliasing has been resolved.
	 */

	genfs_node_init(vp, &ext2fs_genfsops);
	ip->i_devvp = ump->um_devvp;
	VREF(ip->i_devvp);

	/*
	 * Set up a generation number for this inode if it does not
	 * already have one. This should only happen on old filesystems.
	 */

	if (ip->i_e2fs_gen == 0) {
		if (++ext2gennumber < (u_long)time_second)
			ext2gennumber = time_second;
		ip->i_e2fs_gen = ext2gennumber;
		if ((vp->v_mount->mnt_flag & MNT_RDONLY) == 0)
			ip->i_flag |= IN_MODIFIED;
	}
	vp->v_size = ext2fs_size(ip);
	*vpp = vp;
	return (0);
}

/*
 * File handle to vnode
 *
 * Have to be really careful about stale file handles:
 * - check that the inode number is valid
 * - call ext2fs_vget() to get the locked inode
 * - check for an unallocated inode (i_mode == 0)
 */
int
ext2fs_fhtovp(struct mount *mp, struct fid *fhp, struct vnode **vpp)
{
	struct inode *ip;
	struct vnode *nvp;
	int error;
	struct ufid ufh;
	struct m_ext2fs *fs;

	if (fhp->fid_len != sizeof(struct ufid))
		return EINVAL;

	memcpy(&ufh, fhp, sizeof(struct ufid));
	fs = VFSTOUFS(mp)->um_e2fs;
	if ((ufh.ufid_ino < EXT2_FIRSTINO && ufh.ufid_ino != EXT2_ROOTINO) ||
		ufh.ufid_ino >= fs->e2fs_ncg * fs->e2fs.e2fs_ipg)
		return (ESTALE);

	if ((error = VFS_VGET(mp, ufh.ufid_ino, &nvp)) != 0) {
		*vpp = NULLVP;
		return (error);
	}
	ip = VTOI(nvp);
	if (ip->i_e2fs_mode == 0 || ip->i_e2fs_dtime != 0 ||
		ip->i_e2fs_gen != ufh.ufid_gen) {
		vput(nvp);
		*vpp = NULLVP;
		return (ESTALE);
	}
	*vpp = nvp;
	return (0);
}

/*
 * Vnode pointer to File handle
 */
/* ARGSUSED */
int
ext2fs_vptofh(struct vnode *vp, struct fid *fhp, size_t *fh_size)
{
	struct inode *ip;
	struct ufid ufh;

	if (*fh_size < sizeof(struct ufid)) {
		*fh_size = sizeof(struct ufid);
		return E2BIG;
	}
	*fh_size = sizeof(struct ufid);

	ip = VTOI(vp);
	memset(&ufh, 0, sizeof(ufh));
	ufh.ufid_len = sizeof(struct ufid);
	ufh.ufid_ino = ip->i_number;
	ufh.ufid_gen = ip->i_e2fs_gen;
	memcpy(fhp, &ufh, sizeof(ufh));
	return (0);
}

SYSCTL_SETUP(sysctl_vfs_ext2fs_setup, "sysctl vfs.ext2fs subtree setup")
{

	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT,
		       CTLTYPE_NODE, "vfs", NULL,
		       NULL, 0, NULL, 0,
		       CTL_VFS, CTL_EOL);
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT,
		       CTLTYPE_NODE, "ext2fs",
		       SYSCTL_DESCR("Linux EXT2FS file system"),
		       NULL, 0, NULL, 0,
		       CTL_VFS, 17, CTL_EOL);
	/*
	 * XXX the "17" above could be dynamic, thereby eliminating
	 * one more instance of the "number to vfs" mapping problem,
	 * but "17" is the order as taken from sys/mount.h
	 */
}

/*
 * Write a superblock and associated information back to disk.
 */
int
ext2fs_sbupdate(struct ufsmount *mp, int waitfor)
{
	struct m_ext2fs *fs = mp->um_e2fs;
	struct buf *bp;
	int error = 0;

	bp = getblk(mp->um_devvp, SBLOCK, SBSIZE, 0, 0);
	e2fs_sbsave(&fs->e2fs, (struct ext2fs*)bp->b_data);
	if (waitfor == MNT_WAIT)
		error = bwrite(bp);
	else
		bawrite(bp);
	return (error);
}

int
ext2fs_cgupdate(struct ufsmount *mp, int waitfor)
{
	struct m_ext2fs *fs = mp->um_e2fs;
	struct buf *bp;
	int i, error = 0, allerror = 0;

	allerror = ext2fs_sbupdate(mp, waitfor);
	for (i = 0; i < fs->e2fs_ngdb; i++) {
		bp = getblk(mp->um_devvp, fsbtodb(fs, ((fs->e2fs_bsize>1024)?0:1)+i+1),
			fs->e2fs_bsize, 0, 0);
		e2fs_cgsave(&fs->e2fs_gd[i* fs->e2fs_bsize / sizeof(struct ext2_gd)],
				(struct ext2_gd*)bp->b_data, fs->e2fs_bsize);
		if (waitfor == MNT_WAIT)
			error = bwrite(bp);
		else
			bawrite(bp);
	}

	if (!allerror && error)
		allerror = error;
	return (allerror);
}

static int
ext2fs_checksb(struct ext2fs *fs, int ronly)
{
	if (fs2h16(fs->e2fs_magic) != E2FS_MAGIC) {
		return (EINVAL);		/* XXX needs translation */
	}
	if (fs2h32(fs->e2fs_rev) > E2FS_REV1) {
#ifdef DIAGNOSTIC
		printf("Ext2 fs: unsupported revision number: %x\n",
					fs2h32(fs->e2fs_rev));
#endif
		return (EINVAL);		/* XXX needs translation */
	}
	if (fs2h32(fs->e2fs_log_bsize) > 2) { /* block size = 1024|2048|4096 */
#ifdef DIAGNOSTIC
		printf("Ext2 fs: bad block size: %d (expected <=2 for ext2 fs)\n",
			fs2h32(fs->e2fs_log_bsize));
#endif
		return (EINVAL);	   /* XXX needs translation */
	}
	if (fs2h32(fs->e2fs_rev) > E2FS_REV0) {
		if (fs2h32(fs->e2fs_first_ino) != EXT2_FIRSTINO ||
		    fs2h16(fs->e2fs_inode_size) != EXT2_DINODE_SIZE) {
			printf("Ext2 fs: unsupported inode size\n");
			return (EINVAL);      /* XXX needs translation */
		}
		if (fs2h32(fs->e2fs_features_incompat) &
		    ~EXT2F_INCOMPAT_SUPP) {
			printf("Ext2 fs: unsupported optional feature\n");
			return (EINVAL);      /* XXX needs translation */
		}
		if (!ronly && fs2h32(fs->e2fs_features_rocompat) &
		    ~EXT2F_ROCOMPAT_SUPP) {
			return (EROFS);      /* XXX needs translation */
		}
	}
	return (0);
}
