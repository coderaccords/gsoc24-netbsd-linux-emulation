/* 
 * dhcpcd - DHCP client daemon
 * Copyright 2006-2008 Roy Marples <roy@marples.name>
 * All rights reserved

 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
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
 */

#include <sys/types.h>
#include <sys/socket.h>

#include <errno.h>
#include <poll.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>

#include "common.h"
#include "signals.h"

static int signal_pipe[2];
static int signals[5];

static const int handle_sigs[] = {
	SIGHUP,
	SIGALRM,
	SIGTERM,
	SIGINT
};

static void
signal_handler(int sig)
{
	unsigned int i = 0;
	int serrno = errno;

	/* Add a signal to our stack */
	while (signals[i])
		i++;
	if (i <= sizeof(signals) / sizeof(signals[0]))
		signals[i] = sig;

	write(signal_pipe[1], &sig, sizeof(sig));

	/* Restore errno */
	errno = serrno;
}

int
signal_fd(void)
{
	return (signal_pipe[0]);
}

/* Check if we have a signal or not */
int
signal_exists(const struct pollfd *fd)
{
	if (signals[0] || (fd && fd->revents & POLLIN))
		return 0;
	return -1;
}

/* Read a signal from the signal pipe. Returns 0 if there is
 * no signal, -1 on error (and sets errno appropriately), and
 * your signal on success */
int
signal_read(struct pollfd *fd)
{
	int sig = -1;
	unsigned int i = 0;
	char buf[16];
	size_t bytes;

	/* Pop a signal off the our stack */
	if (signals[0]) {
		sig = signals[0];
		while (i < (sizeof(signals) / sizeof(signals[0])) - 1) {
			signals[i] = signals[i + 1];
			if (!signals[++i])
				break;
		}
	}

	if (fd && fd->revents & POLLIN) {
		memset(buf, 0, sizeof(buf));
		bytes = read(signal_pipe[0], buf, sizeof(buf));

		if (bytes >= sizeof(sig))
			memcpy(&sig, buf, sizeof(sig));

		/* We need to clear us from rset if nothing left in the buffer
		 * in case we are called many times */
		if (bytes == sizeof(sig))
			fd->revents = 0;
	}

	return sig;
}

/* Call this before doing anything else. Sets up the socket pair
 * and installs the signal handler */
int
signal_init(void)
{
	if (pipe(signal_pipe) == -1)
		return -1;

	/* Stop any scripts from inheriting us */
	close_on_exec(signal_pipe[0]);
	close_on_exec(signal_pipe[1]);

	memset(signals, 0, sizeof(signals));
	return 0;
}

static int
signal_handle(void (*func)(int), int restore)
{
	unsigned int i;
	struct sigaction sa, sa_old;

	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = func;
	sigemptyset(&sa.sa_mask);

	for (i = 0; i < sizeof(handle_sigs) / sizeof(handle_sigs[0]); i++)
		if (sigaction(handle_sigs[i], &sa, &sa_old) == -1)
			return -1;
		if (restore && sigaction(handle_sigs[i], &sa_old, NULL) == -1)
				return -1;
	return 0;
}

int
signal_setup(void)
{
	return signal_handle(signal_handler, 0);
}

int
signal_reset(void)
{
	return signal_handle(SIG_DFL, 0);
}

int
signal_clear(void) 
{
	return signal_handle(SIG_IGN, 1);
}
