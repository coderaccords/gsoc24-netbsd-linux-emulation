/*	$NetBSD: plumvideo.c,v 1.4 2000/03/13 18:49:17 uch Exp $ */

/*
 * Copyright (c) 1999, 2000, by UCHIYAMA Yasushi
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. The name of the developer may NOT be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#include "opt_tx39_debug.h"
#include "hpcfb.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>

#include <dev/cons.h> /* consdev */

#include <machine/bus.h>
#include <machine/intr.h>

#include <hpcmips/tx/tx39var.h>
#include <hpcmips/dev/plumvar.h>
#include <hpcmips/dev/plumicuvar.h>
#include <hpcmips/dev/plumpowervar.h>
#include <hpcmips/dev/plumvideoreg.h>

#include <machine/bootinfo.h>

#if NHPCFB > 0
#include <arch/hpcmips/dev/hpcfbvar.h>
#include <arch/hpcmips/dev/hpcfbio.h>
#include <arch/hpcmips/dev/bivideovar.h>
#endif
#include <machine/autoconf.h> /* XXX */

#ifdef PLUMVIDEODEBUG
int	plumvideo_debug = 1;
#define	DPRINTF(arg) if (plumvideo_debug) printf arg;
#define	DPRINTFN(n, arg) if (plumvideo_debug > (n)) printf arg;
#else
#define	DPRINTF(arg)
#define DPRINTFN(n, arg)
#endif

int	plumvideo_match __P((struct device*, struct cfdata*, void*));
void	plumvideo_attach __P((struct device*, struct device*, void*));
int	plumvideo_print __P((void*, const char*));

struct plumvideo_softc {
	struct	device		sc_dev;
	plum_chipset_tag_t	sc_pc;
	bus_space_tag_t		sc_regt;
	bus_space_handle_t	sc_regh;
	bus_space_tag_t		sc_iot;
	bus_space_handle_t	sc_ioh;
};

struct cfattach plumvideo_ca = {
	sizeof(struct plumvideo_softc), plumvideo_match, plumvideo_attach
};

struct fb_attach_args {
	const char *fba_name;
};

void	plumvideo_bootinforefil __P((struct plumvideo_softc*));
#ifdef PLUMVIDEODEBUG
void	plumvideo_dump __P((struct plumvideo_softc*));
#endif

int
plumvideo_match(parent, cf, aux)
	struct device *parent;
	struct cfdata *cf;
	void *aux;
{
	/*
	 * VRAM area also uses as UHOSTC shared RAM.
	 */
	return 2; /* 1st attach group */
}

void
plumvideo_attach(parent, self, aux)
	struct device *parent;
	struct device *self;
	void *aux;
{
	struct plum_attach_args *pa = aux;
	struct plumvideo_softc *sc = (void*)self;
	struct mainbus_attach_args ma; /* XXX */

	sc->sc_pc	= pa->pa_pc;
	sc->sc_regt	= pa->pa_regt;
	sc->sc_iot	= pa->pa_iot;

	if (bus_space_map(sc->sc_regt, PLUM_VIDEO_REGBASE, 
			  PLUM_VIDEO_REGSIZE, 0, &sc->sc_regh)) {
		printf(": register map failed\n");
		return;
	}
	if (bus_space_map(sc->sc_iot, PLUM_VIDEO_VRAM_IOBASE,
			  PLUM_VIDEO_VRAM_IOSIZE, 0, &sc->sc_ioh)) {
		printf(": V-RAM map failed\n");
		return;
	}

	printf("\n");

	/*
	 * Power control
	 */
#ifndef PLUMVIDEODEBUG
	if (bootinfo->bi_cnuse & BI_CNUSE_SERIAL) {
		/* LCD power on and display off */
		plum_power_disestablish(sc->sc_pc, PLUM_PWR_LCD);
		/* power off V-RAM */
		plum_power_disestablish(sc->sc_pc, PLUM_PWR_EXTPW2);
		/* power off LCD */
		plum_power_disestablish(sc->sc_pc, PLUM_PWR_EXTPW1);
		/* power off RAMDAC */
		plum_power_disestablish(sc->sc_pc, PLUM_PWR_EXTPW0);
		/* back-light off */
		plum_power_disestablish(sc->sc_pc, PLUM_PWR_BKL);
	} else 
#endif
	{
		/* LCD power on and display on */
		plum_power_establish(sc->sc_pc, PLUM_PWR_LCD);
		/* supply power to V-RAM */
		plum_power_establish(sc->sc_pc, PLUM_PWR_EXTPW2);
		/* supply power to LCD */
		plum_power_establish(sc->sc_pc, PLUM_PWR_EXTPW1);
		/* back-light on */
		plum_power_establish(sc->sc_pc, PLUM_PWR_BKL);
	}

	/* 
	 *  reinstall bootinfo 
	 */
	plumvideo_bootinforefil(sc);

#ifdef PLUMVIDEODEBUG
	if (plumvideo_debug)
		plumvideo_dump(sc);
#endif

#if NHPCFB > 0
	if(!cn_tab && hpcfb_cnattach(0, 0, 0, 0)) {
		panic("plumvideo_attach: can't init fb console");
	}
	/* Attach frame buffer device */
	ma.ma_name = "bivideo"; /* XXX */
	config_found(self, &ma, plumvideo_print);
#endif
}

int
plumvideo_print(aux, pnp)
	void *aux;
	const char *pnp;
{
	return pnp ? QUIET : UNCONF;
}

void
plumvideo_bootinforefil(sc)
	struct plumvideo_softc *sc;
{
	bus_space_tag_t regt = sc->sc_regt;
	bus_space_handle_t regh = sc->sc_regh;
	bus_space_handle_t ioh = sc->sc_ioh;
	plumreg_t reg;

	reg = plum_conf_read(regt, regh, PLUM_VIDEO_PLGMD_REG);
	switch (reg & PLUM_VIDEO_PLGMD_MASK) {
	default:
		reg |= PLUM_VIDEO_PLGMD_8BPP; /* XXX */
		plum_conf_write(regt, regh, PLUM_VIDEO_PLGMD_REG, reg);
		/* FALLTHROUGH */
	case PLUM_VIDEO_PLGMD_8BPP:
		bootinfo->fb_line_bytes = bootinfo->fb_width * 8 / 8;
		break;
	case PLUM_VIDEO_PLGMD_16BPP:
		bootinfo->fb_line_bytes = bootinfo->fb_width * 16 / 8;
		break;
	}
	
	bootinfo->fb_addr = (unsigned char *)ioh;
}

#ifdef PLUMVIDEODEBUG
void
plumvideo_dump(sc)
	struct plumvideo_softc *sc;
{
	bus_space_tag_t regt = sc->sc_regt;
	bus_space_handle_t regh = sc->sc_regh;

	plumreg_t reg;
	int i;

	for (i = 0; i < 0x160; i+=4) {
		reg = plum_conf_read(regt, regh, i);
		printf("0x%03x %08x", i, reg);
		bitdisp(reg);
	}
}
#endif /* PLUMVIDEODEBUG */
