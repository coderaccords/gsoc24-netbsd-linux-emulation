/*	$NetBSD: nlist.c,v 1.14 1996/09/30 18:27:02 thorpej Exp $	*/

/*-
 * Copyright (c) 1996 Christopher G. Demetriou.  All rights reserved.
 * Copyright (c) 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
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
 */

#ifndef lint
#if 0
static char sccsid[] = "from: @(#)nlist.c	8.1 (Berkeley) 6/6/93";
#else
static char *rcsid = "$NetBSD: nlist.c,v 1.14 1996/09/30 18:27:02 thorpej Exp $";
#endif
#endif /* not lint */

#include <sys/param.h>

#include <a.out.h>
#include <db.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <kvm.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "extern.h"

static struct {
        int     (*knlist) __P((const char *, DB *));
} knlist_fmts[] = {
#ifdef NLIST_AOUT
        {       create_knlist_aout          },
#endif
#ifdef NLIST_ECOFF
        {       create_knlist_ecoff         },
#endif
#ifdef NLIST_ELF32
        {       create_knlist_elf32         },
#endif
#ifdef NLIST_ELF64
        {       create_knlist_elf64         },
#endif
};
        
void
create_knlist(name, db)
	const char *name;
	DB *db;
{
	int i;

        for (i = 0; i < sizeof(knlist_fmts) / sizeof(knlist_fmts[0]); i++)
                if ((*knlist_fmts[i].knlist)(name, db) != -1)
                        return;
	warnx("%s: file format not recognized", name);
	punt();
}

/*
 * XXX: Using this value from machine/param.h introduces a
 * XXX: machine dependency on this program, so /usr can not
 * XXX: be shared between (i.e.) several m68k machines.
 * Instead of compiling in KERNTEXTOFF or KERNBASE, try to
 * determine the text start address from a standard symbol.
 * For backward compatibility, use the old compiled-in way
 * when the standard symbol name is not found.
 */
#ifndef KERNTEXTOFF
#define KERNTEXTOFF KERNBASE
#endif

u_long
get_kerntext(name)
	const char *name;
{
	struct nlist nl[2];

	bzero((caddr_t)nl, sizeof(nl));
	nl[0].n_un.n_name = "_kernel_text";

	if (nlist(name, nl) != 0)
		return (KERNTEXTOFF);

	return (nl[0].n_value);
}
