/*	$NetBSD: if_sq.c,v 1.21 2004/10/30 18:08:35 thorpej Exp $	*/

/*
 * Copyright (c) 2001 Rafal K. Boni
 * Copyright (c) 1998, 1999, 2000 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * Portions of this code are derived from software contributed to The
 * NetBSD Foundation by Jason R. Thorpe of the Numerical Aerospace
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
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: if_sq.c,v 1.21 2004/10/30 18:08:35 thorpej Exp $");

#include "bpfilter.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/callout.h>
#include <sys/mbuf.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/errno.h>
#include <sys/syslog.h>

#include <uvm/uvm_extern.h>

#include <machine/endian.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_media.h>
#include <net/if_ether.h>

#if NBPFILTER > 0
#include <net/bpf.h>
#endif

#include <machine/bus.h>
#include <machine/intr.h>

#include <dev/ic/seeq8003reg.h>

#include <sgimips/hpc/sqvar.h>
#include <sgimips/hpc/hpcvar.h>
#include <sgimips/hpc/hpcreg.h>

#include <dev/arcbios/arcbios.h>
#include <dev/arcbios/arcbiosvar.h>

#define static

/*
 * Short TODO list:
 *	(1) Do counters for bad-RX packets.
 *	(2) Allow multi-segment transmits, instead of copying to a single,
 *	    contiguous mbuf.
 *	(3) Verify sq_stop() turns off enough stuff; I was still getting
 *	    seeq interrupts after sq_stop().
 *	(4) Implement EDLC modes: especially packet auto-pad and simplex
 *	    mode.
 *	(5) Should the driver filter out its own transmissions in non-EDLC
 *	    mode?
 *	(6) Multicast support -- multicast filter, address management, ...
 *	(7) Deal with RB0 (recv buffer overflow) on reception.  Will need
 *	    to figure out if RB0 is read-only as stated in one spot in the
 *	    HPC spec or read-write (ie, is the 'write a one to clear it')
 *	    the correct thing?
 */

#if defined(SQ_DEBUG)
 int sq_debug = 0;
 #define SQ_DPRINTF(x) if (sq_debug) printf x
#else
 #define SQ_DPRINTF(x)
#endif

static int	sq_match(struct device *, struct cfdata *, void *);
static void	sq_attach(struct device *, struct device *, void *);
static int	sq_init(struct ifnet *);
static void	sq_start(struct ifnet *);
static void	sq_stop(struct ifnet *, int);
static void	sq_watchdog(struct ifnet *);
static int	sq_ioctl(struct ifnet *, u_long, caddr_t);

static void	sq_set_filter(struct sq_softc *);
static int	sq_intr(void *);
static int	sq_rxintr(struct sq_softc *);
static int	sq_txintr(struct sq_softc *);
static void	sq_reset(struct sq_softc *);
static int 	sq_add_rxbuf(struct sq_softc *, int);
static void 	sq_dump_buffer(u_int32_t addr, u_int32_t len);

static void	enaddr_aton(const char*, u_int8_t*);

/* Actions */
#define SQ_RESET		1
#define SQ_ADD_TO_DMA		2
#define SQ_START_DMA		3
#define SQ_DONE_DMA		4
#define SQ_RESTART_DMA		5
#define SQ_TXINTR_ENTER		6
#define SQ_TXINTR_EXIT		7
#define SQ_TXINTR_BUSY		8

struct sq_action_trace {
	int action;
	int bufno;
	int status;
	int freebuf;
};

#define SQ_TRACEBUF_SIZE	100
int sq_trace_idx = 0;
struct sq_action_trace sq_trace[SQ_TRACEBUF_SIZE];

void sq_trace_dump(struct sq_softc* sc);

#define SQ_TRACE(act, buf, stat, free) do {				\
	sq_trace[sq_trace_idx].action = (act);				\
	sq_trace[sq_trace_idx].bufno = (buf);				\
	sq_trace[sq_trace_idx].status = (stat);				\
	sq_trace[sq_trace_idx].freebuf = (free);			\
	if (++sq_trace_idx == SQ_TRACEBUF_SIZE) {			\
		memset(&sq_trace, 0, sizeof(sq_trace));			\
		sq_trace_idx = 0;					\
	}								\
} while (0)

CFATTACH_DECL(sq, sizeof(struct sq_softc),
    sq_match, sq_attach, NULL, NULL);

#define        ETHER_PAD_LEN (ETHER_MIN_LEN - ETHER_CRC_LEN)

static int
sq_match(struct device *parent, struct cfdata *cf, void *aux)
{
	struct hpc_attach_args *ha = aux;

	if (strcmp(ha->ha_name, cf->cf_name) == 0)
		return (1);

	return (0);
}

static void
sq_attach(struct device *parent, struct device *self, void *aux)
{
	int i, err;
	char* macaddr;
	struct sq_softc *sc = (void *)self;
	struct hpc_attach_args *haa = aux;
	struct ifnet *ifp = &sc->sc_ethercom.ec_if;

	sc->sc_hpct = haa->ha_st;
	sc->hpc_regs = haa->hpc_regs;      /* HPC register definitions */

	if ((err = bus_space_subregion(haa->ha_st, haa->ha_sh,
				       haa->ha_dmaoff,
				       sc->hpc_regs->enet_regs_size,
				       &sc->sc_hpch)) != 0) {
		printf(": unable to map HPC DMA registers, error = %d\n", err);
		goto fail_0;
	}

	sc->sc_regt = haa->ha_st;
	if ((err = bus_space_subregion(haa->ha_st, haa->ha_sh,
				       haa->ha_devoff,
				       sc->hpc_regs->enet_devregs_size,
				       &sc->sc_regh)) != 0) {
		printf(": unable to map Seeq registers, error = %d\n", err);
		goto fail_0;
	}

	sc->sc_dmat = haa->ha_dmat;

	if ((err = bus_dmamem_alloc(sc->sc_dmat, sizeof(struct sq_control),
				    PAGE_SIZE, PAGE_SIZE, &sc->sc_cdseg,
				    1, &sc->sc_ncdseg, BUS_DMA_NOWAIT)) != 0) {
		printf(": unable to allocate control data, error = %d\n", err);
		goto fail_0;
	}

	if ((err = bus_dmamem_map(sc->sc_dmat, &sc->sc_cdseg, sc->sc_ncdseg,
				  sizeof(struct sq_control),
				  (caddr_t *)&sc->sc_control,
				  BUS_DMA_NOWAIT | BUS_DMA_COHERENT)) != 0) {
		printf(": unable to map control data, error = %d\n", err);
		goto fail_1;
	}

	if ((err = bus_dmamap_create(sc->sc_dmat, sizeof(struct sq_control),
				     1, sizeof(struct sq_control), PAGE_SIZE,
				     BUS_DMA_NOWAIT, &sc->sc_cdmap)) != 0) {
		printf(": unable to create DMA map for control data, error "
			"= %d\n", err);
		goto fail_2;
	}

	if ((err = bus_dmamap_load(sc->sc_dmat, sc->sc_cdmap, sc->sc_control,
				   sizeof(struct sq_control),
				   NULL, BUS_DMA_NOWAIT)) != 0) {
		printf(": unable to load DMA map for control data, error "
			"= %d\n", err);
		goto fail_3;
	}

	memset(sc->sc_control, 0, sizeof(struct sq_control));

	/* Create transmit buffer DMA maps */
	for (i = 0; i < SQ_NTXDESC; i++) {
	    if ((err = bus_dmamap_create(sc->sc_dmat, MCLBYTES, 1, MCLBYTES,
					 0, BUS_DMA_NOWAIT,
					 &sc->sc_txmap[i])) != 0) {
		    printf(": unable to create tx DMA map %d, error = %d\n",
			   i, err);
		    goto fail_4;
	    }
	}

	/* Create receive buffer DMA maps */
	for (i = 0; i < SQ_NRXDESC; i++) {
	    if ((err = bus_dmamap_create(sc->sc_dmat, MCLBYTES, 1, MCLBYTES,
					 0, BUS_DMA_NOWAIT,
					 &sc->sc_rxmap[i])) != 0) {
		    printf(": unable to create rx DMA map %d, error = %d\n",
			   i, err);
		    goto fail_5;
	    }
	}

	/* Pre-allocate the receive buffers.  */
	for (i = 0; i < SQ_NRXDESC; i++) {
		if ((err = sq_add_rxbuf(sc, i)) != 0) {
			printf(": unable to allocate or map rx buffer %d\n,"
			       " error = %d\n", i, err);
			goto fail_6;
		}
	}

	if ((macaddr = ARCBIOS->GetEnvironmentVariable("eaddr")) == NULL) {
		printf(": unable to get MAC address!\n");
		goto fail_6;
	}

	evcnt_attach_dynamic(&sc->sq_intrcnt, EVCNT_TYPE_INTR, NULL,
					      self->dv_xname, "intr");

	if ((cpu_intr_establish(haa->ha_irq, IPL_NET, sq_intr, sc)) == NULL) {
		printf(": unable to establish interrupt!\n");
		goto fail_6;
	}

	/* Reset the chip to a known state. */
	sq_reset(sc);

	/*
	 * Determine if we're an 8003 or 80c03 by setting the first
	 * MAC address register to non-zero, and then reading it back.
	 * If it's zero, we have an 80c03, because we will have read
	 * the TxCollLSB register.
	 */
	bus_space_write_1(sc->sc_regt, sc->sc_regh, SEEQ_TXCOLLS0, 0xa5);
	if (bus_space_read_1(sc->sc_regt, sc->sc_regh, SEEQ_TXCOLLS0) == 0)
		sc->sc_type = SQ_TYPE_80C03;
	else
		sc->sc_type = SQ_TYPE_8003;
	bus_space_write_1(sc->sc_regt, sc->sc_regh, SEEQ_TXCOLLS0, 0x00);

	printf(": SGI Seeq %s\n",
	    sc->sc_type == SQ_TYPE_80C03 ? "80c03" : "8003");

	enaddr_aton(macaddr, sc->sc_enaddr);

	printf("%s: Ethernet address %s\n", sc->sc_dev.dv_xname,
					   ether_sprintf(sc->sc_enaddr));

	strcpy(ifp->if_xname, sc->sc_dev.dv_xname);
	ifp->if_softc = sc;
	ifp->if_mtu = ETHERMTU;
	ifp->if_init = sq_init;
	ifp->if_stop = sq_stop;
	ifp->if_start = sq_start;
	ifp->if_ioctl = sq_ioctl;
	ifp->if_watchdog = sq_watchdog;
	ifp->if_flags = IFF_BROADCAST | IFF_NOTRAILERS | IFF_MULTICAST;
	IFQ_SET_READY(&ifp->if_snd);

	if_attach(ifp);
	ether_ifattach(ifp, sc->sc_enaddr);

	memset(&sq_trace, 0, sizeof(sq_trace));
	/* Done! */
	return;

	/*
	 * Free any resources we've allocated during the failed attach
	 * attempt.  Do this in reverse order and fall through.
	 */
fail_6:
	for (i = 0; i < SQ_NRXDESC; i++) {
		if (sc->sc_rxmbuf[i] != NULL) {
			bus_dmamap_unload(sc->sc_dmat, sc->sc_rxmap[i]);
			m_freem(sc->sc_rxmbuf[i]);
		}
	}
fail_5:
	for (i = 0; i < SQ_NRXDESC; i++) {
	    if (sc->sc_rxmap[i] != NULL)
		bus_dmamap_destroy(sc->sc_dmat, sc->sc_rxmap[i]);
	}
fail_4:
	for (i = 0; i < SQ_NTXDESC; i++) {
	    if (sc->sc_txmap[i] !=  NULL)
		bus_dmamap_destroy(sc->sc_dmat, sc->sc_txmap[i]);
	}
	bus_dmamap_unload(sc->sc_dmat, sc->sc_cdmap);
fail_3:
	bus_dmamap_destroy(sc->sc_dmat, sc->sc_cdmap);
fail_2:
	bus_dmamem_unmap(sc->sc_dmat, (caddr_t) sc->sc_control,
				      sizeof(struct sq_control));
fail_1:
	bus_dmamem_free(sc->sc_dmat, &sc->sc_cdseg, sc->sc_ncdseg);
fail_0:
	return;
}

/* Set up data to get the interface up and running. */
int
sq_init(struct ifnet *ifp)
{
	int i;
	u_int32_t reg;
	struct sq_softc *sc = ifp->if_softc;

	/* Cancel any in-progress I/O */
	sq_stop(ifp, 0);

	sc->sc_nextrx = 0;

	sc->sc_nfreetx = SQ_NTXDESC;
	sc->sc_nexttx = sc->sc_prevtx = 0;

	SQ_TRACE(SQ_RESET, 0, 0, sc->sc_nfreetx);

	/* Set into 8003 mode, bank 0 to program ethernet address */
	bus_space_write_1(sc->sc_regt, sc->sc_regh, SEEQ_TXCMD, TXCMD_BANK0);

	/* Now write the address */
	for (i = 0; i < ETHER_ADDR_LEN; i++)
		bus_space_write_1(sc->sc_regt, sc->sc_regh, i,
		    sc->sc_enaddr[i]);

	sc->sc_rxcmd = RXCMD_IE_CRC |
		       RXCMD_IE_DRIB |
		       RXCMD_IE_SHORT |
		       RXCMD_IE_END |
		       RXCMD_IE_GOOD;

	/*
	 * Set the receive filter -- this will add some bits to the
	 * prototype RXCMD register.  Do this before setting the
	 * transmit config register, since we might need to switch
	 * banks.
	 */
	sq_set_filter(sc);

	/* Set up Seeq transmit command register */
	bus_space_write_1(sc->sc_regt, sc->sc_regh, SEEQ_TXCMD,
						    TXCMD_IE_UFLOW |
						    TXCMD_IE_COLL |
						    TXCMD_IE_16COLL |
						    TXCMD_IE_GOOD);

	/* Now write the receive command register. */
	bus_space_write_1(sc->sc_regt, sc->sc_regh, SEEQ_RXCMD, sc->sc_rxcmd);

	/* Set up HPC ethernet DMA config */
	if (sc->hpc_regs->revision == 3) {
		reg = bus_space_read_4(sc->sc_hpct, sc->sc_hpch,
				sc->hpc_regs->enetr_dmacfg);
		bus_space_write_4(sc->sc_hpct, sc->sc_hpch,
				sc->hpc_regs->enetr_dmacfg,
			    	reg | ENETR_DMACFG_FIX_RXDC |
				ENETR_DMACFG_FIX_INTR |
				ENETR_DMACFG_FIX_EOP);
	}

	/* Pass the start of the receive ring to the HPC */
	bus_space_write_4(sc->sc_hpct, sc->sc_hpch, sc->hpc_regs->enetr_ndbp,
						    SQ_CDRXADDR(sc, 0));

	/* And turn on the HPC ethernet receive channel */
	bus_space_write_4(sc->sc_hpct, sc->sc_hpch, sc->hpc_regs->enetr_ctl,
				       sc->hpc_regs->enetr_ctl_active);

	ifp->if_flags |= IFF_RUNNING;
	ifp->if_flags &= ~IFF_OACTIVE;

	return 0;
}

static void
sq_set_filter(struct sq_softc *sc)
{
	struct ethercom *ec = &sc->sc_ethercom;
	struct ifnet *ifp = &sc->sc_ethercom.ec_if;
	struct ether_multi *enm;
	struct ether_multistep step;

	/*
	 * Check for promiscuous mode.  Also implies
	 * all-multicast.
	 */
	if (ifp->if_flags & IFF_PROMISC) {
		sc->sc_rxcmd |= RXCMD_REC_ALL;
		ifp->if_flags |= IFF_ALLMULTI;
		return;
	}

	/*
	 * The 8003 has no hash table.  If we have any multicast
	 * addresses on the list, enable reception of all multicast
	 * frames.
	 *
	 * XXX The 80c03 has a hash table.  We should use it.
	 */

	ETHER_FIRST_MULTI(step, ec, enm);

	if (enm == NULL) {
		sc->sc_rxcmd &= ~RXCMD_REC_MASK;
		sc->sc_rxcmd |= RXCMD_REC_BROAD;

		ifp->if_flags &= ~IFF_ALLMULTI;
		return;
	}

	sc->sc_rxcmd |= RXCMD_REC_MULTI;
	ifp->if_flags |= IFF_ALLMULTI;
}

int
sq_ioctl(struct ifnet *ifp, u_long cmd, caddr_t data)
{
	int s, error = 0;

	s = splnet();

	error = ether_ioctl(ifp, cmd, data);
	if (error == ENETRESET) {
		/*
		 * Multicast list has changed; set the hardware filter
		 * accordingly.
		 */
		if (ifp->if_flags & IFF_RUNNING)
			error = sq_init(ifp);
		else
			error = 0;
	}

	splx(s);
	return (error);
}

void
sq_start(struct ifnet *ifp)
{
	struct sq_softc *sc = ifp->if_softc;
	u_int32_t status;
	struct mbuf *m0, *m;
	bus_dmamap_t dmamap;
	int err, totlen, nexttx, firsttx, lasttx = -1, ofree, seg;

	if ((ifp->if_flags & (IFF_RUNNING|IFF_OACTIVE)) != IFF_RUNNING)
		return;

	/*
	 * Remember the previous number of free descriptors and
	 * the first descriptor we'll use.
	 */
	ofree = sc->sc_nfreetx;
	firsttx = sc->sc_nexttx;

	/*
	 * Loop through the send queue, setting up transmit descriptors
	 * until we drain the queue, or use up all available transmit
	 * descriptors.
	 */
	while (sc->sc_nfreetx != 0) {
		/*
		 * Grab a packet off the queue.
		 */
		IFQ_POLL(&ifp->if_snd, m0);
		if (m0 == NULL)
			break;
		m = NULL;

		dmamap = sc->sc_txmap[sc->sc_nexttx];

		/*
		 * Load the DMA map.  If this fails, the packet either
		 * didn't fit in the alloted number of segments, or we were
		 * short on resources.  In this case, we'll copy and try
		 * again.
		 * Also copy it if we need to pad, so that we are sure there
		 * is room for the pad buffer.
		 * XXX the right way of doing this is to use a static buffer
		 * for padding and adding it to the transmit descriptor (see
		 * sys/dev/pci/if_tl.c for example). We can't do this here yet
		 * because we can't send packets with more than one fragment.
		 */
		if (m0->m_pkthdr.len < ETHER_PAD_LEN ||
		    bus_dmamap_load_mbuf(sc->sc_dmat, dmamap, m0,
						      BUS_DMA_NOWAIT) != 0) {
			MGETHDR(m, M_DONTWAIT, MT_DATA);
			if (m == NULL) {
				printf("%s: unable to allocate Tx mbuf\n",
				    sc->sc_dev.dv_xname);
				break;
			}
			if (m0->m_pkthdr.len > MHLEN) {
				MCLGET(m, M_DONTWAIT);
				if ((m->m_flags & M_EXT) == 0) {
					printf("%s: unable to allocate Tx "
					    "cluster\n", sc->sc_dev.dv_xname);
					m_freem(m);
					break;
				}
			}

			m_copydata(m0, 0, m0->m_pkthdr.len, mtod(m, caddr_t));
			if (m0->m_pkthdr.len < ETHER_PAD_LEN) {
				memset(mtod(m, char *) + m0->m_pkthdr.len, 0,
				    ETHER_PAD_LEN - m0->m_pkthdr.len);
				m->m_pkthdr.len = m->m_len = ETHER_PAD_LEN;
			} else
				m->m_pkthdr.len = m->m_len = m0->m_pkthdr.len;

			if ((err = bus_dmamap_load_mbuf(sc->sc_dmat, dmamap,
						m, BUS_DMA_NOWAIT)) != 0) {
				printf("%s: unable to load Tx buffer, "
				    "error = %d\n", sc->sc_dev.dv_xname, err);
				break;
			}
		}

		/*
		 * Ensure we have enough descriptors free to describe
		 * the packet.
		 */
		if (dmamap->dm_nsegs > sc->sc_nfreetx) {
			/*
			 * Not enough free descriptors to transmit this
			 * packet.  We haven't committed to anything yet,
			 * so just unload the DMA map, put the packet
			 * back on the queue, and punt.  Notify the upper
			 * layer that there are no more slots left.
			 *
			 * XXX We could allocate an mbuf and copy, but
			 * XXX it is worth it?
			 */
			ifp->if_flags |= IFF_OACTIVE;
			bus_dmamap_unload(sc->sc_dmat, dmamap);
			if (m != NULL)
				m_freem(m);
			break;
		}

		IFQ_DEQUEUE(&ifp->if_snd, m0);
#if NBPFILTER > 0
		/*
		 * Pass the packet to any BPF listeners.
		 */
		if (ifp->if_bpf)
			bpf_mtap(ifp->if_bpf, m0);
#endif /* NBPFILTER > 0 */
		if (m != NULL) {
			m_freem(m0);
			m0 = m;
		}

		/*
		 * WE ARE NOW COMMITTED TO TRANSMITTING THE PACKET.
		 */

		/* Sync the DMA map. */
		bus_dmamap_sync(sc->sc_dmat, dmamap, 0, dmamap->dm_mapsize,
		    BUS_DMASYNC_PREWRITE);

		/*
		 * Initialize the transmit descriptors.
		 */
		for (nexttx = sc->sc_nexttx, seg = 0, totlen = 0;
		     seg < dmamap->dm_nsegs;
		     seg++, nexttx = SQ_NEXTTX(nexttx)) {
			if (sc->hpc_regs->revision == 3) {	
				sc->sc_txdesc[nexttx].hpc3_hdd_bufptr =
					    dmamap->dm_segs[seg].ds_addr;
				sc->sc_txdesc[nexttx].hpc3_hdd_ctl =
					    dmamap->dm_segs[seg].ds_len;
			} else {
				sc->sc_txdesc[nexttx].hpc1_hdd_bufptr =
					    dmamap->dm_segs[seg].ds_addr;
				sc->sc_txdesc[nexttx].hpc1_hdd_ctl =
					    dmamap->dm_segs[seg].ds_len;
			}
			sc->sc_txdesc[nexttx].hdd_descptr=
					    SQ_CDTXADDR(sc, SQ_NEXTTX(nexttx));
			lasttx = nexttx;
			totlen += dmamap->dm_segs[seg].ds_len;
		}

		/* Last descriptor gets end-of-packet */
		KASSERT(lasttx != -1);
		if (sc->hpc_regs->revision == 3)
			sc->sc_txdesc[lasttx].hpc3_hdd_ctl |= HDD_CTL_EOPACKET;
		else
			sc->sc_txdesc[lasttx].hpc1_hdd_ctl |=
							HPC1_HDD_CTL_EOPACKET;

		SQ_DPRINTF(("%s: transmit %d-%d, len %d\n", sc->sc_dev.dv_xname,
						       sc->sc_nexttx, lasttx,
						       totlen));

		if (ifp->if_flags & IFF_DEBUG) {
			printf("     transmit chain:\n");
			for (seg = sc->sc_nexttx;; seg = SQ_NEXTTX(seg)) {
				printf("     descriptor %d:\n", seg);
				printf("       hdd_bufptr:      0x%08x\n",
					(sc->hpc_regs->revision == 3) ?
					    sc->sc_txdesc[seg].hpc3_hdd_bufptr :
					    sc->sc_txdesc[seg].hpc1_hdd_bufptr);
				printf("       hdd_ctl: 0x%08x\n",
					(sc->hpc_regs->revision == 3) ?
					    sc->sc_txdesc[seg].hpc3_hdd_ctl:
					    sc->sc_txdesc[seg].hpc1_hdd_ctl);
				printf("       hdd_descptr:      0x%08x\n",
					sc->sc_txdesc[seg].hdd_descptr);

				if (seg == lasttx)
					break;
			}
		}

		/* Sync the descriptors we're using. */
		SQ_CDTXSYNC(sc, sc->sc_nexttx, dmamap->dm_nsegs,
				BUS_DMASYNC_PREREAD|BUS_DMASYNC_PREWRITE);

		/* Store a pointer to the packet so we can free it later */
		sc->sc_txmbuf[sc->sc_nexttx] = m0;

		/* Advance the tx pointer. */
		sc->sc_nfreetx -= dmamap->dm_nsegs;
		sc->sc_nexttx = nexttx;

	}

	/* All transmit descriptors used up, let upper layers know */
	if (sc->sc_nfreetx == 0)
		ifp->if_flags |= IFF_OACTIVE;

	if (sc->sc_nfreetx != ofree) {
		SQ_DPRINTF(("%s: %d packets enqueued, first %d, INTR on %d\n",
			    sc->sc_dev.dv_xname, lasttx - firsttx + 1,
			    firsttx, lasttx));

		/*
		 * Cause a transmit interrupt to happen on the
		 * last packet we enqueued, mark it as the last
		 * descriptor.
		 *
		 * HDD_CTL_EOPACKET && HDD_CTL_INTR cause an 
		 * interrupt.
		 */
		KASSERT(lasttx != -1);
		if (sc->hpc_regs->revision == 3) {
			sc->sc_txdesc[lasttx].hpc3_hdd_ctl |= HDD_CTL_INTR |
							HDD_CTL_EOCHAIN;
		} else {
			sc->sc_txdesc[lasttx].hpc1_hdd_ctl |= HPC1_HDD_CTL_INTR;
			sc->sc_txdesc[lasttx].hpc1_hdd_bufptr |=
							HPC1_HDD_CTL_EOCHAIN;
		}

		SQ_CDTXSYNC(sc, lasttx, 1,
				BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);

		/*
		 * There is a potential race condition here if the HPC
		 * DMA channel is active and we try and either update
		 * the 'next descriptor' pointer in the HPC PIO space
		 * or the 'next descriptor' pointer in a previous desc-
		 * riptor.
		 *
		 * To avoid this, if the channel is active, we rely on
		 * the transmit interrupt routine noticing that there
		 * are more packets to send and restarting the HPC DMA
		 * engine, rather than mucking with the DMA state here.
		 */
		status = bus_space_read_4(sc->sc_hpct, sc->sc_hpch,
				sc->hpc_regs->enetx_ctl);

		if ((status & sc->hpc_regs->enetx_ctl_active) != 0) {
			SQ_TRACE(SQ_ADD_TO_DMA, firsttx, status,
			    sc->sc_nfreetx);

			/* NB: hpc3_hdd_ctl is also hpc1_hdd_bufptr */
			sc->sc_txdesc[SQ_PREVTX(firsttx)].hpc3_hdd_ctl &=
			    ~HDD_CTL_EOCHAIN;

			SQ_CDTXSYNC(sc, SQ_PREVTX(firsttx),  1,
			    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);
		} else {
			SQ_TRACE(SQ_START_DMA, firsttx, status, sc->sc_nfreetx);

			bus_space_write_4(sc->sc_hpct, sc->sc_hpch,
			    sc->hpc_regs->enetx_ndbp, SQ_CDTXADDR(sc, firsttx));

			if (sc->hpc_regs->revision != 3) {
				bus_space_write_4(sc->sc_hpct, sc->sc_hpch,
				  HPC1_ENETX_CFXBP, SQ_CDTXADDR(sc, firsttx));
				bus_space_write_4(sc->sc_hpct, sc->sc_hpch,
				  HPC1_ENETX_CBP, SQ_CDTXADDR(sc, firsttx));
			}

			/* Kick DMA channel into life */
			bus_space_write_4(sc->sc_hpct, sc->sc_hpch,
					  sc->hpc_regs->enetx_ctl,
					  sc->hpc_regs->enetx_ctl_active);
		}

		/* Set a watchdog timer in case the chip flakes out. */
		ifp->if_timer = 5;
	}
}

void
sq_stop(struct ifnet *ifp, int disable)
{
	int i;
	struct sq_softc *sc = ifp->if_softc;

	for (i =0; i < SQ_NTXDESC; i++) {
		if (sc->sc_txmbuf[i] != NULL) {
			bus_dmamap_unload(sc->sc_dmat, sc->sc_txmap[i]);
			m_freem(sc->sc_txmbuf[i]);
			sc->sc_txmbuf[i] = NULL;
		}
	}

	/* Clear Seeq transmit/receive command registers */
	bus_space_write_1(sc->sc_regt, sc->sc_regh, SEEQ_TXCMD, 0);
	bus_space_write_1(sc->sc_regt, sc->sc_regh, SEEQ_RXCMD, 0);

	sq_reset(sc);

	ifp->if_flags &= ~(IFF_RUNNING | IFF_OACTIVE);
	ifp->if_timer = 0;
}

/* Device timeout/watchdog routine. */
void
sq_watchdog(struct ifnet *ifp)
{
	u_int32_t status;
	struct sq_softc *sc = ifp->if_softc;

	status = bus_space_read_4(sc->sc_hpct, sc->sc_hpch,
				  sc->hpc_regs->enetx_ctl);
	log(LOG_ERR, "%s: device timeout (prev %d, next %d, free %d, "
		     "status %08x)\n", sc->sc_dev.dv_xname, sc->sc_prevtx,
				       sc->sc_nexttx, sc->sc_nfreetx, status);

	sq_trace_dump(sc);

	memset(&sq_trace, 0, sizeof(sq_trace));
	sq_trace_idx = 0;

	++ifp->if_oerrors;

	sq_init(ifp);
}

void sq_trace_dump(struct sq_softc* sc)
{
	int i;

	for(i = 0; i < sq_trace_idx; i++) {
		printf("%s: [%d] action %d, buf %d, free %d, status %08x\n",
			sc->sc_dev.dv_xname, i, sq_trace[i].action,
			sq_trace[i].bufno, sq_trace[i].freebuf,
			sq_trace[i].status);
	}
}

static int
sq_intr(void * arg)
{
	struct sq_softc *sc = arg;
	struct ifnet *ifp = &sc->sc_ethercom.ec_if;
	int handled = 0;
	u_int32_t stat;

	stat = bus_space_read_4(sc->sc_hpct, sc->sc_hpch,
				sc->hpc_regs->enetr_reset);

	if ((stat & 2) == 0) {
		printf("%s: Unexpected interrupt!\n", sc->sc_dev.dv_xname);
		return 0;
	}

	bus_space_write_4(sc->sc_hpct, sc->sc_hpch,
			  sc->hpc_regs->enetr_reset, (stat | 2));

	/*
	 * If the interface isn't running, the interrupt couldn't
	 * possibly have come from us.
	 */
	if ((ifp->if_flags & IFF_RUNNING) == 0)
		return 0;

	sc->sq_intrcnt.ev_count++;

	/* Always check for received packets */
	if (sq_rxintr(sc) != 0)
		handled++;

	/* Only handle transmit interrupts if we actually sent something */
	if (sc->sc_nfreetx < SQ_NTXDESC) {
		sq_txintr(sc);
		handled++;
	}

#if NRND > 0
	if (handled)
		rnd_add_uint32(&sc->rnd_source, stat);
#endif
	return (handled);
}

static int
sq_rxintr(struct sq_softc *sc)
{
	int count = 0;
	struct mbuf* m;
	int i, framelen;
	u_int8_t pktstat;
	u_int32_t status;
	u_int32_t ctl_reg;
	int new_end, orig_end;
	struct ifnet *ifp = &sc->sc_ethercom.ec_if;

	for(i = sc->sc_nextrx;; i = SQ_NEXTRX(i)) {
		SQ_CDRXSYNC(sc, i, BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);

		/* If this is a CPU-owned buffer, we're at the end of the list */
		if (sc->hpc_regs->revision == 3)
			ctl_reg = sc->sc_rxdesc[i].hpc3_hdd_ctl & HDD_CTL_OWN;
		else
			ctl_reg = sc->sc_rxdesc[i].hpc1_hdd_ctl &
							HPC1_HDD_CTL_OWN;

		if (ctl_reg) {
#if defined(SQ_DEBUG)
			u_int32_t reg;

			reg = bus_space_read_4(sc->sc_hpct, sc->sc_hpch,
			    sc->hpc_regs->enetr_ctl);
			SQ_DPRINTF(("%s: rxintr: done at %d (ctl %08x)\n",
			    sc->sc_dev.dv_xname, i, reg));
#endif
			break;
		}

		count++;

		m = sc->sc_rxmbuf[i];
		framelen = m->m_ext.ext_size - 3;
		if (sc->hpc_regs->revision == 3)
		    framelen -=
			HDD_CTL_BYTECNT(sc->sc_rxdesc[i].hpc3_hdd_ctl);
		else
		    framelen -=
			HPC1_HDD_CTL_BYTECNT(sc->sc_rxdesc[i].hpc1_hdd_ctl);

		/* Now sync the actual packet data */
		bus_dmamap_sync(sc->sc_dmat, sc->sc_rxmap[i], 0,
		    sc->sc_rxmap[i]->dm_mapsize, BUS_DMASYNC_POSTREAD);

		pktstat = *((u_int8_t*)m->m_data + framelen + 2);

		if ((pktstat & RXSTAT_GOOD) == 0) {
			ifp->if_ierrors++;

			if (pktstat & RXSTAT_OFLOW)
				printf("%s: receive FIFO overflow\n",
				    sc->sc_dev.dv_xname);

			bus_dmamap_sync(sc->sc_dmat, sc->sc_rxmap[i], 0,
			    sc->sc_rxmap[i]->dm_mapsize,
			    BUS_DMASYNC_PREREAD);
			SQ_INIT_RXDESC(sc, i);
			continue;
		}

		if (sq_add_rxbuf(sc, i) != 0) {
			ifp->if_ierrors++;
			bus_dmamap_sync(sc->sc_dmat, sc->sc_rxmap[i], 0,
			    sc->sc_rxmap[i]->dm_mapsize,
			    BUS_DMASYNC_PREREAD);
			SQ_INIT_RXDESC(sc, i);
			continue;
		}


		m->m_data += 2;
		m->m_pkthdr.rcvif = ifp;
		m->m_pkthdr.len = m->m_len = framelen;

		ifp->if_ipackets++;

		SQ_DPRINTF(("%s: sq_rxintr: buf %d len %d\n",
			    sc->sc_dev.dv_xname, i, framelen));

#if NBPFILTER > 0
		if (ifp->if_bpf)
			bpf_mtap(ifp->if_bpf, m);
#endif
		(*ifp->if_input)(ifp, m);
	}


	/* If anything happened, move ring start/end pointers to new spot */
	if (i != sc->sc_nextrx) {
		/* NB: hpc3_hdd_ctl is also hpc1_hdd_bufptr */

		new_end = SQ_PREVRX(i);
		sc->sc_rxdesc[new_end].hpc3_hdd_ctl |= HDD_CTL_EOCHAIN;
		SQ_CDRXSYNC(sc, new_end, BUS_DMASYNC_PREREAD |
		    BUS_DMASYNC_PREWRITE);

		orig_end = SQ_PREVRX(sc->sc_nextrx);
		sc->sc_rxdesc[orig_end].hpc3_hdd_ctl &= ~HDD_CTL_EOCHAIN;
		SQ_CDRXSYNC(sc, orig_end, BUS_DMASYNC_PREREAD |
		    BUS_DMASYNC_PREWRITE);

		sc->sc_nextrx = i;
	}

	status = bus_space_read_4(sc->sc_hpct, sc->sc_hpch,
				  sc->hpc_regs->enetr_ctl);

	/* If receive channel is stopped, restart it... */
	if ((status & sc->hpc_regs->enetr_ctl_active) == 0) {
		/* Pass the start of the receive ring to the HPC */
		bus_space_write_4(sc->sc_hpct, sc->sc_hpch,
		    sc->hpc_regs->enetr_ndbp, SQ_CDRXADDR(sc, sc->sc_nextrx));

		/* And turn on the HPC ethernet receive channel */
		bus_space_write_4(sc->sc_hpct, sc->sc_hpch,
		    sc->hpc_regs->enetr_ctl, sc->hpc_regs->enetr_ctl_active);
	}

	return count;
}

static int
sq_txintr(struct sq_softc *sc)
{
	int i;
	int shift = 0;
	u_int32_t status;
	u_int32_t hpc1_ready = 0;
	u_int32_t hpc3_not_ready = 1;
	struct ifnet *ifp = &sc->sc_ethercom.ec_if;

	if (sc->hpc_regs->revision != 3)
		shift = 16;

	status = bus_space_read_4(sc->sc_hpct, sc->sc_hpch,
				  sc->hpc_regs->enetx_ctl) >> shift;

	SQ_TRACE(SQ_TXINTR_ENTER, sc->sc_prevtx, status, sc->sc_nfreetx);

	if ((status & ( (sc->hpc_regs->enetx_ctl_active >> shift) | TXSTAT_GOOD)) == 0) {
/* XXX */ printf("txstat: %x\n", status);
		if (status & TXSTAT_COLL)
			ifp->if_collisions++;

		if (status & TXSTAT_UFLOW) {
			printf("%s: transmit underflow\n", sc->sc_dev.dv_xname);
			ifp->if_oerrors++;
		}

		if (status & TXSTAT_16COLL) {
			printf("%s: max collisions reached\n", sc->sc_dev.dv_xname);
			ifp->if_oerrors++;
			ifp->if_collisions += 16;
		}
	}

	i = sc->sc_prevtx;
	while (sc->sc_nfreetx < SQ_NTXDESC) {
		/*
		 * Check status first so we don't end up with a case of
		 * the buffer not being finished while the DMA channel
		 * has gone idle.
		 */
		status = bus_space_read_4(sc->sc_hpct, sc->sc_hpch,
					sc->hpc_regs->enetx_ctl) >> shift;

		SQ_CDTXSYNC(sc, i, sc->sc_txmap[i]->dm_nsegs,
				BUS_DMASYNC_POSTREAD|BUS_DMASYNC_POSTWRITE);

		/*
		 * If not yet transmitted, try and start DMA engine again.
		 * HPC3 tags transmitted descriptors with XMITDONE whereas
		 * HPC1 will not halt before sending through EOCHAIN.
		 */
		if (sc->hpc_regs->revision == 3) {
			hpc3_not_ready =
			    sc->sc_txdesc[i].hpc3_hdd_ctl & HDD_CTL_XMITDONE;
		} else {
			if (hpc1_ready)
				hpc1_ready++;
			else {
				if (sc->sc_txdesc[i].hpc1_hdd_ctl &
							HPC1_HDD_CTL_EOPACKET)
					hpc1_ready = 1;
			} 
		}
		
		if (hpc3_not_ready == 0 || hpc1_ready == 2) {
			if ((status & (sc->hpc_regs->enetx_ctl_active >> shift)) == 0) { // XXX
				SQ_TRACE(SQ_RESTART_DMA, i, status,
				    sc->sc_nfreetx);

				bus_space_write_4(sc->sc_hpct, sc->sc_hpch,
				  sc->hpc_regs->enetx_ndbp, SQ_CDTXADDR(sc, i));

				if (sc->hpc_regs->revision != 3) {
				  bus_space_write_4(sc->sc_hpct, sc->sc_hpch,
                                          HPC1_ENETX_CFXBP, SQ_CDTXADDR(sc, i));
                                  bus_space_write_4(sc->sc_hpct, sc->sc_hpch,
                                          HPC1_ENETX_CBP, SQ_CDTXADDR(sc, i));
				}

				/* Kick DMA channel into life */
				bus_space_write_4(sc->sc_hpct, sc->sc_hpch,
					  sc->hpc_regs->enetx_ctl,
					  sc->hpc_regs->enetx_ctl_active);

				/*
				 * Set a watchdog timer in case the chip
				 * flakes out.
				 */
				ifp->if_timer = 5;
			} else {
				SQ_TRACE(SQ_TXINTR_BUSY, i, status,
				    sc->sc_nfreetx);
			}
			break;
		}

		/* Sync the packet data, unload DMA map, free mbuf */
		bus_dmamap_sync(sc->sc_dmat, sc->sc_txmap[i], 0,
				sc->sc_txmap[i]->dm_mapsize,
				BUS_DMASYNC_POSTWRITE);
		bus_dmamap_unload(sc->sc_dmat, sc->sc_txmap[i]);
		m_freem(sc->sc_txmbuf[i]);
		sc->sc_txmbuf[i] = NULL;

		ifp->if_opackets++;
		sc->sc_nfreetx++;

		SQ_TRACE(SQ_DONE_DMA, i, status, sc->sc_nfreetx);
		i = SQ_NEXTTX(i);
	}

	/* prevtx now points to next xmit packet not yet finished */
	sc->sc_prevtx = i;

	/* If we have buffers free, let upper layers know */
	if (sc->sc_nfreetx > 0)
		ifp->if_flags &= ~IFF_OACTIVE;

	/* If all packets have left the coop, cancel watchdog */
	if (sc->sc_nfreetx == SQ_NTXDESC)
		ifp->if_timer = 0;

	SQ_TRACE(SQ_TXINTR_EXIT, sc->sc_prevtx, status, sc->sc_nfreetx);
	sq_start(ifp);

	return 1;
}


void
sq_reset(struct sq_softc *sc)
{
	/* Stop HPC dma channels */
	bus_space_write_4(sc->sc_hpct, sc->sc_hpch, sc->hpc_regs->enetr_ctl, 0);
	bus_space_write_4(sc->sc_hpct, sc->sc_hpch, sc->hpc_regs->enetx_ctl, 0);

	bus_space_write_4(sc->sc_hpct, sc->sc_hpch, sc->hpc_regs->enetr_reset, 3);
	delay(20);
	bus_space_write_4(sc->sc_hpct, sc->sc_hpch, sc->hpc_regs->enetr_reset, 0);
}

/* sq_add_rxbuf: Add a receive buffer to the indicated descriptor. */
int
sq_add_rxbuf(struct sq_softc *sc, int idx)
{
	int err;
	struct mbuf *m;

	MGETHDR(m, M_DONTWAIT, MT_DATA);
	if (m == NULL)
		return (ENOBUFS);

	MCLGET(m, M_DONTWAIT);
	if ((m->m_flags & M_EXT) == 0) {
		m_freem(m);
		return (ENOBUFS);
	}

	if (sc->sc_rxmbuf[idx] != NULL)
		bus_dmamap_unload(sc->sc_dmat, sc->sc_rxmap[idx]);

	sc->sc_rxmbuf[idx] = m;

	if ((err = bus_dmamap_load(sc->sc_dmat, sc->sc_rxmap[idx],
				   m->m_ext.ext_buf, m->m_ext.ext_size,
				   NULL, BUS_DMA_NOWAIT)) != 0) {
		printf("%s: can't load rx DMA map %d, error = %d\n",
		    sc->sc_dev.dv_xname, idx, err);
		panic("sq_add_rxbuf");	/* XXX */
	}

	bus_dmamap_sync(sc->sc_dmat, sc->sc_rxmap[idx], 0,
			sc->sc_rxmap[idx]->dm_mapsize, BUS_DMASYNC_PREREAD);

	SQ_INIT_RXDESC(sc, idx);

	return 0;
}

void
sq_dump_buffer(u_int32_t addr, u_int32_t len)
{
	u_int i;
	u_char* physaddr = (char*) MIPS_PHYS_TO_KSEG1((caddr_t)addr);

	if (len == 0)
		return;

	printf("%p: ", physaddr);

	for(i = 0; i < len; i++) {
		printf("%02x ", *(physaddr + i) & 0xff);
		if ((i % 16) ==  15 && i != len - 1)
		    printf("\n%p: ", physaddr + i);
	}

	printf("\n");
}


void
enaddr_aton(const char* str, u_int8_t* eaddr)
{
	int i;
	char c;

	for(i = 0; i < ETHER_ADDR_LEN; i++) {
		if (*str == ':')
			str++;

		c = *str++;
		if (isdigit(c)) {
			eaddr[i] = (c - '0');
		} else if (isxdigit(c)) {
			eaddr[i] = (toupper(c) + 10 - 'A');
		}

		c = *str++;
		if (isdigit(c)) {
			eaddr[i] = (eaddr[i] << 4) | (c - '0');
		} else if (isxdigit(c)) {
			eaddr[i] = (eaddr[i] << 4) | (toupper(c) + 10 - 'A');
		}
	}
}
