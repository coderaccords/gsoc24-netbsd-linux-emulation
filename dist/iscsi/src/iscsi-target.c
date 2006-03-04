/* $NetBSD: iscsi-target.c,v 1.5 2006/03/04 21:56:11 agc Exp $ */

/*
 * Copyright � 2006 Alistair Crooks.  All rights reserved.
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
 *	This product includes software developed by Alistair Crooks
 *	for the NetBSD project.
 * 4. The name of the author may not be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include "config.h"

#define EXTERN

#ifdef HAVE_SIGNAL_H
#include <signal.h>
#endif
 
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
    
#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include <unistd.h>

#include "iscsi.h"
#include "util.h"
#include "target.h"
#include "device.h"

#include "conffile.h"
#include "storage.h"

static int      g_main_pid;

static globals_t	g;

int
main(int argc, char **argv)
{
	const char	*cf;
	targv_t		tv;
	devv_t		dv;
	extv_t		ev;
	char            TargetName[1024];
	int		detach_me_harder;
	int             i;

	(void) memset(&g, 0x0, sizeof(g));
	(void) memset(&tv, 0x0, sizeof(tv));
	(void) memset(&dv, 0x0, sizeof(dv));
	(void) memset(&ev, 0x0, sizeof(ev));

	/* set defaults */
	(void) strlcpy(TargetName, DEFAULT_TARGET_NAME, sizeof(TargetName));
	detach_me_harder = 1;
	g.port = ISCSI_PORT;

	cf = _PATH_ISCSI_TARGETS;

	while ((i = getopt(argc, argv, "DVb:f:p:t:v:")) != -1) {
		switch (i) {
		case 'D':
			detach_me_harder = 0;
			break;
		case 'V':
			(void) printf("\"%s\" %s\nPlease send all bug reports to %s\n", PACKAGE_NAME, PACKAGE_VERSION, PACKAGE_BUGREPORT);
			exit(EXIT_SUCCESS);
			/* NOTREACHED */
		case 'b':
			device_set_var("blocklen", optarg);
			break;
		case 'f':
			cf = optarg;
			break;
		case 'p':
			g.port = (uint16_t) atoi(optarg);
			break;
		case 't':
			(void) strlcpy(TargetName, optarg, sizeof(TargetName));
			break;
		case 'v':
			if (strcmp(optarg, "net") == 0) {
				set_debug("net");
			} else if (strcmp(optarg, "iscsi") == 0) {
				set_debug("iscsi");
			} else if (strcmp(optarg, "scsi") == 0) {
				set_debug("scsi");
			} else if (strcmp(optarg, "all") == 0) {
				set_debug("all");
			}
			break;
		}
	}

	if (!read_conf_file(cf, &tv, &dv, &ev)) {
		(void) fprintf(stderr, "Error: can't open `%s'\n", cf);
		return EXIT_FAILURE;
	}

	(void) signal(SIGPIPE, SIG_IGN);

	g_main_pid = ISCSI_GETPID;

	if (tv.c == 0) {
		(void) fprintf(stderr, "No targets to initialise\n");
		return EXIT_FAILURE;
	}
	/* Initialize target */
	if (target_init(&g, &tv, TargetName) != 0) {
		TRACE_ERROR("target_init() failed\n");
		exit(EXIT_FAILURE);
	}

#ifdef HAVE_DAEMON
	/* if we are supposed to be a daemon, detach from controlling tty */
	if (detach_me_harder && daemon(0, 0) < 0) {
		TRACE_ERROR("daemon() failed\n");
		exit(EXIT_FAILURE);
	}
#endif

	/* write pid to a file */
	write_pid_file(_PATH_ISCSI_PID_FILE);
	
	/* Wait for connections */
	if (target_listen(&g) != 0) {
		TRACE_ERROR("target_listen() failed\n");
	}

	return EXIT_SUCCESS;
}
