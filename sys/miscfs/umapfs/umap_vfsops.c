/*	$NetBSD: umap_vfsops.c,v 1.86 2010/11/19 06:44:46 dholland Exp $	*/

/*
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software donated to Berkeley by
 * the UCLA Ficus project.
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
 *	from: @(#)null_vfsops.c       1.5 (Berkeley) 7/10/92
 *	@(#)umap_vfsops.c	8.8 (Berkeley) 5/14/95
 */

/*
 * Umap Layer
 * (See mount_umap(8) for a description of this layer.)
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: umap_vfsops.c,v 1.86 2010/11/19 06:44:46 dholland Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/sysctl.h>
#include <sys/proc.h>
#include <sys/time.h>
#include <sys/vnode.h>
#include <sys/mount.h>
#include <sys/namei.h>
#include <sys/malloc.h>
#include <sys/kauth.h>
#include <sys/module.h>

#include <miscfs/umapfs/umap.h>
#include <miscfs/genfs/layer_extern.h>

MODULE(MODULE_CLASS_VFS, umap, "layerfs");

VFS_PROTOS(umapfs);

static struct sysctllog *umapfs_sysctl_log;

/*
 * Mount umap layer
 */
int
umapfs_mount(struct mount *mp, const char *path, void *data, size_t *data_len)
{
	struct lwp *l = curlwp;
	struct pathbuf *pb;
	struct nameidata nd;
	struct umap_args *args = data;
	struct vnode *lowerrootvp, *vp;
	struct umap_mount *amp;
	int error;
#ifdef UMAPFS_DIAGNOSTIC
	int i;
#endif

	if (*data_len < sizeof *args)
		return EINVAL;

	if (mp->mnt_flag & MNT_GETARGS) {
		amp = MOUNTTOUMAPMOUNT(mp);
		if (amp == NULL)
			return EIO;
		args->la.target = NULL;
		args->nentries = amp->info_nentries;
		args->gnentries = amp->info_gnentries;
		*data_len = sizeof *args;
		return 0;
	}

	/* only for root */
	if ((error = kauth_authorize_generic(l->l_cred, KAUTH_GENERIC_ISSUSER,
	    NULL)) != 0)
		return error;

#ifdef UMAPFS_DIAGNOSTIC
	printf("umapfs_mount(mp = %p)\n", mp);
#endif

	/*
	 * Update is not supported
	 */
	if (mp->mnt_flag & MNT_UPDATE)
		return EOPNOTSUPP;

	/*
	 * Find lower node
	 */
	error = pathbuf_copyin(args->umap_target, &pb);
	if (error) {
		return error;
	}
	NDINIT(&nd, LOOKUP, FOLLOW|LOCKLEAF, pb);
	if ((error = namei(&nd)) != 0) {
		pathbuf_destroy(pb);
		return error;
	}

	/*
	 * Sanity check on lower vnode
	 */
	lowerrootvp = nd.ni_vp;
	pathbuf_destroy(pb);
#ifdef UMAPFS_DIAGNOSTIC
	printf("vp = %p, check for VDIR...\n", lowerrootvp);
#endif

	if (lowerrootvp->v_type != VDIR) {
		vput(lowerrootvp);
		return (EINVAL);
	}

#ifdef UMAPFS_DIAGNOSTIC
	printf("mp = %p\n", mp);
#endif

	amp = (struct umap_mount *) malloc(sizeof(struct umap_mount),
				M_UFSMNT, M_WAITOK);	/* XXX */
	memset(amp, 0, sizeof(struct umap_mount));

	mp->mnt_data = amp;
	amp->umapm_vfs = lowerrootvp->v_mount;
	if (amp->umapm_vfs->mnt_flag & MNT_LOCAL)
		mp->mnt_flag |= MNT_LOCAL;

	/*
	 * Now copy in the number of entries and maps for umap mapping.
	 */
	if (args->nentries > MAPFILEENTRIES || args->gnentries > GMAPFILEENTRIES) {
		vput(lowerrootvp);
		return (error);
	}

	amp->info_nentries = args->nentries;
	amp->info_gnentries = args->gnentries;
	error = copyin(args->mapdata, amp->info_mapdata,
	    2*sizeof(u_long)*args->nentries);
	if (error) {
		vput(lowerrootvp);
		return (error);
	}

#ifdef UMAPFS_DIAGNOSTIC
	printf("umap_mount:nentries %d\n",args->nentries);
	for (i = 0; i < args->nentries; i++)
		printf("   %ld maps to %ld\n", amp->info_mapdata[i][0],
	 	    amp->info_mapdata[i][1]);
#endif

	error = copyin(args->gmapdata, amp->info_gmapdata,
	    2*sizeof(u_long)*args->gnentries);
	if (error) {
		vput(lowerrootvp);
		return (error);
	}

#ifdef UMAPFS_DIAGNOSTIC
	printf("umap_mount:gnentries %d\n",args->gnentries);
	for (i = 0; i < args->gnentries; i++)
		printf("\tgroup %ld maps to %ld\n",
		    amp->info_gmapdata[i][0],
	 	    amp->info_gmapdata[i][1]);
#endif

	/*
	 * Make sure the mount point's sufficiently initialized
	 * that the node create call will work.
	 */
	vfs_getnewfsid(mp);
	amp->umapm_size = sizeof(struct umap_node);
	amp->umapm_tag = VT_UMAP;
	amp->umapm_bypass = umap_bypass;
	amp->umapm_alloc = layer_node_alloc;	/* the default alloc is fine */
	amp->umapm_vnodeop_p = umap_vnodeop_p;
	mutex_init(&amp->umapm_hashlock, MUTEX_DEFAULT, IPL_NONE);
	amp->umapm_node_hashtbl = hashinit(NUMAPNODECACHE, HASH_LIST, true,
	    &amp->umapm_node_hash);


	/*
	 * fix up umap node for root vnode.
	 */
	error = layer_node_create(mp, lowerrootvp, &vp);
	/*
	 * Make sure the node alias worked
	 */
	if (error) {
		vput(lowerrootvp);
		hashdone(amp->umapm_node_hashtbl, HASH_LIST,
		    amp->umapm_node_hash);
		free(amp, M_UFSMNT);	/* XXX */
		return (error);
	}
	/*
	 * Unlock the node (either the lower or the alias)
	 */
	vp->v_vflag |= VV_ROOT;
	VOP_UNLOCK(vp);

	/*
	 * Keep a held reference to the root vnode.
	 * It is vrele'd in umapfs_unmount.
	 */
	amp->umapm_rootvp = vp;

	error = set_statvfs_info(path, UIO_USERSPACE, args->umap_target,
	    UIO_USERSPACE, mp->mnt_op->vfs_name, mp, l);
#ifdef UMAPFS_DIAGNOSTIC
	printf("umapfs_mount: lower %s, alias at %s\n",
		mp->mnt_stat.f_mntfromname, mp->mnt_stat.f_mntonname);
#endif
	return error;
}

/*
 * Free reference to umap layer
 */
int
umapfs_unmount(struct mount *mp, int mntflags)
{
	struct umap_mount *amp = MOUNTTOUMAPMOUNT(mp);
	struct vnode *rtvp = amp->umapm_rootvp;
	int error;
	int flags = 0;

#ifdef UMAPFS_DIAGNOSTIC
	printf("umapfs_unmount(mp = %p)\n", mp);
#endif

	if (mntflags & MNT_FORCE)
		flags |= FORCECLOSE;

	if (rtvp->v_usecount > 1 && (mntflags & MNT_FORCE) == 0)
		return (EBUSY);
	if ((error = vflush(mp, rtvp, flags)) != 0)
		return (error);

#ifdef UMAPFS_DIAGNOSTIC
	vprint("alias root of lower", rtvp);
#endif
	/*
	 * Blow it away for future re-use
	 */
	vgone(rtvp);
	/*
	 * Finally, throw away the umap_mount structure
	 */
	mutex_destroy(&amp->umapm_hashlock);
	hashdone(amp->umapm_node_hashtbl, HASH_LIST, amp->umapm_node_hash);
	free(amp, M_UFSMNT);	/* XXX */
	mp->mnt_data = NULL;
	return (0);
}

extern const struct vnodeopv_desc umapfs_vnodeop_opv_desc;

const struct vnodeopv_desc * const umapfs_vnodeopv_descs[] = {
	&umapfs_vnodeop_opv_desc,
	NULL,
};

struct vfsops umapfs_vfsops = {
	MOUNT_UMAP,
	sizeof (struct umap_args),
	umapfs_mount,
	layerfs_start,
	umapfs_unmount,
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
	umapfs_vnodeopv_descs,
	0,				/* vfs_refcount */
	{ NULL, NULL },
};

static int
umap_modcmd(modcmd_t cmd, void *arg)
{
	int error;

	switch (cmd) {
	case MODULE_CMD_INIT:
		error = vfs_attach(&umapfs_vfsops);
		if (error != 0)
			break;
		sysctl_createv(&umapfs_sysctl_log, 0, NULL, NULL,
			       CTLFLAG_PERMANENT,
			       CTLTYPE_NODE, "vfs", NULL,
			       NULL, 0, NULL, 0,
			       CTL_VFS, CTL_EOL);
		sysctl_createv(&umapfs_sysctl_log, 0, NULL, NULL,
			       CTLFLAG_PERMANENT,
			       CTLTYPE_NODE, "umap",
			       SYSCTL_DESCR("UID/GID remapping file system"),
			       NULL, 0, NULL, 0,
			       CTL_VFS, 10, CTL_EOL);
		/*
		 * XXX the "10" above could be dynamic, thereby eliminating
		 * one more instance of the "number to vfs" mapping problem,
		 * but "10" is the order as taken from sys/mount.h
		 */
		break;
	case MODULE_CMD_FINI:
		error = vfs_detach(&umapfs_vfsops);
		if (error != 0)
			break;
		sysctl_teardown(&umapfs_sysctl_log);
		break;
	default:
		error = ENOTTY;
		break;
	}

	return (error);
}
