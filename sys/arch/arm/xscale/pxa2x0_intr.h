/*	$NetBSD: pxa2x0_intr.h,v 1.5 2005/06/05 15:28:27 he Exp $ */

/* Derived from i80321_intr.h */

/*
 * Copyright (c) 2001, 2002 Wasabi Systems, Inc.
 * All rights reserved.
 *
 * Written by Jason R. Thorpe for Wasabi Systems, Inc.
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
 *	This product includes software developed for the NetBSD Project by
 *	Wasabi Systems, Inc.
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

#ifndef _PXA2X0_INTR_H_
#define _PXA2X0_INTR_H_

#define	ARM_IRQ_HANDLER	_C_LABEL(pxa2x0_irq_handler)

#ifndef _LOCORE

#include <arm/cpu.h>
#include <arm/armreg.h>
#include <arm/cpufunc.h>
#include <machine/atomic.h>
#include <machine/intr.h>
#include <arm/softintr.h>

#include <arm/xscale/pxa2x0reg.h>

vaddr_t pxaic_base;		/* Shared with pxa2x0_irq.S */
#define read_icu(offset) (*(volatile uint32_t *)(pxaic_base+(offset)))
#define write_icu(offset,value) \
 (*(volatile uint32_t *)(pxaic_base+(offset))=(value))

extern __volatile int current_spl_level;
extern __volatile int intr_mask;
extern __volatile int softint_pending;
extern int pxa2x0_imask[];
void pxa2x0_do_pending(void);

/*
 * Cotulla's integrated ICU doesn't have IRQ0..7, so
 * we map software interrupts to bit 0..3
 */
#define SI_TO_IRQBIT(si)  (1U<<(si))

static __inline void
pxa2x0_setipl(int new)
{
	current_spl_level = new;
	intr_mask = pxa2x0_imask[current_spl_level];
	write_icu( SAIPIC_MR, intr_mask );
}


static __inline void
pxa2x0_splx(int new)
{
	int psw;

	psw = disable_interrupts(I32_bit);
	pxa2x0_setipl(new);
	restore_interrupts(psw);

	/* If there are software interrupts to process, do it. */
	if (softint_pending & intr_mask)
		pxa2x0_do_pending();
}


static __inline int
pxa2x0_splraise(int ipl)
{
	int	old, psw;

	old = current_spl_level;
	if( ipl > current_spl_level ){
		psw = disable_interrupts(I32_bit);
		pxa2x0_setipl(ipl);
		restore_interrupts(psw);
	}

	return (old);
}

static __inline int
pxa2x0_spllower(int ipl)
{
	int old = current_spl_level;
	int psw = disable_interrupts(I32_bit);
	pxa2x0_splx(ipl);
	restore_interrupts(psw);
	return(old);
}

static __inline void
pxa2x0_setsoftintr(int si)
{
	atomic_set_bit( (u_int *)__UNVOLATILE(&softint_pending),
		SI_TO_IRQBIT(si) );

	/* Process unmasked pending soft interrupts. */
	if ( softint_pending & intr_mask )
		pxa2x0_do_pending();
}


/*
 * An useful function for interrupt handlers.
 * XXX: This shouldn't be here.
 */
static __inline int
find_first_bit( uint32_t bits )
{
	int count;

	/* since CLZ is available only on ARMv5, this isn't portable
	 * to all ARM CPUs.  This file is for PXA2[15]0 processor. 
	 */
	asm( "clz %0, %1" : "=r" (count) : "r" (bits) );
	return 31-count;
}


int	_splraise(int);
int	_spllower(int);
void	splx(int);
void	_setsoftintr(int);

#if !defined(EVBARM_SPL_NOINLINE)

#define splx(new)		pxa2x0_splx(new)
#define	_spllower(ipl)		pxa2x0_spllower(ipl)
#define	_splraise(ipl)		pxa2x0_splraise(ipl)
#define	_setsoftintr(si)	pxa2x0_setsoftintr(si)

#endif	/* !EVBARM_SPL_NOINTR */

/*
 * This function *MUST* be called very early on in a port's
 * initarm() function, before ANY spl*() functions are called.
 *
 * The parameter is the virtual address of the PXA2x0's Interrupt
 * Controller registers.
 */
void pxa2x0_intr_bootstrap(vaddr_t);

void pxa2x0_irq_handler(void *);
void *pxa2x0_intr_establish(int irqno, int level,
			    int (*func)(void *), void *cookie);
void pxa2x0_update_intr_masks(int irqno, int level);
extern __volatile int current_spl_level;

#endif /* ! _LOCORE */

#endif /* _PXA2X0_INTR_H_ */
