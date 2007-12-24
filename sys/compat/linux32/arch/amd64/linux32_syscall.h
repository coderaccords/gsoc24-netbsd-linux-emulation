/* $NetBSD: linux32_syscall.h,v 1.26 2007/12/24 14:26:50 njoly Exp $ */

/*
 * System call numbers.
 *
 * DO NOT EDIT-- this file is automatically generated.
 * created from	NetBSD: syscalls.master,v 1.23 2007/12/24 14:25:30 njoly Exp
 */

#ifndef _LINUX32_SYS_SYSCALL_H_
#define	_LINUX32_SYS_SYSCALL_H_

#define	LINUX32_SYS_MAXSYSARGS	8

/* syscall: "syscall" ret: "int" args: */
#define	LINUX32_SYS_syscall	0

/* syscall: "netbsd32_exit" ret: "int" args: "int" */
#define	LINUX32_SYS_netbsd32_exit	1

/* syscall: "fork" ret: "int" args: */
#define	LINUX32_SYS_fork	2

/* syscall: "netbsd32_read" ret: "netbsd32_ssize_t" args: "int" "netbsd32_voidp" "netbsd32_size_t" */
#define	LINUX32_SYS_netbsd32_read	3

/* syscall: "netbsd32_write" ret: "netbsd32_ssize_t" args: "int" "const netbsd32_voidp" "netbsd32_size_t" */
#define	LINUX32_SYS_netbsd32_write	4

/* syscall: "open" ret: "int" args: "const netbsd32_charp" "int" "int" */
#define	LINUX32_SYS_open	5

/* syscall: "netbsd32_close" ret: "int" args: "int" */
#define	LINUX32_SYS_netbsd32_close	6

/* syscall: "waitpid" ret: "int" args: "int" "netbsd32_intp" "int" */
#define	LINUX32_SYS_waitpid	7

/* syscall: "creat" ret: "int" args: "const netbsd32_charp" "int" */
#define	LINUX32_SYS_creat	8

/* syscall: "netbsd32_link" ret: "int" args: "const netbsd32_charp" "const netbsd32_charp" */
#define	LINUX32_SYS_netbsd32_link	9

/* syscall: "unlink" ret: "int" args: "const netbsd32_charp" */
#define	LINUX32_SYS_unlink	10

/* syscall: "netbsd32_execve" ret: "int" args: "const netbsd32_charp" "netbsd32_charpp" "netbsd32_charpp" */
#define	LINUX32_SYS_netbsd32_execve	11

/* syscall: "netbsd32_chdir" ret: "int" args: "const netbsd32_charp" */
#define	LINUX32_SYS_netbsd32_chdir	12

/* syscall: "time" ret: "int" args: "linux32_timep_t" */
#define	LINUX32_SYS_time	13

/* syscall: "mknod" ret: "int" args: "const netbsd32_charp" "int" "int" */
#define	LINUX32_SYS_mknod	14

/* syscall: "netbsd32_chmod" ret: "int" args: "const netbsd32_charp" "int" */
#define	LINUX32_SYS_netbsd32_chmod	15

/* syscall: "lchown16" ret: "int" args: "const netbsd32_charp" "int" "int" */
#define	LINUX32_SYS_lchown16	16

/* syscall: "break" ret: "int" args: "netbsd32_charp" */
#define	LINUX32_SYS_break	17

				/* 18 is obsolete ostat */
/* syscall: "compat_43_netbsd32_olseek" ret: "netbsd32_long" args: "int" "netbsd32_long" "int" */
#define	LINUX32_SYS_compat_43_netbsd32_olseek	19

/* syscall: "getpid" ret: "pid_t" args: */
#define	LINUX32_SYS_getpid	20

/* syscall: "linux_setuid16" ret: "int" args: "uid_t" */
#define	LINUX32_SYS_linux_setuid16	23

/* syscall: "linux_getuid16" ret: "uid_t" args: */
#define	LINUX32_SYS_linux_getuid16	24

/* syscall: "stime" ret: "int" args: "linux32_timep_t" */
#define	LINUX32_SYS_stime	25

/* syscall: "alarm" ret: "int" args: "unsigned int" */
#define	LINUX32_SYS_alarm	27

				/* 28 is obsolete ofstat */
/* syscall: "pause" ret: "int" args: */
#define	LINUX32_SYS_pause	29

/* syscall: "utime" ret: "int" args: "const netbsd32_charp" "linux32_utimbufp_t" */
#define	LINUX32_SYS_utime	30

				/* 31 is obsolete stty */
				/* 32 is obsolete gtty */
/* syscall: "netbsd32_access" ret: "int" args: "const netbsd32_charp" "int" */
#define	LINUX32_SYS_netbsd32_access	33

/* syscall: "nice" ret: "int" args: "int" */
#define	LINUX32_SYS_nice	34

				/* 35 is obsolete ftime */
/* syscall: "sync" ret: "int" args: */
#define	LINUX32_SYS_sync	36

/* syscall: "kill" ret: "int" args: "int" "int" */
#define	LINUX32_SYS_kill	37

/* syscall: "rename" ret: "int" args: "const netbsd32_charp" "const netbsd32_charp" */
#define	LINUX32_SYS_rename	38

/* syscall: "netbsd32_mkdir" ret: "int" args: "const netbsd32_charp" "int" */
#define	LINUX32_SYS_netbsd32_mkdir	39

/* syscall: "netbsd32_rmdir" ret: "int" args: "const netbsd32_charp" */
#define	LINUX32_SYS_netbsd32_rmdir	40

/* syscall: "netbsd32_dup" ret: "int" args: "u_int" */
#define	LINUX32_SYS_netbsd32_dup	41

/* syscall: "pipe" ret: "int" args: "netbsd32_intp" */
#define	LINUX32_SYS_pipe	42

/* syscall: "times" ret: "int" args: "linux32_tmsp_t" */
#define	LINUX32_SYS_times	43

				/* 44 is obsolete prof */
/* syscall: "brk" ret: "int" args: "netbsd32_charp" */
#define	LINUX32_SYS_brk	45

/* syscall: "linux_setgid16" ret: "int" args: "gid_t" */
#define	LINUX32_SYS_linux_setgid16	46

/* syscall: "linux_getgid16" ret: "gid_t" args: */
#define	LINUX32_SYS_linux_getgid16	47

/* syscall: "signal" ret: "int" args: "int" "linux32_handler_t" */
#define	LINUX32_SYS_signal	48

/* syscall: "linux_geteuid16" ret: "uid_t" args: */
#define	LINUX32_SYS_linux_geteuid16	49

/* syscall: "linux_getegid16" ret: "gid_t" args: */
#define	LINUX32_SYS_linux_getegid16	50

/* syscall: "netbsd32_acct" ret: "int" args: "netbsd32_charp" */
#define	LINUX32_SYS_netbsd32_acct	51

				/* 52 is obsolete phys */
				/* 53 is obsolete lock */
/* syscall: "ioctl" ret: "int" args: "int" "netbsd32_u_long" "netbsd32_charp" */
#define	LINUX32_SYS_ioctl	54

/* syscall: "fcntl" ret: "int" args: "int" "int" "netbsd32_voidp" */
#define	LINUX32_SYS_fcntl	55

				/* 56 is obsolete mpx */
/* syscall: "netbsd32_setpgid" ret: "int" args: "int" "int" */
#define	LINUX32_SYS_netbsd32_setpgid	57

				/* 58 is obsolete ulimit */
/* syscall: "oldolduname" ret: "int" args: "linux32_oldold_utsnamep_t" */
#define	LINUX32_SYS_oldolduname	59

/* syscall: "netbsd32_umask" ret: "int" args: "int" */
#define	LINUX32_SYS_netbsd32_umask	60

/* syscall: "netbsd32_chroot" ret: "int" args: "netbsd32_charp" */
#define	LINUX32_SYS_netbsd32_chroot	61

/* syscall: "netbsd32_dup2" ret: "int" args: "u_int" "u_int" */
#define	LINUX32_SYS_netbsd32_dup2	63

/* syscall: "getppid" ret: "pid_t" args: */
#define	LINUX32_SYS_getppid	64

/* syscall: "getpgrp" ret: "int" args: */
#define	LINUX32_SYS_getpgrp	65

/* syscall: "setsid" ret: "int" args: */
#define	LINUX32_SYS_setsid	66

/* syscall: "setreuid16" ret: "int" args: "int" "int" */
#define	LINUX32_SYS_setreuid16	70

/* syscall: "setregid16" ret: "int" args: "int" "int" */
#define	LINUX32_SYS_setregid16	71

/* syscall: "compat_43_netbsd32_osethostname" ret: "int" args: "netbsd32_charp" "u_int" */
#define	LINUX32_SYS_compat_43_netbsd32_osethostname	74

/* syscall: "setrlimit" ret: "int" args: "u_int" "netbsd32_orlimitp_t" */
#define	LINUX32_SYS_setrlimit	75

/* syscall: "getrlimit" ret: "int" args: "u_int" "netbsd32_orlimitp_t" */
#define	LINUX32_SYS_getrlimit	76

/* syscall: "netbsd32_getrusage" ret: "int" args: "int" "netbsd32_rusagep_t" */
#define	LINUX32_SYS_netbsd32_getrusage	77

/* syscall: "gettimeofday" ret: "int" args: "netbsd32_timevalp_t" "netbsd32_timezonep_t" */
#define	LINUX32_SYS_gettimeofday	78

/* syscall: "settimeofday" ret: "int" args: "netbsd32_timevalp_t" "netbsd32_timezonep_t" */
#define	LINUX32_SYS_settimeofday	79

/* syscall: "getgroups16" ret: "int" args: "int" "linux32_gidp_t" */
#define	LINUX32_SYS_getgroups16	80

/* syscall: "setgroups16" ret: "int" args: "int" "linux32_gidp_t" */
#define	LINUX32_SYS_setgroups16	81

/* syscall: "oldselect" ret: "int" args: "linux32_oldselectp_t" */
#define	LINUX32_SYS_oldselect	82

/* syscall: "netbsd32_symlink" ret: "int" args: "netbsd32_charp" "const netbsd32_charp" */
#define	LINUX32_SYS_netbsd32_symlink	83

/* syscall: "compat_43_netbsd32_lstat43" ret: "int" args: "const netbsd32_charp" "netbsd32_stat43p_t" */
#define	LINUX32_SYS_compat_43_netbsd32_lstat43	84

/* syscall: "netbsd32_readlink" ret: "int" args: "const netbsd32_charp" "netbsd32_charp" "int" */
#define	LINUX32_SYS_netbsd32_readlink	85

/* syscall: "swapon" ret: "int" args: "netbsd32_charp" */
#define	LINUX32_SYS_swapon	87

/* syscall: "reboot" ret: "int" args: "int" "int" "int" "netbsd32_voidp" */
#define	LINUX32_SYS_reboot	88

/* syscall: "old_mmap" ret: "int" args: "linux32_oldmmapp" */
#define	LINUX32_SYS_old_mmap	90

/* syscall: "netbsd32_munmap" ret: "int" args: "netbsd32_caddr_t" "int" */
#define	LINUX32_SYS_netbsd32_munmap	91

/* syscall: "truncate" ret: "int" args: "const netbsd32_charp" "netbsd32_long" */
#define	LINUX32_SYS_truncate	92

/* syscall: "compat_43_netbsd32_oftruncate" ret: "int" args: "int" "netbsd32_long" */
#define	LINUX32_SYS_compat_43_netbsd32_oftruncate	93

/* syscall: "netbsd32_fchmod" ret: "int" args: "int" "int" */
#define	LINUX32_SYS_netbsd32_fchmod	94

/* syscall: "fchown16" ret: "int" args: "int" "int" "int" */
#define	LINUX32_SYS_fchown16	95

/* syscall: "getpriority" ret: "int" args: "int" "int" */
#define	LINUX32_SYS_getpriority	96

/* syscall: "setpriority" ret: "int" args: "int" "int" "int" */
#define	LINUX32_SYS_setpriority	97

/* syscall: "statfs" ret: "int" args: "const netbsd32_charp" "linux32_statfsp" */
#define	LINUX32_SYS_statfs	99

/* syscall: "socketcall" ret: "int" args: "int" "netbsd32_voidp" */
#define	LINUX32_SYS_socketcall	102

/* syscall: "netbsd32_setitimer" ret: "int" args: "u_int" "netbsd32_itimervalp_t" "netbsd32_itimervalp_t" */
#define	LINUX32_SYS_netbsd32_setitimer	104

/* syscall: "netbsd32_getitimer" ret: "int" args: "u_int" "netbsd32_itimervalp_t" */
#define	LINUX32_SYS_netbsd32_getitimer	105

/* syscall: "wait4" ret: "int" args: "int" "netbsd32_intp" "int" "netbsd32_rusagep_t" */
#define	LINUX32_SYS_wait4	114

/* syscall: "swapoff" ret: "int" args: "const netbsd32_charp" */
#define	LINUX32_SYS_swapoff	115

/* syscall: "sysinfo" ret: "int" args: "linux32_sysinfop_t" */
#define	LINUX32_SYS_sysinfo	116

/* syscall: "netbsd32_fsync" ret: "int" args: "int" */
#define	LINUX32_SYS_netbsd32_fsync	118

/* syscall: "sigreturn" ret: "int" args: "linux32_sigcontextp_t" */
#define	LINUX32_SYS_sigreturn	119

/* syscall: "clone" ret: "int" args: "int" "netbsd32_voidp" */
#define	LINUX32_SYS_clone	120

/* syscall: "uname" ret: "int" args: "linux32_utsnamep" */
#define	LINUX32_SYS_uname	122

/* syscall: "mprotect" ret: "int" args: "netbsd32_voidp" "netbsd32_size_t" "int" */
#define	LINUX32_SYS_mprotect	125

/* syscall: "netbsd32_getpgid" ret: "int" args: "pid_t" */
#define	LINUX32_SYS_netbsd32_getpgid	132

/* syscall: "netbsd32_fchdir" ret: "int" args: "int" */
#define	LINUX32_SYS_netbsd32_fchdir	133

/* syscall: "setfsuid16" ret: "int" args: "uid_t" */
#define	LINUX32_SYS_setfsuid16	138

/* syscall: "setfsgid16" ret: "int" args: "gid_t" */
#define	LINUX32_SYS_setfsgid16	139

/* syscall: "llseek" ret: "int" args: "int" "u_int32_t" "u_int32_t" "netbsd32_caddr_t" "int" */
#define	LINUX32_SYS_llseek	140

/* syscall: "getdents" ret: "int" args: "int" "linux32_direntp_t" "unsigned int" */
#define	LINUX32_SYS_getdents	141

/* syscall: "select" ret: "int" args: "int" "netbsd32_fd_setp_t" "netbsd32_fd_setp_t" "netbsd32_fd_setp_t" "netbsd32_timevalp_t" */
#define	LINUX32_SYS_select	142

/* syscall: "netbsd32_flock" ret: "int" args: "int" "int" */
#define	LINUX32_SYS_netbsd32_flock	143

/* syscall: "netbsd32_readv" ret: "int" args: "int" "const netbsd32_iovecp_t" "u_int" */
#define	LINUX32_SYS_netbsd32_readv	145

/* syscall: "netbsd32_writev" ret: "netbsd32_ssize_t" args: "int" "const netbsd32_iovecp_t" "int" */
#define	LINUX32_SYS_netbsd32_writev	146

/* syscall: "netbsd32_getsid" ret: "pid_t" args: "pid_t" */
#define	LINUX32_SYS_netbsd32_getsid	147

/* syscall: "fdatasync" ret: "int" args: "int" */
#define	LINUX32_SYS_fdatasync	148

/* syscall: "__sysctl" ret: "int" args: "linux32___sysctlp_t" */
#define	LINUX32_SYS___sysctl	149

/* syscall: "netbsd32_mlockall" ret: "int" args: "int" */
#define	LINUX32_SYS_netbsd32_mlockall	152

/* syscall: "munlockall" ret: "int" args: */
#define	LINUX32_SYS_munlockall	153

/* syscall: "sched_getparam" ret: "int" args: "pid_t" "linux32_sched_paramp_t" */
#define	LINUX32_SYS_sched_getparam	155

/* syscall: "sched_setscheduler" ret: "int" args: "pid_t" "int" "const linux32_sched_paramp_t" */
#define	LINUX32_SYS_sched_setscheduler	156

/* syscall: "sched_getscheduler" ret: "int" args: "pid_t" */
#define	LINUX32_SYS_sched_getscheduler	157

/* syscall: "sched_yield" ret: "int" args: */
#define	LINUX32_SYS_sched_yield	158

/* syscall: "netbsd32_nanosleep" ret: "int" args: "const netbsd32_timespecp_t rqtp netbsd32_timespecp_t" */
#define	LINUX32_SYS_netbsd32_nanosleep	162

/* syscall: "mremap" ret: "int" args: "netbsd32_voidp" "netbsd32_size_t" "netbsd32_size_t" "netbsd32_u_long" */
#define	LINUX32_SYS_mremap	163

/* syscall: "setresuid16" ret: "int" args: "uid_t" "uid_t" "uid_t" */
#define	LINUX32_SYS_setresuid16	164

/* syscall: "netbsd32_poll" ret: "int" args: "netbsd32_pollfdp_t" "u_int" "int" */
#define	LINUX32_SYS_netbsd32_poll	168

/* syscall: "setresgid16" ret: "int" args: "gid_t" "gid_t" "gid_t" */
#define	LINUX32_SYS_setresgid16	170

/* syscall: "rt_sigreturn" ret: "int" args: "linux32_ucontextp_t" */
#define	LINUX32_SYS_rt_sigreturn	173

/* syscall: "rt_sigaction" ret: "int" args: "int" "const linux32_sigactionp_t" "linux32_sigactionp_t" "netbsd32_size_t" */
#define	LINUX32_SYS_rt_sigaction	174

/* syscall: "rt_sigprocmask" ret: "int" args: "int" "const linux32_sigsetp_t" "linux32_sigsetp_t" "netbsd32_size_t" */
#define	LINUX32_SYS_rt_sigprocmask	175

/* syscall: "rt_sigsuspend" ret: "int" args: "linux32_sigsetp_t" "netbsd32_size_t" */
#define	LINUX32_SYS_rt_sigsuspend	179

/* syscall: "chown16" ret: "int" args: "const netbsd32_charp" "int" "int" */
#define	LINUX32_SYS_chown16	182

/* syscall: "netbsd32___getcwd" ret: "int" args: "netbsd32_charp" "netbsd32_size_t" */
#define	LINUX32_SYS_netbsd32___getcwd	183

/* syscall: "__vfork14" ret: "int" args: */
#define	LINUX32_SYS___vfork14	190

/* syscall: "ugetrlimit" ret: "int" args: "int" "netbsd32_orlimitp_t" */
#define	LINUX32_SYS_ugetrlimit	191

#define linux32_sys_mmap2_args linux32_sys_mmap_args
/* syscall: "mmap2" ret: "linux32_off_t" args: "netbsd32_u_long" "netbsd32_size_t" "int" "int" "int" "linux32_off_t" */
#define	LINUX32_SYS_mmap2	192

/* syscall: "stat64" ret: "int" args: "const netbsd32_charp" "linux32_stat64p" */
#define	LINUX32_SYS_stat64	195

/* syscall: "lstat64" ret: "int" args: "const netbsd32_charp" "linux32_stat64p" */
#define	LINUX32_SYS_lstat64	196

/* syscall: "fstat64" ret: "int" args: "int" "linux32_stat64p" */
#define	LINUX32_SYS_fstat64	197

/* syscall: "netbsd32___posix_lchown" ret: "int" args: "const netbsd32_charp" "int" "int" */
#define	LINUX32_SYS_netbsd32___posix_lchown	198

/* syscall: "getuid" ret: "uid_t" args: */
#define	LINUX32_SYS_getuid	199

/* syscall: "getgid" ret: "gid_t" args: */
#define	LINUX32_SYS_getgid	200

/* syscall: "geteuid" ret: "uid_t" args: */
#define	LINUX32_SYS_geteuid	201

/* syscall: "getegid" ret: "gid_t" args: */
#define	LINUX32_SYS_getegid	202

/* syscall: "netbsd32_setreuid" ret: "int" args: "uid_t" "uid_t" */
#define	LINUX32_SYS_netbsd32_setreuid	203

/* syscall: "netbsd32_setregid" ret: "int" args: "gid_t" "gid_t" */
#define	LINUX32_SYS_netbsd32_setregid	204

/* syscall: "netbsd32_getgroups" ret: "int" args: "int" "netbsd32_gid_tp" */
#define	LINUX32_SYS_netbsd32_getgroups	205

/* syscall: "netbsd32_setgroups" ret: "int" args: "int" "netbsd32_gid_tp" */
#define	LINUX32_SYS_netbsd32_setgroups	206

/* syscall: "netbsd32___posix_fchown" ret: "int" args: "int" "uid_t" "gid_t" */
#define	LINUX32_SYS_netbsd32___posix_fchown	207

/* syscall: "setresuid" ret: "int" args: "uid_t" "uid_t" "uid_t" */
#define	LINUX32_SYS_setresuid	208

/* syscall: "setresgid" ret: "int" args: "gid_t" "gid_t" "gid_t" */
#define	LINUX32_SYS_setresgid	210

/* syscall: "netbsd32___posix_chown" ret: "int" args: "const netbsd32_charp" "int" "int" */
#define	LINUX32_SYS_netbsd32___posix_chown	212

/* syscall: "netbsd32_setuid" ret: "int" args: "uid_t" */
#define	LINUX32_SYS_netbsd32_setuid	213

/* syscall: "netbsd32_setgid" ret: "int" args: "gid_t" */
#define	LINUX32_SYS_netbsd32_setgid	214

/* syscall: "setfsuid" ret: "int" args: "uid_t" */
#define	LINUX32_SYS_setfsuid	215

/* syscall: "setfsgid" ret: "int" args: "gid_t" */
#define	LINUX32_SYS_setfsgid	216

/* syscall: "netbsd32_madvise" ret: "int" args: "netbsd32_voidp" "netbsd32_size_t" "int" */
#define	LINUX32_SYS_netbsd32_madvise	219

/* syscall: "getdents64" ret: "int" args: "int" "linux32_dirent64p_t" "unsigned int" */
#define	LINUX32_SYS_getdents64	220

/* syscall: "fcntl64" ret: "int" args: "int" "int" "netbsd32_voidp" */
#define	LINUX32_SYS_fcntl64	221

/* syscall: "gettid" ret: "pid_t" args: */
#define	LINUX32_SYS_gettid	224

/* syscall: "exit_group" ret: "int" args: "int" */
#define	LINUX32_SYS_exit_group	252

#define	LINUX32_SYS_MAXSYSCALL	284
#define	LINUX32_SYS_NSYSENT	512
#endif /* _LINUX32_SYS_SYSCALL_H_ */
