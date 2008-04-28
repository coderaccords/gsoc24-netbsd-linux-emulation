/*	$NetBSD: kern_synch.c,v 1.234 2008/04/28 21:17:16 ad Exp $	*/

/*-
 * Copyright (c) 1999, 2000, 2004, 2006, 2007, 2008 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe of the Numerical Aerospace Simulation Facility,
 * NASA Ames Research Center, by Charles M. Hannum, Andrew Doran and
 * Daniel Sieger.
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
 * Copyright (c) 2007, 2008 Mindaugas Rasiukevicius <rmind at NetBSD org>
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

/*-
 * Copyright (c) 1982, 1986, 1990, 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 * (c) UNIX System Laboratories, Inc.
 * All or some portions of this file are derived from material licensed
 * to the University of California by American Telephone and Telegraph
 * Co. or Unix System Laboratories, Inc. and are reproduced herein with
 * the permission of UNIX System Laboratories, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)kern_synch.c	8.9 (Berkeley) 5/19/95
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: kern_synch.c,v 1.234 2008/04/28 21:17:16 ad Exp $");

#include "opt_kstack.h"
#include "opt_lockdebug.h"
#include "opt_multiprocessor.h"
#include "opt_perfctrs.h"

#define	__MUTEX_PRIVATE

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/kernel.h>
#if defined(PERFCTRS)
#include <sys/pmc.h>
#endif
#include <sys/cpu.h>
#include <sys/resourcevar.h>
#include <sys/sched.h>
#include <sys/syscall_stats.h>
#include <sys/sleepq.h>
#include <sys/lockdebug.h>
#include <sys/evcnt.h>
#include <sys/intr.h>
#include <sys/lwpctl.h>
#include <sys/atomic.h>
#include <sys/simplelock.h>
#include <sys/bitops.h>
#include <sys/kmem.h>
#include <sys/sysctl.h>
#include <sys/idle.h>

#include <uvm/uvm_extern.h>

#include <dev/lockstat.h>

/*
 * Priority related defintions.
 */
#define	PRI_TS_COUNT	(NPRI_USER)
#define	PRI_RT_COUNT	(PRI_COUNT - PRI_TS_COUNT)
#define	PRI_HTS_RANGE	(PRI_TS_COUNT / 10)

#define	PRI_HIGHEST_TS	(MAXPRI_USER)

/*
 * Bits per map.
 */
#define	BITMAP_BITS	(32)
#define	BITMAP_SHIFT	(5)
#define	BITMAP_MSB	(0x80000000U)
#define	BITMAP_MASK	(BITMAP_BITS - 1)

/*
 * Structures, runqueue.
 */

typedef struct {
	TAILQ_HEAD(, lwp) q_head;
} queue_t;

typedef struct {
	/* Lock and bitmap */
	uint32_t	r_bitmap[PRI_COUNT >> BITMAP_SHIFT];
	/* Counters */
	u_int		r_count;	/* Count of the threads */
	u_int		r_avgcount;	/* Average count of threads */
	u_int		r_mcount;	/* Count of migratable threads */
	/* Runqueues */
	queue_t		r_rt_queue[PRI_RT_COUNT];
	queue_t		r_ts_queue[PRI_TS_COUNT];
} runqueue_t;

static u_int	sched_unsleep(struct lwp *, bool);
static void	sched_changepri(struct lwp *, pri_t);
static void	sched_lendpri(struct lwp *, pri_t);
static void	*sched_getrq(runqueue_t *, const pri_t);
#ifdef MULTIPROCESSOR
static lwp_t	*sched_catchlwp(void);
static void	sched_balance(void *);
#endif

syncobj_t sleep_syncobj = {
	SOBJ_SLEEPQ_SORTED,
	sleepq_unsleep,
	sleepq_changepri,
	sleepq_lendpri,
	syncobj_noowner,
};

syncobj_t sched_syncobj = {
	SOBJ_SLEEPQ_SORTED,
	sched_unsleep,
	sched_changepri,
	sched_lendpri,
	syncobj_noowner,
};

const int 	schedppq = 1;
callout_t 	sched_pstats_ch;
unsigned	sched_pstats_ticks;
kcondvar_t	lbolt;			/* once a second sleep address */

/*
 * Preemption control.
 */
int		sched_upreempt_pri = PRI_KERNEL;
#if 0
int		sched_kpreempt_pri = PRI_USER_RT;
#else
/* XXX disable for now until any bugs are worked out. */
int		sched_kpreempt_pri = 1000;
#endif

static struct evcnt kpreempt_ev_crit;
static struct evcnt kpreempt_ev_klock;
static struct evcnt kpreempt_ev_ipl;
static struct evcnt kpreempt_ev_immed;

/*
 * Migration and balancing.
 */
static u_int	cacheht_time;		/* Cache hotness time */
static u_int	min_catch;		/* Minimal LWP count for catching */
static u_int	balance_period;		/* Balance period */
static struct cpu_info *worker_ci;	/* Victim CPU */
#ifdef MULTIPROCESSOR
static struct callout balance_ch;	/* Callout of balancer */
#endif

/*
 * During autoconfiguration or after a panic, a sleep will simply lower the
 * priority briefly to allow interrupts, then return.  The priority to be
 * used (safepri) is machine-dependent, thus this value is initialized and
 * maintained in the machine-dependent layers.  This priority will typically
 * be 0, or the lowest priority that is safe for use on the interrupt stack;
 * it can be made higher to block network software interrupts after panics.
 */
int	safepri;

/*
 * OBSOLETE INTERFACE
 *
 * General sleep call.  Suspends the current process until a wakeup is
 * performed on the specified identifier.  The process will then be made
 * runnable with the specified priority.  Sleeps at most timo/hz seconds (0
 * means no timeout).  If pri includes PCATCH flag, signals are checked
 * before and after sleeping, else signals are not checked.  Returns 0 if
 * awakened, EWOULDBLOCK if the timeout expires.  If PCATCH is set and a
 * signal needs to be delivered, ERESTART is returned if the current system
 * call should be restarted if possible, and EINTR is returned if the system
 * call should be interrupted by the signal (return EINTR).
 *
 * The interlock is held until we are on a sleep queue. The interlock will
 * be locked before returning back to the caller unless the PNORELOCK flag
 * is specified, in which case the interlock will always be unlocked upon
 * return.
 */
int
ltsleep(wchan_t ident, pri_t priority, const char *wmesg, int timo,
	volatile struct simplelock *interlock)
{
	struct lwp *l = curlwp;
	sleepq_t *sq;
	int error;

	KASSERT((l->l_pflag & LP_INTR) == 0);

	if (sleepq_dontsleep(l)) {
		(void)sleepq_abort(NULL, 0);
		if ((priority & PNORELOCK) != 0)
			simple_unlock(interlock);
		return 0;
	}

	l->l_kpriority = true;
	sq = sleeptab_lookup(&sleeptab, ident);
	sleepq_enter(sq, l);
	sleepq_enqueue(sq, ident, wmesg, &sleep_syncobj);

	if (interlock != NULL) {
		KASSERT(simple_lock_held(interlock));
		simple_unlock(interlock);
	}

	error = sleepq_block(timo, priority & PCATCH);

	if (interlock != NULL && (priority & PNORELOCK) == 0)
		simple_lock(interlock);
 
	return error;
}

int
mtsleep(wchan_t ident, pri_t priority, const char *wmesg, int timo,
	kmutex_t *mtx)
{
	struct lwp *l = curlwp;
	sleepq_t *sq;
	int error;

	KASSERT((l->l_pflag & LP_INTR) == 0);

	if (sleepq_dontsleep(l)) {
		(void)sleepq_abort(mtx, (priority & PNORELOCK) != 0);
		return 0;
	}

	l->l_kpriority = true;
	sq = sleeptab_lookup(&sleeptab, ident);
	sleepq_enter(sq, l);
	sleepq_enqueue(sq, ident, wmesg, &sleep_syncobj);
	mutex_exit(mtx);
	error = sleepq_block(timo, priority & PCATCH);

	if ((priority & PNORELOCK) == 0)
		mutex_enter(mtx);
 
	return error;
}

/*
 * General sleep call for situations where a wake-up is not expected.
 */
int
kpause(const char *wmesg, bool intr, int timo, kmutex_t *mtx)
{
	struct lwp *l = curlwp;
	sleepq_t *sq;
	int error;

	if (sleepq_dontsleep(l))
		return sleepq_abort(NULL, 0);

	if (mtx != NULL)
		mutex_exit(mtx);
	l->l_kpriority = true;
	sq = sleeptab_lookup(&sleeptab, l);
	sleepq_enter(sq, l);
	sleepq_enqueue(sq, l, wmesg, &sleep_syncobj);
	error = sleepq_block(timo, intr);
	if (mtx != NULL)
		mutex_enter(mtx);

	return error;
}

/*
 * OBSOLETE INTERFACE
 *
 * Make all processes sleeping on the specified identifier runnable.
 */
void
wakeup(wchan_t ident)
{
	sleepq_t *sq;

	if (cold)
		return;

	sq = sleeptab_lookup(&sleeptab, ident);
	sleepq_wake(sq, ident, (u_int)-1);
}

/*
 * OBSOLETE INTERFACE
 *
 * Make the highest priority process first in line on the specified
 * identifier runnable.
 */
void 
wakeup_one(wchan_t ident)
{
	sleepq_t *sq;

	if (cold)
		return;

	sq = sleeptab_lookup(&sleeptab, ident);
	sleepq_wake(sq, ident, 1);
}


/*
 * General yield call.  Puts the current process back on its run queue and
 * performs a voluntary context switch.  Should only be called when the
 * current process explicitly requests it (eg sched_yield(2)).
 */
void
yield(void)
{
	struct lwp *l = curlwp;

	KERNEL_UNLOCK_ALL(l, &l->l_biglocks);
	lwp_lock(l);
	KASSERT(lwp_locked(l, l->l_cpu->ci_schedstate.spc_lwplock));
	KASSERT(l->l_stat == LSONPROC);
	l->l_kpriority = false;
	(void)mi_switch(l);
	KERNEL_LOCK(l->l_biglocks, l);
}

/*
 * General preemption call.  Puts the current process back on its run queue
 * and performs an involuntary context switch.
 */
void
preempt(void)
{
	struct lwp *l = curlwp;

	KERNEL_UNLOCK_ALL(l, &l->l_biglocks);
	lwp_lock(l);
	KASSERT(lwp_locked(l, l->l_cpu->ci_schedstate.spc_lwplock));
	KASSERT(l->l_stat == LSONPROC);
	l->l_kpriority = false;
	l->l_nivcsw++;
	(void)mi_switch(l);
	KERNEL_LOCK(l->l_biglocks, l);
}

/*
 * Handle a request made by another agent to preempt the current LWP
 * in-kernel.  Usually called when l_dopreempt may be non-zero.
 *
 * Character addresses for lockstat only.
 */
static char	in_critical_section;
static char	kernel_lock_held;
static char	spl_raised;
static char	is_softint;

bool
kpreempt(uintptr_t where)
{
	uintptr_t failed;
	lwp_t *l;
	int s, dop;

	l = curlwp;
	failed = 0;
	while ((dop = l->l_dopreempt) != 0) {
		if (l->l_stat != LSONPROC) {
			/*
			 * About to block (or die), let it happen.
			 * Doesn't really count as "preemption has
			 * been blocked", since we're going to
			 * context switch.
			 */
			l->l_dopreempt = 0;
			return true;
		}
		if (__predict_false((l->l_flag & LW_IDLE) != 0)) {
			/* Can't preempt idle loop, don't count as failure. */
		    	l->l_dopreempt = 0;
		    	return true;
		}
		if (__predict_false(l->l_nopreempt != 0)) {
			/* LWP holds preemption disabled, explicitly. */
			if ((dop & DOPREEMPT_COUNTED) == 0) {
				kpreempt_ev_crit.ev_count++;
			}
			failed = (uintptr_t)&in_critical_section;
			break;
		}
		if (__predict_false((l->l_pflag & LP_INTR) != 0)) {
		    	/* Can't preempt soft interrupts yet. */
		    	l->l_dopreempt = 0;
		    	failed = (uintptr_t)&is_softint;
		    	break;
		}
		s = splsched();
		if (__predict_false(l->l_blcnt != 0 ||
		    curcpu()->ci_biglock_wanted != NULL)) {
			/* Hold or want kernel_lock, code is not MT safe. */
			splx(s);
			if ((dop & DOPREEMPT_COUNTED) == 0) {
				kpreempt_ev_klock.ev_count++;
			}
			failed = (uintptr_t)&kernel_lock_held;
			break;
		}
		if (__predict_false(!cpu_kpreempt_enter(where, s))) {
			/*
			 * It may be that the IPL is too high.
			 * kpreempt_enter() can schedule an
			 * interrupt to retry later.
			 */
			splx(s);
			if ((dop & DOPREEMPT_COUNTED) == 0) {
				kpreempt_ev_ipl.ev_count++;
			}
			failed = (uintptr_t)&spl_raised;
			break;
		}
		/* Do it! */
		if (__predict_true((dop & DOPREEMPT_COUNTED) == 0)) {
			kpreempt_ev_immed.ev_count++;
		}
		lwp_lock(l);
		mi_switch(l);
		l->l_nopreempt++;
		splx(s);

		/* Take care of any MD cleanup. */
		cpu_kpreempt_exit(where);
		l->l_nopreempt--;
	}

	/* Record preemption failure for reporting via lockstat. */
	if (__predict_false(failed)) {
		atomic_or_uint(&l->l_dopreempt, DOPREEMPT_COUNTED);
		int lsflag = 0;
		LOCKSTAT_ENTER(lsflag);
		/* Might recurse, make it atomic. */
		if (__predict_false(lsflag)) {
			if (where == 0) {
				where = (uintptr_t)__builtin_return_address(0);
			}
			if (atomic_cas_ptr_ni((void *)&l->l_pfailaddr,
			    NULL, (void *)where) == NULL) {
				LOCKSTAT_START_TIMER(lsflag, l->l_pfailtime);
				l->l_pfaillock = failed;
			}
		}
		LOCKSTAT_EXIT(lsflag);
	}

	return failed;
}

/*
 * Return true if preemption is explicitly disabled.
 */
bool
kpreempt_disabled(void)
{
	lwp_t *l;

	l = curlwp;

	return l->l_nopreempt != 0 || l->l_stat == LSZOMB ||
	    (l->l_flag & LW_IDLE) != 0 || cpu_kpreempt_disabled();
}

/*
 * Disable kernel preemption.
 */
void
kpreempt_disable(void)
{

	KPREEMPT_DISABLE(curlwp);
}

/*
 * Reenable kernel preemption.
 */
void
kpreempt_enable(void)
{

	KPREEMPT_ENABLE(curlwp);
}

/*
 * Compute the amount of time during which the current lwp was running.
 *
 * - update l_rtime unless it's an idle lwp.
 */

void
updatertime(lwp_t *l, const struct bintime *now)
{

	if ((l->l_flag & LW_IDLE) != 0)
		return;

	/* rtime += now - stime */
	bintime_add(&l->l_rtime, now);
	bintime_sub(&l->l_rtime, &l->l_stime);
}

/*
 * The machine independent parts of context switch.
 *
 * Returns 1 if another LWP was actually run.
 */
int
mi_switch(lwp_t *l)
{
	struct cpu_info *ci, *tci = NULL;
	struct schedstate_percpu *spc;
	struct lwp *newl;
	int retval, oldspl;
	struct bintime bt;
	bool returning;

	KASSERT(lwp_locked(l, NULL));
	KASSERT(kpreempt_disabled());
	LOCKDEBUG_BARRIER(l->l_mutex, 1);

#ifdef KSTACK_CHECK_MAGIC
	kstack_check_magic(l);
#endif

	binuptime(&bt);

	KASSERT(l->l_cpu == curcpu());
	ci = l->l_cpu;
	spc = &ci->ci_schedstate;
	returning = false;
	newl = NULL;

	/*
	 * If we have been asked to switch to a specific LWP, then there
	 * is no need to inspect the run queues.  If a soft interrupt is
	 * blocking, then return to the interrupted thread without adjusting
	 * VM context or its start time: neither have been changed in order
	 * to take the interrupt.
	 */
	if (l->l_switchto != NULL) {
		if ((l->l_pflag & LP_INTR) != 0) {
			returning = true;
			softint_block(l);
			if ((l->l_flag & LW_TIMEINTR) != 0)
				updatertime(l, &bt);
		}
		newl = l->l_switchto;
		l->l_switchto = NULL;
	}
#ifndef __HAVE_FAST_SOFTINTS
	else if (ci->ci_data.cpu_softints != 0) {
		/* There are pending soft interrupts, so pick one. */
		newl = softint_picklwp();
		newl->l_stat = LSONPROC;
		newl->l_flag |= LW_RUNNING;
	}
#endif	/* !__HAVE_FAST_SOFTINTS */

	/* Count time spent in current system call */
	if (!returning) {
		SYSCALL_TIME_SLEEP(l);

		/*
		 * XXXSMP If we are using h/w performance counters,
		 * save context.
		 */
#if PERFCTRS
		if (PMC_ENABLED(l->l_proc)) {
			pmc_save_context(l->l_proc);
		}
#endif
		updatertime(l, &bt);
	}

	/*
	 * If on the CPU and we have gotten this far, then we must yield.
	 */
	KASSERT(l->l_stat != LSRUN);
	if (l->l_stat == LSONPROC && (l->l_target_cpu || l != newl)) {
		KASSERT(lwp_locked(l, spc->spc_lwplock));

		if (l->l_target_cpu == l->l_cpu) {
			l->l_target_cpu = NULL;
		} else {
			tci = l->l_target_cpu;
		}

		if (__predict_false(tci != NULL)) {
			/* Double-lock the runqueues */
			spc_dlock(ci, tci);
		} else {
			/* Lock the runqueue */
			spc_lock(ci);
		}

		if ((l->l_flag & LW_IDLE) == 0) {
			l->l_stat = LSRUN;
			if (__predict_false(tci != NULL)) {
				/* 
				 * Set the new CPU, lock and unset the
				 * l_target_cpu - thread will be enqueued
				 * to the runqueue of target CPU.
				 */
				l->l_cpu = tci;
				lwp_setlock(l, tci->ci_schedstate.spc_mutex);
				l->l_target_cpu = NULL;
			} else {
				lwp_setlock(l, spc->spc_mutex);
			}
			sched_enqueue(l, true);
		} else {
			KASSERT(tci == NULL);
			l->l_stat = LSIDL;
		}
	} else {
		/* Lock the runqueue */
		spc_lock(ci);
	}

	/*
	 * Let sched_nextlwp() select the LWP to run the CPU next.
	 * If no LWP is runnable, select the idle LWP.
	 * 
	 * Note that spc_lwplock might not necessary be held, and
	 * new thread would be unlocked after setting the LWP-lock.
	 */
	if (newl == NULL) {
		newl = sched_nextlwp();
		if (newl != NULL) {
			sched_dequeue(newl);
			KASSERT(lwp_locked(newl, spc->spc_mutex));
			newl->l_stat = LSONPROC;
			newl->l_cpu = ci;
			newl->l_flag |= LW_RUNNING;
			lwp_setlock(newl, spc->spc_lwplock);
		} else {
			newl = ci->ci_data.cpu_idlelwp;
			newl->l_stat = LSONPROC;
			newl->l_flag |= LW_RUNNING;
		}
		/*
		 * Only clear want_resched if there are no
		 * pending (slow) software interrupts.
		 */
		ci->ci_want_resched = ci->ci_data.cpu_softints;
		spc->spc_flags &= ~SPCF_SWITCHCLEAR;
		spc->spc_curpriority = lwp_eprio(newl);
	}

	/* Items that must be updated with the CPU locked. */
	if (!returning) {
		/* Update the new LWP's start time. */
		newl->l_stime = bt;

		/*
		 * ci_curlwp changes when a fast soft interrupt occurs.
		 * We use cpu_onproc to keep track of which kernel or
		 * user thread is running 'underneath' the software
		 * interrupt.  This is important for time accounting,
		 * itimers and forcing user threads to preempt (aston).
		 */
		ci->ci_data.cpu_onproc = newl;
	}

	/* Kernel preemption related tasks. */
	l->l_dopreempt = 0;
	if (__predict_false(l->l_pfailaddr != 0)) {
		LOCKSTAT_FLAG(lsflag);
		LOCKSTAT_ENTER(lsflag);
		LOCKSTAT_STOP_TIMER(lsflag, l->l_pfailtime);
		LOCKSTAT_EVENT_RA(lsflag, l->l_pfaillock, LB_NOPREEMPT|LB_SPIN,
		    1, l->l_pfailtime, l->l_pfailaddr);
		LOCKSTAT_EXIT(lsflag);
		l->l_pfailtime = 0;
		l->l_pfaillock = 0;
		l->l_pfailaddr = 0;
	}

	if (l != newl) {
		struct lwp *prevlwp;

		/* Release all locks, but leave the current LWP locked */
		if (l->l_mutex == l->l_cpu->ci_schedstate.spc_mutex) {
			/*
			 * In case of migration, drop the local runqueue
			 * lock, thread is on other runqueue now.
			 */
			if (__predict_false(tci != NULL))
				spc_unlock(ci);
			/*
			 * Drop spc_lwplock, if the current LWP has been moved
			 * to the run queue (it is now locked by spc_mutex).
			 */
			mutex_spin_exit(spc->spc_lwplock);
		} else {
			/*
			 * Otherwise, drop the spc_mutex, we are done with the
			 * run queues.
			 */
			mutex_spin_exit(spc->spc_mutex);
			KASSERT(tci == NULL);
		}

		/*
		 * Mark that context switch is going to be perfomed
		 * for this LWP, to protect it from being switched
		 * to on another CPU.
		 */
		KASSERT(l->l_ctxswtch == 0);
		l->l_ctxswtch = 1;
		l->l_ncsw++;
		l->l_flag &= ~LW_RUNNING;

		/*
		 * Increase the count of spin-mutexes before the release
		 * of the last lock - we must remain at IPL_SCHED during
		 * the context switch.
		 */
		oldspl = MUTEX_SPIN_OLDSPL(ci);
		ci->ci_mtx_count--;
		lwp_unlock(l);

		/* Count the context switch on this CPU. */
		ci->ci_data.cpu_nswtch++;

		/* Update status for lwpctl, if present. */
		if (l->l_lwpctl != NULL)
			l->l_lwpctl->lc_curcpu = LWPCTL_CPU_NONE;

		/*
		 * Save old VM context, unless a soft interrupt
		 * handler is blocking.
		 */
		if (!returning)
			pmap_deactivate(l);

		/*
		 * We may need to spin-wait for if 'newl' is still
		 * context switching on another CPU.
		 */
		if (newl->l_ctxswtch != 0) {
			u_int count;
			count = SPINLOCK_BACKOFF_MIN;
			while (newl->l_ctxswtch)
				SPINLOCK_BACKOFF(count);
		}

		/* Switch to the new LWP.. */
		prevlwp = cpu_switchto(l, newl, returning);
		ci = curcpu();

		/*
		 * Switched away - we have new curlwp.
		 * Restore VM context and IPL.
		 */
		pmap_activate(l);
		if (prevlwp != NULL) {
			/* Normalize the count of the spin-mutexes */
			ci->ci_mtx_count++;
			/* Unmark the state of context switch */
			membar_exit();
			prevlwp->l_ctxswtch = 0;
		}

		/* Update status for lwpctl, if present. */
		if (l->l_lwpctl != NULL) {
			l->l_lwpctl->lc_curcpu = (int)cpu_index(ci);
			l->l_lwpctl->lc_pctr++;
		}

		KASSERT(l->l_cpu == ci);
		splx(oldspl);
		retval = 1;
	} else {
		/* Nothing to do - just unlock and return. */
		KASSERT(tci == NULL);
		spc_unlock(ci);
		lwp_unlock(l);
		retval = 0;
	}

	KASSERT(l == curlwp);
	KASSERT(l->l_stat == LSONPROC);

	/*
	 * XXXSMP If we are using h/w performance counters, restore context.
	 * XXXSMP preemption problem.
	 */
#if PERFCTRS
	if (PMC_ENABLED(l->l_proc)) {
		pmc_restore_context(l->l_proc);
	}
#endif
	SYSCALL_TIME_WAKEUP(l);
	LOCKDEBUG_BARRIER(NULL, 1);

	return retval;
}

/*
 * Change process state to be runnable, placing it on the run queue if it is
 * in memory, and awakening the swapper if it isn't in memory.
 *
 * Call with the process and LWP locked.  Will return with the LWP unlocked.
 */
void
setrunnable(struct lwp *l)
{
	struct proc *p = l->l_proc;
	struct cpu_info *ci;
	sigset_t *ss;

	KASSERT((l->l_flag & LW_IDLE) == 0);
	KASSERT(mutex_owned(p->p_lock));
	KASSERT(lwp_locked(l, NULL));
	KASSERT(l->l_mutex != l->l_cpu->ci_schedstate.spc_mutex);

	switch (l->l_stat) {
	case LSSTOP:
		/*
		 * If we're being traced (possibly because someone attached us
		 * while we were stopped), check for a signal from the debugger.
		 */
		if ((p->p_slflag & PSL_TRACED) != 0 && p->p_xstat != 0) {
			if ((sigprop[p->p_xstat] & SA_TOLWP) != 0)
				ss = &l->l_sigpend.sp_set;
			else
				ss = &p->p_sigpend.sp_set;
			sigaddset(ss, p->p_xstat);
			signotify(l);
		}
		p->p_nrlwps++;
		break;
	case LSSUSPENDED:
		l->l_flag &= ~LW_WSUSPEND;
		p->p_nrlwps++;
		cv_broadcast(&p->p_lwpcv);
		break;
	case LSSLEEP:
		KASSERT(l->l_wchan != NULL);
		break;
	default:
		panic("setrunnable: lwp %p state was %d", l, l->l_stat);
	}

	/*
	 * If the LWP was sleeping interruptably, then it's OK to start it
	 * again.  If not, mark it as still sleeping.
	 */
	if (l->l_wchan != NULL) {
		l->l_stat = LSSLEEP;
		/* lwp_unsleep() will release the lock. */
		lwp_unsleep(l, true);
		return;
	}

	/*
	 * If the LWP is still on the CPU, mark it as LSONPROC.  It may be
	 * about to call mi_switch(), in which case it will yield.
	 */
	if ((l->l_flag & LW_RUNNING) != 0) {
		l->l_stat = LSONPROC;
		l->l_slptime = 0;
		lwp_unlock(l);
		return;
	}

	/*
	 * Look for a CPU to run.
	 * Set the LWP runnable.
	 */
	ci = sched_takecpu(l);
	l->l_cpu = ci;
	if (l->l_mutex != l->l_cpu->ci_schedstate.spc_mutex) {
		lwp_unlock_to(l, ci->ci_schedstate.spc_mutex);
		lwp_lock(l);
	}
	sched_setrunnable(l);
	l->l_stat = LSRUN;
	l->l_slptime = 0;

	/*
	 * If thread is swapped out - wake the swapper to bring it back in.
	 * Otherwise, enter it into a run queue.
	 */
	if (l->l_flag & LW_INMEM) {
		sched_enqueue(l, false);
		resched_cpu(l);
		lwp_unlock(l);
	} else {
		lwp_unlock(l);
		uvm_kick_scheduler();
	}
}

/*
 * suspendsched:
 *
 *	Convert all non-L_SYSTEM LSSLEEP or LSRUN LWPs to LSSUSPENDED. 
 */
void
suspendsched(void)
{
	CPU_INFO_ITERATOR cii;
	struct cpu_info *ci;
	struct lwp *l;
	struct proc *p;

	/*
	 * We do this by process in order not to violate the locking rules.
	 */
	mutex_enter(proc_lock);
	PROCLIST_FOREACH(p, &allproc) {
		mutex_enter(p->p_lock);

		if ((p->p_flag & PK_SYSTEM) != 0) {
			mutex_exit(p->p_lock);
			continue;
		}

		p->p_stat = SSTOP;

		LIST_FOREACH(l, &p->p_lwps, l_sibling) {
			if (l == curlwp)
				continue;

			lwp_lock(l);

			/*
			 * Set L_WREBOOT so that the LWP will suspend itself
			 * when it tries to return to user mode.  We want to
			 * try and get to get as many LWPs as possible to
			 * the user / kernel boundary, so that they will
			 * release any locks that they hold.
			 */
			l->l_flag |= (LW_WREBOOT | LW_WSUSPEND);

			if (l->l_stat == LSSLEEP &&
			    (l->l_flag & LW_SINTR) != 0) {
				/* setrunnable() will release the lock. */
				setrunnable(l);
				continue;
			}

			lwp_unlock(l);
		}

		mutex_exit(p->p_lock);
	}
	mutex_exit(proc_lock);

	/*
	 * Kick all CPUs to make them preempt any LWPs running in user mode. 
	 * They'll trap into the kernel and suspend themselves in userret().
	 */
	for (CPU_INFO_FOREACH(cii, ci)) {
		spc_lock(ci);
		cpu_need_resched(ci, RESCHED_IMMED);
		spc_unlock(ci);
	}
}

/*
 * sched_unsleep:
 *
 *	The is called when the LWP has not been awoken normally but instead
 *	interrupted: for example, if the sleep timed out.  Because of this,
 *	it's not a valid action for running or idle LWPs.
 */
static u_int
sched_unsleep(struct lwp *l, bool cleanup)
{

	lwp_unlock(l);
	panic("sched_unsleep");
}

void
resched_cpu(struct lwp *l)
{
	struct cpu_info *ci;

	/*
	 * XXXSMP
	 * Since l->l_cpu persists across a context switch,
	 * this gives us *very weak* processor affinity, in
	 * that we notify the CPU on which the process last
	 * ran that it should try to switch.
	 *
	 * This does not guarantee that the process will run on
	 * that processor next, because another processor might
	 * grab it the next time it performs a context switch.
	 *
	 * This also does not handle the case where its last
	 * CPU is running a higher-priority process, but every
	 * other CPU is running a lower-priority process.  There
	 * are ways to handle this situation, but they're not
	 * currently very pretty, and we also need to weigh the
	 * cost of moving a process from one CPU to another.
	 */
	ci = l->l_cpu;
	if (lwp_eprio(l) > ci->ci_schedstate.spc_curpriority)
		cpu_need_resched(ci, 0);
}

static void
sched_changepri(struct lwp *l, pri_t pri)
{

	KASSERT(lwp_locked(l, NULL));

	if (l->l_stat == LSRUN && (l->l_flag & LW_INMEM) != 0) {
		KASSERT(lwp_locked(l, l->l_cpu->ci_schedstate.spc_mutex));
		sched_dequeue(l);
		l->l_priority = pri;
		sched_enqueue(l, false);
	} else {
		l->l_priority = pri;
	}
	resched_cpu(l);
}

static void
sched_lendpri(struct lwp *l, pri_t pri)
{

	KASSERT(lwp_locked(l, NULL));

	if (l->l_stat == LSRUN && (l->l_flag & LW_INMEM) != 0) {
		KASSERT(lwp_locked(l, l->l_cpu->ci_schedstate.spc_mutex));
		sched_dequeue(l);
		l->l_inheritedprio = pri;
		sched_enqueue(l, false);
	} else {
		l->l_inheritedprio = pri;
	}
	resched_cpu(l);
}

struct lwp *
syncobj_noowner(wchan_t wchan)
{

	return NULL;
}

/* decay 95% of `p_pctcpu' in 60 seconds; see CCPU_SHIFT before changing */
fixpt_t	ccpu = 0.95122942450071400909 * FSCALE;		/* exp(-1/20) */

/*
 * If `ccpu' is not equal to `exp(-1/20)' and you still want to use the
 * faster/more-accurate formula, you'll have to estimate CCPU_SHIFT below
 * and possibly adjust FSHIFT in "param.h" so that (FSHIFT >= CCPU_SHIFT).
 *
 * To estimate CCPU_SHIFT for exp(-1/20), the following formula was used:
 *	1 - exp(-1/20) ~= 0.0487 ~= 0.0488 == 1 (fixed pt, *11* bits).
 *
 * If you dont want to bother with the faster/more-accurate formula, you
 * can set CCPU_SHIFT to (FSHIFT + 1) which will use a slower/less-accurate
 * (more general) method of calculating the %age of CPU used by a process.
 */
#define	CCPU_SHIFT	(FSHIFT + 1)

/*
 * sched_pstats:
 *
 * Update process statistics and check CPU resource allocation.
 * Call scheduler-specific hook to eventually adjust process/LWP
 * priorities.
 */
/* ARGSUSED */
void
sched_pstats(void *arg)
{
	struct rlimit *rlim;
	struct lwp *l;
	struct proc *p;
	int sig, clkhz;
	long runtm;

	sched_pstats_ticks++;

	mutex_enter(proc_lock);
	PROCLIST_FOREACH(p, &allproc) {
		/*
		 * Increment time in/out of memory and sleep time (if
		 * sleeping).  We ignore overflow; with 16-bit int's
		 * (remember them?) overflow takes 45 days.
		 */
		mutex_enter(p->p_lock);
		mutex_spin_enter(&p->p_stmutex);
		runtm = p->p_rtime.sec;
		LIST_FOREACH(l, &p->p_lwps, l_sibling) {
			if ((l->l_flag & LW_IDLE) != 0)
				continue;
			lwp_lock(l);
			runtm += l->l_rtime.sec;
			l->l_swtime++;
			sched_pstats_hook(l);
			lwp_unlock(l);

			/*
			 * p_pctcpu is only for ps.
			 */
			l->l_pctcpu = (l->l_pctcpu * ccpu) >> FSHIFT;
			if (l->l_slptime < 1) {
				clkhz = stathz != 0 ? stathz : hz;
#if	(FSHIFT >= CCPU_SHIFT)
				l->l_pctcpu += (clkhz == 100) ?
				    ((fixpt_t)l->l_cpticks) <<
				        (FSHIFT - CCPU_SHIFT) :
				    100 * (((fixpt_t) p->p_cpticks)
				        << (FSHIFT - CCPU_SHIFT)) / clkhz;
#else
				l->l_pctcpu += ((FSCALE - ccpu) *
				    (l->l_cpticks * FSCALE / clkhz)) >> FSHIFT;
#endif
				l->l_cpticks = 0;
			}
		}
		p->p_pctcpu = (p->p_pctcpu * ccpu) >> FSHIFT;
		mutex_spin_exit(&p->p_stmutex);

		/*
		 * Check if the process exceeds its CPU resource allocation.
		 * If over max, kill it.
		 */
		rlim = &p->p_rlimit[RLIMIT_CPU];
		sig = 0;
		if (runtm >= rlim->rlim_cur) {
			if (runtm >= rlim->rlim_max)
				sig = SIGKILL;
			else {
				sig = SIGXCPU;
				if (rlim->rlim_cur < rlim->rlim_max)
					rlim->rlim_cur += 5;
			}
		}
		mutex_exit(p->p_lock);
		if (sig)
			psignal(p, sig);
	}
	mutex_exit(proc_lock);
	uvm_meter();
	cv_wakeup(&lbolt);
	callout_schedule(&sched_pstats_ch, hz);
}

void
sched_init(void)
{

	cv_init(&lbolt, "lbolt");
	callout_init(&sched_pstats_ch, CALLOUT_MPSAFE);
	callout_setfunc(&sched_pstats_ch, sched_pstats, NULL);

	/* Balancing */
	worker_ci = curcpu();
	cacheht_time = mstohz(5);		/* ~5 ms  */
	balance_period = mstohz(300);		/* ~300ms */

	/* Minimal count of LWPs for catching: log2(count of CPUs) */
	min_catch = min(ilog2(ncpu), 4);

	evcnt_attach_dynamic(&kpreempt_ev_crit, EVCNT_TYPE_INTR, NULL,
	   "kpreempt", "defer: critical section");
	evcnt_attach_dynamic(&kpreempt_ev_klock, EVCNT_TYPE_INTR, NULL,
	   "kpreempt", "defer: kernel_lock");
	evcnt_attach_dynamic(&kpreempt_ev_ipl, EVCNT_TYPE_INTR, NULL,
	   "kpreempt", "defer: IPL");
	evcnt_attach_dynamic(&kpreempt_ev_immed, EVCNT_TYPE_INTR, NULL,
	   "kpreempt", "immediate");

	/* Initialize balancing callout and run it */
#ifdef MULTIPROCESSOR
	callout_init(&balance_ch, CALLOUT_MPSAFE);
	callout_setfunc(&balance_ch, sched_balance, NULL);
	callout_schedule(&balance_ch, balance_period);
#endif
	sched_pstats(NULL);
}

SYSCTL_SETUP(sysctl_sched_setup, "sysctl sched setup")
{
	const struct sysctlnode *node = NULL;

	sysctl_createv(clog, 0, NULL, NULL,
		CTLFLAG_PERMANENT,
		CTLTYPE_NODE, "kern", NULL,
		NULL, 0, NULL, 0,
		CTL_KERN, CTL_EOL);
	sysctl_createv(clog, 0, NULL, &node,
		CTLFLAG_PERMANENT,
		CTLTYPE_NODE, "sched",
		SYSCTL_DESCR("Scheduler options"),
		NULL, 0, NULL, 0,
		CTL_KERN, CTL_CREATE, CTL_EOL);

	if (node == NULL)
		return;

	sysctl_createv(clog, 0, &node, NULL,
		CTLFLAG_PERMANENT | CTLFLAG_READWRITE,
		CTLTYPE_INT, "cacheht_time",
		SYSCTL_DESCR("Cache hotness time (in ticks)"),
		NULL, 0, &cacheht_time, 0,
		CTL_CREATE, CTL_EOL);
	sysctl_createv(clog, 0, &node, NULL,
		CTLFLAG_PERMANENT | CTLFLAG_READWRITE,
		CTLTYPE_INT, "balance_period",
		SYSCTL_DESCR("Balance period (in ticks)"),
		NULL, 0, &balance_period, 0,
		CTL_CREATE, CTL_EOL);
	sysctl_createv(clog, 0, &node, NULL,
		CTLFLAG_PERMANENT | CTLFLAG_READWRITE,
		CTLTYPE_INT, "min_catch",
		SYSCTL_DESCR("Minimal count of threads for catching"),
		NULL, 0, &min_catch, 0,
		CTL_CREATE, CTL_EOL);
	sysctl_createv(clog, 0, &node, NULL,
		CTLFLAG_PERMANENT | CTLFLAG_READWRITE,
		CTLTYPE_INT, "timesoftints",
		SYSCTL_DESCR("Track CPU time for soft interrupts"),
		NULL, 0, &softint_timing, 0,
		CTL_CREATE, CTL_EOL);
	sysctl_createv(clog, 0, &node, NULL,
		CTLFLAG_PERMANENT | CTLFLAG_READWRITE,
		CTLTYPE_INT, "kpreempt_pri",
		SYSCTL_DESCR("Minimum priority to trigger kernel preemption"),
		NULL, 0, &sched_kpreempt_pri, 0,
		CTL_CREATE, CTL_EOL);
	sysctl_createv(clog, 0, &node, NULL,
		CTLFLAG_PERMANENT | CTLFLAG_READWRITE,
		CTLTYPE_INT, "upreempt_pri",
		SYSCTL_DESCR("Minimum priority to trigger user preemption"),
		NULL, 0, &sched_upreempt_pri, 0,
		CTL_CREATE, CTL_EOL);
}

void
sched_cpuattach(struct cpu_info *ci)
{
	runqueue_t *ci_rq;
	void *rq_ptr;
	u_int i, size;

	if (ci->ci_schedstate.spc_lwplock == NULL) {
		ci->ci_schedstate.spc_lwplock =
		    mutex_obj_alloc(MUTEX_DEFAULT, IPL_SCHED);
	}
	if (ci == lwp0.l_cpu) {
		/* Initialize the scheduler structure of the primary LWP */
		lwp0.l_mutex = ci->ci_schedstate.spc_lwplock;
	}
	if (ci->ci_schedstate.spc_mutex != NULL) {
		/* Already initialized. */
		return;
	}

	/* Allocate the run queue */
	size = roundup2(sizeof(runqueue_t), coherency_unit) + coherency_unit;
	rq_ptr = kmem_zalloc(size, KM_SLEEP);
	if (rq_ptr == NULL) {
		panic("sched_cpuattach: could not allocate the runqueue");
	}
	ci_rq = (void *)(roundup2((uintptr_t)(rq_ptr), coherency_unit));

	/* Initialize run queues */
	ci->ci_schedstate.spc_mutex =
	    mutex_obj_alloc(MUTEX_DEFAULT, IPL_SCHED);
	for (i = 0; i < PRI_RT_COUNT; i++)
		TAILQ_INIT(&ci_rq->r_rt_queue[i].q_head);
	for (i = 0; i < PRI_TS_COUNT; i++)
		TAILQ_INIT(&ci_rq->r_ts_queue[i].q_head);

	ci->ci_schedstate.spc_sched_info = ci_rq;
}

/*
 * Control of the runqueue.
 */

static void *
sched_getrq(runqueue_t *ci_rq, const pri_t prio)
{

	KASSERT(prio < PRI_COUNT);
	return (prio <= PRI_HIGHEST_TS) ?
	    &ci_rq->r_ts_queue[prio].q_head :
	    &ci_rq->r_rt_queue[prio - PRI_HIGHEST_TS - 1].q_head;
}

void
sched_enqueue(struct lwp *l, bool swtch)
{
	runqueue_t *ci_rq;
	struct schedstate_percpu *spc;
	TAILQ_HEAD(, lwp) *q_head;
	const pri_t eprio = lwp_eprio(l);
	struct cpu_info *ci;
	int type;

	ci = l->l_cpu;
	spc = &ci->ci_schedstate;
	ci_rq = spc->spc_sched_info;
	KASSERT(lwp_locked(l, l->l_cpu->ci_schedstate.spc_mutex));

	/* Update the last run time on switch */
	if (__predict_true(swtch == true)) {
		l->l_rticks = hardclock_ticks;
		l->l_rticksum += (hardclock_ticks - l->l_rticks);
	} else if (l->l_rticks == 0)
		l->l_rticks = hardclock_ticks;

	/* Enqueue the thread */
	q_head = sched_getrq(ci_rq, eprio);
	if (TAILQ_EMPTY(q_head)) {
		u_int i;
		uint32_t q;

		/* Mark bit */
		i = eprio >> BITMAP_SHIFT;
		q = BITMAP_MSB >> (eprio & BITMAP_MASK);
		KASSERT((ci_rq->r_bitmap[i] & q) == 0);
		ci_rq->r_bitmap[i] |= q;
	}
	TAILQ_INSERT_TAIL(q_head, l, l_runq);
	ci_rq->r_count++;
	if ((l->l_pflag & LP_BOUND) == 0)
		ci_rq->r_mcount++;

	/*
	 * Update the value of highest priority in the runqueue,
	 * if priority of this thread is higher.
	 */
	if (eprio > spc->spc_maxpriority)
		spc->spc_maxpriority = eprio;

	sched_newts(l);

	/*
	 * Wake the chosen CPU or cause a preemption if the newly
	 * enqueued thread has higher priority.  Don't cause a 
	 * preemption if the thread is yielding (swtch).
	 */
	if (!swtch && eprio > spc->spc_curpriority) {
		if (eprio >= sched_kpreempt_pri)
			type = RESCHED_KPREEMPT;
		else if (eprio >= sched_upreempt_pri)
			type = RESCHED_IMMED;
		else
			type = 0;
		cpu_need_resched(ci, type);
	}
}

void
sched_dequeue(struct lwp *l)
{
	runqueue_t *ci_rq;
	TAILQ_HEAD(, lwp) *q_head;
	struct schedstate_percpu *spc;
	const pri_t eprio = lwp_eprio(l);

	spc = & l->l_cpu->ci_schedstate;
	ci_rq = spc->spc_sched_info;
	KASSERT(lwp_locked(l, spc->spc_mutex));

	KASSERT(eprio <= spc->spc_maxpriority); 
	KASSERT(ci_rq->r_bitmap[eprio >> BITMAP_SHIFT] != 0);
	KASSERT(ci_rq->r_count > 0);

	ci_rq->r_count--;
	if ((l->l_pflag & LP_BOUND) == 0)
		ci_rq->r_mcount--;

	q_head = sched_getrq(ci_rq, eprio);
	TAILQ_REMOVE(q_head, l, l_runq);
	if (TAILQ_EMPTY(q_head)) {
		u_int i;
		uint32_t q;

		/* Unmark bit */
		i = eprio >> BITMAP_SHIFT;
		q = BITMAP_MSB >> (eprio & BITMAP_MASK);
		KASSERT((ci_rq->r_bitmap[i] & q) != 0);
		ci_rq->r_bitmap[i] &= ~q;

		/*
		 * Update the value of highest priority in the runqueue, in a
		 * case it was a last thread in the queue of highest priority.
		 */
		if (eprio != spc->spc_maxpriority)
			return;

		do {
			if (ci_rq->r_bitmap[i] != 0) {
				q = ffs(ci_rq->r_bitmap[i]);
				spc->spc_maxpriority =
				    (i << BITMAP_SHIFT) + (BITMAP_BITS - q);
				return;
			}
		} while (i--);

		/* If not found - set the lowest value */
		spc->spc_maxpriority = 0;
	}
}

/*
 * Migration and balancing.
 */

#ifdef MULTIPROCESSOR

/* Estimate if LWP is cache-hot */
static inline bool
lwp_cache_hot(const struct lwp *l)
{

	if (l->l_slptime || l->l_rticks == 0)
		return false;

	return (hardclock_ticks - l->l_rticks <= cacheht_time);
}

/* Check if LWP can migrate to the chosen CPU */
static inline bool
sched_migratable(const struct lwp *l, struct cpu_info *ci)
{
	const struct schedstate_percpu *spc = &ci->ci_schedstate;

	/* CPU is offline */
	if (__predict_false(spc->spc_flags & SPCF_OFFLINE))
		return false;

	/* Affinity bind */
	if (__predict_false(l->l_flag & LW_AFFINITY))
		return CPU_ISSET(cpu_index(ci), &l->l_affinity);

	/* Processor-set */
	return (spc->spc_psid == l->l_psid);
}

/*
 * Estimate the migration of LWP to the other CPU.
 * Take and return the CPU, if migration is needed.
 */
struct cpu_info *
sched_takecpu(struct lwp *l)
{
	struct cpu_info *ci, *tci, *first, *next;
	struct schedstate_percpu *spc;
	runqueue_t *ci_rq, *ici_rq;
	pri_t eprio, lpri, pri;

	KASSERT(lwp_locked(l, NULL));

	ci = l->l_cpu;
	spc = &ci->ci_schedstate;
	ci_rq = spc->spc_sched_info;

	/* If thread is strictly bound, do not estimate other CPUs */
	if (l->l_pflag & LP_BOUND)
		return ci;

	/* CPU of this thread is idling - run there */
	if (ci_rq->r_count == 0)
		return ci;

	eprio = lwp_eprio(l);

	/* Stay if thread is cache-hot */
	if (__predict_true(l->l_stat != LSIDL) &&
	    lwp_cache_hot(l) && eprio >= spc->spc_curpriority)
		return ci;

	/* Run on current CPU if priority of thread is higher */
	ci = curcpu();
	spc = &ci->ci_schedstate;
	if (eprio > spc->spc_curpriority && sched_migratable(l, ci))
		return ci;

	/*
	 * Look for the CPU with the lowest priority thread.  In case of
	 * equal priority, choose the CPU with the fewest of threads.
	 */
	first = l->l_cpu;
	ci = first;
	tci = first;
	lpri = PRI_COUNT;
	do {
		next = CIRCLEQ_LOOP_NEXT(&cpu_queue, ci, ci_data.cpu_qchain);
		spc = &ci->ci_schedstate;
		ici_rq = spc->spc_sched_info;
		pri = max(spc->spc_curpriority, spc->spc_maxpriority);
		if (pri > lpri)
			continue;

		if (pri == lpri && ci_rq->r_count < ici_rq->r_count)
			continue;

		if (!sched_migratable(l, ci))
			continue;

		lpri = pri;
		tci = ci;
		ci_rq = ici_rq;
	} while (ci = next, ci != first);

	return tci;
}

/*
 * Tries to catch an LWP from the runqueue of other CPU.
 */
static struct lwp *
sched_catchlwp(void)
{
	struct cpu_info *curci = curcpu(), *ci = worker_ci;
	struct schedstate_percpu *spc;
	TAILQ_HEAD(, lwp) *q_head;
	runqueue_t *ci_rq;
	struct lwp *l;

	if (curci == ci)
		return NULL;

	/* Lockless check */
	spc = &ci->ci_schedstate;
	ci_rq = spc->spc_sched_info;
	if (ci_rq->r_mcount < min_catch)
		return NULL;

	/*
	 * Double-lock the runqueues.
	 */
	if (curci < ci) {
		spc_lock(ci);
	} else if (!mutex_tryenter(ci->ci_schedstate.spc_mutex)) {
		const runqueue_t *cur_rq = curci->ci_schedstate.spc_sched_info;

		spc_unlock(curci);
		spc_lock(ci);
		spc_lock(curci);

		if (cur_rq->r_count) {
			spc_unlock(ci);
			return NULL;
		}
	}

	if (ci_rq->r_mcount < min_catch) {
		spc_unlock(ci);
		return NULL;
	}

	/* Take the highest priority thread */
	q_head = sched_getrq(ci_rq, spc->spc_maxpriority);
	l = TAILQ_FIRST(q_head);

	for (;;) {
		/* Check the first and next result from the queue */
		if (l == NULL)
			break;
		KASSERT(l->l_stat == LSRUN);
		KASSERT(l->l_flag & LW_INMEM);

		/* Look for threads, whose are allowed to migrate */
		if ((l->l_pflag & LP_BOUND) || lwp_cache_hot(l) ||
		    !sched_migratable(l, curci)) {
			l = TAILQ_NEXT(l, l_runq);
			continue;
		}

		/* Grab the thread, and move to the local run queue */
		sched_dequeue(l);
		l->l_cpu = curci;
		lwp_unlock_to(l, curci->ci_schedstate.spc_mutex);
		sched_enqueue(l, false);
		return l;
	}
	spc_unlock(ci);

	return l;
}

/*
 * Periodical calculations for balancing.
 */
static void
sched_balance(void *nocallout)
{
	struct cpu_info *ci, *hci;
	runqueue_t *ci_rq;
	CPU_INFO_ITERATOR cii;
	u_int highest;

	hci = curcpu();
	highest = 0;

	/* Make lockless countings */
	for (CPU_INFO_FOREACH(cii, ci)) {
		ci_rq = ci->ci_schedstate.spc_sched_info;

		/* Average count of the threads */
		ci_rq->r_avgcount = (ci_rq->r_avgcount + ci_rq->r_mcount) >> 1;

		/* Look for CPU with the highest average */
		if (ci_rq->r_avgcount > highest) {
			hci = ci;
			highest = ci_rq->r_avgcount;
		}
	}

	/* Update the worker */
	worker_ci = hci;

	if (nocallout == NULL)
		callout_schedule(&balance_ch, balance_period);
}

#else

struct cpu_info *
sched_takecpu(struct lwp *l)
{

	return l->l_cpu;
}

#endif	/* MULTIPROCESSOR */

/*
 * Scheduler mill.
 */
struct lwp *
sched_nextlwp(void)
{
	struct cpu_info *ci = curcpu();
	struct schedstate_percpu *spc;
	TAILQ_HEAD(, lwp) *q_head;
	runqueue_t *ci_rq;
	struct lwp *l;

	spc = &ci->ci_schedstate;
	ci_rq = spc->spc_sched_info;

#ifdef MULTIPROCESSOR
	/* If runqueue is empty, try to catch some thread from other CPU */
	if (__predict_false(spc->spc_flags & SPCF_OFFLINE)) {
		if ((ci_rq->r_count - ci_rq->r_mcount) == 0)
			return NULL;
	} else if (ci_rq->r_count == 0) {
		/* Reset the counter, and call the balancer */
		ci_rq->r_avgcount = 0;
		sched_balance(ci);

		/* The re-locking will be done inside */
		return sched_catchlwp();
	}
#else
	if (ci_rq->r_count == 0)
		return NULL;
#endif

	/* Take the highest priority thread */
	KASSERT(ci_rq->r_bitmap[spc->spc_maxpriority >> BITMAP_SHIFT]);
	q_head = sched_getrq(ci_rq, spc->spc_maxpriority);
	l = TAILQ_FIRST(q_head);
	KASSERT(l != NULL);

	sched_oncpu(l);
	l->l_rticks = hardclock_ticks;

	return l;
}

bool
sched_curcpu_runnable_p(void)
{
	const struct cpu_info *ci;
	const runqueue_t *ci_rq;
	bool rv;

	kpreempt_disable();
	ci = curcpu();
	ci_rq = ci->ci_schedstate.spc_sched_info;

#ifndef __HAVE_FAST_SOFTINTS
	if (ci->ci_data.cpu_softints) {
		kpreempt_enable();
		return true;
	}
#endif

	if (ci->ci_schedstate.spc_flags & SPCF_OFFLINE)
		rv = (ci_rq->r_count - ci_rq->r_mcount);
	else
		rv = ci_rq->r_count != 0;
	kpreempt_enable();

	return rv;
}

/*
 * Debugging.
 */

#ifdef DDB

void
sched_print_runqueue(void (*pr)(const char *, ...)
    __attribute__((__format__(__printf__,1,2))))
{
	runqueue_t *ci_rq;
	struct schedstate_percpu *spc;
	struct lwp *l;
	struct proc *p;
	int i;
	struct cpu_info *ci;
	CPU_INFO_ITERATOR cii;

	for (CPU_INFO_FOREACH(cii, ci)) {
		spc = &ci->ci_schedstate;
		ci_rq = spc->spc_sched_info;

		(*pr)("Run-queue (CPU = %u):\n", ci->ci_index);
		(*pr)(" pid.lid = %d.%d, threads count = %u, "
		    "avgcount = %u, highest pri = %d\n",
#ifdef MULTIPROCESSOR
		    ci->ci_curlwp->l_proc->p_pid, ci->ci_curlwp->l_lid,
#else
		    curlwp->l_proc->p_pid, curlwp->l_lid,
#endif
		    ci_rq->r_count, ci_rq->r_avgcount, spc->spc_maxpriority);
		i = (PRI_COUNT >> BITMAP_SHIFT) - 1;
		do {
			uint32_t q;
			q = ci_rq->r_bitmap[i];
			(*pr)(" bitmap[%d] => [ %d (0x%x) ]\n", i, ffs(q), q);
		} while (i--);
	}

	(*pr)("   %5s %4s %4s %10s %3s %18s %4s %s\n",
	    "LID", "PRI", "EPRI", "FL", "ST", "LWP", "CPU", "LRTIME");

	PROCLIST_FOREACH(p, &allproc) {
		(*pr)(" /- %d (%s)\n", (int)p->p_pid, p->p_comm);
		LIST_FOREACH(l, &p->p_lwps, l_sibling) {
			ci = l->l_cpu;
			(*pr)(" | %5d %4u %4u 0x%8.8x %3s %18p %4u %u\n",
			    (int)l->l_lid, l->l_priority, lwp_eprio(l),
			    l->l_flag, l->l_stat == LSRUN ? "RQ" :
			    (l->l_stat == LSSLEEP ? "SQ" : "-"),
			    l, ci->ci_index,
			    (u_int)(hardclock_ticks - l->l_rticks));
		}
	}
}

#endif
