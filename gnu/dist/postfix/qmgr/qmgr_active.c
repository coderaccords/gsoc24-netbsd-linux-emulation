/*++
/* NAME
/*	qmgr_active 3
/* SUMMARY
/*	active queue management
/* SYNOPSIS
/*	#include "qmgr.h"
/*
/*	void	qmgr_active_feed(scan_info, queue_id)
/*	QMGR_SCAN *scan_info;
/*	const char *queue_id;
/*
/*	void	qmgr_active_drain()
/*
/*	int	qmgr_active_done(message)
/*	QMGR_MESSAGE *message;
/* DESCRIPTION
/*	These functions maintain the active message queue: the set
/*	of messages that the queue manager is actually working on.
/*	The active queue is limited in size. Messages are drained
/*	from the active queue by allocating a delivery process and
/*	by delivering mail via that process.  Messages leak into the
/*	active queue only when the active queue is small enough.
/*	Damaged message files are saved to the "corrupt" directory.
/*
/*	qmgr_active_feed() inserts the named message file into
/*	the active queue. Message files with the wrong name or
/*	with other wrong properties are skipped but not removed.
/*	The following queue flags are recognized, other flags being
/*	ignored:
/* .IP QMGR_SCAN_ALL
/*	Examine all queue files. Normally, deferred queue files with
/*	future time stamps are ignored, and incoming queue files with
/*	future time stamps are frowned upon.
/* .PP
/*	qmgr_active_drain() allocates one delivery process.
/*	Process allocation is asynchronous. Once the delivery
/*	process is available, an attempt is made to deliver
/*	a message via it. Message delivery is asynchronous, too.
/*
/*	qmgr_active_done() deals with a message after delivery
/*	has been tried for all in-core recipients. If the message
/*	was bounced, a bounce message is sent to the sender, or
/*	to the Errors-To: address if one was specified.
/*	If there are more on-file recipients, a new batch of
/*	in-core recipients is read from the queue file. Otherwise,
/*	if a delivery agent marked the queue file as corrupt,
/*	the queue file is moved to the "corrupt" queue (surprise);
/*	if at least one delivery failed, the message is moved
/*	to the deferred queue. The time stamps of a deferred queue
/*	file are set to the nearest wakeup time of its recipient
/*	sites (if delivery failed due to a problem with a next-hop
/*	host), are set into the future by the amount of time the
/*	message was queued (per-message exponential backoff), or are set
/*	into the future by a minimal backoff time, whichever is more.
/*	The minimal_backoff_time parameter specifies the minimal
/*	amount of time between delivery attempts; maximal_backoff_time
/*	specifies an upper limit.
/* DIAGNOSTICS
/*	Fatal: queue file access failures, out of memory.
/*	Panic: interface violations, internal consistency errors.
/*	Warnings: corrupt message file. A corrupt message is saved
/*	to the "corrupt" queue for further inspection.
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
#include <sys/stat.h>
#include <dirent.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <utime.h>
#include <errno.h>

#ifndef S_IRWXU				/* What? no POSIX system? */
#define S_IRWXU 0700
#endif

/* Utility library. */

#include <msg.h>
#include <events.h>
#include <mymalloc.h>
#include <vstream.h>

/* Global library. */

#include <mail_params.h>
#include <mail_open_ok.h>
#include <mail_queue.h>
#include <recipient_list.h>
#include <bounce.h>
#include <defer.h>
#include <rec_type.h>

/* Application-specific. */

#include "qmgr.h"

/* qmgr_active_corrupt - move corrupted file out of the way */

static void qmgr_active_corrupt(const char *queue_id)
{
    char   *myname = "qmgr_active_corrupt";

    if (mail_queue_rename(queue_id, MAIL_QUEUE_ACTIVE, MAIL_QUEUE_CORRUPT)) {
	if (errno != ENOENT)
	    msg_fatal("%s: save corrupt file queue %s id %s: %m",
		      myname, MAIL_QUEUE_ACTIVE, queue_id);
	msg_warn("%s: save corrupt file queue %s id %s: %m",
		 myname, MAIL_QUEUE_ACTIVE, queue_id);
    } else {
	msg_warn("corrupt file queue %s id %s", MAIL_QUEUE_ACTIVE, queue_id);
    }
}

/* qmgr_active_defer - defer queue file */

static void qmgr_active_defer(const char *queue_name, const char *queue_id,
			              int delay)
{
    char   *myname = "qmgr_active_defer";
    const char *path;
    struct utimbuf tbuf;

    if (msg_verbose)
	msg_info("wakeup %s after %ld secs", queue_id, (long) delay);

    tbuf.actime = tbuf.modtime = event_time() + delay;
    path = mail_queue_path((VSTRING *) 0, queue_name, queue_id);
    if (utime(path, &tbuf) < 0)
	msg_fatal("%s: update %s time stamps: %m", myname, path);
    if (mail_queue_rename(queue_id, queue_name, MAIL_QUEUE_DEFERRED)) {
	if (errno != ENOENT)
	    msg_fatal("%s: rename %s from %s to %s: %m", myname,
		      queue_id, queue_name, MAIL_QUEUE_DEFERRED);
	msg_warn("%s: rename %s from %s to %s: %m", myname,
		 queue_id, queue_name, MAIL_QUEUE_DEFERRED);
    } else if (msg_verbose) {
	msg_info("%s: defer %s", myname, queue_id);
    }
}

/* qmgr_active_feed - feed one message into active queue */

void    qmgr_active_feed(QMGR_SCAN *scan_info, const char *queue_id)
{
    char   *myname = "qmgr_active_feed";
    QMGR_MESSAGE *message;
    struct stat st;
    const char *path;

    if (strcmp(scan_info->queue, MAIL_QUEUE_ACTIVE) == 0)
	msg_panic("%s: bad queue %s", myname, scan_info->queue);
    if (msg_verbose)
	msg_info("%s: queue %s", myname, scan_info->queue);

    /*
     * Make sure this is something we are willing to open.
     */
    if (mail_open_ok(scan_info->queue, queue_id, &st, &path) == MAIL_OPEN_NO)
	return;

    if (msg_verbose)
	msg_info("%s: %s", myname, path);

    /*
     * Skip files that have time stamps into the future. They need to cool
     * down. Incoming and deferred files can have future time stamps.
     */
    if ((scan_info->flags & QMGR_SCAN_ALL) == 0
	&& st.st_mtime > time((time_t *) 0) + 1) {
	if (msg_verbose)
	    msg_info("%s: skip %s (%ld seconds)", myname, queue_id,
		     (long) (st.st_mtime - event_time()));
	return;
    }

    /*
     * Move the message to the active queue. File access errors are fatal.
     */
    if (mail_queue_rename(queue_id, scan_info->queue, MAIL_QUEUE_ACTIVE)) {
	if (errno != ENOENT)
	    msg_fatal("%s: %s: rename from %s to %s: %m", myname,
		      queue_id, scan_info->queue, MAIL_QUEUE_ACTIVE);
	msg_warn("%s: %s: rename from %s to %s: %m", myname,
		 queue_id, scan_info->queue, MAIL_QUEUE_ACTIVE);
	return;
    }

    /*
     * Extract envelope information: sender and recipients. At this point,
     * mail addresses have been processed by the cleanup service so they
     * should be in canonical form. Generate requests to deliver this
     * message.
     * 
     * Throwing away queue files seems bad, especially when they made it this
     * far into the mail system. Therefore we save bad files to a separate
     * directory for further inspection.
     * 
     * After queue manager restart it is possible that a queue file is still
     * being delivered. In that case (the file is locked), defer delivery by
     * a minimal amount of time.
     */
    if ((message = qmgr_message_alloc(MAIL_QUEUE_ACTIVE, queue_id,
				      scan_info->flags)) == 0) {
	qmgr_active_corrupt(queue_id);
    } else if (message == QMGR_MESSAGE_LOCKED) {
	qmgr_active_defer(MAIL_QUEUE_ACTIVE, queue_id, var_min_backoff_time);
    } else {

	/*
	 * Special case if all recipients were already delivered. Send any
	 * bounces and clean up.
	 */
	if (message->refcount == 0)
	    qmgr_active_done(message);
    }
}

/* qmgr_active_done - dispose of message after recipients have been tried */

void    qmgr_active_done(QMGR_MESSAGE *message)
{
    char   *myname = "qmgr_active_done";
    struct stat st;
    const char *path;
    int     delay;

    if (msg_verbose)
	msg_info("%s: %s", myname, message->queue_id);

    /*
     * During a previous iteration, an attempt to bounce this message may
     * have failed, so there may still be a bounce log lying around. XXX By
     * groping around in the bounce queue, we're trespassing on the bounce
     * service's territory. But doing so is more robust than depending on the
     * bounce daemon to do the lookup for us, and for us to do the deleting
     * after we have received a successful status from the bounce service.
     * The bounce queue directory blocks are most likely in memory anyway. If
     * these lookups become a performance problem we will have to build an
     * in-core cache into the bounce daemon.
     * 
     * Don't bounce when the bounce log is empty. The bounce process obviously
     * failed, and the delivery agent will have requested that the message be
     * deferred.
     */
    if (stat(mail_queue_path((VSTRING *) 0, MAIL_QUEUE_BOUNCE, message->queue_id), &st) == 0) {
	if (st.st_size == 0) {
	    if (mail_queue_remove(MAIL_QUEUE_BOUNCE, message->queue_id))
		msg_fatal("remove %s %s: %m",
			  MAIL_QUEUE_BOUNCE, message->queue_id);
	} else {
	    if (msg_verbose)
		msg_info("%s: bounce %s", myname, message->queue_id);
	    message->flags |= bounce_flush(BOUNCE_FLAG_KEEP,
					   message->queue_name,
					   message->queue_id,
					   message->errors_to);
	}
    }

    /*
     * A delivery agent marks a queue file as corrupt by changing its
     * attributes, and by pretending that delivery was deferred.
     */
    if (message->flags
	&& !mail_open_ok(MAIL_QUEUE_ACTIVE, message->queue_id, &st, &path)) {
	qmgr_active_corrupt(message->queue_id);
	qmgr_message_free(message);
	return;
    }

    /*
     * If we did not read all recipients from this file, go read some more,
     * but remember whether some recipients have to be tried again.
     * 
     * Throwing away queue files seems bad, especially when they made it this
     * far into the mail system. Therefore we save bad files to a separate
     * directory for further inspection by a human being.
     */
    if (message->rcpt_offset > 0) {
	if (qmgr_message_realloc(message) == 0) {
	    qmgr_active_corrupt(message->queue_id);
	    qmgr_message_free(message);
	} else {
	    if (message->refcount == 0)
		qmgr_active_done(message);	/* recurse for consistency */
	}
	return;
    }

    /*
     * If we get to this point we have tried all recipients for this message.
     * If the message is too old, try to bounce it.
     */
#define HOUR	3600
#define DAY	86400

    if (message->flags) {
	if (event_time() > message->arrival_time + var_max_queue_time * DAY) {
	    if (msg_verbose)
		msg_info("%s: too old, bouncing %s", myname, message->queue_id);
	    message->flags = defer_flush(BOUNCE_FLAG_KEEP,
					 message->queue_name,
					 message->queue_id,
					 message->errors_to);
	} else if (message->warn_time > 0
		   && event_time() > message->warn_time) {
	    if (msg_verbose)
		msg_info("%s: sending defer warning for %s", myname, message->queue_id);
	    if (defer_warn(BOUNCE_FLAG_KEEP,
			   message->queue_name,
			   message->queue_id,
			   message->errors_to) == 0) {
		qmgr_message_update_warn(message);
	    }
	}
    }

    /*
     * Some recipients need to be tried again. Move the queue file time
     * stamps into the future by the amount of time that the message is
     * delayed, and move the message to the deferred queue. Impose minimal
     * and maximal backoff times.
     * 
     * Since we look at actual time in queue, not time since last delivery
     * attempt, backoff times will be distributed. However, we can still see
     * spikes in delivery activity because the interval between deferred
     * queue scans is finite.
     */
    if (message->flags) {
	if (message->arrival_time > 0) {
	    delay = event_time() - message->arrival_time;
	    if (delay > var_max_backoff_time)
		delay = var_max_backoff_time;
	    if (delay < var_min_backoff_time)
		delay = var_min_backoff_time;
	} else {
	    delay = var_min_backoff_time;
	}
	qmgr_active_defer(message->queue_name, message->queue_id, delay);
    }

    /*
     * All recipients done. Remove the queue file.
     */
    else {
	if (mail_queue_remove(message->queue_name, message->queue_id)) {
	    if (errno != ENOENT)
		msg_fatal("%s: remove %s from %s: %m", myname,
			  message->queue_id, message->queue_name);
	    msg_warn("%s: remove %s from %s: %m", myname,
		     message->queue_id, message->queue_name);
	} else if (msg_verbose) {
	    msg_info("%s: remove %s", myname, message->queue_id);
	}
    }

    /*
     * Finally, delete the in-core message structure.
     */
    qmgr_message_free(message);
}

/* qmgr_active_drain - drain active queue by allocating a delivery process */

void    qmgr_active_drain(void)
{
    QMGR_TRANSPORT *transport;

    /*
     * Use round-robin search to find a transport with pending mail. Allocate
     * a delivery process. The process allocation completes asynchronously.
     */
    if ((transport = qmgr_transport_select()) != 0) {
	if (msg_verbose)
	    msg_info("qmgr_active_drain: allocate %s", transport->name);
	qmgr_transport_alloc(transport, qmgr_deliver);
    }
}
