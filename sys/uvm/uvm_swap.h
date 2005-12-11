/*	$NetBSD: uvm_swap.h,v 1.14 2005/12/11 12:25:29 christos Exp $	*/

/*
 * Copyright (c) 1997 Matthew R. Green
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
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * from: Id: uvm_swap.h,v 1.1.2.6 1997/12/15 05:39:31 mrg Exp
 */

#ifndef _UVM_UVM_SWAP_H_
#define _UVM_UVM_SWAP_H_

#define	SWSLOT_BAD	(-1)

#ifdef _KERNEL
#if defined(_KERNEL_OPT)
#include "opt_vmswap.h"
#endif

struct swapent;

#if defined(VMSWAP)
int	uvm_swap_get(struct vm_page *, int, int);
int	uvm_swap_put(int, struct vm_page **, int, int);
int	uvm_swap_alloc(int *, boolean_t);
void	uvm_swap_free(int, int);
void	uvm_swap_markbad(int, int);
boolean_t	uvm_swapisfull(void);
#else /* defined(VMSWAP) */
#define	uvm_swapisfull()	TRUE
#endif /* defined(VMSWAP) */
void	uvm_swap_stats(int, struct swapent *, int, register_t *);

#endif /* _KERNEL */

#endif /* _UVM_UVM_SWAP_H_ */
