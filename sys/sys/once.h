/*	$NetBSD: once.h,v 1.1 2005/10/29 11:13:21 yamt Exp $	*/

/*-
 * Copyright (c)2005 YAMAMOTO Takashi,
 * All rights reserved.
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

#if !defined(_SYS_ONCE_H_)
#define	_SYS_ONCE_H_

#include <sys/lock.h>

typedef struct {
	struct simplelock o_lock;
	int o_flags;
#define	ONCE_RUNNING	1
#define	ONCE_DONE	2
} once_t;

void _run_once(once_t *, void (*)(void));

#define	ONCE_DECL(o) \
	once_t (o) = { \
		.o_lock = SIMPLELOCK_INITIALIZER, \
		.o_flags = 0, \
	};

#define	RUN_ONCE(o, fn) \
	do { \
		if (__predict_false(((o)->o_flags & ONCE_DONE) == 0)) { \
			_run_once((o), (fn)); \
		} \
	} while (0 /* CONSTCOND */)

#endif /* !defined(_SYS_ONCE_H_) */
