/*	$NetBSD: param.h,v 1.6 2001/01/03 18:14:46 itojun Exp $	*/

/*-
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * William Jolitz.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
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
 *	@(#)param.h	5.8 (Berkeley) 6/28/91
 */

/*
 * Machine dependent constants for Intel 386.
 */

#ifndef _EVBSH3_PARAM_H_
#define _EVBSH3_PARAM_H_

#ifdef _KERNEL
#ifdef _LOCORE
#include <machine/psl.h>
#else
#include <machine/cpu.h>
#endif
#endif

/* will be changed to sh3eb/el */
#define	_MACHINE_ARCH	sh3
#define	MACHINE_ARCH	"sh3"

#define	_MACHINE	evbsh3
#define	MACHINE		"evbsh3"

#define	MID_MACHINE	MID_SH3

/*
 * Round p (pointer or byte index) up to a correctly-aligned value
 * for all data types (int, long, ...).   The result is u_int and
 * must be cast to any desired pointer type.
 *
 * ALIGNED_POINTER is a boolean macro that checks whether an address
 * is valid to fetch data elements of type t from on this architecture.
 * This does not reflect the optimal alignment, just the possibility
 * (within reasonable limits). 
 *
 */
#define ALIGNBYTES		(sizeof(int) - 1)
#define ALIGN(p)		(((u_int)(p) + ALIGNBYTES) &~ ALIGNBYTES)
#define ALIGNED_POINTER(p, t)	((((u_long)(p)) & (sizeof(t)-1)) == 0)
/* #define ALIGNED_POINTER(p, t)	1 */

#define	PGSHIFT		12		/* LOG2(NBPG) */
#define	NBPG		(1 << PGSHIFT)	/* bytes/page */
#define	PGOFSET		(NBPG-1)	/* byte offset into page */
#define	NPTEPG		(NBPG/(sizeof (pt_entry_t)))

#ifdef _KERNEL
#ifndef _LOCORE
extern vaddr_t ram_start;
#define	KERNBASE	ram_start	/* start of kernel virtual space */
#define	KERNTEXTOFF	ram_start	/* start of kernel text */
#else
#define	KERNBASE	IOM_RAM_BEGIN	/* start of kernel virtual space */
#define	KERNTEXTOFF	IOM_RAM_BEGIN	/* start of kernel text */
#endif
#endif
#define	KERNSIZE	0x01800000	/* size of kernel virtual space */
#define	BTOPKERNBASE	((u_long)KERNBASE >> PGSHIFT)

#define NPGDIR (1*NBPG)
#define SYSMAP (5*NBPG) /* page table area offset from _end */

#define	DEV_BSHIFT	9		/* log2(DEV_BSIZE) */
#define	DEV_BSIZE	(1 << DEV_BSHIFT)
#define	BLKDEV_IOSIZE	2048
#define	MAXPHYS		(64 * 1024)	/* max raw I/O transfer size */

#define	SSIZE		1		/* initial stack size/NBPG */
#define	SINCR		1		/* increment of stack/NBPG */
#if 0
#define	UPAGES		2		/* pages of u-area */
#else
#define	UPAGES		4		/* pages of u-area */
#endif
#define	USPACE		(UPAGES * NBPG)	/* total size of u-area */

#ifndef MSGBUFSIZE
#define MSGBUFSIZE	NBPG		/* default message buffer size */
#endif

/*
 * Constants related to network buffer management.
 * MCLBYTES must be no larger than CLBYTES (the software page size), and,
 * on machines that exchange pages of input or output buffers with mbuf
 * clusters (MAPPED_MBUFS), MCLBYTES must also be an integral multiple
 * of the hardware page size.
 */
#define	MSIZE		256		/* size of an mbuf */

#ifndef MCLSHIFT
# define	MCLSHIFT	11	/* convert bytes to m_buf clusters */
#endif	/* MCLSHIFT */

#define	MCLBYTES	(1 << MCLSHIFT)	/* size of a m_buf cluster */
#define	MCLOFSET	(MCLBYTES - 1)	/* offset within a m_buf cluster */

#ifndef NMBCLUSTERS
#ifdef GATEWAY
#define	NMBCLUSTERS	512		/* map size, max cluster allocation */
#else
#define	NMBCLUSTERS	256		/* map size, max cluster allocation */
#endif
#endif

/*
 * Minimum and maximum sizes of the kernel malloc arena in PAGE_SIZE-sized
 * logical pages.
 */
#define	NKMEMPAGES_MIN_DEFAULT	((6 * 1024 * 1024) >> PAGE_SHIFT)
#define	NKMEMPAGES_MAX_DEFAULT	((6 * 1024 * 1024) >> PAGE_SHIFT)

/* pages ("clicks") to disk blocks */
#define	ctod(x)		((x) << (PGSHIFT - DEV_BSHIFT))
#define	dtoc(x)		((x) >> (PGSHIFT - DEV_BSHIFT))

/* bytes to pages */
#define	ctob(x)		((x) << PGSHIFT)
#define	btoc(x)		(((x) + PGOFSET) >> PGSHIFT)

/* bytes to disk blocks */
#define	dbtob(x)	((x) << DEV_BSHIFT)
#define	btodb(x)	((x) >> DEV_BSHIFT)

/*
 * Map a ``block device block'' to a file system block.
 * This should be device dependent, and should use the bsize
 * field from the disk label.
 * For now though just use DEV_BSIZE.
 */
#define	bdbtofsb(bn)	((bn) / (BLKDEV_IOSIZE / DEV_BSIZE))

/*
 * Mach derived conversion macros
 */
#define	sh3_round_pdr(x)	((((unsigned)(x)) + PDOFSET) & ~PDOFSET)
#define	sh3_trunc_pdr(x)	((unsigned)(x) & ~PDOFSET)
#define	sh3_btod(x)		((unsigned)(x) >> PDSHIFT)
#define	sh3_dtob(x)		((unsigned)(x) << PDSHIFT)
#define	sh3_round_page(x)	((((unsigned)(x)) + PGOFSET) & ~PGOFSET)
#define	sh3_trunc_page(x)	((unsigned)(x) & ~PGOFSET)
#define	sh3_btop(x)		((unsigned)(x) >> PGSHIFT)
#define	sh3_ptob(x)		((unsigned)(x) << PGSHIFT)

#endif /* _EVBSH3_PARAM_H_ */
