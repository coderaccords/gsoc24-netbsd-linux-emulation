/*++
/* NAME
/*	master_wakeup 3
/* SUMMARY
/*	Postfix master - start/stop service wakeup timers
/* SYNOPSIS
/*	#include "master.h"
/*
/*	void	master_wakeup_init(serv)
/*	MASTER_SERV *serv;
/*
/*	void	master_wakeup_cleanup(serv)
/*	MASTER_SERV *serv;
/* DESCRIPTION
/*	This module implements automatic service wakeup. In order to
/*	wakeup a service, a wakeup trigger is sent to the corresponding
/*	service port or FIFO, and a timer is started to repeat this sequence
/*	after a configurable amount of time.
/*
/*	master_wakeup_init() wakes up the named service. No wakeup
/*	is done or scheduled when a zero wakeup time is given, or when
/*	the service has been throttled in the mean time.
/*	It is OK to call master_wakeup_init() while a timer is already
/*	running for the named service. The effect is to restart the
/*	wakeup timer.
/*
/*	master_wakeup_cleanup() cancels the wakeup timer for the named
/*	service. It is an error to disable a service while it still has
/*	an active wakeup timer (doing so would cause a dangling reference
/*	to a non-existent service).
/*	It is OK to call master_wakeup_cleanup() even when no timer is
/*	active for the named service.
/* DIAGNOSTICS
/* BUGS
/* SEE ALSO
/*	inet_trigger(3), internet-domain client
/*	unix_trigger(3), unix-domain client
/*	fifo_trigger(3), fifo client
/* LICENSE
/* .ad
/* .fi
/*	The Secure Mailer license must be distributed with this software.
/* AUTHOR(S)
/*	Wietse Venema
/*	IBM T.J. Watson Research
/*	P.O. Box 704
/*	Yorktown Heights, NY 10598, USA
/*--*/

/* System library. */

#include <sys_defs.h>
#include <unistd.h>
#include <string.h>

/* Utility library. */

#include <msg.h>
#include <trigger.h>
#include <events.h>

/* Global library. */

#include <mail_proto.h>			/* triggers */

/* Application-specific. */

#include "mail_server.h"
#include "master.h"

/* master_wakeup_timer_event - wakeup event handler */

static void master_wakeup_timer_event(int unused_event, char *context)
{
    char   *myname = "master_wakeup_timer_event";
    MASTER_SERV *serv = (MASTER_SERV *) context;
    int     status;
    static char wakeup = TRIGGER_REQ_WAKEUP;

    /*
     * Don't wakeup services whose automatic wakeup feature was turned off in
     * the mean time.
     */
    if (serv->wakeup_time == 0)
	return;

    /*
     * Don't wake up services that are throttled. Find out what transport to
     * use. We can't block here so we choose a short timeout.
     */
#define BRIEFLY	1

    if (MASTER_THROTTLED(serv) == 0) {
	if (msg_verbose)
	    msg_info("%s: service %s", myname, serv->name);

	switch (serv->type) {
	case MASTER_SERV_TYPE_INET:
	    status = inet_trigger(serv->name, &wakeup, sizeof(wakeup), BRIEFLY);
	    break;
	case MASTER_SERV_TYPE_UNIX:
	    status = LOCAL_TRIGGER(serv->name, &wakeup, sizeof(wakeup), BRIEFLY);
	    break;
	case MASTER_SERV_TYPE_FIFO:
	    status = fifo_trigger(serv->name, &wakeup, sizeof(wakeup), BRIEFLY);
	    break;
	default:
	    msg_panic("%s: unknown service type: %d", myname, serv->type);
	}
	if (status < 0)
	    msg_warn("%s: service %s: %m", myname, serv->name);
    }

    /*
     * Schedule another wakeup event.
     */
    event_request_timer(master_wakeup_timer_event, (char *) serv,
			serv->wakeup_time);
}

/* master_wakeup_init - start automatic service wakeup */

void    master_wakeup_init(MASTER_SERV *serv)
{
    char   *myname = "master_wakeup_init";

    if (serv->wakeup_time == 0)
	return;
    if (msg_verbose)
	msg_info("%s: service %s time %d",
		 myname, serv->name, serv->wakeup_time);
    master_wakeup_timer_event(0, (char *) serv);
}

/* master_wakeup_cleanup - cancel wakeup timer */

void    master_wakeup_cleanup(MASTER_SERV *serv)
{
    char   *myname = "master_wakeup_cleanup";

    /*
     * Cleanup, even when the wakeup feature has been turned off. There might
     * still be a pending timer. Don't depend on the code that reloads the
     * config file to reset the wakeup timer when things change.
     */
    if (msg_verbose)
	msg_info("%s: service %s", myname, serv->name);

    event_cancel_timer(master_wakeup_timer_event, (char *) serv);
}
