/*	$NetBSD: rtl81x9var.h,v 1.33 2006/11/11 13:41:06 tsutsui Exp $	*/

/*
 * Copyright (c) 1997, 1998
 *	Bill Paul <wpaul@ctr.columbia.edu>.  All rights reserved.
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
 *	This product includes software developed by Bill Paul.
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY Bill Paul AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL Bill Paul OR THE VOICES IN HIS HEAD
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 *
 *	FreeBSD Id: if_rlreg.h,v 1.9 1999/06/20 18:56:09 wpaul Exp
 */

#include "rnd.h"

#if NRND > 0
#include <sys/rnd.h>
#endif

#define RTK_ETHER_ALIGN	2
#define RTK_RXSTAT_LEN	4

#ifdef __NO_STRICT_ALIGNMENT
/*
 * XXX According to PR kern/33763, some 8168 and variants can't DMA
 * XXX RX packet data into unaligned buffer. This means such chips will
 * XXX never work on !__NO_STRICT_ALIGNMENT hosts without copying buffer.
 */
#define RE_ETHER_ALIGN	0
#else
#define RE_ETHER_ALIGN	2
#endif

struct rtk_type {
	uint16_t		rtk_vid;
	uint16_t		rtk_did;
	int			rtk_basetype;
	const char		*rtk_name;
};

struct rtk_hwrev {
	uint32_t		rtk_rev;
	int			rtk_type;
	const char		*rtk_desc;
};

#define RTK_8129		1
#define RTK_8139		2
#define RTK_8139CPLUS		3
#define RTK_8169		4

#define RTK_ISCPLUS(x)	((x)->rtk_type == RTK_8139CPLUS || \
			 (x)->rtk_type == RTK_8169)


struct rtk_mii_frame {
	uint8_t			mii_stdelim;
	uint8_t			mii_opcode;
	uint8_t			mii_phyaddr;
	uint8_t			mii_regaddr;
	uint8_t			mii_turnaround;
	uint16_t		mii_data;
};

/*
 * MII constants
 */
#define RTK_MII_STARTDELIM	0x01
#define RTK_MII_READOP		0x02
#define RTK_MII_WRITEOP		0x01
#define RTK_MII_TURNAROUND	0x02


/*
 * The RealTek doesn't use a fragment-based descriptor mechanism.
 * Instead, there are only four register sets, each or which represents
 * one 'descriptor.' Basically, each TX descriptor is just a contiguous
 * packet buffer (32-bit aligned!) and we place the buffer addresses in
 * the registers so the chip knows where they are. 
 * 
 * We can sort of kludge together the same kind of buffer management
 * used in previous drivers, but we have to do buffer copies almost all
 * the time, so it doesn't really buy us much.
 * 
 * For reception, there's just one large buffer where the chip stores
 * all received packets.
 */

#ifdef dreamcast
/*
 * XXX dreamcast has only 32KB DMA'able memory on its PCI bridge.
 * XXX Maybe this should be handled by prop_dictionary, or
 * XXX some other new API which returns available DMA resources.
 */
#define RTK_RX_BUF_SZ		RTK_RXBUF_16
#else
#define RTK_RX_BUF_SZ		RTK_RXBUF_64
#endif
#define RTK_RXBUFLEN		RTK_RXBUF_LEN(RTK_RX_BUF_SZ)
#define RTK_TX_LIST_CNT		4

/*
 * The 8139C+ and 8169 gigE chips support descriptor-based TX
 * and RX. In fact, they even support TCP large send. Descriptors
 * must be allocated in contiguous blocks that are aligned on a
 * 256-byte boundary. The RX rings can hold a maximum of 64 descriptors.
 * The TX rings can hold upto 64 descriptors on 8139C+, and
 * 1024 descriptors on 8169 gigE chips.
 */
#define RE_RING_ALIGN		256

#define RE_RX_DESC_CNT		64
#define RE_TX_DESC_CNT_8139	64
#define RE_TX_DESC_CNT_8169	1024
#define RE_TX_QLEN		64

struct re_rxsoft {
	struct mbuf		*rxs_mbuf;
	bus_dmamap_t		rxs_dmamap;
};

struct re_txq {
	struct mbuf *txq_mbuf;
	bus_dmamap_t txq_dmamap;
	int txq_descidx;
};

struct re_list_data {
	struct re_txq		re_txq[RE_TX_QLEN];
	int			re_txq_considx;
	int			re_txq_prodidx;

	bus_dmamap_t		re_tx_list_map;
	struct re_desc		*re_tx_list;
	int			re_tx_free;	/* # of free descriptors */
	int			re_tx_nextfree; /* next descriptor to use */
	int			re_tx_desc_cnt; /* # of descriptors */
	bus_dma_segment_t 	re_tx_listseg;
	int			re_tx_listnseg;

	struct re_rxsoft	re_rxsoft[RE_RX_DESC_CNT];
	bus_dmamap_t		re_rx_list_map;
	struct re_desc		*re_rx_list;
	int			re_rx_prodidx;
	bus_dma_segment_t 	re_rx_listseg;
	int			re_rx_listnseg;
};

struct rtk_tx_desc {
	SIMPLEQ_ENTRY(rtk_tx_desc) txd_q;
	struct mbuf		*txd_mbuf;
	bus_dmamap_t		txd_dmamap;
	bus_addr_t		txd_txaddr;
	bus_addr_t		txd_txstat;
};

struct rtk_softc {
	struct device sc_dev;		/* generic device structures */
	struct ethercom		ethercom;	/* interface info */
	struct mii_data		mii;
	struct callout		rtk_tick_ch;	/* tick callout */
	bus_space_handle_t	rtk_bhandle;	/* bus space handle */
	bus_space_tag_t		rtk_btag;	/* bus space tag */
	int			rtk_type;
	bus_dma_tag_t 		sc_dmat;

	bus_dma_segment_t 	sc_dmaseg;	/* for rtk(4) */
	int			sc_dmanseg;	/* for rtk(4) */

	bus_dmamap_t 		recv_dmamap;	/* for rtk(4) */
	caddr_t			rtk_rx_buf;

	struct rtk_tx_desc	rtk_tx_descs[RTK_TX_LIST_CNT];
	SIMPLEQ_HEAD(, rtk_tx_desc) rtk_tx_free;
	SIMPLEQ_HEAD(, rtk_tx_desc) rtk_tx_dirty;

	struct re_list_data	re_ldata;
	struct mbuf		*re_head;
	struct mbuf		*re_tail;
	uint32_t		re_rxlenmask;
	int			re_testmode;

	int			sc_flags;	/* misc flags */
	int			sc_txthresh;	/* Early tx threshold */
	int			sc_rev;		/* revision within rtk_type */

	void	*sc_sdhook;			/* shutdown hook */
	void	*sc_powerhook;			/* power management hook */

	/* Power management hooks. */
	int	(*sc_enable)	(struct rtk_softc *);
	void	(*sc_disable)	(struct rtk_softc *);
	void	(*sc_power)	(struct rtk_softc *, int);
#if NRND > 0
	rndsource_element_t     rnd_source;
#endif
};

#define RE_TX_DESC_CNT(sc)	((sc)->re_ldata.re_tx_desc_cnt)
#define RE_TX_LIST_SZ(sc)	(RE_TX_DESC_CNT(sc) * sizeof(struct re_desc))
#define RE_NEXT_TX_DESC(sc, x)	(((x) + 1) % RE_TX_DESC_CNT(sc))

#define RE_RX_LIST_SZ		(RE_RX_DESC_CNT * sizeof(struct re_desc))
#define RE_NEXT_RX_DESC(sc, x)	(((x) + 1) % RE_RX_DESC_CNT)

#define RE_NEXT_TXQ(sc, x)	(((x) + 1) % RE_TX_QLEN)

#define RE_TXDESCSYNC(sc, idx, ops)					\
	bus_dmamap_sync((sc)->sc_dmat,					\
	    (sc)->re_ldata.re_tx_list_map,				\
	    sizeof(struct re_desc) * (idx),				\
	    sizeof(struct re_desc),					\
	    (ops))
#define RE_RXDESCSYNC(sc, idx, ops)					\
	bus_dmamap_sync((sc)->sc_dmat,					\
	    (sc)->re_ldata.re_rx_list_map,				\
	    sizeof(struct re_desc) * (idx),				\
	    sizeof(struct re_desc),					\
	    (ops))

#define RTK_ATTACHED 0x00000001 /* attach has succeeded */
#define RTK_ENABLED  0x00000002 /* chip is enabled	*/

#define RTK_IS_ENABLED(sc)	((sc)->sc_flags & RTK_ENABLED)

#define RTK_TXTH_MAX	RTK_TXTH_1536

/*
 * register space access macros
 */
#define CSR_WRITE_4(sc, reg, val)	\
	bus_space_write_4(sc->rtk_btag, sc->rtk_bhandle, reg, val)
#define CSR_WRITE_2(sc, reg, val)	\
	bus_space_write_2(sc->rtk_btag, sc->rtk_bhandle, reg, val)
#define CSR_WRITE_1(sc, reg, val)	\
	bus_space_write_1(sc->rtk_btag, sc->rtk_bhandle, reg, val)

#define CSR_READ_4(sc, reg)		\
	bus_space_read_4(sc->rtk_btag, sc->rtk_bhandle, reg)
#define CSR_READ_2(sc, reg)		\
	bus_space_read_2(sc->rtk_btag, sc->rtk_bhandle, reg)
#define CSR_READ_1(sc, reg)		\
	bus_space_read_1(sc->rtk_btag, sc->rtk_bhandle, reg)

#define RTK_TIMEOUT		1000

/*
 * PCI low memory base and low I/O base registers
 */

#define RTK_PCI_LOIO		0x10
#define RTK_PCI_LOMEM		0x14

#ifdef _KERNEL
uint16_t rtk_read_eeprom(struct rtk_softc *, int, int);
void	rtk_setmulti(struct rtk_softc *);
void	rtk_attach(struct rtk_softc *);
int	rtk_detach(struct rtk_softc *);
int	rtk_activate(struct device *, enum devact);
int	rtk_intr(void *);
#endif /* _KERNEL */
