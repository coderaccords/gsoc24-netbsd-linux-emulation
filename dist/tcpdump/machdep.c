/*	$NetBSD: machdep.c,v 1.1.1.1 2001/06/25 19:26:32 itojun Exp $	*/

/*
 * Copyright (c) 1996, 1997
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that: (1) source code distributions
 * retain the above copyright notice and this paragraph in its entirety, (2)
 * distributions including binary code include the above copyright notice and
 * this paragraph in its entirety in the documentation or other materials
 * provided with the distribution, and (3) all advertising materials mentioning
 * features or use of this software display the following acknowledgement:
 * ``This product includes software developed by the University of California,
 * Lawrence Berkeley Laboratory and its contributors.'' Neither the name of
 * the University nor the names of its contributors may be used to endorse
 * or promote products derived from this software without specific prior
 * written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#ifndef lint
static const char rcsid[] =
    "@(#) Header: /tcpdump/master/tcpdump/machdep.c,v 1.6 2000/01/17 06:24:23 itojun Exp (LBL)";
#endif

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <sys/types.h>
#ifdef __osf__
#include <sys/sysinfo.h>
#include <sys/proc.h>
#endif

#include "machdep.h"

int
abort_on_misalignment(char *ebuf, size_t ebufsiz)
{
#ifdef __osf__
	static int buf[2] = { SSIN_UACPROC, UAC_SIGBUS };

	if (setsysinfo(SSI_NVPAIRS, (caddr_t)buf, 1, 0, 0) < 0) {
		(void)snprintf(ebuf, ebufsiz, "setsysinfo: errno %d", errno);
		return (-1);
	}
#endif
	return (0);
}
