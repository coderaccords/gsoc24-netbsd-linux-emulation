/*	$NetBSD: tmpfs_fifoops.c,v 1.5 2005/12/11 12:24:29 christos Exp $	*/

/*
 * Copyright (c) 2005 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Julio M. Merino Vidal, developed as part of Google's Summer of Code
 * 2005 program.
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
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
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
 * tmpfs vnode interface for named pipes.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: tmpfs_fifoops.c,v 1.5 2005/12/11 12:24:29 christos Exp $");

#include <sys/param.h>
#include <sys/vnode.h>

#include <fs/tmpfs/tmpfs.h>
#include <fs/tmpfs/tmpfs_fifoops.h>

/* --------------------------------------------------------------------- */

/*
 * vnode operations vector used for fifos stored in a tmpfs file system.
 */
int (**tmpfs_fifoop_p)(void *);
const struct vnodeopv_entry_desc tmpfs_fifoop_entries[] = {
	{ &vop_default_desc,		vn_default_error },
	{ &vop_lookup_desc,		tmpfs_fifo_lookup },
	{ &vop_create_desc,		tmpfs_fifo_create },
	{ &vop_mknod_desc,		tmpfs_fifo_mknod },
	{ &vop_open_desc,		tmpfs_fifo_open },
	{ &vop_close_desc,		tmpfs_fifo_close },
	{ &vop_access_desc,		tmpfs_fifo_access },
	{ &vop_getattr_desc,		tmpfs_fifo_getattr },
	{ &vop_setattr_desc,		tmpfs_fifo_setattr },
	{ &vop_read_desc,		tmpfs_fifo_read },
	{ &vop_write_desc,		tmpfs_fifo_write },
	{ &vop_ioctl_desc,		tmpfs_fifo_ioctl },
	{ &vop_fcntl_desc,		tmpfs_fifo_fcntl },
	{ &vop_poll_desc,		tmpfs_fifo_poll },
	{ &vop_kqfilter_desc,		tmpfs_fifo_kqfilter },
	{ &vop_revoke_desc,		tmpfs_fifo_revoke },
	{ &vop_mmap_desc,		tmpfs_fifo_mmap },
	{ &vop_fsync_desc,		tmpfs_fifo_fsync },
	{ &vop_seek_desc,		tmpfs_fifo_seek },
	{ &vop_remove_desc,		tmpfs_fifo_remove },
	{ &vop_link_desc,		tmpfs_fifo_link },
	{ &vop_rename_desc,		tmpfs_fifo_rename },
	{ &vop_mkdir_desc,		tmpfs_fifo_mkdir },
	{ &vop_rmdir_desc,		tmpfs_fifo_rmdir },
	{ &vop_symlink_desc,		tmpfs_fifo_symlink },
	{ &vop_readdir_desc,		tmpfs_fifo_readdir },
	{ &vop_readlink_desc,		tmpfs_fifo_readlink },
	{ &vop_abortop_desc,		tmpfs_fifo_abortop },
	{ &vop_inactive_desc,		tmpfs_fifo_inactive },
	{ &vop_reclaim_desc,		tmpfs_fifo_reclaim },
	{ &vop_lock_desc,		tmpfs_fifo_lock },
	{ &vop_unlock_desc,		tmpfs_fifo_unlock },
	{ &vop_bmap_desc,		tmpfs_fifo_bmap },
	{ &vop_strategy_desc,		tmpfs_fifo_strategy },
	{ &vop_print_desc,		tmpfs_fifo_print },
	{ &vop_pathconf_desc,		tmpfs_fifo_pathconf },
	{ &vop_islocked_desc,		tmpfs_fifo_islocked },
	{ &vop_advlock_desc,		tmpfs_fifo_advlock },
	{ &vop_lease_desc,		tmpfs_fifo_lease },
	{ &vop_bwrite_desc,		tmpfs_fifo_bwrite },
	{ &vop_getpages_desc,		tmpfs_fifo_getpages },
	{ &vop_putpages_desc,		tmpfs_fifo_putpages },
	{ NULL, NULL }
};
const struct vnodeopv_desc tmpfs_fifoop_opv_desc =
	{ &tmpfs_fifoop_p, tmpfs_fifoop_entries };

/* --------------------------------------------------------------------- */

int
tmpfs_fifo_close(void *v)
{
	struct vnode *vp = ((struct vop_close_args *)v)->a_vp;

	int error;

	tmpfs_update(vp, NULL, NULL, UPDATE_CLOSE);
	error = VOCALL(fifo_vnodeop_p, VOFFSET(vop_close), v);

	return error;
}

/* --------------------------------------------------------------------- */

int
tmpfs_fifo_read(void *v)
{
	struct vnode *vp = ((struct vop_read_args *)v)->a_vp;

	VP_TO_TMPFS_NODE(vp)->tn_status |= TMPFS_NODE_ACCESSED;
	return VOCALL(fifo_vnodeop_p, VOFFSET(vop_read), v);
}

/* --------------------------------------------------------------------- */

int
tmpfs_fifo_write(void *v)
{
	struct vnode *vp = ((struct vop_write_args *)v)->a_vp;

	VP_TO_TMPFS_NODE(vp)->tn_status |= TMPFS_NODE_MODIFIED;
	return VOCALL(fifo_vnodeop_p, VOFFSET(vop_write), v);
}
