/*	$NetBSD: cmureg.h,v 1.3 2001/05/17 05:04:30 sato Exp $	*/

/*-
 * Copyright (c) 1999 SATO Kazumi. All rights reserved.
 * Copyright (c) 1999 PocketBSD Project. All rights reserved.
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
 *	This product includes software developed by the PocketBSD project
 *	and its contributors.
 * 4. Neither the name of the project nor the names of its contributors
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
 */

/*
 *	CMU (CLock MASK UNIT) Registers.
 *		start 0x0B000060 (Vr4102-4111)
 *		start 0x0F000060 (Vr4122)
 */

#define	CMUCLKMASK		0x000	/* CMU Clock Mask Register */
#define		VR4122_CMUMSKPCIU	((1<<13)|(1<<7))	/* 1 supply PCICLK */
#define		VR4122_CMUMSKSCSI	(1<<12)		/* 1 supply CSI 18.432MHz clock */
#define		VR4122_CMUMSKDSIU	(1<<11)		/* 1 supply DSIU 18.432MHz clock */
#define		CMUMSKFFIR		(1<<10)		/* 1 supply 48MHz to FIR */
#define		CMUMSKSHSP		(1<<9)		/* 1 supply 18.432MHz to HSP */ /* 4102-4121 */
#define		CMUMSKSSIU		(1<<8)		/* 1 supply 18.432MHz to SIU */
#define		CMUMSKDSIU		(1<<5)		/* 1 supply Tclock to DSIU */
#define		CMUMSKFIR		(1<<4)		/* 1 supply Tclock to FIR */
#define		CMUMSKKIU		(1<<3)		/* 1 supply Tclock to KIU */ /* 4102-4121 */
#define		CMUMSKAIU		(1<<2)		/* 1 supply Tclock to AIU */ /* 4102-4121 */
#define		CMUMSKSIU		(1<<1)		/* 1 supply Tclock to SIU */
#define		CMUMSKPIU		(1)		/* 1 supply Tclock to PIU */ /* 4102-4121 */

/* END cmureg.h */
