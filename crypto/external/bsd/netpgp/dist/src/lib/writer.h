/*-
 * Copyright (c) 2009 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Alistair Crooks (agc@NetBSD.org)
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
/*
 * Copyright (c) 2005-2008 Nominet UK (www.nic.uk)
 * All rights reserved.
 * Contributors: Ben Laurie, Rachel Willmer. The Contributors have asserted
 * their moral rights under the UK Copyright Design and Patents Act 1988 to
 * be recorded as the authors of this copyright work.
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you may not
 * use this file except in compliance with the License.
 *
 * You may obtain a copy of the License at
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/** \file
 */

#ifndef WRITER_H_
#define WRITER_H_

#include "types.h"
#include "packet.h"
#include "crypto.h"
#include "errors.h"
#include "keyring.h"

/**
 * \ingroup Create
 * This struct contains the required information about one writer
 */
/**
 * \ingroup Writer
 * the writer function prototype
 */

typedef struct __ops_writer_info __ops_writer_info_t;
typedef unsigned 
__ops_writer_t(const unsigned char *,
	     unsigned,
	     __ops_error_t **,
	     __ops_writer_info_t *);
typedef unsigned 
__ops_writer_finaliser_t(__ops_error_t **, __ops_writer_info_t *);
typedef void    __ops_writer_destroyer_t(__ops_writer_info_t *);

/** Writer settings */
struct __ops_writer_info {
	__ops_writer_t   *writer;	/* !< the writer itself */
	__ops_writer_finaliser_t *finaliser;	/* !< the writer's finaliser */
	__ops_writer_destroyer_t *destroyer;	/* !< the writer's destroyer */
	void           *arg;	/* writer-specific argument */
	__ops_writer_info_t *next;/* !< next writer in the stack */
};


void           *__ops_writer_get_arg(__ops_writer_info_t *);
unsigned 
__ops_stacked_write(const void *, unsigned,
		  __ops_error_t **,
		  __ops_writer_info_t *);

void 
__ops_writer_set(__ops_createinfo_t *,
	       __ops_writer_t *,
	       __ops_writer_finaliser_t *,
	       __ops_writer_destroyer_t *,
	       void *);
void 
__ops_writer_push(__ops_createinfo_t *,
		__ops_writer_t *,
		__ops_writer_finaliser_t *,
		__ops_writer_destroyer_t *,
		void *);
void            __ops_writer_pop(__ops_createinfo_t *);
void            __ops_writer_generic_destroyer(__ops_writer_info_t *);
unsigned 
__ops_writer_passthrough(const unsigned char *,
		       unsigned,
		       __ops_error_t **,
		       __ops_writer_info_t *);

void            __ops_writer_set_fd(__ops_createinfo_t *, int);
unsigned   __ops_writer_close(__ops_createinfo_t *);

unsigned 
__ops_write(const void *, unsigned, __ops_createinfo_t *);
unsigned   __ops_write_length(unsigned, __ops_createinfo_t *);
unsigned   __ops_write_ptag(__ops_content_tag_t, __ops_createinfo_t *);
unsigned __ops_write_scalar(unsigned, unsigned, __ops_createinfo_t *);
unsigned   __ops_write_mpi(const BIGNUM *, __ops_createinfo_t *);
unsigned   __ops_write_encrypted_mpi(const BIGNUM *, __ops_crypt_t *, __ops_createinfo_t *);

void            writer_info_delete(__ops_writer_info_t *);
unsigned   writer_info_finalise(__ops_error_t **, __ops_writer_info_t *);

void __ops_push_stream_enc_se_ip(__ops_createinfo_t *, const __ops_keydata_t *);

#endif /* WRITER_H_ */
