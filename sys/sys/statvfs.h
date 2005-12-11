/*	$NetBSD: statvfs.h,v 1.8 2005/12/11 12:25:21 christos Exp $	 */

/*-
 * Copyright (c) 2004 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Christos Zoulas.
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

#ifndef	_SYS_STATVFS_H_
#define	_SYS_STATVFS_H_

#include <sys/cdefs.h>
#include <sys/featuretest.h>
#include <sys/stdint.h>
#include <machine/ansi.h>
#include <sys/ansi.h>
#include <sys/fstypes.h>

#define	_VFS_NAMELEN	32
#define	_VFS_MNAMELEN	1024

#ifndef	fsblkcnt_t
typedef	__fsblkcnt_t	fsblkcnt_t;	/* fs block count (statvfs) */
#define	fsblkcnt_t	__fsblkcnt_t
#endif

#ifndef	fsfilcnt_t
typedef	__fsfilcnt_t	fsfilcnt_t;	/* fs file count */
#define	fsfilcnt_t	__fsfilcnt_t
#endif

#ifndef	uid_t
typedef	__uid_t		uid_t;		/* user id */
#define	uid_t		__uid_t
#endif

#ifdef	_BSD_SIZE_T_
typedef	_BSD_SIZE_T_		size_t;
#define	_SIZE_T
#undef	_BSD_SIZE_T_
#endif

struct statvfs {
	unsigned long	f_flag;		/* copy of mount exported flags */
	unsigned long	f_bsize;	/* file system block size */
	unsigned long	f_frsize;	/* fundamental file system block size */
	unsigned long	f_iosize;	/* optimal file system block size */

	fsblkcnt_t	f_blocks;	/* number of blocks in file system, */
					/*   (in units of f_frsize) */
	fsblkcnt_t	f_bfree;	/* free blocks avail in file system */
	fsblkcnt_t	f_bavail;	/* free blocks avail to non-root */
	fsblkcnt_t	f_bresvd;	/* blocks reserved for root */

	fsfilcnt_t	f_files;	/* total file nodes in file system */
	fsfilcnt_t	f_ffree;	/* free file nodes in file system */
	fsfilcnt_t	f_favail;	/* free file nodes avail to non-root */
	fsfilcnt_t	f_fresvd;	/* file nodes reserved for root */

	uint64_t  	f_syncreads;	/* count of sync reads since mount */
	uint64_t  	f_syncwrites;	/* count of sync writes since mount */

	uint64_t  	f_asyncreads;	/* count of async reads since mount */
	uint64_t  	f_asyncwrites;	/* count of async writes since mount */

	fsid_t		f_fsidx;	/* NetBSD compatible fsid */
	unsigned long	f_fsid;		/* Posix compatible fsid */
	unsigned long	f_namemax;	/* maximum filename length */
	uid_t		f_owner;	/* user that mounted the file system */

	uint32_t	f_spare[4];	/* spare space */

	char	f_fstypename[_VFS_NAMELEN]; /* fs type name */
	char	f_mntonname[_VFS_MNAMELEN];  /* directory on which mounted */
	char	f_mntfromname[_VFS_MNAMELEN];  /* mounted file system */

};

#if defined(_NETBSD_SOURCE) && !defined(_POSIX_SOURCE) && \
    !defined(_XOPEN_SOURCE)
#define	VFS_NAMELEN	_VFS_NAMELEN
#define	VFS_MNAMELEN	_VFS_MNAMELEN
#endif

#define	ST_RDONLY	MNT_RDONLY
#define	ST_SYNCHRONOUS	MNT_SYNCHRONOUS
#define	ST_NOEXEC	MNT_NOEXEC
#define	ST_NOSUID	MNT_NOSUID
#define	ST_NODEV	MNT_NODEV
#define	ST_UNION	MNT_UNION
#define	ST_ASYNC	MNT_ASYNC
#define	ST_NOCOREDUMP	MNT_NOCOREDUMP
#define	ST_IGNORE	MNT_IGNORE
#define	ST_MAGICLINKS	MNT_MAGICLINKS
#define	ST_NOATIME	MNT_NOATIME
#define	ST_SYMPERM	MNT_SYMPERM
#define	ST_NODEVMTIME	MNT_NODEVMTIME
#define	ST_SOFTDEP	MNT_SOFTDEP

#define	ST_EXRDONLY	MNT_EXRDONLY
#define	ST_EXPORTED	MNT_EXPORTED
#define	ST_DEFEXPORTED	MNT_DEFEXPORTED
#define	ST_EXPORTANON	MNT_EXPORTANON
#define	ST_EXKERB	MNT_EXKERB
#define	ST_EXNORESPORT	MNT_EXNORESPORT
#define	ST_EXPUBLIC	MNT_EXPUBLIC

#define	ST_LOCAL	MNT_LOCAL
#define	ST_QUOTA	MNT_QUOTA
#define	ST_ROOTFS	MNT_ROOTFS


#define	ST_WAIT		MNT_WAIT
#define	ST_NOWAIT	MNT_NOWAIT

#if defined(_KERNEL) || defined(_STANDALONE)
struct mount;
struct lwp;

int	set_statvfs_info(const char *, int, const char *, int,
    struct mount *, struct lwp *);
void	copy_statvfs_info(struct statvfs *, const struct mount *);
int	dostatvfs(struct mount *, struct statvfs *, struct lwp *, int, int);
#else
__BEGIN_DECLS
int	statvfs(const char *__restrict, struct statvfs *__restrict);
int	fstatvfs(int, struct statvfs *);
int	getvfsstat(struct statvfs *, size_t, int);
#ifndef __LIBC12_SOURCE__
int	getmntinfo(struct statvfs **, int) __RENAME(__getmntinfo13);
#endif /* __LIBC12_SOURCE__ */
#if defined(_NETBSD_SOURCE)
int	fhstatvfs(const fhandle_t *, struct statvfs *);

int	statvfs1(const char *__restrict, struct statvfs *__restrict, int);
int	fstatvfs1(int, struct statvfs *, int);
int	fhstatvfs1(const fhandle_t *, struct statvfs *, int);
#endif /* _NETBSD_SOURCE */
__END_DECLS
#endif /* _KERNEL || _STANDALONE */
#endif /* !_SYS_STATVFS_H_ */
