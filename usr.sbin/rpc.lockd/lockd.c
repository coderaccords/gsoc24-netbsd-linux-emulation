/*	$NetBSD: lockd.c,v 1.9 2007/11/04 23:12:50 christos Exp $	*/

/*
 * Copyright (c) 1995
 *	A.R. Gordon (andrew.gordon@net-tel.co.uk).  All rights reserved.
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
 *	This product includes software developed for the FreeBSD project
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY ANDREW GORDON AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#include <sys/cdefs.h>
#ifndef lint
__RCSID("$NetBSD: lockd.c,v 1.9 2007/11/04 23:12:50 christos Exp $");
#endif

/*
 * main() function for NFS lock daemon.  Most of the code in this
 * file was generated by running rpcgen /usr/include/rpcsvc/nlm_prot.x.
 *
 * The actual program logic is in the file lock_proc.c
 */

#include <sys/types.h>
#include <sys/socket.h>

#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <syslog.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include <util.h>
#include <netconfig.h>

#include <rpc/rpc.h>
#include <rpcsvc/sm_inter.h>

#include "lockd.h"
#include <rpcsvc/nlm_prot.h>

int		debug_level = 0;	/* 0 = no debugging syslog() calls */
int		_rpcsvcdirty = 0;

int grace_expired;

void	nlm_prog_0(struct svc_req *, SVCXPRT *);
void	nlm_prog_1(struct svc_req *, SVCXPRT *);
void	nlm_prog_3(struct svc_req *, SVCXPRT *);
void	nlm_prog_4(struct svc_req *, SVCXPRT *);
static void	usage(void) __attribute__((__noreturn__));

static void sigalarm_handler(int);

static const char *transports[] = { "udp", "tcp", "udp6", "tcp6" };

int
main(int argc, char **argv)
{
	SVCXPRT *transp;
	int ch, i, maxindex, s;
	struct sigaction sigchild, sigalarm;
	int grace_period = 30;
	struct netconfig *nconf;
	int maxrec = RPC_MAXDATASIZE;

	(void)setprogname(*argv);
	while ((ch = getopt(argc, argv, "d:g:")) != (-1)) {
		switch (ch) {
		case 'd':
			debug_level = atoi(optarg);
			if (!debug_level) {
				usage();
				/* NOTREACHED */
			}
			break;
		case 'g':
			grace_period = atoi(optarg);
			if (!grace_period) {
				usage();
				/* NOTREACHED */
			}
			break;
		default:
		case '?':
			usage();
			/* NOTREACHED */
		}
	}

	(void)rpcb_unset(NLM_PROG, NLM_SM, NULL);
	(void)rpcb_unset(NLM_PROG, NLM_VERS, NULL);
	(void)rpcb_unset(NLM_PROG, NLM_VERSX, NULL);
	(void)rpcb_unset(NLM_PROG, NLM_VERS4, NULL);

	/*
	 * Check if IPv6 support is present.
	 */
	s = socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP);
	if (s < 0)
		maxindex = 2;
	else {
		(void)close(s);
		maxindex = 4;
	}

	(void)rpc_control(RPC_SVC_CONNMAXREC_SET, &maxrec);

	for (i = 0; i < maxindex; i++) {
		nconf = getnetconfigent(transports[i]);
		if (nconf == NULL)
			errx(1, "cannot get udp netconf.");

		transp = svc_tli_create(RPC_ANYFD, nconf, NULL, RPC_MAXDATASIZE,
		    RPC_MAXDATASIZE);
		if (transp == NULL) {
			errx(1, "cannot create %s service.", transports[i]);
			/* NOTREACHED */
		}
		if (!svc_reg(transp, NLM_PROG, NLM_SM, nlm_prog_0, nconf)) {
			errx(1, "unable to register (NLM_PROG, NLM_SM, %s)",
			    transports[i]);
			/* NOTREACHED */
		}
		if (!svc_reg(transp, NLM_PROG, NLM_VERS, nlm_prog_1, nconf)) {
			errx(1, "unable to register (NLM_PROG, NLM_VERS, %s)",
			    transports[i]);
			/* NOTREACHED */
		}
		if (!svc_reg(transp, NLM_PROG, NLM_VERSX, nlm_prog_3, nconf)) {
			errx(1, "unable to register (NLM_PROG, NLM_VERSX, %s)",
			    transports[i]);
			/* NOTREACHED */
		}
		if (!svc_reg(transp, NLM_PROG, NLM_VERS4, nlm_prog_4, nconf)) {
			errx(1, "unable to register (NLM_PROG, NLM_VERS4, %s)",
			    transports[i]);
			/* NOTREACHED */
		}
		freenetconfigent(nconf);
	}

	/*
	 * Note that it is NOT sensible to run this program from inetd - the
	 * protocol assumes that it will run immediately at boot time.
	 */
	if (daemon(0, 0) == -1) {
		err(1, "cannot fork");
		/* NOTREACHED */
	}
	(void)pidfile(NULL);

	openlog("rpc.lockd", 0, LOG_DAEMON);
	if (debug_level)
		syslog(LOG_INFO, "Starting, debug level %d", debug_level);
	else
		syslog(LOG_INFO, "Starting");

	sigchild.sa_handler = sigchild_handler;
	(void)sigemptyset(&sigchild.sa_mask);
	sigchild.sa_flags = SA_RESTART;
	if (sigaction(SIGCHLD, &sigchild, NULL) != 0) {
		syslog(LOG_WARNING, "sigaction(SIGCHLD) failed (%m)");
		exit(1);
	}
	sigalarm.sa_handler = sigalarm_handler;
	(void)sigemptyset(&sigalarm.sa_mask);
	sigalarm.sa_flags = SA_RESETHAND; /* should only happen once */
	sigalarm.sa_flags |= SA_RESTART;
	if (sigaction(SIGALRM, &sigalarm, NULL) != 0) {
		syslog(LOG_WARNING, "sigaction(SIGALRM) failed (%m)");
		exit(1);
	}
	grace_expired = 0;
	if (alarm(10) == (unsigned int)-1) {
		syslog(LOG_WARNING, "alarm failed (%m)");
		exit(1);
	}

	svc_run();		/* Should never return */
	return 1;
}

static void
/*ARGSUSED*/
sigalarm_handler(int s)
{
	grace_expired = 1;
}

static void
usage(void)
{
	(void)fprintf(stderr, "Usage: %s[-d <debuglevel>] [-g <grace period>]",
	    getprogname());
	exit(1);
}
