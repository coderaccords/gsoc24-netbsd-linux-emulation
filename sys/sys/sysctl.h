/*	$NetBSD: sysctl.h,v 1.112 2004/03/24 17:21:02 atatat Exp $	*/

/*
 * Copyright (c) 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Mike Karels at Berkeley Software Design, Inc.
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
 *	@(#)sysctl.h	8.1 (Berkeley) 6/2/93
 */

#ifndef _SYS_SYSCTL_H_
#define	_SYS_SYSCTL_H_

/*
 * These are for the eproc structure defined below.
 */
#include <sys/time.h>
#include <sys/ucred.h>
#include <sys/ucontext.h>
#include <sys/proc.h>
#include <uvm/uvm_extern.h>

/*
 * Definitions for sysctl call.  The sysctl call uses a hierarchical name
 * for objects that can be examined or modified.  The name is expressed as
 * a sequence of integers.  Like a file path name, the meaning of each
 * component depends on its place in the hierarchy.  The top-level and kern
 * identifiers are defined here, and other identifiers are defined in the
 * respective subsystem header files.
 */

#define	CTL_MAXNAME	12	/* largest number of components supported */
#define SYSCTL_NAMELEN	32	/* longest name allowed for a node */

#define CREATE_BASE	(1024)	/* start of dynamic mib allocation */
#define SYSCTL_DEFSIZE	8	/* initial size of a child set */

/*
 * Each subsystem defined by sysctl defines a list of variables
 * for that subsystem. Each name is either a node with further
 * levels defined below it, or it is a leaf of some particular
 * type given below. Each sysctl level defines a set of name/type
 * pairs to be used by sysctl(1) in manipulating the subsystem.
 */
struct ctlname {
	const char *ctl_name;	/* subsystem name */
	int	ctl_type;	/* type of name */
};
#define	CTLTYPE_NODE	1	/* name is a node */
#define	CTLTYPE_INT	2	/* name describes an integer */
#define	CTLTYPE_STRING	3	/* name describes a string */
#define	CTLTYPE_QUAD	4	/* name describes a 64-bit number */
#define	CTLTYPE_STRUCT	5	/* name describes a structure */

/*
 * Flags that apply to each node, governing access and other features
 */
#define CTLFLAG_READONLY	0x00000000
#define CTLFLAG_READONLY1	0x00000010
#define CTLFLAG_READONLY2	0x00000020
/* #define CTLFLAG_READ*	0x00000040 */
#define CTLFLAG_READWRITE	0x00000070
#define CTLFLAG_ANYWRITE	0x00000080
#define CTLFLAG_PRIVATE		0x00000100
#define CTLFLAG_PERMANENT	0x00000200
#define CTLFLAG_OWNDATA		0x00000400
#define CTLFLAG_IMMEDIATE	0x00000800
#define CTLFLAG_HEX		0x00001000
#define CTLFLAG_ROOT		0x00002000
#define CTLFLAG_ANYNUMBER	0x00004000
#define CTLFLAG_HIDDEN		0x00008000
#define CTLFLAG_ALIAS		0x00010000
#define CTLFLAG_MMAP		0x00020000

/*
 * sysctl API version
 */
#define SYSCTL_VERS_MASK	0xff000000
#define SYSCTL_VERS_0		0x00000000
#define SYSCTL_VERS_1		0x01000000
#define SYSCTL_VERSION		SYSCTL_VERS_1
#define SYSCTL_VERS(f)		((f) & SYSCTL_VERS_MASK)

/*
 * Flags that can be set by a create request from user-space
 */
#define SYSCTL_USERFLAGS	(CTLFLAG_READWRITE|\
				CTLFLAG_ANYWRITE|\
				CTLFLAG_PRIVATE|\
				CTLFLAG_OWNDATA|\
				CTLFLAG_IMMEDIATE|\
				CTLFLAG_HEX|\
				CTLFLAG_HIDDEN)

/*
 * Accessor macros
 */
#define SYSCTL_TYPEMASK		0x0000000f
#define SYSCTL_TYPE(x)		((x) & SYSCTL_TYPEMASK)
#define SYSCTL_FLAGMASK		0x00fffff0
#define SYSCTL_FLAGS(x)		((x) & SYSCTL_FLAGMASK)

/*
 * Meta-identifiers
 */
#define CTL_EOL		-1		/* end of createv/destroyv list */
#define CTL_QUERY	-2		/* enumerates children of a node */
#define CTL_CREATE	-3		/* node create request */
#define CTL_CREATESYM	-4		/* node create request with symbol */
#define CTL_DESTROY	-5		/* node destroy request */
#define CTL_MMAP	-6		/* mmap request */

/*
 * Top-level identifiers
 */
#define	CTL_UNSPEC	0		/* unused */
#define	CTL_KERN	1		/* "high kernel": proc, limits */
#define	CTL_VM		2		/* virtual memory */
#define	CTL_VFS		3		/* file system, mount type is next */
#define	CTL_NET		4		/* network, see socket.h */
#define	CTL_DEBUG	5		/* debugging parameters */
#define	CTL_HW		6		/* generic CPU/io */
#define	CTL_MACHDEP	7		/* machine dependent */
#define	CTL_USER	8		/* user-level */
#define	CTL_DDB		9		/* in-kernel debugger */
#define	CTL_PROC	10		/* per-proc attr */
#define	CTL_VENDOR	11		/* vendor-specific data */
#define	CTL_EMUL	12		/* emulation-specific data */
#define	CTL_MAXID	13		/* number of valid top-level ids */

#define	CTL_NAMES { \
	{ 0, 0 }, \
	{ "kern", CTLTYPE_NODE }, \
	{ "vm", CTLTYPE_NODE }, \
	{ "vfs", CTLTYPE_NODE }, \
	{ "net", CTLTYPE_NODE }, \
	{ "debug", CTLTYPE_NODE }, \
	{ "hw", CTLTYPE_NODE }, \
	{ "machdep", CTLTYPE_NODE }, \
	{ "user", CTLTYPE_NODE }, \
	{ "ddb", CTLTYPE_NODE }, \
	{ "proc", CTLTYPE_NODE }, \
	{ "vendor", CTLTYPE_NODE }, \
	{ "emul", CTLTYPE_NODE }, \
}

/*
 * The "vendor" toplevel name is to be used by vendors who wish to
 * have their own private MIB tree. If you do that, please use
 * vendor.<yourname>.*
 */

/*
 * CTL_KERN identifiers
 */
#define	KERN_OSTYPE	 	 1	/* string: system version */
#define	KERN_OSRELEASE	 	 2	/* string: system release */
#define	KERN_OSREV	 	 3	/* int: system revision */
#define	KERN_VERSION	 	 4	/* string: compile time info */
#define	KERN_MAXVNODES	 	 5	/* int: max vnodes */
#define	KERN_MAXPROC	 	 6	/* int: max processes */
#define	KERN_MAXFILES	 	 7	/* int: max open files */
#define	KERN_ARGMAX	 	 8	/* int: max arguments to exec */
#define	KERN_SECURELVL	 	 9	/* int: system security level */
#define	KERN_HOSTNAME		10	/* string: hostname */
#define	KERN_HOSTID		11	/* int: host identifier */
#define	KERN_CLOCKRATE		12	/* struct: struct clockrate */
#define	KERN_VNODE		13	/* struct: vnode structures */
#define	KERN_PROC		14	/* struct: process entries */
#define	KERN_FILE		15	/* struct: file entries */
#define	KERN_PROF		16	/* node: kernel profiling info */
#define	KERN_POSIX1		17	/* int: POSIX.1 version */
#define	KERN_NGROUPS		18	/* int: # of supplemental group ids */
#define	KERN_JOB_CONTROL	19	/* int: is job control available */
#define	KERN_SAVED_IDS		20	/* int: saved set-user/group-ID */
#define	KERN_BOOTTIME		21	/* struct: time kernel was booted */
#define	KERN_DOMAINNAME		22	/* string: (YP) domainname */
#define	KERN_MAXPARTITIONS	23	/* int: number of partitions/disk */
#define	KERN_RAWPARTITION	24	/* int: raw partition number */
#define	KERN_NTPTIME		25	/* struct: extended-precision time */
#define	KERN_TIMEX		26	/* struct: ntp timekeeping state */
#define	KERN_AUTONICETIME	27	/* int: proc time before autonice */
#define	KERN_AUTONICEVAL	28	/* int: auto nice value */
#define	KERN_RTC_OFFSET		29	/* int: offset of rtc from gmt */
#define	KERN_ROOT_DEVICE	30	/* string: root device */
#define	KERN_MSGBUFSIZE		31	/* int: max # of chars in msg buffer */
#define	KERN_FSYNC		32	/* int: file synchronization support */
#define	KERN_SYSVMSG		33	/* int: SysV message queue suppoprt */
#define	KERN_SYSVSEM		34	/* int: SysV semaphore support */
#define	KERN_SYSVSHM		35	/* int: SysV shared memory support */
#define	KERN_OLDSHORTCORENAME	36	/* old, unimplemented */
#define	KERN_SYNCHRONIZED_IO	37	/* int: POSIX synchronized I/O */
#define	KERN_IOV_MAX		38	/* int: max iovec's for readv(2) etc. */
#define	KERN_MBUF		39	/* node: mbuf parameters */
#define	KERN_MAPPED_FILES	40	/* int: POSIX memory mapped files */
#define	KERN_MEMLOCK		41	/* int: POSIX memory locking */
#define	KERN_MEMLOCK_RANGE	42	/* int: POSIX memory range locking */
#define	KERN_MEMORY_PROTECTION	43	/* int: POSIX memory protections */
#define	KERN_LOGIN_NAME_MAX	44	/* int: max length login name + NUL */
#define	KERN_DEFCORENAME	45	/* old: sort core name format */
#define	KERN_LOGSIGEXIT		46	/* int: log signalled processes */
#define	KERN_PROC2		47	/* struct: process entries */
#define	KERN_PROC_ARGS		48	/* struct: process argv/env */
#define	KERN_FSCALE		49	/* int: fixpt FSCALE */
#define	KERN_CCPU		50	/* int: fixpt ccpu */
#define	KERN_CP_TIME		51	/* struct: CPU time counters */
#define	KERN_SYSVIPC_INFO	52	/* number of valid kern ids */
#define	KERN_MSGBUF		53	/* kernel message buffer */
#define	KERN_CONSDEV		54	/* dev_t: console terminal device */
#define	KERN_MAXPTYS		55	/* int: maximum number of ptys */
#define	KERN_PIPE		56	/* node: pipe limits */
#define	KERN_MAXPHYS		57	/* int: kernel value of MAXPHYS */
#define	KERN_SBMAX		58	/* int: max socket buffer size */
#define	KERN_TKSTAT		59	/* tty in/out counters */
#define	KERN_MONOTONIC_CLOCK	60	/* int: POSIX monotonic clock */
#define	KERN_URND		61	/* int: random integer from urandom */
#ifndef _KERNEL
#define	KERN_ARND		KERN_URND	/* compat w/ openbsd */
#endif
#define	KERN_LABELSECTOR	62	/* int: disklabel sector */
#define	KERN_LABELOFFSET	63	/* int: offset of label within sector */
#define	KERN_LWP		64	/* struct: lwp entries */
#define	KERN_FORKFSLEEP		65	/* int: sleep length on failed fork */
#define	KERN_POSIX_THREADS	66	/* int: POSIX Threads option */
#define	KERN_POSIX_SEMAPHORES	67	/* int: POSIX Semaphores option */
#define	KERN_POSIX_BARRIERS	68	/* int: POSIX Barriers option */
#define	KERN_POSIX_TIMERS	69	/* int: POSIX Timers option */
#define	KERN_POSIX_SPIN_LOCKS	70	/* int: POSIX Spin Locks option */
#define	KERN_POSIX_READER_WRITER_LOCKS 71 /* int: POSIX R/W Locks option */
#define	KERN_DUMP_ON_PANIC	72	/* int: dump on panic */
#define	KERN_SOMAXKVA		73	/* int: max socket kernel virtual mem */
#define	KERN_ROOT_PARTITION	74	/* int: root partition */
#define	KERN_DRIVERS		75	/* struct: driver names and majors #s */
#define	KERN_BUF		76	/* struct: buffers */
#define	KERN_MAXID		77	/* number of valid kern ids */


#define	CTL_KERN_NAMES { \
	{ 0, 0 }, \
	{ "ostype", CTLTYPE_STRING }, \
	{ "osrelease", CTLTYPE_STRING }, \
	{ "osrevision", CTLTYPE_INT }, \
	{ "version", CTLTYPE_STRING }, \
	{ "maxvnodes", CTLTYPE_INT }, \
	{ "maxproc", CTLTYPE_INT }, \
	{ "maxfiles", CTLTYPE_INT }, \
	{ "argmax", CTLTYPE_INT }, \
	{ "securelevel", CTLTYPE_INT }, \
	{ "hostname", CTLTYPE_STRING }, \
	{ "hostid", CTLTYPE_INT }, \
	{ "clockrate", CTLTYPE_STRUCT }, \
	{ "vnode", CTLTYPE_STRUCT }, \
	{ "proc", CTLTYPE_STRUCT }, \
	{ "file", CTLTYPE_STRUCT }, \
	{ "profiling", CTLTYPE_NODE }, \
	{ "posix1version", CTLTYPE_INT }, \
	{ "ngroups", CTLTYPE_INT }, \
	{ "job_control", CTLTYPE_INT }, \
	{ "saved_ids", CTLTYPE_INT }, \
	{ "boottime", CTLTYPE_STRUCT }, \
	{ "domainname", CTLTYPE_STRING }, \
	{ "maxpartitions", CTLTYPE_INT }, \
	{ "rawpartition", CTLTYPE_INT }, \
	{ "ntptime", CTLTYPE_STRUCT }, \
	{ "timex", CTLTYPE_STRUCT }, \
	{ "autonicetime", CTLTYPE_INT }, \
	{ "autoniceval", CTLTYPE_INT }, \
	{ "rtc_offset", CTLTYPE_INT }, \
	{ "root_device", CTLTYPE_STRING }, \
	{ "msgbufsize", CTLTYPE_INT }, \
	{ "fsync", CTLTYPE_INT }, \
	{ "sysvmsg", CTLTYPE_INT }, \
	{ "sysvsem", CTLTYPE_INT }, \
	{ "sysvshm", CTLTYPE_INT }, \
	{ 0, 0 }, \
	{ "synchronized_io", CTLTYPE_INT }, \
	{ "iov_max", CTLTYPE_INT }, \
	{ "mbuf", CTLTYPE_NODE }, \
	{ "mapped_files", CTLTYPE_INT }, \
	{ "memlock", CTLTYPE_INT }, \
	{ "memlock_range", CTLTYPE_INT }, \
	{ "memory_protection", CTLTYPE_INT }, \
	{ "login_name_max", CTLTYPE_INT }, \
	{ "defcorename", CTLTYPE_STRING }, \
	{ "logsigexit", CTLTYPE_INT }, \
	{ "proc2", CTLTYPE_STRUCT }, \
	{ "proc_args", CTLTYPE_STRING }, \
	{ "fscale", CTLTYPE_INT }, \
	{ "ccpu", CTLTYPE_INT }, \
	{ "cp_time", CTLTYPE_STRUCT }, \
	{ "sysvipc_info", CTLTYPE_STRUCT }, \
	{ "msgbuf", CTLTYPE_STRUCT }, \
	{ "consdev", CTLTYPE_STRUCT }, \
	{ "maxptys", CTLTYPE_INT }, \
	{ "pipe", CTLTYPE_NODE }, \
	{ "maxphys", CTLTYPE_INT }, \
	{ "sbmax", CTLTYPE_INT }, \
	{ "tkstat", CTLTYPE_NODE }, \
	{ "monotonic_clock", CTLTYPE_INT }, \
	{ "urandom", CTLTYPE_INT }, \
	{ "labelsector", CTLTYPE_INT }, \
	{ "labeloffset", CTLTYPE_INT }, \
	{ "lwp", CTLTYPE_STRUCT }, \
	{ "forkfsleep", CTLTYPE_INT }, \
	{ "posix_threads", CTLTYPE_INT }, \
	{ "posix_semaphores", CTLTYPE_INT }, \
	{ "posix_barriers", CTLTYPE_INT }, \
	{ "posix_timers", CTLTYPE_INT }, \
	{ "posix_spin_locks", CTLTYPE_INT }, \
	{ "posix_reader_writer_locks", CTLTYPE_INT }, \
	{ "dump_on_panic", CTLTYPE_INT}, \
	{ "somaxkva", CTLTYPE_INT}, \
	{ "root_partition", CTLTYPE_INT}, \
	{ "drivers", CTLTYPE_STRUCT }, \
}

/*
 * KERN_PROC subtypes
 */
#define	KERN_PROC_ALL		 0	/* everything */
#define	KERN_PROC_PID		 1	/* by process id */
#define	KERN_PROC_PGRP		 2	/* by process group id */
#define	KERN_PROC_SESSION	 3	/* by session of pid */
#define	KERN_PROC_TTY		 4	/* by controlling tty */
#define	KERN_PROC_UID		 5	/* by effective uid */
#define	KERN_PROC_RUID		 6	/* by real uid */
#define	KERN_PROC_GID		 7	/* by effective gid */
#define	KERN_PROC_RGID		 8	/* by real gid */

/*
 * KERN_PROC_TTY sub-subtypes
 */
#define	KERN_PROC_TTY_NODEV	NODEV		/* no controlling tty */
#define	KERN_PROC_TTY_REVOKE	((dev_t)-2)	/* revoked tty */

/*
 * KERN_PROC subtype ops return arrays of augmented proc structures:
 */
struct kinfo_proc {
	struct	proc kp_proc;			/* proc structure */
	struct	eproc {
		struct	proc *e_paddr;		/* address of proc */
		struct	session *e_sess;	/* session pointer */
		struct	pcred e_pcred;		/* process credentials */
		struct	ucred e_ucred;		/* current credentials */
		struct	vmspace e_vm;		/* address space */
		pid_t	e_ppid;			/* parent process id */
		pid_t	e_pgid;			/* process group id */
		short	e_jobc;			/* job control counter */
		dev_t	e_tdev;			/* controlling tty dev */
		pid_t	e_tpgid;		/* tty process group id */
		struct	session *e_tsess;	/* tty session pointer */
#define	WMESGLEN	8
		char	e_wmesg[WMESGLEN];	/* wchan message */
		segsz_t e_xsize;		/* text size */
		short	e_xrssize;		/* text rss */
		short	e_xccount;		/* text references */
		short	e_xswrss;
		long	e_flag;
#define	EPROC_CTTY	0x01	/* controlling tty vnode active */
#define	EPROC_SLEADER	0x02	/* session leader */
		char	e_login[MAXLOGNAME];	/* setlogin() name */
		pid_t	e_sid;			/* session id */
		long	e_spare[3];
	} kp_eproc;
};

/*
 * Convert pointer to 64 bit unsigned integer for struct
 * kinfo_proc2, etc.
 */
#define PTRTOUINT64(p) ((u_int64_t)(uintptr_t)(p))
#define UINT64TOPTR(u) ((void *)(uintptr_t)(u))

/*
 * KERN_PROC2 subtype ops return arrays of relatively fixed size
 * structures of process info.   Use 8 byte alignment, and new
 * elements should only be added to the end of this structure so
 * binary compatibility can be preserved.
 */
#define	KI_NGROUPS	16
#define	KI_MAXCOMLEN	24	/* extra for 8 byte alignment */
#define	KI_WMESGLEN	8
#define	KI_MAXLOGNAME	24	/* extra for 8 byte alignment */

#define KI_NOCPU	(~(u_int64_t)0)

typedef struct {
	u_int32_t	__bits[4];
} ki_sigset_t;

struct kinfo_proc2 {
	u_int64_t p_forw;		/* PTR: linked run/sleep queue. */
	u_int64_t p_back;
	u_int64_t p_paddr;		/* PTR: address of proc */

	u_int64_t p_addr;		/* PTR: Kernel virtual addr of u-area */
	u_int64_t p_fd;			/* PTR: Ptr to open files structure. */
	u_int64_t p_cwdi;		/* PTR: cdir/rdir/cmask info */
	u_int64_t p_stats;		/* PTR: Accounting/statistics */
	u_int64_t p_limit;		/* PTR: Process limits. */
	u_int64_t p_vmspace;		/* PTR: Address space. */
	u_int64_t p_sigacts;		/* PTR: Signal actions, state */
	u_int64_t p_sess;		/* PTR: session pointer */
	u_int64_t p_tsess;		/* PTR: tty session pointer */
	u_int64_t p_ru;			/* PTR: Exit information. XXX */

	int32_t	p_eflag;		/* LONG: extra kinfo_proc2 flags */
	int32_t	p_exitsig;		/* INT: signal to sent to parent on exit */
	int32_t	p_flag;			/* INT: P_* flags. */

	int32_t	p_pid;			/* PID_T: Process identifier. */
	int32_t	p_ppid;			/* PID_T: Parent process id */
	int32_t	p_sid;			/* PID_T: session id */
	int32_t	p__pgid;		/* PID_T: process group id */
					/* XXX: <sys/proc.h> hijacks p_pgid */
	int32_t	p_tpgid;		/* PID_T: tty process group id */

	u_int32_t p_uid;		/* UID_T: effective user id */
	u_int32_t p_ruid;		/* UID_T: real user id */
	u_int32_t p_gid;		/* GID_T: effective group id */
	u_int32_t p_rgid;		/* GID_T: real group id */

	u_int32_t p_groups[KI_NGROUPS];	/* GID_T: groups */
	int16_t	p_ngroups;		/* SHORT: number of groups */

	int16_t	p_jobc;			/* SHORT: job control counter */
	u_int32_t p_tdev;		/* DEV_T: controlling tty dev */

	u_int32_t p_estcpu;		/* U_INT: Time averaged value of p_cpticks. */
	u_int32_t p_rtime_sec;		/* STRUCT TIMEVAL: Real time. */
	u_int32_t p_rtime_usec;		/* STRUCT TIMEVAL: Real time. */
	int32_t	p_cpticks;		/* INT: Ticks of CPU time. */
	u_int32_t p_pctcpu;		/* FIXPT_T: %cpu for this process during p_swtime */
	u_int32_t p_swtime;		/* U_INT: Time swapped in or out. */
	u_int32_t p_slptime;		/* U_INT: Time since last blocked. */
	int32_t	p_schedflags;		/* INT: PSCHED_* flags */

	u_int64_t p_uticks;		/* U_QUAD_T: Statclock hits in user mode. */
	u_int64_t p_sticks;		/* U_QUAD_T: Statclock hits in system mode. */
	u_int64_t p_iticks;		/* U_QUAD_T: Statclock hits processing intr. */

	u_int64_t p_tracep;		/* PTR: Trace to vnode or file */
	int32_t	p_traceflag;		/* INT: Kernel trace points. */

	int32_t	p_holdcnt;              /* INT: If non-zero, don't swap. */

	ki_sigset_t p_siglist;		/* SIGSET_T: Signals arrived but not delivered. */
	ki_sigset_t p_sigmask;		/* SIGSET_T: Current signal mask. */
	ki_sigset_t p_sigignore;	/* SIGSET_T: Signals being ignored. */
	ki_sigset_t p_sigcatch;		/* SIGSET_T: Signals being caught by user. */

	int8_t	p_stat;			/* CHAR: S* process status (from LWP). */
	u_int8_t p_priority;		/* U_CHAR: Process priority. */
	u_int8_t p_usrpri;		/* U_CHAR: User-priority based on p_cpu and p_nice. */
	u_int8_t p_nice;		/* U_CHAR: Process "nice" value. */

	u_int16_t p_xstat;		/* U_SHORT: Exit status for wait; also stop signal. */
	u_int16_t p_acflag;		/* U_SHORT: Accounting flags. */

	char	p_comm[KI_MAXCOMLEN];

	char	p_wmesg[KI_WMESGLEN];	/* wchan message */
	u_int64_t p_wchan;		/* PTR: sleep address. */

	char	p_login[KI_MAXLOGNAME];	/* setlogin() name */

	int32_t	p_vm_rssize;		/* SEGSZ_T: current resident set size in pages */
	int32_t	p_vm_tsize;		/* SEGSZ_T: text size (pages) */
	int32_t	p_vm_dsize;		/* SEGSZ_T: data size (pages) */
	int32_t	p_vm_ssize;		/* SEGSZ_T: stack size (pages) */

	int64_t	p_uvalid;		/* CHAR: following p_u* members from struct user are valid */
					/* XXX 64 bits for alignment */
	u_int32_t p_ustart_sec;		/* STRUCT TIMEVAL: starting time. */
	u_int32_t p_ustart_usec;	/* STRUCT TIMEVAL: starting time. */

	u_int32_t p_uutime_sec;		/* STRUCT TIMEVAL: user time. */
	u_int32_t p_uutime_usec;	/* STRUCT TIMEVAL: user time. */
	u_int32_t p_ustime_sec;		/* STRUCT TIMEVAL: system time. */
	u_int32_t p_ustime_usec;	/* STRUCT TIMEVAL: system time. */

	u_int64_t p_uru_maxrss;		/* LONG: max resident set size. */
	u_int64_t p_uru_ixrss;		/* LONG: integral shared memory size. */
	u_int64_t p_uru_idrss;		/* LONG: integral unshared data ". */
	u_int64_t p_uru_isrss;		/* LONG: integral unshared stack ". */
	u_int64_t p_uru_minflt;		/* LONG: page reclaims. */
	u_int64_t p_uru_majflt;		/* LONG: page faults. */
	u_int64_t p_uru_nswap;		/* LONG: swaps. */
	u_int64_t p_uru_inblock;	/* LONG: block input operations. */
	u_int64_t p_uru_oublock;	/* LONG: block output operations. */
	u_int64_t p_uru_msgsnd;		/* LONG: messages sent. */
	u_int64_t p_uru_msgrcv;		/* LONG: messages received. */
	u_int64_t p_uru_nsignals;	/* LONG: signals received. */
	u_int64_t p_uru_nvcsw;		/* LONG: voluntary context switches. */
	u_int64_t p_uru_nivcsw;		/* LONG: involuntary ". */

	u_int32_t p_uctime_sec;		/* STRUCT TIMEVAL: child u+s time. */
	u_int32_t p_uctime_usec;	/* STRUCT TIMEVAL: child u+s time. */
	u_int64_t p_cpuid;		/* LONG: CPU id */
	u_int64_t p_realflag;	       	/* INT: P_* flags (not including LWPs). */
	u_int64_t p_nlwps;		/* LONG: Number of LWPs */
	u_int64_t p_nrlwps;		/* LONG: Number of running LWPs */
	u_int64_t p_realstat;		/* LONG: non-LWP process status */
	u_int32_t p_svuid;		/* UID_T: saved user id */
	u_int32_t p_svgid;		/* GID_T: saved group id */
};

/*
 * KERN_LWP structure. See notes on KERN_PROC2 about adding elements.
 */
struct kinfo_lwp {
	u_int64_t l_forw;		/* PTR: linked run/sleep queue. */
	u_int64_t l_back;
	u_int64_t l_laddr;		/* PTR: Address of LWP */
	u_int64_t l_addr;		/* PTR: Kernel virtual addr of u-area */
	int32_t	l_lid;			/* LWPID_T: LWP identifier */
	int32_t	l_flag;			/* INT: L_* flags. */
	u_int32_t l_swtime;		/* U_INT: Time swapped in or out. */
	u_int32_t l_slptime;		/* U_INT: Time since last blocked. */
	int32_t	l_schedflags;		/* INT: PSCHED_* flags */
	int32_t	l_holdcnt;              /* INT: If non-zero, don't swap. */
	u_int8_t l_priority;		/* U_CHAR: Process priority. */
	u_int8_t l_usrpri;		/* U_CHAR: User-priority based on l_cpu and p_nice. */
	int8_t	l_stat;			/* CHAR: S* process status. */
	int8_t	l_pad1;			/* fill out to 4-byte boundary */
	int32_t	l_pad2;			/* .. and then to an 8-byte boundary */
	char	l_wmesg[KI_WMESGLEN];	/* wchan message */
	u_int64_t l_wchan;		/* PTR: sleep address. */
	u_int64_t l_cpuid;		/* LONG: CPU id */
};

/*
 * KERN_PROC_ARGS subtypes
 */
#define	KERN_PROC_ARGV		1	/* argv */
#define	KERN_PROC_NARGV		2	/* number of strings in above */
#define	KERN_PROC_ENV		3	/* environ */
#define	KERN_PROC_NENV		4	/* number of strings in above */

/*
 * KERN_SYSVIPC_INFO subtypes
 */
#define	KERN_SYSVIPC_MSG_INFO		1	/* msginfo and msqid_ds */
#define	KERN_SYSVIPC_SEM_INFO		2	/* seminfo and semid_ds */
#define	KERN_SYSVIPC_SHM_INFO		3	/* shminfo and shmid_ds */

/*
 * tty counter sysctl variables
 */
#define	KERN_TKSTAT_NIN			1	/* total input character */
#define	KERN_TKSTAT_NOUT		2	/* total output character */
#define	KERN_TKSTAT_CANCC		3	/* canonical input character */
#define	KERN_TKSTAT_RAWCC		4	/* raw input character */
#define	KERN_TKSTAT_MAXID		5	/* number of valid TKSTAT ids */

#define	KERN_TKSTAT_NAMES { \
	{ 0, 0 }, \
	{ "nin", CTLTYPE_QUAD }, \
	{ "nout", CTLTYPE_QUAD }, \
	{ "cancc", CTLTYPE_QUAD }, \
	{ "rawcc", CTLTYPE_QUAD }, \
}

/*
 * kern.drivers returns an array of these.
 */

struct kinfo_drivers {
	int32_t		d_cmajor;
	int32_t		d_bmajor;
	char		d_name[24];
};

/*
 * KERN_BUF subtypes, like KERN_PROC2, where the four following mib
 * entries specify "which type of buf", "which particular buf",
 * "sizeof buf", and "how many".  Currently, only "all buf" is
 * defined.
 */
#define	KERN_BUF_ALL	0		/* all buffers */

/*
 * kern.buf returns an array of these structures, which are designed
 * both to be immune to 32/64 bit emulation issues and to provide
 * backwards compatibility.  Note that the order here differs slightly
 * from the real struct buf in order to achieve proper 64 bit
 * alignment.
 */
struct buf_sysctl {
	uint32_t b_flags;	/* LONG: B_* flags */
	int32_t  b_error;	/* INT: Errno value */
	int32_t  b_prio;	/* INT: Hint for buffer queue discipline */
	uint32_t b_dev;		/* DEV_T: Device associated with buffer */
	uint64_t b_bufsize;	/* LONG: Allocated buffer size */
	uint64_t b_bcount;	/* LONG: Valid bytes in buffer */
	uint64_t b_resid;	/* LONG: Remaining I/O */
	uint64_t b_addr;	/* CADDR_T: Memory, superblocks, indirect... */
	uint64_t b_blkno;	/* DADDR_T: Underlying physical block number */
	uint64_t b_rawblkno;	/* DADDR_T: Raw underlying physical block */
	uint64_t b_iodone;	/* PTR: Function called upon completion */
	uint64_t b_proc;	/* PTR: Associated proc if B_PHYS set */
	uint64_t b_vp;		/* PTR: File vnode */
	uint64_t b_saveaddr;	/* PTR: Original b_addr for physio */
	uint64_t b_lblkno;	/* DADDR_T: Logical block number */
};

/*
 * CTL_HW identifiers
 */
#define	HW_MACHINE	 1		/* string: machine class */
#define	HW_MODEL	 2		/* string: specific machine model */
#define	HW_NCPU		 3		/* int: number of cpus */
#define	HW_BYTEORDER	 4		/* int: machine byte order */
#define	HW_PHYSMEM	 5		/* int: total memory (bytes) */
#define	HW_USERMEM	 6		/* int: non-kernel memory (bytes) */
#define	HW_PAGESIZE	 7		/* int: software page size */
#define	HW_DISKNAMES	 8		/* string: disk drive names */
#define	HW_DISKSTATS	 9		/* struct: diskstats[] */
#define	HW_MACHINE_ARCH	10		/* string: machine architecture */
#define	HW_ALIGNBYTES	11		/* int: ALIGNBYTES for the kernel */
#define	HW_CNMAGIC	12		/* string: console magic sequence(s) */
#define	HW_PHYSMEM64	13		/* quad: total memory (bytes) */
#define	HW_USERMEM64	14		/* quad: non-kernel memory (bytes) */
#define	HW_MAXID	15		/* number of valid hw ids */

#define	CTL_HW_NAMES { \
	{ 0, 0 }, \
	{ "machine", CTLTYPE_STRING }, \
	{ "model", CTLTYPE_STRING }, \
	{ "ncpu", CTLTYPE_INT }, \
	{ "byteorder", CTLTYPE_INT }, \
	{ "physmem", CTLTYPE_INT }, \
	{ "usermem", CTLTYPE_INT }, \
	{ "pagesize", CTLTYPE_INT }, \
	{ "disknames", CTLTYPE_STRING }, \
	{ "diskstats", CTLTYPE_STRUCT }, \
	{ "machine_arch", CTLTYPE_STRING }, \
	{ "alignbytes", CTLTYPE_INT }, \
	{ "cnmagic", CTLTYPE_STRING }, \
	{ "physmem64", CTLTYPE_QUAD }, \
	{ "usermem64", CTLTYPE_QUAD }, \
}

/*
 * CTL_USER definitions
 */
#define	USER_CS_PATH		 1	/* string: _CS_PATH */
#define	USER_BC_BASE_MAX	 2	/* int: BC_BASE_MAX */
#define	USER_BC_DIM_MAX		 3	/* int: BC_DIM_MAX */
#define	USER_BC_SCALE_MAX	 4	/* int: BC_SCALE_MAX */
#define	USER_BC_STRING_MAX	 5	/* int: BC_STRING_MAX */
#define	USER_COLL_WEIGHTS_MAX	 6	/* int: COLL_WEIGHTS_MAX */
#define	USER_EXPR_NEST_MAX	 7	/* int: EXPR_NEST_MAX */
#define	USER_LINE_MAX		 8	/* int: LINE_MAX */
#define	USER_RE_DUP_MAX		 9	/* int: RE_DUP_MAX */
#define	USER_POSIX2_VERSION	10	/* int: POSIX2_VERSION */
#define	USER_POSIX2_C_BIND	11	/* int: POSIX2_C_BIND */
#define	USER_POSIX2_C_DEV	12	/* int: POSIX2_C_DEV */
#define	USER_POSIX2_CHAR_TERM	13	/* int: POSIX2_CHAR_TERM */
#define	USER_POSIX2_FORT_DEV	14	/* int: POSIX2_FORT_DEV */
#define	USER_POSIX2_FORT_RUN	15	/* int: POSIX2_FORT_RUN */
#define	USER_POSIX2_LOCALEDEF	16	/* int: POSIX2_LOCALEDEF */
#define	USER_POSIX2_SW_DEV	17	/* int: POSIX2_SW_DEV */
#define	USER_POSIX2_UPE		18	/* int: POSIX2_UPE */
#define	USER_STREAM_MAX		19	/* int: POSIX2_STREAM_MAX */
#define	USER_TZNAME_MAX		20	/* int: POSIX2_TZNAME_MAX */
#define	USER_ATEXIT_MAX		21	/* int: {ATEXIT_MAX} */
#define	USER_MAXID		22	/* number of valid user ids */

#define	CTL_USER_NAMES { \
	{ 0, 0 }, \
	{ "cs_path", CTLTYPE_STRING }, \
	{ "bc_base_max", CTLTYPE_INT }, \
	{ "bc_dim_max", CTLTYPE_INT }, \
	{ "bc_scale_max", CTLTYPE_INT }, \
	{ "bc_string_max", CTLTYPE_INT }, \
	{ "coll_weights_max", CTLTYPE_INT }, \
	{ "expr_nest_max", CTLTYPE_INT }, \
	{ "line_max", CTLTYPE_INT }, \
	{ "re_dup_max", CTLTYPE_INT }, \
	{ "posix2_version", CTLTYPE_INT }, \
	{ "posix2_c_bind", CTLTYPE_INT }, \
	{ "posix2_c_dev", CTLTYPE_INT }, \
	{ "posix2_char_term", CTLTYPE_INT }, \
	{ "posix2_fort_dev", CTLTYPE_INT }, \
	{ "posix2_fort_run", CTLTYPE_INT }, \
	{ "posix2_localedef", CTLTYPE_INT }, \
	{ "posix2_sw_dev", CTLTYPE_INT }, \
	{ "posix2_upe", CTLTYPE_INT }, \
	{ "stream_max", CTLTYPE_INT }, \
	{ "tzname_max", CTLTYPE_INT }, \
	{ "atexit_max", CTLTYPE_INT }, \
}

/*
 * CTL_DDB definitions
 */
#define	DDBCTL_RADIX		1	/* int: Input and output radix */
#define	DDBCTL_MAXOFF		2	/* int: max symbol offset */
#define	DDBCTL_MAXWIDTH		3	/* int: width of the display line */
#define	DDBCTL_LINES		4	/* int: number of display lines */
#define	DDBCTL_TABSTOPS		5	/* int: tab width */
#define	DDBCTL_ONPANIC		6	/* int: DDB on panic if non-zero */
#define	DDBCTL_FROMCONSOLE	7	/* int: DDB via console if non-zero */
#define	DDBCTL_MAXID		8	/* number of valid DDB ids */

#define	CTL_DDB_NAMES { \
	{ 0, 0 }, \
	{ "radix", CTLTYPE_INT }, \
	{ "maxoff", CTLTYPE_INT }, \
	{ "maxwidth", CTLTYPE_INT }, \
	{ "lines", CTLTYPE_INT }, \
	{ "tabstops", CTLTYPE_INT }, \
	{ "onpanic", CTLTYPE_INT }, \
	{ "fromconsole", CTLTYPE_INT }, \
}

/*
 * CTL_DEBUG definitions
 *
 * Second level identifier specifies which debug variable.
 * Third level identifier specifies which stucture component.
 */
#define	CTL_DEBUG_NAME		0	/* string: variable name */
#define	CTL_DEBUG_VALUE		1	/* int: variable value */
#define	CTL_DEBUG_MAXID		20

/*
 * CTL_PROC subtype. Either a PID, or a magic value for the current proc.
 */

#define	PROC_CURPROC	(~((u_int)1 << 31))

/*
 * CTL_PROC tree: either corename (string), or a limit
 * (rlimit.<type>.{hard,soft}, int).
 */
#define	PROC_PID_CORENAME	1
#define	PROC_PID_LIMIT		2
#define	PROC_PID_STOPFORK	3
#define	PROC_PID_STOPEXEC	4
#define	PROC_PID_STOPEXIT	5
#define	PROC_PID_MAXID		6

#define	PROC_PID_NAMES { \
	{ 0, 0 }, \
	{ "corename", CTLTYPE_STRING }, \
	{ "rlimit", CTLTYPE_NODE }, \
	{ "stopfork", CTLTYPE_INT }, \
	{ "stopexec", CTLTYPE_INT }, \
	{ "stopexit", CTLTYPE_INT }, \
}

/* Limit types from <sys/resources.h> */
#define	PROC_PID_LIMIT_CPU	(RLIMIT_CPU+1)
#define	PROC_PID_LIMIT_FSIZE	(RLIMIT_FSIZE+1)
#define	PROC_PID_LIMIT_DATA	(RLIMIT_DATA+1)
#define	PROC_PID_LIMIT_STACK	(RLIMIT_STACK+1)
#define	PROC_PID_LIMIT_CORE	(RLIMIT_CORE+1)
#define	PROC_PID_LIMIT_RSS	(RLIMIT_RSS+1)
#define	PROC_PID_LIMIT_MEMLOCK	(RLIMIT_MEMLOCK+1)
#define PROC_PID_LIMIT_NPROC	(RLIMIT_NPROC+1)
#define	PROC_PID_LIMIT_NOFILE	(RLIMIT_NOFILE+1)
#define	PROC_PID_LIMIT_MAXID 	10

#define	PROC_PID_LIMIT_NAMES { \
	{ 0, 0 }, \
	{ "cputime", CTLTYPE_NODE }, \
	{ "filesize", CTLTYPE_NODE }, \
	{ "datasize", CTLTYPE_NODE }, \
	{ "stacksize", CTLTYPE_NODE }, \
	{ "coredumpsize", CTLTYPE_NODE }, \
	{ "memoryuse", CTLTYPE_NODE }, \
	{ "memorylocked", CTLTYPE_NODE }, \
	{ "maxproc", CTLTYPE_NODE }, \
	{ "descriptors", CTLTYPE_NODE }, \
}
/* for each type, either hard or soft value */
#define	PROC_PID_LIMIT_TYPE_SOFT	1
#define	PROC_PID_LIMIT_TYPE_HARD	2
#define	PROC_PID_LIMIT_TYPE_MAXID	3

#define	PROC_PID_LIMIT_TYPE_NAMES { \
	{0, 0}, \
	{ "soft", CTLTYPE_QUAD }, \
	{ "hard", CTLTYPE_QUAD }, \
}

/*
 * CTL_EMUL definitions
 *
 * Second level identifier specifies which emulation variable.
 * Subsequent levels are specified in the emulations themselves.
 */
#define	EMUL_LINUX	1
#define	EMUL_IRIX	2
#define	EMUL_DARWIN	3

#define	EMUL_MAXID	4
#define	CTL_EMUL_NAMES { \
	{ 0, 0 }, \
	{ "linux", CTLTYPE_NODE }, \
	{ "irix", CTLTYPE_NODE }, \
	{ "darwin", CTLTYPE_NODE }, \
}

#ifdef	_KERNEL
/*
 * A log of nodes created by a setup function or set of setup
 * functions so that they can be torn down in one "transaction"
 * when no longer needed.
 *
 * Users of the log merely pass a pointer to a pointer, and the sysctl
 * infrastructure takes care of the rest.
 */
struct sysctllog;

/*
 * CTL_DEBUG variables.
 *
 * These are declared as separate variables so that they can be
 * individually initialized at the location of their associated
 * variable. The loader prevents multiple use by issuing errors
 * if a variable is initialized in more than one place. They are
 * aggregated into an array in debug_sysctl(), so that it can
 * conveniently locate them when querried. If more debugging
 * variables are added, they must also be declared here and also
 * entered into the array.
 *
 * Note that the debug subtree is largely obsolescent in terms of
 * functionality now that we have dynamic sysctl, but the
 * infrastructure is retained for backwards compatibility.
 */
struct ctldebug {
	char	*debugname;	/* name of debugging variable */
	int	*debugvar;	/* pointer to debugging variable */
};
#ifdef	DEBUG
extern struct ctldebug debug0, debug1, debug2, debug3, debug4;
extern struct ctldebug debug5, debug6, debug7, debug8, debug9;
extern struct ctldebug debug10, debug11, debug12, debug13, debug14;
extern struct ctldebug debug15, debug16, debug17, debug18, debug19;
#endif	/* DEBUG */

#define SYSCTLFN_PROTO const int *, u_int, void *, \
	size_t *, const void *, size_t, \
	const int *, struct lwp *, const struct sysctlnode *
#define SYSCTLFN_ARGS const int *name, u_int namelen, void *oldp, \
	size_t *oldlenp, const void *newp, size_t newlen, \
	const int *oname, struct lwp *l, const struct sysctlnode *rnode
#define SYSCTLFN_RWPROTO const int *, u_int, void *, \
	size_t *, const void *, size_t, \
	const int *, struct lwp *, struct sysctlnode *
#define SYSCTLFN_RWARGS const int *name, u_int namelen, void *oldp, \
	size_t *oldlenp, const void *newp, size_t newlen, \
	const int *oname, struct lwp *l, struct sysctlnode *rnode
#define SYSCTLFN_CALL(node) name, namelen, oldp, \
	oldlenp, newp, newlen, \
	oname, l, (struct sysctlnode *)node

#ifdef SYSCTL_DEBUG_SETUP
#define SYSCTL_SETUP(name, desc)				\
	static void __CONCAT(___,name)(struct sysctllog **);	\
	static void name(struct sysctllog **clog) {		\
		printf("%s\n", desc);				\
		__CONCAT(___,name)(clog); }			\
	__link_set_add_text(sysctl_funcs, name);		\
	static void __CONCAT(___,name)(struct sysctllog **clog)
#else /* SYSCTL_DEBUG_SETUP */
#define SYSCTL_SETUP(name, desc)				\
	static void name(struct sysctllog **);			\
	__link_set_add_text(sysctl_funcs, name);		\
	static void name(struct sysctllog **clog)
#endif /* SYSCTL_DEBUG_SETUP */
typedef void (*sysctl_setup_func)(struct sysctllog **);

/*
 * Internal sysctl function calling convention:
 *
 *	(*sysctlfn)(name, namelen, oldval, oldlenp, newval, newlen,
 *		    origname, lwp, node);
 *
 * The name parameter points at the next component of the name to be
 * interpreted.  The namelen parameter is the number of integers in
 * the name.  The origname parameter points to the start of the name
 * being parsed.  The node parameter points to the node on which the
 * current operation is to be performed.
 */
typedef int (*sysctlfn)(SYSCTLFN_PROTO);

/*
 * used in more than just sysctl
 */
void	fill_eproc(struct proc *, struct eproc *);

/*
 * subsystem setup
 */
void	sysctl_init(void);

/*
 * typical syscall call order
 */
int	sysctl_lock(struct lwp *, void *, size_t);
int	sysctl_dispatch(SYSCTLFN_RWPROTO);
void	sysctl_unlock(struct lwp *);

/*
 * tree navigation primitives (must obtain lock before using these)
 */
int	sysctl_locate(struct lwp *, const int *, u_int, struct sysctlnode **,
		      int *);
int	sysctl_query(SYSCTLFN_PROTO);
#ifdef SYSCTL_DEBUG_CREATE
#define sysctl_create _sysctl_create
#endif /* SYSCTL_DEBUG_CREATE */
int	sysctl_create(SYSCTLFN_RWPROTO);
int	sysctl_destroy(SYSCTLFN_RWPROTO);
int	sysctl_lookup(SYSCTLFN_RWPROTO);

/*
 * simple variadic interface for adding/removing nodes
 */
int	sysctl_createv(struct sysctllog **, int,
		       struct sysctlnode **, struct sysctlnode **,
		       int, int, const char *, const char *,
		       sysctlfn, u_quad_t, void *, size_t, ...);
int	sysctl_destroyv(struct sysctlnode *, ...);

/*
 * miscellany
 */
void	sysctl_dump(const struct sysctlnode *);
void	sysctl_free(struct sysctlnode *);
void	sysctl_teardown(struct sysctllog **);

/*
 * simple interface similar to old interface for in-kernel consumption
 */
int	old_sysctl(int *, u_int, void *, size_t *, void *, size_t, struct lwp *);

/*
 * these helpers are in other files (XXX so should the nodes be) or
 * are used by more than one node
 */
int	sysctl_hw_disknames(SYSCTLFN_PROTO);
int	sysctl_hw_diskstats(SYSCTLFN_PROTO);
int	sysctl_kern_vnode(SYSCTLFN_PROTO);
int	sysctl_net_inet_ip_ports(SYSCTLFN_PROTO);
int	sysctl_consdev(SYSCTLFN_PROTO);
int	sysctl_root_device(SYSCTLFN_PROTO);

/*
 * primitive helper stubs
 */
int	sysctl_needfunc(SYSCTLFN_PROTO);
int	sysctl_notavail(SYSCTLFN_PROTO);
int	sysctl_null(SYSCTLFN_PROTO);

MALLOC_DECLARE(M_SYSCTLNODE);
MALLOC_DECLARE(M_SYSCTLDATA);

#else	/* !_KERNEL */
#include <sys/cdefs.h>

typedef void *sysctlfn;

__BEGIN_DECLS
int	sysctl __P((int *, u_int, void *, size_t *, const void *, size_t));
__END_DECLS

#endif	/* !_KERNEL */

#ifdef __COMPAT_SYSCTL
/*
 * node version 0
 */
struct sysctlnode0 {
	uint sysctl0_flags;		/* flags and type */
	int sysctl0_num;		/* mib number */ 
	size_t sysctl0_size;		/* size of instrumented data */
	char sysctl0_name[SYSCTL_NAMELEN]; /* node name */
	union {
		struct {
			uint scn0_csize; /* size of child node array */
			uint scn0_clen;	/* number of valid children */
			struct sysctlnode0 *scn0_child; /* array of child nodes */
		} scu0_node;
		int scu0_alias;		/* node this node refers to */
		int scu0_idata;		/* immediate "int" data */
		u_quad_t scu0_qdata;	/* immediate "u_quad_t" data */
		void *scu0_data;	/* pointer to external data */
	} sysctl0_un;
	sysctlfn sysctl0_func;		/* access helper function */
	struct sysctlnode0 *sysctl0_parent; /* parent of this node */
	uint sysctl0_ver;		/* node's version vs. rest of tree */
};

#define sysctl0_csize	sysctl0_un.scu0_node.scn0_csize
#define sysctl0_clen	sysctl0_un.scu0_node.scn0_clen
#define sysctl0_child	sysctl0_un.scu0_node.scn0_child
#define sysctl0_alias	sysctl0_un.scu0_alias
#define sysctl0_data	sysctl0_un.scu0_data
#define sysctl0_idata	sysctl0_un.scu0_idata
#define sysctl0_qdata	sysctl0_un.scu0_qdata

#endif /* __COMPAT_SYSCTL */

/*
 * padding makes alignment magically "work" for 32/64 compatibility at
 * the expense of making things bigger on 32 bit platforms.
 */
#if defined(LP64) || (BYTE_ORDER == LITTLE_ENDIAN)
#define __sysc_pad(type) union { uint64_t __sysc_upad; \
	struct { type __sysc_sdatum; } __sysc_ustr; }
#else
#define __sysc_pad(type) union { uint64_t __sysc_upad; \
	struct { uint32_t __sysc_spad; type __sysc_sdatum; } __sysc_ustr; }
#endif
#define __sysc_unpad(x) x.__sysc_ustr.__sysc_sdatum

struct sysctlnode {
	uint32_t sysctl_flags;		/* flags and type */
	int32_t sysctl_num;		/* mib number */
	char sysctl_name[SYSCTL_NAMELEN]; /* node name */
	uint32_t sysctl_ver;		/* node's version vs. rest of tree */
	uint32_t __rsvd;
	union {
		struct {
			uint32_t suc_csize;	/* size of child node array */
			uint32_t suc_clen;	/* number of valid children */
			__sysc_pad(struct sysctlnode*) _suc_child; /* array of child nodes */
		} scu_child;
		struct {
			__sysc_pad(void*) _sud_data; /* pointer to external data */
			__sysc_pad(size_t) _sud_offset; /* offset to data */
		} scu_data;
		int32_t scu_alias;		/* node this node refers to */
		int32_t scu_idata;		/* immediate "int" data */
		u_quad_t scu_qdata;		/* immediate "u_quad_t" data */
	} sysctl_un;
	__sysc_pad(size_t) _sysctl_size;	/* size of instrumented data */
	__sysc_pad(sysctlfn) _sysctl_func;	/* access helper function */
	__sysc_pad(struct sysctlnode*) _sysctl_parent; /* parent of this node */
	__sysc_pad(const char *) _sysctl_desc;	/* description of node */
};

/*
 * padded data
 */
#define suc_child	__sysc_unpad(_suc_child)
#define sud_data	__sysc_unpad(_sud_data)
#define sud_offset	__sysc_unpad(_sud_offset)
#define sysctl_size	__sysc_unpad(_sysctl_size)
#define sysctl_func	__sysc_unpad(_sysctl_func)
#define sysctl_parent	__sysc_unpad(_sysctl_parent)
#define sysctl_desc	__sysc_unpad(_sysctl_desc)

/*
 * nested data (may also be padded)
 */
#define sysctl_csize	sysctl_un.scu_child.suc_csize
#define sysctl_clen	sysctl_un.scu_child.suc_clen
#define sysctl_child	sysctl_un.scu_child.suc_child
#define sysctl_data	sysctl_un.scu_data.sud_data
#define sysctl_offset	sysctl_un.scu_data.sud_offset
#define sysctl_alias	sysctl_un.scu_alias
#define sysctl_idata	sysctl_un.scu_idata
#define sysctl_qdata	sysctl_un.scu_qdata

static __inline struct sysctlnode *
sysctl_rootof(struct sysctlnode *n)
{
	while (n->sysctl_parent != NULL)
		n = n->sysctl_parent;
	return (n);
}

#endif	/* !_SYS_SYSCTL_H_ */
