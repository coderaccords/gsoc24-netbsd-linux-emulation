/*	$NetBSD: umass_quirks.c,v 1.51 2003/09/09 01:35:47 mycroft Exp $	*/

/*
 * Copyright (c) 2001 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by MAEKAWA Masahide (gehenna@NetBSD.org).
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
 *	  This product includes software developed by the NetBSD
 *	  Foundation, Inc. and its contributors.
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

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: umass_quirks.c,v 1.51 2003/09/09 01:35:47 mycroft Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/buf.h>

#include <dev/scsipi/scsipi_all.h> /* for scsiconf.h below */
#include <dev/scsipi/scsiconf.h> /* for quirks defines */

#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usbdevs.h>

#include <dev/usb/umassvar.h>
#include <dev/usb/umass_quirks.h>

Static usbd_status umass_init_insystem(struct umass_softc *);
Static usbd_status umass_init_shuttle(struct umass_softc *);

Static void umass_fixup_sony(struct umass_softc *);
Static void umass_fixup_yedata(struct umass_softc *);

Static const struct umass_quirk umass_quirks[] = {
	{ { USB_VENDOR_FUJIPHOTO, USB_PRODUCT_FUJIPHOTO_MASS0100 },
	  UMASS_WPROTO_UNSPEC, UMASS_CPROTO_UNSPEC,
	  0,
	  PQUIRK_NOTUR | PQUIRK_NOSENSE,
	  UMATCH_DEVCLASS_DEVSUBCLASS_DEVPROTO,
	  NULL, NULL
	},

	{ { USB_VENDOR_INSYSTEM, USB_PRODUCT_INSYSTEM_USBCABLE },
	  UMASS_WPROTO_CBI, UMASS_CPROTO_ATAPI,
	  0,
	  PQUIRK_NOTUR,
	  UMATCH_VENDOR_PRODUCT,
	  umass_init_insystem, NULL
	},

	{ { USB_VENDOR_IOMEGA, USB_PRODUCT_IOMEGA_ZIP100 },
	  UMASS_WPROTO_UNSPEC, UMASS_CPROTO_UNSPEC,
	  0,
	  PQUIRK_NOTUR,
	  UMATCH_DEVCLASS_DEVSUBCLASS_DEVPROTO,
	  NULL, NULL
	},

	{ { USB_VENDOR_IOMEGA, USB_PRODUCT_IOMEGA_ZIP250 },
	  UMASS_WPROTO_UNSPEC, UMASS_CPROTO_UNSPEC,
	  0,
	  PQUIRK_NOTUR,
	  UMATCH_DEVCLASS_DEVSUBCLASS_DEVPROTO,
	  NULL, NULL
	},


	{ { USB_VENDOR_MELCO, USB_PRODUCT_MELCO_DUBPXXG },
	  UMASS_WPROTO_UNSPEC, UMASS_CPROTO_UNSPEC,
	  UMASS_QUIRK_FORCE_SHORT_INQUIRY,
	  0,
	  UMATCH_DEVCLASS_DEVSUBCLASS_DEVPROTO,
	  NULL, NULL
	},

	{ { USB_VENDOR_OLYMPUS, USB_PRODUCT_OLYMPUS_C1 },
	  UMASS_WPROTO_UNSPEC, UMASS_CPROTO_UNSPEC,
	  UMASS_QUIRK_WRONG_CSWSIG,
	  0,
	  UMATCH_DEVCLASS_DEVSUBCLASS_DEVPROTO,
	  NULL, NULL
	},

	{ { USB_VENDOR_SCANLOGIC, USB_PRODUCT_SCANLOGIC_SL11R },
	  UMASS_WPROTO_UNSPEC, UMASS_CPROTO_UFI,
	  UMASS_QUIRK_WRONG_CSWTAG,
	  0,
	  UMATCH_VENDOR_PRODUCT,
	  NULL, NULL
	},

	{ { USB_VENDOR_SHUTTLE, USB_PRODUCT_SHUTTLE_EUSB },
	  UMASS_WPROTO_CBI_I, UMASS_CPROTO_ATAPI,
	  0,
	  PQUIRK_NOTUR,
	  UMATCH_VENDOR_PRODUCT,
	  umass_init_shuttle, NULL
	},

	{ { USB_VENDOR_SHUTTLE, USB_PRODUCT_SHUTTLE_ZIOMMC },
	  UMASS_WPROTO_CBI_I, UMASS_CPROTO_ATAPI,
	  0,
	  PQUIRK_NOTUR,
	  UMATCH_VENDOR_PRODUCT,
	  NULL, NULL
	},

	{ { USB_VENDOR_SONY, USB_PRODUCT_SONY_DSC },
	  UMASS_WPROTO_UNSPEC, UMASS_CPROTO_UNSPEC,
	  0,
	  0,
	  UMATCH_DEVCLASS_DEVSUBCLASS_DEVPROTO,
	  NULL, umass_fixup_sony
	},

	{ { USB_VENDOR_SONY, USB_PRODUCT_SONY_MSC_U03 },
	  UMASS_WPROTO_CBI, UMASS_CPROTO_UFI,
	  0,
	  0,
	  UMATCH_DEVCLASS_DEVSUBCLASS_DEVPROTO,
	  NULL, umass_fixup_sony
	},

	{ { USB_VENDOR_SONY, USB_PRODUCT_SONY_MSC },
	  UMASS_WPROTO_CBI, UMASS_CPROTO_UFI,
	  UMASS_QUIRK_FORCE_SHORT_INQUIRY | UMASS_QUIRK_RS_NO_CLEAR_UA,
	  0,
	  UMATCH_DEVCLASS_DEVSUBCLASS_DEVPROTO,
	  NULL, NULL
	},

	{ { USB_VENDOR_YANO, USB_PRODUCT_YANO_U640MO },
	  UMASS_WPROTO_CBI_I, UMASS_CPROTO_ATAPI,
	  UMASS_QUIRK_FORCE_SHORT_INQUIRY,
	  0,
	  UMATCH_VENDOR_PRODUCT,
	  NULL, NULL
	},

	{ { USB_VENDOR_YEDATA, USB_PRODUCT_YEDATA_FLASHBUSTERU },
	  UMASS_WPROTO_UNSPEC, UMASS_CPROTO_UFI,
	  UMASS_QUIRK_RS_NO_CLEAR_UA,
	  0,
	  UMATCH_VENDOR_PRODUCT_REV,
	  NULL, umass_fixup_yedata
	},

	{ { USB_VENDOR_GENESYS,USB_PRODUCT_GENESYS_GL641USB },
	  UMASS_WPROTO_UNSPEC, UMASS_CPROTO_UNSPEC,
	  UMASS_QUIRK_FORCE_SHORT_INQUIRY,
	  0,
	  UMATCH_DEVCLASS_DEVSUBCLASS_DEVPROTO,
	  NULL, NULL
	},
};

const struct umass_quirk *
umass_lookup(u_int16_t vendor, u_int16_t product)
{
	return ((const struct umass_quirk *)
		usb_lookup(umass_quirks, vendor, product));
}

Static usbd_status
umass_init_insystem(struct umass_softc *sc)
{
	usbd_status err;

	err = usbd_set_interface(sc->sc_iface, 1);
	if (err) {
		DPRINTF(UDMASS_USB,
			("%s: could not switch to Alt Interface 1\n",
			USBDEVNAME(sc->sc_dev)));
		return (err);
	}

	return (USBD_NORMAL_COMPLETION);
}

Static usbd_status
umass_init_shuttle(struct umass_softc *sc)
{
	usb_device_request_t req;
	u_int8_t status[2];

	/* The Linux driver does this */
	req.bmRequestType = UT_READ_VENDOR_DEVICE;
	req.bRequest = 1;
	USETW(req.wValue, 0);
	USETW(req.wIndex, sc->sc_ifaceno);
	USETW(req.wLength, sizeof(status));

	return (usbd_do_request(sc->sc_udev, &req, &status));
}

Static void
umass_fixup_sony(struct umass_softc *sc)
{
	usb_interface_descriptor_t *id;

	id = usbd_get_interface_descriptor(sc->sc_iface);
	if (id->bInterfaceSubClass == 0xff) {
		sc->sc_cmd = UMASS_CPROTO_RBC;
	}
}

Static void
umass_fixup_yedata(struct umass_softc *sc)
{
	usb_device_descriptor_t *dd;

	dd = usbd_get_device_descriptor(sc->sc_udev);

	/*
	 * Revisions < 1.28 do not handle the interrupt endpoint very well.
	 */
	if (UGETW(dd->bcdDevice) < 0x128)
		sc->sc_wire = UMASS_WPROTO_CBI;
	else
		sc->sc_wire = UMASS_WPROTO_CBI_I;

	/*
	 * Revisions < 1.28 do not have the TEST UNIT READY command
	 * Revisions == 1.28 have a broken TEST UNIT READY
	 */
	if (UGETW(dd->bcdDevice) <= 0x128)
		sc->sc_busquirks |= PQUIRK_NOTUR;
}
