/*	$NetBSD: kern_systrace.c,v 1.17 2002/10/08 14:46:24 provos Exp $	*/

/*
 * Copyright 2002 Niels Provos <provos@citi.umich.edu>
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Niels Provos.
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
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: kern_systrace.c,v 1.17 2002/10/08 14:46:24 provos Exp $");

#include "opt_systrace.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/tree.h>
#include <sys/malloc.h>
#include <sys/syscall.h>
#include <sys/vnode.h>
#include <sys/errno.h>
#include <sys/conf.h>
#include <sys/device.h>
#include <sys/proc.h>
#include <sys/file.h>
#include <sys/filedesc.h>
#include <sys/filio.h>
#include <sys/signalvar.h>
#include <sys/lock.h>
#include <sys/pool.h>
#include <sys/mount.h>
#include <sys/poll.h>
#include <sys/ptrace.h>
#include <sys/systrace.h>

#include <compat/common/compat_util.h>

#ifdef __NetBSD__
#define	SYSTRACE_LOCK(fst, p)	lockmgr(&fst->lock, LK_EXCLUSIVE, NULL)
#define	SYSTRACE_UNLOCK(fst, p)	lockmgr(&fst->lock, LK_RELEASE, NULL)
#else
#define	SYSTRACE_LOCK(fst, p)	lockmgr(&fst->lock, LK_EXCLUSIVE, NULL, p)
#define	SYSTRACE_UNLOCK(fst, p)	lockmgr(&fst->lock, LK_RELEASE, NULL, p)
#endif
#ifndef	M_XDATA
#define	M_XDATA		M_FILE	/* XXX */
#endif

#ifdef __NetBSD__
dev_type_open(systraceopen);
#else
cdev_decl(systrace);
#endif

#ifdef __NetBSD__
int	systracef_read(struct file *, off_t *, struct uio *, struct ucred *,
		int);
int	systracef_write(struct file *, off_t *, struct uio *, struct ucred *,
		int);
int	systracef_fcntl(struct file *, u_int, caddr_t, struct proc *);
int	systracef_poll(struct file *, int, struct proc *);
#else
int	systracef_read(struct file *, off_t *, struct uio *, struct ucred *);
int	systracef_write(struct file *, off_t *, struct uio *, struct ucred *);
int	systracef_select(struct file *, int, struct proc *);
int	systracef_kqfilter(struct file *, struct knote *);
#endif
int	systracef_ioctl(struct file *, u_long, caddr_t, struct proc *);
int	systracef_stat(struct file *, struct stat *, struct proc *);
int	systracef_close(struct file *, struct proc *);

struct str_policy {
	TAILQ_ENTRY(str_policy) next;

	int nr;

	const struct emul *emul;	/* Is only valid for this emulation */

	int refcount;

	int nsysent;
	u_char *sysent;
};

#define STR_PROC_ONQUEUE	0x01
#define STR_PROC_WAITANSWER	0x02
#define STR_PROC_SYSCALLRES	0x04
#define STR_PROC_REPORT		0x08	/* Report emulation */
#define STR_PROC_NEEDSEQNR	0x10	/* Answer must quote seqnr */

struct str_process {
	TAILQ_ENTRY(str_process) next;
	TAILQ_ENTRY(str_process) msg_next;

	struct proc *proc;
	const struct emul *oldemul;
	uid_t olduid;
	gid_t oldgid;

	pid_t pid;

	struct fsystrace *parent;
	struct str_policy *policy;

	struct systrace_replace *replace;

	int flags;
	short answer;
	short error;
	u_int16_t seqnr;	/* expected reply sequence number */

	struct str_message msg;
};

void systrace_lock(void);
void systrace_unlock(void);

/* Needs to be called with fst locked */

int	systrace_attach(struct fsystrace *, pid_t);
int	systrace_detach(struct str_process *);
int	systrace_answer(struct str_process *, struct systrace_answer *);
int	systrace_io(struct str_process *, struct systrace_io *);
int	systrace_policy(struct fsystrace *, struct systrace_policy *);
int	systrace_preprepl(struct str_process *, struct systrace_replace *);
int	systrace_replace(struct str_process *, size_t, register_t []);
int	systrace_getcwd(struct fsystrace *, struct str_process *);

int	systrace_processready(struct str_process *);
struct proc *systrace_find(struct str_process *);
struct str_process *systrace_findpid(struct fsystrace *fst, pid_t pid);
void	systrace_wakeup(struct fsystrace *);
void	systrace_closepolicy(struct fsystrace *, struct str_policy *);
int	systrace_insert_process(struct fsystrace *, struct proc *);
struct str_policy *systrace_newpolicy(struct fsystrace *, int);
int	systrace_msg_child(struct fsystrace *, struct str_process *, pid_t);
int	systrace_msg_ask(struct fsystrace *, struct str_process *,
	    int, size_t, register_t []);
int	systrace_msg_result(struct fsystrace *, struct str_process *,
	    int, int, size_t, register_t [], register_t []);
int	systrace_msg_emul(struct fsystrace *, struct str_process *);
int	systrace_msg_ugid(struct fsystrace *, struct str_process *);
int	systrace_make_msg(struct str_process *, int);

static struct fileops systracefops = {
	systracef_read,
	systracef_write,
	systracef_ioctl,
#ifdef __NetBSD__
	systracef_fcntl,
	systracef_poll,
#else
	systracef_select,
	systracef_kqfilter,
#endif
	systracef_stat,
	systracef_close
};

struct pool systr_proc_pl;
struct pool systr_policy_pl;

int systrace_debug = 0;
struct lock systrace_lck;

#ifdef __NetBSD__
const struct cdevsw systrace_cdevsw = {
	systraceopen, noclose, noread, nowrite, noioctl,
	nostop, notty, nopoll, nommap,
};
#endif

#define DPRINTF(y)	if (systrace_debug) printf y;

/* ARGSUSED */
int
systracef_read(struct file *fp, off_t *poff, struct uio *uio,
    struct ucred *cred
#ifdef __NetBSD__
    , int flags
#endif
)
{
	struct fsystrace *fst = (struct fsystrace *)fp->f_data;
	struct str_process *process;
	int error = 0;

	if (uio->uio_resid != sizeof(struct str_message))
		return (EINVAL);

 again:
	systrace_lock();
	SYSTRACE_LOCK(fst, curproc);
	systrace_unlock();
	if ((process = TAILQ_FIRST(&fst->messages)) != NULL) {
		error = uiomove((caddr_t)&process->msg,
		    sizeof(struct str_message), uio);
		if (!error) {
			TAILQ_REMOVE(&fst->messages, process, msg_next);
			CLR(process->flags, STR_PROC_ONQUEUE);

			if (SYSTR_MSG_NOPROCESS(process))
				pool_put(&systr_proc_pl, process);

		}
	} else if (TAILQ_FIRST(&fst->processes) == NULL) {
		/* EOF situation */
		;
	} else {
		if (fp->f_flag & FNONBLOCK)
			error = EAGAIN;
		else {
			SYSTRACE_UNLOCK(fst, curproc);
			error = tsleep(fst, PWAIT|PCATCH, "systrrd", 0);
			if (error)
				goto out;
			goto again;
		}

	}

	SYSTRACE_UNLOCK(fst, curproc);
 out:
	return (error);
}

/* ARGSUSED */
int
systracef_write(struct file *fp, off_t *poff, struct uio *uio,
    struct ucred *cred
#ifdef __NetBSD__
    , int flags
#endif
)
{
	return (EIO);
}

#define POLICY_VALID(x)	((x) == SYSTR_POLICY_PERMIT || \
			 (x) == SYSTR_POLICY_ASK || \
			 (x) == SYSTR_POLICY_NEVER)

/* ARGSUSED */
int
systracef_ioctl(struct file *fp, u_long cmd, caddr_t data, struct proc *p)
{
	int ret = 0;
	struct fsystrace *fst = (struct fsystrace *)fp->f_data;
#ifdef __NetBSD__
	struct cwdinfo *cwdp;
#else
	struct filedesc *fdp;
#endif
	struct str_process *strp;
	pid_t pid = 0;

	switch (cmd) {
	case FIONBIO:
	case FIOASYNC:
		return (0);

	case STRIOCDETACH:
	case STRIOCREPORT:
		pid = *(pid_t *)data;
		if (!pid)
			ret = EINVAL;
		break;
	case STRIOCANSWER:
		pid = ((struct systrace_answer *)data)->stra_pid;
		if (!pid)
			ret = EINVAL;
		break;
	case STRIOCIO:
		pid = ((struct systrace_io *)data)->strio_pid;
		if (!pid)
			ret = EINVAL;
		break;
	case STRIOCGETCWD:
		pid = *(pid_t *)data;
		if (!pid)
			ret = EINVAL;
		break;
	case STRIOCATTACH:
	case STRIOCRESCWD:
	case STRIOCPOLICY:
		break;
	case STRIOCREPLACE:
		pid = ((struct systrace_replace *)data)->strr_pid;
		if (!pid)
			ret = EINVAL;
		break;
	default:
		ret = EINVAL;
		break;
	}

	if (ret)
		return (ret);

	systrace_lock();
	SYSTRACE_LOCK(fst, curproc);
	systrace_unlock();
	if (pid) {
		strp = systrace_findpid(fst, pid);
		if (strp == NULL) {
			ret = ESRCH;
			goto unlock;
		}
	}

	switch (cmd) {
	case STRIOCATTACH:
		pid = *(pid_t *)data;
		if (!pid)
			ret = EINVAL;
		else
			ret = systrace_attach(fst, pid);
		DPRINTF(("%s: attach to %u: %d\n", __func__, pid, ret));
		break;
	case STRIOCDETACH:
		ret = systrace_detach(strp);
		break;
	case STRIOCREPORT:
		SET(strp->flags, STR_PROC_REPORT);
		break;
	case STRIOCANSWER:
		ret = systrace_answer(strp, (struct systrace_answer *)data);
		break;
	case STRIOCIO:
		ret = systrace_io(strp, (struct systrace_io *)data);
		break;
	case STRIOCPOLICY:
		ret = systrace_policy(fst, (struct systrace_policy *)data);
		break;
	case STRIOCREPLACE:
		ret = systrace_preprepl(strp, (struct systrace_replace *)data);
		break;
	case STRIOCRESCWD:
		if (!fst->fd_pid) {
			ret = EINVAL;
			break;
		}
#ifdef __NetBSD__
		cwdp = p->p_cwdi;

		/* Release cwd from other process */
		if (cwdp->cwdi_cdir)
			vrele(cwdp->cwdi_cdir);
		if (cwdp->cwdi_rdir)
			vrele(cwdp->cwdi_rdir);
		cwdp->cwdi_cdir = fst->fd_cdir;
		cwdp->cwdi_rdir = fst->fd_rdir;
#else
		fdp = p->p_fd;

		/* Release cwd from other process */
		if (fdp->fd_cdir)
			vrele(fdp->fd_cdir);
		if (fdp->fd_rdir)
			vrele(fdp->fd_rdir);
		/* This restores the cwd we had before */
		fdp->fd_cdir = fst->fd_cdir;
		fdp->fd_rdir = fst->fd_rdir;
#endif
		/* Note that we are normal again */
		fst->fd_pid = 0;
		fst->fd_cdir = fst->fd_rdir = NULL;
		break;
	case STRIOCGETCWD:
		ret = systrace_getcwd(fst, strp);
		break;
	default:
		ret = EINVAL;
		break;
	}

 unlock:
	SYSTRACE_UNLOCK(fst, curproc);

	return (ret);
}

#ifdef __NetBSD__
/* ARGSUSED */
int
systracef_fcntl(struct file *fp, u_int cmd, caddr_t data, struct proc *p)
{

	if (cmd == FNONBLOCK || cmd == FASYNC)
		return 0;

	return (EOPNOTSUPP);
}
#endif

#ifdef __NetBSD__
int
systracef_poll(struct file *fp, int events, struct proc *p)
{
	struct fsystrace *fst = (struct fsystrace *)fp->f_data;
	int revents = 0;

	if ((events & (POLLIN | POLLRDNORM)) == 0)
		return (revents);

	systrace_lock();
	SYSTRACE_LOCK(fst, p);
	systrace_unlock();
	if (!TAILQ_EMPTY(&fst->messages))
		revents |= events & (POLLIN | POLLRDNORM);
	if (revents == 0)
		selrecord(p, &fst->si);
	SYSTRACE_UNLOCK(fst, p);

	return (revents);
}
#else
int
systracef_select(struct file *fp, int which, struct proc *p)
{
	struct fsystrace *fst = (struct fsystrace *)fp->f_data;
	int ready = 0;

	if (which != FREAD)
		return (0);

	systrace_lock();
	SYSTRACE_LOCK(fst, p);
	systrace_unlock();
	ready = TAILQ_FIRST(&fst->messages) != NULL;
	if (!ready)
		selrecord(p, &fst->si);
	SYSTRACE_UNLOCK(fst, p);

	return (ready);
}
#endif /* __NetBSD__ */

#ifndef __NetBSD__
/* ARGSUSED */
int
systracef_kqfilter(struct file *fp, struct knote *kn)
{
	return (1);
}
#endif


/* ARGSUSED */
int
systracef_stat(struct file *fp, struct stat *sb, struct proc *p)
{
	return (EOPNOTSUPP);
}

/* ARGSUSED */
int
systracef_close(struct file *fp, struct proc *p)
{
	struct fsystrace *fst = (struct fsystrace *)fp->f_data;
	struct str_process *strp;
	struct str_policy *strpol;

	systrace_lock();
	SYSTRACE_LOCK(fst, curproc);
	systrace_unlock();

	/* Untrace all processes */
	for (strp = TAILQ_FIRST(&fst->processes); strp;
	    strp = TAILQ_FIRST(&fst->processes)) {
		struct proc *q = strp->proc;

		systrace_detach(strp);
		psignal(q, SIGKILL);
	}

	/* Clean up fork and exit messages */
	for (strp = TAILQ_FIRST(&fst->messages); strp;
	    strp = TAILQ_FIRST(&fst->messages)) {
		TAILQ_REMOVE(&fst->messages, strp, msg_next);
		pool_put(&systr_proc_pl, strp);
	}

	/* Clean up all policies */
	for (strpol = TAILQ_FIRST(&fst->policies); strpol;
	    strpol = TAILQ_FIRST(&fst->policies))
		systrace_closepolicy(fst, strpol);

	/* Release vnodes */
	if (fst->fd_cdir)
		vrele(fst->fd_cdir);
	if (fst->fd_rdir)
		vrele(fst->fd_rdir);
	SYSTRACE_UNLOCK(fst, curproc);

	FREE(fp->f_data, M_XDATA);
	fp->f_data = NULL;

	return (0);
}

void
systrace_lock(void)
{
#ifdef __NetBSD__
	lockmgr(&systrace_lck, LK_EXCLUSIVE, NULL);
#else
	lockmgr(&systrace_lck, LK_EXCLUSIVE, NULL, curproc);
#endif
}

void
systrace_unlock(void)
{
#ifdef __NetBSD__
	lockmgr(&systrace_lck, LK_RELEASE, NULL);
#else
	lockmgr(&systrace_lck, LK_RELEASE, NULL, curproc);
#endif
}

void
systrace_init(void)
{
	pool_init(&systr_proc_pl, sizeof(struct str_process), 0, 0, 0,
	    "strprocpl", NULL);
	pool_init(&systr_policy_pl, sizeof(struct str_policy), 0, 0, 0,
	    "strpolpl", NULL);
	lockinit(&systrace_lck, PLOCK, "systrace", 0, 0);
}

int
systraceopen(dev_t dev, int flag, int mode, struct proc *p)
{
	struct fsystrace *fst;
	struct file *fp;
	int error, fd;

	/* falloc() will use the descriptor for us. */
	if ((error = falloc(p, &fp, &fd)) != 0)
		return (error);

	MALLOC(fst, struct fsystrace *, sizeof(*fst), M_XDATA, M_WAITOK);

	memset(fst, 0, sizeof(struct fsystrace));
	lockinit(&fst->lock, PLOCK, "systrace", 0, 0);

	TAILQ_INIT(&fst->processes);
	TAILQ_INIT(&fst->messages);
	TAILQ_INIT(&fst->policies);

	if (suser(p->p_ucred, &p->p_acflag) == 0)
		fst->issuser = 1;
	fst->p_ruid = p->p_cred->p_ruid;
	fst->p_rgid = p->p_cred->p_rgid;

	fp->f_flag = FREAD | FWRITE;
	fp->f_type = DTYPE_MISC;
	fp->f_ops = &systracefops;
	fp->f_data = (caddr_t) fst;

	p->p_dupfd = fd;
	FILE_SET_MATURE(fp);
	FILE_UNUSE(fp, p);

	return (ENXIO);
}

void
systrace_wakeup(struct fsystrace *fst)
{
	wakeup((caddr_t)fst);
	selwakeup(&fst->si);
}

struct proc *
systrace_find(struct str_process *strp)
{
	struct proc *proc;

	if ((proc = pfind(strp->pid)) == NULL)
		return (NULL);

	if (proc != strp->proc)
		return (NULL);

	if (!ISSET(proc->p_flag, P_SYSTRACE))
		return (NULL);

	return (proc);
}

void
systrace_sys_exit(struct proc *proc)
{
	struct str_process *strp;
	struct fsystrace *fst;

	systrace_lock();
	strp = proc->p_systrace;
	if (strp != NULL) {
		fst = strp->parent;
		SYSTRACE_LOCK(fst, curproc);
		systrace_unlock();

		/* Insert Exit message */
		systrace_msg_child(fst, strp, -1);

		systrace_detach(strp);
		SYSTRACE_UNLOCK(fst, curproc);
	} else
		systrace_unlock();
	CLR(proc->p_flag, P_SYSTRACE);
}

void
systrace_sys_fork(struct proc *oldproc, struct proc *p)
{
	struct str_process *oldstrp, *strp;
	struct fsystrace *fst;

	systrace_lock();
	oldstrp = oldproc->p_systrace;
	if (oldstrp == NULL) {
		systrace_unlock();
		return;
	}

	fst = oldstrp->parent;
	SYSTRACE_LOCK(fst, curproc);
	systrace_unlock();

	if (systrace_insert_process(fst, p))
		goto out;
	if ((strp = systrace_findpid(fst, p->p_pid)) == NULL)
		panic("systrace_fork");

	/* Reference policy */
	if ((strp->policy = oldstrp->policy) != NULL)
		strp->policy->refcount++;

	/* Insert fork message */
	systrace_msg_child(fst, oldstrp, p->p_pid);
 out:
	SYSTRACE_UNLOCK(fst, curproc);
}

int
systrace_enter(struct proc *p, register_t code, void *v, register_t retval[])
{
	const struct sysent *callp;
	struct str_process *strp;
	struct str_policy *strpolicy;
	struct fsystrace *fst;
	int policy, error = 0, report = 0, maycontrol = 0;

	systrace_lock();
	strp = p->p_systrace;
	if (strp == NULL) {
		systrace_unlock();
		return (EINVAL);
	}

	KASSERT(strp->proc == p);

	fst = strp->parent;

	SYSTRACE_LOCK(fst, p);
	systrace_unlock();

	/*
	 * We can not monitor a SUID process unless we are root,
	 * but we wait until it executes something unprivileged.
	 * A non-root user may only monitor if the real uid and
	 * real gid match the monitored process.  Changing the
	 * uid or gid causes P_SUGID to be set.
	 */
	if (fst->issuser)
		maycontrol = 1;
	else if (!(p->p_flag & P_SUGID)) {
		maycontrol = fst->p_ruid == p->p_cred->p_ruid &&
		    fst->p_rgid == p->p_cred->p_rgid;
	}

	if (!maycontrol) {
		policy = SYSTR_POLICY_PERMIT;
	} else {
		/* Find out current policy */
		if ((strpolicy = strp->policy) == NULL)
			policy = SYSTR_POLICY_ASK;
		else {
			if (code >= strpolicy->nsysent)
				policy = SYSTR_POLICY_NEVER;
			else
				policy = strpolicy->sysent[code];
		}
	}

	callp = p->p_emul->e_sysent + code;
	switch (policy) {
	case SYSTR_POLICY_PERMIT:
		break;
	case SYSTR_POLICY_ASK:
		/* Puts the current process to sleep, return unlocked */
		error = systrace_msg_ask(fst, strp, code, callp->sy_argsize, v);

		/* lock has been released in systrace_msg_ask() */
		fst = NULL;
		/* We might have detached by now for some reason */
		if (!error && (strp = p->p_systrace) != NULL) {
			/* XXX - do I need to lock here? */
			if (strp->answer == SYSTR_POLICY_NEVER) {
				error = strp->error;
				if (strp->replace != NULL) {
					free(strp->replace, M_XDATA);
					strp->replace = NULL;
				}
			} else {
				if (ISSET(strp->flags, STR_PROC_SYSCALLRES)) {
#ifndef __NetBSD__
					CLR(strp->flags, STR_PROC_SYSCALLRES);
#endif
					report = 1;
				}
				/* Replace the arguments if necessary */
				if (strp->replace != NULL) {
					error = systrace_replace(strp, callp->sy_argsize, v);
				}
			}
		}
		break;
	default:
		if (policy > 0)
			error = policy;
		else
			error = EPERM;
		break;
	}

	if (fst) {
		SYSTRACE_UNLOCK(fst, p);
	}
	if (strp != NULL) {
		if (error) {
			strp->oldemul = NULL;
			return (error);
		}

		strp->oldemul = p->p_emul;
		strp->olduid = p->p_cred->p_ruid;
		strp->oldgid = p->p_cred->p_rgid;
	}
	return (error);
}

void
systrace_exit(struct proc *p, register_t code, void *v, register_t retval[],
    int error)
{
	const struct sysent *callp;
	struct str_process *strp;
	struct fsystrace *fst;

	/* Report change in emulation */
	systrace_lock();
	strp = p->p_systrace;
	if (strp == NULL || strp->oldemul == NULL) {
		systrace_unlock();
		return;
	}
	DPRINTF(("exit syscall %lu, oldemul %p\n", (u_long)code, strp->oldemul));

	if (p->p_flag & P_SUGID) {
		if ((fst = strp->parent) == NULL || !fst->issuser) {
			systrace_unlock();
			return;
		}
	}

	/* See if we should force a report */
	if (ISSET(strp->flags, STR_PROC_REPORT)) {
		CLR(strp->flags, STR_PROC_REPORT);
		strp->oldemul = NULL;
	}

	if (p->p_emul != strp->oldemul && strp != NULL) {
		fst = strp->parent;
		SYSTRACE_LOCK(fst, p);
		systrace_unlock();

		/* Old policy is without meaning now */
		if (strp->policy) {
			systrace_closepolicy(fst, strp->policy);
			strp->policy = NULL;
		}
		systrace_msg_emul(fst, strp);
	} else
		systrace_unlock();

	/* Report if effective uid or gid changed */
	systrace_lock();
	strp = p->p_systrace;
	if (strp != NULL && (strp->olduid != p->p_cred->p_ruid ||
	    strp->oldgid != p->p_cred->p_rgid)) {

		fst = strp->parent;
		SYSTRACE_LOCK(fst, p);
		systrace_unlock();

		systrace_msg_ugid(fst, strp);
	} else
		systrace_unlock();

	/* Report result from system call */
	systrace_lock();
	strp = p->p_systrace;
	if (strp != NULL && ISSET(strp->flags, STR_PROC_SYSCALLRES)) {
		CLR(strp->flags, STR_PROC_SYSCALLRES);
		fst = strp->parent;
		SYSTRACE_LOCK(fst, p);
		systrace_unlock();
		DPRINTF(("will ask syscall %lu, strp %p\n", (u_long)code, strp));

		callp = p->p_emul->e_sysent + code;
		systrace_msg_result(fst, strp, error, code,
		    callp->sy_argsize, v, retval);
	} else {
		DPRINTF(("will not ask syscall %lu, strp %p\n", (u_long)code, strp));
		systrace_unlock();
	}
}

/* Called with fst locked */

int
systrace_answer(struct str_process *strp, struct systrace_answer *ans)
{
	int error = 0;

	DPRINTF(("%s: %u: policy %d\n", __func__,
	    ans->stra_pid, ans->stra_policy));

	if (!POLICY_VALID(ans->stra_policy)) {
		error = EINVAL;
		goto out;
	}

	/* Check if answer is in sync with us */
	if (ans->stra_seqnr != strp->seqnr) {
		error = ESRCH;
		goto out;
	}

	if ((error = systrace_processready(strp)) != 0)
		goto out;

	strp->answer = ans->stra_policy;
	strp->error = ans->stra_error;
	if (!strp->error)
		strp->error = EPERM;
	if (ISSET(ans->stra_flags, SYSTR_FLAGS_RESULT))
		SET(strp->flags, STR_PROC_SYSCALLRES);

	/* Clearing the flag indicates to the process that it woke up */
	CLR(strp->flags, STR_PROC_WAITANSWER);
	wakeup(strp);
 out:

	return (error);
}

int
systrace_policy(struct fsystrace *fst, struct systrace_policy *pol)
{
	struct str_policy *strpol;
	struct str_process *strp;

	switch(pol->strp_op) {
	case SYSTR_POLICY_NEW:
		DPRINTF(("%s: new, ents %d\n", __func__,
			    pol->strp_maxents));
		if (pol->strp_maxents <= 0 || pol->strp_maxents > 1024)
			return (EINVAL);
		strpol = systrace_newpolicy(fst, pol->strp_maxents);
		if (strpol == NULL)
			return (ENOBUFS);
		pol->strp_num = strpol->nr;
		break;
	case SYSTR_POLICY_ASSIGN:
		DPRINTF(("%s: %d -> pid %d\n", __func__,
			    pol->strp_num, pol->strp_pid));

		/* Find right policy by number */
		TAILQ_FOREACH(strpol, &fst->policies, next)
		    if (strpol->nr == pol->strp_num)
			    break;
		if (strpol == NULL)
			return (EINVAL);

		strp = systrace_findpid(fst, pol->strp_pid);
		if (strp == NULL)
			return (EINVAL);

		/* Check that emulation matches */
		if (strpol->emul && strpol->emul != strp->proc->p_emul)
			return (EINVAL);

		if (strp->policy)
			systrace_closepolicy(fst, strp->policy);
		strp->policy = strpol;
		strpol->refcount++;

		/* Record emulation for this policy */
		if (strpol->emul == NULL)
			strpol->emul = strp->proc->p_emul;

		break;
	case SYSTR_POLICY_MODIFY:
		DPRINTF(("%s: %d: code %d -> policy %d\n", __func__,
		    pol->strp_num, pol->strp_code, pol->strp_policy));
		if (!POLICY_VALID(pol->strp_policy))
			return (EINVAL);
		TAILQ_FOREACH(strpol, &fst->policies, next)
		    if (strpol->nr == pol->strp_num)
			    break;
		if (strpol == NULL)
			return (EINVAL);
		if (pol->strp_code < 0 || pol->strp_code >= strpol->nsysent)
			return (EINVAL);
		strpol->sysent[pol->strp_code] = pol->strp_policy;
		break;
	default:
		return (EINVAL);
	}

	return (0);
}

int
systrace_processready(struct str_process *strp)
{
	if (ISSET(strp->flags, STR_PROC_ONQUEUE))
		return (EBUSY);

	if (!ISSET(strp->flags, STR_PROC_WAITANSWER))
		return (EBUSY);

	if (strp->proc->p_stat != SSLEEP)
		return (EBUSY);

	return (0);
}

int
systrace_getcwd(struct fsystrace *fst, struct str_process *strp)
{
#ifdef __NetBSD__
	struct cwdinfo *mycwdp, *cwdp;
#else
	struct filedesc *myfdp, *fdp;
#endif
	int error;

	DPRINTF(("%s: %d\n", __func__, strp->pid));

	error = systrace_processready(strp);
	if (error)
		return (error);

#ifdef __NetBSD__
	mycwdp = curproc->p_cwdi;
	cwdp = strp->proc->p_cwdi;
	if (mycwdp == NULL || cwdp == NULL)
		return (EINVAL);

	/* Store our current values */
	fst->fd_pid = strp->pid;
	fst->fd_cdir = mycwdp->cwdi_cdir;
	fst->fd_rdir = mycwdp->cwdi_rdir;

	if ((mycwdp->cwdi_cdir = cwdp->cwdi_cdir) != NULL)
		VREF(mycwdp->cwdi_cdir);
	if ((mycwdp->cwdi_rdir = cwdp->cwdi_rdir) != NULL)
		VREF(mycwdp->cwdi_rdir);
#else
	myfdp = curproc->p_fd;
	fdp = strp->proc->p_fd;
	if (myfdp == NULL || fdp == NULL)
		return (EINVAL);

	/* Store our current values */
	fst->fd_pid = strp->pid;
	fst->fd_cdir = myfdp->fd_cdir;
	fst->fd_rdir = myfdp->fd_rdir;

	if ((myfdp->fd_cdir = fdp->fd_cdir) != NULL)
		VREF(myfdp->fd_cdir);
	if ((myfdp->fd_rdir = fdp->fd_rdir) != NULL)
		VREF(myfdp->fd_rdir);
#endif

	return (0);
}

int
systrace_io(struct str_process *strp, struct systrace_io *io)
{
	struct proc *p = curproc, *t = strp->proc;
	struct uio uio;
	struct iovec iov;
	int error = 0;

	DPRINTF(("%s: %u: %p(%lu)\n", __func__,
	    io->strio_pid, io->strio_offs, (u_long)io->strio_len));

	switch (io->strio_op) {
	case SYSTR_READ:
		uio.uio_rw = UIO_READ;
		break;
	case SYSTR_WRITE:
		uio.uio_rw = UIO_WRITE;
		break;
	default:
		return (EINVAL);
	}

	error = systrace_processready(strp);
	if (error)
		goto out;

	iov.iov_base = io->strio_addr;
	iov.iov_len = io->strio_len;
	uio.uio_iov = &iov;
	uio.uio_iovcnt = 1;
	uio.uio_offset = (off_t)(long)io->strio_offs;
	uio.uio_resid = io->strio_len;
	uio.uio_segflg = UIO_USERSPACE;
	uio.uio_procp = p;

#ifdef __NetBSD__
	error = process_domem(p, t, &uio);
#else
	error = procfs_domem(p, t, NULL, &uio);
#endif
	io->strio_len -= uio.uio_resid;
 out:

	return (error);
}

int
systrace_attach(struct fsystrace *fst, pid_t pid)
{
	int error = 0;
	struct proc *proc, *p = curproc;

	if ((proc = pfind(pid)) == NULL) {
		error = ESRCH;
		goto out;
	}

	if (ISSET(proc->p_flag, P_INEXEC)) {
		error = EAGAIN;
		goto out;
	}

	/*
	 * You can't attach to a process if:
	 *	(1) it's the process that's doing the attaching,
	 */
	if (proc->p_pid == p->p_pid) {
		error = EINVAL;
		goto out;
	}

	/*
	 *	(2) it's a system process
	 */
	if (ISSET(proc->p_flag, P_SYSTEM)) {
		error = EPERM;
		goto out;
	}

	/*
	 *	(3) it's being traced already
	 */
	if (ISSET(proc->p_flag, P_SYSTRACE)) {
		error = EBUSY;
		goto out;
	}

	/*
	 *	(4) it's not owned by you, or the last exec
	 *	    gave us setuid/setgid privs (unless
	 *	    you're root), or...
	 *
	 *      [Note: once P_SUGID gets set in execve(), it stays
	 *	set until the process does another execve(). Hence
	 *	this prevents a setuid process which revokes it's
	 *	special privilidges using setuid() from being
	 *	traced. This is good security.]
	 */
	if ((proc->p_cred->p_ruid != p->p_cred->p_ruid ||
		ISSET(proc->p_flag, P_SUGID)) &&
	    (error = suser(p->p_ucred, &p->p_acflag)) != 0)
		goto out;

	/*
	 *	(5) ...it's init, which controls the security level
	 *	    of the entire system, and the system was not
	 *          compiled with permanently insecure mode turned
	 *	    on.
	 */
	if ((proc->p_pid == 1) && (securelevel > -1)) {
		error = EPERM;
		goto out;
	}

	error = systrace_insert_process(fst, proc);

#if defined(__NetBSD__) && defined(__HAVE_SYSCALL_INTERN)
	/*
	 * Make sure we're using the version of the syscall handler that
	 * has systrace hooks.
	 */
	if (!error)
		(*proc->p_emul->e_syscall_intern)(proc);
#endif
 out:
	return (error);
}

/* Prepare to replace arguments */

int
systrace_preprepl(struct str_process *strp, struct systrace_replace *repl)
{
	size_t len;
	int i, ret = 0;

	ret = systrace_processready(strp);
	if (ret)
		return (ret);

	if (strp->replace != NULL) {
		free(strp->replace, M_XDATA);
		strp->replace = NULL;
	}

	if (repl->strr_nrepl < 0 || repl->strr_nrepl > SYSTR_MAXARGS)
		return (EINVAL);

	for (i = 0, len = 0; i < repl->strr_nrepl; i++) {
		len += repl->strr_offlen[i];
		if (repl->strr_offlen[i] == 0)
			continue;
		if (repl->strr_offlen[i] + repl->strr_off[i] > len)
			return (EINVAL);
	}

	/* Make sure that the length adds up */
	if (repl->strr_len != len)
		return (EINVAL);

	/* Check against a maximum length */
	if (repl->strr_len > 2048)
		return (EINVAL);

	strp->replace = (struct systrace_replace *)
	    malloc(sizeof(struct systrace_replace) + len, M_XDATA, M_WAITOK);

	memcpy(strp->replace, repl, sizeof(struct systrace_replace));
	ret = copyin(repl->strr_base, strp->replace + 1, len);
	if (ret) {
		free(strp->replace, M_XDATA);
		strp->replace = NULL;
		return (ret);
	}

	/* Adjust the offset */
	repl = strp->replace;
	repl->strr_base = (caddr_t)(repl + 1);

	return (0);
}

/*
 * Replace the arguments with arguments from the monitoring process.
 */

int
systrace_replace(struct str_process *strp, size_t argsize, register_t args[])
{
	struct proc *p = strp->proc;
	struct systrace_replace *repl = strp->replace;
	caddr_t sg, kdata, udata, kbase, ubase;
	int i, maxarg, ind, ret = 0;

	maxarg = argsize/sizeof(register_t);
#ifdef __NetBSD__
	sg = stackgap_init(p, 0);
	ubase = stackgap_alloc(p, &sg, repl->strr_len);
#else
	sg = stackgap_init(p->p_emul);
	ubase = stackgap_alloc(&sg, repl->strr_len);
#endif

	kbase = repl->strr_base;
	for (i = 0; i < maxarg && i < repl->strr_nrepl; i++) {
		ind = repl->strr_argind[i];
		if (ind < 0 || ind >= maxarg) {
			ret = EINVAL;
			goto out;
		}
		if (repl->strr_offlen[i] == 0) {
			args[ind] = repl->strr_off[i];
			continue;
		}
		kdata = kbase + repl->strr_off[i];
		udata = ubase + repl->strr_off[i];
		if (copyout(kdata, udata, repl->strr_offlen[i])) {
			ret = EINVAL;
			goto out;
		}

		/* Replace the argument with the new address */
		args[ind] = (register_t)(intptr_t)udata;
	}

 out:
	free(repl, M_XDATA);
	strp->replace = NULL;
	return (ret);
}

struct str_process *
systrace_findpid(struct fsystrace *fst, pid_t pid)
{
	struct str_process *strp;
	struct proc *proc = NULL;

	TAILQ_FOREACH(strp, &fst->processes, next)
	    if (strp->pid == pid)
		    break;

	if (strp == NULL)
		return (NULL);

	proc = systrace_find(strp);

	return (proc ? strp : NULL);
}

int
systrace_detach(struct str_process *strp)
{
	struct proc *proc;
	struct fsystrace *fst = NULL;
	int error = 0;

	DPRINTF(("%s: Trying to detach from %d\n", __func__, strp->pid));

	if ((proc = systrace_find(strp)) != NULL) {
		CLR(proc->p_flag, P_SYSTRACE);
		proc->p_systrace = NULL;
	} else
		error = ESRCH;

	if (ISSET(strp->flags, STR_PROC_WAITANSWER)) {
		CLR(strp->flags, STR_PROC_WAITANSWER);
		wakeup(strp);
	}

	fst = strp->parent;
	systrace_wakeup(fst);

	if (ISSET(strp->flags, STR_PROC_ONQUEUE))
		TAILQ_REMOVE(&fst->messages, strp, msg_next);

	TAILQ_REMOVE(&fst->processes, strp, next);
	fst->nprocesses--;

	if (strp->policy)
		systrace_closepolicy(fst, strp->policy);
	if (strp->replace)
		free(strp->replace, M_XDATA);
	pool_put(&systr_proc_pl, strp);

	return (error);
}

void
systrace_closepolicy(struct fsystrace *fst, struct str_policy *policy)
{
	if (--policy->refcount)
		return;

	fst->npolicies--;

	if (policy->nsysent)
		free(policy->sysent, M_XDATA);

	TAILQ_REMOVE(&fst->policies, policy, next);

	pool_put(&systr_policy_pl, policy);
}


int
systrace_insert_process(struct fsystrace *fst, struct proc *proc)
{
	struct str_process *strp;

	strp = pool_get(&systr_proc_pl, PR_NOWAIT);
	if (strp == NULL)
		return (ENOBUFS);

	memset((caddr_t)strp, 0, sizeof(struct str_process));
	strp->pid = proc->p_pid;
	strp->proc = proc;
	strp->parent = fst;

	TAILQ_INSERT_TAIL(&fst->processes, strp, next);
	fst->nprocesses++;

	proc->p_systrace = strp;
	SET(proc->p_flag, P_SYSTRACE);

	return (0);
}

struct str_policy *
systrace_newpolicy(struct fsystrace *fst, int maxents)
{
	struct str_policy *pol;
	int i;

	if (fst->npolicies > SYSTR_MAX_POLICIES && !fst->issuser)
		return (NULL);

	pol = pool_get(&systr_policy_pl, PR_NOWAIT);
	if (pol == NULL)
		return (NULL);

	DPRINTF(("%s: allocating %d -> %lu\n", __func__,
		     maxents, (u_long)maxents * sizeof(int)));

	memset((caddr_t)pol, 0, sizeof(struct str_policy));

	pol->sysent = (u_char *)malloc(maxents * sizeof(u_char),
	    M_XDATA, M_WAITOK);
	pol->nsysent = maxents;
	for (i = 0; i < maxents; i++)
		pol->sysent[i] = SYSTR_POLICY_ASK;

	fst->npolicies++;
	pol->nr = fst->npolicynr++;
	pol->refcount = 1;

	TAILQ_INSERT_TAIL(&fst->policies, pol, next);

	return (pol);
}

int
systrace_msg_ask(struct fsystrace *fst, struct str_process *strp,
    int code, size_t argsize, register_t args[])
{
	struct str_msg_ask *msg_ask = &strp->msg.msg_data.msg_ask;
	int i;

	msg_ask->code = code;
	msg_ask->argsize = argsize;
	for (i = 0; i < (argsize/sizeof(register_t)) && i < SYSTR_MAXARGS; i++)
		msg_ask->args[i] = args[i];

	return (systrace_make_msg(strp, SYSTR_MSG_ASK));
}

int
systrace_msg_result(struct fsystrace *fst, struct str_process *strp,
    int error, int code, size_t argsize, register_t args[], register_t rval[])
{
	struct str_msg_ask *msg_ask = &strp->msg.msg_data.msg_ask;
	int i;

	msg_ask->code = code;
	msg_ask->argsize = argsize;
	msg_ask->result = error;
	for (i = 0; i < (argsize/sizeof(register_t)) && i < SYSTR_MAXARGS; i++)
		msg_ask->args[i] = args[i];

	msg_ask->rval[0] = rval[0];
	msg_ask->rval[1] = rval[1];

	return (systrace_make_msg(strp, SYSTR_MSG_RES));
}

int
systrace_msg_emul(struct fsystrace *fst, struct str_process *strp)
{
	struct str_msg_emul *msg_emul = &strp->msg.msg_data.msg_emul;
	struct proc *p = strp->proc;

	memcpy(msg_emul->emul, p->p_emul->e_name, SYSTR_EMULEN);

	return (systrace_make_msg(strp, SYSTR_MSG_EMUL));
}

int
systrace_msg_ugid(struct fsystrace *fst, struct str_process *strp)
{
	struct str_msg_ugid *msg_ugid = &strp->msg.msg_data.msg_ugid;
	struct proc *p = strp->proc;

	msg_ugid->uid = p->p_cred->p_ruid;
	msg_ugid->gid = p->p_cred->p_rgid;

	return (systrace_make_msg(strp, SYSTR_MSG_UGID));
}

int
systrace_make_msg(struct str_process *strp, int type)
{
	struct str_message *msg = &strp->msg;
	struct fsystrace *fst = strp->parent;
#ifndef __NetBSD__
	struct proc *p = strp->proc;
#endif
	int st;

	msg->msg_seqnr = ++strp->seqnr;
	msg->msg_type = type;
	msg->msg_pid = strp->pid;
	if (strp->policy)
		msg->msg_policy = strp->policy->nr;
	else
		msg->msg_policy = -1;

	SET(strp->flags, STR_PROC_WAITANSWER);
	if (ISSET(strp->flags, STR_PROC_ONQUEUE))
		goto out;

	TAILQ_INSERT_TAIL(&fst->messages, strp, msg_next);
	SET(strp->flags, STR_PROC_ONQUEUE);

 out:
	systrace_wakeup(fst);

	/* Release the lock - XXX */
#ifndef __NetBSD__
	SYSTRACE_UNLOCK(fst, p);
#else
	SYSTRACE_UNLOCK(fst, strp->proc);
#endif

	while (1) {
		st = tsleep(strp, PWAIT | PCATCH, "systrmsg", 0);
		if (st != 0)
			return (EINTR);
		/* If we detach, then everything is permitted */
		if ((strp = curproc->p_systrace) == NULL)
			return (0);
		if (!ISSET(strp->flags, STR_PROC_WAITANSWER))
			break;
	}

	return (0);
}

int
systrace_msg_child(struct fsystrace *fst, struct str_process *strp, pid_t npid)
{
	struct str_process *nstrp;
	struct str_message *msg;
	struct str_msg_child *msg_child;

	nstrp = pool_get(&systr_proc_pl, PR_WAITOK);
	memset(nstrp, 0, sizeof(struct str_process));

	DPRINTF(("%s: %p: pid %d -> pid %d\n", __func__,
		    nstrp, strp->pid, npid));

	msg = &nstrp->msg;
	msg_child = &msg->msg_data.msg_child;

	msg->msg_type = SYSTR_MSG_CHILD;
	msg->msg_pid = strp->pid;
	if (strp->policy)
		msg->msg_policy = strp->policy->nr;
	else
		msg->msg_policy = -1;
	msg_child->new_pid = npid;

	TAILQ_INSERT_TAIL(&fst->messages, nstrp, msg_next);

	systrace_wakeup(fst);

	return (0);
}
