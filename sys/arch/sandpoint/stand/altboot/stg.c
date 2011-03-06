/* $NetBSD: stg.c,v 1.1 2011/03/06 13:55:12 phx Exp $ */

/*-
 * Copyright (c) 2011 Frank Wille.
 * All rights reserved.
 *
 * Written by Frank Wille for The NetBSD Project.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
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

#include <sys/param.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>

#include <lib/libsa/stand.h>
#include <lib/libsa/net.h>

#include "globals.h"

#define CSR_WRITE_1(l, r, v)	*(volatile uint8_t *)((l)->csr+(r)) = (v)
#define CSR_READ_1(l, r)	*(volatile uint8_t *)((l)->csr+(r))
#define CSR_WRITE_2(l, r, v)	out16rb((l)->csr+(r), (v))
#define CSR_READ_2(l, r)	in16rb((l)->csr+(r))
#define CSR_WRITE_4(l, r, v)	out32rb((l)->csr+(r), (v))
#define CSR_READ_4(l, r)	in32rb((l)->csr+(r))
#define VTOPHYS(va)		(uint32_t)(va)
#define DEVTOV(pa)		(uint32_t)(pa)
#define wbinv(adr, siz)		_wbinv(VTOPHYS(adr), (uint32_t)(siz))
#define inv(adr, siz)		_inv(VTOPHYS(adr), (uint32_t)(siz))
#define DELAY(n)		delay(n)
#define ALLOC(T,A)		(T *)allocaligned(sizeof(T),(A))

struct desc {
	uint64_t xd0, xd1, xd2;
};
/* xd1 */
#define RXLEN(x)		((x) & 0xffff)
#define RXERRORMASK		0x3f0000LL
#define TXNOALIGN		(1ULL << 16)
#define TXFRAGCOUNT(x)		(((uint64_t)((x) & 0xf)) << 48)
#define DONE			(1ULL << 31)
/* xd2 */
#define FRAGADDR(x)		((uint64_t)(x))
#define FRAGLEN(x)		(((uint64_t)((x) & 0xffff)) << 48)

#define STGE_DMACtrl		0x00
#define  DMAC_RxDMAComplete	(1U << 3)
#define  DMAC_RxDMAPollNow	(1U << 4)
#define  DMAC_TxDMAComplete	(1U << 11)
#define  DMAC_TxDMAPollNow	(1U << 12)
#define STGE_TFDListPtrLo	0x10
#define STGE_TFDListPtrHi	0x14
#define STGE_RFDListPtrLo	0x1c
#define STGE_RFDListPtrHi	0x20
#define STGE_AsicCtrl		0x30
#define  AC_PhyMedia		(1U << 7)
#define  AC_GlobalReset		(1U << 16)
#define  AC_RxReset		(1U << 17)
#define  AC_TxReset		(1U << 18)
#define  AC_DMA			(1U << 19) 
#define  AC_FIFO		(1U << 20)
#define  AC_Network		(1U << 21)
#define  AC_Host		(1U << 22)
#define  AC_AutoInit		(1U << 23)
#define  AC_RstOut		(1U << 24)
#define  AC_ResetBusy		(1U << 26)
#define STGE_EepromData		0x48
#define STGE_EepromCtrl		0x4a
#define  EC_EepromAddress(x)	((x) & 0xff)
#define  EC_EepromOpcode(x)	((x) << 8)
#define  EC_OP_RR		2
#define  EC_EepromBusy		(1U << 15)
#define STGE_IntEnable		0x5c
#define STGE_MACCtrl		0x6c
#define  MC_TxEnable		(1U << 24)
#define  MC_RxEnable		(1U << 27)
#define STGE_PhyCtrl		0x76
#define  PC_MgmtClk		(1U << 0)
#define  PC_MgmtData		(1U << 1)
#define  PC_MgmtDir		(1U << 2)
#define  PC_PhyDuplexPolarity	(1U << 3)
#define  PC_PhyDuplexStatus	(1U << 4)
#define  PC_PhyLnkPolarity	(1U << 5)
#define  PC_LinkSpeed(x)	(((x) >> 6) & 3)
#define  PC_LinkSpeed_Down	0
#define  PC_LinkSpeed_10	1
#define  PC_LinkSpeed_100	2
#define  PC_LinkSpeed_1000	3
#define STGE_StationAddress0	0x78
#define STGE_StationAddress1	0x7a
#define STGE_StationAddress2	0x7c

#define STGE_EEPROM_SA0		0x10

#define MII_PSSR		0x11	/* MAKPHY status register */
#define  PSSR_DUPLEX		0x2000	/* FDX */
#define  PSSR_RESOLVED		0x0800	/* speed and duplex resolved */
#define  PSSR_LINK		0x0400  /* link indication */
#define  PSSR_SPEED(x)		(((x) >> 14) & 0x3)
#define  SPEED10		0
#define  SPEED100		1
#define  SPEED1000 		2

#define FRAMESIZE	1536

struct local {
	struct desc txd[2];
	struct desc rxd[2];
	uint8_t rxstore[2][FRAMESIZE];
	unsigned csr, rx, tx, phy;
	uint16_t bmsr, anlpar;
	uint8_t phyctrl_saved;
};

static int mii_read(struct local *, int, int);
static void mii_write(struct local *, int, int, int);
static void mii_initphy(struct local *);
static void mii_dealan(struct local *, unsigned);
static void mii_bitbang_sync(struct local *);
static void mii_bitbang_send(struct local *, uint32_t, int);
static void mii_bitbang_clk(struct local *, uint8_t);
static int eeprom_wait(struct local *);

int
stg_match(unsigned tag, void *data)
{
	unsigned v;

	v = pcicfgread(tag, PCI_ID_REG);
	switch (v) {
	case PCI_DEVICE(0x13f0, 0x1023):	/* ST1023 */
		return 1;
	}
	return 0;
}

void *
stg_init(unsigned tag, void *data)
{
	struct local *l;
	struct desc *txd, *rxd;
	uint8_t *en;
	unsigned i;
	uint32_t reg;

	l = ALLOC(struct local, 32);		/* desc alignment */
	memset(l, 0, sizeof(struct local));
	l->csr = DEVTOV(pcicfgread(tag, 0x14));	/* first try mem space */
	if (l->csr == 0)
		l->csr = DEVTOV(PCI_XIOBASE + (pcicfgread(tag, 0x10) & ~01));

	/* reset the chip */
	reg = CSR_READ_4(l, STGE_AsicCtrl);
	CSR_WRITE_4(l, STGE_AsicCtrl, reg | AC_GlobalReset | AC_RxReset |
	    AC_TxReset | AC_DMA | AC_FIFO | AC_Network | AC_Host |
	    AC_AutoInit | ((reg & AC_PhyMedia) ? AC_RstOut : 0));
	DELAY(50000);
	for (i = 0; i < 1000; i++) {
		DELAY(5000);
		if ((CSR_READ_4(l, STGE_AsicCtrl) & AC_ResetBusy) == 0)
			break;
	}
	if (i >= 1000)
		printf("NIC reset failed to complete!\n");
	DELAY(1000);

	mii_initphy(l);

	/* read ethernet address */
	en = data;
	if (PCI_PRODUCT(pcicfgread(tag, PCI_ID_REG)) != 0x1023) {
		/* read from station address registers when not ST1023 */
		en[0] = CSR_READ_2(l, STGE_StationAddress0) & 0xff;
		en[1] = CSR_READ_2(l, STGE_StationAddress0) >> 8;
		en[2] = CSR_READ_2(l, STGE_StationAddress1) & 0xff;
		en[3] = CSR_READ_2(l, STGE_StationAddress1) >> 8;
		en[4] = CSR_READ_2(l, STGE_StationAddress2) & 0xff;
		en[5] = CSR_READ_2(l, STGE_StationAddress2) >> 8;
	} else {
		/* ST1023: read the address from the serial EEPROM */
		static uint8_t bad[2][6] = {
			{ 0x00,0x00,0x00,0x00,0x00,0x00 },
			{ 0xff,0xff,0xff,0xff,0xff,0xff }
		};
		uint16_t addr[3];

		for (i = 0; i < 3; i++) {
			if (eeprom_wait(l) != 0)
				printf("NIC: serial EEPROM is not ready!\n");
			CSR_WRITE_2(l, STGE_EepromCtrl,
			    EC_EepromAddress(STGE_EEPROM_SA0 + i) |
			    EC_EepromOpcode(EC_OP_RR));
			if (eeprom_wait(l) != 0)
				printf("NIC: serial EEPROM read time out!\n");
			addr[i] = le16toh(CSR_READ_2(l, STGE_EepromData));
		}
		(void)memcpy(en, addr, 6);

		/* try to read MAC from Flash, when EEPROM is empty/missing */
		if (memcmp(en, bad[0], 6) == 0 || memcmp(en, bad[1], 6) == 0)
			read_mac_from_flash(en);

		/* set the station address now */
		for (i = 0; i < 6; i++)
			CSR_WRITE_1(l, STGE_StationAddress0 + i, en[i]);
	}
	printf("MAC address %02x:%02x:%02x:%02x:%02x:%02x\n",
	    en[0], en[1], en[2], en[3], en[4], en[5]);

	DPRINTF(("PHY %d (%04x.%04x)\n", l->phy,
	    mii_read(l, l->phy, 2), mii_read(l, l->phy, 3)));

	mii_dealan(l, 5);

	reg = CSR_READ_1(l, STGE_PhyCtrl);
	switch (PC_LinkSpeed(reg)) {
	case PC_LinkSpeed_1000:
		printf("1000Mbps");
		break;
	case PC_LinkSpeed_100:
		printf("100Mbps");
		break;
	case PC_LinkSpeed_10:
		printf("10Mbps");
		break;
	}
	if (reg & PC_PhyDuplexStatus)
		printf("-FDX");
	printf("\n");

	/* setup descriptors */
	txd = &l->txd[0];
	txd[0].xd0 = htole64(VTOPHYS(&txd[1]));
	txd[0].xd1 = htole64(DONE);
	txd[1].xd0 = htole64(VTOPHYS(&txd[0]));
	txd[1].xd1 = htole64(DONE);
	rxd = &l->rxd[0];
	rxd[0].xd0 = htole64(VTOPHYS(&rxd[1]));
	rxd[0].xd2 = htole64(FRAGADDR(VTOPHYS(l->rxstore[0])) |
	    FRAGLEN(FRAMESIZE));
	rxd[1].xd0 = htole64(VTOPHYS(&rxd[0]));
	rxd[1].xd2 = htole64(FRAGADDR(VTOPHYS(l->rxstore[1])) |
	    FRAGLEN(FRAMESIZE));
	wbinv(l, sizeof(struct local));

	CSR_WRITE_2(l, STGE_IntEnable, 0);
	CSR_WRITE_4(l, STGE_TFDListPtrHi, 0);
	CSR_WRITE_4(l, STGE_TFDListPtrLo, VTOPHYS(txd));
	CSR_WRITE_4(l, STGE_RFDListPtrHi, 0);
	CSR_WRITE_4(l, STGE_RFDListPtrLo, VTOPHYS(rxd));
	CSR_WRITE_4(l, STGE_MACCtrl, MC_TxEnable | MC_RxEnable);
#if 0
	CSR_WRITE_4(l, STGE_DMACtrl, DMAC_RxDMAPollNow | DMAC_TxDMAPollNow);
#endif
	return l;
}

int
stg_send(void *dev, char *buf, unsigned len)
{
	struct local *l = dev;
	volatile struct desc *txd;
	unsigned loop;

	wbinv(buf, len);
	txd = &l->txd[l->tx];
	txd->xd1 = htole64(DONE);
	wbinv(txd, sizeof(struct desc));
	txd->xd2 = htole64(FRAGADDR(VTOPHYS(buf)) | FRAGLEN(len));
	txd->xd1 = htole64(DONE | TXNOALIGN | 0x400000 | TXFRAGCOUNT(1));
	txd->xd1 = htole64(TXNOALIGN | 0x400000 | TXFRAGCOUNT(1));
	wbinv(txd, sizeof(struct desc));
	CSR_WRITE_4(l, STGE_DMACtrl, DMAC_TxDMAPollNow);	/* XXX ? */
	loop = 100;
	do {
		if ((le64toh(txd->xd1) & DONE) != 0)
			goto done;
		DELAY(10);
		inv(txd, sizeof(struct desc));
	} while (--loop > 0);
	printf("xmit failed\n");
	return -1;
  done:
	l->tx ^= 1;
	return len;
}

int
stg_recv(void *dev, char *buf, unsigned maxlen, unsigned timo)
{
	struct local *l = dev;
	volatile struct desc *rxd;
	uint64_t sts;
	unsigned bound, len;
	uint8_t *ptr;

	bound = 1000 * timo;
  again:
	rxd = &l->rxd[l->rx];
	do {
		inv(rxd, sizeof(struct desc));
		sts = le64toh(rxd->xd1);
		if ((sts & DONE) != 0)
			goto gotone;
		DELAY(1000);	/* 1 milli second */
	} while (--bound > 0);
	errno = 0;
	return -1;
  gotone:
	if ((sts & RXERRORMASK) != 0) {
		rxd->xd1 = 0;
		wbinv(rxd, sizeof(struct desc));
		l->rx ^= 1;
		goto again;
	}
	len = RXLEN(sts);
	if (len > maxlen)
		len = maxlen;
	ptr = l->rxstore[l->rx];
	inv(ptr, len);
	memcpy(buf, ptr, len);
	rxd->xd1 = 0;
	wbinv(rxd, sizeof(struct desc));
	l->rx ^= 1;
	return len;
}

#define MIICMD_START	1
#define MIICMD_READ	2
#define MIICMD_WRITE	1
#define MIICMD_ACK	2

/* read the MII by bitbanging STGE_PhyCtrl */
static int
mii_read(struct local *l, int phy, int reg)
{
	int data, i;
	uint8_t v;

	/* initiate read access */
	data = 0;
	mii_bitbang_sync(l);
	mii_bitbang_send(l, MIICMD_START, 2);
	mii_bitbang_send(l, MIICMD_READ, 2);
	mii_bitbang_send(l, phy, 5);
	mii_bitbang_send(l, reg, 5);

	/* switch direction to PHY->host */
	v = l->phyctrl_saved;
	CSR_WRITE_1(l, STGE_PhyCtrl, v);
	DELAY(1);
	mii_bitbang_clk(l, v);
	if (CSR_READ_1(l, STGE_PhyCtrl) & PC_MgmtData)
		printf("MII: read error\n");
	mii_bitbang_clk(l, v);

	/* read data */
	for (i = 0; i < 16; i++) {
		data <<= 1;
		if ((CSR_READ_1(l, STGE_PhyCtrl) & PC_MgmtData) != 0)
			data |= 1;
		mii_bitbang_clk(l, v);
	}
	/* reset direction to host->PHY */
	CSR_WRITE_1(l, STGE_PhyCtrl, v | PC_MgmtDir);
	return data;
}

/* write the MII by bitbanging STGE_PhyCtrl */
static void
mii_write(struct local *l, int phy, int reg, int val)
{

	/* initiate write access */
	mii_bitbang_sync(l);
	mii_bitbang_send(l, MIICMD_START, 2);
	mii_bitbang_send(l, MIICMD_WRITE, 2);
	mii_bitbang_send(l, phy, 5);
	mii_bitbang_send(l, reg, 5);

	/* send data */
	mii_bitbang_send(l, MIICMD_ACK, 2);
	mii_bitbang_send(l, val, 16);

	CSR_WRITE_1(l, STGE_PhyCtrl, l->phyctrl_saved | PC_MgmtDir);
}

#define MII_BMCR	0x00	/* Basic mode control register (rw) */
#define  BMCR_RESET	0x8000	/* reset */
#define  BMCR_AUTOEN	0x1000	/* autonegotiation enable */
#define  BMCR_ISO	0x0400	/* isolate */
#define  BMCR_STARTNEG	0x0200	/* restart autonegotiation */
#define MII_BMSR	0x01	/* Basic mode status register (ro) */
#define  BMSR_ACOMP	0x0020	/* Autonegotiation complete */
#define  BMSR_LINK	0x0004	/* Link status */
#define MII_ANAR	0x04	/* Autonegotiation advertisement (rw) */
#define  ANAR_FC	0x0400	/* local device supports PAUSE */
#define  ANAR_TX_FD	0x0100	/* local device supports 100bTx FD */
#define  ANAR_TX	0x0080	/* local device supports 100bTx */
#define  ANAR_10_FD	0x0040	/* local device supports 10bT FD */
#define  ANAR_10	0x0020	/* local device supports 10bT */
#define  ANAR_CSMA	0x0001	/* protocol selector CSMA/CD */
#define MII_ANLPAR	0x05	/* Autonegotiation lnk partner abilities (rw) */

static void
mii_initphy(struct local *l)
{
	int phy, ctl, sts, bound;

	l->phyctrl_saved = CSR_READ_1(l, STGE_PhyCtrl) &
	    (PC_PhyDuplexPolarity | PC_PhyLnkPolarity);

	for (phy = 0; phy < 32; phy++) {
		ctl = mii_read(l, phy, MII_BMCR);
		sts = mii_read(l, phy, MII_BMSR);
		if (ctl != 0xffff && sts != 0xffff && sts != 0)
			goto found;
	}
	printf("MII: no PHY found\n");
	return;

  found:
	ctl = mii_read(l, phy, MII_BMCR);
	mii_write(l, phy, MII_BMCR, ctl | BMCR_RESET);

	bound = 100;
	do {
		DELAY(10);
		ctl = mii_read(l, phy, MII_BMCR);
		if (ctl == 0xffff) {
			printf("MII: PHY %d has died after reset\n", phy);
			return;
		}
	} while (bound-- > 0 && (ctl & BMCR_RESET));
	if (bound == 0)
		printf("PHY %d reset failed\n", phy);

	ctl &= ~BMCR_ISO;
	mii_write(l, phy, MII_BMCR, ctl);
	sts = mii_read(l, phy, MII_BMSR) |
	    mii_read(l, phy, MII_BMSR); /* read twice */
	l->phy = phy;
	l->bmsr = sts;
}

static void
mii_dealan(struct local *l, unsigned timo)
{
	unsigned anar, bound;

	anar = ANAR_TX_FD | ANAR_TX | ANAR_10_FD | ANAR_10 | ANAR_CSMA;
	mii_write(l, l->phy, MII_ANAR, anar);
	mii_write(l, l->phy, MII_BMCR, BMCR_AUTOEN | BMCR_STARTNEG);
	l->anlpar = 0;
	bound = getsecs() + timo;
	do {
		l->bmsr = mii_read(l, l->phy, MII_BMSR) |
		   mii_read(l, l->phy, MII_BMSR); /* read twice */
		if ((l->bmsr & BMSR_LINK) && (l->bmsr & BMSR_ACOMP)) {
			l->anlpar = mii_read(l, l->phy, MII_ANLPAR);
			break;
		}
		DELAY(10 * 1000);
	} while (getsecs() < bound);
}

static void
mii_bitbang_sync(struct local *l)
{
	int i;
	uint8_t v;

	v = l->phyctrl_saved | PC_MgmtDir | PC_MgmtData;
	CSR_WRITE_1(l, STGE_PhyCtrl, v);
	DELAY(1);
	for (i = 0; i < 32; i++)
		mii_bitbang_clk(l, v);
}

static void
mii_bitbang_send(struct local *l, uint32_t data, int nbits)
{
	uint32_t i;
	uint8_t v;

	v = l->phyctrl_saved | PC_MgmtDir;
	CSR_WRITE_1(l, STGE_PhyCtrl, v);
	DELAY(1);
	for (i = 1 << (nbits - 1); i != 0; i >>= 1) {
		if (data & i)
			v |= PC_MgmtData;
		else
			v &= ~PC_MgmtData;
		CSR_WRITE_1(l, STGE_PhyCtrl, v);
		DELAY(1);
		mii_bitbang_clk(l, v);
	}
}

static void
mii_bitbang_clk(struct local *l, uint8_t v)
{

	CSR_WRITE_1(l, STGE_PhyCtrl, v | PC_MgmtClk);
	DELAY(1);
	CSR_WRITE_1(l, STGE_PhyCtrl, v);
	DELAY(1);
}

static int
eeprom_wait(struct local *l)
{
	int i;

	for (i = 0; i < 1000; i++) {
		DELAY(1000);
		if ((CSR_READ_2(l, STGE_EepromCtrl) & EC_EepromBusy) == 0)
			return 0;
	}
	return 1;
}
