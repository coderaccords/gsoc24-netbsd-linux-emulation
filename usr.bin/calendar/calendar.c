/*	$NetBSD: calendar.c,v 1.44 2008/03/07 19:22:22 christos Exp $	*/

/*
 * Copyright (c) 1989, 1993, 1994
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
 * 3. Neither the name of the University nor the names of its contributors
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
static char sccsid[] = "@(#)calendar.c	8.4 (Berkeley) 1/7/95";
#endif
__RCSID("$NetBSD: calendar.c,v 1.44 2008/03/07 19:22:22 christos Exp $");
#endif /* not lint */

#include <sys/param.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <sys/wait.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <pwd.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <tzfile.h>
#include <unistd.h>

#include "pathnames.h"

static unsigned short lookahead = 1;
static unsigned short weekend = 2;
static char *fname = NULL;
static char *datestr = NULL;
static const char *defaultnames[] = {"calendar", ".calendar", _PATH_SYSTEM_CALENDAR, NULL};
static struct passwd *pw;
static char path[MAXPATHLEN + 1];
static bool doall = false;
static bool cpp_restricted = false;

/* 1-based month, 0-based days, cumulative */
static const int daytab[][14] = {
	{ 0, -1, 30, 58, 89, 119, 150, 180, 211, 242, 272, 303, 333, 364 },
	{ 0, -1, 30, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334, 365 },
};
static struct tm *tp;
static const int *cumdays;
static int offset, yrdays;
static char dayname[10];

static struct iovec header[] = {
	{ __UNCONST("From: "), 6 },
	{ NULL, 0 },
	{ __UNCONST(" (Reminder Service)\nTo: "), 24 },
	{ NULL, 0 },
	{ __UNCONST("\nSubject: "), 10 },
	{ NULL, 0 },
	{ __UNCONST("'s Calendar\nPrecedence: bulk\n\n"),  30 },
};

static const char *days[] = {
	"sun", "mon", "tue", "wed", "thu", "fri", "sat", NULL,
};

static const char *months[] = {
	"jan", "feb", "mar", "apr", "may", "jun",
	"jul", "aug", "sep", "oct", "nov", "dec", NULL,
};

static void	 atodays(int, char *, unsigned short *);
static void	 cal(void);
static void	 closecal(FILE *);
static int	 getday(char *);
static int	 getfield(char *, char **, int *);
static void	 getmmdd(struct tm *, char *);
static int	 getmonth(char *);
static bool	 isnow(char *);
static FILE	*opencal(void);
static void	 settime(void);
static void	 usage(void) __dead;

int
main(int argc, char **argv)
{
	int ch;
	const char *caldir;

	(void)setprogname(argv[0]);	/* for portability */

	while ((ch = getopt(argc, argv, "-ad:f:l:w:x")) != -1) {
		switch (ch) {
		case '-':		/* backward contemptible */
		case 'a':
			if (getuid()) {
				errno = EPERM;
				err(EXIT_FAILURE, NULL);
			}
			doall = true;
			break;
		case 'd':
			datestr = optarg;
			break;
		case 'f':
			fname = optarg;
			break;
		case 'l':
			atodays(ch, optarg, &lookahead);
			break;
		case 'w':
			atodays(ch, optarg, &weekend);
			break;
		case 'x':
			cpp_restricted = true;
			break;
		case '?':
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;

	if (argc)
		usage();

	settime();
	if (doall) {
		/*
		 * XXX - This ignores the user's CALENDAR_DIR variable.
		 *       Run under user's login shell?
		 */
		while ((pw = getpwent()) != NULL) {
			(void)setegid(pw->pw_gid);
			(void)seteuid(pw->pw_uid);
			if (chdir(pw->pw_dir) != -1)
				cal();
			(void)seteuid(0);
		}
	} else if ((caldir = getenv("CALENDAR_DIR")) != NULL) {
		if (chdir(caldir) != -1)
			cal();
	} else if ((pw = getpwuid(geteuid())) != NULL) {
		if (chdir(pw->pw_dir) != -1)
			cal();
	}
	return 0;
}

static void
cal(void)
{
	bool printing;
	FILE *fp;
	char *line;

	if ((fp = opencal()) == NULL)
		return;
	printing = false;
	while ((line = fparseln(stdin,
		    NULL, NULL, NULL, FPARSELN_UNESCCOMM)) != NULL) {
		if (line[0] == '\0')
			continue;
		if (line[0] != '\t')
			printing = isnow(line);
		if (printing)
			(void)fprintf(fp, "%s\n", line);
		free(line);

	}
	closecal(fp);
}

static void
settime(void)
{
	time_t now;

	(void)time(&now);
	tp = localtime(&now);
	if (datestr)
		getmmdd(tp, datestr);

	if (isleap(tp->tm_year + TM_YEAR_BASE)) {
		yrdays = DAYSPERLYEAR;
		cumdays = daytab[1];
	} else {
		yrdays = DAYSPERNYEAR;
		cumdays = daytab[0];
	}
	/* Friday displays Monday's events */
	offset = tp->tm_wday == 5 ? lookahead + weekend : lookahead;
	header[5].iov_base = dayname;
	header[5].iov_len = strftime(dayname, sizeof(dayname), "%A", tp);
}

/*
 * Possible date formats include any combination of:
 *	3-charmonth			(January, Jan, Jan)
 *	3-charweekday			(Friday, Monday, mon.)
 *	numeric month or day		(1, 2, 04)
 *
 * Any character may separate them, or they may not be separated.  Any line,
 * following a line that is matched, that starts with "whitespace", is shown
 * along with the matched line.
 */
static bool
isnow(char *endp)
{
	int day;
	int flags;
	int month;
	int v1;
	int v2;

#define	F_ISMONTH	0x01
#define	F_ISDAY		0x02
#define F_WILDMONTH	0x04
#define F_WILDDAY	0x08

	flags = 0;

	/* didn't recognize anything, skip it */
	if (!(v1 = getfield(endp, &endp, &flags)))
		return false;

	if (flags & F_ISDAY || v1 > 12) {
		/* found a day */
		day = v1;
		/* if no recognizable month, assume wildcard ('*') month */
		if ((month = getfield(endp, &endp, &flags)) != 0) {
			flags |= F_ISMONTH | F_WILDMONTH;
			month = tp->tm_mon + 1;
		}
	} else if (flags & F_ISMONTH) {
		month = v1;
		/* if no recognizable day, assume the first */
		if ((day = getfield(endp, &endp, &flags)) != 0)
			day = 1;
	} else {
		v2 = getfield(endp, &endp, &flags);
		if (flags & F_ISMONTH) {
			day = v1;
			month = v2;
		} else {
			/* F_ISDAY set, v2 > 12, or no way to tell */
			month = v1;
			/* if no recognizable day, assume the first */
			day = v2 ? v2 : 1;
		}
	}
	if (flags & F_WILDMONTH && flags & F_WILDDAY)
		return true;

	if (flags & F_WILDMONTH && flags & F_ISDAY && day == tp->tm_mday)
		return true;

	if (flags & F_ISMONTH && flags & F_WILDDAY && month == tp->tm_mon + 1)
		return true;

	if (flags & F_ISDAY)
		day = tp->tm_mday + (((day - 1) - tp->tm_wday + 7) % 7);
	day = cumdays[month] + day;

	/* if today or today + offset days */
	if (day >= tp->tm_yday && day <= tp->tm_yday + offset)
		return true;

	/* if number of days left in this year + days to event in next year */
	if (yrdays - tp->tm_yday + day <= offset)
		return true;

	return false;
}

static int
getfield(char *p, char **endp, int *flags)
{
	int val;
	char *start;
	char savech;

#define FLDCHAR(a) (*p != '\0' && !isdigit((unsigned char)*p) && \
    !isalpha((unsigned char)*p) && *p != '*')

	for (/*EMPTY*/; FLDCHAR(*p); ++p)
		continue;
	if (*p == '*') {			/* `*' is current month */
		if (!(*flags & F_ISMONTH)) {
			*flags |= F_ISMONTH | F_WILDMONTH;
			*endp = p + 1;
			return tp->tm_mon + 1;
		} else {
			*flags |= F_ISDAY | F_WILDDAY;
			*endp = p + 1;
			return 1;
		}
	}
	if (isdigit((unsigned char)*p)) {
		val = (int)strtol(p, &p, 10);	/* if 0, it's failure */
		for (/*EMPTY*/; FLDCHAR(*p); ++p)
			continue;
		*endp = p;
		return val;
	}
	for (start = p; *p != '\0' && isalpha((unsigned char)*++p); /*EMPTY*/)
		continue;
	savech = *p;
	*p = '\0';
	if ((val = getmonth(start)) != 0)
		*flags |= F_ISMONTH;
	else if ((val = getday(start)) != 0)
		*flags |= F_ISDAY;
	else {
		*p = savech;
		return 0;
	}
	for (*p = savech; FLDCHAR(*p); ++p)
		continue;
	*endp = p;
	return val;
}

static FILE *
opencal(void)
{
	int fd;
	int pdes[2];
	const char **name;

	/* open up calendar file as stdin */
	if (fname == NULL) {
		for (name = defaultnames; *name != NULL; name++) {
			if (freopen(*name, "rf", stdin) == NULL)
				continue;
			else
				break;
		}
		if (*name == NULL) {
			if (doall)
				return NULL;
			err(EXIT_FAILURE, "Cannot open calendar file");
		}
	} else if (freopen(fname, "rf", stdin) == NULL) {
		if (doall)
			return NULL;
		err(EXIT_FAILURE, "Cannot open `%s'", fname);
	}

	if (pipe(pdes) == -1) {
		warn("Cannot open pipe");
		return NULL;
	}

	switch (fork()) {
	case -1:
		/* error */
		(void)close(pdes[0]);
		(void)close(pdes[1]);
		return NULL;
	case 0:
		/* child -- stdin already setup, set stdout to pipe input */
		if (pdes[1] != STDOUT_FILENO) {
			(void)dup2(pdes[1], STDOUT_FILENO);
			(void)close(pdes[1]);
		}
		(void)close(pdes[0]);
		/* tell CPP to only open regular files */
		if(!cpp_restricted && setenv("CPP_RESTRICTED", "", 1) == -1)
			err(EXIT_FAILURE, "Cannot restrict cpp");
		cpp_restricted = true;

		(void)execl(_PATH_CPP, "cpp", "-traditional", "-P", "-I.",
		    "-I" _PATH_CALENDARS, NULL);
		err(EXIT_FAILURE, "Cannot exec `%s'", _PATH_CPP);
		/*NOTREACHED*/
	default:
		/* parent -- set stdin to pipe output */
		(void)dup2(pdes[0], STDIN_FILENO);
		(void)close(pdes[0]);
		(void)close(pdes[1]);

		/* not reading all calendar files, just set output to stdout */
		if (!doall)
			return stdout;

		/*
		 * Set output to a temporary file, so if no output
		 * don't send mail.
		 */
		(void)snprintf(path, sizeof(path), "%s/_calXXXXXX", _PATH_TMP);
		if ((fd = mkstemp(path)) == -1) {
			warn("Cannot create temporary file");
			return NULL;
		}
		return fdopen(fd, "w+");
	}
	/*NOTREACHED*/
}

static void
closecal(FILE *fp)
{
	struct stat sbuf;
	ssize_t nread;
	int pdes[2];
	int status;
	char buf[1024];

	if (!doall)
		return;

	(void)rewind(fp);
	if (fstat(fileno(fp), &sbuf) == -1 || sbuf.st_size == 0)
		goto done;
	if (pipe(pdes) == -1)
		goto done;

	switch (fork()) {
	case -1:
		/* error */
		(void)close(pdes[0]);
		(void)close(pdes[1]);
		break;
	case 0:
		/* child -- set stdin to pipe output */
		if (pdes[0] != STDIN_FILENO) {
			(void)dup2(pdes[0], STDIN_FILENO);
			(void)close(pdes[0]);
		}
		(void)close(pdes[1]);
		(void)execl(_PATH_SENDMAIL, "sendmail", "-i", "-t", "-F",
		    "\"Reminder Service\"", "-f", "root", NULL);
		err(EXIT_FAILURE, "Cannot exec `%s'", _PATH_SENDMAIL);
		/*NOTREACHED*/
	default:
		/* parent -- write to pipe input */
		(void)close(pdes[0]);

		header[1].iov_base = header[3].iov_base = (void *)pw->pw_name;
		header[1].iov_len = header[3].iov_len = strlen(pw->pw_name);
		(void)writev(pdes[1], header, 7);
		while ((nread = read(fileno(fp), buf, sizeof(buf))) > 0)
			(void)write(pdes[1], buf, (size_t)nread);
		(void)close(pdes[1]);
		break;
	}

done:	(void)fclose(fp);
	(void)unlink(path);
	while (wait(&status) != -1)
		continue;
}

static int
getmonth(char *s)
{
	const char **p;

	for (p = months; *p; ++p)
		if (strncasecmp(s, *p, 3) == 0)
			return (int)(p - months) + 1;
	return 0;
}

static int
getday(char *s)
{
	const char **p;

	for (p = days; *p; ++p)
		if (strncasecmp(s, *p, 3) == 0)
			return (int)(p - days) + 1;
	return 0;
}

static void
atodays(int ch, char *arg, unsigned short *rvp)
{
	int u;

	u = atoi(arg);
	if (u < 0 || u > 366)
		warnx("-%c %d out of range 0-366, ignored.", ch, u);
	else
		*rvp = u;
}

#define todigit(x) ((x) - '0')
#define ATOI2(x) (todigit((x)[0]) * 10 + todigit((x)[1]))
#define ISDIG2(x) (isdigit((unsigned char)(x)[0]) && isdigit((unsigned char)(x)[1]))

static void
getmmdd(struct tm *ptm, char *ds)
{
	bool ok = false;
	struct tm ttm;

	ttm = *ptm;
	ttm.tm_isdst = -1;

	if (ISDIG2(ds)) {
		ttm.tm_mon = ATOI2(ds) - 1;
		ds += 2;
	}
	if (ISDIG2(ds)) {
		ttm.tm_mday = ATOI2(ds);
		ds += 2;
		ok = true;
	}
	if (ok) {
		if (ISDIG2(ds) && ISDIG2(ds + 2)) {
			ttm.tm_year = ATOI2(ds) * 100 - TM_YEAR_BASE;
			ds += 2;
			ttm.tm_year += ATOI2(ds);
		} else if (ISDIG2(ds)) {
			ttm.tm_year = ATOI2(ds);
			if (ttm.tm_year < 69)
				ttm.tm_year += 2000 - TM_YEAR_BASE;
			else
				ttm.tm_year += 1900 - TM_YEAR_BASE;
		}
	}
	if (ok && mktime(&ttm) == -1)
		ok = false;

	if (ok)
		*ptm = ttm;
	else {
		warnx("Can't convert `%s' to date, ignored.", ds);
		usage();
	}
}

__dead
static void
usage(void)
{
	(void)fprintf(stderr, "usage: %s [-ax] [-d MMDD[[YY]YY]"
	    " [-f fname] [-l days] [-w days]\n", getprogname());
	exit(1);
}
