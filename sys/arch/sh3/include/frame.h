/*	$NetBSD: frame.h,v 1.1 1999/09/13 10:31:18 itojun Exp $	*/

/*-
 * Copyright (c) 1995 Charles M. Hannum.  All rights reserved.
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * William Jolitz.
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
 *	@(#)frame.h	5.2 (Berkeley) 1/18/91
 */

#ifndef _SH3_FRAME_H_
#define _SH3_FRAME_H_

#include <sys/signal.h>

/*
 * System stack frames.
 */

/*
 * Exception/Trap Stack Frame
 */
struct trapframe {
	int	tf_trapno;
	int	dummy;
	int	tf_spc;
	int	tf_ssr;
	int	tf_macl;
	int	tf_mach;
	int	tf_pr;
	int	tf_r14;
	int	tf_r13;
	int	tf_r12;
	int	tf_r11;
	int	tf_r10;
	int	tf_r9;
	int	tf_r8;
	int	tf_r7;
	int	tf_r6;
	int	tf_r5;
	int	tf_r4;
	int	tf_r3;
	int	tf_r2;
	int	tf_r1;
	int	tf_r0;
	int	tf_r15;
};

/*
 * Interrupt stack frame
 */
struct intrframe {
	int	if_trapno;
	int	dummy;
	int	if_spc;
	int	if_ssr;
	int	if_macl;
	int	if_mach;
	int	if_pr;
	int	if_r14;
	int	if_r13;
	int	if_r12;
	int	if_r11;
	int	if_r10;
	int	if_r9;
	int	if_r8;
	int	if_r7;
	int	if_r6;
	int	if_r5;
	int	if_r4;
	int	if_r3;
	int	if_r2;
	int	if_r1;
	int	if_r0;
	int	if_r15;
	int	if_pri;
};

/*
 * Stack frame inside cpu_switch()
 */
struct switchframe {
	int	sf_ppl;
	int	sf_r14;
	int	sf_r13;
	int	sf_r12;
	int	sf_r11;
	int	sf_r10;
	int	sf_r9;
	int	sf_r8;
	int	sf_pr;
};

/*
 * Signal frame
 */
struct sigframe {
	int	sf_signum;
	int	sf_code;
	struct	sigcontext *sf_scp;
	sig_t	sf_handler;
	struct	sigcontext sf_sc;
};

#endif /* !_SH3_FRAME_H_ */
