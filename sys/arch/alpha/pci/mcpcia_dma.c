/* $NetBSD: mcpcia_dma.c,v 1.1 1998/04/15 00:50:14 mjacob Exp $ */

/*-
 * Copyright (c) 1997, 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe of the Numerical Aerospace Simulation Facility,
 * NASA Ames Research Center. This code happens to have been written
 * by Matthew Jacob for NASA/Ames.
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
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPEMCPCIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>			/* RCS ID & Copyright macro defns */

__KERNEL_RCSID(0, "$NetBSD: mcpcia_dma.c,v 1.1 1998/04/15 00:50:14 mjacob Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <sys/malloc.h>
#include <vm/vm.h>

#define _ALPHA_BUS_DMA_PRIVATE
#include <machine/bus.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <alpha/pci/mcpciareg.h>
#include <alpha/pci/mcpciavar.h>
#include <alpha/pci/pci_kn300.h>

bus_dma_tag_t mcpcia_dma_get_tag __P((bus_dma_tag_t, alpha_bus_t));

int	mcpcia_bus_dmamap_create_sgmap __P((bus_dma_tag_t, bus_size_t, int,
	    bus_size_t, bus_size_t, int, bus_dmamap_t *));

void	mcpcia_bus_dmamap_destroy_sgmap __P((bus_dma_tag_t, bus_dmamap_t));

int	mcpcia_bus_dmamap_load_direct __P((bus_dma_tag_t, bus_dmamap_t, void *,
	    bus_size_t, struct proc *, int));

int	mcpcia_bus_dmamap_load_sgmap __P((bus_dma_tag_t, bus_dmamap_t, void *,
	    bus_size_t, struct proc *, int));

int	mcpcia_bus_dmamap_load_mbuf_direct __P((bus_dma_tag_t, bus_dmamap_t,
	    struct mbuf *, int));

int	mcpcia_bus_dmamap_load_mbuf_sgmap __P((bus_dma_tag_t, bus_dmamap_t,
	    struct mbuf *, int));

int	mcpcia_bus_dmamap_load_uio_direct __P((bus_dma_tag_t, bus_dmamap_t,
	    struct uio *, int));
int	mcpcia_bus_dmamap_load_uio_sgmap __P((bus_dma_tag_t, bus_dmamap_t,
	    struct uio *, int));

int	mcpcia_bus_dmamap_load_raw_direct __P((bus_dma_tag_t, bus_dmamap_t,
	    bus_dma_segment_t *, int, bus_size_t, int));
int	mcpcia_bus_dmamap_load_raw_sgmap __P((bus_dma_tag_t, bus_dmamap_t,
	    bus_dma_segment_t *, int, bus_size_t, int));

void	mcpcia_bus_dmamap_unload_sgmap __P((bus_dma_tag_t, bus_dmamap_t));

#define	MCPCIA_DIRECT_MAPPED_BASE	0x80000000
#define	MCPCIA_SG_MAPPED_BASE		(8 << 20)

#define	MCPCIA_SGTLB_INVALIDATE(mcp)	\
	alpha_mb(),	\
	REGVAL(MCPCIA_SG_TBIA(mcp)) = 0xdeadbeef, \
	alpha_mb()

void
mcpcia_dma_init(ccp)
	struct mcpcia_config *ccp;
{
	bus_dma_tag_t t;

	/*
	 * Initialize the DMA tag used for direct-mapped DMA.
	 */
	t = &ccp->cc_dmat_direct;
	t->_cookie = ccp;
	t->_get_tag = mcpcia_dma_get_tag;
	t->_dmamap_create = _bus_dmamap_create;
	t->_dmamap_destroy = _bus_dmamap_destroy;
	t->_dmamap_load = mcpcia_bus_dmamap_load_direct;
	t->_dmamap_load_mbuf = mcpcia_bus_dmamap_load_mbuf_direct;
	t->_dmamap_load_uio = mcpcia_bus_dmamap_load_uio_direct;
	t->_dmamap_load_raw = mcpcia_bus_dmamap_load_raw_direct;
	t->_dmamap_unload = _bus_dmamap_unload;
	t->_dmamap_sync = _bus_dmamap_sync;

	t->_dmamem_alloc = _bus_dmamem_alloc;
	t->_dmamem_free = _bus_dmamem_free;
	t->_dmamem_map = _bus_dmamem_map;
	t->_dmamem_unmap = _bus_dmamem_unmap;
	t->_dmamem_mmap = _bus_dmamem_mmap;

	/*
	 * Initialize the DMA tag used for sgmap-mapped DMA.
	 */
	t = &ccp->cc_dmat_sgmap;
	t->_cookie = ccp;
	t->_get_tag = mcpcia_dma_get_tag;
	t->_dmamap_create = mcpcia_bus_dmamap_create_sgmap;
	t->_dmamap_destroy = mcpcia_bus_dmamap_destroy_sgmap;
	t->_dmamap_load = mcpcia_bus_dmamap_load_sgmap;
	t->_dmamap_load_mbuf = mcpcia_bus_dmamap_load_mbuf_sgmap;
	t->_dmamap_load_uio = mcpcia_bus_dmamap_load_uio_sgmap;
	t->_dmamap_load_raw = mcpcia_bus_dmamap_load_raw_sgmap;
	t->_dmamap_unload = mcpcia_bus_dmamap_unload_sgmap;
	t->_dmamap_sync = _bus_dmamap_sync;

	t->_dmamem_alloc = _bus_dmamem_alloc;
	t->_dmamem_free = _bus_dmamem_free;
	t->_dmamem_map = _bus_dmamem_map;
	t->_dmamem_unmap = _bus_dmamem_unmap;
	t->_dmamem_mmap = _bus_dmamem_mmap;

	/*
	 * The SRM console is supposed to set up an 8MB direct mapped window
	 * at 8MB (window 0) and a 1MB direct mapped window at 1MB. Useless.
	 *
	 * The DU && OpenVMS convention is to have an 8MB S/G window
	 * at 8MB for window 0, a 1GB direct mapped window at 2GB
	 * for window 1, a 1 GB S/G window at window 2 and a 512 MB
	 * S/G window at window 3. This seems overkill.
	 *
	 * We'll just go with the 8MB S/G window and one 1GB direct window.
	 */

	/*
	 * Initialize the SGMAP.
	 *
	 * Must align page table to its size.
	 */
	alpha_sgmap_init(t, &ccp->cc_sgmap, "mcpcia_sgmap",
	    MCPCIA_SG_MAPPED_BASE, 0, (8 << 20),
	    sizeof(u_int64_t), NULL, 8 << 10);

	if (ccp->cc_sgmap.aps_ptpa & (1024-1)) {
		panic("mcpcia_dma_init: bad page table address %x",
			ccp->cc_sgmap.aps_ptpa);
	}

	/*
	 * Disable windows first.
	 */

	REGVAL(MCPCIA_W0_BASE(ccp->cc_sc)) = 0;
	REGVAL(MCPCIA_W1_BASE(ccp->cc_sc)) = 0;
	REGVAL(MCPCIA_W2_BASE(ccp->cc_sc)) = 0;
	REGVAL(MCPCIA_W3_BASE(ccp->cc_sc)) = 0;
	REGVAL(MCPCIA_T0_BASE(ccp->cc_sc)) = 0;
	REGVAL(MCPCIA_T1_BASE(ccp->cc_sc)) = 0;
	REGVAL(MCPCIA_T2_BASE(ccp->cc_sc)) = 0;
	REGVAL(MCPCIA_T3_BASE(ccp->cc_sc)) = 0;
	alpha_mb();

	/*
	 * Set up window 0 as an 8MB SGMAP-mapped window starting at 8MB.
	 */
	REGVAL(MCPCIA_W0_MASK(ccp->cc_sc)) = MCPCIA_WMASK_8M;
	REGVAL(MCPCIA_T0_BASE(ccp->cc_sc)) =
		ccp->cc_sgmap.aps_ptpa >> MCPCIA_TBASEX_SHIFT;
	REGVAL(MCPCIA_W0_BASE(ccp->cc_sc)) =
		MCPCIA_WBASE_EN | MCPCIA_WBASE_SG | MCPCIA_SG_MAPPED_BASE;
	alpha_mb();

	MCPCIA_SGTLB_INVALIDATE(ccp->cc_sc);

	/*
	 * Set up window 1 as a 1 GB Direct-mapped window starting at 2GB.
	 */

	REGVAL(MCPCIA_W0_MASK(ccp->cc_sc)) = MCPCIA_WMASK_1G;
	REGVAL(MCPCIA_T0_BASE(ccp->cc_sc)) = 0;
	REGVAL(MCPCIA_W0_BASE(ccp->cc_sc)) =
		MCPCIA_DIRECT_MAPPED_BASE | MCPCIA_WBASE_EN;
	alpha_mb();

	/* XXX XXX BEGIN XXX XXX */
	{							/* XXX */
		extern vm_offset_t alpha_XXX_dmamap_or;		/* XXX */
		alpha_XXX_dmamap_or = MCPCIA_DIRECT_MAPPED_BASE;/* XXX */
	}							/* XXX */
	/* XXX XXX END XXX XXX */
}

/*
 * Return the bus dma tag to be used for the specified bus type.
 * INTERNAL USE ONLY!
 */
bus_dma_tag_t
mcpcia_dma_get_tag(t, bustype)
	bus_dma_tag_t t;
	alpha_bus_t bustype;
{
	struct mcpcia_config *ccp = t->_cookie;
	extern int physmem;

	switch (bustype) {
	case ALPHA_BUS_PCI:
	case ALPHA_BUS_EISA:
		/*
		 * As best as I can tell from the DU source, you can't
		 * do DIRECT MAPPED and S/G at the same time.
		 */
		return (&ccp->cc_dmat_direct);

	case ALPHA_BUS_ISA:
		/*
		 * ISA doesn't have enough address bits to use
		 * the direct-mapped DMA window, so we must use
		 * SGMAPs.
		 */
		return (&ccp->cc_dmat_sgmap);

	default:
		panic("mcpcia_dma_get_tag: shouldn't be here, really...");
	}
}

/*
 * Create a MCPCIA SGMAP-mapped DMA map.
 */
int
mcpcia_bus_dmamap_create_sgmap(t, size, nsegments, maxsegsz, boundary,
    flags, dmamp)
	bus_dma_tag_t t;
	bus_size_t size;
	int nsegments;
	bus_size_t maxsegsz;
	bus_size_t boundary;
	int flags;
	bus_dmamap_t *dmamp;
{
	struct mcpcia_config *ccp = t->_cookie;
	bus_dmamap_t map;
	int error;

	error = _bus_dmamap_create(t, size, nsegments, maxsegsz,
	    boundary, flags, dmamp);
	if (error)
		return (error);

	map = *dmamp;

	if (flags & BUS_DMA_ALLOCNOW) {
		error = alpha_sgmap_alloc(map, round_page(size),
		    &ccp->cc_sgmap, flags);
		if (error)
			mcpcia_bus_dmamap_destroy_sgmap(t, map);
	}

	return (error);
}

/*
 * Destroy a MCPCIA SGMAP-mapped DMA map.
 */
void
mcpcia_bus_dmamap_destroy_sgmap(t, map)
	bus_dma_tag_t t;
	bus_dmamap_t map;
{
	struct mcpcia_config *ccp = t->_cookie;

	if (map->_dm_flags & DMAMAP_HAS_SGMAP)
		alpha_sgmap_free(map, &ccp->cc_sgmap);

	_bus_dmamap_destroy(t, map);
}

/*
 * Load a MCPCIA direct-mapped DMA map with a linear buffer.
 */
int
mcpcia_bus_dmamap_load_direct(t, map, buf, buflen, p, flags)
	bus_dma_tag_t t;
	bus_dmamap_t map;
	void *buf;
	bus_size_t buflen;
	struct proc *p;
	int flags;
{
	return (_bus_dmamap_load_direct_common(t, map, buf, buflen, p,
	    flags, MCPCIA_DIRECT_MAPPED_BASE));
}

/*
 * Load a MCPCIA SGMAP-mapped DMA map with a linear buffer.
 */
int
mcpcia_bus_dmamap_load_sgmap(t, map, buf, buflen, p, flags)
	bus_dma_tag_t t;
	bus_dmamap_t map;
	void *buf;
	bus_size_t buflen;
	struct proc *p;
	int flags;
{
	int error;
	struct mcpcia_config *ccp = t->_cookie;

	error = pci_sgmap_pte64_load(t, map, buf, buflen, p,
		flags, &ccp->cc_sgmap);
	if (error == 0)
		MCPCIA_SGTLB_INVALIDATE(ccp->cc_sc);
	return (error);
}

/*
 * Load a MCPCIA direct-mapped DMA map with an mbuf chain.
 */
int
mcpcia_bus_dmamap_load_mbuf_direct(t, map, m, flags)
	bus_dma_tag_t t;
	bus_dmamap_t map;
	struct mbuf *m;
	int flags;
{
	return (_bus_dmamap_load_mbuf_direct_common(t, map, m,
	    flags, MCPCIA_DIRECT_MAPPED_BASE));
}

/*
 * Load a MCPCIA SGMAP-mapped DMA map with an mbuf chain.
 */
int
mcpcia_bus_dmamap_load_mbuf_sgmap(t, map, m, flags)
	bus_dma_tag_t t;
	bus_dmamap_t map;
	struct mbuf *m;
	int flags;
{
	int error;
	struct mcpcia_config *ccp = t->_cookie;

	error = pci_sgmap_pte64_load_mbuf(t, map, m, flags, &ccp->cc_sgmap);
	if (error == 0)
		MCPCIA_SGTLB_INVALIDATE(ccp->cc_sc);
	return (error);
}

/*
 * Load a MCPCIA direct-mapped DMA map with a uio.
 */
int
mcpcia_bus_dmamap_load_uio_direct(t, map, uio, flags)
	bus_dma_tag_t t;
	bus_dmamap_t map;
	struct uio *uio;
	int flags;
{
	return (_bus_dmamap_load_uio_direct_common(t, map, uio,
	    flags, MCPCIA_DIRECT_MAPPED_BASE));
}

/*
 * Load a MCPCIA SGMAP-mapped DMA map with a uio.
 */
int
mcpcia_bus_dmamap_load_uio_sgmap(t, map, uio, flags)
	bus_dma_tag_t t;
	bus_dmamap_t map;
	struct uio *uio;
	int flags;
{
	int error;
	struct mcpcia_config *ccp = t->_cookie;

	error = pci_sgmap_pte64_load_uio(t, map, uio, flags, &ccp->cc_sgmap);
	if (error == 0)
		MCPCIA_SGTLB_INVALIDATE(ccp->cc_sc);
	return (error);
}

/*
 * Load a MCPCIA direct-mapped DMA map with raw memory.
 */
int
mcpcia_bus_dmamap_load_raw_direct(t, map, segs, nsegs, size, flags)
	bus_dma_tag_t t;
	bus_dmamap_t map;
	bus_dma_segment_t *segs;
	int nsegs;
	bus_size_t size;
	int flags;
{
	return (_bus_dmamap_load_raw_direct_common(t, map, segs, nsegs,
	    size, flags, MCPCIA_DIRECT_MAPPED_BASE));
}

/*
 * Load a MCPCIA SGMAP-mapped DMA map with raw memory.
 */
int
mcpcia_bus_dmamap_load_raw_sgmap(t, map, segs, nsegs, size, flags)
	bus_dma_tag_t t;
	bus_dmamap_t map;
	bus_dma_segment_t *segs;
	int nsegs;
	bus_size_t size;
	int flags;
{
	int error;
	struct mcpcia_config *ccp = t->_cookie;

	error = pci_sgmap_pte64_load_raw(t, map, segs, nsegs,
		size, flags, &ccp->cc_sgmap);
	if (error == 0)
		MCPCIA_SGTLB_INVALIDATE(ccp->cc_sc);
	return (error);
}

/*
 * Unload a MCPCIA DMA map.
 */
void
mcpcia_bus_dmamap_unload_sgmap(t, map)
	bus_dma_tag_t t;
	bus_dmamap_t map;
{
	struct mcpcia_config *ccp = t->_cookie;

	/*
	 * Invalidate any SGMAP page table entries used by this mapping.
	 */
	pci_sgmap_pte64_unload(t, map, &ccp->cc_sgmap);
	MCPCIA_SGTLB_INVALIDATE(ccp->cc_sc);

	/*
	 * Do the generic bits of the unload.
	 */
	_bus_dmamap_unload(t, map);
}
