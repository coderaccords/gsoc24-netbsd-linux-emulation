/*	$NetBSD: proc.h,v 1.109 2000/11/19 00:54:50 sommerfeld Exp $	*/

/*-
 * Copyright (c) 1986, 1989, 1991, 1993
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
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
 *	@(#)proc.h	8.15 (Berkeley) 5/19/95
 */

#ifndef _SYS_PROC_H_
#define	_SYS_PROC_H_

#if defined(_KERNEL) && !defined(_LKM)
#include "opt_multiprocessor.h"
#endif

#if defined(_KERNEL)
#include <machine/cpu.h>		/* curcpu() and cpu_info */
#endif
#include <machine/proc.h>		/* Machine-dependent proc substruct. */
#include <sys/lock.h>
#include <sys/queue.h>
#include <sys/callout.h>

/*
 * One structure allocated per session.
 */
struct	session {
	int	s_count;		/* Ref cnt; pgrps in session. */
	struct	proc *s_leader;		/* Session leader. */
	struct	vnode *s_ttyvp;		/* Vnode of controlling terminal. */
	struct	tty *s_ttyp;		/* Controlling terminal. */
	char	s_login[MAXLOGNAME];	/* Setlogin() name. */
	pid_t	s_sid;			/* session ID (pid of leader) */
};

/*
 * One structure allocated per process group.
 */
struct	pgrp {
	LIST_ENTRY(pgrp) pg_hash;	/* Hash chain. */
	LIST_HEAD(, proc) pg_members;	/* Pointer to pgrp members. */
	struct	session *pg_session;	/* Pointer to session. */
	pid_t	pg_id;			/* Pgrp id. */
	int	pg_jobc;	/* # procs qualifying pgrp for job control */
};

/*
 * One structure allocated per emulation.
 */
struct exec_package;
struct ps_strings;

struct	emul {
	char	e_name[8];		/* Symbolic name */
	int	*e_errno;		/* Errno array */
					/* Signal sending function */
	void	(*e_sendsig) __P((sig_t, int, sigset_t *, u_long));
	int	e_nosys;		/* Offset of the nosys() syscall */
	int	e_nsysent;		/* Number of system call entries */
	struct sysent *e_sysent;	/* System call array */
	const char * const *e_syscallnames;	/* System call name array */
	int	e_arglen;		/* Extra argument size in words */
					/* Copy arguments on the new stack */
	void	*(*e_copyargs) __P((struct exec_package *, struct ps_strings *,
				    void *, void *));
					/* Set registers before execution */
	void	(*e_setregs) __P((struct proc *, struct exec_package *,
				  u_long));

	char	*e_sigcode;		/* Start of sigcode */
	char	*e_esigcode;		/* End of sigcode */

	/* Per-process hooks */
	void	(*e_proc_exec) __P((struct proc *, struct exec_package *));
	void	(*e_proc_fork) __P((struct proc *p, struct proc *parent));
	void	(*e_proc_exit) __P((struct proc *));
};

/*
 * Description of a process.
 *
 * This structure contains the information needed to manage a thread of
 * control, known in UN*X as a process; it has references to substructures
 * containing descriptions of things that the process uses, but may share
 * with related processes.  The process structure and the substructures
 * are always addressible except for those marked "(PROC ONLY)" below,
 * which might be addressible only on a processor on which the process
 * is running.
 */
struct	proc {
	struct	proc *p_forw;		/* Doubly-linked run/sleep queue. */
	struct	proc *p_back;
	LIST_ENTRY(proc) p_list;	/* List of all processes. */

	/* substructures: */
	struct	pcred *p_cred;		/* Process owner's identity. */
	struct	filedesc *p_fd;		/* Ptr to open files structure. */
	struct	cwdinfo *p_cwdi;	/* cdir/rdir/cmask info */
	struct	pstats *p_stats;	/* Accounting/statistics (PROC ONLY). */
	struct	plimit *p_limit;	/* Process limits. */
	struct	vmspace *p_vmspace;	/* Address space. */
	struct	sigacts *p_sigacts;	/* Signal actions, state (PROC ONLY). */

#define	p_ucred		p_cred->pc_ucred
#define	p_rlimit	p_limit->pl_rlimit

	int	p_exitsig;		/* signal to sent to parent on exit */
	int	p_flag;			/* P_* flags. */
	struct cpu_info * __volatile p_cpu; /* CPU we're running on if
					       SONPROC */
	u_char	p_unused;		/* XXX: used to be emulation flag */
	char	p_stat;			/* S* process status. */
	char	p_pad1[2];

	pid_t	p_pid;			/* Process identifier. */
	LIST_ENTRY(proc) p_hash;	/* Hash chain. */
	LIST_ENTRY(proc) p_pglist;	/* List of processes in pgrp. */
	struct	proc *p_pptr;	 	/* Pointer to parent process. */
	LIST_ENTRY(proc) p_sibling;	/* List of sibling processes. */
	LIST_HEAD(, proc) p_children;	/* Pointer to list of children. */

/* The following fields are all zeroed upon creation in fork. */
#define	p_startzero	p_oppid

	pid_t	p_oppid;	 /* Save parent pid during ptrace. XXX */
	int	p_dupfd;	 /* Sideways return value from filedescopen. XXX */

	/* scheduling */
	u_int	p_estcpu;	 /* Time averaged value of p_cpticks. XXX belongs in p_startcopy section */
	int	p_cpticks;	 /* Ticks of cpu time. */
	fixpt_t	p_pctcpu;	 /* %cpu for this process during p_swtime */
	void	*p_wchan;	 /* Sleep address. */
	struct callout p_tsleep_ch;/* callout for tsleep */
	const char *p_wmesg;	 /* Reason for sleep. */
	u_int	p_swtime;	 /* Time swapped in or out. */
	u_int	p_slptime;	 /* Time since last blocked. */

	struct	callout p_realit_ch;	/* real time callout */
	struct	itimerval p_realtimer;	/* Alarm timer. */
	struct	timeval p_rtime;	/* Real time. */
	u_quad_t p_uticks;		/* Statclock hits in user mode. */
	u_quad_t p_sticks;		/* Statclock hits in system mode. */
	u_quad_t p_iticks;		/* Statclock hits processing intr. */

	int	p_traceflag;		/* Kernel trace points. */
	struct 	file *p_tracep;		/* Trace to file */

	sigset_t p_siglist;		/* Signals arrived but not delivered. */
	char	p_sigcheck;		/* May have deliverable signals. */

	struct	vnode *p_textvp;	/* Vnode of executable. */

	int	p_locks;		/* DEBUG: lockmgr count of held locks */

	int	p_holdcnt;		/* If non-zero, don't swap. */
	struct	emul *p_emul;		/* Emulation information */
	void	*p_emuldata;		/* Per-process emulation data, or NULL.
					 * Malloc type M_EMULDATA */

/* End area that is zeroed on creation. */
#define	p_endzero	p_startcopy

/* The following fields are all copied upon creation in fork. */
#define	p_startcopy	p_sigmask

	sigset_t p_sigmask;	/* Current signal mask. */
	sigset_t p_sigignore;	/* Signals being ignored. */
	sigset_t p_sigcatch;	/* Signals being caught by user. */

	u_char	p_priority;	/* Process priority. */
	u_char	p_usrpri;	/* User-priority based on p_cpu and p_nice. */
	u_char	p_nice;		/* Process "nice" value. */
	char	p_comm[MAXCOMLEN+1];

	struct 	pgrp *p_pgrp;	/* Pointer to process group. */
	void	*p_ctxlink;	/* uc_link {get,set}context */

	struct	ps_strings *p_psstr;	/* address of process's ps_strings */
	size_t	p_psargv;		/* offset of ps_argvstr in above */
	size_t	p_psnargv;		/* offset of ps_nargvstr in above */
	size_t	p_psenv;		/* offset of ps_envstr in above */
	size_t	p_psnenv;		/* offset of ps_nenvstr in above */

/* End area that is copied on creation. */
#define	p_endcopy	p_thread

	void	*p_thread;	/* Id for this "thread"; Mach glue. XXX */
	struct	user *p_addr;	/* Kernel virtual addr of u-area (PROC ONLY). */
	struct	mdproc p_md;	/* Any machine-dependent fields. */

	u_short	p_xstat;	/* Exit status for wait; also stop signal. */
	u_short	p_acflag;	/* Accounting flags. */
	struct	rusage *p_ru;	/* Exit information. XXX */
};

#define	p_session	p_pgrp->pg_session
#define	p_pgid		p_pgrp->pg_id

/*
 * Status values.
 *
 * A note about SRUN and SONPROC: SRUN indicates that a process is
 * runnable but *not* yet running, i.e. is on a run queue.  SONPROC
 * indicates that the process is actually executing on a CPU, i.e.
 * it is no longer on a run queue.
 */
#define	SIDL	1		/* Process being created by fork. */
#define	SRUN	2		/* Currently runnable. */
#define	SSLEEP	3		/* Sleeping on an address. */
#define	SSTOP	4		/* Process debugging or suspension. */
#define	SZOMB	5		/* Awaiting collection by parent. */
#define	SDEAD	6		/* Process is almost a zombie. */
#define	SONPROC	7		/* Process is currently on a CPU */

#define	P_ZOMBIE(p)	((p)->p_stat == SZOMB || (p)->p_stat == SDEAD)

/* These flags are kept in p_flag. */
#define	P_ADVLOCK	0x00001	/* Process may hold a POSIX advisory lock. */
#define	P_CONTROLT	0x00002	/* Has a controlling terminal. */
#define	P_INMEM		0x00004	/* Loaded into memory. */
#define	P_NOCLDSTOP	0x00008	/* No SIGCHLD when children stop. */
#define	P_PPWAIT	0x00010	/* Parent is waiting for child to exec/exit. */
#define	P_PROFIL	0x00020	/* Has started profiling. */
#define	P_SELECT	0x00040	/* Selecting; wakeup/waiting danger. */
#define	P_SINTR		0x00080	/* Sleep is interruptible. */
#define	P_SUGID		0x00100	/* Had set id privileges since last exec. */
#define	P_SYSTEM	0x00200	/* System proc: no sigs, stats or swapping. */
#define	P_TIMEOUT	0x00400	/* Timing out during sleep. */
#define	P_TRACED	0x00800	/* Debugged process being traced. */
#define	P_WAITED	0x01000	/* Debugging process has waited for child. */
#define	P_WEXIT		0x02000	/* Working on exiting. */
#define	P_EXEC		0x04000	/* Process called exec. */
#define	P_OWEUPC	0x08000	/* Owe process an addupc() call at next ast. */
#define	P_FSTRACE	0x10000	/* Debugger process being traced by procfs */
#define	P_NOCLDWAIT	0x20000	/* No zombies if child dies */
#define	P_32		0x40000	/* 32-bit process (used on 64-bit kernels) */
#define P_BIGLOCK	0x80000 /* Process needs kernel "big lock" to run */


/*
 * Macro to compute the exit signal to be delivered.
 */
#define	P_EXITSIG(p)	(((p)->p_flag & (P_TRACED|P_FSTRACE)) ? SIGCHLD : \
			 p->p_exitsig)

/*
 * MOVE TO ucred.h?
 *
 * Shareable process credentials (always resident).  This includes a reference
 * to the current user credentials as well as real and saved ids that may be
 * used to change ids.
 */
struct	pcred {
	struct	ucred *pc_ucred;	/* Current credentials. */
	uid_t	p_ruid;			/* Real user id. */
	uid_t	p_svuid;		/* Saved effective user id. */
	gid_t	p_rgid;			/* Real group id. */
	gid_t	p_svgid;		/* Saved effective group id. */
	int	p_refcnt;		/* Number of references. */
};

LIST_HEAD(proclist, proc);		/* a list of processes */

/*
 * This structure associates a proclist with its lock.
 */
struct proclist_desc {
	struct proclist *pd_list;	/* the list */
	/*
	 * XXX Add a pointer to the proclist's lock eventually.
	 */
};

#ifdef _KERNEL
/*
 * We use process IDs <= PID_MAX; PID_MAX + 1 must also fit in a pid_t,
 * as it is used to represent "no process group".
 */
#define	PID_MAX		30000
#define	NO_PID		30001

#define SESS_LEADER(p)	((p)->p_session->s_leader == (p))
#define	SESSHOLD(s)	((s)->s_count++)
#define	SESSRELE(s) {							\
	if (--(s)->s_count == 0)					\
		FREE(s, M_SESSION);					\
}

#define	PHOLD(p) {							\
	if ((p)->p_holdcnt++ == 0 && ((p)->p_flag & P_INMEM) == 0)	\
		uvm_swapin(p);						\
}
#define	PRELE(p)	(--(p)->p_holdcnt)

/*
 * Flags passed to fork1().
 */
#define	FORK_PPWAIT	0x01		/* block parent until child exit */
#define	FORK_SHAREVM	0x02		/* share vmspace with parent */
#define	FORK_SHARECWD	0x04		/* share cdir/rdir/cmask */
#define	FORK_SHAREFILES	0x08		/* share file descriptors */
#define	FORK_SHARESIGS	0x10		/* share signal actions */

#define	PIDHASH(pid)	(&pidhashtbl[(pid) & pidhash])
extern LIST_HEAD(pidhashhead, proc) *pidhashtbl;
extern u_long pidhash;

#define	PGRPHASH(pgid)	(&pgrphashtbl[(pgid) & pgrphash])
extern LIST_HEAD(pgrphashhead, pgrp) *pgrphashtbl;
extern u_long pgrphash;

/*
 * Allow machine-dependent code to override curproc in <machine/cpu.h> for
 * its own convenience.  Otherwise, we declare it as appropriate.
 */
#if !defined(curproc)
#if defined(MULTIPROCESSOR)
#define	curproc	curcpu()->ci_curproc	/* Current running proc. */
#else
extern struct proc *curproc;		/* Current running proc. */
#endif /* MULTIPROCESSOR */
#endif /* ! curproc */

extern struct proc proc0;		/* Process slot for swapper. */
extern int nprocs, maxproc;		/* Current and max number of procs. */

extern struct proclist allproc;		/* List of all processes. */
extern struct proclist zombproc;	/* List of zombie processes. */

extern struct proclist deadproc;	/* List of dead processes. */
extern struct simplelock deadproc_slock;

extern struct proc *initproc;		/* Process slots for init, pager. */

extern const struct proclist_desc proclists[];

extern struct pool proc_pool;		/* memory pool for procs */
extern struct pool pcred_pool;		/* memory pool for pcreds */
extern struct pool plimit_pool;		/* memory pool for plimits */
extern struct pool rusage_pool;		/* memory pool for rusages */

struct proc *pfind __P((pid_t));	/* Find process by id. */
struct pgrp *pgfind __P((pid_t));	/* Find process group by id. */

struct simplelock;

int	chgproccnt __P((uid_t uid, int diff));
int	enterpgrp __P((struct proc *p, pid_t pgid, int mksess));
void	fixjobc __P((struct proc *p, struct pgrp *pgrp, int entering));
int	inferior __P((struct proc *p, struct proc *q));
int	leavepgrp __P((struct proc *p));
void	yield __P((void));
void	preempt __P((struct proc *));
void	mi_switch __P((struct proc *));
void	pgdelete __P((struct pgrp *pgrp));
void	procinit __P((void));
void	remrunqueue __P((struct proc *));
void	resetpriority __P((struct proc *));
void	setrunnable __P((struct proc *));
void	setrunqueue __P((struct proc *));
void	suspendsched __P((void));
int	ltsleep __P((void *chan, int pri, const char *wmesg, int timo,
	    __volatile struct simplelock *));
void	unsleep __P((struct proc *));
void	wakeup __P((void *chan));
void	wakeup_one __P((void *chan));
void	reaper __P((void *));
void	exit1 __P((struct proc *, int));
void	exit2 __P((struct proc *));
int	fork1 __P((struct proc *, int, int, void *, size_t,
	    void (*)(void *), void *, register_t *, struct proc **));
void	rqinit __P((void));
int	groupmember __P((gid_t, struct ucred *));
void	cpu_switch __P((struct proc *));
void	cpu_wait __P((struct proc *));
void	cpu_exit __P((struct proc *));
void	cpu_fork __P((struct proc *, struct proc *, void *, size_t,
	    void (*)(void *), void *));

void	child_return __P((void *));

int	proc_isunder __P((struct proc *, struct proc*));

#if defined(MULTIPROCESSOR) || defined(LOCKDEBUG)
/* Process list lock; see kern_proc.c for locking protocol details. */
extern struct lock proclist_lock;

void	proclist_lock_read __P((void));
void	proclist_unlock_read __P((void));
int	proclist_lock_write __P((void));
void	proclist_unlock_write __P((int));

#else

#define	proclist_lock_read()	/* nothing */
#define	proclist_unlock_read()
#define	proclist_lock_write() splclock()
#define	proclist_unlock_write(s) splx(s)

#endif

void	p_sugid __P((struct proc*));

/* Compatbility with old, non-interlocked tsleep call. */
#define	tsleep(chan, pri, wmesg, timo)					\
	ltsleep(chan, pri, wmesg, timo, NULL)

#if defined(MULTIPROCESSOR)
void	proc_trampoline_mp(void);	/* XXX */
#endif

#endif	/* _KERNEL */
#endif	/* !_SYS_PROC_H_ */
