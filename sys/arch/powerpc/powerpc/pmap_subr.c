/*	$NetBSD: pmap_subr.c,v 1.2 2002/07/18 22:51:58 matt Exp $	*/
/*-
 * Copyright (c) 2001 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Matt Thomas <matt@3am-software.com> of Allegro Networks, Inc.
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

#include "opt_altivec.h"
#include <sys/param.h>
#include <sys/malloc.h>
#include <sys/proc.h>
#include <sys/sched.h>
#include <sys/systm.h>

#include <uvm/uvm_extern.h>
#ifdef PPC_MPC6XX
#include <powerpc/mpc6xx/vmparam.h>
#ifdef ALTIVEC
#include <powerpc/altivec.h>
#endif
#endif
#include <powerpc/psl.h>

#define	MFMSR()		mfmsr()
#define	MTMSR(psl)	__asm __volatile("sync; mtmsr %0; isync" :: "r"(psl))

static __inline u_int32_t
mfmsr(void)
{
	u_int32_t psl;
	__asm __volatile("mfmsr %0" : "=r"(psl) : );
	return psl;
}

/*
 * This file uses a sick & twisted method to deal with the common pmap
 * operations of zero'ing, copying, and syncing the page with the
 * instruction cache.
 *
 * When a PowerPC CPU takes an exception (interrupt or trap), that 
 * exception is handled with the MMU off.  The handler has to explicitly
 * renable the MMU before continuing.  The state of the MMU will be restored
 * when the exception is returned from.
 *
 * Therefore if we disable the MMU we know that doesn't affect any exception.
 * So it's safe for us to disable the MMU so we can deal with physical
 * addresses without having to map any pages via a BAT or into a page table.
 *
 * It's also safe to do regardless of IPL.
 *
 * However while relocation is off, we MUST not access the kernel stack in
 * any manner since it will probably no longer be mapped.  This mean no
 * calls while relocation is off.  The AltiVEC routines need to handle the
 * MSR fiddling themselves so they save things on the stack.
 */

/*
 * Fill the given physical page with zeroes.
 */
void
pmap_zero_page(paddr_t pa)
{
	size_t linewidth;
	u_int32_t msr;

#ifdef ALTIVEC
	if (pmap_use_altivec) {
		vzeropage(pa);
		return;
	}
#endif

	/*
	 * Turn off data relocation (DMMU off).
	 */
#ifdef PPC_MPC6XX
	if (pa >= SEGMENT_LENGTH) {
#endif
		msr = MFMSR();
		MTMSR(msr & ~PSL_DR);
#ifdef PPC_MPC6XX
	}
#endif

	/*
	 * Zero the page.  Since DR is off, the address is assumed to
	 * valid but we know that UVM will never pass a uncacheable page.
	 * Don't use dcbz if we don't know the cache width.
	 */
	if ((linewidth = curcpu()->ci_ci.dcache_line_size) == 0) {
		long *dp = (long *)pa;
		long * const ep = dp + NBPG/sizeof(dp[0]);
		do {
			dp[0] = 0; dp[1] = 0; dp[2] = 0; dp[3] = 0;
			dp[4] = 0; dp[5] = 0; dp[6] = 0; dp[7] = 0;
		} while ((dp += 8) < ep);
	} else {
		size_t i = 0;
		do {
			__asm ("dcbz %0,%1" :: "r"(pa), "r"(i)); i += linewidth;
			__asm ("dcbz %0,%1" :: "r"(pa), "r"(i)); i += linewidth;
		} while (i < NBPG);
	}

	/*
	 * Restore data relocation (DMMU on).
	 */
#ifdef PPC_MPC6XX
	if (pa >= SEGMENT_LENGTH)
#endif
		MTMSR(msr);
}

/*
 * Copy the given physical source page to its destination.
 */
void
pmap_copy_page(paddr_t src, paddr_t dst)
{
	const long *sp;
	long *dp;
	size_t i;
	u_int32_t msr;

#ifdef ALTIVEC
	if (pmap_use_altivec) {
		vcopypage(dst, src);
		return;
	}
#endif

#ifdef PPC_MPC6XX
	if (src < SEGMENT_LENGTH && dst < SEGMENT_LENGTH) {
		/*
		 * Copy the page (memcpy is optimized, right? :)
		 */
		memcpy((void *) dst, (void *) src, NBPG);
		return;
	}
#endif

	/*
	 * Turn off data relocation (DMMU off).
	 */
	msr = MFMSR();
	MTMSR(msr & ~PSL_DR);

	/*
	 * Copy the page.  Don't use memcpy as we can't refer to the
	 * kernel stack at this point.
	 */
	sp = (long *) src;
	dp = (long *) dst;
	for (i = 0; i < NBPG/sizeof(dp[0]); i += 8, dp += 8, sp += 8) {
		dp[0] = sp[0]; dp[1] = sp[1]; dp[2] = sp[2]; dp[3] = sp[3];
		dp[4] = sp[4]; dp[5] = sp[5]; dp[6] = sp[6]; dp[7] = sp[7];
	}

	/*
	 * Restore data relocation (DMMU on).
	 */
	MTMSR(msr);
}

void
pmap_syncicache(paddr_t pa, psize_t len)
{
	const size_t linewidth = curcpu()->ci_ci.icache_line_size;
	u_int32_t msr;
	size_t i;

#ifdef PPC_MPC6XX
	if (pa + len <= SEGMENT_LENGTH) {
		__syncicache((void *)pa, len);
		return;
	}
#endif

	/*
	 * Turn off instruction and data relocation (MMU off).
	 */
	msr = MFMSR();
	MTMSR(msr & ~(PSL_IR|PSL_DR));

	/*
	 * Make sure to start on a cache boundary.
	 */
	len += pa - (pa & ~linewidth);
	pa &= ~linewidth;

	/*
	 * Write out the data cache
	 */
	i = 0;
	do {
		__asm ("dcbst %0,%1" :: "r"(pa), "r"(i)); i += linewidth;
	} while (i < len);

	/*
	 * Wait for it to finish
	 */
	__asm __volatile("sync");

	/*
	 * Now invalidate the instruction cache.
	 */
	i = 0;
	do {
		__asm ("icbi %0,%1" :: "r"(pa), "r"(i)); i += linewidth;
	} while (i < len);

	/*
	 * Restore relocation (MMU on).  (this will do the required
	 * sync and isync).
	 */
	MTMSR(msr);
}

boolean_t
pmap_pageidlezero(paddr_t pa)
{
	u_int32_t msr;
	u_int32_t *dp = (u_int32_t *) pa;
	boolean_t rv = TRUE;
	int i;

#ifdef PPC_MPC6XX
	if (pa < SEGMENT_LENGTH) {
		for (i = 0; i < NBPG / sizeof(dp[0]); i++) {
			if (sched_whichqs != 0)
				return FALSE;
			*dp++ = 0;
		}
		return TRUE;
	}
#endif

	/*
	 * Turn off instruction and data relocation (MMU off).
	 */
	msr = MFMSR();
	MTMSR(msr & ~(PSL_IR|PSL_DR));

	/*
	 * Zero the page until a process becomes runnable.
	 */
	for (i = 0; i < NBPG / sizeof(dp[0]); i++) {
		if (sched_whichqs != 0) {
			rv = FALSE;
			break;
		}
		*dp++ = 0;
	}

	/*
	 * Restore relocation (MMU on).
	 */
	MTMSR(msr);
	return rv;
}
