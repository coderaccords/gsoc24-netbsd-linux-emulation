/*	$NetBSD: mcclock_ibus.c,v 1.17 2011/02/20 07:50:24 matt Exp $	*/

/*
 * Copyright (c) 1994, 1995, 1996 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: Chris G. Demetriou
 *
 * Permission to use, copy, modify and distribute this software and
 * its documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND
 * FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */

#include <sys/cdefs.h>			/* RCS ID & Copyright macro defns */
__KERNEL_RCSID(0, "$NetBSD: mcclock_ibus.c,v 1.17 2011/02/20 07:50:24 matt Exp $");

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <sys/systm.h>
#include <dev/clock_subr.h>

#include <dev/dec/mcclockvar.h>
#include <dev/dec/mcclock_pad32.h>

#include <pmax/ibus/ibusvar.h>

static int	mcclock_ibus_match(device_t, cfdata_t, void *);
static void	mcclock_ibus_attach(device_t, device_t, void *);

CFATTACH_DECL(mcclock_ibus, sizeof (struct mcclock_pad32_softc),
    mcclock_ibus_match, mcclock_ibus_attach, NULL, NULL);

static int
mcclock_ibus_match(device_t parent, cfdata_t cf, void *aux)
{
	struct ibus_attach_args *ia = aux;

	if (strcmp("mc146818", ia->ia_name) != 0)
		return (0);

	if (badaddr((void *)ia->ia_addr, sizeof(uint32_t)))
		return (0);

	return (1);
}

static void
mcclock_ibus_attach(device_t parent, device_t self, void *aux)
{
	struct ibus_attach_args *ia = aux;
	struct mcclock_pad32_softc *sc = device_private(self);

	sc->sc_dp = (struct mcclock_pad32_clockdatum*)ia->ia_addr;

	/* Attach MI driver, using busfns with TC-style register padding */
	mcclock_attach(&sc->sc_mcclock, &mcclock_pad32_busfns);
}
