/*	$NetBSD: extern.h,v 1.11 2005/02/22 01:08:43 christos Exp $	*/

/*
 * Copyright (c) 1994
 *	The Regents of the University of California.  All rights reserved.
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
 *
 *	@(#)extern.h	8.1 (Berkeley) 4/2/94
 */

/* return values from pw_init() and pw_arg_end() */
enum {
	PW_USE_FORCE,
	PW_USE,
	PW_DONT_USE
};

#ifdef KERBEROS5
int	krb5_init __P((const char *));
int	krb5_arg __P((char, const char *));
int	krb5_arg_end __P((void));
void	krb5_end __P((void));
int	krb5_chpw __P((const char *));
#endif
#ifdef KERBEROS
int	krb4_init __P((const char *));
int	krb4_arg __P((char, const char *));
int	krb4_arg_end __P((void));
void	krb4_end __P((void));
int	krb4_chpw __P((const char *));
#endif
#ifdef YP
int	yp_init __P((const char *));
int	yp_arg __P((char, const char *));
int	yp_arg_end __P((void));
void	yp_end __P((void));
int	yp_chpw __P((const char *));
#endif
#ifdef USE_PAM
/* PAM */
int	pwpam_init __P((const char *));
int	pwpam_arg __P((char, const char *));
int	pwpam_arg_end __P((void));
void	pwpam_end __P((void));
int	pwpam_chpw __P((const char *));
#endif
/* local */
int	local_init __P((const char *));
int	local_arg __P((char, const char *));
int	local_arg_end __P((void));
void	local_end __P((void));
int	local_chpw __P((const char *));
