/*	$NetBSD: rumpclient.h,v 1.6 2011/02/14 14:56:23 pooka Exp $	*/

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

#ifndef _RUMP_RUMPCLIENT_H_
#define _RUMP_RUMPCLIENT_H_

#include <sys/types.h>

__BEGIN_DECLS

int rumpclient_syscall(int, const void *, size_t, register_t *);
int rumpclient_init(void);

struct rumpclient_fork;
struct rumpclient_fork *rumpclient_prefork(void);
int			rumpclient_fork_init(struct rumpclient_fork *);

#define RUMPCLIENT_RETRYCONN_INFTIME ((time_t)-1)
#define RUMPCLIENT_RETRYCONN_ONCE ((time_t)-2)
#define RUMPCLIENT_RETRYCONN_DIE ((time_t)-3)
void rumpclient_setconnretry(time_t);

enum rumpclient_closevariant {
	RUMPCLIENT_CLOSE_CLOSE,
	RUMPCLIENT_CLOSE_DUP2,
	RUMPCLIENT_CLOSE_FCLOSEM
};
int rumpclient__closenotify(int *, enum rumpclient_closevariant);
int rumpclient__exec_augmentenv(char *const[], char *const[], char ***);

__END_DECLS

#endif /* _RUMP_RUMPCLIENT_H_ */
