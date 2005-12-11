/* $NetBSD: wss_acpi.c,v 1.14 2005/12/11 12:21:02 christos Exp $ */

/*
 * Copyright (c) 2002 Jared D. McNeill <jmcneill@invisible.ca>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: wss_acpi.c,v 1.14 2005/12/11 12:21:02 christos Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/ioctl.h>
#include <sys/syslog.h>
#include <sys/device.h>
#include <sys/proc.h>

#include <machine/bus.h>

#include <sys/audioio.h>
#include <dev/audio_if.h>

#include <dev/isa/isavar.h>
#include <dev/isa/isadmavar.h>

#include <dev/acpi/acpica.h>
#include <dev/acpi/acpireg.h>
#include <dev/acpi/acpivar.h>

#include <dev/isa/ad1848var.h>
#include <dev/isa/wssreg.h>
#include <dev/isa/wssvar.h>

static int	wss_acpi_match(struct device *, struct cfdata *, void *);
static void	wss_acpi_attach(struct device *, struct device *, void *);

CFATTACH_DECL(wss_acpi, sizeof(struct wss_softc), wss_acpi_match,
    wss_acpi_attach, NULL, NULL);

/*
 * Supported device IDs
 */

struct wss_acpi_hint {
	char idstr[8];
	int io_region_idx_ad1848;	/* which region index is the DAC?  */
	int io_region_idx_opl;		/* which region index is the OPL?  */
	int offset_ad1848;		/* offset from start of DAC region */
};

static struct wss_acpi_hint wss_acpi_hints[] = {
	{ "NMX2210", 1, 2, WSS_CODEC },
	{ "CSC0000", 0, 1, 0 },		/* Dell Latitude CPi */
	{ "CSC0100", 0, 1, 0 },		/* CS4610 with CS4236 codec */
	{ { 0 }, 0, 0, 0 }
};

static int wss_acpi_hints_index (const char *);

static int
wss_acpi_hints_index(idstr)
	const char *idstr;
{
	int idx = 0;

	while (wss_acpi_hints[idx].idstr[0] != 0) {
		if (!strcmp(wss_acpi_hints[idx].idstr, idstr))
			return idx;
		++idx;
	}

	return -1;
}

/*
 * wss_acpi_match: autoconf(9) match routine
 */
static int
wss_acpi_match(struct device *parent, struct cfdata *match, void *aux)
{
	struct acpi_attach_args *aa = aux;

	if (aa->aa_node->ad_type != ACPI_TYPE_DEVICE ||
	    wss_acpi_hints_index(aa->aa_node->ad_devinfo->HardwareId.Value) == -1)
		return 0;

	return 1;
}

/*
 * wss_acpi_attach: autoconf(9) attach routine
 */
static void
wss_acpi_attach(struct device *parent, struct device *self, void *aux)
{
	struct wss_softc *sc = (struct wss_softc *)self;
	struct acpi_attach_args *aa = aux;
	struct acpi_resources res;
	struct acpi_io *dspio, *oplio;
	struct acpi_irq *irq;
	struct acpi_drq *playdrq, *recdrq;
	struct audio_attach_args arg;
	ACPI_STATUS rv;
	struct wss_acpi_hint *wah;

	printf(": NeoMagic 256AV audio\n");

	wah = &wss_acpi_hints[
	    wss_acpi_hints_index(aa->aa_node->ad_devinfo->HardwareId.Value)];

	/* Parse our resources */
	rv = acpi_resource_parse(&sc->sc_ad1848.sc_ad1848.sc_dev,
	    aa->aa_node->ad_handle, "_CRS", &res,
	    &acpi_resource_parse_ops_default);
	if (ACPI_FAILURE(rv))
		return;

	/* Find and map our i/o registers */
	sc->sc_iot = aa->aa_iot;
	dspio = acpi_res_io(&res, wah->io_region_idx_ad1848);
	oplio = acpi_res_io(&res, wah->io_region_idx_opl);
	if (dspio == NULL || oplio == NULL) {
		printf("%s: unable to find i/o registers resource\n",
		    sc->sc_ad1848.sc_ad1848.sc_dev.dv_xname);
		goto out;
	}
	if (bus_space_map(sc->sc_iot, dspio->ar_base, dspio->ar_length,
	    0, &sc->sc_ioh) != 0) {
		printf("%s: unable to map i/o registers\n",
		    sc->sc_ad1848.sc_ad1848.sc_dev.dv_xname);
		goto out;
	}
	if (bus_space_map(sc->sc_iot, oplio->ar_base, oplio->ar_length,
	    0, &sc->sc_opl_ioh) != 0) {
		printf("%s: unable to map opl i/o registers\n",
		    sc->sc_ad1848.sc_ad1848.sc_dev.dv_xname);
		goto out;
	}

	sc->wss_ic = aa->aa_ic;

	/* Find our IRQ */
	irq = acpi_res_irq(&res, 0);
	if (irq == NULL) {
		printf("%s: unable to find irq resource\n",
		    sc->sc_ad1848.sc_ad1848.sc_dev.dv_xname);
		/* XXX bus_space_unmap */
		goto out;
	}
	sc->wss_irq = irq->ar_irq;

	/* Find our playback and record DRQs */
	playdrq = acpi_res_drq(&res, 0);
	recdrq = acpi_res_drq(&res, 1);
	if (playdrq == NULL || recdrq == NULL) {
		printf("%s: unable to find drq resources\n",
		    sc->sc_ad1848.sc_ad1848.sc_dev.dv_xname);
		/* XXX bus_space_unmap */
		goto out;
	}
	sc->wss_playdrq = playdrq->ar_drq;
	sc->wss_recdrq = recdrq->ar_drq;

	sc->sc_ad1848.sc_ad1848.sc_iot = sc->sc_iot;
	bus_space_subregion(sc->sc_iot, sc->sc_ioh, wah->offset_ad1848, 4,
	    &sc->sc_ad1848.sc_ad1848.sc_ioh);

	/* Look for the ad1848 */
	if (!ad1848_isa_probe(&sc->sc_ad1848)) {
		printf("%s: ad1848 probe failed\n",
		    sc->sc_ad1848.sc_ad1848.sc_dev.dv_xname);
		/* XXX cleanup */
		goto out;
	}

	printf("%s", sc->sc_ad1848.sc_ad1848.sc_dev.dv_xname);
	/* Attach our wss device */
	wssattach(sc);

	arg.type = AUDIODEV_TYPE_OPL;
	arg.hwif = 0;
	arg.hdl = 0;
	config_found(self, &arg, audioprint);

 out:
	acpi_resource_cleanup(&res);
}
