/* $Id: saslc_private.h,v 1.2 2011/01/29 23:35:31 agc Exp $ */

/* Copyright (c) 2010 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Mateusz Kocielski.
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
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.	IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _SASLC_PRIVATE_H_
#define _SASLC_PRIVATE_H_

#include <saslc.h>
#include <sys/queue.h>
#include "dict.h"
#include "mech.h"
#include "error.h"

typedef struct saslc__sess_list_t saslc__sess_list_t;
LIST_HEAD(saslc__sess_list_t, saslc_sess_t);

/** library context structure */
struct saslc_t {
	char *appname;                  /**< application name */
	saslc__dict_t *prop;            /**< configuration options */
	saslc__mech_list_t mechanisms;  /**< available mechanisms */
	saslc__error_t err;             /**< error */
        saslc__sess_list_t sessions;    /**< sessions */
};

/** session context structure */
struct saslc_sess_t {
	saslc_t *context;               /**< library context */
	const saslc__mech_t *mech;      /**< mechanism */
	void *mech_sess;                /**< mechanism session */
	saslc__dict_t *prop;            /**< session properties */
	saslc__error_t err;             /**< error */
        LIST_ENTRY(saslc_sess_t) nodes; /**< nodes */
};

#endif /* ! _SASLC_PRIVATE_H_ */
