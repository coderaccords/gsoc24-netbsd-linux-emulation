/* $NetBSD: t_siginfo.c,v 1.7 2011/01/02 21:39:24 pgoyette Exp $ */

/*-
 * Copyright (c) 2010 The NetBSD Foundation, Inc.
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

#include <atf-c.h>

#include <sys/inttypes.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <sys/ucontext.h>
#include <sys/wait.h>

#include <assert.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <setjmp.h>

#ifndef __vax__
#include <ieeefp.h>
#endif

/* for sigchild */
pid_t child;
int code;
int status;

/* for sigfpe */
sig_atomic_t fltdiv_signalled = 0;
sig_atomic_t intdiv_signalled = 0;

static void
sig_debug(int signo, siginfo_t *info, ucontext_t *ctx)
{
	unsigned int i;

	printf("%d %p %p\n", signo, info, ctx);
	if (info != NULL) {
		printf("si_signo=%d\n", info->si_signo);
		printf("si_errno=%d\n", info->si_errno);
		printf("si_code=%d\n", info->si_code);
		printf("si_value.sival_int=%d\n", info->si_value.sival_int);
	}
	if (ctx != NULL) {
		printf("uc_flags 0x%x\n", ctx->uc_flags);
		printf("uc_link %p\n", ctx->uc_link);
		for (i = 0; i < __arraycount(ctx->uc_sigmask.__bits); i++)
			printf("uc_sigmask[%d] 0x%x\n", i,
			    ctx->uc_sigmask.__bits[i]);
		printf("uc_stack %p %lu 0x%x\n", ctx->uc_stack.ss_sp, 
		    (unsigned long)ctx->uc_stack.ss_size,
		    ctx->uc_stack.ss_flags);
		for (i = 0; i < __arraycount(ctx->uc_mcontext.__gregs); i++)
			printf("uc_mcontext.greg[%d] 0x%lx\n", i,
			    (long)ctx->uc_mcontext.__gregs[i]);
	}
}

static void
sigalrm_action(int signo, siginfo_t *info, void *ptr)
{

	sig_debug(signo, info, (ucontext_t *)ptr);

	ATF_REQUIRE_EQ(info->si_signo, SIGALRM);
	ATF_REQUIRE_EQ(info->si_code, SI_TIMER);
	ATF_REQUIRE_EQ(info->si_value.sival_int, ITIMER_REAL);

	atf_tc_pass();
	/* NOTREACHED */
}

ATF_TC(sigalarm);

ATF_TC_HEAD(sigalarm, tc)
{ 

	atf_tc_set_md_var(tc, "descr",
	    "Checks that signal trampoline correctly calls SIGALRM handler"); 
}

ATF_TC_BODY(sigalarm, tc)
{
	struct sigaction sa;
	sa.sa_flags = SA_SIGINFO;
	sa.sa_sigaction = sigalrm_action;
	sigemptyset(&sa.sa_mask);
	sigaction(SIGALRM, &sa, NULL);
	for (;;) {
		alarm(1);
		sleep(1);
	}
	atf_tc_fail("SIGALRM handler wasn't called"); 
}

static void
sigchild_action(int signo, siginfo_t *info, void *ptr)
{
	if (info != NULL) {
		printf("info=%p\n", info);
		printf("ptr=%p\n", ptr);
		printf("si_signo=%d\n", info->si_signo);
		printf("si_errno=%d\n", info->si_errno);
		printf("si_code=%d\n", info->si_code);
		printf("si_uid=%d\n", info->si_uid);
		printf("si_pid=%d\n", info->si_pid);
		printf("si_status=%d\n", info->si_status);
		printf("si_utime=%lu\n", (unsigned long int)info->si_utime);
		printf("si_stime=%lu\n", (unsigned long int)info->si_stime);
	}
	ATF_REQUIRE_EQ(info->si_code, code);
	ATF_REQUIRE_EQ(info->si_signo, SIGCHLD);
	ATF_REQUIRE_EQ(info->si_uid, getuid());
	ATF_REQUIRE_EQ(info->si_pid, child);
	if (WIFEXITED(info->si_status))
		ATF_REQUIRE_EQ(WEXITSTATUS(info->si_status), status);
	else if (WIFSTOPPED(info->si_status))
		ATF_REQUIRE_EQ(WSTOPSIG(info->si_status), status);
	else if (WIFSIGNALED(info->si_status))
		ATF_REQUIRE_EQ(WTERMSIG(info->si_status), status);
}

static void
setchildhandler(void (*action)(int, siginfo_t *, void *))
{
	struct sigaction sa;
	sa.sa_flags = SA_SIGINFO;
	sa.sa_sigaction = action;
	sigemptyset(&sa.sa_mask);
	sigaction(SIGCHLD, &sa, NULL);
}

static void
sigchild_setup(void)
{
	sigset_t set;
	struct rlimit rlim;

	(void)getrlimit(RLIMIT_CORE, &rlim);
	rlim.rlim_cur = rlim.rlim_max;
	(void)setrlimit(RLIMIT_CORE, &rlim);

	setchildhandler(sigchild_action);
	sigemptyset(&set);
	sigaddset(&set, SIGCHLD);
	sigprocmask(SIG_BLOCK, &set, NULL);
}

ATF_TC(sigchild_normal);
ATF_TC_HEAD(sigchild_normal, tc)
{

	atf_tc_set_md_var(tc, "descr",
	    "Checks that signal trampoline correctly calls SIGCHLD handler "
	    "when child exits normally");
}

ATF_TC_BODY(sigchild_normal, tc)
{
	sigset_t set;

	sigchild_setup(); 

	status = 25;
	code = CLD_EXITED;

	switch ((child = fork())) {
	case 0:
		sleep(1);
		exit(status);
	case -1:
		atf_tc_fail("fork failed");
	default:
		sigemptyset(&set);
		sigsuspend(&set);
	}
}

ATF_TC(sigchild_dump);
ATF_TC_HEAD(sigchild_dump, tc)
{

	atf_tc_set_md_var(tc, "descr",
	    "Checks that signal trampoline correctly calls SIGCHLD handler "
	    "when child segfaults");
}

ATF_TC_BODY(sigchild_dump, tc)
{ 
	sigset_t set;

	sigchild_setup(); 

	status = SIGSEGV;
	code = CLD_DUMPED;

	switch ((child = fork())) {
	case 0:
		sleep(1);
		*(long *)0 = 0;
		atf_tc_fail("Child did not segfault");
		/* NOTREACHED */
	case -1:
		atf_tc_fail("fork failed");
	default:
		sigemptyset(&set);
		sigsuspend(&set);
	}
}

ATF_TC(sigchild_kill);
ATF_TC_HEAD(sigchild_kill, tc)
{

	atf_tc_set_md_var(tc, "descr",
	    "Checks that signal trampoline correctly calls SIGCHLD handler "
	    "when child is killed");
}

ATF_TC_BODY(sigchild_kill, tc)
{ 
	sigset_t set;

	sigchild_setup(); 

	status = SIGPIPE;
	code = CLD_KILLED;

	switch ((child = fork())) {
	case 0:
		sigemptyset(&set);
		sigsuspend(&set);
		break;
	case -1:
		atf_tc_fail("fork failed");
	default:
		kill(child, SIGPIPE);
		sigemptyset(&set);
		sigsuspend(&set);
	}
}

static sigjmp_buf sigfpe_flt_env;
static void
sigfpe_flt_action(int signo, siginfo_t *info, void *ptr)
{

	sig_debug(signo, info, (ucontext_t *)ptr);

	if (fltdiv_signalled++ != 0)
		atf_tc_fail("FPE handler called more than once");

	ATF_REQUIRE_EQ(info->si_signo, SIGFPE);
	ATF_REQUIRE_EQ(info->si_code, FPE_FLTDIV);
	ATF_REQUIRE_EQ(info->si_errno, 0);

	siglongjmp(sigfpe_flt_env, 1);
}

ATF_TC(sigfpe_flt);
ATF_TC_HEAD(sigfpe_flt, tc)
{

	atf_tc_set_md_var(tc, "descr",
	    "Checks that signal trampoline correctly calls SIGFPE handler "
	    "for floating div-by-zero");
}

ATF_TC_BODY(sigfpe_flt, tc)
{ 
	struct sigaction sa;
	double d = strtod("0", NULL);

	if (sigsetjmp(sigfpe_flt_env, 0) == 0) {
		sa.sa_flags = SA_SIGINFO;
		sa.sa_sigaction = sigfpe_flt_action;
		sigemptyset(&sa.sa_mask);
		sigaction(SIGFPE, &sa, NULL);
#ifndef __vax__
		fpsetmask(FP_X_INV|FP_X_DZ|FP_X_OFL|FP_X_UFL|FP_X_IMP);
#endif
		printf("%g\n", 1 / d);
	}
	if (fltdiv_signalled == 0)
		atf_tc_fail("FPE signal handler was not invoked");
}

static sigjmp_buf sigfpe_int_env;
static void
sigfpe_int_action(int signo, siginfo_t *info, void *ptr)
{

	sig_debug(signo, info, (ucontext_t *)ptr);

	if (intdiv_signalled++ != 0)
		atf_tc_fail("INTDIV handler called more than once");

	ATF_REQUIRE_EQ(info->si_signo, SIGFPE);
	if (info->si_code == FPE_FLTDIV)
		atf_tc_expect_fail("PR port-i386/43655 : integer div-by-zero "
		    "reports FPE_FLTDIV instead of FPE_INTDIV");
	ATF_REQUIRE_EQ(info->si_code, FPE_INTDIV);
	atf_tc_expect_pass();
	ATF_REQUIRE_EQ(info->si_errno, 0);

	siglongjmp(sigfpe_int_env, 1);
}

ATF_TC(sigfpe_int);
ATF_TC_HEAD(sigfpe_int, tc)
{

	atf_tc_set_md_var(tc, "descr",
	    "Checks that signal trampoline correctly calls SIGFPE handler "
	    "for integer div-by-zero");
}

ATF_TC_BODY(sigfpe_int, tc)
{ 
	struct sigaction sa;
	long l = strtol("0", NULL, 10);

	if (sigsetjmp(sigfpe_int_env, 0) == 0) {
		sa.sa_flags = SA_SIGINFO;
		sa.sa_sigaction = sigfpe_int_action;
		sigemptyset(&sa.sa_mask);
		sigaction(SIGFPE, &sa, NULL);
#ifndef __vax__
		fpsetmask(FP_X_INV|FP_X_DZ|FP_X_OFL|FP_X_UFL|FP_X_IMP);
#endif
		printf("%ld\n", 1 / l);
	}
	if (intdiv_signalled == 0)
		atf_tc_fail("FPE signal handler was not invoked");
}

static void
sigsegv_action(int signo, siginfo_t *info, void *ptr)
{

	sig_debug(signo, info, (ucontext_t *)ptr);

	ATF_REQUIRE_EQ(info->si_signo, SIGSEGV);
	ATF_REQUIRE_EQ(info->si_errno, 0);
	ATF_REQUIRE_EQ(info->si_code, SEGV_MAPERR);
	ATF_REQUIRE_EQ(info->si_addr, (void *)0);

	atf_tc_pass();
	/* NOTREACHED */
}

ATF_TC(sigsegv);
ATF_TC_HEAD(sigsegv, tc)
{

	atf_tc_set_md_var(tc, "descr",
	    "Checks that signal trampoline correctly calls SIGSEGV handler");
}

ATF_TC_BODY(sigsegv, tc)
{
	struct sigaction sa;

	sa.sa_flags = SA_SIGINFO;
	sa.sa_sigaction = sigsegv_action;
	sigemptyset(&sa.sa_mask);
	sigaction(SIGSEGV, &sa, NULL);

	*(long *)0 = 0;
	atf_tc_fail("Test did not fault as expected");
}

ATF_TP_ADD_TCS(tp)
{

	ATF_TP_ADD_TC(tp, sigalarm);
	ATF_TP_ADD_TC(tp, sigchild_normal);
	ATF_TP_ADD_TC(tp, sigchild_dump);
	ATF_TP_ADD_TC(tp, sigchild_kill);
	ATF_TP_ADD_TC(tp, sigfpe_flt);
	ATF_TP_ADD_TC(tp, sigfpe_int);
	ATF_TP_ADD_TC(tp, sigsegv);

	return atf_no_error();
}
