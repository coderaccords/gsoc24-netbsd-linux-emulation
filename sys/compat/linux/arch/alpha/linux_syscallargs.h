/*	$NetBSD: linux_syscallargs.h,v 1.8 1999/03/27 01:15:58 tron Exp $	*/

/*
 * System call argument lists.
 *
 * DO NOT EDIT-- this file is automatically generated.
 * created from	NetBSD: syscalls.master,v 1.8 1999/03/27 01:10:56 tron Exp 
 */

#ifndef _LINUX_SYS__SYSCALLARGS_H_
#define _LINUX_SYS__SYSCALLARGS_H_

#ifdef	syscallarg
#undef	syscallarg
#endif

#define	syscallarg(x)								\
		union {								\
			register_t pad;						\
			struct { x datum; } le;					\
			struct {						\
				int8_t pad[ (sizeof (register_t) < sizeof (x))	\
					? 0					\
					: sizeof (register_t) - sizeof (x)];	\
				x datum;					\
			} be;							\
		}

struct linux_sys_creat_args {
	syscallarg(char *) path;
	syscallarg(int) mode;
};

struct linux_sys_unlink_args {
	syscallarg(char *) path;
};

struct linux_sys_chdir_args {
	syscallarg(char *) path;
};

struct linux_sys_mknod_args {
	syscallarg(char *) path;
	syscallarg(int) mode;
	syscallarg(int) dev;
};

struct linux_sys_chmod_args {
	syscallarg(char *) path;
	syscallarg(int) mode;
};

struct linux_sys_chown_args {
	syscallarg(char *) path;
	syscallarg(int) uid;
	syscallarg(int) gid;
};

struct linux_sys_brk_args {
	syscallarg(char *) nsize;
};

struct linux_sys_access_args {
	syscallarg(char *) path;
	syscallarg(int) flags;
};

struct linux_sys_kill_args {
	syscallarg(int) pid;
	syscallarg(int) signum;
};

struct linux_sys_open_args {
	syscallarg(char *) path;
	syscallarg(int) flags;
	syscallarg(int) mode;
};

struct linux_sys_sigpending_args {
	syscallarg(linux_old_sigset_t *) set;
};

struct linux_sys_ioctl_args {
	syscallarg(int) fd;
	syscallarg(u_long) com;
	syscallarg(caddr_t) data;
};

struct linux_sys_symlink_args {
	syscallarg(char *) path;
	syscallarg(char *) to;
};

struct linux_sys_readlink_args {
	syscallarg(char *) name;
	syscallarg(char *) buf;
	syscallarg(int) count;
};

struct linux_sys_execve_args {
	syscallarg(char *) path;
	syscallarg(char **) argp;
	syscallarg(char **) envp;
};

struct linux_sys_stat_args {
	syscallarg(char *) path;
	syscallarg(struct linux_stat *) sp;
};

struct linux_sys_lstat_args {
	syscallarg(char *) path;
	syscallarg(struct linux_stat *) sp;
};

struct linux_sys_fstat_args {
	syscallarg(int) fd;
	syscallarg(struct linux_stat *) sp;
};

struct linux_sys_fcntl_args {
	syscallarg(int) fd;
	syscallarg(int) cmd;
	syscallarg(void *) arg;
};

struct linux_sys_socket_args {
	syscallarg(int) domain;
	syscallarg(int) type;
	syscallarg(int) protocol;
};

struct linux_sys_sigreturn_args {
	syscallarg(struct linux_sigframe *) sfp;
};

struct linux_sys_setsockopt_args {
	syscallarg(int) s;
	syscallarg(int) level;
	syscallarg(int) optname;
	syscallarg(void *) optval;
	syscallarg(int) optlen;
};

struct linux_sys_sigsuspend_args {
	syscallarg(caddr_t) restart;
	syscallarg(int) oldmask;
	syscallarg(int) mask;
};

struct linux_sys_getsockopt_args {
	syscallarg(int) s;
	syscallarg(int) level;
	syscallarg(int) optname;
	syscallarg(void *) optval;
	syscallarg(int *) optlen;
};

struct linux_sys_fchown_args {
	syscallarg(int) fd;
	syscallarg(int) uid;
	syscallarg(int) gid;
};

struct linux_sys_recvfrom_args {
	syscallarg(int) s;
	syscallarg(void *) buf;
	syscallarg(int) len;
	syscallarg(int) flags;
	syscallarg(struct sockaddr *) from;
	syscallarg(int *) fromlen;
};

struct linux_sys_setreuid_args {
	syscallarg(int) ruid;
	syscallarg(int) euid;
};

struct linux_sys_setregid_args {
	syscallarg(int) rgid;
	syscallarg(int) egid;
};

struct linux_sys_rename_args {
	syscallarg(char *) from;
	syscallarg(char *) to;
};

struct linux_sys_truncate_args {
	syscallarg(char *) path;
	syscallarg(long) length;
};

struct linux_sys_sendto_args {
	syscallarg(int) s;
	syscallarg(void *) msg;
	syscallarg(int) len;
	syscallarg(int) flags;
	syscallarg(struct sockaddr *) to;
	syscallarg(int) tolen;
};

struct linux_sys_socketpair_args {
	syscallarg(int) domain;
	syscallarg(int) type;
	syscallarg(int) protocol;
	syscallarg(int *) rsv;
};

struct linux_sys_mkdir_args {
	syscallarg(char *) path;
	syscallarg(int) mode;
};

struct linux_sys_rmdir_args {
	syscallarg(char *) path;
};

struct linux_sys_sigaction_args {
	syscallarg(int) signum;
	syscallarg(const struct linux_old_sigaction *) nsa;
	syscallarg(struct linux_old_sigaction *) osa;
};

struct linux_sys_olduname_args {
	syscallarg(struct linux_old_utsname *) up;
};

struct linux_sys_lchown_args {
	syscallarg(char *) path;
	syscallarg(int) uid;
	syscallarg(int) gid;
};

struct linux_sys_msync_args {
	syscallarg(caddr_t) addr;
	syscallarg(int) len;
	syscallarg(int) fl;
};

struct linux_sys_getpgid_args {
	syscallarg(int) pid;
};

struct linux_sys_fdatasync_args {
	syscallarg(int) fd;
};

struct linux_sys_getdents_args {
	syscallarg(int) fd;
	syscallarg(struct linux_dirent *) dent;
	syscallarg(unsigned int) count;
};

struct linux_sys_uselib_args {
	syscallarg(char *) path;
};

struct linux_sys___sysctl_args {
	syscallarg(struct linux___sysctl *) lsp;
};

struct linux_sys_times_args {
	syscallarg(struct times *) tms;
};

struct linux_sys_personality_args {
	syscallarg(int) per;
};

struct linux_sys_statfs_args {
	syscallarg(char *) path;
	syscallarg(struct linux_statfs *) sp;
};

struct linux_sys_fstatfs_args {
	syscallarg(int) fd;
	syscallarg(struct linux_statfs *) sp;
};

struct linux_sys_uname_args {
	syscallarg(struct linux_utsname *) up;
};

struct linux_sys_mremap_args {
	syscallarg(void *) old_address;
	syscallarg(size_t) old_size;
	syscallarg(size_t) new_size;
	syscallarg(u_long) flags;
};

struct linux_sys_rt_sigreturn_args {
	syscallarg(struct linux_rt_sigframe *) sfp;
};

struct linux_sys_rt_sigaction_args {
	syscallarg(int) signum;
	syscallarg(const struct linux_sigaction *) nsa;
	syscallarg(struct linux_sigaction *) osa;
	syscallarg(size_t) sigsetsize;
};

struct linux_sys_rt_sigprocmask_args {
	syscallarg(int) how;
	syscallarg(const linux_sigset_t *) set;
	syscallarg(linux_sigset_t *) oset;
	syscallarg(size_t) sigsetsize;
};

struct linux_sys_rt_sigpending_args {
	syscallarg(linux_sigset_t *) set;
	syscallarg(size_t) sigsetsize;
};

struct linux_sys_rt_queueinfo_args {
	syscallarg(int) pid;
	syscallarg(int) signum;
	syscallarg(linux_siginfo_t *) uinfo;
};

struct linux_sys_rt_sigsuspend_args {
	syscallarg(linux_sigset_t *) unewset;
	syscallarg(size_t) sigsetsize;
};

struct linux_sys_select_args {
	syscallarg(int) nfds;
	syscallarg(fd_set *) readfds;
	syscallarg(fd_set *) writefds;
	syscallarg(fd_set *) exceptfds;
	syscallarg(struct timeval *) timeout;
};

struct linux_sys_wait4_args {
	syscallarg(int) pid;
	syscallarg(int *) status;
	syscallarg(int) options;
	syscallarg(struct rusage *) rusage;
};

/*
 * System call prototypes.
 */

int	sys_nosys	__P((struct proc *, void *, register_t *));
int	sys_exit	__P((struct proc *, void *, register_t *));
int	sys_fork	__P((struct proc *, void *, register_t *));
int	sys_read	__P((struct proc *, void *, register_t *));
int	sys_write	__P((struct proc *, void *, register_t *));
int	sys_close	__P((struct proc *, void *, register_t *));
int	linux_sys_creat	__P((struct proc *, void *, register_t *));
int	sys_link	__P((struct proc *, void *, register_t *));
int	linux_sys_unlink	__P((struct proc *, void *, register_t *));
int	linux_sys_chdir	__P((struct proc *, void *, register_t *));
int	sys_fchdir	__P((struct proc *, void *, register_t *));
int	linux_sys_mknod	__P((struct proc *, void *, register_t *));
int	linux_sys_chmod	__P((struct proc *, void *, register_t *));
int	linux_sys_chown	__P((struct proc *, void *, register_t *));
int	linux_sys_brk	__P((struct proc *, void *, register_t *));
int	compat_43_sys_lseek	__P((struct proc *, void *, register_t *));
int	sys_getpid	__P((struct proc *, void *, register_t *));
int	sys_setuid	__P((struct proc *, void *, register_t *));
int	sys_getuid	__P((struct proc *, void *, register_t *));
int	linux_sys_access	__P((struct proc *, void *, register_t *));
int	sys_sync	__P((struct proc *, void *, register_t *));
int	linux_sys_kill	__P((struct proc *, void *, register_t *));
int	sys_setpgid	__P((struct proc *, void *, register_t *));
int	sys_dup	__P((struct proc *, void *, register_t *));
int	linux_sys_pipe	__P((struct proc *, void *, register_t *));
int	linux_sys_open	__P((struct proc *, void *, register_t *));
int	sys_getgid	__P((struct proc *, void *, register_t *));
int	sys_acct	__P((struct proc *, void *, register_t *));
int	linux_sys_sigpending	__P((struct proc *, void *, register_t *));
int	linux_sys_ioctl	__P((struct proc *, void *, register_t *));
int	linux_sys_symlink	__P((struct proc *, void *, register_t *));
int	linux_sys_readlink	__P((struct proc *, void *, register_t *));
int	linux_sys_execve	__P((struct proc *, void *, register_t *));
int	sys_umask	__P((struct proc *, void *, register_t *));
int	sys_chroot	__P((struct proc *, void *, register_t *));
int	sys_getpgrp	__P((struct proc *, void *, register_t *));
int	compat_43_sys_getpagesize	__P((struct proc *, void *, register_t *));
int	linux_sys_stat	__P((struct proc *, void *, register_t *));
int	linux_sys_lstat	__P((struct proc *, void *, register_t *));
int	linux_sys_mmap	__P((struct proc *, void *, register_t *));
int	sys_munmap	__P((struct proc *, void *, register_t *));
int	sys_mprotect	__P((struct proc *, void *, register_t *));
int	sys_getgroups	__P((struct proc *, void *, register_t *));
int	sys_setgroups	__P((struct proc *, void *, register_t *));
int	compat_43_sys_gethostname	__P((struct proc *, void *, register_t *));
int	compat_43_sys_sethostname	__P((struct proc *, void *, register_t *));
int	sys_dup2	__P((struct proc *, void *, register_t *));
int	linux_sys_fstat	__P((struct proc *, void *, register_t *));
int	linux_sys_fcntl	__P((struct proc *, void *, register_t *));
int	sys_fsync	__P((struct proc *, void *, register_t *));
int	sys_setpriority	__P((struct proc *, void *, register_t *));
int	linux_sys_socket	__P((struct proc *, void *, register_t *));
int	sys_connect	__P((struct proc *, void *, register_t *));
int	compat_43_sys_accept	__P((struct proc *, void *, register_t *));
int	sys_getpriority	__P((struct proc *, void *, register_t *));
int	compat_43_sys_send	__P((struct proc *, void *, register_t *));
int	compat_43_sys_recv	__P((struct proc *, void *, register_t *));
int	linux_sys_sigreturn	__P((struct proc *, void *, register_t *));
int	sys_bind	__P((struct proc *, void *, register_t *));
int	linux_sys_setsockopt	__P((struct proc *, void *, register_t *));
int	sys_listen	__P((struct proc *, void *, register_t *));
int	linux_sys_sigsuspend	__P((struct proc *, void *, register_t *));
int	sys_recvmsg	__P((struct proc *, void *, register_t *));
int	sys_sendmsg	__P((struct proc *, void *, register_t *));
int	linux_sys_getsockopt	__P((struct proc *, void *, register_t *));
int	sys_readv	__P((struct proc *, void *, register_t *));
int	sys_writev	__P((struct proc *, void *, register_t *));
int	linux_sys_fchown	__P((struct proc *, void *, register_t *));
int	sys_fchmod	__P((struct proc *, void *, register_t *));
int	linux_sys_recvfrom	__P((struct proc *, void *, register_t *));
int	linux_sys_setreuid	__P((struct proc *, void *, register_t *));
int	linux_sys_setregid	__P((struct proc *, void *, register_t *));
int	linux_sys_rename	__P((struct proc *, void *, register_t *));
int	linux_sys_truncate	__P((struct proc *, void *, register_t *));
int	compat_43_sys_ftruncate	__P((struct proc *, void *, register_t *));
int	sys_flock	__P((struct proc *, void *, register_t *));
int	sys_setgid	__P((struct proc *, void *, register_t *));
int	linux_sys_sendto	__P((struct proc *, void *, register_t *));
int	sys_shutdown	__P((struct proc *, void *, register_t *));
int	linux_sys_socketpair	__P((struct proc *, void *, register_t *));
int	linux_sys_mkdir	__P((struct proc *, void *, register_t *));
int	linux_sys_rmdir	__P((struct proc *, void *, register_t *));
int	compat_43_sys_getpeername	__P((struct proc *, void *, register_t *));
int	compat_43_sys_getrlimit	__P((struct proc *, void *, register_t *));
int	compat_43_sys_setrlimit	__P((struct proc *, void *, register_t *));
int	sys_setsid	__P((struct proc *, void *, register_t *));
int	compat_43_sys_getsockname	__P((struct proc *, void *, register_t *));
int	linux_sys_sigaction	__P((struct proc *, void *, register_t *));
#ifdef SYSVMSG
int	linux_sys_msgctl	__P((struct proc *, void *, register_t *));
int	sys_msgget	__P((struct proc *, void *, register_t *));
int	sys_msgrcv	__P((struct proc *, void *, register_t *));
int	sys_msgsnd	__P((struct proc *, void *, register_t *));
#else
#endif
#ifdef SYSVSEM
int	linux_sys_semctl	__P((struct proc *, void *, register_t *));
int	sys_semget	__P((struct proc *, void *, register_t *));
int	sys_semop	__P((struct proc *, void *, register_t *));
#else
#endif
int	linux_sys_olduname	__P((struct proc *, void *, register_t *));
int	linux_sys_lchown	__P((struct proc *, void *, register_t *));
#ifdef SYSVSHM
int	linux_sys_shmat	__P((struct proc *, void *, register_t *));
int	linux_sys_shmctl	__P((struct proc *, void *, register_t *));
int	sys_shmdt	__P((struct proc *, void *, register_t *));
int	sys_shmget	__P((struct proc *, void *, register_t *));
#else
#endif
int	linux_sys_msync	__P((struct proc *, void *, register_t *));
int	linux_sys_getpgid	__P((struct proc *, void *, register_t *));
int	sys_getsid	__P((struct proc *, void *, register_t *));
int	linux_sys_fdatasync	__P((struct proc *, void *, register_t *));
int	linux_sys_getdents	__P((struct proc *, void *, register_t *));
int	sys_reboot	__P((struct proc *, void *, register_t *));
#ifdef EXEC_AOUT
int	linux_sys_uselib	__P((struct proc *, void *, register_t *));
#else
#endif
int	sys_mlock	__P((struct proc *, void *, register_t *));
int	sys_munlock	__P((struct proc *, void *, register_t *));
int	linux_sys___sysctl	__P((struct proc *, void *, register_t *));
int	compat_12_sys_swapon	__P((struct proc *, void *, register_t *));
int	linux_sys_times	__P((struct proc *, void *, register_t *));
int	linux_sys_personality	__P((struct proc *, void *, register_t *));
int	linux_sys_statfs	__P((struct proc *, void *, register_t *));
int	linux_sys_fstatfs	__P((struct proc *, void *, register_t *));
int	linux_sys_uname	__P((struct proc *, void *, register_t *));
int	sys_nanosleep	__P((struct proc *, void *, register_t *));
int	linux_sys_mremap	__P((struct proc *, void *, register_t *));
int	sys_pread	__P((struct proc *, void *, register_t *));
int	sys_pwrite	__P((struct proc *, void *, register_t *));
int	linux_sys_rt_sigreturn	__P((struct proc *, void *, register_t *));
int	linux_sys_rt_sigaction	__P((struct proc *, void *, register_t *));
int	linux_sys_rt_sigprocmask	__P((struct proc *, void *, register_t *));
int	linux_sys_rt_sigpending	__P((struct proc *, void *, register_t *));
int	linux_sys_rt_queueinfo	__P((struct proc *, void *, register_t *));
int	linux_sys_rt_sigsuspend	__P((struct proc *, void *, register_t *));
int	linux_sys_select	__P((struct proc *, void *, register_t *));
int	sys_gettimeofday	__P((struct proc *, void *, register_t *));
int	sys_settimeofday	__P((struct proc *, void *, register_t *));
int	sys_getitimer	__P((struct proc *, void *, register_t *));
int	sys_setitimer	__P((struct proc *, void *, register_t *));
int	sys_utimes	__P((struct proc *, void *, register_t *));
int	sys_getrusage	__P((struct proc *, void *, register_t *));
int	linux_sys_wait4	__P((struct proc *, void *, register_t *));
int	sys___getcwd	__P((struct proc *, void *, register_t *));
#endif /* _LINUX_SYS__SYSCALLARGS_H_ */
