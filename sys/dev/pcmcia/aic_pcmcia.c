/*	$NetBSD: aic_pcmcia.c,v 1.26 2004/08/10 06:23:50 mycroft Exp $	*/

/*
 * Copyright (c) 1997 Marc Horowitz.  All rights reserved.
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
 *	This product includes software developed by Marc Horowitz.
 * 4. The name of the author may not be used to endorse or promote products
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
__KERNEL_RCSID(0, "$NetBSD: aic_pcmcia.c,v 1.26 2004/08/10 06:23:50 mycroft Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/select.h>
#include <sys/device.h>

#include <machine/cpu.h>
#include <machine/bus.h>
#include <machine/intr.h>

#include <dev/scsipi/scsipi_all.h>
#include <dev/scsipi/scsipiconf.h>
#include <dev/scsipi/scsi_all.h>

#include <dev/ic/aic6360var.h>

#include <dev/pcmcia/pcmciareg.h>
#include <dev/pcmcia/pcmciavar.h>
#include <dev/pcmcia/pcmciadevs.h>

struct aic_pcmcia_softc {
	struct aic_softc sc_aic;		/* real "aic" softc */

	/* PCMCIA-specific goo. */
	struct pcmcia_function *sc_pf;		/* our PCMCIA function */
	void *sc_ih;				/* interrupt handler */

	int sc_state;
#define AIC_PCMCIA_ATTACH1	1
#define	AIC_PCMCIA_ATTACH2	2
#define	AIC_PCMCIA_ATTACHED	3
};

int	aic_pcmcia_match __P((struct device *, struct cfdata *, void *));
int	aic_pcmcia_validate_config __P((struct pcmcia_config_entry *));
void	aic_pcmcia_attach __P((struct device *, struct device *, void *));
int	aic_pcmcia_detach __P((struct device *, int));
int	aic_pcmcia_enable __P((struct device *, int));

CFATTACH_DECL(aic_pcmcia, sizeof(struct aic_pcmcia_softc),
    aic_pcmcia_match, aic_pcmcia_attach, aic_pcmcia_detach, aic_activate);

const struct pcmcia_product aic_pcmcia_products[] = {
	{ "",	PCMCIA_VENDOR_ADAPTEC,
	  PCMCIA_PRODUCT_ADAPTEC_APA1460,	0 },

	{ "",	PCMCIA_VENDOR_ADAPTEC,
	  PCMCIA_PRODUCT_ADAPTEC_APA1460A,	0 },

	{ "",	PCMCIA_VENDOR_NEWMEDIA,
	  PCMCIA_PRODUCT_NEWMEDIA_BUSTOASTER,	0 },

	{ NULL }
};

int
aic_pcmcia_match(parent, match, aux)
	struct device *parent;
	struct cfdata *match;
	void *aux;
{
	struct pcmcia_attach_args *pa = aux;

	if (pcmcia_product_lookup(pa, aic_pcmcia_products,
	    sizeof aic_pcmcia_products[0], NULL) != NULL)
		return (1);
	return (0);
}

int
aic_pcmcia_validate_config(cfe)
	struct pcmcia_config_entry *cfe;
{
	if (cfe->iftype != PCMCIA_IFTYPE_IO ||
	    cfe->num_memspace != 0 ||
	    cfe->num_iospace != 1)
		return (EINVAL);
	return (0);
}

void
aic_pcmcia_attach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct aic_pcmcia_softc *psc = (void *)self;
	struct aic_softc *sc = &psc->sc_aic;
	struct pcmcia_attach_args *pa = aux;
	struct pcmcia_config_entry *cfe;
	struct pcmcia_function *pf = pa->pf;
	int error;

	aprint_normal("\n");
	psc->sc_pf = pf;

	error = pcmcia_function_configure(pf, aic_pcmcia_validate_config);
	if (error) {
		aprint_error("%s: configure failed, error=%d\n", self->dv_xname,
		    error);
		return;
	}

	cfe = pf->cfe;
	sc->sc_iot = cfe->iospace[0].handle.iot;
	sc->sc_ioh = cfe->iospace[0].handle.ioh;

	error = aic_pcmcia_enable(self, 1);
	if (error) {
		aprint_error("%s: enable failed, error=%d\n", self->dv_xname,
		    error);
		goto fail;
	}

	if (!aic_find(sc->sc_iot, sc->sc_ioh)) {
		aprint_error("%s: unable to detect chip!\n", self->dv_xname);
		goto fail2;
	}

	/* We can enable and disable the controller. */
	sc->sc_adapter.adapt_enable = aic_pcmcia_enable;

	psc->sc_state = AIC_PCMCIA_ATTACH1;
	aicattach(sc);
	if (psc->sc_state == AIC_PCMCIA_ATTACH1)
		aic_pcmcia_enable(self, 0);
	psc->sc_state = AIC_PCMCIA_ATTACHED;
	return;

fail2:
	aic_pcmcia_enable(self, 0);
fail:
	pcmcia_function_unconfigure(pf);
}

int
aic_pcmcia_detach(self, flags)
	struct device *self;
	int flags;
{
	struct aic_pcmcia_softc *sc = (void *)self;
	int error;

	if (sc->sc_state != AIC_PCMCIA_ATTACHED)
		return (0);

	error = aic_detach(self, flags);
	if (error)
		return (error);

	pcmcia_function_unconfigure(sc->sc_pf);

	return (0);
}

int
aic_pcmcia_enable(self, onoff)
	struct device *self;
	int onoff;
{
	struct aic_pcmcia_softc *sc = (void *)self;

	if (onoff) {
		/*
		 * If attach is in progress, we already have the device
		 * powered up.
		 */
		if (sc->sc_state == AIC_PCMCIA_ATTACH1) {
			sc->sc_state = AIC_PCMCIA_ATTACH2;
		} else {
			/* Establish the interrupt handler. */
			sc->sc_ih = pcmcia_intr_establish(sc->sc_pf, IPL_BIO,
			    aicintr, &sc->sc_aic);
			if (sc->sc_ih == NULL) {
				printf("%s: couldn't establish interrupt handler\n",
				    sc->sc_aic.sc_dev.dv_xname);
				return (EIO);
			}

			if (pcmcia_function_enable(sc->sc_pf)) {
				printf("%s: couldn't enable PCMCIA function\n",
				    sc->sc_aic.sc_dev.dv_xname);
				pcmcia_intr_disestablish(sc->sc_pf, sc->sc_ih);
				return (EIO);
			}

			/* Initialize only chip.  */
			aic_init(&sc->sc_aic, 0);
		}
	} else {
		pcmcia_function_disable(sc->sc_pf);
		pcmcia_intr_disestablish(sc->sc_pf, sc->sc_ih);
		sc->sc_ih = 0;
	}

	return (0);
}
