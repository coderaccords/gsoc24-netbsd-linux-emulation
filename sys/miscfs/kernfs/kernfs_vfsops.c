/*	$NetBSD: kernfs_vfsops.c,v 1.55 2003/09/26 03:08:18 atatat Exp $	*/

/*
 * Copyright (c) 1992, 1993, 1995
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software donated to Berkeley by
 * Jan-Simon Pendry.
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
 *	@(#)kernfs_vfsops.c	8.10 (Berkeley) 5/14/95
 */

/*
 * Kernel params Filesystem
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: kernfs_vfsops.c,v 1.55 2003/09/26 03:08:18 atatat Exp $");

#ifdef _KERNEL_OPT
#include "opt_compat_netbsd.h"
#endif

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/proc.h>
#include <sys/vnode.h>
#include <sys/mount.h>
#include <sys/namei.h>
#include <sys/malloc.h>
#include <sys/syslog.h>

#include <miscfs/specfs/specdev.h>
#include <miscfs/kernfs/kernfs.h>

MALLOC_DEFINE(M_KERNFSMNT, "kernfs mount", "kernfs mount structures");

dev_t rrootdev = NODEV;

void	kernfs_init __P((void));
void	kernfs_reinit __P((void));
void	kernfs_done __P((void));
void	kernfs_get_rrootdev __P((void));
int	kernfs_mount __P((struct mount *, const char *, void *,
	    struct nameidata *, struct proc *));
int	kernfs_start __P((struct mount *, int, struct proc *));
int	kernfs_unmount __P((struct mount *, int, struct proc *));
int	kernfs_statfs __P((struct mount *, struct statfs *, struct proc *));
int	kernfs_quotactl __P((struct mount *, int, uid_t, caddr_t,
			     struct proc *));
int	kernfs_sync __P((struct mount *, int, struct ucred *, struct proc *));
int	kernfs_vget __P((struct mount *, ino_t, struct vnode **));
int	kernfs_fhtovp __P((struct mount *, struct fid *, struct vnode **));
int	kernfs_checkexp __P((struct mount *, struct mbuf *, int *,
			   struct ucred **));
int	kernfs_vptofh __P((struct vnode *, struct fid *));
int	kernfs_sysctl __P((int *, u_int, void *, size_t *, void *, size_t,
			   struct proc *));

void
kernfs_init()
{
#ifdef _LKM
	malloc_type_attach(M_KERNFSMNT);
#endif
	kernfs_hashinit();
}

void
kernfs_reinit()
{
	kernfs_hashreinit();
}

void
kernfs_done()
{
#ifdef _LKM
	malloc_type_detach(M_KERNFSMNT);
#endif
	kernfs_hashdone();
}

void
kernfs_get_rrootdev()
{
	static int tried = 0;

	if (tried) {
		/* Already did it once. */
		return;
	}
	tried = 1;

	if (rootdev == NODEV)
		return;
	rrootdev = devsw_blk2chr(rootdev);
	if (rrootdev != NODEV)
		return;
	rrootdev = NODEV;
	printf("kernfs_get_rrootdev: no raw root device\n");
}

/*
 * Mount the Kernel params filesystem
 */
int
kernfs_mount(mp, path, data, ndp, p)
	struct mount *mp;
	const char *path;
	void *data;
	struct nameidata *ndp;
	struct proc *p;
{
	int error = 0;
	struct kernfs_mount *fmp;

	if (UIO_MX & (UIO_MX - 1)) {
		log(LOG_ERR, "kernfs: invalid directory entry size");
		return (EINVAL);
	}

	if (mp->mnt_flag & MNT_GETARGS)
		return 0;
	/*
	 * Update is a no-op
	 */
	if (mp->mnt_flag & MNT_UPDATE)
		return (EOPNOTSUPP);

	MALLOC(fmp, struct kernfs_mount *, sizeof(struct kernfs_mount),
	    M_KERNFSMNT, M_WAITOK);
	memset(fmp, 0, sizeof(*fmp));
	TAILQ_INIT(&fmp->nodelist);

	mp->mnt_data = fmp;
	mp->mnt_flag |= MNT_LOCAL;
	vfs_getnewfsid(mp);

	error = set_statfs_info(path, UIO_USERSPACE, "kernfs", UIO_SYSSPACE,
	    mp, p);

	kernfs_get_rrootdev();
	return error;
}

int
kernfs_start(mp, flags, p)
	struct mount *mp;
	int flags;
	struct proc *p;
{

	return (0);
}

int
kernfs_unmount(mp, mntflags, p)
	struct mount *mp;
	int mntflags;
	struct proc *p;
{
	int error;
	int flags = 0;

	if (mntflags & MNT_FORCE)
		flags |= FORCECLOSE;

	if ((error = vflush(mp, 0, flags)) != 0)
		return (error);

	/*
	 * Finally, throw away the kernfs_mount structure
	 */
	free(mp->mnt_data, M_KERNFSMNT);
	mp->mnt_data = 0;
	return (0);
}

int
kernfs_root(mp, vpp)
	struct mount *mp;
	struct vnode **vpp;
{

	/* setup "." */
	return (kernfs_allocvp(mp, vpp, Kern, &kern_targets[0], 0));
}

int
kernfs_quotactl(mp, cmd, uid, arg, p)
	struct mount *mp;
	int cmd;
	uid_t uid;
	caddr_t arg;
	struct proc *p;
{

	return (EOPNOTSUPP);
}

int
kernfs_statfs(mp, sbp, p)
	struct mount *mp;
	struct statfs *sbp;
	struct proc *p;
{

	sbp->f_bsize = DEV_BSIZE;
	sbp->f_iosize = DEV_BSIZE;
	sbp->f_blocks = 2;		/* 1K to keep df happy */
	sbp->f_bfree = 0;
	sbp->f_bavail = 0;
	sbp->f_files = 1024;	/* XXX lie */
	sbp->f_ffree = 128;	/* XXX lie */
#ifdef COMPAT_09
	sbp->f_type = 7;
#else
	sbp->f_type = 0;
#endif
	copy_statfs_info(sbp, mp);
	return (0);
}

/*ARGSUSED*/
int
kernfs_sync(mp, waitfor, uc, p)
	struct mount *mp;
	int waitfor;
	struct ucred *uc;
	struct proc *p;
{

	return (0);
}

/*
 * Kernfs flat namespace lookup.
 * Currently unsupported.
 */
int
kernfs_vget(mp, ino, vpp)
	struct mount *mp;
	ino_t ino;
	struct vnode **vpp;
{

	return (EOPNOTSUPP);
}

/*ARGSUSED*/
int
kernfs_fhtovp(mp, fhp, vpp)
	struct mount *mp;
	struct fid *fhp;
	struct vnode **vpp;
{

	return (EOPNOTSUPP);
}

/*ARGSUSED*/
int
kernfs_checkexp(mp, mb, what, anon)
	struct mount *mp;
	struct mbuf *mb;
	int *what;
	struct ucred **anon;
{

	return (EOPNOTSUPP);
}

/*ARGSUSED*/
int
kernfs_vptofh(vp, fhp)
	struct vnode *vp;
	struct fid *fhp;
{

	return (EOPNOTSUPP);
}

int
kernfs_sysctl(name, namelen, oldp, oldlenp, newp, newlen, p)
	int *name;
	u_int namelen;
	void *oldp;
	size_t *oldlenp;
	void *newp;
	size_t newlen;
	struct proc *p;
{
	return (EOPNOTSUPP);
}

extern const struct vnodeopv_desc kernfs_vnodeop_opv_desc;

const struct vnodeopv_desc * const kernfs_vnodeopv_descs[] = {
	&kernfs_vnodeop_opv_desc,
	NULL,
};

struct vfsops kernfs_vfsops = {
	MOUNT_KERNFS,
	kernfs_mount,
	kernfs_start,
	kernfs_unmount,
	kernfs_root,
	kernfs_quotactl,
	kernfs_statfs,
	kernfs_sync,
	kernfs_vget,
	kernfs_fhtovp,
	kernfs_vptofh,
	kernfs_init,
	kernfs_reinit,
	kernfs_done,
	kernfs_sysctl,
	NULL,				/* vfs_mountroot */
	kernfs_checkexp,
	kernfs_vnodeopv_descs,
};
