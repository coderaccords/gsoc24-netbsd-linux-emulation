/*	$NetBSD: umass.c,v 1.7 1999/08/29 19:58:55 thorpej Exp $	*/

/*-
 * Copyright (c) 1999 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe.
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
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
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

/*-
 * Copyright (c) 1999 MAEKAWA Masahide <bishop@rr.iij4u.or.jp>,
 *		      Nick Hibma <hibma@skylink.it>
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
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	FreeBSD: src/sys/dev/usb/umass.c,v 1.8 1999/06/20 15:46:13 n_hibma Exp
 */

/*
 * Universal Serial Bus Mass Storage Class Control/Interrupt/Bulk (CBI)
 * Specification:
 *
 *	http://www.usb.org/developers/usbmass-cbi10.pdf
 *
 * Universal Serial Bus Mass Storage Bulk Only 1.0rc4 Specification:
 *
 *	http://www.usb.org/developers/usbmassbulk_10rc4.pdf
 *
 * Relevant parts of the old spec (Bulk-only 0.9) have been quoted
 * in the source.
 */

/* To do:
 *	x The umass_usb_transfer routine uses synchroneous transfers. This
 *	  should be changed to async and state handling.
 *
 *	x Should handle more than just Iomega USB Zip drives.  There are
 *	  a fair number of USB->SCSI dongles out there.
 *
 *	x Need to implement SCSI command timeout/abort handling.
 *
 *	x Need to handle SCSI Sense handling.
 *
 *	x Need to handle hot-unplug.
 *
 *	x Add support for other than Bulk.
 *
 *	x Add support for other than SCSI.
 */

/* Authors: (with short acronyms for comments)
 *   NWH - Nick Hibma <hibma@skylink.it>
 *   JRT - Jason R. Thorpe <thorpej@shagadelic.org>
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/device.h>
#include <sys/buf.h>
#include <sys/proc.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usbdi_util.h>

#include <dev/scsipi/scsi_all.h>
#include <dev/scsipi/scsipi_all.h>
#include <dev/scsipi/scsiconf.h> 

#ifdef UMASS_DEBUG
#define	DPRINTF(m, x)	if (umassdebug & (m)) logprintf x
#define UDMASS_SCSI	0x00020000
#define UDMASS_USB	0x00040000
#define UDMASS_BULK	0x00080000
#define UDMASS_ALL	0xffff0000
int umassdebug = /* UDMASS_SCSI|UDMASS_BULK|UDMASS_USB */ 0;
#else
#define	DPRINTF(m, x)
#endif

typedef struct umass_softc {
	bdevice			sc_dev;		/* base device */
	usbd_interface_handle	sc_iface;	/* the interface we use */

	u_int8_t		sc_bulkout;	/* bulk-out Endpoint Address */
	usbd_pipe_handle	sc_bulkout_pipe;
	u_int8_t		sc_bulkin;	/* bulk-in Endpoint Address */
	usbd_pipe_handle	sc_bulkin_pipe;

	struct scsipi_link	sc_link;	/* prototype for devs */
	struct scsipi_adapter	sc_adapter;
} umass_softc_t;

#define USBD_COMMAND_FAILED	USBD_INVAL	/* redefine some errors for */

#define UMASS_SCSIID_HOST	0x00
#define UMASS_SCSIID_DEVICE	0x01

#define DIR_OUT		0
#define DIR_IN		1
#define DIR_NONE	2

/* Bulk-Only specific request */
#define	UR_RESET	0xff
#define	UR_GET_MAX_LUN	0xfe

/* Bulk-Only Mass Storage features */
/* Command Block Wrapper */
typedef struct {
	uDWord		dCBWSignature;
#define  CBWSIGNATURE		0x43425355
	uDWord		dCBWTag;
	uDWord		dCBWDataTransferLength;
	uByte		bCBWFlags;
#define	 CBWFLAGS_OUT	0x00
#define	 CBWFLAGS_IN	0x80
	uByte		bCBWLUN;
	uByte		bCDBLength;
	uByte		CBWCDB[16];
} usb_bulk_cbw_t;
#define	USB_BULK_CBW_SIZE	31

/* Command Status Wrapper */
typedef struct {
	uDWord		dCSWSignature;
#define	 CSWSIGNATURE		0x53425355
	uDWord		dCSWTag;
	uDWord		dCSWDataResidue;
	uByte		bCSWStatus;
#define  CSWSTATUS_GOOD		0x0
#define  CSWSTATUS_FAILED	0x1
#define  CSWSTATUS_PHASE	0x2
} usb_bulk_csw_t;
#define	USB_BULK_CSW_SIZE	13


USB_DECLARE_DRIVER(umass);

/* USB related functions */
usbd_status umass_usb_transfer __P((usbd_interface_handle iface,
				usbd_pipe_handle pipe,
		                void *buf, int buflen,
				int flags, int *xfer_size));

/* Bulk-Only related functions */
usbd_status umass_bulk_reset	__P((umass_softc_t *sc));
usbd_status umass_bulk_get_max_lun __P((umass_softc_t *sc, u_int8_t *maxlun));
usbd_status umass_bulk_transfer	__P((umass_softc_t *sc, int lun,
				void *cmd, int cmdlen,
		    		void *data, int datalen,
				int dir, int *residue));

/* SCSIPI related functions */
struct scsipi_device umass_dev = {
	NULL,			/* Use default error handler */
	NULL,			/* have a queue, served by this */
	NULL,			/* have no async handler */
	NULL,			/* Use default `done' routine */
};

void	umass_minphys		__P((struct buf *));
int	umass_scsi_cmd		__P((struct scsipi_xfer *));



USB_MATCH(umass)
{
	USB_MATCH_START(umass, uaa);
	usb_interface_descriptor_t *id;

	if (!uaa->iface)
		return(UMATCH_NONE);

	id = usbd_get_interface_descriptor(uaa->iface);
	if (id
	    && id->bInterfaceClass == UCLASS_MASS
	    && id->bInterfaceSubClass == USUBCLASS_SCSI
	    && id->bInterfaceProtocol == UPROTO_MASS_BULK)
		return(UMATCH_IFACECLASS_IFACESUBCLASS_IFACEPROTO);

	return(UMATCH_NONE);
}

USB_ATTACH(umass)
{
	USB_ATTACH_START(umass, sc, uaa);
	usb_interface_descriptor_t *id;
	usb_endpoint_descriptor_t *ed;
	char devinfo[1024];
	usbd_status err;
	int i;
	u_int8_t maxlun;

	sc->sc_iface = uaa->iface;
	sc->sc_bulkout_pipe = NULL;
	sc->sc_bulkin_pipe = NULL;

	usbd_devinfo(uaa->device, 0, devinfo);
	USB_ATTACH_SETUP;

	id = usbd_get_interface_descriptor(sc->sc_iface);
	printf("%s: %s, iclass %d/%d/%d\n", USBDEVNAME(sc->sc_dev), devinfo,
	       id->bInterfaceClass, id->bInterfaceSubClass,
	       id->bInterfaceProtocol);

	/*
	 * A Bulk-Only Mass Storage device supports the following endpoints,
	 * in addition to the Endpoint 0 for Control transfer that is required
	 * of all USB devices:
	 * (a) bulk-in endpoint.
	 * (b) bulk-out endpoint.
	 *
	 * The endpoint addresses are not fixed, so we have to read them
	 * from the device descriptors of the current interface.
	 */
	for (i = 0 ; i < id->bNumEndpoints ; i++) {
		ed = usbd_interface2endpoint_descriptor(sc->sc_iface, i);
		if (!ed) {
			printf("%s: could not read endpoint descriptor\n",
			       USBDEVNAME(sc->sc_dev));
			USB_ATTACH_ERROR_RETURN;
		}
		if (UE_GET_DIR(ed->bEndpointAddress) == UE_IN
		    && (ed->bmAttributes & UE_XFERTYPE) == UE_BULK) {
			sc->sc_bulkin = ed->bEndpointAddress;
		} else if (UE_GET_DIR(ed->bEndpointAddress) == UE_OUT
		    && (ed->bmAttributes & UE_XFERTYPE) == UE_BULK) {
			sc->sc_bulkout = ed->bEndpointAddress;
		}
	}

	/*
	 * Get the maximum LUN supported by the device.
	 */
	err = umass_bulk_get_max_lun(sc, &maxlun);
	if (err != USBD_NORMAL_COMPLETION) {
		printf("%s: unable to get Max Lun: %s\n",
		    USBDEVNAME(sc->sc_dev), usbd_errstr(err));
		USB_ATTACH_ERROR_RETURN;
	}

	/* Open the bulk-in and -out pipe */
	err = usbd_open_pipe(sc->sc_iface, sc->sc_bulkout,
				USBD_EXCLUSIVE_USE, &sc->sc_bulkout_pipe);
	if (err) {
		DPRINTF(UDMASS_USB, ("cannot open bulk out pipe (address %d)\n",
			sc->sc_bulkout));
		USB_ATTACH_ERROR_RETURN;
	}
	err = usbd_open_pipe(sc->sc_iface, sc->sc_bulkin,
				USBD_EXCLUSIVE_USE, &sc->sc_bulkin_pipe);
	if (err) {
		DPRINTF(UDMASS_USB, ("cannot open bulk in pipe (address %d)\n",
			sc->sc_bulkin));
		usbd_close_pipe(sc->sc_bulkout_pipe);
		USB_ATTACH_ERROR_RETURN;
	}

	/* attach the device to the SCSI layer */
	sc->sc_adapter.scsipi_cmd = umass_scsi_cmd;
	sc->sc_adapter.scsipi_minphys = umass_minphys;

	sc->sc_link.scsipi_scsi.channel = SCSI_CHANNEL_ONLY_ONE;
	sc->sc_link.adapter_softc = sc;
	sc->sc_link.scsipi_scsi.adapter_target = UMASS_SCSIID_HOST;
	sc->sc_link.adapter = &sc->sc_adapter;
	sc->sc_link.device = &umass_dev;
	sc->sc_link.openings = 4;		/* XXX */
	sc->sc_link.scsipi_scsi.max_target = UMASS_SCSIID_DEVICE; /* XXX */
	sc->sc_link.scsipi_scsi.max_lun = maxlun;
	sc->sc_link.type = BUS_SCSI;

	if (config_found(&sc->sc_dev, &sc->sc_link, scsiprint) == NULL) {
		usbd_close_pipe(sc->sc_bulkout_pipe);
		usbd_close_pipe(sc->sc_bulkin_pipe);
		/* XXX Not really an error. */
		USB_ATTACH_ERROR_RETURN;
	}

	USB_ATTACH_SUCCESS_RETURN;
}

int
umass_activate(self, act)
	struct device *self;
	enum devact act;
{

	switch (act) {
	case DVACT_ACTIVATE:
		return (EOPNOTSUPP);
		break;

	case DVACT_DEACTIVATE:
		/* XXX Not supported yet. */
		return (EOPNOTSUPP);
		break;
	}
	return (0);
}

int
umass_detach(self, flags)
	struct device *self;
	int flags;
{

	/* XXX Not supported yet. */
	return (EOPNOTSUPP);
}

/* Performs a request over a pipe.
 *
 * flags: Can be set to USBD_SHORT_XFER_OK
 * xfer_size: if not null returns the nr. of bytes transferred
 *
 * If the returned error is USBD_STALLED the pipe stall has
 * been cleared again.
 */

usbd_status
umass_usb_transfer(usbd_interface_handle iface, usbd_pipe_handle pipe,
		   void *buf, int buflen, int flags, int *xfer_size)
{
	usbd_request_handle reqh;
	usbd_private_handle priv;
	void *buffer;
	int size;
	usbd_status err;

	/* A transfer is done synchronously. We create and schedule the
	 * transfer and then wait for it to complete
	 */

	reqh = usbd_alloc_request();
	if (!reqh) {
		DPRINTF(UDMASS_USB, ("Not enough memory\n"));
		return USBD_NOMEM;
	}

	(void) usbd_setup_request(reqh, pipe, 0, buf, buflen,
				flags, 3000 /*ms*/, NULL);
	err = usbd_sync_transfer(reqh);
	if (err) {
		DPRINTF(UDMASS_USB, ("transfer failed, %s\n",
			usbd_errstr(err)));
		usbd_free_request(reqh);
		return(err);
	}

	usbd_get_request_status(reqh, &priv, &buffer, &size, &err);

	if (xfer_size)
		*xfer_size = size;

	usbd_free_request(reqh);
	return(USBD_NORMAL_COMPLETION);
}

usbd_status
umass_bulk_get_max_lun(umass_softc_t *sc, u_int8_t *maxlun)
{
	usbd_device_handle dev;
	usb_device_request_t req;
	usbd_status err;
	usb_interface_descriptor_t *id;

	DPRINTF(UDMASS_BULK, ("%s: Get Max Lun\n", USBDEVNAME(sc->sc_dev)));

	usbd_interface2device_handle(sc->sc_iface, &dev);
	id = usbd_get_interface_descriptor(sc->sc_iface);

	/* The Get Max Lun command is a class-specific request. */
	req.bmRequestType = UT_WRITE_CLASS_INTERFACE;
	req.bRequest = UR_GET_MAX_LUN;
	USETW(req.wValue, 0);
	USETW(req.wIndex, id->bInterfaceNumber);
	USETW(req.wLength, 1);

	err = usbd_do_request(dev, &req, maxlun);
	switch (err) {
	case USBD_NORMAL_COMPLETION:
		DPRINTF(UDMASS_BULK, ("%s: Max Lun %d\n",
		    USBDEVNAME(sc->sc_dev), *maxlun));
		break;

	case USBD_STALLED:
		/*
		 * Device doesn't support Get Max Lun request.  Default
		 * to `0' (one LUN).
		 */
		*maxlun = 0;
		err = USBD_NORMAL_COMPLETION;
		DPRINTF(UDMASS_BULK, ("%s: Get Max Lun not supported\n",
		    USBDEVNAME(sc->sc_dev)));
		break;

	default:
		printf("%s: Get Max Lun failed: %s\n",
		    USBDEVNAME(sc->sc_dev), usbd_errstr(err));
		/* XXX Should we port_reset the device? */
	}

	return (err);
}

usbd_status
umass_bulk_reset(umass_softc_t *sc)
{
	usbd_device_handle dev;
        usb_device_request_t req;
	usbd_status err;
	usb_interface_descriptor_t *id;

	/*
	 * Reset recovery (5.3.4 in Universal Serial Bus Mass Storage Class)
	 *
	 * For Reset Recovery the host shall issue in the following order:
	 * a) a Bulk-Only Mass Storage Reset
	 * b) a Clear Feature HALT to the Bulk-In endpoint
	 * c) a Clear Feature HALT to the Bulk-Out endpoint
	 */

	DPRINTF(UDMASS_BULK, ("%s: Reset\n",
		USBDEVNAME(sc->sc_dev)));

	usbd_interface2device_handle(sc->sc_iface, &dev);
	id = usbd_get_interface_descriptor(sc->sc_iface);

	/* the reset command is a class specific interface request */
	req.bmRequestType = UT_WRITE_CLASS_INTERFACE;
	req.bRequest = UR_RESET;
	USETW(req.wValue, 0);
	USETW(req.wIndex, id->bInterfaceNumber);
	USETW(req.wLength, 0);

	err = usbd_do_request(dev, &req, 0);
	if (err) {
		printf("%s: Reset failed, %s\n",
			USBDEVNAME(sc->sc_dev), usbd_errstr(err));
		/* XXX we should port_reset the device */
		return(err);
	}

	usbd_clear_endpoint_stall(sc->sc_bulkout_pipe);
	usbd_clear_endpoint_stall(sc->sc_bulkin_pipe);

	/*
	 * XXX we should convert this into a more friendly delay.
	 * Perhaps a tsleep (or is this routine run from int context?)
	 */

	DELAY(2500000 /*us*/);

	return(USBD_NORMAL_COMPLETION);
}

/*
 * Do a Bulk-Only transfer with cmdlen bytes from cmd, possibly
 * a data phase of datalen bytes from/to data and finally a csw read
 * phase.
 *
 * If the data direction was inbound a maximum of datalen bytes
 * is stored in the buffer pointed to by data.
 * The status returned is USBD_NORMAL_COMPLETION,
 * USBD_IOERROR, USBD_COMMAND_FAILED.
 * In the last case *residue is set to the residue from the CSW,
 * otherwise to 0.
 *
 * For the functionality of this subroutine see the Mass Storage
 * Spec., the graphs on page 14 and page 19 and beyong (v0.9 of
 * the spec).
 */

usbd_status
umass_bulk_transfer(umass_softc_t *sc, int lun, void *cmd, int cmdlen,
		    void *data, int datalen, int dir, int *residue)
{
	static int dCBWtag = 42;	/* tag to be used in transfers, 
					 * incremented at each transfer */
	usb_bulk_cbw_t cbw;		/* command block wrapper struct */
	usb_bulk_csw_t csw;		/* command status wrapper struct */
	u_int32_t n = 0;		/* number of bytes transported */
	usbd_status err;

#ifdef UMASS_DEBUG
	u_int8_t *c = cmd;

	/* check the given arguments */
	if (!data && datalen > 0) {	/* no buffer for transfer */
		DPRINTF(UDMASS_BULK, ("%s: no buffer, but datalen > 0 !\n",
			USBDEVNAME(sc->sc_dev)));
		return USBD_IOERROR;
	}

	DPRINTF(UDMASS_BULK, ("%s: cmd: %d bytes (0x%02x%02x%02x%02x%02x%02x%s)"
		", data: %d bytes, dir: %s\n",
		USBDEVNAME(sc->sc_dev),
		cmdlen, c[0], c[1], c[2], c[3], c[4], c[5],
		(cmdlen > 6? "...":""),
		datalen, (dir == DIR_IN? "in":"out")));
#endif

	if (dir == DIR_NONE || datalen == 0) {		/* make sure they correspond */
		datalen = 0;
		dir = DIR_NONE;
	}

	*residue = 0;			/* reset residue */

	/*
	 * Determine the direction of transferring data and data length.
	 *
	 * dCBWDataTransferLength (datalen) :
	 *   This field indicates the number of bytes of data that the host
	 *   intends to transfer on the IN or OUT Bulk endpoint(as indicated by
	 *   the Direction bit) during the execution of this command. If this
	 *   field is set to 0, the device will expect that no data will be
	 *   transferred IN or OUT during this command, regardless of the value
	 *   of the Direction bit defined in dCBWFlags.
	 *
	 * dCBWFlags (dir) :
	 *   The bits of the Flags field are defined as follows:
	 *     Bits 0-6  reserved
	 *     Bit  7    Direction - this bit shall be ignored if the
	 *                           dCBWDataTransferLength field is zero.
	 *               0 = data Out from host to device
	 *               1 = data In from device to host
	 */


	/*
	 * Command transport phase
	 */

	/* Fill in the Command Block Wrapper */
	USETDW(cbw.dCBWSignature, CBWSIGNATURE);
	USETDW(cbw.dCBWTag, dCBWtag++);
	USETDW(cbw.dCBWDataTransferLength, datalen);
	/* we do not check for DIR_NONE below (see text on dCBWFlags above) */
	cbw.bCBWFlags = (dir == DIR_IN? CBWFLAGS_IN:CBWFLAGS_OUT);
	cbw.bCBWLUN = lun;
	cbw.bCDBLength = cmdlen;
	bcopy(cmd, cbw.CBWCDB, cmdlen);

	/* Send the CBW from host to device via bulk-out endpoint. */
	err = umass_usb_transfer(sc->sc_iface, sc->sc_bulkout_pipe,
				&cbw, USB_BULK_CBW_SIZE, 0, NULL);
	if (err) {
		DPRINTF(UDMASS_BULK, ("%s: failed to send CBW\n",
		         USBDEVNAME(sc->sc_dev)));
		/* If the device detects that the CBW is invalid, then the
		 * device may STALL both bulk endpoints and require a
		 * Bulk-Only MS Reset
		 */
		umass_bulk_reset(sc);
		return(USBD_IOERROR);
	}


	/*
	 * Data transport phase (only if there is data to be sent/received)
	 */

	if (dir == DIR_IN) {
		/* we allow short transfers for bulk-in pipes */
		err = umass_usb_transfer(sc->sc_iface, sc->sc_bulkin_pipe,
					data, datalen,
					USBD_SHORT_XFER_OK, &n);
		if (err)
			DPRINTF(UDMASS_BULK, ("%s: failed to receive data, "
				"(%d bytes, n = %d), %s\n", 
				USBDEVNAME(sc->sc_dev),
				datalen, n, usbd_errstr(err)));
	} else if (dir == DIR_OUT) {
		err = umass_usb_transfer(sc->sc_iface,
					sc->sc_bulkout_pipe,
					data, datalen, 0, &n);
		if (err)
			DPRINTF(UDMASS_BULK, ("%s: failed to send data, "
				"(%d bytes, n = %d), %s\n", 
				USBDEVNAME(sc->sc_dev),
				datalen, n, usbd_errstr(err)));
	}
	if (err && err != USBD_STALLED)
		return(USBD_IOERROR);


	/*
	 * Status transport phase
	 */

	/* Read the Command Status Wrapper via bulk-in endpoint. */
	err = umass_usb_transfer(sc->sc_iface, sc->sc_bulkin_pipe,
				&csw, USB_BULK_CSW_SIZE, 0, NULL);
	/* Try again if the bulk-in pipe was stalled */
	if (err == USBD_STALLED) {
		err = usbd_clear_endpoint_stall(sc->sc_bulkin_pipe);
		if (!err) {
			err = umass_usb_transfer(sc->sc_iface, sc->sc_bulkin_pipe,
						&csw, USB_BULK_CSW_SIZE, 0, NULL);
		}
	}
	if (err && err != USBD_STALLED)
		return(USBD_IOERROR);

	/*
	 * Check the CSW for status and validity, and check for fatal errors
	 */

	/* Invalid CSW: Wrong signature or wrong tag might indicate
	 * that the device is confused -> reset it.
	 * Other fatal errors: STALL on read of CSW and Phase error
	 * or unknown status.
	 */
	if (err == USBD_STALLED
	    || UGETDW(csw.dCSWSignature) != CSWSIGNATURE
	    || UGETDW(csw.dCSWTag) != UGETDW(cbw.dCBWTag)
	    || csw.bCSWStatus == CSWSTATUS_PHASE
	    || csw.bCSWStatus > CSWSTATUS_PHASE) {
		if (err) {
			printf("%s: failed to read CSW, %s\n",
			       USBDEVNAME(sc->sc_dev), usbd_errstr(err));
		} else if (csw.bCSWStatus == CSWSTATUS_PHASE) {
			printf("%s: Phase Error, residue = %d, n = %d\n",
				USBDEVNAME(sc->sc_dev),
				UGETDW(csw.dCSWDataResidue), n);
		} else if (csw.bCSWStatus > CSWSTATUS_PHASE) {
			printf("%s: Unknown status %d in CSW\n",
				USBDEVNAME(sc->sc_dev), csw.bCSWStatus);
		} else {
			printf("%s: invalid CSW, sig = 0x%08x, tag = %d (!= %d)\n",
				USBDEVNAME(sc->sc_dev),
				UGETDW(csw.dCSWSignature),
				UGETDW(csw.dCSWTag), UGETDW(cbw.dCBWTag));
		}
		umass_bulk_reset(sc);
		return(USBD_IOERROR);
	}

	if (csw.bCSWStatus == CSWSTATUS_FAILED) {
		DPRINTF(UDMASS_BULK, ("%s: Command Failed, "
			"residue = %d, n = %d\n",
			USBDEVNAME(sc->sc_dev),
			UGETDW(csw.dCSWDataResidue), n));
		*residue = UGETDW(csw.dCSWDataResidue);
		return(USBD_COMMAND_FAILED);
	}

	/*
	 * XXX a residue not equal to 0 might indicate that something
	 * is wrong. Does CAM high level drivers check this for us?
	 */

	return(USBD_NORMAL_COMPLETION);
}


/*
 * SCSIPI specific functions
 */

int
umass_scsi_cmd(xs)
	struct scsipi_xfer *xs;
{
	struct scsipi_link *sc_link = xs->sc_link;
	struct umass_softc *sc = sc_link->adapter_softc;
	int residue, dir;
	usbd_status err;

	DPRINTF(UDMASS_SCSI, ("%s: umass_scsi_cmd %d:%d\n",
	    USBDEVNAME(sc->sc_dev),
	    sc_link->scsipi_scsi.target, sc_link->scsipi_scsi.lun));

#ifdef UMASS_DEBUG
	if (sc_link->scsipi_scsi.target != UMASS_SCSIID_DEVICE ||
	    sc_link->scsipi_scsi.lun != 0) {
		DPRINTF(UDMASS_SCSI, ("%s: Wrong SCSI ID %d or LUN %d\n",
		    USBDEVNAME(sc->sc_dev),
		    sc_link->scsipi_scsi.target,
		    sc_link->scsipi_scsi.lun));
		xs->error = XS_DRIVER_STUFFUP;
		return (COMPLETE);
	}
#endif

	dir = DIR_NONE;
	if (xs->datalen) {
		switch (xs->flags & (SCSI_DATA_IN|SCSI_DATA_OUT)) {
		case SCSI_DATA_IN:
			dir = DIR_IN;
			break;
		case SCSI_DATA_OUT:
			dir = DIR_OUT;
			break;
		}
	}

	err = umass_bulk_transfer(sc, sc_link->scsipi_scsi.lun,
	    xs->cmd, xs->cmdlen, xs->data, xs->datalen, dir, &residue);

	/*
	 * FAILED commands are supposed to be SCSI failed commands
	 * and are therefore considered to be successfull CDW/CSW  
	 * transfers.  PHASE errors are more serious and should return
	 * an error to the SCSIPI system.
	 *
	 * XXX This is however more based on empirical evidence than on
	 * hard proof from the Bulk-Only spec.
	 */
	if (err == USBD_NORMAL_COMPLETION)
		xs->error = XS_NOERROR;
	else
		xs->error = XS_DRIVER_STUFFUP;	/* XXX */
	xs->resid = residue;

	DPRINTF(UDMASS_SCSI, ("%s: umass_scsi_cmd: error = %d, resid = 0x%x\n",
	    USBDEVNAME(sc->sc_dev), xs->error, xs->resid));

	xs->flags |= ITSDONE;
	scsipi_done(xs);

	/*
	 * XXXJRT We must return successfully queued if we're an
	 * XXXJRT `asynchronous' command, otherwise `xs' will be
	 * XXXJRT freed twice: once in scsipi_done(), and once in
	 * XXXJRT scsi_scsipi_cmd().
	 */
	if (SCSIPI_XFER_ASYNC(xs))
		return (SUCCESSFULLY_QUEUED);

	return (COMPLETE);
}

void
umass_minphys(bp)
	struct buf *bp;
{

	/* No limit here. */
	minphys(bp);
}
