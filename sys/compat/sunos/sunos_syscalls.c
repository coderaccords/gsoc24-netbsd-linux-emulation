/*
 * System call names.
 *
 * DO NOT EDIT-- this file is automatically generated.
 * created from	$Id: sunos_syscalls.c,v 1.8 1993/11/22 22:54:48 deraadt Exp $
 */

char *sun_syscallnames[] = {
	"syscall",			/* 0 = syscall */
	"exit",			/* 1 = exit */
	"fork",			/* 2 = fork */
	"read",			/* 3 = read */
	"write",			/* 4 = write */
	"sun_open",			/* 5 = sun_open */
	"close",			/* 6 = close */
	"sun_wait4",			/* 7 = sun_wait4 */
	"sun_creat",			/* 8 = sun_creat */
	"link",			/* 9 = link */
	"unlink",			/* 10 = unlink */
	"sun_execv",			/* 11 = sun_execv */
	"chdir",			/* 12 = chdir */
	"old.sun_time",		/* 13 = old sun_time */
	"mknod",			/* 14 = mknod */
	"chmod",			/* 15 = chmod */
	"chown",			/* 16 = chown */
	"break",			/* 17 = break */
	"old.sun_stat",		/* 18 = old sun_stat */
	"lseek",			/* 19 = lseek */
	"getpid",			/* 20 = getpid */
	"obs_sun_old_mount",			/* 21 = obsolete sun_old_mount */
	"#22",			/* 22 = System V umount */
	"old.setuid",		/* 23 = old setuid */
	"getuid",			/* 24 = getuid */
	"old.sun_stime",		/* 25 = old sun_stime */
	"#26",			/* 26 = sun_ptrace */
	"old.sun_alarm",		/* 27 = old sun_alarm */
	"old.sun_fstat",		/* 28 = old sun_fstat */
	"old.sun_pause",		/* 29 = old sun_pause */
	"old.sun_utime",		/* 30 = old sun_utime */
	"#31",			/* 31 = was stty */
	"#32",			/* 32 = was gtty */
	"access",			/* 33 = access */
	"old.sun_nice",		/* 34 = old sun_nice */
	"old.sun_ftime",		/* 35 = old sun_ftime */
	"sync",			/* 36 = sync */
	"kill",			/* 37 = kill */
	"stat",			/* 38 = stat */
	"old.sun_setpgrp",		/* 39 = old sun_setpgrp */
	"lstat",			/* 40 = lstat */
	"dup",			/* 41 = dup */
	"pipe",			/* 42 = pipe */
	"old.sun_times",		/* 43 = old sun_times */
	"profil",			/* 44 = profil */
	"#45",			/* 45 = nosys */
	"old.sun_setgid",		/* 46 = old sun_setgid */
	"getgid",			/* 47 = getgid */
	"old.sun_ssig",		/* 48 = old sun_ssig */
	"#49",			/* 49 = reserved for USG */
	"#50",			/* 50 = reserved for USG */
	"acct",			/* 51 = acct */
	"#52",			/* 52 = nosys */
	"sun_mctl",			/* 53 = sun_mctl */
	"sun_ioctl",			/* 54 = sun_ioctl */
	"reboot",			/* 55 = reboot */
	"obs_sun_owait3",			/* 56 = obsolete sun_owait3 */
	"symlink",			/* 57 = symlink */
	"readlink",			/* 58 = readlink */
	"execve",			/* 59 = execve */
	"umask",			/* 60 = umask */
	"chroot",			/* 61 = chroot */
	"fstat",			/* 62 = fstat */
	"#63",			/* 63 = nosys */
	"getpagesize",			/* 64 = getpagesize */
	"sun_omsync",			/* 65 = sun_omsync */
	"vfork",			/* 66 = vfork */
	"obs_vread",			/* 67 = obsolete vread */
	"obs_vwrite",			/* 68 = obsolete vwrite */
	"sbrk",			/* 69 = sbrk */
	"sstk",			/* 70 = sstk */
	"mmap",			/* 71 = mmap */
	"vadvise",			/* 72 = vadvise */
	"munmap",			/* 73 = munmap */
	"mprotect",			/* 74 = mprotect */
	"madvise",			/* 75 = madvise */
	"old.vhangup",		/* 76 = old vhangup */
	"old.vlimit",		/* 77 = old vlimit */
	"mincore",			/* 78 = mincore */
	"getgroups",			/* 79 = getgroups */
	"setgroups",			/* 80 = setgroups */
	"getpgrp",			/* 81 = getpgrp */
	"sun_setpgid",			/* 82 = sun_setpgid */
	"setitimer",			/* 83 = setitimer */
	"old.wait",		/* 84 = old wait */
	"swapon",			/* 85 = swapon */
	"getitimer",			/* 86 = getitimer */
	"gethostname",			/* 87 = gethostname */
	"sethostname",			/* 88 = sethostname */
	"getdtablesize",			/* 89 = getdtablesize */
	"dup2",			/* 90 = dup2 */
	"#91",			/* 91 = getdopt */
	"fcntl",			/* 92 = fcntl */
	"select",			/* 93 = select */
	"#94",			/* 94 = setdopt */
	"fsync",			/* 95 = fsync */
	"setpriority",			/* 96 = setpriority */
	"socket",			/* 97 = socket */
	"connect",			/* 98 = connect */
	"oaccept",			/* 99 = oaccept */
	"getpriority",			/* 100 = getpriority */
	"osend",			/* 101 = osend */
	"orecv",			/* 102 = orecv */
	"#103",			/* 103 = old socketaddr */
	"bind",			/* 104 = bind */
	"sun_setsockopt",			/* 105 = sun_setsockopt */
	"listen",			/* 106 = listen */
	"old.vtimes",		/* 107 = old vtimes */
	"osigvec",			/* 108 = osigvec */
	"osigblock",			/* 109 = osigblock */
	"osigsetmask",			/* 110 = osigsetmask */
	"sigsuspend",			/* 111 = sigsuspend */
	"sigstack",			/* 112 = sigstack */
	"orecvmsg",			/* 113 = orecvmsg */
	"osendmsg",			/* 114 = osendmsg */
	"obs_vtrace",			/* 115 = obsolete vtrace */
	"gettimeofday",			/* 116 = gettimeofday */
	"getrusage",			/* 117 = getrusage */
	"getsockopt",			/* 118 = getsockopt */
	"#119",			/* 119 = nosys */
	"readv",			/* 120 = readv */
	"writev",			/* 121 = writev */
	"settimeofday",			/* 122 = settimeofday */
	"fchown",			/* 123 = fchown */
	"fchmod",			/* 124 = fchmod */
	"orecvfrom",			/* 125 = orecvfrom */
	"osetreuid",			/* 126 = osetreuid */
	"osetregid",			/* 127 = osetregid */
	"rename",			/* 128 = rename */
	"truncate",			/* 129 = truncate */
	"ftruncate",			/* 130 = ftruncate */
	"flock",			/* 131 = flock */
	"#132",			/* 132 = nosys */
	"sendto",			/* 133 = sendto */
	"shutdown",			/* 134 = shutdown */
	"socketpair",			/* 135 = socketpair */
	"mkdir",			/* 136 = mkdir */
	"rmdir",			/* 137 = rmdir */
	"utimes",			/* 138 = utimes */
	"sigreturn",			/* 139 = sigreturn */
	"adjtime",			/* 140 = adjtime */
	"ogetpeername",			/* 141 = ogetpeername */
	"gethostid",			/* 142 = gethostid */
	"#143",			/* 143 = old sethostid */
	"getrlimit",			/* 144 = getrlimit */
	"setrlimit",			/* 145 = setrlimit */
	"okillpg",			/* 146 = okillpg */
	"#147",			/* 147 = nosys */
	"#148",			/* 148 = nosys */
	"#149",			/* 149 = nosys */
	"ogetsockname",			/* 150 = ogetsockname */
	"#151",			/* 151 = getmsg */
	"#152",			/* 152 = putmsg */
	"#153",			/* 153 = poll */
	"#154",			/* 154 = nosys */
	"#155",			/* 155 = nosys */
	"getdirentries",			/* 156 = getdirentries */
	"statfs",			/* 157 = statfs */
	"fstatfs",			/* 158 = fstatfs */
	"sun_unmount",			/* 159 = sun_unmount */
	"#160",			/* 160 = nosys */
	"#161",			/* 161 = nosys */
	"getdomainname",			/* 162 = getdomainname */
	"setdomainname",			/* 163 = setdomainname */
	"#164",			/* 164 = rtschedule */
	"#165",			/* 165 = quotactl */
	"#166",			/* 166 = exportfs */
	"sun_mount",			/* 167 = sun_mount */
	"#168",			/* 168 = ustat */
#ifdef SYSVSEM
	"semsys",			/* 169 = semsys */
#else
	"#169",			/* 169 = nosys */
#endif
#ifdef SYSVMSG
	"msgsys",			/* 170 = msgsys */
#else
	"#170",			/* 170 = nosys */
#endif
#ifdef SYSVSHM
	"shmsys",			/* 171 = shmsys */
#else
	"#171",			/* 171 = nosys */
#endif
	"sun_auditsys",			/* 172 = sun_auditsys */
	"#173",			/* 173 = rfssys */
	"sun_getdents",			/* 174 = sun_getdents */
	"setsid",			/* 175 = setsid */
	"fchdir",			/* 176 = fchdir */
	"sun_fchroot",			/* 177 = sun_fchroot */
	"#178",			/* 178 = nosys */
	"#179",			/* 179 = nosys */
	"#180",			/* 180 = nosys */
	"#181",			/* 181 = nosys */
	"#182",			/* 182 = nosys */
	"sun_sigpending",			/* 183 = sun_sigpending */
	"#184",			/* 184 = nosys */
	"setpgid",			/* 185 = setpgid */
	"#186",			/* 186 = pathconf */
	"#187",			/* 187 = fpathconf */
	"#188",			/* 188 = sysconf */
	"sun_uname",			/* 189 = sun_uname */
};
