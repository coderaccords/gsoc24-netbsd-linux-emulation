/* $NetBSD: ibcs2_syscallargs.h,v 1.37 2003/09/10 19:47:49 christos Exp $ */

/*
 * System call argument lists.
 *
 * DO NOT EDIT-- this file is automatically generated.
 * created from	NetBSD: syscalls.master,v 1.28 2003/09/10 19:47:40 christos Exp 
 */

#ifndef _IBCS2_SYS__SYSCALLARGS_H_
#define	_IBCS2_SYS__SYSCALLARGS_H_

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

struct ibcs2_sys_read_args {
	syscallarg(int) fd;
	syscallarg(char *) buf;
	syscallarg(u_int) nbytes;
};

struct ibcs2_sys_open_args {
	syscallarg(const char *) path;
	syscallarg(int) flags;
	syscallarg(int) mode;
};

struct ibcs2_sys_waitsys_args {
	syscallarg(int) a1;
	syscallarg(int) a2;
	syscallarg(int) a3;
};

struct ibcs2_sys_creat_args {
	syscallarg(const char *) path;
	syscallarg(int) mode;
};

struct ibcs2_sys_unlink_args {
	syscallarg(const char *) path;
};

struct ibcs2_sys_execv_args {
	syscallarg(const char *) path;
	syscallarg(char **) argp;
};

struct ibcs2_sys_chdir_args {
	syscallarg(const char *) path;
};

struct ibcs2_sys_time_args {
	syscallarg(ibcs2_time_t *) tp;
};

struct ibcs2_sys_mknod_args {
	syscallarg(const char *) path;
	syscallarg(int) mode;
	syscallarg(int) dev;
};

struct ibcs2_sys_chmod_args {
	syscallarg(const char *) path;
	syscallarg(int) mode;
};

struct ibcs2_sys_chown_args {
	syscallarg(const char *) path;
	syscallarg(int) uid;
	syscallarg(int) gid;
};

struct ibcs2_sys_stat_args {
	syscallarg(const char *) path;
	syscallarg(struct ibcs2_stat *) st;
};

struct ibcs2_sys_mount_args {
	syscallarg(char *) special;
	syscallarg(char *) dir;
	syscallarg(int) flags;
	syscallarg(int) fstype;
	syscallarg(char *) data;
	syscallarg(int) len;
};

struct ibcs2_sys_umount_args {
	syscallarg(char *) name;
};

struct ibcs2_sys_setuid_args {
	syscallarg(int) uid;
};

struct ibcs2_sys_stime_args {
	syscallarg(long *) timep;
};

struct ibcs2_sys_alarm_args {
	syscallarg(unsigned) sec;
};

struct ibcs2_sys_fstat_args {
	syscallarg(int) fd;
	syscallarg(struct ibcs2_stat *) st;
};

struct ibcs2_sys_utime_args {
	syscallarg(const char *) path;
	syscallarg(struct ibcs2_utimbuf *) buf;
};

struct ibcs2_sys_gtty_args {
	syscallarg(int) fd;
	syscallarg(struct ibcs2_sgttyb *) tb;
};

struct ibcs2_sys_access_args {
	syscallarg(const char *) path;
	syscallarg(int) flags;
};

struct ibcs2_sys_nice_args {
	syscallarg(int) incr;
};

struct ibcs2_sys_statfs_args {
	syscallarg(const char *) path;
	syscallarg(struct ibcs2_statfs *) buf;
	syscallarg(int) len;
	syscallarg(int) fstype;
};

struct ibcs2_sys_kill_args {
	syscallarg(int) pid;
	syscallarg(int) signo;
};

struct ibcs2_sys_fstatfs_args {
	syscallarg(int) fd;
	syscallarg(struct ibcs2_statfs *) buf;
	syscallarg(int) len;
	syscallarg(int) fstype;
};

struct ibcs2_sys_pgrpsys_args {
	syscallarg(int) type;
	syscallarg(caddr_t) dummy;
	syscallarg(int) pid;
	syscallarg(int) pgid;
};

struct ibcs2_sys_times_args {
	syscallarg(struct tms *) tp;
};

struct ibcs2_sys_plock_args {
	syscallarg(int) cmd;
};

struct ibcs2_sys_setgid_args {
	syscallarg(int) gid;
};

struct ibcs2_sys_sigsys_args {
	syscallarg(int) sig;
	syscallarg(ibcs2_sig_t) fp;
};

struct ibcs2_sys_msgsys_args {
	syscallarg(int) which;
	syscallarg(int) a2;
	syscallarg(int) a3;
	syscallarg(int) a4;
	syscallarg(int) a5;
	syscallarg(int) a6;
};

struct ibcs2_sys_sysmachine_args {
	syscallarg(int) cmd;
	syscallarg(int) arg;
};

struct ibcs2_sys_shmsys_args {
	syscallarg(int) which;
	syscallarg(int) a2;
	syscallarg(int) a3;
	syscallarg(int) a4;
};

struct ibcs2_sys_semsys_args {
	syscallarg(int) which;
	syscallarg(int) a2;
	syscallarg(int) a3;
	syscallarg(int) a4;
	syscallarg(int) a5;
};

struct ibcs2_sys_ioctl_args {
	syscallarg(int) fd;
	syscallarg(int) cmd;
	syscallarg(caddr_t) data;
};

struct ibcs2_sys_uadmin_args {
	syscallarg(int) cmd;
	syscallarg(int) func;
	syscallarg(caddr_t) data;
};

struct ibcs2_sys_utssys_args {
	syscallarg(int) a1;
	syscallarg(int) a2;
	syscallarg(int) flag;
};

struct ibcs2_sys_execve_args {
	syscallarg(const char *) path;
	syscallarg(char **) argp;
	syscallarg(char **) envp;
};

struct ibcs2_sys_fcntl_args {
	syscallarg(int) fd;
	syscallarg(int) cmd;
	syscallarg(char *) arg;
};

struct ibcs2_sys_ulimit_args {
	syscallarg(int) cmd;
	syscallarg(int) newlimit;
};

struct ibcs2_sys_rmdir_args {
	syscallarg(const char *) path;
};

struct ibcs2_sys_mkdir_args {
	syscallarg(const char *) path;
	syscallarg(int) mode;
};

struct ibcs2_sys_getdents_args {
	syscallarg(int) fd;
	syscallarg(char *) buf;
	syscallarg(int) nbytes;
};

struct ibcs2_sys_sysfs_args {
	syscallarg(int) cmd;
	syscallarg(caddr_t) d1;
	syscallarg(char *) buf;
};

struct ibcs2_sys_getmsg_args {
	syscallarg(int) fd;
	syscallarg(struct ibcs2_stropts *) ctl;
	syscallarg(struct ibcs2_stropts *) dat;
	syscallarg(int *) flags;
};

struct ibcs2_sys_putmsg_args {
	syscallarg(int) fd;
	syscallarg(struct ibcs2_stropts *) ctl;
	syscallarg(struct ibcs2_stropts *) dat;
	syscallarg(int) flags;
};

struct ibcs2_sys_symlink_args {
	syscallarg(const char *) path;
	syscallarg(const char *) link;
};

struct ibcs2_sys_lstat_args {
	syscallarg(const char *) path;
	syscallarg(struct ibcs2_stat *) st;
};

struct ibcs2_sys_readlink_args {
	syscallarg(const char *) path;
	syscallarg(char *) buf;
	syscallarg(int) count;
};

struct ibcs2_sys_sigaltstack_args {
	syscallarg(const struct ibcs2_sigaltstack *) nss;
	syscallarg(struct ibcs2_sigaltstack *) oss;
};

struct ibcs2_sys_statvfs_args {
	syscallarg(const char *) path;
	syscallarg(struct ibcs2_statvfs *) buf;
};

struct ibcs2_sys_fstatvfs_args {
	syscallarg(int) fd;
	syscallarg(struct ibcs2_statvfs *) buf;
};

struct ibcs2_sys_mmap_args {
	syscallarg(ibcs2_caddr_t) addr;
	syscallarg(ibcs2_size_t) len;
	syscallarg(int) prot;
	syscallarg(int) flags;
	syscallarg(int) fd;
	syscallarg(ibcs2_off_t) off;
};

struct ibcs2_sys_memcntl_args {
	syscallarg(ibcs2_caddr_t) addr;
	syscallarg(ibcs2_size_t) len;
	syscallarg(int) cmd;
	syscallarg(ibcs2_caddr_t) arg;
	syscallarg(int) attr;
	syscallarg(int) mask;
};

struct ibcs2_sys_gettimeofday_args {
	syscallarg(struct timeval *) tp;
};

struct ibcs2_sys_settimeofday_args {
	syscallarg(struct timeval *) tp;
};

struct xenix_sys_locking_args {
	syscallarg(int) fd;
	syscallarg(int) blk;
	syscallarg(int) size;
};

struct xenix_sys_rdchk_args {
	syscallarg(int) fd;
};

struct xenix_sys_chsize_args {
	syscallarg(int) fd;
	syscallarg(long) size;
};

struct xenix_sys_ftime_args {
	syscallarg(struct xenix_timeb *) tp;
};

struct xenix_sys_nap_args {
	syscallarg(long) millisec;
};

struct ibcs2_sys_eaccess_args {
	syscallarg(const char *) path;
	syscallarg(int) flags;
};

struct ibcs2_sys_sigaction_args {
	syscallarg(int) signum;
	syscallarg(const struct ibcs2_sigaction *) nsa;
	syscallarg(struct ibcs2_sigaction *) osa;
};

struct ibcs2_sys_sigprocmask_args {
	syscallarg(int) how;
	syscallarg(const ibcs2_sigset_t *) set;
	syscallarg(ibcs2_sigset_t *) oset;
};

struct ibcs2_sys_sigpending_args {
	syscallarg(ibcs2_sigset_t *) set;
};

struct ibcs2_sys_sigsuspend_args {
	syscallarg(const ibcs2_sigset_t *) set;
};

struct ibcs2_sys_getgroups_args {
	syscallarg(int) gidsetsize;
	syscallarg(ibcs2_gid_t *) gidset;
};

struct ibcs2_sys_setgroups_args {
	syscallarg(int) gidsetsize;
	syscallarg(ibcs2_gid_t *) gidset;
};

struct ibcs2_sys_sysconf_args {
	syscallarg(int) name;
};

struct ibcs2_sys_pathconf_args {
	syscallarg(char *) path;
	syscallarg(int) name;
};

struct ibcs2_sys_fpathconf_args {
	syscallarg(int) fd;
	syscallarg(int) name;
};

struct ibcs2_sys_rename_args {
	syscallarg(const char *) from;
	syscallarg(const char *) to;
};

struct ibcs2_sys_scoinfo_args {
	syscallarg(struct scoutsname *) bp;
	syscallarg(int) len;
};

/*
 * System call prototypes.
 */

int	sys_nosys(struct lwp *, void *, register_t *);
int	sys_exit(struct lwp *, void *, register_t *);
int	sys_fork(struct lwp *, void *, register_t *);
int	ibcs2_sys_read(struct lwp *, void *, register_t *);
int	sys_write(struct lwp *, void *, register_t *);
int	ibcs2_sys_open(struct lwp *, void *, register_t *);
int	sys_close(struct lwp *, void *, register_t *);
int	ibcs2_sys_waitsys(struct lwp *, void *, register_t *);
int	ibcs2_sys_creat(struct lwp *, void *, register_t *);
int	sys_link(struct lwp *, void *, register_t *);
int	ibcs2_sys_unlink(struct lwp *, void *, register_t *);
int	ibcs2_sys_execv(struct lwp *, void *, register_t *);
int	ibcs2_sys_chdir(struct lwp *, void *, register_t *);
int	ibcs2_sys_time(struct lwp *, void *, register_t *);
int	ibcs2_sys_mknod(struct lwp *, void *, register_t *);
int	ibcs2_sys_chmod(struct lwp *, void *, register_t *);
int	ibcs2_sys_chown(struct lwp *, void *, register_t *);
int	sys_obreak(struct lwp *, void *, register_t *);
int	ibcs2_sys_stat(struct lwp *, void *, register_t *);
int	compat_43_sys_lseek(struct lwp *, void *, register_t *);
int	sys_getpid_with_ppid(struct lwp *, void *, register_t *);
int	ibcs2_sys_mount(struct lwp *, void *, register_t *);
int	ibcs2_sys_umount(struct lwp *, void *, register_t *);
int	ibcs2_sys_setuid(struct lwp *, void *, register_t *);
int	sys_getuid_with_euid(struct lwp *, void *, register_t *);
int	ibcs2_sys_stime(struct lwp *, void *, register_t *);
int	ibcs2_sys_alarm(struct lwp *, void *, register_t *);
int	ibcs2_sys_fstat(struct lwp *, void *, register_t *);
int	ibcs2_sys_pause(struct lwp *, void *, register_t *);
int	ibcs2_sys_utime(struct lwp *, void *, register_t *);
int	ibcs2_sys_gtty(struct lwp *, void *, register_t *);
int	ibcs2_sys_access(struct lwp *, void *, register_t *);
int	ibcs2_sys_nice(struct lwp *, void *, register_t *);
int	ibcs2_sys_statfs(struct lwp *, void *, register_t *);
int	sys_sync(struct lwp *, void *, register_t *);
int	ibcs2_sys_kill(struct lwp *, void *, register_t *);
int	ibcs2_sys_fstatfs(struct lwp *, void *, register_t *);
int	ibcs2_sys_pgrpsys(struct lwp *, void *, register_t *);
int	sys_dup(struct lwp *, void *, register_t *);
int	sys_pipe(struct lwp *, void *, register_t *);
int	ibcs2_sys_times(struct lwp *, void *, register_t *);
int	ibcs2_sys_plock(struct lwp *, void *, register_t *);
int	ibcs2_sys_setgid(struct lwp *, void *, register_t *);
int	sys_getgid_with_egid(struct lwp *, void *, register_t *);
int	ibcs2_sys_sigsys(struct lwp *, void *, register_t *);
#ifdef SYSVMSG
int	ibcs2_sys_msgsys(struct lwp *, void *, register_t *);
#else
#endif
int	ibcs2_sys_sysmachine(struct lwp *, void *, register_t *);
#ifdef SYSVSHM
int	ibcs2_sys_shmsys(struct lwp *, void *, register_t *);
#else
#endif
#ifdef SYSVSEM
int	ibcs2_sys_semsys(struct lwp *, void *, register_t *);
#else
#endif
int	ibcs2_sys_ioctl(struct lwp *, void *, register_t *);
int	ibcs2_sys_uadmin(struct lwp *, void *, register_t *);
int	ibcs2_sys_utssys(struct lwp *, void *, register_t *);
int	sys_fsync(struct lwp *, void *, register_t *);
int	ibcs2_sys_execve(struct lwp *, void *, register_t *);
int	sys_umask(struct lwp *, void *, register_t *);
int	sys_chroot(struct lwp *, void *, register_t *);
int	ibcs2_sys_fcntl(struct lwp *, void *, register_t *);
int	ibcs2_sys_ulimit(struct lwp *, void *, register_t *);
int	ibcs2_sys_rmdir(struct lwp *, void *, register_t *);
int	ibcs2_sys_mkdir(struct lwp *, void *, register_t *);
int	ibcs2_sys_getdents(struct lwp *, void *, register_t *);
int	ibcs2_sys_sysfs(struct lwp *, void *, register_t *);
int	ibcs2_sys_getmsg(struct lwp *, void *, register_t *);
int	ibcs2_sys_putmsg(struct lwp *, void *, register_t *);
int	sys_poll(struct lwp *, void *, register_t *);
int	ibcs2_sys_symlink(struct lwp *, void *, register_t *);
int	ibcs2_sys_lstat(struct lwp *, void *, register_t *);
int	ibcs2_sys_readlink(struct lwp *, void *, register_t *);
int	sys_fchmod(struct lwp *, void *, register_t *);
int	sys___posix_fchown(struct lwp *, void *, register_t *);
int	compat_16_sys___sigreturn14(struct lwp *, void *, register_t *);
int	ibcs2_sys_sigaltstack(struct lwp *, void *, register_t *);
int	ibcs2_sys_statvfs(struct lwp *, void *, register_t *);
int	ibcs2_sys_fstatvfs(struct lwp *, void *, register_t *);
int	ibcs2_sys_mmap(struct lwp *, void *, register_t *);
int	sys_mprotect(struct lwp *, void *, register_t *);
int	sys_munmap(struct lwp *, void *, register_t *);
int	sys_fchdir(struct lwp *, void *, register_t *);
int	sys_readv(struct lwp *, void *, register_t *);
int	sys_writev(struct lwp *, void *, register_t *);
int	ibcs2_sys_memcntl(struct lwp *, void *, register_t *);
int	ibcs2_sys_gettimeofday(struct lwp *, void *, register_t *);
int	ibcs2_sys_settimeofday(struct lwp *, void *, register_t *);
int	compat_43_sys_truncate(struct lwp *, void *, register_t *);
int	compat_43_sys_ftruncate(struct lwp *, void *, register_t *);
int	xenix_sys_locking(struct lwp *, void *, register_t *);
int	xenix_sys_rdchk(struct lwp *, void *, register_t *);
int	xenix_sys_chsize(struct lwp *, void *, register_t *);
int	xenix_sys_ftime(struct lwp *, void *, register_t *);
int	xenix_sys_nap(struct lwp *, void *, register_t *);
int	sys_select(struct lwp *, void *, register_t *);
int	ibcs2_sys_eaccess(struct lwp *, void *, register_t *);
int	ibcs2_sys_sigaction(struct lwp *, void *, register_t *);
int	ibcs2_sys_sigprocmask(struct lwp *, void *, register_t *);
int	ibcs2_sys_sigpending(struct lwp *, void *, register_t *);
int	ibcs2_sys_sigsuspend(struct lwp *, void *, register_t *);
int	ibcs2_sys_getgroups(struct lwp *, void *, register_t *);
int	ibcs2_sys_setgroups(struct lwp *, void *, register_t *);
int	ibcs2_sys_sysconf(struct lwp *, void *, register_t *);
int	ibcs2_sys_pathconf(struct lwp *, void *, register_t *);
int	ibcs2_sys_fpathconf(struct lwp *, void *, register_t *);
int	ibcs2_sys_rename(struct lwp *, void *, register_t *);
int	ibcs2_sys_scoinfo(struct lwp *, void *, register_t *);
#endif /* _IBCS2_SYS__SYSCALLARGS_H_ */
