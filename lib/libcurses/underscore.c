/*	$NetBSD: underscore.c,v 1.8 2004/01/20 08:31:46 wiz Exp $	*/

/*-
 * Copyright (c) 1999 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Julian Coleman.
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
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
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
#ifndef lint
__RCSID("$NetBSD: underscore.c,v 1.8 2004/01/20 08:31:46 wiz Exp $");
#endif				/* not lint */

#include "curses.h"
#include "curses_private.h"

#ifndef _CURSES_USE_MACROS

/*
 * underscore --
 *	Enter underscore mode on stdscr.
 */
int
underscore(void)
{
	return wunderscore(stdscr);
}


/*
 * underend --
 *	Exit underscore mode on stdscr.
 */
int
underend(void)
{
	return wunderend(stdscr);
}

#endif

/*
 * wunderscore --
 *	Enter underscore mode.
 */
int
wunderscore(WINDOW *win)
{
	/* If can underscore, set the screen underscore bit. */
	if ((__tc_us != NULL && __tc_ue != NULL) || __tc_uc != NULL) {
#ifdef DEBUG
		__CTRACE("wunderscore\n");
#endif
		win->wattr |= __UNDERSCORE;
	}
	return (1);
}

/*
 * wunderend --
 *	Exit underscore mode.
 */
int
wunderend(WINDOW *win)
{
	if (__tc_ue != NULL) {
#ifdef DEBUG
		__CTRACE("wunderend\n");
#endif
		win->wattr &= ~__UNDERSCORE;
	}
        return 1;
}
