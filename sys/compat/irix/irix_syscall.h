/* $NetBSD: irix_syscall.h,v 1.13 2001/12/23 20:57:30 manu Exp $ */

/*
 * System call numbers.
 *
 * DO NOT EDIT-- this file is automatically generated.
 * created from	NetBSD: syscalls.master,v 1.11 2001/12/23 20:15:04 manu Exp 
 */

/* syscall: "syscall" ret: "int" args: */
#define	IRIX_SYS_syscall	0

/* syscall: "exit" ret: "int" args: "int" */
#define	IRIX_SYS_exit	1

/* syscall: "fork" ret: "int" args: */
#define	IRIX_SYS_fork	2

/* syscall: "read" ret: "int" args: "int" "char *" "u_int" */
#define	IRIX_SYS_read	3

/* syscall: "write" ret: "int" args: "int" "char *" "u_int" */
#define	IRIX_SYS_write	4

/* syscall: "open" ret: "int" args: "const char *" "int" "int" */
#define	IRIX_SYS_open	5

/* syscall: "close" ret: "int" args: "int" */
#define	IRIX_SYS_close	6

				/* 7 is obsolete wait */
/* syscall: "creat" ret: "int" args: "const char *" "int" */
#define	IRIX_SYS_creat	8

/* syscall: "link" ret: "int" args: "char *" "char *" */
#define	IRIX_SYS_link	9

/* syscall: "unlink" ret: "int" args: "char *" */
#define	IRIX_SYS_unlink	10

/* syscall: "execv" ret: "int" args: "const char *" "char **" */
#define	IRIX_SYS_execv	11

/* syscall: "chdir" ret: "int" args: "char *" */
#define	IRIX_SYS_chdir	12

/* syscall: "time" ret: "int" args: "svr4_time_t *" */
#define	IRIX_SYS_time	13

				/* 14 is obsolete mknod */
/* syscall: "chmod" ret: "int" args: "char *" "int" */
#define	IRIX_SYS_chmod	15

/* syscall: "chown" ret: "int" args: "char *" "int" "int" */
#define	IRIX_SYS_chown	16

/* syscall: "break" ret: "int" args: "caddr_t" */
#define	IRIX_SYS_break	17

				/* 18 is obsolete stat */
/* syscall: "lseek" ret: "long" args: "int" "long" "int" */
#define	IRIX_SYS_lseek	19

/* syscall: "getpid" ret: "pid_t" args: */
#define	IRIX_SYS_getpid	20

/* syscall: "setuid" ret: "int" args: "uid_t" */
#define	IRIX_SYS_setuid	23

/* syscall: "getuid_with_euid" ret: "uid_t" args: */
#define	IRIX_SYS_getuid_with_euid	24

				/* 27 is obsolete alarm */
/* syscall: "fstat" ret: "int" args: "int" "struct svr4_stat *" */
#define	IRIX_SYS_fstat	28

/* syscall: "pause" ret: "int" args: */
#define	IRIX_SYS_pause	29

/* syscall: "utime" ret: "int" args: "const char *" "struct svr4_utimbuf *" */
#define	IRIX_SYS_utime	30

/* syscall: "access" ret: "int" args: "const char *" "int" */
#define	IRIX_SYS_access	33

/* syscall: "nice" ret: "int" args: "int" */
#define	IRIX_SYS_nice	34

/* syscall: "sync" ret: "int" args: */
#define	IRIX_SYS_sync	36

/* syscall: "kill" ret: "int" args: "int" "int" */
#define	IRIX_SYS_kill	37

/* syscall: "pgrpsys" ret: "int" args: "int" "int" "int" */
#define	IRIX_SYS_pgrpsys	39

/* syscall: "syssgi" ret: "ptrdiff_t" args: "int" "void *" "void *" "void *" "void *" "void *" */
#define	IRIX_SYS_syssgi	40

/* syscall: "dup" ret: "int" args: "u_int" */
#define	IRIX_SYS_dup	41

/* syscall: "pipe" ret: "int" args: */
#define	IRIX_SYS_pipe	42

/* syscall: "times" ret: "int" args: "struct tms *" */
#define	IRIX_SYS_times	43

/* syscall: "setgid" ret: "int" args: "gid_t" */
#define	IRIX_SYS_setgid	46

/* syscall: "getgid_with_egid" ret: "gid_t" args: */
#define	IRIX_SYS_getgid_with_egid	47

				/* 48 is obsolete ssig */
/* syscall: "msgsys" ret: "int" args: "int" "int" "int" "int" "int" */
#define	IRIX_SYS_msgsys	49

/* syscall: "shmsys" ret: "int" args: "int" "int" "int" "int" */
#define	IRIX_SYS_shmsys	52

/* syscall: "semsys" ret: "int" args: "int" "int" "int" "int" "int" */
#define	IRIX_SYS_semsys	53

/* syscall: "ioctl" ret: "int" args: "int" "u_long" "caddr_t" */
#define	IRIX_SYS_ioctl	54

/* syscall: "sysmp" ret: "int" args: "int" "void *" "void *" "void *" "void *" */
#define	IRIX_SYS_sysmp	56

/* syscall: "utssys" ret: "int" args: "void *" "void *" "int" "void *" */
#define	IRIX_SYS_utssys	57

/* syscall: "execve" ret: "int" args: "const char *" "char **" "char **" */
#define	IRIX_SYS_execve	59

/* syscall: "umask" ret: "int" args: "int" */
#define	IRIX_SYS_umask	60

/* syscall: "chroot" ret: "int" args: "char *" */
#define	IRIX_SYS_chroot	61

/* syscall: "fcntl" ret: "int" args: "int" "int" "char *" */
#define	IRIX_SYS_fcntl	62

/* syscall: "ulimit" ret: "long" args: "int" "long" */
#define	IRIX_SYS_ulimit	63

				/* 70 is obsolete advfs */
				/* 71 is obsolete unadvfs */
				/* 72 is obsolete rmount */
				/* 73 is obsolete rumount */
				/* 74 is obsolete rfstart */
				/* 75 is obsolete sigret */
				/* 76 is obsolete rdebug */
				/* 77 is obsolete rfstop */
/* syscall: "rmdir" ret: "int" args: "char *" */
#define	IRIX_SYS_rmdir	79

/* syscall: "mkdir" ret: "int" args: "char *" "int" */
#define	IRIX_SYS_mkdir	80

/* syscall: "getdents" ret: "int" args: "int" "irix_dirent_t *" "int" */
#define	IRIX_SYS_getdents	81

/* syscall: "sginap" ret: "long" args: "long" */
#define	IRIX_SYS_sginap	82

/* syscall: "getmsg" ret: "int" args: "int" "struct svr4_strbuf *" "struct svr4_strbuf *" "int *" */
#define	IRIX_SYS_getmsg	85

/* syscall: "putmsg" ret: "int" args: "int" "struct svr4_strbuf *" "struct svr4_strbuf *" "int" */
#define	IRIX_SYS_putmsg	86

/* syscall: "poll" ret: "int" args: "struct pollfd *" "u_int" "int" */
#define	IRIX_SYS_poll	87

/* syscall: "sigreturn" ret: "int" args: "struct irix_sigframe *" */
#define	IRIX_SYS_sigreturn	88

/* syscall: "prctl" ret: "ptrdiff_t" args: "unsigned int" "void *" */
#define	IRIX_SYS_prctl	130

/* syscall: "mmap" ret: "void *" args: "void *" "svr4_size_t" "int" "int" "int" "svr4_off_t" */
#define	IRIX_SYS_mmap	134

/* syscall: "xstat" ret: "int" args: "const int" "const char *" "struct stat *" */
#define	IRIX_SYS_xstat	158

/* syscall: "lxstat" ret: "int" args: "const int" "const char *" "struct stat *" */
#define	IRIX_SYS_lxstat	159

/* syscall: "fxstat" ret: "int" args: "const int" "const int" "struct stat *" */
#define	IRIX_SYS_fxstat	160

/* syscall: "sigaction" ret: "int" args: "int" "const struct svr4_sigaction *" "struct svr4_sigaction *" */
#define	IRIX_SYS_sigaction	162

/* syscall: "sigpending" ret: "int" args: "int" "svr4_sigset_t *" */
#define	IRIX_SYS_sigpending	163

/* syscall: "sigprocmask" ret: "int" args: "int" "const svr4_sigset_t *" "svr4_sigset_t *" */
#define	IRIX_SYS_sigprocmask	164

/* syscall: "sigsuspend" ret: "int" args: "const svr4_sigset_t *" */
#define	IRIX_SYS_sigsuspend	165

/* syscall: "getmountid" ret: "int" args: "const char *" "irix_mountid_t *" */
#define	IRIX_SYS_getmountid	203

/* syscall: "ngetdents" ret: "int" args: "int" "irix_dirent_t *" "unsigned short" "int *" */
#define	IRIX_SYS_ngetdents	207

#define	IRIX_SYS_MAXSYSCALL	236
#define	IRIX_SYS_NSYSENT	236
