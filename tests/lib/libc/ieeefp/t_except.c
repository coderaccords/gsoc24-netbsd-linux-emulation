/*	$NetBSD: t_except.c,v 1.3 2011/01/03 09:10:54 dholland Exp $	*/

/*-
 * Copyright (c) 1995 The NetBSD Foundation, Inc.
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
#include <atf-c/config.h>

#include <stdio.h>
#include <signal.h>
#include <assert.h>
#include <ieeefp.h>
#include <float.h>
#include <setjmp.h>
#include <stdlib.h>
#include <string.h>

void sigfpe(int, siginfo_t *, void *);
volatile sig_atomic_t signal_caught;
volatile int sicode;

#ifdef USE_FLOAT
#define FPTYPE float
#else
#ifdef USE_LONGDOUBLE
#define FPTYPE long double
#else
#define FPTYPE double
#endif
#endif
static volatile const float	f_one   = 1.0;
static volatile const float	f_zero  = 0.0;
static volatile const double	d_one   = 1.0;
static volatile const double	d_zero  = 0.0;
static volatile const long double ld_one  = 1.0;
static volatile const long double ld_zero = 0.0;

static volatile const float	f_huge = FLT_MAX;
static volatile const float	f_tiny = FLT_MIN;
static volatile const double	d_huge = DBL_MAX;
static volatile const double	d_tiny = DBL_MIN;
static volatile const long double ld_huge = LDBL_MAX;
static volatile const long double ld_tiny = LDBL_MIN;

static volatile float f_x;
static volatile double d_x;
static volatile long double ld_x;

/* trip divide by zero */
static void
f_dz(void)
{

	f_x = f_one / f_zero;
}

static void
d_dz(void)
{

	d_x = d_one / d_zero;
}

static void
ld_dz(void)
{

	ld_x = ld_one / ld_zero;
}

/* trip invalid operation */
static void
d_inv(void)
{

	d_x = d_zero / d_zero;
}

static void
ld_inv(void)
{

	ld_x = ld_zero / ld_zero;
}

static void
f_inv(void)
{

	f_x = f_zero / f_zero;
}

/* trip overflow */
static void
f_ofl(void)
{

	f_x = f_huge * f_huge;
}

static void
d_ofl(void)
{

	d_x = d_huge * d_huge;
}

static void
ld_ofl(void)
{

	ld_x = ld_huge * ld_huge;
}

/* trip underflow */
static void
f_ufl(void)
{

	f_x = f_tiny * f_tiny;
}

static void
d_ufl(void)
{

	d_x = d_tiny * d_tiny;
}

static void
ld_ufl(void)
{

	ld_x = ld_tiny * ld_tiny;
}

struct ops {
	void (*op)(void);
	fp_except mask;
	int sicode;
};

static const struct ops float_ops[] = {
	{ f_dz, FP_X_DZ, FPE_FLTDIV },
	{ f_inv, FP_X_INV, FPE_FLTINV },
	{ f_ofl, FP_X_OFL, FPE_FLTOVF },
	{ f_ufl, FP_X_UFL, FPE_FLTUND },
	{ NULL, 0, 0 }
};

static const struct ops double_ops[] = {
	{ d_dz, FP_X_DZ, FPE_FLTDIV },
	{ d_inv, FP_X_INV, FPE_FLTINV },
	{ d_ofl, FP_X_OFL, FPE_FLTOVF },
	{ d_ufl, FP_X_UFL, FPE_FLTUND },
	{ NULL, 0, 0 }
};

static const struct ops long_double_ops[] = {
	{ ld_dz, FP_X_DZ, FPE_FLTDIV },
	{ ld_inv, FP_X_INV, FPE_FLTINV },
	{ ld_ofl, FP_X_OFL, FPE_FLTOVF },
	{ ld_ufl, FP_X_UFL, FPE_FLTUND },
	{ NULL, 0, 0 }
};

static sigjmp_buf b;

static void
masked(const struct ops *test_ops)
{
	struct sigaction sa;
	fp_except ex1, ex2;
	static const struct ops *t;

	/* mask all exceptions, clear history */
	fpsetmask(0);
	fpsetsticky(0);

	/* set up signal handler */
	sa.sa_sigaction = sigfpe;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_SIGINFO;
	sigaction(SIGFPE, &sa, 0);
	signal_caught = 0;

	/*
	 * exceptions masked, check whether "sticky" bits are set correctly
	 */
	for (t = test_ops; t->op != NULL; t++) {
		(*t->op)();
		ex1 = fpgetsticky();
		ATF_CHECK_EQ(ex1 & t->mask, t->mask);
		ATF_CHECK_EQ(signal_caught, 0);

		/* check correct fpsetsticky() behaviour */
		ex2 = fpsetsticky(0);
		ATF_CHECK_EQ(fpgetsticky(), 0);
		ATF_CHECK_EQ(ex1, ex2);
	}
}

/* force delayed exceptions to be delivered */
#define BARRIER() fpsetmask(0); f_x = f_one * f_one

static void
unmasked(const struct ops *test_ops)
{
	struct sigaction sa;
	int r;
	const struct ops *volatile t;

	/* mask all exceptions, clear history */
	fpsetmask(0);
	fpsetsticky(0);

	/* set up signal handler */
	sa.sa_sigaction = sigfpe;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_SIGINFO;
	sigaction(SIGFPE, &sa, 0);
	signal_caught = 0;

	/*
	 * exception unmasked, check SIGFPE delivery and correct siginfo
	 */
	for (t = test_ops; t->op != NULL; t++) {
		fpsetmask(t->mask);
		r = sigsetjmp(b, 1);
		if (!r) {
			(*t->op)();
			BARRIER();
		}
		ATF_CHECK_EQ(signal_caught, 1);
		ATF_CHECK_EQ(sicode, t->sicode);
		signal_caught = 0;
	}
}

void
sigfpe(int s, siginfo_t *si, void *c)
{
	signal_caught = 1;
	sicode = si->si_code;
	siglongjmp(b, 1);
}

#define TEST(m, t)							\
	ATF_TC(m##_##t);						\
									\
	ATF_TC_HEAD(m##_##t, tc)					\
	{								\
									\
		atf_tc_set_md_var(tc, "descr",				\
		    "Test " ___STRING(m) " exceptions for "		\
		    ___STRING(t) "values");				\
	}								\
									\
	ATF_TC_BODY(m##_##t, tc)					\
	{								\
									\
		m(t##_ops);						\
	}

TEST(masked, float)
TEST(masked, double)
TEST(masked, long_double)
TEST(unmasked, float)
TEST(unmasked, double)
TEST(unmasked, long_double)

ATF_TP_ADD_TCS(tp)
{
	const char *arch;

	arch = atf_config_get("atf_machine");
	if (strcmp("vax", arch) == 0 || strcmp("m68000", arch) == 0)
		printf("Test not applicable on %s", arch);
	else {
		ATF_TP_ADD_TC(tp, masked_float);
		ATF_TP_ADD_TC(tp, masked_double);
		ATF_TP_ADD_TC(tp, masked_long_double);
		ATF_TP_ADD_TC(tp, unmasked_float);
		ATF_TP_ADD_TC(tp, unmasked_double);
		ATF_TP_ADD_TC(tp, unmasked_long_double);
	}

	return atf_no_error();
}
