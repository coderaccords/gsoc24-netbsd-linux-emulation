/*	$NetBSD: lfs_extern.h,v 1.73 2005/12/11 12:25:26 christos Exp $	*/

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
/*-
 * Copyright (c) 1991, 1993, 1994
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
 *	@(#)lfs_extern.h	8.6 (Berkeley) 5/8/95
 */

#ifndef _UFS_LFS_LFS_EXTERN_H_
#define _UFS_LFS_LFS_EXTERN_H_

#ifdef _KERNEL
#include <sys/mallocvar.h>

MALLOC_DECLARE(M_SEGMENT);
#endif

/* Copied from ext2fs for ITIMES.  XXX This is a bogus use of v_tag. */
#define IS_LFS_VNODE(vp)   (vp->v_tag == VT_LFS)

/*
 * Sysctl values for LFS.
 */
#define LFS_WRITEINDIR	 1 /* flush indirect blocks on non-checkpoint writes */
#define LFS_CLEAN_VNHEAD 2 /* put prev unrefed cleaned vnodes on head of free list */
#define LFS_DOSTATS	 3
#define LFS_MAXPAGES	 4
#define LFS_FS_PAGETRIP	 5
#define LFS_STATS	 6
#define LFS_DO_RFW	 7
#define LFS_DEBUGLOG	 8
#define LFS_MAXID	 9

#define LFS_NAMES { \
	{ 0, 0 }, \
	{ "flushindir", CTLTYPE_INT }, \
	{ "clean_vnhead", CTLTYPE_INT }, \
	{ "dostats", CTLTYPE_INT }, \
	{ "maxpages", CTLTYPE_INT }, \
	{ "debug", CTLTYPE_NODE }, \
}

struct fid;
struct mount;
struct nameidata;
struct proc;
struct statvfs;
struct timeval;
struct inode;
struct uio;
struct mbuf;
struct ufs1_dinode;
struct buf;
struct vnode;
struct dlfs;
struct lfs;
struct segment;
struct ucred;
struct block_info;

extern int lfs_allclean_wakeup;
extern struct pool lfs_inode_pool;		/* memory pool for inodes */
extern struct pool lfs_dinode_pool;		/* memory pool for dinodes */
extern struct pool lfs_inoext_pool;	/* memory pool for inode extension */
extern struct pool lfs_lbnentry_pool;   /* memory pool for balloc accounting */

extern int locked_queue_count;
extern long locked_queue_bytes;
extern int lfs_subsys_pages;
extern int lfs_dirvcount;
extern struct simplelock lfs_subsys_lock;
extern int lfs_debug_log_subsys[];

__BEGIN_DECLS
/* lfs_alloc.c */
int lfs_rf_valloc(struct lfs *, ino_t, int, struct lwp *, struct vnode **);
void lfs_vcreate(struct mount *, ino_t, struct vnode *);
int lfs_valloc(struct vnode *, int, struct ucred *, struct vnode **);
int lfs_vfree(struct vnode *, ino_t, int);

/* lfs_balloc.c */
int lfs_balloc(struct vnode *, off_t, int, struct ucred *, int, struct buf **);
void lfs_register_block(struct vnode *, daddr_t);
void lfs_deregister_block(struct vnode *, daddr_t);
void lfs_deregister_all(struct vnode *);

/* lfs_bio.c */
int lfs_availwait(struct lfs *, int);
int lfs_bwrite_ext(struct buf *, int);
int lfs_fits(struct lfs *, int);
void lfs_flush_fs(struct lfs *, int);
void lfs_flush(struct lfs *, int, int);
int lfs_check(struct vnode *, daddr_t, int);
void lfs_freebuf(struct lfs *, struct buf *);
struct buf *lfs_newbuf(struct lfs *, struct vnode *, daddr_t, size_t, int);
void lfs_countlocked(int *, long *, const char *);
int lfs_reserve(struct lfs *, struct vnode *, struct vnode *, int);

/* lfs_cksum.c */
u_int32_t cksum(void *, size_t);
u_int32_t lfs_cksum_part(void *, size_t, u_int32_t);
#define lfs_cksum_fold(sum)	(sum)
u_int32_t lfs_sb_cksum(struct dlfs *);

/* lfs_debug.c */
#ifdef DEBUG
int lfs_bwrite_log(struct buf *, const char *, int);
void lfs_dumplog(void);
void lfs_dump_super(struct lfs *);
void lfs_dump_dinode(struct ufs1_dinode *);
void lfs_check_bpp(struct lfs *, struct segment *, char *, int);
void lfs_check_segsum(struct lfs *, struct segment *, char *, int);
void lfs_debug_log(int, const char *, ...);
#endif /* DEBUG */

/* lfs_inode.c */
int lfs_update(struct vnode *, const struct timespec *, const struct timespec *,
    int);
int lfs_truncate(struct vnode *, off_t, int, struct ucred *, struct lwp *);
struct ufs1_dinode *lfs_ifind(struct lfs *, ino_t, struct buf *);

/* lfs_segment.c */
void lfs_imtime(struct lfs *);
int lfs_vflush(struct vnode *);
int lfs_segwrite(struct mount *, int);
void lfs_writefile(struct lfs *, struct segment *, struct vnode *);
int lfs_writeinode(struct lfs *, struct segment *, struct inode *);
int lfs_gatherblock(struct segment *, struct buf *, int *);
int lfs_gather(struct lfs *, struct segment *, struct vnode *, int (*match )(struct lfs *, struct buf *));
void lfs_update_single(struct lfs *, struct segment *, struct vnode *,
    daddr_t, int32_t, int);
void lfs_updatemeta(struct segment *);
int lfs_rewind(struct lfs *, int);
void lfs_unset_inval_all(struct lfs *);
int lfs_initseg(struct lfs *);
int lfs_writeseg(struct lfs *, struct segment *);
void lfs_writesuper(struct lfs *, daddr_t);
int lfs_match_data(struct lfs *, struct buf *);
int lfs_match_indir(struct lfs *, struct buf *);
int lfs_match_dindir(struct lfs *, struct buf *);
int lfs_match_tindir(struct lfs *, struct buf *);
void lfs_callback(struct buf *);
int lfs_vref(struct vnode *);
void lfs_vunref(struct vnode *);
void lfs_vunref_head(struct vnode *);

/* lfs_subr.c */
int lfs_blkatoff(struct vnode *, off_t, char **, struct buf **);
void lfs_setup_resblks(struct lfs *);
void lfs_pad_check(unsigned char *, int, char *, int);
void lfs_free_resblks(struct lfs *);
void *lfs_malloc(struct lfs *, size_t, int);
void lfs_free(struct lfs *, void *, int);
int lfs_seglock(struct lfs *, unsigned long);
void lfs_segunlock(struct lfs *);
int lfs_writer_enter(struct lfs *, const char *);
void lfs_writer_leave(struct lfs *);

/* lfs_syscalls.c */
int lfs_fastvget(struct mount *, ino_t, daddr_t, struct vnode **, struct ufs1_dinode *);
struct buf *lfs_fakebuf(struct lfs *, struct vnode *, int, size_t, caddr_t);
int lfs_do_segclean(struct lfs *, unsigned long);
void lfs_fakebuf_iodone(struct buf *);
int lfs_segwait(fsid_t *, struct timeval *);
int lfs_bmapv(struct proc *, fsid_t *, struct block_info *, int);
int lfs_markv(struct proc *, fsid_t *, struct block_info *, int);

/* lfs_vfsops.c */
void lfs_init(void);
void lfs_reinit(void);
void lfs_done(void);
int lfs_mountroot(void);
int lfs_mount(struct mount *, const char *, void *, struct nameidata *, struct lwp *);
int lfs_unmount(struct mount *, int, struct lwp *);
int lfs_statvfs(struct mount *, struct statvfs *, struct lwp *);
int lfs_sync(struct mount *, int, struct ucred *, struct lwp *);
int lfs_vget(struct mount *, ino_t, struct vnode **);
int lfs_fhtovp(struct mount *, struct fid *, struct vnode **);
int lfs_vptofh(struct vnode *, struct fid *);
void lfs_vinit(struct mount *, struct vnode **);
int lfs_resize_fs(struct lfs *, int);

/* lfs_vnops.c */
void lfs_mark_vnode(struct vnode *);
void lfs_unmark_vnode(struct vnode *);
int lfs_gop_alloc(struct vnode *, off_t, off_t, int, struct ucred *);
void lfs_gop_size(struct vnode *, off_t, off_t *, int);
int lfs_putpages_ext(void *, int);
int lfs_gatherpages(struct vnode *);

int lfs_bwrite	 (void *);
int lfs_fsync	 (void *);
int lfs_symlink	 (void *);
int lfs_mknod	 (void *);
int lfs_create	 (void *);
int lfs_mkdir	 (void *);
int lfs_read	 (void *);
int lfs_remove	 (void *);
int lfs_rmdir	 (void *);
int lfs_link	 (void *);
int lfs_mmap	 (void *);
int lfs_rename	 (void *);
int lfs_getattr	 (void *);
int lfs_setattr	 (void *);
int lfs_close	 (void *);
int lfsspec_close(void *);
int lfsfifo_close(void *);
int lfs_fcntl	 (void *);
int lfs_inactive (void *);
int lfs_reclaim	 (void *);
int lfs_strategy (void *);
int lfs_write	 (void *);
int lfs_getpages (void *);
int lfs_putpages (void *);

#ifdef SYSCTL_SETUP_PROTO
SYSCTL_SETUP_PROTO(sysctl_vfs_lfs_setup);
#endif /* SYSCTL_SETUP_PROTO */

__END_DECLS
extern int lfs_mount_type;
extern int (**lfs_vnodeop_p)(void *);
extern int (**lfs_specop_p)(void *);
extern int (**lfs_fifoop_p)(void *);
extern const struct genfs_ops lfs_genfsops;

#endif /* !_UFS_LFS_LFS_EXTERN_H_ */
