/*
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This software was developed by the Computer Systems Engineering group
 * at Lawrence Berkeley Laboratory under DARPA contract BG 91-66 and
 * contributed to Berkeley.
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
 * from: Header: brk.s,v 1.3 92/06/25 12:56:05 mccanne Exp
 * $Id: brk.s,v 1.1 1993/09/05 22:18:45 deraadt Exp $
 */

#if defined(LIBC_SCCS) && !defined(lint)
	.asciz "@(#)brk.s	8.1 (Berkeley) 6/4/93"
#endif /* LIBC_SCCS and not lint */

#include "SYS.h"

	.globl	curbrk
	.globl	minbrk

	.data
minbrk:	.long	_end			! lower brk limit; also for gmon code
	.text

ENTRY(brk)
	sethi	%hi(minbrk), %o1	! %o1 = minbrk
	ld	[%o1 + %lo(minbrk)], %o1
	cmp	%o1, %o0		! if (minbrk > %o0)
	bg,a	0f
	 mov	%o1, %o0		!	%o0 = minbrk
0:
	mov	%o0, %o2		! save argument to syscall
	mov	SYS_break, %g1
	t	ST_SYSCALL
	bcc,a	1f
	 sethi	%hi(curbrk), %g1
	ERROR()
1:
	retl				! success, return 0 & record new break
	 st	%o2, [%g1 + %lo(curbrk)]
