/*	$NetBSD: autoconf.c,v 1.16 2000/02/01 04:04:19 danw Exp $	*/

/*
 * Copyright (C) 1995, 1996 Wolfgang Solfrank.
 * Copyright (C) 1995, 1996 TooLs GmbH.
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by TooLs GmbH.
 * 4. The name of TooLs GmbH may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY TOOLS GMBH ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL TOOLS GMBH BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/conf.h>
#include <sys/device.h>
#include <sys/reboot.h>
#include <sys/systm.h>

#include <machine/autoconf.h>
#include <machine/bus.h>
#include <machine/pio.h>
#include <machine/stdarg.h>

#include <dev/ofw/openfirm.h>
#include <dev/pci/pcivar.h>
#include <dev/scsipi/scsi_all.h>
#include <dev/scsipi/scsipi_all.h>
#include <dev/scsipi/scsiconf.h>
#include <dev/ata/atavar.h>
#include <dev/ic/wdcvar.h>

void findroot __P((void));
int OF_interpret __P((char *cmd, int nreturns, ...));

extern char bootpath[256];
char cbootpath[256];
struct device *booted_device;	/* boot device */
int booted_partition;		/* ...and partition on that device */

#define INT_ENABLE_REG (interrupt_reg + 0x24)
#define INT_CLEAR_REG  (interrupt_reg + 0x28)
u_char *interrupt_reg;

#define HEATHROW_FCR_OFFSET 0x38
u_int *heathrow_FCR = NULL;

/*
 * Determine device configuration for a machine.
 */
void
cpu_configure()
{
	int node, reg[5];
	int msr;
	int phandle;
	char *p;

	node = OF_finddevice("mac-io");
	if (node == -1)
		node = OF_finddevice("/pci/mac-io");
	if (node != -1 &&
	    OF_getprop(node, "assigned-addresses", reg, sizeof(reg)) != -1) {
		interrupt_reg = mapiodev(reg[2], NBPG);
		heathrow_FCR = mapiodev(reg[2] + HEATHROW_FCR_OFFSET, 4);
	} else
		interrupt_reg = mapiodev(0xf3000000, NBPG);

	out32rb(INT_ENABLE_REG, 0);		/* disable all intr. */
	out32rb(INT_CLEAR_REG, 0xffffffff);	/* clear pending intr. */

	calc_delayconst();

	/* Canonicalize bootpath. */
	strcpy(cbootpath, bootpath);
	p = strchr(cbootpath, ':');
	if (p)
		*p = '\0';
	phandle = OF_finddevice(cbootpath);
	if (phandle != -1)
		OF_package_to_path(phandle, cbootpath, sizeof(cbootpath) - 1);

	if (config_rootfound("mainbus", NULL) == NULL)
		panic("configure: mainbus not configured");

	(void)spl0();

	/*
	 * Now allow hardware interrupts.
	 */
	asm volatile ("mfmsr %0; ori %0,%0,%1; mtmsr %0"
		      : "=r"(msr) : "K"((u_short)(PSL_EE|PSL_RI)));
}

#define DEVICE_IS(dev, name) \
	(!strncmp(dev->dv_xname, name, sizeof(name) - 1) && \
	dev->dv_xname[sizeof(name) - 1] >= '0' && \
	dev->dv_xname[sizeof(name) - 1] <= '9')

/*
 * device_register is called from config_attach as each device is
 * attached. We use it to find the NetBSD device corresponding to the
 * known OF boot device.
 */
void
device_register(dev, aux)
	struct device *dev;
	void *aux;
{
	static struct device *parent = NULL;
	static char *bp = bootpath + 1, *cp = cbootpath;
	unsigned long addr;
	char *p;

	if (booted_device || dev->dv_parent != parent)
		return;

	/* Skip over devices not represented in the OF tree. */
	if (DEVICE_IS(dev, "mainbus") || DEVICE_IS(dev, "scsibus") ||
	    DEVICE_IS(dev, "atapibus") || DEVICE_IS(parent, "ppb")) {
		parent = dev;
		return;
	}

	/* Get the address part of the current path component. The
	 * last component of the canonical bootpath may have no
	 * address (eg, "disk"), in which case we need to get the
	 * address from the original bootpath instead.
	 */
	p = strchr(cp, '@');
	if (!p) {
		if (bp)
			p = strchr(bp, '@');
		if (!p)
			addr = 0;
		else {
			addr = strtoul(p + 1, NULL, 16);
			p = NULL;
		}
	} else
		addr = strtoul(p + 1, &p, 16);

	if (DEVICE_IS(parent, "mainbus") && DEVICE_IS(dev, "pci")) {
		struct pcibus_attach_args *pba = aux;
		int n;

		for (n = 0; n < 2; n++) {
			if (pci_bridges[n].present &&
			    pci_bridges[n].bus == pba->pba_bus)
				break;
		}
		if (n == 2 || addr != pci_bridges[n].reg[0])
			return;
	} else if (DEVICE_IS(parent, "pci")) {
		struct pci_attach_args *pa = aux;

		if (addr != pa->pa_device)
			return;
	} else if (DEVICE_IS(parent, "obio")) {
		struct confargs *ca = aux;

		if (addr != ca->ca_reg[0])
			return;
	} else if (DEVICE_IS(parent, "scsibus") ||
		   DEVICE_IS(parent, "atapibus")) {
		struct scsipibus_attach_args *sa = aux;

		if (parent->dv_xname[0] == 's') {
			if (addr != sa->sa_sc_link->scsipi_scsi.target)
				return;
		} else {
			if (addr != sa->sa_sc_link->scsipi_atapi.drive)
				return;
		}
	} else if (DEVICE_IS(parent, "pciide")) {
		struct ata_atapi_attach *aa = aux;

		if (addr != aa->aa_channel)
			return;

		/*
		 * OF splits channel and drive into separate path
		 * components, so check the addr part of the next
		 * component. (Ignore bp, because the canonical path
		 * will be complete in the pciide case.)
		 */
		p = strchr(p, '@');
		if (!p++)
			return;
		if (strtoul(p, &p, 16) != aa->aa_drv_data->drive)
			return;
	} else
		return;

	/* If we reach this point, then dev is a match for the current
	 * path component.
	 */

	if (p && *p) {
		parent = dev;
		cp = p;
		bp = strchr(bp, '/');
		if (bp)
			bp++;
		return;
	} else {
		booted_device = dev;
		return;
	}
}

/*
 * Setup root device.
 * Configure swap area.
 */
void
cpu_rootconf()
{
	printf("boot device: %s\n",
	    booted_device ? booted_device->dv_xname : "<unknown>");

	setroot(booted_device, booted_partition);
}

int
#ifdef __STDC__
OF_interpret(char *cmd, int nreturns, ...)
#else
OF_interpret(cmd, nreturns, va_alist)
	char *cmd;
	int nreturns;
	va_dcl
#endif
{
	va_list ap;
	int i;
	static struct {
		char *name;
		int nargs;
		int nreturns;
		char *cmd;
		int status;
		int results[8];
	} args = {
		"interpret",
		1,
		2,
	};

	ofw_stack();
	if (nreturns > 8)
		return -1;
	if ((i = strlen(cmd)) >= NBPG)
		return -1;
	ofbcopy(cmd, OF_buf, i + 1);
	args.cmd = OF_buf;
	args.nargs = 1;
	args.nreturns = nreturns + 1;
	if (openfirmware(&args) == -1)
		return -1;
	va_start(ap, nreturns);
	for (i = 0; i < nreturns; i++)
		*va_arg(ap, int *) = args.results[i];
	va_end(ap);
	return args.status;
}

/*
 * Find OF-device corresponding to the PCI device.
 */
int
pcidev_to_ofdev(pc, tag)
	pci_chipset_tag_t pc;
	pcitag_t tag;
{
	int bus, dev, func;
	u_int reg[5];
	int p, q;
	int l, b, d, f;

	pci_decompose_tag(pc, tag, &bus, &dev, &func);

	for (q = OF_peer(0); q; q = p) {
		l = OF_getprop(q, "assigned-addresses", reg, sizeof(reg));
		if (l > 4) {
			b = (reg[0] >> 16) & 0xff;
			d = (reg[0] >> 11) & 0x1f;
			f = (reg[0] >> 8) & 0x07;

			if (b == bus && d == dev && f == func)
				return q;
		}
		if ((p = OF_child(q)))
			continue;
		while (q) {
			if ((p = OF_peer(q)))
				break;
			q = OF_parent(q);
		}
	}
	return 0;
}
