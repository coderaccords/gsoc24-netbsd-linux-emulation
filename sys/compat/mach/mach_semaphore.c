/*	$NetBSD: mach_semaphore.c,v 1.7 2003/12/09 11:29:01 manu Exp $ */

/*-
 * Copyright (c) 2002-2003 The NetBSD Foundation, Inc.
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
__KERNEL_RCSID(0, "$NetBSD: mach_semaphore.c,v 1.7 2003/12/09 11:29:01 manu Exp $");

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/signal.h>
#include <sys/pool.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/proc.h>

#include <compat/mach/mach_types.h>
#include <compat/mach/mach_message.h>
#include <compat/mach/mach_semaphore.h>
#include <compat/mach/mach_clock.h>
#include <compat/mach/mach_errno.h>
#include <compat/mach/mach_port.h>
#include <compat/mach/mach_services.h>
#include <compat/mach/mach_syscallargs.h>

/* Semaphore list, lock, pools */
static LIST_HEAD(mach_semaphore_list, mach_semaphore) mach_semaphore_list; 
static struct lock mach_semaphore_list_lock;
static struct pool mach_semaphore_list_pool;
static struct pool mach_waiting_proc_pool;

/* Function to manipulate them */
static struct mach_semaphore *mach_semaphore_get(int, int);
static void mach_semaphore_put(struct mach_semaphore *);
static struct mach_waiting_proc *mach_waiting_proc_get
    (struct proc *, struct mach_semaphore *);
static void mach_waiting_proc_put
    (struct mach_waiting_proc *, struct mach_semaphore *, int);

int
mach_sys_semaphore_wait_trap(l, v, retval)
	struct lwp *l;
	void *v;
	register_t *retval;
{
	struct mach_sys_semaphore_wait_trap_args /* {
		syscallarg(mach_port_name_t) wait_name;
	} */ *uap = v;
	struct mach_semaphore *ms;
	struct mach_waiting_proc *mwp;
	struct mach_right *mr;
	mach_port_t mn;
	int blocked = 0;

	mn = SCARG(uap, wait_name);
	if ((mr = mach_right_check(mn, l, MACH_PORT_TYPE_ALL_RIGHTS)) == 0)
		return EPERM;
		
	if (mr->mr_port->mp_datatype != MACH_MP_SEMAPHORE)
		return EINVAL;

	ms = (struct mach_semaphore *)mr->mr_port->mp_data;

	lockmgr(&ms->ms_lock, LK_EXCLUSIVE, NULL);	
	ms->ms_value--;
	if (ms->ms_value < 0)
		blocked = 1;
	lockmgr(&ms->ms_lock, LK_RELEASE, NULL);	

	if (blocked != 0) {
		mwp = mach_waiting_proc_get(l->l_proc, ms);	
		while (ms->ms_value < 0)
			tsleep(mwp, PZERO|PCATCH, "sem_wait", 0);
		mach_waiting_proc_put(mwp, ms, 0);
	}
	return 0;
}

int
mach_sys_semaphore_signal_trap(l, v, retval)
	struct lwp *l;
	void *v;
	register_t *retval;
{
	struct mach_sys_semaphore_signal_trap_args /* {
		syscallarg(mach_port_name_t) signal_name;
	} */ *uap = v;
	struct mach_semaphore *ms;
	struct mach_waiting_proc *mwp;
	struct mach_right *mr;
	mach_port_t mn;
	int unblocked = 0;

	mn = SCARG(uap, signal_name);
	if ((mr = mach_right_check(mn, l, MACH_PORT_TYPE_ALL_RIGHTS)) == 0)
		return EPERM;
		
	if (mr->mr_port->mp_datatype != MACH_MP_SEMAPHORE)
		return EINVAL;

	ms = (struct mach_semaphore *)mr->mr_port->mp_datatype;

	lockmgr(&ms->ms_lock, LK_EXCLUSIVE, NULL);	
	ms->ms_value++;
	if (ms->ms_value >= 0)
		unblocked = 1;
	lockmgr(&ms->ms_lock, LK_RELEASE, NULL);	

	if (unblocked != 0) {
		lockmgr(&ms->ms_lock, LK_SHARED, NULL);	
		mwp = TAILQ_FIRST(&ms->ms_waiting);
		wakeup(mwp);
		lockmgr(&ms->ms_lock, LK_RELEASE, NULL);	
	}
	return 0;
}

int 
mach_semaphore_create(args)
	struct mach_trap_args *args;
{
	mach_semaphore_create_request_t *req = args->smsg;
	mach_semaphore_create_reply_t *rep = args->rmsg;
	size_t *msglen = args->rsize;
	struct lwp *l = args->l;
	struct mach_semaphore *ms;
	struct mach_port *mp;
	struct mach_right *mr;

	ms = mach_semaphore_get(req->req_value, req->req_policy);

	mp = mach_port_get();
	mp->mp_datatype = MACH_MP_SEMAPHORE;
	mp->mp_data = (void *)ms;
	
	mr = mach_right_get(mp, l, MACH_PORT_TYPE_SEND, 0);

	*msglen = sizeof(*rep);
	mach_set_header(rep, req, *msglen);
	mach_add_port_desc(rep, mr->mr_name);
	mach_set_trailer(rep, *msglen);

	return 0;
}

int 
mach_semaphore_destroy(args)
	struct mach_trap_args *args;
{
	mach_semaphore_destroy_request_t *req = args->smsg;
	mach_semaphore_destroy_reply_t *rep = args->rmsg;
	struct lwp *l = args->l;
	size_t *msglen = args->rsize;
	struct mach_semaphore *ms;
	struct mach_right *mr;
	mach_port_t mn;

	mn = req->req_sem.name;
	if ((mr = mach_right_check(mn, l, MACH_PORT_TYPE_ALL_RIGHTS)) == 0)
		return mach_msg_error(args, EPERM);
		
	if (mr->mr_port->mp_datatype != MACH_MP_SEMAPHORE)
		return mach_msg_error(args, EINVAL);

	ms = (struct mach_semaphore *)mr->mr_port->mp_data;
	mach_semaphore_put(ms);
	mach_right_put(mr, MACH_PORT_TYPE_REF_RIGHTS);
	
	*msglen = sizeof(*rep);
	mach_set_header(rep, req, *msglen);

	rep->rep_retval = 0;

	mach_set_trailer(rep, *msglen);

	return 0;
}

void
mach_semaphore_init(void)
{
	LIST_INIT(&mach_semaphore_list);
	lockinit(&mach_semaphore_list_lock, PZERO|PCATCH, "mach_sem", 0, 0);
	pool_init(&mach_semaphore_list_pool, sizeof (struct mach_semaphore),
	    0, 0, 128, "mach_sem_pool", NULL);
	pool_init(&mach_waiting_proc_pool, sizeof (struct mach_waiting_proc),
	    0, 0, 128, "mach_waitp_pool", NULL);

	return;
}

static struct mach_semaphore *
mach_semaphore_get(value, policy)
	int value;
	int policy;
{
	struct mach_semaphore *ms;

	ms = (struct mach_semaphore *)pool_get(&mach_semaphore_list_pool,
	    M_WAITOK);
	ms->ms_value = value;
	ms->ms_policy = policy;
	TAILQ_INIT(&ms->ms_waiting);
	lockinit(&ms->ms_lock, PZERO|PCATCH, "mach_waitp", 0, 0);

	lockmgr(&mach_semaphore_list_lock, LK_EXCLUSIVE, NULL);
	LIST_INSERT_HEAD(&mach_semaphore_list, ms, ms_list);
	lockmgr(&mach_semaphore_list_lock, LK_RELEASE, NULL);

	return ms;
}

static void
mach_semaphore_put(ms)
	struct mach_semaphore *ms;
{
	struct mach_waiting_proc *mwp;

	lockmgr(&ms->ms_lock, LK_EXCLUSIVE, NULL);	
	while ((mwp = TAILQ_FIRST(&ms->ms_waiting)) != NULL) 
		mach_waiting_proc_put(mwp, ms, 0);
	lockmgr(&ms->ms_lock, LK_RELEASE, NULL);	
	lockmgr(&ms->ms_lock, LK_DRAIN, NULL);	

	lockmgr(&mach_semaphore_list_lock, LK_EXCLUSIVE, NULL);
	LIST_REMOVE(ms, ms_list);
	lockmgr(&mach_semaphore_list_lock, LK_RELEASE, NULL);

	pool_put(&mach_semaphore_list_pool, ms);

	return;
}

static struct mach_waiting_proc *
mach_waiting_proc_get(p, ms)
	struct proc *p;
	struct mach_semaphore *ms;
{
	struct mach_waiting_proc *mwp;

	mwp = (struct mach_waiting_proc *)pool_get(&mach_waiting_proc_pool,
	    M_WAITOK);
	mwp->mwp_p = p;

	lockmgr(&ms->ms_lock, LK_EXCLUSIVE, NULL);	
	TAILQ_INSERT_TAIL(&ms->ms_waiting, mwp, mwp_list);
	lockmgr(&ms->ms_lock, LK_RELEASE, NULL);	

	return mwp;
}

static void
mach_waiting_proc_put(mwp, ms, locked)
	struct mach_waiting_proc *mwp;
	struct mach_semaphore *ms;
	int locked;
{
	if (!locked)
		lockmgr(&ms->ms_lock, LK_EXCLUSIVE, NULL);	
	TAILQ_REMOVE(&ms->ms_waiting, mwp, mwp_list);
	if (!locked)
		lockmgr(&ms->ms_lock, LK_RELEASE, NULL);	
	pool_put(&mach_waiting_proc_pool, mwp);

	return;
}

/* 
 * Cleanup after process exit. Need improvements, there 
 * can be some memory leaks here.
 */
void
mach_semaphore_cleanup(p)
	struct proc *p;
{
	struct mach_semaphore *ms;
	struct mach_waiting_proc *mwp;

	lockmgr(&mach_semaphore_list_lock, LK_SHARED, NULL);
	LIST_FOREACH(ms, &mach_semaphore_list, ms_list) {
		lockmgr(&ms->ms_lock, LK_SHARED, NULL);
		TAILQ_FOREACH(mwp, &ms->ms_waiting, mwp_list) 
			if (mwp->mwp_p == p) {
				lockmgr(&ms->ms_lock, LK_UPGRADE, NULL);
				mach_waiting_proc_put(mwp, ms, 0);
				ms->ms_value++;
				if (ms->ms_value >= 0)
					wakeup(TAILQ_FIRST(&ms->ms_waiting));
			}
		lockmgr(&ms->ms_lock, LK_RELEASE, NULL);
	}
	lockmgr(&mach_semaphore_list_lock, LK_RELEASE, NULL);
	
	return;
}
