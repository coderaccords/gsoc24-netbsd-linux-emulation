/*	$NetBSD: tx39uartreg.h,v 1.2 2001/06/14 11:09:56 uch Exp $ */

/*-
 * Copyright (c) 1999 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by UCHIYAMA Yasushi.
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
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
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
 * Toshiba TX3912/3922 UART module
 */

#define	TX39_UARTACTRL1_REG	0x0b0
#define	TX39_UARTACTRL2_REG	0x0b4
#define	TX39_UARTADMACTRL1_REG	0x0b8
#define	TX39_UARTADMACTRL2_REG	0x0bc
#define	TX39_UARTADMACNT_REG	0x0c0
#define	TX39_UARTATXHOLD_REG	0x0c4
#define	TX39_UARTARXHOLD_REG	0x0c4

#define	TX39_UARTBCTRL1_REG	0x0c8
#define	TX39_UARTBCTRL2_REG	0x0cc
#define	TX39_UARTBDMACTRL1_REG	0x0d0
#define	TX39_UARTBDMACTRL2_REG	0x0d4
#define	TX39_UARTBDMACNT_REG	0x0d8
#define	TX39_UARTBTXHOLD_REG	0x0dc
#define	TX39_UARTBRXHOLD_REG	0x0dc

#define TX39_UARTA_REG_START	0x0b0
#define TX39_UARTB_REG_START	0x0c8
#define	TX39_UARTCTRL1_REG(x)						\
	(((x) ? TX39_UARTB_REG_START : TX39_UARTA_REG_START))
#define	TX39_UARTCTRL2_REG(x)						\
	(((x) ? TX39_UARTB_REG_START : TX39_UARTA_REG_START) + 4) 
#define	TX39_UARTDMACTRL1_REG(x)					\
	(((x) ? TX39_UARTB_REG_START : TX39_UARTA_REG_START) + 8)
#define	TX39_UARTDMACTRL2_REG(x)					\
	(((x) ? TX39_UARTB_REG_START : TX39_UARTA_REG_START) + 12)
#define	TX39_UARTDMACNT_REG(x)						\
	(((x) ? TX39_UARTB_REG_START : TX39_UARTA_REG_START) + 16)
#define	TX39_UARTTXHOLD_REG(x)						\
	(((x) ? TX39_UARTB_REG_START : TX39_UARTA_REG_START) + 20)
#define	TX39_UARTRXHOLD_REG(x)						\
	(((x) ? TX39_UARTB_REG_START : TX39_UARTA_REG_START) + 20)

/*
 *	UART Control 1 Register
 */
/* R */
#define	TX39_UARTCTRL1_UARTON		0x80000000
#define	TX39_UARTCTRL1_EMPTY		0x40000000
#define	TX39_UARTCTRL1_PRXHOLDFULL	0x20000000
#define	TX39_UARTCTRL1_RXHOLDFULL	0x10000000
/* R/W */
#define	TX39_UARTCTRL1_ENDMARX		0x00008000
#define	TX39_UARTCTRL1_ENDMATX		0x00004000
#define	TX39_UARTCTRL1_TESTMODE		0x00002000
#define	TX39_UARTCTRL1_ENBREAHALT	0x00001000
#define	TX39_UARTCTRL1_ENDMATEST	0x00000800
#define	TX39_UARTCTRL1_ENDMALOOP	0x00000400
#define	TX39_UARTCTRL1_PULSEOPT2	0x00000200
#define	TX39_UARTCTRL1_PULSEOPT1	0x00000100
#define	TX39_UARTCTRL1_DTINVERT		0x00000080
#define	TX39_UARTCTRL1_DISTXD		0x00000040
#define	TX39_UARTCTRL1_TWOSTOP		0x00000020
#define	TX39_UARTCTRL1_LOOPBACK		0x00000010
#define	TX39_UARTCTRL1_BIT7		0x00000008
#define	TX39_UARTCTRL1_EVENPARITY	0x00000004
#define	TX39_UARTCTRL1_ENPARITY		0x00000002
#define	TX39_UARTCTRL1_ENUART		0x00000001

/*
 *	UART Control 2 Register
 */
/* W */
/* 
 *	BaudRate = UART Clock Hz / ((BAUDRATE + 1) * 16)
 */
#define TX3922_UARTCLOCKHZ	9216000
#define TX3912_UARTCLOCKHZ	3686400

#define TX39_UARTCTRL2_BAUDRATE_SHIFT	0

#define TX3912_UARTCTRL2_BAUDRATE_MASK	0x3ff
#define TX3922_UARTCTRL2_BAUDRATE_MASK	0x7ff

#ifdef TX391X
#define TX39_UARTCLOCKHZ		TX3912_UARTCLOCKHZ
#define TX39_UARTCTRL2_BAUDRATE_MASK	TX3912_UARTCTRL2_BAUDRATE_MASK
#elif defined TX392X
#define TX39_UARTCLOCKHZ		TX3922_UARTCLOCKHZ
#define TX39_UARTCTRL2_BAUDRATE_MASK	TX3922_UARTCTRL2_BAUDRATE_MASK
#endif

#define TX39_UARTCTRL2_BAUDRATE_SET(cr, val)				\
	((cr) | (((val) << TX39_UARTCTRL2_BAUDRATE_SHIFT) &		\
	(TX39_UARTCTRL2_BAUDRATE_MASK << TX39_UARTCTRL2_BAUDRATE_SHIFT)))

/*
 *	UART DMA Control 1 Register
 */
/* W */
#define TX39_UARTDMACTRL1_DMASTARTVAL_MASK	0xfffffffc
#define TX39_UARTDMACTRL1_DMASTARTVAL_SET(cr, val)			\
	((cr) | ((val) & TX39_UARTDMACTRL1_DMASTARTVAL_MASK))

/*
 *	UART DMA Control 2 Register
 */
/* W */
#define TX39_UARTDMACTRL2_DMALENGTH_MASK	0x0000ffff
#define TX39_UARTDMACTRL2_DMALENGTH_SET(cr, val)			\
	((cr) | ((val) & TX39_UARTDMACTRL1_DMALENGTH_MASK))

/*
 *	UART DMA Count Register
 */
/* R */
#define TX39_UARTDMACNT_DMACNT_SHIFT	0
#define TX39_UARTDMACNT_DMACNT_MASK	0xffff
#define TX39_UARTDMACNT_DMACNT(cr)					\
	((cr) & TX39_UARTDMACNT_DMACNT_MASK)

/*
 *	UART Transmit Holding Register
 */
/* W */
#define	TX39_UARTTXHOLD_BREAK		0x00000100
#define TX39_UARTTXHOLD_TXDATA_SHIFT	0
#define TX39_UARTTXHOLD_TXDATA_MASK	0x000000ff
#define TX39_UARTTXHOLD_TXDATA_SET(cr, val)				\
	((cr) | ((val) & TX39_UARTTXHOLD_TXDATA_MASK))

/*
 *	UART Receiver Holding Register
 */
/* R */
#define TX39_UARTRXHOLD_RXDATA_SHIFT	0
#define TX39_UARTRXHOLD_RXDATA_MASK	0x000000ff
#define	TX39_UARTRXHOLD_RXDATA(cr)					\
	((cr) & TX39_UARTRXHOLD_RXDATA_MASK)
