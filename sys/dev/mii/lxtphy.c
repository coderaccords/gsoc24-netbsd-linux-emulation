/*	$NetBSD: lxtphy.c,v 1.2 1998/11/02 22:31:37 thorpej Exp $	*/

/*-
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
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
 * Copyright (c) 1997 Manuel Bouyer.  All rights reserved.
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
 *	This product includes software developed by Manuel Bouyer.
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

/*
 * driver for Level One's LXT-970 ethernet 10/100 PHY
 * datasheet from www.level1.com
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <sys/malloc.h>
#include <sys/socket.h>

#include <net/if.h>
#include <net/if_media.h>

#include <dev/mii/mii.h>
#include <dev/mii/miivar.h>
#include <dev/mii/miidevs.h>

#include <dev/mii/lxtphyreg.h>

struct lxtphy_softc {
	struct mii_softc sc_mii;		/* generic PHY */
	int sc_capabilities;
	int sc_ticks;
	int sc_active;
};

int	lxtphymatch __P((struct device *, struct cfdata *, void *));
void	lxtphyattach __P((struct device *, struct device *, void *));

struct cfattach lxtphy_ca = {
	sizeof(struct lxtphy_softc), lxtphymatch, lxtphyattach
};

#define	LXTPHY_READ(sc, reg) \
    (*(sc)->sc_mii.mii_pdata->mii_readreg)((sc)->sc_mii.mii_dev.dv_parent, \
	(sc)->sc_mii.mii_phy, (reg))

#define	LXTPHY_WRITE(sc, reg, val) \
    (*(sc)->sc_mii.mii_pdata->mii_writereg)((sc)->sc_mii.mii_dev.dv_parent, \
	(sc)->sc_mii.mii_phy, (reg), (val))

int	lxtphy_service __P((struct mii_softc *, struct mii_data *, int));
void	lxtphy_reset __P((struct lxtphy_softc *));
void	lxtphy_auto __P((struct lxtphy_softc *));
void	lxtphy_status __P((struct lxtphy_softc *));

int
lxtphymatch(parent, match, aux)
	struct device *parent;
	struct cfdata *match;
	void *aux;
{
	struct mii_attach_args *ma = aux;

	if (MII_OUI(ma->mii_id1, ma->mii_id2) == MII_OUI_LEVEL1 &&
	    MII_MODEL(ma->mii_id2) == MII_MODEL_LEVEL1_LXT970)
		return (1);

	return (0);
}

void
lxtphyattach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct lxtphy_softc *sc = (struct lxtphy_softc *)self;
	struct mii_attach_args *ma = aux;
	struct mii_data *mii = ma->mii_data;

	printf(": %s, rev. %d\n", MII_STR_LEVEL1_LXT970,
	    MII_REV(ma->mii_id2));

	sc->sc_mii.mii_inst = mii->mii_instance;
	sc->sc_mii.mii_phy = ma->mii_phyno;
	sc->sc_mii.mii_service = lxtphy_service;
	sc->sc_mii.mii_pdata = mii;

#define	ADD(m, c)	ifmedia_add(&mii->mii_media, (m), (c), NULL)

	ADD(IFM_MAKEWORD(IFM_ETHER, IFM_NONE, 0, sc->sc_mii.mii_inst),
	    BMCR_ISO);
	ADD(IFM_MAKEWORD(IFM_ETHER, IFM_100_TX, IFM_LOOP, sc->sc_mii.mii_inst),
	    BMCR_LOOP|BMCR_S100);

	lxtphy_reset(sc);

	sc->sc_capabilities = LXTPHY_READ(sc, MII_BMSR) & ma->mii_capmask;
	printf("%s: ", sc->sc_mii.mii_dev.dv_xname);
	if ((sc->sc_capabilities & BMSR_MEDIAMASK) == 0)
		printf("no media present");
	else
		mii_add_media(mii, sc->sc_capabilities, sc->sc_mii.mii_inst);
	printf("\n");
#undef ADD
}

int
lxtphy_service(self, mii, cmd)
	struct mii_softc *self;
	struct mii_data *mii;
	int cmd;
{
	struct lxtphy_softc *sc = (struct lxtphy_softc *)self;
	struct ifmedia_entry *ife = mii->mii_media.ifm_cur;
	int reg;

	switch (cmd) {
	case MII_POLLSTAT:
		/*
		 * If we're not polling our PHY instance, just return.
		 */
		if (IFM_INST(ife->ifm_media) != sc->sc_mii.mii_inst)
			return (0);
		break;

	case MII_MEDIACHG:
		/*
		 * If the media indicates a different PHY instance,
		 * isolate ourselves.
		 */
		if (IFM_INST(ife->ifm_media) != sc->sc_mii.mii_inst) {
			reg = LXTPHY_READ(sc, MII_BMCR);
			LXTPHY_WRITE(sc, MII_BMCR, reg | BMCR_ISO);
			return (0);
		}

		/*
		 * If the interface is not up, don't do anything.
		 */
		if ((mii->mii_ifp->if_flags & IFF_UP) == 0)
			break;

		switch (IFM_SUBTYPE(ife->ifm_media)) {
		case IFM_AUTO:
			/*
			 * If we're already in auto mode, just return.
			 */
			if (LXTPHY_READ(sc, MII_BMCR) & BMCR_AUTOEN)
				return (0);
			lxtphy_auto(sc);
			break;
		case IFM_100_T4:
			/*
			 * XXX Not supported as a manual setting right now.
			 */
			return (EINVAL);
		default:
			/*
			 * BMCR data is stored in the ifmedia entry.
			 */
			LXTPHY_WRITE(sc, MII_ANAR, mii_anar(ife->ifm_media));
			LXTPHY_WRITE(sc, MII_BMCR, ife->ifm_data);
		}
		break;

	case MII_TICK:
		/*
		 * If we're not currently selected, just return.
		 */
		if (IFM_INST(ife->ifm_media) != sc->sc_mii.mii_inst)
			return (0);

		/*
		 * Only used for autonegotiation.
		 */
		if (IFM_SUBTYPE(ife->ifm_media) != IFM_AUTO)
			return (0);

		/*
		 * Is the interface even up?
		 */
		if ((mii->mii_ifp->if_flags & IFF_UP) == 0)
			return (0);

		/*
		 * Check to see if we have link.  If we do, we don't
		 * need to restart the autonegotiation process.  Use
		 * the LXT CSR instead of the BMSR, since the CSR's
		 * link indication is dynamic, not latched, so only
		 * one register read is required.
		 */
		reg = LXTPHY_READ(sc, MII_LXTPHY_CSR);
		if (reg & CSR_LINK)
			return (0);

		/*
		 * Only retry autonegotiation every 5 seconds.
		 */
		if (++sc->sc_ticks != 5)
			return (0);

		sc->sc_ticks = 0;
		lxtphy_reset(sc);
		lxtphy_auto(sc);
		break;
	}

	/* Update the media status. */
	lxtphy_status(sc);

	/* Callback if something changed. */
	if (sc->sc_active != mii->mii_media_active || cmd == MII_MEDIACHG) {
		(*mii->mii_statchg)(sc->sc_mii.mii_dev.dv_parent);
		sc->sc_active = mii->mii_media_active;
	}
	return (0);
}

void
lxtphy_status(sc)
	struct lxtphy_softc *sc;
{
	struct mii_data *mii = sc->sc_mii.mii_pdata;
	int bmcr, csr;

	mii->mii_media_status = IFM_AVALID;
	mii->mii_media_active = IFM_ETHER;

	/*
	 * Get link status from the CSR; we need to read the CSR
	 * for media type anyhow, and the link status in the CSR
	 * doens't latch, so fewer register reads are required.
	 */
	csr = LXTPHY_READ(sc, MII_LXTPHY_CSR);
	if (csr & CSR_LINK)
		mii->mii_media_status |= IFM_ACTIVE;

	bmcr = LXTPHY_READ(sc, MII_BMCR);
	if (bmcr & BMCR_ISO) {
		mii->mii_media_active |= IFM_NONE;
		mii->mii_media_status = 0;
		return;
	}

	if (bmcr & BMCR_LOOP)
		mii->mii_media_active |= IFM_LOOP;

	if ((bmcr & BMCR_AUTOEN) && (csr & CSR_ACOMP) == 0) {
		/* Erg, still trying, I guess... */
		mii->mii_media_active |= IFM_NONE;
		return;
	}

	if (csr & CSR_SPEED)
		mii->mii_media_active |= IFM_100_TX;
	else
		mii->mii_media_active |= IFM_10_T;
	if (csr & CSR_DUPLEX)
		mii->mii_media_active |= IFM_FDX;
}

void
lxtphy_auto(sc)
	struct lxtphy_softc *sc;
{
	int csr, i;

	LXTPHY_WRITE(sc, MII_ANAR,
	    BMSR_MEDIA_TO_ANAR(sc->sc_capabilities) | ANAR_CSMA);
	LXTPHY_WRITE(sc, MII_BMCR, BMCR_AUTOEN | BMCR_STARTNEG);

	/* Wait 500ms for it to complete. */
	for (i = 0; i < 500; i++) {
		/*
		 * Again, use the LXT CSR rather than BMSR, since it
		 * doesn't latch.
		 */
		if ((csr = LXTPHY_READ(sc, MII_LXTPHY_CSR)) & CSR_ACOMP)
			return;
		delay(1000);
	}
#if 0
	if ((csr & CSR_ACOMP) == 0)
		printf("%s: autonegotiation failed to complete\n",
		    sc->sc_mii.mii_dev.dv_xname);
#endif
}

void
lxtphy_reset(sc)
	struct lxtphy_softc *sc;
{
	int reg, i;

	LXTPHY_WRITE(sc, MII_BMCR, BMCR_RESET|BMCR_ISO);

	/* Wait 100ms for it to complete. */
	for (i = 0; i < 100; i++) {
		reg = LXTPHY_READ(sc, MII_BMCR);
		if ((reg & BMCR_RESET) == 0)
			break;
		delay(1000);
	}

	/* Make sure the PHY is isolated. */
	if (sc->sc_mii.mii_inst != 0)
		LXTPHY_WRITE(sc, MII_BMCR, reg | BMCR_ISO);
}
