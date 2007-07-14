/* $NetBSD: syscall.h,v 1.185 2007/07/14 15:41:30 dsl Exp $ */

/*
 * System call numbers.
 *
 * DO NOT EDIT-- this file is automatically generated.
 * created from	NetBSD: syscalls.master,v 1.170 2007/07/14 15:38:40 dsl Exp
 */

#ifndef _SYS_SYSCALL_H_
#define	_SYS_SYSCALL_H_

/* syscall: "syscall" ret: "int" args: "int" "..." */
#define	SYS_syscall	0

/* syscall: "exit" ret: "void" args: "int" */
#define	SYS_exit	1

/* syscall: "fork" ret: "int" args: */
#define	SYS_fork	2

/* syscall: "read" ret: "ssize_t" args: "int" "void *" "size_t" */
#define	SYS_read	3

/* syscall: "write" ret: "ssize_t" args: "int" "const void *" "size_t" */
#define	SYS_write	4

/* syscall: "open" ret: "int" args: "const char *" "int" "..." */
#define	SYS_open	5

/* syscall: "close" ret: "int" args: "int" */
#define	SYS_close	6

/* syscall: "wait4" ret: "int" args: "int" "int *" "int" "struct rusage *" */
#define	SYS_wait4	7

/* syscall: "compat_43_ocreat" ret: "int" args: "const char *" "mode_t" */
#define	SYS_compat_43_ocreat	8

/* syscall: "link" ret: "int" args: "const char *" "const char *" */
#define	SYS_link	9

/* syscall: "unlink" ret: "int" args: "const char *" */
#define	SYS_unlink	10

				/* 11 is obsolete execv */
/* syscall: "chdir" ret: "int" args: "const char *" */
#define	SYS_chdir	12

/* syscall: "fchdir" ret: "int" args: "int" */
#define	SYS_fchdir	13

/* syscall: "mknod" ret: "int" args: "const char *" "mode_t" "dev_t" */
#define	SYS_mknod	14

/* syscall: "chmod" ret: "int" args: "const char *" "mode_t" */
#define	SYS_chmod	15

/* syscall: "chown" ret: "int" args: "const char *" "uid_t" "gid_t" */
#define	SYS_chown	16

/* syscall: "break" ret: "int" args: "char *" */
#define	SYS_break	17

/* syscall: "compat_20_getfsstat" ret: "int" args: "struct statfs12 *" "long" "int" */
#define	SYS_compat_20_getfsstat	18

/* syscall: "compat_43_olseek" ret: "long" args: "int" "long" "int" */
#define	SYS_compat_43_olseek	19

#ifdef COMPAT_43
/* syscall: "getpid" ret: "pid_t" args: */
#define	SYS_getpid	20

#else
/* syscall: "getpid" ret: "pid_t" args: */
#define	SYS_getpid	20

#endif
/* syscall: "compat_40_mount" ret: "int" args: "const char *" "const char *" "int" "void *" */
#define	SYS_compat_40_mount	21

/* syscall: "unmount" ret: "int" args: "const char *" "int" */
#define	SYS_unmount	22

/* syscall: "setuid" ret: "int" args: "uid_t" */
#define	SYS_setuid	23

#ifdef COMPAT_43
/* syscall: "getuid" ret: "uid_t" args: */
#define	SYS_getuid	24

#else
/* syscall: "getuid" ret: "uid_t" args: */
#define	SYS_getuid	24

#endif
/* syscall: "geteuid" ret: "uid_t" args: */
#define	SYS_geteuid	25

/* syscall: "ptrace" ret: "int" args: "int" "pid_t" "void *" "int" */
#define	SYS_ptrace	26

/* syscall: "recvmsg" ret: "ssize_t" args: "int" "struct msghdr *" "int" */
#define	SYS_recvmsg	27

/* syscall: "sendmsg" ret: "ssize_t" args: "int" "const struct msghdr *" "int" */
#define	SYS_sendmsg	28

/* syscall: "recvfrom" ret: "ssize_t" args: "int" "void *" "size_t" "int" "struct sockaddr *" "unsigned int *" */
#define	SYS_recvfrom	29

/* syscall: "accept" ret: "int" args: "int" "struct sockaddr *" "unsigned int *" */
#define	SYS_accept	30

/* syscall: "getpeername" ret: "int" args: "int" "struct sockaddr *" "unsigned int *" */
#define	SYS_getpeername	31

/* syscall: "getsockname" ret: "int" args: "int" "struct sockaddr *" "unsigned int *" */
#define	SYS_getsockname	32

/* syscall: "access" ret: "int" args: "const char *" "int" */
#define	SYS_access	33

/* syscall: "chflags" ret: "int" args: "const char *" "u_long" */
#define	SYS_chflags	34

/* syscall: "fchflags" ret: "int" args: "int" "u_long" */
#define	SYS_fchflags	35

/* syscall: "sync" ret: "void" args: */
#define	SYS_sync	36

/* syscall: "kill" ret: "int" args: "int" "int" */
#define	SYS_kill	37

/* syscall: "compat_43_stat43" ret: "int" args: "const char *" "struct stat43 *" */
#define	SYS_compat_43_stat43	38

/* syscall: "getppid" ret: "pid_t" args: */
#define	SYS_getppid	39

/* syscall: "compat_43_lstat43" ret: "int" args: "const char *" "struct stat43 *" */
#define	SYS_compat_43_lstat43	40

/* syscall: "dup" ret: "int" args: "int" */
#define	SYS_dup	41

/* syscall: "pipe" ret: "int" args: */
#define	SYS_pipe	42

/* syscall: "getegid" ret: "gid_t" args: */
#define	SYS_getegid	43

/* syscall: "profil" ret: "int" args: "char *" "size_t" "u_long" "u_int" */
#define	SYS_profil	44

#if defined(KTRACE) || !defined(_KERNEL)
/* syscall: "ktrace" ret: "int" args: "const char *" "int" "int" "int" */
#define	SYS_ktrace	45

#else
				/* 45 is excluded ktrace */
#endif
/* syscall: "compat_13_sigaction13" ret: "int" args: "int" "const struct sigaction13 *" "struct sigaction13 *" */
#define	SYS_compat_13_sigaction13	46

#ifdef COMPAT_43
/* syscall: "getgid" ret: "gid_t" args: */
#define	SYS_getgid	47

#else
/* syscall: "getgid" ret: "gid_t" args: */
#define	SYS_getgid	47

#endif
/* syscall: "compat_13_sigprocmask13" ret: "int" args: "int" "int" */
#define	SYS_compat_13_sigprocmask13	48

/* syscall: "__getlogin" ret: "int" args: "char *" "size_t" */
#define	SYS___getlogin	49

/* syscall: "__setlogin" ret: "int" args: "const char *" */
#define	SYS___setlogin	50

/* syscall: "acct" ret: "int" args: "const char *" */
#define	SYS_acct	51

/* syscall: "compat_13_sigpending13" ret: "int" args: */
#define	SYS_compat_13_sigpending13	52

/* syscall: "compat_13_sigaltstack13" ret: "int" args: "const struct sigaltstack13 *" "struct sigaltstack13 *" */
#define	SYS_compat_13_sigaltstack13	53

/* syscall: "ioctl" ret: "int" args: "int" "u_long" "..." */
#define	SYS_ioctl	54

/* syscall: "compat_12_oreboot" ret: "int" args: "int" */
#define	SYS_compat_12_oreboot	55

/* syscall: "revoke" ret: "int" args: "const char *" */
#define	SYS_revoke	56

/* syscall: "symlink" ret: "int" args: "const char *" "const char *" */
#define	SYS_symlink	57

/* syscall: "readlink" ret: "ssize_t" args: "const char *" "char *" "size_t" */
#define	SYS_readlink	58

/* syscall: "execve" ret: "int" args: "const char *" "char *const *" "char *const *" */
#define	SYS_execve	59

/* syscall: "umask" ret: "mode_t" args: "mode_t" */
#define	SYS_umask	60

/* syscall: "chroot" ret: "int" args: "const char *" */
#define	SYS_chroot	61

/* syscall: "compat_43_fstat43" ret: "int" args: "int" "struct stat43 *" */
#define	SYS_compat_43_fstat43	62

/* syscall: "compat_43_ogetkerninfo" ret: "int" args: "int" "char *" "int *" "int" */
#define	SYS_compat_43_ogetkerninfo	63

/* syscall: "compat_43_ogetpagesize" ret: "int" args: */
#define	SYS_compat_43_ogetpagesize	64

/* syscall: "compat_12_msync" ret: "int" args: "void *" "size_t" */
#define	SYS_compat_12_msync	65

/* syscall: "vfork" ret: "int" args: */
#define	SYS_vfork	66

				/* 67 is obsolete vread */
				/* 68 is obsolete vwrite */
/* syscall: "sbrk" ret: "int" args: "intptr_t" */
#define	SYS_sbrk	69

/* syscall: "sstk" ret: "int" args: "int" */
#define	SYS_sstk	70

/* syscall: "compat_43_ommap" ret: "int" args: "void *" "size_t" "int" "int" "int" "long" */
#define	SYS_compat_43_ommap	71

/* syscall: "vadvise" ret: "int" args: "int" */
#define	SYS_vadvise	72

/* syscall: "munmap" ret: "int" args: "void *" "size_t" */
#define	SYS_munmap	73

/* syscall: "mprotect" ret: "int" args: "void *" "size_t" "int" */
#define	SYS_mprotect	74

/* syscall: "madvise" ret: "int" args: "void *" "size_t" "int" */
#define	SYS_madvise	75

				/* 76 is obsolete vhangup */
				/* 77 is obsolete vlimit */
/* syscall: "mincore" ret: "int" args: "void *" "size_t" "char *" */
#define	SYS_mincore	78

/* syscall: "getgroups" ret: "int" args: "int" "gid_t *" */
#define	SYS_getgroups	79

/* syscall: "setgroups" ret: "int" args: "int" "const gid_t *" */
#define	SYS_setgroups	80

/* syscall: "getpgrp" ret: "int" args: */
#define	SYS_getpgrp	81

/* syscall: "setpgid" ret: "int" args: "int" "int" */
#define	SYS_setpgid	82

/* syscall: "setitimer" ret: "int" args: "int" "const struct itimerval *" "struct itimerval *" */
#define	SYS_setitimer	83

/* syscall: "compat_43_owait" ret: "int" args: */
#define	SYS_compat_43_owait	84

/* syscall: "compat_12_oswapon" ret: "int" args: "const char *" */
#define	SYS_compat_12_oswapon	85

/* syscall: "getitimer" ret: "int" args: "int" "struct itimerval *" */
#define	SYS_getitimer	86

/* syscall: "compat_43_ogethostname" ret: "int" args: "char *" "u_int" */
#define	SYS_compat_43_ogethostname	87

/* syscall: "compat_43_osethostname" ret: "int" args: "char *" "u_int" */
#define	SYS_compat_43_osethostname	88

/* syscall: "compat_43_ogetdtablesize" ret: "int" args: */
#define	SYS_compat_43_ogetdtablesize	89

/* syscall: "dup2" ret: "int" args: "int" "int" */
#define	SYS_dup2	90

/* syscall: "fcntl" ret: "int" args: "int" "int" "..." */
#define	SYS_fcntl	92

/* syscall: "select" ret: "int" args: "int" "fd_set *" "fd_set *" "fd_set *" "struct timeval *" */
#define	SYS_select	93

/* syscall: "fsync" ret: "int" args: "int" */
#define	SYS_fsync	95

/* syscall: "setpriority" ret: "int" args: "int" "id_t" "int" */
#define	SYS_setpriority	96

/* syscall: "compat_30_socket" ret: "int" args: "int" "int" "int" */
#define	SYS_compat_30_socket	97

/* syscall: "connect" ret: "int" args: "int" "const struct sockaddr *" "unsigned int" */
#define	SYS_connect	98

/* syscall: "compat_43_oaccept" ret: "int" args: "int" "void *" "int *" */
#define	SYS_compat_43_oaccept	99

/* syscall: "getpriority" ret: "int" args: "int" "id_t" */
#define	SYS_getpriority	100

/* syscall: "compat_43_osend" ret: "int" args: "int" "void *" "int" "int" */
#define	SYS_compat_43_osend	101

/* syscall: "compat_43_orecv" ret: "int" args: "int" "void *" "int" "int" */
#define	SYS_compat_43_orecv	102

/* syscall: "compat_13_sigreturn13" ret: "int" args: "struct sigcontext13 *" */
#define	SYS_compat_13_sigreturn13	103

/* syscall: "bind" ret: "int" args: "int" "const struct sockaddr *" "unsigned int" */
#define	SYS_bind	104

/* syscall: "setsockopt" ret: "int" args: "int" "int" "int" "const void *" "unsigned int" */
#define	SYS_setsockopt	105

/* syscall: "listen" ret: "int" args: "int" "int" */
#define	SYS_listen	106

				/* 107 is obsolete vtimes */
/* syscall: "compat_43_osigvec" ret: "int" args: "int" "struct sigvec *" "struct sigvec *" */
#define	SYS_compat_43_osigvec	108

/* syscall: "compat_43_osigblock" ret: "int" args: "int" */
#define	SYS_compat_43_osigblock	109

/* syscall: "compat_43_osigsetmask" ret: "int" args: "int" */
#define	SYS_compat_43_osigsetmask	110

/* syscall: "compat_13_sigsuspend13" ret: "int" args: "int" */
#define	SYS_compat_13_sigsuspend13	111

/* syscall: "compat_43_osigstack" ret: "int" args: "struct sigstack *" "struct sigstack *" */
#define	SYS_compat_43_osigstack	112

/* syscall: "compat_43_orecvmsg" ret: "int" args: "int" "struct omsghdr *" "int" */
#define	SYS_compat_43_orecvmsg	113

/* syscall: "compat_43_osendmsg" ret: "int" args: "int" "void *" "int" */
#define	SYS_compat_43_osendmsg	114

				/* 115 is obsolete vtrace */
/* syscall: "gettimeofday" ret: "int" args: "struct timeval *" "void *" */
#define	SYS_gettimeofday	116

/* syscall: "getrusage" ret: "int" args: "int" "struct rusage *" */
#define	SYS_getrusage	117

/* syscall: "getsockopt" ret: "int" args: "int" "int" "int" "void *" "unsigned int *" */
#define	SYS_getsockopt	118

				/* 119 is obsolete resuba */
/* syscall: "readv" ret: "ssize_t" args: "int" "const struct iovec *" "int" */
#define	SYS_readv	120

/* syscall: "writev" ret: "ssize_t" args: "int" "const struct iovec *" "int" */
#define	SYS_writev	121

/* syscall: "settimeofday" ret: "int" args: "const struct timeval *" "const void *" */
#define	SYS_settimeofday	122

/* syscall: "fchown" ret: "int" args: "int" "uid_t" "gid_t" */
#define	SYS_fchown	123

/* syscall: "fchmod" ret: "int" args: "int" "mode_t" */
#define	SYS_fchmod	124

/* syscall: "compat_43_orecvfrom" ret: "int" args: "int" "void *" "size_t" "int" "void *" "int *" */
#define	SYS_compat_43_orecvfrom	125

/* syscall: "setreuid" ret: "int" args: "uid_t" "uid_t" */
#define	SYS_setreuid	126

/* syscall: "setregid" ret: "int" args: "gid_t" "gid_t" */
#define	SYS_setregid	127

/* syscall: "rename" ret: "int" args: "const char *" "const char *" */
#define	SYS_rename	128

/* syscall: "compat_43_otruncate" ret: "int" args: "const char *" "long" */
#define	SYS_compat_43_otruncate	129

/* syscall: "compat_43_oftruncate" ret: "int" args: "int" "long" */
#define	SYS_compat_43_oftruncate	130

/* syscall: "flock" ret: "int" args: "int" "int" */
#define	SYS_flock	131

/* syscall: "mkfifo" ret: "int" args: "const char *" "mode_t" */
#define	SYS_mkfifo	132

/* syscall: "sendto" ret: "ssize_t" args: "int" "const void *" "size_t" "int" "const struct sockaddr *" "unsigned int" */
#define	SYS_sendto	133

/* syscall: "shutdown" ret: "int" args: "int" "int" */
#define	SYS_shutdown	134

/* syscall: "socketpair" ret: "int" args: "int" "int" "int" "int *" */
#define	SYS_socketpair	135

/* syscall: "mkdir" ret: "int" args: "const char *" "mode_t" */
#define	SYS_mkdir	136

/* syscall: "rmdir" ret: "int" args: "const char *" */
#define	SYS_rmdir	137

/* syscall: "utimes" ret: "int" args: "const char *" "const struct timeval *" */
#define	SYS_utimes	138

				/* 139 is obsolete 4.2 sigreturn */
/* syscall: "adjtime" ret: "int" args: "const struct timeval *" "struct timeval *" */
#define	SYS_adjtime	140

/* syscall: "compat_43_ogetpeername" ret: "int" args: "int" "void *" "int *" */
#define	SYS_compat_43_ogetpeername	141

/* syscall: "compat_43_ogethostid" ret: "int32_t" args: */
#define	SYS_compat_43_ogethostid	142

/* syscall: "compat_43_osethostid" ret: "int" args: "int32_t" */
#define	SYS_compat_43_osethostid	143

/* syscall: "compat_43_ogetrlimit" ret: "int" args: "int" "struct orlimit *" */
#define	SYS_compat_43_ogetrlimit	144

/* syscall: "compat_43_osetrlimit" ret: "int" args: "int" "const struct orlimit *" */
#define	SYS_compat_43_osetrlimit	145

/* syscall: "compat_43_okillpg" ret: "int" args: "int" "int" */
#define	SYS_compat_43_okillpg	146

/* syscall: "setsid" ret: "int" args: */
#define	SYS_setsid	147

/* syscall: "quotactl" ret: "int" args: "const char *" "int" "int" "void *" */
#define	SYS_quotactl	148

/* syscall: "compat_43_oquota" ret: "int" args: */
#define	SYS_compat_43_oquota	149

/* syscall: "compat_43_ogetsockname" ret: "int" args: "int" "void *" "int *" */
#define	SYS_compat_43_ogetsockname	150

#if defined(NFS) || defined(NFSSERVER) || !defined(_KERNEL)
/* syscall: "nfssvc" ret: "int" args: "int" "void *" */
#define	SYS_nfssvc	155

#else
				/* 155 is excluded nfssvc */
#endif
/* syscall: "compat_43_ogetdirentries" ret: "int" args: "int" "char *" "u_int" "long *" */
#define	SYS_compat_43_ogetdirentries	156

/* syscall: "compat_20_statfs" ret: "int" args: "const char *" "struct statfs12 *" */
#define	SYS_compat_20_statfs	157

/* syscall: "compat_20_fstatfs" ret: "int" args: "int" "struct statfs12 *" */
#define	SYS_compat_20_fstatfs	158

/* syscall: "compat_30_getfh" ret: "int" args: "const char *" "struct compat_30_fhandle *" */
#define	SYS_compat_30_getfh	161

/* syscall: "compat_09_ogetdomainname" ret: "int" args: "char *" "int" */
#define	SYS_compat_09_ogetdomainname	162

/* syscall: "compat_09_osetdomainname" ret: "int" args: "char *" "int" */
#define	SYS_compat_09_osetdomainname	163

/* syscall: "compat_09_ouname" ret: "int" args: "struct outsname *" */
#define	SYS_compat_09_ouname	164

/* syscall: "sysarch" ret: "int" args: "int" "void *" */
#define	SYS_sysarch	165

#if (defined(SYSVSEM) || !defined(_KERNEL)) && !defined(_LP64)
/* syscall: "compat_10_osemsys" ret: "int" args: "int" "int" "int" "int" "int" */
#define	SYS_compat_10_osemsys	169

#else
				/* 169 is excluded 1.0 semsys */
#endif
#if (defined(SYSVMSG) || !defined(_KERNEL)) && !defined(_LP64)
/* syscall: "compat_10_omsgsys" ret: "int" args: "int" "int" "int" "int" "int" "int" */
#define	SYS_compat_10_omsgsys	170

#else
				/* 170 is excluded 1.0 msgsys */
#endif
#if (defined(SYSVSHM) || !defined(_KERNEL)) && !defined(_LP64)
/* syscall: "compat_10_oshmsys" ret: "int" args: "int" "int" "int" "int" */
#define	SYS_compat_10_oshmsys	171

#else
				/* 171 is excluded 1.0 shmsys */
#endif
/* syscall: "pread" ret: "ssize_t" args: "int" "void *" "size_t" "int" "off_t" */
#define	SYS_pread	173

/* syscall: "pwrite" ret: "ssize_t" args: "int" "const void *" "size_t" "int" "off_t" */
#define	SYS_pwrite	174

/* syscall: "compat_30_ntp_gettime" ret: "int" args: "struct ntptimeval30 *" */
#define	SYS_compat_30_ntp_gettime	175

#if defined(NTP) || !defined(_KERNEL)
/* syscall: "ntp_adjtime" ret: "int" args: "struct timex *" */
#define	SYS_ntp_adjtime	176

#else
				/* 176 is excluded ntp_adjtime */
#endif
/* syscall: "setgid" ret: "int" args: "gid_t" */
#define	SYS_setgid	181

/* syscall: "setegid" ret: "int" args: "gid_t" */
#define	SYS_setegid	182

/* syscall: "seteuid" ret: "int" args: "uid_t" */
#define	SYS_seteuid	183

#if defined(LFS) || !defined(_KERNEL)
/* syscall: "lfs_bmapv" ret: "int" args: "fsid_t *" "struct block_info *" "int" */
#define	SYS_lfs_bmapv	184

/* syscall: "lfs_markv" ret: "int" args: "fsid_t *" "struct block_info *" "int" */
#define	SYS_lfs_markv	185

/* syscall: "lfs_segclean" ret: "int" args: "fsid_t *" "u_long" */
#define	SYS_lfs_segclean	186

/* syscall: "lfs_segwait" ret: "int" args: "fsid_t *" "struct timeval *" */
#define	SYS_lfs_segwait	187

#else
				/* 184 is excluded lfs_bmapv */
				/* 185 is excluded lfs_markv */
				/* 186 is excluded lfs_segclean */
				/* 187 is excluded lfs_segwait */
#endif
/* syscall: "compat_12_stat12" ret: "int" args: "const char *" "struct stat12 *" */
#define	SYS_compat_12_stat12	188

/* syscall: "compat_12_fstat12" ret: "int" args: "int" "struct stat12 *" */
#define	SYS_compat_12_fstat12	189

/* syscall: "compat_12_lstat12" ret: "int" args: "const char *" "struct stat12 *" */
#define	SYS_compat_12_lstat12	190

/* syscall: "pathconf" ret: "long" args: "const char *" "int" */
#define	SYS_pathconf	191

/* syscall: "fpathconf" ret: "long" args: "int" "int" */
#define	SYS_fpathconf	192

/* syscall: "getrlimit" ret: "int" args: "int" "struct rlimit *" */
#define	SYS_getrlimit	194

/* syscall: "setrlimit" ret: "int" args: "int" "const struct rlimit *" */
#define	SYS_setrlimit	195

/* syscall: "compat_12_getdirentries" ret: "int" args: "int" "char *" "u_int" "long *" */
#define	SYS_compat_12_getdirentries	196

/* syscall: "mmap" ret: "void *" args: "void *" "size_t" "int" "int" "int" "long" "off_t" */
#define	SYS_mmap	197

/* syscall: "__syscall" ret: "quad_t" args: "quad_t" "..." */
#define	SYS___syscall	198

/* syscall: "lseek" ret: "off_t" args: "int" "int" "off_t" "int" */
#define	SYS_lseek	199

/* syscall: "truncate" ret: "int" args: "const char *" "int" "off_t" */
#define	SYS_truncate	200

/* syscall: "ftruncate" ret: "int" args: "int" "int" "off_t" */
#define	SYS_ftruncate	201

/* syscall: "__sysctl" ret: "int" args: "const int *" "u_int" "void *" "size_t *" "const void *" "size_t" */
#define	SYS___sysctl	202

/* syscall: "mlock" ret: "int" args: "const void *" "size_t" */
#define	SYS_mlock	203

/* syscall: "munlock" ret: "int" args: "const void *" "size_t" */
#define	SYS_munlock	204

/* syscall: "undelete" ret: "int" args: "const char *" */
#define	SYS_undelete	205

/* syscall: "futimes" ret: "int" args: "int" "const struct timeval *" */
#define	SYS_futimes	206

/* syscall: "getpgid" ret: "pid_t" args: "pid_t" */
#define	SYS_getpgid	207

/* syscall: "reboot" ret: "int" args: "int" "char *" */
#define	SYS_reboot	208

/* syscall: "poll" ret: "int" args: "struct pollfd *" "u_int" "int" */
#define	SYS_poll	209

#if defined(LKM) || !defined(_KERNEL)
#else	/* !LKM */
				/* 210 is excluded lkmnosys */
				/* 211 is excluded lkmnosys */
				/* 212 is excluded lkmnosys */
				/* 213 is excluded lkmnosys */
				/* 214 is excluded lkmnosys */
				/* 215 is excluded lkmnosys */
				/* 216 is excluded lkmnosys */
				/* 217 is excluded lkmnosys */
				/* 218 is excluded lkmnosys */
				/* 219 is excluded lkmnosys */
#endif	/* !LKM */
#if defined(SYSVSEM) || !defined(_KERNEL)
/* syscall: "compat_14___semctl" ret: "int" args: "int" "int" "int" "union __semun *" */
#define	SYS_compat_14___semctl	220

/* syscall: "semget" ret: "int" args: "key_t" "int" "int" */
#define	SYS_semget	221

/* syscall: "semop" ret: "int" args: "int" "struct sembuf *" "size_t" */
#define	SYS_semop	222

/* syscall: "semconfig" ret: "int" args: "int" */
#define	SYS_semconfig	223

#else
				/* 220 is excluded compat_14_semctl */
				/* 221 is excluded semget */
				/* 222 is excluded semop */
				/* 223 is excluded semconfig */
#endif
#if defined(SYSVMSG) || !defined(_KERNEL)
/* syscall: "compat_14_msgctl" ret: "int" args: "int" "int" "struct msqid_ds14 *" */
#define	SYS_compat_14_msgctl	224

/* syscall: "msgget" ret: "int" args: "key_t" "int" */
#define	SYS_msgget	225

/* syscall: "msgsnd" ret: "int" args: "int" "const void *" "size_t" "int" */
#define	SYS_msgsnd	226

/* syscall: "msgrcv" ret: "ssize_t" args: "int" "void *" "size_t" "long" "int" */
#define	SYS_msgrcv	227

#else
				/* 224 is excluded compat_14_msgctl */
				/* 225 is excluded msgget */
				/* 226 is excluded msgsnd */
				/* 227 is excluded msgrcv */
#endif
#if defined(SYSVSHM) || !defined(_KERNEL)
/* syscall: "shmat" ret: "void *" args: "int" "const void *" "int" */
#define	SYS_shmat	228

/* syscall: "compat_14_shmctl" ret: "int" args: "int" "int" "struct shmid_ds14 *" */
#define	SYS_compat_14_shmctl	229

/* syscall: "shmdt" ret: "int" args: "const void *" */
#define	SYS_shmdt	230

/* syscall: "shmget" ret: "int" args: "key_t" "size_t" "int" */
#define	SYS_shmget	231

#else
				/* 228 is excluded shmat */
				/* 229 is excluded compat_14_shmctl */
				/* 230 is excluded shmdt */
				/* 231 is excluded shmget */
#endif
/* syscall: "clock_gettime" ret: "int" args: "clockid_t" "struct timespec *" */
#define	SYS_clock_gettime	232

/* syscall: "clock_settime" ret: "int" args: "clockid_t" "const struct timespec *" */
#define	SYS_clock_settime	233

/* syscall: "clock_getres" ret: "int" args: "clockid_t" "struct timespec *" */
#define	SYS_clock_getres	234

/* syscall: "timer_create" ret: "int" args: "clockid_t" "struct sigevent *" "timer_t *" */
#define	SYS_timer_create	235

/* syscall: "timer_delete" ret: "int" args: "timer_t" */
#define	SYS_timer_delete	236

/* syscall: "timer_settime" ret: "int" args: "timer_t" "int" "const struct itimerspec *" "struct itimerspec *" */
#define	SYS_timer_settime	237

/* syscall: "timer_gettime" ret: "int" args: "timer_t" "struct itimerspec *" */
#define	SYS_timer_gettime	238

/* syscall: "timer_getoverrun" ret: "int" args: "timer_t" */
#define	SYS_timer_getoverrun	239

/* syscall: "nanosleep" ret: "int" args: "const struct timespec *" "struct timespec *" */
#define	SYS_nanosleep	240

/* syscall: "fdatasync" ret: "int" args: "int" */
#define	SYS_fdatasync	241

/* syscall: "mlockall" ret: "int" args: "int" */
#define	SYS_mlockall	242

/* syscall: "munlockall" ret: "int" args: */
#define	SYS_munlockall	243

/* syscall: "__sigtimedwait" ret: "int" args: "const sigset_t *" "siginfo_t *" "struct timespec *" */
#define	SYS___sigtimedwait	244

#if defined(P1003_1B_SEMAPHORE) || (!defined(_KERNEL) && defined(_LIBC))
/* syscall: "_ksem_init" ret: "int" args: "unsigned int" "semid_t *" */
#define	SYS__ksem_init	247

/* syscall: "_ksem_open" ret: "int" args: "const char *" "int" "mode_t" "unsigned int" "semid_t *" */
#define	SYS__ksem_open	248

/* syscall: "_ksem_unlink" ret: "int" args: "const char *" */
#define	SYS__ksem_unlink	249

/* syscall: "_ksem_close" ret: "int" args: "semid_t" */
#define	SYS__ksem_close	250

/* syscall: "_ksem_post" ret: "int" args: "semid_t" */
#define	SYS__ksem_post	251

/* syscall: "_ksem_wait" ret: "int" args: "semid_t" */
#define	SYS__ksem_wait	252

/* syscall: "_ksem_trywait" ret: "int" args: "semid_t" */
#define	SYS__ksem_trywait	253

/* syscall: "_ksem_getvalue" ret: "int" args: "semid_t" "unsigned int *" */
#define	SYS__ksem_getvalue	254

/* syscall: "_ksem_destroy" ret: "int" args: "semid_t" */
#define	SYS__ksem_destroy	255

#else
				/* 247 is excluded sys__ksem_init */
				/* 248 is excluded sys__ksem_open */
				/* 249 is excluded sys__ksem_unlink */
				/* 250 is excluded sys__ksem_close */
				/* 251 is excluded sys__ksem_post */
				/* 252 is excluded sys__ksem_wait */
				/* 253 is excluded sys__ksem_trywait */
				/* 254 is excluded sys__ksem_getvalue */
				/* 255 is excluded sys__ksem_destroy */
#endif
/* syscall: "__posix_rename" ret: "int" args: "const char *" "const char *" */
#define	SYS___posix_rename	270

/* syscall: "swapctl" ret: "int" args: "int" "void *" "int" */
#define	SYS_swapctl	271

/* syscall: "compat_30_getdents" ret: "int" args: "int" "char *" "size_t" */
#define	SYS_compat_30_getdents	272

/* syscall: "minherit" ret: "int" args: "void *" "size_t" "int" */
#define	SYS_minherit	273

/* syscall: "lchmod" ret: "int" args: "const char *" "mode_t" */
#define	SYS_lchmod	274

/* syscall: "lchown" ret: "int" args: "const char *" "uid_t" "gid_t" */
#define	SYS_lchown	275

/* syscall: "lutimes" ret: "int" args: "const char *" "const struct timeval *" */
#define	SYS_lutimes	276

/* syscall: "__msync13" ret: "int" args: "void *" "size_t" "int" */
#define	SYS___msync13	277

/* syscall: "compat_30___stat13" ret: "int" args: "const char *" "struct stat13 *" */
#define	SYS_compat_30___stat13	278

/* syscall: "compat_30___fstat13" ret: "int" args: "int" "struct stat13 *" */
#define	SYS_compat_30___fstat13	279

/* syscall: "compat_30___lstat13" ret: "int" args: "const char *" "struct stat13 *" */
#define	SYS_compat_30___lstat13	280

/* syscall: "__sigaltstack14" ret: "int" args: "const struct sigaltstack *" "struct sigaltstack *" */
#define	SYS___sigaltstack14	281

/* syscall: "__vfork14" ret: "int" args: */
#define	SYS___vfork14	282

/* syscall: "__posix_chown" ret: "int" args: "const char *" "uid_t" "gid_t" */
#define	SYS___posix_chown	283

/* syscall: "__posix_fchown" ret: "int" args: "int" "uid_t" "gid_t" */
#define	SYS___posix_fchown	284

/* syscall: "__posix_lchown" ret: "int" args: "const char *" "uid_t" "gid_t" */
#define	SYS___posix_lchown	285

/* syscall: "getsid" ret: "pid_t" args: "pid_t" */
#define	SYS_getsid	286

/* syscall: "__clone" ret: "pid_t" args: "int" "void *" */
#define	SYS___clone	287

#if defined(KTRACE) || !defined(_KERNEL)
/* syscall: "fktrace" ret: "int" args: "const int" "int" "int" "int" */
#define	SYS_fktrace	288

#else
				/* 288 is excluded ktrace */
#endif
/* syscall: "preadv" ret: "ssize_t" args: "int" "const struct iovec *" "int" "int" "off_t" */
#define	SYS_preadv	289

/* syscall: "pwritev" ret: "ssize_t" args: "int" "const struct iovec *" "int" "int" "off_t" */
#define	SYS_pwritev	290

/* syscall: "compat_16___sigaction14" ret: "int" args: "int" "const struct sigaction *" "struct sigaction *" */
#define	SYS_compat_16___sigaction14	291

/* syscall: "__sigpending14" ret: "int" args: "sigset_t *" */
#define	SYS___sigpending14	292

/* syscall: "__sigprocmask14" ret: "int" args: "int" "const sigset_t *" "sigset_t *" */
#define	SYS___sigprocmask14	293

/* syscall: "__sigsuspend14" ret: "int" args: "const sigset_t *" */
#define	SYS___sigsuspend14	294

/* syscall: "compat_16___sigreturn14" ret: "int" args: "struct sigcontext *" */
#define	SYS_compat_16___sigreturn14	295

/* syscall: "__getcwd" ret: "int" args: "char *" "size_t" */
#define	SYS___getcwd	296

/* syscall: "fchroot" ret: "int" args: "int" */
#define	SYS_fchroot	297

/* syscall: "compat_30_fhopen" ret: "int" args: "const struct compat_30_fhandle *" "int" */
#define	SYS_compat_30_fhopen	298

/* syscall: "compat_30_fhstat" ret: "int" args: "const struct compat_30_fhandle *" "struct stat13 *" */
#define	SYS_compat_30_fhstat	299

/* syscall: "compat_20_fhstatfs" ret: "int" args: "const struct compat_30_fhandle *" "struct statfs12 *" */
#define	SYS_compat_20_fhstatfs	300

#if defined(SYSVSEM) || !defined(_KERNEL)
/* syscall: "____semctl13" ret: "int" args: "int" "int" "int" "..." */
#define	SYS_____semctl13	301

#else
				/* 301 is excluded ____semctl13 */
#endif
#if defined(SYSVMSG) || !defined(_KERNEL)
/* syscall: "__msgctl13" ret: "int" args: "int" "int" "struct msqid_ds *" */
#define	SYS___msgctl13	302

#else
				/* 302 is excluded __msgctl13 */
#endif
#if defined(SYSVSHM) || !defined(_KERNEL)
/* syscall: "__shmctl13" ret: "int" args: "int" "int" "struct shmid_ds *" */
#define	SYS___shmctl13	303

#else
				/* 303 is excluded __shmctl13 */
#endif
/* syscall: "lchflags" ret: "int" args: "const char *" "u_long" */
#define	SYS_lchflags	304

/* syscall: "issetugid" ret: "int" args: */
#define	SYS_issetugid	305

/* syscall: "utrace" ret: "int" args: "const char *" "void *" "size_t" */
#define	SYS_utrace	306

/* syscall: "getcontext" ret: "int" args: "struct __ucontext *" */
#define	SYS_getcontext	307

/* syscall: "setcontext" ret: "int" args: "const struct __ucontext *" */
#define	SYS_setcontext	308

/* syscall: "_lwp_create" ret: "int" args: "const struct __ucontext *" "u_long" "lwpid_t *" */
#define	SYS__lwp_create	309

/* syscall: "_lwp_exit" ret: "int" args: */
#define	SYS__lwp_exit	310

/* syscall: "_lwp_self" ret: "lwpid_t" args: */
#define	SYS__lwp_self	311

/* syscall: "_lwp_wait" ret: "int" args: "lwpid_t" "lwpid_t *" */
#define	SYS__lwp_wait	312

/* syscall: "_lwp_suspend" ret: "int" args: "lwpid_t" */
#define	SYS__lwp_suspend	313

/* syscall: "_lwp_continue" ret: "int" args: "lwpid_t" */
#define	SYS__lwp_continue	314

/* syscall: "_lwp_wakeup" ret: "int" args: "lwpid_t" */
#define	SYS__lwp_wakeup	315

/* syscall: "_lwp_getprivate" ret: "void *" args: */
#define	SYS__lwp_getprivate	316

/* syscall: "_lwp_setprivate" ret: "void" args: "void *" */
#define	SYS__lwp_setprivate	317

/* syscall: "_lwp_kill" ret: "int" args: "lwpid_t" "int" */
#define	SYS__lwp_kill	318

/* syscall: "_lwp_detach" ret: "int" args: "lwpid_t" */
#define	SYS__lwp_detach	319

/* syscall: "_lwp_park" ret: "int" args: "const struct timespec *" "struct __ucontext *" "const void *" */
#define	SYS__lwp_park	320

/* syscall: "_lwp_unpark" ret: "int" args: "lwpid_t" "const void *" */
#define	SYS__lwp_unpark	321

/* syscall: "_lwp_unpark_all" ret: "ssize_t" args: "const lwpid_t *" "size_t" "const void *" */
#define	SYS__lwp_unpark_all	322

/* syscall: "sa_register" ret: "int" args: */
#define	SYS_sa_register	330

/* syscall: "sa_stacks" ret: "int" args: */
#define	SYS_sa_stacks	331

/* syscall: "sa_enable" ret: "int" args: */
#define	SYS_sa_enable	332

/* syscall: "sa_setconcurrency" ret: "int" args: */
#define	SYS_sa_setconcurrency	333

/* syscall: "sa_yield" ret: "int" args: */
#define	SYS_sa_yield	334

/* syscall: "sa_preempt" ret: "int" args: */
#define	SYS_sa_preempt	335

/* syscall: "sa_unblockyield" ret: "int" args: */
#define	SYS_sa_unblockyield	336

/* syscall: "__sigaction_sigtramp" ret: "int" args: "int" "const struct sigaction *" "struct sigaction *" "const void *" "int" */
#define	SYS___sigaction_sigtramp	340

/* syscall: "pmc_get_info" ret: "int" args: "int" "int" "void *" */
#define	SYS_pmc_get_info	341

/* syscall: "pmc_control" ret: "int" args: "int" "int" "void *" */
#define	SYS_pmc_control	342

/* syscall: "rasctl" ret: "int" args: "void *" "size_t" "int" */
#define	SYS_rasctl	343

/* syscall: "kqueue" ret: "int" args: */
#define	SYS_kqueue	344

/* syscall: "kevent" ret: "int" args: "int" "const struct kevent *" "size_t" "struct kevent *" "size_t" "const struct timespec *" */
#define	SYS_kevent	345

/* syscall: "sched_yield" ret: "int" args: */
#define	SYS_sched_yield	350

/* syscall: "fsync_range" ret: "int" args: "int" "int" "off_t" "off_t" */
#define	SYS_fsync_range	354

/* syscall: "uuidgen" ret: "int" args: "struct uuid *" "int" */
#define	SYS_uuidgen	355

/* syscall: "getvfsstat" ret: "int" args: "struct statvfs *" "size_t" "int" */
#define	SYS_getvfsstat	356

/* syscall: "statvfs1" ret: "int" args: "const char *" "struct statvfs *" "int" */
#define	SYS_statvfs1	357

/* syscall: "fstatvfs1" ret: "int" args: "int" "struct statvfs *" "int" */
#define	SYS_fstatvfs1	358

/* syscall: "compat_30_fhstatvfs1" ret: "int" args: "const struct compat_30_fhandle *" "struct statvfs *" "int" */
#define	SYS_compat_30_fhstatvfs1	359

/* syscall: "extattrctl" ret: "int" args: "const char *" "int" "const char *" "int" "const char *" */
#define	SYS_extattrctl	360

/* syscall: "extattr_set_file" ret: "int" args: "const char *" "int" "const char *" "const void *" "size_t" */
#define	SYS_extattr_set_file	361

/* syscall: "extattr_get_file" ret: "ssize_t" args: "const char *" "int" "const char *" "void *" "size_t" */
#define	SYS_extattr_get_file	362

/* syscall: "extattr_delete_file" ret: "int" args: "const char *" "int" "const char *" */
#define	SYS_extattr_delete_file	363

/* syscall: "extattr_set_fd" ret: "int" args: "int" "int" "const char *" "const void *" "size_t" */
#define	SYS_extattr_set_fd	364

/* syscall: "extattr_get_fd" ret: "ssize_t" args: "int" "int" "const char *" "void *" "size_t" */
#define	SYS_extattr_get_fd	365

/* syscall: "extattr_delete_fd" ret: "int" args: "int" "int" "const char *" */
#define	SYS_extattr_delete_fd	366

/* syscall: "extattr_set_link" ret: "int" args: "const char *" "int" "const char *" "const void *" "size_t" */
#define	SYS_extattr_set_link	367

/* syscall: "extattr_get_link" ret: "ssize_t" args: "const char *" "int" "const char *" "void *" "size_t" */
#define	SYS_extattr_get_link	368

/* syscall: "extattr_delete_link" ret: "int" args: "const char *" "int" "const char *" */
#define	SYS_extattr_delete_link	369

/* syscall: "extattr_list_fd" ret: "ssize_t" args: "int" "int" "void *" "size_t" */
#define	SYS_extattr_list_fd	370

/* syscall: "extattr_list_file" ret: "ssize_t" args: "const char *" "int" "void *" "size_t" */
#define	SYS_extattr_list_file	371

/* syscall: "extattr_list_link" ret: "ssize_t" args: "const char *" "int" "void *" "size_t" */
#define	SYS_extattr_list_link	372

/* syscall: "pselect" ret: "int" args: "int" "fd_set *" "fd_set *" "fd_set *" "const struct timespec *" "const sigset_t *" */
#define	SYS_pselect	373

/* syscall: "pollts" ret: "int" args: "struct pollfd *" "u_int" "const struct timespec *" "const sigset_t *" */
#define	SYS_pollts	374

/* syscall: "setxattr" ret: "int" args: "const char *" "const char *" "void *" "size_t" "int" */
#define	SYS_setxattr	375

/* syscall: "lsetxattr" ret: "int" args: "const char *" "const char *" "void *" "size_t" "int" */
#define	SYS_lsetxattr	376

/* syscall: "fsetxattr" ret: "int" args: "int" "const char *" "void *" "size_t" "int" */
#define	SYS_fsetxattr	377

/* syscall: "getxattr" ret: "int" args: "const char *" "const char *" "void *" "size_t" */
#define	SYS_getxattr	378

/* syscall: "lgetxattr" ret: "int" args: "const char *" "const char *" "void *" "size_t" */
#define	SYS_lgetxattr	379

/* syscall: "fgetxattr" ret: "int" args: "int" "const char *" "void *" "size_t" */
#define	SYS_fgetxattr	380

/* syscall: "listxattr" ret: "int" args: "const char *" "char *" "size_t" */
#define	SYS_listxattr	381

/* syscall: "llistxattr" ret: "int" args: "const char *" "char *" "size_t" */
#define	SYS_llistxattr	382

/* syscall: "flistxattr" ret: "int" args: "int" "char *" "size_t" */
#define	SYS_flistxattr	383

/* syscall: "removexattr" ret: "int" args: "const char *" "const char *" */
#define	SYS_removexattr	384

/* syscall: "lremovexattr" ret: "int" args: "const char *" "const char *" */
#define	SYS_lremovexattr	385

/* syscall: "fremovexattr" ret: "int" args: "int" "const char *" */
#define	SYS_fremovexattr	386

/* syscall: "__stat30" ret: "int" args: "const char *" "struct stat *" */
#define	SYS___stat30	387

/* syscall: "__fstat30" ret: "int" args: "int" "struct stat *" */
#define	SYS___fstat30	388

/* syscall: "__lstat30" ret: "int" args: "const char *" "struct stat *" */
#define	SYS___lstat30	389

/* syscall: "__getdents30" ret: "int" args: "int" "char *" "size_t" */
#define	SYS___getdents30	390

/* syscall: "posix_fadvise" ret: "int" args: "int" "off_t" "off_t" "int" */
#define	SYS_posix_fadvise	391

/* syscall: "compat_30___fhstat30" ret: "int" args: "const struct compat_30_fhandle *" "struct stat *" */
#define	SYS_compat_30___fhstat30	392

/* syscall: "__ntp_gettime30" ret: "int" args: "struct ntptimeval *" */
#define	SYS___ntp_gettime30	393

/* syscall: "__socket30" ret: "int" args: "int" "int" "int" */
#define	SYS___socket30	394

/* syscall: "__getfh30" ret: "int" args: "const char *" "void *" "size_t *" */
#define	SYS___getfh30	395

/* syscall: "__fhopen40" ret: "int" args: "const void *" "size_t" "int" */
#define	SYS___fhopen40	396

/* syscall: "__fhstatvfs140" ret: "int" args: "const void *" "size_t" "struct statvfs *" "int" */
#define	SYS___fhstatvfs140	397

/* syscall: "__fhstat40" ret: "int" args: "const void *" "size_t" "struct stat *" */
#define	SYS___fhstat40	398

/* syscall: "aio_cancel" ret: "int" args: "int" "struct aiocb *" */
#define	SYS_aio_cancel	399

/* syscall: "aio_error" ret: "int" args: "const struct aiocb *" */
#define	SYS_aio_error	400

/* syscall: "aio_fsync" ret: "int" args: "int" "struct aiocb *" */
#define	SYS_aio_fsync	401

/* syscall: "aio_read" ret: "int" args: "struct aiocb *" */
#define	SYS_aio_read	402

/* syscall: "aio_return" ret: "int" args: "struct aiocb *" */
#define	SYS_aio_return	403

/* syscall: "aio_suspend" ret: "int" args: "const struct aiocb *const *" "int" "const struct timespec *" */
#define	SYS_aio_suspend	404

/* syscall: "aio_write" ret: "int" args: "struct aiocb *" */
#define	SYS_aio_write	405

/* syscall: "lio_listio" ret: "int" args: "int" "struct aiocb *const *" "int" "struct sigevent *" */
#define	SYS_lio_listio	406

/* syscall: "__mount50" ret: "int" args: "const char *" "const char *" "int" "void *" "size_t" */
#define	SYS___mount50	410

#define	SYS_MAXSYSCALL	411
#define	SYS_NSYSENT	512
#endif /* _SYS_SYSCALL_H_ */
