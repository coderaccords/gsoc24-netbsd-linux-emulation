/*	$NetBSD: stb1var.h,v 1.4 2002/11/23 09:25:55 scw Exp $	*/

/*
 * Copyright 2002 Wasabi Systems, Inc.
 * All rights reserved.
 *
 * Written by Steve C. Woodford for Wasabi Systems, Inc.
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
 *      This product includes software developed for the NetBSD Project by
 *      Wasabi Systems, Inc.
 * 4. The name of Wasabi Systems, Inc. may not be used to endorse
 *    or promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY WASABI SYSTEMS, INC. ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL WASABI SYSTEMS, INC
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _STB1VAR_H
#define _STB1VAR_H

#include <sh5/pte.h>

extern	void	_sh5_stb1_tlbinv_cookie(pteh_t, tlbcookie_t);
extern	void	_sh5_stb1_tlbinv_all(void);
extern	void	_sh5_stb1_tlbload(void);
extern	void	_sh5_stb1_cache_dpurge(vaddr_t, paddr_t, vsize_t);
extern	void	_sh5_stb1_cache_dpurge_iinv(vaddr_t, paddr_t, vsize_t);
extern	void	_sh5_stb1_cache_dinv(vaddr_t, paddr_t, vsize_t);
extern	void	_sh5_stb1_cache_dinv_iinv(vaddr_t, paddr_t, vsize_t);
extern	void	_sh5_stb1_cache_iinv(vaddr_t, paddr_t, vsize_t);
extern	void	_sh5_stb1_cache_iinv_all(void);
extern	void	_sh5_stb1_cache_purge_all(void);

#endif /* _STB1VAR_H */
