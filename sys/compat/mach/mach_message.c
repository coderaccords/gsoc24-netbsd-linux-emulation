/*	$NetBSD: mach_message.c,v 1.10 2002/12/26 11:41:46 manu Exp $ */

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

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: mach_message.c,v 1.10 2002/12/26 11:41:46 manu Exp $");

#include "opt_ktrace.h"
#include "opt_compat_mach.h" /* For COMPAT_MACH in <sys/ktrace.h> */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/signal.h>
#include <sys/proc.h>
#include <sys/kernel.h>
#include <sys/queue.h>
#include <sys/malloc.h>
#include <sys/pool.h>
#ifdef KTRACE
#include <sys/ktrace.h>
#endif 

#include <compat/mach/mach_types.h>
#include <compat/mach/mach_message.h>
#include <compat/mach/mach_port.h>
#include <compat/mach/mach_exec.h>
#include <compat/mach/mach_clock.h>
#include <compat/mach/mach_syscallargs.h>

/* Mach message pool */
static struct pool mach_message_pool;

int
mach_sys_msg_overwrite_trap(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct mach_sys_msg_overwrite_trap_args /* {
		syscallarg(mach_msg_header_t *) msg;
		syscallarg(mach_msg_option_t) option;
		syscallarg(mach_msg_size_t) send_size;
		syscallarg(mach_msg_size_t) rcv_size;
		syscallarg(mach_port_name_t) rcv_name;
		syscallarg(mach_msg_timeout_t) timeout;
		syscallarg(mach_port_name_t) notify;
		syscallarg(mach_msg_header_t *) rcv_msg;
		syscallarg(mach_msg_size_t) scatter_list_size;
	} */ *uap = v;
	int error = 0;
	struct mach_subsystem_namemap *map;
	struct mach_trap_args args;
	struct mach_emuldata *med;
	mach_msg_header_t *sm;
	mach_msg_header_t *rm;
	mach_msg_header_t *urm;
	size_t send_size, rcv_size;
	struct mach_port *mp;
	struct mach_message *mm;
	struct mach_right *mr;
	struct mach_right *cmr;
	int timeout;

	/*
	 * If neither send nor recieve, do nothing.
	 */
	if (SCARG(uap, option) & ~(MACH_SEND_MSG | MACH_RCV_MSG)) 
		return 0;

	/* 
	 * XXX Sanity check on the message size. This is not an accurate
	 * emulation, since Mach messages can be as large as 4GB. 
	 * Additionnaly, this does not address DoS attack by queueing
	 * lots of big messages in the kernel.
	 */
	send_size = SCARG(uap, send_size);
	rcv_size = SCARG(uap, rcv_size);
	if ((send_size > MACH_MAX_MSG_LEN) || (rcv_size > MACH_MAX_MSG_LEN)) {
		*retval = MACH_SEND_TOO_LARGE;
		return 0;
	}

	/* 
	 * Two options: receive or send. If both are 
	 * set, we must send, and then receive. If
	 * send fail, then we skip recieve.
	 */
	if (SCARG(uap, option) & MACH_SEND_MSG) {
		if (SCARG(uap, msg) == NULL) {
			*retval = MACH_SEND_INVALID_DATA;
			return 0;
		}

		/* 
		 * Allocate memory for the message and its reply,
		 * and copy the whole message in the kernel.
		 */
		sm = malloc(send_size, M_EMULDATA, M_WAITOK);
		if ((error = copyin(SCARG(uap, msg), sm, send_size)) != 0) {
			*retval = MACH_SEND_INVALID_DATA;	
			goto out1;
		}

#ifdef KTRACE
		/* Dump the Mach message */
		if (KTRPOINT(p, KTR_MMSG))
			ktrmmsg(p, (char *)sm, send_size); 
#endif

		/* 
		 * Check that the process has send a send right on 
		 * the remote port. 
		 */
		if (mach_right_check((struct mach_right *)sm->msgh_remote_port,
		    p, MACH_PORT_TYPE_SEND | MACH_PORT_TYPE_SEND_ONCE) == 0) {
			*retval = MACH_SEND_INVALID_RIGHT;
			goto out1;
		}

		if (mach_right_check((struct mach_right *)sm->msgh_local_port,
		    p, MACH_PORT_TYPE_RECEIVE) == 0) {
			*retval = MACH_SEND_INVALID_RIGHT;
			goto out1;
		}

		/*
		 * If the remote port is a special port (host, kernel or
		 * bootstrap), the message will be handled by the kernel.
		 */
		med = (struct mach_emuldata *)p->p_emuldata;
		mp = ((struct mach_right *)sm->msgh_remote_port)->mr_port;
		if ((mp == med->med_host) || (mp == med->med_kernel)) {
			/* 
			 * Look for the function that will handle it,
			 * using the message id.
			 */
			for (map = mach_namemap; map->map_id; map++)
				if (map->map_id == sm->msgh_id)
					break;
			
			/* 
			 * If no match, give up, and display a warning.
			 */
			if (map->map_handler == NULL) {
				uprintf("No mach trap handler for id = %d\n",
				    sm->msgh_id);
				*retval = MACH_SEND_INVALID_DEST;
				goto out3;
			}
#ifdef KTRACE
			/*
			 * It is convenient to record in kernel trace 
			 * the name of the handler that has been used,
			 * it makes traces easier to read. The user
			 * facility does not produce a perfect result,
			 * but at least we have the information.
			 */
			ktruser(p, map->map_name, NULL, 0, 0);
#endif
			/* 
			 * Invoke the handler. We give it the opportunity
			 * to shorten rcv_size if there is less data in
			 * the reply than what the sender expected.
			 */
			rm = malloc(rcv_size, M_EMULDATA, M_WAITOK | M_ZERO);

			args.p = p;
			args.smsg = sm;
			args.rmsg = rm;
			args.rsize = &rcv_size;
			if ((*retval = (*map->map_handler)(&args)) != 0) 
				goto out3;
			
			/* 
			 * Catch potential bug in the handler
			 */
			if (rcv_size > SCARG(uap, rcv_size)) {
				uprintf("mach_msg: reply too big in %s\n",
				    map->map_name);
				rcv_size = SCARG(uap, rcv_size);
			}

			/*
			 * Queue the reply
			 */
			mr = (struct mach_right *)sm->msgh_local_port;
			mp = mr->mr_port;
			(void)mach_message_get(rm, rcv_size, mp);
#ifdef DEBUG_MACH
			printf("pid %d: message queued on port %p (id %d)\n", 
			    p->p_pid, mp, rm->msgh_id);
#endif
			wakeup(mp->mp_recv);
out3:			free(sm, M_EMULDATA);

		} else {

			/* 
			 * The message is not to be handled by the kernel. 
			 * Queue the message in the remote port, and wakeup 
			 * any process that would be sleeping for it.
		 	 */
			mr = (struct mach_right *)sm->msgh_remote_port;
			mp = mr->mr_port;
			(void)mach_message_get(sm, send_size, mp);
#ifdef DEBUG_MACH
			printf("pid %d: message queued on port %p (%d)\n", 
			    p->p_pid, mp, sm->msgh_id);
#endif
			wakeup(mp->mp_recv);

			 /* 
			  * If the port is in a port set, wakup any process
			  * sleeping on the port set head.
			  */
			 if (mp->mp_recv->mr_sethead != NULL)
				wakeup(mp->mp_recv->mr_sethead);
		}

out1:
		if (error != 0) {
			free(sm, M_EMULDATA);
			return 0;
		}
	}

	/*
	 * Receiving messages.
	 */
	if (SCARG(uap, option) & MACH_RCV_MSG) {
		/* 
		 * Find a buffer for the reply
		 */
		if (SCARG(uap, rcv_msg) != NULL)
			urm = SCARG(uap, rcv_msg);
		else if (SCARG(uap, msg) != NULL)
			urm = SCARG(uap, msg);
		else {
			*retval = MACH_RCV_INVALID_DATA;
			return 0;
		}

		if (SCARG(uap, option) & MACH_RCV_TIMEOUT)
			timeout = SCARG(uap, timeout) * hz / 1000;
		else
			timeout = 0;

		/* 
		 * Check for receive right on the port 
		 */
		mr = (struct mach_right *)SCARG(uap, rcv_name);
		if ((mach_right_check(mr, p, MACH_PORT_TYPE_RECEIVE)) == 0) {
			
			/* 
			 * Is it a port set?
			 */
			if ((mach_right_check(mr, p, 
			    MACH_PORT_TYPE_PORT_SET)) == 0) {
				*retval = MACH_RCV_INVALID_NAME;
				return 0;
			}
			
			/* 
			 * This is a port set. For each port in the
			 * port set, check we have receive right, and
			 * and check if we have some message.
			 */
			LIST_FOREACH(cmr, &mr->mr_set, mr_setlist) {
				if ((mach_right_check(cmr, p, 
				    MACH_PORT_TYPE_RECEIVE)) == 0) {
					*retval = MACH_RCV_INVALID_NAME;
					return 0;
				}

				mp = cmr->mr_port;	
#ifdef DEBUG_MACH
				if (mp->mp_recv != cmr)
					uprintf("mach_msg_trap: bad receive "
					    "port/right\n");
#endif
				if (mp->mp_count != 0)
					break;
			}

			/* 
			 * If cmr is NULL then we found no message on
			 * any port. Sleep on the port set until we get 
			 * some or until we get a timeout.
			 */
			if (cmr == NULL) {
#ifdef DEBUG_MACH
				printf("pid %d: wait on port %p\n", 
				    p->p_pid, mp);
#endif
				error = tsleep(mr, PZERO, "mach_msg", timeout);
				if ((error == ERESTART) || (error == EINTR)) {
					*retval = MACH_RCV_INTERRUPTED;
					return 0;
				}

				/* 
				 * Check we did not loose the receive right
				 * while we were sleeping.
				 */
				if ((mach_right_check(mr, p, 
				     MACH_PORT_TYPE_PORT_SET)) == 0) {
					*retval = MACH_RCV_PORT_DIED;
					return 0;
				}

				/*
				 * Is there any pending message for 
				 * a port in the port set?
				 */
				LIST_FOREACH(cmr, &mr->mr_set, mr_setlist) {
					mp = cmr->mr_port;	
					if (mp->mp_count != 0)
						break;
				}

				if (cmr == NULL) {
					*retval = MACH_RCV_TIMED_OUT;
					return 0;
				}
			}
			
			/* 
			 * We found a port with a pending message.
			 */
			mp = cmr->mr_port;

		} else {
			/*
			 * This is a receive on a simple port (no port set).
			 * If there is no message queued on the port,
			 * block until we get some.
			 */
			mp = mr->mr_port;

#ifdef DEBUG_MACH
			if (mp->mp_recv != mr)
				uprintf("mach_msg_trap: bad receive "
				    "port/right\n");
#endif
#ifdef DEBUG_MACH
			printf("pid %d: wait on port %p\n", p->p_pid, mp);
#endif
			if (mp->mp_count == 0) {
				error = tsleep(mr, PZERO, "mach_msg", timeout);
				if ((error == ERESTART) || (error == EINTR)) {
					*retval = MACH_RCV_INTERRUPTED;
					return 0;
				}

				/* 
				 * Check we did not loose the receive right
				 * while we were sleeping.
				 */
				if ((mach_right_check(mr, p, 
				     MACH_PORT_TYPE_RECEIVE)) == 0) {
					*retval = MACH_RCV_PORT_DIED;
					return 0;
				}

				if (mp->mp_count == 0) {
					*retval = MACH_RCV_TIMED_OUT;
					return 0;
				}
			}
		}

		/* 
		 * Dequeue the message.
		 * XXX Do we really need to lock here? There could be
		 * only one reader process, so mm will not disapear
		 * except if there is a port refcount error in our code.
		 */
		lockmgr(&mp->mp_msglock, LK_SHARED, NULL);
		mm = TAILQ_FIRST(&mp->mp_msglist);
#ifdef DEBUG_MACH
		printf("pid %d: dequeue message on port %p (id %d)\n", 
		    p->p_pid, mp, mm->mm_msg->msgh_id);
#endif

		if (mm->mm_size > rcv_size) {
			/* 
			 * If MACH_RCV_LARGE was not set, destroy the
			 * message. If it was set, just notice that 
			 * the message is too big.
			 */
			if ((SCARG(uap, option) & MACH_RCV_LARGE) == 0) {
				free(mm->mm_msg, M_EMULDATA);
				mach_message_put_shlocked(mm);
			}		
			*retval = MACH_RCV_TOO_LARGE;
			goto unlock;
		}

		if ((error = copyout(mm->mm_msg, urm, mm->mm_size)) != 0) {
			*retval = MACH_RCV_INVALID_DATA;
			goto unlock;
		}
#ifdef KTRACE
		/* Dump the Mach message */
		if (KTRPOINT(p, KTR_MMSG))
			ktrmmsg(p, (char *)mm->mm_msg, mm->mm_size); 
#endif

		free(mm->mm_msg, M_EMULDATA);
		mach_message_put_shlocked(mm); /* decrease mp_count */
unlock:
		lockmgr(&mp->mp_msglock, LK_RELEASE, NULL);
	}

	return 0;
}

int
mach_sys_msg_trap(p, v, retval)
	struct proc *p;
	void *v;
	 register_t *retval;
{
	struct mach_sys_msg_trap_args /* {
		syscallarg(mach_msg_header_t *) msg;
		syscallarg(mach_msg_option_t) option;
		syscallarg(mach_msg_size_t) send_size;
		syscallarg(mach_msg_size_t) rcv_size;
		syscallarg(mach_port_name_t) rcv_name;
		syscallarg(mach_msg_timeout_t) timeout;
		syscallarg(mach_port_name_t) notify;
	} */ *uap = v;
	struct mach_sys_msg_overwrite_trap_args cup;

	SCARG(&cup, msg) = SCARG(uap, msg);
	SCARG(&cup, option) = SCARG(uap, option);
	SCARG(&cup, send_size) = SCARG(uap, send_size);
	SCARG(&cup, rcv_size) = SCARG(uap, rcv_size);
	SCARG(&cup, rcv_name) = SCARG(uap, rcv_name);
	SCARG(&cup, timeout) = SCARG(uap, timeout);
	SCARG(&cup, notify) = SCARG(uap, notify);
	SCARG(&cup, rcv_msg) = NULL;
	SCARG(&cup, scatter_list_size) = 0;

	return mach_sys_msg_overwrite_trap(p, &cup, retval);
}

void
mach_message_init(void)
{
	pool_init(&mach_message_pool, sizeof (struct mach_message),
	    0, 0, 128, "mach_message_pool", NULL);
	return;
}

struct mach_message *
mach_message_get(msgh, size, mp)
	mach_msg_header_t *msgh;
	size_t size;
	struct mach_port *mp;
{
	struct mach_message *mm;

	mm = (struct mach_message *)pool_get(&mach_message_pool, M_WAITOK);
	bzero(mm, sizeof(*mm));
	mm->mm_msg = msgh;
	mm->mm_size = size;
	mm->mm_port = mp;
	
	lockmgr(&mp->mp_msglock, LK_EXCLUSIVE, NULL);
	TAILQ_INSERT_TAIL(&mp->mp_msglist, mm, mm_list);
	mp->mp_count++;
	lockmgr(&mp->mp_msglock, LK_RELEASE, NULL);

	return mm;
}

void
mach_message_put(mm)
	struct mach_message *mm;
{
	struct mach_port *mp;

	mp = mm->mm_port;

	lockmgr(&mp->mp_msglock, LK_EXCLUSIVE, NULL);
	mach_message_put_exclocked(mm);
	lockmgr(&mp->mp_msglock, LK_RELEASE, NULL);

	return;
}

void
mach_message_put_shlocked(mm)
	struct mach_message *mm;
{
	struct mach_port *mp;

	mp = mm->mm_port;

	lockmgr(&mp->mp_msglock, LK_UPGRADE, NULL);
	mach_message_put_exclocked(mm);
	lockmgr(&mp->mp_msglock, LK_DOWNGRADE, NULL);

	return;
}

void
mach_message_put_exclocked(mm)
	struct mach_message *mm;
{
	struct mach_port *mp;

	mp = mm->mm_port;

	TAILQ_REMOVE(&mp->mp_msglist, mm, mm_list);
	mp->mp_count--;

	pool_put(&mach_message_pool, mm);

	return;
}
