/* $NetBSD: irix_syscalls.c,v 1.48 2002/08/02 23:02:53 manu Exp $ */

/*
 * System call names.
 *
 * DO NOT EDIT-- this file is automatically generated.
 * created from	NetBSD: syscalls.master,v 1.45 2002/06/12 20:33:21 manu Exp 
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: irix_syscalls.c,v 1.48 2002/08/02 23:02:53 manu Exp $");

#if defined(_KERNEL_OPT)
#if defined(_KERNEL_OPT)
#include "opt_ntp.h"
#include "opt_sysv.h"
#include "opt_compat_43.h"
#endif
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/signal.h>
#include <sys/mount.h>
#include <sys/poll.h>
#include <sys/ioctl_compat.h>
#include <sys/syscallargs.h>
#include <compat/svr4/svr4_types.h>
#include <compat/svr4/svr4_signal.h>
#include <compat/svr4/svr4_ucontext.h>
#include <compat/svr4/svr4_lwp.h>
#include <compat/svr4/svr4_statvfs.h>
#include <compat/svr4/svr4_syscallargs.h>
#include <compat/irix/irix_types.h>
#include <compat/irix/irix_signal.h>
#include <compat/irix/irix_syscallargs.h>
#endif /* _KERNEL_OPT */

const char *const irix_syscallnames[] = {
	"syscall",			/* 0 = syscall */
	"exit",			/* 1 = exit */
	"fork",			/* 2 = fork */
	"read",			/* 3 = read */
	"write",			/* 4 = write */
	"open",			/* 5 = open */
	"close",			/* 6 = close */
	"#7 (obsolete wait)",		/* 7 = obsolete wait */
	"creat",			/* 8 = creat */
	"link",			/* 9 = link */
	"unlink",			/* 10 = unlink */
	"execv",			/* 11 = execv */
	"chdir",			/* 12 = chdir */
	"time",			/* 13 = time */
	"#14 (obsolete mknod)",		/* 14 = obsolete mknod */
	"chmod",			/* 15 = chmod */
	"chown",			/* 16 = chown */
	"break",			/* 17 = break */
	"#18 (obsolete stat)",		/* 18 = obsolete stat */
	"lseek",			/* 19 = lseek */
	"getpid",			/* 20 = getpid */
	"#21 (unimplemented old_mount)",		/* 21 = unimplemented old_mount */
	"#22 (unimplemented System V umount)",		/* 22 = unimplemented System V umount */
	"setuid",			/* 23 = setuid */
	"getuid_with_euid",			/* 24 = getuid_with_euid */
	"#25 (unimplemented stime)",		/* 25 = unimplemented stime */
	"#26 (unimplemented ptrace)",		/* 26 = unimplemented ptrace */
	"alarm",			/* 27 = alarm */
	"fstat",			/* 28 = fstat */
	"pause",			/* 29 = pause */
	"utime",			/* 30 = utime */
	"#31 (unimplemented was stty)",		/* 31 = unimplemented was stty */
	"#32 (unimplemented was gtty)",		/* 32 = unimplemented was gtty */
	"access",			/* 33 = access */
	"nice",			/* 34 = nice */
	"#35 (unimplemented statfs)",		/* 35 = unimplemented statfs */
	"sync",			/* 36 = sync */
	"kill",			/* 37 = kill */
	"#38 (unimplemented fstatfs)",		/* 38 = unimplemented fstatfs */
	"pgrpsys",			/* 39 = pgrpsys */
	"syssgi",			/* 40 = syssgi */
	"dup",			/* 41 = dup */
	"pipe",			/* 42 = pipe */
	"times",			/* 43 = times */
	"#44 (unimplemented profil)",		/* 44 = unimplemented profil */
	"#45 (unimplemented plock)",		/* 45 = unimplemented plock */
	"setgid",			/* 46 = setgid */
	"getgid_with_egid",			/* 47 = getgid_with_egid */
	"#48 (obsolete ssig)",		/* 48 = obsolete ssig */
#ifdef SYSVMSG
	"msgsys",			/* 49 = msgsys */
#else
	"#49 (unimplemented msgsys)",		/* 49 = unimplemented msgsys */
#endif
	"#50 (unimplemented sysmips)",		/* 50 = unimplemented sysmips */
	"#51 (unimplemented acct)",		/* 51 = unimplemented acct */
#ifdef SYSVSHM
	"shmsys",			/* 52 = shmsys */
#else
	"#52 (unimplemented shmsys)",		/* 52 = unimplemented shmsys */
#endif
#ifdef SYSVSEM
	"semsys",			/* 53 = semsys */
#else
	"#53 (unimplemented semsys)",		/* 53 = unimplemented semsys */
#endif
	"ioctl",			/* 54 = ioctl */
	"#55 (unimplemented uadmin)",		/* 55 = unimplemented uadmin */
	"sysmp",			/* 56 = sysmp */
	"utssys",			/* 57 = utssys */
	"#58 (unimplemented)",		/* 58 = unimplemented */
	"execve",			/* 59 = execve */
	"umask",			/* 60 = umask */
	"chroot",			/* 61 = chroot */
	"fcntl",			/* 62 = fcntl */
	"ulimit",			/* 63 = ulimit */
	"#64 (unimplemented reserved for unix/pc)",		/* 64 = unimplemented reserved for unix/pc */
	"#65 (unimplemented reserved for unix/pc)",		/* 65 = unimplemented reserved for unix/pc */
	"#66 (unimplemented reserved for unix/pc)",		/* 66 = unimplemented reserved for unix/pc */
	"#67 (unimplemented reserved for unix/pc)",		/* 67 = unimplemented reserved for unix/pc */
	"#68 (unimplemented reserved for unix/pc)",		/* 68 = unimplemented reserved for unix/pc */
	"#69 (unimplemented reserved for unix/pc)",		/* 69 = unimplemented reserved for unix/pc */
	"#70 (obsolete advfs)",		/* 70 = obsolete advfs */
	"#71 (obsolete unadvfs)",		/* 71 = obsolete unadvfs */
	"#72 (obsolete rmount)",		/* 72 = obsolete rmount */
	"#73 (obsolete rumount)",		/* 73 = obsolete rumount */
	"#74 (obsolete rfstart)",		/* 74 = obsolete rfstart */
	"getrlimit64",			/* 75 = getrlimit64 */
	"setrlimit64",			/* 76 = setrlimit64 */
	"nanosleep",			/* 77 = nanosleep */
	"lseek64",			/* 78 = lseek64 */
	"rmdir",			/* 79 = rmdir */
	"mkdir",			/* 80 = mkdir */
	"getdents",			/* 81 = getdents */
	"sginap",			/* 82 = sginap */
	"#83 (unimplemented sgikopt)",		/* 83 = unimplemented sgikopt */
	"#84 (unimplemented sysfs)",		/* 84 = unimplemented sysfs */
	"getmsg",			/* 85 = getmsg */
	"putmsg",			/* 86 = putmsg */
	"poll",			/* 87 = poll */
	"sigreturn",			/* 88 = sigreturn */
	"accept",			/* 89 = accept */
	"bind",			/* 90 = bind */
	"connect",			/* 91 = connect */
	"gethostid",			/* 92 = gethostid */
	"getpeername",			/* 93 = getpeername */
	"getsockname",			/* 94 = getsockname */
	"getsockopt",			/* 95 = getsockopt */
	"listen",			/* 96 = listen */
	"recv",			/* 97 = recv */
	"recvfrom",			/* 98 = recvfrom */
	"recvmsg",			/* 99 = recvmsg */
	"select",			/* 100 = select */
	"send",			/* 101 = send */
	"sendmsg",			/* 102 = sendmsg */
	"sendto",			/* 103 = sendto */
	"sethostid",			/* 104 = sethostid */
	"setsockopt",			/* 105 = setsockopt */
	"shutdown",			/* 106 = shutdown */
	"socket",			/* 107 = socket */
	"gethostname",			/* 108 = gethostname */
	"sethostname",			/* 109 = sethostname */
	"getdomainname",			/* 110 = getdomainname */
	"setdomainname",			/* 111 = setdomainname */
	"truncate",			/* 112 = truncate */
	"ftruncate",			/* 113 = ftruncate */
	"rename",			/* 114 = rename */
	"symlink",			/* 115 = symlink */
	"readlink",			/* 116 = readlink */
	"#117 (unimplemented lstat)",		/* 117 = unimplemented lstat */
	"#118 (unimplemented)",		/* 118 = unimplemented */
	"#119 (unimplemented nfs_svc)",		/* 119 = unimplemented nfs_svc */
	"#120 (unimplemented nfs_getfh)",		/* 120 = unimplemented nfs_getfh */
	"#121 (unimplemented async_daemon)",		/* 121 = unimplemented async_daemon */
	"#122 (unimplemented exportfs)",		/* 122 = unimplemented exportfs */
	"setregid",			/* 123 = setregid */
	"setreuid",			/* 124 = setreuid */
	"getitimer",			/* 125 = getitimer */
	"setitimer",			/* 126 = setitimer */
	"adjtime",			/* 127 = adjtime */
	"gettimeofday",			/* 128 = gettimeofday */
	"sproc",			/* 129 = sproc */
	"prctl",			/* 130 = prctl */
	"procblk",			/* 131 = procblk */
	"sprocsp",			/* 132 = sprocsp */
	"#133 (unimplemented sgigsc)",		/* 133 = unimplemented sgigsc */
	"mmap",			/* 134 = mmap */
	"munmap",			/* 135 = munmap */
	"mprotect",			/* 136 = mprotect */
	"__msync13",			/* 137 = __msync13 */
	"#138 (unimplemented madvise)",		/* 138 = unimplemented madvise */
	"#139 (unimplemented pagelock)",		/* 139 = unimplemented pagelock */
	"#140 (unimplemented getpagesize)",		/* 140 = unimplemented getpagesize */
	"#141 (unimplemented quotactl)",		/* 141 = unimplemented quotactl */
	"#142 (unimplemented)",		/* 142 = unimplemented */
	"getpgrp",			/* 143 = getpgrp */
	"setpgrp",			/* 144 = setpgrp */
	"#145 (unimplemented vhangup)",		/* 145 = unimplemented vhangup */
	"fsync",			/* 146 = fsync */
	"fchdir",			/* 147 = fchdir */
	"getrlimit",			/* 148 = getrlimit */
	"setrlimit",			/* 149 = setrlimit */
	"#150 (unimplemented cacheflush)",		/* 150 = unimplemented cacheflush */
	"#151 (unimplemented cachectl)",		/* 151 = unimplemented cachectl */
	"fchown",			/* 152 = fchown */
	"fchmod",			/* 153 = fchmod */
	"#154 (unimplemented wait3)",		/* 154 = unimplemented wait3 */
	"#155 (unimplemented socketpair)",		/* 155 = unimplemented socketpair */
	"systeminfo",			/* 156 = systeminfo */
	"#157 (unimplemented uname)",		/* 157 = unimplemented uname */
	"xstat",			/* 158 = xstat */
	"lxstat",			/* 159 = lxstat */
	"fxstat",			/* 160 = fxstat */
	"#161 (unimplemented xmknod)",		/* 161 = unimplemented xmknod */
	"sigaction",			/* 162 = sigaction */
	"sigpending",			/* 163 = sigpending */
	"sigprocmask",			/* 164 = sigprocmask */
	"sigsuspend",			/* 165 = sigsuspend */
	"#166 (unimplemented sigpoll_sys)",		/* 166 = unimplemented sigpoll_sys */
	"swapctl",			/* 167 = swapctl */
	"getcontext",			/* 168 = getcontext */
	"setcontext",			/* 169 = setcontext */
	"waitsys",			/* 170 = waitsys */
	"#171 (unimplemented sigstack)",		/* 171 = unimplemented sigstack */
	"#172 (unimplemented sigaltstack)",		/* 172 = unimplemented sigaltstack */
	"#173 (unimplemented sigsendset)",		/* 173 = unimplemented sigsendset */
	"statvfs",			/* 174 = statvfs */
	"fstatvfs",			/* 175 = fstatvfs */
	"#176 (unimplemented getpmsg)",		/* 176 = unimplemented getpmsg */
	"#177 (unimplemented putpmsg)",		/* 177 = unimplemented putpmsg */
	"#178 (unimplemented lchown)",		/* 178 = unimplemented lchown */
	"#179 (unimplemented priocntl)",		/* 179 = unimplemented priocntl */
	"#180 (unimplemented sigqueue)",		/* 180 = unimplemented sigqueue */
	"readv",			/* 181 = readv */
	"writev",			/* 182 = writev */
	"truncate64",			/* 183 = truncate64 */
	"ftruncate64",			/* 184 = ftruncate64 */
	"mmap64",			/* 185 = mmap64 */
	"#186 (unimplemented dmi)",		/* 186 = unimplemented dmi */
	"pread",			/* 187 = pread */
	"pwrite",			/* 188 = pwrite */
	"#189 (unimplemented fdatasync)",		/* 189 = unimplemented fdatasync */
	"#190 (unimplemented sgifastpath)",		/* 190 = unimplemented sgifastpath */
	"#191 (unimplemented attr_get)",		/* 191 = unimplemented attr_get */
	"#192 (unimplemented attr_getf)",		/* 192 = unimplemented attr_getf */
	"#193 (unimplemented attr_set)",		/* 193 = unimplemented attr_set */
	"#194 (unimplemented attr_setf)",		/* 194 = unimplemented attr_setf */
	"#195 (unimplemented attr_remove)",		/* 195 = unimplemented attr_remove */
	"#196 (unimplemented attr_removef)",		/* 196 = unimplemented attr_removef */
	"#197 (unimplemented attr_list)",		/* 197 = unimplemented attr_list */
	"#198 (unimplemented attr_listf)",		/* 198 = unimplemented attr_listf */
	"#199 (unimplemented attr_multi)",		/* 199 = unimplemented attr_multi */
	"#200 (unimplemented attr_multif)",		/* 200 = unimplemented attr_multif */
	"#201 (unimplemented statvfs64)",		/* 201 = unimplemented statvfs64 */
	"#202 (unimplemented fstatvfs64)",		/* 202 = unimplemented fstatvfs64 */
	"getmountid",			/* 203 = getmountid */
	"#204 (unimplemented nsproc)",		/* 204 = unimplemented nsproc */
	"getdents64",			/* 205 = getdents64 */
	"#206 (unimplemented afs_syscall)",		/* 206 = unimplemented afs_syscall */
	"ngetdents",			/* 207 = ngetdents */
	"ngetdents64",			/* 208 = ngetdents64 */
	"#209 (unimplemented sgi_sesmgr)",		/* 209 = unimplemented sgi_sesmgr */
	"pidsprocsp",			/* 210 = pidsprocsp */
	"#211 (unimplemented rexec)",		/* 211 = unimplemented rexec */
	"#212 (unimplemented timer_create)",		/* 212 = unimplemented timer_create */
	"#213 (unimplemented timer_delete)",		/* 213 = unimplemented timer_delete */
	"#214 (unimplemented timer_settime)",		/* 214 = unimplemented timer_settime */
	"#215 (unimplemented timer_gettime)",		/* 215 = unimplemented timer_gettime */
	"#216 (unimplemented timer_setoverrun)",		/* 216 = unimplemented timer_setoverrun */
	"#217 (unimplemented sched_rr_get_interval)",		/* 217 = unimplemented sched_rr_get_interval */
	"#218 (unimplemented sched_yield)",		/* 218 = unimplemented sched_yield */
	"#219 (unimplemented sched_getscheduler)",		/* 219 = unimplemented sched_getscheduler */
	"#220 (unimplemented sched_setscheduler)",		/* 220 = unimplemented sched_setscheduler */
	"#221 (unimplemented sched_getparam)",		/* 221 = unimplemented sched_getparam */
	"#222 (unimplemented sched_setparam)",		/* 222 = unimplemented sched_setparam */
	"usync_cntl",			/* 223 = usync_cntl */
	"#224 (unimplemented psema_cntl)",		/* 224 = unimplemented psema_cntl */
	"#225 (unimplemented restartreturn)",		/* 225 = unimplemented restartreturn */
	"#226 (unimplemented sysget)",		/* 226 = unimplemented sysget */
	"#227 (unimplemented xpg4_recvmsg)",		/* 227 = unimplemented xpg4_recvmsg */
	"#228 (unimplemented umfscall)",		/* 228 = unimplemented umfscall */
	"#229 (unimplemented nsproctid)",		/* 229 = unimplemented nsproctid */
	"#230 (unimplemented rexec_complete)",		/* 230 = unimplemented rexec_complete */
	"#231 (unimplemented xpg4_sigaltstack)",		/* 231 = unimplemented xpg4_sigaltstack */
	"#232 (unimplemented xpg4_sigaltstack)",		/* 232 = unimplemented xpg4_sigaltstack */
	"#233 (unimplemented xpg4_setregid)",		/* 233 = unimplemented xpg4_setregid */
	"#234 (unimplemented linkfollow)",		/* 234 = unimplemented linkfollow */
	"#235 (unimplemented utimets)",		/* 235 = unimplemented utimets */
};
