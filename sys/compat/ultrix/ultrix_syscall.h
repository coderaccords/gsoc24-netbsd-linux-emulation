/* $NetBSD: ultrix_syscall.h,v 1.50 2005/12/11 12:20:30 christos Exp $ */

/*
 * System call numbers.
 *
 * DO NOT EDIT-- this file is automatically generated.
 * created from	NetBSD: syscalls.master,v 1.39 2005/02/26 23:10:22 perry Exp
 */

/* syscall: "syscall" ret: "int" args: */
#define	ULTRIX_SYS_syscall	0

/* syscall: "exit" ret: "int" args: "int" */
#define	ULTRIX_SYS_exit	1

/* syscall: "fork" ret: "int" args: */
#define	ULTRIX_SYS_fork	2

/* syscall: "read" ret: "int" args: "int" "char *" "u_int" */
#define	ULTRIX_SYS_read	3

/* syscall: "write" ret: "int" args: "int" "char *" "u_int" */
#define	ULTRIX_SYS_write	4

/* syscall: "open" ret: "int" args: "const char *" "int" "int" */
#define	ULTRIX_SYS_open	5

/* syscall: "close" ret: "int" args: "int" */
#define	ULTRIX_SYS_close	6

/* syscall: "owait" ret: "int" args: */
#define	ULTRIX_SYS_owait	7

/* syscall: "creat" ret: "int" args: "const char *" "int" */
#define	ULTRIX_SYS_creat	8

/* syscall: "link" ret: "int" args: "char *" "char *" */
#define	ULTRIX_SYS_link	9

/* syscall: "unlink" ret: "int" args: "char *" */
#define	ULTRIX_SYS_unlink	10

/* syscall: "execv" ret: "int" args: "const char *" "char **" */
#define	ULTRIX_SYS_execv	11

/* syscall: "chdir" ret: "int" args: "char *" */
#define	ULTRIX_SYS_chdir	12

				/* 13 is obsolete time */
/* syscall: "mknod" ret: "int" args: "const char *" "int" "int" */
#define	ULTRIX_SYS_mknod	14

/* syscall: "chmod" ret: "int" args: "char *" "int" */
#define	ULTRIX_SYS_chmod	15

/* syscall: "__posix_chown" ret: "int" args: "char *" "int" "int" */
#define	ULTRIX_SYS___posix_chown	16

/* syscall: "break" ret: "int" args: "char *" */
#define	ULTRIX_SYS_break	17

				/* 18 is obsolete stat */
/* syscall: "lseek" ret: "long" args: "int" "long" "int" */
#define	ULTRIX_SYS_lseek	19

/* syscall: "getpid" ret: "pid_t" args: */
#define	ULTRIX_SYS_getpid	20

/* syscall: "mount" ret: "int" args: "char *" "char *" "int" "int" "caddr_t" */
#define	ULTRIX_SYS_mount	21

				/* 22 is obsolete sysV_unmount */
/* syscall: "setuid" ret: "int" args: "uid_t" */
#define	ULTRIX_SYS_setuid	23

/* syscall: "getuid" ret: "uid_t" args: */
#define	ULTRIX_SYS_getuid	24

				/* 25 is obsolete v7 stime */
				/* 26 is obsolete v7 ptrace */
				/* 27 is obsolete v7 alarm */
				/* 28 is obsolete v7 fstat */
				/* 29 is obsolete v7 pause */
				/* 30 is obsolete v7 utime */
				/* 31 is obsolete v7 stty */
				/* 32 is obsolete v7 gtty */
/* syscall: "access" ret: "int" args: "const char *" "int" */
#define	ULTRIX_SYS_access	33

				/* 34 is obsolete v7 nice */
				/* 35 is obsolete v7 ftime */
/* syscall: "sync" ret: "int" args: */
#define	ULTRIX_SYS_sync	36

/* syscall: "kill" ret: "int" args: "int" "int" */
#define	ULTRIX_SYS_kill	37

/* syscall: "stat43" ret: "int" args: "const char *" "struct stat43 *" */
#define	ULTRIX_SYS_stat43	38

				/* 39 is obsolete v7 setpgrp */
/* syscall: "lstat43" ret: "int" args: "const char *" "struct stat43 *" */
#define	ULTRIX_SYS_lstat43	40

/* syscall: "dup" ret: "int" args: "u_int" */
#define	ULTRIX_SYS_dup	41

/* syscall: "pipe" ret: "int" args: */
#define	ULTRIX_SYS_pipe	42

				/* 43 is obsolete v7 times */
/* syscall: "profil" ret: "int" args: "caddr_t" "u_int" "u_int" "u_int" */
#define	ULTRIX_SYS_profil	44

				/* 46 is obsolete v7 setgid */
/* syscall: "getgid" ret: "gid_t" args: */
#define	ULTRIX_SYS_getgid	47

/* syscall: "acct" ret: "int" args: "char *" */
#define	ULTRIX_SYS_acct	51

/* syscall: "ioctl" ret: "int" args: "int" "u_long" "caddr_t" */
#define	ULTRIX_SYS_ioctl	54

/* syscall: "reboot" ret: "int" args: "int" */
#define	ULTRIX_SYS_reboot	55

/* syscall: "symlink" ret: "int" args: "char *" "char *" */
#define	ULTRIX_SYS_symlink	57

/* syscall: "readlink" ret: "int" args: "char *" "char *" "int" */
#define	ULTRIX_SYS_readlink	58

/* syscall: "execve" ret: "int" args: "const char *" "char **" "char **" */
#define	ULTRIX_SYS_execve	59

/* syscall: "umask" ret: "int" args: "int" */
#define	ULTRIX_SYS_umask	60

/* syscall: "chroot" ret: "int" args: "char *" */
#define	ULTRIX_SYS_chroot	61

/* syscall: "fstat" ret: "int" args: "int" "struct stat43 *" */
#define	ULTRIX_SYS_fstat	62

/* syscall: "getpagesize" ret: "int" args: */
#define	ULTRIX_SYS_getpagesize	64

/* syscall: "vfork" ret: "int" args: */
#define	ULTRIX_SYS_vfork	66

				/* 67 is obsolete vread */
				/* 68 is obsolete vwrite */
/* syscall: "sbrk" ret: "int" args: "intptr_t" */
#define	ULTRIX_SYS_sbrk	69

/* syscall: "sstk" ret: "int" args: "int" */
#define	ULTRIX_SYS_sstk	70

/* syscall: "mmap" ret: "int" args: "caddr_t" "size_t" "int" "u_int" "int" "long" */
#define	ULTRIX_SYS_mmap	71

/* syscall: "vadvise" ret: "int" args: "int" */
#define	ULTRIX_SYS_vadvise	72

/* syscall: "munmap" ret: "int" args: "caddr_t" "size_t" */
#define	ULTRIX_SYS_munmap	73

/* syscall: "mprotect" ret: "int" args: "caddr_t" "size_t" "int" */
#define	ULTRIX_SYS_mprotect	74

/* syscall: "madvise" ret: "int" args: "caddr_t" "size_t" "int" */
#define	ULTRIX_SYS_madvise	75

/* syscall: "vhangup" ret: "int" args: */
#define	ULTRIX_SYS_vhangup	76

/* syscall: "mincore" ret: "int" args: "caddr_t" "int" "char *" */
#define	ULTRIX_SYS_mincore	78

/* syscall: "getgroups" ret: "int" args: "u_int" "gid_t *" */
#define	ULTRIX_SYS_getgroups	79

/* syscall: "setgroups" ret: "int" args: "u_int" "gid_t *" */
#define	ULTRIX_SYS_setgroups	80

/* syscall: "getpgrp" ret: "int" args: */
#define	ULTRIX_SYS_getpgrp	81

/* syscall: "setpgrp" ret: "int" args: "int" "int" */
#define	ULTRIX_SYS_setpgrp	82

/* syscall: "setitimer" ret: "int" args: "u_int" "struct itimerval *" "struct itimerval *" */
#define	ULTRIX_SYS_setitimer	83

/* syscall: "wait3" ret: "int" args: "int *" "int" "struct rusage *" */
#define	ULTRIX_SYS_wait3	84

/* syscall: "swapon" ret: "int" args: "char *" */
#define	ULTRIX_SYS_swapon	85

/* syscall: "getitimer" ret: "int" args: "u_int" "struct itimerval *" */
#define	ULTRIX_SYS_getitimer	86

/* syscall: "gethostname" ret: "int" args: "char *" "u_int" */
#define	ULTRIX_SYS_gethostname	87

/* syscall: "sethostname" ret: "int" args: "char *" "u_int" */
#define	ULTRIX_SYS_sethostname	88

/* syscall: "getdtablesize" ret: "int" args: */
#define	ULTRIX_SYS_getdtablesize	89

/* syscall: "dup2" ret: "int" args: "u_int" "u_int" */
#define	ULTRIX_SYS_dup2	90

/* syscall: "fcntl" ret: "int" args: "int" "int" "void *" */
#define	ULTRIX_SYS_fcntl	92

/* syscall: "select" ret: "int" args: "u_int" "fd_set *" "fd_set *" "fd_set *" "struct timeval *" */
#define	ULTRIX_SYS_select	93

/* syscall: "fsync" ret: "int" args: "int" */
#define	ULTRIX_SYS_fsync	95

/* syscall: "setpriority" ret: "int" args: "int" "int" "int" */
#define	ULTRIX_SYS_setpriority	96

/* syscall: "socket" ret: "int" args: "int" "int" "int" */
#define	ULTRIX_SYS_socket	97

/* syscall: "connect" ret: "int" args: "int" "caddr_t" "int" */
#define	ULTRIX_SYS_connect	98

/* syscall: "accept" ret: "int" args: "int" "caddr_t" "int *" */
#define	ULTRIX_SYS_accept	99

/* syscall: "getpriority" ret: "int" args: "int" "int" */
#define	ULTRIX_SYS_getpriority	100

/* syscall: "send" ret: "int" args: "int" "caddr_t" "int" "int" */
#define	ULTRIX_SYS_send	101

/* syscall: "recv" ret: "int" args: "int" "caddr_t" "int" "int" */
#define	ULTRIX_SYS_recv	102

/* syscall: "sigreturn" ret: "int" args: "struct sigcontext *" */
#define	ULTRIX_SYS_sigreturn	103

/* syscall: "bind" ret: "int" args: "int" "caddr_t" "int" */
#define	ULTRIX_SYS_bind	104

/* syscall: "setsockopt" ret: "int" args: "int" "int" "int" "caddr_t" "int" */
#define	ULTRIX_SYS_setsockopt	105

/* syscall: "listen" ret: "int" args: "int" "int" */
#define	ULTRIX_SYS_listen	106

/* syscall: "sigvec" ret: "int" args: "int" "struct sigvec *" "struct sigvec *" */
#define	ULTRIX_SYS_sigvec	108

/* syscall: "sigblock" ret: "int" args: "int" */
#define	ULTRIX_SYS_sigblock	109

/* syscall: "sigsetmask" ret: "int" args: "int" */
#define	ULTRIX_SYS_sigsetmask	110

/* syscall: "sigsuspend" ret: "int" args: "int" */
#define	ULTRIX_SYS_sigsuspend	111

/* syscall: "sigstack" ret: "int" args: "struct sigstack *" "struct sigstack *" */
#define	ULTRIX_SYS_sigstack	112

/* syscall: "recvmsg" ret: "int" args: "int" "struct omsghdr *" "int" */
#define	ULTRIX_SYS_recvmsg	113

/* syscall: "sendmsg" ret: "int" args: "int" "caddr_t" "int" */
#define	ULTRIX_SYS_sendmsg	114

				/* 115 is obsolete vtrace */
/* syscall: "gettimeofday" ret: "int" args: "struct timeval *" "struct timezone *" */
#define	ULTRIX_SYS_gettimeofday	116

/* syscall: "getrusage" ret: "int" args: "int" "struct rusage *" */
#define	ULTRIX_SYS_getrusage	117

/* syscall: "getsockopt" ret: "int" args: "int" "int" "int" "caddr_t" "int *" */
#define	ULTRIX_SYS_getsockopt	118

/* syscall: "readv" ret: "int" args: "int" "struct iovec *" "u_int" */
#define	ULTRIX_SYS_readv	120

/* syscall: "writev" ret: "int" args: "int" "struct iovec *" "u_int" */
#define	ULTRIX_SYS_writev	121

/* syscall: "settimeofday" ret: "int" args: "struct timeval *" "struct timezone *" */
#define	ULTRIX_SYS_settimeofday	122

/* syscall: "__posix_fchown" ret: "int" args: "int" "int" "int" */
#define	ULTRIX_SYS___posix_fchown	123

/* syscall: "fchmod" ret: "int" args: "int" "int" */
#define	ULTRIX_SYS_fchmod	124

/* syscall: "recvfrom" ret: "int" args: "int" "caddr_t" "size_t" "int" "caddr_t" "int *" */
#define	ULTRIX_SYS_recvfrom	125

/* syscall: "setreuid" ret: "int" args: "int" "int" */
#define	ULTRIX_SYS_setreuid	126

/* syscall: "setregid" ret: "int" args: "int" "int" */
#define	ULTRIX_SYS_setregid	127

/* syscall: "rename" ret: "int" args: "char *" "char *" */
#define	ULTRIX_SYS_rename	128

/* syscall: "truncate" ret: "int" args: "char *" "long" */
#define	ULTRIX_SYS_truncate	129

/* syscall: "ftruncate" ret: "int" args: "int" "long" */
#define	ULTRIX_SYS_ftruncate	130

/* syscall: "flock" ret: "int" args: "int" "int" */
#define	ULTRIX_SYS_flock	131

/* syscall: "sendto" ret: "int" args: "int" "caddr_t" "size_t" "int" "caddr_t" "int" */
#define	ULTRIX_SYS_sendto	133

/* syscall: "shutdown" ret: "int" args: "int" "int" */
#define	ULTRIX_SYS_shutdown	134

/* syscall: "socketpair" ret: "int" args: "int" "int" "int" "int *" */
#define	ULTRIX_SYS_socketpair	135

/* syscall: "mkdir" ret: "int" args: "char *" "int" */
#define	ULTRIX_SYS_mkdir	136

/* syscall: "rmdir" ret: "int" args: "char *" */
#define	ULTRIX_SYS_rmdir	137

/* syscall: "utimes" ret: "int" args: "char *" "struct timeval *" */
#define	ULTRIX_SYS_utimes	138

/* syscall: "sigcleanup" ret: "int" args: "struct sigcontext *" */
#define	ULTRIX_SYS_sigcleanup	139

/* syscall: "adjtime" ret: "int" args: "struct timeval *" "struct timeval *" */
#define	ULTRIX_SYS_adjtime	140

/* syscall: "getpeername" ret: "int" args: "int" "caddr_t" "int *" */
#define	ULTRIX_SYS_getpeername	141

/* syscall: "gethostid" ret: "int" args: */
#define	ULTRIX_SYS_gethostid	142

/* syscall: "getrlimit" ret: "int" args: "u_int" "struct orlimit *" */
#define	ULTRIX_SYS_getrlimit	144

/* syscall: "setrlimit" ret: "int" args: "u_int" "struct orlimit *" */
#define	ULTRIX_SYS_setrlimit	145

/* syscall: "killpg" ret: "int" args: "int" "int" */
#define	ULTRIX_SYS_killpg	146

/* syscall: "getsockname" ret: "int" args: "int" "caddr_t" "int *" */
#define	ULTRIX_SYS_getsockname	150

#ifdef __mips
/* syscall: "cacheflush" ret: "int" args: "char *" "int" "int" */
#define	ULTRIX_SYS_cacheflush	152

/* syscall: "cachectl" ret: "int" args: "char *" "int" "int" */
#define	ULTRIX_SYS_cachectl	153

#else	/* !mips */
#endif	/* !mips */
#ifdef NFSSERVER
/* syscall: "nfssvc" ret: "int" args: "int" */
#define	ULTRIX_SYS_nfssvc	158

#else
#endif
/* syscall: "getdirentries" ret: "int" args: "int" "char *" "u_int" "long *" */
#define	ULTRIX_SYS_getdirentries	159

/* syscall: "statfs" ret: "int" args: "const char *" "struct ultrix_statfs *" */
#define	ULTRIX_SYS_statfs	160

/* syscall: "fstatfs" ret: "int" args: "int" "struct ultrix_statfs *" */
#define	ULTRIX_SYS_fstatfs	161

#ifdef NFS
/* syscall: "async_daemon" ret: "int" args: */
#define	ULTRIX_SYS_async_daemon	163

/* syscall: "getfh" ret: "int" args: "char *" "fhandle_t *" */
#define	ULTRIX_SYS_getfh	164

#else
#endif
/* syscall: "getdomainname" ret: "int" args: "char *" "int" */
#define	ULTRIX_SYS_getdomainname	165

/* syscall: "setdomainname" ret: "int" args: "char *" "int" */
#define	ULTRIX_SYS_setdomainname	166

/* syscall: "quotactl" ret: "int" args: "int" "char *" "int" "caddr_t" */
#define	ULTRIX_SYS_quotactl	168

/* syscall: "exportfs" ret: "int" args: "char *" "char *" */
#define	ULTRIX_SYS_exportfs	169

/* syscall: "uname" ret: "int" args: "struct ultrix_utsname *" */
#define	ULTRIX_SYS_uname	179

/* syscall: "shmsys" ret: "int" args: "u_int" "u_int" "u_int" "u_int" */
#define	ULTRIX_SYS_shmsys	180

/* syscall: "ustat" ret: "int" args: "int" "struct ultrix_ustat *" */
#define	ULTRIX_SYS_ustat	183

/* syscall: "getmnt" ret: "int" args: "int *" "struct ultrix_fs_data *" "int" "int" "char *" */
#define	ULTRIX_SYS_getmnt	184

/* syscall: "sigpending" ret: "int" args: "int *" */
#define	ULTRIX_SYS_sigpending	187

/* syscall: "setsid" ret: "int" args: */
#define	ULTRIX_SYS_setsid	188

/* syscall: "waitpid" ret: "int" args: "int" "int *" "int" */
#define	ULTRIX_SYS_waitpid	189

/* syscall: "getsysinfo" ret: "int" args: "unsigned" "char *" "unsigned" "int *" "char *" */
#define	ULTRIX_SYS_getsysinfo	256

/* syscall: "setsysinfo" ret: "int" args: "unsigned" "char *" "unsigned" "unsigned" "unsigned" */
#define	ULTRIX_SYS_setsysinfo	257

#define	ULTRIX_SYS_MAXSYSCALL	258
#define	ULTRIX_SYS_NSYSENT	512
