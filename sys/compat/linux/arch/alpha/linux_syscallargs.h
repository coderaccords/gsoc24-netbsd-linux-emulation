/* $NetBSD: linux_syscallargs.h,v 1.26 2000/12/09 05:37:04 mycroft Exp $ */

/*
 * System call argument lists.
 *
 * DO NOT EDIT-- this file is automatically generated.
 * created from	NetBSD: syscalls.master,v 1.25 2000/12/09 05:27:29 mycroft Exp 
 */

#ifndef _LINUX_SYS__SYSCALLARGS_H_
#define _LINUX_SYS__SYSCALLARGS_H_

#ifdef	syscallarg
#undef	syscallarg
#endif

#define	syscallarg(x)							\
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

struct osf1_sys_wait4_args {
	syscallarg(int) pid;
	syscallarg(int *) status;
	syscallarg(int) options;
	syscallarg(struct osf1_rusage *) rusage;
};

struct linux_sys_creat_args {
	syscallarg(const char *) path;
	syscallarg(mode_t) mode;
};

struct linux_sys_unlink_args {
	syscallarg(const char *) path;
};

struct linux_sys_chdir_args {
	syscallarg(const char *) path;
};

struct linux_sys_mknod_args {
	syscallarg(const char *) path;
	syscallarg(int) mode;
	syscallarg(int) dev;
};

struct linux_sys_chmod_args {
	syscallarg(const char *) path;
	syscallarg(int) mode;
};

struct linux_sys_chown_args {
	syscallarg(const char *) path;
	syscallarg(int) uid;
	syscallarg(int) gid;
};

struct linux_sys_brk_args {
	syscallarg(char *) nsize;
};

struct osf1_sys_mount_args {
	syscallarg(int) type;
	syscallarg(const char *) path;
	syscallarg(int) flags;
	syscallarg(caddr_t) data;
};

struct linux_sys_ptrace_args {
	syscallarg(long) request;
	syscallarg(long) pid;
	syscallarg(long) addr;
	syscallarg(long) data;
};

struct linux_sys_access_args {
	syscallarg(const char *) path;
	syscallarg(int) flags;
};

struct linux_sys_kill_args {
	syscallarg(int) pid;
	syscallarg(int) signum;
};

struct osf1_sys_set_program_attributes_args {
	syscallarg(caddr_t) taddr;
	syscallarg(unsigned long) tsize;
	syscallarg(caddr_t) daddr;
	syscallarg(unsigned long) dsize;
};

struct linux_sys_open_args {
	syscallarg(const char *) path;
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
	syscallarg(const char *) path;
	syscallarg(const char *) to;
};

struct linux_sys_readlink_args {
	syscallarg(const char *) name;
	syscallarg(char *) buf;
	syscallarg(int) count;
};

struct linux_sys_execve_args {
	syscallarg(const char *) path;
	syscallarg(char **) argp;
	syscallarg(char **) envp;
};

struct linux_sys_stat_args {
	syscallarg(const char *) path;
	syscallarg(struct linux_stat *) sp;
};

struct linux_sys_lstat_args {
	syscallarg(const char *) path;
	syscallarg(struct linux_stat *) sp;
};

struct osf1_sys_setitimer_args {
	syscallarg(u_int) which;
	syscallarg(struct osf1_itimerval *) itv;
	syscallarg(struct osf1_itimerval *) oitv;
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

struct osf1_sys_select_args {
	syscallarg(u_int) nd;
	syscallarg(fd_set *) in;
	syscallarg(fd_set *) ou;
	syscallarg(fd_set *) ex;
	syscallarg(struct osf1_timeval *) tv;
};

struct linux_sys_socket_args {
	syscallarg(int) domain;
	syscallarg(int) type;
	syscallarg(int) protocol;
};

struct linux_sys_connect_args {
	syscallarg(int) s;
	syscallarg(const struct sockaddr *) name;
	syscallarg(unsigned int) namelen;
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

struct osf1_sys_gettimeofday_args {
	syscallarg(struct osf1_timeval *) tv;
	syscallarg(struct osf1_timezone *) tzp;
};

struct osf1_sys_getrusage_args {
	syscallarg(int) who;
	syscallarg(struct osf1_rusage *) rusage;
};

struct linux_sys_getsockopt_args {
	syscallarg(int) s;
	syscallarg(int) level;
	syscallarg(int) optname;
	syscallarg(void *) optval;
	syscallarg(int *) optlen;
};

struct osf1_sys_settimeofday_args {
	syscallarg(struct osf1_timeval *) tv;
	syscallarg(struct osf1_timezone *) tzp;
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
	syscallarg(const char *) from;
	syscallarg(const char *) to;
};

struct linux_sys_truncate_args {
	syscallarg(const char *) path;
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
	syscallarg(const char *) path;
	syscallarg(int) mode;
};

struct linux_sys_rmdir_args {
	syscallarg(const char *) path;
};

struct osf1_sys_utimes_args {
	syscallarg(const char *) path;
	syscallarg(const struct osf1_timeval *) tptr;
};

struct linux_sys_sigaction_args {
	syscallarg(int) signum;
	syscallarg(const struct linux_old_sigaction *) nsa;
	syscallarg(struct linux_old_sigaction *) osa;
};

struct osf1_sys_statfs_args {
	syscallarg(const char *) path;
	syscallarg(struct osf1_statfs *) buf;
	syscallarg(int) len;
};

struct osf1_sys_fstatfs_args {
	syscallarg(int) fd;
	syscallarg(struct osf1_statfs *) buf;
	syscallarg(int) len;
};

struct linux_sys_setdomainname_args {
	syscallarg(char *) domainname;
	syscallarg(int) len;
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

struct osf1_sys_sysinfo_args {
	syscallarg(int) cmd;
	syscallarg(char) buf;
	syscallarg(long) len;
};

struct osf1_sys_usleep_thread_args {
	syscallarg(struct osf1_timeval *) sleep;
	syscallarg(struct osf1_timeval *) slept;
};

struct osf1_sys_getsysinfo_args {
	syscallarg(u_long) op;
	syscallarg(caddr_t) buffer;
	syscallarg(u_long) nbytes;
	syscallarg(caddr_t) arg;
	syscallarg(u_long) flag;
};

struct osf1_sys_setsysinfo_args {
	syscallarg(u_long) op;
	syscallarg(caddr_t) buffer;
	syscallarg(u_long) nbytes;
	syscallarg(caddr_t) arg;
	syscallarg(u_long) flag;
};

struct linux_sys_fdatasync_args {
	syscallarg(int) fd;
};

struct linux_sys_swapoff_args {
	syscallarg(const char *) path;
};

struct linux_sys_getdents_args {
	syscallarg(int) fd;
	syscallarg(struct linux_dirent *) dent;
	syscallarg(unsigned int) count;
};

struct linux_sys_reboot_args {
	syscallarg(int) magic1;
	syscallarg(int) magic2;
	syscallarg(int) cmd;
	syscallarg(void *) arg;
};

struct linux_sys_clone_args {
	syscallarg(int) flags;
	syscallarg(void *) stack;
};

struct linux_sys_uselib_args {
	syscallarg(const char *) path;
};

struct linux_sys___sysctl_args {
	syscallarg(struct linux___sysctl *) lsp;
};

struct linux_sys_swapon_args {
	syscallarg(const char *) name;
};

struct linux_sys_times_args {
	syscallarg(struct times *) tms;
};

struct linux_sys_personality_args {
	syscallarg(int) per;
};

struct linux_sys_setfsuid_args {
	syscallarg(uid_t) uid;
};

struct linux_sys_statfs_args {
	syscallarg(const char *) path;
	syscallarg(struct linux_statfs *) sp;
};

struct linux_sys_fstatfs_args {
	syscallarg(int) fd;
	syscallarg(struct linux_statfs *) sp;
};

struct linux_sys_sched_setparam_args {
	syscallarg(pid_t) pid;
	syscallarg(const struct linux_sched_param *) sp;
};

struct linux_sys_sched_getparam_args {
	syscallarg(pid_t) pid;
	syscallarg(struct linux_sched_param *) sp;
};

struct linux_sys_sched_setscheduler_args {
	syscallarg(pid_t) pid;
	syscallarg(int) policy;
	syscallarg(const struct linux_sched_param *) sp;
};

struct linux_sys_sched_getscheduler_args {
	syscallarg(pid_t) pid;
};

struct linux_sys_sched_get_priority_max_args {
	syscallarg(int) policy;
};

struct linux_sys_sched_get_priority_min_args {
	syscallarg(int) policy;
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

struct linux_sys_setresuid_args {
	syscallarg(uid_t) ruid;
	syscallarg(uid_t) euid;
	syscallarg(uid_t) suid;
};

struct linux_sys_getresuid_args {
	syscallarg(uid_t *) ruid;
	syscallarg(uid_t *) euid;
	syscallarg(uid_t *) suid;
};

struct linux_sys_pread_args {
	syscallarg(int) fd;
	syscallarg(char *) buf;
	syscallarg(size_t) nbyte;
	syscallarg(linux_off_t) offset;
};

struct linux_sys_pwrite_args {
	syscallarg(int) fd;
	syscallarg(char *) buf;
	syscallarg(size_t) nbyte;
	syscallarg(linux_off_t) offset;
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

int	sys_nosys(struct proc *, void *, register_t *);
int	sys_exit(struct proc *, void *, register_t *);
int	sys_fork(struct proc *, void *, register_t *);
int	sys_read(struct proc *, void *, register_t *);
int	sys_write(struct proc *, void *, register_t *);
int	sys_close(struct proc *, void *, register_t *);
int	osf1_sys_wait4(struct proc *, void *, register_t *);
int	linux_sys_creat(struct proc *, void *, register_t *);
int	sys_link(struct proc *, void *, register_t *);
int	linux_sys_unlink(struct proc *, void *, register_t *);
int	linux_sys_chdir(struct proc *, void *, register_t *);
int	sys_fchdir(struct proc *, void *, register_t *);
int	linux_sys_mknod(struct proc *, void *, register_t *);
int	linux_sys_chmod(struct proc *, void *, register_t *);
int	linux_sys_chown(struct proc *, void *, register_t *);
int	linux_sys_brk(struct proc *, void *, register_t *);
int	compat_43_sys_lseek(struct proc *, void *, register_t *);
int	sys_getpid(struct proc *, void *, register_t *);
int	osf1_sys_mount(struct proc *, void *, register_t *);
int	sys_setuid(struct proc *, void *, register_t *);
int	sys_getuid(struct proc *, void *, register_t *);
int	linux_sys_ptrace(struct proc *, void *, register_t *);
int	linux_sys_access(struct proc *, void *, register_t *);
int	sys_sync(struct proc *, void *, register_t *);
int	linux_sys_kill(struct proc *, void *, register_t *);
int	sys_setpgid(struct proc *, void *, register_t *);
int	sys_dup(struct proc *, void *, register_t *);
int	linux_sys_pipe(struct proc *, void *, register_t *);
int	osf1_sys_set_program_attributes(struct proc *, void *, register_t *);
int	linux_sys_open(struct proc *, void *, register_t *);
int	sys_getgid(struct proc *, void *, register_t *);
int	compat_13_sys_sigprocmask(struct proc *, void *, register_t *);
int	sys_acct(struct proc *, void *, register_t *);
int	linux_sys_sigpending(struct proc *, void *, register_t *);
int	linux_sys_ioctl(struct proc *, void *, register_t *);
int	linux_sys_symlink(struct proc *, void *, register_t *);
int	linux_sys_readlink(struct proc *, void *, register_t *);
int	linux_sys_execve(struct proc *, void *, register_t *);
int	sys_umask(struct proc *, void *, register_t *);
int	sys_chroot(struct proc *, void *, register_t *);
int	sys_getpgrp(struct proc *, void *, register_t *);
int	compat_43_sys_getpagesize(struct proc *, void *, register_t *);
int	sys___vfork14(struct proc *, void *, register_t *);
int	linux_sys_stat(struct proc *, void *, register_t *);
int	linux_sys_lstat(struct proc *, void *, register_t *);
int	linux_sys_mmap(struct proc *, void *, register_t *);
int	sys_munmap(struct proc *, void *, register_t *);
int	sys_mprotect(struct proc *, void *, register_t *);
int	sys_getgroups(struct proc *, void *, register_t *);
int	sys_setgroups(struct proc *, void *, register_t *);
int	osf1_sys_setitimer(struct proc *, void *, register_t *);
int	compat_43_sys_gethostname(struct proc *, void *, register_t *);
int	compat_43_sys_sethostname(struct proc *, void *, register_t *);
int	sys_dup2(struct proc *, void *, register_t *);
int	linux_sys_fstat(struct proc *, void *, register_t *);
int	linux_sys_fcntl(struct proc *, void *, register_t *);
int	osf1_sys_select(struct proc *, void *, register_t *);
int	sys_poll(struct proc *, void *, register_t *);
int	sys_fsync(struct proc *, void *, register_t *);
int	sys_setpriority(struct proc *, void *, register_t *);
int	linux_sys_socket(struct proc *, void *, register_t *);
int	linux_sys_connect(struct proc *, void *, register_t *);
int	compat_43_sys_accept(struct proc *, void *, register_t *);
int	sys_getpriority(struct proc *, void *, register_t *);
int	compat_43_sys_send(struct proc *, void *, register_t *);
int	compat_43_sys_recv(struct proc *, void *, register_t *);
int	linux_sys_sigreturn(struct proc *, void *, register_t *);
int	sys_bind(struct proc *, void *, register_t *);
int	linux_sys_setsockopt(struct proc *, void *, register_t *);
int	sys_listen(struct proc *, void *, register_t *);
int	linux_sys_sigsuspend(struct proc *, void *, register_t *);
int	compat_43_sys_sigstack(struct proc *, void *, register_t *);
int	sys_recvmsg(struct proc *, void *, register_t *);
int	sys_sendmsg(struct proc *, void *, register_t *);
int	osf1_sys_gettimeofday(struct proc *, void *, register_t *);
int	osf1_sys_getrusage(struct proc *, void *, register_t *);
int	linux_sys_getsockopt(struct proc *, void *, register_t *);
int	sys_readv(struct proc *, void *, register_t *);
int	sys_writev(struct proc *, void *, register_t *);
int	osf1_sys_settimeofday(struct proc *, void *, register_t *);
int	linux_sys_fchown(struct proc *, void *, register_t *);
int	sys_fchmod(struct proc *, void *, register_t *);
int	linux_sys_recvfrom(struct proc *, void *, register_t *);
int	linux_sys_setreuid(struct proc *, void *, register_t *);
int	linux_sys_setregid(struct proc *, void *, register_t *);
int	linux_sys_rename(struct proc *, void *, register_t *);
int	linux_sys_truncate(struct proc *, void *, register_t *);
int	compat_43_sys_ftruncate(struct proc *, void *, register_t *);
int	sys_flock(struct proc *, void *, register_t *);
int	sys_setgid(struct proc *, void *, register_t *);
int	linux_sys_sendto(struct proc *, void *, register_t *);
int	sys_shutdown(struct proc *, void *, register_t *);
int	linux_sys_socketpair(struct proc *, void *, register_t *);
int	linux_sys_mkdir(struct proc *, void *, register_t *);
int	linux_sys_rmdir(struct proc *, void *, register_t *);
int	osf1_sys_utimes(struct proc *, void *, register_t *);
int	compat_43_sys_getpeername(struct proc *, void *, register_t *);
int	compat_43_sys_getrlimit(struct proc *, void *, register_t *);
int	compat_43_sys_setrlimit(struct proc *, void *, register_t *);
int	sys_setsid(struct proc *, void *, register_t *);
int	compat_43_sys_getsockname(struct proc *, void *, register_t *);
int	linux_sys_sigaction(struct proc *, void *, register_t *);
int	compat_43_sys_getdirentries(struct proc *, void *, register_t *);
int	osf1_sys_statfs(struct proc *, void *, register_t *);
int	osf1_sys_fstatfs(struct proc *, void *, register_t *);
int	compat_09_sys_getdomainname(struct proc *, void *, register_t *);
int	linux_sys_setdomainname(struct proc *, void *, register_t *);
#ifdef SYSVMSG
int	linux_sys_msgctl(struct proc *, void *, register_t *);
int	sys_msgget(struct proc *, void *, register_t *);
int	sys_msgrcv(struct proc *, void *, register_t *);
int	sys_msgsnd(struct proc *, void *, register_t *);
#else
#endif
#ifdef SYSVSEM
int	linux_sys_semctl(struct proc *, void *, register_t *);
int	sys_semget(struct proc *, void *, register_t *);
int	sys_semop(struct proc *, void *, register_t *);
#else
#endif
int	linux_sys_olduname(struct proc *, void *, register_t *);
int	linux_sys_lchown(struct proc *, void *, register_t *);
#ifdef SYSVSHM
int	linux_sys_shmat(struct proc *, void *, register_t *);
int	linux_sys_shmctl(struct proc *, void *, register_t *);
int	sys_shmdt(struct proc *, void *, register_t *);
int	sys_shmget(struct proc *, void *, register_t *);
#else
#endif
int	linux_sys_msync(struct proc *, void *, register_t *);
int	linux_sys_getpgid(struct proc *, void *, register_t *);
int	sys_getsid(struct proc *, void *, register_t *);
int	osf1_sys_sysinfo(struct proc *, void *, register_t *);
int	osf1_sys_usleep_thread(struct proc *, void *, register_t *);
int	osf1_sys_getsysinfo(struct proc *, void *, register_t *);
int	osf1_sys_setsysinfo(struct proc *, void *, register_t *);
int	linux_sys_fdatasync(struct proc *, void *, register_t *);
int	linux_sys_swapoff(struct proc *, void *, register_t *);
int	linux_sys_getdents(struct proc *, void *, register_t *);
int	linux_sys_reboot(struct proc *, void *, register_t *);
int	linux_sys_clone(struct proc *, void *, register_t *);
#ifdef EXEC_AOUT
int	linux_sys_uselib(struct proc *, void *, register_t *);
#else
#endif
int	sys_mlock(struct proc *, void *, register_t *);
int	sys_munlock(struct proc *, void *, register_t *);
int	sys_mlockall(struct proc *, void *, register_t *);
int	sys_munlockall(struct proc *, void *, register_t *);
int	linux_sys___sysctl(struct proc *, void *, register_t *);
int	linux_sys_swapon(struct proc *, void *, register_t *);
int	linux_sys_times(struct proc *, void *, register_t *);
int	linux_sys_personality(struct proc *, void *, register_t *);
int	linux_sys_setfsuid(struct proc *, void *, register_t *);
int	linux_sys_statfs(struct proc *, void *, register_t *);
int	linux_sys_fstatfs(struct proc *, void *, register_t *);
int	linux_sys_sched_setparam(struct proc *, void *, register_t *);
int	linux_sys_sched_getparam(struct proc *, void *, register_t *);
int	linux_sys_sched_setscheduler(struct proc *, void *, register_t *);
int	linux_sys_sched_getscheduler(struct proc *, void *, register_t *);
int	linux_sys_sched_yield(struct proc *, void *, register_t *);
int	linux_sys_sched_get_priority_max(struct proc *, void *, register_t *);
int	linux_sys_sched_get_priority_min(struct proc *, void *, register_t *);
int	linux_sys_uname(struct proc *, void *, register_t *);
int	sys_nanosleep(struct proc *, void *, register_t *);
int	linux_sys_mremap(struct proc *, void *, register_t *);
int	linux_sys_setresuid(struct proc *, void *, register_t *);
int	linux_sys_getresuid(struct proc *, void *, register_t *);
int	linux_sys_pread(struct proc *, void *, register_t *);
int	linux_sys_pwrite(struct proc *, void *, register_t *);
int	linux_sys_rt_sigreturn(struct proc *, void *, register_t *);
int	linux_sys_rt_sigaction(struct proc *, void *, register_t *);
int	linux_sys_rt_sigprocmask(struct proc *, void *, register_t *);
int	linux_sys_rt_sigpending(struct proc *, void *, register_t *);
int	linux_sys_rt_queueinfo(struct proc *, void *, register_t *);
int	linux_sys_rt_sigsuspend(struct proc *, void *, register_t *);
int	linux_sys_select(struct proc *, void *, register_t *);
int	sys_gettimeofday(struct proc *, void *, register_t *);
int	sys_settimeofday(struct proc *, void *, register_t *);
int	sys_getitimer(struct proc *, void *, register_t *);
int	sys_setitimer(struct proc *, void *, register_t *);
int	sys_utimes(struct proc *, void *, register_t *);
int	sys_getrusage(struct proc *, void *, register_t *);
int	linux_sys_wait4(struct proc *, void *, register_t *);
int	sys___getcwd(struct proc *, void *, register_t *);
#endif /* _LINUX_SYS__SYSCALLARGS_H_ */
