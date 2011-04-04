/* $NetBSD: t_mmap.c,v 1.2 2011/04/04 10:30:29 jruoho Exp $ */

/*-
 * Copyright (c) 2011 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jukka Ruohonen.
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
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
#include <sys/cdefs.h>
__RCSID("$NetBSD: t_mmap.c,v 1.2 2011/04/04 10:30:29 jruoho Exp $");

#include <sys/param.h>
#include <sys/mman.h>
#include <sys/sysctl.h>
#include <sys/wait.h>

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <atf-c.h>

static long	page = 0;
static char	path[] = "/tmp/mmap";
static void	map_check(void *, int);
static void	map_sighandler(int);

static void
map_check(void *map, int flag)
{

	if (flag != 0) {
		ATF_REQUIRE(map == MAP_FAILED);
		return;
	}

	ATF_REQUIRE(map != MAP_FAILED);
	ATF_REQUIRE(munmap(map, page) == 0);
}

static void
map_sighandler(int signo)
{
	_exit(signo);
}

ATF_TC(mmap_err);
ATF_TC_HEAD(mmap_err, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test error conditions of mmap(2)");
}

ATF_TC_BODY(mmap_err, tc)
{
	size_t addr = SIZE_MAX;
	void *map;

	errno = 0;
	map = mmap(NULL, 3, PROT_READ, MAP_FILE, -1, 0);

	ATF_REQUIRE(map == MAP_FAILED);
	ATF_REQUIRE(errno == EBADF);

	errno = 0;
	map = mmap(&addr, page, PROT_READ, MAP_FIXED, -1, 0);

	ATF_REQUIRE(map == MAP_FAILED);
	ATF_REQUIRE(errno == EINVAL);

	errno = 0;
	map = mmap(NULL, page, PROT_READ, MAP_ANON, INT_MAX, 0);

	ATF_REQUIRE(map == MAP_FAILED);
	ATF_REQUIRE(errno == EINVAL);
}

ATF_TC_WITH_CLEANUP(mmap_prot_1);
ATF_TC_HEAD(mmap_prot_1, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test mmap(2) protections, #1");
}

ATF_TC_BODY(mmap_prot_1, tc)
{
	void *map;
	int fd;

	/*
	 * Open a file write-only and try to
	 * map it read-only. This should fail.
	 */
	fd = open(path, O_WRONLY | O_CREAT, 0700);

	if (fd < 0)
		return;

	ATF_REQUIRE(write(fd, "XXX", 3) == 3);

	map = mmap(NULL, 3, PROT_READ, MAP_FILE, fd, 0);
	map_check(map, 1);

	map = mmap(NULL, 3, PROT_WRITE, MAP_FILE, fd, 0);
	map_check(map, 0);

	ATF_REQUIRE(close(fd) == 0);
}

ATF_TC_CLEANUP(mmap_prot_1, tc)
{
	(void)unlink(path);
}

ATF_TC(mmap_prot_2);
ATF_TC_HEAD(mmap_prot_2, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test mmap(2) protections, #2");
}

ATF_TC_BODY(mmap_prot_2, tc)
{
	char buf[2];
	void *map;
	pid_t pid;
	int sta;

	/*
	 * Make a PROT_NONE mapping and try to access it.
	 * If we catch a SIGSEGV, all works as expected.
	 */
	map = mmap(NULL, page, PROT_NONE, MAP_ANON, -1, 0);
	ATF_REQUIRE(map != MAP_FAILED);

	pid = fork();
	ATF_REQUIRE(pid >= 0);

	if (pid == 0) {
		ATF_REQUIRE(signal(SIGSEGV, map_sighandler) != SIG_ERR);
		ATF_REQUIRE(strlcpy(buf, map, sizeof(buf)) != 0);
	}

	(void)wait(&sta);

	ATF_REQUIRE(WIFEXITED(sta) != 0);
	ATF_REQUIRE(WEXITSTATUS(sta) == SIGSEGV);
	ATF_REQUIRE(munmap(map, page) == 0);
}

ATF_TC_WITH_CLEANUP(mmap_prot_3);
ATF_TC_HEAD(mmap_prot_3, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test mmap(2) protections, #3");
}

ATF_TC_BODY(mmap_prot_3, tc)
{
	char buf[2];
	int fd, sta;
	void *map;
	pid_t pid;

	/*
	 * Open a file, change the permissions
	 * to read-only, and try to map it as
	 * PROT_NONE. This should succeed, but
	 * the access should generate SIGSEGV.
	 */
	fd = open(path, O_RDWR | O_CREAT, 0700);

	if (fd < 0)
		return;

	ATF_REQUIRE(write(fd, "XXX", 3) == 3);
	ATF_REQUIRE(close(fd) == 0);
	ATF_REQUIRE(chmod(path, 0444) == 0);

	fd = open(path, O_RDONLY);
	ATF_REQUIRE(fd != -1);

	map = mmap(NULL, 3, PROT_NONE, MAP_FILE | MAP_SHARED, fd, 0);
	ATF_REQUIRE(map != MAP_FAILED);

	pid = fork();

	ATF_REQUIRE(pid >= 0);

	if (pid == 0) {
		ATF_REQUIRE(signal(SIGSEGV, map_sighandler) != SIG_ERR);
		ATF_REQUIRE(strlcpy(buf, map, sizeof(buf)) != 0);
	}

	(void)wait(&sta);

	ATF_REQUIRE(WIFEXITED(sta) != 0);
	ATF_REQUIRE(WEXITSTATUS(sta) == SIGSEGV);
	ATF_REQUIRE(munmap(map, 3) == 0);
}

ATF_TC_CLEANUP(mmap_prot_3, tc)
{
	(void)unlink(path);
}

ATF_TC_WITH_CLEANUP(mmap_truncate);
ATF_TC_HEAD(mmap_truncate, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test mmap(2) and ftruncate(2)");
}

ATF_TC_BODY(mmap_truncate, tc)
{
	char *map;
	long i;
	int fd;

	fd = open(path, O_RDWR | O_CREAT, 0700);

	if (fd < 0)
		return;

	/*
	 * See that ftruncate(2) works
	 * while the file is mapped.
	 */
	ATF_REQUIRE(ftruncate(fd, page) == 0);

	map = mmap(NULL, page, PROT_READ | PROT_WRITE, MAP_FILE, fd, 0);
	ATF_REQUIRE(map != MAP_FAILED);

	for (i = 0; i < page; i++)
		map[i] = 'x';

	ATF_REQUIRE(ftruncate(fd, 0) == 0);
	ATF_REQUIRE(ftruncate(fd, page / 8) == 0);
	ATF_REQUIRE(ftruncate(fd, page / 4) == 0);
	ATF_REQUIRE(ftruncate(fd, page / 2) == 0);
	ATF_REQUIRE(ftruncate(fd, page / 12) == 0);
	ATF_REQUIRE(ftruncate(fd, page / 64) == 0);

	ATF_REQUIRE(close(fd) == 0);
}

ATF_TC_CLEANUP(mmap_truncate, tc)
{
	(void)unlink(path);
}

ATF_TC(mmap_va0);
ATF_TC_HEAD(mmap_va0, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test mmap(2) and vm.user_va0_disable");
}

ATF_TC_BODY(mmap_va0, tc)
{
	int flags = MAP_ANON | MAP_FIXED | MAP_PRIVATE;
	size_t len = sizeof(int);
	void *map;
	int val;

	/*
	 * Make an anonymous fixed mapping at zero address. If the address
	 * is restricted as noted in security(7), the syscall should fail.
	 */
	if (sysctlbyname("vm.user_va0_disable", &val, &len, NULL, 0) != 0)
		atf_tc_fail("failed to read vm.user_va0_disable");

	map = mmap(NULL, page, PROT_EXEC, flags, -1, 0);
	map_check(map, val);

	map = mmap(NULL, page, PROT_READ, flags, -1, 0);
	map_check(map, val);

	map = mmap(NULL, page, PROT_WRITE, flags, -1, 0);
	map_check(map, val);

	map = mmap(NULL, page, PROT_READ|PROT_WRITE, flags, -1, 0);
	map_check(map, val);

	map = mmap(NULL, page, PROT_EXEC|PROT_READ|PROT_WRITE, flags, -1, 0);
	map_check(map, val);
}

ATF_TP_ADD_TCS(tp)
{
	page = sysconf(_SC_PAGESIZE);
	ATF_REQUIRE(page >= 0);

	ATF_TP_ADD_TC(tp, mmap_err);
	ATF_TP_ADD_TC(tp, mmap_prot_1);
	ATF_TP_ADD_TC(tp, mmap_prot_2);
	ATF_TP_ADD_TC(tp, mmap_prot_3);
	ATF_TP_ADD_TC(tp, mmap_truncate);
	ATF_TP_ADD_TC(tp, mmap_va0);

	return atf_no_error();
}
