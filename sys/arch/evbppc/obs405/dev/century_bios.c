/*	$NetBSD: century_bios.c,v 1.1 2005/03/18 15:31:58 shige Exp $	*/

/*
 * Copyright (c) 2004 Shigeyuki Fukushima.
 * All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: century_bios.c,v 1.1 2005/03/18 15:31:58 shige Exp $");

#include <sys/param.h>
#include <sys/systm.h>

#include <machine/cpu.h>
#include <machine/century_bios.h>

/*
 * OpenBlockS S/R Board configuration structure.
 *
 * IBM405GP Monitor (Ver. 1.8, REV-01  H/W)
 * Century Version  (MA-300) (Oct. 30, 2001)
 */
struct board_bios_data {
	unsigned char	mac_address_local[6];
	unsigned char	boot_mode;
	unsigned char	mem_size;		/* MB */
};

static struct board_bios_data	board_bios;
static unsigned int		board_mem_size;
static unsigned int		board_cpu_speed;
static unsigned int		board_plb_speed;
static unsigned int		board_pci_speed;

void
bios_board_init(void *info_block, u_int sysclk_base)
{

        /* Initialize cache info for memcpy, etc. */
        cpu_probe_cache();

	/* Save info block */
	memcpy(&board_bios, info_block, sizeof(board_bios));
	board_mem_size = board_bios.mem_size * 1024 * 1024;

	board_cpu_speed = 0x0bebc200;
	board_plb_speed = 0x05f5e100;
	board_pci_speed = 0x01f78a40;
}

unsigned int
bios_board_memsize_get(void)
{
	return board_mem_size;
}

void
bios_board_info_set(void)
{

	/* Initialize board properties database */
	board_info_init();

	if (board_info_set("mem-size",
		&board_mem_size, 
		sizeof(board_mem_size), PROP_CONST, 0))
		panic("setting mem-size");

	if (board_info_set("emac0-mac-addr",
		&board_bios.mac_address_local, 
		sizeof(board_bios.mac_address_local), PROP_CONST, 0))
		panic("setting emac0-mac-addr");

	if (board_info_set("processor-frequency",
		&board_cpu_speed, 
		sizeof(board_cpu_speed), PROP_CONST, 0))
		panic("setting processor-frequency");

	if (board_info_set("plb-frequency",
		&board_plb_speed, 
		sizeof(board_plb_speed), PROP_CONST, 0))
		panic("setting plb-frequency");

	if (board_info_set("pci-frequency",
		&board_pci_speed, 
		sizeof(board_pci_speed), PROP_CONST, 0))
		panic("setting pci-frequency");
}


void
bios_board_print(void)
{

	printf("Board config data:\n");
	printf("  mem_size = %u\n", board_mem_size);
	printf("  mac_address_local = %02x:%02x:%02x:%02x:%02x:%02x\n",
	    board_bios.mac_address_local[0], board_bios.mac_address_local[1],
	    board_bios.mac_address_local[2], board_bios.mac_address_local[3],
	    board_bios.mac_address_local[4], board_bios.mac_address_local[5]);
	printf("  processor_speed = %u\n", board_cpu_speed);
	printf("  plb_speed = %u\n", board_plb_speed);
	printf("  pci_speed = %u\n", board_pci_speed);
}
