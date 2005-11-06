/* $NetBSD: linux_syscalls.c,v 1.62 2005/11/06 21:50:28 manu Exp $ */

/*
 * System call names.
 *
 * DO NOT EDIT-- this file is automatically generated.
 * created from	NetBSD: syscalls.master,v 1.76 2005/11/06 18:16:31 manu Exp
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: linux_syscalls.c,v 1.62 2005/11/06 21:50:28 manu Exp $");

#if defined(_KERNEL_OPT)
#if defined(_KERNEL_OPT)
#include "opt_compat_43.h"
#endif
#include <sys/param.h>
#include <sys/poll.h>
#include <sys/systm.h>
#include <sys/signal.h>
#include <sys/mount.h>
#include <sys/sa.h>
#include <sys/syscallargs.h>
#include <compat/linux/common/linux_types.h>
#include <compat/linux/common/linux_mmap.h>
#include <compat/linux/common/linux_signal.h>
#include <compat/linux/common/linux_siginfo.h>
#include <compat/linux/common/linux_machdep.h>
#include <compat/linux/linux_syscallargs.h>
#endif /* _KERNEL_OPT */

const char *const linux_syscallnames[] = {
	"syscall",			/* 0 = syscall */
	"exit",			/* 1 = exit */
	"fork",			/* 2 = fork */
	"read",			/* 3 = read */
	"write",			/* 4 = write */
	"open",			/* 5 = open */
	"close",			/* 6 = close */
	"waitpid",			/* 7 = waitpid */
	"creat",			/* 8 = creat */
	"link",			/* 9 = link */
	"unlink",			/* 10 = unlink */
	"execve",			/* 11 = execve */
	"chdir",			/* 12 = chdir */
	"time",			/* 13 = time */
	"mknod",			/* 14 = mknod */
	"chmod",			/* 15 = chmod */
	"lchown16",			/* 16 = lchown16 */
	"break",			/* 17 = break */
	"#18 (obsolete ostat)",		/* 18 = obsolete ostat */
	"lseek",			/* 19 = lseek */
	"getpid",			/* 20 = getpid */
	"#21 (unimplemented mount)",		/* 21 = unimplemented mount */
	"#22 (unimplemented umount)",		/* 22 = unimplemented umount */
	"linux_setuid16",			/* 23 = linux_setuid16 */
	"linux_getuid16",			/* 24 = linux_getuid16 */
	"stime",			/* 25 = stime */
	"ptrace",			/* 26 = ptrace */
	"alarm",			/* 27 = alarm */
	"#28 (obsolete ofstat)",		/* 28 = obsolete ofstat */
	"pause",			/* 29 = pause */
	"utime",			/* 30 = utime */
	"#31 (obsolete stty)",		/* 31 = obsolete stty */
	"#32 (obsolete gtty)",		/* 32 = obsolete gtty */
	"access",			/* 33 = access */
	"nice",			/* 34 = nice */
	"#35 (obsolete ftime)",		/* 35 = obsolete ftime */
	"sync",			/* 36 = sync */
	"kill",			/* 37 = kill */
	"rename",			/* 38 = rename */
	"mkdir",			/* 39 = mkdir */
	"rmdir",			/* 40 = rmdir */
	"dup",			/* 41 = dup */
	"pipe",			/* 42 = pipe */
	"times",			/* 43 = times */
	"#44 (obsolete prof)",		/* 44 = obsolete prof */
	"brk",			/* 45 = brk */
	"linux_setgid16",			/* 46 = linux_setgid16 */
	"linux_getgid16",			/* 47 = linux_getgid16 */
	"signal",			/* 48 = signal */
	"linux_geteuid16",			/* 49 = linux_geteuid16 */
	"linux_getegid16",			/* 50 = linux_getegid16 */
	"acct",			/* 51 = acct */
	"#52 (obsolete phys)",		/* 52 = obsolete phys */
	"#53 (obsolete lock)",		/* 53 = obsolete lock */
	"ioctl",			/* 54 = ioctl */
	"fcntl",			/* 55 = fcntl */
	"#56 (obsolete mpx)",		/* 56 = obsolete mpx */
	"setpgid",			/* 57 = setpgid */
	"#58 (obsolete ulimit)",		/* 58 = obsolete ulimit */
	"oldolduname",			/* 59 = oldolduname */
	"umask",			/* 60 = umask */
	"chroot",			/* 61 = chroot */
	"#62 (unimplemented ustat)",		/* 62 = unimplemented ustat */
	"dup2",			/* 63 = dup2 */
	"getppid",			/* 64 = getppid */
	"getpgrp",			/* 65 = getpgrp */
	"setsid",			/* 66 = setsid */
	"sigaction",			/* 67 = sigaction */
	"siggetmask",			/* 68 = siggetmask */
	"sigsetmask",			/* 69 = sigsetmask */
	"setreuid16",			/* 70 = setreuid16 */
	"setregid16",			/* 71 = setregid16 */
	"sigsuspend",			/* 72 = sigsuspend */
	"sigpending",			/* 73 = sigpending */
	"sethostname",			/* 74 = sethostname */
	"setrlimit",			/* 75 = setrlimit */
	"getrlimit",			/* 76 = getrlimit */
	"getrusage",			/* 77 = getrusage */
	"gettimeofday",			/* 78 = gettimeofday */
	"settimeofday",			/* 79 = settimeofday */
	"getgroups16",			/* 80 = getgroups16 */
	"setgroups16",			/* 81 = setgroups16 */
	"oldselect",			/* 82 = oldselect */
	"symlink",			/* 83 = symlink */
	"oolstat",			/* 84 = oolstat */
	"readlink",			/* 85 = readlink */
#ifdef EXEC_AOUT
	"uselib",			/* 86 = uselib */
#else
	"#86 (unimplemented sys_uselib)",		/* 86 = unimplemented sys_uselib */
#endif
	"swapon",			/* 87 = swapon */
	"reboot",			/* 88 = reboot */
	"readdir",			/* 89 = readdir */
	"old_mmap",			/* 90 = old_mmap */
	"munmap",			/* 91 = munmap */
	"truncate",			/* 92 = truncate */
	"ftruncate",			/* 93 = ftruncate */
	"fchmod",			/* 94 = fchmod */
	"fchown16",			/* 95 = fchown16 */
	"getpriority",			/* 96 = getpriority */
	"setpriority",			/* 97 = setpriority */
	"profil",			/* 98 = profil */
	"statfs",			/* 99 = statfs */
	"fstatfs",			/* 100 = fstatfs */
	"ioperm",			/* 101 = ioperm */
	"socketcall",			/* 102 = socketcall */
	"#103 (unimplemented syslog)",		/* 103 = unimplemented syslog */
	"setitimer",			/* 104 = setitimer */
	"getitimer",			/* 105 = getitimer */
	"stat",			/* 106 = stat */
	"lstat",			/* 107 = lstat */
	"fstat",			/* 108 = fstat */
	"olduname",			/* 109 = olduname */
	"iopl",			/* 110 = iopl */
	"#111 (unimplemented vhangup)",		/* 111 = unimplemented vhangup */
	"#112 (unimplemented idle)",		/* 112 = unimplemented idle */
	"#113 (unimplemented vm86old)",		/* 113 = unimplemented vm86old */
	"wait4",			/* 114 = wait4 */
	"swapoff",			/* 115 = swapoff */
	"sysinfo",			/* 116 = sysinfo */
	"ipc",			/* 117 = ipc */
	"fsync",			/* 118 = fsync */
	"sigreturn",			/* 119 = sigreturn */
	"clone",			/* 120 = clone */
	"setdomainname",			/* 121 = setdomainname */
	"uname",			/* 122 = uname */
	"modify_ldt",			/* 123 = modify_ldt */
	"#124 (unimplemented adjtimex)",		/* 124 = unimplemented adjtimex */
	"mprotect",			/* 125 = mprotect */
	"sigprocmask",			/* 126 = sigprocmask */
	"#127 (unimplemented create_module)",		/* 127 = unimplemented create_module */
	"#128 (unimplemented init_module)",		/* 128 = unimplemented init_module */
	"#129 (unimplemented delete_module)",		/* 129 = unimplemented delete_module */
	"#130 (unimplemented get_kernel_syms)",		/* 130 = unimplemented get_kernel_syms */
	"#131 (unimplemented quotactl)",		/* 131 = unimplemented quotactl */
	"getpgid",			/* 132 = getpgid */
	"fchdir",			/* 133 = fchdir */
	"#134 (unimplemented bdflush)",		/* 134 = unimplemented bdflush */
	"#135 (unimplemented sysfs)",		/* 135 = unimplemented sysfs */
	"personality",			/* 136 = personality */
	"#137 (unimplemented afs_syscall)",		/* 137 = unimplemented afs_syscall */
	"linux_setfsuid16",			/* 138 = linux_setfsuid16 */
	"linux_getfsuid16",			/* 139 = linux_getfsuid16 */
	"llseek",			/* 140 = llseek */
	"getdents",			/* 141 = getdents */
	"select",			/* 142 = select */
	"flock",			/* 143 = flock */
	"msync",			/* 144 = msync */
	"readv",			/* 145 = readv */
	"writev",			/* 146 = writev */
	"getsid",			/* 147 = getsid */
	"fdatasync",			/* 148 = fdatasync */
	"__sysctl",			/* 149 = __sysctl */
	"mlock",			/* 150 = mlock */
	"munlock",			/* 151 = munlock */
	"mlockall",			/* 152 = mlockall */
	"munlockall",			/* 153 = munlockall */
	"sched_setparam",			/* 154 = sched_setparam */
	"sched_getparam",			/* 155 = sched_getparam */
	"sched_setscheduler",			/* 156 = sched_setscheduler */
	"sched_getscheduler",			/* 157 = sched_getscheduler */
	"sched_yield",			/* 158 = sched_yield */
	"sched_get_priority_max",			/* 159 = sched_get_priority_max */
	"sched_get_priority_min",			/* 160 = sched_get_priority_min */
	"#161 (unimplemented sys_sched_rr_get_interval)",		/* 161 = unimplemented sys_sched_rr_get_interval */
	"nanosleep",			/* 162 = nanosleep */
	"mremap",			/* 163 = mremap */
	"setresuid16",			/* 164 = setresuid16 */
	"linux_getresuid16",			/* 165 = linux_getresuid16 */
	"#166 (unimplemented vm86)",		/* 166 = unimplemented vm86 */
	"#167 (unimplemented query_module)",		/* 167 = unimplemented query_module */
	"poll",			/* 168 = poll */
	"#169 (unimplemented nfsservctl)",		/* 169 = unimplemented nfsservctl */
	"setresgid16",			/* 170 = setresgid16 */
	"linux_getresgid16",			/* 171 = linux_getresgid16 */
	"#172 (unimplemented prctl)",		/* 172 = unimplemented prctl */
	"rt_sigreturn",			/* 173 = rt_sigreturn */
	"rt_sigaction",			/* 174 = rt_sigaction */
	"rt_sigprocmask",			/* 175 = rt_sigprocmask */
	"rt_sigpending",			/* 176 = rt_sigpending */
	"#177 (unimplemented rt_sigtimedwait)",		/* 177 = unimplemented rt_sigtimedwait */
	"rt_queueinfo",			/* 178 = rt_queueinfo */
	"rt_sigsuspend",			/* 179 = rt_sigsuspend */
	"pread",			/* 180 = pread */
	"pwrite",			/* 181 = pwrite */
	"chown16",			/* 182 = chown16 */
	"__getcwd",			/* 183 = __getcwd */
	"#184 (unimplemented capget)",		/* 184 = unimplemented capget */
	"#185 (unimplemented capset)",		/* 185 = unimplemented capset */
	"sigaltstack",			/* 186 = sigaltstack */
	"#187 (unimplemented sendfile)",		/* 187 = unimplemented sendfile */
	"#188 (unimplemented getpmsg)",		/* 188 = unimplemented getpmsg */
	"#189 (unimplemented putpmsg)",		/* 189 = unimplemented putpmsg */
	"__vfork14",			/* 190 = __vfork14 */
	"ugetrlimit",			/* 191 = ugetrlimit */
	"mmap2",			/* 192 = mmap2 */
	"truncate64",			/* 193 = truncate64 */
	"ftruncate64",			/* 194 = ftruncate64 */
	"stat64",			/* 195 = stat64 */
	"lstat64",			/* 196 = lstat64 */
	"fstat64",			/* 197 = fstat64 */
	"lchown",			/* 198 = lchown */
	"getuid",			/* 199 = getuid */
	"getgid",			/* 200 = getgid */
	"geteuid",			/* 201 = geteuid */
	"getegid",			/* 202 = getegid */
	"setreuid",			/* 203 = setreuid */
	"setregid",			/* 204 = setregid */
	"getgroups",			/* 205 = getgroups */
	"setgroups",			/* 206 = setgroups */
	"__posix_fchown",			/* 207 = __posix_fchown */
	"setresuid",			/* 208 = setresuid */
	"getresuid",			/* 209 = getresuid */
	"setresgid",			/* 210 = setresgid */
	"getresgid",			/* 211 = getresgid */
	"chown",			/* 212 = chown */
	"setuid",			/* 213 = setuid */
	"setgid",			/* 214 = setgid */
	"setfsuid",			/* 215 = setfsuid */
	"getfsuid",			/* 216 = getfsuid */
	"#217 (unimplemented pivot_root)",		/* 217 = unimplemented pivot_root */
	"mincore",			/* 218 = mincore */
	"madvise",			/* 219 = madvise */
	"getdents64",			/* 220 = getdents64 */
	"fcntl64",			/* 221 = fcntl64 */
	"#222 (unimplemented / * unused * /)",		/* 222 = unimplemented / * unused * / */
	"#223 (unimplemented / * unused * /)",		/* 223 = unimplemented / * unused * / */
	"#224 (unimplemented gettid)",		/* 224 = unimplemented gettid */
	"#225 (unimplemented readahead)",		/* 225 = unimplemented readahead */
	"setxattr",			/* 226 = setxattr */
	"lsetxattr",			/* 227 = lsetxattr */
	"fsetxattr",			/* 228 = fsetxattr */
	"getxattr",			/* 229 = getxattr */
	"lgetxattr",			/* 230 = lgetxattr */
	"fgetxattr",			/* 231 = fgetxattr */
	"listxattr",			/* 232 = listxattr */
	"llistxattr",			/* 233 = llistxattr */
	"flistxattr",			/* 234 = flistxattr */
	"removexattr",			/* 235 = removexattr */
	"lremovexattr",			/* 236 = lremovexattr */
	"fremovexattr",			/* 237 = fremovexattr */
	"#238 (unimplemented tkill)",		/* 238 = unimplemented tkill */
	"#239 (unimplemented sendfile64)",		/* 239 = unimplemented sendfile64 */
	"#240 (unimplemented futex)",		/* 240 = unimplemented futex */
	"#241 (unimplemented sched_setaffinity)",		/* 241 = unimplemented sched_setaffinity */
	"#242 (unimplemented sched_getaffinity)",		/* 242 = unimplemented sched_getaffinity */
	"#243 (unimplemented set_thread_area)",		/* 243 = unimplemented set_thread_area */
	"#244 (unimplemented get_thread_area)",		/* 244 = unimplemented get_thread_area */
	"#245 (unimplemented io_setup)",		/* 245 = unimplemented io_setup */
	"#246 (unimplemented io_destroy)",		/* 246 = unimplemented io_destroy */
	"#247 (unimplemented io_getevents)",		/* 247 = unimplemented io_getevents */
	"#248 (unimplemented io_submit)",		/* 248 = unimplemented io_submit */
	"#249 (unimplemented io_cancel)",		/* 249 = unimplemented io_cancel */
	"#250 (unimplemented fadvise64)",		/* 250 = unimplemented fadvise64 */
	"#251 (unimplemented / * unused * /)",		/* 251 = unimplemented / * unused * / */
	"exit_group",			/* 252 = exit_group */
	"#253 (unimplemented lookup_dcookie)",		/* 253 = unimplemented lookup_dcookie */
	"#254 (unimplemented epoll_create)",		/* 254 = unimplemented epoll_create */
	"#255 (unimplemented epoll_ctl)",		/* 255 = unimplemented epoll_ctl */
	"#256 (unimplemented epoll_wait)",		/* 256 = unimplemented epoll_wait */
	"#257 (unimplemented remap_file_pages)",		/* 257 = unimplemented remap_file_pages */
	"#258 (unimplemented set_tid_address)",		/* 258 = unimplemented set_tid_address */
	"#259 (unimplemented timer_create)",		/* 259 = unimplemented timer_create */
	"#260 (unimplemented timer_settime)",		/* 260 = unimplemented timer_settime */
	"#261 (unimplemented timer_gettime)",		/* 261 = unimplemented timer_gettime */
	"#262 (unimplemented timer_getoverrun)",		/* 262 = unimplemented timer_getoverrun */
	"#263 (unimplemented timer_delete)",		/* 263 = unimplemented timer_delete */
	"clock_settime",			/* 264 = clock_settime */
	"clock_gettime",			/* 265 = clock_gettime */
	"clock_getres",			/* 266 = clock_getres */
	"clock_nanosleep",			/* 267 = clock_nanosleep */
	"statfs64",			/* 268 = statfs64 */
	"fstatfs64",			/* 269 = fstatfs64 */
	"#270 (unimplemented tgkill)",		/* 270 = unimplemented tgkill */
	"#271 (unimplemented utimes)",		/* 271 = unimplemented utimes */
	"#272 (unimplemented fadvise64_64)",		/* 272 = unimplemented fadvise64_64 */
	"#273 (unimplemented vserver)",		/* 273 = unimplemented vserver */
	"#274 (unimplemented mbind)",		/* 274 = unimplemented mbind */
	"#275 (unimplemented get_mempolicy)",		/* 275 = unimplemented get_mempolicy */
	"#276 (unimplemented set_mempolicy)",		/* 276 = unimplemented set_mempolicy */
	"#277 (unimplemented mq_open)",		/* 277 = unimplemented mq_open */
	"#278 (unimplemented mq_unlink)",		/* 278 = unimplemented mq_unlink */
	"#279 (unimplemented mq_timedsend)",		/* 279 = unimplemented mq_timedsend */
	"#280 (unimplemented mq_timedreceive)",		/* 280 = unimplemented mq_timedreceive */
	"#281 (unimplemented mq_notify)",		/* 281 = unimplemented mq_notify */
	"#282 (unimplemented mq_getsetattr)",		/* 282 = unimplemented mq_getsetattr */
	"#283 (unimplemented sys_kexec_load)",		/* 283 = unimplemented sys_kexec_load */
};
