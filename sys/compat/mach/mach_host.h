/*	$NetBSD: mach_host.h,v 1.2 2002/11/10 09:41:45 manu Exp $ */

/*-
 * Copyright (c) 2002 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Emmanuel Dreyfus
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
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef	_MACH_HOST_H_
#define	_MACH_HOST_H_

#include <compat/mach/mach_types.h>
#include <compat/mach/mach_message.h>

/* host_info */

typedef mach_integer_t mach_host_flavor_t;

typedef struct {
	mach_msg_header_t req_msgh;
	mach_ndr_record_t req_ndr;
	mach_host_flavor_t req_flavor;
	mach_msg_type_number_t req_count;
} mach_host_info_request_t;

typedef struct {
	mach_msg_header_t rep_msgh;
	mach_ndr_record_t rep_ndr;
	mach_kern_return_t rep_retval;
	mach_msg_type_number_t rep_count;
	mach_integer_t rep_data[12];
	mach_msg_trailer_t rep_trailer;
} mach_host_info_reply_t;

#define MACH_HOST_BASIC_INFO		1
#define MACH_HOST_SCHED_INFO		3
#define MACH_HOST_RESOURCE_SIZES	4
#define MACH_HOST_PRIORITY_INFO		5
#define MACH_HOST_SEMAPHORE_TRAPS	7
#define MACH_HOST_MACH_MSG_TRAP		8

struct mach_host_basic_info {
	mach_integer_t		max_cpus;
	mach_integer_t		avail_cpus;
	mach_vm_size_t		memory_size;
	mach_cpu_type_t		cpu_type;
	mach_cpu_subtype_t	cpu_subtype;
}; 

struct mach_host_sched_info {
	mach_integer_t		min_timeout;
	mach_integer_t		min_quantum;
};
	
struct mach_kernel_resource_sizes {
	mach_vm_size_t	 task;
	mach_vm_size_t	 thread;
	mach_vm_size_t	 port;
	mach_vm_size_t	 memory_region;
	mach_vm_size_t	 memory_object;
};

struct mach_host_priority_info {
	mach_integer_t	kernel_priority;
	mach_integer_t	system_priority;
	mach_integer_t	server_priority;
	mach_integer_t	user_priority;
	mach_integer_t	depress_priority;
	mach_integer_t	idle_priority;
	mach_integer_t	minimum_priority;
	mach_integer_t	maximum_priority;
};

/* host_page_size */

typedef struct {
	mach_msg_header_t req_msgh;
} mach_host_page_size_request_t;

typedef struct {
	mach_msg_header_t rep_msgh;
	mach_ndr_record_t rep_ndr;
	mach_kern_return_t rep_retval;
	mach_vm_size_t rep_page_size;
} mach_host_page_size_reply_t;

int mach_host_info __P((mach_msg_header_t *));
int mach_host_page_size __P((mach_msg_header_t *));

#endif /* _MACH_HOST_H_ */
