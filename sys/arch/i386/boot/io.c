/*	$NetBSD: io.c,v 1.10 1995/01/09 22:13:10 ws Exp $	*/

/*
 * Ported to boot 386BSD by Julian Elischer (julian@tfs.com) Sept 1992
 *
 * Mach Operating System
 * Copyright (c) 1992, 1991 Carnegie Mellon University
 * All Rights Reserved.
 * 
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND FOR
 * ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 * 
 * Carnegie Mellon requests users of this software to return to
 * 
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 * 
 * any improvements or extensions that they make and grant Carnegie Mellon
 * the rights to redistribute these changes.
 */

#include <sys/types.h>
#include <machine/pio.h>

#define K_RDWR 		0x60		/* keyboard data & cmds (read/write) */
#define K_STATUS 	0x64		/* keyboard status */
#define K_CMD	 	0x64		/* keybd ctlr command (write-only) */

#define K_OBUF_FUL 	0x01		/* output buffer full */
#define K_IBUF_FUL 	0x02		/* input buffer full */

#define KC_CMD_WIN	0xd0		/* read  output port */
#define KC_CMD_WOUT	0xd1		/* write output port */
#define KB_A20		0xdf		/* enable A20,
					   enable output buffer full interrupt
					   enable data line
					   enable clock line */

/*
 * Gate A20 for high memory
 */
unsigned char	x_20 = KB_A20;
gateA20()
{
#ifdef	IBM_L40
	outb(0x92, 0x2);
#else	IBM_L40
	while (inb(K_STATUS) & K_IBUF_FUL);
	while (inb(K_STATUS) & K_OBUF_FUL)
		(void)inb(K_RDWR);

	outb(K_CMD, KC_CMD_WOUT);
	while (inb(K_STATUS) & K_IBUF_FUL);
	outb(K_RDWR, x_20);
	while (inb(K_STATUS) & K_IBUF_FUL);
#endif	IBM_L40
}

/* printf - only handles %d as decimal, %c as char, %s as string */

printf(format,data)
	char *format;
	int data;
{
	int *dataptr = &data;
	char c;

	reset_twiddle();
	while (c = *format++) {
		if (c != '%') {
			putchar(c);
			continue;
		}
		c = *format++;
		if (c == 'd') {
			int num = *dataptr++;
			char buf[10], *ptr = buf;
			if (num < 0) {
				num = -num;
				putchar('-');
			}
			do
				*ptr++ = '0' + num % 10;
			while (num /= 10);
			do
				putchar(*--ptr);
			while (ptr != buf);
		} else if (c == 'x') {
			unsigned int num = (unsigned int)*dataptr++, dig;
			char buf[8], *ptr = buf;
			do
				*ptr++ = (dig = (num & 0xf)) > 9?
					'a' + dig - 10 :
					'0' + dig;
			while (num >>= 4);
			do
				putchar(*--ptr);
			while (ptr != buf);
		} else if (c == 'c') {
			putchar((*dataptr++) & 0xff);
		} else if (c == 's') {
			char *ptr = (char *)*dataptr++;
			while (c = *ptr++)
				putchar(c);
		}
	}
}

putchar(c)
{
	if (c == '\n')
		putc('\r');
	putc(c);
}

getchar()
{
	int c;

	if ((c = getc()) == '\r')
		c = '\n';
	if (c == '\b') {
		putchar('\b');
		putchar(' ');
	}
	putchar(c);
	return(c);
}

gets(buf)
	char *buf;
{
	int	i;
	char *ptr = buf;

	for (i = 240000; i > 0; i--)
		if (ischar())
			for (;;) {
				*ptr = getchar() & 0xff;
				if (*ptr == '\n' || *ptr == '\r') {
					*ptr = '\0';
					return 1;
				} else if (*ptr == '\b') {
					if (ptr > buf)
						ptr--;
				} else
					ptr++;
			}
	return 0;
}

strcmp(s1, s2)
	char *s1, *s2;
{
	while (*s1 == *s2) {
		if (!*s1++)
			return 0;
		s2++;
	}
	return 1;
}

strlen(s)
	char *s;
{
	int n;

	for (n = 0; *s++; n++);
	return n;
}

bcopy(from, to, len)
	char *from, *to;
	int len;
{
	if (from > to)
		while (--len >= 0)
			*to++ = *from++;
	else {
		to += len;
		from += len;
		while (--len >= 0)
			*--to = *--from;
	}
}

static int tw_on;
static int tw_pos;
static char tw_chars[] = "|/-\\";

reset_twiddle()
{
	if (tw_on)
		putchar('\b');
	tw_on = 0;
	tw_pos = 0;
}

twiddle()
{
	if (tw_on)
		putchar('\b');
	else
		tw_on = 1;
	putchar(tw_chars[tw_pos++]);
	tw_pos %= (sizeof(tw_chars) - 1);
}
