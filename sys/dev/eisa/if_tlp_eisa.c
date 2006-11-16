/*	$NetBSD: if_tlp_eisa.c,v 1.18 2006/11/16 01:32:50 christos Exp $	*/

/*-
 * Copyright (c) 1999, 2000 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe of the Numerical Aerospace Simulation Facility,
 * NASA Ames Research Center.
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
 * EISA bus front-end for the Digital Semiconductor ``Tulip'' (21x4x)
 * Ethernet controller family driver.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: if_tlp_eisa.c,v 1.18 2006/11/16 01:32:50 christos Exp $");

#include "opt_inet.h"
#include "bpfilter.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/errno.h>
#include <sys/device.h>

#include <machine/endian.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_media.h>
#include <net/if_ether.h>

#if NBPFILTER > 0
#include <net/bpf.h>
#endif

#ifdef INET
#include <netinet/in.h>
#include <netinet/if_inarp.h>
#endif


#include <machine/bus.h>
#include <machine/intr.h>

#include <dev/mii/miivar.h>
#include <dev/mii/mii_bitbang.h>

#include <dev/ic/tulipreg.h>
#include <dev/ic/tulipvar.h>

#include <dev/eisa/eisareg.h>
#include <dev/eisa/eisavar.h>
#include <dev/eisa/eisadevs.h>

#include <dev/pci/pcireg.h>

/*
 * DE425 configuration registers; literal offsets from CSR base.
 * This is effectively the 21040 PCI configuration space interleaved
 * into the CSR space (CSRs are space 16 bytes on the DE425).
 *
 * What a cool address decoder hack they must have on that board...
 */
#define	DE425_CFID		0x08	/* Configuration ID */
#define	DE425_CFCS		0x0c	/* Configuration Command-Status */
#define	DE425_CFRV		0x18	/* Configuration Revision */
#define	DE425_CFLT		0x1c	/* Configuration Latency Timer */
#define	DE425_CBIO		0x28	/* Configuration Base I/O Address */
#define	DE425_CFDA		0x2c	/* Configuration Driver Area */
#define	DE425_ENETROM		0xc90	/* Offset in I/O space for ENETROM */
#define	DE425_CFG0		0xc88	/* IRQ Configuration Register */

struct tulip_eisa_softc {
	struct tulip_softc sc_tulip;	/* real Tulip softc */

	/* EISA-specific goo. */
	void	*sc_ih;			/* interrupt handle */
};

static int	tlp_eisa_match(struct device *, struct cfdata *, void *);
static void	tlp_eisa_attach(struct device *, struct device *, void *);

CFATTACH_DECL(tlp_eisa, sizeof(struct tulip_eisa_softc),
    tlp_eisa_match, tlp_eisa_attach, NULL, NULL);

static const int tlp_eisa_irqs[] = { 5, 9, 10, 11 };

static const struct tulip_eisa_product {
	const char	*tep_eisaid;	/* EISA ID */
	const char	*tep_name;	/* device name */
	tulip_chip_t	tep_chip;	/* base Tulip chip type */
} tlp_eisa_products[] = {
	{ "DEC4250",			"DEC DE425",
	  TULIP_CHIP_DE425 },

	{ NULL,				NULL,
	  TULIP_CHIP_INVALID },
};

static const struct tulip_eisa_product *
tlp_eisa_lookup(const struct eisa_attach_args *ea)
{
	const struct tulip_eisa_product *tep;

	for (tep = tlp_eisa_products;
	     tep->tep_chip != TULIP_CHIP_INVALID; tep++)
		if (strcmp(ea->ea_idstring, tep->tep_eisaid) == 0)
			return (tep);
	return (NULL);
}

static int
tlp_eisa_match(struct device *parent, struct cfdata *match,
    void *aux)
{
	struct eisa_attach_args *ea = aux;

	if (tlp_eisa_lookup(ea) != NULL)
		return (1);

	return (0);
}

static void
tlp_eisa_attach(struct device *parent, struct device *self, void *aux)
{
	static const u_int8_t testpat[] =
	    { 0xff, 0, 0x55, 0xaa, 0xff, 0, 0x55, 0xaa };
	struct tulip_eisa_softc *esc = device_private(self);
	struct tulip_softc *sc = &esc->sc_tulip;
	struct eisa_attach_args *ea = aux;
	eisa_chipset_tag_t ec = ea->ea_ec;
	eisa_intr_handle_t ih;
	bus_space_tag_t iot = ea->ea_iot;
	bus_space_handle_t ioh;
	const char *intrstr;
	const struct tulip_eisa_product *tep;
	u_int8_t enaddr[ETHER_ADDR_LEN], tmpbuf[sizeof(testpat)];
	u_int32_t val;
	int irq, i, cnt;

	/*
	 * Map the device.
	 */
	if (bus_space_map(iot, EISA_SLOT_ADDR(ea->ea_slot),
	    EISA_SLOT_SIZE, 0, &ioh)) {
		printf(": unable to map I/O space\n");
		return;
	}

	sc->sc_st = iot;
	sc->sc_sh = ioh;

	tep = tlp_eisa_lookup(ea);
	if (tep == NULL) {
		printf("\n");
		panic("tlp_eisa_attach: impossible");
	}
	sc->sc_chip = tep->tep_chip;

	/*
	 * DE425's registers are 16 bytes long; the PCI configuration
	 * space registers are interleaved in the I/O space.
	 */
	sc->sc_regshift = 4;

	/*
	 * No power management hooks.
	 */
	sc->sc_flags |= TULIPF_ENABLED;

	/*
	 * CBIO must map the EISA slot, and I/O access and Bus Mastering
	 * must be enabled.
	 */
	bus_space_write_4(iot, ioh, DE425_CBIO, EISA_SLOT_ADDR(ea->ea_slot));
	bus_space_write_4(iot, ioh, DE425_CFCS,
	    bus_space_read_4(iot, ioh, DE425_CFCS) |
	    PCI_COMMAND_IO_ENABLE | PCI_COMMAND_MASTER_ENABLE);

	/*
	 * Get revision info.
	 */
	sc->sc_rev = bus_space_read_4(iot, ioh, DE425_CFRV) & 0xff;

	printf(": %s Ethernet, pass %d.%d\n",
	    tep->tep_name, (sc->sc_rev >> 4) & 0xf, sc->sc_rev & 0xf);

	sc->sc_dmat = ea->ea_dmat;

	/*
	 * EISA doesn't have a cache line register.
	 */
	sc->sc_cacheline = 0;

	/*
	 * Find the beginning of the Ethernet Address ROM.
	 */
	for (i = 0, cnt = 0; i < sizeof(testpat) && cnt < 32; cnt++) {
		tmpbuf[i] = bus_space_read_1(iot, ioh, DE425_ENETROM);
		if (tmpbuf[i] == testpat[i])
			i++;
		else
			i = 0;
	}

	/*
	 * ...and now read the contents of the Ethernet Address ROM.
	 */
	sc->sc_srom = malloc(32, M_DEVBUF, M_WAITOK|M_ZERO);
	for (i = 0; i < 32; i++)
		sc->sc_srom[i] = bus_space_read_1(iot, ioh, DE425_ENETROM);

	/*
	 * None of the DE425 boards have the new-style SROMs.
	 */
	if (tlp_parse_old_srom(sc, enaddr) == 0) {
		printf("%s: unable to decode old-style SROM\n",
		    sc->sc_dev.dv_xname);
		return;
	}

	/*
	 * All DE425 boards use the 21040 media switch.
	 */
	sc->sc_mediasw = &tlp_21040_mediasw;

	/*
	 * Figure out which IRQ we want to use, and determine of it's
	 * edge- or level-triggered.
	 */
	val = bus_space_read_4(iot, ioh, DE425_CFG0);
	irq = tlp_eisa_irqs[(val >> 1) & 0x03];

	/*
	 * Map and establish our interrupt.
	 */
	if (eisa_intr_map(ec, irq, &ih)) {
		printf("%s: unable to map interrupt (%u)\n",
		    sc->sc_dev.dv_xname, irq);
		return;
	}
	intrstr = eisa_intr_string(ec, ih);
	esc->sc_ih = eisa_intr_establish(ec, ih,
	    (val & 0x01) ? IST_EDGE : IST_LEVEL, IPL_NET, tlp_intr, sc);
	if (esc->sc_ih == NULL) {
		printf("%s: unable to establish interrupt",
		    sc->sc_dev.dv_xname);
		if (intrstr != NULL)
			printf(" at %s", intrstr);
		printf("\n");
		return;
	}
	if (intrstr != NULL)
		printf("%s: interrupting at %s\n", sc->sc_dev.dv_xname,
		    intrstr);

	/*
	 * Finish off the attach.
	 */
	tlp_attach(sc, enaddr);
}
