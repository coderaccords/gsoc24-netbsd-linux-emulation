/*	$NetBSD: intrcnt.h,v 1.4 1996/04/13 00:25:24 cgd Exp $	*/

/*
 * Copyright (c) 1995, 1996 Carnegie-Mellon University.
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

#define	INTRNAMES_DEFINITION						\
/* 0x00 */	ASCIZ "clock";						\
		ASCIZ "ISA irq 0";					\
		ASCIZ "ISA irq 1";					\
		ASCIZ "ISA irq 2";					\
		ASCIZ "ISA irq 3";					\
		ASCIZ "ISA irq 4";					\
		ASCIZ "ISA irq 5";					\
		ASCIZ "ISA irq 6";					\
		ASCIZ "ISA irq 7";					\
		ASCIZ "ISA irq 8";					\
		ASCIZ "ISA irq 9";					\
		ASCIZ "ISA irq 10";					\
		ASCIZ "ISA irq 11";					\
		ASCIZ "ISA irq 12";					\
		ASCIZ "ISA irq 13";					\
		ASCIZ "ISA irq 14";					\
/* 0x10 */	ASCIZ "ISA irq 15";					\
		ASCIZ "KN20AA irq 0";					\
		ASCIZ "KN20AA irq 1";					\
		ASCIZ "KN20AA irq 2";					\
		ASCIZ "KN20AA irq 3";					\
		ASCIZ "KN20AA irq 4";					\
		ASCIZ "KN20AA irq 5";					\
		ASCIZ "KN20AA irq 6";					\
		ASCIZ "KN20AA irq 7";					\
		ASCIZ "KN20AA irq 8";					\
		ASCIZ "KN20AA irq 9";					\
		ASCIZ "KN20AA irq 10";					\
		ASCIZ "KN20AA irq 11";					\
		ASCIZ "KN20AA irq 12";					\
		ASCIZ "KN20AA irq 13";					\
		ASCIZ "KN20AA irq 14";					\
/* 0x20 */	ASCIZ "KN20AA irq 15";					\
		ASCIZ "KN20AA irq 16";					\
		ASCIZ "KN20AA irq 17";					\
		ASCIZ "KN20AA irq 18";					\
		ASCIZ "KN20AA irq 19";					\
		ASCIZ "KN20AA irq 20";					\
		ASCIZ "KN20AA irq 21";					\
		ASCIZ "KN20AA irq 22";					\
		ASCIZ "KN20AA irq 23";					\
		ASCIZ "KN20AA irq 24";					\
		ASCIZ "KN20AA irq 25";					\
		ASCIZ "KN20AA irq 26";					\
		ASCIZ "KN20AA irq 27";					\
		ASCIZ "KN20AA irq 28";					\
		ASCIZ "KN20AA irq 29";					\
		ASCIZ "KN20AA irq 30";					\
/* 0x30 */	ASCIZ "KN20AA irq 31";

#define INTRCNT_DEFINITION						\
/* 0x00 */	.quad 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0;	\
/* 0x10 */	.quad 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0;	\
/* 0x20 */	.quad 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0;	\
/* 0x30 */	.quad 0;

#define	INTRCNT_CLOCK		0
#define INTRCNT_ISA_IRQ		(INTRCNT_CLOCK + 1)
#define	INTRCNT_ISA_IRQ_LEN	16
#define INTRCNT_KN20AA_IRQ	(INTRCNT_ISA_IRQ + INTRCNT_ISA_IRQ_LEN)
#define INTRCNT_KN20AA_IRQ_LEN	32

#ifndef _LOCORE
extern long intrcnt[];
#endif
