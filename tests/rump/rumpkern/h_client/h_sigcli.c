/*	$NetBSD: h_sigcli.c,v 1.1 2011/01/06 07:00:28 pooka Exp $	*/

#include <sys/types.h>
#include <sys/sysctl.h>

#include <err.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <rump/rump_syscalls.h>
#include <rump/rumpclient.h>

static const int hostnamemib[] = { CTL_KERN, KERN_HOSTNAME };
static char hostnamebuf[128];

static void
sighand(int sig)
{
	char buf[128];
	size_t blen = sizeof(buf);

	if (rump_sys___sysctl(hostnamemib, __arraycount(hostnamemib),
	    buf, &blen, NULL, 0) == -1)
		err(1, "sighand sysctl");
	if (strcmp(buf, hostnamebuf) != 0)
		errx(1, "sighandler hostname");
}

int
main(void)
{
	char buf[128];
	struct itimerval itv;
	size_t hnbsize;
	int i;
	size_t blen;

	if (rumpclient_init() == -1)
		err(1, "rumpclient init");

	hnbsize = sizeof(hostnamebuf);
	if (rump_sys___sysctl(hostnamemib, __arraycount(hostnamemib),
	    hostnamebuf, &hnbsize, NULL, 0) == -1)
		err(1, "sysctl");

	if (signal(SIGALRM, sighand) == SIG_ERR)
		err(1, "signal");

	itv.it_interval.tv_sec = itv.it_value.tv_sec = 0;
	itv.it_interval.tv_usec = itv.it_value.tv_usec = 10000; /* 10ms */

	if (setitimer(ITIMER_REAL, &itv, NULL) == -1)
		err(1, "itimer");

	for (i = 0; i < 20000; i++) {
		blen = sizeof(buf);
		if (rump_sys___sysctl(hostnamemib, __arraycount(hostnamemib),
		    buf, &blen, NULL, 0) == -1)
			err(1, "sysctl");
		if (strcmp(buf, hostnamebuf) != 0)
			errx(1, "main hostname");
	}
}
