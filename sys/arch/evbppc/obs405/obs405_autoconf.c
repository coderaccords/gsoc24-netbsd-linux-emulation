/*	$NetBSD: obs405_autoconf.c,v 1.2 2005/12/11 12:17:12 christos Exp $	*/

/*
 * Copyright 2004 Shigeyuki Fukushima.
 * All rights reserved.
 *
 * Written by Shigeyuki Fukushima for The NetBSD Project.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials provided
 *    with the distribution.
 * 3. The name of the author may not be used to endorse or promote
 *    products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
 * USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */
#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: obs405_autoconf.c,v 1.2 2005/12/11 12:17:12 christos Exp $");

#include <sys/param.h>
#include <sys/conf.h>
#include <sys/systm.h>
#include <sys/device.h>

#include <machine/obs405.h>

#include <powerpc/ibm4xx/dev/comopbvar.h>


void
cpu_rootconf(void)
{

	setroot(booted_device, booted_partition);
}

void obs405_device_register(struct device *dev, void *aux, int com_freq)
{
	struct device *parent = dev->dv_parent;

	/* register "com" device */
	if (strcmp(dev->dv_cfdata->cf_name, "com") == 0 &&
	    strcmp(parent->dv_cfdata->cf_name, "opb") == 0) {
		/* Set the frequency of the on-chip UART. */
		com_opb_device_register(dev, com_freq);
		return;
	}

	ibm4xx_device_register(dev, aux);
}
