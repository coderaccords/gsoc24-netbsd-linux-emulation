/* $NetBSD: nextdisplay.c,v 1.1 1999/01/28 11:46:23 dbj Exp $ */
/*
 * Copyright (c) 1998 Matt DeBergalis
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
 *      This product includes software developed by Matt DeBergalis
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
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


#include <sys/cdefs.h>			/* RCS ID & Copyright macro defns */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <sys/malloc.h>

#include <machine/cpu.h>
#include <machine/bus.h>

#include <next68k/dev/nextdisplayvar.h>
#include <dev/wscons/wsconsio.h>

#include <dev/rcons/raster.h>
#include <dev/wscons/wscons_raster.h>
#include <dev/wscons/wsdisplayvar.h>

int nextdisplay_match __P((struct device *, struct cfdata *, void *));
void nextdisplay_attach __P((struct device *, struct device *, void *));

struct cfattach nextdisplay_ca = {
	sizeof(struct nextdisplay_softc), 
	nextdisplay_match,
	nextdisplay_attach,
};

const struct wsdisplay_emulops nextdisplay_mono_emulops = {
	rcons_cursor,
	rcons_mapchar,
	rcons_putchar,
	rcons_copycols,
	rcons_erasecols,
	rcons_copyrows,
	rcons_eraserows,
	rcons_alloc_attr
};

struct wsscreen_descr nextdisplay_mono = {
	"std",
	0, 0, /* will be filled in -- XXX shouldn't, it's global */
	&nextdisplay_mono_emulops,
	0, 0
};

const struct wsscreen_descr *_nextdisplay_scrlist_mono[] = {
	&nextdisplay_mono,
};

const struct wsscreen_list nextdisplay_screenlist_mono = {
	sizeof(_nextdisplay_scrlist_mono) / sizeof(struct wsscreen_descr *),
	_nextdisplay_scrlist_mono
};

static int	nextdisplay_ioctl __P((void *, u_long, caddr_t, int, struct proc *));
static int	nextdisplay_mmap __P((void *, off_t, int));
static int	nextdisplay_alloc_screen __P((void *, const struct wsscreen_descr *,
		void **, int *, int *, long *));
static void	nextdisplay_free_screen __P((void *, void *));
static void	nextdisplay_show_screen __P((void *, void *));
static int	nextdisplay_load_font __P((void *, void *, struct wsdisplay_font *));

const struct wsdisplay_accessops nextdisplay_accessops = {
	nextdisplay_ioctl,
	nextdisplay_mmap,
	nextdisplay_alloc_screen,
	nextdisplay_free_screen,
	nextdisplay_show_screen,
	nextdisplay_load_font
};

void nextdisplay_init(struct nextdisplay_config *, paddr_t);

paddr_t nextdisplay_consaddr;
static int nextdisplay_is_console __P((paddr_t addr));

static struct nextdisplay_config nextdisplay_console_dc;

static int
nextdisplay_is_console(paddr_t addr)
{
	return (nextdisplay_console_dc.isconsole
			&& (addr == nextdisplay_consaddr));
}

int
nextdisplay_match(parent, match, aux)
	struct device *parent;
	struct cfdata *match;
	void *aux;
{
	return (1);
}

void
nextdisplay_init(dc, addr)
	struct nextdisplay_config *dc;
	paddr_t addr;
{
	struct raster *rap;
	struct rcons *rcp;
	int i;

	/* printf("in nextdisplay_init\n"); */

	dc->dc_vaddr = addr;
	dc->dc_paddr = VIDEOP(addr);
	dc->dc_size = NEXT_P_VIDEOSIZE;

	dc->dc_wid = 1152;
	dc->dc_ht = 832;
	dc->dc_depth = 2;
	dc->dc_rowbytes = dc->dc_wid * dc->dc_depth / 8;

	dc->dc_videobase = dc->dc_vaddr;

#if 0
	printf("intiobase at: %08x\n", intiobase);
	printf("intiolimit at: %08x\n", intiolimit);
	printf("videobase at: %08x\n", videobase);
	printf("videolimit at: %08x\n", videolimit);

	printf("virtual fb at: %08x\n", dc->dc_vaddr);
	printf("physical fb at: %08x\n", dc->dc_paddr);
	printf("fb size: %08x\n", dc->dc_size);

	printf("dc_wid: %08x\n", dc->dc_wid);
	printf("dc_ht: %08x\n", dc->dc_ht);
	printf("dc_depth: %08x\n", dc->dc_depth);
	printf("dc_rowbytes: %08x\n", dc->dc_rowbytes);
	printf("dc_videobase: %08x\n", dc->dc_videobase);
#endif

	/* clear the screen */
	for (i = 0; i < dc->dc_ht * dc->dc_rowbytes; i += sizeof(u_int32_t))
		*(u_int32_t *)(dc->dc_videobase + i) = 0x00000000;

	rap = &dc->dc_raster;
	rap->width = dc->dc_wid;
	rap->height = dc->dc_ht;
	rap->depth = 2;
	rap->linelongs = dc->dc_rowbytes / sizeof(u_int32_t);
	rap->pixels = (u_int32_t *)dc->dc_videobase;

	/* initialize the raster console blitter */
	rcp = &dc->dc_rcons;
	rcp->rc_sp = rap;
	rcp->rc_crow = rcp->rc_ccol = -1;
	rcp->rc_crowp = &rcp->rc_crow;
	rcp->rc_ccolp = &rcp->rc_ccol;
	rcons_init(rcp, 34, 80);

	nextdisplay_mono.nrows = dc->dc_rcons.rc_maxrow;
	nextdisplay_mono.ncols = dc->dc_rcons.rc_maxcol;
}

void
nextdisplay_attach(parent, self, aux)
	struct device *parent;
	struct device *self;
	void *aux;
{
	struct nextdisplay_softc *sc;
	struct wsemuldisplaydev_attach_args waa;
	int isconsole;

	sc = (struct nextdisplay_softc *)self;

	printf("\n");

	isconsole = nextdisplay_is_console(videobase);
				
	if( isconsole ) {
		sc->sc_dc = &nextdisplay_console_dc;
		sc->nscreens = 1;
	} else {
		sc->sc_dc = (struct nextdisplay_config *)
				malloc(sizeof(struct nextdisplay_config), M_DEVBUF, M_WAITOK);
		nextdisplay_init(sc->sc_dc, videobase);
	}

	/* initialize the raster */
	waa.console = isconsole;
	waa.scrdata = &nextdisplay_screenlist_mono;
	waa.accessops = &nextdisplay_accessops;
	waa.accesscookie = sc;

	config_found(self, &waa, wsemuldisplaydevprint);
}


int
nextdisplay_ioctl(v, cmd, data, flag, p)
	void *v;
	u_long cmd;
	caddr_t data;
	int flag;
	struct proc *p;
{
	struct nextdisplay_softc *sc = v;
	struct nextdisplay_config *dc = sc->sc_dc;

	switch (cmd) {
	case WSDISPLAYIO_GTYPE:
		*(int *)data = dc->dc_type;
		return 0;

	case WSDISPLAYIO_SCURSOR:
		printf("nextdisplay_ioctl: wsdisplayio_scursor\n");
		return ENOTTY;

	case WSDISPLAYIO_SCURPOS:
		printf("nextdisplay_ioctl: wsdisplayio_scurpos\n");
		return ENOTTY;

	case WSDISPLAYIO_GINFO:
	case WSDISPLAYIO_GETCMAP:
	case WSDISPLAYIO_PUTCMAP:
	case WSDISPLAYIO_GVIDEO:
	case WSDISPLAYIO_SVIDEO:
	case WSDISPLAYIO_GCURPOS:
	case WSDISPLAYIO_GCURMAX:
	case WSDISPLAYIO_GCURSOR:
		printf("nextdisplay_ioctl: listed but unsupported ioctl\n");
		return ENOTTY;
	}
	printf("nextdisplay_ioctl: unsupported ioctl\n");
	return -1;
}

static int
nextdisplay_mmap(v, offset, prot)
	void *v;
	off_t offset;
	int prot;
{

	/* XXX */
	return -1;
}

int
nextdisplay_alloc_screen(v, type, cookiep, curxp, curyp, defattrp)
	void *v;
	const struct wsscreen_descr *type;
	void **cookiep;
	int *curxp, *curyp;
	long *defattrp;
{
	struct nextdisplay_softc *sc = v;
	long defattr;

	if (sc->nscreens > 0)
		return (ENOMEM);

	*cookiep = &sc->sc_dc->dc_rcons; /* one and only for now */
	*curxp = 0;
	*curyp = 0;
	rcons_alloc_attr(&sc->sc_dc->dc_rcons, 0, 0, 0, &defattr);
	*defattrp = defattr;
	sc->nscreens++;
	return (0);
}

void
nextdisplay_free_screen(v, cookie)
	void *v;
	void *cookie;
{
	struct nextdisplay_softc *sc = v;

	if (sc->sc_dc == &nextdisplay_console_dc)
		panic("cfb_free_screen: console");

	sc->nscreens--;
}

void
nextdisplay_show_screen(v, cookie)
	void *v;
	void *cookie;
{
}

static int
nextdisplay_load_font(v, cookie, font)
	void *v;
	void *cookie;
	struct wsdisplay_font *font;
{
	return (EINVAL);
}

int
nextdisplay_cnattach(addr)
	paddr_t addr;
{
	struct nextdisplay_config *dc = &nextdisplay_console_dc;
	long defattr;

	/* set up the display */
	nextdisplay_init(&nextdisplay_console_dc, addr);

	rcons_alloc_attr(&dc->dc_rcons, 0, 0, 0, &defattr);

	wsdisplay_cnattach(&nextdisplay_mono, &dc->dc_rcons,
			0, 0, defattr);

	nextdisplay_consaddr = addr;
	dc->isconsole = 1;
	return (0);
}
