/*	$NetBSD: scsipi_base.c,v 1.9 1998/09/14 05:49:21 scottr Exp $	*/

/*-
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Charles M. Hannum.
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

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/buf.h>
#include <sys/uio.h>
#include <sys/malloc.h>
#include <sys/pool.h>
#include <sys/errno.h>
#include <sys/device.h>
#include <sys/proc.h>

#include <dev/scsipi/scsipi_all.h>
#include <dev/scsipi/scsi_all.h>
#include <dev/scsipi/scsipi_disk.h>
#include <dev/scsipi/scsipiconf.h>
#include <dev/scsipi/scsipi_base.h>

struct pool scsipi_xfer_pool;

int	sc_err1 __P((struct scsipi_xfer *, int));

/*
 * Called when a scsibus is attached to initialize global data.
 */
void
scsipi_init()
{
	static int scsipi_init_done;

	if (scsipi_init_done)
		return;
	scsipi_init_done = 1;

	/* Initialize the scsipi_xfer pool. */
	pool_init(&scsipi_xfer_pool, sizeof(struct scsipi_xfer), 0,
	    0, 0, "scxspl", 0, NULL, NULL, M_DEVBUF);
}

/*
 * Get a scsipi transfer structure for the caller. Charge the structure
 * to the device that is referenced by the sc_link structure. If the
 * sc_link structure has no 'credits' then the device already has the
 * maximum number or outstanding operations under way. In this stage,
 * wait on the structure so that when one is freed, we are awoken again
 * If the SCSI_NOSLEEP flag is set, then do not wait, but rather, return
 * a NULL pointer, signifying that no slots were available
 * Note in the link structure, that we are waiting on it.
 */

struct scsipi_xfer *
scsipi_get_xs(sc_link, flags)
	struct scsipi_link *sc_link;	/* who to charge the xs to */
	int flags;			/* if this call can sleep */
{
	struct scsipi_xfer *xs;
	int s;

	SC_DEBUG(sc_link, SDEV_DB3, ("scsipi_get_xs\n"));

	s = splbio();
	while (sc_link->openings <= 0) {
		SC_DEBUG(sc_link, SDEV_DB3, ("sleeping\n"));
		if ((flags & SCSI_NOSLEEP) != 0) {
			splx(s);
			return (0);
		}
		sc_link->flags |= SDEV_WAITING;
		(void)tsleep(sc_link, PRIBIO, "getxs", 0);
	}
	SC_DEBUG(sc_link, SDEV_DB3, ("calling pool_get\n"));
	xs = pool_get(&scsipi_xfer_pool,
	    ((flags & SCSI_NOSLEEP) != 0 ? PR_NOWAIT : PR_WAITOK));
	if (xs != NULL)
		sc_link->openings--;
	else {
		(*sc_link->sc_print_addr)(sc_link);
		printf("cannot allocate scsipi xs\n");
	}
	splx(s);

	SC_DEBUG(sc_link, SDEV_DB3, ("returning\n"));

	/*
	 * zeroes out the command, as ATAPI may use longer commands
	 * than SCSI
	 */
	if (xs != NULL) {
		xs->flags = INUSE | flags;
		bzero(&xs->cmdstore, sizeof(xs->cmdstore));
	}
	return (xs);
}

/*
 * Given a scsipi_xfer struct, and a device (referenced through sc_link)
 * return the struct to the free pool and credit the device with it
 * If another process is waiting for an xs, do a wakeup, let it proceed
 *
 * MUST BE CALLED AT splbio()!!
 */
void
scsipi_free_xs(xs, flags)
	struct scsipi_xfer *xs;
	int flags;
{
	struct scsipi_link *sc_link = xs->sc_link;

	xs->flags &= ~INUSE;
	pool_put(&scsipi_xfer_pool, xs);

	SC_DEBUG(sc_link, SDEV_DB3, ("scsipi_free_xs\n"));
	/* if was 0 and someone waits, wake them up */
	sc_link->openings++;
	if ((sc_link->flags & SDEV_WAITING) != 0) {
		sc_link->flags &= ~SDEV_WAITING;
		wakeup(sc_link);
	} else {
		if (sc_link->device->start) {
			SC_DEBUG(sc_link, SDEV_DB2,
			    ("calling private start()\n"));
			(*(sc_link->device->start))(sc_link->device_softc);
		}
	}
}

/*
 * Find out from the device what its capacity is.
 */
u_long
scsipi_size(sc_link, flags)
	struct scsipi_link *sc_link;
	int flags;
{
	struct scsipi_read_cap_data rdcap;
	struct scsipi_read_capacity scsipi_cmd;

	/*
	 * make up a scsipi command and ask the scsipi driver to do
	 * it for you.
	 */
	bzero(&scsipi_cmd, sizeof(scsipi_cmd));
	scsipi_cmd.opcode = READ_CAPACITY;

	/*
	 * If the command works, interpret the result as a 4 byte
	 * number of blocks
	 */
	if (scsipi_command(sc_link, (struct scsipi_generic *)&scsipi_cmd,
	    sizeof(scsipi_cmd), (u_char *)&rdcap, sizeof(rdcap),
	    2, 20000, NULL, flags | SCSI_DATA_IN) != 0) {
		sc_link->sc_print_addr(sc_link);
		printf("could not get size\n");
		return (0);
	}

	return (_4btol(rdcap.addr) + 1);
}

/*
 * Get scsipi driver to send a "are you ready?" command
 */
int
scsipi_test_unit_ready(sc_link, flags)
	struct scsipi_link *sc_link;
	int flags;
{
	struct scsipi_test_unit_ready scsipi_cmd;

	/* some ATAPI drives don't support TEST_UNIT_READY. Sigh */
	if (sc_link->quirks & ADEV_NOTUR)
		return (0);

	bzero(&scsipi_cmd, sizeof(scsipi_cmd));
	scsipi_cmd.opcode = TEST_UNIT_READY;

	return (scsipi_command(sc_link,
	    (struct scsipi_generic *)&scsipi_cmd, sizeof(scsipi_cmd),
	    0, 0, 2, 10000, NULL, flags));
}

/*
 * Do a scsipi operation asking a device what it is
 * Use the scsipi_cmd routine in the switch table.
 * XXX actually this is only used for scsi devices, because I have the feeling
 * that some atapi CDROM may not implement it, althouh it marked as mandatory
 * in the atapi specs.
 */
int
scsipi_inquire(sc_link, inqbuf, flags)
	struct scsipi_link *sc_link;
	struct scsipi_inquiry_data *inqbuf;
	int flags;
{
	struct scsipi_inquiry scsipi_cmd;

	bzero(&scsipi_cmd, sizeof(scsipi_cmd));
	scsipi_cmd.opcode = INQUIRY;
	scsipi_cmd.length = sizeof(struct scsipi_inquiry_data);

	return (scsipi_command(sc_link,
	    (struct scsipi_generic *) &scsipi_cmd, sizeof(scsipi_cmd),
	    (u_char *) inqbuf, sizeof(struct scsipi_inquiry_data),
	    2, 10000, NULL, SCSI_DATA_IN | flags));
}

/*
 * Prevent or allow the user to remove the media
 */
int
scsipi_prevent(sc_link, type, flags)
	struct scsipi_link *sc_link;
	int type, flags;
{
	struct scsipi_prevent scsipi_cmd;

	if (sc_link->quirks & ADEV_NODOORLOCK)
		return (0);

	bzero(&scsipi_cmd, sizeof(scsipi_cmd));
	scsipi_cmd.opcode = PREVENT_ALLOW;
	scsipi_cmd.how = type;
	return (scsipi_command(sc_link,
	    (struct scsipi_generic *) &scsipi_cmd, sizeof(scsipi_cmd),
	    0, 0, 2, 5000, NULL, flags));
}

/*
 * Get scsipi driver to send a "start up" command
 */
int
scsipi_start(sc_link, type, flags)
	struct scsipi_link *sc_link;
	int type, flags;
{
	struct scsipi_start_stop scsipi_cmd;

	bzero(&scsipi_cmd, sizeof(scsipi_cmd));
	scsipi_cmd.opcode = START_STOP;
	scsipi_cmd.byte2 = 0x00;
	scsipi_cmd.how = type;
	return (scsipi_command(sc_link,
	    (struct scsipi_generic *) &scsipi_cmd, sizeof(scsipi_cmd),
	    0, 0, 2, type == SSS_START ? 30000 : 10000, NULL, flags));
}

/*
 * This routine is called by the scsipi interrupt when the transfer is
 * complete.
 */
void
scsipi_done(xs)
	struct scsipi_xfer *xs;
{
	struct scsipi_link *sc_link = xs->sc_link;
	struct buf *bp;
	int error;

	SC_DEBUG(sc_link, SDEV_DB2, ("scsipi_done\n"));
#ifdef	SCSIDEBUG
	if ((sc_link->flags & SDEV_DB1) != 0)
		show_scsipi_cmd(xs);
#endif /* SCSIDEBUG */

	/*
	 * If it's a user level request, bypass all usual completion
	 * processing, let the user work it out..  We take
	 * reponsibility for freeing the xs when the user returns.
	 * (and restarting the device's queue).
	 */
	if ((xs->flags & SCSI_USER) != 0) {
		SC_DEBUG(sc_link, SDEV_DB3, ("calling user done()\n"));
		scsipi_user_done(xs); /* to take a copy of the sense etc. */
		SC_DEBUG(sc_link, SDEV_DB3, ("returned from user done()\n "));

		/*
		 * If this was an asynchronous operation (i.e. adapter
		 * returned SUCCESSFULLY_QUEUED when the command was
		 * submitted), we need to free the scsipi_xfer here.
		 */
		if (xs->flags & SCSI_ASYNCREQ)
			scsipi_free_xs(xs, SCSI_NOSLEEP);
		SC_DEBUG(sc_link, SDEV_DB3, ("returning to adapter\n"));
		return;
	}

	if (!((xs->flags & (SCSI_NOSLEEP | SCSI_POLL)) == SCSI_NOSLEEP)) {
		/*
		 * if it's a normal upper level request, then ask
		 * the upper level code to handle error checking
		 * rather than doing it here at interrupt time
		 */
		wakeup(xs);
		return;
	}

	/*
	 * Go and handle errors now.
	 * If it returns ERESTART then we should RETRY
	 */
retry:
	error = sc_err1(xs, 1);
	if (error == ERESTART)
		switch (scsipi_command_direct(xs)) {
		case SUCCESSFULLY_QUEUED:
			return;

		case TRY_AGAIN_LATER:
			xs->error = XS_BUSY;
		case COMPLETE:
			goto retry;
		}

	bp = xs->bp;
	if (bp) {
		if (error) {
			bp->b_error = error;
			bp->b_flags |= B_ERROR;
			bp->b_resid = bp->b_bcount;
		} else {
			bp->b_error = 0;
			bp->b_resid = xs->resid;
		}
	}
	if (sc_link->device->done) {
		/*
		 * Tell the device the operation is actually complete.
		 * No more will happen with this xfer.  This for
		 * notification of the upper-level driver only; they
		 * won't be returning any meaningful information to us.
		 */
		(*sc_link->device->done)(xs);
	}
	/*
	 * If this was an asynchronous operation (i.e. adapter
	 * returned SUCCESSFULLY_QUEUED when the command was
	 * submitted), we need to free the scsipi_xfer here.
	 */
	if (xs->flags & SCSI_ASYNCREQ)
		scsipi_free_xs(xs, SCSI_NOSLEEP);
	if (bp)
		biodone(bp);
}

int
scsipi_execute_xs(xs)
	struct scsipi_xfer *xs;
{
	int error;
	int s;

	xs->flags &= ~(ITSDONE|SCSI_ASYNCREQ);
	xs->error = XS_NOERROR;
	xs->resid = xs->datalen;
	xs->status = 0;

retry:
	/*
	 * Do the transfer. If we are polling we will return:
	 * COMPLETE,  Was poll, and scsipi_done has been called
	 * TRY_AGAIN_LATER, Adapter short resources, try again
	 *
	 * if under full steam (interrupts) it will return:
	 * SUCCESSFULLY_QUEUED, will do a wakeup when complete
	 * TRY_AGAIN_LATER, (as for polling)
	 * After the wakeup, we must still check if it succeeded
	 *
	 * If we have a SCSI_NOSLEEP (typically because we have a buf)
	 * we just return.  All the error proccessing and the buffer
	 * code both expect us to return straight to them, so as soon
	 * as the command is queued, return.
	 */
#ifdef SCSIDEBUG
	if (xs->sc_link->flags & SDEV_DB3) {
		printf("scsipi_exec_cmd: ");
		show_scsipi_xs(xs);
		printf("\n");
	}
#endif
	switch (scsipi_command_direct(xs)) {
	case SUCCESSFULLY_QUEUED:
		if ((xs->flags & (SCSI_NOSLEEP | SCSI_POLL)) == SCSI_NOSLEEP) {
			/*
			 * The request will complete asynchronously.  In this
			 * case, we need scsipi_done() to free the scsipi_xfer.
			 */
			xs->flags |= SCSI_ASYNCREQ;
			return (EJUSTRETURN);
		}
#ifdef DIAGNOSTIC
		if (xs->flags & SCSI_NOSLEEP)
			panic("scsipi_execute_xs: NOSLEEP and POLL");
#endif
		s = splbio();
		while ((xs->flags & ITSDONE) == 0)
			tsleep(xs, PRIBIO + 1, "scsipi_cmd", 0);
		splx(s);
	case COMPLETE:		/* Polling command completed ok */
		if (xs->bp)
			return (0);
	doit:
		SC_DEBUG(xs->sc_link, SDEV_DB3, ("back in cmd()\n"));
		if ((error = sc_err1(xs, 0)) != ERESTART)
			return (error);
		goto retry;

	case TRY_AGAIN_LATER:	/* adapter resource shortage */
		xs->error = XS_BUSY;
		goto doit;

	default:
		panic("scsipi_execute_xs: invalid return code");
	}

#ifdef DIAGNOSTIC
	panic("scsipi_execute_xs: impossible");
#endif
	return (EINVAL);
}

int
sc_err1(xs, async)
	struct scsipi_xfer *xs;
	int async;
{
	int error;

	SC_DEBUG(xs->sc_link, SDEV_DB3, ("sc_err1,err = 0x%x \n", xs->error));

	/*
	 * If it has a buf, we might be working with
	 * a request from the buffer cache or some other
	 * piece of code that requires us to process
	 * errors at inetrrupt time. We have probably
	 * been called by scsipi_done()
	 */
	switch (xs->error) {
	case XS_NOERROR:	/* nearly always hit this one */
		error = 0;
		break;

	case XS_SENSE:
		if ((error = (*xs->sc_link->scsipi_interpret_sense)(xs)) ==
		    ERESTART)
			goto retry;
		SC_DEBUG(xs->sc_link, SDEV_DB3,
		    ("scsipi_interpret_sense returned %d\n", error));
		break;

	case XS_BUSY:
		if (xs->retries) {
			if ((xs->flags & SCSI_POLL) != 0)
				delay(1000000);
			else if ((xs->flags & SCSI_NOSLEEP) == 0)
				tsleep(&lbolt, PRIBIO, "scbusy", 0);
			else
#if 0
				timeout(scsipi_requeue, xs, hz);
#else
				goto lose;
#endif
		}
	case XS_TIMEOUT:
	retry:
		if (xs->retries--) {
			xs->error = XS_NOERROR;
			xs->flags &= ~ITSDONE;
			return (ERESTART);
		}
	case XS_DRIVER_STUFFUP:
	lose:
		error = EIO;
		break;

	case XS_SELTIMEOUT:
		/* XXX Disable device? */
		error = EIO;
		break;

	default:
		(*xs->sc_link->sc_print_addr)(xs->sc_link);
		printf("unknown error category from scsipi driver\n");
		error = EIO;
		break;
	}

	return (error);
}

#ifdef	SCSIDEBUG
/*
 * Given a scsipi_xfer, dump the request, in all it's glory
 */
void
show_scsipi_xs(xs)
	struct scsipi_xfer *xs;
{

	printf("xs(%p): ", xs);
	printf("flg(0x%x)", xs->flags);
	printf("sc_link(%p)", xs->sc_link);
	printf("retr(0x%x)", xs->retries);
	printf("timo(0x%x)", xs->timeout);
	printf("cmd(%p)", xs->cmd);
	printf("len(0x%x)", xs->cmdlen);
	printf("data(%p)", xs->data);
	printf("len(0x%x)", xs->datalen);
	printf("res(0x%x)", xs->resid);
	printf("err(0x%x)", xs->error);
	printf("bp(%p)", xs->bp);
	show_scsipi_cmd(xs);
}

void
show_scsipi_cmd(xs)
	struct scsipi_xfer *xs;
{
	u_char *b = (u_char *) xs->cmd;
	int i = 0;

	(*xs->sc_link->sc_print_addr)(xs->sc_link);
	printf("command: ");

	if ((xs->flags & SCSI_RESET) == 0) {
		while (i < xs->cmdlen) {
			if (i)
				printf(",");
			printf("0x%x", b[i++]);
		}
		printf("-[%d bytes]\n", xs->datalen);
		if (xs->datalen)
			show_mem(xs->data, min(64, xs->datalen));
	} else
		printf("-RESET-\n");
}

void
show_mem(address, num)
	u_char *address;
	int num;
{
	int x;

	printf("------------------------------");
	for (x = 0; x < num; x++) {
		if ((x % 16) == 0)
			printf("\n%03d: ", x);
		printf("%02x ", *address++);
	}
	printf("\n------------------------------\n");
}
#endif /*SCSIDEBUG */

