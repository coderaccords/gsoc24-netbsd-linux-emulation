/* $NetBSD: svr4_syscallargs.h,v 1.72 2005/12/11 12:20:26 christos Exp $ */

/*
 * System call argument lists.
 *
 * DO NOT EDIT-- this file is automatically generated.
 * created from	NetBSD: syscalls.master,v 1.52 2003/01/18 08:44:27 thorpej Exp
 */

#ifndef _SVR4_SYS__SYSCALLARGS_H_
#define	_SVR4_SYS__SYSCALLARGS_H_

#ifdef	syscallarg
#undef	syscallarg
#endif

#define	syscallarg(x)							\
	union {								\
		register_t pad;						\
		struct { x datum; } le;					\
		struct { /* LINTED zero array dimension */		\
			int8_t pad[  /* CONSTCOND */			\
				(sizeof (register_t) < sizeof (x))	\
				? 0					\
				: sizeof (register_t) - sizeof (x)];	\
			x datum;					\
		} be;							\
	}

struct svr4_sys_open_args {
	syscallarg(const char *) path;
	syscallarg(int) flags;
	syscallarg(int) mode;
};

struct svr4_sys_wait_args {
	syscallarg(int *) status;
};

struct svr4_sys_creat_args {
	syscallarg(const char *) path;
	syscallarg(int) mode;
};

struct svr4_sys_execv_args {
	syscallarg(const char *) path;
	syscallarg(char **) argp;
};

struct svr4_sys_time_args {
	syscallarg(svr4_time_t *) t;
};

struct svr4_sys_mknod_args {
	syscallarg(const char *) path;
	syscallarg(int) mode;
	syscallarg(int) dev;
};

struct svr4_sys_break_args {
	syscallarg(caddr_t) nsize;
};

struct svr4_sys_stat_args {
	syscallarg(const char *) path;
	syscallarg(struct svr4_stat *) ub;
};

struct svr4_sys_alarm_args {
	syscallarg(unsigned) sec;
};

struct svr4_sys_fstat_args {
	syscallarg(int) fd;
	syscallarg(struct svr4_stat *) sb;
};

struct svr4_sys_utime_args {
	syscallarg(const char *) path;
	syscallarg(struct svr4_utimbuf *) ubuf;
};

struct svr4_sys_access_args {
	syscallarg(const char *) path;
	syscallarg(int) flags;
};

struct svr4_sys_nice_args {
	syscallarg(int) prio;
};

struct svr4_sys_kill_args {
	syscallarg(int) pid;
	syscallarg(int) signum;
};

struct svr4_sys_pgrpsys_args {
	syscallarg(int) cmd;
	syscallarg(int) pid;
	syscallarg(int) pgid;
};

struct svr4_sys_times_args {
	syscallarg(struct tms *) tp;
};

struct svr4_sys_signal_args {
	syscallarg(int) signum;
	syscallarg(svr4_sig_t) handler;
};
#ifdef SYSVMSG

struct svr4_sys_msgsys_args {
	syscallarg(int) what;
	syscallarg(int) a2;
	syscallarg(int) a3;
	syscallarg(int) a4;
	syscallarg(int) a5;
};
#else
#endif

struct svr4_sys_sysarch_args {
	syscallarg(int) op;
	syscallarg(void *) a1;
};
#ifdef SYSVSHM

struct svr4_sys_shmsys_args {
	syscallarg(int) what;
	syscallarg(int) a2;
	syscallarg(int) a3;
	syscallarg(int) a4;
};
#else
#endif
#ifdef SYSVSEM

struct svr4_sys_semsys_args {
	syscallarg(int) what;
	syscallarg(int) a2;
	syscallarg(int) a3;
	syscallarg(int) a4;
	syscallarg(int) a5;
};
#else
#endif

struct svr4_sys_ioctl_args {
	syscallarg(int) fd;
	syscallarg(u_long) com;
	syscallarg(caddr_t) data;
};

struct svr4_sys_utssys_args {
	syscallarg(void *) a1;
	syscallarg(void *) a2;
	syscallarg(int) sel;
	syscallarg(void *) a3;
};

struct svr4_sys_execve_args {
	syscallarg(const char *) path;
	syscallarg(char **) argp;
	syscallarg(char **) envp;
};

struct svr4_sys_fcntl_args {
	syscallarg(int) fd;
	syscallarg(int) cmd;
	syscallarg(char *) arg;
};

struct svr4_sys_ulimit_args {
	syscallarg(int) cmd;
	syscallarg(long) newlimit;
};

struct svr4_sys_getdents_args {
	syscallarg(int) fd;
	syscallarg(char *) buf;
	syscallarg(int) nbytes;
};

struct svr4_sys_getmsg_args {
	syscallarg(int) fd;
	syscallarg(struct svr4_strbuf *) ctl;
	syscallarg(struct svr4_strbuf *) dat;
	syscallarg(int *) flags;
};

struct svr4_sys_putmsg_args {
	syscallarg(int) fd;
	syscallarg(struct svr4_strbuf *) ctl;
	syscallarg(struct svr4_strbuf *) dat;
	syscallarg(int) flags;
};

struct svr4_sys_lstat_args {
	syscallarg(const char *) path;
	syscallarg(struct svr4_stat *) ub;
};

struct svr4_sys_sigprocmask_args {
	syscallarg(int) how;
	syscallarg(const svr4_sigset_t *) set;
	syscallarg(svr4_sigset_t *) oset;
};

struct svr4_sys_sigsuspend_args {
	syscallarg(const svr4_sigset_t *) set;
};

struct svr4_sys_sigaltstack_args {
	syscallarg(const struct svr4_sigaltstack *) nss;
	syscallarg(struct svr4_sigaltstack *) oss;
};

struct svr4_sys_sigaction_args {
	syscallarg(int) signum;
	syscallarg(const struct svr4_sigaction *) nsa;
	syscallarg(struct svr4_sigaction *) osa;
};

struct svr4_sys_sigpending_args {
	syscallarg(int) what;
	syscallarg(svr4_sigset_t *) set;
};

struct svr4_sys_context_args {
	syscallarg(int) func;
	syscallarg(struct svr4_ucontext *) uc;
};

struct svr4_sys_statvfs_args {
	syscallarg(const char *) path;
	syscallarg(struct svr4_statvfs *) fs;
};

struct svr4_sys_fstatvfs_args {
	syscallarg(int) fd;
	syscallarg(struct svr4_statvfs *) fs;
};

struct svr4_sys_waitsys_args {
	syscallarg(int) grp;
	syscallarg(int) id;
	syscallarg(union svr4_siginfo *) info;
	syscallarg(int) options;
};

struct svr4_sys_hrtsys_args {
	syscallarg(int) cmd;
	syscallarg(int) fun;
	syscallarg(int) sub;
	syscallarg(void *) rv1;
	syscallarg(void *) rv2;
};

struct svr4_sys_pathconf_args {
	syscallarg(const char *) path;
	syscallarg(int) name;
};

struct svr4_sys_mmap_args {
	syscallarg(void *) addr;
	syscallarg(svr4_size_t) len;
	syscallarg(int) prot;
	syscallarg(int) flags;
	syscallarg(int) fd;
	syscallarg(svr4_off_t) pos;
};

struct svr4_sys_fpathconf_args {
	syscallarg(int) fd;
	syscallarg(int) name;
};

struct svr4_sys_xstat_args {
	syscallarg(int) two;
	syscallarg(const char *) path;
	syscallarg(struct svr4_xstat *) ub;
};

struct svr4_sys_lxstat_args {
	syscallarg(int) two;
	syscallarg(const char *) path;
	syscallarg(struct svr4_xstat *) ub;
};

struct svr4_sys_fxstat_args {
	syscallarg(int) two;
	syscallarg(int) fd;
	syscallarg(struct svr4_xstat *) sb;
};

struct svr4_sys_xmknod_args {
	syscallarg(int) two;
	syscallarg(char *) path;
	syscallarg(svr4_mode_t) mode;
	syscallarg(svr4_dev_t) dev;
};

struct svr4_sys_setrlimit_args {
	syscallarg(int) which;
	syscallarg(const struct svr4_rlimit *) rlp;
};

struct svr4_sys_getrlimit_args {
	syscallarg(int) which;
	syscallarg(struct svr4_rlimit *) rlp;
};

struct svr4_sys_memcntl_args {
	syscallarg(void *) addr;
	syscallarg(svr4_size_t) len;
	syscallarg(int) cmd;
	syscallarg(void *) arg;
	syscallarg(int) attr;
	syscallarg(int) mask;
};

struct svr4_sys_uname_args {
	syscallarg(struct svr4_utsname *) name;
	syscallarg(int) dummy;
};

struct svr4_sys_sysconfig_args {
	syscallarg(int) name;
};

struct svr4_sys_systeminfo_args {
	syscallarg(int) what;
	syscallarg(char *) buf;
	syscallarg(long) len;
};

struct svr4_sys__lwp_info_args {
	syscallarg(struct svr4_lwpinfo *) lwpinfo;
};

struct svr4_sys_utimes_args {
	syscallarg(const char *) path;
	syscallarg(struct timeval *) tptr;
};

struct svr4_sys_gettimeofday_args {
	syscallarg(struct timeval *) tp;
};

struct svr4_sys__lwp_create_args {
	syscallarg(svr4_ucontext_t *) uc;
	syscallarg(unsigned long) flags;
	syscallarg(svr4_lwpid_t *) lwpid;
};

struct svr4_sys__lwp_suspend_args {
	syscallarg(svr4_lwpid_t) lwpid;
};

struct svr4_sys__lwp_continue_args {
	syscallarg(svr4_lwpid_t) lwpid;
};

struct svr4_sys__lwp_kill_args {
	syscallarg(svr4_lwpid_t) lwpid;
	syscallarg(int) signum;
};

struct svr4_sys__lwp_setprivate_args {
	syscallarg(void *) buffer;
};

struct svr4_sys__lwp_wait_args {
	syscallarg(svr4_lwpid_t) wait_for;
	syscallarg(svr4_lwpid_t *) departed_lwp;
};

struct svr4_sys_pread_args {
	syscallarg(int) fd;
	syscallarg(void *) buf;
	syscallarg(size_t) nbyte;
	syscallarg(svr4_off_t) off;
};

struct svr4_sys_pwrite_args {
	syscallarg(int) fd;
	syscallarg(const void *) buf;
	syscallarg(size_t) nbyte;
	syscallarg(svr4_off_t) off;
};

struct svr4_sys_llseek_args {
	syscallarg(int) fd;
	syscallarg(long) offset1;
	syscallarg(long) offset2;
	syscallarg(int) whence;
};

struct svr4_sys_acl_args {
	syscallarg(char *) path;
	syscallarg(int) cmd;
	syscallarg(int) num;
	syscallarg(struct svr4_aclent *) buf;
};

struct svr4_sys_auditsys_args {
	syscallarg(int) code;
	syscallarg(int) a1;
	syscallarg(int) a2;
	syscallarg(int) a3;
	syscallarg(int) a4;
	syscallarg(int) a5;
};

struct svr4_sys_facl_args {
	syscallarg(int) fd;
	syscallarg(int) cmd;
	syscallarg(int) num;
	syscallarg(struct svr4_aclent *) buf;
};

struct svr4_sys_resolvepath_args {
	syscallarg(const char *) path;
	syscallarg(char *) buf;
	syscallarg(size_t) bufsiz;
};

struct svr4_sys_getdents64_args {
	syscallarg(int) fd;
	syscallarg(struct svr4_dirent64 *) dp;
	syscallarg(int) nbytes;
};

struct svr4_sys_mmap64_args {
	syscallarg(void *) addr;
	syscallarg(svr4_size_t) len;
	syscallarg(int) prot;
	syscallarg(int) flags;
	syscallarg(int) fd;
	syscallarg(svr4_off64_t) pos;
};

struct svr4_sys_stat64_args {
	syscallarg(const char *) path;
	syscallarg(struct svr4_stat64 *) sb;
};

struct svr4_sys_lstat64_args {
	syscallarg(const char *) path;
	syscallarg(struct svr4_stat64 *) sb;
};

struct svr4_sys_fstat64_args {
	syscallarg(int) fd;
	syscallarg(struct svr4_stat64 *) sb;
};

struct svr4_sys_statvfs64_args {
	syscallarg(const char *) path;
	syscallarg(struct svr4_statvfs64 *) fs;
};

struct svr4_sys_fstatvfs64_args {
	syscallarg(int) fd;
	syscallarg(struct svr4_statvfs64 *) fs;
};

struct svr4_sys_setrlimit64_args {
	syscallarg(int) which;
	syscallarg(const struct svr4_rlimit64 *) rlp;
};

struct svr4_sys_getrlimit64_args {
	syscallarg(int) which;
	syscallarg(struct svr4_rlimit64 *) rlp;
};

struct svr4_sys_pread64_args {
	syscallarg(int) fd;
	syscallarg(void *) buf;
	syscallarg(size_t) nbyte;
	syscallarg(svr4_off64_t) off;
};

struct svr4_sys_pwrite64_args {
	syscallarg(int) fd;
	syscallarg(const void *) buf;
	syscallarg(size_t) nbyte;
	syscallarg(svr4_off64_t) off;
};

struct svr4_sys_creat64_args {
	syscallarg(char *) path;
	syscallarg(int) mode;
};

struct svr4_sys_open64_args {
	syscallarg(char *) path;
	syscallarg(int) flags;
	syscallarg(int) mode;
};

struct svr4_sys_socket_args {
	syscallarg(int) domain;
	syscallarg(int) type;
	syscallarg(int) protocol;
};
#if defined(NTP) || !defined(_KERNEL)
#else
#endif

/*
 * System call prototypes.
 */

int	sys_nosys(struct lwp *, void *, register_t *);

int	sys_exit(struct lwp *, void *, register_t *);

int	sys_fork(struct lwp *, void *, register_t *);

int	sys_read(struct lwp *, void *, register_t *);

int	sys_write(struct lwp *, void *, register_t *);

int	svr4_sys_open(struct lwp *, void *, register_t *);

int	sys_close(struct lwp *, void *, register_t *);

int	svr4_sys_wait(struct lwp *, void *, register_t *);

int	svr4_sys_creat(struct lwp *, void *, register_t *);

int	sys_link(struct lwp *, void *, register_t *);

int	sys_unlink(struct lwp *, void *, register_t *);

int	svr4_sys_execv(struct lwp *, void *, register_t *);

int	sys_chdir(struct lwp *, void *, register_t *);

int	svr4_sys_time(struct lwp *, void *, register_t *);

int	svr4_sys_mknod(struct lwp *, void *, register_t *);

int	sys_chmod(struct lwp *, void *, register_t *);

int	sys___posix_chown(struct lwp *, void *, register_t *);

int	svr4_sys_break(struct lwp *, void *, register_t *);

int	svr4_sys_stat(struct lwp *, void *, register_t *);

int	compat_43_sys_lseek(struct lwp *, void *, register_t *);

int	sys_getpid(struct lwp *, void *, register_t *);

int	sys_setuid(struct lwp *, void *, register_t *);

int	sys_getuid_with_euid(struct lwp *, void *, register_t *);

int	svr4_sys_alarm(struct lwp *, void *, register_t *);

int	svr4_sys_fstat(struct lwp *, void *, register_t *);

int	svr4_sys_pause(struct lwp *, void *, register_t *);

int	svr4_sys_utime(struct lwp *, void *, register_t *);

int	svr4_sys_access(struct lwp *, void *, register_t *);

int	svr4_sys_nice(struct lwp *, void *, register_t *);

int	sys_sync(struct lwp *, void *, register_t *);

int	svr4_sys_kill(struct lwp *, void *, register_t *);

int	svr4_sys_pgrpsys(struct lwp *, void *, register_t *);

int	sys_dup(struct lwp *, void *, register_t *);

int	sys_pipe(struct lwp *, void *, register_t *);

int	svr4_sys_times(struct lwp *, void *, register_t *);

int	sys_setgid(struct lwp *, void *, register_t *);

int	sys_getgid_with_egid(struct lwp *, void *, register_t *);

int	svr4_sys_signal(struct lwp *, void *, register_t *);

#ifdef SYSVMSG
int	svr4_sys_msgsys(struct lwp *, void *, register_t *);

#else
#endif
int	svr4_sys_sysarch(struct lwp *, void *, register_t *);

#ifdef SYSVSHM
int	svr4_sys_shmsys(struct lwp *, void *, register_t *);

#else
#endif
#ifdef SYSVSEM
int	svr4_sys_semsys(struct lwp *, void *, register_t *);

#else
#endif
int	svr4_sys_ioctl(struct lwp *, void *, register_t *);

int	svr4_sys_utssys(struct lwp *, void *, register_t *);

int	sys_fsync(struct lwp *, void *, register_t *);

int	svr4_sys_execve(struct lwp *, void *, register_t *);

int	sys_umask(struct lwp *, void *, register_t *);

int	sys_chroot(struct lwp *, void *, register_t *);

int	svr4_sys_fcntl(struct lwp *, void *, register_t *);

int	svr4_sys_ulimit(struct lwp *, void *, register_t *);

int	sys_rmdir(struct lwp *, void *, register_t *);

int	sys_mkdir(struct lwp *, void *, register_t *);

int	svr4_sys_getdents(struct lwp *, void *, register_t *);

int	svr4_sys_getmsg(struct lwp *, void *, register_t *);

int	svr4_sys_putmsg(struct lwp *, void *, register_t *);

int	sys_poll(struct lwp *, void *, register_t *);

int	svr4_sys_lstat(struct lwp *, void *, register_t *);

int	sys_symlink(struct lwp *, void *, register_t *);

int	sys_readlink(struct lwp *, void *, register_t *);

int	sys_getgroups(struct lwp *, void *, register_t *);

int	sys_setgroups(struct lwp *, void *, register_t *);

int	sys_fchmod(struct lwp *, void *, register_t *);

int	sys___posix_fchown(struct lwp *, void *, register_t *);

int	svr4_sys_sigprocmask(struct lwp *, void *, register_t *);

int	svr4_sys_sigsuspend(struct lwp *, void *, register_t *);

int	svr4_sys_sigaltstack(struct lwp *, void *, register_t *);

int	svr4_sys_sigaction(struct lwp *, void *, register_t *);

int	svr4_sys_sigpending(struct lwp *, void *, register_t *);

int	svr4_sys_context(struct lwp *, void *, register_t *);

int	svr4_sys_statvfs(struct lwp *, void *, register_t *);

int	svr4_sys_fstatvfs(struct lwp *, void *, register_t *);

int	svr4_sys_waitsys(struct lwp *, void *, register_t *);

int	svr4_sys_hrtsys(struct lwp *, void *, register_t *);

int	svr4_sys_pathconf(struct lwp *, void *, register_t *);

int	svr4_sys_mmap(struct lwp *, void *, register_t *);

int	sys_mprotect(struct lwp *, void *, register_t *);

int	sys_munmap(struct lwp *, void *, register_t *);

int	svr4_sys_fpathconf(struct lwp *, void *, register_t *);

int	sys_vfork(struct lwp *, void *, register_t *);

int	sys_fchdir(struct lwp *, void *, register_t *);

int	sys_readv(struct lwp *, void *, register_t *);

int	sys_writev(struct lwp *, void *, register_t *);

int	svr4_sys_xstat(struct lwp *, void *, register_t *);

int	svr4_sys_lxstat(struct lwp *, void *, register_t *);

int	svr4_sys_fxstat(struct lwp *, void *, register_t *);

int	svr4_sys_xmknod(struct lwp *, void *, register_t *);

int	svr4_sys_setrlimit(struct lwp *, void *, register_t *);

int	svr4_sys_getrlimit(struct lwp *, void *, register_t *);

int	sys___posix_lchown(struct lwp *, void *, register_t *);

int	svr4_sys_memcntl(struct lwp *, void *, register_t *);

int	sys___posix_rename(struct lwp *, void *, register_t *);

int	svr4_sys_uname(struct lwp *, void *, register_t *);

int	sys_setegid(struct lwp *, void *, register_t *);

int	svr4_sys_sysconfig(struct lwp *, void *, register_t *);

int	sys_adjtime(struct lwp *, void *, register_t *);

int	svr4_sys_systeminfo(struct lwp *, void *, register_t *);

int	sys_seteuid(struct lwp *, void *, register_t *);

int	svr4_sys__lwp_info(struct lwp *, void *, register_t *);

int	sys_fchroot(struct lwp *, void *, register_t *);

int	svr4_sys_utimes(struct lwp *, void *, register_t *);

int	svr4_sys_vhangup(struct lwp *, void *, register_t *);

int	svr4_sys_gettimeofday(struct lwp *, void *, register_t *);

int	sys_getitimer(struct lwp *, void *, register_t *);

int	sys_setitimer(struct lwp *, void *, register_t *);

int	svr4_sys__lwp_create(struct lwp *, void *, register_t *);

int	svr4_sys__lwp_exit(struct lwp *, void *, register_t *);

int	svr4_sys__lwp_suspend(struct lwp *, void *, register_t *);

int	svr4_sys__lwp_continue(struct lwp *, void *, register_t *);

int	svr4_sys__lwp_kill(struct lwp *, void *, register_t *);

int	svr4_sys__lwp_self(struct lwp *, void *, register_t *);

int	svr4_sys__lwp_getprivate(struct lwp *, void *, register_t *);

int	svr4_sys__lwp_setprivate(struct lwp *, void *, register_t *);

int	svr4_sys__lwp_wait(struct lwp *, void *, register_t *);

int	svr4_sys_pread(struct lwp *, void *, register_t *);

int	svr4_sys_pwrite(struct lwp *, void *, register_t *);

int	svr4_sys_llseek(struct lwp *, void *, register_t *);

int	svr4_sys_acl(struct lwp *, void *, register_t *);

int	svr4_sys_auditsys(struct lwp *, void *, register_t *);

int	sys_nanosleep(struct lwp *, void *, register_t *);

int	svr4_sys_facl(struct lwp *, void *, register_t *);

int	sys_setreuid(struct lwp *, void *, register_t *);

int	sys_setregid(struct lwp *, void *, register_t *);

int	svr4_sys_resolvepath(struct lwp *, void *, register_t *);

int	svr4_sys_getdents64(struct lwp *, void *, register_t *);

int	svr4_sys_mmap64(struct lwp *, void *, register_t *);

int	svr4_sys_stat64(struct lwp *, void *, register_t *);

int	svr4_sys_lstat64(struct lwp *, void *, register_t *);

int	svr4_sys_fstat64(struct lwp *, void *, register_t *);

int	svr4_sys_statvfs64(struct lwp *, void *, register_t *);

int	svr4_sys_fstatvfs64(struct lwp *, void *, register_t *);

int	svr4_sys_setrlimit64(struct lwp *, void *, register_t *);

int	svr4_sys_getrlimit64(struct lwp *, void *, register_t *);

int	svr4_sys_pread64(struct lwp *, void *, register_t *);

int	svr4_sys_pwrite64(struct lwp *, void *, register_t *);

int	svr4_sys_creat64(struct lwp *, void *, register_t *);

int	svr4_sys_open64(struct lwp *, void *, register_t *);

int	svr4_sys_socket(struct lwp *, void *, register_t *);

int	sys_socketpair(struct lwp *, void *, register_t *);

int	sys_bind(struct lwp *, void *, register_t *);

int	sys_listen(struct lwp *, void *, register_t *);

int	compat_43_sys_accept(struct lwp *, void *, register_t *);

int	sys_connect(struct lwp *, void *, register_t *);

int	sys_shutdown(struct lwp *, void *, register_t *);

int	compat_43_sys_recv(struct lwp *, void *, register_t *);

int	compat_43_sys_recvfrom(struct lwp *, void *, register_t *);

int	compat_43_sys_recvmsg(struct lwp *, void *, register_t *);

int	compat_43_sys_send(struct lwp *, void *, register_t *);

int	compat_43_sys_sendmsg(struct lwp *, void *, register_t *);

int	sys_sendto(struct lwp *, void *, register_t *);

int	compat_43_sys_getpeername(struct lwp *, void *, register_t *);

int	compat_43_sys_getsockname(struct lwp *, void *, register_t *);

int	sys_getsockopt(struct lwp *, void *, register_t *);

int	sys_setsockopt(struct lwp *, void *, register_t *);

int	sys_ntp_gettime(struct lwp *, void *, register_t *);

#if defined(NTP) || !defined(_KERNEL)
int	sys_ntp_adjtime(struct lwp *, void *, register_t *);

#else
#endif
#endif /* _SVR4_SYS__SYSCALLARGS_H_ */
