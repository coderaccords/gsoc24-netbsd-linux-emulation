/* netboot
 *
 * source in this file came from
 * the original 386BSD boot block.
 *
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * William Jolitz.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)cga.c	5.3 (Berkeley) 4/28/91
 *	$Id: cga.c,v 1.2 1994/02/15 15:10:20 mycroft Exp $
 */

#include "proto.h"

#define	COL		80
#define	ROW		25
#define	CHR		2
#define MONO_BASE	0x3B4
#define MONO_BUF	0xB0000
#define CGA_BASE	0x3D4
#define CGA_BUF		0xB8000

static u_char	att = 0x7 ;
u_char *Crtat = (u_char *)CGA_BUF;

static unsigned int addr_6845 = CGA_BASE;

static void cursor(int pos) {
  outb(addr_6845, 14);
  outb(addr_6845+1, pos >> 8);
  outb(addr_6845, 15);
  outb(addr_6845+1, pos&0xff);
}

void
putc(int c) {
#ifdef USE_BIOS
  asm("
	movb	%0, %%cl
	call	_prot_to_real
	.byte	0x66
	mov	$0x1, %%ebx
	movb	$0xe, %%ah
	movb	%%cl, %%al
	sti
	int	$0x10
	cli
	.byte	0x66
	call	_real_to_prot
	" : : "g" (ch));
#else
  static u_char *crtat = 0;
  unsigned cursorat; u_short was;
  u_char *cp;

  if (crtat == 0) {

    /* XXX probe to find if a color or monochrome display */
    was = *(u_short *)Crtat;
    *(u_short *)Crtat = 0xA55A;
    if (*(u_short *)Crtat != 0xA55A) {
      Crtat = (u_char *) MONO_BUF;
      addr_6845 = MONO_BASE;
    }
    *(u_short *)Crtat = was;

    /* Extract cursor location */
    outb(addr_6845,14);
    cursorat = inb(addr_6845+1)<<8 ;
    outb(addr_6845,15);
    cursorat |= inb(addr_6845+1);

    if(cursorat <= COL*ROW) {
      crtat = Crtat + cursorat*CHR;
      /* att = crtat[1]; */	/* use current attribute present */
    } else	crtat = Crtat;

    /* clean display */
    for (cp = crtat; cp < Crtat+ROW*COL*CHR; cp += 2) {
      cp[0] = ' ';
      cp[1] = att;
    }
  }

  switch (c) {

  case '\t':
    do
      putc(' ');
    while ((int)crtat % (8*CHR));
    break;

  case '\010':
    crtat -= CHR;
    break;

  case '\r':
    crtat -= (crtat - Crtat) % (COL*CHR);
    break;

  case '\n':
    crtat += COL*CHR ;
    break;

  default:
    crtat[0] = c;
    crtat[1] = att;
    crtat += CHR;
    break ;
  }

  /* implement a scroll */
  if (crtat >= Crtat+COL*ROW*CHR) {
    /* move text up */
    bcopy((char *)(Crtat+COL*CHR), (char *)Crtat, COL*(ROW-1)*CHR);

    /* clear line */
    for (cp = Crtat+ COL*(ROW-1)*CHR;
	 cp < Crtat + COL*ROW*CHR ; cp += 2)
      cp[0] = ' ';

    crtat -= COL*CHR ;
  }
  cursor((crtat-Crtat)/CHR);
#endif
}

void
putchar(int c)
{
  if (c == '\n')
    putc('\r');
  putc(c);
}

/* printf - only handles %d as decimal, %c as char, %s as string */

void
printf(format,data)
     const char *format;
     int data;
{
  int *dataptr = &data;
  char c;
  while ((c = *format++)) {
    if (c != '%')
      putchar(c);
    else {
      switch (c = *format++) {
      case 'd': {
	int num = *dataptr++;
	char buf[10], *ptr = buf;
	if (num<0) {
	  num = -num;
	  putchar('-');
	}
	do
	  *ptr++ = '0'+num%10;
	while ((num /= 10));
	do
	  putchar(*--ptr);
	while (ptr != buf);
	break;
      }
      case 'x': {
	int num = *dataptr++, dig;
	char buf[8], *ptr = buf;
	do
	  *ptr++ = (dig=(num&0xf)) > 9?
	    'a' + dig - 10 :
	      '0' + dig;
	while ((num >>= 4));
	do
	  putchar(*--ptr);
	while (ptr != buf);
	break;
      }
      case 'c': putchar((*dataptr++)&0xff); break;
      case 's': {
	char *ptr = (char *)*dataptr++;
	while ((c = *ptr++))
	  putchar(c);
	break;
      }
      }
    }
  }
}
