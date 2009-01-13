/* $NetBSD: osf1_syscalls.c,v 1.58 2009/01/13 22:33:17 pooka Exp $ */

/*
 * System call names.
 *
 * DO NOT EDIT-- this file is automatically generated.
 * created from	NetBSD: syscalls.master,v 1.48 2009/01/13 22:27:43 pooka Exp
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: osf1_syscalls.c,v 1.58 2009/01/13 22:33:17 pooka Exp $");

#if defined(_KERNEL_OPT)
#if defined(_KERNEL_OPT)
#include "opt_compat_43.h"
#endif
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/signal.h>
#include <sys/mount.h>
#include <sys/poll.h>
#include <sys/syscallargs.h>
#include <compat/osf1/osf1.h>
#include <compat/osf1/osf1_syscallargs.h>
#endif /* _KERNEL_OPT */

const char *const osf1_syscallnames[] = {
	/*   0 */	"syscall",
	/*   1 */	"exit",
	/*   2 */	"fork",
	/*   3 */	"read",
	/*   4 */	"write",
	/*   5 */	"#5 (unimplemented old open)",
	/*   6 */	"close",
	/*   7 */	"wait4",
	/*   8 */	"#8 (unimplemented old creat)",
	/*   9 */	"link",
	/*  10 */	"unlink",
	/*  11 */	"#11 (unimplemented execv)",
	/*  12 */	"chdir",
	/*  13 */	"fchdir",
	/*  14 */	"mknod",
	/*  15 */	"chmod",
	/*  16 */	"__posix_chown",
	/*  17 */	"obreak",
	/*  18 */	"getfsstat",
	/*  19 */	"lseek",
	/*  20 */	"getpid_with_ppid",
	/*  21 */	"mount",
	/*  22 */	"unmount",
	/*  23 */	"setuid",
	/*  24 */	"getuid_with_euid",
	/*  25 */	"#25 (unimplemented exec_with_loader)",
	/*  26 */	"#26 (unimplemented ptrace)",
	/*  27 */	"recvmsg_xopen",
	/*  28 */	"sendmsg_xopen",
	/*  29 */	"#29 (unimplemented recvfrom)",
	/*  30 */	"#30 (unimplemented accept)",
	/*  31 */	"#31 (unimplemented getpeername)",
	/*  32 */	"#32 (unimplemented getsockname)",
	/*  33 */	"access",
	/*  34 */	"#34 (unimplemented chflags)",
	/*  35 */	"#35 (unimplemented fchflags)",
	/*  36 */	"sync",
	/*  37 */	"kill",
	/*  38 */	"#38 (unimplemented old stat)",
	/*  39 */	"setpgid",
	/*  40 */	"#40 (unimplemented old lstat)",
	/*  41 */	"dup",
	/*  42 */	"pipe",
	/*  43 */	"set_program_attributes",
	/*  44 */	"#44 (unimplemented profil)",
	/*  45 */	"open",
	/*  46 */	"#46 (obsolete sigaction)",
	/*  47 */	"getgid_with_egid",
	/*  48 */	"sigprocmask",
	/*  49 */	"__getlogin",
	/*  50 */	"__setlogin",
	/*  51 */	"acct",
	/*  52 */	"#52 (unimplemented sigpending)",
	/*  53 */	"classcntl",
	/*  54 */	"ioctl",
	/*  55 */	"reboot",
	/*  56 */	"revoke",
	/*  57 */	"symlink",
	/*  58 */	"readlink",
	/*  59 */	"execve",
	/*  60 */	"umask",
	/*  61 */	"chroot",
	/*  62 */	"#62 (unimplemented old fstat)",
	/*  63 */	"getpgrp",
	/*  64 */	"getpagesize",
	/*  65 */	"#65 (unimplemented mremap)",
	/*  66 */	"vfork",
	/*  67 */	"stat",
	/*  68 */	"lstat",
	/*  69 */	"#69 (unimplemented sbrk)",
	/*  70 */	"#70 (unimplemented sstk)",
	/*  71 */	"mmap",
	/*  72 */	"#72 (unimplemented ovadvise)",
	/*  73 */	"munmap",
	/*  74 */	"mprotect",
	/*  75 */	"madvise",
	/*  76 */	"#76 (unimplemented old vhangup)",
	/*  77 */	"#77 (unimplemented kmodcall)",
	/*  78 */	"#78 (unimplemented mincore)",
	/*  79 */	"getgroups",
	/*  80 */	"setgroups",
	/*  81 */	"#81 (unimplemented old getpgrp)",
	/*  82 */	"setpgrp",
	/*  83 */	"setitimer",
	/*  84 */	"#84 (unimplemented old wait)",
	/*  85 */	"#85 (unimplemented table)",
	/*  86 */	"getitimer",
	/*  87 */	"gethostname",
	/*  88 */	"sethostname",
	/*  89 */	"getdtablesize",
	/*  90 */	"dup2",
	/*  91 */	"fstat",
	/*  92 */	"fcntl",
	/*  93 */	"select",
	/*  94 */	"poll",
	/*  95 */	"fsync",
	/*  96 */	"setpriority",
	/*  97 */	"socket",
	/*  98 */	"connect",
	/*  99 */	"accept",
	/* 100 */	"getpriority",
	/* 101 */	"send",
	/* 102 */	"recv",
	/* 103 */	"sigreturn",
	/* 104 */	"bind",
	/* 105 */	"setsockopt",
	/* 106 */	"listen",
	/* 107 */	"#107 (unimplemented plock)",
	/* 108 */	"#108 (unimplemented old sigvec)",
	/* 109 */	"#109 (unimplemented old sigblock)",
	/* 110 */	"#110 (unimplemented old sigsetmask)",
	/* 111 */	"sigsuspend",
	/* 112 */	"sigstack",
	/* 113 */	"#113 (unimplemented old recvmsg)",
	/* 114 */	"#114 (unimplemented old sendmsg)",
	/* 115 */	"#115 (obsolete vtrace)",
	/* 116 */	"gettimeofday",
	/* 117 */	"getrusage",
	/* 118 */	"getsockopt",
	/* 119 */	"#119 (unimplemented)",
	/* 120 */	"readv",
	/* 121 */	"writev",
	/* 122 */	"settimeofday",
	/* 123 */	"__posix_fchown",
	/* 124 */	"fchmod",
	/* 125 */	"recvfrom",
	/* 126 */	"setreuid",
	/* 127 */	"setregid",
	/* 128 */	"__posix_rename",
	/* 129 */	"truncate",
	/* 130 */	"ftruncate",
	/* 131 */	"flock",
	/* 132 */	"setgid",
	/* 133 */	"sendto",
	/* 134 */	"shutdown",
	/* 135 */	"socketpair",
	/* 136 */	"mkdir",
	/* 137 */	"rmdir",
	/* 138 */	"utimes",
	/* 139 */	"#139 (obsolete 4.2 sigreturn)",
	/* 140 */	"#140 (unimplemented adjtime)",
	/* 141 */	"getpeername",
	/* 142 */	"gethostid",
	/* 143 */	"sethostid",
	/* 144 */	"getrlimit",
	/* 145 */	"setrlimit",
	/* 146 */	"#146 (unimplemented old killpg)",
	/* 147 */	"setsid",
	/* 148 */	"#148 (unimplemented quotactl)",
	/* 149 */	"quota",
	/* 150 */	"getsockname",
	/* 151 */	"#151 (unimplemented pread)",
	/* 152 */	"#152 (unimplemented pwrite)",
	/* 153 */	"#153 (unimplemented pid_block)",
	/* 154 */	"#154 (unimplemented pid_unblock)",
	/* 155 */	"#155 (unimplemented signal_urti)",
	/* 156 */	"sigaction",
	/* 157 */	"#157 (unimplemented sigwaitprim)",
	/* 158 */	"#158 (unimplemented nfssvc)",
	/* 159 */	"getdirentries",
	/* 160 */	"statfs",
	/* 161 */	"fstatfs",
	/* 162 */	"#162 (unimplemented)",
	/* 163 */	"#163 (unimplemented async_daemon)",
	/* 164 */	"#164 (unimplemented getfh)",
	/* 165 */	"getdomainname",
	/* 166 */	"setdomainname",
	/* 167 */	"#167 (unimplemented)",
	/* 168 */	"#168 (unimplemented)",
	/* 169 */	"#169 (unimplemented exportfs)",
	/* 170 */	"#170 (unimplemented)",
	/* 171 */	"#171 (unimplemented)",
	/* 172 */	"#172 (unimplemented alt msgctl)",
	/* 173 */	"#173 (unimplemented alt msgget)",
	/* 174 */	"#174 (unimplemented alt msgrcv)",
	/* 175 */	"#175 (unimplemented alt msgsnd)",
	/* 176 */	"#176 (unimplemented alt semctl)",
	/* 177 */	"#177 (unimplemented alt semget)",
	/* 178 */	"#178 (unimplemented alt semop)",
	/* 179 */	"#179 (unimplemented alt uname)",
	/* 180 */	"#180 (unimplemented)",
	/* 181 */	"#181 (unimplemented alt plock)",
	/* 182 */	"#182 (unimplemented lockf)",
	/* 183 */	"#183 (unimplemented)",
	/* 184 */	"#184 (unimplemented getmnt)",
	/* 185 */	"#185 (unimplemented)",
	/* 186 */	"#186 (unimplemented unmount)",
	/* 187 */	"#187 (unimplemented alt sigpending)",
	/* 188 */	"#188 (unimplemented alt setsid)",
	/* 189 */	"#189 (unimplemented)",
	/* 190 */	"#190 (unimplemented)",
	/* 191 */	"#191 (unimplemented)",
	/* 192 */	"#192 (unimplemented)",
	/* 193 */	"#193 (unimplemented)",
	/* 194 */	"#194 (unimplemented)",
	/* 195 */	"#195 (unimplemented)",
	/* 196 */	"#196 (unimplemented)",
	/* 197 */	"#197 (unimplemented)",
	/* 198 */	"#198 (unimplemented)",
	/* 199 */	"#199 (unimplemented swapon)",
	/* 200 */	"#200 (unimplemented msgctl)",
	/* 201 */	"#201 (unimplemented msgget)",
	/* 202 */	"#202 (unimplemented msgrcv)",
	/* 203 */	"#203 (unimplemented msgsnd)",
	/* 204 */	"#204 (unimplemented semctl)",
	/* 205 */	"#205 (unimplemented semget)",
	/* 206 */	"#206 (unimplemented semop)",
	/* 207 */	"uname",
	/* 208 */	"__posix_lchown",
	/* 209 */	"shmat",
	/* 210 */	"shmctl",
	/* 211 */	"shmdt",
	/* 212 */	"shmget",
	/* 213 */	"#213 (unimplemented mvalid)",
	/* 214 */	"#214 (unimplemented getaddressconf)",
	/* 215 */	"#215 (unimplemented msleep)",
	/* 216 */	"#216 (unimplemented mwakeup)",
	/* 217 */	"#217 (unimplemented msync)",
	/* 218 */	"#218 (unimplemented signal)",
	/* 219 */	"#219 (unimplemented utc gettime)",
	/* 220 */	"#220 (unimplemented utc adjtime)",
	/* 221 */	"#221 (unimplemented)",
	/* 222 */	"#222 (unimplemented security)",
	/* 223 */	"#223 (unimplemented kloadcall)",
	/* 224 */	"stat2",
	/* 225 */	"lstat2",
	/* 226 */	"fstat2",
	/* 227 */	"#227 (unimplemented statfs2)",
	/* 228 */	"#228 (unimplemented fstatfs2)",
	/* 229 */	"#229 (unimplemented getfsstat2)",
	/* 230 */	"#230 (unimplemented)",
	/* 231 */	"#231 (unimplemented)",
	/* 232 */	"#232 (unimplemented)",
	/* 233 */	"getpgid",
	/* 234 */	"getsid",
	/* 235 */	"sigaltstack",
	/* 236 */	"#236 (unimplemented waitid)",
	/* 237 */	"#237 (unimplemented priocntlset)",
	/* 238 */	"#238 (unimplemented sigsendset)",
	/* 239 */	"#239 (unimplemented set_speculative)",
	/* 240 */	"#240 (unimplemented msfs_syscall)",
	/* 241 */	"sysinfo",
	/* 242 */	"#242 (unimplemented uadmin)",
	/* 243 */	"#243 (unimplemented fuser)",
	/* 244 */	"#244 (unimplemented proplist_syscall)",
	/* 245 */	"#245 (unimplemented ntp_adjtime)",
	/* 246 */	"#246 (unimplemented ntp_gettime)",
	/* 247 */	"pathconf",
	/* 248 */	"fpathconf",
	/* 249 */	"#249 (unimplemented)",
	/* 250 */	"#250 (unimplemented uswitch)",
	/* 251 */	"usleep_thread",
	/* 252 */	"#252 (unimplemented audcntl)",
	/* 253 */	"#253 (unimplemented audgen)",
	/* 254 */	"#254 (unimplemented sysfs)",
	/* 255 */	"#255 (unimplemented subsys_info)",
	/* 256 */	"getsysinfo",
	/* 257 */	"setsysinfo",
	/* 258 */	"#258 (unimplemented afs_syscall)",
	/* 259 */	"#259 (unimplemented swapctl)",
	/* 260 */	"#260 (unimplemented memcntl)",
	/* 261 */	"#261 (unimplemented fdatasync)",
	/* 262 */	"#262 (unimplemented oflock)",
	/* 263 */	"#263 (unimplemented _F64_readv)",
	/* 264 */	"#264 (unimplemented _F64_writev)",
	/* 265 */	"#265 (unimplemented cdslxlate)",
	/* 266 */	"#266 (unimplemented sendfile)",
};
