/*	$NetBSD: hasmntopt.c,v 1.1.1.2 1997/10/26 00:02:15 christos Exp $	*/

/*
 * Copyright (c) 1997 Erez Zadok
 * Copyright (c) 1990 Jan-Simon Pendry
 * Copyright (c) 1990 Imperial College of Science, Technology & Medicine
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Jan-Simon Pendry at Imperial College, London.
 *
 * Redistribution and use in source and binary forms, with or without
n * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by the University of
 *      California, Berkeley and its contributors.
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
 *      %W% (Berkeley) %G%
 *
 * Id: hasmntopt.c,v 5.2.2.2 1992/05/31 16:35:45 jsp Exp 
 *
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif /* HAVE_CONFIG_H */
#include <am_defs.h>
#include <amu.h>

#ifndef MNTMAXSTR
# define MNTMAXSTR	128
#endif /* not MNTMAXSTR */


/*
 * Some systems don't provide these to the user,
 * but amd needs them, so...
 *
 * From: Piete Brooks <pb@cl.cam.ac.uk>
 */

static char *
nextmntopt(char **p)
{
  char *cp = *p;
  char *rp;

  /*
   * Skip past white space
   */
  while (*cp && isspace(*cp))
    cp++;

  /*
   * Word starts here
   */
  rp = cp;

  /*
   * Scan to send of string or separator
   */
  while (*cp && *cp != ',')
    cp++;

  /*
   * If separator found the overwrite with nul char.
   */
  if (*cp) {
    *cp = '\0';
    cp++;
  }

  /*
   * Return value for next call
   */
  *p = cp;
  return rp;
}

/*
 * replacement for hasmntopt if the system does not have it.
 */
char *
hasmntopt(mntent_t *mnt, char *opt)
{
  char t[MNTMAXSTR];
  char *f;
  char *o = t;
  int l = strlen(opt);

  strcpy(t, mnt->mnt_opts);

  while (*(f = nextmntopt(&o)))
    if (strncmp(opt, f, l) == 0)
      return f - t + mnt->mnt_opts;

  return 0;
}
