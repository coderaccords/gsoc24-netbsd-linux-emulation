/*	$NetBSD: vmparam3x.h,v 1.5 1998/06/10 02:42:14 gwr Exp $	*/

/*
 * Copyright (c) 1988 University of Utah.
 * Copyright (c) 1982, 1986, 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * the Systems Programming Group of the University of Utah Computer
 * Science Department.
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
 *	from: Utah $Hdr: vmparam.h 1.16 91/01/18$
 *	from: @(#)vmparam.h	7.3 (Berkeley) 5/7/91
 *	vmparam.h,v 1.2 1993/05/22 07:58:38 cgd Exp
 */

/*
 * Machine dependent constants for Sun3X
 */

/*
 * USRTEXT is the start of the user text/data space, while
 * USRSTACK is the top (end) of the user stack.
 */
#define	USRTEXT 	NBPG		/* Start of user text */
#define	USRSTACK	KERNBASE	/* High end of user stack */

/*
 * Virtual memory related constants, all in bytes
 */
#ifndef MAXTSIZ
#define	MAXTSIZ		(8*1024*1024)		/* max text size */
#endif
#ifndef DFLDSIZ
#define	DFLDSIZ		(32*1024*1024)		/* initial data size limit */
#endif
#ifndef MAXDSIZ
#define	MAXDSIZ		(128*1024*1024)		/* max data size */
#endif
#ifndef	DFLSSIZ
#define	DFLSSIZ		(2*1024*1024)		/* initial stack size limit */
#endif
#ifndef	MAXSSIZ
#define	MAXSSIZ		(32*1024*1024)		/* max stack size */
#endif

/*
 * PTEs for mapping user space into the kernel for phyio operations.
 * The actual limitation for physio requests will be the DVMA space,
 * and that is fixed by hardware design at 1MB.  We could make the
 * physio map larger than that, but it would not buy us much.
 */
#ifndef USRIOSIZE
#define USRIOSIZE	128	/* 1 MB */
#endif

/*
 * PTEs for system V style shared memory.
 * This is basically slop for kmempt which we actually allocate (malloc) from.
 */
#ifndef SHMMAXPGS
#define SHMMAXPGS	2048 	/* 16 MB */
#endif

/*
 * Mach-derived constants:
 */

/* user/kernel map constants */
#define VM_MIN_ADDRESS		((vm_offset_t)0)
#define VM_MAX_ADDRESS		((vm_offset_t)KERNBASE)
#define VM_MAXUSER_ADDRESS	((vm_offset_t)KERNBASE)
#define VM_MIN_KERNEL_ADDRESS	((vm_offset_t)KERNBASE)
#define VM_MAX_KERNEL_ADDRESS	((vm_offset_t)KERN_END)

/* virtual sizes (bytes) for various kernel submaps */
#define VM_MBUF_SIZE		(NMBCLUSTERS*MCLBYTES)
#define VM_KMEM_SIZE		(NKMEMCLUSTERS*CLBYTES)
#define VM_PHYS_SIZE		(USRIOSIZE*CLBYTES)

#define MACHINE_NEW_NONCONTIG	/* VM <=> pmap interface modifier */

#define VM_PHYSSEG_MAX		4
#define VM_PHYSSEG_STRAT	VM_PSTRAT_BSEARCH
#define VM_PHYSSEG_NOADD	/* can't add RAM after vm_mem_init */

struct pmap_physseg {
	/* NULL */
};
