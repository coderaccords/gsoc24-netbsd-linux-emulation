/*	$NetBSD: kern_allocsys.c,v 1.26 2003/10/26 10:45:03 jdolecek Exp $	*/

/*-
 * Copyright (c) 1999 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Luke Mewburn.
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
 *	This product includes software developed by the NetBSD
 *	Foundation, Inc. and its contributors.
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

/*-
 * Copyright (c) 1982, 1987, 1990 The Regents of the University of California.
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
 * 3. Neither the name of the University nor the names of its contributors
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

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: kern_allocsys.c,v 1.26 2003/10/26 10:45:03 jdolecek Exp $");

#include "opt_bufcache.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/buf.h>
#include <uvm/uvm_extern.h>
/*
 * Declare these as initialized data so we can patch them.
 */
#ifndef	NBUF
# define NBUF 0
#endif

#ifndef	BUFPAGES
# define BUFPAGES 0
#endif

#ifdef BUFCACHE
# if (BUFCACHE < 5) || (BUFCACHE > 95)
#  error BUFCACHE is not between 5 and 95
# endif
#else
  /* Default to 10% of first 2MB and 5% of remaining. */
# define BUFCACHE 0
#endif

u_int	nbuf = NBUF;
u_int	bufpages = BUFPAGES;	/* optional hardwired count */
u_int	bufcache = BUFCACHE;	/* % of RAM to use for buffer cache */

#define	ALLOCSYS(base, name, type, num) \
	    (name) = (type *)(base); (base) = (caddr_t)ALIGN((name)+(num))

/*
 * Allocate space for system data structures.  We are given
 * a starting virtual address and we return a final virtual
 * address; along the way we set each data structure pointer.
 *
 * We call allocsys() with 0 to find out how much space we want,
 * allocate that much and fill it with zeroes, and then call
 * allocsys() again with the correct base virtual address.
 *
 */

caddr_t
allocsys(caddr_t v, caddr_t (*mdcallback)(caddr_t))
{
	/*
	 * Determine how many buffers to allocate.
	 *
	 *	- If bufcache is specified, use that % of memory
	 *	  for the buffer cache.
	 *
	 *	- Otherwise, we default to the traditional BSD
	 *	  formula of 10% of the first 2MB and 5% of
	 *	  the remaining.
	 */
	if (bufpages == 0) {
		if (bufcache != 0) {
			if (bufcache < 5 || bufcache > 95)
				panic("bufcache is out of range (%d)",
				    bufcache);
			bufpages = physmem / 100 * bufcache;
		} else {
			if (physmem < btoc(2 * 1024 * 1024))
				bufpages = physmem / 10;
			else
				bufpages = (btoc(2 * 1024 * 1024) + physmem) /
				    20;
		}
	}

#ifdef DIAGNOSTIC
	if (bufpages == 0)
		panic("bufpages = 0");
#endif

	/*
	 * Call the mdcallback now; it may need to adjust bufpages.
	 */
	if (mdcallback != NULL)
		v = mdcallback(v);

	/* 
	 * Ensure a minimum of 16 buffers.
	 */
	if (nbuf == 0) {
		nbuf = bufpages;
		if (nbuf < 16)
			nbuf = 16;
	}

#ifdef VM_MAX_KERNEL_BUF
	/*
	 * XXX stopgap measure to prevent wasting too much KVM on
	 * the sparsely filled buffer cache.
	 */
	if (nbuf > VM_MAX_KERNEL_BUF / MAXBSIZE)
		nbuf = VM_MAX_KERNEL_BUF / MAXBSIZE;
#endif

	ALLOCSYS(v, buf, struct buf, nbuf);

	return (v);
}
