/* $NetBSD: mkdir.c,v 1.25 2001/09/16 22:07:14 wiz Exp $ */

/*
 * Copyright (c) 1983, 1992, 1993
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

#include <sys/cdefs.h>
#ifndef lint
__COPYRIGHT("@(#) Copyright (c) 1983, 1992, 1993\n\
	The Regents of the University of California.  All rights reserved.\n");
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "@(#)mkdir.c	8.2 (Berkeley) 1/25/94";
#else
__RCSID("$NetBSD: mkdir.c,v 1.25 2001/09/16 22:07:14 wiz Exp $");
#endif
#endif /* not lint */

#include <sys/types.h>
#include <sys/stat.h>

#include <err.h>
#include <errno.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int	mkpath(char *, mode_t, mode_t);
void	usage(void);
int	main(int, char *[]);

int
main(int argc, char *argv[])
{
	int ch, exitval, pflag;
	void *set;
	mode_t mode, dir_mode;

	setprogname(argv[0]);
	(void)setlocale(LC_ALL, "");

	/*
	 * The default file mode is a=rwx (0777) with selected permissions
	 * removed in accordance with the file mode creation mask.  For
	 * intermediate path name components, the mode is the default modified
	 * by u+wx so that the subdirectories can always be created.
	 */
	mode = (S_IRWXU | S_IRWXG | S_IRWXO) & ~umask(0);
	dir_mode = mode | S_IWUSR | S_IXUSR;

	pflag = 0;
	while ((ch = getopt(argc, argv, "m:p")) != -1)
		switch(ch) {
		case 'p':
			pflag = 1;
			break;
		case 'm':
			if ((set = setmode(optarg)) == NULL)
				errx(1, "invalid file mode: %s", optarg);
			mode = getmode(set, S_IRWXU | S_IRWXG | S_IRWXO);
			free(set);
			break;
		case '?':
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

	if (*argv == NULL)
		usage();
	
	for (exitval = EXIT_SUCCESS; *argv != NULL; ++argv) {
		char *slash;

		/* Remove trailing slashes, per POSIX. */
		slash = strrchr(*argv, '\0');
		while (--slash > *argv && *slash == '/')
			*slash = '\0';

		if (pflag) {
			if (mkpath(*argv, mode, dir_mode) < 0)
				exitval = EXIT_FAILURE;
		} else {
			if (mkdir(*argv, mode) < 0) {
				warn("%s", *argv);
				exitval = EXIT_FAILURE;
			} else {
				/*
				 * The mkdir() and umask() calls both honor
				 * only the file permission bits, so if you try
				 * to set a mode including the sticky, setuid,
				 * setgid bits you lose them. So chmod().
				 */
				if ((mode & ~(S_IRWXU|S_IRWXG|S_IRWXO)) != 0 &&
				    chmod(*argv, mode) == -1) {
					warn("%s", *argv);
					exitval = EXIT_FAILURE;
				}
			}
		}
	}
	exit(exitval);
	/* NOTREACHED */
}

/*
 * mkpath -- create directories.  
 *	path     - path
 *	mode     - file mode of terminal directory
 *	dir_mode - file mode of intermediate directories
 */
int
mkpath(char *path, mode_t mode, mode_t dir_mode)
{
	struct stat sb;
	char *slash;
	int done;

	done = 0;
	slash = path;

	while (!done) {
		slash += strspn(slash, "/");
		slash += strcspn(slash, "/");

		done = (*slash == '\0');
		*slash = '\0';

		if (stat(path, &sb)) {
			if (errno != ENOENT
			    || mkdir(path, done ? mode : dir_mode)) {
				warn("%s", path);
				return (-1);
			}
			/*
			 * The mkdir() and umask() calls both honor only the
			 * file permission bits, so if you try to set a mode
			 * including the sticky, setuid, setgid bits you lose
			 * them. So chmod().
                         */
			if (done && (mode & ~(S_IRWXU|S_IRWXG|S_IRWXU)) != 0 &&
			    chmod(path, mode) == -1) {
				warn("%s", path);
				return (-1);
			}
		} else if (!S_ISDIR(sb.st_mode)) {
			warnx("%s: %s", path, strerror(ENOTDIR));
			return (-1);
		}
		    
		*slash = '/';
	}

	return (0);
}

void
usage(void)
{
	(void)fprintf(stderr, "usage: %s [-p] [-m mode] dirname ...\n", getprogname());
	exit(EXIT_FAILURE);
	/* NOTREACHED */
}
