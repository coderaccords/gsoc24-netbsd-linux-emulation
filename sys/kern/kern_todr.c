/*	$NetBSD: kern_todr.c,v 1.10 2006/09/07 04:51:42 gdamore Exp $	*/

/*
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * the Systems Programming Group of the University of Utah Computer
 * Science Department and Ralph Campbell.
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
 * from: Utah Hdr: clock.c 1.18 91/01/21
 *
 *	@(#)clock.c	8.1 (Berkeley) 6/10/93
 */
/*
 * Copyright (c) 1988 University of Utah.
 *
 * This code is derived from software contributed to Berkeley by
 * the Systems Programming Group of the University of Utah Computer
 * Science Department and Ralph Campbell.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
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
 * from: Utah Hdr: clock.c 1.18 91/01/21
 *
 *	@(#)clock.c	8.1 (Berkeley) 6/10/93
 */
#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: kern_todr.c,v 1.10 2006/09/07 04:51:42 gdamore Exp $");

#include <sys/param.h>

#ifdef	__HAVE_GENERIC_TODR

#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/timetc.h>
#include <dev/clock_subr.h>	/* hmm.. this should probably move to sys */

static todr_chip_handle_t todr_handle = NULL;

/*
 * Attach the clock device to todr_handle.
 */
void
todr_attach(todr_chip_handle_t todr)
{

        if (todr_handle) {
                printf("todr_attach: TOD already configured\n");
		return;
	}
        todr_handle = todr;
}

static int timeset = 0;

/*
 * Set up the system's time, given a `reasonable' time value.
 */
void
inittodr(time_t base)
{
	int badbase = 0, waszero = (base == 0), goodtime = 0, badrtc = 0;
#ifdef	__HAVE_TIMECOUNTER
	struct timespec ts;
#endif
	struct timeval tv;

	if (base < 5 * SECYR) {
		struct clock_ymdhms basedate;

		/*
		 * If base is 0, assume filesystem time is just unknown
		 * instead of preposterous. Don't bark.
		 */
		if (base != 0)
			printf("WARNING: preposterous time in file system\n");
		/* not going to use it anyway, if the chip is readable */
		basedate.dt_year = 2006;
		basedate.dt_mon = 1;
		basedate.dt_day = 1;
		basedate.dt_hour = 12;
		basedate.dt_min = 0;
		basedate.dt_sec = 0;
		base = clock_ymdhms_to_secs(&basedate);
		badbase = 1;
	}

	/*
	 * Some ports need to be supplied base in order to fabricate a time_t.
	 */
	tv.tv_sec = base;
	tv.tv_usec = 0;

	if ((todr_handle == NULL) ||
	    (todr_gettime(todr_handle, &tv) != 0) ||
	    (tv.tv_sec < (5 * SECYR))) {

		if (todr_handle != NULL)
			printf("WARNING: preposterous TOD clock time\n");
		else
			printf("WARNING: no TOD clock present\n");
		badrtc = 1;
	} else {
		int deltat = tv.tv_sec - base;

		if (deltat < 0)
			deltat = -deltat;

		if ((badbase == 0) && deltat >= 2 * SECDAY) {
			
			if (tv.tv_sec < base) {
				/*
				 * The clock should never go backwards
				 * relative to filesystem time.  If it
				 * does by more than the threshold,
				 * believe the filesystem.
				 */
				printf("WARNING: clock lost %d days\n",
				    deltat / SECDAY);
				badrtc = 1;
			}
			else {
				printf("WARNING: clock gained %d days\n",
				    deltat / SECDAY);
			}
		} else {
			goodtime = 1;
		}
	}

	/* if the rtc time is bad, use the filesystem time */
	if (badrtc) {
		if (badbase) {
			printf("WARNING: using default initial time\n");
		} else {
			printf("WARNING: using filesystem time\n");
		}
		tv.tv_sec = base;
		tv.tv_usec = 0;
	}

	timeset = 1;

#ifdef	__HAVE_TIMECOUNTER
	ts.tv_sec = tv.tv_sec;
	ts.tv_nsec = tv.tv_usec * 1000;
	tc_setclock(&ts);
#else
	time = tv;
#endif

	if (waszero || goodtime)
		return;

	printf("WARNING: CHECK AND RESET THE DATE!\n");
}

/*
 * Reset the TODR based on the time value; used when the TODR
 * has a preposterous value and also when the time is reset
 * by the stime system call.  Also called when the TODR goes past
 * TODRZERO + 100*(SECYEAR+2*SECDAY) (e.g. on Jan 2 just after midnight)
 * to wrap the TODR around.
 */
void
resettodr(void)
{
#ifdef	__HAVE_TIMECOUNTER
	struct timeval	time;
#endif

	/*
	 * We might have been called by boot() due to a crash early
	 * on.  Don't reset the clock chip if we don't know what time
	 * it is.
	 */
	if (!timeset)
		return;

#ifdef	__HAVE_TIMECOUNTER
	getmicrotime(&time);
#endif

	if (time.tv_sec == 0)
		return;

	if (todr_handle)
		if (todr_settime(todr_handle, &time) != 0)
			printf("Cannot set TOD clock time\n");
}

#endif	/* __HAVE_GENERIC_TODR */


int
todr_gettime(todr_chip_handle_t tch, struct timeval *tvp)
{
	struct clock_ymdhms	dt;
	int			rv;

	if (tch->todr_gettime)
		return tch->todr_gettime(tch, tvp);

	if (tch->todr_gettime_ymdhms) {
		if ((rv = tch->todr_gettime_ymdhms(tch, &dt)) != 0)	
			return rv;

		/*
		 * Formerly we had code here that explicitly checked
		 * for 2038 year rollover.
		 *
		 * However, clock_ymdhms_to_secs performs the same
		 * check for us, so we need not worry about it.  Note
		 * that this assumes that the tvp->tv_sec member is is
		 * a time_t.
		 */

		/* simple sanity checks */
		if (dt.dt_mon > 12 || dt.dt_day > 31 ||
		    dt.dt_hour >= 24 || dt.dt_min >= 60 || dt.dt_sec >= 60)
			return -1;

		tvp->tv_sec = clock_ymdhms_to_secs(&dt) + rtc_offset * 60;
		tvp->tv_usec = 0;
		return tvp->tv_sec < 0 ? -1 : 0;
	}

	return -1;
}

int
todr_settime(todr_chip_handle_t tch, volatile struct timeval *tvp)
{
	struct clock_ymdhms	dt;

	if (tch->todr_settime)
		return tch->todr_settime(tch, tvp);

	if (tch->todr_settime_ymdhms) {
		time_t	sec = tvp->tv_sec - rtc_offset * 60;
		if (tvp->tv_usec >= 500000)
			sec++;
		clock_secs_to_ymdhms(sec, &dt);
		return tch->todr_settime_ymdhms(tch, &dt);
	}

	return -1;
}
