/*	$NetBSD: param.h,v 1.13 2011/02/08 20:20:19 rmind Exp $	*/

/*
 * Copyright (c) 1988 University of Utah.
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * the Systems Programming Group of the University of Utah Computer
 * Science Department and Ralph Campbell.
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
 * from: Utah Hdr: machparam.h 1.11 89/08/14
 *
 *	@(#)param.h	8.1 (Berkeley) 6/10/93
 */

/*
 * Machine-dependent constants (VM, etc) common across MIPS cpus
 */

#ifndef	_MIPSCO_PARAM_H_
#define	_MIPSCO_PARAM_H_

#include <mips/mips_param.h>

/*
 * Machine dependent constants for Mips Corp. machines
 */

#define	_MACHINE	mipsco
#define	MACHINE		"mipsco"

#define	KERNBASE	0x80000000	/* start of kernel virtual */
#define KERNTEXTOFF	0x80001000	/* start of kernel text for kvm_mkdb */
#define	BTOPKERNBASE	((u_long)KERNBASE >> PGSHIFT)

#define	DEV_BSIZE	512
#define	DEV_BSHIFT	9		/* log2(DEV_BSIZE) */
#define BLKDEV_IOSIZE	2048
#define	MAXPHYS		(64 * 1024)	/* max raw I/O transfer size */

#ifdef _KERNEL
#ifndef _LOCORE

#include <machine/intr.h>

extern void delay(int n);
extern int cpuspeed;
static __inline void __attribute__((__unused__))
DELAY(int n)
{
	register int __N = cpuspeed * n;

	do {
		__asm("addiu %0,%1,-1" : "=r" (__N) : "0" (__N));
	} while (__N > 0);
}

#endif	/* !_LOCORE */
#endif	/* _KERNEL */

#endif	/* !_MIPSCO_PARAM_H_ */
