/*	$NetBSD: buf.h,v 1.69 2004/01/10 14:39:50 yamt Exp $	*/

/*-
 * Copyright (c) 1999, 2000 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe of the Numerical Aerospace Simulation Facility,
 * NASA Ames Research Center.
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

/*
 * Copyright (c) 1982, 1986, 1989, 1993
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
 *	@(#)buf.h	8.9 (Berkeley) 3/30/95
 */

#ifndef _SYS_BUF_H_
#define	_SYS_BUF_H_

#include <sys/pool.h>
#include <sys/queue.h>
#include <sys/lock.h>

struct buf;
struct mount;
struct vnode;

#define NOLIST ((struct buf *)0x87654321)

/*
 * To avoid including <ufs/ffs/softdep.h> 
 */   
LIST_HEAD(workhead, worklist);

/*
 * Device driver buffer queue.
 */
struct bufq_state {
	void (*bq_put)(struct bufq_state *, struct buf *);
	struct buf *(*bq_get)(struct bufq_state *, int);
	void *bq_private;
	int bq_flags;			/* Flags from bufq_alloc() */
};

/*
 * Flags for bufq_alloc.
 */
#define BUFQ_SORT_RAWBLOCK	0x0001	/* Sort by b_rawblkno */
#define BUFQ_SORT_CYLINDER	0x0002	/* Sort by b_cylinder, b_rawblkno */

#define BUFQ_FCFS		0x0010	/* First-come first-serve */
#define BUFQ_DISKSORT		0x0020	/* Min seek sort */
#define BUFQ_READ_PRIO		0x0030	/* Min seek and read priority */

#define BUFQ_SORT_MASK		0x000f
#define BUFQ_METHOD_MASK	0x00f0

#ifdef _KERNEL

void	bufq_alloc(struct bufq_state *, int);
void	bufq_free(struct bufq_state *);

#define BUFQ_PUT(bufq, bp) \
	(*(bufq)->bq_put)((bufq), (bp))	/* Put buffer in queue */
#define BUFQ_GET(bufq) \
	(*(bufq)->bq_get)((bufq), 1)	/* Get and remove buffer from queue */
#define BUFQ_PEEK(bufq) \
	(*(bufq)->bq_get)((bufq), 0)	/* Get buffer from queue */

#endif /* _KERNEL */

/*
 * These are currently used only by the soft dependency code, hence
 * are stored once in a global variable. If other subsystems wanted
 * to use these hooks, a pointer to a set of bio_ops could be added
 * to each buffer.
 */
struct bio_ops {
 	void	(*io_start)(struct buf *);
 	void	(*io_complete)(struct buf *);
 	void	(*io_deallocate)(struct buf *);
 	int	(*io_fsync)(struct vnode *);
 	int	(*io_sync)(struct mount *);
	void	(*io_movedeps)(struct buf *, struct buf *);
	int	(*io_countdeps)(struct buf *, int);
	void	(*io_pageiodone)(struct buf *);
};

/*
 * The buffer header describes an I/O operation in the kernel.
 */
struct buf {
	TAILQ_ENTRY(buf) b_actq;	/* Device driver queue when active. */
	struct simplelock b_interlock;	/* Lock for b_flags changes */
	volatile long	b_flags;	/* B_* flags. */
	int	b_error;		/* Errno value. */
	int	b_prio;			/* Hint for buffer queue discipline. */
	long	b_bufsize;		/* Allocated buffer size. */
	long	b_bcount;		/* Valid bytes in buffer. */
	long	b_resid;		/* Remaining I/O. */
	dev_t	b_dev;			/* Device associated with buffer. */
	struct {
		caddr_t	b_addr;		/* Memory, superblocks, indirect etc. */
	} b_un;
	daddr_t	b_blkno;		/* Underlying physical block number
					   (partition relative) */
	daddr_t	b_rawblkno;		/* Raw underlying physical block
					   number (not partition relative) */
					/* Function to call upon completion. */
	void	(*b_iodone)(struct buf *);
	struct  proc *b_proc;		/* Associated proc if B_PHYS set. */
	struct	vnode *b_vp;		/* File vnode. */
	struct  workhead b_dep;		/* List of filesystem dependencies. */
	void	*b_saveaddr;		/* Original b_addr for physio. */

	/*
	 * private data for owner.
	 *  - buffer cache buffers are owned by corresponding filesystem.
	 *  - non-buffer cache buffers are owned by subsystem which
	 *    allocated them. (filesystem, disk driver, etc)
	 */
	union {
		void *bf_private;
		off_t bf_dcookie;	/* NFS: Offset cookie if dir block */
	} b_fspriv;
#define	b_private	b_fspriv.bf_private
#define	b_dcookie	b_fspriv.bf_dcookie

	/*
	 * buffer cache specific data
	 */
	LIST_ENTRY(buf) b_hash;		/* Hash chain. */
	LIST_ENTRY(buf) b_vnbufs;	/* Buffer's associated vnode. */
	TAILQ_ENTRY(buf) b_freelist;	/* Free list position if not active. */
	daddr_t	b_lblkno;		/* Logical block number. */
};

#define	BUF_INIT(bp)							\
do {									\
	LIST_INIT(&(bp)->b_dep);					\
	simple_lock_init(&(bp)->b_interlock);				\
	BIO_SETPRIO((bp), BPRIO_DEFAULT);				\
} while (/*CONSTCOND*/0)

/*
 * For portability with historic industry practice, the cylinder number has
 * to be maintained in the `b_resid' field.
 */
#define	b_cylinder b_resid		/* Cylinder number for disksort(). */

/* Device driver compatibility definitions. */
#define	b_data	 b_un.b_addr		/* b_un.b_addr is not changeable. */

/*
 * These flags are kept in b_flags.
 */
#define	B_AGE		0x00000001	/* Move to age queue when I/O done. */
#define	B_ASYNC		0x00000004	/* Start I/O, do not wait. */
#define	B_BAD		0x00000008	/* Bad block revectoring in progress. */
#define	B_BUSY		0x00000010	/* I/O in progress. */
#define B_SCANNED	0x00000020	/* Block already pushed during sync */
#define	B_CALL		0x00000040	/* Call b_iodone from biodone. */
#define	B_DELWRI	0x00000080	/* Delay I/O until buffer reused. */
#define	B_DIRTY		0x00000100	/* Dirty page to be pushed out async. */
#define	B_DONE		0x00000200	/* I/O completed. */
#define	B_EINTR		0x00000400	/* I/O was interrupted */
#define	B_ERROR		0x00000800	/* I/O error occurred. */
#define	B_GATHERED	0x00001000	/* LFS: already in a segment. */
#define	B_INVAL		0x00002000	/* Does not contain valid info. */
#define	B_LOCKED	0x00004000	/* Locked in core (not reusable). */
#define	B_NOCACHE	0x00008000	/* Do not cache block after use. */
#define	B_CACHE		0x00020000	/* Bread found us in the cache. */
#define	B_PHYS		0x00040000	/* I/O to user memory. */
#define	B_RAW		0x00080000	/* Set by physio for raw transfers. */
#define	B_READ		0x00100000	/* Read buffer. */
#define	B_TAPE		0x00200000	/* Magnetic tape I/O. */
#define	B_WANTED	0x00800000	/* Process wants this buffer. */
#define	B_WRITE		0x00000000	/* Write buffer (pseudo flag). */
#define	B_XXX		0x02000000	/* Debugging flag. */
#define	B_VFLUSH	0x04000000	/* Buffer is being synced. */

/*
 * This structure describes a clustered I/O.  It is stored in the b_saveaddr
 * field of the buffer on which I/O is done.  At I/O completion, cluster
 * callback uses the structure to parcel I/O's to individual buffers, and
 * then free's this structure.
 */
struct cluster_save {
	long	bs_bcount;		/* Saved b_bcount. */
	long	bs_bufsize;		/* Saved b_bufsize. */
	void	*bs_saveaddr;		/* Saved b_addr. */
	int	bs_nchildren;		/* Number of associated buffers. */
	struct buf **bs_children;	/* List of associated buffers. */
};

/*
 * Zero out the buffer's data area.
 */
#define	clrbuf(bp)							\
do {									\
	memset((bp)->b_data, 0, (u_int)(bp)->b_bcount);			\
	(bp)->b_resid = 0;						\
} while (/* CONSTCOND */ 0)

/* Flags to low-level allocation routines. */
#define B_CLRBUF	0x01	/* Request allocated buffer be cleared. */
#define B_SYNC		0x02	/* Do all allocations synchronously. */

#ifdef _KERNEL

#define	BIO_GETPRIO(bp)		((bp)->b_prio)
#define	BIO_SETPRIO(bp, prio)	(bp)->b_prio = (prio)
#define	BIO_COPYPRIO(bp1, bp2)	BIO_SETPRIO(bp1, BIO_GETPRIO(bp2))

#define	BPRIO_NPRIO		3
#define	BPRIO_TIMECRITICAL	2
#define	BPRIO_TIMELIMITED	1
#define	BPRIO_TIMENONCRITICAL	0
#define	BPRIO_DEFAULT		BPRIO_TIMELIMITED

extern	struct bio_ops bioops;
extern	u_int nbuf;		/* The number of buffer headers */
extern	struct buf *buf;	/* The buffer headers. */
extern	char *buffers;		/* The buffer contents. */
extern	u_int bufpages;		/* Number of memory pages in the buffer pool. */

/*
 * Pool of I/O buffers.  Access to this pool must be protected with
 * splbio().
 */
extern	struct pool bufpool;

__BEGIN_DECLS
void	allocbuf(struct buf *, int, int);
void	bawrite(struct buf *);
void	bdirty(struct buf *);
void	bdwrite(struct buf *);
void	biodone(struct buf *);
int	biowait(struct buf *);
int	bread(struct vnode *, daddr_t, int, struct ucred *, struct buf **);
int	breada(struct vnode *, daddr_t, int, daddr_t, int, struct ucred *,
	       struct buf **);
int	breadn(struct vnode *, daddr_t, int, daddr_t *, int *, int,
	       struct ucred *, struct buf **);
void	brelse(struct buf *);
void	bremfree(struct buf *);
void	bufinit(void);
int	bwrite(struct buf *);
struct buf *getblk(struct vnode *, daddr_t, int, int, int);
struct buf *geteblk(int);
struct buf *getnewbuf(int, int, int);
struct buf *incore(struct vnode *, daddr_t);

void	minphys(struct buf *);
int	physio(void (*)(struct buf *), struct buf *, dev_t, int,
	       void (*)(struct buf *), struct uio *);

void	brelvp(struct buf *);
void	reassignbuf(struct buf *, struct vnode *);
void	bgetvp(struct vnode *, struct buf *);
int	buf_syncwait(void);
u_long	buf_memcalc(void);
int	buf_drain(int);
int	buf_setvalimit(vsize_t);
#ifdef DDB
void	vfs_buf_print(struct buf *, int, void (*)(const char *, ...));
#endif
__END_DECLS
#endif
#endif /* !_SYS_BUF_H_ */
