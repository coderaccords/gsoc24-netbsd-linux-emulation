/* $NetBSD: pmap.h,v 1.17 1998/05/19 00:29:03 thorpej Exp $ */

/*-
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe of the Numerical Aerospace Simulation Facility,
 * NASA Ames Research Center and by Chris G. Demetriou.
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
 * Copyright (c) 1987 Carnegie-Mellon University
 * Copyright (c) 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
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
 *	@(#)pmap.h	8.1 (Berkeley) 6/10/93
 */

#ifndef	_PMAP_MACHINE_
#define	_PMAP_MACHINE_

#include <sys/queue.h>
#include <machine/pte.h>

/*
 * Machine-dependent virtual memory state.
 *
 * If we ever support processor numbers higher than 63, we'll have to
 * rethink the CPU mask.
 *
 * XXX When we support multiple processors, pm_asn and pm_asngen need
 * XXX to be per-processor.
 */
struct pmap {
	LIST_ENTRY(pmap)	pm_list;	/* list of all pmaps */
	pt_entry_t		*pm_lev1map;	/* level 1 map */
	int			pm_count;	/* pmap reference count */
	simple_lock_data_t	pm_lock;	/* lock on pmap */
	struct pmap_statistics	pm_stats;	/* pmap statistics */
	long			pm_nlev2;	/* level 2 pt page count */
	long			pm_nlev3;	/* level 3 pt page count */
	unsigned int		pm_asn;		/* address space number */
	unsigned long		pm_asngen;	/* ASN generation number */
	unsigned long		pm_cpus;	/* mask of CPUs using pmap */
};

typedef struct pmap	*pmap_t;

#define	PMAP_ASN_RESERVED	0	/* reserved for Lev1map users */

extern struct pmap	kernel_pmap_store;

#define pmap_kernel()	(&kernel_pmap_store)

/*
 * For each vm_page_t, there is a list of all currently valid virtual
 * mappings of that page.  An entry is a pv_entry_t, the list is pv_table.
 */
typedef struct pv_entry {
	LIST_ENTRY(pv_entry) pv_list;	/* pv_entry list */
	struct pmap	*pv_pmap;	/* pmap where mapping lies */
	vm_offset_t	pv_va;		/* virtual address for mapping */
} *pv_entry_t;

/*
 * The head of the list of pv_entry_t's, also contains page attributes.
 */
struct pv_head {
	LIST_HEAD(, pv_entry) pvh_list;		/* pv_entry list */
	int pvh_attrs;				/* page attributes */
	int pvh_usage;				/* page usage */
	int pvh_ptref;				/* ref count if PT page */
};

/* pvh_attrs */
#define	PGA_MODIFIED		0x01		/* modified */
#define	PGA_REFERENCED		0x02		/* referenced */

/* pvh_usage */
#define	PGU_NORMAL		0		/* free or normal use */
#define	PGU_PVENT		1		/* PV entries */
#define	PGU_L1PT		2		/* level 1 page table */
#define	PGU_L2PT		3		/* level 2 page table */
#define	PGU_L3PT		4		/* level 3 page table */

#define	PGU_ISPTPAGE(pgu)	((pgu) >= PGU_L1PT)

struct pv_page_info {
	TAILQ_ENTRY(pv_page) pgi_list;
	LIST_HEAD(, pv_entry) pgi_freelist;
	int pgi_nfree;
};

#define	NPVPPG ((NBPG - sizeof(struct pv_page_info)) / sizeof(struct pv_entry))

struct pv_page {
	struct pv_page_info pvp_pgi;
	struct pv_entry pvp_pv[NPVPPG];
};

#ifdef _KERNEL

#ifndef _LKM
#include "opt_new_scc_driver.h"
#include "opt_dec_3000_300.h"			/* XXX */
#include "opt_dec_3000_500.h"			/* XXX */
#include "opt_dec_kn8ae.h"			/* XXX */
#include "opt_dec_kn300.h"			/* XXX */

#if defined(NEW_SCC_DRIVER)
#if defined(DEC_KN8AE) || defined(DEC_KN300)
#define	_PMAP_MAY_USE_PROM_CONSOLE
#endif
#else /* ! NEW_SCC_DRIVER */
#if defined(DEC_3000_300)		\
 || defined(DEC_3000_500)		\
 || defined(DEC_KN300)			\
 || defined(DEC_KN8AE) 				/* XXX */
#define _PMAP_MAY_USE_PROM_CONSOLE		/* XXX */
#endif						/* XXX */
#endif /* NEW_SCC_DRIVER */
#endif /* _LKM */
 
#define	pmap_resident_count(pmap)	((pmap)->pm_stats.resident_count)
#define	pmap_wired_count(pmap)		((pmap)->pm_stats.wired_count)

extern	pt_entry_t *VPT;		/* Virtual Page Table */

#define	PMAP_STEAL_MEMORY		/* enable pmap_steal_memory() */

vm_offset_t vtophys __P((vm_offset_t));

/* Machine-specific functions. */
void	pmap_bootstrap __P((vm_offset_t ptaddr, u_int maxasn));
void	pmap_emulate_reference __P((struct proc *p, vm_offset_t v,
		int user, int write));
#ifdef _PMAP_MAY_USE_PROM_CONSOLE
int	pmap_uses_prom_console __P((void));
#endif

#define	pmap_pte_pa(pte)	(PG_PFNUM(*(pte)) << PGSHIFT)
#define	pmap_pte_prot(pte)	(*(pte) & PG_PROT)
#define	pmap_pte_w(pte)		(*(pte) & PG_WIRED)
#define	pmap_pte_v(pte)		(*(pte) & PG_V)
#define	pmap_pte_pv(pte)	(*(pte) & PG_PVLIST)
#define	pmap_pte_asm(pte)	(*(pte) & PG_ASM)

#define	pmap_pte_set_w(pte, v)						\
do {									\
	if (v)								\
		*(pte) |= PG_WIRED;					\
	else								\
		*(pte) &= ~PG_WIRED;					\
} while (0)

#define	pmap_pte_w_chg(pte, nw)	((nw) ^ pmap_pte_w(pte))

#define	pmap_pte_set_prot(pte, np)					\
do {									\
	*(pte) &= ~PG_PROT;						\
	*(pte) |= (np);							\
} while (0)

#define	pmap_pte_prot_chg(pte, np) ((np) ^ pmap_pte_prot(pte))

static __inline pt_entry_t *pmap_l2pte __P((pmap_t, vm_offset_t, pt_entry_t *));
static __inline pt_entry_t *pmap_l3pte __P((pmap_t, vm_offset_t, pt_entry_t *));

#define	pmap_l1pte(pmap, v)						\
	(&(pmap)->pm_lev1map[l1pte_index((vm_offset_t)(v))])

static __inline pt_entry_t *
pmap_l2pte(pmap, v, l1pte)
	pmap_t pmap;
	vm_offset_t v;
	pt_entry_t *l1pte;
{
	pt_entry_t *lev2map;

	if (l1pte == NULL) {
		l1pte = pmap_l1pte(pmap, v);
		if (pmap_pte_v(l1pte) == 0)
			return (NULL);
	}

	lev2map = (pt_entry_t *)ALPHA_PHYS_TO_K0SEG(pmap_pte_pa(l1pte));
	return (&lev2map[l2pte_index(v)]);
}

static __inline pt_entry_t *
pmap_l3pte(pmap, v, l2pte)
	pmap_t pmap;
	vm_offset_t v;
	pt_entry_t *l2pte;
{
	pt_entry_t *l1pte, *lev2map, *lev3map;

	if (l2pte == NULL) {
		l1pte = pmap_l1pte(pmap, v);
		if (pmap_pte_v(l1pte) == 0)
			return (NULL);

		lev2map = (pt_entry_t *)ALPHA_PHYS_TO_K0SEG(pmap_pte_pa(l1pte));
		l2pte = &lev2map[l2pte_index(v)];
		if (pmap_pte_v(l2pte) == 0)
			return (NULL);
	}

	lev3map = (pt_entry_t *)ALPHA_PHYS_TO_K0SEG(pmap_pte_pa(l2pte));
	return (&lev3map[l3pte_index(v)]);
}

#endif /* _KERNEL */

#endif /* _PMAP_MACHINE_ */
