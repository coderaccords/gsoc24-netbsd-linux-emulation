/*      $NetBSD: hijack.c,v 1.59 2011/02/20 23:47:04 pooka Exp $	*/

/*-
 * Copyright (c) 2011 Antti Kantee.  All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__RCSID("$NetBSD: hijack.c,v 1.59 2011/02/20 23:47:04 pooka Exp $");

#define __ssp_weak_name(fun) _hijack_ ## fun

#include <sys/param.h>
#include <sys/types.h>
#include <sys/event.h>
#include <sys/ioctl.h>
#include <sys/mount.h>
#include <sys/poll.h>
#include <sys/socket.h>
#include <sys/statvfs.h>

#include <rump/rumpclient.h>
#include <rump/rump_syscalls.h>

#include <assert.h>
#include <dlfcn.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <pthread.h>
#include <signal.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

enum dualcall {
	DUALCALL_WRITE, DUALCALL_WRITEV,
	DUALCALL_IOCTL, DUALCALL_FCNTL,
	DUALCALL_SOCKET, DUALCALL_ACCEPT, DUALCALL_BIND, DUALCALL_CONNECT,
	DUALCALL_GETPEERNAME, DUALCALL_GETSOCKNAME, DUALCALL_LISTEN,
	DUALCALL_RECVFROM, DUALCALL_RECVMSG,
	DUALCALL_SENDTO, DUALCALL_SENDMSG,
	DUALCALL_GETSOCKOPT, DUALCALL_SETSOCKOPT,
	DUALCALL_SHUTDOWN,
	DUALCALL_READ, DUALCALL_READV,
	DUALCALL_DUP2,
	DUALCALL_CLOSE,
	DUALCALL_POLLTS,
	DUALCALL_KEVENT,
	DUALCALL_STAT, DUALCALL_LSTAT, DUALCALL_FSTAT,
	DUALCALL_CHMOD, DUALCALL_LCHMOD, DUALCALL_FCHMOD,
	DUALCALL_CHOWN, DUALCALL_LCHOWN, DUALCALL_FCHOWN,
	DUALCALL_OPEN,
	DUALCALL_STATVFS1, DUALCALL_FSTATVFS1,
	DUALCALL_CHDIR, DUALCALL_FCHDIR,
	DUALCALL_LSEEK,
	DUALCALL_GETDENTS,
	DUALCALL_UNLINK, DUALCALL_SYMLINK, DUALCALL_READLINK,
	DUALCALL_RENAME,
	DUALCALL_MKDIR, DUALCALL_RMDIR,
	DUALCALL_UTIMES, DUALCALL_LUTIMES, DUALCALL_FUTIMES,
	DUALCALL_TRUNCATE, DUALCALL_FTRUNCATE,
	DUALCALL_FSYNC, DUALCALL_FSYNC_RANGE,
	DUALCALL_MOUNT, DUALCALL_UNMOUNT,
	DUALCALL___GETCWD,
	DUALCALL__NUM
};

#define RSYS_STRING(a) __STRING(a)
#define RSYS_NAME(a) RSYS_STRING(__CONCAT(RUMP_SYS_RENAME_,a))

/*
 * Would be nice to get this automatically in sync with libc.
 * Also, this does not work for compat-using binaries!
 */
#if !__NetBSD_Prereq__(5,99,7)
#define REALSELECT select
#define REALPOLLTS pollts
#define REALKEVENT kevent
#define REALSTAT __stat30
#define REALLSTAT __lstat30
#define REALFSTAT __fstat30
#define REALUTIMES utimes
#define REALLUTIMES lutimes
#define REALFUTIMES futimes
#else
#define REALSELECT _sys___select50
#define REALPOLLTS _sys___pollts50
#define REALKEVENT _sys___kevent50
#define REALSTAT __stat50
#define REALLSTAT __lstat50
#define REALFSTAT __fstat50
#define REALUTIMES __utimes50
#define REALLUTIMES __lutimes50
#define REALFUTIMES __futimes50
#endif
#define REALREAD _sys_read
#define REALGETDENTS __getdents30
#define REALMOUNT __mount50
#define REALLSEEK _lseek

int REALSELECT(int, fd_set *, fd_set *, fd_set *, struct timeval *);
int REALPOLLTS(struct pollfd *, nfds_t,
	       const struct timespec *, const sigset_t *);
int REALKEVENT(int, const struct kevent *, size_t, struct kevent *, size_t,
	       const struct timespec *);
ssize_t REALREAD(int, void *, size_t);
int REALSTAT(const char *, struct stat *);
int REALLSTAT(const char *, struct stat *);
int REALFSTAT(int, struct stat *);
int REALGETDENTS(int, char *, size_t);
int REALUTIMES(const char *, const struct timeval [2]);
int REALLUTIMES(const char *, const struct timeval [2]);
int REALFUTIMES(int, const struct timeval [2]);
int REALMOUNT(const char *, const char *, int, void *, size_t);
off_t REALLSEEK(int, off_t, int);
int __getcwd(char *, size_t);

#define S(a) __STRING(a)
struct sysnames {
	enum dualcall scm_callnum;
	const char *scm_hostname;
	const char *scm_rumpname;
} syscnames[] = {
	{ DUALCALL_SOCKET,	"__socket30",	RSYS_NAME(SOCKET)	},
	{ DUALCALL_ACCEPT,	"accept",	RSYS_NAME(ACCEPT)	},
	{ DUALCALL_BIND,	"bind",		RSYS_NAME(BIND)		},
	{ DUALCALL_CONNECT,	"connect",	RSYS_NAME(CONNECT)	},
	{ DUALCALL_GETPEERNAME,	"getpeername",	RSYS_NAME(GETPEERNAME)	},
	{ DUALCALL_GETSOCKNAME,	"getsockname",	RSYS_NAME(GETSOCKNAME)	},
	{ DUALCALL_LISTEN,	"listen",	RSYS_NAME(LISTEN)	},
	{ DUALCALL_RECVFROM,	"recvfrom",	RSYS_NAME(RECVFROM)	},
	{ DUALCALL_RECVMSG,	"recvmsg",	RSYS_NAME(RECVMSG)	},
	{ DUALCALL_SENDTO,	"sendto",	RSYS_NAME(SENDTO)	},
	{ DUALCALL_SENDMSG,	"sendmsg",	RSYS_NAME(SENDMSG)	},
	{ DUALCALL_GETSOCKOPT,	"getsockopt",	RSYS_NAME(GETSOCKOPT)	},
	{ DUALCALL_SETSOCKOPT,	"setsockopt",	RSYS_NAME(SETSOCKOPT)	},
	{ DUALCALL_SHUTDOWN,	"shutdown",	RSYS_NAME(SHUTDOWN)	},
	{ DUALCALL_READ,	S(REALREAD),	RSYS_NAME(READ)		},
	{ DUALCALL_READV,	"readv",	RSYS_NAME(READV)	},
	{ DUALCALL_WRITE,	"write",	RSYS_NAME(WRITE)	},
	{ DUALCALL_WRITEV,	"writev",	RSYS_NAME(WRITEV)	},
	{ DUALCALL_IOCTL,	"ioctl",	RSYS_NAME(IOCTL)	},
	{ DUALCALL_FCNTL,	"fcntl",	RSYS_NAME(FCNTL)	},
	{ DUALCALL_DUP2,	"dup2",		RSYS_NAME(DUP2)		},
	{ DUALCALL_CLOSE,	"close",	RSYS_NAME(CLOSE)	},
	{ DUALCALL_POLLTS,	S(REALPOLLTS),	RSYS_NAME(POLLTS)	},
	{ DUALCALL_KEVENT,	S(REALKEVENT),	RSYS_NAME(KEVENT)	},
	{ DUALCALL_STAT,	S(REALSTAT),	RSYS_NAME(STAT)		},
	{ DUALCALL_LSTAT,	S(REALLSTAT),	RSYS_NAME(LSTAT)	},
	{ DUALCALL_FSTAT,	S(REALFSTAT),	RSYS_NAME(FSTAT)	},
	{ DUALCALL_CHOWN,	"chown",	RSYS_NAME(CHOWN)	},
	{ DUALCALL_LCHOWN,	"lchown",	RSYS_NAME(LCHOWN)	},
	{ DUALCALL_FCHOWN,	"fchown",	RSYS_NAME(FCHOWN)	},
	{ DUALCALL_CHMOD,	"chmod",	RSYS_NAME(CHMOD)	},
	{ DUALCALL_LCHMOD,	"lchmod",	RSYS_NAME(LCHMOD)	},
	{ DUALCALL_FCHMOD,	"fchmod",	RSYS_NAME(FCHMOD)	},
	{ DUALCALL_UTIMES,	S(REALUTIMES),	RSYS_NAME(UTIMES)	},
	{ DUALCALL_LUTIMES,	S(REALLUTIMES),	RSYS_NAME(LUTIMES)	},
	{ DUALCALL_FUTIMES,	S(REALFUTIMES),	RSYS_NAME(FUTIMES)	},
	{ DUALCALL_OPEN,	"open",		RSYS_NAME(OPEN)		},
	{ DUALCALL_STATVFS1,	"statvfs1",	RSYS_NAME(STATVFS1)	},
	{ DUALCALL_FSTATVFS1,	"fstatvfs1",	RSYS_NAME(FSTATVFS1)	},
	{ DUALCALL_CHDIR,	"chdir",	RSYS_NAME(CHDIR)	},
	{ DUALCALL_FCHDIR,	"fchdir",	RSYS_NAME(FCHDIR)	},
	{ DUALCALL_LSEEK,	S(REALLSEEK),	RSYS_NAME(LSEEK)	},
	{ DUALCALL_GETDENTS,	"__getdents30",	RSYS_NAME(GETDENTS)	},
	{ DUALCALL_UNLINK,	"unlink",	RSYS_NAME(UNLINK)	},
	{ DUALCALL_SYMLINK,	"symlink",	RSYS_NAME(SYMLINK)	},
	{ DUALCALL_READLINK,	"readlink",	RSYS_NAME(READLINK)	},
	{ DUALCALL_RENAME,	"rename",	RSYS_NAME(RENAME)	},
	{ DUALCALL_MKDIR,	"mkdir",	RSYS_NAME(MKDIR)	},
	{ DUALCALL_RMDIR,	"rmdir",	RSYS_NAME(RMDIR)	},
	{ DUALCALL_TRUNCATE,	"truncate",	RSYS_NAME(TRUNCATE)	},
	{ DUALCALL_FTRUNCATE,	"ftruncate",	RSYS_NAME(FTRUNCATE)	},
	{ DUALCALL_FSYNC,	"fsync",	RSYS_NAME(FSYNC)	},
	{ DUALCALL_FSYNC_RANGE,	"fsync_range",	RSYS_NAME(FSYNC_RANGE)	},
	{ DUALCALL_MOUNT,	S(REALMOUNT),	RSYS_NAME(MOUNT)	},
	{ DUALCALL_UNMOUNT,	"unmount",	RSYS_NAME(UNMOUNT)	},
	{ DUALCALL___GETCWD,	"__getcwd",	RSYS_NAME(__GETCWD)	},
};
#undef S

struct bothsys {
	void *bs_host;
	void *bs_rump;
} syscalls[DUALCALL__NUM];
#define GETSYSCALL(which, name) syscalls[DUALCALL_##name].bs_##which

pid_t	(*host_fork)(void);
int	(*host_daemon)(int, int);
int	(*host_execve)(const char *, char *const[], char *const[]);

/* ok, we need *two* bits per dup2'd fd to track fd+HIJACKOFF aliases */
static uint32_t dup2mask;
#define ISDUP2D(fd) (((fd) < 16) && (1<<(fd) & dup2mask))
#define SETDUP2(fd) \
    do { if ((fd) < 16) dup2mask |= (1<<(fd)); } while (/*CONSTCOND*/0)
#define CLRDUP2(fd) \
    do { if ((fd) < 16) dup2mask &= ~(1<<(fd)); } while (/*CONSTCOND*/0)
#define ISDUP2ALIAS(fd) (((fd) < 16) && (1<<((fd)+16) & dup2mask))
#define SETDUP2ALIAS(fd) \
    do { if ((fd) < 16) dup2mask |= (1<<((fd)+16)); } while (/*CONSTCOND*/0)
#define CLRDUP2ALIAS(fd) \
    do { if ((fd) < 16) dup2mask &= ~(1<<((fd)+16)); } while (/*CONSTCOND*/0)

//#define DEBUGJACK
#ifdef DEBUGJACK
#define DPRINTF(x) mydprintf x
static void
mydprintf(const char *fmt, ...)
{
	va_list ap;

	if (ISDUP2D(STDERR_FILENO))
		return;

	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
}

#else
#define DPRINTF(x)
#endif

#define FDCALL(type, name, rcname, args, proto, vars)			\
type name args								\
{									\
	type (*fun) proto;						\
									\
	DPRINTF(("%s -> %d\n", __STRING(name), fd));			\
	if (fd_isrump(fd)) {						\
		fun = syscalls[rcname].bs_rump;				\
		fd = fd_host2rump(fd);					\
	} else {							\
		fun = syscalls[rcname].bs_host;				\
	}								\
									\
	return fun vars;						\
}

#define PATHCALL(type, name, rcname, args, proto, vars)			\
type name args								\
{									\
	type (*fun) proto;						\
									\
	DPRINTF(("%s -> %s\n", __STRING(name), path));			\
	if (path_isrump(path)) {					\
		fun = syscalls[rcname].bs_rump;				\
		path = path_host2rump(path);				\
	} else {							\
		fun = syscalls[rcname].bs_host;				\
	}								\
									\
	return fun vars;						\
}

/*
 * This is called from librumpclient in case of LD_PRELOAD.
 * It ensures correct RTLD_NEXT.
 *
 * ... except, it's apparently extremely difficult to force
 * at least gcc to generate an actual stack frame here.  So
 * sprinkle some volatile foobar and baz to throw the optimizer
 * off the scent and generate a variable assignment with the
 * return value.  The posterboy for this meltdown is amd64
 * with -O2.  At least with gcc 4.1.3 i386 works regardless of
 * optimization.
 */
volatile int rumphijack_unrope; /* there, unhang yourself */
static void *
hijackdlsym(void *handle, const char *symbol)
{
	void *rv;

	rv = dlsym(handle, symbol);
	rumphijack_unrope = *(volatile int *)rv;

	return (void *)rv;
}

/*
 * This tracks if our process is in a subdirectory of /rump.
 * It's preserved over exec.
 */
static bool pwdinrump = false;

/*
 * These variables are set from the RUMPHIJACK string and control
 * which operations can product rump kernel file descriptors.
 * This should be easily extendable for future needs.
 */
#define RUMPHIJACK_DEFAULT "path=/rump,socket=all:nolocal"
static bool rumpsockets[PF_MAX];
static const char *rumpprefix;
static size_t rumpprefixlen;

static struct {
	int pf;
	const char *name;
} socketmap[] = {
	{ PF_LOCAL, "local" },
	{ PF_INET, "inet" },
	{ PF_LINK, "link" },
#ifdef PF_OROUTE
	{ PF_OROUTE, "oroute" },
#endif
	{ PF_ROUTE, "route" },
	{ PF_INET6, "inet6" },
#ifdef PF_MPLS 
	{ PF_MPLS, "mpls" },
#endif
	{ -1, NULL }
};

static void
sockparser(char *buf)
{
	char *p, *l;
	bool value;
	int i;

	/* if "all" is present, it must be specified first */
	if (strncmp(buf, "all", strlen("all")) == 0) {
		for (i = 0; i < (int)__arraycount(rumpsockets); i++) {
			rumpsockets[i] = true;
		}
		buf += strlen("all");
		if (*buf == ':')
			buf++;
	}

	for (p = strtok_r(buf, ":", &l); p; p = strtok_r(NULL, ":", &l)) {
		value = true;
		if (strncmp(p, "no", strlen("no")) == 0) {
			value = false;
			p += strlen("no");
		}

		for (i = 0; socketmap[i].name; i++) {
			if (strcmp(p, socketmap[i].name) == 0) {
				rumpsockets[socketmap[i].pf] = value;
				break;
			}
		}
		if (socketmap[i].name == NULL) {
			warnx("invalid socket specifier %s", p);
		}
	}
}

static void
pathparser(char *buf)
{

	/* sanity-check */
	if (*buf != '/')
		errx(1, "hijack path specifier must begin with ``/''");
	rumpprefixlen = strlen(buf);
	if (rumpprefixlen < 2)
		errx(1, "invalid hijack prefix: %s", buf);
	if (buf[rumpprefixlen-1] == '/' && strspn(buf, "/") != rumpprefixlen)
		errx(1, "hijack prefix may end in slash only if pure "
		    "slash, gave %s", buf);

	if ((rumpprefix = strdup(buf)) == NULL)
		err(1, "strdup");
	rumpprefixlen = strlen(rumpprefix);
}

static struct {
	void (*parsefn)(char *);
	const char *name;
} hijackparse[] = {
	{ sockparser, "socket" },
	{ pathparser, "path" },
	{ NULL, NULL },
};

static void
parsehijack(char *hijack)
{
	char *p, *p2, *l;
	const char *hijackcopy;
	int i;

	if ((hijackcopy = strdup(hijack)) == NULL)
		err(1, "strdup");

	/* disable everything explicitly */
	for (i = 0; i < PF_MAX; i++)
		rumpsockets[i] = false;

	for (p = strtok_r(hijack, ",", &l); p; p = strtok_r(NULL, ",", &l)) {
		p2 = strchr(p, '=');
		if (!p2)
			errx(1, "invalid hijack specifier: %s", hijackcopy);

		for (i = 0; hijackparse[i].parsefn; i++) {
			if (strncmp(hijackparse[i].name, p,
			    (size_t)(p2-p)) == 0) {
				hijackparse[i].parsefn(p2+1);
				break;
			}
		}
	}

}

static void __attribute__((constructor))
rcinit(void)
{
	char buf[1024];
	extern void *(*rumpclient_dlsym)(void *, const char *);
	unsigned i, j;

	rumpclient_dlsym = hijackdlsym;
	host_fork = dlsym(RTLD_NEXT, "fork");
	host_daemon = dlsym(RTLD_NEXT, "daemon");
	host_execve = dlsym(RTLD_NEXT, "execve");

	/*
	 * In theory cannot print anything during lookups because
	 * we might not have the call vector set up.  so, the errx()
	 * is a bit of a strech, but it might work.
	 */

	for (i = 0; i < DUALCALL__NUM; i++) {
		/* build runtime O(1) access */
		for (j = 0; j < __arraycount(syscnames); j++) {
			if (syscnames[j].scm_callnum == i)
				break;
		}

		if (j == __arraycount(syscnames))
			errx(1, "rumphijack error: syscall pos %d missing", i);

		syscalls[i].bs_host = dlsym(RTLD_NEXT,
		    syscnames[j].scm_hostname);
		if (syscalls[i].bs_host == NULL)
			errx(1, "hostcall %s not found missing",
			    syscnames[j].scm_hostname);

		syscalls[i].bs_rump = dlsym(RTLD_NEXT,
		    syscnames[j].scm_rumpname);
		if (syscalls[i].bs_rump == NULL)
			errx(1, "rumpcall %s not found missing",
			    syscnames[j].scm_rumpname);
	}

	if (rumpclient_init() == -1)
		err(1, "rumpclient init");

	/* check which syscalls we're supposed to hijack */
	if (getenv_r("RUMPHIJACK", buf, sizeof(buf)) == -1) {
		strcpy(buf, RUMPHIJACK_DEFAULT);
	}
	parsehijack(buf);

	/* set client persistence level */
	if (getenv_r("RUMPHIJACK_RETRYCONNECT", buf, sizeof(buf)) != -1) {
		if (strcmp(buf, "die") == 0)
			rumpclient_setconnretry(RUMPCLIENT_RETRYCONN_DIE);
		else if (strcmp(buf, "inftime") == 0)
			rumpclient_setconnretry(RUMPCLIENT_RETRYCONN_INFTIME);
		else if (strcmp(buf, "once") == 0)
			rumpclient_setconnretry(RUMPCLIENT_RETRYCONN_ONCE);
		else {
			time_t timeout;
			char *ep;

			timeout = (time_t)strtoll(buf, &ep, 10);
			if (timeout <= 0 || ep != buf + strlen(buf))
				errx(1, "RUMPHIJACK_RETRYCONNECT must be "
				    "keyword or integer, got: %s", buf);

			rumpclient_setconnretry(timeout);
		}
	}

	if (getenv_r("RUMPHIJACK__DUP2MASK", buf, sizeof(buf)) == 0) {
		dup2mask = strtoul(buf, NULL, 10);
		unsetenv("RUMPHIJACK__DUP2MASK");
	}
	if (getenv_r("RUMPHIJACK__PWDINRUMP", buf, sizeof(buf)) == 0) {
		pwdinrump = true;
		unsetenv("RUMPHIJACK__PWDINRUMP");
	}
}

/* XXX: need runtime selection.  low for now due to FD_SETSIZE */
#define HIJACK_FDOFF 128
static int
fd_rump2host(int fd)
{

	if (fd == -1)
		return fd;

	if (!ISDUP2D(fd))
		fd += HIJACK_FDOFF;

	return fd;
}

static int
fd_host2rump(int fd)
{

	if (!ISDUP2D(fd))
		fd -= HIJACK_FDOFF;
	return fd;
}

static bool
fd_isrump(int fd)
{

	return ISDUP2D(fd) || fd >= HIJACK_FDOFF;
}

#define assertfd(_fd_) assert(ISDUP2D(_fd_) || (_fd_) >= HIJACK_FDOFF)

static bool
path_isrump(const char *path)
{

	if (rumpprefix == NULL)
		return false;

	if (*path == '/') {
		if (strncmp(path, rumpprefix, rumpprefixlen) == 0)
			return true;
		return false;
	} else {
		return pwdinrump;
	}
}

static const char *rootpath = "/";
static const char *
path_host2rump(const char *path)
{
	const char *rv;

	if (*path == '/') {
		rv = path + rumpprefixlen;
		if (*rv == '\0')
			rv = rootpath;
	} else {
		rv = path;
	}

	return rv;
}

static int
dodup(int oldd, int minfd)
{
	int (*op_fcntl)(int, int, ...);
	int newd;
	int isrump;

	DPRINTF(("dup -> %d (minfd %d)\n", oldd, minfd));
	if (fd_isrump(oldd)) {
		op_fcntl = GETSYSCALL(rump, FCNTL);
		oldd = fd_host2rump(oldd);
		isrump = 1;
	} else {
		op_fcntl = GETSYSCALL(host, FCNTL);
		isrump = 0;
	}

	newd = op_fcntl(oldd, F_DUPFD, minfd);

	if (isrump)
		newd = fd_rump2host(newd);
	DPRINTF(("dup <- %d\n", newd));

	return newd;
}

/*
 * dup a host file descriptor so that it doesn't collide with the dup2mask
 */
static int
fd_dupgood(int fd)
{
	int (*op_fcntl)(int, int, ...) = GETSYSCALL(host, FCNTL);
	int (*op_close)(int) = GETSYSCALL(host, CLOSE);
	int ofd, i;

	for (i = 1; ISDUP2D(fd); i++) {
		ofd = fd;
		fd = op_fcntl(ofd, F_DUPFD, i);
		op_close(ofd);
	}

	return fd;
}

int
open(const char *path, int flags, ...)
{
	int (*op_open)(const char *, int, ...);
	bool isrump;
	va_list ap;
	int fd;

	if (path_isrump(path)) {
		path = path_host2rump(path);
		op_open = GETSYSCALL(rump, OPEN);
		isrump = true;
	} else {
		op_open = GETSYSCALL(host, OPEN);
		isrump = false;
	}

	va_start(ap, flags);
	fd = op_open(path, flags, va_arg(ap, mode_t));
	va_end(ap);

	if (isrump)
		fd = fd_rump2host(fd);
	else
		fd = fd_dupgood(fd);
	return fd;
}

int
chdir(const char *path)
{
	int (*op_chdir)(const char *);
	bool isrump;
	int rv;

	if (path_isrump(path)) {
		op_chdir = GETSYSCALL(rump, CHDIR);
		isrump = true;
		path = path_host2rump(path);
	} else {
		op_chdir = GETSYSCALL(host, CHDIR);
		isrump = false;
	}

	rv = op_chdir(path);
	if (rv == 0) {
		if (isrump)
			pwdinrump = true;
		else
			pwdinrump = false;
	}

	return rv;
}

int
fchdir(int fd)
{
	int (*op_fchdir)(int);
	bool isrump;
	int rv;

	if (fd_isrump(fd)) {
		op_fchdir = GETSYSCALL(rump, FCHDIR);
		isrump = true;
		fd = fd_host2rump(fd);
	} else {
		op_fchdir = GETSYSCALL(host, FCHDIR);
		isrump = false;
	}

	rv = op_fchdir(fd);
	if (rv == 0) {
		if (isrump)
			pwdinrump = true;
		else
			pwdinrump = false;
	}

	return rv;
}

int
__getcwd(char *bufp, size_t len)
{
	int (*op___getcwd)(char *, size_t);
	int rv;

	if (pwdinrump) {
		size_t prefixgap;
		bool iamslash;

		if (rumpprefix[rumpprefixlen-1] == '/')
			iamslash = true;
		else
			iamslash = false;

		if (iamslash)
			prefixgap = rumpprefixlen - 1; /* ``//+path'' */
		else
			prefixgap = rumpprefixlen; /* ``/pfx+/path'' */
		if (len <= prefixgap) {
			return ERANGE;
		}

		op___getcwd = GETSYSCALL(rump, __GETCWD);
		rv = op___getcwd(bufp + prefixgap, len - prefixgap);
		if (rv == -1)
			return rv;

		/* augment the "/" part only for a non-root path */
		memcpy(bufp, rumpprefix, rumpprefixlen);

		/* append / only to non-root cwd */
		if (rv != 2)
			bufp[prefixgap] = '/';

		/* don't append extra slash in the purely-slash case */
		if (rv == 2 && !iamslash)
			bufp[rumpprefixlen] = '\0';

		return rv;
	} else {
		op___getcwd = GETSYSCALL(host, __GETCWD);
		return op___getcwd(bufp, len);
	}
}

int
rename(const char *from, const char *to)
{
	int (*op_rename)(const char *, const char *);

	if (path_isrump(from)) {
		if (!path_isrump(to))
			return EXDEV;

		from = path_host2rump(from);
		to = path_host2rump(to);
		op_rename = GETSYSCALL(rump, RENAME);
	} else {
		if (path_isrump(to))
			return EXDEV;

		op_rename = GETSYSCALL(host, RENAME);
	}

	return op_rename(from, to);
}

int __socket30(int, int, int);
int
__socket30(int domain, int type, int protocol)
{
	int (*op_socket)(int, int, int);
	int fd;
	bool isrump;

	isrump = domain < PF_MAX && rumpsockets[domain];

	if (isrump)
		op_socket = GETSYSCALL(rump, SOCKET);
	else
		op_socket = GETSYSCALL(host, SOCKET);
	fd = op_socket(domain, type, protocol);

	if (isrump)
		fd = fd_rump2host(fd);
	else
		fd = fd_dupgood(fd);
	DPRINTF(("socket <- %d\n", fd));

	return fd;
}

int
accept(int s, struct sockaddr *addr, socklen_t *addrlen)
{
	int (*op_accept)(int, struct sockaddr *, socklen_t *);
	int fd;
	bool isrump;

	isrump = fd_isrump(s);

	DPRINTF(("accept -> %d", s));
	if (isrump) {
		op_accept = GETSYSCALL(rump, ACCEPT);
		s = fd_host2rump(s);
	} else {
		op_accept = GETSYSCALL(host, ACCEPT);
	}
	fd = op_accept(s, addr, addrlen);
	if (fd != -1 && isrump)
		fd = fd_rump2host(fd);
	else
		fd = fd_dupgood(fd);

	DPRINTF((" <- %d\n", fd));

	return fd;
}

/*
 * ioctl and fcntl are varargs calls and need special treatment
 */
int
ioctl(int fd, unsigned long cmd, ...)
{
	int (*op_ioctl)(int, unsigned long cmd, ...);
	va_list ap;
	int rv;

	DPRINTF(("ioctl -> %d\n", fd));
	if (fd_isrump(fd)) {
		fd = fd_host2rump(fd);
		op_ioctl = GETSYSCALL(rump, IOCTL);
	} else {
		op_ioctl = GETSYSCALL(host, IOCTL);
	}

	va_start(ap, cmd);
	rv = op_ioctl(fd, cmd, va_arg(ap, void *));
	va_end(ap);
	return rv;
}

#include <syslog.h>
int
fcntl(int fd, int cmd, ...)
{
	int (*op_fcntl)(int, int, ...);
	va_list ap;
	int rv, minfd, i;

	DPRINTF(("fcntl -> %d (cmd %d)\n", fd, cmd));

	switch (cmd) {
	case F_DUPFD:
		va_start(ap, cmd);
		minfd = va_arg(ap, int);
		va_end(ap);
		return dodup(fd, minfd);

	case F_CLOSEM:
		/*
		 * So, if fd < HIJACKOFF, we want to do a host closem.
		 */

		if (fd < HIJACK_FDOFF) {
			int closemfd = fd;

			if (rumpclient__closenotify(&closemfd,
			    RUMPCLIENT_CLOSE_FCLOSEM) == -1)
				return -1;
			op_fcntl = GETSYSCALL(host, FCNTL);
			rv = op_fcntl(closemfd, cmd);
			if (rv)
				return rv;
		}

		/*
		 * Additionally, we want to do a rump closem, but only
		 * for the file descriptors not within the dup2mask.
		 */

		/* why don't we offer fls()? */
		for (i = 15; i >= 0; i--) {
			if (ISDUP2D(i))
				break;
		}
		
		if (fd >= HIJACK_FDOFF)
			fd -= HIJACK_FDOFF;
		else
			fd = 0;
		fd = MAX(i+1, fd);

		/* hmm, maybe we should close rump fd's not within dup2mask? */

		return rump_sys_fcntl(fd, F_CLOSEM);

	case F_MAXFD:
		/*
		 * For maxfd, if there's a rump kernel fd, return
		 * it hostified.  Otherwise, return host's MAXFD
		 * return value.
		 */
		if ((rv = rump_sys_fcntl(fd, F_MAXFD)) != -1) {
			/*
			 * This might go a little wrong in case
			 * of dup2 to [012], but I'm not sure if
			 * there's a justification for tracking
			 * that info.  Consider e.g.
			 * dup2(rumpfd, 2) followed by rump_sys_open()
			 * returning 1.  We should return 1+HIJACKOFF,
			 * not 2+HIJACKOFF.  However, if [01] is not
			 * open, the correct return value is 2.
			 */
			return fd_rump2host(fd);
		} else {
			op_fcntl = GETSYSCALL(host, FCNTL);
			return op_fcntl(fd, F_MAXFD);
		}
		/*NOTREACHED*/

	default:
		if (fd_isrump(fd)) {
			fd = fd_host2rump(fd);
			op_fcntl = GETSYSCALL(rump, FCNTL);
		} else {
			op_fcntl = GETSYSCALL(host, FCNTL);
		}

		va_start(ap, cmd);
		rv = op_fcntl(fd, cmd, va_arg(ap, void *));
		va_end(ap);
		return rv;
	}
	/*NOTREACHED*/
}

int
close(int fd)
{
	int (*op_close)(int);
	int rv;

	DPRINTF(("close -> %d\n", fd));
	if (fd_isrump(fd)) {
		int undup2 = 0;

		fd = fd_host2rump(fd);
		if (ISDUP2ALIAS(fd)) {
			_DIAGASSERT(ISDUP2D(fd));
			CLRDUP2ALIAS(fd);
			return 0;
		}

		if (ISDUP2D(fd))
			undup2 = 1;
		op_close = GETSYSCALL(rump, CLOSE);
		rv = op_close(fd);
		if (rv == 0 && undup2)
			CLRDUP2(fd);
	} else {
		if (rumpclient__closenotify(&fd, RUMPCLIENT_CLOSE_CLOSE) == -1)
			return -1;
		op_close = GETSYSCALL(host, CLOSE);
		rv = op_close(fd);
	}

	return rv;
}

/*
 * write cannot issue a standard debug printf due to recursion
 */
ssize_t
write(int fd, const void *buf, size_t blen)
{
	ssize_t (*op_write)(int, const void *, size_t);

	if (fd_isrump(fd)) {
		fd = fd_host2rump(fd);
		op_write = GETSYSCALL(rump, WRITE);
	} else {
		op_write = GETSYSCALL(host, WRITE);
	}

	return op_write(fd, buf, blen);
}

/*
 * dup2 is special.  we allow dup2 of a rump kernel fd to 0-2 since
 * many programs do that.  dup2 of a rump kernel fd to another value
 * not >= fdoff is an error.
 *
 * Note: cannot rump2host newd, because it is often hardcoded.
 */
int
dup2(int oldd, int newd)
{
	int (*host_dup2)(int, int);
	int rv;

	DPRINTF(("dup2 -> %d (o) -> %d (n)\n", oldd, newd));

	if (fd_isrump(oldd)) {
		if (!(newd >= 0 && newd <= 2))
			return EBADF;
		oldd = fd_host2rump(oldd);
		if (oldd == newd) {
			SETDUP2(newd);
			SETDUP2ALIAS(newd);
			return newd;
		}
		rv = rump_sys_dup2(oldd, newd);
		if (rv != -1)
			SETDUP2(newd);
	} else {
		host_dup2 = syscalls[DUALCALL_DUP2].bs_host;
		if (rumpclient__closenotify(&newd, RUMPCLIENT_CLOSE_DUP2) == -1)
			return -1;
		rv = host_dup2(oldd, newd);
	}

	return rv;
}

int
dup(int oldd)
{

	return dodup(oldd, 0);
}

pid_t
fork()
{
	pid_t rv;

	DPRINTF(("fork\n"));

	rv = rumpclient__dofork(host_fork);

	DPRINTF(("fork returns %d\n", rv));
	return rv;
}
/* we do not have the luxury of not requiring a stackframe */
__strong_alias(__vfork14,fork);

int
daemon(int nochdir, int noclose)
{
	struct rumpclient_fork *rf;

	if ((rf = rumpclient_prefork()) == NULL)
		return -1;

	if (host_daemon(nochdir, noclose) == -1)
		return -1;

	if (rumpclient_fork_init(rf) == -1)
		return -1;

	return 0;
}

int
execve(const char *path, char *const argv[], char *const envp[])
{
	char buf[128];
	char *dup2str;
	const char *pwdinrumpstr;
	char **newenv;
	size_t nelem;
	int rv, sverrno;
	int bonus = 1, i = 0;

	if (dup2mask) {
		snprintf(buf, sizeof(buf), "RUMPHIJACK__DUP2MASK=%u", dup2mask);
		dup2str = malloc(strlen(buf)+1);
		if (dup2str == NULL)
			return ENOMEM;
		strcpy(dup2str, buf);
		bonus++;
	} else {
		dup2str = NULL;
	}

	if (pwdinrump) {
		pwdinrumpstr = "RUMPHIJACK__PWDINRUMP=true";
		bonus++;
	} else {
		pwdinrumpstr = NULL;
	}

	for (nelem = 0; envp && envp[nelem]; nelem++)
		continue;
	newenv = malloc(sizeof(*newenv) * nelem+bonus);
	if (newenv == NULL) {
		free(dup2str);
		return ENOMEM;
	}
	memcpy(newenv, envp, nelem*sizeof(*newenv));
	if (dup2str) {
		newenv[nelem+i] = dup2str;
		i++;
	}
	if (pwdinrumpstr) {
		newenv[nelem+i] = __UNCONST(pwdinrumpstr);
		i++;
	}
	newenv[nelem+i] = NULL;
	_DIAGASSERT(i < bonus);

	rv = rumpclient_exec(path, argv, newenv);

	_DIAGASSERT(rv != 0);
	sverrno = errno;
	free(newenv);
	free(dup2str);
	errno = sverrno;
	return rv;
}

/*
 * select is done by calling poll.
 */
int
REALSELECT(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds,
	struct timeval *timeout)
{
	struct pollfd *pfds;
	struct timespec ts, *tsp = NULL;
	nfds_t realnfds;
	int i, j;
	int rv, incr;

	DPRINTF(("select\n"));

	/*
	 * Well, first we must scan the fds to figure out how many
	 * fds there really are.  This is because up to and including
	 * nb5 poll() silently refuses nfds > process_maxopen_fds.
	 * Seems to be fixed in current, thank the maker.
	 * god damn cluster...bomb.
	 */
	
	for (i = 0, realnfds = 0; i < nfds; i++) {
		if (readfds && FD_ISSET(i, readfds)) {
			realnfds++;
			continue;
		}
		if (writefds && FD_ISSET(i, writefds)) {
			realnfds++;
			continue;
		}
		if (exceptfds && FD_ISSET(i, exceptfds)) {
			realnfds++;
			continue;
		}
	}

	if (realnfds) {
		pfds = calloc(realnfds, sizeof(*pfds));
		if (!pfds)
			return -1;
	} else {
		pfds = NULL;
	}

	for (i = 0, j = 0; i < nfds; i++) {
		incr = 0;
		if (readfds && FD_ISSET(i, readfds)) {
			pfds[j].fd = i;
			pfds[j].events |= POLLIN;
			incr=1;
		}
		if (writefds && FD_ISSET(i, writefds)) {
			pfds[j].fd = i;
			pfds[j].events |= POLLOUT;
			incr=1;
		}
		if (exceptfds && FD_ISSET(i, exceptfds)) {
			pfds[j].fd = i;
			pfds[j].events |= POLLHUP|POLLERR;
			incr=1;
		}
		if (incr)
			j++;
	}
	assert(j == (int)realnfds);

	if (timeout) {
		TIMEVAL_TO_TIMESPEC(timeout, &ts);
		tsp = &ts;
	}
	rv = REALPOLLTS(pfds, realnfds, tsp, NULL);
	/*
	 * "If select() returns with an error the descriptor sets
	 * will be unmodified"
	 */
	if (rv < 0)
		goto out;

	/*
	 * zero out results (can't use FD_ZERO for the
	 * obvious select-me-not reason).  whee.
	 *
	 * We do this here since some software ignores the return
	 * value of select, and hence if the timeout expires, it may
	 * assume all input descriptors have activity.
	 */
	for (i = 0; i < nfds; i++) {
		if (readfds)
			FD_CLR(i, readfds);
		if (writefds)
			FD_CLR(i, writefds);
		if (exceptfds)
			FD_CLR(i, exceptfds);
	}
	if (rv == 0)
		goto out;

	/*
	 * We have >0 fds with activity.  Harvest the results.
	 */
	for (i = 0; i < (int)realnfds; i++) {
		if (readfds) {
			if (pfds[i].revents & POLLIN) {
				FD_SET(pfds[i].fd, readfds);
			}
		}
		if (writefds) {
			if (pfds[i].revents & POLLOUT) {
				FD_SET(pfds[i].fd, writefds);
			}
		}
		if (exceptfds) {
			if (pfds[i].revents & (POLLHUP|POLLERR)) {
				FD_SET(pfds[i].fd, exceptfds);
			}
		}
	}

 out:
	free(pfds);
	return rv;
}

static void
checkpoll(struct pollfd *fds, nfds_t nfds, int *hostcall, int *rumpcall)
{
	nfds_t i;

	for (i = 0; i < nfds; i++) {
		if (fds[i].fd == -1)
			continue;

		if (fd_isrump(fds[i].fd))
			(*rumpcall)++;
		else
			(*hostcall)++;
	}
}

static void
adjustpoll(struct pollfd *fds, nfds_t nfds, int (*fdadj)(int))
{
	nfds_t i;

	for (i = 0; i < nfds; i++) {
		fds[i].fd = fdadj(fds[i].fd);
	}
}

/*
 * poll is easy as long as the call comes in the fds only in one
 * kernel.  otherwise its quite tricky...
 */
struct pollarg {
	struct pollfd *pfds;
	nfds_t nfds;
	const struct timespec *ts;
	const sigset_t *sigmask;
	int pipefd;
	int errnum;
};

static void *
hostpoll(void *arg)
{
	int (*op_pollts)(struct pollfd *, nfds_t, const struct timespec *,
			 const sigset_t *);
	struct pollarg *parg = arg;
	intptr_t rv;

	op_pollts = GETSYSCALL(host, POLLTS);
	rv = op_pollts(parg->pfds, parg->nfds, parg->ts, parg->sigmask);
	if (rv == -1)
		parg->errnum = errno;
	rump_sys_write(parg->pipefd, &rv, sizeof(rv));

	return (void *)(intptr_t)rv;
}

int
REALPOLLTS(struct pollfd *fds, nfds_t nfds, const struct timespec *ts,
	const sigset_t *sigmask)
{
	int (*op_pollts)(struct pollfd *, nfds_t, const struct timespec *,
			 const sigset_t *);
	int (*host_close)(int);
	int hostcall = 0, rumpcall = 0;
	pthread_t pt;
	nfds_t i;
	int rv;

	DPRINTF(("poll\n"));
	checkpoll(fds, nfds, &hostcall, &rumpcall);

	if (hostcall && rumpcall) {
		struct pollfd *pfd_host = NULL, *pfd_rump = NULL;
		int rpipe[2] = {-1,-1}, hpipe[2] = {-1,-1};
		struct pollarg parg;
		uintptr_t lrv;
		int sverrno = 0, trv;

		/*
		 * ok, this is where it gets tricky.  We must support
		 * this since it's a very common operation in certain
		 * types of software (telnet, netcat, etc).  We allocate
		 * two vectors and run two poll commands in separate
		 * threads.  Whichever returns first "wins" and the
		 * other kernel's fds won't show activity.
		 */
		rv = -1;

		/* allocate full vector for O(n) joining after call */
		pfd_host = malloc(sizeof(*pfd_host)*(nfds+1));
		if (!pfd_host)
			goto out;
		pfd_rump = malloc(sizeof(*pfd_rump)*(nfds+1));
		if (!pfd_rump) {
			goto out;
		}

		/*
		 * then, open two pipes, one for notifications
		 * to each kernel.
		 */
		if ((rv = rump_sys_pipe(rpipe)) == -1) {
			sverrno = errno;
		}
		if (rv == 0 && (rv = pipe(hpipe)) == -1) {
			sverrno = errno;
		}

		/* split vectors (or signal errors) */
		for (i = 0; i < nfds; i++) {
			int fd;

			fds[i].revents = 0;
			if (fds[i].fd == -1) {
				pfd_host[i].fd = -1;
				pfd_rump[i].fd = -1;
			} else if (fd_isrump(fds[i].fd)) {
				pfd_host[i].fd = -1;
				fd = fd_host2rump(fds[i].fd);
				if (fd == rpipe[0] || fd == rpipe[1]) {
					fds[i].revents = POLLNVAL;
					if (rv != -1)
						rv++;
				}
				pfd_rump[i].fd = fd;
				pfd_rump[i].events = fds[i].events;
			} else {
				pfd_rump[i].fd = -1;
				fd = fds[i].fd;
				if (fd == hpipe[0] || fd == hpipe[1]) {
					fds[i].revents = POLLNVAL;
					if (rv != -1)
						rv++;
				}
				pfd_host[i].fd = fd;
				pfd_host[i].events = fds[i].events;
			}
			pfd_rump[i].revents = pfd_host[i].revents = 0;
		}
		if (rv) {
			goto out;
		}

		pfd_host[nfds].fd = hpipe[0];
		pfd_host[nfds].events = POLLIN;
		pfd_rump[nfds].fd = rpipe[0];
		pfd_rump[nfds].events = POLLIN;

		/*
		 * then, create a thread to do host part and meanwhile
		 * do rump kernel part right here
		 */

		parg.pfds = pfd_host;
		parg.nfds = nfds+1;
		parg.ts = ts;
		parg.sigmask = sigmask;
		parg.pipefd = rpipe[1];
		pthread_create(&pt, NULL, hostpoll, &parg);

		op_pollts = GETSYSCALL(rump, POLLTS);
		lrv = op_pollts(pfd_rump, nfds+1, ts, NULL);
		sverrno = errno;
		write(hpipe[1], &rv, sizeof(rv));
		pthread_join(pt, (void *)&trv);

		/* check who "won" and merge results */
		if (lrv != 0 && pfd_host[nfds].revents & POLLIN) {
			rv = trv;

			for (i = 0; i < nfds; i++) {
				if (pfd_rump[i].fd != -1)
					fds[i].revents = pfd_rump[i].revents;
			}
			sverrno = parg.errnum;
		} else if (trv != 0 && pfd_rump[nfds].revents & POLLIN) {
			rv = trv;

			for (i = 0; i < nfds; i++) {
				if (pfd_host[i].fd != -1)
					fds[i].revents = pfd_host[i].revents;
			}
		} else {
			rv = 0;
		}

 out:
		host_close = GETSYSCALL(host, CLOSE);
		if (rpipe[0] != -1)
			rump_sys_close(rpipe[0]);
		if (rpipe[1] != -1)
			rump_sys_close(rpipe[1]);
		if (hpipe[0] != -1)
			host_close(hpipe[0]);
		if (hpipe[1] != -1)
			host_close(hpipe[1]);
		free(pfd_host);
		free(pfd_rump);
		errno = sverrno;
	} else {
		if (hostcall) {
			op_pollts = GETSYSCALL(host, POLLTS);
		} else {
			op_pollts = GETSYSCALL(rump, POLLTS);
			adjustpoll(fds, nfds, fd_host2rump);
		}

		rv = op_pollts(fds, nfds, ts, sigmask);
		if (rumpcall)
			adjustpoll(fds, nfds, fd_rump2host);
	}

	return rv;
}

int
poll(struct pollfd *fds, nfds_t nfds, int timeout)
{
	struct timespec ts;
	struct timespec *tsp = NULL;

	if (timeout != INFTIM) {
		ts.tv_sec = timeout / 1000;
		ts.tv_nsec = (timeout % 1000) * 1000*1000;

		tsp = &ts;
	}

	return REALPOLLTS(fds, nfds, tsp, NULL);
}

int
REALKEVENT(int kq, const struct kevent *changelist, size_t nchanges,
	struct kevent *eventlist, size_t nevents,
	const struct timespec *timeout)
{
	int (*op_kevent)(int, const struct kevent *, size_t,
		struct kevent *, size_t, const struct timespec *);
	const struct kevent *ev;
	size_t i;

	/*
	 * Check that we don't attempt to kevent rump kernel fd's.
	 * That needs similar treatment to select/poll, but is slightly
	 * trickier since we need to manage to different kq descriptors.
	 * (TODO, in case you're wondering).
	 */
	for (i = 0; i < nchanges; i++) {
		ev = &changelist[i];
		if (ev->filter == EVFILT_READ || ev->filter == EVFILT_WRITE ||
		    ev->filter == EVFILT_VNODE) {
			if (fd_isrump((int)ev->ident))
				return ENOTSUP;
		}
	}

	op_kevent = GETSYSCALL(host, KEVENT);
	return op_kevent(kq, changelist, nchanges, eventlist, nevents, timeout);
}

/*
 * Rest are std type calls.
 */

FDCALL(int, bind, DUALCALL_BIND,					\
	(int fd, const struct sockaddr *name, socklen_t namelen),	\
	(int, const struct sockaddr *, socklen_t),			\
	(fd, name, namelen))

FDCALL(int, connect, DUALCALL_CONNECT,					\
	(int fd, const struct sockaddr *name, socklen_t namelen),	\
	(int, const struct sockaddr *, socklen_t),			\
	(fd, name, namelen))

FDCALL(int, getpeername, DUALCALL_GETPEERNAME,				\
	(int fd, struct sockaddr *name, socklen_t *namelen),		\
	(int, struct sockaddr *, socklen_t *),				\
	(fd, name, namelen))

FDCALL(int, getsockname, DUALCALL_GETSOCKNAME, 				\
	(int fd, struct sockaddr *name, socklen_t *namelen),		\
	(int, struct sockaddr *, socklen_t *),				\
	(fd, name, namelen))

FDCALL(int, listen, DUALCALL_LISTEN,	 				\
	(int fd, int backlog),						\
	(int, int),							\
	(fd, backlog))

FDCALL(ssize_t, recvfrom, DUALCALL_RECVFROM, 				\
	(int fd, void *buf, size_t len, int flags,			\
	    struct sockaddr *from, socklen_t *fromlen),			\
	(int, void *, size_t, int, struct sockaddr *, socklen_t *),	\
	(fd, buf, len, flags, from, fromlen))

FDCALL(ssize_t, sendto, DUALCALL_SENDTO, 				\
	(int fd, const void *buf, size_t len, int flags,		\
	    const struct sockaddr *to, socklen_t tolen),		\
	(int, const void *, size_t, int,				\
	    const struct sockaddr *, socklen_t),			\
	(fd, buf, len, flags, to, tolen))

FDCALL(ssize_t, recvmsg, DUALCALL_RECVMSG, 				\
	(int fd, struct msghdr *msg, int flags),			\
	(int, struct msghdr *, int),					\
	(fd, msg, flags))

FDCALL(ssize_t, sendmsg, DUALCALL_SENDMSG, 				\
	(int fd, const struct msghdr *msg, int flags),			\
	(int, const struct msghdr *, int),				\
	(fd, msg, flags))

FDCALL(int, getsockopt, DUALCALL_GETSOCKOPT, 				\
	(int fd, int level, int optn, void *optval, socklen_t *optlen),	\
	(int, int, int, void *, socklen_t *),				\
	(fd, level, optn, optval, optlen))

FDCALL(int, setsockopt, DUALCALL_SETSOCKOPT, 				\
	(int fd, int level, int optn,					\
	    const void *optval, socklen_t optlen),			\
	(int, int, int, const void *, socklen_t),			\
	(fd, level, optn, optval, optlen))

FDCALL(int, shutdown, DUALCALL_SHUTDOWN, 				\
	(int fd, int how),						\
	(int, int),							\
	(fd, how))

#if _FORTIFY_SOURCE > 0
#define STUB(fun) __ssp_weak_name(fun)
ssize_t _sys_readlink(const char * __restrict, char * __restrict, size_t);
ssize_t
STUB(readlink)(const char * __restrict path, char * __restrict buf,
    size_t bufsiz)
{
	return _sys_readlink(path, buf, bufsiz);
}

char *_sys_getcwd(char *, size_t);
char *
STUB(getcwd)(char *buf, size_t size)
{
	return _sys_getcwd(buf, size);
}
#else
#define STUB(fun) fun
#endif

FDCALL(ssize_t, REALREAD, DUALCALL_READ,				\
	(int fd, void *buf, size_t buflen),				\
	(int, void *, size_t),						\
	(fd, buf, buflen))

FDCALL(ssize_t, readv, DUALCALL_READV, 					\
	(int fd, const struct iovec *iov, int iovcnt),			\
	(int, const struct iovec *, int),				\
	(fd, iov, iovcnt))

FDCALL(ssize_t, writev, DUALCALL_WRITEV, 				\
	(int fd, const struct iovec *iov, int iovcnt),			\
	(int, const struct iovec *, int),				\
	(fd, iov, iovcnt))

FDCALL(int, REALFSTAT, DUALCALL_FSTAT,					\
	(int fd, struct stat *sb),					\
	(int, struct stat *),						\
	(fd, sb))

FDCALL(int, fstatvfs1, DUALCALL_FSTATVFS1,				\
	(int fd, struct statvfs *buf, int flags),			\
	(int, struct statvfs *, int),					\
	(fd, buf, flags))

FDCALL(off_t, REALLSEEK, DUALCALL_LSEEK,				\
	(int fd, off_t offset, int whence),				\
	(int, off_t, int),						\
	(fd, offset, whence))

FDCALL(int, REALGETDENTS, DUALCALL_GETDENTS,				\
	(int fd, char *buf, size_t nbytes),				\
	(int, char *, size_t),						\
	(fd, buf, nbytes))

FDCALL(int, fchown, DUALCALL_FCHOWN,					\
	(int fd, uid_t owner, gid_t group),				\
	(int, uid_t, gid_t),						\
	(fd, owner, group))

FDCALL(int, fchmod, DUALCALL_FCHMOD,					\
	(int fd, mode_t mode),						\
	(int, mode_t),							\
	(fd, mode))

FDCALL(int, ftruncate, DUALCALL_FTRUNCATE,				\
	(int fd, off_t length),						\
	(int, off_t),							\
	(fd, length))

FDCALL(int, fsync, DUALCALL_FSYNC,					\
	(int fd),							\
	(int),								\
	(fd))

FDCALL(int, fsync_range, DUALCALL_FSYNC_RANGE,				\
	(int fd, int how, off_t start, off_t length),			\
	(int, int, off_t, off_t),					\
	(fd, how, start, length))

FDCALL(int, futimes, DUALCALL_FUTIMES,					\
	(int fd, const struct timeval *tv),				\
	(int, const struct timeval *),					\
	(fd, tv))

/*
 * path-based selectors
 */

PATHCALL(int, REALSTAT, DUALCALL_STAT,					\
	(const char *path, struct stat *sb),				\
	(const char *, struct stat *),					\
	(path, sb))

PATHCALL(int, REALLSTAT, DUALCALL_LSTAT,				\
	(const char *path, struct stat *sb),				\
	(const char *, struct stat *),					\
	(path, sb))

PATHCALL(int, chown, DUALCALL_CHOWN,					\
	(const char *path, uid_t owner, gid_t group),			\
	(const char *, uid_t, gid_t),					\
	(path, owner, group))

PATHCALL(int, lchown, DUALCALL_LCHOWN,					\
	(const char *path, uid_t owner, gid_t group),			\
	(const char *, uid_t, gid_t),					\
	(path, owner, group))

PATHCALL(int, chmod, DUALCALL_CHMOD,					\
	(const char *path, mode_t mode),				\
	(const char *, mode_t),						\
	(path, mode))

PATHCALL(int, lchmod, DUALCALL_LCHMOD,					\
	(const char *path, mode_t mode),				\
	(const char *, mode_t),						\
	(path, mode))

PATHCALL(int, statvfs1, DUALCALL_STATVFS1,				\
	(const char *path, struct statvfs *buf, int flags),		\
	(const char *, struct statvfs *, int),				\
	(path, buf, flags))

PATHCALL(int, unlink, DUALCALL_UNLINK,					\
	(const char *path),						\
	(const char *),							\
	(path))

PATHCALL(int, symlink, DUALCALL_SYMLINK,				\
	(const char *target, const char *path),				\
	(const char *, const char *),					\
	(target, path))

PATHCALL(ssize_t, readlink, DUALCALL_READLINK,				\
	(const char *path, char *buf, size_t bufsiz),			\
	(const char *, char *, size_t),					\
	(path, buf, bufsiz))

PATHCALL(int, mkdir, DUALCALL_MKDIR,					\
	(const char *path, mode_t mode),				\
	(const char *, mode_t),						\
	(path, mode))

PATHCALL(int, rmdir, DUALCALL_RMDIR,					\
	(const char *path),						\
	(const char *),							\
	(path))

PATHCALL(int, utimes, DUALCALL_UTIMES,					\
	(const char *path, const struct timeval *tv),			\
	(const char *, const struct timeval *),				\
	(path, tv))

PATHCALL(int, lutimes, DUALCALL_LUTIMES,				\
	(const char *path, const struct timeval *tv),			\
	(const char *, const struct timeval *),				\
	(path, tv))

PATHCALL(int, truncate, DUALCALL_TRUNCATE,				\
	(const char *path, off_t length),				\
	(const char *, off_t),						\
	(path, length))

/*
 * Note: with mount the decisive parameter is the mount
 * destination directory.  This is because we don't really know
 * about the "source" directory in a generic call (and besides,
 * it might not even exist, cf. nfs).
 */
PATHCALL(int, REALMOUNT, DUALCALL_MOUNT,				\
	(const char *type, const char *path, int flags,			\
	    void *data, size_t dlen),					\
	(const char *, const char *, int, void *, size_t),		\
	(type, path, flags, data, dlen))

PATHCALL(int, unmount, DUALCALL_UNMOUNT,				\
	(const char *path, int flags),					\
	(const char *, int),						\
	(path, flags))
