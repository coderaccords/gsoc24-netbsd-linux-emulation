/*	$NetBSD: pci_machdep.h,v 1.3 2003/10/16 22:56:29 fvdl Exp $	*/

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

#ifndef _X86_PCI_MACHDEP_H
#define _X86_PCI_MACHDEP_H

/*
 * Machine-specific definitions for PCI autoconfiguration.
 */
#define	__HAVE_PCIIDE_MACHDEP_COMPAT_INTR_ESTABLISH

/*
 * Many i386 PCI systems only work properly with I/O mapped space, in
 * particular, buses behind PCI-PCI bridges may not have memory
 * space mapped at all.  For this reason, tell drivers that have
 * a choice that we "prefer" I/O space.
 */
#define	PCI_PREFER_IOSPACE

/*
 * i386-specific PCI structure and type definitions.
 * NOT TO BE USED DIRECTLY BY MACHINE INDEPENDENT CODE.
 *
 * Configuration tag; created from a {bus,device,function} triplet by
 * pci_make_tag(), and passed to pci_conf_read() and pci_conf_write().
 * We could instead always pass the {bus,device,function} triplet to
 * the read and write routines, but this would cause extra overhead.
 *
 * Mode 2 is historical and deprecated by the Revision 2.0 specification.
 */
union x86_pci_tag_u {
	u_int32_t mode1;
	struct {
		u_int16_t port;
		u_int8_t enable;
		u_int8_t forward;
	} mode2;
};

extern struct x86_bus_dma_tag pci_bus_dma_tag;
#ifdef _LP64
extern struct x86_bus_dma_tag pci_bus_dma64_tag;
#endif

/*
 * Types provided to machine-independent PCI code
 */
typedef void *pci_chipset_tag_t;
typedef union x86_pci_tag_u pcitag_t;
typedef int pci_intr_handle_t;

/*
 * i386-specific PCI variables and functions.
 * NOT TO BE USED DIRECTLY BY MACHINE INDEPENDENT CODE.
 */
extern int pci_mode;
int		pci_mode_detect(void);
int		pci_bus_flags(void);
struct		pci_attach_args;

/*
 * Functions provided to machine-independent PCI code.
 */
void		pci_attach_hook(struct device *, struct device *,
		    struct pcibus_attach_args *);
int		pci_bus_maxdevs(pci_chipset_tag_t, int);
pcitag_t	pci_make_tag(pci_chipset_tag_t, int, int, int);
void		pci_decompose_tag(pci_chipset_tag_t, pcitag_t,
		    int *, int *, int *);
pcireg_t	pci_conf_read(pci_chipset_tag_t, pcitag_t, int);
void		pci_conf_write(pci_chipset_tag_t, pcitag_t, int,
		    pcireg_t);
int		pci_intr_map(struct pci_attach_args *, pci_intr_handle_t *);
const char	*pci_intr_string(pci_chipset_tag_t, pci_intr_handle_t);
const struct evcnt *pci_intr_evcnt(pci_chipset_tag_t, pci_intr_handle_t);
void		*pci_intr_establish(pci_chipset_tag_t, pci_intr_handle_t,
		    int, int (*)(void *), void *);
void		pci_intr_disestablish(pci_chipset_tag_t, void *);

#define	pci_enumerate_bus(sc, m, p)					\
	pci_enumerate_bus_generic((sc), (m), (p))

/*
 * ALL OF THE FOLLOWING ARE MACHINE-DEPENDENT, AND SHOULD NOT BE USED
 * BY PORTABLE CODE.
 */

/*
 * Section 6.2.4, `Miscellaneous Functions' of the PCI Specification,
 * says that 255 means `unknown' or `no connection' to the interrupt
 * controller on a PC.
 */
#define	X86_PCI_INTERRUPT_LINE_NO_CONNECTION	0xff

#endif /* _X86_PCI_MACHDEP_H */
