/*	$NetBSD: calendar.c,v 1.27 2002/11/30 03:10:55 lukem Exp $	*/

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
static char sccsid[] = "@(#)calendar.c	8.4 (Berkeley) 1/7/95";
#endif
__RCSID("$NetBSD: calendar.c,v 1.27 2002/11/30 03:10:55 lukem Exp $");
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <tzfile.h>
#include <unistd.h>

#include "pathnames.h"

#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif

static unsigned short lookahead = 1, weekend = 2;
static char *fname = "calendar", *datestr = NULL;
static struct passwd *pw;
static int doall;
static char path[MAXPATHLEN + 1];

/* 1-based month, 0-based days, cumulative */
static int daytab[][14] = {
	{ 0, -1, 30, 58, 89, 119, 150, 180, 211, 242, 272, 303, 333, 364 },
	{ 0, -1, 30, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334, 365 },
};
static struct tm *tp;
static int *cumdays, offset, yrdays;
static char dayname[10];

static struct iovec header[] = {
	{ "From: ", 6 },
	{ NULL, 0 },
	{ " (Reminder Service)\nTo: ", 24 },
	{ NULL, 0 },
	{ "\nSubject: ", 10 },
	{ NULL, 0 },
	{ "'s Calendar\nPrecedence: bulk\n\n",  30 },
};

static char *days[] = {
	"sun", "mon", "tue", "wed", "thu", "fri", "sat", NULL,
};

static char *months[] = {
	"jan", "feb", "mar", "apr", "may", "jun",
	"jul", "aug", "sep", "oct", "nov", "dec", NULL,
};

int	 main(int, char **);

static void	 atodays(int, char *, unsigned short *);
static void	 cal(void);
static void	 closecal(FILE *);
static int	 getday(char *);
static int	 getfield(char *, char **, int *);
static void	 getmmdd(struct tm *, char *);
static int	 getmonth(char *);
static int	 isnow(char *);
static FILE	*opencal(void);
static void	 settime(void);
static void	 usage(void) __attribute__((__noreturn__));

int
main(argc, argv)
	int argc;
	char *argv[];
{
	int ch;
	const char *caldir;

	while ((ch = getopt(argc, argv, "-ad:f:l:w:")) != -1)
		switch (ch) {
		case '-':		/* backward contemptible */
		case 'a':
			if (getuid()) {
				errno = EPERM;
				err(1, NULL);
			}
			doall = 1;
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
		case '?':
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

	if (argc)
		usage();

	settime();
	if (doall) {
		while ((pw = getpwent()) != NULL) {
			(void)setegid(pw->pw_gid);
			(void)seteuid(pw->pw_uid);
			if (!chdir(pw->pw_dir))
				cal();
			(void)seteuid(0);
		}
	} else if ((caldir = getenv("CALENDAR_DIR")) != NULL) {
		if(!chdir(caldir))
			cal();
	} else {
		cal();
	}
	exit(0);
}

static void
cal(void)
{
	int printing;
	FILE *fp;
	char *line;

	if ((fp = opencal()) == NULL)
		return;
	while ((line = fparseln(stdin, NULL, NULL, NULL, 0)) != NULL) {
		if (line[0] == '\0')
			continue;
		if (line[0] != '\t')
			printing = isnow(line) ? 1 : 0;
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
	if (datestr) {
		getmmdd(tp, datestr);
	}
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
static int
isnow(endp)
	char *endp;
{
	int day, flags, month, v1, v2;

#define	F_ISMONTH	0x01
#define	F_ISDAY		0x02
	flags = 0;
	/* didn't recognize anything, skip it */
	if (!(v1 = getfield(endp, &endp, &flags)))
		return (0);
	if (flags & F_ISDAY || v1 > 12) {
		/* found a day */
		day = v1;
		month = tp->tm_mon + 1;
	} else if (flags & F_ISMONTH) {
		month = v1;
		/* if no recognizable day, assume the first */
		if (!(day = getfield(endp, &endp, &flags)))
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
	if (flags & F_ISDAY)
		day = tp->tm_mday + (((day - 1) - tp->tm_wday + 7) % 7);
	day = cumdays[month] + day;

	/* if today or today + offset days */
	if (day >= tp->tm_yday && day <= tp->tm_yday + offset)
		return (1);
	/* if number of days left in this year + days to event in next year */
	if (yrdays - tp->tm_yday + day <= offset)
		return (1);
	return (0);
}

static int
getfield(p, endp, flags)
	char *p, **endp;
	int *flags;
{
	int val;
	char *start, savech;

#define FLDCHAR(a) (*p != '\0' && !isdigit((unsigned char)*p) && \
    !isalpha((unsigned char)*p) && *p != '*')

	for (; FLDCHAR(*p); ++p)
		continue;
	if (*p == '*') {			/* `*' is current month */
		*flags |= F_ISMONTH;
		*endp = p+1;
		return (tp->tm_mon + 1);
	}
	if (isdigit((unsigned char)*p)) {
		val = strtol(p, &p, 10);	/* if 0, it's failure */
		for (; FLDCHAR(*p); ++p)
			continue;
		*endp = p;
		return (val);
	}
	for (start = p; *p != '\0' && isalpha((unsigned char)*++p);)
		continue;
	savech = *p;
	*p = '\0';
	if ((val = getmonth(start)) != 0) {
		*flags |= F_ISMONTH;
	} else if ((val = getday(start)) != 0) {
		*flags |= F_ISDAY;
	} else {
		*p = savech;
		return (0);
	}
	for (*p = savech; FLDCHAR(*p); ++p)
		continue;
	*endp = p;
	return (val);
}

static FILE *
opencal(void)
{
	int fd, pdes[2];

	/* open up calendar file as stdin */
	if (!freopen(fname, "rf", stdin)) {
		if (doall)
			return (NULL);
		err(1, "Cannot open `%s'", fname);
	}
	if (pipe(pdes) < 0) {
		warn("Cannot open pipe");
		return (NULL);
	}
	switch (fork()) {
	case -1:			/* error */
		(void)close(pdes[0]);
		(void)close(pdes[1]);
		return (NULL);
	case 0:
		/* child -- stdin already setup, set stdout to pipe input */
		if (pdes[1] != STDOUT_FILENO) {
			(void)dup2(pdes[1], STDOUT_FILENO);
			(void)close(pdes[1]);
		}
		(void)close(pdes[0]);
		(void)execl(_PATH_CPP, "cpp", "-traditional", "-P", "-I.",
		    "-I" _PATH_CALENDARS, NULL);
		err(1, "Cannot exec `%s'", _PATH_CPP);
		/*NOTREACHED*/
	}
	/* parent -- set stdin to pipe output */
	(void)dup2(pdes[0], STDIN_FILENO);
	(void)close(pdes[0]);
	(void)close(pdes[1]);

	/* not reading all calendar files, just set output to stdout */
	if (!doall)
		return (stdout);

	/* set output to a temporary file, so if no output don't send mail */
	(void)snprintf(path, sizeof(path), "%s/_calXXXXXX", _PATH_TMP);
	if ((fd = mkstemp(path)) < 0) {
		warn("Cannot create temporary file");
		return (NULL);
	}
	return (fdopen(fd, "w+"));
}

static void
closecal(fp)
	FILE *fp;
{
	struct stat sbuf;
	int nread, pdes[2], status;
	char buf[1024];

	if (!doall)
		return;

	(void)rewind(fp);
	if (fstat(fileno(fp), &sbuf) || !sbuf.st_size)
		goto done;
	if (pipe(pdes) < 0)
		goto done;
	switch (fork()) {
	case -1:			/* error */
		(void)close(pdes[0]);
		(void)close(pdes[1]);
		goto done;
	case 0:
		/* child -- set stdin to pipe output */
		if (pdes[0] != STDIN_FILENO) {
			(void)dup2(pdes[0], STDIN_FILENO);
			(void)close(pdes[0]);
		}
		(void)close(pdes[1]);
		(void)execl(_PATH_SENDMAIL, "sendmail", "-i", "-t", "-F",
		    "\"Reminder Service\"", "-f", "root", NULL);
		err(1, "Cannot exec `%s'", _PATH_SENDMAIL);
		/*NOTREACHED*/
	}
	/* parent -- write to pipe input */
	(void)close(pdes[0]);

	header[1].iov_base = header[3].iov_base = (void *)pw->pw_name;
	header[1].iov_len = header[3].iov_len = strlen(pw->pw_name);
	writev(pdes[1], header, 7);
	while ((nread = read(fileno(fp), buf, sizeof(buf))) > 0)
		(void)write(pdes[1], buf, nread);
	(void)close(pdes[1]);

done:	(void)fclose(fp);
	(void)unlink(path);
	while (wait(&status) >= 0)
		continue;
}

static int
getmonth(s)
	char *s;
{
	char **p;

	for (p = months; *p; ++p)
		if (!strncasecmp(s, *p, 3))
			return ((p - months) + 1);
	return (0);
}

static int
getday(s)
	char *s;
{
	char **p;

	for (p = days; *p; ++p)
		if (!strncasecmp(s, *p, 3))
			return ((p - days) + 1);
	return (0);
}

static void
atodays(int ch, char *optarg, unsigned short *days)
{
	int u;

	u = atoi(optarg);
	if ((u < 0) || (u > 366)) {
		warnx("-%c %d out of range 0-366, ignored.", ch, u);
	} else {
		*days = u;
	}
}

#define todigit(x) ((x) - '0')
#define ATOI2(x) (todigit((x)[0]) * 10 + todigit((x)[1]))
#define ISDIG2(x) (isdigit((unsigned char)(x)[0]) && isdigit((unsigned char)(x)[1]))

static void
getmmdd(struct tm *tp, char *ds)
{
	int ok = FALSE;
	struct tm ttm;

	ttm = *tp;
	ttm.tm_isdst = -1;

	if (ISDIG2(ds)) {
		ttm.tm_mon = ATOI2(ds) - 1;
		ds += 2;
	}

	if (ISDIG2(ds)) {
		ttm.tm_mday = ATOI2(ds);
		ds += 2;

		ok = TRUE;
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
	
	if (ok && (mktime(&ttm) < 0)) {
		ok = FALSE;
	}
	
	if (ok) {
		*tp = ttm;
	} else {
		warnx("Can't convert `%s' to date, ignored.", ds);
		usage();
	}
}

static void
usage(void)
{
	(void)fprintf(stderr, "Usage: %s [-a] [-d MMDD[[YY]YY]"
	    " [-f fname] [-l days] [-w days]\n", getprogname());
	exit(1);
}
