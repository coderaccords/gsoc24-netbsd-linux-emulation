/*	$NetBSD: if_le_isa.c,v 1.26 2001/05/30 11:46:33 mrg Exp $	*/

/*-
 * Copyright (c) 1997, 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Charles M. Hannum and by Jason R. Thorpe of the Numerical Aerospace
 * Simulation Facility, NASA Ames Research Center.
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

/*-
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Ralph Campbell and Rick Macklem.
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
 *	@(#)if_le.c	8.2 (Berkeley) 11/16/93
 */

#include "opt_inet.h"
#include "bpfilter.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/syslog.h>
#include <sys/socket.h>
#include <sys/device.h>

#include <uvm/uvm_extern.h>

#include <net/if.h>
#include <net/if_ether.h>
#include <net/if_media.h>

#ifdef INET
#include <netinet/in.h>
#include <netinet/if_inarp.h>
#endif

#include <machine/cpu.h>
#include <machine/intr.h>
#include <machine/bus.h>

#include <dev/isa/isareg.h>
#include <dev/isa/isavar.h>
#include <dev/isa/isadmavar.h>

#include <dev/ic/lancereg.h>
#include <dev/ic/lancevar.h>
#include <dev/ic/am7990reg.h>
#include <dev/ic/am7990var.h>

#include <dev/isa/if_levar.h>

int ne2100_isa_probe __P((struct device *, struct cfdata *, void *));
int bicc_isa_probe __P((struct device *, struct cfdata *, void *));
void le_dummyattach __P((struct device *, struct device *, void *));
int le_dummyprobe __P((struct device *, struct cfdata *, void *));
void le_ne2100_attach __P((struct device *, struct device *, void *));
void le_bicc_attach __P((struct device *, struct device *, void *));

struct cfattach nele_ca = {
	sizeof(struct device), ne2100_isa_probe, le_dummyattach
}, le_nele_ca = {
	sizeof(struct le_softc), le_dummyprobe, le_ne2100_attach
}, bicc_ca = {
	sizeof(struct device), bicc_isa_probe, le_dummyattach
}, le_bicc_ca = {
	sizeof(struct le_softc), le_dummyprobe, le_bicc_attach
};

struct le_isa_params {
	char *name;
	int iosize, rap, rdp;
	int macstart, macstride;
} ne2100_params = {
	"NE2100",
	24, NE2100_RAP, NE2100_RDP,
	0, 1
}, bicc_params = {
	"BICC Isolan",
	16, BICC_RAP, BICC_RDP,
	0, 2
};

int lance_isa_probe __P((struct isa_attach_args *, struct le_isa_params *));
void le_isa_attach __P((struct device *, struct le_softc *,
			struct isa_attach_args *, struct le_isa_params *));

int le_isa_intredge __P((void *));

#if defined(_KERNEL_OPT)
#include "opt_ddb.h"
#endif

#ifdef DDB
#define	integrate
#define hide
#else
#define	integrate	static __inline
#define hide		static
#endif

hide void le_isa_wrcsr __P((struct lance_softc *, u_int16_t, u_int16_t));
hide u_int16_t le_isa_rdcsr __P((struct lance_softc *, u_int16_t));  

#define	LE_ISA_MEMSIZE	16384

hide void
le_isa_wrcsr(sc, port, val)
	struct lance_softc *sc;
	u_int16_t port, val;
{
	struct le_softc *lesc = (struct le_softc *)sc;
	bus_space_tag_t iot = lesc->sc_iot;
	bus_space_handle_t ioh = lesc->sc_ioh;

	bus_space_write_2(iot, ioh, lesc->sc_rap, port);
	bus_space_write_2(iot, ioh, lesc->sc_rdp, val);
}

hide u_int16_t
le_isa_rdcsr(sc, port)
	struct lance_softc *sc;
	u_int16_t port;
{
	struct le_softc *lesc = (struct le_softc *)sc;
	bus_space_tag_t iot = lesc->sc_iot;
	bus_space_handle_t ioh = lesc->sc_ioh; 
	u_int16_t val;

	bus_space_write_2(iot, ioh, lesc->sc_rap, port);
	val = bus_space_read_2(iot, ioh, lesc->sc_rdp); 
	return (val);
}

int
ne2100_isa_probe(parent, match, aux)
	struct device *parent;
	struct cfdata *match;
	void *aux;
{
	return (lance_isa_probe(aux, &ne2100_params));
}

int
bicc_isa_probe(parent, match, aux)
	struct device *parent;
	struct cfdata *match;
	void *aux;
{
	return (lance_isa_probe(aux, &bicc_params));
}

/*
 * Determine which chip is present on the card.
 */
int
lance_isa_probe(ia, p)
	struct isa_attach_args *ia;
	struct le_isa_params *p;
{
	bus_space_tag_t iot = ia->ia_iot;
	bus_space_handle_t ioh;
	int rap, rdp;
	int rv = 0;

	/* Disallow wildcarded i/o address. */
	if (ia->ia_iobase == ISACF_PORT_DEFAULT)
		return (0);

	/* Map i/o space. */
	if (bus_space_map(iot, ia->ia_iobase, p->iosize, 0, &ioh))
		return (0);

	rap = p->rap;
	rdp = p->rdp;

	/* Stop the LANCE chip and put it in a known state. */
	bus_space_write_2(iot, ioh, rap, LE_CSR0);
	bus_space_write_2(iot, ioh, rdp, LE_C0_STOP);
	delay(100);

	bus_space_write_2(iot, ioh, rap, LE_CSR0);
	if (bus_space_read_2(iot, ioh, rdp) != LE_C0_STOP)
		goto bad;

	bus_space_write_2(iot, ioh, rap, LE_CSR3);
	bus_space_write_2(iot, ioh, rdp, 0);

	ia->ia_iosize = p->iosize;
	rv = 1;

bad:
	bus_space_unmap(iot, ioh, p->iosize);
	return (rv);
}

void
le_dummyattach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	printf("\n");

	config_found(self, aux, 0);
}

int
le_dummyprobe(parent, match, aux)
	struct device *parent;
	struct cfdata *match;
	void *aux;
{
	return (1);
}

void
le_ne2100_attach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	le_isa_attach(parent, (void *)self, aux, &ne2100_params);
}

void
le_bicc_attach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	le_isa_attach(parent, (void *)self, aux, &bicc_params);
}

void
le_isa_attach(parent, lesc, ia, p)
	struct device *parent;
	struct le_softc *lesc;
	struct isa_attach_args *ia;
	struct le_isa_params *p;
{
	struct lance_softc *sc = &lesc->sc_am7990.lsc;
	bus_space_tag_t iot = ia->ia_iot;
	bus_space_handle_t ioh;
	bus_dma_tag_t dmat = ia->ia_dmat;
	bus_dma_segment_t seg;
	int i, rseg, error;

	printf(": %s Ethernet\n", p->name);

	if (bus_space_map(iot, ia->ia_iobase, p->iosize, 0, &ioh))
		panic("%s: can't map io", sc->sc_dev.dv_xname);

	/*
	 * Extract the physical MAC address from the ROM.
	 */
	for (i = 0; i < sizeof(sc->sc_enaddr); i++)
		sc->sc_enaddr[i] =
		    bus_space_read_1(iot, ioh, p->macstart + i * p->macstride);

	lesc->sc_iot = iot;
	lesc->sc_ioh = ioh;
	lesc->sc_dmat = dmat;
	lesc->sc_rap = p->rap;
	lesc->sc_rdp = p->rdp;

	/*
	 * Allocate a DMA area for the card.
	 */
	if (bus_dmamem_alloc(dmat, LE_ISA_MEMSIZE, PAGE_SIZE, 0, &seg, 1,
			     &rseg, BUS_DMA_NOWAIT)) {
		printf("%s: couldn't allocate memory for card\n",
		       sc->sc_dev.dv_xname);
		return;
	}
	if (bus_dmamem_map(dmat, &seg, rseg, LE_ISA_MEMSIZE,
			   (caddr_t *)&sc->sc_mem,
			   BUS_DMA_NOWAIT|BUS_DMA_COHERENT)) {
		printf("%s: couldn't map memory for card\n",
		       sc->sc_dev.dv_xname);
		return;
	}

	/*
	 * Create and load the DMA map for the DMA area.
	 */
	if (bus_dmamap_create(dmat, LE_ISA_MEMSIZE, 1,
			LE_ISA_MEMSIZE, 0, BUS_DMA_NOWAIT, &lesc->sc_dmam)) {
		printf("%s: couldn't create DMA map\n",
		       sc->sc_dev.dv_xname);
		bus_dmamem_free(dmat, &seg, rseg);
		return;
	}
	if (bus_dmamap_load(dmat, lesc->sc_dmam,
			sc->sc_mem, LE_ISA_MEMSIZE, NULL, BUS_DMA_NOWAIT)) {
		printf("%s: coundn't load DMA map\n",
		       sc->sc_dev.dv_xname);
		bus_dmamem_free(dmat, &seg, rseg);
		return;
	}

	sc->sc_conf3 = 0;
	sc->sc_addr = lesc->sc_dmam->dm_segs[0].ds_addr;
	sc->sc_memsize = LE_ISA_MEMSIZE;

	sc->sc_copytodesc = lance_copytobuf_contig;
	sc->sc_copyfromdesc = lance_copyfrombuf_contig;
	sc->sc_copytobuf = lance_copytobuf_contig;
	sc->sc_copyfrombuf = lance_copyfrombuf_contig;
	sc->sc_zerobuf = lance_zerobuf_contig;

	sc->sc_rdcsr = le_isa_rdcsr;
	sc->sc_wrcsr = le_isa_wrcsr;
	sc->sc_hwinit = NULL;

	if (ia->ia_drq != DRQUNK) {
		if ((error = isa_dmacascade(ia->ia_ic, ia->ia_drq)) != 0) {
			printf("%s: unable to cascade DRQ, error = %d\n",
			    sc->sc_dev.dv_xname, error);
			return;
		}
	}

	lesc->sc_ih = isa_intr_establish(ia->ia_ic, ia->ia_irq, IST_EDGE,
	    IPL_NET, le_isa_intredge, sc);

	printf("%s", sc->sc_dev.dv_xname);
	am7990_config(&lesc->sc_am7990);
}

/*
 * Controller interrupt.
 */
int
le_isa_intredge(arg)
	void *arg;
{

	if (am7990_intr(arg) == 0)
		return (0);
	for (;;)
		if (am7990_intr(arg) == 0)
			return (1);
}
