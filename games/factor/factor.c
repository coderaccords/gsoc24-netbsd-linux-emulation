/*	$NetBSD: factor.c,v 1.20 2010/04/22 14:28:48 drochner Exp $	*/

/*
 * Copyright (c) 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Landon Curt Noll.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
#ifndef lint
__COPYRIGHT("@(#) Copyright (c) 1989, 1993\
 The Regents of the University of California.  All rights reserved.");
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "@(#)factor.c	8.4 (Berkeley) 5/4/95";
#else
__RCSID("$NetBSD: factor.c,v 1.20 2010/04/22 14:28:48 drochner Exp $");
#endif
#endif /* not lint */

/*
 * factor - factor a number into primes
 *
 * By: Landon Curt Noll   chongo@toad.com,   ...!{sun,tolsoft}!hoptoad!chongo
 *
 *   chongo <for a good prime call: 391581 * 2^216193 - 1> /\oo/\
 *
 * usage:
 *	factor [number] ...
 *
 * The form of the output is:
 *
 *	number: factor1 factor1 factor2 factor3 factor3 factor3 ...
 *
 * where factor1 <= factor2 <= factor3 <= ...
 *
 * If no args are given, the list of numbers are read from stdin.
 */

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#ifdef HAVE_OPENSSL
#include <openssl/bn.h>
#else
typedef long	BIGNUM;
typedef u_long	BN_ULONG;
static int BN_dec2bn(BIGNUM **a, const char *str);
#define BN_new()		((BIGNUM *)calloc(sizeof(BIGNUM), 1))
#define BN_is_zero(v)		(*(v) == 0)
#define BN_is_one(v)		(*(v) == 1)
#define BN_new()		((BIGNUM *)calloc(sizeof(BIGNUM), 1))
#define BN_is_zero(v)		(*(v) == 0)
#define BN_is_one(v)		(*(v) == 1)
#define BN_mod_word(a, b)	(*(a) % (b))
#endif

#include "primes.h"

/*
 * prime[i] is the (i-1)th prime.
 *
 * We are able to sieve 2^32-1 because this byte table yields all primes
 * up to 65537 and 65537^2 > 2^32-1.
 */
extern const ubig prime[];
extern const ubig *pr_limit;		/* largest prime in the prime array */

#define	PRIME_CHECKS	5

#ifdef HAVE_OPENSSL 
static BN_CTX *ctx;			/* just use a global context */
#endif

static void pr_fact(BIGNUM *);		/* print factors of a value */
static void BN_print_dec_fp(FILE *, const BIGNUM *);
static void usage(void) __dead;
#ifdef HAVE_OPENSSL
static void pollard_pminus1(BIGNUM *);	/* print factors for big numbers */
#else
static char *BN_bn2dec(const BIGNUM *);
static BN_ULONG BN_div_word(BIGNUM *, BN_ULONG);
#endif


#ifndef HAVE_OPENSSL
static int
BN_dec2bn(BIGNUM **a, const char *str)
{
	char *p;

	errno = 0;
	**a = strtoul(str, &p, 10);
	if (errno)
		err(1, "%s", str);
	return (*p == '\n' || *p == '\0');
}
#endif

int
main(int argc, char *argv[])
{
	BIGNUM *val;
	int ch;
	char *p, buf[LINE_MAX];		/* > max number of digits. */

#ifdef HAVE_OPENSSL 
	ctx = BN_CTX_new();
#endif
	val = BN_new();
	if (val == NULL)
		errx(1, "can't initialise bignum");

	while ((ch = getopt(argc, argv, "")) != -1)
		switch (ch) {
		case '?':
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

	/* No args supplied, read numbers from stdin. */
	if (argc == 0)
		for (;;) {
			if (fgets(buf, sizeof(buf), stdin) == NULL) {
				if (ferror(stdin))
					err(1, "stdin");
				exit (0);
			}
			for (p = buf; isblank(*p); ++p);
			if (*p == '\n' || *p == '\0')
				continue;
			if (*p == '-')
				errx(1, "negative numbers aren't permitted.");
			if (BN_dec2bn(&val, buf) == 0)
				errx(1, "%s: illegal numeric format.", argv[0]);
			pr_fact(val);
		}
	/* Factor the arguments. */
	else
		for (; *argv != NULL; ++argv) {
			if (argv[0][0] == '-')
				errx(1, "negative numbers aren't permitted.");
			if (BN_dec2bn(&val, argv[0]) == 0)
				errx(1, "%s: illegal numeric format.", argv[0]);
			pr_fact(val);
		}
	exit(0);
}

/*
 * pr_fact - print the factors of a number
 *
 * If the number is 0 or 1, then print the number and return.
 * If the number is < 0, print -1, negate the number and continue
 * processing.
 *
 * Print the factors of the number, from the lowest to the highest.
 * A factor will be printed numtiple times if it divides the value
 * multiple times.
 *
 * Factors are printed with leading tabs.
 */
static void
pr_fact(BIGNUM *val)
{
	const ubig *fact;		/* The factor found. */

	/* Firewall - catch 0 and 1. */
	if (BN_is_zero(val))	/* Historical practice; 0 just exits. */
		exit(0);
	if (BN_is_one(val)) {
		printf("1: 1\n");
		return;
	}

	/* Factor value. */

	BN_print_dec_fp(stdout, val);
	putchar(':');
	for (fact = &prime[0]; !BN_is_one(val); ++fact) {
		/* Look for the smallest factor. */
		while (fact <= pr_limit) {
			if (BN_mod_word(val, (BN_ULONG)*fact) == 0)
				break;
			fact++;
		}

		/* Watch for primes larger than the table. */
		if (fact > pr_limit) {
#ifdef HAVE_OPENSSL
			BIGNUM *bnfact;

			bnfact = BN_new();
			BN_set_word(bnfact, *(fact - 1));
			BN_sqr(bnfact, bnfact, ctx);
			if (BN_cmp(bnfact, val) > 0
			    || BN_is_prime(val, PRIME_CHECKS, NULL, NULL,
					   NULL) == 1) {
				putchar(' ');
				BN_print_dec_fp(stdout, val);
			} else
				pollard_pminus1(val);
#else
			printf(" %s", BN_bn2dec(val));
#endif
			break;
		}

		/* Divide factor out until none are left. */
		do {
			printf(" %lu", *fact);
			BN_div_word(val, (BN_ULONG)*fact);
		} while (BN_mod_word(val, (BN_ULONG)*fact) == 0);

		/* Let the user know we're doing something. */
		fflush(stdout);
	}
	putchar('\n');
}

/*
 * Sigh..  No _decimal_ output to file functions in BN.
 */
static void
BN_print_dec_fp(FILE *fp, const BIGNUM *num)
{
	char *buf;
	
	buf = BN_bn2dec(num);
	if (buf == NULL)
		return;	/* XXX do anything here? */
	fprintf(fp, buf);
	free(buf);
}

void
usage(void)
{
	fprintf(stderr, "usage: factor [value ...]\n");
	exit (0);
}




#ifdef HAVE_OPENSSL
/* pollard p-1, algorithm from Jim Gillogly, May 2000 */

static void
pollard_pminus1(BIGNUM *val)
{
	BIGNUM *base, *rbase, *num, *i, *x;

	base = BN_new();
	rbase = BN_new();
	num = BN_new();
	i = BN_new();
	x = BN_new();

	BN_set_word(rbase, 1);
 newbase:
	BN_add_word(rbase, 1);
	BN_set_word(i, 2);
	BN_copy(base, rbase);

	for (;;) {
		BN_mod_exp(base, base, i, val, ctx);
		if (BN_is_one(base))
			goto newbase;

		BN_copy(x, base);
		BN_sub_word(x, 1);
		BN_gcd(x, x, val, ctx);

		if (!BN_is_one(x)) {
			if (BN_is_prime(x, PRIME_CHECKS, NULL, NULL,
			    NULL) == 1) {
				putchar(' ');
				BN_print_dec_fp(stdout, x);
			} else
				pollard_pminus1(x);
			fflush(stdout);

			BN_div(num, NULL, val, x, ctx);
			if (BN_is_one(num))
				return;
			if (BN_is_prime(num, PRIME_CHECKS, NULL, NULL,
			    NULL) == 1) {
				putchar(' ');
				BN_print_dec_fp(stdout, num);
				fflush(stdout);
				return;
			}
			BN_copy(val, num);
		}
		BN_add_word(i, 1);
	}
}
#else
char *
BN_bn2dec(const BIGNUM *val)
{
	char *buf;

	buf = malloc(100);
	if (!buf)
		return buf;
	snprintf(buf, 100, "%ld", (long)*val);
	return buf;
}

static BN_ULONG
BN_div_word(BIGNUM *a, BN_ULONG b)
{
	BN_ULONG mod;

	mod = *a % b;
	*a /= b;
	return mod;
}
#endif
