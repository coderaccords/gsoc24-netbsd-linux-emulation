/*	$NetBSD: main.c,v 1.10 1999/08/19 14:12:34 agc Exp $	*/

#include <sys/cdefs.h>
#ifndef lint
#if 0
static char *rcsid = "from FreeBSD Id: main.c,v 1.16 1997/10/08 07:45:43 charnier Exp";
#else
__RCSID("$NetBSD: main.c,v 1.10 1999/08/19 14:12:34 agc Exp $");
#endif
#endif

/*
 *
 * FreeBSD install - a package for the installation and maintainance
 * of non-core utilities.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * Jordan K. Hubbard
 * 18 July 1993
 *
 * This is the add module.
 *
 */

#include <err.h>
#include <sys/param.h>
#include "lib.h"
#include "add.h"

static char Options[] = "hvIRfnp:SMt:";

char	*Prefix		= NULL;
Boolean	NoInstall	= FALSE;
Boolean	NoRecord	= FALSE;

char	*Mode		= NULL;
char	*Owner		= NULL;
char	*Group		= NULL;
char	*PkgName	= NULL;
char	*Directory	= NULL;
char	FirstPen[FILENAME_MAX];
add_mode_t AddMode	= NORMAL;

static void
usage(void)
{
    fprintf(stderr, "%s\n%s\n",
		"usage: pkg_add [-vInfRMS] [-t template] [-p prefix]",
		"               pkg-name [pkg-name ...]");
    exit(1);
}

int
main(int argc, char **argv)
{
    int ch, error;
    lpkg_head_t pkgs;
    lpkg_t *lpp;
    char *cp;
    char pkgname[MAXPATHLEN];

    while ((ch = getopt(argc, argv, Options)) != -1) {
	switch(ch) {
	case 'v':
	    Verbose = TRUE;
	    break;

	case 'p':
	    Prefix = optarg;
	    break;

	case 'I':
	    NoInstall = TRUE;
	    break;

	case 'R':
	    NoRecord = TRUE;
	    break;

	case 'f':
	    Force = TRUE;
	    break;

	case 'n':
	    Fake = TRUE;
	    Verbose = TRUE;
	    break;

	case 't':
	    strcpy(FirstPen, optarg);
	    break;

	case 'S':
	    AddMode = SLAVE;
	    break;

	case 'M':
	    AddMode = MASTER;
	    break;

	case 'h':
	case '?':
	default:
	    usage();
	    break;
	}
    }
    argc -= optind;
    argv += optind;

    TAILQ_INIT(&pkgs);
    
    if (AddMode != SLAVE) {
	/* Get all the remaining package names, if any */
	for (ch = 0; *argv; ch++, argv++) {
	    if (!strcmp(*argv, "-"))	/* stdin? */
		lpp = alloc_lpkg("-");
	    else if (URLlength(*argv) > 0)	/* preserve URLs */
		lpp = alloc_lpkg(*argv);
	    else {			/* expand all pathnames to fullnames */
		char *s;
		    
		if (fexists(*argv)) {	/* refers to a file directly */
		    lpp = alloc_lpkg(realpath(*argv, pkgname));
		} else if (ispkgpattern(*argv)
			 && (s=findbestmatchingname(dirname_of(*argv),
						    basename_of(*argv))) != NULL) {
		    if (Verbose)
			printf("Using %s for %s\n",s, *argv);

		    lpp = alloc_lpkg(realpath(s, pkgname));
		} else {
		    /* look for the file(pattern) in the expected places */
		    if (!(cp = fileFindByPath(NULL, *argv))) {
			lpp = NULL;
			warnx("can't find package '%s'", *argv);
		    } else
			lpp = alloc_lpkg(cp);
		}
	    }
	    if (lpp)
		TAILQ_INSERT_TAIL(&pkgs, lpp, lp_link);
	}
    }
    /* If no packages, yelp */
    else if (!ch)
	warnx("missing package name(s)"), usage();
    else if (ch > 1 && AddMode == MASTER)
	warnx("only one package name may be specified with master mode"),
	usage();
    if ((error = pkg_perform(&pkgs)) != 0) {
	if (Verbose)
	    warnx("%d package addition(s) failed", error);
	return error;
    }
    else
	return 0;
}
