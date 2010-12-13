/*	$NetBSD: rump_allserver.c,v 1.8 2010/12/13 14:26:22 pooka Exp $	*/

/*-
 * Copyright (c) 2010 Antti Kantee.  All Rights Reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
#ifndef lint
__RCSID("$NetBSD: rump_allserver.c,v 1.8 2010/12/13 14:26:22 pooka Exp $");
#endif /* !lint */

#include <sys/types.h>
#include <sys/signal.h>
#include <sys/module.h>

#include <rump/rump.h>
#include <rump/rump_syscalls.h>

#include <dlfcn.h>
#include <err.h>
#include <errno.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void
usage(void)
{

	fprintf(stderr, "usage: %s [args] bindurl\n", getprogname());
	exit(1);
}

static void
die(int sflag, int error, const char *reason)
{

	warnx("%s: %s", reason, strerror(error));
	if (!sflag)
		rump_daemonize_done(error);
	exit(1);
}

static sem_t sigsem;
static void
sigreboot(int sig)
{

	sem_post(&sigsem);
}

int
main(int argc, char *argv[])
{
	const char *serverurl;
	char **modarray = NULL;
	unsigned nmods = 0, curmod = 0, i;
	int error;
	int ch, sflag;

	setprogname(argv[0]);

	sflag = 0;
	while ((ch = getopt(argc, argv, "l:m:s")) != -1) {
		switch (ch) {
		case 'l':
			if (dlopen(optarg, RTLD_LAZY|RTLD_GLOBAL) == NULL) {
				char pb[MAXPATHLEN];
				/* try to mimic linker -l syntax */

				snprintf(pb, sizeof(pb), "lib%s.so", optarg);
				if (dlopen(pb, RTLD_LAZY|RTLD_GLOBAL) == NULL) {
					errx(1, "dlopen %s failed: %s",
					    pb, dlerror());
				}
			}
			break;
		case 'm':
			if (nmods - curmod == 0) {
				modarray = realloc(modarray,
				    (nmods+16) * sizeof(char *));
				if (modarray == NULL)
					err(1, "realloc");
				nmods += 16;
			}
			modarray[curmod++] = optarg;
			break;
		case 's':
			sflag = 1;
			break;
		default:
			usage();
			/*NOTREACHED*/
		}
	}

	argc -= optind;
	argv += optind;

	if (argc != 1)
		usage();

	serverurl = argv[0];

	if (!sflag) {
		error = rump_daemonize_begin();
		if (error)
			errx(1, "rump daemonize: %s", strerror(error));
	}

	error = rump_init();
	if (error)
		die(sflag, error, "rump init failed");

	for (i = 0; i < curmod; i++) {
		struct modctl_load ml;

#define ETFSKEY "/module.mod"
		if ((error = rump_pub_etfs_register(ETFSKEY,
		    modarray[0], RUMP_ETFS_REG)) != 0)
			die(sflag, error, "module etfs register failed");
		memset(&ml, 0, sizeof(ml));
		ml.ml_filename = ETFSKEY;
		if (rump_sys_modctl(MODCTL_LOAD, &ml) == -1)
			die(sflag, errno, "module load failed");
		rump_pub_etfs_remove(ETFSKEY);
	}

	error = rump_init_server(serverurl);
	if (error)
		die(sflag, error, "rump server init failed");

	if (!sflag)
		rump_daemonize_done(RUMP_DAEMONIZE_SUCCESS);

	sem_init(&sigsem, 0, 0);
	signal(SIGTERM, sigreboot);
	signal(SIGINT, sigreboot);
	sem_wait(&sigsem);

	rump_sys_reboot(0, NULL);
	/*NOTREACHED*/

	return 0;
}
