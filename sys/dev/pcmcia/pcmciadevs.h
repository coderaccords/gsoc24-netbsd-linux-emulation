/*	$NetBSD: pcmciadevs.h,v 1.53 1999/10/27 19:10:58 is Exp $	*/

/*
 * THIS FILE AUTOMATICALLY GENERATED.  DO NOT EDIT.
 *
 * generated from:
 *	NetBSD: pcmciadevs,v 1.50 1999/10/27 19:10:02 is Exp 
 */

/*-
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Christos Zoulas.
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
 * List of known PCMCIA vendors
 */

#define	PCMCIA_VENDOR_FUJITSU	0x0004	/* Fujitsu Corporation */
#define	PCMCIA_VENDOR_SANDISK	0x0045	/* Sandisk Corporation */
#define	PCMCIA_VENDOR_NEWMEDIA	0x0057	/* New Media Corporation */
#define	PCMCIA_VENDOR_IBM	0x00a4	/* IBM Corporation */
#define	PCMCIA_VENDOR_MOTOROLA	0x0109	/* Motorola Corporation */
#define	PCMCIA_VENDOR_3COM	0x0101	/* 3Com Corporation */
#define	PCMCIA_VENDOR_MEGAHERTZ	0x0102	/* Megahertz Corporation */
#define	PCMCIA_VENDOR_SOCKET	0x0104	/* Socket Communications */
#define	PCMCIA_VENDOR_TDK	0x0105	/* TDK Corporation */
#define	PCMCIA_VENDOR_SMC	0x0108	/* Standard Microsystems Corporation */
#define	PCMCIA_VENDOR_USROBOTICS	0x0115	/* US Robotics Corporation */
#define	PCMCIA_VENDOR_MEGAHERTZ2	0x0128	/* Megahertz Corporation */
#define	PCMCIA_VENDOR_ADAPTEC	0x012f	/* Adaptec Corporation */
#define	PCMCIA_VENDOR_LINKSYS	0x0149	/* Linksys Corporation */
#define	PCMCIA_VENDOR_SIMPLETECH	0x014d	/* Simple Technology */
#define	PCMCIA_VENDOR_LUCENT	0x0156	/* Lucent Technologies */
#define	PCMCIA_VENDOR_DAYNA	0x0194	/* Dayna Corporation */
#define	PCMCIA_VENDOR_IODATA	0x01bf	/* I-O DATA */
#define	PCMCIA_VENDOR_LASAT	0x3401	/* Lasat Communications A/S */
#define	PCMCIA_VENDOR_COMPEX	0x8a01	/* Compex Corporation */
#define	PCMCIA_VENDOR_CONTEC	0xc001	/* Contec */
#define	PCMCIA_VENDOR_COREGA	0xc00f	/* Corega K.K. */
#define	PCMCIA_VENDOR_ALLIEDTELESIS	0xc00f	/* Allied Telesis K.K. */
#define	PCMCIA_VENDOR_HAGIWARASYSCOM	0xc012	/* Hagiwara SYS-COM */

/*
 * List of known products.  Grouped by vendor.
 */
/* Adaptec Products */
#define	PCMCIA_CIS_ADAPTEC_APA1460	{ NULL, NULL, NULL, NULL }
#define	PCMCIA_PRODUCT_ADAPTEC_APA1460	0x0001
#define	PCMCIA_STR_ADAPTEC_APA1460	"Adaptec APA-1460 SlimSCSI"
#define	PCMCIA_CIS_ADAPTEC_APA1460A	{ NULL, NULL, NULL, NULL }
#define	PCMCIA_PRODUCT_ADAPTEC_APA1460A	0x0002
#define	PCMCIA_STR_ADAPTEC_APA1460A	"Adaptec APA-1460A SlimSCSI"

/* 3COM Products */
#define	PCMCIA_CIS_3COM_3C562	{ NULL, NULL, NULL, NULL }
#define	PCMCIA_PRODUCT_3COM_3C562	0x0562
#define	PCMCIA_STR_3COM_3C562	"3Com 3c562 33.6 Modem/10Mbps Ethernet"
#define	PCMCIA_CIS_3COM_3C589	{ NULL, NULL, NULL, NULL }
#define	PCMCIA_PRODUCT_3COM_3C589	0x0589
#define	PCMCIA_STR_3COM_3C589	"3Com 3c589 10Mbps Ethernet"
#define	PCMCIA_CIS_3COM_3C574	{ NULL, NULL, NULL, NULL }
#define	PCMCIA_PRODUCT_3COM_3C574	0x0574
#define	PCMCIA_STR_3COM_3C574	"3Com 3c574-TX 10/100Mbps Ethernet"
#define	PCMCIA_CIS_3COM_3CXEM556	{ NULL, NULL, NULL, NULL }
#define	PCMCIA_PRODUCT_3COM_3CXEM556	0x0035
#define	PCMCIA_STR_3COM_3CXEM556	"3Com/Megahertz 3CXEM556 Ethernet/Modem"
#define	PCMCIA_CIS_3COM_3CXEM556INT	{ NULL, NULL, NULL, NULL }
#define	PCMCIA_PRODUCT_3COM_3CXEM556INT	0x003d
#define	PCMCIA_STR_3COM_3CXEM556INT	"3Com/Megahertz 3CXEM556-INT Ethernet/Modem"
#define	PCMCIA_CIS_3COM_3CCFEM556BI	{ NULL, NULL, NULL, NULL }
#define	PCMCIA_PRODUCT_3COM_3CCFEM556BI	0x0556
#define	PCMCIA_STR_3COM_3CCFEM556BI	"3Com/Megahertz 3CCFEM556BI Ethernet/Modem"

/* Compex Products */
#define	PCMCIA_CIS_COMPEX_LINKPORT_ENET_B	{ NULL, NULL, NULL, NULL }
#define	PCMCIA_PRODUCT_COMPEX_LINKPORT_ENET_B	0x0100
#define	PCMCIA_STR_COMPEX_LINKPORT_ENET_B	"Compex Linkport ENET-B Ethernet"

/* Lasat Products */
#define	PCMCIA_CIS_LASAT_CREDIT_288	{ NULL, NULL, NULL, NULL }
#define	PCMCIA_PRODUCT_LASAT_CREDIT_288	0x2811
#define	PCMCIA_STR_LASAT_CREDIT_288	"Lasat Credit 288 Modem"

/* Dayna Products */
#define	PCMCIA_CIS_DAYNA_COMMUNICARD_E_1	{ NULL, NULL, NULL, NULL }
#define	PCMCIA_PRODUCT_DAYNA_COMMUNICARD_E_1	0x002d
#define	PCMCIA_STR_DAYNA_COMMUNICARD_E_1	"Dayna CommuniCard E"
#define	PCMCIA_CIS_DAYNA_COMMUNICARD_E_2	{ NULL, NULL, NULL, NULL }
#define	PCMCIA_PRODUCT_DAYNA_COMMUNICARD_E_2	0x002f
#define	PCMCIA_STR_DAYNA_COMMUNICARD_E_2	"Dayna CommuniCard E"

/* DIGITAL Products */
#define	PCMCIA_CIS_DIGITAL_MOBILE_MEDIA_CDROM	{ NULL, NULL, NULL, NULL }
#define	PCMCIA_PRODUCT_DIGITAL_MOBILE_MEDIA_CDROM	0x0d00
#define	PCMCIA_STR_DIGITAL_MOBILE_MEDIA_CDROM	"Digital Mobile Media CD-ROM"

/* Fujutsu Products */
#define	PCMCIA_CIS_FUJITSU_SCSI600	{ NULL, NULL, NULL, NULL }
#define	PCMCIA_PRODUCT_FUJITSU_SCSI600	0x0401
#define	PCMCIA_STR_FUJITSU_SCSI600	"Fujitsu SCSI 600 Interface"

/* Motorola Products */
#define	PCMCIA_CIS_MOTOROLA_POWER144	{ NULL, NULL, NULL, NULL }
#define	PCMCIA_PRODUCT_MOTOROLA_POWER144	0x0105
#define	PCMCIA_STR_MOTOROLA_POWER144	"Motorola Power 14.4 Modem"
#define	PCMCIA_CIS_MOTOROLA_PM100C	{ NULL, NULL, NULL, NULL }
#define	PCMCIA_PRODUCT_MOTOROLA_PM100C	0x0302
#define	PCMCIA_STR_MOTOROLA_PM100C	"Motorola Personal Messenger 100C CDPD Modem"

/* Fujitsu Products */
#define	PCMCIA_CIS_FUJITSU_LA501	{ NULL, NULL, NULL, NULL }
#define	PCMCIA_PRODUCT_FUJITSU_LA501	0x2000
#define	PCMCIA_STR_FUJITSU_LA501	"Fujitsu Towa LA501 Ethernet"
#define	PCMCIA_CIS_FUJITSU_LA10S	{ NULL, NULL, NULL, NULL }
#define	PCMCIA_PRODUCT_FUJITSU_LA10S	0x1003
#define	PCMCIA_STR_FUJITSU_LA10S	"Fujitsu Compact Flash Ethernet"

/* IBM Products */
#define	PCMCIA_CIS_IBM_INFOMOVER	{ NULL, NULL, NULL, NULL }
#define	PCMCIA_PRODUCT_IBM_INFOMOVER	0x0002
#define	PCMCIA_STR_IBM_INFOMOVER	"National Semiconductor InfoMover"
#define	PCMCIA_CIS_IBM_HOME_AND_AWAY	{ NULL, NULL, NULL, NULL }
#define	PCMCIA_PRODUCT_IBM_HOME_AND_AWAY	0x002e
#define	PCMCIA_STR_IBM_HOME_AND_AWAY	"IBM Home and Away Modem"
#define	PCMCIA_CIS_IBM_WIRELESS_LAN_ENTRY	{ NULL, NULL, NULL, NULL }
#define	PCMCIA_PRODUCT_IBM_WIRELESS_LAN_ENTRY	0x0032
#define	PCMCIA_STR_IBM_WIRELESS_LAN_ENTRY	"Wireless LAN Entry"
#define	PCMCIA_CIS_IBM_PORTABLE_CDROM_DRIVE	{ NULL, NULL, NULL, NULL }
#define	PCMCIA_PRODUCT_IBM_PORTABLE_CDROM_DRIVE	0x002d
#define	PCMCIA_STR_IBM_PORTABLE_CDROM_DRIVE	"PCMCIA Portable CD-ROM Drive"
#define	PCMCIA_CIS_IBM_ETHERJET_PCCARD	{ NULL, NULL, NULL, NULL }
#define	PCMCIA_PRODUCT_IBM_ETHERJET_PCCARD	0x003f
#define	PCMCIA_STR_IBM_ETHERJET_PCCARD	"IBM EtherJet Ethernet PC Card"

/* I-O DATA */
#define	PCMCIA_CIS_IODATA_PCLAT	{ NULL, NULL, NULL, NULL }
#define	PCMCIA_PRODUCT_IODATA_PCLAT	0x2216
#define	PCMCIA_STR_IODATA_PCLAT	"I-O DATA PCLA/T"

/* Linksys corporation */
#define	PCMCIA_CIS_LINKSYS_ECARD_1	{ NULL, NULL, NULL, NULL }
#define	PCMCIA_PRODUCT_LINKSYS_ECARD_1	0x0265
#define	PCMCIA_STR_LINKSYS_ECARD_1	"Linksys EthernetCard or D-Link DE-650"
#define	PCMCIA_CIS_LINKSYS_COMBO_ECARD	{ NULL, NULL, NULL, NULL }
#define	PCMCIA_PRODUCT_LINKSYS_COMBO_ECARD	0xc1ab
#define	PCMCIA_STR_LINKSYS_COMBO_ECARD	"Linksys Combo EthernetCard"
#define	PCMCIA_CIS_LINKSYS_TRUST_COMBO_ECARD	{ NULL, NULL, NULL, NULL }
#define	PCMCIA_PRODUCT_LINKSYS_TRUST_COMBO_ECARD	0x021b
#define	PCMCIA_STR_LINKSYS_TRUST_COMBO_ECARD	"Trust (Linksys) Combo EthernetCard"

/* Megahertz Products */
#define	PCMCIA_CIS_MEGAHERTZ_XJ4288	{ NULL, NULL, NULL, NULL }
#define	PCMCIA_PRODUCT_MEGAHERTZ_XJ4288	0x0023
#define	PCMCIA_STR_MEGAHERTZ_XJ4288	"Megahertz XJ4288 Modem"
#define	PCMCIA_CIS_MEGAHERTZ_XJ5560	{ NULL, NULL, NULL, NULL }
#define	PCMCIA_PRODUCT_MEGAHERTZ_XJ5560	0x0034
#define	PCMCIA_STR_MEGAHERTZ_XJ5560	"Megahertz X-JACK 56kbps Modem"
#define	PCMCIA_CIS_MEGAHERTZ2_XJACK	{ NULL, NULL, NULL, NULL }
#define	PCMCIA_PRODUCT_MEGAHERTZ2_XJACK	0x0103
#define	PCMCIA_STR_MEGAHERTZ2_XJACK	"Megahertz X-JACK Ethernet"
#define	PCMCIA_CIS_MEGAHERTZ_XJEM3336	{ NULL, NULL, NULL, NULL }
#define	PCMCIA_PRODUCT_MEGAHERTZ_XJEM3336	0x0006
#define	PCMCIA_STR_MEGAHERTZ_XJEM3336	"Megahertz X-JACK Ethernet Modem"

/* US Robotics Products */
#define	PCMCIA_CIS_USROBOTICS_WORLDPORT144	{ NULL, NULL, NULL, NULL }
#define	PCMCIA_PRODUCT_USROBOTICS_WORLDPORT144	0x3330
#define	PCMCIA_STR_USROBOTICS_WORLDPORT144	"US Robotics WorldPort 14.4 Modem"

/* Sandisk Products */
#define	PCMCIA_CIS_SANDISK_SDCFB	{ NULL, NULL, NULL, NULL }
#define	PCMCIA_PRODUCT_SANDISK_SDCFB	0x0401
#define	PCMCIA_STR_SANDISK_SDCFB	"Sandisk CompactFlash Card"

/* Simple Technology Products */
#define	PCMCIA_CIS_SIMPLETECH_COMMUNICATOR288	{ NULL, NULL, NULL, NULL }
#define	PCMCIA_PRODUCT_SIMPLETECH_COMMUNICATOR288	0x0100
#define	PCMCIA_STR_SIMPLETECH_COMMUNICATOR288	"Simple Technology 28.8 Communicator"
/* Simpletech ID also used by Symbol */
#define	PCMCIA_CIS_SIMPLETECH_SPECTRUM24	{ NULL, NULL, NULL, NULL }
#define	PCMCIA_PRODUCT_SIMPLETECH_SPECTRUM24	0x801
#define	PCMCIA_STR_SIMPLETECH_SPECTRUM24	"Symbol Spectrum24 WLAN Adapter"


/* Socket Communications Products */
#define	PCMCIA_CIS_SOCKET_PAGECARD	{ NULL, NULL, NULL, NULL }
#define	PCMCIA_PRODUCT_SOCKET_PAGECARD	0x0003
#define	PCMCIA_STR_SOCKET_PAGECARD	"Socket Communications PageCard"
#define	PCMCIA_CIS_SOCKET_DUAL_RS232	{ NULL, NULL, NULL, NULL }
#define	PCMCIA_PRODUCT_SOCKET_DUAL_RS232	0x0006
#define	PCMCIA_STR_SOCKET_DUAL_RS232	"Socket Communications Dual RS232"

/* TDK Products */
#define	PCMCIA_CIS_TDK_LAK_CD021BX	{ NULL, NULL, NULL, NULL }
#define	PCMCIA_PRODUCT_TDK_LAK_CD021BX	0x0200
#define	PCMCIA_STR_TDK_LAK_CD021BX	"TDK LAK-CD021BX Ethernet"
#define	PCMCIA_CIS_TDK_DFL9610	{ NULL, NULL, NULL, NULL }
#define	PCMCIA_PRODUCT_TDK_DFL9610	0x0d0a
#define	PCMCIA_STR_TDK_DFL9610	"TDK DFL9610 Ethernet & Digital Cellular"

/* TDK Vendor ID also used by Xircom! */
#define	PCMCIA_CIS_TDK_XIR_CE_10	{ NULL, NULL, NULL, NULL }
#define	PCMCIA_PRODUCT_TDK_XIR_CE_10	0x0108
#define	PCMCIA_STR_TDK_XIR_CE_10	"Xircom CreditCard Ethernet"
#define	PCMCIA_CIS_TDK_XIR_CEM_10	{ NULL, NULL, NULL, NULL }
#define	PCMCIA_PRODUCT_TDK_XIR_CEM_10	0x110a
#define	PCMCIA_STR_TDK_XIR_CEM_10	"Xircom CreditCard Ethernet + Modem"
#define	PCMCIA_CIS_TDK_XIR_PS_CE2_10	{ NULL, NULL, NULL, NULL }
#define	PCMCIA_PRODUCT_TDK_XIR_PS_CE2_10	0x010b
#define	PCMCIA_STR_TDK_XIR_PS_CE2_10	"Xircom CreditCard CE2 Ethernet"
#define	PCMCIA_CIS_TDK_XIR_CNW	{ NULL, NULL, NULL, NULL }
#define	PCMCIA_PRODUCT_TDK_XIR_CNW	0x0802
#define	PCMCIA_STR_TDK_XIR_CNW	"Xircom CreditCard Netwave"

/* New Media Products */
#define	PCMCIA_CIS_NEWMEDIA_BASICS	{ NULL, NULL, NULL, NULL }
#define	PCMCIA_PRODUCT_NEWMEDIA_BASICS	0x0019
#define	PCMCIA_STR_NEWMEDIA_BASICS	"New Media BASICS Ethernet"
#define	PCMCIA_CIS_NEWMEDIA_BUSTOASTER	{ NULL, NULL, NULL, NULL }
#define	PCMCIA_PRODUCT_NEWMEDIA_BUSTOASTER	0xc102
#define	PCMCIA_STR_NEWMEDIA_BUSTOASTER	"New Media BusToaster SCSI Host Adapter"

/* Standard Microsystems Corporation Products */
#define	PCMCIA_CIS_SMC_8016	{ NULL, NULL, NULL, NULL }
#define	PCMCIA_PRODUCT_SMC_8016	0x0105
#define	PCMCIA_STR_SMC_8016	"SMC 8016 EtherCard"
#define	PCMCIA_CIS_SMC_EZCARD	{ NULL, NULL, NULL, NULL }
#define	PCMCIA_PRODUCT_SMC_EZCARD	0x8022
#define	PCMCIA_STR_SMC_EZCARD	"SMC EXCard 10 PCMCIA"

/* Contec C-NET(PC) */
#define	PCMCIA_CIS_CONTEC_CNETPC	{ NULL, NULL, NULL, NULL }
#define	PCMCIA_PRODUCT_CONTEC_CNETPC	0x0000
#define	PCMCIA_STR_CONTEC_CNETPC	"Contec C-NET(PC)C"

/* Allied Telesis K.K. */
#define	PCMCIA_CIS_ALLIEDTELESIS_LA_PCM	{ NULL, NULL, NULL, NULL }
#define	PCMCIA_PRODUCT_ALLIEDTELESIS_LA_PCM	0x0002
#define	PCMCIA_STR_ALLIEDTELESIS_LA_PCM	"Allied Telesis LA-PCM"

/* Lucent WaveLAN/IEEE */
#define	PCMCIA_CIS_LUCENT_WAVELAN_IEEE	{ NULL, NULL, NULL, NULL }
#define	PCMCIA_PRODUCT_LUCENT_WAVELAN_IEEE	0x0002
#define	PCMCIA_STR_LUCENT_WAVELAN_IEEE	"WaveLAN/IEEE"

/* Cards we know only by their cis */
#define	PCMCIA_VENDOR_PREMAX	-1	/* Premax */
#define	PCMCIA_VENDOR_PLANET	-1	/* Planet */
#define	PCMCIA_VENDOR_PLANEX	-1	/* Planex Communications Inc */
#define	PCMCIA_VENDOR_DLINK	-1	/* D-Link */
#define	PCMCIA_VENDOR_RPTI	-1	/* RPTI */
#define	PCMCIA_VENDOR_ACCTON	-1	/* ACCTON */
#define	PCMCIA_VENDOR_YEDATA	-1	/* Y-E DATA */
#define	PCMCIA_VENDOR_DIGITAL	-1	/* Digital Equipment Corporation */
#define	PCMCIA_VENDOR_TEAC	-1	/* TEAC */
#define	PCMCIA_VENDOR_SVEC	-1	/* SVEC/Hawking Technology */
#define	PCMCIA_VENDOR_AMBICOM	-1	/* AmbiCom Inc */

#define	PCMCIA_CIS_MEGAHERTZ_XJ2288	{ "MEGAHERTZ", "MODEM XJ2288", NULL, NULL }
#define	PCMCIA_PRODUCT_MEGAHERTZ_XJ2288	-1
#define	PCMCIA_STR_MEGAHERTZ_XJ2288	"Megahertz XJ2288 Modem"
#define	PCMCIA_CIS_PREMAX_PE200	{ "PMX   ", "PE-200", NULL, NULL }
#define	PCMCIA_PRODUCT_PREMAX_PE200	-1
#define	PCMCIA_STR_PREMAX_PE200	"PreMax PE-200"
#define	PCMCIA_CIS_PLANET_SMARTCOM2000	{ "PCMCIA", "UE2212", NULL, NULL }
#define	PCMCIA_PRODUCT_PLANET_SMARTCOM2000	-1
#define	PCMCIA_STR_PLANET_SMARTCOM2000	"Planet SmartCOM 2000"
/*
 * vendor ID of FNW-3600-T is LINKSYS(0x0149) and product ID is 0xc1ab, but
 * it conflicts with LINKSYS Combo EhternetCard.
 */
#define	PCMCIA_CIS_PLANEX_FNW3600T	{ "Fast Ethernet", "Adapter", "1.0", NULL }
#define	PCMCIA_PRODUCT_PLANEX_FNW3600T	-1
#define	PCMCIA_STR_PLANEX_FNW3600T	"Planex FNW-3600-T"
#define	PCMCIA_CIS_DLINK_DE650	{ "D-Link", "DE-650", NULL, NULL }
#define	PCMCIA_PRODUCT_DLINK_DE650	-1
#define	PCMCIA_STR_DLINK_DE650	"D-Link DE-650"
#define	PCMCIA_CIS_DLINK_DE660	{ "D-Link", "DE-660", NULL, NULL }
#define	PCMCIA_PRODUCT_DLINK_DE660	-1
#define	PCMCIA_STR_DLINK_DE660	"D-Link DE-660"
#define	PCMCIA_CIS_RPTI_EP401	{ "RPTI", "EP401 Ethernet NE2000 Compatible", NULL, NULL }
#define	PCMCIA_PRODUCT_RPTI_EP401	-1
#define	PCMCIA_STR_RPTI_EP401	"RPTI EP401"
#define	PCMCIA_CIS_ACCTON_EN2212	{ "ACCTON", "EN2212", NULL, NULL }
#define	PCMCIA_PRODUCT_ACCTON_EN2212	-1
#define	PCMCIA_STR_ACCTON_EN2212	"Accton EN2212"
#define	PCMCIA_CIS_YEDATA_EXTERNAL_FDD	{ "Y-E DATA", "External FDD", NULL, NULL }
#define	PCMCIA_PRODUCT_YEDATA_EXTERNAL_FDD	-1
#define	PCMCIA_STR_YEDATA_EXTERNAL_FDD	"Y-E DATA External FDD"
#define	PCMCIA_CIS_DIGITAL_DEPCMXX	{ "DIGITAL", "DEPCM-XX", NULL, NULL }
#define	PCMCIA_PRODUCT_DIGITAL_DEPCMXX	-1
#define	PCMCIA_STR_DIGITAL_DEPCMXX	"DEC DEPCM-BA"
#define	PCMCIA_CIS_TEAC_IDECARDII	{ NULL, "NinjaATA-", NULL, NULL }
#define	PCMCIA_PRODUCT_TEAC_IDECARDII	-1
#define	PCMCIA_STR_TEAC_IDECARDII	"TEAC IDE Card/II"
#define	PCMCIA_CIS_LINKSYS_ECARD_2	{ "LINKSYS", "E-CARD", NULL, NULL }
#define	PCMCIA_PRODUCT_LINKSYS_ECARD_2	-1
#define	PCMCIA_STR_LINKSYS_ECARD_2	"Linksys E-Card"
#define	PCMCIA_CIS_COREGA_ETHER_PCC_T	{ "corega K.K.", "corega Ether PCC-T", NULL, NULL }
#define	PCMCIA_PRODUCT_COREGA_ETHER_PCC_T	-1
#define	PCMCIA_STR_COREGA_ETHER_PCC_T	"Corega"
#define	PCMCIA_CIS_COREGA_ETHER_II_PCC_T	{ "corega K.K.", "corega EtherII PCC-T", NULL, NULL }
#define	PCMCIA_PRODUCT_COREGA_ETHER_II_PCC_T	-1
#define	PCMCIA_STR_COREGA_ETHER_II_PCC_T	"Corega"
#define	PCMCIA_CIS_SVEC_COMBOCARD	{ "Ethernet", "Adapter", NULL, NULL }
#define	PCMCIA_PRODUCT_SVEC_COMBOCARD	-1
#define	PCMCIA_STR_SVEC_COMBOCARD	"SVEC/Hawking Tech. Combo Card"
#define	PCMCIA_CIS_SVEC_LANCARD	{ "SVEC", "FD605 PCMCIA EtherNet Card", "V1-1", NULL }
#define	PCMCIA_PRODUCT_SVEC_LANCARD	-1
#define	PCMCIA_STR_SVEC_LANCARD	"SVEC PCMCIA Lan Card"
#define	PCMCIA_CIS_AMBICOM_AMB8002T	{ "AmbiCom Inc", "AMB8002T", NULL, NULL }
#define	PCMCIA_PRODUCT_AMBICOM_AMB8002T	-1
#define	PCMCIA_STR_AMBICOM_AMB8002T	"AmbiCom AMB8002T"
