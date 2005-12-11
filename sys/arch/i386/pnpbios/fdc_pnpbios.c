/*	$NetBSD: fdc_pnpbios.c,v 1.9 2005/12/11 12:17:47 christos Exp $	*/

/*-
 * Copyright (c) 2000 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe.
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
 *	This product includes software developed by the NetBSD
 *	Foundation, Inc. and its contributors.
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

/*
 * PNPBIOS attachment for the PC Floppy Controller driver.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: fdc_pnpbios.c,v 1.9 2005/12/11 12:17:47 christos Exp $");

#include "rnd.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/callout.h>
#include <sys/device.h>
#include <sys/buf.h>
#include <sys/queue.h>
#if NRND > 0
#include <sys/rnd.h>
#endif

#include <machine/bus.h>
#include <machine/intr.h>

#include <dev/isa/isavar.h>
#include <dev/isa/isadmavar.h>

#include <dev/isa/fdcvar.h>

#include <i386/pnpbios/pnpbiosvar.h>

int	fdc_pnpbios_match(struct device *, struct cfdata *, void *);
void	fdc_pnpbios_attach(struct device *, struct device *, void *);

struct fdc_pnpbios_softc {
        struct fdc_softc sc_fdc;        /* base fdc device */

        bus_space_handle_t sc_baseioh;  /* base I/O handle */
};


CFATTACH_DECL(fdc_pnpbios, sizeof(struct fdc_pnpbios_softc),
    fdc_pnpbios_match, fdc_pnpbios_attach, NULL, NULL);

int
fdc_pnpbios_match(struct device *parent,
    struct cfdata *match,
    void *aux)
{
	struct pnpbiosdev_attach_args *aa = aux;

	if (strcmp(aa->idstr, "PNP0700") == 0)
		return (1);

	return (0);
}

void
fdc_pnpbios_attach(struct device *parent,
    struct device *self,
    void *aux)
{
	struct fdc_softc *fdc = (void *) self;
        struct fdc_pnpbios_softc *pdc = (void *) self;
	struct pnpbiosdev_attach_args *aa = aux;
        int size, base;
        
	printf("\n");

	fdc->sc_ic = aa->ic;

	if (pnpbios_io_map(aa->pbt, aa->resc, 0, &fdc->sc_iot,
            &pdc->sc_baseioh)) {
		printf("%s: unable to map I/O space\n", fdc->sc_dev.dv_xname);
		return;
	}

	/* 
         * Start accounting for "odd" pnpbios's. Some probe as 4 ports,
         * some as 6 and some don't give the ctl port back. 
         */

        if (pnpbios_getiosize(aa->pbt, aa->resc, 0, &size)) {
                printf("%s: can't get iobase size\n", fdc->sc_dev.dv_xname);
                return;
        }

        switch (size) {

        /* Easy case. This matches right up with what the fdc code expects. */
        case 4:
                fdc->sc_ioh = pdc->sc_baseioh;
                break;

        /* Map a subregion so this all lines up with the fdc code. */
        case 6:
                if (bus_space_subregion(fdc->sc_iot, pdc->sc_baseioh, 2, 4,
                    &fdc->sc_ioh)) {
                        printf("%s: unable to subregion I/O space\n",
                            fdc->sc_dev.dv_xname);
                        return;
                }
                break;
        default:
                printf ("%s: unknown size: %d of io mapping\n",
                    fdc->sc_dev.dv_xname, size);
                return;
        }
        
        /* 
         * XXX: This is bad. If the pnpbios claims only 1 I/O range then it's
         * omitting the controller I/O port. (One has to exist for there to
         * be a working fdc). Just try and force the mapping in. 
         */

        if (aa->resc->numio == 1) {

                if (pnpbios_getiobase(aa->pbt, aa->resc, 0, 0, &base)) {
                        printf ("%s: can't get iobase\n",
                            fdc->sc_dev.dv_xname);
                        return;
                }
                if (bus_space_map(fdc->sc_iot, base + size + 1, 1, 0,
                    &fdc->sc_fdctlioh)) {
                        printf("%s: unable to force map CTL I/O space\n", 
                            fdc->sc_dev.dv_xname);
                        return;
                }
        } else if (pnpbios_io_map(aa->pbt, aa->resc, 1, &fdc->sc_iot,
            &fdc->sc_fdctlioh)) {
                printf("%s: unable to map CTL I/O space\n",
		    fdc->sc_dev.dv_xname);
		return;
	}

	if (pnpbios_getdmachan(aa->pbt, aa->resc, 0, &fdc->sc_drq)) {
		printf("%s: unable to get DMA channel\n",
		    fdc->sc_dev.dv_xname);
		return;
	}

	pnpbios_print_devres(self, aa);
        if (aa->resc->numio == 1)
                printf("%s: ctl io %x didn't probe. Forced attach\n",
                    fdc->sc_dev.dv_xname, base + size + 1);

	fdc->sc_ih = pnpbios_intr_establish(aa->pbt, aa->resc, 0, IPL_BIO,
	    fdcintr, fdc);

	fdcattach(fdc);
}
