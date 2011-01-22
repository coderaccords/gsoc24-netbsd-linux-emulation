/*	$NetBSD: boot.h,v 1.6 2011/01/22 19:19:24 joerg Exp $	*/

/*-
 * Copyright (c) 2005 The NetBSD Foundation, Inc.
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

#ifndef _BOOT_H_
#define _BOOT_H_

#include <machine/bootinfo.h>

#if defined(_DEBUG)
#define DPRINTF(x)	printf x;
#else
#define DPRINTF(x)
#endif

/*
 * vers.c (generated by newvers.sh)
 */
extern const char bootprog_rev[];
extern const char bootprog_name[];

/* bootinfo.c */
extern u_long	bi_init(u_long);
extern void	bi_add(void *, int, size_t);

/* sparc64.c */
extern ssize_t	sparc64_read(int, void *, size_t);
extern void*	sparc64_memcpy(void *, const void *, size_t);
extern void*	sparc64_memset(void *, int, size_t);
extern void	sparc64_bi_add(void);
extern void	sparc64_finalize_tlb(u_long);

/* srt0.s */
extern u_int	get_cpuid(void);

/* loadfile_machdep.c */
#define LOADFILE_NOP_ALLOCATOR		0x0
#define LOADFILE_OFW_ALLOCATOR		0x1
#define LOADFILE_MMU_ALLOCATOR		0x2
extern void	loadfile_set_allocator(int);

/* ofdev.c */
char *filename(char*, char*);

#endif /* _BOOT_H_ */
