/*	$NetBSD: intr.h,v 1.20 2008/01/15 10:35:33 martin Exp $ */

/*-
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Paul Kranenburg.
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

#ifndef _SPARC64_INTR_H_
#define _SPARC64_INTR_H_

#include <machine/cpuset.h>

/* XXX - arbitrary numbers; no interpretation is defined yet */
#define	IPL_NONE	0		/* nothing */
#define	IPL_SOFTCLOCK	1		/* timeouts */
#define	IPL_SOFTBIO	1		/* block I/O */
#define	IPL_SOFTNET	1		/* protocol stack */
#define	IPL_SOFTSERIAL	4		/* serial */
#define	IPL_VM		PIL_VM		/* memory allocation */
#define	IPL_SCHED	PIL_SCHED	/* scheduler */
#define	IPL_HIGH	PIL_HIGH	/* everything */
#define	IPL_HALT	5		/* cpu stop-self */
#define	IPL_PAUSE	13		/* pause cpu */
#define	IPL_FDSOFT	PIL_FDSOFT	/* floppy */

void save_and_clear_fpstate(struct lwp *);

#if defined(MULTIPROCESSOR)
void	sparc64_ipi_init (void);
int	sparc64_ipi_halt_thiscpu (void *);
int	sparc64_ipi_pause_thiscpu (void *);
void	sparc64_ipi_drop_fpstate (void *);
void	sparc64_ipi_save_fpstate (void *);
void	sparc64_ipi_nop (void *);
void	mp_halt_cpus (void);
void	mp_pause_cpus (void);
void	mp_resume_cpus (void);
int	mp_cpu_is_paused (sparc64_cpuset_t);
#endif

#endif /* _SPARC64_INTR_H_ */
