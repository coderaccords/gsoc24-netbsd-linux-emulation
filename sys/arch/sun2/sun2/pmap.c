/*	$NetBSD: pmap.c,v 1.5 2001/04/21 23:51:22 thorpej Exp $	*/

/*-
 * Copyright (c) 1996 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Adam Glass, Gordon W. Ross, and Matthew Fredette.
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

/*
 * Some notes:
 *
 * sun2s have contexts (8).  In this pmap design, the kernel is mapped
 * into context zero.  Processes take up a known portion of the context,
 * and compete for the available contexts on a LRU basis.
 *
 * sun2s also have this evil "PMEG" crapola.  Essentially each "context"'s
 * address space is defined by the 512 one-byte entries in the segment map.
 * Each of these 1-byte entries points to a "Page Map Entry Group" (PMEG)
 * which contains the mappings for that virtual segment.  (This strange
 * terminology invented by Sun and preserved here for consistency.)
 * Each PMEG maps a segment of 32Kb length, with 16 pages of 2Kb each.
 *
 * As you might guess, these PMEGs are in short supply and heavy demand.
 * PMEGs allocated to the kernel are "static" in the sense that they can't
 * be stolen from it.  PMEGs allocated to a particular segment of a
 * pmap's virtual space will be fought over by the other pmaps.
 *
 * This pmap was once sys/arch/sun3/sun3/pmap.c revision 1.124.
 */

/*
 * Cache management:
 * sun2's don't have cache implementations, but for now the caching
 * code remains in.  it's harmless (and, due to our 0 definitions of
 * PG_NC and BADALIAS, should optimize away), and keeping it in makes
 * it easier to diff this file against its cousin, sys/arch/sun3/sun3/pmap.c.
 */

/*
 * wanted attributes:
 *       pmegs that aren't needed by a pmap remain in the MMU.
 *       quick context switches between pmaps
 */

/*
 * Project1:  Use a "null" context for processes that have not
 * touched any user-space address recently.  This is efficient
 * for things that stay in the kernel for a while, waking up
 * to handle some I/O then going back to sleep (i.e. nfsd).
 * If and when such a process returns to user-mode, it will
 * fault and be given a real context at that time.
 *
 * This also lets context switch be fast, because all we need
 * to do there for the MMU is slam the context register.
 *
 * Project2:  Use a private pool of PV elements.  This pool can be
 * fixed size because the total mapped virtual space supported by
 * the MMU H/W (and this pmap) is fixed for all time.
 */

#include "opt_ddb.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/malloc.h>
#include <sys/pool.h>
#include <sys/user.h>
#include <sys/queue.h>
#include <sys/kcore.h>

#include <uvm/uvm.h>

/* XXX - Pager hacks... (explain?) */
#define PAGER_SVA (uvm.pager_sva)
#define PAGER_EVA (uvm.pager_eva)

#include <machine/cpu.h>
#include <machine/dvma.h>
#include <machine/idprom.h>
#include <machine/kcore.h>
#include <machine/promlib.h>
#include <machine/pmap.h>
#include <machine/pte.h>
#include <machine/vmparam.h>

#include <sun2/sun2/control.h>
#include <sun2/sun2/fc.h>
#include <sun2/sun2/machdep.h>

#ifdef DDB
#include <ddb/db_output.h>
#else
#define db_printf printf
#endif

/* Verify this correspondence between definitions. */
#if	(PMAP_OBIO << PG_MOD_SHIFT) != PGT_OBIO
#error	"PMAP_XXX definitions don't match pte.h!"
#endif

/* Type bits in a "pseudo" physical address. (XXX: pmap.h?) */
#define PMAP_TYPE	PMAP_MBIO

/*
 * Local convenience macros
 */

#define DVMA_MAP_END	(DVMA_MAP_BASE + DVMA_MAP_AVAIL)

/* User segments are all of them. */
#define	NUSEG	(NSEGMAP)

#define VA_SEGNUM(x)	((u_int)(x) >> SEGSHIFT)

/*
 * Only "main memory" pages are registered in the pv_lists.
 * This macro is used to determine if a given pte refers to
 * "main memory" or not.  One slight hack here deserves more
 * explanation:  On the Sun-2, the bwtwo and zsc1 appear
 * as PG_OBMEM devices at 0x00700000 and 0x00780000, 
 * respectively.  We do not want to consider these as 
 * "main memory" so the macro below treats obmem addresses
 * >= 0x00700000 as device addresses.  NB: this means for now,
 * you can't have a headless Sun-2 with 8MB of main memory.
 */
#define	IS_MAIN_MEM(pte) (((pte) & PG_TYPE) == 0 && PG_PA(pte) < 0x00700000)

/* Does this (pseudo) PA represent device space? */
#define PA_IS_DEV(pa) (((pa) & PMAP_TYPE) != 0 || (pa) >= 0x00700000)

#define	BADALIAS(a1, a2)	(0)


/*
 * Debugging support.
 */
#define	PMD_ENTER	1
#define	PMD_LINK	2
#define	PMD_PROTECT	4
#define	PMD_SWITCH	8
#define PMD_COW		0x10
#define PMD_MODBIT	0x20
#define PMD_REFBIT	0x40
#define PMD_WIRING	0x80
#define PMD_CONTEXT	0x100
#define PMD_CREATE	0x200
#define PMD_SEGMAP	0x400
#define PMD_SETPTE	0x800
#define PMD_FAULT  0x1000

#define	PMD_REMOVE	PMD_ENTER
#define	PMD_UNLINK	PMD_LINK

#ifdef	PMAP_DEBUG
int pmap_debug = 0;
int pmap_db_watchva = -1;
int pmap_db_watchpmeg = -1;
#endif	/* PMAP_DEBUG */


/*
 * Miscellaneous variables.
 *
 * For simplicity, this interface retains the variables
 * that were used in the old interface (without NONCONTIG).
 * These are set in pmap_bootstrap() and used in
 * pmap_next_page().
 */
vm_offset_t virtual_avail, virtual_end;
vm_offset_t avail_start, avail_end;
#define	managed(pa)	(((pa) >= avail_start) && ((pa) < avail_end))

/* used to skip a single hole in RAM */
static vm_offset_t hole_start, hole_size;

/* This is for pmap_next_page() */
static vm_offset_t avail_next;

/* This is where we map a PMEG without a context. */
static vm_offset_t temp_seg_va;
static int temp_seg_inuse;

/*
 * Location to store virtual addresses
 * to be used in copy/zero operations.
 */
vm_offset_t tmp_vpages[2] = {
	KERNBASE + NBPG * 5,
	KERNBASE + NBPG * 6 };
int tmp_vpages_inuse;

static int pmap_version = 1;
struct pmap kernel_pmap_store;
#define kernel_pmap (&kernel_pmap_store)
static u_char kernel_segmap[NSEGMAP];

/* memory pool for pmap structures */
struct pool	pmap_pmap_pool;

/* statistics... */
struct pmap_stats {
	int	ps_enter_firstpv;	/* pv heads entered */
	int	ps_enter_secondpv;	/* pv nonheads entered */
	int	ps_unlink_pvfirst;	/* of pv_unlinks on head */
	int	ps_unlink_pvsearch;	/* of pv_unlink searches */
	int	ps_pmeg_faultin;	/* pmegs reloaded */
	int	ps_changeprots;		/* of calls to changeprot */
	int	ps_changewire;		/* useless wiring changes */
	int	ps_npg_prot_all;	/* of active pages protected */
	int	ps_npg_prot_actual;	/* pages actually affected */
	int	ps_vac_uncached;	/* non-cached due to bad alias */
	int	ps_vac_recached;	/* re-cached when bad alias gone */
} pmap_stats;


/*
 * locking issues:  These used to do spl* stuff.
 * XXX: Use these for reentrance detection?
 */
#define PMAP_LOCK() 	(void)/XXX
#define PMAP_UNLOCK()	(void)/XXX

#define pmap_lock(pmap) simple_lock(&pmap->pm_lock)
#define pmap_unlock(pmap) simple_unlock(&pmap->pm_lock)
#define pmap_add_ref(pmap) ++pmap->pm_refcount
#define pmap_del_ref(pmap) --pmap->pm_refcount
#define pmap_refcount(pmap) pmap->pm_refcount

/*
 * Note that splvm() is used in routines called at splnet() and
 * MUST NOT lower the priority.  For this reason we arrange that:
 *    splimp = max(splnet,splbio)
 * Would splvm() be more natural here? (same level as splimp).
 */

#ifdef	PMAP_DEBUG
#define	CHECK_SPL() do { \
	if ((getsr() & PSL_IPL) < PSL_IPL4) \
		panic("pmap: bad spl, line %d", __LINE__); \
} while (0)
#else	/* PMAP_DEBUG */
#define	CHECK_SPL() (void)0
#endif	/* PMAP_DEBUG */


/*
 * PV support.
 * (i.e. Find all virtual mappings of a physical page.)
 */

/*
 * XXX - Could eliminate this by causing managed() to return 0
 * ( avail_start = avail_end = 0 )
 */
int pv_initialized = 0;

/* One of these for each mapped virtual page. */
struct pv_entry {
	struct pv_entry *pv_next;
	pmap_t	       pv_pmap;
	vm_offset_t      pv_va;
};
typedef struct pv_entry *pv_entry_t;

/* Table of PV list heads (per physical page). */
static struct pv_entry **pv_head_tbl;

/* Free list of PV entries. */
static struct pv_entry *pv_free_list;

/* Table of flags (per physical page). */
static u_char *pv_flags_tbl;

/* These are as in the MMU but shifted by PV_SHIFT. */
#define PV_SHIFT	20
#define PV_VALID  (PG_VALID >> PV_SHIFT)
#define PV_NC     (PG_NC >> PV_SHIFT)
#define PV_TYPE   (PG_TYPE >> PV_SHIFT)
#define PV_REF    (PG_REF >> PV_SHIFT)
#define PV_MOD    (PG_MOD >> PV_SHIFT)


/*
 * context structures, and queues
 */

struct context_state {
	TAILQ_ENTRY(context_state) context_link;
	int            context_num;
	struct pmap   *context_upmap;
};
typedef struct context_state *context_t;

#define INVALID_CONTEXT -1	/* impossible value */
#define EMPTY_CONTEXT 0
#define KERNEL_CONTEXT 0
#define FIRST_CONTEXT 1
#define	has_context(pmap)	(((pmap)->pm_ctxnum != EMPTY_CONTEXT) == ((pmap) != kernel_pmap))

TAILQ_HEAD(context_tailq, context_state)
	context_free_queue, context_active_queue;

static struct context_state context_array[NCONTEXT];


/*
 * PMEG structures, queues, and macros
 */
#define PMEGQ_FREE     0
#define PMEGQ_INACTIVE 1
#define PMEGQ_ACTIVE   2
#define PMEGQ_KERNEL   3
#define PMEGQ_NONE     4

struct pmeg_state {
	TAILQ_ENTRY(pmeg_state) pmeg_link;
	int            pmeg_index;
	pmap_t         pmeg_owner;
	int            pmeg_version;
	vm_offset_t    pmeg_va;
	int            pmeg_wired;
	int            pmeg_reserved;
	int            pmeg_vpages;
	int            pmeg_qstate;
};

typedef struct pmeg_state *pmeg_t;

#define PMEG_INVAL (NPMEG-1)
#define PMEG_NULL (pmeg_t) NULL

/* XXX - Replace pmeg_kernel_queue with pmeg_wired_queue ? */
TAILQ_HEAD(pmeg_tailq, pmeg_state)
	pmeg_free_queue, pmeg_inactive_queue,
	pmeg_active_queue, pmeg_kernel_queue;

static struct pmeg_state pmeg_array[NPMEG];


/*
 * prototypes
 */
static int get_pte_pmeg __P((int, int));
static void set_pte_pmeg __P((int, int, int));

static void context_allocate __P((pmap_t pmap));
static void context_free __P((pmap_t pmap));
static void context_init __P((void));

static void pmeg_init __P((void));
static void pmeg_reserve __P((int pmeg_num));

static pmeg_t pmeg_allocate __P((pmap_t pmap, vm_offset_t va));
static void pmeg_mon_init __P((vm_offset_t sva, vm_offset_t eva, int keep));
static void pmeg_release __P((pmeg_t pmegp));
static void pmeg_free __P((pmeg_t pmegp));
static pmeg_t pmeg_cache __P((pmap_t pmap, vm_offset_t va));
static void pmeg_set_wiring __P((pmeg_t pmegp, vm_offset_t va, int));

static int  pv_link   __P((pmap_t pmap, int pte, vm_offset_t va));
static void pv_unlink __P((pmap_t pmap, int pte, vm_offset_t va));
static void pv_remove_all __P((vm_offset_t pa));
static void pv_changepte __P((vm_offset_t pa, int, int));
static u_int pv_syncflags __P((pv_entry_t));
static void pv_init __P((void));

static void pmeg_clean __P((pmeg_t pmegp));
static void pmeg_clean_free __P((void));

static void pmap_common_init __P((pmap_t pmap));
static void pmap_kernel_init __P((pmap_t pmap));
static void pmap_user_init __P((pmap_t pmap));
static void pmap_page_upload __P((void));

static void pmap_enter_kernel __P((vm_offset_t va,
	int new_pte, boolean_t wired));
static void pmap_enter_user __P((pmap_t pmap, vm_offset_t va,
	int new_pte, boolean_t wired));

static void pmap_protect1 __P((pmap_t, vm_offset_t, vm_offset_t));
static void pmap_protect_mmu __P((pmap_t, vm_offset_t, vm_offset_t));
static void pmap_protect_noctx __P((pmap_t, vm_offset_t, vm_offset_t));

static void pmap_remove1 __P((pmap_t pmap, vm_offset_t, vm_offset_t));
static void pmap_remove_mmu __P((pmap_t, vm_offset_t, vm_offset_t));
static void pmap_remove_noctx __P((pmap_t, vm_offset_t, vm_offset_t));

static int  pmap_fault_reload __P((struct pmap *, vm_offset_t, int));

/* Called only from locore.s and pmap.c */
void	_pmap_switch __P((pmap_t pmap));

#ifdef	PMAP_DEBUG
void pmap_print __P((pmap_t pmap));
void pv_print __P((vm_offset_t pa));
void pmeg_print __P((pmeg_t pmegp));
static void pmeg_verify_empty __P((vm_offset_t va));
#endif	/* PMAP_DEBUG */
void pmap_pinit __P((pmap_t));
void pmap_release __P((pmap_t));

/*
 * Various in-line helper functions.
 */

static inline pmap_t
current_pmap __P((void))
{
	struct proc *p;
	struct vmspace *vm;
	vm_map_t	map;
	pmap_t	pmap;

	p = curproc;	/* XXX */
	if (p == NULL)
		pmap = kernel_pmap;
	else {
		vm = p->p_vmspace;
		map = &vm->vm_map;
		pmap = vm_map_pmap(map);
	}

	return (pmap);
}

static inline struct pv_entry **
pa_to_pvhead(vm_offset_t pa)
{
	int idx;

	idx = PA_PGNUM(pa);
#ifdef	DIAGNOSTIC
	if (PA_IS_DEV(pa) || (idx >= physmem))
		panic("pmap:pa_to_pvhead: bad pa=0x%lx", pa);
#endif
	return (&pv_head_tbl[idx]);
}

static inline u_char *
pa_to_pvflags(vm_offset_t pa)
{
	int idx;

	idx = PA_PGNUM(pa);
#ifdef	DIAGNOSTIC
	if (PA_IS_DEV(pa) || (idx >= physmem))
		panic("pmap:pa_to_pvflags: bad pa=0x%lx", pa);
#endif
	return (&pv_flags_tbl[idx]);
}

static inline pmeg_t
pmeg_p(int sme)
{
#ifdef	DIAGNOSTIC
	if (sme < 0 || sme >= SEGINV)
		panic("pmeg_p: bad sme");
#endif
	return &pmeg_array[sme];
}

#define is_pmeg_wired(pmegp) (pmegp->pmeg_wired != 0)

static void
pmeg_set_wiring(pmegp, va, flag)
	pmeg_t pmegp;
	vm_offset_t va;
	int flag;
{
	int idx, mask;

	idx = VA_PTE_NUM(va);
	mask = 1 << idx;

	if (flag)
		pmegp->pmeg_wired |= mask;
	else
		pmegp->pmeg_wired &= ~mask;
}

/*
 * Save the MOD bit from the given PTE using its PA
 */
static void
save_modref_bits(int pte)
{
	u_char *pv_flags;

	if (pv_initialized == 0)
		return;

	/* Only main memory is ever in the pv_lists */
	if (!IS_MAIN_MEM(pte))
		return;

	CHECK_SPL();

	pv_flags = pa_to_pvflags(PG_PA(pte));
	*pv_flags |= ((pte & PG_MODREF) >> PV_SHIFT);
}


/****************************************************************
 * Context management functions.
 */

/* part of pmap_bootstrap */
static void
context_init()
{
	int i;

	TAILQ_INIT(&context_free_queue);
	TAILQ_INIT(&context_active_queue);

	/* Leave EMPTY_CONTEXT out of the free list. */
	context_array[0].context_upmap = kernel_pmap;

	for (i = 1; i < NCONTEXT; i++) {
		context_array[i].context_num = i;
		context_array[i].context_upmap = NULL;
		TAILQ_INSERT_TAIL(&context_free_queue, &context_array[i],
						  context_link);
#ifdef	PMAP_DEBUG
		if (pmap_debug & PMD_CONTEXT)
			printf("context_init: sizeof(context_array[0])=%d\n",
				   sizeof(context_array[0]));
#endif
	}
}

/* Get us a context (steal one if necessary). */
static void
context_allocate(pmap)
	pmap_t pmap;
{
	context_t context;

	CHECK_SPL();

#ifdef	DIAGNOSTIC
	if (pmap == kernel_pmap)
		panic("context_allocate: kernel_pmap");
	if (has_context(pmap))
		panic("pmap: pmap already has context allocated to it");
#endif

	context = TAILQ_FIRST(&context_free_queue);
	if (context == NULL) {
		/* Steal the head of the active queue. */
		context = TAILQ_FIRST(&context_active_queue);
		if (context == NULL)
			panic("pmap: no contexts left?");
#ifdef	PMAP_DEBUG
		if (pmap_debug & PMD_CONTEXT)
			printf("context_allocate: steal ctx %d from pmap %p\n",
				   context->context_num, context->context_upmap);
#endif
		context_free(context->context_upmap);
		context = TAILQ_FIRST(&context_free_queue);
	}
	TAILQ_REMOVE(&context_free_queue, context, context_link);

	if (context->context_upmap != NULL)
		panic("pmap: context in use???");

	context->context_upmap = pmap;
	pmap->pm_ctxnum = context->context_num;

	TAILQ_INSERT_TAIL(&context_active_queue, context, context_link);

	/*
	 * We could reload the MMU here, but that would
	 * artificially move PMEGs from the inactive queue
	 * to the active queue, so do lazy reloading.
	 * XXX - Need to reload wired pmegs though...
	 * XXX: Verify the context it is empty?
	 */
}

/*
 * Unload the context and put it on the free queue.
 */
static void
context_free(pmap)		/* :) */
	pmap_t pmap;
{
	int saved_ctxnum, ctxnum;
	int i, sme;
	context_t contextp;
	vm_offset_t va;

	CHECK_SPL();

	ctxnum = pmap->pm_ctxnum;
	if (ctxnum < FIRST_CONTEXT || ctxnum >= NCONTEXT)
		panic("pmap: context_free ctxnum");
	contextp = &context_array[ctxnum];

	/* Temporary context change. */
	saved_ctxnum = get_context();
	set_context(ctxnum);

	/* Before unloading translations, flush cache. */
#ifdef	HAVECACHE
	if (cache_size)
		cache_flush_context();
#endif

	/* Unload MMU (but keep in SW segmap). */
	for (i=0, va=0; i < NUSEG; i++, va+=NBSG) {

#if !defined(PMAP_DEBUG)
		/* Short-cut using the S/W segmap (if !debug). */
		if (pmap->pm_segmap[i] == SEGINV)
			continue;
#endif

		/* Check the H/W segmap. */
		sme = get_segmap(va);
		if (sme == SEGINV)
			continue;

		/* Found valid PMEG in the segmap. */
#ifdef	PMAP_DEBUG
		if (pmap_debug & PMD_SEGMAP)
			printf("pmap: set_segmap ctx=%d v=0x%lx old=0x%x new=ff (cf)\n",
				   ctxnum, va, sme);
#endif
#ifdef	DIAGNOSTIC
		if (sme != pmap->pm_segmap[i])
			panic("context_free: unknown sme at va=0x%lx", va);
#endif
		/* Did cache flush above (whole context). */
		set_segmap(va, SEGINV);
		/* In this case, do not clear pm_segmap. */
		/* XXX: Maybe inline this call? */
		pmeg_release(pmeg_p(sme));
	}

	/* Restore previous context. */
	set_context(saved_ctxnum);

	/* Dequeue, update, requeue. */
	TAILQ_REMOVE(&context_active_queue, contextp, context_link);
	pmap->pm_ctxnum = EMPTY_CONTEXT;
	contextp->context_upmap = NULL;
	TAILQ_INSERT_TAIL(&context_free_queue, contextp, context_link);
}


/****************************************************************
 * PMEG management functions.
 */

static void
pmeg_init()
{
	int x;

	/* clear pmeg array, put it all on the free pmeq queue */

	TAILQ_INIT(&pmeg_free_queue);
	TAILQ_INIT(&pmeg_inactive_queue);
	TAILQ_INIT(&pmeg_active_queue);
	TAILQ_INIT(&pmeg_kernel_queue);

	bzero(pmeg_array, NPMEG*sizeof(struct pmeg_state));
	for (x =0 ; x<NPMEG; x++) {
		TAILQ_INSERT_TAIL(&pmeg_free_queue, &pmeg_array[x],
				  pmeg_link);
		pmeg_array[x].pmeg_qstate = PMEGQ_FREE;
		pmeg_array[x].pmeg_index = x;
	}

	/* The last pmeg is not usable. */
	pmeg_reserve(SEGINV);
}

/*
 * Reserve a pmeg (forever) for use by PROM, etc.
 * Contents are left as-is.  Called very early...
 */
void
pmeg_reserve(sme)
	int sme;
{
	pmeg_t pmegp;

	/* Can not use pmeg_p() because it fails on SEGINV. */
	pmegp = &pmeg_array[sme];

	if (pmegp->pmeg_reserved) {
		prom_printf("pmeg_reserve: already reserved\n");
		prom_abort();
	}
	if (pmegp->pmeg_owner) {
		prom_printf("pmeg_reserve: already owned\n");
		prom_abort();
	}

	/* Owned by kernel, but not really usable... */
	pmegp->pmeg_owner = kernel_pmap;
	pmegp->pmeg_reserved++;	/* keep count, just in case */
	TAILQ_REMOVE(&pmeg_free_queue, pmegp, pmeg_link);
	pmegp->pmeg_qstate = PMEGQ_NONE;
}

/*
 * Examine PMEGs used by the monitor, and either
 * reserve them (keep=1) or clear them (keep=0)
 */
static void
pmeg_mon_init(sva, eva, keep)
	vm_offset_t sva, eva;
	int keep;	/* true: steal, false: clear */
{
	vm_offset_t pgva, endseg;
	int pte, valid;
	unsigned char sme;

#ifdef	PMAP_DEBUG
	if (pmap_debug & PMD_SEGMAP)
		prom_printf("pmeg_mon_init(0x%x, 0x%x, %d)\n",
		           sva, eva, keep);
#endif

	sva &= ~(NBSG-1);

	while (sva < eva) {
		sme = get_segmap(sva);
		if (sme != SEGINV) {
			valid = 0;
			endseg = sva + NBSG;
			for (pgva = sva; pgva < endseg; pgva += NBPG) {
				pte = get_pte(pgva);
				if (pte & PG_VALID) {
					valid++;
				}
			}
#ifdef	PMAP_DEBUG
			if (pmap_debug & PMD_SEGMAP)
				prom_printf(" sva=0x%x seg=0x%x valid=%d\n",
				           sva, sme, valid);
#endif
			if (keep && valid)
				pmeg_reserve(sme);
			else set_segmap(sva, SEGINV);
		}
		sva += NBSG;
	}
}

/*
 * This is used only during pmap_bootstrap, so we can
 * get away with borrowing a slot in the segmap.
 */
static void
pmeg_clean(pmegp)
	pmeg_t pmegp;
{
	int sme;
	vm_offset_t va;

	sme = get_segmap(0);
	if (sme != SEGINV)
		panic("pmeg_clean");

	sme = pmegp->pmeg_index;
	set_segmap(0, sme);

	for (va = 0; va < NBSG; va += NBPG)
		set_pte(va, PG_INVAL);

	set_segmap(0, SEGINV);
}

/*
 * This routine makes sure that pmegs on the pmeg_free_queue contain
 * no valid ptes.  It pulls things off the queue, cleans them, and
 * puts them at the end.  The ending condition is finding the first
 * queue element at the head of the queue again.
 */
static void
pmeg_clean_free()
{
	pmeg_t pmegp, pmegp_first;

	pmegp = TAILQ_FIRST(&pmeg_free_queue);
	if (pmegp == NULL)
		panic("pmap: no free pmegs available to clean");

	pmegp_first = NULL;

	for (;;) {
		pmegp = TAILQ_FIRST(&pmeg_free_queue);
		TAILQ_REMOVE(&pmeg_free_queue, pmegp, pmeg_link);

		pmegp->pmeg_qstate = PMEGQ_NONE;
		pmeg_clean(pmegp);
		pmegp->pmeg_qstate = PMEGQ_FREE;

		TAILQ_INSERT_TAIL(&pmeg_free_queue, pmegp, pmeg_link);

		if (pmegp == pmegp_first)
			break;
		if (pmegp_first == NULL)
			pmegp_first = pmegp;
	}
}

/*
 * Allocate a PMEG by whatever means necessary.
 * (May invalidate some mappings!)
 */
static pmeg_t
pmeg_allocate(pmap, va)
	pmap_t pmap;
	vm_offset_t va;
{
	pmeg_t pmegp;

	CHECK_SPL();

#ifdef	DIAGNOSTIC
	if (va & SEGOFSET) {
		panic("pmap:pmeg_allocate: va=0x%lx", va);
	}
#endif

	/* Get one onto the free list if necessary. */
	pmegp = TAILQ_FIRST(&pmeg_free_queue);
	if (!pmegp) {
		/* Try inactive queue... */
		pmegp = TAILQ_FIRST(&pmeg_inactive_queue);
		if (!pmegp) {
			/* Try active queue... */
			pmegp = TAILQ_FIRST(&pmeg_active_queue);
		}
		if (!pmegp) {
			panic("pmeg_allocate: failed");
		}
		/*
		 * Remove mappings to free-up a pmeg
		 * (so it will go onto the free list).
		 * XXX - Should this call up into the VM layer
		 * to notify it when pages are deactivated?
		 * See: vm_page.c:vm_page_deactivate(vm_page_t)
		 * XXX - Skip this one if it is wired?
		 */
		pmap_remove1(pmegp->pmeg_owner,
		             pmegp->pmeg_va,
		             pmegp->pmeg_va + NBSG);
	}

	/* OK, free list has something for us to take. */
	pmegp = TAILQ_FIRST(&pmeg_free_queue);
#ifdef	DIAGNOSTIC
	if (pmegp == NULL)
		panic("pmeg_allocagte: still none free?");
	if ((pmegp->pmeg_qstate != PMEGQ_FREE) ||
		(pmegp->pmeg_index == SEGINV) ||
		(pmegp->pmeg_vpages))
		panic("pmeg_allocate: bad pmegp=%p", pmegp);
#endif
#ifdef	PMAP_DEBUG
	if (pmegp->pmeg_index == pmap_db_watchpmeg) {
		db_printf("pmeg_allocate: watch pmegp=%p\n", pmegp);
		Debugger();
	}
#endif

	TAILQ_REMOVE(&pmeg_free_queue, pmegp, pmeg_link);

	/* Reassign this PMEG for the caller. */
	pmegp->pmeg_owner = pmap;
	pmegp->pmeg_version = pmap->pm_version;
	pmegp->pmeg_va = va;
	pmegp->pmeg_wired = 0;
	pmegp->pmeg_reserved  = 0;
	pmegp->pmeg_vpages  = 0;
	if (pmap == kernel_pmap) {
		TAILQ_INSERT_TAIL(&pmeg_kernel_queue, pmegp, pmeg_link);
		pmegp->pmeg_qstate = PMEGQ_KERNEL;
	} else {
		TAILQ_INSERT_TAIL(&pmeg_active_queue, pmegp, pmeg_link);
		pmegp->pmeg_qstate = PMEGQ_ACTIVE;
	}
	/* Caller will verify that it's empty (if debugging). */
	return pmegp;
}

/*
 * Put pmeg on the inactive queue, leaving its contents intact.
 * This happens when we loose our context.  We may reclaim
 * this pmeg later if it is still in the inactive queue.
 */
static void
pmeg_release(pmegp)
	pmeg_t pmegp;
{

	CHECK_SPL();

#ifdef	DIAGNOSTIC
	if ((pmegp->pmeg_owner == kernel_pmap) ||
		(pmegp->pmeg_qstate != PMEGQ_ACTIVE))
		panic("pmeg_release: bad pmeg=%p", pmegp);
#endif

	TAILQ_REMOVE(&pmeg_active_queue, pmegp, pmeg_link);
	pmegp->pmeg_qstate = PMEGQ_INACTIVE;
	TAILQ_INSERT_TAIL(&pmeg_inactive_queue, pmegp, pmeg_link);
}

/*
 * Move the pmeg to the free queue from wherever it is.
 * The pmeg will be clean.  It might be in kernel_pmap.
 */
static void
pmeg_free(pmegp)
	pmeg_t pmegp;
{

	CHECK_SPL();

#ifdef	DIAGNOSTIC
	/* Caller should verify that it's empty. */
	if (pmegp->pmeg_vpages != 0)
		panic("pmeg_free: vpages");
#endif

	switch (pmegp->pmeg_qstate) {
	case PMEGQ_ACTIVE:
		TAILQ_REMOVE(&pmeg_active_queue, pmegp, pmeg_link);
		break;
	case PMEGQ_INACTIVE:
		TAILQ_REMOVE(&pmeg_inactive_queue, pmegp, pmeg_link);
		break;
	case PMEGQ_KERNEL:
		TAILQ_REMOVE(&pmeg_kernel_queue, pmegp, pmeg_link);
		break;
	default:
		panic("pmeg_free: releasing bad pmeg");
		break;
	}

#ifdef	PMAP_DEBUG
	if (pmegp->pmeg_index == pmap_db_watchpmeg) {
		db_printf("pmeg_free: watch pmeg 0x%x\n",
			   pmegp->pmeg_index);
		Debugger();
	}
#endif

	pmegp->pmeg_owner = NULL;
	pmegp->pmeg_qstate = PMEGQ_FREE;
	TAILQ_INSERT_TAIL(&pmeg_free_queue, pmegp, pmeg_link);
}

/*
 * Find a PMEG that was put on the inactive queue when we
 * had our context stolen.  If found, move to active queue.
 */
static pmeg_t
pmeg_cache(pmap, va)
	pmap_t pmap;
	vm_offset_t va;
{
	int sme, segnum;
	pmeg_t pmegp;

	CHECK_SPL();

#ifdef	DIAGNOSTIC
	if (pmap == kernel_pmap)
		panic("pmeg_cache: kernel_pmap");
	if (va & SEGOFSET) {
		panic("pmap:pmeg_cache: va=0x%lx", va);
	}
#endif

	if (pmap->pm_segmap == NULL)
		return PMEG_NULL;

	segnum = VA_SEGNUM(va);
	if (segnum > NUSEG)		/* out of range */
		return PMEG_NULL;

	sme = pmap->pm_segmap[segnum];
	if (sme == SEGINV)	/* nothing cached */
		return PMEG_NULL;

	pmegp = pmeg_p(sme);

#ifdef	PMAP_DEBUG
	if (pmegp->pmeg_index == pmap_db_watchpmeg) {
		db_printf("pmeg_cache: watch pmeg 0x%x\n", pmegp->pmeg_index);
		Debugger();
	}
#endif

	/*
	 * Our segmap named a PMEG.  If it is no longer ours,
	 * invalidate that entry in our segmap and return NULL.
	 */
	if ((pmegp->pmeg_owner != pmap) ||
		(pmegp->pmeg_version != pmap->pm_version) ||
		(pmegp->pmeg_va != va))
	{
#ifdef	PMAP_DEBUG
		db_printf("pmap:pmeg_cache: invalid pmeg: sme=0x%x\n", sme);
		pmeg_print(pmegp);
		Debugger();
#endif
		pmap->pm_segmap[segnum] = SEGINV;
		return PMEG_NULL; /* cache lookup failed */
	}

#ifdef	DIAGNOSTIC
	/* Make sure it is on the inactive queue. */
	if (pmegp->pmeg_qstate != PMEGQ_INACTIVE)
		panic("pmeg_cache: pmeg was taken: %p", pmegp);
#endif

	TAILQ_REMOVE(&pmeg_inactive_queue, pmegp, pmeg_link);
	pmegp->pmeg_qstate = PMEGQ_ACTIVE;
	TAILQ_INSERT_TAIL(&pmeg_active_queue, pmegp, pmeg_link);

	return pmegp;
}

#ifdef	PMAP_DEBUG
static void
pmeg_verify_empty(va)
	vm_offset_t va;
{
	vm_offset_t eva;
	int pte;

	for (eva = va + NBSG;  va < eva; va += NBPG) {
		pte = get_pte(va);
		if (pte & PG_VALID)
			panic("pmeg_verify_empty");
	}
}
#endif	/* PMAP_DEBUG */


/****************************************************************
 * Physical-to-virutal lookup support
 *
 * Need memory for the pv_alloc/pv_free list heads
 * and elements.  We know how many to allocate since
 * there is one list head for each physical page, and
 * at most one element for each PMEG slot.
 */
static void
pv_init()
{
	int npp, nvp, sz;
	pv_entry_t pv;
	char *p;

	/* total allocation size */
	sz = 0;

	/*
	 * Data for each physical page.
	 * Each "mod/ref" flag is a char.
	 * Each PV head is a pointer.
	 * Note physmem is in pages.
	 */
	npp = ALIGN(physmem);
	sz += (npp * sizeof(*pv_flags_tbl));
	sz += (npp * sizeof(*pv_head_tbl));

	/*
	 * Data for each virtual page (all PMEGs).
	 * One pv_entry for each page frame.
	 */
	nvp = NPMEG * NPAGSEG;
	sz += (nvp * sizeof(*pv_free_list));

	/* Now allocate the whole thing. */
	sz = m68k_round_page(sz);
	p = (char*) uvm_km_alloc(kernel_map, sz);
	if (p == NULL)
		panic("pmap:pv_init: alloc failed");
	bzero(p, sz);

	/* Now divide up the space. */
	pv_flags_tbl = (void *) p;
	p += (npp * sizeof(*pv_flags_tbl));
	pv_head_tbl = (void*) p;
	p += (npp * sizeof(*pv_head_tbl));
	pv_free_list = (void *) p;
	p += (nvp * sizeof(*pv_free_list));

	/* Finally, make pv_free_list into a list. */
	for (pv = pv_free_list; (char*)pv < p; pv++)
		pv->pv_next = &pv[1];
	pv[-1].pv_next = 0;

	pv_initialized++;
}

/*
 * Set or clear bits in all PTEs mapping a page.
 * Also does syncflags work while we are there...
 */
static void
pv_changepte(pa, set_bits, clear_bits)
	vm_offset_t pa;
	int set_bits;
	int clear_bits;
{
	pv_entry_t *head, pv;
	u_char *pv_flags;
	pmap_t pmap;
	vm_offset_t va;
	int pte, sme;
	int saved_ctx;
	boolean_t in_ctx;
	u_int flags;

	CHECK_SPL();

	if (!pv_initialized)
		return;
	if ((set_bits == 0) && (clear_bits == 0))
		return;

	pv_flags = pa_to_pvflags(pa);
	head     = pa_to_pvhead(pa);

	/* If no mappings, no work to do. */
	if (*head == NULL)
		return;

#ifdef	DIAGNOSTIC
	/* This function should only clear these bits: */
	if (clear_bits & ~(PG_WRITE | PG_NC | PG_REF | PG_MOD))
		panic("pv_changepte: clear=0x%x\n", clear_bits);
#endif

	flags = 0;
	saved_ctx = get_context();
	for (pv = *head; pv != NULL; pv = pv->pv_next) {
		pmap = pv->pv_pmap;
		va = pv->pv_va;

#ifdef	DIAGNOSTIC
		if (pmap == NULL)
			panic("pv_changepte: null pmap");
		if (pmap->pm_segmap == NULL)
			panic("pv_changepte: null segmap");
#endif

		/* XXX don't write protect pager mappings */
		if (clear_bits & PG_WRITE) {
			if (va >= PAGER_SVA && va < PAGER_EVA) {
#ifdef	PMAP_DEBUG
				/* XXX - Does this actually happen? */
				printf("pv_changepte: in pager!\n");
				Debugger();
#endif
				continue;
			}
		}

		/* Is the PTE currently accessable in some context? */
		in_ctx = FALSE;
		sme = SEGINV;	/* kill warning */
		if (pmap == kernel_pmap) {
			set_context(KERNEL_CONTEXT);
			in_ctx = TRUE;
		}
		else if (has_context(pmap)) {
			/* PMEG may be inactive. */
			set_context(pmap->pm_ctxnum);
			sme = get_segmap(va);
			if (sme != SEGINV)
				in_ctx = TRUE;
		}

		if (in_ctx == TRUE) {
			/*
			 * The PTE is in the current context.
			 * Make sure PTE is up-to-date with VAC.
			 */
#ifdef	HAVECACHE
			if (cache_size)
				cache_flush_page(va);
#endif
			pte = get_pte(va);
		} else {
			/*
			 * The PTE is not in any context.
			 */
			sme = pmap->pm_segmap[VA_SEGNUM(va)];
			if (sme == SEGINV)
				panic("pv_changepte: SEGINV");
			pte = get_pte_pmeg(sme, VA_PTE_NUM(va));
		}

#ifdef	DIAGNOSTIC
		/* PV entries point only to valid mappings. */
		if ((pte & PG_VALID) == 0)
			panic("pv_changepte: not PG_VALID at va=0x%lx\n", va);
#endif
		/* Get these while it's easy. */
		if (pte & PG_MODREF) {
			flags |= (pte & PG_MODREF);
			pte &= ~PG_MODREF;
		}

		/* Finally, set and clear some bits. */
		pte |= set_bits;
		pte &= ~clear_bits;

		if (in_ctx == TRUE) {
			/* Did cache flush above. */
			set_pte(va, pte);
		} else {
			set_pte_pmeg(sme, VA_PTE_NUM(va), pte);
		}
	}
	set_context(saved_ctx);

	*pv_flags |= (flags >> PV_SHIFT);
}

/*
 * Return ref and mod bits from pvlist,
 * and turns off same in hardware PTEs.
 */
static u_int
pv_syncflags(pv)
	pv_entry_t pv;
{
	pmap_t pmap;
	vm_offset_t va;
	int pte, sme;
	int saved_ctx;
	boolean_t in_ctx;
	u_int flags;

	CHECK_SPL();

	if (!pv_initialized)
		return(0);

	/* If no mappings, no work to do. */
	if (pv == NULL)
		return (0);

	flags = 0;
	saved_ctx = get_context();
	for ( ; pv != NULL; pv = pv->pv_next) {
		pmap = pv->pv_pmap;
		va = pv->pv_va;
		sme = SEGINV;	/* kill warning */

#ifdef	DIAGNOSTIC
		/*
		 * Only the head may have a null pmap, and
		 * we checked for that above.
		 */
		if (pmap == NULL)
			panic("pv_syncflags: null pmap");
		if (pmap->pm_segmap == NULL)
			panic("pv_syncflags: null segmap");
#endif

		/* Is the PTE currently accessable in some context? */
		in_ctx = FALSE;
		if (pmap == kernel_pmap) {
			set_context(KERNEL_CONTEXT);
			in_ctx = TRUE;
		}
		else if (has_context(pmap)) {
			/* PMEG may be inactive. */
			set_context(pmap->pm_ctxnum);
			sme = get_segmap(va);
			if (sme != SEGINV)
				in_ctx = TRUE;
		}

		if (in_ctx == TRUE) {
			/*
			 * The PTE is in the current context.
			 * Make sure PTE is up-to-date with VAC.
			 */
#ifdef	HAVECACHE
			if (cache_size)
				cache_flush_page(va);
#endif
			pte = get_pte(va);
		} else {
			/*
			 * The PTE is not in any context.
			 * XXX - Consider syncing MODREF bits
			 * when the PMEG looses its context?
			 */
			sme = pmap->pm_segmap[VA_SEGNUM(va)];
			if (sme == SEGINV)
				panic("pv_syncflags: SEGINV");
			pte = get_pte_pmeg(sme, VA_PTE_NUM(va));
		}

#ifdef	DIAGNOSTIC
		/* PV entries point only to valid mappings. */
		if ((pte & PG_VALID) == 0)
			panic("pv_syncflags: not PG_VALID at va=0x%lx\n", va);
#endif
		/* OK, do what we came here for... */
		if (pte & PG_MODREF) {
			flags |= (pte & PG_MODREF);
			pte &= ~PG_MODREF;
		}

		if (in_ctx == TRUE) {
			/* Did cache flush above. */
			set_pte(va, pte);
		} else {
			set_pte_pmeg(sme, VA_PTE_NUM(va), pte);
		}
	}
	set_context(saved_ctx);

	return (flags >> PV_SHIFT);
}

/* Remove all mappings for the physical page. */
static void
pv_remove_all(pa)
	vm_offset_t pa;
{
	pv_entry_t *head, pv;
	pmap_t pmap;
	vm_offset_t va;

	CHECK_SPL();

#ifdef PMAP_DEBUG
	if (pmap_debug & PMD_REMOVE)
		printf("pv_remove_all(0x%lx)\n", pa);
#endif

	if (!pv_initialized)
		return;

	head = pa_to_pvhead(pa);
	while ((pv = *head) != NULL) {
		pmap = pv->pv_pmap;
		va   = pv->pv_va;
		pmap_remove1(pmap, va, va + NBPG);
#ifdef PMAP_DEBUG
		/* Make sure it went away. */
		if (pv == *head) {
			db_printf("pv_remove_all: head unchanged for pa=0x%lx\n", pa);
			Debugger();
		}
#endif
	}
}

/*
 * The pmap system is asked to lookup all mappings that point to a
 * given physical memory address.  This function adds a new element
 * to the list of mappings maintained for the given physical address.
 * Returns PV_NC if the (new) pvlist says that the address cannot
 * be cached.
 */
static int
pv_link(pmap, pte, va)
	pmap_t pmap;
	int pte;
	vm_offset_t va;
{
	vm_offset_t pa;
	pv_entry_t *head, pv;
	u_char *pv_flags;
	int flags;

	if (!pv_initialized)
		return 0;

	CHECK_SPL();

	/* Only the non-cached bit is of interest here. */
	flags = (pte & PG_NC) ? PV_NC : 0;
	pa = PG_PA(pte);

#ifdef PMAP_DEBUG
	if ((pmap_debug & PMD_LINK) || (va == pmap_db_watchva)) {
		printf("pv_link(%p, 0x%x, 0x%lx)\n", pmap, pte, va);
		/* pv_print(pa); */
	}
#endif

	pv_flags = pa_to_pvflags(pa);
	head     = pa_to_pvhead(pa);

#ifdef	DIAGNOSTIC
	/* See if this mapping is already in the list. */
	for (pv = *head; pv != NULL; pv = pv->pv_next) {
		if ((pv->pv_pmap == pmap) && (pv->pv_va == va))
			panic("pv_link: duplicate entry for PA=0x%lx", pa);
	}
#endif

	/*
	 * Does this new mapping cause VAC alias problems?
	 */
	*pv_flags |= flags;
	if ((*pv_flags & PV_NC) == 0) {
		for (pv = *head; pv != NULL; pv = pv->pv_next) {
			if (BADALIAS(va, pv->pv_va)) {
				*pv_flags |= PV_NC;
				pv_changepte(pa, PG_NC, 0);
				pmap_stats.ps_vac_uncached++;
				break;
			}
		}
	}

	/* Allocate a PV element (pv_alloc()). */
	pv = pv_free_list;
	if (pv == NULL)
		panic("pv_link: pv_alloc");
	pv_free_list = pv->pv_next;
	pv->pv_next = 0;

	/* Insert new entry at the head. */
	pv->pv_pmap = pmap;
	pv->pv_va   = va;
	pv->pv_next = *head;
	*head = pv;

	return (*pv_flags & PV_NC);
}

/*
 * pv_unlink is a helper function for pmap_remove.
 * It removes the appropriate (pmap, pa, va) entry.
 *
 * Once the entry is removed, if the pv_table head has the cache
 * inhibit bit set, see if we can turn that off; if so, walk the
 * pvlist and turn off PG_NC in each PTE.  (The pvlist is by
 * definition nonempty, since it must have at least two elements
 * in it to have PV_NC set, and we only remove one here.)
 */
static void
pv_unlink(pmap, pte, va)
	pmap_t pmap;
	int pte;
	vm_offset_t va;
{
	vm_offset_t pa;
	pv_entry_t *head, *ppv, pv;
	u_char *pv_flags;

	if (!pv_initialized)
		return;

	CHECK_SPL();

	pa = PG_PA(pte);
#ifdef PMAP_DEBUG
	if ((pmap_debug & PMD_LINK) || (va == pmap_db_watchva)) {
		printf("pv_unlink(%p, 0x%x, 0x%lx)\n", pmap, pte, va);
		/* pv_print(pa); */
	}
#endif

	pv_flags = pa_to_pvflags(pa);
	head     = pa_to_pvhead(pa);

	/*
	 * Find the entry.
	 */
	ppv = head;
	pv = *ppv;
	while (pv) {
		if ((pv->pv_pmap == pmap) && (pv->pv_va == va))
			goto found;
		ppv = &pv->pv_next;
		pv  =  pv->pv_next;
	}
#ifdef PMAP_DEBUG
	db_printf("pv_unlink: not found (pa=0x%lx,va=0x%lx)\n", pa, va);
	Debugger();
#endif
	return;

found:
	/* Unlink this entry from the list and clear it. */
	*ppv = pv->pv_next;
	pv->pv_pmap = NULL;
	pv->pv_va   = 0;

	/* Insert it on the head of the free list. (pv_free()) */
	pv->pv_next = pv_free_list;
	pv_free_list = pv;
	pv = NULL;

	/* Do any non-cached mappings remain? */
	if ((*pv_flags & PV_NC) == 0)
		return;
	if ((pv = *head) == NULL)
		return;

	/*
	 * Have non-cached mappings.  See if we can fix that now.
	 */
	va = pv->pv_va;
	for (pv = pv->pv_next; pv != NULL; pv = pv->pv_next) {
		/* If there is a DVMA mapping, leave it NC. */
		if (va >= DVMA_MAP_BASE)
			return;
		/* If there are VAC alias problems, leave NC. */
		if (BADALIAS(va, pv->pv_va))
			return;
	}
	/* OK, there are no "problem" mappings. */
	*pv_flags &= ~PV_NC;
	pv_changepte(pa, 0, PG_NC);
	pmap_stats.ps_vac_recached++;
}


/****************************************************************
 * Bootstrap and Initialization, etc.
 */

void
pmap_common_init(pmap)
	pmap_t pmap;
{
	bzero(pmap, sizeof(struct pmap));
	pmap->pm_refcount=1;
	pmap->pm_version = pmap_version++;
	pmap->pm_ctxnum = EMPTY_CONTEXT;
	simple_lock_init(&pmap->pm_lock);
}

/*
 * Prepare the kernel for VM operations.
 * This is called by locore2.c:_vm_init()
 * after the "start/end" globals are set.
 * This function must NOT leave context zero.
 */
void
pmap_bootstrap(nextva)
	vm_offset_t nextva;
{
	vm_offset_t va, eva;
	int i, pte, sme;
	extern char etext[];

	nextva = m68k_round_page(nextva);

	/* Steal some special-purpose, already mapped pages? */

	/*
	 * Determine the range of kernel virtual space available.
	 * It is segment-aligned to simplify PMEG management.
	 */
	virtual_avail = m68k_round_seg(nextva);
	virtual_end = VM_MAX_KERNEL_ADDRESS;

	/*
	 * Determine the range of physical memory available.
	 * Physical memory at zero was remapped to KERNBASE.
	 */
	avail_start = nextva - KERNBASE;
	avail_end = prom_memsize();
	avail_end = m68k_trunc_page(avail_end);

	/*
	 * Report the actual amount of physical memory,
	 * even though the PROM takes a few pages.
	 */
	physmem = (btoc(avail_end) + 0xF) & ~0xF;

	/*
	 * Done allocating PAGES of virtual space, so
	 * clean out the rest of the last used segment.
	 */
	for (va = nextva; va < virtual_avail; va += NBPG)
		set_pte(va, PG_INVAL);

	/*
	 * Now that we are done stealing physical pages, etc.
	 * figure out which PMEGs are used by those mappings
	 * and either reserve them or clear them out.
	 * -- but first, init PMEG management.
	 * This puts all PMEGs in the free list.
	 * We will allocte the in-use ones.
	 */
	pmeg_init();

	/*
	 * Unmap low virtual segments.
	 * VA range: [0 .. KERNBASE]
	 */
	for (va = 0; va < KERNBASE; va += NBSG)
		set_segmap(va, SEGINV);

	/*
	 * Reserve PMEGS for kernel text/data/bss
	 * and the misc pages taken above.
	 * VA range: [KERNBASE .. virtual_avail]
	 */
	for ( ; va < virtual_avail; va += NBSG) {
		sme = get_segmap(va);
		if (sme == SEGINV) {
			prom_printf("kernel text/data/bss not mapped\n");
			prom_abort();
		}
		pmeg_reserve(sme);
	}

	/*
	 * Unmap kernel virtual space.  Make sure to leave no valid
	 * segmap entries in the MMU unless pmeg_array records them.
	 * VA range: [vseg_avail .. virtual_end]
	 */
	for ( ; va < virtual_end; va += NBSG)
		set_segmap(va, SEGINV);

	/*
	 * Reserve PMEGs used by the PROM monitor (device mappings).
	 * Free up any pmegs in this range which have no mappings.
	 * VA range: [0x00E00000 .. 0x00F00000]
	 */
	pmeg_mon_init(SUN2_MONSTART, SUN2_MONEND, TRUE);

	/*
	 * Unmap any pmegs left in DVMA space by the PROM.
	 * DO NOT kill the last one! (owned by the PROM!)
	 * VA range: [0x00F00000 .. 0x00FE0000]
	 */
	pmeg_mon_init(SUN2_MONEND, SUN2_MONEND + DVMA_MAP_SIZE, FALSE);

	/*
	 * Done reserving PMEGs and/or clearing out mappings.
	 *
	 * Now verify the mapping protections and such for the
	 * important parts of the address space (in VA order).
	 * Note that the Sun PROM usually leaves the memory
	 * mapped with everything non-cached...
	 */

	/*
	 * On both a Sun2 and Sun3, the kernel text starts at virtual
	 * KERNTEXTOFF, physical 0x4000.  locore.s has set up a temporary
	 * stack starting at virtual (KERNTEXTOFF - sizeof(struct exec)),
	 * physical 0x3FE0, and growing down.
	 *
	 * On the Sun3, this means there are two physical pages before the
	 * kernel text.  On the Sun3, page zero is reserved for the msgbuf
	 * and page one contains the temporary stack.
	 *
	 * On the Sun2, there are eight physical pages before the kernel
	 * text.  Pages 0-3 are used by the PROM.
	 */
	va = KERNBASE;
	eva = KERNBASE + (NBPG * 4);
	for(va = KERNBASE; va < eva; va += NBPG) {
			pte = get_pte(va);
			pte |= (PG_SYSTEM | PG_WRITE);
			set_pte(va, pte);
	}

	/*
	 * We start the msgbuf at page four.
	 */
	pte = get_pte(va);
	pte |= (PG_SYSTEM | PG_WRITE | PG_NC);
	set_pte(va, pte);
	va += NBPG;
	/* Initialize msgbufaddr later, in machdep.c */

	/*
	 * On the Sun3, two of the three dead pages in SUN3_MONSHORTSEG
	 * are used for tmp_vpages.  The Sun2 doesn't have this
	 * short-segment concept, so we reserve virtual pages five and six
	 * after KERNBASE for this.
	 */
	set_pte(va, PG_INVAL);
	va += NBPG;
	set_pte(va, PG_INVAL);
	va += NBPG;

	/*
	 * Page seven remains for the temporary kernel stack.  Hopefully
	 * this is enough space.
	 */
	pte = get_pte(va);
	pte &= ~(PG_NC);
	pte |= (PG_SYSTEM | PG_WRITE);
	set_pte(va, pte);
	va += NBPG;

	/*
	 * Next is the kernel text.
	 *
	 * Verify protection bits on kernel text/data/bss
	 * All of kernel text, data, and bss are cached.
	 * Text is read-only (except in db_write_ktext).
	 */
	eva = m68k_trunc_page(etext);
	while (va < eva) {
		pte = get_pte(va);
		if ((pte & (PG_VALID|PG_TYPE)) != PG_VALID) {
			prom_printf("invalid page at 0x%x\n", va);
		}
		pte &= ~(PG_WRITE|PG_NC);
		/* Kernel text is read-only */
		pte |= (PG_SYSTEM);
		set_pte(va, pte);
		va += NBPG;
	}
	/* data, bss, etc. */
	while (va < nextva) {
		pte = get_pte(va);
		if ((pte & (PG_VALID|PG_TYPE)) != PG_VALID) {
			prom_printf("invalid page at 0x%x\n", va);
		}
		pte &= ~(PG_NC);
		pte |= (PG_SYSTEM | PG_WRITE);
		set_pte(va, pte);
		va += NBPG;
	}

	/*
	 * Initialize all of the other contexts.
	 */
#ifdef	DIAGNOSTIC
	/* Near the beginning of locore.s we set context zero. */
	if (get_context() != 0) {
		prom_printf("pmap_bootstrap: not in context zero?\n");
		prom_abort();
	}
#endif	/* DIAGNOSTIC */
	for (va = 0; va < (vm_offset_t) (NBSG * NSEGMAP); va += NBSG) {
		for (i = 1; i < NCONTEXT; i++) {
			set_context(i);
			set_segmap(va, SEGINV);
		}
	}
	set_context(KERNEL_CONTEXT);

	/*
	 * Reserve a segment for the kernel to use to access a pmeg
	 * that is not currently mapped into any context/segmap.
	 * The kernel temporarily maps such a pmeg into this segment.
	 */
	temp_seg_va = virtual_avail;
	virtual_avail += NBSG;
#ifdef	DIAGNOSTIC
	if (temp_seg_va & SEGOFSET) {
		prom_printf("pmap_bootstrap: temp_seg_va\n");
		prom_abort();
	}
#endif

	/* Initialization for pmap_next_page() */
	avail_next = avail_start;

	uvmexp.pagesize = NBPG;
	uvm_setpagesize();

	/* after setting up some structures */

	pmap_common_init(kernel_pmap);
	pmap_kernel_init(kernel_pmap);

	context_init();

	pmeg_clean_free();

	pmap_page_upload();
}

/*
 * Give the kernel pmap a segmap, just so there are not
 * so many special cases required.  Maybe faster too,
 * because this lets pmap_remove() and pmap_protect()
 * use a S/W copy of the segmap to avoid function calls.
 */
void
pmap_kernel_init(pmap)
	 pmap_t pmap;
{
	vm_offset_t va;
	int i, sme;

	for (i=0, va=0; i < NSEGMAP; i++, va+=NBSG) {
		sme = get_segmap(va);
		kernel_segmap[i] = sme;
	}
	pmap->pm_segmap = kernel_segmap;
}


/****************************************************************
 * PMAP interface functions.
 */

/*
 * Support functions for vm_page_bootstrap().
 */

/*
 * How much virtual space does this kernel have?
 * (After mapping kernel text, data, etc.)
 */
void
pmap_virtual_space(v_start, v_end)
	vm_offset_t *v_start;
	vm_offset_t *v_end;
{
	*v_start = virtual_avail;
	*v_end   = virtual_end;
}

/* Provide memory to the VM system. */
static void
pmap_page_upload()
{
	int a, b, c, d;

	if (hole_size) {
		/*
		 * Supply the memory in two segments so the
		 * reserved memory (3/50 video ram at 1MB)
		 * can be carved from the front of the 2nd.
		 */
		a = atop(avail_start);
		b = atop(hole_start);
		uvm_page_physload(a, b, a, b, VM_FREELIST_DEFAULT);
		c = atop(hole_start + hole_size);
		d = atop(avail_end);
		uvm_page_physload(b, d, c, d, VM_FREELIST_DEFAULT);
	} else {
		a = atop(avail_start);
		d = atop(avail_end);
		uvm_page_physload(a, d, a, d, VM_FREELIST_DEFAULT);
	}
}

/*
 * pmap_page_index()
 *
 * Given a physical address, return a page index.
 *
 * There can be some values that we never return (i.e. a hole)
 * as long as the range of indices returned by this function
 * is smaller than the value returned by pmap_free_pages().
 * The returned index does NOT need to start at zero.
 * (This is normally a macro in pmap.h)
 */
#ifndef	pmap_page_index
int
pmap_page_index(pa)
	vm_offset_t pa;
{
	int idx;

#ifdef	DIAGNOSTIC
	if (pa < avail_start || pa >= avail_end)
		panic("pmap_page_index: pa=0x%lx", pa);
#endif	/* DIAGNOSTIC */

	idx = atop(pa);
	return (idx);
}
#endif	/* !pmap_page_index */


/*
 *	Initialize the pmap module.
 *	Called by vm_init, to initialize any structures that the pmap
 *	system needs to map virtual memory.
 */
void
pmap_init()
{

	pv_init();

	/* Initialize the pmap pool. */
	pool_init(&pmap_pmap_pool, sizeof(struct pmap), 0, 0, 0, "pmappl",
	    0, pool_page_alloc_nointr, pool_page_free_nointr, M_VMPMAP);
}

/*
 * Map a range of kernel virtual address space.
 * This might be used for device mappings, or to
 * record the mapping for kernel text/data/bss.
 * Return VA following the mapped range.
 */
vm_offset_t
pmap_map(va, pa, endpa, prot)
	vm_offset_t	va;
	vm_offset_t	pa;
	vm_offset_t	endpa;
	int		prot;
{
	int sz;

	sz = endpa - pa;
	do {
		pmap_enter(kernel_pmap, va, pa, prot, 0);
		va += NBPG;
		pa += NBPG;
		sz -= NBPG;
	} while (sz > 0);
	return(va);
}

void
pmap_user_init(pmap)
	pmap_t pmap;
{
	int i;
	pmap->pm_segmap = malloc(sizeof(char)*NUSEG, M_VMPMAP, M_WAITOK);
	for (i=0; i < NUSEG; i++) {
		pmap->pm_segmap[i] = SEGINV;
	}
}

/*
 *	Create and return a physical map.
 *
 *	If the size specified for the map
 *	is zero, the map is an actual physical
 *	map, and may be referenced by the
 *	hardware.
 *
 *	If the size specified is non-zero,
 *	the map will be used in software only, and
 *	is bounded by that size.
 */
pmap_t
pmap_create()
{
	pmap_t pmap;

	pmap = pool_get(&pmap_pmap_pool, PR_WAITOK);
	pmap_pinit(pmap);
	return pmap;
}

/*
 * Release any resources held by the given physical map.
 * Called when a pmap initialized by pmap_pinit is being released.
 * Should only be called if the map contains no valid mappings.
 */
void
pmap_release(pmap)
	struct pmap *pmap;
{
	int s;

	s = splvm();

	if (pmap == kernel_pmap)
		panic("pmap_release: kernel_pmap!");

	if (has_context(pmap)) {
#ifdef	PMAP_DEBUG
		if (pmap_debug & PMD_CONTEXT)
			printf("pmap_release(%p): free ctx %d\n",
				   pmap, pmap->pm_ctxnum);
#endif
		context_free(pmap);
	}
	free(pmap->pm_segmap, M_VMPMAP);
	pmap->pm_segmap = NULL;

	splx(s);
}


/*
 *	Retire the given physical map from service.
 *	Should only be called if the map contains
 *	no valid mappings.
 */
void
pmap_destroy(pmap)
	pmap_t pmap;
{
	int count;

	if (pmap == NULL)
		return;	/* Duh! */

#ifdef PMAP_DEBUG
	if (pmap_debug & PMD_CREATE)
		printf("pmap_destroy(%p)\n", pmap);
#endif
	if (pmap == kernel_pmap)
		panic("pmap_destroy: kernel_pmap!");
	pmap_lock(pmap);
	count = pmap_del_ref(pmap);
	pmap_unlock(pmap);
	if (count == 0) {
		pmap_release(pmap);
		pool_put(&pmap_pmap_pool, pmap);
	}
}

/*
 *	Add a reference to the specified pmap.
 */
void
pmap_reference(pmap)
	pmap_t	pmap;
{
	if (pmap != NULL) {
		pmap_lock(pmap);
		pmap_add_ref(pmap);
		pmap_unlock(pmap);
	}
}


/*
 *	Insert the given physical page (p) at
 *	the specified virtual address (v) in the
 *	target physical map with the protection requested.
 *
 *	The physical address is page aligned, but may have some
 *	low bits set indicating an OBIO or VME bus page, or just
 *	that the non-cache bit should be set (i.e PMAP_NC).
 *
 *	If specified, the page will be wired down, meaning
 *	that the related pte can not be reclaimed.
 *
 *	NB:  This is the only routine which MAY NOT lazy-evaluate
 *	or lose information.  That is, this routine must actually
 *	insert this page into the given map NOW.
 */
int
pmap_enter(pmap, va, pa, prot, flags)
	pmap_t pmap;
	vm_offset_t va;
	vm_offset_t pa;
	vm_prot_t prot;
	int flags;
{
	int new_pte, s;
	boolean_t wired = (flags & PMAP_WIRED) != 0;

#ifdef	PMAP_DEBUG
	if ((pmap_debug & PMD_ENTER) ||
		(va == pmap_db_watchva))
		printf("pmap_enter(%p, 0x%lx, 0x%lx, 0x%x, 0x%x)\n",
			   pmap, va, pa, prot, wired);
#endif

	/* Get page-type bits from low part of the PA... */
	new_pte = (pa & PMAP_SPEC) << PG_MOD_SHIFT;

	/* ...now the valid and writable bits... */
	new_pte |= PG_VALID;
	if (prot & VM_PROT_WRITE)
		new_pte |= PG_WRITE;

	/* ...and finally the page-frame number. */
	new_pte |= PA_PGNUM(pa);

	/*
	 * treatment varies significantly:
	 *  kernel ptes are always in the mmu
	 *  user ptes may not necessarily? be in the mmu.  pmap may not
	 *   be in the mmu either.
	 *
	 */
	s = splvm();
	if (pmap == kernel_pmap) {
		new_pte |= PG_SYSTEM;
		pmap_enter_kernel(va, new_pte, wired);
	} else {
		pmap_enter_user(pmap, va, new_pte, wired);
	}
	splx(s);
	return (0);
}

static void
pmap_enter_kernel(pgva, new_pte, wired)
	vm_offset_t pgva;
	int new_pte;
	boolean_t wired;
{
	pmap_t pmap = kernel_pmap;
	pmeg_t pmegp;
	int do_pv, old_pte, sme;
	vm_offset_t segva;
	int saved_ctx;

	CHECK_SPL();

	/*
	  need to handle possibly allocating additional pmegs
	  need to make sure they cant be stolen from the kernel;
	  map any new pmegs into context zero, make sure rest of pmeg is null;
	  deal with pv_stuff; possibly caching problems;
	  must also deal with changes too.
	  */
	saved_ctx = get_context();
	set_context(KERNEL_CONTEXT);

	/*
	 * In detail:
	 *
	 * (a) lock pmap
	 * (b) Is the VA in a already mapped segment, if so
	 *	 look to see if that VA address is "valid".  If it is, then
	 *	 action is a change to an existing pte
	 * (c) if not mapped segment, need to allocate pmeg
	 * (d) if adding pte entry or changing physaddr of existing one,
	 *		use pv_stuff, for change, pmap_remove() possibly.
	 * (e) change/add pte
	 */

#ifdef	DIAGNOSTIC
	if ((pgva < virtual_avail) || (pgva >= DVMA_MAP_END))
		panic("pmap_enter_kernel: bad va=0x%lx", pgva);
	if ((new_pte & (PG_VALID | PG_SYSTEM)) != (PG_VALID | PG_SYSTEM))
		panic("pmap_enter_kernel: bad pte");
#endif

	if (pgva >= DVMA_MAP_BASE) {
		/* This is DVMA space.  Always want it non-cached. */
		new_pte |= PG_NC;
	}

	segva = m68k_trunc_seg(pgva);
	do_pv = TRUE;

	/* Do we have a PMEG? */
	sme = get_segmap(segva);
	if (sme != SEGINV) {
		/* Found a PMEG in the segmap.  Cool. */
		pmegp = pmeg_p(sme);
#ifdef	DIAGNOSTIC
		/* Make sure it is the right PMEG. */
		if (sme != pmap->pm_segmap[VA_SEGNUM(segva)])
			panic("pmap_enter_kernel: wrong sme at VA=0x%lx", segva);
		/* Make sure it is ours. */
		if (pmegp->pmeg_owner != pmap)
			panic("pmap_enter_kernel: MMU has bad pmeg 0x%x", sme);
#endif
	} else {
		/* No PMEG in the segmap.  Have to allocate one. */
		pmegp = pmeg_allocate(pmap, segva);
		sme = pmegp->pmeg_index;
		pmap->pm_segmap[VA_SEGNUM(segva)] = sme;
		set_segmap(segva, sme);
#ifdef	PMAP_DEBUG
		pmeg_verify_empty(segva);
		if (pmap_debug & PMD_SEGMAP) {
			printf("pmap: set_segmap pmap=%p va=0x%lx sme=0x%x (ek)\n",
				   pmap, segva, sme);
		}
#endif
		/* There are no existing mappings to deal with. */
		old_pte = 0;
		goto add_pte;
	}

	/*
	 * We have a PMEG.  Is the VA already mapped to somewhere?
	 *	(a) if so, is it same pa? (really a protection change)
	 *	(b) if not same pa, then we have to unlink from old pa
	 */
	old_pte = get_pte(pgva);
	if ((old_pte & PG_VALID) == 0)
		goto add_pte;

	/* Have valid translation.  Flush cache before changing it. */
#ifdef	HAVECACHE
	if (cache_size) {
		cache_flush_page(pgva);
		/* Get fresh mod/ref bits from write-back. */
		old_pte = get_pte(pgva);
	}
#endif

	/* XXX - removing valid page here, way lame... -glass */
	pmegp->pmeg_vpages--;

	if (!IS_MAIN_MEM(old_pte)) {
		/* Was not main memory, so no pv_entry for it. */
		goto add_pte;
	}

	/* Old mapping was main memory.  Save mod/ref bits. */
	save_modref_bits(old_pte);

	/*
	 * If not changing the type or pfnum then re-use pv_entry.
	 * Note we get here only with old_pte having PGT_OBMEM.
	 */
	if ((old_pte & (PG_TYPE|PG_FRAME)) ==
		(new_pte & (PG_TYPE|PG_FRAME)) )
	{
		do_pv = FALSE;		/* re-use pv_entry */
		new_pte |= (old_pte & PG_NC);
		goto add_pte;
	}

	/* OK, different type or PA, have to kill old pv_entry. */
	pv_unlink(pmap, old_pte, pgva);

 add_pte:	/* can be destructive */
	pmeg_set_wiring(pmegp, pgva, wired);

	/* Anything but MAIN_MEM is mapped non-cached. */
	if (!IS_MAIN_MEM(new_pte)) {
		new_pte |= PG_NC;
		do_pv = FALSE;
	}
	if (do_pv == TRUE) {
		if (pv_link(pmap, new_pte, pgva) & PV_NC)
			new_pte |= PG_NC;
	}
#ifdef	PMAP_DEBUG
	if ((pmap_debug & PMD_SETPTE) || (pgva == pmap_db_watchva)) {
		printf("pmap: set_pte pmap=%p va=0x%lx old=0x%x new=0x%x (ek)\n",
			   pmap, pgva, old_pte, new_pte);
	}
#endif
	/* cache flush done above */
	set_pte(pgva, new_pte);
	set_context(saved_ctx);
	pmegp->pmeg_vpages++;
}


static void
pmap_enter_user(pmap, pgva, new_pte, wired)
	pmap_t pmap;
	vm_offset_t pgva;
	int new_pte;
	boolean_t wired;
{
	int do_pv, old_pte, sme;
	vm_offset_t segva;
	pmeg_t pmegp;

	CHECK_SPL();

#ifdef	DIAGNOSTIC
	if (pgva >= VM_MAXUSER_ADDRESS)
		panic("pmap_enter_user: bad va=0x%lx", pgva);
	if ((new_pte & (PG_VALID | PG_SYSTEM)) != PG_VALID)
		panic("pmap_enter_user: bad pte");
#endif
#ifdef	PMAP_DEBUG
	/*
	 * Some user pages are wired here, and a later
	 * call to pmap_unwire() will unwire them.
	 * XXX - Need a separate list for wired user pmegs
	 * so they can not be stolen from the active list.
	 * XXX - Note: vm_fault.c assumes pmap_extract will
	 * work on wired mappings, so must preserve them...
	 * XXX: Maybe keep a list of wired PMEGs?
	 */
	if (wired && (pmap_debug & PMD_WIRING)) {
		db_printf("pmap_enter_user: attempt to wire user page, ignored\n");
		Debugger();
	}
#endif

	/* Validate this assumption. */
	if (pmap != current_pmap()) {
#ifdef	PMAP_DEBUG
		/* Aparently, this never happens. */
		db_printf("pmap_enter_user: not curproc\n");
		Debugger();
#endif
		/* Just throw it out (fault it in later). */
		/* XXX: But must remember it if wired... */
		return;
	}

	segva = m68k_trunc_seg(pgva);
	do_pv = TRUE;

	/*
	 * If this pmap was sharing the "empty" context,
	 * allocate a real context for its exclusive use.
	 */
	if (!has_context(pmap)) {
		context_allocate(pmap);
#ifdef PMAP_DEBUG
		if (pmap_debug & PMD_CONTEXT)
			printf("pmap_enter(%p) got context %d\n",
				   pmap, pmap->pm_ctxnum);
#endif
		set_context(pmap->pm_ctxnum);
	} else {
#ifdef	PMAP_DEBUG
		/* Make sure context is correct. */
		if (pmap->pm_ctxnum != get_context()) {
			db_printf("pmap_enter_user: wrong context\n");
			Debugger();
			/* XXX: OK to proceed? */
			set_context(pmap->pm_ctxnum);
		}
#endif
	}

	/*
	 * We have a context.  Do we have a PMEG?
	 */
	sme = get_segmap(segva);
	if (sme != SEGINV) {
		/* Found a PMEG in the segmap.  Cool. */
		pmegp = pmeg_p(sme);
#ifdef	DIAGNOSTIC
		/* Make sure it is the right PMEG. */
		if (sme != pmap->pm_segmap[VA_SEGNUM(segva)])
			panic("pmap_enter_user: wrong sme at VA=0x%lx", segva);
		/* Make sure it is ours. */
		if (pmegp->pmeg_owner != pmap)
			panic("pmap_enter_user: MMU has bad pmeg 0x%x", sme);
#endif
	} else {
		/* Not in the segmap.  Try the S/W cache. */
		pmegp = pmeg_cache(pmap, segva);
		if (pmegp) {
			/* Found PMEG in cache.  Just reload it. */
			sme = pmegp->pmeg_index;
			set_segmap(segva, sme);
		} else {
			/* PMEG not in cache, so allocate one. */
			pmegp = pmeg_allocate(pmap, segva);
			sme = pmegp->pmeg_index;
			pmap->pm_segmap[VA_SEGNUM(segva)] = sme;
			set_segmap(segva, sme);
#ifdef	PMAP_DEBUG
			pmeg_verify_empty(segva);
#endif
		}
#ifdef	PMAP_DEBUG
		if (pmap_debug & PMD_SEGMAP) {
			printf("pmap: set_segmap pmap=%p va=0x%lx sme=0x%x (eu)\n",
				   pmap, segva, sme);
		}
#endif
	}

	/*
	 * We have a PMEG.  Is the VA already mapped to somewhere?
	 *	(a) if so, is it same pa? (really a protection change)
	 *	(b) if not same pa, then we have to unlink from old pa
	 */
	old_pte = get_pte(pgva);
	if ((old_pte & PG_VALID) == 0)
		goto add_pte;

	/* Have valid translation.  Flush cache before changing it. */
#ifdef	HAVECACHE
	if (cache_size) {
		cache_flush_page(pgva);
		/* Get fresh mod/ref bits from write-back. */
		old_pte = get_pte(pgva);
	}
#endif

	/* XXX - removing valid page here, way lame... -glass */
	pmegp->pmeg_vpages--;

	if (!IS_MAIN_MEM(old_pte)) {
		/* Was not main memory, so no pv_entry for it. */
		goto add_pte;
	}

	/* Old mapping was main memory.  Save mod/ref bits. */
	save_modref_bits(old_pte);

	/*
	 * If not changing the type or pfnum then re-use pv_entry.
	 * Note we get here only with old_pte having PGT_OBMEM.
	 */
	if ((old_pte & (PG_TYPE|PG_FRAME)) ==
		(new_pte & (PG_TYPE|PG_FRAME)) )
	{
		do_pv = FALSE;		/* re-use pv_entry */
		new_pte |= (old_pte & PG_NC);
		goto add_pte;
	}

	/* OK, different type or PA, have to kill old pv_entry. */
	pv_unlink(pmap, old_pte, pgva);

 add_pte:
	/* XXX - Wiring changes on user pmaps? */
	/* pmeg_set_wiring(pmegp, pgva, wired); */

	/* Anything but MAIN_MEM is mapped non-cached. */
	if (!IS_MAIN_MEM(new_pte)) {
		new_pte |= PG_NC;
		do_pv = FALSE;
	}
	if (do_pv == TRUE) {
		if (pv_link(pmap, new_pte, pgva) & PV_NC)
			new_pte |= PG_NC;
	}
#ifdef	PMAP_DEBUG
	if ((pmap_debug & PMD_SETPTE) || (pgva == pmap_db_watchva)) {
		printf("pmap: set_pte pmap=%p va=0x%lx old=0x%x new=0x%x (eu)\n",
			   pmap, pgva, old_pte, new_pte);
	}
#endif
	/* cache flush done above */
	set_pte(pgva, new_pte);
	pmegp->pmeg_vpages++;
}

void
pmap_kenter_pa(va, pa, prot)
	vaddr_t va;
	paddr_t pa;
	vm_prot_t prot;
{
	pmap_enter(pmap_kernel(), va, pa, prot, PMAP_WIRED);
}

void
pmap_kenter_pgs(va, pgs, npgs)
	vaddr_t va;
	struct vm_page **pgs;
	int npgs;
{
	int i;

	for (i = 0; i < npgs; i++, va += PAGE_SIZE) {
		pmap_enter(pmap_kernel(), va, VM_PAGE_TO_PHYS(pgs[i]),
				VM_PROT_READ|VM_PROT_WRITE, PMAP_WIRED);
	}
}

void
pmap_kremove(va, len)
	vaddr_t va;
	vsize_t len;
{
	for (len >>= PAGE_SHIFT; len > 0; len--, va += PAGE_SIZE) {
		pmap_remove(pmap_kernel(), va, va + PAGE_SIZE);
	}
}


/*
 * The trap handler calls this so we can try to resolve
 * user-level faults by reloading a PMEG.
 * If that does not prodce a valid mapping,
 * call vm_fault as usual.
 *
 * XXX: Merge this with the next function?
 */
int
_pmap_fault(map, va, ftype)
	vm_map_t map;
	vm_offset_t va;
	vm_prot_t ftype;
{
	pmap_t pmap;
	int rv;

	pmap = vm_map_pmap(map);
	if (map == kernel_map) {
		/* Do not allow faults below the "managed" space. */
		if (va < virtual_avail) {
			/*
			 * Most pages below virtual_avail are read-only,
			 * so I will assume it is a protection failure.
			 */
			return EACCES;
		}
	} else {
		/* User map.  Try reload shortcut. */
		if (pmap_fault_reload(pmap, va, ftype))
			return 0;
	}
	rv = uvm_fault(map, va, 0, ftype);

#ifdef	PMAP_DEBUG
	if (pmap_debug & PMD_FAULT) {
		printf("pmap_fault(%p, 0x%lx, 0x%x) -> 0x%x\n",
			   map, va, ftype, rv);
	}
#endif

	return (rv);
}

/*
 * This is a shortcut used by the trap handler to
 * reload PMEGs into a user segmap without calling
 * the actual VM fault handler.  Returns TRUE if:
 *	the PMEG was reloaded, and
 *	it has a valid PTE at va.
 * Otherwise return zero and let VM code handle it.
 */
int
pmap_fault_reload(pmap, pgva, ftype)
	pmap_t pmap;
	vm_offset_t pgva;
	vm_prot_t ftype;
{
	int rv, s, pte, chkpte, sme;
	vm_offset_t segva;
	pmeg_t pmegp;

	if (pgva >= VM_MAXUSER_ADDRESS)
		return (0);
	if (pmap->pm_segmap == NULL) {
#ifdef	PMAP_DEBUG
		db_printf("pmap_fault_reload: null segmap\n");
		Debugger();
#endif
		return (0);
	}

	/* Short-cut using the S/W segmap. */
	if (pmap->pm_segmap[VA_SEGNUM(pgva)] == SEGINV)
		return (0);

	segva = m68k_trunc_seg(pgva);
	chkpte = PG_VALID;
	if (ftype & VM_PROT_WRITE)
		chkpte |= PG_WRITE;
	rv = 0;

	s = splvm();

	/*
	 * Given that we faulted on a user-space address, we will
	 * probably need a context.  Get a context now so we can
	 * try to resolve the fault with a segmap reload.
	 */
	if (!has_context(pmap)) {
		context_allocate(pmap);
#ifdef PMAP_DEBUG
		if (pmap_debug & PMD_CONTEXT)
			printf("pmap_fault(%p) got context %d\n",
				   pmap, pmap->pm_ctxnum);
#endif
		set_context(pmap->pm_ctxnum);
	} else {
#ifdef	PMAP_DEBUG
		/* Make sure context is correct. */
		if (pmap->pm_ctxnum != get_context()) {
			db_printf("pmap_fault_reload: wrong context\n");
			Debugger();
			/* XXX: OK to proceed? */
			set_context(pmap->pm_ctxnum);
		}
#endif
	}

	sme = get_segmap(segva);
	if (sme == SEGINV) {
		/* See if there is something to reload. */
		pmegp = pmeg_cache(pmap, segva);
		if (pmegp) {
			/* Found one!  OK, reload it. */
			pmap_stats.ps_pmeg_faultin++;
			sme = pmegp->pmeg_index;
			set_segmap(segva, sme);
			pte = get_pte(pgva);
			if (pte & chkpte)
				rv = 1;
		}
	}

	splx(s);
	return (rv);
}


/*
 * Clear the modify bit for the given physical page.
 */
boolean_t
pmap_clear_modify(pg)
	struct vm_page *pg;
{
	paddr_t pa = VM_PAGE_TO_PHYS(pg);
	pv_entry_t *head;
	u_char *pv_flags;
	int s;
	boolean_t rv;

	if (!pv_initialized)
		return FALSE;

	/* The VM code may call this on device addresses! */
	if (PA_IS_DEV(pa))
		return FALSE;

	pv_flags = pa_to_pvflags(pa);
	head     = pa_to_pvhead(pa);

	s = splvm();
	*pv_flags |= pv_syncflags(*head);
	rv = *pv_flags & PV_MOD;
	*pv_flags &= ~PV_MOD;
	splx(s);
	return rv;
}

/*
 * Tell whether the given physical page has been modified.
 */
int
pmap_is_modified(pg)
	struct vm_page *pg;
{
	paddr_t pa = VM_PAGE_TO_PHYS(pg);
	pv_entry_t *head;
	u_char *pv_flags;
	int rv, s;

	if (!pv_initialized)
		return (0);

	/* The VM code may call this on device addresses! */
	if (PA_IS_DEV(pa))
		return (0);

	pv_flags = pa_to_pvflags(pa);
	head     = pa_to_pvhead(pa);

	s = splvm();
	if ((*pv_flags & PV_MOD) == 0)
		*pv_flags |= pv_syncflags(*head);
	rv = (*pv_flags & PV_MOD);
	splx(s);

	return (rv);
}

/*
 * Clear the reference bit for the given physical page.
 * It's OK to just remove mappings if that's easier.
 */
boolean_t
pmap_clear_reference(pg)
	struct vm_page *pg;
{
	paddr_t pa = VM_PAGE_TO_PHYS(pg);
	pv_entry_t *head;
	u_char *pv_flags;
	int s;
	boolean_t rv;

	if (!pv_initialized)
		return FALSE;

	/* The VM code may call this on device addresses! */
	if (PA_IS_DEV(pa))
		return FALSE;

	pv_flags = pa_to_pvflags(pa);
	head     = pa_to_pvhead(pa);

	s = splvm();
	*pv_flags |= pv_syncflags(*head);
	rv = *pv_flags & PV_REF;
	*pv_flags &= ~PV_REF;
	splx(s);
	return rv;
}

/*
 * Tell whether the given physical page has been referenced.
 * It's OK to just return FALSE if page is not mapped.
 */
boolean_t
pmap_is_referenced(pg)
	struct vm_page *pg;
{
	paddr_t pa = VM_PAGE_TO_PHYS(pg);
	pv_entry_t *head;
	u_char *pv_flags;
	int s;
	boolean_t rv;

	if (!pv_initialized)
		return (FALSE);

	/* The VM code may call this on device addresses! */
	if (PA_IS_DEV(pa))
		return (FALSE);

	pv_flags = pa_to_pvflags(pa);
	head     = pa_to_pvhead(pa);

	s = splvm();
	if ((*pv_flags & PV_REF) == 0)
		*pv_flags |= pv_syncflags(*head);
	rv = (*pv_flags & PV_REF);
	splx(s);

	return (rv);
}


/*
 * This is called by locore.s:cpu_switch() when it is
 * switching to a new process.  Load new translations.
 */
void
_pmap_switch(pmap)
	pmap_t pmap;
{

	CHECK_SPL();

	/*
	 * Since we maintain completely separate user and kernel address
	 * spaces, whenever we switch to a process, we need to make sure
	 * that it has a context allocated.
	 */
	if (!has_context(pmap)) {
		context_allocate(pmap);
#ifdef PMAP_DEBUG
		if (pmap_debug & PMD_CONTEXT)
			printf("_pmap_switch(%p) got context %d\n",
				   pmap, pmap->pm_ctxnum);
#endif
	}
	set_context(pmap->pm_ctxnum);
}

/*
 * Exported version of pmap_activate().  This is called from the
 * machine-independent VM code when a process is given a new pmap.
 * If (p == curproc) do like cpu_switch would do; otherwise just
 * take this as notification that the process has a new pmap.
 */
void
pmap_activate(p)
	struct proc *p;
{
	pmap_t pmap = p->p_vmspace->vm_map.pmap;
	int s;

	if (p == curproc) {
		s = splvm();
		_pmap_switch(pmap);
		splx(s);
	}
}

/*
 * Deactivate the address space of the specified process.
 * XXX The semantics of this function are not currently well-defined.
 */
void
pmap_deactivate(p)
	struct proc *p;
{
	/* not implemented. */
}

/*
 *	Routine:	pmap_unwire
 *	Function:	Clear the wired attribute for a map/virtual-address
 *			pair.
 *	In/out conditions:
 *			The mapping must already exist in the pmap.
 */
void
pmap_unwire(pmap, va)
	pmap_t	pmap;
	vm_offset_t	va;
{
	int s, sme;
	int wiremask, ptenum;
	pmeg_t pmegp;
	int saved_ctx;

	if (pmap == NULL)
		return;
#ifdef PMAP_DEBUG
	if (pmap_debug & PMD_WIRING)
		printf("pmap_unwire(pmap=%p, va=0x%lx)\n",
			   pmap, va);
#endif
	/*
	 * We are asked to unwire pages that were wired when
	 * pmap_enter() was called and we ignored wiring.
	 * (VM code appears to wire a stack page during fork.)
	 */
	if (pmap != kernel_pmap) {
#ifdef PMAP_DEBUG
		if (pmap_debug & PMD_WIRING) {
			db_printf("  (user pmap -- ignored)\n");
			Debugger();
		}
#endif
		return;
	}

	ptenum = VA_PTE_NUM(va);
	wiremask = 1 << ptenum;

	s = splvm();

	saved_ctx = get_context();
	set_context(KERNEL_CONTEXT);
	sme = get_segmap(va);
	set_context(saved_ctx);
	if (sme == SEGINV)
		panic("pmap_unwire: invalid va=0x%lx", va);
	pmegp = pmeg_p(sme);
	pmegp->pmeg_wired &= ~wiremask;

	splx(s);
}

/*
 *	Copy the range specified by src_addr/len
 *	from the source map to the range dst_addr/len
 *	in the destination map.
 *
 *	This routine is only advisory and need not do anything.
 */
void
pmap_copy(dst_pmap, src_pmap, dst_addr, len, src_addr)
	pmap_t		dst_pmap;
	pmap_t		src_pmap;
	vm_offset_t	dst_addr;
	vm_size_t	len;
	vm_offset_t	src_addr;
{
}

/*
 * This extracts the PMEG associated with the given map/virtual
 * address pair.  Returns SEGINV if VA not valid.
 */
int
_pmap_extract_pmeg(pmap, va)
		pmap_t	pmap;
		vm_offset_t va;
{
		int s, saved_ctx, segnum, sme;

		s = splvm();

		if (pmap == kernel_pmap) {
				saved_ctx = get_context();
				set_context(KERNEL_CONTEXT);
				sme = get_segmap(va);
				set_context(saved_ctx);
		} else {
				/* This is rare, so do it the easy way. */
				segnum = VA_SEGNUM(va);
				sme = pmap->pm_segmap[segnum];
		}
		
		splx(s);
		return (sme);
}

/*
 *	Routine:	pmap_extract
 *	Function:
 *		Extract the physical page address associated
 *		with the given map/virtual_address pair.
 *	Returns zero if VA not valid.
 */
boolean_t
pmap_extract(pmap, va, pap)
	pmap_t	pmap;
	vm_offset_t va;
	paddr_t *pap;
{
	int s, sme, segnum, ptenum, pte;
	paddr_t pa;
	int saved_ctx;

	pte = 0;
	s = splvm();

	if (pmap == kernel_pmap) {
		saved_ctx = get_context();
		set_context(KERNEL_CONTEXT);
		sme = get_segmap(va);
		if (sme != SEGINV)
			pte = get_pte(va);
		set_context(saved_ctx);
	} else {
		/* This is rare, so do it the easy way. */
		segnum = VA_SEGNUM(va);
		sme = pmap->pm_segmap[segnum];
		if (sme != SEGINV) {
			ptenum = VA_PTE_NUM(va);
			pte = get_pte_pmeg(sme, ptenum);
		}
	}

	splx(s);

	if ((pte & PG_VALID) == 0) {
#ifdef PMAP_DEBUG
		db_printf("pmap_extract: invalid va=0x%lx\n", va);
		Debugger();
#endif
		return (FALSE);
	}
	pa = PG_PA(pte);
#ifdef	DIAGNOSTIC
	if (pte & PG_TYPE) {
		panic("pmap_extract: not main mem, va=0x%lx\n", va);
	}
#endif
	if (pap != NULL)
		*pap = pa;
	return (TRUE);
}


/*
 *	  pmap_page_protect:
 *
 *	  Lower the permission for all mappings to a given page.
 */
void
pmap_page_protect(pg, prot)
	struct vm_page *pg;
	vm_prot_t	   prot;
{
	paddr_t pa = VM_PAGE_TO_PHYS(pg);
	int s;

	/* The VM code may call this on device addresses! */
	if (PA_IS_DEV(pa))
		return;

	s = splvm();

#ifdef PMAP_DEBUG
	if (pmap_debug & PMD_PROTECT)
		printf("pmap_page_protect(0x%lx, 0x%lx)\n", pa, prot);
#endif
	switch (prot) {
	case VM_PROT_ALL:
		break;
	case VM_PROT_READ:
	case VM_PROT_READ|VM_PROT_EXECUTE:
		pv_changepte(pa, 0, PG_WRITE);
		break;
	default:
		/* remove mapping for all pmaps that have it */
		pv_remove_all(pa);
		break;
	}

	splx(s);
}

/*
 * Turn a cdevsw d_mmap value into a byte address for pmap_enter.
 * XXX	this should almost certainly be done differently, and
 *	elsewhere, or even not at all
 */
#ifndef	pmap_phys_address
vm_offset_t
pmap_phys_address(x)
	int x;
{
	return (x);
}
#endif

/*
 * Initialize a preallocated and zeroed pmap structure,
 * such as one in a vmspace structure.
 */
void
pmap_pinit(pmap)
	pmap_t pmap;
{
	pmap_common_init(pmap);
	pmap_user_init(pmap);
}

/*
 *	Reduce the permissions on the specified
 *	range of this map as requested.
 *	(Make pages read-only.)
 */
void
pmap_protect(pmap, sva, eva, prot)
	pmap_t pmap;
	vm_offset_t sva, eva;
	vm_prot_t	prot;
{
	vm_offset_t va, neva;
	int segnum;

	if (pmap == NULL)
		return;

	/* If leaving writable, nothing to do. */
	if (prot & VM_PROT_WRITE)
		return;

	/* If removing all permissions, just unmap. */
	if ((prot & VM_PROT_READ) == 0) {
		pmap_remove(pmap, sva, eva);
		return;
	}

#ifdef	PMAP_DEBUG
	if ((pmap_debug & PMD_PROTECT) ||
		((sva <= pmap_db_watchva && eva > pmap_db_watchva)))
		printf("pmap_protect(%p, 0x%lx, 0x%lx)\n", pmap, sva, eva);
#endif

	if (pmap == kernel_pmap) {
		if (sva < virtual_avail)
			sva = virtual_avail;
		if (eva > DVMA_MAP_END) {
#ifdef	PMAP_DEBUG
			db_printf("pmap_protect: eva=0x%lx\n", eva);
			Debugger();
#endif
			eva = DVMA_MAP_END;
		}
	} else {
		if (eva > VM_MAXUSER_ADDRESS)
			eva = VM_MAXUSER_ADDRESS;
	}

	va = sva;
	segnum = VA_SEGNUM(va);
	while (va < eva) {
		neva = m68k_trunc_seg(va) + NBSG;
		if (neva > eva)
			neva = eva;
		if (pmap->pm_segmap[segnum] != SEGINV)
			pmap_protect1(pmap, va, neva);
		va = neva;
		segnum++;
	}
}

/*
 * Remove write permissions in given range.
 * (guaranteed to be within one segment)
 * similar to pmap_remove1()
 */
void
pmap_protect1(pmap, sva, eva)
	pmap_t pmap;
	vm_offset_t sva, eva;
{
	int old_ctx, s, sme;
	boolean_t in_ctx;

	s = splvm();

#ifdef	DIAGNOSTIC
	if (m68k_trunc_seg(sva) != m68k_trunc_seg(eva-1))
		panic("pmap_protect1: bad range!");
#endif

	if (pmap == kernel_pmap) {
		old_ctx = get_context();
		set_context(KERNEL_CONTEXT);
		sme = get_segmap(sva);
		if (sme != SEGINV) {
			pmap_protect_mmu(pmap, sva, eva);
		}
		set_context(old_ctx);
		goto out;
	}
	/* It is a user pmap. */

	/* There is a PMEG, but maybe not active. */
	old_ctx = INVALID_CONTEXT;
	in_ctx = FALSE;
	if (has_context(pmap)) {
		/* Temporary context change. */
		old_ctx = get_context();
		set_context(pmap->pm_ctxnum);
		sme = get_segmap(sva);
		if (sme != SEGINV)
			in_ctx = TRUE;
	}

	if (in_ctx == TRUE)
		pmap_protect_mmu(pmap, sva, eva);
	else
		pmap_protect_noctx(pmap, sva, eva);

	if (old_ctx != INVALID_CONTEXT) {
		/* Restore previous context. */
		set_context(old_ctx);
	}

out:
	splx(s);
}

/*
 * Remove write permissions, all in one PMEG,
 * where that PMEG is currently in the MMU.
 * The current context is already correct.
 */
void
pmap_protect_mmu(pmap, sva, eva)
	pmap_t pmap;
	vm_offset_t sva, eva;
{
	pmeg_t pmegp;
	vm_offset_t pgva, segva;
	int pte, sme;
#ifdef	HAVECACHE
	int flush_by_page = 0;
#endif

	CHECK_SPL();

#ifdef	DIAGNOSTIC
		if (pmap->pm_ctxnum != get_context())
			panic("pmap_protect_mmu: wrong context");
#endif

	segva = m68k_trunc_seg(sva);
	sme = get_segmap(segva);

#ifdef	DIAGNOSTIC
	/* Make sure it is valid and known. */
	if (sme == SEGINV)
		panic("pmap_protect_mmu: SEGINV");
	if (pmap->pm_segmap && (pmap->pm_segmap[VA_SEGNUM(segva)] != sme))
		panic("pmap_protect_mmu: incorrect sme, va=0x%lx", segva);
#endif

	pmegp = pmeg_p(sme);
	/* have pmeg, will travel */

#ifdef	DIAGNOSTIC
	/* Make sure we own the pmeg, right va, etc. */
	if ((pmegp->pmeg_va != segva) ||
		(pmegp->pmeg_owner != pmap) ||
		(pmegp->pmeg_version != pmap->pm_version))
	{
		panic("pmap_protect_mmu: bad pmeg=%p", pmegp);
	}
	if (pmegp->pmeg_vpages <= 0)
		panic("pmap_protect_mmu: no valid pages?");
#endif

#ifdef	HAVECACHE
	if (cache_size) {
		/*
		 * If the range to be removed is larger than the cache,
		 * it will be cheaper to flush this segment entirely.
		 */
		if (cache_size < (eva - sva)) {
			/* cheaper to flush whole segment */
			cache_flush_segment(segva);
		} else {
			flush_by_page = 1;
		}
	}
#endif

	/* Remove write permission in the given range. */
	for (pgva = sva; pgva < eva; pgva += NBPG) {
		pte = get_pte(pgva);
		if (pte & PG_VALID) {
#ifdef	HAVECACHE
			if (flush_by_page) {
				cache_flush_page(pgva);
				/* Get fresh mod/ref bits from write-back. */
				pte = get_pte(pgva);
			}
#endif
			if (IS_MAIN_MEM(pte)) {
				save_modref_bits(pte);
			}
			pte &= ~(PG_WRITE | PG_MODREF);
			set_pte(pgva, pte);
		}
	}
}

/*
 * Remove write permissions, all in one PMEG,
 * where it is not currently in any context.
 */
void
pmap_protect_noctx(pmap, sva, eva)
	pmap_t pmap;
	vm_offset_t sva, eva;
{
	int old_ctx, pte, sme, segnum;
	vm_offset_t pgva, segva;

	CHECK_SPL();

#ifdef	DIAGNOSTIC
	/* Kernel always in a context (actually, in context zero). */
	if (pmap == kernel_pmap)
		panic("pmap_protect_noctx: kernel_pmap");
	if (pmap->pm_segmap == NULL)
		panic("pmap_protect_noctx: null segmap");
#endif

	segva = m68k_trunc_seg(sva);
	segnum = VA_SEGNUM(segva);
	sme = pmap->pm_segmap[segnum];
	if (sme == SEGINV)
		return;

	/*
	 * Switch to the kernel context so we can access the PMEG
	 * using the temporary segment.
	 */
	old_ctx = get_context();
	set_context(KERNEL_CONTEXT);
	if (temp_seg_inuse)
		panic("pmap_protect_noctx: temp_seg_inuse");
	temp_seg_inuse++;
	set_segmap(temp_seg_va, sme);
	sva += (temp_seg_va - segva);
	eva += (temp_seg_va - segva);

	/* Remove write permission in the given range. */
	for (pgva = sva; pgva < eva; pgva += NBPG) {
		pte = get_pte(pgva);
		if (pte & PG_VALID) {
			/* No cache flush needed. */
			if (IS_MAIN_MEM(pte)) {
				save_modref_bits(pte);
			}
			pte &= ~(PG_WRITE | PG_MODREF);
			set_pte(pgva, pte);
		}
	}

	/*
	 * Release the temporary segment, and
	 * restore the previous context.
	 */
	set_segmap(temp_seg_va, SEGINV);
	temp_seg_inuse--;
	set_context(old_ctx);
}


/*
 *	Remove the given range of addresses from the specified map.
 *
 *	It is assumed that the start and end are properly
 *	rounded to the page size.
 */
void
pmap_remove(pmap, sva, eva)
	pmap_t pmap;
	vm_offset_t sva, eva;
{
	vm_offset_t va, neva;
	int segnum;

	if (pmap == NULL)
		return;

#ifdef	PMAP_DEBUG
	if ((pmap_debug & PMD_REMOVE) ||
		((sva <= pmap_db_watchva && eva > pmap_db_watchva)))
		printf("pmap_remove(%p, 0x%lx, 0x%lx)\n", pmap, sva, eva);
#endif

	if (pmap == kernel_pmap) {
		if (sva < virtual_avail)
			sva = virtual_avail;
		if (eva > DVMA_MAP_END) {
#ifdef	PMAP_DEBUG
			db_printf("pmap_remove: eva=0x%lx\n", eva);
			Debugger();
#endif
			eva = DVMA_MAP_END;
		}
	} else {
		if (eva > VM_MAXUSER_ADDRESS)
			eva = VM_MAXUSER_ADDRESS;
	}

	va = sva;
	segnum = VA_SEGNUM(va);
	while (va < eva) {
		neva = m68k_trunc_seg(va) + NBSG;
		if (neva > eva)
			neva = eva;
		if (pmap->pm_segmap[segnum] != SEGINV)
			pmap_remove1(pmap, va, neva);
		va = neva;
		segnum++;
	}
}

/*
 * Remove user mappings, all within one segment
 */
void
pmap_remove1(pmap, sva, eva)
	pmap_t pmap;
	vm_offset_t sva, eva;
{
	int old_ctx, s, sme;
	boolean_t in_ctx;

	s = splvm();

#ifdef	DIAGNOSTIC
	if (m68k_trunc_seg(sva) != m68k_trunc_seg(eva-1))
		panic("pmap_remove1: bad range!");
#endif

	if (pmap == kernel_pmap) {
		old_ctx = get_context();
		set_context(KERNEL_CONTEXT);
		sme = get_segmap(sva);
		if (sme != SEGINV)
			pmap_remove_mmu(pmap, sva, eva);
		set_context(old_ctx);
		goto out;
	}
	/* It is a user pmap. */

	/* There is a PMEG, but maybe not active. */
	old_ctx = INVALID_CONTEXT;
	in_ctx = FALSE;
	if (has_context(pmap)) {
		/* Temporary context change. */
		old_ctx = get_context();
		set_context(pmap->pm_ctxnum);
		sme = get_segmap(sva);
		if (sme != SEGINV)
			in_ctx = TRUE;
	}

	if (in_ctx == TRUE)
		pmap_remove_mmu(pmap, sva, eva);
	else
		pmap_remove_noctx(pmap, sva, eva);

	if (old_ctx != INVALID_CONTEXT) {
		/* Restore previous context. */
		set_context(old_ctx);
	}

out:
	splx(s);
}

/*
 * Remove some mappings, all in one PMEG,
 * where that PMEG is currently in the MMU.
 * The current context is already correct.
 * If no PTEs remain valid in the PMEG, free it.
 */
void
pmap_remove_mmu(pmap, sva, eva)
	pmap_t pmap;
	vm_offset_t sva, eva;
{
	pmeg_t pmegp;
	vm_offset_t pgva, segva;
	int pte, sme;
#ifdef	HAVECACHE
	int flush_by_page = 0;
#endif

	CHECK_SPL();

#ifdef	DIAGNOSTIC
		if (pmap->pm_ctxnum != get_context())
			panic("pmap_remove_mmu: wrong context");
#endif

	segva = m68k_trunc_seg(sva);
	sme = get_segmap(segva);

#ifdef	DIAGNOSTIC
	/* Make sure it is valid and known. */
	if (sme == SEGINV)
		panic("pmap_remove_mmu: SEGINV");
	if (pmap->pm_segmap && (pmap->pm_segmap[VA_SEGNUM(segva)] != sme))
		panic("pmap_remove_mmu: incorrect sme, va=0x%lx", segva);
#endif

	pmegp = pmeg_p(sme);
	/* have pmeg, will travel */

#ifdef	DIAGNOSTIC
	/* Make sure we own the pmeg, right va, etc. */
	if ((pmegp->pmeg_va != segva) ||
		(pmegp->pmeg_owner != pmap) ||
		(pmegp->pmeg_version != pmap->pm_version))
	{
		panic("pmap_remove_mmu: bad pmeg=%p", pmegp);
	}
	if (pmegp->pmeg_vpages <= 0)
		panic("pmap_remove_mmu: no valid pages?");
#endif

#ifdef	HAVECACHE
	if (cache_size) {
		/*
		 * If the range to be removed is larger than the cache,
		 * it will be cheaper to flush this segment entirely.
		 */
		if (cache_size < (eva - sva)) {
			/* cheaper to flush whole segment */
			cache_flush_segment(segva);
		} else {
			flush_by_page = 1;
		}
	}
#endif

	/* Invalidate the PTEs in the given range. */
	for (pgva = sva; pgva < eva; pgva += NBPG) {
		pte = get_pte(pgva);
		if (pte & PG_VALID) {
#ifdef	HAVECACHE
			if (flush_by_page) {
				cache_flush_page(pgva);
				/* Get fresh mod/ref bits from write-back. */
				pte = get_pte(pgva);
			}
#endif
			if (IS_MAIN_MEM(pte)) {
				save_modref_bits(pte);
				pv_unlink(pmap, pte, pgva);
			}
#ifdef	PMAP_DEBUG
			if ((pmap_debug & PMD_SETPTE) || (pgva == pmap_db_watchva)) {
				printf("pmap: set_pte pmap=%p va=0x%lx"
					   " old=0x%x new=0x%x (rrmmu)\n",
					   pmap, pgva, pte, PG_INVAL);
			}
#endif
			set_pte(pgva, PG_INVAL);
			pmegp->pmeg_vpages--;
		}
	}

	if (pmegp->pmeg_vpages <= 0) {
		/* We are done with this pmeg. */
		if (is_pmeg_wired(pmegp)) {
#ifdef	PMAP_DEBUG
			if (pmap_debug & PMD_WIRING) {
				db_printf("pmap: removing wired pmeg: %p\n", pmegp);
				Debugger();
			}
#endif	/* PMAP_DEBUG */
		}

#ifdef	PMAP_DEBUG
		if (pmap_debug & PMD_SEGMAP) {
			printf("pmap: set_segmap ctx=%d v=0x%lx old=0x%x new=ff (rm)\n",
			    pmap->pm_ctxnum, segva, pmegp->pmeg_index);
		}
		pmeg_verify_empty(segva);
#endif

		/* Remove it from the MMU. */
		if (kernel_pmap == pmap) {
			/* Did cache flush above. */
			set_segmap(segva, SEGINV);
		} else {
			/* Did cache flush above. */
			set_segmap(segva, SEGINV);
		}
		pmap->pm_segmap[VA_SEGNUM(segva)] = SEGINV;
		/* Now, put it on the free list. */
		pmeg_free(pmegp);
	}
}

/*
 * Remove some mappings, all in one PMEG,
 * where it is not currently in any context.
 */
void
pmap_remove_noctx(pmap, sva, eva)
	pmap_t pmap;
	vm_offset_t sva, eva;
{
	pmeg_t pmegp;
	int old_ctx, pte, sme, segnum;
	vm_offset_t pgva, segva;

	CHECK_SPL();

#ifdef	DIAGNOSTIC
	/* Kernel always in a context (actually, in context zero). */
	if (pmap == kernel_pmap)
		panic("pmap_remove_noctx: kernel_pmap");
	if (pmap->pm_segmap == NULL)
		panic("pmap_remove_noctx: null segmap");
#endif

	segva = m68k_trunc_seg(sva);
	segnum = VA_SEGNUM(segva);
	sme = pmap->pm_segmap[segnum];
	if (sme == SEGINV)
		return;
	pmegp = pmeg_p(sme);

	/*
	 * Switch to the kernel context so we can access the PMEG
	 * using the temporary segment.
	 */
	old_ctx = get_context();
	set_context(KERNEL_CONTEXT);
	if (temp_seg_inuse)
		panic("pmap_remove_noctx: temp_seg_inuse");
	temp_seg_inuse++;
	set_segmap(temp_seg_va, sme);
	sva += (temp_seg_va - segva);
	eva += (temp_seg_va - segva);

	/* Invalidate the PTEs in the given range. */
	for (pgva = sva; pgva < eva; pgva += NBPG) {
		pte = get_pte(pgva);
		if (pte & PG_VALID) {
			/* No cache flush needed. */
			if (IS_MAIN_MEM(pte)) {
				save_modref_bits(pte);
				pv_unlink(pmap, pte, pgva - (temp_seg_va - segva));
			}
#ifdef	PMAP_DEBUG
			if ((pmap_debug & PMD_SETPTE) || (pgva == pmap_db_watchva)) {
				printf("pmap: set_pte pmap=%p va=0x%lx"
					   " old=0x%x new=0x%x (rrncx)\n",
					   pmap, pgva, pte, PG_INVAL);
			}
#endif
			set_pte(pgva, PG_INVAL);
			pmegp->pmeg_vpages--;
		}
	}

	/*
	 * Release the temporary segment, and
	 * restore the previous context.
	 */
	set_segmap(temp_seg_va, SEGINV);
	temp_seg_inuse--;
	set_context(old_ctx);

	if (pmegp->pmeg_vpages <= 0) {
		/* We are done with this pmeg. */
		if (is_pmeg_wired(pmegp)) {
#ifdef	PMAP_DEBUG
			if (pmap_debug & PMD_WIRING) {
				db_printf("pmap: removing wired pmeg: %p\n", pmegp);
				Debugger();
			}
#endif	/* PMAP_DEBUG */
		}

		pmap->pm_segmap[segnum] = SEGINV;
		pmeg_free(pmegp);
	}
}


/*
 * Count resident pages in this pmap.
 * See: kern_sysctl.c:pmap_resident_count
 */
segsz_t
pmap_resident_pages(pmap)
	pmap_t pmap;
{
	int i, sme, pages;
	pmeg_t pmeg;

	if (pmap->pm_segmap == 0)
		return (0);

	pages = 0;
	for (i = 0; i < NUSEG; i++) {
		sme = pmap->pm_segmap[i];
		if (sme != SEGINV) {
			pmeg = pmeg_p(sme);
			pages += pmeg->pmeg_vpages;
		}
	}
	return (pages);
}

/*
 * Count wired pages in this pmap.
 * See vm_mmap.c:pmap_wired_count
 */
segsz_t
pmap_wired_pages(pmap)
	pmap_t pmap;
{
	int i, mask, sme, pages;
	pmeg_t pmeg;

	if (pmap->pm_segmap == 0)
		return (0);

	pages = 0;
	for (i = 0; i < NUSEG; i++) {
		sme = pmap->pm_segmap[i];
		if (sme != SEGINV) {
			pmeg = pmeg_p(sme);
			mask = 0x8000;
			do {
				if (pmeg->pmeg_wired & mask)
					pages++;
				mask = (mask >> 1);
			} while (mask);
		}
	}
	return (pages);
}


/*
 *	pmap_copy_page copies the specified (machine independent)
 *	page by mapping the page into virtual memory and using
 *	bcopy to copy the page, one machine dependent page at a
 *	time.
 */
void
pmap_copy_page(src, dst)
	vm_offset_t	src, dst;
{
	int pte;
	int s;
	int saved_ctx;

	s = splvm();

#ifdef	PMAP_DEBUG
	if (pmap_debug & PMD_COW)
		printf("pmap_copy_page: 0x%lx -> 0x%lx\n", src, dst);
#endif

	/*
	 * Temporarily switch to the kernel context to use the
	 * tmp_vpages.
	 */
	saved_ctx = get_context();
	set_context(KERNEL_CONTEXT);
	if (tmp_vpages_inuse)
		panic("pmap_copy_page: vpages inuse");
	tmp_vpages_inuse++;

	/* PG_PERM is short for (PG_VALID|PG_WRITE|PG_SYSTEM|PG_NC) */
	/* All mappings to vmp_vpages are non-cached, so no flush. */
	pte = PG_PERM | PA_PGNUM(src);
	set_pte(tmp_vpages[0], pte);
	pte = PG_PERM | PA_PGNUM(dst);
	set_pte(tmp_vpages[1], pte);
	copypage((char *) tmp_vpages[0], (char *) tmp_vpages[1]);
	set_pte(tmp_vpages[0], PG_INVAL);
	set_pte(tmp_vpages[1], PG_INVAL);

	tmp_vpages_inuse--;
	set_context(saved_ctx);

	splx(s);
}

/*
 *	pmap_zero_page zeros the specified (machine independent)
 *	page by mapping the page into virtual memory and using
 *	bzero to clear its contents, one machine dependent page
 *	at a time.
 */
void
pmap_zero_page(pa)
	vm_offset_t	pa;
{
	int pte;
	int s;
	int saved_ctx;

	s = splvm();

#ifdef	PMAP_DEBUG
	if (pmap_debug & PMD_COW)
		printf("pmap_zero_page: 0x%lx\n", pa);
#endif

	/*
	 * Temporarily switch to the kernel context to use the
	 * tmp_vpages.
	 */
	saved_ctx = get_context();
	set_context(KERNEL_CONTEXT);
	if (tmp_vpages_inuse)
		panic("pmap_zero_page: vpages inuse");
	tmp_vpages_inuse++;

	/* PG_PERM is short for (PG_VALID|PG_WRITE|PG_SYSTEM|PG_NC) */
	/* All mappings to vmp_vpages are non-cached, so no flush. */
	pte = PG_PERM | PA_PGNUM(pa);
	set_pte(tmp_vpages[0], pte);
	zeropage((char *) tmp_vpages[0]);
	set_pte(tmp_vpages[0], PG_INVAL);

	tmp_vpages_inuse--;
	set_context(saved_ctx);

	splx(s);
}

/*
 *	Routine:	pmap_collect
 *	Function:
 *		Garbage collects the physical map system for
 *		pages which are no longer used.
 *		Success need not be guaranteed -- that is, there
 *		may well be pages which are not referenced, but
 *		others may be collected.
 *	Usage:
 *		Called by the pageout daemon when pages are scarce.
 */
void
pmap_collect(pmap)
	pmap_t pmap;
{
}

/*
 * Find first virtual address >= *va that is
 * least likely to cause cache aliases.
 * (This will just seg-align mappings.)
 */
void
pmap_prefer(fo, va)
	register vm_offset_t fo;
	register vm_offset_t *va;
{
	register long	d;

	d = fo - *va;
	d &= SEGOFSET;
	*va += d;
}

/*
 * Fill in the sun2-specific part of the kernel core header
 * for dumpsys().  (See machdep.c for the rest.)
 */
void
pmap_kcore_hdr(sh)
	struct sun2_kcore_hdr *sh;
{
	vm_offset_t va;
	u_char *cp, *ep;
	int saved_ctx;

	sh->segshift = SEGSHIFT;
	sh->pg_frame = PG_FRAME;
	sh->pg_valid = PG_VALID;

	/* Copy the kernel segmap (256 bytes). */
	va = KERNBASE;
	cp = sh->ksegmap;
	ep = cp + sizeof(sh->ksegmap);
	saved_ctx = get_context();
	set_context(KERNEL_CONTEXT);
	do {
		*cp = get_segmap(va);
		va += NBSG;
		cp++;
	} while (cp < ep);
	set_context(saved_ctx);
}

/*
 * Copy the pagemap RAM into the passed buffer (one page)
 * starting at OFF in the pagemap RAM.
 */
void
pmap_get_pagemap(pt, off)
	int *pt;
	int off;
{
	vm_offset_t va, va_end;
	int sme, sme_end;	/* SegMap Entry numbers */
	int saved_ctx;

	sme = (off / (NPAGSEG * sizeof(*pt)));	/* PMEG to start on */
	sme_end = sme + (NBPG / (NPAGSEG * sizeof(*pt))); /* where to stop */
	va_end = temp_seg_va + NBSG;

	saved_ctx = get_context();
	set_context(KERNEL_CONTEXT);
	do {
		set_segmap(temp_seg_va, sme);
		va = temp_seg_va;
		do {
			*pt++ = get_pte(va);
			va += NBPG;
		} while (va < va_end);
		sme++;
	} while (sme < sme_end);
	set_segmap(temp_seg_va, SEGINV);
	set_context(saved_ctx);
}


/*
 * Helper functions for changing unloaded PMEGs
 */

static int
get_pte_pmeg(int pmeg_num, int page_num)
{
	vm_offset_t va;
	int pte;
	int saved_ctx;

	CHECK_SPL();
	saved_ctx = get_context();
	set_context(KERNEL_CONTEXT);
	if (temp_seg_inuse)
		panic("get_pte_pmeg: temp_seg_inuse");
	temp_seg_inuse++;

	va = temp_seg_va;
	set_segmap(temp_seg_va, pmeg_num);
	va += NBPG*page_num;
	pte = get_pte(va);
	set_segmap(temp_seg_va, SEGINV);

	temp_seg_inuse--;
	set_context(saved_ctx);
	return pte;
}

static void
set_pte_pmeg(int pmeg_num, int page_num, int pte)
{
	vm_offset_t va;
	int saved_ctx;

	CHECK_SPL();
	saved_ctx = get_context();
	set_context(KERNEL_CONTEXT);
	if (temp_seg_inuse)
		panic("set_pte_pmeg: temp_seg_inuse");
	temp_seg_inuse++;

	/* We never access data in temp_seg_va so no need to flush. */
	va = temp_seg_va;
	set_segmap(temp_seg_va, pmeg_num);
	va += NBPG*page_num;
	set_pte(va, pte);
	set_segmap(temp_seg_va, SEGINV);

	temp_seg_inuse--;
	set_context(saved_ctx);
}

/*
 *	Routine:        pmap_procwr
 * 
 *	Function:
 *		Synchronize caches corresponding to [addr, addr+len) in p.
 */   
void
pmap_procwr(p, va, len)
	struct proc	*p;
	vaddr_t		va;
	size_t		len;
{
}


#ifdef	PMAP_DEBUG
/* Things to call from the debugger. */

void
pmap_print(pmap)
	pmap_t pmap;
{
	db_printf(" pm_ctxnum=%d\n", pmap->pm_ctxnum);
	db_printf(" pm_version=0x%x\n", pmap->pm_version);
	db_printf(" pm_segmap=%p\n", pmap->pm_segmap);
}

void
pmeg_print(pmegp)
	pmeg_t pmegp;
{
	db_printf("link_next=%p  link_prev=%p\n",
	    pmegp->pmeg_link.tqe_next,
	    pmegp->pmeg_link.tqe_prev);
	db_printf("index=0x%x owner=%p own_vers=0x%x\n",
	    pmegp->pmeg_index, pmegp->pmeg_owner, pmegp->pmeg_version);
	db_printf("va=0x%lx wired=0x%x reserved=0x%x vpgs=0x%x qstate=0x%x\n",
	    pmegp->pmeg_va, pmegp->pmeg_wired,
	    pmegp->pmeg_reserved, pmegp->pmeg_vpages,
	    pmegp->pmeg_qstate);
}

void
pv_print(pa)
	vm_offset_t pa;
{
	pv_entry_t pv;
	int idx;

	if (!pv_initialized) {
		db_printf("no pv_flags_tbl\n");
		return;
	}
	idx = PA_PGNUM(pa);
	if (idx >= physmem) {
		db_printf("bad address\n");
		return;
	}
	db_printf("pa=0x%lx, flags=0x%x\n",
			  pa, pv_flags_tbl[idx]);

	pv = pv_head_tbl[idx];
	while (pv) {
		db_printf(" pv_entry %p pmap %p va 0x%lx next %p\n",
			   pv, pv->pv_pmap, pv->pv_va, pv->pv_next);
		pv = pv->pv_next;
	}
}
#endif	/* PMAP_DEBUG */

/*
 * Local Variables:
 * tab-width: 4
 * End:
 */
