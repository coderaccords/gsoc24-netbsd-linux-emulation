/*	$NetBSD: cy_pci.c,v 1.11 2001/01/20 02:15:02 thorpej Exp $	*/

/*
 * cy_pci.c
 *
 * Driver for Cyclades Cyclom-8/16/32 multiport serial cards
 * (currently not tested with Cyclom-32 cards)
 *
 * Timo Rossi, 1996
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>

#include <machine/bus.h>
#include <machine/intr.h>

#include <dev/pci/pcivar.h>
#include <dev/pci/pcireg.h>
#include <dev/pci/pcidevs.h>

#include <dev/ic/cd1400reg.h>
#include <dev/ic/cyreg.h>
#include <dev/ic/cyvar.h>

struct cy_pci_softc {
	struct cy_softc sc_cy;		/* real cy softc */

	bus_space_tag_t sc_iot;		/* PLX runtime i/o tag */
	bus_space_handle_t sc_ioh;	/* PLX runtime i/o handle */
};

int	cy_pci_match(struct device *, struct cfdata *, void *);
void	cy_pci_attach(struct device *, struct device *, void *);

struct cfattach cy_pci_ca = {
	sizeof(struct cy_pci_softc), cy_pci_match, cy_pci_attach
};

static const struct cy_pci_product {
	pci_product_id_t cp_product;	/* product ID */
	pcireg_t cp_memtype;		/* memory type */
} cy_pci_products[] = {
	{ PCI_PRODUCT_CYCLADES_CYCLOMY_1,
	  PCI_MAPREG_TYPE_MEM|PCI_MAPREG_MEM_TYPE_32BIT_1M },
	{ PCI_PRODUCT_CYCLADES_CYCLOM4Y_1,
	  PCI_MAPREG_TYPE_MEM|PCI_MAPREG_MEM_TYPE_32BIT_1M },
	{ PCI_PRODUCT_CYCLADES_CYCLOM8Y_1,
	  PCI_MAPREG_TYPE_MEM|PCI_MAPREG_MEM_TYPE_32BIT_1M },

	{ PCI_PRODUCT_CYCLADES_CYCLOMY_2,
	  PCI_MAPREG_TYPE_MEM|PCI_MAPREG_MEM_TYPE_32BIT },
	{ PCI_PRODUCT_CYCLADES_CYCLOM4Y_2,
	  PCI_MAPREG_TYPE_MEM|PCI_MAPREG_MEM_TYPE_32BIT },
	{ PCI_PRODUCT_CYCLADES_CYCLOM8Y_2,
	  PCI_MAPREG_TYPE_MEM|PCI_MAPREG_MEM_TYPE_32BIT },

	{ 0,
	  0 },
};
static const int cy_pci_nproducts =
    sizeof(cy_pci_products) / sizeof(cy_pci_products[0]);

static const struct cy_pci_product *
cy_pci_lookup(const struct pci_attach_args *pa)
{
	const struct cy_pci_product *cp;
	int i;

	if (PCI_VENDOR(pa->pa_id) != PCI_VENDOR_CYCLADES)
		return (NULL);

	for (i = 0; i < cy_pci_nproducts; i++) {
		cp = &cy_pci_products[i];
		if (PCI_PRODUCT(pa->pa_id) == cp->cp_product)
			return (cp);
	}
	return (NULL);
}

int
cy_pci_match(struct device *parent, struct cfdata *match, void *aux)
{
	struct pci_attach_args *pa = aux;

	return (cy_pci_lookup(pa) != NULL);
}

void
cy_pci_attach(struct device *parent, struct device *self, void *aux)
{
	struct cy_pci_softc *psc = (void *) self;
	struct cy_softc *sc = (void *) &psc->sc_cy;
	struct pci_attach_args *pa = aux;
	pci_intr_handle_t ih;
	const struct cy_pci_product *cp;
	const char *intrstr;
	int plx_ver;

	sc->sc_bustype = CY_BUSTYPE_PCI;

	cp = cy_pci_lookup(pa);
	if (cp == NULL)
		panic("cy_pci_attach: impossible");

	printf(": Cyclades-Y multiport serial\n");

	if (pci_mapreg_map(pa, 0x14, PCI_MAPREG_TYPE_IO, 0,
	    &psc->sc_iot, &psc->sc_ioh, NULL, NULL) != 0) {
		printf("%s: unable to map PLX registers\n",
		    sc->sc_dev.dv_xname);
		return;
	}

	if (pci_mapreg_map(pa, 0x18, cp->cp_memtype, 0,
	    &sc->sc_memt, &sc->sc_bsh, NULL, NULL) != 0) {
		printf("%s: unable to map device registers\n",
		    sc->sc_dev.dv_xname);
		return;
	}

	if (cy_find(sc) == 0) {
		printf("%s: unable to find CD1400s\n", sc->sc_dev.dv_xname);
		return;
	}

	/*
	 * XXX Like the Cyclades-Z, we should really check the EEPROM to
	 * determine the "poll or interrupt" setting.  For now, we always
	 * map the interrupt and enable it in the PLX.
	 */

	/* Map and establish the interrupt. */
	if (pci_intr_map(pa, &ih) != 0) {
		printf(": unable to map interrupt\n");
		return;
	}
	intrstr = pci_intr_string(pa->pa_pc, ih);
	sc->sc_ih = pci_intr_establish(pa->pa_pc, ih, IPL_TTY, cy_intr, sc);
	if (sc->sc_ih == NULL) {
		printf(": unable to establish interrupt");
		if (intrstr != NULL)
			printf(" at %s", intrstr);
		printf("\n");
		return;
	}
	printf("%s: interrupting at %s\n", sc->sc_dev.dv_xname, intrstr);

	/* attach the hardware */
	cy_attach(parent, self, aux);

	plx_ver = bus_space_read_1(sc->sc_memt, sc->sc_bsh, CY_PLX_VER) & 0x0f;

	/* Enable PCI card interrupts */
	switch (plx_ver) {
	case CY_PLX_9050:
		bus_space_write_2(psc->sc_iot, psc->sc_ioh, CY_PCI_INTENA_9050,
		    bus_space_read_2(psc->sc_iot, psc->sc_ioh,
		    CY_PCI_INTENA_9050) | 0x40);
		break;

	case CY_PLX_9060:
	case CY_PLX_9080:
	default:
		bus_space_write_2(psc->sc_iot, psc->sc_ioh, CY_PCI_INTENA,
		    bus_space_read_2(psc->sc_iot, psc->sc_ioh,
		    CY_PCI_INTENA) | 0x900);
	}
}
