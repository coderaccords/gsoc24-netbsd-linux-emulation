/*	$NetBSD: getoldopt.c,v 1.11 2001/10/25 05:33:33 lukem Exp $	*/

/*
 * Plug-compatible replacement for getopt() for parsing tar-like
 * arguments.  If the first argument begins with "-", it uses getopt;
 * otherwise, it uses the old rules used by tar, dump, and ps.
 *
 * Written 25 August 1985 by John Gilmore (ihnp4!hoptoad!gnu) and placed
 * in the Public Domain for your edification and enjoyment.
 */

#include <sys/cdefs.h>
#ifndef lint
__RCSID("$NetBSD: getoldopt.c,v 1.11 2001/10/25 05:33:33 lukem Exp $");
#endif /* not lint */

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <sys/stat.h>
#include "pax.h"
#include "extern.h"

int
getoldopt(int argc, char **argv, const char *optstring,
	struct option *longopts, int *index)
{
	static char	*key;		/* Points to next keyletter */
	static char	use_getopt;	/* !=0 if argv[1][0] was '-' */
	char		c;
	char		*place;

	optarg = NULL;

	if (key == NULL) {		/* First time */
		if (argc < 2) return -1;
		key = argv[1];
		if (*key == '-')
			use_getopt++;
		else
			optind = 2;
	}

	if (use_getopt)
		return ((longopts != NULL) ?
			getopt_long(argc, argv, optstring, longopts, index) :
			getopt(argc, argv, optstring));

	c = *key++;
	if (c == '\0') {
		key--;
		return -1;
	}
	place = strchr(optstring, c);

	if (place == NULL || c == ':') {
		fprintf(stderr, "%s: unknown option %c\n", argv[0], c);
		return('?');
	}

	place++;
	if (*place == ':') {
		if (optind < argc) {
			optarg = argv[optind];
			optind++;
		} else {
			fprintf(stderr, "%s: %c argument missing\n",
				argv[0], c);
			return('?');
		}
	}

	return(c);
}
