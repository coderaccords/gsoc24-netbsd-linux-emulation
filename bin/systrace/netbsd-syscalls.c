/*	$NetBSD: netbsd-syscalls.c,v 1.3 2002/06/18 01:37:12 thorpej Exp $	*/

/*
 * Copyright 2002 Niels Provos <provos@citi.umich.edu>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Niels Provos.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__RCSID("$NetBSD: netbsd-syscalls.c,v 1.3 2002/06/18 01:37:12 thorpej Exp $");

#include <sys/types.h>
#include <sys/param.h>

#include <sys/syscall.h>

#include "../../sys/compat/aout/aout_syscall.h"
#include "../../sys/compat/aoutm68k/aoutm68k_syscall.h"
#include "../../sys/compat/freebsd/freebsd_syscall.h"
#include "../../sys/compat/hpux/hpux_syscall.h"
#include "../../sys/compat/ibcs2/ibcs2_syscall.h"
#include "../../sys/compat/irix/irix_syscall.h"
#include "../../sys/compat/linux/linux_syscall.h"
#include "../../sys/compat/mach/mach_syscall.h"
#include "../../sys/compat/netbsd32/netbsd32_syscall.h"
#include "../../sys/compat/osf1/osf1_syscall.h"
#include "../../sys/compat/pecoff/pecoff_syscall.h"
#include "../../sys/compat/sunos/sunos_syscall.h"
#include "../../sys/compat/sunos32/sunos32_syscall.h"
#include "../../sys/compat/svr4/svr4_syscall.h"
#include "../../sys/compat/svr4_32/svr4_32_syscall.h"
#include "../../sys/compat/ultrix/ultrix_syscall.h"

#define KTRACE
#define NFSCLIENT
#define NFSSERVER
#define SYSVSEM
#define SYSVMSG
#define SYSVSHM
#define LFS
#define NTP
#include "../../sys/kern/syscalls.c"

#include "../../sys/compat/aout/aout_syscalls.c"
#include "../../sys/compat/aoutm68k/aoutm68k_syscalls.c"
#include "../../sys/compat/freebsd/freebsd_syscalls.c"
#include "../../sys/compat/hpux/hpux_syscalls.c"
#include "../../sys/compat/ibcs2/ibcs2_syscalls.c"
#include "../../sys/compat/irix/irix_syscalls.c"
#include "../../sys/compat/linux/linux_syscalls.c"
#include "../../sys/compat/mach/mach_syscalls.c"
#include "../../sys/compat/netbsd32/netbsd32_syscalls.c"
#include "../../sys/compat/osf1/osf1_syscalls.c"
#include "../../sys/compat/pecoff/pecoff_syscalls.c"
#include "../../sys/compat/sunos/sunos_syscalls.c"
#include "../../sys/compat/sunos32/sunos32_syscalls.c"
#include "../../sys/compat/svr4/svr4_syscalls.c"
#include "../../sys/compat/svr4_32/svr4_32_syscalls.c"
#include "../../sys/compat/ultrix/ultrix_syscalls.c"
#undef KTRACE
#undef NFSCLIENT
#undef NFSSERVER
#undef SYSVSEM
#undef SYSVMSG
#undef SYSVSHM
#undef LFS
#undef NTP

#include <sys/ioctl.h>
#include <sys/tree.h>
#include <sys/systrace.h>

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <err.h>

#include "intercept.h"

struct emulation {
	char *name;			/* Emulation name */
	const char * const *sysnames;	/* Array of system call names */
	int  nsysnames;			/* Number of */
};

static struct emulation emulations[] = {
	{ "netbsd",	syscallnames,		SYS_MAXSYSCALL },

	{ "aout",	aout_syscallnames,	AOUT_SYS_MAXSYSCALL },
	{ "aoutm68k",	aoutm68k_syscallnames,	AOUTM68K_SYS_MAXSYSCALL },
	{ "freebsd",	freebsd_syscallnames,	FREEBSD_SYS_MAXSYSCALL },
	{ "hpux",	hpux_syscallnames,	HPUX_SYS_MAXSYSCALL },
	{ "ibcs2",	ibcs2_syscallnames,	IBCS2_SYS_MAXSYSCALL },
	{ "irix",	irix_syscallnames,	IRIX_SYS_MAXSYSCALL },
	{ "linux",	linux_syscallnames,	LINUX_SYS_MAXSYSCALL },
	{ "mach",	mach_syscallnames,	MACH_SYS_MAXSYSCALL },
	{ "netbsd32",	netbsd32_syscallnames,	netbsd32_SYS_MAXSYSCALL },
	{ "osf1",	osf1_syscallnames,	OSF1_SYS_MAXSYSCALL },
	{ "pecoff",	pecoff_syscallnames,	PECOFF_SYS_MAXSYSCALL },
	{ "sunos",	sunos_syscallnames,	SUNOS_SYS_MAXSYSCALL },
	{ "sunos32",	sunos32_syscallnames,	SUNOS32_SYS_MAXSYSCALL },
	{ "svr4",	svr4_syscallnames,	SVR4_SYS_MAXSYSCALL },
	{ "svr4_32",	svr4_32_syscallnames,	SVR4_32_SYS_MAXSYSCALL },
	{ "ultrix",	ultrix_syscallnames,	ULTRIX_SYS_MAXSYSCALL },
	{ NULL,		NULL,			NULL }
};

struct nbsd_data {
	struct emulation *current;
	struct emulation *commit;
};


static int nbsd_init(void);
static int nbsd_attach(int, pid_t);
static int nbsd_report(int, pid_t);
static int nbsd_detach(int, pid_t);
static int nbsd_open(void);
static struct intercept_pid *nbsd_getpid(pid_t);
static void nbsd_freepid(struct intercept_pid *);
static void nbsd_clonepid(struct intercept_pid *, struct intercept_pid *);
static struct emulation *nbsd_find_emulation(const char *);
static int nbsd_set_emulation(pid_t, char *);
static struct emulation *nbsd_switch_emulation(struct nbsd_data *);
static const char *nbsd_syscall_name(pid_t, int);
static int nbsd_syscall_number(const char *, const char *);
static short nbsd_translate_policy(short);
static short nbsd_translate_flags(short);
static int nbsd_translate_errno(int);
static int nbsd_answer(int, pid_t, short, int, short);
static int nbsd_newpolicy(int);
static int nbsd_assignpolicy(int, pid_t, int);
static int nbsd_modifypolicy(int, int, int, short);
static int nbsd_io(int, pid_t, int, void *, u_char *, size_t);
static char *nbsd_getcwd(int, pid_t, char *, size_t);
static int nbsd_argument(int, void *, int, void **);
static int nbsd_read(int);

static int
nbsd_init(void)
{
	return (0);
}

static int
nbsd_attach(int fd, pid_t pid)
{
	if (ioctl(fd, STRIOCATTACH, &pid) == -1)
		return (-1);

	return (0);
}

static int
nbsd_report(int fd, pid_t pid)
{
	if (ioctl(fd, STRIOCREPORT, &pid) == -1)
		return (-1);

	return (0);
}

static int
nbsd_detach(int fd, pid_t pid)
{
	if (ioctl(fd, STRIOCDETACH, &pid) == -1)
		return (-1);

	return (0);
}

static int
nbsd_open(void)
{
	char *path = "/dev/systrace";
	int fd;

	fd = open(path, O_RDONLY, 0);
	if (fd == -1) {
		warn("open: %s", path);
		return (-1);
	}

	if (fcntl(fd, F_SETFD, 1) == -1)
		warn("fcntl(F_SETFD)");

	return (fd);
}

static struct intercept_pid *
nbsd_getpid(pid_t pid)
{
	struct intercept_pid *icpid;
	struct nbsd_data *data;

	icpid = intercept_getpid(pid);
	if (icpid == NULL)
		return (NULL);
	if (icpid->data != NULL)
		return (icpid);

	if ((icpid->data = malloc(sizeof(struct nbsd_data))) == NULL)
		err(1, "%s:%d: malloc", __func__, __LINE__);
	
	data = icpid->data;
	data->current = &emulations[0];
	data->commit = NULL;

	return (icpid);
}

static void
nbsd_freepid(struct intercept_pid *ipid)
{
	if (ipid->data != NULL)
		free(ipid->data);
}

static void
nbsd_clonepid(struct intercept_pid *opid, struct intercept_pid *npid)
{
	if (opid->data == NULL) {
		npid->data = NULL;
		return;
	}

	if ((npid->data = malloc(sizeof(struct nbsd_data))) == NULL)
		err(1, "%s:%d: malloc", __func__, __LINE__);
	memcpy(npid->data, opid->data, sizeof(struct nbsd_data));
}

static struct emulation *
nbsd_find_emulation(const char *name)
{
	struct emulation *tmp;

	tmp = emulations;
	while (tmp->name) {
		if (!strcmp(tmp->name, name))
			break;
		tmp++;
	}

	if (!tmp->name)
		return (NULL);

	return (tmp);
}

static int
nbsd_set_emulation(pid_t pidnr, char *name)
{
	struct emulation *tmp;
	struct intercept_pid *pid;
	struct nbsd_data *data;

	if ((tmp = nbsd_find_emulation(name)) == NULL)
		return (-1);

	pid = intercept_getpid(pidnr);
	if (pid == NULL)
		return (-1);
	data = pid->data;

	data->commit = tmp;

	return (0);
}

static struct emulation *
nbsd_switch_emulation(struct nbsd_data *data)
{
	data->current = data->commit;
	data->commit = NULL;

	return data->current;
}

static const char *
nbsd_syscall_name(pid_t pidnr, int number)
{
	struct intercept_pid *pid;
	struct emulation *current;

	pid = nbsd_getpid(pidnr);
	if (pid == NULL)
		return (NULL);
	current = ((struct nbsd_data *)pid->data)->current;

	if (number < 0 || number >= current->nsysnames)
		return (NULL);

	return (current->sysnames[number]);
}

static int
nbsd_syscall_number(const char *emulation, const char *name)
{
	struct emulation *current;
	int i;

	current = nbsd_find_emulation(emulation);
	if (current == NULL)
		return (-1);

	for (i = 0; i < current->nsysnames; i++)
		if (!strcmp(name, current->sysnames[i]))
			return (i);

	return (-1);
}

static short
nbsd_translate_policy(short policy)
{
	switch (policy) {
	case ICPOLICY_ASK:
		return (SYSTR_POLICY_ASK);
	case ICPOLICY_PERMIT:
		return (SYSTR_POLICY_PERMIT);
	case ICPOLICY_NEVER:
	default:
		return (SYSTR_POLICY_NEVER);
	}
}

static short
nbsd_translate_flags(short flags)
{
	switch (flags) {
	case ICFLAGS_RESULT:
		return (SYSTR_FLAGS_RESULT);
	default:
		return (0);
	}
}

static int
nbsd_translate_errno(int errno)
{
	return (errno);
}

static int
nbsd_answer(int fd, pid_t pid, short policy, int errno, short flags)
{
	struct systrace_answer ans;

	ans.stra_pid = pid;
	ans.stra_policy = nbsd_translate_policy(policy);
	ans.stra_flags = nbsd_translate_flags(flags);
	ans.stra_error = nbsd_translate_errno(errno);

	if (ioctl(fd, STRIOCANSWER, &ans) == -1)
		return (-1);

	return (0);
}

static int
nbsd_newpolicy(int fd)
{
	struct systrace_policy pol;

	pol.strp_op = SYSTR_POLICY_NEW;
	pol.strp_num = -1;
	pol.strp_maxents = 512;

	if (ioctl(fd, STRIOCPOLICY, &pol) == -1)
		return (-1);

	return (pol.strp_num);
}

static int
nbsd_assignpolicy(int fd, pid_t pid, int num)
{
	struct systrace_policy pol;

	pol.strp_op = SYSTR_POLICY_ASSIGN;
	pol.strp_num = num;
	pol.strp_pid = pid;

	if (ioctl(fd, STRIOCPOLICY, &pol) == -1)
		return (-1);

	return (0);
}

static int
nbsd_modifypolicy(int fd, int num, int code, short policy)
{
	struct systrace_policy pol;

	pol.strp_op = SYSTR_POLICY_MODIFY;
	pol.strp_num = num;
	pol.strp_code = code;
	pol.strp_policy = nbsd_translate_policy(policy);

	if (ioctl(fd, STRIOCPOLICY, &pol) == -1)
		return (-1);

	return (0);
}

static int
nbsd_io(int fd, pid_t pid, int op, void *addr, u_char *buf, size_t size)
{
	struct systrace_io io;

	memset(&io, 0, sizeof(io));
	io.strio_pid = pid;
	io.strio_addr = buf;
	io.strio_len = size;
	io.strio_offs = addr;
	io.strio_op = (op == INTERCEPT_READ ? SYSTR_READ : SYSTR_WRITE);
	if (ioctl(fd, STRIOCIO, &io) == -1)
		return (-1);

	return (0);
}

static char *
nbsd_getcwd(int fd, pid_t pid, char *buf, size_t size)
{
	char *path;

	if (ioctl(fd, STRIOCGETCWD, &pid) == -1)
		return (NULL);

	path = getcwd(buf, size);

	if (ioctl(fd, STRIOCRESCWD, 0) == -1)
		warn("%s: ioctl", __func__); /* XXX */

	return (path);
}

static int
nbsd_argument(int off, void *pargs, int argsize, void **pres)
{
	register_t *args = (register_t *)pargs;

	if (off >= argsize / sizeof(register_t))
		return (-1);

	*pres = (void *)args[off];

	return (0);
}

static int
nbsd_read(int fd)
{
	struct str_message msg;
	struct intercept_pid *icpid;
	struct nbsd_data *data;
	struct emulation *current;

	char name[SYSTR_EMULEN+1];
	const char *sysname;
	int code;

	if (read(fd, &msg, sizeof(msg)) != sizeof(msg))
		return (-1);

	icpid = nbsd_getpid(msg.msg_pid);
	if (icpid == NULL)
		return (-1);
	data = icpid->data;

	current = data->current;

	switch(msg.msg_type) {
	case SYSTR_MSG_ASK:
		code = msg.msg_data.msg_ask.code;
		sysname = nbsd_syscall_name(msg.msg_pid, code);

		intercept_syscall(fd, msg.msg_pid, msg.msg_policy,
		    sysname, code, current->name,
		    (void *)msg.msg_data.msg_ask.args,
		    msg.msg_data.msg_ask.argsize);
		break;

	case SYSTR_MSG_RES:
		code = msg.msg_data.msg_ask.code;
		sysname = nbsd_syscall_name(msg.msg_pid, code);

		/* Switch emulation around at the right time */
		if (data->commit != NULL) {
			current = nbsd_switch_emulation(data);
		}

		intercept_syscall_result(fd, msg.msg_pid, msg.msg_policy,
		    sysname, code, current->name,
		    (void *)msg.msg_data.msg_ask.args,
		    msg.msg_data.msg_ask.argsize,
		    msg.msg_data.msg_ask.result,
		    msg.msg_data.msg_ask.rval);
		break;

	case SYSTR_MSG_EMUL:
		memcpy(name, msg.msg_data.msg_emul.emul, SYSTR_EMULEN);
		name[SYSTR_EMULEN] = '\0';
  
		if (nbsd_set_emulation(msg.msg_pid, name) == -1)
			errx(1, "%s:%d: set_emulation(%s)",
			    __func__, __LINE__, name);

		if (icpid->execve_code == -1) {
			icpid->execve_code = 0;

			/* A running attach fake a exec cb */
			current = nbsd_switch_emulation(data);

			intercept_syscall_result(fd,
			    msg.msg_pid, msg.msg_policy,
			    "execve", 0, current->name,
			    NULL, 0, 0, NULL);
			break;
		}

		if (nbsd_answer(fd, msg.msg_pid, 0, 0, 0) == -1)
			err(1, "%s:%d: answer", __func__, __LINE__);
		break;

	case SYSTR_MSG_CHILD:
		intercept_child_info(msg.msg_pid,
		    msg.msg_data.msg_child.new_pid);
		break;
	}
	return (0);
}

struct intercept_system intercept = {
	"netbsd",
	nbsd_init,
	nbsd_open,
	nbsd_attach,
	nbsd_detach,
	nbsd_report,
	nbsd_read,
	nbsd_syscall_number,
	nbsd_getcwd,
	nbsd_io,
	nbsd_argument,
	nbsd_answer,
	nbsd_newpolicy,
	nbsd_assignpolicy,
	nbsd_modifypolicy,
	nbsd_clonepid,
	nbsd_freepid,
};
