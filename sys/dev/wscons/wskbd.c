/* $NetBSD: wskbd.c,v 1.59 2001/11/22 00:57:15 augustss Exp $ */

/*
 * Copyright (c) 1996, 1997 Christopher G. Demetriou.  All rights reserved.
 *
 * Keysym translator:
 * Contributed to The NetBSD Foundation by Juergen Hannken-Illjes.
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
 *      This product includes software developed by Christopher G. Demetriou
 *	for the NetBSD Project.
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

/*
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This software was developed by the Computer Systems Engineering group
 * at Lawrence Berkeley Laboratory under DARPA contract BG 91-66 and
 * contributed to Berkeley.
 *
 * All advertising materials mentioning features or use of this software
 * must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Lawrence Berkeley Laboratory.
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
 *	@(#)kbd.c	8.2 (Berkeley) 10/30/93
 */

/*
 * Keyboard driver (/dev/wskbd*).  Translates incoming bytes to ASCII or
 * to `wscons_events' and passes them up to the appropriate reader.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: wskbd.c,v 1.59 2001/11/22 00:57:15 augustss Exp $");

#include "opt_ddb.h"
#include "opt_kgdb.h"
#include "opt_wsdisplay_compat.h"

#include "wsdisplay.h"
#include "wskbd.h"
#include "wsmux.h"

#include <sys/param.h>
#include <sys/conf.h>
#include <sys/device.h>
#include <sys/ioctl.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/syslog.h>
#include <sys/systm.h>
#include <sys/callout.h>
#include <sys/malloc.h>
#include <sys/tty.h>
#include <sys/signalvar.h>
#include <sys/errno.h>
#include <sys/fcntl.h>
#include <sys/vnode.h>

#include <dev/wscons/wsconsio.h>
#include <dev/wscons/wskbdvar.h>
#include <dev/wscons/wsksymdef.h>
#include <dev/wscons/wsksymvar.h>
#include <dev/wscons/wseventvar.h>
#include <dev/wscons/wscons_callbacks.h>

#ifdef KGDB
#include <sys/kgdb.h>
#endif

#ifdef WSKBD_DEBUG
#define DPRINTF(x)	if (wskbddebug) printf x
int	wskbddebug = 0;
#else
#define DPRINTF(x)
#endif

#include <dev/wscons/wsmuxvar.h>

struct wskbd_internal {
	const struct wskbd_mapdata *t_keymap;

	const struct wskbd_consops *t_consops;
	void	*t_consaccesscookie;

	int	t_modifiers;
	int	t_composelen;		/* remaining entries in t_composebuf */
	keysym_t t_composebuf[2];

	int t_flags;
#define WSKFL_METAESC 1

#define MAXKEYSYMSPERKEY 2 /* ESC <key> at max */
	keysym_t t_symbols[MAXKEYSYMSPERKEY];

	struct wskbd_softc *t_sc;	/* back pointer */
};

struct wskbd_softc {
	struct wsevsrc sc_base;

	struct wskbd_internal *id;

	const struct wskbd_accessops *sc_accessops;
	void *sc_accesscookie;

	int	sc_ledstate;

	int	sc_isconsole;

	struct wskbd_bell_data sc_bell_data;
	struct wskbd_keyrepeat_data sc_keyrepeat_data;

	int	sc_repeating;		/* we've called timeout() */
	struct callout sc_repeat_ch;

	int	sc_translating;		/* xlate to chars for emulation */

	int	sc_maplen;		/* number of entries in sc_map */
	struct wscons_keymap *sc_map;	/* current translation map */
	kbd_t sc_layout; /* current layout */

	int		sc_refcnt;
	u_char		sc_dying;	/* device is being detached */
};

#define MOD_SHIFT_L		(1 << 0)
#define MOD_SHIFT_R		(1 << 1)
#define MOD_SHIFTLOCK		(1 << 2)
#define MOD_CAPSLOCK		(1 << 3)
#define MOD_CONTROL_L		(1 << 4)
#define MOD_CONTROL_R		(1 << 5)
#define MOD_META_L		(1 << 6)
#define MOD_META_R		(1 << 7)
#define MOD_MODESHIFT		(1 << 8)
#define MOD_NUMLOCK		(1 << 9)
#define MOD_COMPOSE		(1 << 10)
#define MOD_HOLDSCREEN		(1 << 11)
#define MOD_COMMAND		(1 << 12)
#define MOD_COMMAND1		(1 << 13)
#define MOD_COMMAND2		(1 << 14)

#define MOD_ANYSHIFT		(MOD_SHIFT_L | MOD_SHIFT_R | MOD_SHIFTLOCK)
#define MOD_ANYCONTROL		(MOD_CONTROL_L | MOD_CONTROL_R)
#define MOD_ANYMETA		(MOD_META_L | MOD_META_R)

#define MOD_ONESET(id, mask)	(((id)->t_modifiers & (mask)) != 0)
#define MOD_ALLSET(id, mask)	(((id)->t_modifiers & (mask)) == (mask))

static int  wskbd_match(struct device *, struct cfdata *, void *);
static void wskbd_attach(struct device *, struct device *, void *);
static int  wskbd_detach(struct device *, int);
static int  wskbd_activate(struct device *, enum devact);

static int  wskbd_displayioctl(struct device *, u_long, caddr_t, int,
			      struct proc *);
#if NWSDISPLAY > 0
static int  wskbd_set_display(struct device *, struct wsevsrc *);
#else
#define wskbd_set_display NULL
#endif

static inline void update_leds(struct wskbd_internal *);
static inline void update_modifier(struct wskbd_internal *, u_int, int, int);
static int internal_command(struct wskbd_softc *, u_int *, keysym_t, keysym_t);
static int wskbd_translate(struct wskbd_internal *, u_int, int);
static int wskbd_enable(struct wskbd_softc *, int);
#if NWSDISPLAY > 0
static void change_displayparam(struct wskbd_softc *, int, int, int);
static void wskbd_holdscreen(struct wskbd_softc *, int);
#endif

static int wskbd_do_ioctl_sc(struct wskbd_softc *, u_long, caddr_t, int,
			     struct proc *);

#if NWSMUX > 0
static int wskbd_mux_open(struct wsevsrc *, struct wseventvar *);
static int wskbd_mux_close(struct wsevsrc *);
#else
#define wskbd_mux_open NULL
#define wskbd_mux_close NULL
#endif

static int wskbd_do_open(struct wskbd_softc *, struct wseventvar *);
static int wskbd_do_ioctl(struct device *, u_long, caddr_t, int, struct proc *);

struct cfattach wskbd_ca = {
	sizeof (struct wskbd_softc), wskbd_match, wskbd_attach,
	wskbd_detach, wskbd_activate
};

extern struct cfdriver wskbd_cd;

#ifndef WSKBD_DEFAULT_BELL_PITCH
#define	WSKBD_DEFAULT_BELL_PITCH	1500	/* 1500Hz */
#endif
#ifndef WSKBD_DEFAULT_BELL_PERIOD
#define	WSKBD_DEFAULT_BELL_PERIOD	100	/* 100ms */
#endif
#ifndef WSKBD_DEFAULT_BELL_VOLUME
#define	WSKBD_DEFAULT_BELL_VOLUME	50	/* 50% volume */
#endif

struct wskbd_bell_data wskbd_default_bell_data = {
	WSKBD_BELL_DOALL,
	WSKBD_DEFAULT_BELL_PITCH,
	WSKBD_DEFAULT_BELL_PERIOD,
	WSKBD_DEFAULT_BELL_VOLUME,
};

#ifndef WSKBD_DEFAULT_KEYREPEAT_DEL1
#define	WSKBD_DEFAULT_KEYREPEAT_DEL1	400	/* 400ms to start repeating */
#endif
#ifndef WSKBD_DEFAULT_KEYREPEAT_DELN
#define	WSKBD_DEFAULT_KEYREPEAT_DELN	100	/* 100ms to between repeats */
#endif

struct wskbd_keyrepeat_data wskbd_default_keyrepeat_data = {
	WSKBD_KEYREPEAT_DOALL,
	WSKBD_DEFAULT_KEYREPEAT_DEL1,
	WSKBD_DEFAULT_KEYREPEAT_DELN,
};

cdev_decl(wskbd);

#if NWSDISPLAY > 0 || NWSMUX > 0
struct wssrcops wskbd_srcops = {
	WSMUX_KBD,
	wskbd_mux_open, wskbd_mux_close, wskbd_do_ioctl,
	wskbd_displayioctl, wskbd_set_display
};
#endif

#if NWSDISPLAY > 0
static void wskbd_repeat(void *v);
#endif

static int wskbd_console_initted;
static struct wskbd_softc *wskbd_console_device;
static struct wskbd_internal wskbd_console_data;

static void wskbd_update_layout(struct wskbd_internal *, kbd_t);

static void
wskbd_update_layout(struct wskbd_internal *id, kbd_t enc)
{

	if (enc & KB_METAESC)
		id->t_flags |= WSKFL_METAESC;
	else
		id->t_flags &= ~WSKFL_METAESC;
}

/*
 * Print function (for parent devices).
 */
int
wskbddevprint(void *aux, const char *pnp)
{
#if 0
	struct wskbddev_attach_args *ap = aux;
#endif

	if (pnp)
		printf("wskbd at %s", pnp);
#if 0
	printf(" console %d", ap->console);
#endif

	return (UNCONF);
}

int
wskbd_match(struct device *parent, struct cfdata *match, void *aux)
{
	struct wskbddev_attach_args *ap = aux;

	if (match->wskbddevcf_console != WSKBDDEVCF_CONSOLE_UNK) {
		/*
		 * If console-ness of device specified, either match
		 * exactly (at high priority), or fail.
		 */
		if (match->wskbddevcf_console != 0 && ap->console != 0)
			return (10);
		else
			return (0);
	}

	/* If console-ness unspecified, it wins. */
	return (1);
}

void
wskbd_attach(struct device *parent, struct device *self, void *aux)
{
	struct wskbd_softc *sc = (struct wskbd_softc *)self;
	struct wskbddev_attach_args *ap = aux;
#if NWSMUX > 0
	int mux, error;
#endif

	sc->sc_isconsole = ap->console;

#if NWSMUX > 0 || NWSDISPLAY > 0
	sc->sc_base.me_ops = &wskbd_srcops;
#endif
#if NWSMUX > 0
	mux = sc->sc_base.me_dv.dv_cfdata->wskbddevcf_mux;
	if (ap->console) {
		/* Ignore mux for console; it always goes to the console mux. */
		/* printf(" (mux %d ignored for console)", mux); */
		mux = -1;
	}
	if (mux >= 0)
		printf(" mux %d", mux);
#else
	if (sc->sc_base.me_dv.dv_cfdata->wskbddevcf_mux >= 0)
		printf(" (mux ignored)");
#endif

	if (ap->console) {
		sc->id = &wskbd_console_data;
	} else {
		sc->id = malloc(sizeof(struct wskbd_internal),
				M_DEVBUF, M_WAITOK);
		memset(sc->id, 0, sizeof(struct wskbd_internal));
		sc->id->t_keymap = ap->keymap;
		wskbd_update_layout(sc->id, ap->keymap->layout);
	}

	callout_init(&sc->sc_repeat_ch);

	sc->id->t_sc = sc;

	sc->sc_accessops = ap->accessops;
	sc->sc_accesscookie = ap->accesscookie;
	sc->sc_repeating = 0;
	sc->sc_translating = 1;
	sc->sc_ledstate = -1; /* force update */

	if (wskbd_load_keymap(sc->id->t_keymap,
			      &sc->sc_map, &sc->sc_maplen) != 0)
		panic("cannot load keymap");

	sc->sc_layout = sc->id->t_keymap->layout;

	/* set default bell and key repeat data */
	sc->sc_bell_data = wskbd_default_bell_data;
	sc->sc_keyrepeat_data = wskbd_default_keyrepeat_data;

	if (ap->console) {
		KASSERT(wskbd_console_initted); 
		KASSERT(wskbd_console_device == NULL);

		wskbd_console_device = sc;

		printf(": console keyboard");

#if NWSDISPLAY > 0
		wsdisplay_set_console_kbd(&sc->sc_base); /* sets me_dispv */
		if (sc->sc_base.me_dispdv != NULL)
			printf(", using %s", sc->sc_base.me_dispdv->dv_xname);
#endif
	}
	printf("\n");

#if NWSMUX > 0
	if (mux >= 0) {
		error = wsmux_attach_sc(wsmux_getmux(mux), &sc->sc_base);
		if (error)
			printf("%s: attach error=%d\n",
			    sc->sc_base.me_dv.dv_xname, error);
	}
#endif
}

void    
wskbd_cnattach(const struct wskbd_consops *consops, void *conscookie,
	const struct wskbd_mapdata *mapdata)
{
	KASSERT(!wskbd_console_initted);

	wskbd_console_data.t_keymap = mapdata;
	wskbd_update_layout(&wskbd_console_data, mapdata->layout);

	wskbd_console_data.t_consops = consops;
	wskbd_console_data.t_consaccesscookie = conscookie;

#if NWSDISPLAY > 0
	wsdisplay_set_cons_kbd(wskbd_cngetc, wskbd_cnpollc, wskbd_cnbell);
#endif

	wskbd_console_initted = 1;
}

void    
wskbd_cndetach(void)
{
	KASSERT(wskbd_console_initted);

	wskbd_console_data.t_keymap = 0;

	wskbd_console_data.t_consops = 0;
	wskbd_console_data.t_consaccesscookie = 0;

#if NWSDISPLAY > 0
	wsdisplay_unset_cons_kbd();
#endif

	wskbd_console_initted = 0;
}

#if NWSDISPLAY > 0
static void
wskbd_repeat(void *v)
{
	struct wskbd_softc *sc = (struct wskbd_softc *)v;
	int s = spltty();

	if (!sc->sc_repeating) {
		/*
		 * race condition: a "key up" event came in when wskbd_repeat()
		 * was already called but not yet spltty()'d
		 */
		splx(s);
		return;
	}
	if (sc->sc_base.me_dispdv != NULL) {
		int i;
		for (i = 0; i < sc->sc_repeating; i++)
			wsdisplay_kbdinput(sc->sc_base.me_dispdv,
					   sc->id->t_symbols[i]);
	}
	callout_reset(&sc->sc_repeat_ch,
	    (hz * sc->sc_keyrepeat_data.delN) / 1000, wskbd_repeat, sc);
	splx(s);
}
#endif

int
wskbd_activate(struct device *self, enum devact act)
{
	struct wskbd_softc *sc = (struct wskbd_softc *)self;

	if (act == DVACT_DEACTIVATE)
		sc->sc_dying = 1;
	return (0);
}

/*
 * Detach a keyboard.  To keep track of users of the softc we keep
 * a reference count that's incremented while inside, e.g., read.
 * If the keyboard is active and the reference count is > 0 (0 is the
 * normal state) we post an event and then wait for the process
 * that had the reference to wake us up again.  Then we blow away the
 * vnode and return (which will deallocate the softc).
 */
int
wskbd_detach(struct device  *self, int flags)
{
	struct wskbd_softc *sc = (struct wskbd_softc *)self;
	struct wseventvar *evar;
	int maj, mn;
	int s;

#if NWSMUX > 0
	/* Tell parent mux we're leaving. */
	if (sc->sc_base.me_parent != NULL)
		wsmux_detach_sc(&sc->sc_base);
#endif

	if (sc->sc_isconsole) {
		KASSERT(wskbd_console_device == sc);
		wskbd_console_device = NULL;
	}

	evar = sc->sc_base.me_evp;
	if (evar != NULL && evar->io != NULL) {
		s = spltty();
		if (--sc->sc_refcnt >= 0) {
			/* Wake everyone by generating a dummy event. */
			if (++evar->put >= WSEVENT_QSIZE)
				evar->put = 0;
			WSEVENT_WAKEUP(evar);
			/* Wait for processes to go away. */
			if (tsleep(sc, PZERO, "wskdet", hz * 60))
				printf("wskbd_detach: %s didn't detach\n",
				       sc->sc_base.me_dv.dv_xname);
		}
		splx(s);
	}

	/* locate the major number */
	for (maj = 0; maj < nchrdev; maj++)
		if (cdevsw[maj].d_open == wskbdopen)
			break;

	/* Nuke the vnodes for any open instances. */
	mn = self->dv_unit;
	vdevgone(maj, mn, mn, VCHR);

	return (0);
}

void
wskbd_input(struct device *dev, u_int type, int value)
{
	struct wskbd_softc *sc = (struct wskbd_softc *)dev; 
	struct wscons_event *ev;
	struct wseventvar *evar;
	struct timeval thistime;
#if NWSDISPLAY > 0
	int num, i;
#endif
	int put;

#if NWSDISPLAY > 0
	if (sc->sc_repeating) {
		sc->sc_repeating = 0;
		callout_stop(&sc->sc_repeat_ch);
	}

	/*
	 * If /dev/wskbdN is not connected in event mode translate and
	 * send upstream.
	 */
	if (sc->sc_translating) {
		num = wskbd_translate(sc->id, type, value);
		if (num > 0) {
			if (sc->sc_base.me_dispdv != NULL) {
				for (i = 0; i < num; i++)
					wsdisplay_kbdinput(
						sc->sc_base.me_dispdv,
						sc->id->t_symbols[i]);
			}

			sc->sc_repeating = num;
			callout_reset(&sc->sc_repeat_ch,
			    (hz * sc->sc_keyrepeat_data.del1) / 1000,
			    wskbd_repeat, sc);
		}
		return;
	}
#endif

	/*
	 * Keyboard is generating events.  Turn this keystroke into an
	 * event and put it in the queue.  If the queue is full, the
	 * keystroke is lost (sorry!).
	 */

	evar = sc->sc_base.me_evp;
	if (evar == NULL) {
		DPRINTF(("wskbd_input: not open\n"));
		return;
	}

#ifdef DIAGNOSTIC
	if (evar->q == NULL) {
		printf("wskbd_input: evar->q=NULL\n");
		return;
	}
#endif

	put = evar->put;
	ev = &evar->q[put];
	put = (put + 1) % WSEVENT_QSIZE;
	if (put == evar->get) {
		log(LOG_WARNING, "%s: event queue overflow\n",
		    sc->sc_base.me_dv.dv_xname);
		return;
	}
	ev->type = type;
	ev->value = value;
	microtime(&thistime);
	TIMEVAL_TO_TIMESPEC(&thistime, &ev->time);
	evar->put = put;
	WSEVENT_WAKEUP(evar);
}

#ifdef WSDISPLAY_COMPAT_RAWKBD
void
wskbd_rawinput(struct device *dev, u_char *buf, int len)
{
#if NWSDISPLAY > 0
	struct wskbd_softc *sc = (struct wskbd_softc *)dev;
	int i;

	if (sc->sc_base.me_dispdv != NULL)
		for (i = 0; i < len; i++)
			wsdisplay_kbdinput(sc->sc_base.me_dispdv, buf[i]);
	/* this is KS_GROUP_Ascii */
#endif
}
#endif /* WSDISPLAY_COMPAT_RAWKBD */

#if NWSDISPLAY > 0
static void
wskbd_holdscreen(struct wskbd_softc *sc, int hold)
{
	int new_state;

	if (sc->sc_base.me_dispdv != NULL) {
		wsdisplay_kbdholdscreen(sc->sc_base.me_dispdv, hold);
		new_state = sc->sc_ledstate;
		if (hold)
			new_state |= WSKBD_LED_SCROLL;
		else
			new_state &= ~WSKBD_LED_SCROLL;
		if (new_state != sc->sc_ledstate) {
			(*sc->sc_accessops->set_leds)(sc->sc_accesscookie,
						      new_state);
			sc->sc_ledstate = new_state;
		}
	}
}
#endif

static int
wskbd_enable(struct wskbd_softc *sc, int on)
{
	int error;

#if 0
/* I don't understand the purpose of this code.  And it seems to
 * break things, so it's out.  -- Lennart
 */
	if (!on && (!sc->sc_translating
#if NWSDISPLAY > 0
		    || sc->sc_base.me_dispdv
#endif
		))
		return (EBUSY);
#endif
#if NWSDISPLAY > 0
	if (sc->sc_base.me_dispdv != NULL)
		return (0);
#endif

	error = (*sc->sc_accessops->enable)(sc->sc_accesscookie, on);
	DPRINTF(("wskbd_enable: sc=%p on=%d res=%d\n", sc, on, error));
	return (error);
}

#if NWSMUX > 0
int
wskbd_mux_open(struct wsevsrc *me, struct wseventvar *evp)
{
	struct wskbd_softc *sc = (struct wskbd_softc *)me;

	if (sc->sc_dying)
		return (EIO);

	if (sc->sc_base.me_evp != NULL)
		return (EBUSY);

	return (wskbd_do_open(sc, evp));
}
#endif

int
wskbdopen(dev_t dev, int flags, int mode, struct proc *p)
{
	struct wskbd_softc *sc;
	struct wseventvar *evar;
	int unit, error;

	unit = minor(dev);
	if (unit >= wskbd_cd.cd_ndevs ||	/* make sure it was attached */
	    (sc = wskbd_cd.cd_devs[unit]) == NULL)
		return (ENXIO);

#if NWSMUX > 0
	DPRINTF(("wskbdopen: %s mux=%p p=%p\n", sc->sc_base.me_dv.dv_xname,
		 sc->sc_base.me_parent, p));
#endif

	if (sc->sc_dying)
		return (EIO);

	if ((flags & (FREAD | FWRITE)) == FWRITE)
		/* Not opening for read, only ioctl is available. */
		return (0);

#if NWSMUX > 0
	if (sc->sc_base.me_parent != NULL) {
		/* Grab the keyboard out of the greedy hands of the mux. */
		DPRINTF(("wskbdopen: detach\n"));
		wsmux_detach_sc(&sc->sc_base);
	}
#endif

	if (sc->sc_base.me_evp != NULL)
		return (EBUSY);

	evar = &sc->sc_base.me_evar;
	wsevent_init(evar);
	evar->io = p;

	error = wskbd_do_open(sc, evar);
	if (error) {
		DPRINTF(("wskbdopen: %s open failed\n",
			 sc->sc_base.me_dv.dv_xname));
		sc->sc_base.me_evp = NULL;
		wsevent_fini(evar);
	}
	return (error);
}

int
wskbd_do_open(struct wskbd_softc *sc, struct wseventvar *evp)
{
	sc->sc_base.me_evp = evp;
	sc->sc_translating = 0;

	return (wskbd_enable(sc, 1));
}

int
wskbdclose(dev_t dev, int flags, int mode, struct proc *p)
{
	struct wskbd_softc *sc = 
	    (struct wskbd_softc *)wskbd_cd.cd_devs[minor(dev)];
	struct wseventvar *evar = sc->sc_base.me_evp;

	if (evar == NULL)
		/* not open for read */
		return (0);

	sc->sc_base.me_evp = NULL;
	sc->sc_translating = 1;
	(void)wskbd_enable(sc, 0);
	wsevent_fini(evar);

	return (0);
}

#if NWSMUX > 0
int
wskbd_mux_close(struct wsevsrc *me)
{
	struct wskbd_softc *sc = (struct wskbd_softc *)me;

	sc->sc_base.me_evp = NULL;
	sc->sc_translating = 1;
	(void)wskbd_enable(sc, 0);

	return (0);
}
#endif

int
wskbdread(dev_t dev, struct uio *uio, int flags)
{
	struct wskbd_softc *sc = wskbd_cd.cd_devs[minor(dev)];
	int error;

	if (sc->sc_dying)
		return (EIO);

#ifdef DIAGNOSTIC
	if (sc->sc_base.me_evp == NULL) {
		printf("wskbdread: evp == NULL\n");
		return (EINVAL);
	}
#endif

	sc->sc_refcnt++;
	error = wsevent_read(sc->sc_base.me_evp, uio, flags);
	if (--sc->sc_refcnt < 0) {
		wakeup(sc);
		error = EIO;
	}
	return (error);
}

int
wskbdioctl(dev_t dev, u_long cmd, caddr_t data, int flag, struct proc *p)
{
	return (wskbd_do_ioctl(wskbd_cd.cd_devs[minor(dev)], cmd, data, flag,p));
}

/* A wrapper around the ioctl() workhorse to make reference counting easy. */
int
wskbd_do_ioctl(struct device *dv, u_long cmd, caddr_t data, int flag, 
	struct proc *p)
{
	struct wskbd_softc *sc = (struct wskbd_softc *)dv;
	int error;

	sc->sc_refcnt++;
	error = wskbd_do_ioctl_sc(sc, cmd, data, flag, p);
	if (--sc->sc_refcnt < 0)
		wakeup(sc);
	return (error);
}

int
wskbd_do_ioctl_sc(struct wskbd_softc *sc, u_long cmd, caddr_t data, int flag,
		  struct proc *p)
{
	int error;

	/*      
	 * Try the generic ioctls that the wskbd interface supports.
	 */
	switch (cmd) {
	case FIONBIO:		/* we will remove this someday (soon???) */
		return (0);

	case FIOASYNC:
		if (sc->sc_base.me_evp == NULL)
			return (EINVAL);
		sc->sc_base.me_evp->async = *(int *)data != 0;
		return (0);

	case TIOCSPGRP:
		if (sc->sc_base.me_evp == NULL)
			return (EINVAL);
		if (*(int *)data != sc->sc_base.me_evp->io->p_pgid)
			return (EPERM);
		return (0);
	}

	/*
	 * Try the keyboard driver for WSKBDIO ioctls.  It returns -1
	 * if it didn't recognize the request.
	 */
	error = wskbd_displayioctl(&sc->sc_base.me_dv, cmd, data, flag, p);
	return (error != -1 ? error : ENOTTY);
}

/*
 * WSKBDIO ioctls, handled in both emulation mode and in ``raw'' mode.
 * Some of these have no real effect in raw mode, however.
 */
static int
wskbd_displayioctl(struct device *dev, u_long cmd, caddr_t data, int flag,
	struct proc *p)
{
	struct wskbd_softc *sc = (struct wskbd_softc *)dev;
	struct wskbd_bell_data *ubdp, *kbdp;
	struct wskbd_keyrepeat_data *ukdp, *kkdp;
	struct wskbd_map_data *umdp;
	struct wskbd_mapdata md;
	kbd_t enc;
	void *buf;
	int len, error;

	switch (cmd) {
#define	SETBELL(dstp, srcp, dfltp)					\
    do {								\
	(dstp)->pitch = ((srcp)->which & WSKBD_BELL_DOPITCH) ?		\
	    (srcp)->pitch : (dfltp)->pitch;				\
	(dstp)->period = ((srcp)->which & WSKBD_BELL_DOPERIOD) ?	\
	    (srcp)->period : (dfltp)->period;				\
	(dstp)->volume = ((srcp)->which & WSKBD_BELL_DOVOLUME) ?	\
	    (srcp)->volume : (dfltp)->volume;				\
	(dstp)->which = WSKBD_BELL_DOALL;				\
    } while (0)

	case WSKBDIO_BELL:
		if ((flag & FWRITE) == 0)
			return (EACCES);
		return ((*sc->sc_accessops->ioctl)(sc->sc_accesscookie,
		    WSKBDIO_COMPLEXBELL, (caddr_t)&sc->sc_bell_data, flag, p));

	case WSKBDIO_COMPLEXBELL:
		if ((flag & FWRITE) == 0)
			return (EACCES);
		ubdp = (struct wskbd_bell_data *)data;
		SETBELL(ubdp, ubdp, &sc->sc_bell_data);
		return ((*sc->sc_accessops->ioctl)(sc->sc_accesscookie,
		    WSKBDIO_COMPLEXBELL, (caddr_t)ubdp, flag, p));

	case WSKBDIO_SETBELL:
		if ((flag & FWRITE) == 0)
			return (EACCES);
		kbdp = &sc->sc_bell_data;
setbell:
		ubdp = (struct wskbd_bell_data *)data;
		SETBELL(kbdp, ubdp, kbdp);
		return (0);

	case WSKBDIO_GETBELL:
		kbdp = &sc->sc_bell_data;
getbell:
		ubdp = (struct wskbd_bell_data *)data;
		SETBELL(ubdp, kbdp, kbdp);
		return (0);

	case WSKBDIO_SETDEFAULTBELL:
		if ((error = suser(p->p_ucred, &p->p_acflag)) != 0)
			return (error);
		kbdp = &wskbd_default_bell_data;
		goto setbell;


	case WSKBDIO_GETDEFAULTBELL:
		kbdp = &wskbd_default_bell_data;
		goto getbell;

#undef SETBELL

#define	SETKEYREPEAT(dstp, srcp, dfltp)					\
    do {								\
	(dstp)->del1 = ((srcp)->which & WSKBD_KEYREPEAT_DODEL1) ?	\
	    (srcp)->del1 : (dfltp)->del1;				\
	(dstp)->delN = ((srcp)->which & WSKBD_KEYREPEAT_DODELN) ?	\
	    (srcp)->delN : (dfltp)->delN;				\
	(dstp)->which = WSKBD_KEYREPEAT_DOALL;				\
    } while (0)

	case WSKBDIO_SETKEYREPEAT:
		if ((flag & FWRITE) == 0)
			return (EACCES);
		kkdp = &sc->sc_keyrepeat_data;
setkeyrepeat:
		ukdp = (struct wskbd_keyrepeat_data *)data;
		SETKEYREPEAT(kkdp, ukdp, kkdp);
		return (0);

	case WSKBDIO_GETKEYREPEAT:
		kkdp = &sc->sc_keyrepeat_data;
getkeyrepeat:
		ukdp = (struct wskbd_keyrepeat_data *)data;
		SETKEYREPEAT(ukdp, kkdp, kkdp);
		return (0);

	case WSKBDIO_SETDEFAULTKEYREPEAT:
		if ((error = suser(p->p_ucred, &p->p_acflag)) != 0)
			return (error);
		kkdp = &wskbd_default_keyrepeat_data;
		goto setkeyrepeat;


	case WSKBDIO_GETDEFAULTKEYREPEAT:
		kkdp = &wskbd_default_keyrepeat_data;
		goto getkeyrepeat;

#undef SETKEYREPEAT

	case WSKBDIO_SETMAP:
		if ((flag & FWRITE) == 0)
			return (EACCES);
		umdp = (struct wskbd_map_data *)data;
		if (umdp->maplen > WSKBDIO_MAXMAPLEN)
			return (EINVAL);

		len = umdp->maplen*sizeof(struct wscons_keymap);
		buf = malloc(len, M_TEMP, M_WAITOK);
		error = copyin(umdp->map, buf, len);
		if (error == 0) {
			wskbd_init_keymap(umdp->maplen,
					  &sc->sc_map, &sc->sc_maplen);
			memcpy(sc->sc_map, buf, len);
			/* drop the variant bits handled by the map */
			sc->sc_layout = KB_USER |
			      (KB_VARIANT(sc->sc_layout) & KB_HANDLEDBYWSKBD);
			wskbd_update_layout(sc->id, sc->sc_layout);
		}
		free(buf, M_TEMP);
		return(error);

	case WSKBDIO_GETMAP:
		umdp = (struct wskbd_map_data *)data;
		if (umdp->maplen > sc->sc_maplen)
			umdp->maplen = sc->sc_maplen;
		error = copyout(sc->sc_map, umdp->map,
				umdp->maplen*sizeof(struct wscons_keymap));
		return(error);

	case WSKBDIO_GETENCODING:
		*((kbd_t *) data) = sc->sc_layout;
		return(0);

	case WSKBDIO_SETENCODING:
		if ((flag & FWRITE) == 0)
			return (EACCES);
		enc = *((kbd_t *)data);
		if (KB_ENCODING(enc) == KB_USER) {
			/* user map must already be loaded */
			if (KB_ENCODING(sc->sc_layout) != KB_USER)
				return (EINVAL);
			/* map variants make no sense */
			if (KB_VARIANT(enc) & ~KB_HANDLEDBYWSKBD)
				return (EINVAL);
		} else {
			md = *(sc->id->t_keymap); /* structure assignment */
			md.layout = enc;
			error = wskbd_load_keymap(&md, &sc->sc_map,
						  &sc->sc_maplen);
			if (error)
				return (error);
		}
		sc->sc_layout = enc;
		wskbd_update_layout(sc->id, enc);
		return (0);
	}

	/*
	 * Try the keyboard driver for WSKBDIO ioctls.  It returns -1
	 * if it didn't recognize the request, and in turn we return
	 * -1 if we didn't recognize the request.
	 */
/* printf("kbdaccess\n"); */
	error = (*sc->sc_accessops->ioctl)(sc->sc_accesscookie, cmd, data,
					   flag, p);
#ifdef WSDISPLAY_COMPAT_RAWKBD
	if (!error && cmd == WSKBDIO_SETMODE && *(int *)data == WSKBD_RAW) {
		int s = spltty();
		sc->id->t_modifiers &= ~(MOD_SHIFT_L | MOD_SHIFT_R
					 | MOD_CONTROL_L | MOD_CONTROL_R
					 | MOD_META_L | MOD_META_R
					 | MOD_COMMAND
					 | MOD_COMMAND1 | MOD_COMMAND2);
#if NWSDISPLAY > 0
		if (sc->sc_repeating) {
			sc->sc_repeating = 0;
			callout_stop(&sc->sc_repeat_ch);
		}
#endif
		splx(s);
	}
#endif
	return (error);
}

int
wskbdpoll(dev_t dev, int events, struct proc *p)
{
	struct wskbd_softc *sc = wskbd_cd.cd_devs[minor(dev)];

	if (sc->sc_base.me_evp == NULL)
		return (EINVAL);
	return (wsevent_poll(sc->sc_base.me_evp, events, p));
}

#if NWSDISPLAY > 0

int
wskbd_pickfree(void)
{
	int i;
	struct wskbd_softc *sc;

	for (i = 0; i < wskbd_cd.cd_ndevs; i++) {
		if ((sc = wskbd_cd.cd_devs[i]) == NULL)
			continue;
		if (sc->sc_base.me_dispdv == NULL)
			return (i);
	}
	return (-1);
}

struct wsevsrc *
wskbd_set_console_display(struct device *displaydv, struct wsevsrc *me)
{
	struct wskbd_softc *sc = wskbd_console_device;

	if (sc == NULL)
		return (NULL);
	sc->sc_base.me_dispdv = displaydv;
#if NWSMUX > 0
	(void)wsmux_attach_sc((struct wsmux_softc *)me, &sc->sc_base);
#endif
	return (&sc->sc_base);
}

int
wskbd_set_display(struct device *dv, struct wsevsrc *me)
{
	struct wskbd_softc *sc = (struct wskbd_softc *)dv;
	struct device *displaydv = me != NULL ? me->me_dispdv : NULL;
	struct device *odisplaydv;
	int error;

	DPRINTF(("wskbd_set_display: %s me=%p odisp=%p disp=%p cons=%d\n",
		 dv->dv_xname, me, sc->sc_base.me_dispdv, displaydv, 
		 sc->sc_isconsole));

	if (sc->sc_isconsole)
		return (EBUSY);

	if (displaydv != NULL) {
		if (sc->sc_base.me_dispdv != NULL)
			return (EBUSY);
	} else {
		if (sc->sc_base.me_dispdv == NULL)
			return (ENXIO);
	}

	odisplaydv = sc->sc_base.me_dispdv;
	sc->sc_base.me_dispdv = NULL;
	error = wskbd_enable(sc, displaydv != NULL);
	sc->sc_base.me_dispdv = displaydv;
	if (error) {
		sc->sc_base.me_dispdv = odisplaydv;
		return (error);
	}

	if (displaydv)
		printf("%s: connecting to %s\n",
		       sc->sc_base.me_dv.dv_xname, displaydv->dv_xname);
	else
		printf("%s: disconnecting from %s\n",
		       sc->sc_base.me_dv.dv_xname, odisplaydv->dv_xname);

	return (0);
}

#endif /* NWSDISPLAY > 0 */

#if NWSMUX > 0
int
wskbd_add_mux(int unit, struct wsmux_softc *muxsc)
{
	struct wskbd_softc *sc;

	if (unit < 0 || unit >= wskbd_cd.cd_ndevs ||
	    (sc = wskbd_cd.cd_devs[unit]) == NULL)
		return (ENXIO);

	if (sc->sc_base.me_parent != NULL || sc->sc_base.me_evp != NULL)
		return (EBUSY);

	return (wsmux_attach_sc(muxsc, &sc->sc_base));
}
#endif

/*
 * Console interface.
 */
int
wskbd_cngetc(dev_t dev)
{
	static int num = 0;
	static int pos;
	u_int type;
	int data;
	keysym_t ks;

	if (!wskbd_console_initted)
		return 0;

	if (wskbd_console_device != NULL &&
	    !wskbd_console_device->sc_translating)
		return 0;

	for(;;) {
		if (num-- > 0) {
			ks = wskbd_console_data.t_symbols[pos++];
			if (KS_GROUP(ks) == KS_GROUP_Ascii)
				return (KS_VALUE(ks));	
		} else {
			(*wskbd_console_data.t_consops->getc)
				(wskbd_console_data.t_consaccesscookie,
				 &type, &data);
			num = wskbd_translate(&wskbd_console_data, type, data);
			pos = 0;
		}
	}
}

void
wskbd_cnpollc(dev_t dev, int poll)
{

	if (!wskbd_console_initted)
		return;

	if (wskbd_console_device != NULL &&
	    !wskbd_console_device->sc_translating)
		return;

	(*wskbd_console_data.t_consops->pollc)
	    (wskbd_console_data.t_consaccesscookie, poll);
}

void
wskbd_cnbell(dev_t dev, u_int pitch, u_int period, u_int volume)
{

	if (!wskbd_console_initted)
		return;

	if (wskbd_console_data.t_consops->bell != NULL)
		(*wskbd_console_data.t_consops->bell)
		    (wskbd_console_data.t_consaccesscookie, pitch, period,
			volume);
}

static inline void
update_leds(struct wskbd_internal *id)
{
	int new_state;

	new_state = 0;
	if (id->t_modifiers & (MOD_SHIFTLOCK | MOD_CAPSLOCK))
		new_state |= WSKBD_LED_CAPS;
	if (id->t_modifiers & MOD_NUMLOCK)
		new_state |= WSKBD_LED_NUM;
	if (id->t_modifiers & MOD_COMPOSE)
		new_state |= WSKBD_LED_COMPOSE;
	if (id->t_modifiers & MOD_HOLDSCREEN)
		new_state |= WSKBD_LED_SCROLL;

	if (id->t_sc && new_state != id->t_sc->sc_ledstate) {
		(*id->t_sc->sc_accessops->set_leds)
		    (id->t_sc->sc_accesscookie, new_state);
		id->t_sc->sc_ledstate = new_state;
	}
}

static inline void
update_modifier(struct wskbd_internal *id, u_int type, int toggle, int mask)
{
	if (toggle) {
		if (type == WSCONS_EVENT_KEY_DOWN)
			id->t_modifiers ^= mask;
	} else {
		if (type == WSCONS_EVENT_KEY_DOWN)
			id->t_modifiers |= mask;
		else
			id->t_modifiers &= ~mask;
	}
}

#if NWSDISPLAY > 0
static void
change_displayparam(struct wskbd_softc *sc, int param, int updown,
	int wraparound)
{
	int res;
	struct wsdisplay_param dp;

	if (sc->sc_base.me_dispdv == NULL)
		return;

	dp.param = param;
	res = wsdisplay_param(sc->sc_base.me_dispdv, WSDISPLAYIO_GETPARAM, &dp);

	if (res == EINVAL)
		return; /* no such parameter */

	dp.curval += updown;
	if (dp.max < dp.curval)
		dp.curval = wraparound ? dp.min : dp.max;
	else
	if (dp.curval < dp.min)
		dp.curval = wraparound ? dp.max : dp.min;
	wsdisplay_param(sc->sc_base.me_dispdv, WSDISPLAYIO_SETPARAM, &dp);
}
#endif

static int
internal_command(struct wskbd_softc *sc, u_int *type, keysym_t ksym,
	keysym_t ksym2)
{
	switch (ksym) {
	case KS_Cmd:
		update_modifier(sc->id, *type, 0, MOD_COMMAND);
		ksym = ksym2;
		break;

	case KS_Cmd1:
		update_modifier(sc->id, *type, 0, MOD_COMMAND1);
		break;

	case KS_Cmd2:
		update_modifier(sc->id, *type, 0, MOD_COMMAND2);
		break;
	}

	if (*type != WSCONS_EVENT_KEY_DOWN ||
	    (! MOD_ONESET(sc->id, MOD_COMMAND) &&
	     ! MOD_ALLSET(sc->id, MOD_COMMAND1 | MOD_COMMAND2)))
		return (0);

	switch (ksym) {
#if defined(DDB) || defined(KGDB)
	case KS_Cmd_Debugger:
		if (sc->sc_isconsole) {
#ifdef DDB
			console_debugger();
#endif
#ifdef KGDB
			kgdb_connect(1);
#endif
		}
		/* discard this key (ddb discarded command modifiers) */
		*type = WSCONS_EVENT_KEY_UP;
		return (1);
#endif

#if NWSDISPLAY > 0
	case KS_Cmd_Screen0:
	case KS_Cmd_Screen1:
	case KS_Cmd_Screen2:
	case KS_Cmd_Screen3:
	case KS_Cmd_Screen4:
	case KS_Cmd_Screen5:
	case KS_Cmd_Screen6:
	case KS_Cmd_Screen7:
	case KS_Cmd_Screen8:
	case KS_Cmd_Screen9:
		wsdisplay_switch(sc->sc_base.me_dispdv, ksym - KS_Cmd_Screen0, 0);
		return (1);
	case KS_Cmd_ResetEmul:
		wsdisplay_reset(sc->sc_base.me_dispdv, WSDISPLAY_RESETEMUL);
		return (1);
	case KS_Cmd_ResetClose:
		wsdisplay_reset(sc->sc_base.me_dispdv, WSDISPLAY_RESETCLOSE);
		return (1);
	case KS_Cmd_BacklightOn:
	case KS_Cmd_BacklightOff:
	case KS_Cmd_BacklightToggle:
		change_displayparam(sc, WSDISPLAYIO_PARAM_BACKLIGHT,
				    ksym == KS_Cmd_BacklightOff ? -1 : 1,
				    ksym == KS_Cmd_BacklightToggle ? 1 : 0);
		return (1);
	case KS_Cmd_BrightnessUp:
	case KS_Cmd_BrightnessDown:
	case KS_Cmd_BrightnessRotate:
		change_displayparam(sc, WSDISPLAYIO_PARAM_BRIGHTNESS,
				    ksym == KS_Cmd_BrightnessDown ? -1 : 1,
				    ksym == KS_Cmd_BrightnessRotate ? 1 : 0);
		return (1);
	case KS_Cmd_ContrastUp:
	case KS_Cmd_ContrastDown:
	case KS_Cmd_ContrastRotate:
		change_displayparam(sc, WSDISPLAYIO_PARAM_CONTRAST,
				    ksym == KS_Cmd_ContrastDown ? -1 : 1,
				    ksym == KS_Cmd_ContrastRotate ? 1 : 0);
		return (1);
#endif
	}
	return (0);
}

static int
wskbd_translate(struct wskbd_internal *id, u_int type, int value)
{
	struct wskbd_softc *sc = id->t_sc;
	keysym_t ksym, res, *group;
	struct wscons_keymap kpbuf, *kp;
	int iscommand = 0;

	if (type == WSCONS_EVENT_ALL_KEYS_UP) {
		id->t_modifiers &= ~(MOD_SHIFT_L | MOD_SHIFT_R
				| MOD_CONTROL_L | MOD_CONTROL_R
				| MOD_META_L | MOD_META_R
				| MOD_MODESHIFT
				| MOD_COMMAND | MOD_COMMAND1 | MOD_COMMAND2);
		update_leds(id);
		return (0);
	}

	if (sc != NULL) {
		if (value < 0 || value >= sc->sc_maplen) {
#ifdef DEBUG
			printf("wskbd_translate: keycode %d out of range\n",
			       value);
#endif
			return (0);
		}
		kp = sc->sc_map + value;
	} else {
		kp = &kpbuf;
		wskbd_get_mapentry(id->t_keymap, value, kp);
	}

	/* if this key has a command, process it first */
	if (sc != NULL && kp->command != KS_voidSymbol)
		iscommand = internal_command(sc, &type, kp->command,
					     kp->group1[0]);

	/* Now update modifiers */
	switch (kp->group1[0]) {
	case KS_Shift_L:
		update_modifier(id, type, 0, MOD_SHIFT_L);
		break;

	case KS_Shift_R:
		update_modifier(id, type, 0, MOD_SHIFT_R);
		break;

	case KS_Shift_Lock:
		update_modifier(id, type, 1, MOD_SHIFTLOCK);
		break;

	case KS_Caps_Lock:
		update_modifier(id, type, 1, MOD_CAPSLOCK);
		break;

	case KS_Control_L:
		update_modifier(id, type, 0, MOD_CONTROL_L);
		break;

	case KS_Control_R:
		update_modifier(id, type, 0, MOD_CONTROL_R);
		break;

	case KS_Alt_L:
		update_modifier(id, type, 0, MOD_META_L);
		break;

	case KS_Alt_R:
		update_modifier(id, type, 0, MOD_META_R);
		break;

	case KS_Mode_switch:
		update_modifier(id, type, 0, MOD_MODESHIFT);
		break;

	case KS_Num_Lock:
		update_modifier(id, type, 1, MOD_NUMLOCK);
		break;

#if NWSDISPLAY > 0
	case KS_Hold_Screen:
		if (sc != NULL) {
			update_modifier(id, type, 1, MOD_HOLDSCREEN);
			wskbd_holdscreen(sc, id->t_modifiers & MOD_HOLDSCREEN);
		}
		break;
#endif
	}

	/* If this is a key release or we are in command mode, we are done */
	if (type != WSCONS_EVENT_KEY_DOWN || iscommand) {
		update_leds(id);
		return (0);
	}

	/* Get the keysym */
	if (id->t_modifiers & MOD_MODESHIFT)
		group = & kp->group2[0];
	else
		group = & kp->group1[0];

	if ((id->t_modifiers & MOD_NUMLOCK) != 0 &&
	    KS_GROUP(group[1]) == KS_GROUP_Keypad) {
		if (MOD_ONESET(id, MOD_ANYSHIFT))
			ksym = group[0];
		else
			ksym = group[1];
	} else if (! MOD_ONESET(id, MOD_ANYSHIFT | MOD_CAPSLOCK)) {
		ksym = group[0];
	} else if (MOD_ONESET(id, MOD_CAPSLOCK)) {
		if (! MOD_ONESET(id, MOD_SHIFT_L | MOD_SHIFT_R))
			ksym = group[0];
		else
			ksym = group[1];
		if (ksym >= KS_a && ksym <= KS_z)
			ksym += KS_A - KS_a;
		else if (ksym >= KS_agrave && ksym <= KS_thorn &&
			 ksym != KS_division)
			ksym += KS_Agrave - KS_agrave;
	} else if (MOD_ONESET(id, MOD_ANYSHIFT)) {
		ksym = group[1];
	} else {
		ksym = group[0];
	}

	/* Process compose sequence and dead accents */
	res = KS_voidSymbol;

	switch (KS_GROUP(ksym)) {
	case KS_GROUP_Ascii:
	case KS_GROUP_Keypad:
	case KS_GROUP_Function:
		res = ksym;
		break;

	case KS_GROUP_Mod:
		if (ksym == KS_Multi_key) {
			update_modifier(id, 1, 0, MOD_COMPOSE);
			id->t_composelen = 2;
		}
		break;

	case KS_GROUP_Dead:
		if (id->t_composelen == 0) {
			update_modifier(id, 1, 0, MOD_COMPOSE);
			id->t_composelen = 1;
			id->t_composebuf[0] = ksym;
		} else
			res = ksym;
		break;
	}

	if (res == KS_voidSymbol) {
		update_leds(id);
		return (0);
	}

	if (id->t_composelen > 0) {
		id->t_composebuf[2 - id->t_composelen] = res;
		if (--id->t_composelen == 0) {
			res = wskbd_compose_value(id->t_composebuf);
			update_modifier(id, 0, 0, MOD_COMPOSE);
		} else {
			return (0);
		}
	}

	update_leds(id);

	/* We are done, return the symbol */
	if (KS_GROUP(res) == KS_GROUP_Ascii) {
		if (MOD_ONESET(id, MOD_ANYCONTROL)) {
			if ((res >= KS_at && res <= KS_z) || res == KS_space)
				res = res & 0x1f;
			else if (res == KS_2)
				res = 0x00;
			else if (res >= KS_3 && res <= KS_7)
				res = KS_Escape + (res - KS_3);
			else if (res == KS_8)
				res = KS_Delete;
		}
		if (MOD_ONESET(id, MOD_ANYMETA)) {
			if (id->t_flags & WSKFL_METAESC) {
				id->t_symbols[0] = KS_Escape;
				id->t_symbols[1] = res;
				return (2);
			} else
				res |= 0x80;
		}
	}

	id->t_symbols[0] = res;
	return (1);
}
