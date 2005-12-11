/*	$NetBSD: ibm_6015.c,v 1.3 2005/12/11 12:18:48 christos Exp $	*/

/*-
 * Copyright (c) 2004 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Klaus J. Klein.
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

#include <sys/param.h>

#include <machine/intr.h>
#include <machine/platform.h>

void pci_intr_fixup_ibm_6015(int, int, int, int *);

struct platform platform_ibm_6015 = {
	"IBM PPS Model 6015",			/* model */
	platform_generic_match,			/* match */
	prep_pci_get_chipset_tag_direct,	/* pci_get_chipset_tag */
	pci_intr_fixup_ibm_6015,		/* pci_intr_fixup */
	init_intr_ivr,				/* init_intr */
	cpu_setup_ibm_generic,			/* cpu_setup */
	reset_prep_generic,			/* reset */
	obiodevs_nodev,				/* obiodevs */
};

void
pci_intr_fixup_ibm_6015(int bus, int dev, int swiz, int *line)
{
	if (bus != 0)
		return;

	switch (dev) {
	case 12:		/* NCR 53c810 */
		*line = 13;
		break;
	case 13:		/* PCI slots */
	case 14:
	case 15:
	case 16:
	case 17:
	case 18:
	case 19:
		*line = 15;
		break;
	}
}
