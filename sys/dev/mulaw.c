/*	$NetBSD: mulaw.c,v 1.6 1997/06/14 22:25:11 thorpej Exp $	*/

/*
 * Copyright (c) 1991-1993 Regents of the University of California.
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
 *	This product includes software developed by the Computer Systems
 *	Engineering Group at Lawrence Berkeley Laboratory.
 * 4. Neither the name of the University nor of the Laboratory may be used
 *    to endorse or promote products derived from this software without
 *    specific prior written permission.
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
 */

#include <sys/types.h>
#include <sys/audioio.h>
#include <machine/endian.h>
#include <dev/mulaw.h>

static u_char mulawtolin[256] = {
	128, 4, 8, 12, 16, 20, 24, 28, 
	32, 36, 40, 44, 48, 52, 56, 60, 
	64, 66, 68, 70, 72, 74, 76, 78, 
	80, 82, 84, 86, 88, 90, 92, 94, 
	96, 97, 98, 99, 100, 101, 102, 103, 
	104, 105, 106, 107, 108, 109, 110, 111, 
	112, 112, 113, 113, 114, 114, 115, 115, 
	116, 116, 117, 117, 118, 118, 119, 119, 
	120, 120, 120, 121, 121, 121, 121, 122, 
	122, 122, 122, 123, 123, 123, 123, 124, 
	124, 124, 124, 124, 125, 125, 125, 125, 
	125, 125, 125, 125, 126, 126, 126, 126, 
	126, 126, 126, 126, 126, 126, 126, 126, 
	127, 127, 127, 127, 127, 127, 127, 127, 
	127, 127, 127, 127, 127, 127, 127, 127, 
	127, 127, 127, 127, 127, 127, 127, 127, 
	255, 251, 247, 243, 239, 235, 231, 227, 
	223, 219, 215, 211, 207, 203, 199, 195, 
	191, 189, 187, 185, 183, 181, 179, 177, 
	175, 173, 171, 169, 167, 165, 163, 161, 
	159, 158, 157, 156, 155, 154, 153, 152, 
	151, 150, 149, 148, 147, 146, 145, 144, 
	143, 143, 142, 142, 141, 141, 140, 140, 
	139, 139, 138, 138, 137, 137, 136, 136, 
	135, 135, 135, 134, 134, 134, 134, 133, 
	133, 133, 133, 132, 132, 132, 132, 131, 
	131, 131, 131, 131, 130, 130, 130, 130, 
	130, 130, 130, 130, 129, 129, 129, 129, 
	129, 129, 129, 129, 129, 129, 129, 129, 
	128, 128, 128, 128, 128, 128, 128, 128, 
	128, 128, 128, 128, 128, 128, 128, 128, 
	128, 128, 128, 128, 128, 128, 128, 128, 
};

static u_char lintomulaw[256] = {
	0, 0, 0, 0, 0, 1, 1, 1, 
	1, 2, 2, 2, 2, 3, 3, 3, 
	3, 4, 4, 4, 4, 5, 5, 5, 
	5, 6, 6, 6, 6, 7, 7, 7, 
	7, 8, 8, 8, 8, 9, 9, 9, 
	9, 10, 10, 10, 10, 11, 11, 11, 
	11, 12, 12, 12, 12, 13, 13, 13, 
	13, 14, 14, 14, 14, 15, 15, 15, 
	15, 16, 16, 17, 17, 18, 18, 19, 
	19, 20, 20, 21, 21, 22, 22, 23, 
	23, 24, 24, 25, 25, 26, 26, 27, 
	27, 28, 28, 29, 29, 30, 30, 31, 
	31, 32, 33, 34, 35, 36, 37, 38, 
	39, 40, 41, 42, 43, 44, 45, 46, 
	47, 48, 50, 52, 54, 56, 58, 60, 
	62, 65, 69, 73, 77, 83, 91, 103, 
	255, 231, 219, 211, 205, 201, 197, 193, 
	190, 188, 186, 184, 182, 180, 178, 176, 
	175, 174, 173, 172, 171, 170, 169, 168, 
	167, 166, 165, 164, 163, 162, 161, 160, 
	159, 159, 158, 158, 157, 157, 156, 156, 
	155, 155, 154, 154, 153, 153, 152, 152, 
	151, 151, 150, 150, 149, 149, 148, 148, 
	147, 147, 146, 146, 145, 145, 144, 144, 
	143, 143, 143, 143, 142, 142, 142, 142, 
	141, 141, 141, 141, 140, 140, 140, 140, 
	139, 139, 139, 139, 138, 138, 138, 138, 
	137, 137, 137, 137, 136, 136, 136, 136, 
	135, 135, 135, 135, 134, 134, 134, 134, 
	133, 133, 133, 133, 132, 132, 132, 132, 
	131, 131, 131, 131, 130, 130, 130, 130, 
	129, 129, 129, 129, 128, 128, 128, 128, 
};

static u_char alawtolin[256] = {
	107, 108, 105, 106, 111, 112, 109, 110, 
	99, 100, 97, 98, 103, 104, 101, 102,
	118, 118, 117, 117, 120, 120, 119, 119, 
	114, 114, 113, 113, 116, 116, 115, 115, 
	42, 46, 34, 38, 58, 62, 50, 54, 
	10, 14, 2, 6, 26, 30, 18, 22, 
	85, 87, 81, 83, 93, 95, 89, 91, 
	69, 71, 65, 67, 77, 79, 73, 75, 
	127, 127, 127, 127, 127, 127, 127, 127, 
	127, 127, 127, 127, 127, 127, 127, 127, 
	128, 128, 128, 128, 128, 128, 128, 128, 
	128, 128, 128, 128, 128, 128, 128, 128, 
	123, 123, 123, 123, 124, 124, 124, 124, 
	121, 121, 121, 121, 122, 122, 122, 122, 
	126, 126, 126, 126, 126, 126, 126, 126, 
	125, 125, 125, 125, 125, 125, 125, 125, 
	149, 148, 151, 150, 145, 144, 147, 146, 
	157, 156, 159, 158, 153, 152, 155, 154, 
	138, 138, 139, 139, 136, 136, 137, 137, 
	142, 142, 143, 143, 140, 140, 141, 141, 
	214, 210, 222, 218, 198, 194, 206, 202, 
	246, 242, 254, 250, 230, 226, 238, 234, 
	171, 169, 175, 173, 163, 161, 167, 165, 
	187, 185, 191, 189, 179, 177, 183, 181, 
	129, 129, 129, 129, 129, 129, 129, 129, 
	129, 129, 129, 129, 129, 129, 129, 129, 
	128, 128, 128, 128, 128, 128, 128, 128, 
	128, 128, 128, 128, 128, 128, 128, 128, 
	133, 133, 133, 133, 132, 132, 132, 132, 
	135, 135, 135, 135, 134, 134, 134, 134, 
	130, 130, 130, 130, 130, 130, 130, 130, 
	131, 131, 131, 131, 131, 131, 131, 131,
};

static u_char lintoalaw[256] = {
	42, 42, 42, 42, 43, 43, 43, 43, 
	40, 40, 40, 40, 41, 41, 41, 41, 
	46, 46, 46, 46, 47, 47, 47, 47, 
	44, 44, 44, 44, 45, 45, 45, 45, 
	34, 34, 34, 34, 35, 35, 35, 35, 
	32, 32, 32, 32, 33, 33, 33, 33, 
	38, 38, 38, 38, 39, 39, 39, 39, 
	36, 36, 36, 36, 37, 37, 37, 37, 
	58, 58, 59, 59, 56, 56, 57, 57, 
	62, 62, 63, 63, 60, 60, 61, 61, 
	50, 50, 51, 51, 48, 48, 49, 49, 
	54, 54, 55, 55, 52, 52, 53, 53, 
	10, 11, 8, 9, 14, 15, 12, 13, 
	2, 3, 0, 1, 6, 7, 4, 5, 
	26, 24, 30, 28, 18, 16, 22, 20, 
	106, 110, 98, 102, 122, 114, 74, 90, 
	213, 197, 245, 253, 229, 225, 237, 233, 
	149, 151, 145, 147, 157, 159, 153, 155, 
	133, 132, 135, 134, 129, 128, 131, 130, 
	141, 140, 143, 142, 137, 136, 139, 138, 
	181, 181, 180, 180, 183, 183, 182, 182, 
	177, 177, 176, 176, 179, 179, 178, 178, 
	189, 189, 188, 188, 191, 191, 190, 190, 
	185, 185, 184, 184, 187, 187, 186, 186, 
	165, 165, 165, 165, 164, 164, 164, 164, 
	167, 167, 167, 167, 166, 166, 166, 166, 
	161, 161, 161, 161, 160, 160, 160, 160, 
	163, 163, 163, 163, 162, 162, 162, 162, 
	173, 173, 173, 173, 172, 172, 172, 172, 
	175, 175, 175, 175, 174, 174, 174, 174, 
	169, 169, 169, 169, 168, 168, 168, 168, 
	171, 171, 171, 171, 170, 170, 170, 170,
};

void
mulaw_to_ulinear8(v, p, cc)
	void *v;
	u_char *p;
	int cc;
{
	while (--cc >= 0) {
		*p = mulawtolin[*p];
		++p;
	}
}

void
ulinear8_to_mulaw(v, p, cc)
	void *v;
	u_char *p;
	int cc;
{
	while (--cc >= 0) {
		*p = lintomulaw[*p];
		++p;
	}
}

void
alaw_to_ulinear8(v, p, cc)
	void *v;
	u_char *p;
	int cc;
{
	while (--cc >= 0) {
		*p = alawtolin[*p];
		++p;
	}
}

void
ulinear8_to_alaw(v, p, cc)
	void *v;
	u_char *p;
	int cc;
{
	while (--cc >= 0) {
		*p = lintoalaw[*p];
		++p;
	}
}

void
change_sign8(v, p, cc)
	void *v;
	u_char *p;
	int cc;
{
	while (--cc >= 0) {
		*p = *p ^ 0x80;
		++p;
	}
}

void
change_sign16(v, p, cc)
	void *v;
	u_char *p;
	int cc;
{
#if BYTE_ORDER == LITTLE_ENDIAN
	while ((cc -=2) >= 0) {
		p[1] = p[1] ^ 0x80;
		p += 2;
	}
#else
	while ((cc -=2) >= 0) {
		*p = *p ^ 0x80;
		p += 2;
	}
#endif
}

void
swap_bytes(v, p, cc)
	void *v;
	u_char *p;
	int cc;
{
	u_char t;

	while ((cc -=2) >= 0) {
		t = p[0];
		p[0] = p[1];
		p[1] = t;
		p += 2;
	}
}

void
swap_bytes_change_sign16(v, p, cc)
	void *v;
	u_char *p;
	int cc;
{
	swap_bytes(v, p, cc);
	change_sign16(v, p, cc);
}

void
change_sign16_swap_bytes(v, p, cc)
	void *v;
	u_char *p;
	int cc;
{
	change_sign16(v, p, cc);
	swap_bytes(v, p, cc);
}
