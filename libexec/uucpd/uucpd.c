/*	$NetBSD: uucpd.c,v 1.12 1998/07/03 04:10:43 mrg Exp $	*/

/*
 * Copyright (c) 1985 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Rick Adams.
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
__COPYRIGHT("@(#) Copyright (c) 1985 The Regents of the University of California.\n\
 All rights reserved.\n");
#if 0
static char sccsid[] = "from: @(#)uucpd.c	5.10 (Berkeley) 2/26/91";
#else
__RCSID("$NetBSD: uucpd.c,v 1.12 1998/07/03 04:10:43 mrg Exp $");
#endif
#endif /* not lint */

/*
 * 4.2BSD TCP/IP server for uucico
 * uucico's TCP channel causes this server to be run at the remote end.
 */

#include <sys/types.h>
#include <sys/wait.h>
#include <sys/ioctl.h>
#include <sys/socket.h>

#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <signal.h>
#include <fcntl.h>
#include <time.h>
#include <pwd.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <utmp.h>
#include <fcntl.h>

#include "pathnames.h"

struct	sockaddr_in hisctladdr;
int hisaddrlen = sizeof hisctladdr;
struct	sockaddr_in myctladdr;
int mypid;

char Username[64];
char *nenv[] = {
	Username,
	NULL,
};

extern char **environ;

void dologout __P((void));
void dologin __P((struct passwd *, struct sockaddr_in *));
void doit __P((struct sockaddr_in *));
int readline __P((char *, int));
int main __P((int, char **));

int
main(argc, argv)
	int argc;
	char **argv;
{
#ifndef BSDINETD
	register int s, tcp_socket;
	struct servent *sp;
#endif /* !BSDINETD */
	extern int errno;

	environ = nenv;
#ifdef BSDINETD
	close(1); close(2);
	dup(0); dup(0);
	hisaddrlen = sizeof (hisctladdr);
	if (getpeername(0, (struct sockaddr *)&hisctladdr, &hisaddrlen) < 0) {
		fprintf(stderr, "%s: ", argv[0]);
		perror("getpeername");
		_exit(1);
	}
	if (fork() == 0)
		doit(&hisctladdr);
	dologout();
	exit(1);
#else /* !BSDINETD */
	sp = getservbyname("uucp", "tcp");
	if (sp == NULL){
		perror("uucpd: getservbyname");
		exit(1);
	}
	if (fork())
		exit(0);
	if ((s=open(_PATH_TTY, 2)) >= 0){
		ioctl(s, TIOCNOTTY, (char *)0);
		close(s);
	}

	bzero((char *)&myctladdr, sizeof (myctladdr));
	myctladdr.sin_len = sizeof(struct sockaddr_in);
	myctladdr.sin_family = AF_INET;
	myctladdr.sin_port = sp->s_port;
	tcp_socket = socket(AF_INET, SOCK_STREAM, 0);
	if (tcp_socket < 0) {
		perror("uucpd: socket");
		exit(1);
	}
	if (bind(tcp_socket, (char *)&myctladdr, sizeof (myctladdr)) < 0) {
		perror("uucpd: bind");
		exit(1);
	}
	listen(tcp_socket, 3);	/* at most 3 simultaneuos uucp connections */
	signal(SIGCHLD, dologout);

	for(;;) {
		s = accept(tcp_socket, &hisctladdr, &hisaddrlen);
		if (s < 0){
			if (errno == EINTR) 
				continue;
			perror("uucpd: accept");
			exit(1);
		}
		if (fork() == 0) {
			close(0); close(1); close(2);
			dup(s); dup(s); dup(s);
			close(tcp_socket); close(s);
			doit(&hisctladdr);
			exit(1);
		}
		close(s);
	}
#endif	/* !BSDINETD */
}

void
doit(sinp)
	struct sockaddr_in *sinp;
{
	char user[64], passwd[64];
	char *xpasswd = NULL;	/* XXX gcc */
	struct passwd *pw;

	alarm(60);
	do {
		printf("login: ");
		fflush(stdout);
		if (readline(user, sizeof user) < 0) {
			fprintf(stderr, "user read\n");
			return;
		}
	} while (user[0] == '\0');
	/* truncate username to 8 characters */
	user[8] = '\0';
	pw = getpwnam(user);
	if (pw == NULL || (pw->pw_passwd && *pw->pw_passwd != '\0')) {
		printf("Password: ");
		fflush(stdout);
		if (readline(passwd, sizeof passwd) < 0) {
			fprintf(stderr, "passwd read\n");
			return;
		}
		if (pw != NULL)
			xpasswd = crypt(passwd, pw->pw_passwd);

		if (pw == NULL || strcmp(xpasswd, pw->pw_passwd)) {
			fprintf(stderr, "Login incorrect.\n");
			return;
		}
	} else
		(void)crypt("dummy password", "PA");	/* must always crypt */
	if (strcmp(pw->pw_shell, _PATH_UUCICO)) {
		fprintf(stderr, "Login incorrect.\n");
		return;
	}
	alarm(0);
	sprintf(Username, "USER=%s", user);
	dologin(pw, sinp);
	setgid(pw->pw_gid);
	initgroups(pw->pw_name, pw->pw_gid);
	chdir(pw->pw_dir);
	setlogin(user);
	setuid(pw->pw_uid);
	execl(_PATH_UUCICO, "uucico", (char *)0);
	perror("uucico server: execl");
}

int
readline(p, n)
	char *p;
	int n;
{
	char c;

	while (n-- > 0) {
		if (read(0, &c, 1) <= 0)
			return(-1);
		c &= 0177;
		if (c == '\r') {
			*p = '\0';
			return(0);
		}
		if (c != '\n')
			*p++ = c;
	}
	return(-1);
}

/* Note that SCPYN is only used on strings that may not be nul terminated */
#define	SCPYN(a, b)	strncpy(a, b, sizeof (a))

struct	utmp utmp;

void
dologout()
{
	union wait status;
	int pid, wtmp;

#ifdef BSDINETD
	while ((pid=wait((int *)&status)) > 0) {
#else  /* !BSDINETD */
	while ((pid=wait3((int *)&status,WNOHANG,0)) > 0) {
#endif /* !BSDINETD */
		wtmp = open(_PATH_WTMP, O_WRONLY|O_APPEND);
		if (wtmp >= 0) {
			sprintf(utmp.ut_line, "uucp%.4d", pid);
			SCPYN(utmp.ut_name, "");
			SCPYN(utmp.ut_host, "");
			(void) time(&utmp.ut_time);
			(void) write(wtmp, (char *)&utmp, sizeof (utmp));
			(void) close(wtmp);
		}
	}
}

/*
 * Record login in wtmp file.
 */
void
dologin(pw, sin)
	struct passwd *pw;
	struct sockaddr_in *sin;
{
	char line[UT_LINESIZE+1];
	char remotehost[UT_HOSTSIZE+1];
	int wtmp, f;
	struct hostent *hp = gethostbyaddr((char *)&sin->sin_addr,
		sizeof (struct in_addr), AF_INET);

	if (hp) {
		strncpy(remotehost, hp->h_name, sizeof (remotehost));
		endhostent();
	} else
		strncpy(remotehost, inet_ntoa(sin->sin_addr),
		    sizeof (remotehost));
	wtmp = open(_PATH_WTMP, O_WRONLY|O_APPEND);
	if (wtmp >= 0) {
		/* hack, but must be unique and no tty line */
		sprintf(line, "uucp%.4d", getpid());
		SCPYN(utmp.ut_line, line);
		SCPYN(utmp.ut_name, pw->pw_name);
		SCPYN(utmp.ut_host, remotehost);
		time(&utmp.ut_time);
		(void) write(wtmp, (char *)&utmp, sizeof (utmp));
		(void) close(wtmp);
	}
	if ((f = open(_PATH_LASTLOG, O_RDWR)) >= 0) {
		struct lastlog ll;

		time(&ll.ll_time);
		lseek(f, pw->pw_uid * sizeof(struct lastlog), 0);
		SCPYN(ll.ll_line, remotehost);
		SCPYN(ll.ll_host, remotehost);
		(void) write(f, (char *) &ll, sizeof ll);
		(void) close(f);
	}
}
