/*	$NetBSD: if_le.c,v 1.11 1996/04/18 00:25:20 cgd Exp $	*/

/*-
 * Copyright (c) 1995 Charles M. Hannum.  All rights reserved.
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Ralph Campbell and Rick Macklem.
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
 *	@(#)if_le.c	8.2 (Berkeley) 11/16/93
 */

#include "bpfilter.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/syslog.h>
#include <sys/socket.h>
#include <sys/device.h>

#include <net/if.h>

#ifdef INET
#include <netinet/in.h>
#include <netinet/if_ether.h>
#endif

#include <machine/autoconf.h>

#include <dev/tc/tcvar.h>
#include <dev/tc/ioasicvar.h>
#include <dev/tc/if_levar.h>

#ifdef pmax
#define wbflush() MachEmptyWriteBuffer()

/* This should be in a header file, but where? */
extern struct cfdriver mainbus_cd;	/* XXX really 3100/5100 b'board */

#include <pmax/pmax/kn01.h>
#include <machine/machConst.h>
#include <pmax/pmax/asic.h>

#else /* Alpha */
#include <machine/rpb.h>
#endif  /* Alpha */


#include <dev/ic/am7990reg.h>
#define LE_NEED_BUF_CONTIG
#define LE_NEED_BUF_GAP2
#define LE_NEED_BUF_GAP16
#include <dev/ic/am7990var.h>

/* access LANCE registers */
void lewritereg();
#define	LERDWR(cntl, src, dst)	{ (dst) = (src); wbflush(); }
#define	LEWREG(src, dst)	lewritereg(&(dst), (src))

#define LE_OFFSET_RAM		0x0
#define LE_OFFSET_LANCE		0x100000
#define LE_OFFSET_ROM		0x1c0000

extern caddr_t le_iomem;

#define	LE_SOFTC(unit)	le_cd.cd_devs[unit]
#define	LE_DELAY(x)	DELAY(x)

int le_tc_match __P((struct device *, void *, void *));
void le_tc_attach __P((struct device *, struct device *, void *));

int leintr __P((void *sc));


struct cfattach le_tc_ca = {
	sizeof(struct le_softc), le_tc_match, le_tc_attach
};

struct cfdriver le_cd = {
	NULL, "le", DV_IFNET
};

integrate void
lewrcsr(sc, port, val)
	struct le_softc *sc;
	u_int16_t port, val;
{
	struct lereg1 *ler1 = sc->sc_r1;

	LEWREG(port, ler1->ler1_rap);
	LERDWR(port, val, ler1->ler1_rdp);
}

integrate u_int16_t
lerdcsr(sc, port)
	struct le_softc *sc;
	u_int16_t port;
{
	struct lereg1 *ler1 = sc->sc_r1;
	u_int16_t val;

	LEWREG(port, ler1->ler1_rap);
	LERDWR(0, ler1->ler1_rdp, val);
	return (val);
}

int
le_tc_match(parent, match, aux)
	struct device *parent;
	void *match, *aux;
{
	struct cfdata *cf = match;
	struct confargs *ca = aux;
#ifdef notdef /* XXX */
	struct tc_cfloc *tc_locp;
	struct asic_cfloc *asic_locp;
#endif

#ifdef notdef /* XXX */
	tclocp = (struct tc_cfloc *)cf->cf_loc;
#endif

	/* XXX CHECK BUS */
	/* make sure that we're looking for this type of device. */
	if (!TC_BUS_MATCHNAME(ca, "PMAD-BA ") && /* untested alpha TC option */
	    !TC_BUS_MATCHNAME(ca, "PMAD-AA ") && /* KN02 b'board, old option */
	    !TC_BUS_MATCHNAME(ca, "lance"))	/* NetBSD name for b'board  */
		return (0);

#ifdef notdef /* XXX */
	/* make sure the unit matches the cfdata */
	if ((cf->cf_unit != tap->ta_unit &&
	     tap->ta_unit != TA_ANYUNIT) ||
	    (tclocp->cf_slot != tap->ta_slot &&
	     tclocp->cf_slot != TC_SLOT_WILD) ||
	    (tclocp->cf_offset != tap->ta_offset &&
	     tclocp->cf_offset != TC_OFFSET_WILD))
		return (0);
#endif

	return (1);
}

void
le_tc_attach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	register struct le_softc *sc = (void *)self;
	struct confargs *ca = aux;
	u_char *cp;	/* pointer to MAC address */
	int i;

	if (parent->dv_cfdata->cf_driver == &ioasic_cd) {
		/* It's on the system IOCTL ASIC */
		volatile u_int *ldp;
		tc_addr_t dma_mask;

		sc->sc_r1 = (struct lereg1 *)
		    MACH_PHYS_TO_UNCACHED(ca->ca_addr);
#ifdef alpha
		sc->sc_r1 = TC_DENSE_TO_SPARSE(sc->sc_r1);
#endif
		sc->sc_mem = (void *)MACH_PHYS_TO_UNCACHED(le_iomem);
/* XXX */	cp = (u_char *)IOASIC_SYS_ETHER_ADDRESS(ioasic_base);

		sc->sc_copytodesc = am7990_copytobuf_gap2;
		sc->sc_copyfromdesc = am7990_copyfrombuf_gap2;
		sc->sc_copytobuf = am7990_copytobuf_gap16;
		sc->sc_copyfrombuf = am7990_copyfrombuf_gap16;
		sc->sc_zerobuf = am7990_zerobuf_gap16;

		/*
		 * And enable Lance dma through the asic.
		 */
		ldp = (volatile u_int *) (IOASIC_REG_LANCE_DMAPTR(ioasic_base));
		dma_mask = ((tc_addr_t)le_iomem << 3);
#ifdef alpha
		/* Set upper 64 bits of DMA mask */
		dma_mask  = (dma_mask & ~(tc_addr_t)0x1f) |
			(((tc_addr_t)le_iomem >> 29) & 0x1f);
#endif /*alpha*/
		*ldp = dma_mask;
		*(volatile u_int *)IOASIC_REG_CSR(ioasic_base) |=
		    IOASIC_CSR_DMAEN_LANCE;
		wbflush();
	}
	else
	if (parent->dv_cfdata->cf_driver == &tc_cd) {
		/* It's on the turbochannel proper, or on KN02 baseboard. */
		sc->sc_r1 = (struct lereg1 *)
		    (ca->ca_addr + LE_OFFSET_LANCE);
		sc->sc_mem = (void *)
		    (ca->ca_addr + LE_OFFSET_RAM);
		cp = (u_char *)(ca->ca_addr + LE_OFFSET_ROM + 2);

		sc->sc_copytodesc = am7990_copytobuf_contig;
		sc->sc_copyfromdesc = am7990_copyfrombuf_contig;
		sc->sc_copytobuf = am7990_copytobuf_contig;
		sc->sc_copyfrombuf = am7990_copyfrombuf_contig;
		sc->sc_zerobuf = am7990_zerobuf_contig;
	}
#ifdef pmax
	 else if (parent->dv_cfdata->cf_driver == &mainbus_cd) {
		/* It's on the baseboard, attached directly to mainbus. */

		sc->sc_r1 = (struct lereg1 *)(ca->ca_addr);
/*XXX*/		sc->sc_mem = (void *)MACH_PHYS_TO_UNCACHED(0x19000000);
/*XXX*/		cp = (u_char *)(MACH_PHYS_TO_UNCACHED(KN01_SYS_CLOCK) + 1);

		sc->sc_copytodesc = am7990_copytobuf_gap2;
		sc->sc_copyfromdesc = am7990_copyfrombuf_gap2;
		sc->sc_copytobuf = am7990_copytobuf_gap2;
		sc->sc_copyfrombuf = am7990_copyfrombuf_gap2;
		sc->sc_zerobuf = am7990_zerobuf_gap2;
	}
#endif

	sc->sc_conf3 = 0;
	sc->sc_addr = 0;
	sc->sc_memsize = 65536;

	/*
	 * Get the ethernet address out of rom
	 */
	for (i = 0; i < sizeof(sc->sc_arpcom.ac_enaddr); i++) {
		sc->sc_arpcom.ac_enaddr[i] = *cp;
		cp += 4;
	}

	sc->sc_arpcom.ac_if.if_name = le_cd.cd_name;
	leconfig(sc);


	BUS_INTR_ESTABLISH(ca, leintr, sc);

	if (parent->dv_cfdata->cf_driver == &ioasic_cd) {
		/* XXX YEECH!!! */
		*(volatile u_int *)IOASIC_REG_IMSK(ioasic_base) |=
			IOASIC_INTR_LANCE;
		wbflush();
	}
}

/*
 * Write a lance register port, reading it back to ensure success. This seems
 * to be necessary during initialization, since the chip appears to be a bit
 * pokey sometimes.
 */
void
lewritereg(regptr, val)
	register volatile u_short *regptr;
	register u_short val;
{
	register int i = 0;

	while (*regptr != val) {
		*regptr = val;
		wbflush();
		if (++i > 10000) {
			printf("le: Reg did not settle (to x%x): x%x\n", val,
			    *regptr);
			return;
		}
		DELAY(100);
	}
}

/*
 * Routines for accessing the transmit and receive buffers are provided
 * by am7990.c, because of the LE_NEED_BUF_* macros defined above.
 * Unfortunately, CPU addressing of these buffers is done in one of
 * 3 ways:
 * - contiguous (for the 3max and turbochannel option card)
 * - gap2, which means shorts (2 bytes) interspersed with short (2 byte)
 *   spaces (for the pmax)
 * - gap16, which means 16bytes interspersed with 16byte spaces
 *   for buffers which must begin on a 32byte boundary (for 3min and maxine)
 * The buffer offset is the logical byte offset, assuming contiguous storage.
 */

#include <dev/ic/am7990.c>
