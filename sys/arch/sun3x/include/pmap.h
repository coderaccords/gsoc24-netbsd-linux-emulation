/*	$NetBSD: pmap.h,v 1.4 1997/02/14 03:55:33 gwr Exp $	*/

/*-
 * Copyright (c) 1997 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jeremy Cooper.
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

#ifndef	_SUN3X_PMAP_H
#define	_SUN3X_PMAP_H

/*
 * Physical map structures exported to the VM code.
 */

struct pmap {
	struct a_tmgr_struct	*pm_a_tmgr; 	/* Level-A table manager */
	u_long              	pm_a_phys;  	/* MMU level-A phys addr */
	int                 	pm_refcount;	/* reference count */
	simple_lock_data_t  	pm_lock;    	/* lock on pmap */
};

typedef struct pmap 	*pmap_t;

#ifdef _KERNEL
extern	struct pmap 	kernel_pmap;
#define	pmap_kernel()	(&kernel_pmap)

/* This is called from locore.s:cpu_switch() */
void	pmap_activate __P((pmap_t pmap));

#define _pmap_fault(map, va, ftype) \
	vm_fault(map, va, ftype, 0)

/* Common function for pmap_resident_count(), pmap_wired_count() */
segsz_t pmap_count __P((pmap_t, int));

/* This needs to be a macro for kern_sysctl.c */
#define	pmap_resident_count(pmap)	(pmap_count((pmap), 0))

/* This needs to be a macro for vm_mmap.c */
#define	pmap_wired_count(pmap)  	(pmap_count((pmap), 1))

/* We use the PA plus some low bits for device mmap. */
#define pmap_phys_address(addr) 	(addr)

/*
 * Flags to tell pmap_enter `this is not to be cached', etc.
 * Since physical addresses are always aligned, we can use
 * the low order bits for this.
 */
#define	PMAP_VME16	0x10	/* pmap will add the necessary offset */
#define	PMAP_VME32	0x20	/* etc. */
#define	PMAP_NC		0x40	/* tells pmap_enter to set PTE_CI */
#define	PMAP_SPEC	0xFF	/* mask to get all above. */

#endif	/* _KERNEL */
#endif	/* _SUN3X_PMAP_H */
