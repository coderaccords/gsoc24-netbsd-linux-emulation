/*	$NetBSD: pci_machdep.h,v 1.1 2001/05/28 16:22:19 thorpej Exp $	*/

/*
 * Copyright (c) 1996 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: Chris G. Demetriou
 *
 * Permission to use, copy, modify and distribute this software and
 * its documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND
 * FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */

/*
 * Machine-specific definitions for PCI autoconfiguration.
 */

/*
 * Types provided to machine-independent PCI code
 */
typedef struct algor_pci_chipset *pci_chipset_tag_t;
typedef u_long pcitag_t;
typedef u_long pci_intr_handle_t;

/*
 * Forward declarations.
 */
struct pci_attach_args;

/*
 * algor-specific PCI structure and type definitions.
 * NOT TO BE USED DIRECTLY BY MACHINE INDEPENDENT CODE.
 */
struct algor_pci_chipset {
	void		*pc_conf_v;
	void		(*pc_attach_hook)(struct device *,
			    struct device *, struct pcibus_attach_args *);
	int		(*pc_bus_maxdevs)(void *, int);
	pcitag_t	(*pc_make_tag)(void *, int, int, int);
	void		(*pc_decompose_tag)(void *, pcitag_t, int *,
			    int *, int *);
	pcireg_t	(*pc_conf_read)(void *, pcitag_t, int);
	void		(*pc_conf_write)(void *, pcitag_t, int, pcireg_t);

	void		*pc_intr_v;
	int		(*pc_intr_map)(struct pci_attach_args *, 
			    pci_intr_handle_t *);
	const char	*(*pc_intr_string)(void *, pci_intr_handle_t);
	const struct evcnt *(*pc_intr_evcnt)(void *, pci_intr_handle_t);
	void		*(*pc_intr_establish)(void *, pci_intr_handle_t,
			    int, int (*)(void *), void *);
	void		(*pc_intr_disestablish)(void *, void *);

	void		(*pc_conf_interrupt)(void *, int, int, int,
			    int, int *);

	/* algor-specific */
	void		*(*pc_pciide_compat_intr_establish)(void *,
			    struct device *, struct pci_attach_args *, int,
			    int (*)(void *), void *);
};

/*
 * Functions provided to machine-independent PCI code.
 */
#define	pci_attach_hook(p, s, pba)					\
    (*(pba)->pba_pc->pc_attach_hook)((p), (s), (pba))
#define	pci_bus_maxdevs(c, b)						\
    (*(c)->pc_bus_maxdevs)((c)->pc_conf_v, (b))
#define	pci_make_tag(c, b, d, f)					\
    (*(c)->pc_make_tag)((c)->pc_conf_v, (b), (d), (f))
#define	pci_decompose_tag(c, t, bp, dp, fp)				\
    (*(c)->pc_decompose_tag)((c)->pc_conf_v, (t), (bp), (dp), (fp))
#define	pci_conf_read(c, t, r)						\
    (*(c)->pc_conf_read)((c)->pc_conf_v, (t), (r))
#define	pci_conf_write(c, t, r, v)					\
    (*(c)->pc_conf_write)((c)->pc_conf_v, (t), (r), (v))
#define	pci_intr_map(pa, ihp)						\
    (*(pa)->pa_pc->pc_intr_map)((pa), (ihp))
#define	pci_intr_string(c, ih)						\
    (*(c)->pc_intr_string)((c)->pc_intr_v, (ih))
#define	pci_intr_evcnt(c, ih)						\
    (*(c)->pc_intr_evcnt)((c)->pc_intr_v, (ih))
#define	pci_intr_establish(c, ih, l, h, a)				\
    (*(c)->pc_intr_establish)((c)->pc_intr_v, (ih), (l), (h), (a))
#define	pci_intr_disestablish(c, iv)					\
    (*(c)->pc_intr_disestablish)((c)->pc_intr_v, (iv))
#define	pci_conf_interrupt(c, b, d, f, s, lp)				\
    (*(c)->pc_conf_interrupt)((c)->pc_intr_v, (b), (d), (f), (s), (lp))

/*
 * algor-specific PCI functions.
 * NOT TO BE USED DIRECTLY BY MACHINE INDEPENDENT CODE.
 */
#define	algor_pciide_compat_intr_establish(c, d, p, ch, f, a)		\
    ((c)->pc_pciide_compat_intr_establish == NULL ? NULL :		\
     (*(c)->pc_pciide_compat_intr_establish)((c)->pc_conf_v, (d), (p),	\
	(ch), (f), (a)))

int	algor_get_ethaddr(struct pci_attach_args *, u_int8_t *);
