/*	$NetBSD: want.c,v 1.5 2005/03/04 17:11:19 christos Exp $	*/

/*
 * Copyright (c) 1987, 1993, 1994
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
static struct utmp *buf;

static void onintr(int);
static int want(struct utmp *, int);
static const char *gethost(struct utmp *, const char *, int);

static const char *
/*ARGSUSED*/
gethost(struct utmp *ut, const char *host, int numeric)
{
#if FIRSTVALID == 0
	return numeric ? "" : host;
#else
	if (numeric) {
		static char buf[512];
		buf[0] = '\0';
		(void)sockaddr_snprintf(buf, sizeof(buf), "%a",
		    (struct sockaddr *)&ut->ut_ss);
		return buf;
	} else
		return host;
#endif
}

#define NULTERM(what) \
	if (check ## what) \
		(void)strlcpy(what ## p = what ## buf, bp->ut_ ## what, \
		    sizeof(what ## buf)); \
	else \
		what ## p = bp->ut_ ## what
	
/*
 * wtmp --
 *	read through the wtmp file
 */
void
wtmp(const char *file, int namesz, int linesz, int hostsz, int numeric)
{
	struct utmp	*bp;		/* current structure */
	TTY	*T;			/* tty list entry */
	struct stat	stb;		/* stat of file for sz */
	time_t	delta;			/* time difference */
	off_t	bl;
	int	bytes, wfd;
	char	*ct, *crmsg;
	size_t  len = sizeof(*buf) * MAXUTMP;
	char namebuf[sizeof(bp->ut_name) + 1], *namep;
	char linebuf[sizeof(bp->ut_line) + 1], *linep;
	char hostbuf[sizeof(bp->ut_host) + 1], *hostp;
	int checkname = namesz > sizeof(bp->ut_name);
	int checkline = linesz > sizeof(bp->ut_line);
	int checkhost = hostsz > sizeof(bp->ut_host);

	if ((buf = malloc(len)) == NULL)
		err(1, "Cannot allocate utmp buffer");

	crmsg = NULL;

	if ((wfd = open(file, O_RDONLY, 0)) < 0 || fstat(wfd, &stb) == -1)
		err(1, "%s", file);
	bl = (stb.st_size + len - 1) / len;

	buf[FIRSTVALID].ut_timefld = time(NULL);
	(void)signal(SIGINT, onintr);
	(void)signal(SIGQUIT, onintr);

	while (--bl >= 0) {
		if (lseek(wfd, bl * len, SEEK_SET) == -1 ||
		    (bytes = read(wfd, buf, len)) == -1)
			err(1, "%s", file);
		for (bp = &buf[bytes / sizeof(*buf) - 1]; bp >= buf; --bp) {
			NULTERM(name);
			NULTERM(line);
			NULTERM(host);
			/*
			 * if the terminal line is '~', the machine stopped.
			 * see utmp(5) for more info.
			 */
			if (linep[0] == '~' && !linep[1]) {
				/* everybody just logged out */
				for (T = ttylist; T; T = T->next)
					T->logout = -bp->ut_timefld;
				currentout = -bp->ut_timefld;
				crmsg = strncmp(namep, "shutdown",
				    namesz) ? "crash" : "shutdown";
				if (want(bp, NO)) {
					ct = fmttime(bp->ut_timefld, fulltime);
					printf("%-*.*s  %-*.*s %-*.*s %s\n",
					    namesz, namesz, namep,
					    linesz, linesz, linep,
					    hostsz, hostsz,
					    gethost(bp, hostp, numeric), ct);
					if (maxrec != -1 && !--maxrec)
						return;
				}
				continue;
			}
			/*
			 * if the line is '{' or '|', date got set; see
			 * utmp(5) for more info.
			 */
			if ((linep[0] == '{' || linep[0] == '|') && !linep[1]) {
				if (want(bp, NO)) {
					ct = fmttime(bp->ut_timefld, fulltime);
				printf("%-*.*s  %-*.*s %-*.*s %s\n",
				    namesz, namesz, namep,
				    linesz, linesz, linep,
				    hostsz, hostsz,
				    gethost(bp, hostp, numeric),
				    ct);
					if (maxrec && !--maxrec)
						return;
				}
				continue;
			}
			/* find associated tty */
			for (T = ttylist;; T = T->next) {
				if (!T) {
					/* add new one */
					T = addtty(linep);
					break;
				}
				if (!strncmp(T->tty, linep, LINESIZE))
					break;
			}
			if (TYPE(bp) == SIGNATURE)
				continue;
			if (namep[0] && want(bp, YES)) {
				ct = fmttime(bp->ut_timefld, fulltime);
				printf("%-*.*s  %-*.*s %-*.*s %s ",
				    namesz, namesz, namep,
				    linesz, linesz, linep,
				    hostsz, hostsz,
				    gethost(bp, hostp, numeric),
				    ct);
				if (!T->logout)
					puts("  still logged in");
				else {
					if (T->logout < 0) {
						T->logout = -T->logout;
						printf("- %s", crmsg);
					}
					else
						printf("- %s",
						    fmttime(T->logout,
						    fulltime | TIMEONLY));
					delta = T->logout - bp->ut_timefld;
					if (delta < SECSPERDAY)
						printf("  (%s)\n",
						    fmttime(delta,
						    fulltime | TIMEONLY | GMT));
					else
						printf(" (%ld+%s)\n",
						    delta / SECSPERDAY,
						    fmttime(delta,
						    fulltime | TIMEONLY | GMT));
				}
				if (maxrec != -1 && !--maxrec)
					return;
			}
			T->logout = bp->ut_timefld;
		}
	}
	fulltime = 1;	/* show full time */
	crmsg = fmttime(buf[FIRSTVALID].ut_timefld, FULLTIME);
	if ((ct = strrchr(file, '/')) != NULL)
		ct++;
	printf("\n%s begins %s\n", ct ? ct : file, crmsg);
}

/*
 * want --
 *	see if want this entry
 */
static int
want(struct utmp *bp, int check)
{
	ARG *step;

	if (check) {
		/*
		 * when uucp and ftp log in over a network, the entry in
		 * the utmp file is the name plus their process id.  See
		 * etc/ftpd.c and usr.bin/uucp/uucpd.c for more information.
		 */
		if (!strncmp(bp->ut_line, "ftp", sizeof("ftp") - 1))
			bp->ut_line[3] = '\0';
		else if (!strncmp(bp->ut_line, "uucp", sizeof("uucp") - 1))
			bp->ut_line[4] = '\0';
	}
	if (!arglist)
		return (YES);

	for (step = arglist; step; step = step->next)
		switch(step->type) {
		case HOST_TYPE:
			if (!strncasecmp(step->name, bp->ut_host, HOSTSIZE))
				return (YES);
			break;
		case TTY_TYPE:
			if (!strncmp(step->name, bp->ut_line, LINESIZE))
				return (YES);
			break;
		case USER_TYPE:
			if (!strncmp(step->name, bp->ut_name, NAMESIZE))
				return (YES);
			break;
	}
	return (NO);
}

/*
 * onintr --
 *	on interrupt, we inform the user how far we've gotten
 */
static void
onintr(int signo)
{

	printf("\ninterrupted %s\n", fmttime(buf[FIRSTVALID].ut_timefld,
	    FULLTIME));
	if (signo == SIGINT)
		exit(1);
	(void)fflush(stdout);		/* fix required for rsh */
}
