/*	$NetBSD: ns16550.c,v 1.4 1999/06/24 01:10:32 sakamoto Exp $	*/

/*-
 * Copyright (C) 1995-1997 Gary Thomas (gdt@linuxppc.org)
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
 *      This product includes software developed by Gary Thomas.
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

#ifdef CONS_SERIAL
/*
 * NS16550 support
 */

#include "ns16550.h"

volatile struct NS16550 *
NS16550_init(addr, speed)
	int addr;
	int speed;
{
	volatile struct NS16550 *com_port;

	com_port = (struct NS16550 *)(COMBASE + addr);

	com_port->lcr = 0x80;  /* Access baud rate */
	speed = comspeed(speed);
	com_port->dll = speed;
	com_port->dlm = speed >> 8;

	com_port->lcr = 0x03;  /* 8 data, 1 stop, no parity */
	com_port->mcr = 0x00;
	com_port->fcr = 0x07;  /* Clear & enable FIFOs */
	com_port->ier = 0x00;
	return (com_port);
}


NS16550_putc(volatile struct NS16550 *com_port, unsigned char c)
{
	volatile int i;
	while ((com_port->lsr & LSR_THRE) == 0) ;
	com_port->thr = c;
}

unsigned char NS16550_getc(volatile struct NS16550 *com_port)
{
	while ((com_port->lsr & LSR_DR) == 0) ;
	return (com_port->rbr);
}

int NS16550_scankbd(volatile struct NS16550 *com_port)
{
	if ((com_port->lsr & LSR_DR) == 0)
		return -1;
	return (com_port->rbr);
}

NS16550_test(volatile struct NS16550 *com_port)
{
	return ((com_port->lsr & LSR_DR) != 0);
}
#endif /* CONS_SERIAL */
