/*	$NetBSD: locate.c,v 1.9 1999/08/16 01:41:17 sjg Exp $	*/

/*
 * Copyright (c) 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * James A. Woods.
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

#include <sys/cdefs.h>
#ifndef lint
__COPYRIGHT("@(#) Copyright (c) 1989, 1993\n\
	The Regents of the University of California.  All rights reserved.\n");
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "@(#)locate.c	8.1 (Berkeley) 6/6/93";
#endif
__RCSID("$NetBSD: locate.c,v 1.9 1999/08/16 01:41:17 sjg Exp $");
#endif /* not lint */

/*
 * Ref: Usenix ;login:, Vol 8, No 1, February/March, 1983, p. 8.
 *
 * Locate scans a file list for the full pathname of a file given only part
 * of the name.  The list has been processed with with "front-compression"
 * and bigram coding.  Front compression reduces space by a factor of 4-5,
 * bigram coding by a further 20-25%.
 *
 * The codes are:
 *
 * 	0-28	likeliest differential counts + offset to make nonnegative
 *	30	switch code for out-of-range count to follow in next word
 *	128-255 bigram codes (128 most common, as determined by 'updatedb')
 *	32-127  single character (printable) ascii residue (ie, literal)
 *
 * A novel two-tiered string search technique is employed:
 *
 * First, a metacharacter-free subpattern and partial pathname is matched
 * BACKWARDS to avoid full expansion of the pathname list.  The time savings
 * is 40-50% over forward matching, which cannot efficiently handle
 * overlapped search patterns and compressed path residue.
 *
 * Then, the actual shell glob-style regular expression (if in this form) is
 * matched against the candidate pathnames using the slower routines provided
 * in the standard 'find'.
 */

#include <sys/param.h>

#include <fnmatch.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/queue.h>

#include "locate.h"
#include "pathnames.h"


struct locate_db {
	LIST_ENTRY(locate_db) db_link;
	FILE *db_fp;
};
LIST_HEAD(db_list, locate_db) db_list;

#ifndef NEW
# define NEW(type)      (type *) malloc(sizeof (type))
#endif

void	add_db __P((char *));
int	fastfind __P((FILE *, char *));
int	main __P((int, char **));
char   *patprep __P((char *));


void
add_db(path)
	char *path;
{
	FILE *fp;
	struct locate_db *dbp;

	if (!(path && *path))
		path = _PATH_FCODES;
	if ((fp = fopen(path, "r"))) {
		dbp = NEW(struct locate_db);
		dbp->db_fp = fp;
		LIST_INSERT_HEAD(&db_list, dbp, db_link);
	} else {
		(void)fprintf(stderr, "locate: no database file %s.\n", path);
	}
}
     
int
main(argc, argv)
	int argc;
	char *argv[];
{
	char *locate_path = getenv("LOCATE_PATH");
	char *cp;
	struct locate_db *dbp;
	int c;
	int found = 0;
	
	LIST_INIT(&db_list);
	
	while ((c = getopt(argc, argv, "d:")) != EOF) {
		switch (c) {
		case 'd':
			locate_path = optarg;
			break;
		}
	}
	if (argc <= optind) {
		(void)fprintf(stderr, "usage: locate [-d dbpath] pattern ...\n");
		exit(1);
	}
	if (!locate_path)
		locate_path = _PATH_FCODES;
	if ((cp = strrchr(locate_path, ':'))) {
		locate_path = strdup(locate_path);
		while ((cp = strrchr(locate_path, ':'))) {
			*cp++ = '\0';
			add_db(cp);
		}
	}
	add_db(locate_path);
	if (db_list.lh_first == NULL)
		exit(1);
	for (; optind < argc; ++optind) {
		for (dbp = db_list.lh_first; dbp != NULL;
		     dbp = dbp->db_link.le_next) {
			found |= fastfind(dbp->db_fp, argv[optind]);
		}
	}
	exit(found == 0);
}

int
fastfind(fp, pathpart)
	FILE *fp;
	char *pathpart;
{
	char *p, *s;
	int c;
	int count, found, globflag, printed;
	char *cutoff, *patend, *q;
	char bigram1[NBG], bigram2[NBG], path[MAXPATHLEN];
	
	rewind(fp);
	
	for (c = 0, p = bigram1, s = bigram2; c < NBG; c++)
		p[c] = getc(fp), s[c] = getc(fp);

	p = pathpart;
	globflag = strchr(p, '*') || strchr(p, '?') || strchr(p, '[');
	patend = patprep(p);

	found = printed = 0;
	for (c = getc(fp), count = 0; c != EOF;) {
		count += ((c == SWITCH) ? getw(fp) : c) - OFFSET;
		/* overlay old path */
		for (p = path + count; (c = getc(fp)) > SWITCH;)
			if (c < PARITY)
				*p++ = c;
			else {		/* bigrams are parity-marked */
				c &= PARITY - 1;
				*p++ = bigram1[c], *p++ = bigram2[c];
			}
		*p-- = '\0';
		cutoff = (found ? path : path + count);
		for (found = 0, s = p; s >= cutoff; s--)
			if (*s == *patend) {	/* fast first char check */
				for (p = patend - 1, q = s - 1; *p != '\0';
				    p--, q--)
					if (*q != *p)
						break;
				if (*p == '\0') {	/* fast match success */
					found = 1;
					if (!globflag ||
					    !fnmatch(pathpart, path, 0)) {
						(void)printf("%s\n", path);
						++printed;
					}
					break;
				}
			}
	}
	return (printed);
}

/*
 * extract last glob-free subpattern in name for fast pre-match; prepend
 * '\0' for backwards match; return end of new pattern
 */
static char globfree[100];

char *
patprep(name)
	char *name;
{
	char *endmark, *p, *subp;

	subp = globfree;
	*subp++ = '\0';
	p = name + strlen(name) - 1;
	/* skip trailing metacharacters (and [] ranges) */
	for (; p >= name; p--)
		if (strchr("*?", *p) == 0)
			break;
	if (p < name)
		p = name;
	if (*p == ']')
		for (p--; p >= name; p--)
			if (*p == '[') {
				p--;
				break;
			}
	if (p < name)
		p = name;
	/*
	 * if pattern has only metacharacters, check every path (force '/'
	 * search)
	 */
	if ((p == name) && strchr("?*[]", *p) != 0)
		*subp++ = '/';
	else {
		for (endmark = p; p >= name; p--)
			if (strchr("]*?", *p) != 0)
				break;
		for (++p;
		    (p <= endmark) && subp < (globfree + sizeof(globfree));)
			*subp++ = *p++;
	}
	*subp = '\0';
	return(--subp);
}
