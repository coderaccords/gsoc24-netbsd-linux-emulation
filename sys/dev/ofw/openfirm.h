/*	$NetBSD: openfirm.h,v 1.17 2001/06/19 08:24:07 simonb Exp $	*/

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
/*
 * Prototypes for OpenFirmware Interface Routines
 */

#include <sys/param.h>				/* XXX */
#include <sys/device.h>				/* XXX */

/*
 * Machine-independent OpenFirmware-related structures.
 * XXX THESE DO NOT BELONG HERE.
 */

/*
 * Generic OpenFirmware probe argument.
 * This is how all probe structures must start
 * in order to support generic OpenFirmware device drivers.
 */
struct ofbus_attach_args {
	char *oba_busname;
	int oba_phandle;
	/*
	 * Special unit field for disk devices.
	 * This is a KLUDGE to work around the fact that OpenFirmware
	 * doesn't probe the scsi bus completely.
	 * YES, I THINK THIS IS A BUG IN THE OPENFIRMWARE DEFINITION!!!	XXX
	 * See also ofdisk.c.
	 */
	int oba_unit;
};

/*
 * The softc structure for devices we might be booted from (i.e. we might
 * want to set root/swap to) needs to start with these fields:		XXX
 */
struct ofbus_softc {
	struct device sc_dev;
	int sc_phandle;
	int sc_unit;		/* Might be missing for non-disk devices */
};


/*
 * Functions and variables provided by machine-dependent code.
 */
extern char *OF_buf;

int	OF_peer(int);
int	OF_child(int);
int	OF_parent(int);
int	OF_instance_to_package(int);
int	OF_getproplen(int, char *);
int	OF_getprop(int, char *, void *, int);
int	OF_nextprop(int, char *, void *);
int	OF_setprop(int, char *, const void *, int);
int	OF_finddevice(char *);
int	OF_instance_to_path(int, char *, int);
int	OF_package_to_path(int, char *, int);
int	OF_call_method_1(char *, int, int, ...);
int	OF_call_method(char *, int, int, int, ...);
int	OF_open(char *);
void	OF_close(int);
int	OF_read(int, void *, int);
int	OF_write(int, void *, int);
int	OF_seek(int, u_quad_t);
void	OF_boot(char *) __attribute__((__noreturn__));
void	OF_enter(void);
void	OF_exit(void) __attribute__((__noreturn__));
void	(*OF_set_callback(void(*)(void *)))(void *);
int	openfirmware(void *);

/*
 * Functions and variables provided by machine-independent code.
 */
int	of_compatible(int, const char * const *);
int	of_decode_int(const unsigned char *buf);
int	of_packagename(int, char *, int);

int	*of_network_decode_media(int, int *, int *);
