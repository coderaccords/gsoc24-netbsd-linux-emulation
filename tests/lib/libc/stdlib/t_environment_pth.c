/*	$NetBSD: t_environment_pth.c,v 1.1 2010/11/16 14:03:47 tron Exp $	*/

/*-
 * Copyright (c) 2010 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by 
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
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__RCSID("$NetBSD: t_environment_pth.c,v 1.1 2010/11/16 14:03:47 tron Exp $");

#include <atf-c.h>
#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

ATF_TC(t_threaded);

#define	THREADED_NUM_THREADS	8
#define	THREADED_NUM_VARS	16
#define	THREADED_VAR_NAME	"THREADED%zu"
#define	THREADED_RUN_TIME	10

ATF_TC_HEAD(t_threaded, tc)
{
	char timeout[32];

	atf_tc_set_md_var(tc, "descr",
	    "Test concurrent access to the environment by multiple threads");

	(void)snprintf(timeout, sizeof(timeout), "%d", THREADED_RUN_TIME + 5);
	atf_tc_set_md_var(tc, "timeout", timeout);
}

static void *
thread_getenv_r(void *arg)
{
	time_t endtime;

	endtime = *(time_t *)arg;
	do {
		size_t i;
		char name[32], value[128];

		i = lrand48() % THREADED_NUM_VARS;
		(void)snprintf(name, sizeof(name), THREADED_VAR_NAME, i);

		if (getenv_r(name, value, sizeof(value)) == -1) {
			ATF_CHECK(errno == ENOENT);
		}
	} while (time(NULL) < endtime);

	return NULL;
}

static void *
thread_putenv(void *arg)
{
	time_t endtime;
	size_t i;
	static char vars[THREADED_NUM_VARS][128];

	for (i = 0; i < THREADED_NUM_VARS; i++) {
		(void)snprintf(vars[i], sizeof(vars[i]),
		    THREADED_VAR_NAME "=putenv %ld", i, lrand48());
	}

	endtime = *(time_t *)arg;
	do {
		char name[128];

		i = lrand48() % THREADED_NUM_VARS;
		(void)strlcpy(name, vars[i], sizeof(name));
		*strchr(name, '=') = '\0';

		ATF_CHECK(unsetenv(name) != -1);
		ATF_CHECK(putenv(vars[i]) != -1);
	} while (time(NULL) < endtime);

	return NULL;
}

static void *
thread_setenv(void *arg)
{
	time_t endtime;

	endtime = *(time_t *)arg;
	do {
		size_t i;
		char name[32], value[64];

		i = lrand48() % THREADED_NUM_VARS;
		(void)snprintf(name, sizeof(name), THREADED_VAR_NAME, i);
		(void)snprintf(value, sizeof(value), "setenv %ld", lrand48());

		ATF_CHECK(setenv(name, value, 1) != -1);
	} while (time(NULL) < endtime);

	return NULL;
}

static void *
thread_unsetenv(void *arg)
{
	time_t endtime;

	endtime = *(time_t *)arg;
	do {
		size_t i;
		char name[32];

		i = lrand48() % THREADED_NUM_VARS;
		(void)snprintf(name, sizeof(name), THREADED_VAR_NAME, i);

		ATF_CHECK(unsetenv(name) != -1);
	} while (time(NULL) < endtime);

	return NULL;
}

ATF_TC_BODY(t_threaded, tc)
{
	time_t endtime;
	size_t i, j;
	pthread_t threads[THREADED_NUM_THREADS];

	endtime = time(NULL) + THREADED_RUN_TIME;

	i = 0;
	for (j = 0; j < 2; j++) {
		ATF_CHECK(pthread_create(&threads[i++], NULL, thread_getenv_r,
		    &endtime) == 0);
		ATF_CHECK(pthread_create(&threads[i++], NULL, thread_putenv,
		    &endtime) == 0);
		ATF_CHECK(pthread_create(&threads[i++], NULL, thread_setenv,
		    &endtime) == 0);
		ATF_CHECK(pthread_create(&threads[i++], NULL, thread_unsetenv,
		    &endtime) == 0);
	}

	for (j = 0; j < i; j++)
		ATF_CHECK(pthread_join(threads[j], NULL) == 0);
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, t_threaded);

	return atf_no_error();
}
