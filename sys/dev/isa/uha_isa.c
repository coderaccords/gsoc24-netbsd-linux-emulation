/*	$NetBSD: uha_isa.c,v 1.18 1999/09/30 23:04:41 thorpej Exp $	*/

/*-
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Charles M. Hannum.
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

#include "opt_ddb.h"

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/user.h>

#include <machine/bus.h>
#include <machine/intr.h>

#include <dev/scsipi/scsi_all.h>
#include <dev/scsipi/scsipi_all.h>
#include <dev/scsipi/scsiconf.h>

#include <dev/isa/isavar.h>
#include <dev/isa/isadmavar.h>

#include <dev/ic/uhareg.h>
#include <dev/ic/uhavar.h>

#define	UHA_ISA_IOSIZE	16

int	uha_isa_probe __P((struct device *, struct cfdata *, void *));
void	uha_isa_attach __P((struct device *, struct device *, void *));

struct cfattach uha_isa_ca = {
	sizeof(struct uha_softc), uha_isa_probe, uha_isa_attach
};

#ifndef	DDB
#define Debugger() panic("should call debugger here (uha_isa.c)")
#endif /* ! DDB */

int	u14_find __P((bus_space_tag_t, bus_space_handle_t,
	    struct uha_probe_data *));
void	u14_start_mbox __P((struct uha_softc *, struct uha_mscp *));
int	u14_poll __P((struct uha_softc *, struct scsipi_xfer *, int));
int	u14_intr __P((void *));
void	u14_init __P((struct uha_softc *));

/*
 * Check the slots looking for a board we recognise
 * If we find one, note it's address (slot) and call
 * the actual probe routine to check it out.
 */
int
uha_isa_probe(parent, match, aux)
	struct device *parent;
	struct cfdata *match;
	void *aux;
{
	struct isa_attach_args *ia = aux;
	bus_space_tag_t iot = ia->ia_iot;
	bus_space_handle_t ioh;
	struct uha_probe_data upd;
	int rv;

	/* Disallow wildcarded i/o address. */
	if (ia->ia_iobase == ISACF_PORT_DEFAULT)
		return (0);

	if (bus_space_map(iot, ia->ia_iobase, UHA_ISA_IOSIZE, 0, &ioh))
		return (0);

	rv = u14_find(iot, ioh, &upd);

	bus_space_unmap(iot, ioh, UHA_ISA_IOSIZE);

	if (rv) {
		if (ia->ia_irq != -1 && ia->ia_irq != upd.sc_irq)
			return (0);
		if (ia->ia_drq != -1 && ia->ia_drq != upd.sc_drq)
			return (0);
		ia->ia_irq = upd.sc_irq;
		ia->ia_drq = upd.sc_drq;
		ia->ia_msize = 0;
		ia->ia_iosize = UHA_ISA_IOSIZE;
	}
	return (rv);
}

/*
 * Attach all the sub-devices we can find
 */
void
uha_isa_attach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct isa_attach_args *ia = aux;
	struct uha_softc *sc = (void *)self;
	bus_space_tag_t iot = ia->ia_iot;
	bus_dma_tag_t dmat = ia->ia_dmat;
	bus_space_handle_t ioh;
	struct uha_probe_data upd;
	isa_chipset_tag_t ic = ia->ia_ic;
	int error;

	printf("\n");

	if (bus_space_map(iot, ia->ia_iobase, UHA_ISA_IOSIZE, 0, &ioh)) {
		printf("%s: can't map i/o space\n", sc->sc_dev.dv_xname);
		return;
	}

	sc->sc_iot = iot;
	sc->sc_ioh = ioh;
	sc->sc_dmat = dmat;
	if (!u14_find(iot, ioh, &upd)) {
		printf("%s: u14_find failed\n", sc->sc_dev.dv_xname);
		return;
	}

	if (upd.sc_drq != -1) {
		sc->sc_dmaflags = 0;
		if ((error = isa_dmacascade(ic, upd.sc_drq)) != 0) {
			printf("%s: unable to cascade DRQ, error = %d\n",
			    sc->sc_dev.dv_xname, error);
			return;
		}
	} else {
		/*
		 * We have a VLB controller, and can do 32-bit DMA.
		 */
		sc->sc_dmaflags = ISABUS_DMA_32BIT;
	}

	sc->sc_ih = isa_intr_establish(ic, upd.sc_irq, IST_EDGE, IPL_BIO,
	    u14_intr, sc);
	if (sc->sc_ih == NULL) {
		printf("%s: couldn't establish interrupt\n",
		    sc->sc_dev.dv_xname);
		return;
	}

	/* Save function pointers for later use. */
	sc->start_mbox = u14_start_mbox;
	sc->poll = u14_poll;
	sc->init = u14_init;

	uha_attach(sc, &upd);
}

/*
 * Start the board, ready for normal operation
 */
int
u14_find(iot, ioh, sc)
	bus_space_tag_t iot;
	bus_space_handle_t ioh;
	struct uha_probe_data *sc;
{
	u_int16_t model, config;
	int irq, drq;
	int resetcount = 4000;	/* 4 secs? */

	model = (bus_space_read_1(iot, ioh, U14_ID + 0) << 8) |
		(bus_space_read_1(iot, ioh, U14_ID + 1) << 0);
	if ((model & 0xfff0) != 0x5640)
		return (0);

	config = (bus_space_read_1(iot, ioh, U14_CONFIG + 0) << 8) |
		 (bus_space_read_1(iot, ioh, U14_CONFIG + 1) << 0);

	switch (model & 0x000f) {
	case 0x0000:
		switch (config & U14_DMA_MASK) {
		case U14_DMA_CH5:
			drq = 5;
			break;
		case U14_DMA_CH6:
			drq = 6;
			break;
		case U14_DMA_CH7:
			drq = 7;
			break;
		default:
			printf("u14_find: illegal drq setting %x\n",
			    config & U14_DMA_MASK);
			return (0);
		}
		break;
	case 0x0001:
		/* This is a 34f, and doesn't need an ISA DMA channel. */
		drq = -1;
		break;
	default:
		printf("u14_find: unknown model %x\n", model);
		return (0);
	}

	switch (config & U14_IRQ_MASK) {
	case U14_IRQ10:
		irq = 10;
		break;
	case U14_IRQ11:
		irq = 11;
		break;
	case U14_IRQ14:
		irq = 14;
		break;
	case U14_IRQ15:
		irq = 15;
		break;
	default:
		printf("u14_find: illegal irq setting %x\n",
		    config & U14_IRQ_MASK);
		return (0);
	}

	bus_space_write_1(iot, ioh, U14_LINT, UHA_ASRST);

	while (--resetcount) {
		if (bus_space_read_1(iot, ioh, U14_LINT))
			break;
		delay(1000);	/* 1 mSec per loop */
	}
	if (!resetcount) {
		printf("u14_find: board timed out during reset\n");
		return (0);
	}

	/* if we want to fill in softc, do so now */
	if (sc) {
		sc->sc_irq = irq;
		sc->sc_drq = drq;
		sc->sc_scsi_dev = config & U14_HOSTID_MASK;
	}

	return (1);
}

/*
 * Function to send a command out through a mailbox
 */
void
u14_start_mbox(sc, mscp)
	struct uha_softc *sc;
	struct uha_mscp *mscp;
{
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;
	int spincount = 100000;	/* 1s should be enough */

	while (--spincount) {
		if ((bus_space_read_1(iot, ioh, U14_LINT) & U14_LDIP) == 0)
			break;
		delay(100);
	}
	if (!spincount) {
		printf("%s: uha_start_mbox, board not responding\n",
		    sc->sc_dev.dv_xname);
		Debugger();
	}

	bus_space_write_4(iot, ioh, U14_OGMPTR,
	    sc->sc_dmamap_mscp->dm_segs[0].ds_addr + UHA_MSCP_OFF(mscp));
	if (mscp->flags & MSCP_ABORT)
		bus_space_write_1(iot, ioh, U14_LINT, U14_ABORT);
	else
		bus_space_write_1(iot, ioh, U14_LINT, U14_OGMFULL);

	if ((mscp->xs->xs_control & XS_CTL_POLL) == 0)
		timeout(uha_timeout, mscp, (mscp->timeout * hz) / 1000);
}

/*
 * Function to poll for command completion when in poll mode.
 *
 *	wait = timeout in msec
 */
int
u14_poll(sc, xs, count)
	struct uha_softc *sc;
	struct scsipi_xfer *xs;
	int count;
{
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;

	while (count) {
		/*
		 * If we had interrupts enabled, would we
		 * have got an interrupt?
		 */
		if (bus_space_read_1(iot, ioh, U14_SINT) & U14_SDIP)
			u14_intr(sc);
		if (xs->xs_status & XS_STS_DONE)
			return (0);
		delay(1000);
		count--;
	}
	return (1);
}

/*
 * Catch an interrupt from the adaptor
 */
int
u14_intr(arg)
	void *arg;
{
	struct uha_softc *sc = arg;
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;
	struct uha_mscp *mscp;
	u_char uhastat;
	u_long mboxval;

#ifdef	UHADEBUG
	printf("%s: uhaintr ", sc->sc_dev.dv_xname);
#endif /*UHADEBUG */

	if ((bus_space_read_1(iot, ioh, U14_SINT) & U14_SDIP) == 0)
		return (0);

	for (;;) {
		/*
		 * First get all the information and then
		 * acknowledge the interrupt
		 */
		uhastat = bus_space_read_1(iot, ioh, U14_SINT);
		mboxval = bus_space_read_4(iot, ioh, U14_ICMPTR);
		/* XXX Send an ABORT_ACK instead? */
		bus_space_write_1(iot, ioh, U14_SINT, U14_ICM_ACK);

#ifdef	UHADEBUG
		printf("status = 0x%x ", uhastat);
#endif /*UHADEBUG*/

		/*
		 * Process the completed operation
		 */
		mscp = uha_mscp_phys_kv(sc, mboxval);
		if (!mscp) {
			printf("%s: BAD MSCP RETURNED!\n",
			    sc->sc_dev.dv_xname);
			continue;	/* whatever it was, it'll timeout */
		}

		untimeout(uha_timeout, mscp);
		uha_done(sc, mscp);

		if ((bus_space_read_1(iot, ioh, U14_SINT) & U14_SDIP) == 0)
			return (1);
	}
}

void
u14_init(sc)
	struct uha_softc *sc;
{
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;

	/* make sure interrupts are enabled */
#ifdef UHADEBUG
	printf("u14_init: lmask=%02x, smask=%02x\n",
	    bus_space_read_1(iot, ioh, U14_LMASK),
	    bus_space_read_1(iot, ioh, U14_SMASK));
#endif
	bus_space_write_1(iot, ioh, U14_LMASK, 0xd1);	/* XXX */
	bus_space_write_1(iot, ioh, U14_SMASK, 0x91);	/* XXX */
}
