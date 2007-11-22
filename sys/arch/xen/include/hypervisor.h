/*	$NetBSD: hypervisor.h,v 1.24 2007/11/22 16:16:57 bouyer Exp $	*/

/*
 * Copyright (c) 2006 Manuel Bouyer.
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
 *	This product includes software developed by Manuel Bouyer.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

/*
 * 
 * Communication to/from hypervisor.
 * 
 * Copyright (c) 2002-2004, K A Fraser
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this source file (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy, modify,
 * merge, publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */


#ifndef _XEN_HYPERVISOR_H_
#define _XEN_HYPERVISOR_H_

#include "opt_xen.h"


struct hypervisor_attach_args {
	const char 		*haa_busname;
};

struct xencons_attach_args {
	const char 		*xa_device;
};

struct xen_npx_attach_args {
	const char 		*xa_device;
};


#define	u8 uint8_t
#define	u16 uint16_t
#define	u32 uint32_t
#define	u64 uint64_t
#define	s8 int8_t
#define	s16 int16_t
#define	s32 int32_t
#define	s64 int64_t

#ifdef XEN3
#include <xen/xen3-public/xen.h>
#include <xen/xen3-public/sched.h>
#include <xen/xen3-public/dom0_ops.h>
#include <xen/xen3-public/event_channel.h>
#include <xen/xen3-public/physdev.h>
#include <xen/xen3-public/memory.h>
#include <xen/xen3-public/io/netif.h>
#include <xen/xen3-public/io/blkif.h>
#else
#include <xen/xen-public/xen.h>
#include <xen/xen-public/dom0_ops.h>
#include <xen/xen-public/event_channel.h>
#include <xen/xen-public/physdev.h>
#include <xen/xen-public/io/domain_controller.h>
#include <xen/xen-public/io/netif.h>
#include <xen/xen-public/io/blkif.h>
#endif

#include <machine/hypercalls.h>

#undef u8
#undef u16
#undef u32
#undef u64
#undef s8
#undef s16
#undef s32
#undef s64



/*
 * a placeholder for the start of day information passed up from the hypervisor
 */
union start_info_union
{
    start_info_t start_info;
    char padding[512];
};
extern union start_info_union start_info_union;
#define xen_start_info (start_info_union.start_info)

/* For use in guest OSes. */
volatile extern shared_info_t *HYPERVISOR_shared_info;

/* hypervisor.c */
struct intrframe;
void do_hypervisor_callback(struct intrframe *regs);
void hypervisor_enable_event(unsigned int);

/* hypervisor_machdep.c */
void hypervisor_unmask_event(unsigned int);
void hypervisor_mask_event(unsigned int);
void hypervisor_clear_event(unsigned int);
void hypervisor_enable_ipl(unsigned int);
void hypervisor_set_ipending(u_int32_t, int, int);


/* 
 * Force a proper event-channel callback from Xen after clearing the
 * callback mask. We do this in a very simple manner, by making a call
 * down into Xen. The pending flag will be checked by Xen on return. 
 */
static __inline void hypervisor_force_callback(void)
{
#ifdef XEN3
	(void)HYPERVISOR_xen_version(0, (void*)0);
#else
	(void)HYPERVISOR_xen_version(0);
#endif
} __attribute__((no_instrument_function)) /* used by mcount */

static __inline void
hypervisor_notify_via_evtchn(unsigned int port)
{
	evtchn_op_t op;

	op.cmd = EVTCHNOP_send;
#ifdef XEN3
	op.u.send.port = port;
#else
	op.u.send.local_port = port;
#endif
	(void)HYPERVISOR_event_channel_op(&op);
}

#endif /* _XEN_HYPERVISOR_H_ */
