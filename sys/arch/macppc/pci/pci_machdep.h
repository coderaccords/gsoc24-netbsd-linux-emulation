/*	$NetBSD: pci_machdep.h,v 1.1 1998/05/15 10:15:59 tsubai Exp $	*/

/*
 * Copyright (c) 1996 Christopher G. Demetriou.  All rights reserved.
 * Copyright (c) 1994 Charles Hannum.  All rights reserved.
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
 *	This product includes software developed by Charles Hannum.
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
 * Machine-specific definitions for PCI autoconfiguration.
 */

/*
 * powermac-specific PCI structure and type definitions.
 * NOT TO BE USED DIRECTLY BY MACHINE INDEPENDENT CODE.
 *
 * Configuration tag; created from a {bus,device,function} triplet by
 * pci_make_tag(), and passed to pci_conf_read() and pci_conf_write().
 * We could instead always pass the {bus,device,function} triplet to
 * the read and write routines, but this would cause extra overhead.
 */

struct bandit_addr {
	u_int *addr;
	u_int *data;
	bus_space_tag_t iot;
};
struct bandit_addr bandits[2];

/*
 * Types provided to machine-independent PCI code
 */
typedef int pci_chipset_tag_t;
typedef int pcitag_t;
typedef int pci_intr_handle_t;

/*
 * Functions provided to machine-independent PCI code.
 */
void		pci_attach_hook __P((struct device *, struct device *,
		    struct pcibus_attach_args *));
int		pci_bus_maxdevs __P((pci_chipset_tag_t, int));
pcitag_t	pci_make_tag __P((pci_chipset_tag_t, int, int, int));
void		pci_decompose_tag __P((pci_chipset_tag_t, pcitag_t,
		    int *, int *, int *));
pcireg_t	pci_conf_read __P((pci_chipset_tag_t, pcitag_t, int));
void		pci_conf_write __P((pci_chipset_tag_t, pcitag_t, int,
		    pcireg_t));
int		pci_intr_map __P((pci_chipset_tag_t, pcitag_t, int, int,
		    pci_intr_handle_t *));
const char	*pci_intr_string __P((pci_chipset_tag_t, pci_intr_handle_t));
void		*pci_intr_establish __P((pci_chipset_tag_t, pci_intr_handle_t,
		    int, int (*)(void *), void *));
void		pci_intr_disestablish __P((pci_chipset_tag_t, void *));
