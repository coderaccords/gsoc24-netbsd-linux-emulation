/*	$NetBSD: pci_intr_machdep.c,v 1.18 2010/12/20 00:25:46 matt Exp $	*/

/*-
 * Copyright (c) 1997, 1998, 2009 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe of the Numerical Aerospace Simulation Facility,
 * NASA Ames Research Center.
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

/*
 * Copyright (c) 1996 Christopher G. Demetriou.  All rights reserved.
 * Copyright (c) 1994 Charles M. Hannum.  All rights reserved.
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
 *	This product includes software developed by Charles M. Hannum.
 * 4. The name of the author may not be used to endorse or promote products
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

/*
 * Machine-specific functions for PCI autoconfiguration.
 *
 * On PCs, there are two methods of generating PCI configuration cycles.
 * We try to detect the appropriate mechanism for this machine and set
 * up a few function pointers to access the correct method directly.
 *
 * The configuration method can be hard-coded in the config file by
 * using `options PCI_CONF_MODE=N', where `N' is the configuration mode
 * as defined section 3.6.4.1, `Generating Configuration Cycles'.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: pci_intr_machdep.c,v 1.18 2010/12/20 00:25:46 matt Exp $");

#include <sys/types.h>
#include <sys/param.h>
#include <sys/time.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/device.h>
#include <sys/intr.h>

#include <dev/pci/pcivar.h>

#include "ioapic.h"
#include "eisa.h"
#include "acpica.h"
#include "opt_mpbios.h"
#include "opt_acpi.h"

#if NIOAPIC > 0 || NACPICA > 0
#include <machine/i82093var.h>
#include <machine/mpconfig.h>
#include <machine/mpbiosvar.h>
#include <machine/pic.h>
#endif

#ifdef MPBIOS
#include <machine/mpbiosvar.h>
#endif

#if NACPICA > 0
#include <machine/mpacpi.h>
#endif

#define	MPSAFE_MASK	0x80000000

int
pci_intr_map(struct pci_attach_args *pa, pci_intr_handle_t *ihp)
{
	int pin = pa->pa_intrpin;
	int line = pa->pa_intrline;
	pci_chipset_tag_t pc;
#if NIOAPIC > 0 || NACPICA > 0
	int rawpin = pa->pa_rawintrpin;
	int bus, dev, func;
#endif

	if ((pc = pa->pa_pc) != NULL) {
		if ((pc->pc_present & PCI_OVERRIDE_INTR_MAP) != 0)
			return (*pc->pc_ov->ov_intr_map)(pc->pc_ctx, pa, ihp);
		if (pc->pc_super != NULL) {
			struct pci_attach_args paclone = *pa;
			paclone.pa_pc = pc->pc_super;
			return pci_intr_map(&paclone, ihp);
		}
	}

	if (pin == 0) {
		/* No IRQ used. */
		goto bad;
	}

	*ihp = 0;

	if (pin > PCI_INTERRUPT_PIN_MAX) {
		aprint_normal("pci_intr_map: bad interrupt pin %d\n", pin);
		goto bad;
	}

#if NIOAPIC > 0 || NACPICA > 0
	pci_decompose_tag(pc, pa->pa_tag, &bus, &dev, &func);
	if (mp_busses != NULL) {
		if (intr_find_mpmapping(bus, (dev<<2)|(rawpin-1), ihp) == 0) {
			if ((*ihp & 0xff) == 0)
				*ihp |= line;
			return 0;
		}
		/*
		 * No explicit PCI mapping found. This is not fatal,
		 * we'll try the ISA (or possibly EISA) mappings next.
		 */
	}
#endif

	/*
	 * Section 6.2.4, `Miscellaneous Functions', says that 255 means
	 * `unknown' or `no connection' on a PC.  We assume that a device with
	 * `no connection' either doesn't have an interrupt (in which case the
	 * pin number should be 0, and would have been noticed above), or
	 * wasn't configured by the BIOS (in which case we punt, since there's
	 * no real way we can know how the interrupt lines are mapped in the
	 * hardware).
	 *
	 * XXX
	 * Since IRQ 0 is only used by the clock, and we can't actually be sure
	 * that the BIOS did its job, we also recognize that as meaning that
	 * the BIOS has not configured the device.
	 */
	if (line == 0 || line == X86_PCI_INTERRUPT_LINE_NO_CONNECTION) {
		aprint_normal("pci_intr_map: no mapping for pin %c (line=%02x)\n",
		       '@' + pin, line);
		goto bad;
	} else {
		if (line >= NUM_LEGACY_IRQS) {
			aprint_normal("pci_intr_map: bad interrupt line %d\n", line);
			goto bad;
		}
		if (line == 2) {
			aprint_normal("pci_intr_map: changed line 2 to line 9\n");
			line = 9;
		}
	}
#if NIOAPIC > 0 || NACPICA > 0
	if (mp_busses != NULL) {
		if (intr_find_mpmapping(mp_isa_bus, line, ihp) == 0) {
			if ((*ihp & 0xff) == 0)
				*ihp |= line;
			return 0;
		}
#if NEISA > 0
		if (intr_find_mpmapping(mp_eisa_bus, line, ihp) == 0) {
			if ((*ihp & 0xff) == 0)
				*ihp |= line;
			return 0;
		}
#endif
		aprint_normal("pci_intr_map: bus %d dev %d func %d pin %d; line %d\n",
		    bus, dev, func, pin, line);
		aprint_normal("pci_intr_map: no MP mapping found\n");
	}
#endif

	*ihp = line;
	return 0;

bad:
	*ihp = -1;
	return 1;
}

const char *
pci_intr_string(pci_chipset_tag_t pc, pci_intr_handle_t ih)
{

	if (pc != NULL) {
		if ((pc->pc_present & PCI_OVERRIDE_INTR_STRING) != 0)
			return (*pc->pc_ov->ov_intr_string)(pc->pc_ctx, pc, ih);
		if (pc->pc_super != NULL)
			return pci_intr_string(pc->pc_super, ih);
	}

	return intr_string(ih & ~MPSAFE_MASK);
}


const struct evcnt *
pci_intr_evcnt(pci_chipset_tag_t pc, pci_intr_handle_t ih)
{

	if (pc != NULL) {
		if ((pc->pc_present & PCI_OVERRIDE_INTR_EVCNT) != 0)
			return (*pc->pc_ov->ov_intr_evcnt)(pc->pc_ctx, pc, ih);
		if (pc->pc_super != NULL)
			return pci_intr_evcnt(pc->pc_super, ih);
	}

	/* XXX for now, no evcnt parent reported */
	return NULL;
}

int
pci_intr_setattr(pci_chipset_tag_t pc, pci_intr_handle_t *ih,
		 int attr, uint64_t data)
{

	switch (attr) {
	case PCI_INTR_MPSAFE:
		if (data) {
			 *ih |= MPSAFE_MASK;
		} else {
			 *ih &= ~MPSAFE_MASK;
		}
		/* XXX Set live if already mapped. */
		return 0;
	default:
		return ENODEV;
	}
}

void *
pci_intr_establish(pci_chipset_tag_t pc, pci_intr_handle_t ih,
    int level, int (*func)(void *), void *arg)
{
	int pin, irq;
	struct pic *pic;
#if NIOAPIC > 0
	struct ioapic_softc *ioapic;
#endif
	bool mpsafe;

	if (pc != NULL) {
		if ((pc->pc_present & PCI_OVERRIDE_INTR_ESTABLISH) != 0) {
			return (*pc->pc_ov->ov_intr_establish)(pc->pc_ctx,
			    pc, ih, level, func, arg);
		}
		if (pc->pc_super != NULL) {
			return pci_intr_establish(pc->pc_super, ih, level, func,
			    arg);
		}
	}

	pic = &i8259_pic;
	pin = irq = (ih & ~MPSAFE_MASK);
	mpsafe = ((ih & MPSAFE_MASK) != 0);

#if NIOAPIC > 0
	if (ih & APIC_INT_VIA_APIC) {
		ioapic = ioapic_find(APIC_IRQ_APIC(ih));
		if (ioapic == NULL) {
			aprint_normal("pci_intr_establish: bad ioapic %d\n",
			    APIC_IRQ_APIC(ih));
			return NULL;
		}
		pic = &ioapic->sc_pic;
		pin = APIC_IRQ_PIN(ih);
		irq = APIC_IRQ_LEGACY_IRQ(ih);
		if (irq < 0 || irq >= NUM_LEGACY_IRQS)
			irq = -1;
	}
#endif

	return intr_establish(irq, pic, pin, IST_LEVEL, level, func, arg,
	    mpsafe);
}

void
pci_intr_disestablish(pci_chipset_tag_t pc, void *cookie)
{

	if (pc != NULL) {
		if ((pc->pc_present & PCI_OVERRIDE_INTR_ESTABLISH) != 0) {
			(*pc->pc_ov->ov_intr_disestablish)(pc->pc_ctx,
			    pc, cookie);
			return;
		}
		if (pc->pc_super != NULL) {
			pci_intr_disestablish(pc->pc_super, cookie);
			return;
		}
	}

	intr_disestablish(cookie);
}
