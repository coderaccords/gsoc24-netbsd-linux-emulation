/* $NetBSD: irq.c,v 1.13 2001/01/23 23:58:31 bjh21 Exp $ */

/*-
 * Copyright (c) 2000, 2001 Ben Harris
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
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
/* This file is part of NetBSD/arm26 -- a port of NetBSD to ARM2/3 machines. */
/*
 * irq.c - IOC IRQ handler.
 */

#include <sys/param.h>

__RCSID("$NetBSD: irq.c,v 1.13 2001/01/23 23:58:31 bjh21 Exp $");

#include <sys/device.h>
#include <sys/kernel.h> /* for cold */
#include <sys/malloc.h>
#include <sys/queue.h>
#include <sys/systm.h>

#include <uvm/uvm_extern.h>

#include <machine/bus.h>
#include <machine/frame.h>
#include <machine/intr.h>
#include <machine/irq.h>
#include <machine/machdep.h>

#include <arch/arm26/iobus/iocreg.h>
#include <arch/arm26/iobus/iocvar.h>

#include "opt_ddb.h"
#include "ioeb.h"
#include "unixbp.h"

#ifdef DDB
#include <ddb/db_output.h>
#endif
#if NIOEB > 0
#include <arch/arm26/ioc/ioebvar.h>
#endif
#if NUNIXBP > 0
#include <arch/arm26/podulebus/unixbpvar.h>
#endif

#define NIRQ 20
extern char *irqnames[];

/*
 * Interrupt masks are held in 32-bit integers.  At present, the
 * bottom eight bits are interrupt register A on the IOC, and the next
 * eight are interrupt register B.  After that, on systems with Unix
 * backplanes, there are four bits from there.  FIQs should
 * be represented eventually.
 */

static u_int32_t irqmask[NIPL];

LIST_HEAD(irq_handler_head, irq_handler) irq_list_head =
    LIST_HEAD_INITIALIZER(irq_list_head);

struct irq_handler {
	LIST_ENTRY(irq_handler)	link;
	int	(*func) __P((void *));
	void	*arg;
	u_int32_t	mask;
	int	irqnum;
	int	ipl;
	int	enabled;
	struct	evcnt *ev;
};

volatile static int current_spl = IPL_HIGH;

void
irq_init(void)
{

	irq_genmasks();
	softintr_init();
}

/*
 * This is optimised for speed rather than generality.  It expects to
 * be called an awful lot.
 *
 * On entry, IRQs are disabled on the CPU.
 */

void
irq_handler(struct irqframe *irqf)
{
	int s, status, result, stray;
	struct irq_handler *h;

	KASSERT(the_ioc != NULL);
	/* Get the current interrupt state */
	status = ioc_irq_status_full();
#if NUNIXBP > 0
	status |= unixbp_irq_status_full() << IRQ_UNIXBP_BASE;
#endif

	/* Get interrupt-disabling back to the IOC */
	s = splhigh();
	int_on();

#if 0
	printf("*");
#endif
	uvmexp.intrs++;

	stray = 1;
	/* Find the highest-priority requested interrupt. */
	for (h = irq_list_head.lh_first;
	     h != NULL && h->ipl > s;
	     h = h->link.le_next)
		if (h->enabled && ((status & h->mask) != 0)) {
			splx(h->ipl);
#if 0
			printf("IRQ %d...", h->irqnum);
#endif
			if (h->mask & IOC_IRQ_CLEARABLE_MASK)
				ioc_irq_clear(h->mask);
#if NIOEB > 0
			else if ((h->mask & IOEB_IRQ_CLEARABLE_MASK) &&
			    the_ioeb != NULL)
				ioeb_irq_clear(h->mask);
#endif
			if (h->arg == NULL)
				result = (h->func)(irqf);
			else
				result = (h->func)(h->arg);
			if (result == IRQ_HANDLED) {
				stray = 0;
				h->ev->ev_count++;
				break; /* XXX handle others? */
			}
			if (result == IRQ_MAYBE_HANDLED)
				stray = 0;
		}

	if (stray) {
		panic("Stray IRQ, status = 0x%x, spl = %d, mask = 0x%x",
		      status, s, irqmask[s]);
	}
#if 0
	printf(" handled\n");
#endif
	dosoftints(s); /* May lower spl to s + 1, but no lower. */

	int_off();
	hardsplx(s);
}

struct irq_handler *
irq_establish(int irqnum, int ipl, int (*func)(void *), void *arg,
    struct evcnt *ev)
{
	struct irq_handler *h, *new;

#ifdef DIAGNOSTIC
	if (irqnum >= NIRQ)
		panic("irq_register: bad irq: %d", irqnum);
#endif
	MALLOC(new, struct irq_handler *, sizeof(struct irq_handler),
	       M_DEVBUF, M_WAITOK);
	bzero(new, sizeof(*new));
	new->irqnum = irqnum;
	new->mask = 1 << irqnum;
#if NUNIXBP > 0
	if (irqnum >= IRQ_UNIXBP_BASE)
		new->mask |= 1 << IRQ_PIRQ;
#endif
	new->ipl = ipl;
	new->func = func;
	new->arg = arg;
	new->enabled = 1;
	new->ev = ev;
	if (irq_list_head.lh_first == NULL ||
	    irq_list_head.lh_first->ipl <= ipl)
		/* XXX This shouldn't need to be a special case */
		LIST_INSERT_HEAD(&irq_list_head, new, link);
	else {
		for (h = irq_list_head.lh_first;
		     h->link.le_next != NULL && h->link.le_next->ipl > ipl;
		     h = h->link.le_next);
		LIST_INSERT_AFTER(h, new, link);
	}
	if (new->mask & IOC_IRQ_CLEARABLE_MASK)
		ioc_irq_clear(new->mask);
#if NIOEB > 0
	else if ((h->mask & IOEB_IRQ_CLEARABLE_MASK) && the_ioeb != NULL)
		ioeb_irq_clear(h->mask);
#endif
	irq_genmasks();
	return new;
}

char const *
irq_string(struct irq_handler *h)
{
	static char irq_string_store[10];

#if NUNIXBP > 0
	if (h->irqnum >= IRQ_UNIXBP_BASE)
		snprintf(irq_string_store, 9, "IRQ 13.%d",
		    h->irqnum - IRQ_UNIXBP_BASE);
	else
#endif
		snprintf(irq_string_store, 9, "IRQ %d", h->irqnum);
	irq_string_store[9] = '\0';
	return irq_string_store;
}

void
irq_disable(struct irq_handler *h)
{
	int s;

	s = splhigh();
	h->enabled = 0;
	irq_genmasks();
	splx(s);
}

void
irq_enable(struct irq_handler *h)
{
	int s;

	s = splhigh();
	h->enabled = 1;
	irq_genmasks();
	splx(s);
}


void irq_genmasks()
{
	struct irq_handler *h;
	int s, i;

	/* Paranoia? */
	s = splhigh();
	/* Disable anything we don't understand */
	for (i = 0; i < NIPL; i++)
		irqmask[i] = 0;
	/* Enable interrupts in levels lower than their own */
	for (h = irq_list_head.lh_first; h != NULL; h = h->link.le_next)
		if (h->enabled) {
			if ((h->mask & irqmask[IPL_NONE]) == h->mask)
				/* Shared interrupt -- use lowest priority. */
				for (i = h->ipl; i < NIPL; i++)
					irqmask[i] &= ~h->mask;
			else
				for (i = 0; i < h->ipl; i++)
					irqmask[i] |= h->mask;
		}
	splx(s);
}

int
raisespl(int s)
{

	if (s > current_spl)
		return hardsplx(s);
	else
		return current_spl;
}

void
lowerspl(int s)
{

	if (s < current_spl) {
		dosoftints(s);
		hardsplx(s);
	}
}

int
hardsplx(int s)
{
	int was;

	int_off();
	was = current_spl;
	/* Don't try this till we've found the IOC */
	if (the_ioc != NULL)
		ioc_irq_setmask(irqmask[s]);
#if NUNIXBP > 0
	unixbp_irq_setmask(irqmask[s] >> IRQ_UNIXBP_BASE);
#endif
	current_spl = s;
	return was; /* Restore interrupt state */
}

#ifdef DDB
void
irq_stat(void (*pr)(const char *, ...))
{
	struct irq_handler *h;
	int i;
	u_int32_t last;

	for (h = irq_list_head.lh_first; h != NULL; h = h->link.le_next)
		(*pr)("%12s: ipl %2d, IRQ %2d, mask 0x%05x, count %llu\n",
		    h->ev->ev_group, h->ipl, h->irqnum,
		    h->mask, h->ev->ev_count);
	(*pr)("\n");
	last = -1;
	for (i = 0; i < NIPL; i++)
		if (irqmask[i] != last) { 
			(*pr)("ipl %2d: mask 0x%05x\n", i, irqmask[i]);
			last = irqmask[i];
		}
}
#endif
