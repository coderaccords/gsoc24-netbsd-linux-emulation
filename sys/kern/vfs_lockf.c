/*	$NetBSD: vfs_lockf.c,v 1.38 2005/03/20 19:16:33 christos Exp $	*/

/*
 * Copyright (c) 1982, 1986, 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Scooter Morris at Genentech Inc.
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
 *	@(#)ufs_lockf.c	8.4 (Berkeley) 10/26/94
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: vfs_lockf.c,v 1.38 2005/03/20 19:16:33 christos Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/file.h>
#include <sys/proc.h>
#include <sys/vnode.h>
#include <sys/pool.h>
#include <sys/fcntl.h>
#include <sys/lockf.h>

POOL_INIT(lockfpool, sizeof(struct lockf), 0, 0, 0, "lockfpl",
    &pool_allocator_nointr);

/*
 * This variable controls the maximum number of processes that will
 * be checked in doing deadlock detection.
 */
int maxlockdepth = MAXDEPTH;

#ifdef LOCKF_DEBUG
int	lockf_debug = 0;
#endif

#define NOLOCKF (struct lockf *)0
#define SELF	0x1
#define OTHERS	0x2

static int lf_clearlock(struct lockf *, struct lockf **);
static int lf_findoverlap(struct lockf *,
	    struct lockf *, int, struct lockf ***, struct lockf **);
static struct lockf *lf_getblock(struct lockf *);
static int lf_getlock(struct lockf *, struct flock *);
static int lf_setlock(struct lockf *, struct lockf **, struct simplelock *);
static void lf_split(struct lockf *, struct lockf *, struct lockf **);
static void lf_wakelock(struct lockf *);
static struct lockf *lf_alloc(uid_t, int);
static void lf_free(struct lockf *);


#ifdef LOCKF_DEBUG
static void lf_print(char *, struct lockf *);
static void lf_printlist(char *, struct lockf *);
#endif

/*
 * XXX TODO
 * Misc cleanups: "caddr_t id" should be visible in the API as a
 * "struct proc *".
 * (This requires rototilling all VFS's which support advisory locking).
 */

/*
 * If there's a lot of lock contention on a single vnode, locking
 * schemes which allow for more paralleism would be needed.  Given how
 * infrequently byte-range locks are actually used in typical BSD
 * code, a more complex approach probably isn't worth it.
 */

/*
 * We enforce a limit on locks by uid, so that a single user cannot
 * run the kernel out of memory.  For now, the limit is pretty coarse.
 * There is no limit on root.
 *
 * Splitting a lock will always succeed, regardless of current allocations.
 * If you're slightly above the limit, we still have to permit an allocation
 * so that the unlock can succeed.  If the unlocking causes too many splits,
 * however, you're totally cutoff.
 */
int maxlocksperuid = 1024;

/*
 * 3 options for allowfail.
 * 0 - always allocate.  1 - cutoff at limit.  2 - cutoff at double limit.
 */
struct lockf *
lf_alloc(uid_t uid, int allowfail)
{
	struct uidinfo *uip;
	struct lockf *lock;

	uip = uid_find(uid);
	if (uid && allowfail && uip->ui_lockcnt >
	    (allowfail == 1 ? maxlocksperuid : (maxlocksperuid * 2)))
		return (NULL);
	uip->ui_lockcnt++;
	lock = pool_get(&lockfpool, PR_WAITOK);
	lock->lf_uid = uid;
	return (lock);
}

void
lf_free(struct lockf *lock)
{
	struct uidinfo *uip;

	uip = uid_find(lock->lf_uid);
	uip->ui_lockcnt--;
	pool_put(&lockfpool, lock);
}

/*
 * Do an advisory lock operation.
 */
int
lf_advlock(struct vop_advlock_args *ap, struct lockf **head, off_t size)
{
	struct proc *p = curproc;
	struct flock *fl = ap->a_fl;
	struct lockf *lock = NULL;
	struct lockf *sparelock;
	struct simplelock *interlock = &ap->a_vp->v_interlock;
	off_t start, end;
	int error = 0;

	/*
	 * Convert the flock structure into a start and end.
	 */
	switch (fl->l_whence) {
	case SEEK_SET:
	case SEEK_CUR:
		/*
		 * Caller is responsible for adding any necessary offset
		 * when SEEK_CUR is used.
		 */
		start = fl->l_start;
		break;

	case SEEK_END:
		start = size + fl->l_start;
		break;

	default:
		return EINVAL;
	}
	if (start < 0)
		return EINVAL;

	/*
	 * allocate locks before acquire simple lock.
	 * we need two locks in the worst case.
	 */
	switch (ap->a_op) {
	case F_SETLK:
	case F_UNLCK:
		/*
		 * XXX for F_UNLCK case, we can re-use lock.
		 */
		if ((fl->l_type & F_FLOCK) == 0) {
			/*
			 * byte-range lock might need one more lock.
			 */
			sparelock = lf_alloc(p->p_ucred->cr_uid, 0);
			if (sparelock == NULL) {
				error = ENOMEM;
				goto quit;
			}
			break;
		}
		/* FALLTHROUGH */

	case F_GETLK:
		sparelock = NULL;
		break;

	default:
		return EINVAL;
	}

	lock = lf_alloc(p->p_ucred->cr_uid, ap->a_op != F_UNLCK ? 1 : 2);
	if (lock == NULL) {
		error = ENOMEM;
		goto quit;
	}

	simple_lock(interlock);

	/*
	 * Avoid the common case of unlocking when inode has no locks.
	 */
	if (*head == (struct lockf *)0) {
		if (ap->a_op != F_SETLK) {
			fl->l_type = F_UNLCK;
			error = 0;
			goto quit_unlock;
		}
	}

	if (fl->l_len == 0)
		end = -1;
	else
		end = start + fl->l_len - 1;
	/*
	 * Create the lockf structure.
	 */
	lock->lf_start = start;
	lock->lf_end = end;
	/* XXX NJWLWP
	 * I don't want to make the entire VFS universe use LWPs, because
	 * they don't need them, for the most part. This is an exception,
	 * and a kluge.
	 */

	lock->lf_head = head;
	lock->lf_type = fl->l_type;
	lock->lf_next = (struct lockf *)0;
	TAILQ_INIT(&lock->lf_blkhd);
	lock->lf_flags = ap->a_flags;
	if (lock->lf_flags & F_POSIX) {
		KASSERT(curproc == (struct proc *)ap->a_id);
	}
	lock->lf_id = (struct proc *)ap->a_id;
	lock->lf_lwp = curlwp;

	/*
	 * Do the requested operation.
	 */
	switch (ap->a_op) {

	case F_SETLK:
		error = lf_setlock(lock, &sparelock, interlock);
		lock = NULL; /* lf_setlock freed it */
		break;

	case F_UNLCK:
		error = lf_clearlock(lock, &sparelock);
		break;

	case F_GETLK:
		error = lf_getlock(lock, fl);
		break;

	default:
		break;
		/* NOTREACHED */
	}

quit_unlock:
	simple_unlock(interlock);
quit:
	if (lock)
		lf_free(lock);
	if (sparelock)
		lf_free(sparelock);

	return error;
}

/*
 * Set a byte-range lock.
 */
static int
lf_setlock(struct lockf *lock, struct lockf **sparelock,
    struct simplelock *interlock)
{
	struct lockf *block;
	struct lockf **head = lock->lf_head;
	struct lockf **prev, *overlap, *ltmp;
	static char lockstr[] = "lockf";
	int ovcase, priority, needtolink, error;

#ifdef LOCKF_DEBUG
	if (lockf_debug & 1)
		lf_print("lf_setlock", lock);
#endif /* LOCKF_DEBUG */

	/*
	 * Set the priority
	 */
	priority = PLOCK;
	if (lock->lf_type == F_WRLCK)
		priority += 4;
	priority |= PCATCH;
	/*
	 * Scan lock list for this file looking for locks that would block us.
	 */
	while ((block = lf_getblock(lock)) != NULL) {
		/*
		 * Free the structure and return if nonblocking.
		 */
		if ((lock->lf_flags & F_WAIT) == 0) {
			lf_free(lock);
			return EAGAIN;
		}
		/*
		 * We are blocked. Since flock style locks cover
		 * the whole file, there is no chance for deadlock.
		 * For byte-range locks we must check for deadlock.
		 *
		 * Deadlock detection is done by looking through the
		 * wait channels to see if there are any cycles that
		 * involve us. MAXDEPTH is set just to make sure we
		 * do not go off into neverneverland.
		 */
		if ((lock->lf_flags & F_POSIX) &&
		    (block->lf_flags & F_POSIX)) {
			struct lwp *wlwp;
			struct lockf *waitblock;
			int i = 0;

			/*
			 * The block is waiting on something.  if_lwp will be
			 * 0 once the lock is granted, so we terminate the
			 * loop if we find this.
			 */
			wlwp = block->lf_lwp;
			while (wlwp && (i++ < maxlockdepth)) {
				waitblock = (struct lockf *)wlwp->l_wchan;
				/* Get the owner of the blocking lock */
				waitblock = waitblock->lf_next;
				if ((waitblock->lf_flags & F_POSIX) == 0)
					break;
				wlwp = waitblock->lf_lwp;
				if (wlwp == lock->lf_lwp) {
					lf_free(lock);
					return EDEADLK;
				}
			}
			/*
			 * If we're still following a dependency chain
			 * after maxlockdepth iterations, assume we're in
			 * a cycle to be safe.
			 */
			if (i >= maxlockdepth) {
				lf_free(lock);
				return EDEADLK;
			}
		}
		/*
		 * For flock type locks, we must first remove
		 * any shared locks that we hold before we sleep
		 * waiting for an exclusive lock.
		 */
		if ((lock->lf_flags & F_FLOCK) &&
		    lock->lf_type == F_WRLCK) {
			lock->lf_type = F_UNLCK;
			(void) lf_clearlock(lock, NULL);
			lock->lf_type = F_WRLCK;
		}
		/*
		 * Add our lock to the blocked list and sleep until we're free.
		 * Remember who blocked us (for deadlock detection).
		 */
		lock->lf_next = block;
		TAILQ_INSERT_TAIL(&block->lf_blkhd, lock, lf_block);
#ifdef LOCKF_DEBUG
		if (lockf_debug & 1) {
			lf_print("lf_setlock: blocking on", block);
			lf_printlist("lf_setlock", block);
		}
#endif /* LOCKF_DEBUG */
		error = ltsleep(lock, priority, lockstr, 0, interlock);

		/*
		 * We may have been awakened by a signal (in
		 * which case we must remove ourselves from the
		 * blocked list) and/or by another process
		 * releasing a lock (in which case we have already
		 * been removed from the blocked list and our
		 * lf_next field set to NOLOCKF).
		 */
		if (lock->lf_next != NOLOCKF) {
			TAILQ_REMOVE(&lock->lf_next->lf_blkhd, lock, lf_block);
			lock->lf_next = NOLOCKF;
		}
		if (error) {
			lf_free(lock);
			return error;
		}
	}
	/*
	 * No blocks!!  Add the lock.  Note that we will
	 * downgrade or upgrade any overlapping locks this
	 * process already owns.
	 *
	 * Skip over locks owned by other processes.
	 * Handle any locks that overlap and are owned by ourselves.
	 */
	lock->lf_lwp = 0;
	prev = head;
	block = *head;
	needtolink = 1;
	for (;;) {
		ovcase = lf_findoverlap(block, lock, SELF, &prev, &overlap);
		if (ovcase)
			block = overlap->lf_next;
		/*
		 * Six cases:
		 *	0) no overlap
		 *	1) overlap == lock
		 *	2) overlap contains lock
		 *	3) lock contains overlap
		 *	4) overlap starts before lock
		 *	5) overlap ends after lock
		 */
		switch (ovcase) {
		case 0: /* no overlap */
			if (needtolink) {
				*prev = lock;
				lock->lf_next = overlap;
			}
			break;

		case 1: /* overlap == lock */
			/*
			 * If downgrading lock, others may be
			 * able to acquire it.
			 */
			if (lock->lf_type == F_RDLCK &&
			    overlap->lf_type == F_WRLCK)
				lf_wakelock(overlap);
			overlap->lf_type = lock->lf_type;
			lf_free(lock);
			lock = overlap; /* for debug output below */
			break;

		case 2: /* overlap contains lock */
			/*
			 * Check for common starting point and different types.
			 */
			if (overlap->lf_type == lock->lf_type) {
				lf_free(lock);
				lock = overlap; /* for debug output below */
				break;
			}
			if (overlap->lf_start == lock->lf_start) {
				*prev = lock;
				lock->lf_next = overlap;
				overlap->lf_start = lock->lf_end + 1;
			} else
				lf_split(overlap, lock, sparelock);
			lf_wakelock(overlap);
			break;

		case 3: /* lock contains overlap */
			/*
			 * If downgrading lock, others may be able to
			 * acquire it, otherwise take the list.
			 */
			if (lock->lf_type == F_RDLCK &&
			    overlap->lf_type == F_WRLCK) {
				lf_wakelock(overlap);
			} else {
				while ((ltmp = TAILQ_FIRST(&overlap->lf_blkhd))) {
					KASSERT(ltmp->lf_next == overlap);
					TAILQ_REMOVE(&overlap->lf_blkhd, ltmp,
					    lf_block);
					ltmp->lf_next = lock;
					TAILQ_INSERT_TAIL(&lock->lf_blkhd,
					    ltmp, lf_block);
				}
			}
			/*
			 * Add the new lock if necessary and delete the overlap.
			 */
			if (needtolink) {
				*prev = lock;
				lock->lf_next = overlap->lf_next;
				prev = &lock->lf_next;
				needtolink = 0;
			} else
				*prev = overlap->lf_next;
			lf_free(lock);
			continue;

		case 4: /* overlap starts before lock */
			/*
			 * Add lock after overlap on the list.
			 */
			lock->lf_next = overlap->lf_next;
			overlap->lf_next = lock;
			overlap->lf_end = lock->lf_start - 1;
			prev = &lock->lf_next;
			lf_wakelock(overlap);
			needtolink = 0;
			continue;

		case 5: /* overlap ends after lock */
			/*
			 * Add the new lock before overlap.
			 */
			if (needtolink) {
				*prev = lock;
				lock->lf_next = overlap;
			}
			overlap->lf_start = lock->lf_end + 1;
			lf_wakelock(overlap);
			break;
		}
		break;
	}
#ifdef LOCKF_DEBUG
	if (lockf_debug & 1) {
		lf_print("lf_setlock: got the lock", lock);
		lf_printlist("lf_setlock", lock);
	}
#endif /* LOCKF_DEBUG */
	return 0;
}

/*
 * Remove a byte-range lock on an inode.
 *
 * Generally, find the lock (or an overlap to that lock)
 * and remove it (or shrink it), then wakeup anyone we can.
 */
static int
lf_clearlock(struct lockf *unlock, struct lockf **sparelock)
{
	struct lockf **head = unlock->lf_head;
	struct lockf *lf = *head;
	struct lockf *overlap, **prev;
	int ovcase;

	if (lf == NOLOCKF)
		return 0;
#ifdef LOCKF_DEBUG
	if (unlock->lf_type != F_UNLCK)
		panic("lf_clearlock: bad type");
	if (lockf_debug & 1)
		lf_print("lf_clearlock", unlock);
#endif /* LOCKF_DEBUG */
	prev = head;
	while ((ovcase = lf_findoverlap(lf, unlock, SELF,
					&prev, &overlap)) != 0) {
		/*
		 * Wakeup the list of locks to be retried.
		 */
		lf_wakelock(overlap);

		switch (ovcase) {

		case 1: /* overlap == lock */
			*prev = overlap->lf_next;
			lf_free(overlap);
			break;

		case 2: /* overlap contains lock: split it */
			if (overlap->lf_start == unlock->lf_start) {
				overlap->lf_start = unlock->lf_end + 1;
				break;
			}
			lf_split(overlap, unlock, sparelock);
			overlap->lf_next = unlock->lf_next;
			break;

		case 3: /* lock contains overlap */
			*prev = overlap->lf_next;
			lf = overlap->lf_next;
			lf_free(overlap);
			continue;

		case 4: /* overlap starts before lock */
			overlap->lf_end = unlock->lf_start - 1;
			prev = &overlap->lf_next;
			lf = overlap->lf_next;
			continue;

		case 5: /* overlap ends after lock */
			overlap->lf_start = unlock->lf_end + 1;
			break;
		}
		break;
	}
#ifdef LOCKF_DEBUG
	if (lockf_debug & 1)
		lf_printlist("lf_clearlock", unlock);
#endif /* LOCKF_DEBUG */
	return 0;
}

/*
 * Check whether there is a blocking lock,
 * and if so return its process identifier.
 */
static int
lf_getlock(struct lockf *lock, struct flock *fl)
{
	struct lockf *block;

#ifdef LOCKF_DEBUG
	if (lockf_debug & 1)
		lf_print("lf_getlock", lock);
#endif /* LOCKF_DEBUG */

	if ((block = lf_getblock(lock)) != NULL) {
		fl->l_type = block->lf_type;
		fl->l_whence = SEEK_SET;
		fl->l_start = block->lf_start;
		if (block->lf_end == -1)
			fl->l_len = 0;
		else
			fl->l_len = block->lf_end - block->lf_start + 1;
		if (block->lf_flags & F_POSIX)
			fl->l_pid = ((struct proc *)block->lf_id)->p_pid;
		else
			fl->l_pid = -1;
	} else {
		fl->l_type = F_UNLCK;
	}
	return 0;
}

/*
 * Walk the list of locks for an inode and
 * return the first blocking lock.
 */
static struct lockf *
lf_getblock(struct lockf *lock)
{
	struct lockf **prev, *overlap, *lf = *(lock->lf_head);

	prev = lock->lf_head;
	while (lf_findoverlap(lf, lock, OTHERS, &prev, &overlap) != 0) {
		/*
		 * We've found an overlap, see if it blocks us
		 */
		if ((lock->lf_type == F_WRLCK || overlap->lf_type == F_WRLCK))
			return overlap;
		/*
		 * Nope, point to the next one on the list and
		 * see if it blocks us
		 */
		lf = overlap->lf_next;
	}
	return NOLOCKF;
}

/*
 * Walk the list of locks for an inode to
 * find an overlapping lock (if any).
 *
 * NOTE: this returns only the FIRST overlapping lock.  There
 *	 may be more than one.
 */
static int
lf_findoverlap(struct lockf *lf, struct lockf *lock, int type,
    struct lockf ***prev, struct lockf **overlap)
{
	off_t start, end;

	*overlap = lf;
	if (lf == NOLOCKF)
		return 0;
#ifdef LOCKF_DEBUG
	if (lockf_debug & 2)
		lf_print("lf_findoverlap: looking for overlap in", lock);
#endif /* LOCKF_DEBUG */
	start = lock->lf_start;
	end = lock->lf_end;
	while (lf != NOLOCKF) {
		if (((type == SELF) && lf->lf_id != lock->lf_id) ||
		    ((type == OTHERS) && lf->lf_id == lock->lf_id)) {
			*prev = &lf->lf_next;
			*overlap = lf = lf->lf_next;
			continue;
		}
#ifdef LOCKF_DEBUG
		if (lockf_debug & 2)
			lf_print("\tchecking", lf);
#endif /* LOCKF_DEBUG */
		/*
		 * OK, check for overlap
		 *
		 * Six cases:
		 *	0) no overlap
		 *	1) overlap == lock
		 *	2) overlap contains lock
		 *	3) lock contains overlap
		 *	4) overlap starts before lock
		 *	5) overlap ends after lock
		 */
		if ((lf->lf_end != -1 && start > lf->lf_end) ||
		    (end != -1 && lf->lf_start > end)) {
			/* Case 0 */
#ifdef LOCKF_DEBUG
			if (lockf_debug & 2)
				printf("no overlap\n");
#endif /* LOCKF_DEBUG */
			if ((type & SELF) && end != -1 && lf->lf_start > end)
				return 0;
			*prev = &lf->lf_next;
			*overlap = lf = lf->lf_next;
			continue;
		}
		if ((lf->lf_start == start) && (lf->lf_end == end)) {
			/* Case 1 */
#ifdef LOCKF_DEBUG
			if (lockf_debug & 2)
				printf("overlap == lock\n");
#endif /* LOCKF_DEBUG */
			return 1;
		}
		if ((lf->lf_start <= start) &&
		    (end != -1) &&
		    ((lf->lf_end >= end) || (lf->lf_end == -1))) {
			/* Case 2 */
#ifdef LOCKF_DEBUG
			if (lockf_debug & 2)
				printf("overlap contains lock\n");
#endif /* LOCKF_DEBUG */
			return 2;
		}
		if (start <= lf->lf_start &&
		           (end == -1 ||
			   (lf->lf_end != -1 && end >= lf->lf_end))) {
			/* Case 3 */
#ifdef LOCKF_DEBUG
			if (lockf_debug & 2)
				printf("lock contains overlap\n");
#endif /* LOCKF_DEBUG */
			return 3;
		}
		if ((lf->lf_start < start) &&
			((lf->lf_end >= start) || (lf->lf_end == -1))) {
			/* Case 4 */
#ifdef LOCKF_DEBUG
			if (lockf_debug & 2)
				printf("overlap starts before lock\n");
#endif /* LOCKF_DEBUG */
			return 4;
		}
		if ((lf->lf_start > start) &&
			(end != -1) &&
			((lf->lf_end > end) || (lf->lf_end == -1))) {
			/* Case 5 */
#ifdef LOCKF_DEBUG
			if (lockf_debug & 2)
				printf("overlap ends after lock\n");
#endif /* LOCKF_DEBUG */
			return 5;
		}
		panic("lf_findoverlap: default");
	}
	return 0;
}

/*
 * Split a lock and a contained region into
 * two or three locks as necessary.
 */
static void
lf_split(struct lockf *lock1, struct lockf *lock2, struct lockf **sparelock)
{
	struct lockf *splitlock;

#ifdef LOCKF_DEBUG
	if (lockf_debug & 2) {
		lf_print("lf_split", lock1);
		lf_print("splitting from", lock2);
	}
#endif /* LOCKF_DEBUG */
	/*
	 * Check to see if spliting into only two pieces.
	 */
	if (lock1->lf_start == lock2->lf_start) {
		lock1->lf_start = lock2->lf_end + 1;
		lock2->lf_next = lock1;
		return;
	}
	if (lock1->lf_end == lock2->lf_end) {
		lock1->lf_end = lock2->lf_start - 1;
		lock2->lf_next = lock1->lf_next;
		lock1->lf_next = lock2;
		return;
	}
	/*
	 * Make a new lock consisting of the last part of
	 * the encompassing lock
	 */
	splitlock = *sparelock;
	*sparelock = NULL;
	memcpy(splitlock, lock1, sizeof(*splitlock));
	splitlock->lf_start = lock2->lf_end + 1;
	TAILQ_INIT(&splitlock->lf_blkhd);
	lock1->lf_end = lock2->lf_start - 1;
	/*
	 * OK, now link it in
	 */
	splitlock->lf_next = lock1->lf_next;
	lock2->lf_next = splitlock;
	lock1->lf_next = lock2;
}

/*
 * Wakeup a blocklist
 */
static void
lf_wakelock(struct lockf *listhead)
{
	struct lockf *wakelock;

	while ((wakelock = TAILQ_FIRST(&listhead->lf_blkhd))) {
		KASSERT(wakelock->lf_next == listhead);
		TAILQ_REMOVE(&listhead->lf_blkhd, wakelock, lf_block);
		wakelock->lf_next = NOLOCKF;
#ifdef LOCKF_DEBUG
		if (lockf_debug & 2)
			lf_print("lf_wakelock: awakening", wakelock);
#endif
		wakeup(wakelock);
	}
}

#ifdef LOCKF_DEBUG
/*
 * Print out a lock.
 */
static void
lf_print(char *tag, struct lockf *lock)
{

	printf("%s: lock %p for ", tag, lock);
	if (lock->lf_flags & F_POSIX)
		printf("proc %d", ((struct proc *)lock->lf_id)->p_pid);
	else
		printf("file 0x%p", (struct file *)lock->lf_id);
	printf(" %s, start %qx, end %qx",
		lock->lf_type == F_RDLCK ? "shared" :
		lock->lf_type == F_WRLCK ? "exclusive" :
		lock->lf_type == F_UNLCK ? "unlock" :
		"unknown", lock->lf_start, lock->lf_end);
	if (TAILQ_FIRST(&lock->lf_blkhd))
		printf(" block %p\n", TAILQ_FIRST(&lock->lf_blkhd));
	else
		printf("\n");
}

static void
lf_printlist(char *tag, struct lockf *lock)
{
	struct lockf *lf, *blk;

	printf("%s: Lock list:\n", tag);
	for (lf = *lock->lf_head; lf; lf = lf->lf_next) {
		printf("\tlock %p for ", lf);
		if (lf->lf_flags & F_POSIX)
			printf("proc %d", ((struct proc *)lf->lf_id)->p_pid);
		else
			printf("file 0x%p", (struct file *)lf->lf_id);
		printf(", %s, start %qx, end %qx",
			lf->lf_type == F_RDLCK ? "shared" :
			lf->lf_type == F_WRLCK ? "exclusive" :
			lf->lf_type == F_UNLCK ? "unlock" :
			"unknown", lf->lf_start, lf->lf_end);
		TAILQ_FOREACH(blk, &lf->lf_blkhd, lf_block) {
			if (blk->lf_flags & F_POSIX)
				printf("proc %d",
				    ((struct proc *)blk->lf_id)->p_pid);
			else
				printf("file 0x%p", (struct file *)blk->lf_id);
			printf(", %s, start %qx, end %qx",
				blk->lf_type == F_RDLCK ? "shared" :
				blk->lf_type == F_WRLCK ? "exclusive" :
				blk->lf_type == F_UNLCK ? "unlock" :
				"unknown", blk->lf_start, blk->lf_end);
			if (TAILQ_FIRST(&blk->lf_blkhd))
				 panic("lf_printlist: bad list");
		}
		printf("\n");
	}
}
#endif /* LOCKF_DEBUG */
