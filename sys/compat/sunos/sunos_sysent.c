/*
 * System call switch table.
 *
 * DO NOT EDIT-- this file is automatically generated.
 * created from: syscalls.master,v 1.14 1994/05/01 06:14:50
 */

#include <sys/param.h>
#include <sys/systm.h>

int	nosys();

int	nosys();
int	exit();
int	fork();
int	read();
int	write();
int	sun_open();
int	close();
int	sun_wait4();
int	sun_creat();
int	link();
int	unlink();
int	sun_execv();
int	chdir();
int	sun_mknod();
int	chmod();
int	chown();
int	obreak();
int	olseek();
int	getpid();
int	getuid();
int	access();
int	sync();
int	kill();
int	ostat();
int	olstat();
int	dup();
int	pipe();
int	profil();
int	getgid();
int	acct();
int	sun_mctl();
int	sun_ioctl();
int	reboot();
int	symlink();
int	readlink();
int	execve();
int	umask();
int	chroot();
int	ofstat();
int	getpagesize();
int	sun_omsync();
int	vfork();
int	sbrk();
int	sstk();
int	sun_mmap();
int	ovadvise();
int	munmap();
int	mprotect();
int	madvise();
int	sun_vhangup();
int	mincore();
int	getgroups();
int	setgroups();
int	getpgrp();
int	sun_setpgid();
int	setitimer();
int	swapon();
int	getitimer();
int	gethostname();
int	sethostname();
int	getdtablesize();
int	dup2();
int	fcntl();
int	select();
int	fsync();
int	setpriority();
int	socket();
int	connect();
int	oaccept();
int	getpriority();
int	osend();
int	orecv();
int	bind();
int	sun_setsockopt();
int	listen();
int	osigvec();
int	osigblock();
int	osigsetmask();
int	sigsuspend();
int	sigstack();
int	orecvmsg();
int	osendmsg();
int	gettimeofday();
int	getrusage();
int	getsockopt();
int	readv();
int	writev();
int	settimeofday();
int	fchown();
int	fchmod();
int	orecvfrom();
int	osetreuid();
int	osetregid();
int	rename();
int	otruncate();
int	oftruncate();
int	flock();
int	sendto();
int	shutdown();
int	socketpair();
int	mkdir();
int	rmdir();
int	utimes();
int	sigreturn();
int	adjtime();
int	ogetpeername();
int	gethostid();
int	sun_getrlimit();
int	sun_setrlimit();
int	okillpg();
int	ogetsockname();
#ifdef NFSSERVER
int	sun_nfssvc();
#else
#endif
int	getdirentries();
int	sun_statfs();
int	sun_fstatfs();
int	sun_unmount();
#ifdef NFSCLIENT
int	async_daemon();
int	getfh();
#else
#endif
int	getdomainname();
int	setdomainname();
int	sun_quotactl();
int	sun_exportfs();
int	sun_mount();
int	sun_ustat();
#ifdef SYSVSEM
int	semsys();
#else
#endif
#ifdef SYSVMSG
int	msgsys();
#else
#endif
#ifdef SYSVSHM
int	shmsys();
#else
#endif
int	sun_auditsys();
int	sun_getdents();
int	setsid();
int	fchdir();
int	sun_fchroot();
int	sun_sigpending();
int	setpgid();
int	sun_sysconf();
int	sun_uname();

#ifdef XXX_UNUSED
#define compat(n, name) n, __CONCAT(o,name)

int	osun_time();
int	osun_stat();
int	osetuid();
int	osun_stime();
int	osun_alarm();
int	osun_fstat();
int	osun_pause();
int	osun_utime();
int	osun_nice();
int	osun_ftime();
int	osun_setpgrp();
int	osun_times();
int	osun_setgid();
int	osun_ssig();
int	ovlimit();
int	owait();
int	ovtimes();
#ifdef NFSSERVER
#else
#endif
#ifdef NFSCLIENT
#else
#endif
#ifdef SYSVSEM
#else
#endif
#ifdef SYSVMSG
#else
#endif
#ifdef SYSVSHM
#else
#endif

#else /* XXX_UNUSED */
#define compat(n, name) 0, nosys
#endif /* XXX_UNUSED */

struct sysent sun_sysent[] = {
	{ 0, nosys },			/* 0 = syscall */
	{ 1, exit },			/* 1 = exit */
	{ 0, fork },			/* 2 = fork */
	{ 3, read },			/* 3 = read */
	{ 3, write },			/* 4 = write */
	{ 3, sun_open },			/* 5 = sun_open */
	{ 1, close },			/* 6 = close */
	{ 4, sun_wait4 },			/* 7 = sun_wait4 */
	{ 2, sun_creat },			/* 8 = sun_creat */
	{ 2, link },			/* 9 = link */
	{ 1, unlink },			/* 10 = unlink */
	{ 2, sun_execv },			/* 11 = sun_execv */
	{ 1, chdir },			/* 12 = chdir */
	{ compat(0,sun_time) },		/* 13 = old sun_time */
	{ 3, sun_mknod },			/* 14 = sun_mknod */
	{ 2, chmod },			/* 15 = chmod */
	{ 3, chown },			/* 16 = chown */
	{ 1, obreak },			/* 17 = break */
	{ compat(2,sun_stat) },		/* 18 = old sun_stat */
	{ 3, olseek },			/* 19 = lseek */
	{ 0, getpid },			/* 20 = getpid */
	{ 0, nosys },			/* 21 = obsolete sun_old_mount */
	{ 0, nosys },			/* 22 = System V umount */
	{ compat(1,setuid) },		/* 23 = old setuid */
	{ 0, getuid },			/* 24 = getuid */
	{ compat(1,sun_stime) },		/* 25 = old sun_stime */
	{ 0, nosys },			/* 26 = sun_ptrace */
	{ compat(1,sun_alarm) },		/* 27 = old sun_alarm */
	{ compat(1,sun_fstat) },		/* 28 = old sun_fstat */
	{ compat(0,sun_pause) },		/* 29 = old sun_pause */
	{ compat(2,sun_utime) },		/* 30 = old sun_utime */
	{ 0, nosys },			/* 31 = was stty */
	{ 0, nosys },			/* 32 = was gtty */
	{ 2, access },			/* 33 = access */
	{ compat(1,sun_nice) },		/* 34 = old sun_nice */
	{ compat(1,sun_ftime) },		/* 35 = old sun_ftime */
	{ 0, sync },			/* 36 = sync */
	{ 2, kill },			/* 37 = kill */
	{ 2, ostat },			/* 38 = stat */
	{ compat(2,sun_setpgrp) },		/* 39 = old sun_setpgrp */
	{ 2, olstat },			/* 40 = lstat */
	{ 2, dup },			/* 41 = dup */
	{ 0, pipe },			/* 42 = pipe */
	{ compat(1,sun_times) },		/* 43 = old sun_times */
	{ 4, profil },			/* 44 = profil */
	{ 0, nosys },			/* 45 = nosys */
	{ compat(1,sun_setgid) },		/* 46 = old sun_setgid */
	{ 0, getgid },			/* 47 = getgid */
	{ compat(2,sun_ssig) },		/* 48 = old sun_ssig */
	{ 0, nosys },			/* 49 = reserved for USG */
	{ 0, nosys },			/* 50 = reserved for USG */
	{ 1, acct },			/* 51 = acct */
	{ 0, nosys },			/* 52 = nosys */
	{ 4, sun_mctl },			/* 53 = sun_mctl */
	{ 3, sun_ioctl },			/* 54 = sun_ioctl */
	{ 2, reboot },			/* 55 = reboot */
	{ 0, nosys },			/* 56 = obsolete sun_owait3 */
	{ 2, symlink },			/* 57 = symlink */
	{ 3, readlink },			/* 58 = readlink */
	{ 3, execve },			/* 59 = execve */
	{ 1, umask },			/* 60 = umask */
	{ 1, chroot },			/* 61 = chroot */
	{ 2, ofstat },			/* 62 = ofstat */
	{ 0, nosys },			/* 63 = nosys */
	{ 0, getpagesize },			/* 64 = getpagesize */
	{ 3, sun_omsync },			/* 65 = sun_omsync */
	{ 0, vfork },			/* 66 = vfork */
	{ 0, nosys },			/* 67 = obsolete vread */
	{ 0, nosys },			/* 68 = obsolete vwrite */
	{ 1, sbrk },			/* 69 = sbrk */
	{ 1, sstk },			/* 70 = sstk */
	{ 6, sun_mmap },			/* 71 = mmap */
	{ 1, ovadvise },			/* 72 = vadvise */
	{ 2, munmap },			/* 73 = munmap */
	{ 3, mprotect },			/* 74 = mprotect */
	{ 3, madvise },			/* 75 = madvise */
	{ 0, sun_vhangup },			/* 76 = sun_vhangup */
	{ compat(0,vlimit) },		/* 77 = old vlimit */
	{ 3, mincore },			/* 78 = mincore */
	{ 2, getgroups },			/* 79 = getgroups */
	{ 2, setgroups },			/* 80 = setgroups */
	{ 1, getpgrp },			/* 81 = getpgrp */
	{ 2, sun_setpgid },			/* 82 = sun_setpgid */
	{ 3, setitimer },			/* 83 = setitimer */
	{ compat(0,wait) },		/* 84 = old wait */
	{ 1, swapon },			/* 85 = swapon */
	{ 2, getitimer },			/* 86 = getitimer */
	{ 2, gethostname },			/* 87 = gethostname */
	{ 2, sethostname },			/* 88 = sethostname */
	{ 0, getdtablesize },			/* 89 = getdtablesize */
	{ 2, dup2 },			/* 90 = dup2 */
	{ 0, nosys },			/* 91 = getdopt */
	{ 3, fcntl },			/* 92 = fcntl */
	{ 5, select },			/* 93 = select */
	{ 0, nosys },			/* 94 = setdopt */
	{ 1, fsync },			/* 95 = fsync */
	{ 3, setpriority },			/* 96 = setpriority */
	{ 3, socket },			/* 97 = socket */
	{ 3, connect },			/* 98 = connect */
	{ 3, oaccept },			/* 99 = oaccept */
	{ 2, getpriority },			/* 100 = getpriority */
	{ 4, osend },			/* 101 = osend */
	{ 4, orecv },			/* 102 = orecv */
	{ 0, nosys },			/* 103 = old socketaddr */
	{ 3, bind },			/* 104 = bind */
	{ 5, sun_setsockopt },			/* 105 = sun_setsockopt */
	{ 2, listen },			/* 106 = listen */
	{ compat(0,vtimes) },		/* 107 = old vtimes */
	{ 3, osigvec },			/* 108 = osigvec */
	{ 1, osigblock },			/* 109 = osigblock */
	{ 1, osigsetmask },			/* 110 = osigsetmask */
	{ 1, sigsuspend },			/* 111 = sigsuspend */
	{ 2, sigstack },			/* 112 = sigstack */
	{ 3, orecvmsg },			/* 113 = orecvmsg */
	{ 3, osendmsg },			/* 114 = osendmsg */
	{ 0, nosys },			/* 115 = obsolete vtrace */
	{ 2, gettimeofday },			/* 116 = gettimeofday */
	{ 2, getrusage },			/* 117 = getrusage */
	{ 5, getsockopt },			/* 118 = getsockopt */
	{ 0, nosys },			/* 119 = nosys */
	{ 3, readv },			/* 120 = readv */
	{ 3, writev },			/* 121 = writev */
	{ 2, settimeofday },			/* 122 = settimeofday */
	{ 3, fchown },			/* 123 = fchown */
	{ 2, fchmod },			/* 124 = fchmod */
	{ 6, orecvfrom },			/* 125 = orecvfrom */
	{ 2, osetreuid },			/* 126 = osetreuid */
	{ 2, osetregid },			/* 127 = osetregid */
	{ 2, rename },			/* 128 = rename */
	{ 2, otruncate },			/* 129 = truncate */
	{ 2, oftruncate },			/* 130 = ftruncate */
	{ 2, flock },			/* 131 = flock */
	{ 0, nosys },			/* 132 = nosys */
	{ 6, sendto },			/* 133 = sendto */
	{ 2, shutdown },			/* 134 = shutdown */
	{ 5, socketpair },			/* 135 = socketpair */
	{ 2, mkdir },			/* 136 = mkdir */
	{ 1, rmdir },			/* 137 = rmdir */
	{ 2, utimes },			/* 138 = utimes */
	{ 1, sigreturn },			/* 139 = sigreturn */
	{ 2, adjtime },			/* 140 = adjtime */
	{ 3, ogetpeername },			/* 141 = ogetpeername */
	{ 0, gethostid },			/* 142 = gethostid */
	{ 0, nosys },			/* 143 = old sethostid */
	{ 2, sun_getrlimit },			/* 144 = sun_getrlimit */
	{ 2, sun_setrlimit },			/* 145 = sun_setrlimit */
	{ 2, okillpg },			/* 146 = okillpg */
	{ 0, nosys },			/* 147 = nosys */
	{ 0, nosys },			/* 148 = nosys */
	{ 0, nosys },			/* 149 = nosys */
	{ 3, ogetsockname },			/* 150 = ogetsockname */
	{ 0, nosys },			/* 151 = getmsg */
	{ 0, nosys },			/* 152 = putmsg */
	{ 0, nosys },			/* 153 = poll */
	{ 0, nosys },			/* 154 = nosys */
#ifdef NFSSERVER
	{ 0, sun_nfssvc },			/* 155 = sun_nfssvc */
#else
	{ 0, nosys },			/* 155 = nosys */
#endif
	{ 4, getdirentries },			/* 156 = getdirentries */
	{ 2, sun_statfs },			/* 157 = sun_statfs */
	{ 2, sun_fstatfs },			/* 158 = sun_fstatfs */
	{ 1, sun_unmount },			/* 159 = sun_unmount */
#ifdef NFSCLIENT
	{ 0, async_daemon },			/* 160 = async_daemon */
	{ 2, getfh },			/* 161 = getfh */
#else
	{ 0, nosys },			/* 160 = nosys */
	{ 0, nosys },			/* 161 = nosys */
#endif
	{ 2, getdomainname },			/* 162 = getdomainname */
	{ 2, setdomainname },			/* 163 = setdomainname */
	{ 0, nosys },			/* 164 = rtschedule */
	{ 4, sun_quotactl },			/* 165 = sun_quotactl */
	{ 2, sun_exportfs },			/* 166 = sun_exportfs */
	{ 4, sun_mount },			/* 167 = sun_mount */
	{ 2, sun_ustat },			/* 168 = sun_ustat */
#ifdef SYSVSEM
	{ 5, semsys },			/* 169 = semsys */
#else
	{ 0, nosys },			/* 169 = nosys */
#endif
#ifdef SYSVMSG
	{ 6, msgsys },			/* 170 = msgsys */
#else
	{ 0, nosys },			/* 170 = nosys */
#endif
#ifdef SYSVSHM
	{ 4, shmsys },			/* 171 = shmsys */
#else
	{ 0, nosys },			/* 171 = nosys */
#endif
	{ 4, sun_auditsys },			/* 172 = sun_auditsys */
	{ 0, nosys },			/* 173 = rfssys */
	{ 3, sun_getdents },			/* 174 = sun_getdents */
	{ 1, setsid },			/* 175 = setsid */
	{ 1, fchdir },			/* 176 = fchdir */
	{ 1, sun_fchroot },			/* 177 = sun_fchroot */
	{ 0, nosys },			/* 178 = vpixsys */
	{ 0, nosys },			/* 179 = aioread */
	{ 0, nosys },			/* 180 = aiowrite */
	{ 0, nosys },			/* 181 = aiowait */
	{ 0, nosys },			/* 182 = aiocancel */
	{ 1, sun_sigpending },			/* 183 = sun_sigpending */
	{ 0, nosys },			/* 184 = nosys */
	{ 2, setpgid },			/* 185 = setpgid */
	{ 0, nosys },			/* 186 = pathconf */
	{ 0, nosys },			/* 187 = fpathconf */
	{ 1, sun_sysconf },			/* 188 = sun_sysconf */
	{ 1, sun_uname },			/* 189 = sun_uname */
};

int	nsun_sysent = sizeof(sun_sysent) / sizeof(sun_sysent[0]);
