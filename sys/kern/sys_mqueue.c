/*	$NetBSD: sys_mqueue.c,v 1.31 2011/01/18 20:32:53 rmind Exp $	*/

/*
 * Copyright (c) 2007-2009 Mindaugas Rasiukevicius <rmind at NetBSD org>
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

/*
 * Implementation of POSIX message queues.
 * Defined in the Base Definitions volume of IEEE Std 1003.1-2001.
 *
 * Locking
 * 
 * Global list of message queues (mqueue_head) and proc_t::p_mqueue_cnt
 * counter are protected by mqlist_mtx lock.  The very message queue and
 * its members are protected by mqueue::mq_mtx.
 * 
 * Lock order:
 * 	mqlist_mtx ->
 * 		mqueue::mq_mtx
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: sys_mqueue.c,v 1.31 2011/01/18 20:32:53 rmind Exp $");

#include <sys/param.h>
#include <sys/types.h>
#include <sys/condvar.h>
#include <sys/errno.h>
#include <sys/fcntl.h>
#include <sys/file.h>
#include <sys/filedesc.h>
#include <sys/kauth.h>
#include <sys/kernel.h>
#include <sys/kmem.h>
#include <sys/lwp.h>
#include <sys/mqueue.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/pool.h>
#include <sys/poll.h>
#include <sys/proc.h>
#include <sys/queue.h>
#include <sys/select.h>
#include <sys/signal.h>
#include <sys/signalvar.h>
#include <sys/stat.h>
#include <sys/sysctl.h>
#include <sys/syscall.h>
#include <sys/syscallvar.h>
#include <sys/syscallargs.h>
#include <sys/systm.h>
#include <sys/unistd.h>

#include <miscfs/genfs/genfs.h>

MODULE(MODULE_CLASS_MISC, mqueue, NULL);

/* System-wide limits. */
static u_int			mq_open_max = MQ_OPEN_MAX;
static u_int			mq_prio_max = MQ_PRIO_MAX;
static u_int			mq_max_msgsize = 16 * MQ_DEF_MSGSIZE;
static u_int			mq_def_maxmsg = 32;
static u_int			mq_max_maxmsg = 16 * 32;

static kmutex_t			mqlist_mtx;
static pool_cache_t		mqmsg_cache;
static LIST_HEAD(, mqueue)	mqueue_head;
static struct sysctllog		*mqsysctl_log;

static int	mqueue_sysinit(void);
static int	mqueue_sysfini(bool);
static int	mqueue_sysctl_init(void);
static int	mq_poll_fop(file_t *, int);
static int	mq_stat_fop(file_t *, struct stat *);
static int	mq_close_fop(file_t *);

static const struct fileops mqops = {
	.fo_read = fbadop_read,
	.fo_write = fbadop_write,
	.fo_ioctl = fbadop_ioctl,
	.fo_fcntl = fnullop_fcntl,
	.fo_poll = mq_poll_fop,
	.fo_stat = mq_stat_fop,
	.fo_close = mq_close_fop,
	.fo_kqfilter = fnullop_kqfilter,
	.fo_restart = fnullop_restart,
};

static const struct syscall_package mqueue_syscalls[] = {
	{ SYS_mq_open, 0, (sy_call_t *)sys_mq_open },
	{ SYS_mq_close, 0, (sy_call_t *)sys_mq_close },
	{ SYS_mq_unlink, 0, (sy_call_t *)sys_mq_unlink },
	{ SYS_mq_getattr, 0, (sy_call_t *)sys_mq_getattr },
	{ SYS_mq_setattr, 0, (sy_call_t *)sys_mq_setattr },
	{ SYS_mq_notify, 0, (sy_call_t *)sys_mq_notify },
	{ SYS_mq_send, 0, (sy_call_t *)sys_mq_send },
	{ SYS_mq_receive, 0, (sy_call_t *)sys_mq_receive },
	{ SYS___mq_timedsend50, 0, (sy_call_t *)sys___mq_timedsend50 },
	{ SYS___mq_timedreceive50, 0, (sy_call_t *)sys___mq_timedreceive50 },
	{ 0, 0, NULL }
};

/*
 * Initialisation and unloading of POSIX message queue subsystem.
 */

static int
mqueue_sysinit(void)
{
	int error;

	mqmsg_cache = pool_cache_init(MQ_DEF_MSGSIZE, coherency_unit,
	    0, 0, "mqmsgpl", NULL, IPL_NONE, NULL, NULL, NULL);
	mutex_init(&mqlist_mtx, MUTEX_DEFAULT, IPL_NONE);
	LIST_INIT(&mqueue_head);

	error = mqueue_sysctl_init();
	if (error) {
		(void)mqueue_sysfini(false);
		return error;
	}
	error = syscall_establish(NULL, mqueue_syscalls);
	if (error) {
		(void)mqueue_sysfini(false);
	}
	return error;
}

static int
mqueue_sysfini(bool interface)
{

	if (interface) {
		int error;
		bool inuse;

		/* Stop syscall activity. */
		error = syscall_disestablish(NULL, mqueue_syscalls);
		if (error)
			return error;
		/*
		 * Check if there are any message queues in use.
		 * TODO: We shall support forced unload.
		 */
		mutex_enter(&mqlist_mtx);
		inuse = !LIST_EMPTY(&mqueue_head);
		mutex_exit(&mqlist_mtx);
		if (inuse) {
			error = syscall_establish(NULL, mqueue_syscalls);
			KASSERT(error == 0);
			return EBUSY;
		}
	}

	if (mqsysctl_log != NULL)
		sysctl_teardown(&mqsysctl_log);

	mutex_destroy(&mqlist_mtx);
	pool_cache_destroy(mqmsg_cache);
	return 0;
}

/*
 * Module interface.
 */
static int
mqueue_modcmd(modcmd_t cmd, void *arg)
{

	switch (cmd) {
	case MODULE_CMD_INIT:
		return mqueue_sysinit();
	case MODULE_CMD_FINI:
		return mqueue_sysfini(true);
	default:
		return ENOTTY;
	}
}

/*
 * Free the message.
 */
static void
mqueue_freemsg(struct mq_msg *msg, const size_t size)
{

	if (size > MQ_DEF_MSGSIZE) {
		kmem_free(msg, size);
	} else {
		pool_cache_put(mqmsg_cache, msg);
	}
}

/*
 * Destroy the message queue.
 */
static void
mqueue_destroy(struct mqueue *mq)
{
	struct mq_msg *msg;
	size_t msz;
	u_int i;

	/* Note MQ_PQSIZE + 1. */
	for (i = 0; i <= MQ_PQSIZE; i++) {
		while ((msg = TAILQ_FIRST(&mq->mq_head[i])) != NULL) {
			TAILQ_REMOVE(&mq->mq_head[i], msg, msg_queue);
			msz = sizeof(struct mq_msg) + msg->msg_len;
			mqueue_freemsg(msg, msz);
		}
	}
	seldestroy(&mq->mq_rsel);
	seldestroy(&mq->mq_wsel);
	cv_destroy(&mq->mq_send_cv);
	cv_destroy(&mq->mq_recv_cv);
	mutex_destroy(&mq->mq_mtx);
	kmem_free(mq, sizeof(struct mqueue));
}

/*
 * Lookup for file name in general list of message queues.
 *  => locks the message queue
 */
static void *
mqueue_lookup(char *name)
{
	struct mqueue *mq;
	KASSERT(mutex_owned(&mqlist_mtx));

	LIST_FOREACH(mq, &mqueue_head, mq_list) {
		if (strncmp(mq->mq_name, name, MQ_NAMELEN) == 0) {
			mutex_enter(&mq->mq_mtx);
			return mq;
		}
	}

	return NULL;
}

/*
 * mqueue_get: get the mqueue from the descriptor.
 *  => locks the message queue, if found.
 *  => holds a reference on the file descriptor.
 */
static int
mqueue_get(mqd_t mqd, file_t **fpr)
{
	struct mqueue *mq;
	file_t *fp;

	fp = fd_getfile((int)mqd);
	if (__predict_false(fp == NULL)) {
		return EBADF;
	}
	if (__predict_false(fp->f_type != DTYPE_MQUEUE)) {
		fd_putfile((int)mqd);
		return EBADF;
	}
	mq = fp->f_data;
	mutex_enter(&mq->mq_mtx);

	*fpr = fp;
	return 0;
}

/*
 * mqueue_linear_insert: perform linear insert according to the message
 * priority into the reserved queue (MQ_PQRESQ).  Reserved queue is a
 * sorted list used only when mq_prio_max is increased via sysctl.
 */
static inline void
mqueue_linear_insert(struct mqueue *mq, struct mq_msg *msg)
{
	struct mq_msg *mit;

	TAILQ_FOREACH(mit, &mq->mq_head[MQ_PQRESQ], msg_queue) {
		if (msg->msg_prio > mit->msg_prio)
			break;
	}
	if (mit == NULL) {
		TAILQ_INSERT_TAIL(&mq->mq_head[MQ_PQRESQ], msg, msg_queue);
	} else {
		TAILQ_INSERT_BEFORE(mit, msg, msg_queue);
	}
}

static int
mq_stat_fop(file_t *fp, struct stat *st)
{
	struct mqueue *mq = fp->f_data;

	memset(st, 0, sizeof(*st));

	mutex_enter(&mq->mq_mtx);
	st->st_mode = mq->mq_mode;
	st->st_uid = mq->mq_euid;
	st->st_gid = mq->mq_egid;
	st->st_atimespec = mq->mq_atime;
	st->st_mtimespec = mq->mq_mtime;
	st->st_ctimespec = st->st_birthtimespec = mq->mq_btime;
	st->st_uid = kauth_cred_geteuid(fp->f_cred);
	st->st_gid = kauth_cred_getegid(fp->f_cred);
	mutex_exit(&mq->mq_mtx);

	return 0;
}

static int
mq_poll_fop(file_t *fp, int events)
{
	struct mqueue *mq = fp->f_data;
	struct mq_attr *mqattr;
	int revents = 0;

	mutex_enter(&mq->mq_mtx);
	mqattr = &mq->mq_attrib;
	if (events & (POLLIN | POLLRDNORM)) {
		/* Ready for receiving, if there are messages in the queue. */
		if (mqattr->mq_curmsgs)
			revents |= events & (POLLIN | POLLRDNORM);
		else
			selrecord(curlwp, &mq->mq_rsel);
	}
	if (events & (POLLOUT | POLLWRNORM)) {
		/* Ready for sending, if the message queue is not full. */
		if (mqattr->mq_curmsgs < mqattr->mq_maxmsg)
			revents |= events & (POLLOUT | POLLWRNORM);
		else
			selrecord(curlwp, &mq->mq_wsel);
	}
	mutex_exit(&mq->mq_mtx);

	return revents;
}

static int
mq_close_fop(file_t *fp)
{
	struct proc *p = curproc;
	struct mqueue *mq = fp->f_data;
	bool destroy;

	mutex_enter(&mqlist_mtx);
	mutex_enter(&mq->mq_mtx);

	/* Decrease the counters */
	p->p_mqueue_cnt--;
	mq->mq_refcnt--;

	/* Remove notification if registered for this process */
	if (mq->mq_notify_proc == p)
		mq->mq_notify_proc = NULL;

	/*
	 * If this is the last reference and mqueue is marked for unlink,
	 * remove and later destroy the message queue.
	 */
	if (mq->mq_refcnt == 0 && (mq->mq_attrib.mq_flags & MQ_UNLINK)) {
		LIST_REMOVE(mq, mq_list);
		destroy = true;
	} else
		destroy = false;

	mutex_exit(&mq->mq_mtx);
	mutex_exit(&mqlist_mtx);

	if (destroy)
		mqueue_destroy(mq);

	return 0;
}

static int
mqueue_access(struct mqueue *mq, mode_t mode, kauth_cred_t cred)
{

	if (genfs_can_access(VNON, mq->mq_mode, mq->mq_euid,
	    mq->mq_egid, mode, cred)) {
		return EACCES;
	}

	return 0;
}

/*
 * General mqueue system calls.
 */

int
sys_mq_open(struct lwp *l, const struct sys_mq_open_args *uap,
    register_t *retval)
{
	/* {
		syscallarg(const char *) name;
		syscallarg(int) oflag;
		syscallarg(mode_t) mode;
		syscallarg(struct mq_attr) attr;
	} */
	struct proc *p = l->l_proc;
	struct mqueue *mq, *mq_new = NULL;
	file_t *fp;
	char *name;
	int mqd, error, oflag;

	oflag = SCARG(uap, oflag);

	/* Get the name from the user-space */
	name = kmem_zalloc(MQ_NAMELEN, KM_SLEEP);
	error = copyinstr(SCARG(uap, name), name, MQ_NAMELEN - 1, NULL);
	if (error) {
		kmem_free(name, MQ_NAMELEN);
		return error;
	}

	if (oflag & O_CREAT) {
		struct cwdinfo *cwdi = p->p_cwdi;
		struct mq_attr attr;
		u_int i;

		/* Check the limit */
		if (p->p_mqueue_cnt == mq_open_max) {
			kmem_free(name, MQ_NAMELEN);
			return EMFILE;
		}

		/* Empty name is invalid */
		if (name[0] == '\0') {
			kmem_free(name, MQ_NAMELEN);
			return EINVAL;
		}

		/* Check for mqueue attributes */
		if (SCARG(uap, attr)) {
			error = copyin(SCARG(uap, attr), &attr,
			    sizeof(struct mq_attr));
			if (error) {
				kmem_free(name, MQ_NAMELEN);
				return error;
			}
			if (attr.mq_maxmsg <= 0 ||
			    attr.mq_maxmsg > mq_max_maxmsg ||
			    attr.mq_msgsize <= 0 ||
			    attr.mq_msgsize > mq_max_msgsize) {
				kmem_free(name, MQ_NAMELEN);
				return EINVAL;
			}
			attr.mq_curmsgs = 0;
		} else {
			memset(&attr, 0, sizeof(struct mq_attr));
			attr.mq_maxmsg = mq_def_maxmsg;
			attr.mq_msgsize =
			    MQ_DEF_MSGSIZE - sizeof(struct mq_msg);
		}

		/*
		 * Allocate new mqueue, initialize data structures,
		 * copy the name, attributes and set the flag.
		 */
		mq_new = kmem_zalloc(sizeof(struct mqueue), KM_SLEEP);

		mutex_init(&mq_new->mq_mtx, MUTEX_DEFAULT, IPL_NONE);
		cv_init(&mq_new->mq_send_cv, "mqsendcv");
		cv_init(&mq_new->mq_recv_cv, "mqrecvcv");
		for (i = 0; i < (MQ_PQSIZE + 1); i++) {
			TAILQ_INIT(&mq_new->mq_head[i]);
		}
		selinit(&mq_new->mq_rsel);
		selinit(&mq_new->mq_wsel);

		strlcpy(mq_new->mq_name, name, MQ_NAMELEN);
		memcpy(&mq_new->mq_attrib, &attr, sizeof(struct mq_attr));

		CTASSERT((O_MASK & (MQ_UNLINK | MQ_RECEIVE)) == 0);
		mq_new->mq_attrib.mq_flags = (O_MASK & oflag);

		/* Store mode and effective UID with GID */
		mq_new->mq_mode = ((SCARG(uap, mode) &
		    ~cwdi->cwdi_cmask) & ALLPERMS) & ~S_ISTXT;
		mq_new->mq_euid = kauth_cred_geteuid(l->l_cred);
		mq_new->mq_egid = kauth_cred_getegid(l->l_cred);
	}

	/* Allocate file structure and descriptor */
	error = fd_allocfile(&fp, &mqd);
	if (error) {
		if (mq_new)
			mqueue_destroy(mq_new);
		kmem_free(name, MQ_NAMELEN);
		return error;
	}
	fp->f_type = DTYPE_MQUEUE;
	fp->f_flag = FFLAGS(oflag) & (FREAD | FWRITE);
	fp->f_ops = &mqops;

	/* Look up for mqueue with such name */
	mutex_enter(&mqlist_mtx);
	mq = mqueue_lookup(name);
	if (mq) {
		mode_t acc_mode;

		KASSERT(mutex_owned(&mq->mq_mtx));

		/* Check if mqueue is not marked as unlinking */
		if (mq->mq_attrib.mq_flags & MQ_UNLINK) {
			error = EACCES;
			goto exit;
		}

		/* Fail if O_EXCL is set, and mqueue already exists */
		if ((oflag & O_CREAT) && (oflag & O_EXCL)) {
			error = EEXIST;
			goto exit;
		}

		/*
		 * Check the permissions.  Note the difference between
		 * VREAD/VWRITE and FREAD/FWRITE.
		 */
		acc_mode = 0;
		if (fp->f_flag & FREAD) {
			acc_mode |= VREAD;
		}
		if (fp->f_flag & FWRITE) {
			acc_mode |= VWRITE;
		}
		if (mqueue_access(mq, acc_mode, l->l_cred) != 0) {
			error = EACCES;
			goto exit;
		}
	} else {
		/* Fail if mqueue neither exists, nor we create it */
		if ((oflag & O_CREAT) == 0) {
			mutex_exit(&mqlist_mtx);
			KASSERT(mq_new == NULL);
			fd_abort(p, fp, mqd);
			kmem_free(name, MQ_NAMELEN);
			return ENOENT;
		}

		/* Check the limit */
		if (p->p_mqueue_cnt == mq_open_max) {
			error = EMFILE;
			goto exit;
		}

		/* Insert the queue to the list */
		mq = mq_new;
		mutex_enter(&mq->mq_mtx);
		LIST_INSERT_HEAD(&mqueue_head, mq, mq_list);
		mq_new = NULL;
		getnanotime(&mq->mq_btime);
		mq->mq_atime = mq->mq_mtime = mq->mq_btime;
	}

	/* Increase the counters, and make descriptor ready */
	p->p_mqueue_cnt++;
	mq->mq_refcnt++;
	fp->f_data = mq;
exit:
	mutex_exit(&mq->mq_mtx);
	mutex_exit(&mqlist_mtx);

	if (mq_new)
		mqueue_destroy(mq_new);
	if (error) {
		fd_abort(p, fp, mqd);
	} else {
		fd_affix(p, fp, mqd);
		*retval = mqd;
	}
	kmem_free(name, MQ_NAMELEN);

	return error;
}

int
sys_mq_close(struct lwp *l, const struct sys_mq_close_args *uap,
    register_t *retval)
{

	return sys_close(l, (const void *)uap, retval);
}

/*
 * Primary mq_recv1() function.
 */
int
mq_recv1(mqd_t mqdes, void *msg_ptr, size_t msg_len, u_int *msg_prio,
    struct timespec *ts, ssize_t *mlen)
{
	file_t *fp = NULL;
	struct mqueue *mq;
	struct mq_msg *msg = NULL;
	struct mq_attr *mqattr;
	u_int idx;
	int error;

	/* Get the message queue */
	error = mqueue_get(mqdes, &fp);
	if (error) {
		return error;
	}
	mq = fp->f_data;
	if ((fp->f_flag & FREAD) == 0) {
		error = EBADF;
		goto error;
	}
	getnanotime(&mq->mq_atime);
	mqattr = &mq->mq_attrib;

	/* Check the message size limits */
	if (msg_len < mqattr->mq_msgsize) {
		error = EMSGSIZE;
		goto error;
	}

	/* Check if queue is empty */
	while (mqattr->mq_curmsgs == 0) {
		int t;

		if (mqattr->mq_flags & O_NONBLOCK) {
			error = EAGAIN;
			goto error;
		}
		if (ts) {
			error = abstimeout2timo(ts, &t);
			if (error)
				goto error;
		} else
			t = 0;
		/*
		 * Block until someone sends the message.
		 * While doing this, notification should not be sent.
		 */
		mqattr->mq_flags |= MQ_RECEIVE;
		error = cv_timedwait_sig(&mq->mq_send_cv, &mq->mq_mtx, t);
		mqattr->mq_flags &= ~MQ_RECEIVE;
		if (error || (mqattr->mq_flags & MQ_UNLINK)) {
			error = (error == EWOULDBLOCK) ? ETIMEDOUT : EINTR;
			goto error;
		}
	}

	/*
	 * Find the highest priority message, and remove it from the queue.
	 * At first, reserved queue is checked, bitmap is next.
	 */
	msg = TAILQ_FIRST(&mq->mq_head[MQ_PQRESQ]);
	if (__predict_true(msg == NULL)) {
		idx = ffs(mq->mq_bitmap);
		msg = TAILQ_FIRST(&mq->mq_head[idx]);
		KASSERT(msg != NULL);
	} else {
		idx = MQ_PQRESQ;
	}
	TAILQ_REMOVE(&mq->mq_head[idx], msg, msg_queue);

	/* Unmark the bit, if last message. */
	if (__predict_true(idx) && TAILQ_EMPTY(&mq->mq_head[idx])) {
		KASSERT((MQ_PQSIZE - idx) == msg->msg_prio);
		mq->mq_bitmap &= ~(1 << --idx);
	}

	/* Decrement the counter and signal waiter, if any */
	mqattr->mq_curmsgs--;
	cv_signal(&mq->mq_recv_cv);

	/* Ready for sending now */
	selnotify(&mq->mq_wsel, POLLOUT | POLLWRNORM, 0);
error:
	mutex_exit(&mq->mq_mtx);
	fd_putfile((int)mqdes);
	if (error)
		return error;

	/*
	 * Copy the data to the user-space.
	 * Note: According to POSIX, no message should be removed from the
	 * queue in case of fail - this would be violated.
	 */
	*mlen = msg->msg_len;
	error = copyout(msg->msg_ptr, msg_ptr, msg->msg_len);
	if (error == 0 && msg_prio)
		error = copyout(&msg->msg_prio, msg_prio, sizeof(unsigned));
	mqueue_freemsg(msg, sizeof(struct mq_msg) + msg->msg_len);

	return error;
}

int
sys_mq_receive(struct lwp *l, const struct sys_mq_receive_args *uap,
    register_t *retval)
{
	/* {
		syscallarg(mqd_t) mqdes;
		syscallarg(char *) msg_ptr;
		syscallarg(size_t) msg_len;
		syscallarg(unsigned *) msg_prio;
	} */
	ssize_t mlen;
	int error;

	error = mq_recv1(SCARG(uap, mqdes), SCARG(uap, msg_ptr),
	    SCARG(uap, msg_len), SCARG(uap, msg_prio), NULL, &mlen);
	if (error == 0)
		*retval = mlen;

	return error;
}

int
sys___mq_timedreceive50(struct lwp *l,
    const struct sys___mq_timedreceive50_args *uap, register_t *retval)
{
	/* {
		syscallarg(mqd_t) mqdes;
		syscallarg(char *) msg_ptr;
		syscallarg(size_t) msg_len;
		syscallarg(unsigned *) msg_prio;
		syscallarg(const struct timespec *) abs_timeout;
	} */
	struct timespec ts, *tsp;
	ssize_t mlen;
	int error;

	/* Get and convert time value */
	if (SCARG(uap, abs_timeout)) {
		error = copyin(SCARG(uap, abs_timeout), &ts, sizeof(ts));
		if (error)
			return error;
		tsp = &ts;
	} else {
		tsp = NULL;
	}

	error = mq_recv1(SCARG(uap, mqdes), SCARG(uap, msg_ptr),
	    SCARG(uap, msg_len), SCARG(uap, msg_prio), tsp, &mlen);
	if (error == 0)
		*retval = mlen;

	return error;
}

/*
 * Primary mq_send1() function.
 */
int
mq_send1(mqd_t mqdes, const char *msg_ptr, size_t msg_len, u_int msg_prio,
    struct timespec *ts)
{
	file_t *fp = NULL;
	struct mqueue *mq;
	struct mq_msg *msg;
	struct mq_attr *mqattr;
	struct proc *notify = NULL;
	ksiginfo_t ksi;
	size_t size;
	int error;

	/* Check the priority range */
	if (msg_prio >= mq_prio_max)
		return EINVAL;

	/* Allocate a new message */
	size = sizeof(struct mq_msg) + msg_len;
	if (size > mq_max_msgsize)
		return EMSGSIZE;

	if (size > MQ_DEF_MSGSIZE) {
		msg = kmem_alloc(size, KM_SLEEP);
	} else {
		msg = pool_cache_get(mqmsg_cache, PR_WAITOK);
	}

	/* Get the data from user-space */
	error = copyin(msg_ptr, msg->msg_ptr, msg_len);
	if (error) {
		mqueue_freemsg(msg, size);
		return error;
	}
	msg->msg_len = msg_len;
	msg->msg_prio = msg_prio;

	/* Get the mqueue */
	error = mqueue_get(mqdes, &fp);
	if (error) {
		mqueue_freemsg(msg, size);
		return error;
	}
	mq = fp->f_data;
	if ((fp->f_flag & FWRITE) == 0) {
		error = EBADF;
		goto error;
	}
	getnanotime(&mq->mq_mtime);
	mqattr = &mq->mq_attrib;

	/* Check the message size limit */
	if (msg_len <= 0 || msg_len > mqattr->mq_msgsize) {
		error = EMSGSIZE;
		goto error;
	}

	/* Check if queue is full */
	while (mqattr->mq_curmsgs >= mqattr->mq_maxmsg) {
		int t;

		if (mqattr->mq_flags & O_NONBLOCK) {
			error = EAGAIN;
			goto error;
		}
		if (ts) {
			error = abstimeout2timo(ts, &t);
			if (error)
				goto error;
		} else
			t = 0;
		/* Block until queue becomes available */
		error = cv_timedwait_sig(&mq->mq_recv_cv, &mq->mq_mtx, t);
		if (error || (mqattr->mq_flags & MQ_UNLINK)) {
			error = (error == EWOULDBLOCK) ? ETIMEDOUT : error;
			goto error;
		}
	}
	KASSERT(mqattr->mq_curmsgs < mqattr->mq_maxmsg);

	/*
	 * Insert message into the queue, according to the priority.
	 * Note the difference between index and priority.
	 */
	if (__predict_true(msg_prio < MQ_PQSIZE)) {
		u_int idx = MQ_PQSIZE - msg_prio;

		KASSERT(idx != MQ_PQRESQ);
		TAILQ_INSERT_TAIL(&mq->mq_head[idx], msg, msg_queue);
		mq->mq_bitmap |= (1 << --idx);
	} else {
		mqueue_linear_insert(mq, msg);
	}

	/* Check for the notify */
	if (mqattr->mq_curmsgs == 0 && mq->mq_notify_proc &&
	    (mqattr->mq_flags & MQ_RECEIVE) == 0 &&
	    mq->mq_sig_notify.sigev_notify == SIGEV_SIGNAL) {
		/* Initialize the signal */
		KSI_INIT(&ksi);
		ksi.ksi_signo = mq->mq_sig_notify.sigev_signo;
		ksi.ksi_code = SI_MESGQ;
		ksi.ksi_value = mq->mq_sig_notify.sigev_value;
		/* Unregister the process */
		notify = mq->mq_notify_proc;
		mq->mq_notify_proc = NULL;
	}

	/* Increment the counter and signal waiter, if any */
	mqattr->mq_curmsgs++;
	cv_signal(&mq->mq_send_cv);

	/* Ready for receiving now */
	selnotify(&mq->mq_rsel, POLLIN | POLLRDNORM, 0);
error:
	mutex_exit(&mq->mq_mtx);
	fd_putfile((int)mqdes);

	if (error) {
		mqueue_freemsg(msg, size);
	} else if (notify) {
		/* Send the notify, if needed */
		mutex_enter(proc_lock);
		kpsignal(notify, &ksi, NULL);
		mutex_exit(proc_lock);
	}
	return error;
}

int
sys_mq_send(struct lwp *l, const struct sys_mq_send_args *uap,
    register_t *retval)
{
	/* {
		syscallarg(mqd_t) mqdes;
		syscallarg(const char *) msg_ptr;
		syscallarg(size_t) msg_len;
		syscallarg(unsigned) msg_prio;
	} */

	return mq_send1(SCARG(uap, mqdes), SCARG(uap, msg_ptr),
	    SCARG(uap, msg_len), SCARG(uap, msg_prio), NULL);
}

int
sys___mq_timedsend50(struct lwp *l, const struct sys___mq_timedsend50_args *uap,
    register_t *retval)
{
	/* {
		syscallarg(mqd_t) mqdes;
		syscallarg(const char *) msg_ptr;
		syscallarg(size_t) msg_len;
		syscallarg(unsigned) msg_prio;
		syscallarg(const struct timespec *) abs_timeout;
	} */
	struct timespec ts, *tsp;
	int error;

	/* Get and convert time value */
	if (SCARG(uap, abs_timeout)) {
		error = copyin(SCARG(uap, abs_timeout), &ts, sizeof(ts));
		if (error)
			return error;
		tsp = &ts;
	} else {
		tsp = NULL;
	}

	return mq_send1(SCARG(uap, mqdes), SCARG(uap, msg_ptr),
	    SCARG(uap, msg_len), SCARG(uap, msg_prio), tsp);
}

int
sys_mq_notify(struct lwp *l, const struct sys_mq_notify_args *uap,
    register_t *retval)
{
	/* {
		syscallarg(mqd_t) mqdes;
		syscallarg(const struct sigevent *) notification;
	} */
	file_t *fp = NULL;
	struct mqueue *mq;
	struct sigevent sig;
	int error;

	if (SCARG(uap, notification)) {
		/* Get the signal from user-space */
		error = copyin(SCARG(uap, notification), &sig,
		    sizeof(struct sigevent));
		if (error)
			return error;
		if (sig.sigev_notify == SIGEV_SIGNAL &&
		    (sig.sigev_signo <=0 || sig.sigev_signo >= NSIG))
			return EINVAL;
	}

	error = mqueue_get(SCARG(uap, mqdes), &fp);
	if (error)
		return error;
	mq = fp->f_data;

	if (SCARG(uap, notification)) {
		/* Register notification: set the signal and target process */
		if (mq->mq_notify_proc == NULL) {
			memcpy(&mq->mq_sig_notify, &sig,
			    sizeof(struct sigevent));
			mq->mq_notify_proc = l->l_proc;
		} else {
			/* Fail if someone else already registered */
			error = EBUSY;
		}
	} else {
		/* Unregister the notification */
		mq->mq_notify_proc = NULL;
	}
	mutex_exit(&mq->mq_mtx);
	fd_putfile((int)SCARG(uap, mqdes));

	return error;
}

int
sys_mq_getattr(struct lwp *l, const struct sys_mq_getattr_args *uap,
    register_t *retval)
{
	/* {
		syscallarg(mqd_t) mqdes;
		syscallarg(struct mq_attr *) mqstat;
	} */
	file_t *fp = NULL;
	struct mqueue *mq;
	struct mq_attr attr;
	int error;

	/* Get the message queue */
	error = mqueue_get(SCARG(uap, mqdes), &fp);
	if (error)
		return error;
	mq = fp->f_data;
	memcpy(&attr, &mq->mq_attrib, sizeof(struct mq_attr));
	mutex_exit(&mq->mq_mtx);
	fd_putfile((int)SCARG(uap, mqdes));

	return copyout(&attr, SCARG(uap, mqstat), sizeof(struct mq_attr));
}

int
sys_mq_setattr(struct lwp *l, const struct sys_mq_setattr_args *uap,
    register_t *retval)
{
	/* {
		syscallarg(mqd_t) mqdes;
		syscallarg(const struct mq_attr *) mqstat;
		syscallarg(struct mq_attr *) omqstat;
	} */
	file_t *fp = NULL;
	struct mqueue *mq;
	struct mq_attr attr;
	int error, nonblock;

	error = copyin(SCARG(uap, mqstat), &attr, sizeof(struct mq_attr));
	if (error)
		return error;
	nonblock = (attr.mq_flags & O_NONBLOCK);

	/* Get the message queue */
	error = mqueue_get(SCARG(uap, mqdes), &fp);
	if (error)
		return error;
	mq = fp->f_data;

	/* Copy the old attributes, if needed */
	if (SCARG(uap, omqstat)) {
		memcpy(&attr, &mq->mq_attrib, sizeof(struct mq_attr));
	}

	/* Ignore everything, except O_NONBLOCK */
	if (nonblock)
		mq->mq_attrib.mq_flags |= O_NONBLOCK;
	else
		mq->mq_attrib.mq_flags &= ~O_NONBLOCK;

	mutex_exit(&mq->mq_mtx);
	fd_putfile((int)SCARG(uap, mqdes));

	/*
	 * Copy the data to the user-space.
	 * Note: According to POSIX, the new attributes should not be set in
	 * case of fail - this would be violated.
	 */
	if (SCARG(uap, omqstat))
		error = copyout(&attr, SCARG(uap, omqstat),
		    sizeof(struct mq_attr));

	return error;
}

int
sys_mq_unlink(struct lwp *l, const struct sys_mq_unlink_args *uap,
    register_t *retval)
{
	/* {
		syscallarg(const char *) name;
	} */
	struct mqueue *mq;
	char *name;
	int error, refcnt = 0;

	/* Get the name from the user-space */
	name = kmem_zalloc(MQ_NAMELEN, KM_SLEEP);
	error = copyinstr(SCARG(uap, name), name, MQ_NAMELEN - 1, NULL);
	if (error) {
		kmem_free(name, MQ_NAMELEN);
		return error;
	}

	/* Lookup for this file */
	mutex_enter(&mqlist_mtx);
	mq = mqueue_lookup(name);
	if (mq == NULL) {
		error = ENOENT;
		goto error;
	}

	/* Check the permissions */
	if (kauth_cred_geteuid(l->l_cred) != mq->mq_euid &&
	    kauth_authorize_generic(l->l_cred, KAUTH_GENERIC_ISSUSER, NULL)) {
		mutex_exit(&mq->mq_mtx);
		error = EACCES;
		goto error;
	}

	/* Mark message queue as unlinking, before leaving the window */
	mq->mq_attrib.mq_flags |= MQ_UNLINK;

	/* Wake up all waiters, if there are such */
	cv_broadcast(&mq->mq_send_cv);
	cv_broadcast(&mq->mq_recv_cv);

	selnotify(&mq->mq_rsel, POLLHUP, 0);
	selnotify(&mq->mq_wsel, POLLHUP, 0);

	refcnt = mq->mq_refcnt;
	if (refcnt == 0)
		LIST_REMOVE(mq, mq_list);

	mutex_exit(&mq->mq_mtx);
error:
	mutex_exit(&mqlist_mtx);

	/*
	 * If there are no references - destroy the message
	 * queue, otherwise, the last mq_close() will do that.
	 */
	if (error == 0 && refcnt == 0)
		mqueue_destroy(mq);

	kmem_free(name, MQ_NAMELEN);
	return error;
}

/*
 * System control nodes.
 */
static int
mqueue_sysctl_init(void)
{
	const struct sysctlnode *node = NULL;

	mqsysctl_log = NULL;

	sysctl_createv(&mqsysctl_log, 0, NULL, NULL,
		CTLFLAG_PERMANENT,
		CTLTYPE_NODE, "kern", NULL,
		NULL, 0, NULL, 0,
		CTL_KERN, CTL_EOL);
	sysctl_createv(&mqsysctl_log, 0, NULL, NULL,
		CTLFLAG_PERMANENT|CTLFLAG_IMMEDIATE,
		CTLTYPE_INT, "posix_msg",
		SYSCTL_DESCR("Version of IEEE Std 1003.1 and its "
			     "Message Passing option to which the "
			     "system attempts to conform"),
		NULL, _POSIX_MESSAGE_PASSING, NULL, 0,
		CTL_KERN, CTL_CREATE, CTL_EOL);
	sysctl_createv(&mqsysctl_log, 0, NULL, &node,
		CTLFLAG_PERMANENT,
		CTLTYPE_NODE, "mqueue",
		SYSCTL_DESCR("Message queue options"),
		NULL, 0, NULL, 0,
		CTL_KERN, CTL_CREATE, CTL_EOL);

	if (node == NULL)
		return ENXIO;

	sysctl_createv(&mqsysctl_log, 0, &node, NULL,
		CTLFLAG_PERMANENT | CTLFLAG_READWRITE,
		CTLTYPE_INT, "mq_open_max",
		SYSCTL_DESCR("Maximal number of message queue descriptors "
			     "that process could open"),
		NULL, 0, &mq_open_max, 0,
		CTL_CREATE, CTL_EOL);
	sysctl_createv(&mqsysctl_log, 0, &node, NULL,
		CTLFLAG_PERMANENT | CTLFLAG_READWRITE,
		CTLTYPE_INT, "mq_prio_max",
		SYSCTL_DESCR("Maximal priority of the message"),
		NULL, 0, &mq_prio_max, 0,
		CTL_CREATE, CTL_EOL);
	sysctl_createv(&mqsysctl_log, 0, &node, NULL,
		CTLFLAG_PERMANENT | CTLFLAG_READWRITE,
		CTLTYPE_INT, "mq_max_msgsize",
		SYSCTL_DESCR("Maximal allowed size of the message"),
		NULL, 0, &mq_max_msgsize, 0,
		CTL_CREATE, CTL_EOL);
	sysctl_createv(&mqsysctl_log, 0, &node, NULL,
		CTLFLAG_PERMANENT | CTLFLAG_READWRITE,
		CTLTYPE_INT, "mq_def_maxmsg",
		SYSCTL_DESCR("Default maximal message count"),
		NULL, 0, &mq_def_maxmsg, 0,
		CTL_CREATE, CTL_EOL);
	sysctl_createv(&mqsysctl_log, 0, &node, NULL,
		CTLFLAG_PERMANENT | CTLFLAG_READWRITE,
		CTLTYPE_INT, "mq_max_maxmsg",
		SYSCTL_DESCR("Maximal allowed message count"),
		NULL, 0, &mq_max_maxmsg, 0,
		CTL_CREATE, CTL_EOL);

	return 0;
}

/*
 * Debugging.
 */
#if defined(DDB)

void
mqueue_print_list(void (*pr)(const char *, ...))
{
	struct mqueue *mq;

	(*pr)("Global list of the message queues:\n");
	(*pr)("%20s %10s %8s %8s %3s %4s %4s %4s\n",
	    "Name", "Ptr", "Mode", "Flags",  "Ref",
	    "MaxMsg", "MsgSze", "CurMsg");
	LIST_FOREACH(mq, &mqueue_head, mq_list) {
		(*pr)("%20s %10p %8x %8x %3u %6lu %6lu %6lu\n",
		    mq->mq_name, mq, mq->mq_mode,
		    mq->mq_attrib.mq_flags, mq->mq_refcnt,
		    mq->mq_attrib.mq_maxmsg, mq->mq_attrib.mq_msgsize,
		    mq->mq_attrib.mq_curmsgs);
	}
}

#endif /* defined(DDB) */
