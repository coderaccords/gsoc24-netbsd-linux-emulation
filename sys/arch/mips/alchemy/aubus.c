/* $NetBSD: aubus.c,v 1.1 2002/07/29 15:39:12 simonb Exp $ */

/*
 * Copyright 2001 Wasabi Systems, Inc.
 * All rights reserved.
 *
 * Written by Simon Burge for Wasabi Systems, Inc.
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
 *      This product includes software developed for the NetBSD Project by
 *      Wasabi Systems, Inc.
 * 4. The name of Wasabi Systems, Inc. may not be used to endorse
 *    or promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY WASABI SYSTEMS, INC. ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL WASABI SYSTEMS, INC
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
 *      This product includes software developed by Christopher G. Demetriou
 *	for the NetBSD Project.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
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

#include "locators.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/extent.h>
#include <sys/malloc.h>

#include <machine/bus.h>
#include <machine/locore.h>
#include <mips/alchemy/include/aureg.h>
#include <mips/alchemy/include/aubusvar.h>

struct au1x00_dev {
	const char *name;
	bus_addr_t addr[3];
	int irq[2];
};

/*
 * The devices built in to the Au1000 cpu.
 */
const struct au1x00_dev au1000_devs [] = {
	{ "aucom",	{ UART0_BASE },				   {  0, -1 }},
	{ "aucom",	{ UART1_BASE },				   {  1, -1 }},
	{ "aucom",	{ UART2_BASE },				   {  2, -1 }},
	{ "aucom",	{ UART3_BASE },				   {  3, -1 }},
	{ "aurtc",	{ },					   { -1, -1 }},
	{ "aumac",	{ MAC0_BASE, MAC0_ENABLE, MAC0_DMA_BASE }, { 28, -1 }},
	{ "aumac",	{ MAC1_BASE, MAC1_ENABLE, MAC1_DMA_BASE }, { 29, -1 }},
	{ "auaudio",	{ AC97_BASE },				   { 27, 31 }},
	{ "ohci",	{ USBH_BASE },				   { 26, -1 }},
	{ "usbd",	{ USBD_BASE },				   { 24, 25 }},
	{ "irda",	{ IRDA_BASE },				   { 22, 23 }},
	{ "gpio",	{ SYS_BASE },				   { -1, -1 }},
	{ "i2s",	{ I2S_BASE },				   { -1, -1 }},
	{ "ssi",	{ SSI0_BASE },				   {  4, -1 }},
	{ "ssi",	{ SSI1_BASE },				   {  5, -1 }},
	{ NULL }
};

/*
 * The devices built in to the Au1500 cpu.
 */
const struct au1x00_dev au1500_devs [] = {
	{ "aucom",	{ UART0_BASE },				   {  0, -1 }},
	{ "aucom",	{ UART3_BASE },				   {  3, -1 }},
	{ "aurtc",	{ },					   { -1, -1 }},
	{ "aumac",	{ AU1500_MAC0_BASE, AU1500_MAC0_ENABLE,
			      MAC0_DMA_BASE },			   { 28, -1 }},
	{ "aumac",	{ AU1500_MAC1_BASE, AU1500_MAC1_ENABLE,
			      MAC1_DMA_BASE },			   { 29, -1 }},
	{ "auaudio",	{ AC97_BASE },				   { 27, 31 }},
	{ "ohci",	{ USBH_BASE },				   { 26, -1 }},
	{ "usbd",	{ USBD_BASE },				   { 24, 25 }},
	{ "gpio",	{ SYS_BASE },				   { -1, -1 }},
	{ "gpio2",	{ GPIO2_BASE },				   { -1, -1 }},
	{ "i2s",	{ I2S_BASE },				   { -1, -1 }},
	{ "aupci",	{ },					   { -1, -1 }},
	{ NULL }
};


/*
 * The devices built in to the Au1100 cpu.
 */
const struct au1x00_dev au1100_devs [] = {
	{ "aucom",	{ UART0_BASE },				   {  0, -1 }},
	{ "aucom",	{ UART1_BASE },				   {  1, -1 }},
	{ "aucom",	{ UART3_BASE },				   {  3, -1 }},
	{ "aurtc",	{ },					   { -1, -1 }},
	{ "aumac",	{ MAC0_BASE, MAC0_ENABLE, MAC0_DMA_BASE }, { 28, -1 }},
	{ "auaudio",	{ AC97_BASE },				   { 27, 31 }},
	{ "ohci",	{ USBH_BASE },				   { 26, -1 }},
	{ "usbd",	{ USBD_BASE },				   { 24, 25 }},
	{ "irda",	{ IRDA_BASE },				   { 22, 23 }},
	{ "gpio",	{ SYS_BASE },				   { -1, -1 }},
	{ "gpio2",	{ GPIO2_BASE },				   { 29, -1 }},
	{ "i2s",	{ I2S_BASE },				   { 30, -1 }},
	{ "ssi",	{ SSI0_BASE },				   {  4, -1 }},
	{ "ssi",	{ SSI1_BASE },				   {  5, -1 }},
	{ "sd0",	{ SD0_BASE },				   {  5, -1 }},
	{ "sd1",	{ SD1_BASE },				   {  5, -1 }},
	{ NULL }
};


static int	aubus_match(struct device *, struct cfdata *, void *);
static void	aubus_attach(struct device *, struct device *, void *);
static int	aubus_submatch(struct device *, struct cfdata *, void *);
static int	aubus_print(void *, const char *);

struct cfattach aubus_ca = {
	sizeof(struct device), aubus_match, aubus_attach
};

bus_space_tag_t	aubus_st;		/* XXX */

/*
 * Probe for the aubus; always succeeds.
 */
static int
aubus_match(struct device *parent, struct cfdata *match, void *aux)
{

	return 1;
}

static int
aubus_submatch(struct device *parent, struct cfdata *cf, void *aux)
{
	struct aubus_attach_args *aa = aux;

	if (cf->cf_loc[AUBUSCF_ADDR] != AUBUSCF_ADDR_DEFAULT &&
	    cf->cf_loc[AUBUSCF_ADDR] != aa->aa_addr)
		return (0);

	return ((*cf->cf_attach->ca_match)(parent, cf, aux));
}

/*
 * Attach the aubus.
 */
static void
aubus_attach(struct device *parent, struct device *self, void *aux)
{
	struct aubus_attach_args aa;
	const struct au1x00_dev *ad;

	printf("\n");

	switch (MIPS_PRID_COPTS(cpu_id)) {
	case MIPS_AU1000:
		ad = au1000_devs;
		break;
	case MIPS_AU1500:
		ad = au1100_devs;
		break;
	case MIPS_AU1100:
		ad = au1500_devs;
		break;
	default:
		panic("Unknown Alchemy SOC identification %d",
		    MIPS_PRID_COPTS(cpu_id));
	}

	for (; ad->name != NULL; ad++) {
		aa.aa_name = ad->name;
		aa.aa_st = aubus_st;
		aa.aa_addrs[0] = ad->addr[0];
		aa.aa_addrs[1] = ad->addr[1];
		aa.aa_addrs[2] = ad->addr[2];
		aa.aa_irq[0] = ad->irq[0];
		aa.aa_irq[1] = ad->irq[1];

		(void) config_found_sm(self, &aa, aubus_print, aubus_submatch);
	}
}

static int
aubus_print(void *aux, const char *pnp)
{
	struct aubus_attach_args *aa = aux;

	if (pnp)
		printf("%s at %s", aa->aa_name, pnp);

	if (aa->aa_addr != AUBUSCF_ADDR_DEFAULT)
		printf(" %s 0x%lx", aubuscf_locnames[AUBUSCF_ADDR],
		    aa->aa_addr);
	if (aa->aa_irq[0] >= 0)
		printf(" irq %d", aa->aa_irq[0]);
	if (aa->aa_irq[1] >= 0)
		printf(",%d", aa->aa_irq[1]);
	return (UNCONF);
}
