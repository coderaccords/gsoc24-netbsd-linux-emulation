/*	$NetBSD: if_types.h,v 1.8 1998/02/03 04:20:05 ross Exp $	*/

/*
 * Copyright (c) 1989, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
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
 *	@(#)if_types.h	8.2 (Berkeley) 4/20/94
 */

/*
 * Interface types for benefit of parsing media address headers.
 * This list is derived from the SNMP list of ifTypes, currently
 * documented in RFC1573.
 */

#define	IFT_OTHER	0x1		/* none of the following */
#define	IFT_1822	0x2		/* old-style arpanet imp */
#define	IFT_HDH1822	0x3		/* HDH arpanet imp */
#define	IFT_X25DDN	0x4		/* x25 to imp */
#define	IFT_X25		0x5		/* PDN X25 interface (RFC877) */
#define	IFT_ETHER	0x6		/* Ethernet CSMACD */
#define	IFT_ISO88023	0x7		/* CMSA CD */
#define	IFT_ISO88024	0x8		/* Token Bus */
#define	IFT_ISO88025	0x9		/* Token Ring */
#define	IFT_ISO88026	0xa		/* MAN */
#define	IFT_STARLAN	0xb
#define	IFT_P10		0xc		/* Proteon 10MBit ring */
#define	IFT_P80		0xd		/* Proteon 80MBit ring */
#define	IFT_HY		0xe		/* Hyperchannel */
#define	IFT_FDDI	0xf
#define	IFT_LAPB	0x10
#define	IFT_SDLC	0x11
#define	IFT_T1		0x12
#define	IFT_CEPT	0x13		/* E1 - european T1 */
#define	IFT_ISDNBASIC	0x14
#define	IFT_ISDNPRIMARY	0x15
#define	IFT_PTPSERIAL	0x16		/* Proprietary PTP serial */
#define	IFT_PPP		0x17		/* RFC 1331 */
#define	IFT_LOOP	0x18		/* loopback */
#define	IFT_EON		0x19		/* ISO over IP */
#define	IFT_XETHER	0x1a		/* obsolete 3MB experimental ethernet */
#define	IFT_NSIP	0x1b		/* XNS over IP */
#define	IFT_SLIP	0x1c		/* IP over generic TTY */
#define	IFT_ULTRA	0x1d		/* Ultra Technologies */
#define	IFT_DS3		0x1e		/* Generic T3 */
#define	IFT_SIP		0x1f		/* SMDS */
#define	IFT_FRELAY	0x20		/* Frame Relay DTE only */
#define	IFT_RS232	0x21
#define	IFT_PARA	0x22		/* parallel-port */
#define	IFT_ARCNET	0x23
#define	IFT_ARCNETPLUS	0x24
#define	IFT_ATM		0x25		/* ATM cells */
#define	IFT_MIOX25	0x26
#define	IFT_SONET	0x27		/* SONET or SDH */
#define	IFT_X25PLE	0x28
#define	IFT_ISO88022LLC	0x29
#define	IFT_LOCALTALK	0x2a
#define	IFT_SMDSDXI	0x2b
#define	IFT_FRELAYDCE	0x2c		/* Frame Relay DCE */
#define	IFT_V35		0x2d
#define	IFT_HSSI	0x2e
#define	IFT_HIPPI	0x2f
#define	IFT_MODEM	0x30		/* Generic Modem */
#define	IFT_AAL5	0x31		/* AAL5 over ATM */
#define	IFT_SONETPATH	0x32
#define	IFT_SONETVT	0x33
#define	IFT_SMDSICIP	0x34		/* SMDS InterCarrier Interface */
#define	IFT_PROPVIRTUAL	0x35		/* Proprietary Virtual/internal */
#define	IFT_PROPMUX	0x36		/* Proprietary Multiplexing */
#define IFT_IEEE80212	       0x37	/* 100BaseVG */
#define IFT_FIBRECHANNEL       0x38	/* Fibre Channel */
#define IFT_HIPPIINTERFACE     0x39	/* HIPPI interfaces	 */
#define IFT_FRAMERELAYINTERCONNECT	\
			       0x3a	/* Obsolete, use either 0x20 or 0x2c */
#define IFT_AFLANE8023	       0x3b	/* ATM Emulated LAN for 802.3 */
#define IFT_AFLANE8025	       0x3c	/* ATM Emulated LAN for 802.5 */
#define IFT_CCTEMUL	       0x3d	/* ATM Emulated circuit		  */
#define IFT_FASTETHER	       0x3e	/* Fast Ethernet (100BaseT) */
#define IFT_ISDN	       0x3f	/* ISDN and X.25	    */
#define IFT_V11		       0x40	/* CCITT V.11/X.21		*/
#define IFT_V36		       0x41	/* CCITT V.36			*/
#define IFT_G703AT64K	       0x42	/* CCITT G703 at 64Kbps */
#define IFT_G703AT2MB	       0x43	/* Obsolete see DS1-MIB */
#define IFT_QLLC	       0x44	/* SNA QLLC			*/
#define IFT_FASTETHERFX	       0x45	/* Fast Ethernet (100BaseFX)	*/
#define IFT_CHANNEL	       0x46	/* channel			*/
#define IFT_IEEE80211	       0x47	/* radio spread spectrum	*/
#define IFT_IBM370PARCHAN      0x48	/* IBM System 360/370 OEMI Channel */
#define IFT_ESCON	       0x49	/* IBM Enterprise Systems Connection */
#define IFT_DLSW	       0x4a	/* Data Link Switching */
#define IFT_ISDNS	       0x4b	/* ISDN S/T interface */
#define IFT_ISDNU	       0x4c	/* ISDN U interface */
#define IFT_LAPD	       0x4d	/* Link Access Protocol D */
#define IFT_IPSWITCH	       0x4e	/* IP Switching Objects */
#define IFT_RSRB	       0x4f	/* Remote Source Route Bridging */
#define IFT_ATMLOGICAL	       0x50	/* ATM Logical Port */
#define IFT_DS0		       0x51	/* Digital Signal Level 0 */
#define IFT_DS0BUNDLE	       0x52	/* group of ds0s on the same ds1 */
#define IFT_BSC		       0x53	/* Bisynchronous Protocol */
#define IFT_ASYNC	       0x54	/* Asynchronous Protocol */
#define IFT_CNR		       0x55	/* Combat Net Radio */
#define IFT_ISO88025DTR	       0x56	/* ISO 802.5r DTR */
#define IFT_EPLRS	       0x57	/* Ext Pos Loc Report Sys */
#define IFT_ARAP	       0x58	/* Appletalk Remote Access Protocol */
#define IFT_PROPCNLS	       0x59	/* Proprietary Connectionless Protocol*/
#define IFT_HOSTPAD	       0x5a	/* CCITT-ITU X.29 PAD Protocol */
#define IFT_TERMPAD	       0x5b	/* CCITT-ITU X.3 PAD Facility */
#define IFT_FRAMERELAYMPI      0x5c	/* Multiproto Interconnect over FR */
#define IFT_X213	       0x5d	/* CCITT-ITU X213 */
#define IFT_ADSL	       0x5e	/* Asymmetric Digital Subscriber Loop */
#define IFT_RADSL	       0x5f	/* Rate-Adapt. Digital Subscriber Loop*/
#define IFT_SDSL	       0x60	/* Symmetric Digital Subscriber Loop */
#define IFT_VDSL	       0x61	/* Very H-Speed Digital Subscrib. Loop*/
#define IFT_ISO88025CRFPINT    0x62	/* ISO 802.5 CRFP */
#define IFT_MYRINET	       0x63	/* Myricom Myrinet */
#define IFT_VOICEEM	       0x64	/* voice recEive and transMit */
#define IFT_VOICEFXO	       0x65	/* voice Foreign Exchange Office */
#define IFT_VOICEFXS	       0x66	/* voice Foreign Exchange Station */
#define IFT_VOICEENCAP	       0x67	/* voice encapsulation */
#define IFT_VOICEOVERIP	       0x68	/* voice over IP encapsulation */
#define IFT_ATMDXI	       0x69	/* ATM DXI */
#define IFT_ATMFUNI	       0x6a	/* ATM FUNI */
#define IFT_ATMIMA	       0x6b	/* ATM IMA		      */
#define IFT_PPPMULTILINKBUNDLE 0x6c	/* PPP Multilink Bundle */
#define IFT_IPOVERCDLC	       0x6d	/* IBM ipOverCdlc */
#define IFT_IPOVERCLAW	       0x6e	/* IBM Common Link Access to Workstn */
#define IFT_STACKTOSTACK       0x6f	/* IBM stackToStack */
#define IFT_VIRTUALIPADDRESS   0x70	/* IBM VIPA */
#define IFT_MPC		       0x71	/* IBM multi-protocol channel support */
#define IFT_IPOVERATM	       0x72	/* IBM ipOverAtm */
#define IFT_ISO88025FIBER      0x73	/* ISO 802.5j Fiber Token Ring */
#define IFT_TDLC	       0x74	/* IBM twinaxial data link control */
#define IFT_GIGABITETHERNET    0x75	/* Gigabit Ethernet */
