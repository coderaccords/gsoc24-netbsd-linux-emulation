/* $NetBSD: if_iereg.h,v 1.1 1996/01/31 23:26:01 mark Exp $ */

/*
 * Copyright (C) 1994 Wolfgang Solfrank.
 * Copyright (C) 1994 TooLs GmbH.
 * All rights reserved.
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
 *	This product includes software developed by TooLs GmbH.
 * 4. The name of TooLs GmbH may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY TOOLS GMBH ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL TOOLS GMBH BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *	$Id: if_iereg.h,v 1.1 1996/01/31 23:26:01 mark Exp $
 */

#define	IE_PAGE		0

#define	IE_CONTROL	1
#define	IE_CONT_RESET	1
#define	IE_CONT_LOOP	2
#define	IE_CONT_ATTN	4
#define	IE_CONT_CLI	8

#define	IE_MEMSIZE	65536
#define	IE_PAGESIZE	4096
#define	IE_MEMOFF	0x2000
#define	IE_COFF2POFF(off)	((off) % IE_PAGESIZE * 2)
#define	IE_COFF2PAGE(off)	(((off) % IE_MEMSIZE) / IE_PAGESIZE)

#define	IE_ISCP_ADDR	(IE_SCP_ADDR - sizeof(struct ie_int_sys_conf_ptr))
#define	IE_IBASE	(0x1000000 - IE_MEMSIZE)
#define	IE_SCB_OFF	0

/* Steal most defines from PC driver: */
#include <dev/ic/i82586reg.h>
