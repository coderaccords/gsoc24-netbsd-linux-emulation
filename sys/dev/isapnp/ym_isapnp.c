/*	$NetBSD: ym_isapnp.c,v 1.18 2005/12/11 12:22:16 christos Exp $ */

/*
 * Copyright (c) 1991-1993 Regents of the University of California.
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
 *	This product includes software developed by the Computer Systems
 *	Engineering Group at Lawrence Berkeley Laboratory.
 * 4. Neither the name of the University nor of the Laboratory may be used
 *    to endorse or promote products derived from this software without
 *    specific prior written permission.
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
 */

/*
 *  Driver for the Yamaha OPL3-SA3 chipset. This is found on many laptops
 *  and Pentium (II) motherboards.
 *
 *  Original code from OpenBSD.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: ym_isapnp.c,v 1.18 2005/12/11 12:22:16 christos Exp $");

#include "mpu_ym.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/errno.h>

#include <sys/audioio.h>
#include <dev/audio_if.h>

#include <dev/isa/isavar.h>
#include <dev/isa/isadmavar.h>

#include <dev/isapnp/isapnpreg.h>
#include <dev/isapnp/isapnpvar.h>
#include <dev/isapnp/isapnpdevs.h>

#include <dev/ic/ad1848reg.h>
#include <dev/isa/ad1848var.h>

#include <dev/ic/cs4231reg.h>
#include <dev/isa/cs4231var.h>

#include <dev/ic/opl3sa3reg.h>
#include <dev/isa/wssreg.h>
#include <dev/isa/ymvar.h>

int	ym_isapnp_match(struct device *, struct cfdata *, void *);
void	ym_isapnp_attach(struct device *, struct device *, void *);

CFATTACH_DECL(ym_isapnp, sizeof(struct ym_softc),
    ym_isapnp_match, ym_isapnp_attach, NULL, NULL);

/*
 * Probe / attach routines.
 */

/*
 * Probe for the Yamaha hardware.
 */
int
ym_isapnp_match(struct device *parent, struct cfdata *match, void *aux)
{
	int pri, variant;

	pri = isapnp_devmatch(aux, &isapnp_ym_devinfo, &variant);
	if (pri && variant > 0)
		pri = 0;
	return pri;
}

/*
 * Attach hardware to driver, attach hardware driver to audio
 * pseudo-device driver.
 */
void
ym_isapnp_attach(struct device *parent, struct device *self, void *aux)
{
	struct ym_softc *sc;
	struct ad1848_softc *ac;
	struct isapnp_attach_args *ipa;

	sc = (struct ym_softc *)self;
	ac = &sc->sc_ad1848.sc_ad1848;
	ipa = aux;
	printf("\n");

	if (isapnp_config(ipa->ipa_iot, ipa->ipa_memt, ipa)) {
		printf("%s: error in region allocation\n", self->dv_xname);
		return;
	}

	sc->sc_iot = ipa->ipa_iot;
	sc->sc_ic = ipa->ipa_ic;
	sc->sc_ioh = ipa->ipa_io[1].h;

	sc->ym_irq = ipa->ipa_irq[0].num;
	sc->ym_playdrq = ipa->ipa_drq[0].num;
	sc->ym_recdrq = ipa->ipa_drq[1].num;

	sc->sc_sb_ioh = ipa->ipa_io[0].h;
	sc->sc_opl_ioh = ipa->ipa_io[2].h;
#if NMPU_YM > 0
	sc->sc_mpu_ioh = ipa->ipa_io[3].h;
#endif
	sc->sc_controlioh = ipa->ipa_io[4].h;

	ac->sc_iot = sc->sc_iot;
	if (bus_space_subregion(sc->sc_iot, sc->sc_ioh, WSS_CODEC, AD1848_NPORT,
	    &ac->sc_ioh)) {
		printf("%s: bus_space_subregion failed\n", self->dv_xname);
		return;
	}
	ac->mode = 2;
	ac->MCE_bit = MODE_CHANGE_ENABLE;

	sc->sc_ad1848.sc_ic  = sc->sc_ic;

	printf("%s: %s %s", self->dv_xname, ipa->ipa_devident,
	    ipa->ipa_devclass);

	ym_attach(sc);
}
