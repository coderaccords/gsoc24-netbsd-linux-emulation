/*	$NetBSD: perform.c,v 1.16 1998/10/09 09:35:39 agc Exp $	*/

#include <sys/cdefs.h>
#ifndef lint
#if 0
static const char *rcsid = "from FreeBSD Id: perform.c,v 1.23 1997/10/13 15:03:53 jkh Exp";
#else
__RCSID("$NetBSD: perform.c,v 1.16 1998/10/09 09:35:39 agc Exp $");
#endif
#endif

/*
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
 * 23 Aug 1993
 *
 * This is the main body of the info module.
 *
 */

#include "lib.h"
#include "info.h"

#include <sys/types.h>
#include <sys/stat.h>

#include <err.h>
#include <signal.h>
#include <dirent.h>
#include <ctype.h>

static char    *Home;

static int
pkg_do(char *pkg)
{
	Boolean         installed = FALSE, isTMP = FALSE;
	char            log_dir[FILENAME_MAX];
	char            fname[FILENAME_MAX];
	Package         plist;
	FILE           *fp;
	struct stat     sb;
	char           *cp = NULL;
	int             code = 0;

	if (isURL(pkg)) {
		if ((cp = fileGetURL(NULL, pkg)) != NULL) {
			strcpy(fname, cp);
			isTMP = TRUE;
		}
	} else if (fexists(pkg) && isfile(pkg)) {
		int             len;

		if (*pkg != '/') {
			if (!getcwd(fname, FILENAME_MAX)) {
			    cleanup(0);
			    err(1, "fatal error during execution: getcwd");
			}
			len = strlen(fname);
			snprintf(&fname[len], FILENAME_MAX - len, "/%s", pkg);
		} else
			strcpy(fname, pkg);
		cp = fname;
	} else {
		if ((cp = fileFindByPath(NULL, pkg)) != NULL)
			strncpy(fname, cp, FILENAME_MAX);
	}
	if (cp) {
		if (isURL(pkg)) {
			/* file is already unpacked by fileGetURL() */
			strcpy(PlayPen, cp);
		} else {
			/*
			 * Apply a crude heuristic to see how much space the package will
			 * take up once it's unpacked.  I've noticed that most packages
			 * compress an average of 75%, but we're only unpacking the + files so
			 * be very optimistic.
			 */
			if (stat(fname, &sb) == FAIL) {
				warnx("can't stat package file '%s'", fname);
				code = 1;
				goto bail;
			}
			Home = make_playpen(PlayPen, PlayPenSize, sb.st_size / 2);
			if (unpack(fname, "+*")) {
				warnx("error during unpacking, no info for '%s' available", pkg);
				code = 1;
				goto bail;
			}
		}
	}
	/*
	 * It's not an uninstalled package, try and find it among the
	 * installed
	 */
	else {
		char           *tmp;

		(void) snprintf(log_dir, sizeof(log_dir), "%s/%s", (tmp = getenv(PKG_DBDIR)) ? tmp : DEF_LOG_DIR,
			pkg);
		if (!fexists(log_dir)) {
			warnx("can't find package `%s' installed or in a file!", pkg);
			return 1;
		}
		if (chdir(log_dir) == FAIL) {
			warnx("can't change directory to '%s'!", log_dir);
			return 1;
		}
		installed = TRUE;
	}

	/* Suck in the contents list */
	plist.head = plist.tail = NULL;
	fp = fopen(CONTENTS_FNAME, "r");
	if (!fp) {
		warnx("unable to open %s file", CONTENTS_FNAME);
		code = 1;
		goto bail;
	}
	/* If we have a prefix, add it now */
	read_plist(&plist, fp);
	fclose(fp);

	/*
         * Index is special info type that has to override all others to make
         * any sense.
         */
	if (Flags & SHOW_INDEX) {
		char            tmp[FILENAME_MAX];

		snprintf(tmp, FILENAME_MAX, "%-19s ", pkg);
		show_index(tmp, COMMENT_FNAME);
	} else {
		/* Start showing the package contents */
		if (!Quiet)
			printf("%sInformation for %s:\n\n", InfoPrefix, pkg);
		if (Flags & SHOW_COMMENT)
			show_file("Comment:\n", COMMENT_FNAME);
		if ((Flags & SHOW_REQBY) && !isemptyfile(REQUIRED_BY_FNAME))
			show_file("Required by:\n", REQUIRED_BY_FNAME);
		if (Flags & SHOW_DESC)
			show_file("Description:\n", DESC_FNAME);
		if ((Flags & SHOW_DISPLAY) && fexists(DISPLAY_FNAME))
			show_file("Install notice:\n", DISPLAY_FNAME);
		if (Flags & SHOW_PLIST)
			show_plist("Packing list:\n", &plist, PLIST_SHOW_ALL);
		if ((Flags & SHOW_INSTALL) && fexists(INSTALL_FNAME))
			show_file("Install script:\n", INSTALL_FNAME);
		if ((Flags & SHOW_DEINSTALL) && fexists(DEINSTALL_FNAME))
			show_file("De-Install script:\n", DEINSTALL_FNAME);
		if ((Flags & SHOW_MTREE) && fexists(MTREE_FNAME))
			show_file("mtree file:\n", MTREE_FNAME);
		if (Flags & SHOW_PREFIX)
			show_plist("Prefix(s):\n", &plist, PLIST_CWD);
		if (Flags & SHOW_FILES)
			show_files("Files:\n", &plist);
		if (!Quiet)
			puts(InfoPrefix);
	}
	free_plist(&plist);
bail:
	leave_playpen(Home);
	if (isTMP)
		unlink(fname);
	return code;
}

/* fn to be called for pkgs found */
static int
foundpkg(const char *found, char *data)
{
    if(!Quiet)
	printf("%s\n", found);
    return 0;
}

/* check if a package "pkgspec" (which can be a pattern) is installed */
/* return 0 if found, 1 otherwise (indicating an error). */
static int
check4pkg(char *pkgspec, char *dbdir)
{
	if (strpbrk(pkgspec, "<>[]?*{")) {
	    /* expensive (pattern) match */
	    int found;

	    found=findmatchingname(dbdir, pkgspec, foundpkg, NULL);
	    return !found;
	} else {
		/* simple match */
	char            buf[FILENAME_MAX];
	int             error;
		struct stat     st;

		snprintf(buf, sizeof(buf), "%s/%s", dbdir, pkgspec);
		error = (stat(buf, &st) < 0);
		if (!error && !Quiet)
		printf("%s\n", pkgspec);

	return error;
	}
}

void
cleanup(int sig)
{
	leave_playpen(Home);
	exit(1);
}

int
pkg_perform(char **pkgs)
{
	int             i, err_cnt = 0;
	char           *tmp;

	signal(SIGINT, cleanup);

	tmp = getenv(PKG_DBDIR);
	if (!tmp)
		tmp = DEF_LOG_DIR;
	/* Overriding action? */
	if (CheckPkg) {
		err_cnt += check4pkg(CheckPkg, tmp);
	} else if (AllInstalled) {
		struct dirent  *dp;
		DIR            *dirp;

		if (!(isdir(tmp) || islinktodir(tmp)))
			return 1;
		if ((dirp = opendir(tmp)) != (DIR *) NULL) {
			while ((dp = readdir(dirp)) != (struct dirent *) NULL) {
				if (strcmp(dp->d_name, ".") && strcmp(dp->d_name, "..")) {
					err_cnt += pkg_do(dp->d_name);
				}
			}
			(void) closedir(dirp);
		}
	} else {
		for (i = 0; pkgs[i]; i++) {
			err_cnt += pkg_do(pkgs[i]);
		}
	}
	return err_cnt;
}
