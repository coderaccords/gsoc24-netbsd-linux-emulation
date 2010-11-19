/*	$NetBSD: null_vfsops.c,v 1.83 2010/11/19 06:44:46 dholland Exp $	*/

/*
 * Copyright (c) 1999 National Aeronautics & Space Administration
 * All rights reserved.
 *
 * This software was written by William Studenmund of the
 * Numerical Aerospace Simulation Facility, NASA Ames Research Center.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the National Aeronautics & Space Administration
 *    nor the names of its contributors may be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NATIONAL AERONAUTICS & SPACE ADMINISTRATION
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE ADMINISTRATION OR CONTRIB-
 * UTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

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
 *	from: Id: lofs_vfsops.c,v 1.9 1992/05/30 10:26:24 jsp Exp
 *	from: @(#)lofs_vfsops.c	1.2 (Berkeley) 6/18/92
 *	@(#)null_vfsops.c	8.7 (Berkeley) 5/14/95
 */

/*
 * Null file-system: VFS operations.
 *
 * See null_vnops.c for a description.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: null_vfsops.c,v 1.83 2010/11/19 06:44:46 dholland Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/sysctl.h>
#include <sys/vnode.h>
#include <sys/mount.h>
#include <sys/namei.h>
#include <sys/malloc.h>
#include <sys/module.h>

#include <miscfs/nullfs/null.h>
#include <miscfs/genfs/layer_extern.h>

MODULE(MODULE_CLASS_VFS, null, "layerfs");

VFS_PROTOS(nullfs);

static struct sysctllog *nullfs_sysctl_log;

int
nullfs_mount(struct mount *mp, const char *path, void *data, size_t *data_len)
{
	struct vnode *lowerrootvp, *vp;
	struct null_args *args = data;
	struct null_mount *nmp;
	struct layer_mount *lmp;
	struct pathbuf *pb;
	struct nameidata nd;
	int error;

	if (*data_len < sizeof(*args))
		return EINVAL;

	if (mp->mnt_flag & MNT_GETARGS) {
		lmp = MOUNTTOLAYERMOUNT(mp);
		if (lmp == NULL)
			return EIO;
		args->la.target = NULL;
		*data_len = sizeof(*args);
		return 0;
	}

	/* Update is not supported. */
	if (mp->mnt_flag & MNT_UPDATE)
		return EOPNOTSUPP;

	/* Find the lower vnode and lock it. */
	error = pathbuf_copyin(args->la.target, &pb);
	if (error) {
		return error;
	}
	NDINIT(&nd, LOOKUP, FOLLOW|LOCKLEAF, pb);
	if ((error = namei(&nd)) != 0) {
		pathbuf_destroy(pb);
		return error;
	}
	lowerrootvp = nd.ni_vp;
	pathbuf_destroy(pb);

	/* Create the mount point. */
	nmp = malloc(sizeof(struct null_mount), M_UFSMNT, M_WAITOK | M_ZERO);
	mp->mnt_data = nmp;
	nmp->nullm_vfs = lowerrootvp->v_mount;
	if (nmp->nullm_vfs->mnt_flag & MNT_LOCAL)
		mp->mnt_flag |= MNT_LOCAL;

	/*
	 * Make sure that the mount point is sufficiently initialized
	 * that the node create call will work.
	 */
	vfs_getnewfsid(mp);

	nmp->nullm_size = sizeof(struct null_node);
	nmp->nullm_tag = VT_NULL;
	nmp->nullm_bypass = layer_bypass;
	nmp->nullm_alloc = layer_node_alloc;	/* the default alloc is fine */
	nmp->nullm_vnodeop_p = null_vnodeop_p;
	mutex_init(&nmp->nullm_hashlock, MUTEX_DEFAULT, IPL_NONE);
	nmp->nullm_node_hashtbl = hashinit(desiredvnodes, HASH_LIST, true,
	    &nmp->nullm_node_hash);

	/* Setup a null node for root vnode. */
	error = layer_node_create(mp, lowerrootvp, &vp);
	if (error) {
		vput(lowerrootvp);
		hashdone(nmp->nullm_node_hashtbl, HASH_LIST,
		    nmp->nullm_node_hash);
		free(nmp, M_UFSMNT);
		return error;
	}
	/*
	 * Keep a held reference to the root vnode.  It will be released on
	 * umount.  Note: nullfs is MP-safe.
	 */
	vp->v_vflag |= VV_ROOT;
	nmp->nullm_rootvp = vp;
	mp->mnt_iflag |= IMNT_MPSAFE;
	VOP_UNLOCK(vp);

	error = set_statvfs_info(path, UIO_USERSPACE, args->la.target,
	    UIO_USERSPACE, mp->mnt_op->vfs_name, mp, curlwp);
	return error;
}

int
nullfs_unmount(struct mount *mp, int mntflags)
{
	struct null_mount *nmp = MOUNTTONULLMOUNT(mp);
	struct vnode *null_rootvp = nmp->nullm_rootvp;
	int error, flags = 0;

	if (mntflags & MNT_FORCE)
		flags |= FORCECLOSE;

	if (null_rootvp->v_usecount > 1 && (mntflags & MNT_FORCE) == 0)
		return EBUSY;

	if ((error = vflush(mp, null_rootvp, flags)) != 0)
		return error;

	/* Eliminate all activity and release the vnode. */
	vgone(null_rootvp);

	/* Finally, destroy the mount point structures. */
	hashdone(nmp->nullm_node_hashtbl, HASH_LIST, nmp->nullm_node_hash);
	mutex_destroy(&nmp->nullm_hashlock);
	free(mp->mnt_data, M_UFSMNT);
	mp->mnt_data = NULL;
	return 0;
}

extern const struct vnodeopv_desc null_vnodeop_opv_desc;

const struct vnodeopv_desc * const nullfs_vnodeopv_descs[] = {
	&null_vnodeop_opv_desc,
	NULL,
};

struct vfsops nullfs_vfsops = {
	MOUNT_NULL,
	sizeof (struct null_args),
	nullfs_mount,
	layerfs_start,
	nullfs_unmount,
	layerfs_root,
	layerfs_quotactl,
	layerfs_statvfs,
	layerfs_sync,
	layerfs_vget,
	layerfs_fhtovp,
	layerfs_vptofh,
	layerfs_init,
	NULL,
	layerfs_done,
	NULL,				/* vfs_mountroot */
	layerfs_snapshot,
	vfs_stdextattrctl,
	(void *)eopnotsupp,		/* vfs_suspendctl */
	layerfs_renamelock_enter,
	layerfs_renamelock_exit,
	(void *)eopnotsupp,
	nullfs_vnodeopv_descs,
	0,
	{ NULL, NULL },
};

static int
null_modcmd(modcmd_t cmd, void *arg)
{
	int error;

	switch (cmd) {
	case MODULE_CMD_INIT:
		error = vfs_attach(&nullfs_vfsops);
		if (error != 0)
			break;
		sysctl_createv(&nullfs_sysctl_log, 0, NULL, NULL,
		    CTLFLAG_PERMANENT,
		    CTLTYPE_NODE, "vfs", NULL,
		    NULL, 0, NULL, 0,
		    CTL_VFS, CTL_EOL);
		sysctl_createv(&nullfs_sysctl_log, 0, NULL, NULL,
		    CTLFLAG_PERMANENT,
		    CTLTYPE_NODE, "null",
		    SYSCTL_DESCR("Loopback file system"),
		    NULL, 0, NULL, 0,
		    CTL_VFS, 9, CTL_EOL);
		/*
		 * XXX the "9" above could be dynamic, thereby eliminating
		 * one more instance of the "number to vfs" mapping problem,
		 * but "9" is the order as taken from sys/mount.h
		 */
		break;
	case MODULE_CMD_FINI:
		error = vfs_detach(&nullfs_vfsops);
		if (error != 0)
			break;
		sysctl_teardown(&nullfs_sysctl_log);
		break;
	default:
		error = ENOTTY;
		break;
	}
	return error;
}
