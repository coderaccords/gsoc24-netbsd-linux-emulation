/*	$NetBSD: freebsd_syscalls.c,v 1.35 2000/08/08 02:15:10 itojun Exp $	*/

/*
 * System call names.
 *
 * DO NOT EDIT-- this file is automatically generated.
 * created from	NetBSD: syscalls.master,v 1.30 2000/08/08 02:14:48 itojun Exp 
 */

#if defined(_KERNEL) && !defined(_LKM)
#include "opt_ktrace.h"
#include "opt_nfsserver.h"
#include "opt_ntp.h"
#include "opt_sysv.h"
#include "opt_compat_43.h"
#include "fs_lfs.h"
#include "fs_nfs.h"
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/signal.h>
#include <sys/mount.h>
#include <sys/syscallargs.h>
#include <compat/freebsd/freebsd_syscallargs.h>
#include <machine/freebsd_machdep.h>
#endif /* _KERNEL && ! _LKM */

char *freebsd_syscallnames[] = {
	"syscall",			/* 0 = syscall */
	"exit",			/* 1 = exit */
	"fork",			/* 2 = fork */
	"read",			/* 3 = read */
	"write",			/* 4 = write */
	"open",			/* 5 = open */
	"close",			/* 6 = close */
	"wait4",			/* 7 = wait4 */
	"ocreat",			/* 8 = ocreat */
	"link",			/* 9 = link */
	"unlink",			/* 10 = unlink */
	"#11 (obsolete execv)",		/* 11 = obsolete execv */
	"chdir",			/* 12 = chdir */
	"fchdir",			/* 13 = fchdir */
	"mknod",			/* 14 = mknod */
	"chmod",			/* 15 = chmod */
	"chown",			/* 16 = chown */
	"break",			/* 17 = break */
	"getfsstat",			/* 18 = getfsstat */
	"olseek",			/* 19 = olseek */
	"getpid",			/* 20 = getpid */
	"mount",			/* 21 = mount */
	"unmount",			/* 22 = unmount */
	"setuid",			/* 23 = setuid */
	"getuid",			/* 24 = getuid */
	"geteuid",			/* 25 = geteuid */
	"ptrace",			/* 26 = ptrace */
	"recvmsg",			/* 27 = recvmsg */
	"sendmsg",			/* 28 = sendmsg */
	"recvfrom",			/* 29 = recvfrom */
	"accept",			/* 30 = accept */
	"getpeername",			/* 31 = getpeername */
	"getsockname",			/* 32 = getsockname */
	"access",			/* 33 = access */
	"chflags",			/* 34 = chflags */
	"fchflags",			/* 35 = fchflags */
	"sync",			/* 36 = sync */
	"kill",			/* 37 = kill */
	"stat43",			/* 38 = stat43 */
	"getppid",			/* 39 = getppid */
	"lstat43",			/* 40 = lstat43 */
	"dup",			/* 41 = dup */
	"pipe",			/* 42 = pipe */
	"getegid",			/* 43 = getegid */
	"profil",			/* 44 = profil */
#ifdef KTRACE
	"ktrace",			/* 45 = ktrace */
#else
	"#45 (unimplemented ktrace)",		/* 45 = unimplemented ktrace */
#endif
	"sigaction",			/* 46 = sigaction */
	"getgid",			/* 47 = getgid */
	"sigprocmask",			/* 48 = sigprocmask */
	"__getlogin",			/* 49 = __getlogin */
	"setlogin",			/* 50 = setlogin */
	"acct",			/* 51 = acct */
	"sigpending",			/* 52 = sigpending */
	"sigaltstack",			/* 53 = sigaltstack */
	"ioctl",			/* 54 = ioctl */
	"oreboot",			/* 55 = oreboot */
	"revoke",			/* 56 = revoke */
	"symlink",			/* 57 = symlink */
	"readlink",			/* 58 = readlink */
	"execve",			/* 59 = execve */
	"umask",			/* 60 = umask */
	"chroot",			/* 61 = chroot */
	"fstat43",			/* 62 = fstat43 */
	"ogetkerninfo",			/* 63 = ogetkerninfo */
	"ogetpagesize",			/* 64 = ogetpagesize */
	"msync",			/* 65 = msync */
	"vfork",			/* 66 = vfork */
	"#67 (obsolete vread)",		/* 67 = obsolete vread */
	"#68 (obsolete vwrite)",		/* 68 = obsolete vwrite */
	"sbrk",			/* 69 = sbrk */
	"sstk",			/* 70 = sstk */
	"ommap",			/* 71 = ommap */
	"vadvise",			/* 72 = vadvise */
	"munmap",			/* 73 = munmap */
	"mprotect",			/* 74 = mprotect */
	"madvise",			/* 75 = madvise */
	"#76 (obsolete vhangup)",		/* 76 = obsolete vhangup */
	"#77 (obsolete vlimit)",		/* 77 = obsolete vlimit */
	"mincore",			/* 78 = mincore */
	"getgroups",			/* 79 = getgroups */
	"setgroups",			/* 80 = setgroups */
	"getpgrp",			/* 81 = getpgrp */
	"setpgid",			/* 82 = setpgid */
	"setitimer",			/* 83 = setitimer */
	"owait",			/* 84 = owait */
	"swapon",			/* 85 = swapon */
	"getitimer",			/* 86 = getitimer */
	"ogethostname",			/* 87 = ogethostname */
	"osethostname",			/* 88 = osethostname */
	"ogetdtablesize",			/* 89 = ogetdtablesize */
	"dup2",			/* 90 = dup2 */
	"#91 (unimplemented getdopt)",		/* 91 = unimplemented getdopt */
	"fcntl",			/* 92 = fcntl */
	"select",			/* 93 = select */
	"#94 (unimplemented setdopt)",		/* 94 = unimplemented setdopt */
	"fsync",			/* 95 = fsync */
	"setpriority",			/* 96 = setpriority */
	"socket",			/* 97 = socket */
	"connect",			/* 98 = connect */
	"oaccept",			/* 99 = oaccept */
	"getpriority",			/* 100 = getpriority */
	"osend",			/* 101 = osend */
	"orecv",			/* 102 = orecv */
	"sigreturn",			/* 103 = sigreturn */
	"bind",			/* 104 = bind */
	"setsockopt",			/* 105 = setsockopt */
	"listen",			/* 106 = listen */
	"#107 (obsolete vtimes)",		/* 107 = obsolete vtimes */
	"osigvec",			/* 108 = osigvec */
	"osigblock",			/* 109 = osigblock */
	"osigsetmask",			/* 110 = osigsetmask */
	"sigsuspend",			/* 111 = sigsuspend */
	"osigstack",			/* 112 = osigstack */
	"orecvmsg",			/* 113 = orecvmsg */
	"osendmsg",			/* 114 = osendmsg */
#ifdef TRACE
	"vtrace",			/* 115 = vtrace */
#else
	"#115 (obsolete vtrace)",		/* 115 = obsolete vtrace */
#endif
	"gettimeofday",			/* 116 = gettimeofday */
	"getrusage",			/* 117 = getrusage */
	"getsockopt",			/* 118 = getsockopt */
	"#119 (obsolete resuba)",		/* 119 = obsolete resuba */
	"readv",			/* 120 = readv */
	"writev",			/* 121 = writev */
	"settimeofday",			/* 122 = settimeofday */
	"fchown",			/* 123 = fchown */
	"fchmod",			/* 124 = fchmod */
	"orecvfrom",			/* 125 = orecvfrom */
	"setreuid",			/* 126 = setreuid */
	"setregid",			/* 127 = setregid */
	"rename",			/* 128 = rename */
	"otruncate",			/* 129 = otruncate */
	"oftruncate",			/* 130 = oftruncate */
	"flock",			/* 131 = flock */
	"mkfifo",			/* 132 = mkfifo */
	"sendto",			/* 133 = sendto */
	"shutdown",			/* 134 = shutdown */
	"socketpair",			/* 135 = socketpair */
	"mkdir",			/* 136 = mkdir */
	"rmdir",			/* 137 = rmdir */
	"utimes",			/* 138 = utimes */
	"#139 (obsolete 4.2 sigreturn)",		/* 139 = obsolete 4.2 sigreturn */
	"adjtime",			/* 140 = adjtime */
	"ogetpeername",			/* 141 = ogetpeername */
	"ogethostid",			/* 142 = ogethostid */
	"osethostid",			/* 143 = osethostid */
	"ogetrlimit",			/* 144 = ogetrlimit */
	"osetrlimit",			/* 145 = osetrlimit */
	"okillpg",			/* 146 = okillpg */
	"setsid",			/* 147 = setsid */
	"quotactl",			/* 148 = quotactl */
	"oquota",			/* 149 = oquota */
	"ogetsockname",			/* 150 = ogetsockname */
	"#151 (unimplemented sem_lock)",		/* 151 = unimplemented sem_lock */
	"#152 (unimplemented sem_wakeup)",		/* 152 = unimplemented sem_wakeup */
	"#153 (unimplemented asyncdaemon)",		/* 153 = unimplemented asyncdaemon */
	"#154 (unimplemented)",		/* 154 = unimplemented */
#if defined(NFS) || defined(NFSSERVER)
	"nfssvc",			/* 155 = nfssvc */
#else
	"#155 (unimplemented nfssvc)",		/* 155 = unimplemented nfssvc */
#endif
	"ogetdirentries",			/* 156 = ogetdirentries */
	"statfs",			/* 157 = statfs */
	"fstatfs",			/* 158 = fstatfs */
	"#159 (unimplemented)",		/* 159 = unimplemented */
	"#160 (unimplemented)",		/* 160 = unimplemented */
#ifdef NFS
	"getfh",			/* 161 = getfh */
#else
	"#161 (unimplemented getfh)",		/* 161 = unimplemented getfh */
#endif
	"getdomainname",			/* 162 = getdomainname */
	"setdomainname",			/* 163 = setdomainname */
	"uname",			/* 164 = uname */
	"sysarch",			/* 165 = sysarch */
	"rtprio",			/* 166 = rtprio */
	"#167 (unimplemented)",		/* 167 = unimplemented */
	"#168 (unimplemented)",		/* 168 = unimplemented */
#if defined(SYSVSEM) && !defined(alpha)
	"semsys",			/* 169 = semsys */
#else
	"#169 (unimplemented 1.0 semsys)",		/* 169 = unimplemented 1.0 semsys */
#endif
#if defined(SYSVMSG) && !defined(alpha)
	"msgsys",			/* 170 = msgsys */
#else
	"#170 (unimplemented 1.0 msgsys)",		/* 170 = unimplemented 1.0 msgsys */
#endif
#if defined(SYSVSHM) && !defined(alpha)
	"shmsys",			/* 171 = shmsys */
#else
	"#171 (unimplemented 1.0 shmsys)",		/* 171 = unimplemented 1.0 shmsys */
#endif
	"#172 (unimplemented)",		/* 172 = unimplemented */
	"pread",			/* 173 = pread */
	"pwrite",			/* 174 = pwrite */
	"#175 (unimplemented)",		/* 175 = unimplemented */
#ifdef NTP
	"freebsd_ntp_adjtime",			/* 176 = freebsd_ntp_adjtime */
#else
	"#176 (excluded ntp_adjtime)",		/* 176 = excluded ntp_adjtime */
#endif
	"#177 (unimplemented sfork)",		/* 177 = unimplemented sfork */
	"#178 (unimplemented getdescriptor)",		/* 178 = unimplemented getdescriptor */
	"#179 (unimplemented setdescriptor)",		/* 179 = unimplemented setdescriptor */
	"#180 (unimplemented)",		/* 180 = unimplemented */
	"setgid",			/* 181 = setgid */
	"setegid",			/* 182 = setegid */
	"seteuid",			/* 183 = seteuid */
#ifdef LFS
	"lfs_bmapv",			/* 184 = lfs_bmapv */
	"lfs_markv",			/* 185 = lfs_markv */
	"lfs_segclean",			/* 186 = lfs_segclean */
	"lfs_segwait",			/* 187 = lfs_segwait */
#else
	"#184 (unimplemented)",		/* 184 = unimplemented */
	"#185 (unimplemented)",		/* 185 = unimplemented */
	"#186 (unimplemented)",		/* 186 = unimplemented */
	"#187 (unimplemented)",		/* 187 = unimplemented */
#endif
	"stat",			/* 188 = stat */
	"fstat",			/* 189 = fstat */
	"lstat",			/* 190 = lstat */
	"pathconf",			/* 191 = pathconf */
	"fpathconf",			/* 192 = fpathconf */
	"#193 (unimplemented)",		/* 193 = unimplemented */
	"getrlimit",			/* 194 = getrlimit */
	"setrlimit",			/* 195 = setrlimit */
	"getdirentries",			/* 196 = getdirentries */
	"mmap",			/* 197 = mmap */
	"__syscall",			/* 198 = __syscall */
	"lseek",			/* 199 = lseek */
	"truncate",			/* 200 = truncate */
	"ftruncate",			/* 201 = ftruncate */
	"__sysctl",			/* 202 = __sysctl */
	"mlock",			/* 203 = mlock */
	"munlock",			/* 204 = munlock */
#ifdef FREEBSD_BASED_ON_44LITE_R2
	"undelete",			/* 205 = undelete */
#else
	"#205 (unimplemented undelete)",		/* 205 = unimplemented undelete */
#endif
	"futimes",			/* 206 = futimes */
	"getpgid",			/* 207 = getpgid */
#if 0
	"reboot",			/* 208 = reboot */
#else
	"#208 (unimplemented newreboot)",		/* 208 = unimplemented newreboot */
#endif
	"poll",			/* 209 = poll */
	"#210 (unimplemented)",		/* 210 = unimplemented */
	"#211 (unimplemented)",		/* 211 = unimplemented */
	"#212 (unimplemented)",		/* 212 = unimplemented */
	"#213 (unimplemented)",		/* 213 = unimplemented */
	"#214 (unimplemented)",		/* 214 = unimplemented */
	"#215 (unimplemented)",		/* 215 = unimplemented */
	"#216 (unimplemented)",		/* 216 = unimplemented */
	"#217 (unimplemented)",		/* 217 = unimplemented */
	"#218 (unimplemented)",		/* 218 = unimplemented */
	"#219 (unimplemented)",		/* 219 = unimplemented */
#ifdef SYSVSEM
	"__semctl",			/* 220 = __semctl */
	"semget",			/* 221 = semget */
	"semop",			/* 222 = semop */
	"semconfig",			/* 223 = semconfig */
#else
	"#220 (unimplemented semctl)",		/* 220 = unimplemented semctl */
	"#221 (unimplemented semget)",		/* 221 = unimplemented semget */
	"#222 (unimplemented semop)",		/* 222 = unimplemented semop */
	"#223 (unimplemented semconfig)",		/* 223 = unimplemented semconfig */
#endif
#ifdef SYSVMSG
	"msgctl",			/* 224 = msgctl */
	"msgget",			/* 225 = msgget */
	"msgsnd",			/* 226 = msgsnd */
	"msgrcv",			/* 227 = msgrcv */
#else
	"#224 (unimplemented msgctl)",		/* 224 = unimplemented msgctl */
	"#225 (unimplemented msgget)",		/* 225 = unimplemented msgget */
	"#226 (unimplemented msgsnd)",		/* 226 = unimplemented msgsnd */
	"#227 (unimplemented msgrcv)",		/* 227 = unimplemented msgrcv */
#endif
#ifdef SYSVSHM
	"shmat",			/* 228 = shmat */
	"shmctl",			/* 229 = shmctl */
	"shmdt",			/* 230 = shmdt */
	"shmget",			/* 231 = shmget */
#else
	"#228 (unimplemented shmat)",		/* 228 = unimplemented shmat */
	"#229 (unimplemented shmctl)",		/* 229 = unimplemented shmctl */
	"#230 (unimplemented shmdt)",		/* 230 = unimplemented shmdt */
	"#231 (unimplemented shmget)",		/* 231 = unimplemented shmget */
#endif
	"clock_gettime",			/* 232 = clock_gettime */
	"clock_settime",			/* 233 = clock_settime */
	"clock_getres",			/* 234 = clock_getres */
	"#235 (unimplemented timer_create)",		/* 235 = unimplemented timer_create */
	"#236 (unimplemented timer_delete)",		/* 236 = unimplemented timer_delete */
	"#237 (unimplemented timer_settime)",		/* 237 = unimplemented timer_settime */
	"#238 (unimplemented timer_gettime)",		/* 238 = unimplemented timer_gettime */
	"#239 (unimplemented timer_getoverrun)",		/* 239 = unimplemented timer_getoverrun */
	"nanosleep",			/* 240 = nanosleep */
	"#241 (unimplemented)",		/* 241 = unimplemented */
	"#242 (unimplemented)",		/* 242 = unimplemented */
	"#243 (unimplemented)",		/* 243 = unimplemented */
	"#244 (unimplemented)",		/* 244 = unimplemented */
	"#245 (unimplemented)",		/* 245 = unimplemented */
	"#246 (unimplemented)",		/* 246 = unimplemented */
	"#247 (unimplemented)",		/* 247 = unimplemented */
	"#248 (unimplemented)",		/* 248 = unimplemented */
	"#249 (unimplemented)",		/* 249 = unimplemented */
	"#250 (unimplemented minherit)",		/* 250 = unimplemented minherit */
	"#251 (unimplemented rfork)",		/* 251 = unimplemented rfork */
	"#252 (unimplemented openbsd_poll)",		/* 252 = unimplemented openbsd_poll */
	"issetugid",			/* 253 = issetugid */
	"lchown",			/* 254 = lchown */
	"#255 (unimplemented)",		/* 255 = unimplemented */
	"#256 (unimplemented)",		/* 256 = unimplemented */
	"#257 (unimplemented)",		/* 257 = unimplemented */
	"#258 (unimplemented)",		/* 258 = unimplemented */
	"#259 (unimplemented)",		/* 259 = unimplemented */
	"#260 (unimplemented)",		/* 260 = unimplemented */
	"#261 (unimplemented)",		/* 261 = unimplemented */
	"#262 (unimplemented)",		/* 262 = unimplemented */
	"#263 (unimplemented)",		/* 263 = unimplemented */
	"#264 (unimplemented)",		/* 264 = unimplemented */
	"#265 (unimplemented)",		/* 265 = unimplemented */
	"#266 (unimplemented)",		/* 266 = unimplemented */
	"#267 (unimplemented)",		/* 267 = unimplemented */
	"#268 (unimplemented)",		/* 268 = unimplemented */
	"#269 (unimplemented)",		/* 269 = unimplemented */
	"#270 (unimplemented)",		/* 270 = unimplemented */
	"#271 (unimplemented)",		/* 271 = unimplemented */
	"getdents",			/* 272 = getdents */
	"#273 (unimplemented)",		/* 273 = unimplemented */
	"lchmod",			/* 274 = lchmod */
	"netbsd_lchown",			/* 275 = netbsd_lchown */
	"lutimes",			/* 276 = lutimes */
	"__msync13",			/* 277 = __msync13 */
	"__stat13",			/* 278 = __stat13 */
	"__fstat13",			/* 279 = __fstat13 */
	"__lstat13",			/* 280 = __lstat13 */
	"#281 (unimplemented)",		/* 281 = unimplemented */
	"#282 (unimplemented)",		/* 282 = unimplemented */
	"#283 (unimplemented)",		/* 283 = unimplemented */
	"#284 (unimplemented)",		/* 284 = unimplemented */
	"#285 (unimplemented)",		/* 285 = unimplemented */
	"#286 (unimplemented)",		/* 286 = unimplemented */
	"#287 (unimplemented)",		/* 287 = unimplemented */
	"#288 (unimplemented)",		/* 288 = unimplemented */
	"#289 (unimplemented)",		/* 289 = unimplemented */
	"#290 (unimplemented)",		/* 290 = unimplemented */
	"#291 (unimplemented)",		/* 291 = unimplemented */
	"#292 (unimplemented)",		/* 292 = unimplemented */
	"#293 (unimplemented)",		/* 293 = unimplemented */
	"#294 (unimplemented)",		/* 294 = unimplemented */
	"#295 (unimplemented)",		/* 295 = unimplemented */
	"#296 (unimplemented)",		/* 296 = unimplemented */
	"#297 (unimplemented fhstatfs)",		/* 297 = unimplemented fhstatfs */
	"#298 (unimplemented fhopen)",		/* 298 = unimplemented fhopen */
	"#299 (unimplemented fhstat)",		/* 299 = unimplemented fhstat */
	"#300 (unimplemented modnext)",		/* 300 = unimplemented modnext */
	"#301 (unimplemented modstat)",		/* 301 = unimplemented modstat */
	"#302 (unimplemented modfnext)",		/* 302 = unimplemented modfnext */
	"#303 (unimplemented modfind)",		/* 303 = unimplemented modfind */
	"#304 (unimplemented kldload)",		/* 304 = unimplemented kldload */
	"#305 (unimplemented kldunload)",		/* 305 = unimplemented kldunload */
	"#306 (unimplemented kldfind)",		/* 306 = unimplemented kldfind */
	"#307 (unimplemented kldnext)",		/* 307 = unimplemented kldnext */
	"#308 (unimplemented kldstat)",		/* 308 = unimplemented kldstat */
	"#309 (unimplemented kldfirstmod)",		/* 309 = unimplemented kldfirstmod */
	"getsid",			/* 310 = getsid */
	"#311 (unimplemented setresuid)",		/* 311 = unimplemented setresuid */
	"#312 (unimplemented setresgid)",		/* 312 = unimplemented setresgid */
	"#313 (unimplemented signanosleep)",		/* 313 = unimplemented signanosleep */
	"#314 (unimplemented aio_return)",		/* 314 = unimplemented aio_return */
	"#315 (unimplemented aio_suspend)",		/* 315 = unimplemented aio_suspend */
	"#316 (unimplemented aio_cancel)",		/* 316 = unimplemented aio_cancel */
	"#317 (unimplemented aio_error)",		/* 317 = unimplemented aio_error */
	"#318 (unimplemented aio_read)",		/* 318 = unimplemented aio_read */
	"#319 (unimplemented aio_write)",		/* 319 = unimplemented aio_write */
	"#320 (unimplemented lio_listio)",		/* 320 = unimplemented lio_listio */
	"#321 (unimplemented yield)",		/* 321 = unimplemented yield */
	"#322 (unimplemented thr_sleep)",		/* 322 = unimplemented thr_sleep */
	"#323 (unimplemented thr_wakeup)",		/* 323 = unimplemented thr_wakeup */
	"mlockall",			/* 324 = mlockall */
	"munlockall",			/* 325 = munlockall */
	"__getcwd",			/* 326 = __getcwd */
	"#327 (unimplemented sched_setparam)",		/* 327 = unimplemented sched_setparam */
	"#328 (unimplemented sched_getparam)",		/* 328 = unimplemented sched_getparam */
	"#329 (unimplemented sched_setscheduler)",		/* 329 = unimplemented sched_setscheduler */
	"#330 (unimplemented sched_getscheduler)",		/* 330 = unimplemented sched_getscheduler */
	"#331 (unimplemented sched_yield)",		/* 331 = unimplemented sched_yield */
	"#332 (unimplemented sched_get_priority_max)",		/* 332 = unimplemented sched_get_priority_max */
	"#333 (unimplemented sched_get_priority_min)",		/* 333 = unimplemented sched_get_priority_min */
	"#334 (unimplemented sched_rr_get_interval)",		/* 334 = unimplemented sched_rr_get_interval */
	"#335 (unimplemented utrace)",		/* 335 = unimplemented utrace */
	"#336 (unimplemented sendfile)",		/* 336 = unimplemented sendfile */
	"#337 (unimplemented kldsym)",		/* 337 = unimplemented kldsym */
	"#338 (unimplemented jail)",		/* 338 = unimplemented jail */
	"#339 (unimplemented pioctl)",		/* 339 = unimplemented pioctl */
	"__sigprocmask14",			/* 340 = __sigprocmask14 */
	"__sigsuspend14",			/* 341 = __sigsuspend14 */
	"sigaction4",			/* 342 = sigaction4 */
	"__sigpending14",			/* 343 = __sigpending14 */
	"#344 (unimplemented 4.0 sigreturn)",		/* 344 = unimplemented 4.0 sigreturn */
	"#345 (unimplemented sigtimedwait)",		/* 345 = unimplemented sigtimedwait */
	"#346 (unimplemented sigwaitinfo)",		/* 346 = unimplemented sigwaitinfo */
	"#347 (unimplemented __acl_get_file)",		/* 347 = unimplemented __acl_get_file */
	"#348 (unimplemented __acl_set_file)",		/* 348 = unimplemented __acl_set_file */
	"#349 (unimplemented __acl_get_fd)",		/* 349 = unimplemented __acl_get_fd */
	"#350 (unimplemented __acl_set_fd)",		/* 350 = unimplemented __acl_set_fd */
	"#351 (unimplemented __acl_delete_file)",		/* 351 = unimplemented __acl_delete_file */
	"#352 (unimplemented __acl_delete_fd)",		/* 352 = unimplemented __acl_delete_fd */
	"#353 (unimplemented __acl_aclcheck_file)",		/* 353 = unimplemented __acl_aclcheck_file */
	"#354 (unimplemented __acl_aclcheck_fd)",		/* 354 = unimplemented __acl_aclcheck_fd */
	"#355 (unimplemented extattrctl)",		/* 355 = unimplemented extattrctl */
	"#356 (unimplemented extattr_set_file)",		/* 356 = unimplemented extattr_set_file */
	"#357 (unimplemented extattr_get_file)",		/* 357 = unimplemented extattr_get_file */
	"#358 (unimplemented extattr_delete_file)",		/* 358 = unimplemented extattr_delete_file */
	"#359 (unimplemented aio_waitcomplete)",		/* 359 = unimplemented aio_waitcomplete */
	"#360 (unimplemented getresuid)",		/* 360 = unimplemented getresuid */
	"#361 (unimplemented getresgid)",		/* 361 = unimplemented getresgid */
	"#362 (unimplemented kqueue)",		/* 362 = unimplemented kqueue */
	"#363 (unimplemented kevent)",		/* 363 = unimplemented kevent */
};
