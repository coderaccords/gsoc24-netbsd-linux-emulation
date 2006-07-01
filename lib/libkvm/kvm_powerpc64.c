/*	$NetBSD: kvm_powerpc64.c,v 1.1 2006/07/01 19:21:11 ross Exp $	*/

/*
 * Copyright (c) 2005 Wasabi Systems, Inc.
 * All rights reserved.
 *
 * Written by Allen Briggs for Wasabi Systems, Inc.
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
 *      This product includes software developed for the NetBSD Project by
 *      Wasabi Systems, Inc.
 * 4. The name of Wasabi Systems, Inc. may not be used to endorse
 *    or promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY WASABI SYSTEMS, INC. ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL WASABI SYSTEMS, INC
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
/*-
 * Copyright (C) 1996 Wolfgang Solfrank.
 * Copyright (C) 1996 TooLs GmbH.
 * All rights reserved.
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
 *	This product includes software developed by TooLs GmbH.
 * 4. The name of TooLs GmbH may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY TOOLS GMBH ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL TOOLS GMBH BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * PowerPC machine dependent routines for kvm.
 */

#include <sys/param.h>
#include <sys/exec.h>

#include <uvm/uvm_extern.h>

#include <db.h>
#include <limits.h>
#include <kvm.h>
#include <stdlib.h>
#include <unistd.h>

#include "kvm_private.h"

#include <sys/kcore.h>
#include <machine/kcore.h>

#include <powerpc/spr.h>
#include <powerpc/oea/bat.h>
#include <powerpc/oea/pte.h>

static int	_kvm_match_601bat(kvm_t *kd, u_long va, u_long *pa, int *off);
static int	_kvm_match_bat(kvm_t *kd, u_long va, u_long *pa, int *off);
static int	_kvm_match_sr(kvm_t *kd, u_long va, u_long *pa, int *off);
static struct pte *_kvm_scan_pteg(struct pteg *pteg, uint32_t vsid,
				  uint32_t api, int secondary);

void
_kvm_freevtop(kd)
	kvm_t *kd;
{
	if (kd->vmst != 0)
		free(kd->vmst);
}

/*ARGSUSED*/
int
_kvm_initvtop(kd)
	kvm_t *kd;
{

	return 0;
}

#define SR_VSID_HASH_MASK	0x0007ffff

static struct pte *
_kvm_scan_pteg(pteg, vsid, api, secondary)
	struct pteg *pteg;
	uint32_t vsid;
	uint32_t api;
	int secondary;
{
	struct pte	*pte;
	u_long		ptehi;
	int		i;

	for (i=0 ; i<8 ; i++) {
		pte = &pteg->pt[i];
		ptehi = (u_long) pte->pte_hi;
		if ((ptehi & PTE_VALID) == 0)
			continue;
		if ((ptehi & PTE_HID) != secondary)
			continue;
		if (((ptehi & PTE_VSID) >> PTE_VSID_SHFT) != vsid)
			continue;
		if (((ptehi & PTE_API) >> PTE_API_SHFT) != api)
			continue;
		return pte;
	}
	return NULL;
}

#define HASH_MASK	0x0007ffff

static int
_kvm_match_sr(kd, va, pa, off)
	kvm_t *kd;
	u_long va;
	u_long *pa;
	int *off;
{
	cpu_kcore_hdr_t	*cpu_kh;
	struct pteg	pteg;
	struct pte	*pte;
	uint32_t	sr, pgoff, vsid, pgidx, api, hash;
	uint32_t	htaborg, htabmask, mhash;
	u_long		pteg_vaddr;

	cpu_kh = kd->cpu_data;

	sr = cpu_kh->sr[(va >> 28) & 0xf];
	if ((sr & SR_TYPE) != 0) {
		/* Direct-store segment (shouldn't be) */
		return 0;
	}

	pgoff = va & ADDR_POFF;
	vsid = sr & SR_VSID;
	pgidx = (va & ADDR_PIDX) >> ADDR_PIDX_SHFT;
	api = pgidx >> 10;
	hash = (vsid & HASH_MASK) ^ pgidx;

	htaborg = cpu_kh->sdr1 & 0xffff0000;
	htabmask = cpu_kh->sdr1 & 0x1ff;

	mhash = (hash >> 10) & htabmask;

	pteg_vaddr = ( htaborg & 0xfe000000) | ((hash & 0x3ff) << 6)
		   | ((htaborg & 0x01ff0000) | (mhash << 16));

	if (pread(kd->pmfd, (void *) &pteg, sizeof(pteg),
		  _kvm_pa2off(kd, pteg_vaddr)) != sizeof(pteg)) {
		_kvm_syserr(kd, 0, "could not read primary PTEG");
		return 0;
	}

	if ((pte = _kvm_scan_pteg(&pteg, vsid, api, 0)) != NULL) {
		*pa = (pte->pte_lo & PTE_RPGN) | pgoff;
		*off = NBPG - pgoff;
		return 1;
	}

	hash = (~hash) & HASH_MASK;
	mhash = (hash >> 10) & htabmask;

	pteg_vaddr = ( htaborg & 0xfe000000) | ((hash & 0x3ff) << 6)
		   | ((htaborg & 0x01ff0000) | (mhash << 16));

	if (pread(kd->pmfd, (void *) &pteg, sizeof(pteg),
		  _kvm_pa2off(kd, pteg_vaddr)) != sizeof(pteg)) {
		_kvm_syserr(kd, 0, "could not read secondary PTEG");
		return 0;
	}

	if ((pte = _kvm_scan_pteg(&pteg, vsid, api, 0)) != NULL) {
		*pa = (pte->pte_lo & PTE_RPGN) | pgoff;
		*off = NBPG - pgoff;
		return 1;
	}

	return 0;
}

/*
 * Translate a KVA to a PA
 */
int
_kvm_kvatop(kd, va, pa)
	kvm_t *kd;
	u_long va;
	u_long *pa;
{
	cpu_kcore_hdr_t	*cpu_kh;
	int		offs;
	uint32_t	pvr;

	if (ISALIVE(kd)) {
		_kvm_err(kd, 0, "vatop called in live kernel!");
		return 0;
	}

	cpu_kh = kd->cpu_data;

	pvr = (cpu_kh->pvr >> 16);

	switch (pvr) {

	if (_kvm_match_sr(kd, va, pa, &offs))
		return offs;

	/* No hit -- no translation */
	*pa = (u_long)~0UL;
	return 0;
}

off_t
_kvm_pa2off(kd, pa)
	kvm_t *kd;
	u_long pa;
{
	cpu_kcore_hdr_t	*cpu_kh;
	phys_ram_seg_t	*ram;
	off_t		off;
	void		*e;

	cpu_kh = kd->cpu_data;
	e = (char *) kd->cpu_data + kd->cpu_dsize;
        ram = (void *)((char *)(void *)cpu_kh + ALIGN(sizeof *cpu_kh));
	off = kd->dump_off;
	do {
		if (pa >= ram->start && (pa - ram->start) < ram->size) {
			return off + (pa - ram->start);
		}
		ram++;
		off += ram->size;
	} while ((void *) ram < e && ram->size);

	_kvm_err(kd, 0, "pa2off failed for pa 0x%08lx\n", pa);
	return (off_t) -1;
}

/*
 * Machine-dependent initialization for ALL open kvm descriptors,
 * not just those for a kernel crash dump.  Some architectures
 * have to deal with these NOT being constants!  (i.e. m68k)
 */
int
_kvm_mdopen(kd)
	kvm_t	*kd;
{
	uintptr_t max_uva;
	extern struct ps_strings *__ps_strings;

#if 0   /* XXX - These vary across powerpc machines... */
	kd->usrstack = USRSTACK;
	kd->min_uva = VM_MIN_ADDRESS;
	kd->max_uva = VM_MAXUSER_ADDRESS;
#endif
	/* This is somewhat hack-ish, but it works. */
	max_uva = (uintptr_t) (__ps_strings + 1);
	kd->usrstack = max_uva;
	kd->max_uva  = max_uva;
	kd->min_uva  = 0;

	return (0);
}
