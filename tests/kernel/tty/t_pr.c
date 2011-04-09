/*	$NetBSD: t_pr.c,v 1.5 2011/04/09 11:55:59 martin Exp $	*/

#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/tty.h>

#include <atf-c.h>
#include <fcntl.h>

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <rump/rump.h>
#include <rump/rump_syscalls.h>

static bool init_done = false;

static void
init(void)
{
	if (init_done)
		return;
	rump_init();
	init_done = true;
}

static int
sendsome(int from, int to)
{
	size_t i;
	ssize_t cnt;
	static const char msg[] = "hello world\n";
	char buf[sizeof(msg)+10];

	memset(buf, 0, sizeof(buf));
	rump_sys_write(from, msg, strlen(msg));
	cnt = rump_sys_read(to, buf, sizeof(buf));
	if (cnt < (ssize_t)strlen(msg)) {
		printf("short message read: %zd chars: \"%s\"\n", cnt, buf);
		return 1;
	}
	for (i = 0; i < sizeof(buf); i++) {
		if (buf[i] == '\r' || buf[i] == '\n') {
			buf[i] = '\n';
			buf[i+1] = '\0';
			break;
		}
	}

	return strcmp(buf, msg) != 0;
}

static int
exercise_ptytty(int master, int slave)
{
	int error, flags;

	/*
	 * send a few bytes from master to slave and read them back
	 */
	error = sendsome(master, slave);
	if (error)
		return error;

	flags = FREAD|FWRITE;
	rump_sys_ioctl(master, TIOCFLUSH, &flags);

	/*
	 * and the same in the other direction
	 */
	error = sendsome(slave, master);
	if (error)
		return error;

	flags = FREAD|FWRITE;
	rump_sys_ioctl(master, TIOCFLUSH, &flags);
	return 0;
}

ATF_TC(client_first);
ATF_TC_HEAD(client_first, tc)
{

	atf_tc_set_md_var(tc, "descr",
	    "test basic tty/pty operation when opening client side first");
}

ATF_TC_BODY(client_first, tc)
{
	int master, slave, error, v;

	init();
	slave =  rump_sys_open("/dev/ttyp1", O_RDWR|O_NONBLOCK);
	ATF_CHECK(slave != -1);

	master = rump_sys_open("/dev/ptyp1", O_RDWR);
	ATF_CHECK(master != -1);

	v = 0;
	rump_sys_ioctl(slave, FIOASYNC, &v);
	error = exercise_ptytty(master, slave);
	ATF_CHECK(error == 0);

	rump_sys_close(master);
	rump_sys_close(slave);
}

ATF_TC(master_first);
ATF_TC_HEAD(master_first, tc)
{

	atf_tc_set_md_var(tc, "descr",
	    "test basic tty/pty operation when opening master side first");
}

ATF_TC_BODY(master_first, tc)
{
	int master, slave, error;

	init();
	master = rump_sys_open("/dev/ptyp1", O_RDWR);
	ATF_CHECK(master != -1);

	slave =  rump_sys_open("/dev/ttyp1", O_RDWR);
	ATF_CHECK(slave != -1);

	error = exercise_ptytty(master, slave);
	ATF_CHECK(error == 0);

	rump_sys_close(master);
	rump_sys_close(slave);
}

ATF_TC(ptyioctl);
ATF_TC_HEAD(ptyioctl, tc)
{

	atf_tc_set_md_var(tc, "descr",
	    "ioctl on pty with client side not open");
}

ATF_TC_BODY(ptyioctl, tc)
{
	struct termios tio;
	int fd;

	init();
	fd = rump_sys_open("/dev/ptyp1", O_RDWR);
	ATF_CHECK(fd != -1);

	/*
	 * This used to die with null deref under ptcwakeup()
	 * atf_tc_expect_signal(-1, "PR kern/40688");
	 */
	rump_sys_ioctl(fd, TIOCGETA, &tio);

	rump_sys_close(fd);
}

ATF_TP_ADD_TCS(tp)
{

	ATF_TP_ADD_TC(tp, ptyioctl);
	ATF_TP_ADD_TC(tp, client_first);
	ATF_TP_ADD_TC(tp, master_first);

	return atf_no_error();
}
