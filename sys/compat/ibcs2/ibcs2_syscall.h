/* $NetBSD: ibcs2_syscall.h,v 1.36 2003/09/10 19:47:49 christos Exp $ */

/*
 * System call numbers.
 *
 * DO NOT EDIT-- this file is automatically generated.
 * created from	NetBSD: syscalls.master,v 1.28 2003/09/10 19:47:40 christos Exp 
 */

/* syscall: "syscall" ret: "int" args: */
#define	IBCS2_SYS_syscall	0

/* syscall: "exit" ret: "int" args: "int" */
#define	IBCS2_SYS_exit	1

/* syscall: "fork" ret: "int" args: */
#define	IBCS2_SYS_fork	2

/* syscall: "read" ret: "int" args: "int" "char *" "u_int" */
#define	IBCS2_SYS_read	3

/* syscall: "write" ret: "int" args: "int" "char *" "u_int" */
#define	IBCS2_SYS_write	4

/* syscall: "open" ret: "int" args: "const char *" "int" "int" */
#define	IBCS2_SYS_open	5

/* syscall: "close" ret: "int" args: "int" */
#define	IBCS2_SYS_close	6

/* syscall: "waitsys" ret: "int" args: "int" "int" "int" */
#define	IBCS2_SYS_waitsys	7

/* syscall: "creat" ret: "int" args: "const char *" "int" */
#define	IBCS2_SYS_creat	8

/* syscall: "link" ret: "int" args: "char *" "char *" */
#define	IBCS2_SYS_link	9

/* syscall: "unlink" ret: "int" args: "const char *" */
#define	IBCS2_SYS_unlink	10

/* syscall: "execv" ret: "int" args: "const char *" "char **" */
#define	IBCS2_SYS_execv	11

/* syscall: "chdir" ret: "int" args: "const char *" */
#define	IBCS2_SYS_chdir	12

/* syscall: "time" ret: "int" args: "ibcs2_time_t *" */
#define	IBCS2_SYS_time	13

/* syscall: "mknod" ret: "int" args: "const char *" "int" "int" */
#define	IBCS2_SYS_mknod	14

/* syscall: "chmod" ret: "int" args: "const char *" "int" */
#define	IBCS2_SYS_chmod	15

/* syscall: "chown" ret: "int" args: "const char *" "int" "int" */
#define	IBCS2_SYS_chown	16

/* syscall: "obreak" ret: "int" args: "caddr_t" */
#define	IBCS2_SYS_obreak	17

/* syscall: "stat" ret: "int" args: "const char *" "struct ibcs2_stat *" */
#define	IBCS2_SYS_stat	18

/* syscall: "lseek" ret: "long" args: "int" "long" "int" */
#define	IBCS2_SYS_lseek	19

/* syscall: "getpid_with_ppid" ret: "pid_t" args: */
#define	IBCS2_SYS_getpid_with_ppid	20

/* syscall: "mount" ret: "int" args: "char *" "char *" "int" "int" "char *" "int" */
#define	IBCS2_SYS_mount	21

/* syscall: "umount" ret: "int" args: "char *" */
#define	IBCS2_SYS_umount	22

/* syscall: "setuid" ret: "int" args: "int" */
#define	IBCS2_SYS_setuid	23

/* syscall: "getuid_with_euid" ret: "uid_t" args: */
#define	IBCS2_SYS_getuid_with_euid	24

/* syscall: "stime" ret: "int" args: "long *" */
#define	IBCS2_SYS_stime	25

/* syscall: "alarm" ret: "int" args: "unsigned" */
#define	IBCS2_SYS_alarm	27

/* syscall: "fstat" ret: "int" args: "int" "struct ibcs2_stat *" */
#define	IBCS2_SYS_fstat	28

/* syscall: "pause" ret: "int" args: */
#define	IBCS2_SYS_pause	29

/* syscall: "utime" ret: "int" args: "const char *" "struct ibcs2_utimbuf *" */
#define	IBCS2_SYS_utime	30

/* syscall: "gtty" ret: "int" args: "int" "struct ibcs2_sgttyb *" */
#define	IBCS2_SYS_gtty	32

/* syscall: "access" ret: "int" args: "const char *" "int" */
#define	IBCS2_SYS_access	33

/* syscall: "nice" ret: "int" args: "int" */
#define	IBCS2_SYS_nice	34

/* syscall: "statfs" ret: "int" args: "const char *" "struct ibcs2_statfs *" "int" "int" */
#define	IBCS2_SYS_statfs	35

/* syscall: "sync" ret: "int" args: */
#define	IBCS2_SYS_sync	36

/* syscall: "kill" ret: "int" args: "int" "int" */
#define	IBCS2_SYS_kill	37

/* syscall: "fstatfs" ret: "int" args: "int" "struct ibcs2_statfs *" "int" "int" */
#define	IBCS2_SYS_fstatfs	38

/* syscall: "pgrpsys" ret: "int" args: "int" "caddr_t" "int" "int" */
#define	IBCS2_SYS_pgrpsys	39

/* syscall: "dup" ret: "int" args: "u_int" */
#define	IBCS2_SYS_dup	41

/* syscall: "pipe" ret: "int" args: */
#define	IBCS2_SYS_pipe	42

/* syscall: "times" ret: "int" args: "struct tms *" */
#define	IBCS2_SYS_times	43

/* syscall: "plock" ret: "int" args: "int" */
#define	IBCS2_SYS_plock	45

/* syscall: "setgid" ret: "int" args: "int" */
#define	IBCS2_SYS_setgid	46

/* syscall: "getgid_with_egid" ret: "gid_t" args: */
#define	IBCS2_SYS_getgid_with_egid	47

/* syscall: "sigsys" ret: "int" args: "int" "ibcs2_sig_t" */
#define	IBCS2_SYS_sigsys	48

/* syscall: "msgsys" ret: "int" args: "int" "int" "int" "int" "int" "int" */
#define	IBCS2_SYS_msgsys	49

/* syscall: "sysmachine" ret: "int" args: "int" "int" */
#define	IBCS2_SYS_sysmachine	50

/* syscall: "shmsys" ret: "int" args: "int" "int" "int" "int" */
#define	IBCS2_SYS_shmsys	52

/* syscall: "semsys" ret: "int" args: "int" "int" "int" "int" "int" */
#define	IBCS2_SYS_semsys	53

/* syscall: "ioctl" ret: "int" args: "int" "int" "caddr_t" */
#define	IBCS2_SYS_ioctl	54

/* syscall: "uadmin" ret: "int" args: "int" "int" "caddr_t" */
#define	IBCS2_SYS_uadmin	55

/* syscall: "utssys" ret: "int" args: "int" "int" "int" */
#define	IBCS2_SYS_utssys	57

/* syscall: "fsync" ret: "int" args: "int" */
#define	IBCS2_SYS_fsync	58

/* syscall: "execve" ret: "int" args: "const char *" "char **" "char **" */
#define	IBCS2_SYS_execve	59

/* syscall: "umask" ret: "int" args: "int" */
#define	IBCS2_SYS_umask	60

/* syscall: "chroot" ret: "int" args: "char *" */
#define	IBCS2_SYS_chroot	61

/* syscall: "fcntl" ret: "int" args: "int" "int" "char *" */
#define	IBCS2_SYS_fcntl	62

/* syscall: "ulimit" ret: "long" args: "int" "int" */
#define	IBCS2_SYS_ulimit	63

				/* 70 is obsolete rfs_advfs */
				/* 71 is obsolete rfs_unadvfs */
				/* 72 is obsolete rfs_rmount */
				/* 73 is obsolete rfs_rumount */
				/* 74 is obsolete rfs_rfstart */
				/* 75 is obsolete rfs_sigret */
				/* 76 is obsolete rfs_rdebug */
				/* 77 is obsolete rfs_rfstop */
/* syscall: "rmdir" ret: "int" args: "const char *" */
#define	IBCS2_SYS_rmdir	79

/* syscall: "mkdir" ret: "int" args: "const char *" "int" */
#define	IBCS2_SYS_mkdir	80

/* syscall: "getdents" ret: "int" args: "int" "char *" "int" */
#define	IBCS2_SYS_getdents	81

/* syscall: "sysfs" ret: "int" args: "int" "caddr_t" "char *" */
#define	IBCS2_SYS_sysfs	84

/* syscall: "getmsg" ret: "int" args: "int" "struct ibcs2_stropts *" "struct ibcs2_stropts *" "int *" */
#define	IBCS2_SYS_getmsg	85

/* syscall: "putmsg" ret: "int" args: "int" "struct ibcs2_stropts *" "struct ibcs2_stropts *" "int" */
#define	IBCS2_SYS_putmsg	86

/* syscall: "poll" ret: "int" args: "struct pollfd *" "u_int" "int" */
#define	IBCS2_SYS_poll	87

/* syscall: "symlink" ret: "int" args: "const char *" "const char *" */
#define	IBCS2_SYS_symlink	90

/* syscall: "lstat" ret: "int" args: "const char *" "struct ibcs2_stat *" */
#define	IBCS2_SYS_lstat	91

/* syscall: "readlink" ret: "int" args: "const char *" "char *" "int" */
#define	IBCS2_SYS_readlink	92

/* syscall: "fchmod" ret: "int" args: "int" "int" */
#define	IBCS2_SYS_fchmod	93

/* syscall: "fchown" ret: "int" args: "int" "int" "int" */
#define	IBCS2_SYS_fchown	94

/* syscall: "sigreturn" ret: "int" args: "struct sigcontext *" */
#define	IBCS2_SYS_sigreturn	96

/* syscall: "sigaltstack" ret: "int" args: "const struct ibcs2_sigaltstack *" "struct ibcs2_sigaltstack *" */
#define	IBCS2_SYS_sigaltstack	97

/* syscall: "statvfs" ret: "int" args: "const char *" "struct ibcs2_statvfs *" */
#define	IBCS2_SYS_statvfs	103

/* syscall: "fstatvfs" ret: "int" args: "int" "struct ibcs2_statvfs *" */
#define	IBCS2_SYS_fstatvfs	104

/* syscall: "mmap" ret: "ibcs2_caddr_t" args: "ibcs2_caddr_t" "ibcs2_size_t" "int" "int" "int" "ibcs2_off_t" */
#define	IBCS2_SYS_mmap	115

/* syscall: "mprotect" ret: "int" args: "void *" "int" "int" */
#define	IBCS2_SYS_mprotect	116

/* syscall: "munmap" ret: "int" args: "void *" "int" */
#define	IBCS2_SYS_munmap	117

/* syscall: "fchdir" ret: "int" args: "int" */
#define	IBCS2_SYS_fchdir	120

/* syscall: "readv" ret: "int" args: "int" "struct iovec *" "u_int" */
#define	IBCS2_SYS_readv	121

/* syscall: "writev" ret: "int" args: "int" "struct iovec *" "u_int" */
#define	IBCS2_SYS_writev	122

/* syscall: "memcntl" ret: "int" args: "ibcs2_caddr_t" "ibcs2_size_t" "int" "ibcs2_caddr_t" "int" "int" */
#define	IBCS2_SYS_memcntl	131

/* syscall: "gettimeofday" ret: "int" args: "struct timeval *" */
#define	IBCS2_SYS_gettimeofday	171

/* syscall: "settimeofday" ret: "int" args: "struct timeval *" */
#define	IBCS2_SYS_settimeofday	172

/* syscall: "truncate" ret: "int" args: "char *" "long" */
#define	IBCS2_SYS_truncate	191

/* syscall: "ftruncate" ret: "int" args: "int" "long" */
#define	IBCS2_SYS_ftruncate	192

/* syscall: "locking" ret: "int" args: "int" "int" "int" */
#define	IBCS2_SYS_locking	201

/* syscall: "rdchk" ret: "int" args: "int" */
#define	IBCS2_SYS_rdchk	207

/* syscall: "chsize" ret: "int" args: "int" "long" */
#define	IBCS2_SYS_chsize	210

/* syscall: "ftime" ret: "int" args: "struct xenix_timeb *" */
#define	IBCS2_SYS_ftime	211

/* syscall: "nap" ret: "int" args: "long" */
#define	IBCS2_SYS_nap	212

/* syscall: "select" ret: "int" args: "u_int" "fd_set *" "fd_set *" "fd_set *" "struct timeval *" */
#define	IBCS2_SYS_select	236

/* syscall: "eaccess" ret: "int" args: "const char *" "int" */
#define	IBCS2_SYS_eaccess	237

/* syscall: "sigaction" ret: "int" args: "int" "const struct ibcs2_sigaction *" "struct ibcs2_sigaction *" */
#define	IBCS2_SYS_sigaction	239

/* syscall: "sigprocmask" ret: "int" args: "int" "const ibcs2_sigset_t *" "ibcs2_sigset_t *" */
#define	IBCS2_SYS_sigprocmask	240

/* syscall: "sigpending" ret: "int" args: "ibcs2_sigset_t *" */
#define	IBCS2_SYS_sigpending	241

/* syscall: "sigsuspend" ret: "int" args: "const ibcs2_sigset_t *" */
#define	IBCS2_SYS_sigsuspend	242

/* syscall: "getgroups" ret: "int" args: "int" "ibcs2_gid_t *" */
#define	IBCS2_SYS_getgroups	243

/* syscall: "setgroups" ret: "int" args: "int" "ibcs2_gid_t *" */
#define	IBCS2_SYS_setgroups	244

/* syscall: "sysconf" ret: "int" args: "int" */
#define	IBCS2_SYS_sysconf	245

/* syscall: "pathconf" ret: "int" args: "char *" "int" */
#define	IBCS2_SYS_pathconf	246

/* syscall: "fpathconf" ret: "int" args: "int" "int" */
#define	IBCS2_SYS_fpathconf	247

/* syscall: "rename" ret: "int" args: "const char *" "const char *" */
#define	IBCS2_SYS_rename	248

/* syscall: "scoinfo" ret: "int" args: "struct scoutsname *" "int" */
#define	IBCS2_SYS_scoinfo	250

#define	IBCS2_SYS_MAXSYSCALL	260
#define	IBCS2_SYS_NSYSENT	512
