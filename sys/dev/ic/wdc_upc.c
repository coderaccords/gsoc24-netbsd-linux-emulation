/* $NetBSD: wdc_upc.c,v 1.21 2005/12/11 12:21:29 christos Exp $ */
/*-
 * Copyright (c) 2000 Ben Harris
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
/* This file is part of NetBSD/arm26 -- a port of NetBSD to ARM2/3 machines. */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: wdc_upc.c,v 1.21 2005/12/11 12:21:29 christos Exp $");

#include <sys/param.h>
#include <sys/device.h>
#include <sys/malloc.h>
#include <sys/systm.h>

#include <machine/bus.h>
#include <machine/intr.h>

#include <dev/ata/atavar.h> /* XXX needed by wdcvar.h */

#include <dev/ic/upcvar.h>
#include <dev/ic/wdcreg.h>
#include <dev/ic/wdcvar.h>

static int wdc_upc_match(struct device *, struct cfdata *, void *);
static void wdc_upc_attach(struct device *, struct device *, void *);

struct wdc_upc_softc {
	struct wdc_softc sc_wdc;
	struct ata_channel *sc_chanlist[1];
	struct ata_channel sc_channel;
	struct ata_queue sc_chqueue;
	struct wdc_regs sc_wdc_regs;
};

CFATTACH_DECL(wdc_upc, sizeof(struct wdc_upc_softc),
    wdc_upc_match, wdc_upc_attach, NULL, NULL);

static int
wdc_upc_match(struct device *parent, struct cfdata *cf, void *aux)
{

	/* upc_submatch does it all anyway */
	return 1;
}

static void
wdc_upc_attach(struct device *parent, struct device *self, void *aux)
{
	struct wdc_upc_softc *sc = (struct wdc_upc_softc *)self;
	struct wdc_regs *wdr;
	struct upc_attach_args *ua = aux;
	int i;

	sc->sc_wdc.regs = wdr = &sc->sc_wdc_regs;

	sc->sc_wdc.sc_atac.atac_cap = ATAC_CAP_DATA16;
	sc->sc_wdc.sc_atac.atac_pio_cap = 1; /* XXX ??? */
	sc->sc_wdc.sc_atac.atac_dma_cap = 0;
	sc->sc_wdc.sc_atac.atac_udma_cap = 0;
	sc->sc_wdc.sc_atac.atac_nchannels = 1;
	sc->sc_chanlist[0] = &sc->sc_channel;
	sc->sc_wdc.sc_atac.atac_channels = sc->sc_chanlist;
	wdr->cmd_iot = ua->ua_iot;
	wdr->cmd_baseioh = ua->ua_ioh;
	wdr->ctl_iot = ua->ua_iot;
	wdr->ctl_ioh = ua->ua_ioh2;
	sc->sc_channel.ch_channel = 0;
	sc->sc_channel.ch_atac = &sc->sc_wdc.sc_atac;
	sc->sc_channel.ch_queue = &sc->sc_chqueue;
	for (i = 0; i < WDC_NREG; i++) {
		if (bus_space_subregion(ua->ua_iot, ua->ua_ioh, i,
		    i == 0 ? 4 : 1, &wdr->cmd_iohs[i]) != 0) {
			aprint_error("%s: can't subregion I/O space\n",
			    sc->sc_wdc.sc_atac.atac_dev.dv_xname);
			return;
		}
	}
	wdc_init_shadow_regs(&sc->sc_channel);

	upc_intr_establish(ua->ua_irqhandle, IPL_BIO, wdcintr,
			   &sc->sc_channel);

	aprint_normal("\n");
	aprint_naive("\n");

	wdcattach(&sc->sc_channel);
}
