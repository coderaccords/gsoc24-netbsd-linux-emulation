/* $NetBSD: linux_syscalls.c,v 1.72 2007/12/21 22:28:43 njoly Exp $ */

/*
 * System call names.
 *
 * DO NOT EDIT-- this file is automatically generated.
 * created from	NetBSD: syscalls.master,v 1.87 2007/12/21 22:26:21 njoly Exp
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: linux_syscalls.c,v 1.72 2007/12/21 22:28:43 njoly Exp $");

#if defined(_KERNEL_OPT)
#if defined(_KERNEL_OPT)
#include "opt_compat_43.h"
#endif
#include <sys/param.h>
#include <sys/poll.h>
#include <sys/systm.h>
#include <sys/signal.h>
#include <sys/mount.h>
#include <sys/syscallargs.h>
#include <compat/linux/common/linux_types.h>
#include <compat/linux/common/linux_mmap.h>
#include <compat/linux/common/linux_signal.h>
#include <compat/linux/common/linux_siginfo.h>
#include <compat/linux/common/linux_machdep.h>
#include <compat/linux/linux_syscallargs.h>
#endif /* _KERNEL_OPT */

const char *const linux_syscallnames[] = {
	/*   0 */	"syscall",
	/*   1 */	"exit",
	/*   2 */	"fork",
	/*   3 */	"read",
	/*   4 */	"write",
	/*   5 */	"open",
	/*   6 */	"close",
	/*   7 */	"waitpid",
	/*   8 */	"creat",
	/*   9 */	"link",
	/*  10 */	"unlink",
	/*  11 */	"execve",
	/*  12 */	"chdir",
	/*  13 */	"time",
	/*  14 */	"mknod",
	/*  15 */	"chmod",
	/*  16 */	"lchown16",
	/*  17 */	"break",
	/*  18 */	"#18 (obsolete ostat)",
	/*  19 */	"lseek",
#ifdef	LINUX_NPTL
	/*  20 */	"getpid",
#else
	/*  20 */	"getpid",
#endif
	/*  21 */	"#21 (unimplemented mount)",
	/*  22 */	"#22 (unimplemented umount)",
	/*  23 */	"linux_setuid16",
	/*  24 */	"linux_getuid16",
	/*  25 */	"stime",
	/*  26 */	"ptrace",
	/*  27 */	"alarm",
	/*  28 */	"#28 (obsolete ofstat)",
	/*  29 */	"pause",
	/*  30 */	"utime",
	/*  31 */	"#31 (obsolete stty)",
	/*  32 */	"#32 (obsolete gtty)",
	/*  33 */	"access",
	/*  34 */	"nice",
	/*  35 */	"#35 (obsolete ftime)",
	/*  36 */	"sync",
	/*  37 */	"kill",
	/*  38 */	"__posix_rename",
	/*  39 */	"mkdir",
	/*  40 */	"rmdir",
	/*  41 */	"dup",
	/*  42 */	"pipe",
	/*  43 */	"times",
	/*  44 */	"#44 (obsolete prof)",
	/*  45 */	"brk",
	/*  46 */	"linux_setgid16",
	/*  47 */	"linux_getgid16",
	/*  48 */	"signal",
	/*  49 */	"linux_geteuid16",
	/*  50 */	"linux_getegid16",
	/*  51 */	"acct",
	/*  52 */	"#52 (obsolete phys)",
	/*  53 */	"#53 (obsolete lock)",
	/*  54 */	"ioctl",
	/*  55 */	"fcntl",
	/*  56 */	"#56 (obsolete mpx)",
	/*  57 */	"setpgid",
	/*  58 */	"#58 (obsolete ulimit)",
	/*  59 */	"oldolduname",
	/*  60 */	"umask",
	/*  61 */	"chroot",
	/*  62 */	"#62 (unimplemented ustat)",
	/*  63 */	"dup2",
#ifdef LINUX_NPTL
	/*  64 */	"getppid",
#else
	/*  64 */	"getppid",
#endif
	/*  65 */	"getpgrp",
	/*  66 */	"setsid",
	/*  67 */	"sigaction",
	/*  68 */	"siggetmask",
	/*  69 */	"sigsetmask",
	/*  70 */	"setreuid16",
	/*  71 */	"setregid16",
	/*  72 */	"sigsuspend",
	/*  73 */	"sigpending",
	/*  74 */	"sethostname",
	/*  75 */	"setrlimit",
	/*  76 */	"getrlimit",
	/*  77 */	"getrusage",
	/*  78 */	"gettimeofday",
	/*  79 */	"settimeofday",
	/*  80 */	"getgroups16",
	/*  81 */	"setgroups16",
	/*  82 */	"oldselect",
	/*  83 */	"symlink",
	/*  84 */	"oolstat",
	/*  85 */	"readlink",
#ifdef EXEC_AOUT
	/*  86 */	"uselib",
#else
	/*  86 */	"#86 (unimplemented sys_uselib)",
#endif
	/*  87 */	"swapon",
	/*  88 */	"reboot",
	/*  89 */	"readdir",
	/*  90 */	"old_mmap",
	/*  91 */	"munmap",
	/*  92 */	"truncate",
	/*  93 */	"ftruncate",
	/*  94 */	"fchmod",
	/*  95 */	"fchown16",
	/*  96 */	"getpriority",
	/*  97 */	"setpriority",
	/*  98 */	"profil",
	/*  99 */	"statfs",
	/* 100 */	"fstatfs",
	/* 101 */	"ioperm",
	/* 102 */	"socketcall",
	/* 103 */	"#103 (unimplemented syslog)",
	/* 104 */	"setitimer",
	/* 105 */	"getitimer",
	/* 106 */	"stat",
	/* 107 */	"lstat",
	/* 108 */	"fstat",
	/* 109 */	"olduname",
	/* 110 */	"iopl",
	/* 111 */	"#111 (unimplemented vhangup)",
	/* 112 */	"#112 (unimplemented idle)",
	/* 113 */	"#113 (unimplemented vm86old)",
	/* 114 */	"wait4",
	/* 115 */	"swapoff",
	/* 116 */	"sysinfo",
	/* 117 */	"ipc",
	/* 118 */	"fsync",
	/* 119 */	"sigreturn",
	/* 120 */	"clone",
	/* 121 */	"setdomainname",
	/* 122 */	"uname",
	/* 123 */	"modify_ldt",
	/* 124 */	"#124 (unimplemented adjtimex)",
	/* 125 */	"mprotect",
	/* 126 */	"sigprocmask",
	/* 127 */	"#127 (unimplemented create_module)",
	/* 128 */	"#128 (unimplemented init_module)",
	/* 129 */	"#129 (unimplemented delete_module)",
	/* 130 */	"#130 (unimplemented get_kernel_syms)",
	/* 131 */	"#131 (unimplemented quotactl)",
	/* 132 */	"getpgid",
	/* 133 */	"fchdir",
	/* 134 */	"#134 (unimplemented bdflush)",
	/* 135 */	"#135 (unimplemented sysfs)",
	/* 136 */	"personality",
	/* 137 */	"#137 (unimplemented afs_syscall)",
	/* 138 */	"linux_setfsuid16",
	/* 139 */	"linux_setfsgid16",
	/* 140 */	"llseek",
	/* 141 */	"getdents",
	/* 142 */	"select",
	/* 143 */	"flock",
	/* 144 */	"msync",
	/* 145 */	"readv",
	/* 146 */	"writev",
	/* 147 */	"getsid",
	/* 148 */	"fdatasync",
	/* 149 */	"__sysctl",
	/* 150 */	"mlock",
	/* 151 */	"munlock",
	/* 152 */	"mlockall",
	/* 153 */	"munlockall",
	/* 154 */	"sched_setparam",
	/* 155 */	"sched_getparam",
	/* 156 */	"sched_setscheduler",
	/* 157 */	"sched_getscheduler",
	/* 158 */	"sched_yield",
	/* 159 */	"sched_get_priority_max",
	/* 160 */	"sched_get_priority_min",
	/* 161 */	"#161 (unimplemented sys_sched_rr_get_interval)",
	/* 162 */	"nanosleep",
	/* 163 */	"mremap",
	/* 164 */	"setresuid16",
	/* 165 */	"linux_getresuid16",
	/* 166 */	"#166 (unimplemented vm86)",
	/* 167 */	"#167 (unimplemented query_module)",
	/* 168 */	"poll",
	/* 169 */	"#169 (unimplemented nfsservctl)",
	/* 170 */	"setresgid16",
	/* 171 */	"linux_getresgid16",
	/* 172 */	"#172 (unimplemented prctl)",
	/* 173 */	"rt_sigreturn",
	/* 174 */	"rt_sigaction",
	/* 175 */	"rt_sigprocmask",
	/* 176 */	"rt_sigpending",
	/* 177 */	"#177 (unimplemented rt_sigtimedwait)",
	/* 178 */	"rt_queueinfo",
	/* 179 */	"rt_sigsuspend",
	/* 180 */	"pread",
	/* 181 */	"pwrite",
	/* 182 */	"chown16",
	/* 183 */	"__getcwd",
	/* 184 */	"#184 (unimplemented capget)",
	/* 185 */	"#185 (unimplemented capset)",
	/* 186 */	"sigaltstack",
	/* 187 */	"#187 (unimplemented sendfile)",
	/* 188 */	"#188 (unimplemented getpmsg)",
	/* 189 */	"#189 (unimplemented putpmsg)",
	/* 190 */	"__vfork14",
	/* 191 */	"ugetrlimit",
#define linux_sys_mmap2_args linux_sys_mmap_args
	/* 192 */	"mmap2",
	/* 193 */	"truncate64",
	/* 194 */	"ftruncate64",
	/* 195 */	"stat64",
	/* 196 */	"lstat64",
	/* 197 */	"fstat64",
	/* 198 */	"__posix_lchown",
	/* 199 */	"getuid",
	/* 200 */	"getgid",
	/* 201 */	"geteuid",
	/* 202 */	"getegid",
	/* 203 */	"setreuid",
	/* 204 */	"setregid",
	/* 205 */	"getgroups",
	/* 206 */	"setgroups",
	/* 207 */	"__posix_fchown",
	/* 208 */	"setresuid",
	/* 209 */	"getresuid",
	/* 210 */	"setresgid",
	/* 211 */	"getresgid",
	/* 212 */	"__posix_chown",
	/* 213 */	"setuid",
	/* 214 */	"setgid",
	/* 215 */	"setfsuid",
	/* 216 */	"setfsgid",
	/* 217 */	"#217 (unimplemented pivot_root)",
	/* 218 */	"mincore",
	/* 219 */	"madvise",
	/* 220 */	"getdents64",
	/* 221 */	"fcntl64",
	/* 222 */	"#222 (unimplemented / * unused * /)",
	/* 223 */	"#223 (unimplemented / * unused * /)",
#ifdef LINUX_NPTL
	/* 224 */	"gettid",
#else
	/* 224 */	"#224 (unimplemented gettid)",
#endif
	/* 225 */	"#225 (unimplemented readahead)",
	/* 226 */	"setxattr",
	/* 227 */	"lsetxattr",
	/* 228 */	"fsetxattr",
	/* 229 */	"getxattr",
	/* 230 */	"lgetxattr",
	/* 231 */	"fgetxattr",
	/* 232 */	"listxattr",
	/* 233 */	"llistxattr",
	/* 234 */	"flistxattr",
	/* 235 */	"removexattr",
	/* 236 */	"lremovexattr",
	/* 237 */	"fremovexattr",
#ifdef LINUX_NPTL
	/* 238 */	"tkill",
#else
	/* 238 */	"#238 (unimplemented tkill)",
#endif
	/* 239 */	"#239 (unimplemented sendfile64)",
	/* 240 */	"futex",
#ifdef LINUX_NPTL
	/* 241 */	"sched_setaffinity",
	/* 242 */	"sched_getaffinity",
#else
	/* 241 */	"#241 (unimplemented setaffinity)",
	/* 242 */	"#242 (unimplemented getaffinity)",
#endif
	/* 243 */	"#243 (unimplemented set_thread_area)",
	/* 244 */	"#244 (unimplemented get_thread_area)",
	/* 245 */	"#245 (unimplemented io_setup)",
	/* 246 */	"#246 (unimplemented io_destroy)",
	/* 247 */	"#247 (unimplemented io_getevents)",
	/* 248 */	"#248 (unimplemented io_submit)",
	/* 249 */	"#249 (unimplemented io_cancel)",
	/* 250 */	"#250 (unimplemented fadvise64)",
	/* 251 */	"#251 (unimplemented / * unused * /)",
	/* 252 */	"exit_group",
	/* 253 */	"#253 (unimplemented lookup_dcookie)",
	/* 254 */	"#254 (unimplemented epoll_create)",
	/* 255 */	"#255 (unimplemented epoll_ctl)",
	/* 256 */	"#256 (unimplemented epoll_wait)",
	/* 257 */	"#257 (unimplemented remap_file_pages)",
#ifdef LINUX_NPTL
	/* 258 */	"set_tid_address",
#else
	/* 258 */	"#258 (unimplemented set_tid_address)",
#endif
	/* 259 */	"#259 (unimplemented timer_create)",
	/* 260 */	"#260 (unimplemented timer_settime)",
	/* 261 */	"#261 (unimplemented timer_gettime)",
	/* 262 */	"#262 (unimplemented timer_getoverrun)",
	/* 263 */	"#263 (unimplemented timer_delete)",
	/* 264 */	"clock_settime",
	/* 265 */	"clock_gettime",
	/* 266 */	"clock_getres",
	/* 267 */	"clock_nanosleep",
	/* 268 */	"statfs64",
	/* 269 */	"fstatfs64",
#ifdef LINUX_NPTL
	/* 270 */	"tgkill",
#else
	/* 270 */	"#270 (unimplemented tgkill)",
#endif
	/* 271 */	"#271 (unimplemented utimes)",
	/* 272 */	"#272 (unimplemented fadvise64_64)",
	/* 273 */	"#273 (unimplemented vserver)",
	/* 274 */	"#274 (unimplemented mbind)",
	/* 275 */	"#275 (unimplemented get_mempolicy)",
	/* 276 */	"#276 (unimplemented set_mempolicy)",
	/* 277 */	"#277 (unimplemented mq_open)",
	/* 278 */	"#278 (unimplemented mq_unlink)",
	/* 279 */	"#279 (unimplemented mq_timedsend)",
	/* 280 */	"#280 (unimplemented mq_timedreceive)",
	/* 281 */	"#281 (unimplemented mq_notify)",
	/* 282 */	"#282 (unimplemented mq_getsetattr)",
	/* 283 */	"#283 (unimplemented sys_kexec_load)",
};
