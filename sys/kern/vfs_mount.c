/*	$NetBSD: vfs_mount.c,v 1.3 2011/04/02 06:48:20 rmind Exp $	*/

/*-
 * Copyright (c) 1997-2011 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe of the Numerical Aerospace Simulation Facility,
 * NASA Ames Research Center, by Charles M. Hannum, and by Andrew Doran.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
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
 * Copyright (c) 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
 * (c) UNIX System Laboratories, Inc.
 * All or some portions of this file are derived from material licensed
 * to the University of California by American Telephone and Telegraph
 * Co. or Unix System Laboratories, Inc. and are reproduced herein with
 * the permission of UNIX System Laboratories, Inc.
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
 *	@(#)vfs_subr.c	8.13 (Berkeley) 4/18/94
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: vfs_mount.c,v 1.3 2011/04/02 06:48:20 rmind Exp $");

#include <sys/param.h>
#include <sys/kernel.h>

#include <sys/atomic.h>
#include <sys/buf.h>
#include <sys/conf.h>
#include <sys/fcntl.h>
#include <sys/filedesc.h>
#include <sys/device.h>
#include <sys/kauth.h>
#include <sys/kmem.h>
#include <sys/module.h>
#include <sys/mount.h>
#include <sys/namei.h>
#include <sys/syscallargs.h>
#include <sys/sysctl.h>
#include <sys/systm.h>
#include <sys/vnode.h>

#include <miscfs/genfs/genfs.h>
#include <miscfs/syncfs/syncfs.h>
#include <miscfs/specfs/specdev.h>

/* Root filesystem and device. */
vnode_t *			rootvnode;
struct device *			root_device;

/* Mounted filesystem list. */
struct mntlist			mountlist;
kmutex_t			mountlist_lock;

kmutex_t			mntvnode_lock;
kmutex_t			vfs_list_lock;

static specificdata_domain_t	mount_specificdata_domain;
static kmutex_t			mntid_lock;

static kmutex_t			mountgen_lock;
static uint64_t			mountgen;

void
vfs_mount_sysinit(void)
{

	CIRCLEQ_INIT(&mountlist);
	mutex_init(&mountlist_lock, MUTEX_DEFAULT, IPL_NONE);
	mutex_init(&mntvnode_lock, MUTEX_DEFAULT, IPL_NONE);
	mutex_init(&vfs_list_lock, MUTEX_DEFAULT, IPL_NONE);

	mount_specificdata_domain = specificdata_domain_create();
	mutex_init(&mntid_lock, MUTEX_DEFAULT, IPL_NONE);
	mutex_init(&mountgen_lock, MUTEX_DEFAULT, IPL_NONE);
	mountgen = 0;
}

struct mount *
vfs_mountalloc(struct vfsops *vfsops, vnode_t *vp)
{
	struct mount *mp;
	int error;

	mp = kmem_zalloc(sizeof(*mp), KM_SLEEP);
	if (mp == NULL)
		return NULL;

	mp->mnt_op = vfsops;
	mp->mnt_refcnt = 1;
	TAILQ_INIT(&mp->mnt_vnodelist);
	rw_init(&mp->mnt_unmounting);
	mutex_init(&mp->mnt_renamelock, MUTEX_DEFAULT, IPL_NONE);
	mutex_init(&mp->mnt_updating, MUTEX_DEFAULT, IPL_NONE);
	error = vfs_busy(mp, NULL);
	KASSERT(error == 0);
	mp->mnt_vnodecovered = vp;
	mount_initspecific(mp);

	mutex_enter(&mountgen_lock);
	mp->mnt_gen = mountgen++;
	mutex_exit(&mountgen_lock);

	return mp;
}

/*
 * vfs_rootmountalloc: lookup a filesystem type, and if found allocate and
 * initialize a mount structure for it.
 *
 * Devname is usually updated by mount(8) after booting.
 */
int
vfs_rootmountalloc(const char *fstypename, const char *devname,
    struct mount **mpp)
{
	struct vfsops *vfsp = NULL;
	struct mount *mp;

	mutex_enter(&vfs_list_lock);
	LIST_FOREACH(vfsp, &vfs_list, vfs_list)
		if (!strncmp(vfsp->vfs_name, fstypename, 
		    sizeof(mp->mnt_stat.f_fstypename)))
			break;
	if (vfsp == NULL) {
		mutex_exit(&vfs_list_lock);
		return (ENODEV);
	}
	vfsp->vfs_refcount++;
	mutex_exit(&vfs_list_lock);

	if ((mp = vfs_mountalloc(vfsp, NULL)) == NULL)
		return ENOMEM;
	mp->mnt_flag = MNT_RDONLY;
	(void)strlcpy(mp->mnt_stat.f_fstypename, vfsp->vfs_name,
	    sizeof(mp->mnt_stat.f_fstypename));
	mp->mnt_stat.f_mntonname[0] = '/';
	mp->mnt_stat.f_mntonname[1] = '\0';
	mp->mnt_stat.f_mntfromname[sizeof(mp->mnt_stat.f_mntfromname) - 1] =
	    '\0';
	(void)copystr(devname, mp->mnt_stat.f_mntfromname,
	    sizeof(mp->mnt_stat.f_mntfromname) - 1, 0);
	*mpp = mp;
	return 0;
}

/*
 * vfs_getnewfsid: get a new unique fsid.
 */
void
vfs_getnewfsid(struct mount *mp)
{
	static u_short xxxfs_mntid;
	fsid_t tfsid;
	int mtype;

	mutex_enter(&mntid_lock);
	mtype = makefstype(mp->mnt_op->vfs_name);
	mp->mnt_stat.f_fsidx.__fsid_val[0] = makedev(mtype, 0);
	mp->mnt_stat.f_fsidx.__fsid_val[1] = mtype;
	mp->mnt_stat.f_fsid = mp->mnt_stat.f_fsidx.__fsid_val[0];
	if (xxxfs_mntid == 0)
		++xxxfs_mntid;
	tfsid.__fsid_val[0] = makedev(mtype & 0xff, xxxfs_mntid);
	tfsid.__fsid_val[1] = mtype;
	if (!CIRCLEQ_EMPTY(&mountlist)) {
		while (vfs_getvfs(&tfsid)) {
			tfsid.__fsid_val[0]++;
			xxxfs_mntid++;
		}
	}
	mp->mnt_stat.f_fsidx.__fsid_val[0] = tfsid.__fsid_val[0];
	mp->mnt_stat.f_fsid = mp->mnt_stat.f_fsidx.__fsid_val[0];
	mutex_exit(&mntid_lock);
}

/*
 * Lookup a mount point by filesystem identifier.
 *
 * XXX Needs to add a reference to the mount point.
 */
struct mount *
vfs_getvfs(fsid_t *fsid)
{
	struct mount *mp;

	mutex_enter(&mountlist_lock);
	CIRCLEQ_FOREACH(mp, &mountlist, mnt_list) {
		if (mp->mnt_stat.f_fsidx.__fsid_val[0] == fsid->__fsid_val[0] &&
		    mp->mnt_stat.f_fsidx.__fsid_val[1] == fsid->__fsid_val[1]) {
			mutex_exit(&mountlist_lock);
			return (mp);
		}
	}
	mutex_exit(&mountlist_lock);
	return NULL;
}

/*
 * Drop a reference to a mount structure, freeing if the last reference.
 */
void
vfs_destroy(struct mount *mp)
{

	if (__predict_true((int)atomic_dec_uint_nv(&mp->mnt_refcnt) > 0)) {
		return;
	}

	/*
	 * Nothing else has visibility of the mount: we can now
	 * free the data structures.
	 */
	KASSERT(mp->mnt_refcnt == 0);
	specificdata_fini(mount_specificdata_domain, &mp->mnt_specdataref);
	rw_destroy(&mp->mnt_unmounting);
	mutex_destroy(&mp->mnt_updating);
	mutex_destroy(&mp->mnt_renamelock);
	if (mp->mnt_op != NULL) {
		vfs_delref(mp->mnt_op);
	}
	kmem_free(mp, sizeof(*mp));
}

/*
 * Mark a mount point as busy, and gain a new reference to it.  Used to
 * prevent the file system from being unmounted during critical sections.
 *
 * => The caller must hold a pre-existing reference to the mount.
 * => Will fail if the file system is being unmounted, or is unmounted.
 */
int
vfs_busy(struct mount *mp, struct mount **nextp)
{

	KASSERT(mp->mnt_refcnt > 0);

	if (__predict_false(!rw_tryenter(&mp->mnt_unmounting, RW_READER))) {
		if (nextp != NULL) {
			KASSERT(mutex_owned(&mountlist_lock));
			*nextp = CIRCLEQ_NEXT(mp, mnt_list);
		}
		return EBUSY;
	}
	if (__predict_false((mp->mnt_iflag & IMNT_GONE) != 0)) {
		rw_exit(&mp->mnt_unmounting);
		if (nextp != NULL) {
			KASSERT(mutex_owned(&mountlist_lock));
			*nextp = CIRCLEQ_NEXT(mp, mnt_list);
		}
		return ENOENT;
	}
	if (nextp != NULL) {
		mutex_exit(&mountlist_lock);
	}
	atomic_inc_uint(&mp->mnt_refcnt);
	return 0;
}

/*
 * Unbusy a busy filesystem.
 *
 * => If keepref is true, preserve reference added by vfs_busy().
 * => If nextp != NULL, acquire mountlist_lock.
 */
void
vfs_unbusy(struct mount *mp, bool keepref, struct mount **nextp)
{

	KASSERT(mp->mnt_refcnt > 0);

	if (nextp != NULL) {
		mutex_enter(&mountlist_lock);
	}
	rw_exit(&mp->mnt_unmounting);
	if (!keepref) {
		vfs_destroy(mp);
	}
	if (nextp != NULL) {
		KASSERT(mutex_owned(&mountlist_lock));
		*nextp = CIRCLEQ_NEXT(mp, mnt_list);
	}
}

/*
 * Insert a marker vnode into a mount's vnode list, after the
 * specified vnode.  mntvnode_lock must be held.
 */
void
vmark(vnode_t *mvp, vnode_t *vp)
{
	struct mount *mp = mvp->v_mount;

	KASSERT(mutex_owned(&mntvnode_lock));
	KASSERT((mvp->v_iflag & VI_MARKER) != 0);
	KASSERT(vp->v_mount == mp);

	TAILQ_INSERT_AFTER(&mp->mnt_vnodelist, vp, mvp, v_mntvnodes);
}

/*
 * Remove a marker vnode from a mount's vnode list, and return
 * a pointer to the next vnode in the list.  mntvnode_lock must
 * be held.
 */
vnode_t *
vunmark(vnode_t *mvp)
{
	struct mount *mp = mvp->v_mount;
	vnode_t *vp;

	KASSERT(mutex_owned(&mntvnode_lock));
	KASSERT((mvp->v_iflag & VI_MARKER) != 0);

	vp = TAILQ_NEXT(mvp, v_mntvnodes);
	TAILQ_REMOVE(&mp->mnt_vnodelist, mvp, v_mntvnodes); 

	KASSERT(vp == NULL || vp->v_mount == mp);

	return vp;
}

/*
 * Move a vnode from one mount queue to another.
 */
void
vfs_insmntque(vnode_t *vp, struct mount *mp)
{
	struct mount *omp;

#ifdef DIAGNOSTIC
	if ((mp != NULL) &&
	    (mp->mnt_iflag & IMNT_UNMOUNT) &&
	    vp->v_tag != VT_VFS) {
		panic("insmntque into dying filesystem");
	}
#endif

	mutex_enter(&mntvnode_lock);
	/*
	 * Delete from old mount point vnode list, if on one.
	 */
	if ((omp = vp->v_mount) != NULL)
		TAILQ_REMOVE(&vp->v_mount->mnt_vnodelist, vp, v_mntvnodes);
	/*
	 * Insert into list of vnodes for the new mount point, if
	 * available.  The caller must take a reference on the mount
	 * structure and donate to the vnode.
	 */
	if ((vp->v_mount = mp) != NULL)
		TAILQ_INSERT_TAIL(&mp->mnt_vnodelist, vp, v_mntvnodes);
	mutex_exit(&mntvnode_lock);

	if (omp != NULL) {
		/* Release reference to old mount. */
		vfs_destroy(omp);
	}
}

/*
 * Remove any vnodes in the vnode table belonging to mount point mp.
 *
 * If FORCECLOSE is not specified, there should not be any active ones,
 * return error if any are found (nb: this is a user error, not a
 * system error). If FORCECLOSE is specified, detach any active vnodes
 * that are found.
 *
 * If WRITECLOSE is set, only flush out regular file vnodes open for
 * writing.
 *
 * SKIPSYSTEM causes any vnodes marked VV_SYSTEM to be skipped.
 */
#ifdef DEBUG
int busyprt = 0;	/* print out busy vnodes */
struct ctldebug debug1 = { "busyprt", &busyprt };
#endif

static vnode_t *
vflushnext(vnode_t *mvp, int *when)
{

	if (hardclock_ticks > *when) {
		mutex_exit(&mntvnode_lock);
		yield();
		mutex_enter(&mntvnode_lock);
		*when = hardclock_ticks + hz / 10;
	}
	return vunmark(mvp);
}

int
vflush(struct mount *mp, vnode_t *skipvp, int flags)
{
	vnode_t *vp, *mvp;
	int busy = 0, when = 0;

	/* First, flush out any vnode references from vrele_list. */
	vrele_flush();

	/* Allocate a marker vnode. */
	mvp = vnalloc(mp);
	if (mvp == NULL) {
		return ENOMEM;
	}

	/*
	 * NOTE: not using the TAILQ_FOREACH here since in this loop vgone()
	 * and vclean() are called.
	 */
	mutex_enter(&mntvnode_lock);
	for (vp = TAILQ_FIRST(&mp->mnt_vnodelist); vp != NULL;
	    vp = vflushnext(mvp, &when)) {
		vmark(mvp, vp);
		if (vp->v_mount != mp || vismarker(vp))
			continue;
		/*
		 * Skip over a selected vnode.
		 */
		if (vp == skipvp)
			continue;
		mutex_enter(&vp->v_interlock);
		/*
		 * Ignore clean but still referenced vnodes.
		 */
		if ((vp->v_iflag & VI_CLEAN) != 0) {
			mutex_exit(&vp->v_interlock);
			continue;
		}
		/*
		 * Skip over a vnodes marked VSYSTEM.
		 */
		if ((flags & SKIPSYSTEM) && (vp->v_vflag & VV_SYSTEM)) {
			mutex_exit(&vp->v_interlock);
			continue;
		}
		/*
		 * If WRITECLOSE is set, only flush out regular file
		 * vnodes open for writing.
		 */
		if ((flags & WRITECLOSE) &&
		    (vp->v_writecount == 0 || vp->v_type != VREG)) {
			mutex_exit(&vp->v_interlock);
			continue;
		}
		/*
		 * With v_usecount == 0, all we need to do is clear
		 * out the vnode data structures and we are done.
		 */
		if (vp->v_usecount == 0) {
			mutex_exit(&mntvnode_lock);
			vremfree(vp);
			vp->v_usecount = 1;
			vclean(vp, DOCLOSE);
			vrelel(vp, 0);
			mutex_enter(&mntvnode_lock);
			continue;
		}
		/*
		 * If FORCECLOSE is set, forcibly close the vnode.
		 * For block or character devices, revert to an
		 * anonymous device.  For all other files, just
		 * kill them.
		 */
		if (flags & FORCECLOSE) {
			mutex_exit(&mntvnode_lock);
			atomic_inc_uint(&vp->v_usecount);
			if (vp->v_type != VBLK && vp->v_type != VCHR) {
				vclean(vp, DOCLOSE);
				vrelel(vp, 0);
			} else {
				vclean(vp, 0);
				vp->v_op = spec_vnodeop_p; /* XXXSMP */
				mutex_exit(&vp->v_interlock);
				/*
				 * The vnode isn't clean, but still resides
				 * on the mount list.  Remove it. XXX This
				 * is a bit dodgy.
				 */
				vfs_insmntque(vp, NULL);
				vrele(vp);
			}
			mutex_enter(&mntvnode_lock);
			continue;
		}
#ifdef DEBUG
		if (busyprt)
			vprint("vflush: busy vnode", vp);
#endif
		mutex_exit(&vp->v_interlock);
		busy++;
	}
	mutex_exit(&mntvnode_lock);
	vnfree(mvp);
	if (busy)
		return (EBUSY);
	return (0);
}

/*
 * Remove clean vnodes from a mountpoint's vnode list.
 */
void
vfs_scrubvnlist(struct mount *mp)
{
	vnode_t *vp, *nvp;

retry:
	mutex_enter(&mntvnode_lock);
	for (vp = TAILQ_FIRST(&mp->mnt_vnodelist); vp; vp = nvp) {
		nvp = TAILQ_NEXT(vp, v_mntvnodes);
		mutex_enter(&vp->v_interlock);
		if ((vp->v_iflag & VI_CLEAN) != 0) {
			TAILQ_REMOVE(&mp->mnt_vnodelist, vp, v_mntvnodes);
			vp->v_mount = NULL;
			mutex_exit(&mntvnode_lock);
			mutex_exit(&vp->v_interlock);
			vfs_destroy(mp);
			goto retry;
		}
		mutex_exit(&vp->v_interlock);
	}
	mutex_exit(&mntvnode_lock);
}

/*
 * Mount a file system.
 */

/*
 * Scan all active processes to see if any of them have a current or root
 * directory onto which the new filesystem has just been  mounted. If so,
 * replace them with the new mount point.
 */
static void
mount_checkdirs(vnode_t *olddp)
{
	vnode_t *newdp, *rele1, *rele2;
	struct cwdinfo *cwdi;
	struct proc *p;
	bool retry;

	if (olddp->v_usecount == 1) {
		return;
	}
	if (VFS_ROOT(olddp->v_mountedhere, &newdp))
		panic("mount: lost mount");

	do {
		retry = false;
		mutex_enter(proc_lock);
		PROCLIST_FOREACH(p, &allproc) {
			if ((cwdi = p->p_cwdi) == NULL)
				continue;
			/*
			 * Cannot change to the old directory any more,
			 * so even if we see a stale value it is not a
			 * problem.
			 */
			if (cwdi->cwdi_cdir != olddp &&
			    cwdi->cwdi_rdir != olddp)
				continue;
			retry = true;
			rele1 = NULL;
			rele2 = NULL;
			atomic_inc_uint(&cwdi->cwdi_refcnt);
			mutex_exit(proc_lock);
			rw_enter(&cwdi->cwdi_lock, RW_WRITER);
			if (cwdi->cwdi_cdir == olddp) {
				rele1 = cwdi->cwdi_cdir;
				vref(newdp);
				cwdi->cwdi_cdir = newdp;
			}
			if (cwdi->cwdi_rdir == olddp) {
				rele2 = cwdi->cwdi_rdir;
				vref(newdp);
				cwdi->cwdi_rdir = newdp;
			}
			rw_exit(&cwdi->cwdi_lock);
			cwdfree(cwdi);
			if (rele1 != NULL)
				vrele(rele1);
			if (rele2 != NULL)
				vrele(rele2);
			mutex_enter(proc_lock);
			break;
		}
		mutex_exit(proc_lock);
	} while (retry);

	if (rootvnode == olddp) {
		vrele(rootvnode);
		vref(newdp);
		rootvnode = newdp;
	}
	vput(newdp);
}

int
mount_domount(struct lwp *l, vnode_t **vpp, struct vfsops *vfsops,
    const char *path, int flags, void *data, size_t *data_len)
{
	vnode_t *vp = *vpp;
	struct mount *mp;
	struct vattr va;
	struct pathbuf *pb;
	struct nameidata nd;
	int error;

	error = kauth_authorize_system(l->l_cred, KAUTH_SYSTEM_MOUNT,
	    KAUTH_REQ_SYSTEM_MOUNT_NEW, vp, KAUTH_ARG(flags), data);
	if (error) {
		vfs_delref(vfsops);
		return error;
	}

	/* Cannot make a non-dir a mount-point (from here anyway). */
	if (vp->v_type != VDIR) {
		vfs_delref(vfsops);
		return ENOTDIR;
	}

	/*
	 * If the user is not root, ensure that they own the directory
	 * onto which we are attempting to mount.
	 */
	if ((error = VOP_GETATTR(vp, &va, l->l_cred)) != 0 ||
	    (va.va_uid != kauth_cred_geteuid(l->l_cred) &&
	    (error = kauth_authorize_generic(l->l_cred,
	    KAUTH_GENERIC_ISSUSER, NULL)) != 0)) {
		vfs_delref(vfsops);
		return error;
	}

	if (flags & MNT_EXPORTED) {
		vfs_delref(vfsops);
		return EINVAL;
	}

	if ((mp = vfs_mountalloc(vfsops, vp)) == NULL) {
		vfs_delref(vfsops);
		return ENOMEM;
	}

	mp->mnt_stat.f_owner = kauth_cred_geteuid(l->l_cred);

	/*
	 * The underlying file system may refuse the mount for
	 * various reasons.  Allow the user to force it to happen.
	 *
	 * Set the mount level flags.
	 */
	mp->mnt_flag = flags &
	   (MNT_FORCE | MNT_NOSUID | MNT_NOEXEC | MNT_NODEV |
	    MNT_SYNCHRONOUS | MNT_UNION | MNT_ASYNC | MNT_NOCOREDUMP |
	    MNT_NOATIME | MNT_NODEVMTIME | MNT_SYMPERM | MNT_SOFTDEP |
	    MNT_LOG | MNT_IGNORE | MNT_RDONLY);

	mutex_enter(&mp->mnt_updating);
	error = VFS_MOUNT(mp, path, data, data_len);
	mp->mnt_flag &= ~MNT_OP_FLAGS;

	if (error != 0)
		goto err_unmounted;

	/*
	 * Validate and prepare the mount point.
	 */
	error = pathbuf_copyin(path, &pb);
	if (error != 0) {
		goto err_mounted;
	}
	NDINIT(&nd, LOOKUP, FOLLOW | LOCKLEAF | TRYEMULROOT, pb);
	error = namei(&nd);
	pathbuf_destroy(pb);
	if (error != 0) {
		goto err_mounted;
	}
	if (nd.ni_vp != vp) {
		vput(nd.ni_vp);
		error = EINVAL;
		goto err_mounted;
	}
	if (vp->v_mountedhere != NULL) {
		vput(nd.ni_vp);
		error = EBUSY;
		goto err_mounted;
	}
	error = vinvalbuf(vp, V_SAVE, l->l_cred, l, 0, 0);
	if (error != 0) {
		vput(nd.ni_vp);
		goto err_mounted;
	}

	/*
	 * Put the new filesystem on the mount list after root.
	 */
	cache_purge(vp);
	mp->mnt_iflag &= ~IMNT_WANTRDWR;

	mutex_enter(&mountlist_lock);
	CIRCLEQ_INSERT_TAIL(&mountlist, mp, mnt_list);
	mutex_exit(&mountlist_lock);
	if ((mp->mnt_flag & (MNT_RDONLY | MNT_ASYNC)) == 0)
		error = vfs_allocate_syncvnode(mp);
	if (error == 0)
		vp->v_mountedhere = mp;
	vput(nd.ni_vp);
	if (error != 0)
		goto err_onmountlist;

	mount_checkdirs(vp);
	mutex_exit(&mp->mnt_updating);

	/* Hold an additional reference to the mount across VFS_START(). */
	vfs_unbusy(mp, true, NULL);
	(void) VFS_STATVFS(mp, &mp->mnt_stat);
	error = VFS_START(mp, 0);
	if (error)
		vrele(vp);
	/* Drop reference held for VFS_START(). */
	vfs_destroy(mp);
	*vpp = NULL;
	return error;

err_onmountlist:
	mutex_enter(&mountlist_lock);
	CIRCLEQ_REMOVE(&mountlist, mp, mnt_list);
	mp->mnt_iflag |= IMNT_GONE;
	mutex_exit(&mountlist_lock);

err_mounted:
	if (VFS_UNMOUNT(mp, MNT_FORCE) != 0)
		panic("Unmounting fresh file system failed");

err_unmounted:
	vp->v_mountedhere = NULL;
	mutex_exit(&mp->mnt_updating);
	vfs_unbusy(mp, false, NULL);
	vfs_destroy(mp);

	return error;
}

/*
 * Do the actual file system unmount.  File system is assumed to have
 * been locked by the caller.
 *
 * => Caller hold reference to the mount, explicitly for dounmount().
 */
int
dounmount(struct mount *mp, int flags, struct lwp *l)
{
	vnode_t *coveredvp;
	int error, async, used_syncer;

#if NVERIEXEC > 0
	error = veriexec_unmountchk(mp);
	if (error)
		return (error);
#endif /* NVERIEXEC > 0 */

	/*
	 * XXX Freeze syncer.  Must do this before locking the
	 * mount point.  See dounmount() for details.
	 */
	mutex_enter(&syncer_mutex);
	rw_enter(&mp->mnt_unmounting, RW_WRITER);
	if ((mp->mnt_iflag & IMNT_GONE) != 0) {
		rw_exit(&mp->mnt_unmounting);
		mutex_exit(&syncer_mutex);
		return ENOENT;
	}

	used_syncer = (mp->mnt_syncer != NULL);

	/*
	 * XXX Syncer must be frozen when we get here.  This should really
	 * be done on a per-mountpoint basis, but the syncer doesn't work
	 * like that.
	 *
	 * The caller of dounmount() must acquire syncer_mutex because
	 * the syncer itself acquires locks in syncer_mutex -> vfs_busy
	 * order, and we must preserve that order to avoid deadlock.
	 *
	 * So, if the file system did not use the syncer, now is
	 * the time to release the syncer_mutex.
	 */
	if (used_syncer == 0) {
		mutex_exit(&syncer_mutex);
	}
	mp->mnt_iflag |= IMNT_UNMOUNT;
	async = mp->mnt_flag & MNT_ASYNC;
	mp->mnt_flag &= ~MNT_ASYNC;
	cache_purgevfs(mp);	/* remove cache entries for this file sys */
	if (mp->mnt_syncer != NULL)
		vfs_deallocate_syncvnode(mp);
	error = 0;
	if ((mp->mnt_flag & MNT_RDONLY) == 0) {
		error = VFS_SYNC(mp, MNT_WAIT, l->l_cred);
	}
	vfs_scrubvnlist(mp);
	if (error == 0 || (flags & MNT_FORCE)) {
		error = VFS_UNMOUNT(mp, flags);
	}
	if (error) {
		if ((mp->mnt_flag & (MNT_RDONLY | MNT_ASYNC)) == 0)
			(void) vfs_allocate_syncvnode(mp);
		mp->mnt_iflag &= ~IMNT_UNMOUNT;
		mp->mnt_flag |= async;
		rw_exit(&mp->mnt_unmounting);
		if (used_syncer)
			mutex_exit(&syncer_mutex);
		return (error);
	}
	vfs_scrubvnlist(mp);
	mutex_enter(&mountlist_lock);
	if ((coveredvp = mp->mnt_vnodecovered) != NULLVP)
		coveredvp->v_mountedhere = NULL;
	CIRCLEQ_REMOVE(&mountlist, mp, mnt_list);
	mp->mnt_iflag |= IMNT_GONE;
	mutex_exit(&mountlist_lock);
	if (TAILQ_FIRST(&mp->mnt_vnodelist) != NULL)
		panic("unmount: dangling vnode");
	if (used_syncer)
		mutex_exit(&syncer_mutex);
	vfs_hooks_unmount(mp);
	rw_exit(&mp->mnt_unmounting);
	vfs_destroy(mp);	/* reference from mount() */
	if (coveredvp != NULLVP) {
		vrele(coveredvp);
	}
	return (0);
}

/*
 * Unmount all file systems.
 * We traverse the list in reverse order under the assumption that doing so
 * will avoid needing to worry about dependencies.
 */
bool
vfs_unmountall(struct lwp *l)
{

	printf("unmounting file systems...");
	return vfs_unmountall1(l, true, true);
}

static void
vfs_unmount_print(struct mount *mp, const char *pfx)
{

	aprint_verbose("%sunmounted %s on %s type %s\n", pfx,
	    mp->mnt_stat.f_mntfromname, mp->mnt_stat.f_mntonname,
	    mp->mnt_stat.f_fstypename);
}

bool
vfs_unmount_forceone(struct lwp *l)
{
	struct mount *mp, *nmp;
	int error;

	nmp = NULL;

	CIRCLEQ_FOREACH_REVERSE(mp, &mountlist, mnt_list) {
		if (nmp == NULL || mp->mnt_gen > nmp->mnt_gen) {
			nmp = mp;
		}
	}
	if (nmp == NULL) {
		return false;
	}

#ifdef DEBUG
	printf("\nforcefully unmounting %s (%s)...",
	    nmp->mnt_stat.f_mntonname, nmp->mnt_stat.f_mntfromname);
#endif
	atomic_inc_uint(&nmp->mnt_refcnt);
	if ((error = dounmount(nmp, MNT_FORCE, l)) == 0) {
		vfs_unmount_print(nmp, "forcefully ");
		return true;
	} else {
		vfs_destroy(nmp);
	}

#ifdef DEBUG
	printf("forceful unmount of %s failed with error %d\n",
	    nmp->mnt_stat.f_mntonname, error);
#endif

	return false;
}

bool
vfs_unmountall1(struct lwp *l, bool force, bool verbose)
{
	struct mount *mp, *nmp;
	bool any_error = false, progress = false;
	int error;

	for (mp = CIRCLEQ_LAST(&mountlist);
	     mp != (void *)&mountlist;
	     mp = nmp) {
		nmp = CIRCLEQ_PREV(mp, mnt_list);
#ifdef DEBUG
		printf("\nunmounting %p %s (%s)...",
		    (void *)mp, mp->mnt_stat.f_mntonname,
		    mp->mnt_stat.f_mntfromname);
#endif
		atomic_inc_uint(&mp->mnt_refcnt);
		if ((error = dounmount(mp, force ? MNT_FORCE : 0, l)) == 0) {
			vfs_unmount_print(mp, "");
			progress = true;
		} else {
			vfs_destroy(mp);
			if (verbose) {
				printf("unmount of %s failed with error %d\n",
				    mp->mnt_stat.f_mntonname, error);
			}
			any_error = true;
		}
	}
	if (verbose) {
		printf(" done\n");
	}
	if (any_error && verbose) {
		printf("WARNING: some file systems would not unmount\n");
	}
	return progress;
}

void
vfs_sync_all(struct lwp *l)
{
	printf("syncing disks... ");

	/* remove user processes from run queue */
	suspendsched();
	(void)spl0();

	/* avoid coming back this way again if we panic. */
	doing_shutdown = 1;

	sys_sync(l, NULL, NULL);

	/* Wait for sync to finish. */
	if (buf_syncwait() != 0) {
#if defined(DDB) && defined(DEBUG_HALT_BUSY)
		Debugger();
#endif
		printf("giving up\n");
		return;
	} else
		printf("done\n");
}

/*
 * Sync and unmount file systems before shutting down.
 */
void
vfs_shutdown(void)
{
	lwp_t *l = curlwp;

	vfs_sync_all(l);

	/*
	 * If we have paniced - do not make the situation potentially
	 * worse by unmounting the file systems.
	 */
	if (panicstr != NULL) {
		return;
	}

	/* Unmount file systems. */
	vfs_unmountall(l);
}

/*
 * Print a list of supported file system types (used by vfs_mountroot)
 */
static void
vfs_print_fstypes(void)
{
	struct vfsops *v;
	int cnt = 0;

	mutex_enter(&vfs_list_lock);
	LIST_FOREACH(v, &vfs_list, vfs_list)
		++cnt;
	mutex_exit(&vfs_list_lock);

	if (cnt == 0) {
		printf("WARNING: No file system modules have been loaded.\n");
		return;
	}

	printf("Supported file systems:");
	mutex_enter(&vfs_list_lock);
	LIST_FOREACH(v, &vfs_list, vfs_list) {
		printf(" %s", v->vfs_name);
	}
	mutex_exit(&vfs_list_lock);
	printf("\n");
}

/*
 * Mount the root file system.  If the operator didn't specify a
 * file system to use, try all possible file systems until one
 * succeeds.
 */
int
vfs_mountroot(void)
{
	struct vfsops *v;
	int error = ENODEV;

	if (root_device == NULL)
		panic("vfs_mountroot: root device unknown");

	switch (device_class(root_device)) {
	case DV_IFNET:
		if (rootdev != NODEV)
			panic("vfs_mountroot: rootdev set for DV_IFNET "
			    "(0x%llx -> %llu,%llu)",
			    (unsigned long long)rootdev,
			    (unsigned long long)major(rootdev),
			    (unsigned long long)minor(rootdev));
		break;

	case DV_DISK:
		if (rootdev == NODEV)
			panic("vfs_mountroot: rootdev not set for DV_DISK");
	        if (bdevvp(rootdev, &rootvp))
	                panic("vfs_mountroot: can't get vnode for rootdev");
		error = VOP_OPEN(rootvp, FREAD, FSCRED);
		if (error) {
			printf("vfs_mountroot: can't open root device\n");
			return (error);
		}
		break;

	case DV_VIRTUAL:
		break;

	default:
		printf("%s: inappropriate for root file system\n",
		    device_xname(root_device));
		return (ENODEV);
	}

	/*
	 * If user specified a root fs type, use it.  Make sure the
	 * specified type exists and has a mount_root()
	 */
	if (strcmp(rootfstype, ROOT_FSTYPE_ANY) != 0) {
		v = vfs_getopsbyname(rootfstype);
		error = EFTYPE;
		if (v != NULL) {
			if (v->vfs_mountroot != NULL) {
				error = (v->vfs_mountroot)();
			}
			v->vfs_refcount--;
		}
		goto done;
	}

	/*
	 * Try each file system currently configured into the kernel.
	 */
	mutex_enter(&vfs_list_lock);
	LIST_FOREACH(v, &vfs_list, vfs_list) {
		if (v->vfs_mountroot == NULL)
			continue;
#ifdef DEBUG
		aprint_normal("mountroot: trying %s...\n", v->vfs_name);
#endif
		v->vfs_refcount++;
		mutex_exit(&vfs_list_lock);
		error = (*v->vfs_mountroot)();
		mutex_enter(&vfs_list_lock);
		v->vfs_refcount--;
		if (!error) {
			aprint_normal("root file system type: %s\n",
			    v->vfs_name);
			break;
		}
	}
	mutex_exit(&vfs_list_lock);

	if (v == NULL) {
		vfs_print_fstypes();
		printf("no file system for %s", device_xname(root_device));
		if (device_class(root_device) == DV_DISK)
			printf(" (dev 0x%llx)", (unsigned long long)rootdev);
		printf("\n");
		error = EFTYPE;
	}

done:
	if (error && device_class(root_device) == DV_DISK) {
		VOP_CLOSE(rootvp, FREAD, FSCRED);
		vrele(rootvp);
	}
	if (error == 0) {
		extern struct cwdinfo cwdi0;

		CIRCLEQ_FIRST(&mountlist)->mnt_flag |= MNT_ROOTFS;
		CIRCLEQ_FIRST(&mountlist)->mnt_op->vfs_refcount++;

		/*
		 * Get the vnode for '/'.  Set cwdi0.cwdi_cdir to
		 * reference it.
		 */
		error = VFS_ROOT(CIRCLEQ_FIRST(&mountlist), &rootvnode);
		if (error)
			panic("cannot find root vnode, error=%d", error);
		cwdi0.cwdi_cdir = rootvnode;
		vref(cwdi0.cwdi_cdir);
		VOP_UNLOCK(rootvnode);
		cwdi0.cwdi_rdir = NULL;

		/*
		 * Now that root is mounted, we can fixup initproc's CWD
		 * info.  All other processes are kthreads, which merely
		 * share proc0's CWD info.
		 */
		initproc->p_cwdi->cwdi_cdir = rootvnode;
		vref(initproc->p_cwdi->cwdi_cdir);
		initproc->p_cwdi->cwdi_rdir = NULL;
		/*
		 * Enable loading of modules from the filesystem
		 */
		module_load_vfs_init();

	}
	return (error);
}

/*
 * mount_specific_key_create --
 *	Create a key for subsystem mount-specific data.
 */
int
mount_specific_key_create(specificdata_key_t *keyp, specificdata_dtor_t dtor)
{

	return specificdata_key_create(mount_specificdata_domain, keyp, dtor);
}

/*
 * mount_specific_key_delete --
 *	Delete a key for subsystem mount-specific data.
 */
void
mount_specific_key_delete(specificdata_key_t key)
{

	specificdata_key_delete(mount_specificdata_domain, key);
}

/*
 * mount_initspecific --
 *	Initialize a mount's specificdata container.
 */
void
mount_initspecific(struct mount *mp)
{
	int error;

	error = specificdata_init(mount_specificdata_domain,
				  &mp->mnt_specdataref);
	KASSERT(error == 0);
}

/*
 * mount_finispecific --
 *	Finalize a mount's specificdata container.
 */
void
mount_finispecific(struct mount *mp)
{

	specificdata_fini(mount_specificdata_domain, &mp->mnt_specdataref);
}

/*
 * mount_getspecific --
 *	Return mount-specific data corresponding to the specified key.
 */
void *
mount_getspecific(struct mount *mp, specificdata_key_t key)
{

	return specificdata_getspecific(mount_specificdata_domain,
					 &mp->mnt_specdataref, key);
}

/*
 * mount_setspecific --
 *	Set mount-specific data corresponding to the specified key.
 */
void
mount_setspecific(struct mount *mp, specificdata_key_t key, void *data)
{

	specificdata_setspecific(mount_specificdata_domain,
				 &mp->mnt_specdataref, key, data);
}

/*
 * Check to see if a filesystem is mounted on a block device.
 */
int
vfs_mountedon(vnode_t *vp)
{
	vnode_t *vq;
	int error = 0;

	if (vp->v_type != VBLK)
		return ENOTBLK;
	if (vp->v_specmountpoint != NULL)
		return (EBUSY);
	mutex_enter(&device_lock);
	for (vq = specfs_hash[SPECHASH(vp->v_rdev)]; vq != NULL;
	    vq = vq->v_specnext) {
		if (vq->v_rdev != vp->v_rdev || vq->v_type != vp->v_type)
			continue;
		if (vq->v_specmountpoint != NULL) {
			error = EBUSY;
			break;
		}
	}
	mutex_exit(&device_lock);
	return (error);
}

/*
 * Check if a device pointed to by vp is mounted.
 *
 * Returns:
 *   EINVAL	if it's not a disk
 *   EBUSY	if it's a disk and mounted
 *   0		if it's a disk and not mounted
 */
int
rawdev_mounted(vnode_t *vp, vnode_t **bvpp)
{
	vnode_t *bvp;
	dev_t dev;
	int d_type;

	bvp = NULL;
	dev = vp->v_rdev;
	d_type = D_OTHER;

	if (iskmemvp(vp))
		return EINVAL;

	switch (vp->v_type) {
	case VCHR: {
		const struct cdevsw *cdev;

		cdev = cdevsw_lookup(dev);
		if (cdev != NULL) {
			dev_t blkdev;

			blkdev = devsw_chr2blk(dev);
			if (blkdev != NODEV) {
				if (vfinddev(blkdev, VBLK, &bvp) != 0) {
					d_type = (cdev->d_flag & D_TYPEMASK);
					/* XXX: what if bvp disappears? */
					vrele(bvp);
				}
			}
		}

		break;
		}

	case VBLK: {
		const struct bdevsw *bdev;

		bdev = bdevsw_lookup(dev);
		if (bdev != NULL)
			d_type = (bdev->d_flag & D_TYPEMASK);

		bvp = vp;

		break;
		}

	default:
		break;
	}

	if (d_type != D_DISK)
		return EINVAL;

	if (bvpp != NULL)
		*bvpp = bvp;

	/*
	 * XXX: This is bogus. We should be failing the request
	 * XXX: not only if this specific slice is mounted, but
	 * XXX: if it's on a disk with any other mounted slice.
	 */
	if (vfs_mountedon(bvp))
		return EBUSY;

	return 0;
}

/*
 * Make a 'unique' number from a mount type name.
 */
long
makefstype(const char *type)
{
	long rv;

	for (rv = 0; *type; type++) {
		rv <<= 2;
		rv ^= *type;
	}
	return rv;
}
