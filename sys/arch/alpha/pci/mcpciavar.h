/* $NetBSD: mcpciavar.h,v 1.1 1998/04/15 00:50:14 mjacob Exp $ */

/*
 * Copyright (c) 1998 by Matthew Jacob
 * NASA AMES Research Center.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice immediately at the beginning of the file, without modification,
 *    this list of conditions, and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <dev/pci/pcivar.h>
#include <sys/extent.h>

#include <alpha/pci/pci_sgmap_pte64.h>

#define	_FSTORE	(EXTENT_FIXED_STORAGE_SIZE(8) / sizeof(long))

/*
 * DWLPX configuration.
 */
struct mcpcia_config {
	int				cc_initted;
	struct alpha_bus_space		cc_iot;
	struct alpha_bus_space		cc_memt;
	struct extent *			cc_io_ex;
	struct extent *			cc_d_mem_ex;
	struct extent *			cc_s_mem_ex;
	struct alpha_pci_chipset	cc_pc;
	struct mcpcia_softc *		cc_sc;	/* back pointer */
	long				cc_io_exstorage[_FSTORE];
	long				cc_dmem_exstorage[_FSTORE];
	long				cc_smem_exstorage[_FSTORE];
	unsigned long			cc_sysbase;	/* shorthand */
	struct alpha_bus_dma_tag	cc_dmat_direct;
	struct alpha_bus_dma_tag	cc_dmat_sgmap;
	struct alpha_sgmap		cc_sgmap;
};

struct mcpcia_softc {
	struct device		mcpcia_dev;
	struct mcpcia_softc *	mcpcia_next;	/* next in a list */
	struct mcpcia_config	mcpcia_cc;	/* config info */
	u_int8_t		mcpcia_gid;	/* GID of this MCbus */
	u_int8_t		mcpcia_mid;	/* MCbus Module ID */
};
void mcpcia_config_cleanup __P((void));

extern struct mcpcia_config *mcpcia_eisaccp;

void	mcpcia_init __P((struct mcpcia_softc *));
void	mcpcia_pci_init __P((pci_chipset_tag_t, void *));
void	mcpcia_dma_init __P((struct mcpcia_config *));

void	mcpcia_bus_io_init __P((bus_space_tag_t, void *));
void	mcpcia_bus_mem_init __P((bus_space_tag_t, void *));

/*
 * IO Interrupt handler.
 */
void 	mcpcia_iointr __P((void *, unsigned long));

/*
 * There are four PCI slots per MCPCIA PCI bus here, but some are 'hidden'-
 * none seems to be higher than 6 though.
 */
#define	MCPCIA_MAXDEV	6
#define	MCPCIA_MAXSLOT	8

/*
 * Interrupt Stuff for MCPCIA systems.
 *
 * EISA interrupts (at vector 0x800) have to be shared interrupts-
 * and that can be easily managed. All the PCI interrupts are deterministic
 * in that they start at vector 0x900, 0x40 per PCI slot, 0x200 per
 * MCPCIA, 4 MCPCIAs per GCBUS....
 */
#define MCPCIA_EISA_KEYB_IRQ	1
#define MCPCIA_EISA_MOUSE_IRQ	12
#define MCPCIA_VEC_EISA		0x800
#define MCPCIA_VEC_PCI		0x900

/*
 * Special Vectors
 */
#define	MCPCIA_I2C_CVEC		0xA90
#define	MCPCIA_I2C_BVEC		0xAA0
