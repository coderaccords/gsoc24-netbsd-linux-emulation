/*	$NetBSD: bha.c,v 1.10 1997/03/13 00:38:51 cgd Exp $	*/

#undef BHADIAG
#ifdef DDB
#define	integrate
#else
#define	integrate	static inline
#endif

/*
 * Copyright (c) 1994, 1996 Charles M. Hannum.  All rights reserved.
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
 *	This product includes software developed by Charles M. Hannum.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Originally written by Julian Elischer (julian@tfs.com)
 * for TRW Financial Systems for use under the MACH(2.5) operating system.
 *
 * TRW Financial Systems, in accordance with their agreement with Carnegie
 * Mellon University, makes this software available to CMU to distribute
 * or use in any manner that they see fit as long as this message is kept with
 * the software. For this reason TFS also grants any other persons or
 * organisations permission to use or modify this software.
 *
 * TFS supplies this software to be publicly redistributed
 * on the understanding that TFS is not responsible for the correct
 * functioning of this software in any circumstances.
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/errno.h>
#include <sys/ioctl.h>
#include <sys/device.h>
#include <sys/malloc.h>
#include <sys/buf.h>
#include <sys/proc.h>
#include <sys/user.h>

#include <machine/bus.h>
#include <machine/intr.h>

#include <scsi/scsi_all.h>
#include <scsi/scsiconf.h>

#include <dev/ic/bhareg.h>
#include <dev/ic/bhavar.h>

#ifndef DDB
#define Debugger() panic("should call debugger here (bha.c)")
#endif /* ! DDB */

#ifdef alpha		/* XXX */
/* XXX XXX NEED REAL DMA MAPPING SUPPORT XXX XXX */ 
extern vm_offset_t alpha_XXX_dmamap(vm_offset_t);
#undef vtophys
#define	vtophys(va)	alpha_XXX_dmamap((vm_offset_t) va)
#endif	/* alpha */

#define KVTOPHYS(x)	vtophys(x)

#ifdef BHADEBUG
int     bha_debug = 0;
#endif /* BHADEBUG */

int bha_cmd __P((bus_space_tag_t, bus_space_handle_t, struct bha_softc *,
		 int, u_char *, int, u_char *));
integrate void bha_finish_ccbs __P((struct bha_softc *));
integrate void bha_reset_ccb __P((struct bha_softc *, struct bha_ccb *));
void bha_free_ccb __P((struct bha_softc *, struct bha_ccb *));
integrate void bha_init_ccb __P((struct bha_softc *, struct bha_ccb *));
struct bha_ccb *bha_get_ccb __P((struct bha_softc *, int));
struct bha_ccb *bha_ccb_phys_kv __P((struct bha_softc *, u_long));
void bha_queue_ccb __P((struct bha_softc *, struct bha_ccb *));
void bha_collect_mbo __P((struct bha_softc *));
void bha_start_ccbs __P((struct bha_softc *));
void bha_done __P((struct bha_softc *, struct bha_ccb *));
void bha_init __P((struct bha_softc *));
void bha_inquire_setup_information __P((struct bha_softc *));
void bhaminphys __P((struct buf *));
int bha_scsi_cmd __P((struct scsi_xfer *));
int bha_poll __P((struct bha_softc *, struct scsi_xfer *, int));
void bha_timeout __P((void *arg));

struct scsi_adapter bha_switch = {
	bha_scsi_cmd,
	bhaminphys,
	0,
	0,
};

/* the below structure is so we have a default dev struct for out link struct */
struct scsi_device bha_dev = {
	NULL,			/* Use default error handler */
	NULL,			/* have a queue, served by this */
	NULL,			/* have no async handler */
	NULL,			/* Use default 'done' routine */
};

struct cfdriver bha_cd = {
	NULL, "bha", DV_DULL
};

#define BHA_RESET_TIMEOUT	2000	/* time to wait for reset (mSec) */
#define	BHA_ABORT_TIMEOUT	2000	/* time to wait for abort (mSec) */

/*
 * bha_cmd(iot, ioh, sc, icnt, ibuf, ocnt, obuf)
 *
 * Activate Adapter command
 *    icnt:   number of args (outbound bytes including opcode)
 *    ibuf:   argument buffer
 *    ocnt:   number of expected returned bytes
 *    obuf:   result buffer
 *    wait:   number of seconds to wait for response
 *
 * Performs an adapter command through the ports.  Not to be confused with a
 * scsi command, which is read in via the dma; one of the adapter commands
 * tells it to read in a scsi command.
 */
int
bha_cmd(iot, ioh, sc, icnt, ibuf, ocnt, obuf)
	bus_space_tag_t iot;
	bus_space_handle_t ioh;
	struct bha_softc *sc;
	int icnt, ocnt;
	u_char *ibuf, *obuf;
{
	const char *name;
	register int i;
	int wait;
	u_char sts;
	u_char opcode = ibuf[0];

	if (sc != NULL)
		name = sc->sc_dev.dv_xname;
	else
		name = "(bha probe)";

	/*
	 * Calculate a reasonable timeout for the command.
	 */
	switch (opcode) {
	case BHA_INQUIRE_DEVICES:
	case BHA_INQUIRE_DEVICES_2:
		wait = 90 * 20000;
		break;
	default:
		wait = 1 * 20000;
		break;
	}

	/*
	 * Wait for the adapter to go idle, unless it's one of
	 * the commands which don't need this
	 */
	if (opcode != BHA_MBO_INTR_EN) {
		for (i = 20000; i; i--) {	/* 1 sec? */
			sts = bus_space_read_1(iot, ioh, BHA_STAT_PORT);
			if (sts & BHA_STAT_IDLE)
				break;
			delay(50);
		}
		if (!i) {
			printf("%s: bha_cmd, host not idle(0x%x)\n",
			    name, sts);
			return (1);
		}
	}
	/*
	 * Now that it is idle, if we expect output, preflush the
	 * queue feeding to us.
	 */
	if (ocnt) {
		while ((bus_space_read_1(iot, ioh, BHA_STAT_PORT)) &
		    BHA_STAT_DF)
			bus_space_read_1(iot, ioh, BHA_DATA_PORT);
	}
	/*
	 * Output the command and the number of arguments given
	 * for each byte, first check the port is empty.
	 */
	while (icnt--) {
		for (i = wait; i; i--) {
			sts = bus_space_read_1(iot, ioh, BHA_STAT_PORT);
			if (!(sts & BHA_STAT_CDF))
				break;
			delay(50);
		}
		if (!i) {
			if (opcode != BHA_INQUIRE_REVISION)
				printf("%s: bha_cmd, cmd/data port full\n",
				    name);
			goto bad;
		}
		bus_space_write_1(iot, ioh, BHA_CMD_PORT, *ibuf++);
	}
	/*
	 * If we expect input, loop that many times, each time,
	 * looking for the data register to have valid data
	 */
	while (ocnt--) {
		for (i = wait; i; i--) {
			sts = bus_space_read_1(iot, ioh, BHA_STAT_PORT);
			if (sts & BHA_STAT_DF)
				break;
			delay(50);
		}
		if (!i) {
			if (opcode != BHA_INQUIRE_REVISION)
				printf("%s: bha_cmd, cmd/data port empty %d\n",
				    name, ocnt);
			goto bad;
		}
		*obuf++ = bus_space_read_1(iot, ioh, BHA_DATA_PORT);
	}
	/*
	 * Wait for the board to report a finished instruction.
	 * We may get an extra interrupt for the HACC signal, but this is
	 * unimportant.
	 */
	if (opcode != BHA_MBO_INTR_EN && opcode != BHA_MODIFY_IOPORT) {
		for (i = 20000; i; i--) {	/* 1 sec? */
			sts = bus_space_read_1(iot, ioh, BHA_INTR_PORT);
			/* XXX Need to save this in the interrupt handler? */
			if (sts & BHA_INTR_HACC)
				break;
			delay(50);
		}
		if (!i) {
			printf("%s: bha_cmd, host not finished(0x%x)\n",
			    name, sts);
			return (1);
		}
	}
	bus_space_write_1(iot, ioh, BHA_CTRL_PORT, BHA_CTRL_IRST);
	return (0);

bad:
	bus_space_write_1(iot, ioh, BHA_CTRL_PORT, BHA_CTRL_SRST);
	return (1);
}

/*
 * Attach all the sub-devices we can find
 */
void
bha_attach(sc)
	struct bha_softc *sc;
{

	bha_inquire_setup_information(sc);
	bha_init(sc);
	TAILQ_INIT(&sc->sc_free_ccb);
	TAILQ_INIT(&sc->sc_waiting_ccb);

	/*
	 * fill in the prototype scsi_link.
	 */
	sc->sc_link.channel = SCSI_CHANNEL_ONLY_ONE;
	sc->sc_link.adapter_softc = sc;
	sc->sc_link.adapter_target = sc->sc_scsi_dev;
	sc->sc_link.adapter = &bha_switch;
	sc->sc_link.device = &bha_dev;
	sc->sc_link.openings = 4;
	sc->sc_link.max_target = sc->sc_iswide ? 15 : 7;

	/*
	 * ask the adapter what subunits are present
	 */
	config_found(&sc->sc_dev, &sc->sc_link, scsiprint);
}

integrate void
bha_finish_ccbs(sc)
	struct bha_softc *sc;
{
	struct bha_mbx_in *wmbi;
	struct bha_ccb *ccb;
	int i;

	wmbi = wmbx->tmbi;

	if (wmbi->stat == BHA_MBI_FREE) {
		for (i = 0; i < BHA_MBX_SIZE; i++) {
			if (wmbi->stat != BHA_MBI_FREE) {
				printf("%s: mbi not in round-robin order\n",
				    sc->sc_dev.dv_xname);
				goto AGAIN;
			}
			bha_nextmbx(wmbi, wmbx, mbi);
		}
#ifdef BHADIAGnot
		printf("%s: mbi interrupt with no full mailboxes\n",
		    sc->sc_dev.dv_xname);
#endif
		return;
	}

AGAIN:
	do {
		ccb = bha_ccb_phys_kv(sc, phystol(wmbi->ccb_addr));
		if (!ccb) {
			printf("%s: bad mbi ccb pointer; skipping\n",
			    sc->sc_dev.dv_xname);
			goto next;
		}

#ifdef BHADEBUG
		if (bha_debug) {
			u_char *cp = &ccb->scsi_cmd;
			printf("op=%x %x %x %x %x %x\n",
			    cp[0], cp[1], cp[2], cp[3], cp[4], cp[5]);
			printf("stat %x for mbi addr = 0x%08x, ",
			    wmbi->stat, wmbi);
			printf("ccb addr = 0x%x\n", ccb);
		}
#endif /* BHADEBUG */

		switch (wmbi->stat) {
		case BHA_MBI_OK:
		case BHA_MBI_ERROR:
			if ((ccb->flags & CCB_ABORT) != 0) {
				/*
				 * If we already started an abort, wait for it
				 * to complete before clearing the CCB.  We
				 * could instead just clear CCB_SENDING, but
				 * what if the mailbox was already received?
				 * The worst that happens here is that we clear
				 * the CCB a bit later than we need to.  BFD.
				 */
				goto next;
			}
			break;

		case BHA_MBI_ABORT:
		case BHA_MBI_UNKNOWN:
			/*
			 * Even if the CCB wasn't found, we clear it anyway.
			 * See preceeding comment.
			 */
			break;

		default:
			printf("%s: bad mbi status %02x; skipping\n",
			    sc->sc_dev.dv_xname, wmbi->stat);
			goto next;
		}

		untimeout(bha_timeout, ccb);
		bha_done(sc, ccb);

	next:
		wmbi->stat = BHA_MBI_FREE;
		bha_nextmbx(wmbi, wmbx, mbi);
	} while (wmbi->stat != BHA_MBI_FREE);

	wmbx->tmbi = wmbi;
}

/*
 * Catch an interrupt from the adaptor
 */
int
bha_intr(arg)
	void *arg;
{
	struct bha_softc *sc = arg;
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;
	u_char sts;

#ifdef BHADEBUG
	printf("%s: bha_intr ", sc->sc_dev.dv_xname);
#endif /* BHADEBUG */

	/*
	 * First acknowlege the interrupt, Then if it's not telling about
	 * a completed operation just return.
	 */
	sts = bus_space_read_1(iot, ioh, BHA_INTR_PORT);
	if ((sts & BHA_INTR_ANYINTR) == 0)
		return (0);
	bus_space_write_1(iot, ioh, BHA_CTRL_PORT, BHA_CTRL_IRST);

#ifdef BHADIAG
	/* Make sure we clear CCB_SENDING before finishing a CCB. */
	bha_collect_mbo(sc);
#endif

	/* Mail box out empty? */
	if (sts & BHA_INTR_MBOA) {
		struct bha_toggle toggle;

		toggle.cmd.opcode = BHA_MBO_INTR_EN;
		toggle.cmd.enable = 0;
		bha_cmd(iot, ioh, sc,
		    sizeof(toggle.cmd), (u_char *)&toggle.cmd,
		    0, (u_char *)0);
		bha_start_ccbs(sc);
	}

	/* Mail box in full? */
	if (sts & BHA_INTR_MBIF)
		bha_finish_ccbs(sc);

	return (1);
}

integrate void
bha_reset_ccb(sc, ccb)
	struct bha_softc *sc;
	struct bha_ccb *ccb;
{

	ccb->flags = 0;
}

/*
 * A ccb is put onto the free list.
 */
void
bha_free_ccb(sc, ccb)
	struct bha_softc *sc;
	struct bha_ccb *ccb;
{
	int s;

	s = splbio();

	bha_reset_ccb(sc, ccb);
	TAILQ_INSERT_HEAD(&sc->sc_free_ccb, ccb, chain);

	/*
	 * If there were none, wake anybody waiting for one to come free,
	 * starting with queued entries.
	 */
	if (ccb->chain.tqe_next == 0)
		wakeup(&sc->sc_free_ccb);

	splx(s);
}

integrate void
bha_init_ccb(sc, ccb)
	struct bha_softc *sc;
	struct bha_ccb *ccb;
{
	int hashnum;

	bzero(ccb, sizeof(struct bha_ccb));
	/*
	 * put in the phystokv hash table
	 * Never gets taken out.
	 */
	ccb->hashkey = KVTOPHYS(ccb);
	hashnum = CCB_HASH(ccb->hashkey);
	ccb->nexthash = sc->sc_ccbhash[hashnum];
	sc->sc_ccbhash[hashnum] = ccb;
	bha_reset_ccb(sc, ccb);
}

/*
 * Get a free ccb
 *
 * If there are none, see if we can allocate a new one.  If so, put it in
 * the hash table too otherwise either return an error or sleep.
 */
struct bha_ccb *
bha_get_ccb(sc, flags)
	struct bha_softc *sc;
	int flags;
{
	struct bha_ccb *ccb;
	int s;

	s = splbio();

	/*
	 * If we can and have to, sleep waiting for one to come free
	 * but only if we can't allocate a new one.
	 */
	for (;;) {
		ccb = sc->sc_free_ccb.tqh_first;
		if (ccb) {
			TAILQ_REMOVE(&sc->sc_free_ccb, ccb, chain);
			break;
		}
		if (sc->sc_numccbs < BHA_CCB_MAX) {
			ccb = (struct bha_ccb *) malloc(sizeof(struct bha_ccb),
			    M_TEMP, M_NOWAIT);
			if (!ccb) {
				printf("%s: can't malloc ccb\n",
				    sc->sc_dev.dv_xname);
				goto out;
			}
			bha_init_ccb(sc, ccb);
			sc->sc_numccbs++;
			break;
		}
		if ((flags & SCSI_NOSLEEP) != 0)
			goto out;
		tsleep(&sc->sc_free_ccb, PRIBIO, "bhaccb", 0);
	}

	ccb->flags |= CCB_ALLOC;

out:
	splx(s);
	return (ccb);
}

/*
 * Given a physical address, find the ccb that it corresponds to.
 */
struct bha_ccb *
bha_ccb_phys_kv(sc, ccb_phys)
	struct bha_softc *sc;
	u_long ccb_phys;
{
	int hashnum = CCB_HASH(ccb_phys);
	struct bha_ccb *ccb = sc->sc_ccbhash[hashnum];

	while (ccb) {
		if (ccb->hashkey == ccb_phys)
			break;
		ccb = ccb->nexthash;
	}
	return (ccb);
}

/*
 * Queue a CCB to be sent to the controller, and send it if possible.
 */
void
bha_queue_ccb(sc, ccb)
	struct bha_softc *sc;
	struct bha_ccb *ccb;
{

	TAILQ_INSERT_TAIL(&sc->sc_waiting_ccb, ccb, chain);
	bha_start_ccbs(sc);
}

/*
 * Garbage collect mailboxes that are no longer in use.
 */
void
bha_collect_mbo(sc)
	struct bha_softc *sc;
{
	struct bha_mbx_out *wmbo;	/* Mail Box Out pointer */
#ifdef BHADIAG
	struct bha_ccb *ccb;
#endif

	wmbo = wmbx->cmbo;

	while (sc->sc_mbofull > 0) {
		if (wmbo->cmd != BHA_MBO_FREE)
			break;

#ifdef BHADIAG
		ccb = bha_ccb_phys_kv(sc, phystol(wmbo->ccb_addr));
		ccb->flags &= ~CCB_SENDING;
#endif

		--sc->sc_mbofull;
		bha_nextmbx(wmbo, wmbx, mbo);
	}

	wmbx->cmbo = wmbo;
}

/*
 * Send as many CCBs as we have empty mailboxes for.
 */
void
bha_start_ccbs(sc)
	struct bha_softc *sc;
{
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;
	struct bha_mbx_out *wmbo;	/* Mail Box Out pointer */
	struct bha_ccb *ccb;

	wmbo = wmbx->tmbo;

	while ((ccb = sc->sc_waiting_ccb.tqh_first) != NULL) {
		if (sc->sc_mbofull >= BHA_MBX_SIZE) {
			bha_collect_mbo(sc);
			if (sc->sc_mbofull >= BHA_MBX_SIZE) {
				struct bha_toggle toggle;

				toggle.cmd.opcode = BHA_MBO_INTR_EN;
				toggle.cmd.enable = 1;
				bha_cmd(iot, ioh, sc,
				    sizeof(toggle.cmd), (u_char *)&toggle.cmd,
				    0, (u_char *)0);
				break;
			}
		}

		TAILQ_REMOVE(&sc->sc_waiting_ccb, ccb, chain);
#ifdef BHADIAG
		ccb->flags |= CCB_SENDING;
#endif

		/* Link ccb to mbo. */
		ltophys(KVTOPHYS(ccb), wmbo->ccb_addr);
		if (ccb->flags & CCB_ABORT)
			wmbo->cmd = BHA_MBO_ABORT;
		else
			wmbo->cmd = BHA_MBO_START;

		/* Tell the card to poll immediately. */
		bus_space_write_1(iot, ioh, BHA_CMD_PORT, BHA_START_SCSI);

		if ((ccb->xs->flags & SCSI_POLL) == 0)
			timeout(bha_timeout, ccb, (ccb->timeout * hz) / 1000);

		++sc->sc_mbofull;
		bha_nextmbx(wmbo, wmbx, mbo);
	}

	wmbx->tmbo = wmbo;
}

/*
 * We have a ccb which has been processed by the
 * adaptor, now we look to see how the operation
 * went. Wake up the owner if waiting
 */
void
bha_done(sc, ccb)
	struct bha_softc *sc;
	struct bha_ccb *ccb;
{
	struct scsi_sense_data *s1, *s2;
	struct scsi_xfer *xs = ccb->xs;

	SC_DEBUG(xs->sc_link, SDEV_DB2, ("bha_done\n"));
	/*
	 * Otherwise, put the results of the operation
	 * into the xfer and call whoever started it
	 */
#ifdef BHADIAG
	if (ccb->flags & CCB_SENDING) {
		printf("%s: exiting ccb still in transit!\n",
		    sc->sc_dev.dv_xname);
		Debugger();
		return;
	}
#endif
	if ((ccb->flags & CCB_ALLOC) == 0) {
		printf("%s: exiting ccb not allocated!\n",
		    sc->sc_dev.dv_xname);
		Debugger();
		return;
	}
	if (xs->error == XS_NOERROR) {
		if (ccb->host_stat != BHA_OK) {
			switch (ccb->host_stat) {
			case BHA_SEL_TIMEOUT:	/* No response */
				xs->error = XS_SELTIMEOUT;
				break;
			default:	/* Other scsi protocol messes */
				printf("%s: host_stat %x\n",
				    sc->sc_dev.dv_xname, ccb->host_stat);
				xs->error = XS_DRIVER_STUFFUP;
				break;
			}
		} else if (ccb->target_stat != SCSI_OK) {
			switch (ccb->target_stat) {
			case SCSI_CHECK:
				s1 = &ccb->scsi_sense;
				s2 = &xs->sense;
				*s2 = *s1;
				xs->error = XS_SENSE;
				break;
			case SCSI_BUSY:
				xs->error = XS_BUSY;
				break;
			default:
				printf("%s: target_stat %x\n",
				    sc->sc_dev.dv_xname, ccb->target_stat);
				xs->error = XS_DRIVER_STUFFUP;
				break;
			}
		} else
			xs->resid = 0;
	}
	bha_free_ccb(sc, ccb);
	xs->flags |= ITSDONE;
	scsi_done(xs);
}

/*
 * Find the board and find it's irq/drq
 */
int
bha_find(iot, ioh, sc)
	bus_space_tag_t iot;
	bus_space_handle_t ioh;
	struct bha_softc *sc;
{
	int i, iswide;
	u_char sts;
	struct bha_extended_inquire inquire;
	struct bha_config config;
	int irq, drq;

	/* Check something is at the ports we need to access */
	sts = bus_space_read_1(iot, ioh, BHA_STAT_PORT);
	if (sts == 0xFF)
		return (0);

	/*
	 * Reset board, If it doesn't respond, assume
	 * that it's not there.. good for the probe
	 */

	bus_space_write_1(iot, ioh, BHA_CTRL_PORT,
	    BHA_CTRL_HRST | BHA_CTRL_SRST);

	delay(100);
	for (i = BHA_RESET_TIMEOUT; i; i--) {
		sts = bus_space_read_1(iot, ioh, BHA_STAT_PORT);
		if (sts == (BHA_STAT_IDLE | BHA_STAT_INIT))
			break;
		delay(1000);
	}
	if (!i) {
#ifdef BHADEBUG
		if (bha_debug)
			printf("bha_find: No answer from buslogic board\n");
#endif /* BHADEBUG */
		return (0);
	}

	/*
	 * The BusLogic cards implement an Adaptec 1542 (aha)-compatible
	 * interface. The native bha interface is not compatible with 
	 * an aha. 1542. We need to ensure that we never match an
	 * Adaptec 1542. We must also avoid sending Adaptec-compatible
	 * commands to a real bha, lest it go into 1542 emulation mode.
	 * (On an indirect bus like ISA, we should always probe for BusLogic
	 * interfaces  before Adaptec interfaces).
	 */

	/*
	 * Make sure we don't match an AHA-1542A or AHA-1542B, by checking
	 * for an extended-geometry register.  The 1542[AB] don't have one.
	 */
	sts = bus_space_read_1(iot, ioh, BHA_EXTGEOM_PORT);
	if (sts == 0xFF)
		return (0);

	/*
	 * Check that we actually know how to use this board.
	 */
	delay(1000);
	inquire.cmd.opcode = BHA_INQUIRE_EXTENDED;
	inquire.cmd.len = sizeof(inquire.reply);
	i = bha_cmd(iot, ioh, sc,
	    sizeof(inquire.cmd), (u_char *)&inquire.cmd,
	    sizeof(inquire.reply), (u_char *)&inquire.reply);

	/*
	 * Some 1542Cs (CP, perhaps not CF, may depend on firmware rev)
	 * have the extended-geometry register and also respond to
	 * BHA_INQUIRE_EXTENDED.  Make sure we never  match such cards,
	 * by checking the size of the reply is what a BusLogic card returns.
	 */
	if (i) {
#ifdef BHADEBUG
		printf("bha_find: board returned %d instead of %d to %s\n",
		       i, sizeof(inquire.reply), "INQUIRE_EXTENDED");
#endif
		return (0);
	}

	/* OK, we know we've found a buslogic adaptor. */

	switch (inquire.reply.bus_type) {
	case BHA_BUS_TYPE_24BIT:
	case BHA_BUS_TYPE_32BIT:
		break;
	case BHA_BUS_TYPE_MCA:
		/* We don't grok MicroChannel (yet). */
		return (0);
	default:
		printf("bha_find: illegal bus type %c\n",
		    inquire.reply.bus_type);
		return (0);
	}

	/* Note if we have a wide bus. */
	iswide = inquire.reply.scsi_flags & BHA_SCSI_WIDE;

	/*
	 * Assume we have a board at this stage setup dma channel from
	 * jumpers and save int level
	 */
	delay(1000);
	config.cmd.opcode = BHA_INQUIRE_CONFIG;
	bha_cmd(iot, ioh, sc,
	    sizeof(config.cmd), (u_char *)&config.cmd,
	    sizeof(config.reply), (u_char *)&config.reply);
	switch (config.reply.chan) {
	case EISADMA:
		drq = -1;
		break;
	case CHAN0:
		drq = 0;
		break;
	case CHAN5:
		drq = 5;
		break;
	case CHAN6:
		drq = 6;
		break;
	case CHAN7:
		drq = 7;
		break;
	default:
		printf("bha_find: illegal drq setting %x\n",
		    config.reply.chan);
		return (0);
	}

	switch (config.reply.intr) {
	case INT9:
		irq = 9;
		break;
	case INT10:
		irq = 10;
		break;
	case INT11:
		irq = 11;
		break;
	case INT12:
		irq = 12;
		break;
	case INT14:
		irq = 14;
		break;
	case INT15:
		irq = 15;
		break;
	default:
		printf("bha_find: illegal irq setting %x\n",
		    config.reply.intr);
		return (0);
	}

	/* if we want to fill in softc, do so now */
	if (sc != NULL) {
		sc->sc_irq = irq;
		sc->sc_drq = drq;
		sc->sc_scsi_dev = config.reply.scsi_dev;
		sc->sc_iswide = iswide;
	}

	return (1);
}


/*
 * Disable the ISA-compatiblity ioports on  PCI bha devices,
 * to ensure they're not autoconfigured a second time as an ISA bha.
 */
int
bha_disable_isacompat(sc)
	struct bha_softc *sc;
{
	struct bha_isadisable isa_disable;

	isa_disable.cmd.opcode = BHA_MODIFY_IOPORT;
	isa_disable.cmd.modifier = BHA_IOMODIFY_DISABLE1;
	bha_cmd(sc->sc_iot, sc->sc_ioh, sc,
	    sizeof(isa_disable.cmd), (u_char*)&isa_disable.cmd,
	    0, (u_char *)0);
	return (0);
}


/*
 * Start the board, ready for normal operation
 */
void
bha_init(sc)
	struct bha_softc *sc;
{
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;
	struct bha_devices devices;
	struct bha_setup setup;
	struct bha_mailbox mailbox;
	struct bha_period period;
	int i, rlen;

	/* Enable round-robin scheme - appeared at firmware rev. 3.31. */
	if (strcmp(sc->sc_firmware, "3.31") >= 0) {
		struct bha_toggle toggle;

		toggle.cmd.opcode = BHA_ROUND_ROBIN;
		toggle.cmd.enable = 1;
		bha_cmd(iot, ioh, sc,
		    sizeof(toggle.cmd), (u_char *)&toggle.cmd,
		    0, (u_char *)0);
	}

	/*
	 * Inquire installed devices (to force synchronous negotiation).
	 */

	/*
	 * Poll targets 0 - 7.
	 */
	devices.cmd.opcode = BHA_INQUIRE_DEVICES;
	bha_cmd(iot, ioh, sc,
	    sizeof(devices.cmd), (u_char *)&devices.cmd,
	    sizeof(devices.reply), (u_char *)&devices.reply);

	/*
	 * Poll targets 8 - 15 if we have a wide bus.
	 */
	if (sc->sc_iswide) {
		devices.cmd.opcode = BHA_INQUIRE_DEVICES_2;
		bha_cmd(iot, ioh, sc,
		    sizeof(devices.cmd), (u_char *)&devices.cmd,
		    sizeof(devices.reply), (u_char *)&devices.reply);
	}

	/* Obtain setup information from. */
	rlen = sizeof(setup.reply) +
	    (sc->sc_iswide ? sizeof(setup.reply_w) : 0);
	setup.cmd.opcode = BHA_INQUIRE_SETUP;
	setup.cmd.len = rlen;
	bha_cmd(iot, ioh, sc,
	    sizeof(setup.cmd), (u_char *)&setup.cmd,
	    rlen, (u_char *)&setup.reply);

	printf("%s: %s, %s\n",
	    sc->sc_dev.dv_xname,
	    setup.reply.sync_neg ? "sync" : "async",
	    setup.reply.parity ? "parity" : "no parity");

	for (i = 0; i < 8; i++)
		period.reply.period[i] = setup.reply.sync[i].period * 5 + 20;
	if (sc->sc_iswide) {
		for (i = 0; i < 8; i++)
			period.reply_w.period[i] =
			    setup.reply_w.sync[i].period * 5 + 20;
	}

	if (sc->sc_firmware[0] >= '3') {
		rlen = sizeof(period.reply) +
		    (sc->sc_iswide ? sizeof(period.reply_w) : 0);
		period.cmd.opcode = BHA_INQUIRE_PERIOD;
		period.cmd.len = rlen;
		bha_cmd(iot, ioh, sc,
		    sizeof(period.cmd), (u_char *)&period.cmd,
		    rlen, (u_char *)&period.reply);
	}

	for (i = 0; i < 8; i++) {
		if (!setup.reply.sync[i].valid ||
		    (!setup.reply.sync[i].offset &&
		     !setup.reply.sync[i].period))
			continue;
		printf("%s targ %d: sync, offset %d, period %dnsec\n",
		    sc->sc_dev.dv_xname, i,
		    setup.reply.sync[i].offset, period.reply.period[i] * 10);
	}
	if (sc->sc_iswide) {
		for (i = 0; i < 8; i++) {
			if (!setup.reply_w.sync[i].valid ||
			    (!setup.reply_w.sync[i].offset &&
			     !setup.reply_w.sync[i].period))
				continue;
			printf("%s targ %d: sync, offset %d, period %dnsec\n",
			    sc->sc_dev.dv_xname, i + 8,
			    setup.reply_w.sync[i].offset,
			    period.reply_w.period[i] * 10);
		}
	}

	/*
	 * Set up initial mail box for round-robin operation.
	 */
	for (i = 0; i < BHA_MBX_SIZE; i++) {
		wmbx->mbo[i].cmd = BHA_MBO_FREE;
		wmbx->mbi[i].stat = BHA_MBI_FREE;
	}
	wmbx->cmbo = wmbx->tmbo = &wmbx->mbo[0];
	wmbx->tmbi = &wmbx->mbi[0];
	sc->sc_mbofull = 0;

	/* Initialize mail box. */
	mailbox.cmd.opcode = BHA_MBX_INIT_EXTENDED;
	mailbox.cmd.nmbx = BHA_MBX_SIZE;
	ltophys(KVTOPHYS(wmbx), mailbox.cmd.addr);
	bha_cmd(iot, ioh, sc,
	    sizeof(mailbox.cmd), (u_char *)&mailbox.cmd,
	    0, (u_char *)0);
}

void
bha_inquire_setup_information(sc)
	struct bha_softc *sc;
{
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;
	struct bha_model model;
	struct bha_revision revision;
	struct bha_digit digit;
	char *p;

	/*
	 * Get the firmware revision.
	 */
	p = sc->sc_firmware;
	revision.cmd.opcode = BHA_INQUIRE_REVISION;
	bha_cmd(iot, ioh, sc,
	    sizeof(revision.cmd), (u_char *)&revision.cmd,
	    sizeof(revision.reply), (u_char *)&revision.reply);
	*p++ = revision.reply.firm_revision;
	*p++ = '.';
	*p++ = revision.reply.firm_version;
	digit.cmd.opcode = BHA_INQUIRE_REVISION_3;
	bha_cmd(iot, ioh, sc,
	    sizeof(digit.cmd), (u_char *)&digit.cmd,
	    sizeof(digit.reply), (u_char *)&digit.reply);
	*p++ = digit.reply.digit;
	if (revision.reply.firm_revision >= '3' ||
	    (revision.reply.firm_revision == '3' &&
	     revision.reply.firm_version >= '3')) {
		digit.cmd.opcode = BHA_INQUIRE_REVISION_4;
		bha_cmd(iot, ioh, sc,
		    sizeof(digit.cmd), (u_char *)&digit.cmd,
		    sizeof(digit.reply), (u_char *)&digit.reply);
		*p++ = digit.reply.digit;
	}
	while (p > sc->sc_firmware && (p[-1] == ' ' || p[-1] == '\0'))
		p--;
	*p = '\0';

	/*
	 * Get the model number.
	 */
	if (revision.reply.firm_revision >= '3') {
		p = sc->sc_model;
		model.cmd.opcode = BHA_INQUIRE_MODEL;
		model.cmd.len = sizeof(model.reply);
		bha_cmd(iot, ioh, sc,
		    sizeof(model.cmd), (u_char *)&model.cmd,
		    sizeof(model.reply), (u_char *)&model.reply);
		*p++ = model.reply.id[0];
		*p++ = model.reply.id[1];
		*p++ = model.reply.id[2];
		*p++ = model.reply.id[3];
		while (p > sc->sc_model && (p[-1] == ' ' || p[-1] == '\0'))
			p--;
		*p++ = model.reply.version[0];
		*p++ = model.reply.version[1];
		while (p > sc->sc_model && (p[-1] == ' ' || p[-1] == '\0'))
			p--;
		*p = '\0';
	} else
		strcpy(sc->sc_model, "542B");

	printf("%s: model BT-%s, firmware %s\n", sc->sc_dev.dv_xname,
	    sc->sc_model, sc->sc_firmware);
}

void
bhaminphys(bp)
	struct buf *bp;
{

	if (bp->b_bcount > ((BHA_NSEG - 1) << PGSHIFT))
		bp->b_bcount = ((BHA_NSEG - 1) << PGSHIFT);
	minphys(bp);
}

/*
 * start a scsi operation given the command and the data address.  Also needs
 * the unit, target and lu.
 */
int
bha_scsi_cmd(xs)
	struct scsi_xfer *xs;
{
	struct scsi_link *sc_link = xs->sc_link;
	struct bha_softc *sc = sc_link->adapter_softc;
	struct bha_ccb *ccb;
	struct bha_scat_gath *sg;
	int seg;		/* scatter gather seg being worked on */
	u_long thiskv, thisphys, nextphys;
	int bytes_this_seg, bytes_this_page, datalen, flags;
#ifdef TFS
	struct iovec *iovp;
#endif
	int s;

	SC_DEBUG(sc_link, SDEV_DB2, ("bha_scsi_cmd\n"));
	/*
	 * get a ccb to use. If the transfer
	 * is from a buf (possibly from interrupt time)
	 * then we can't allow it to sleep
	 */
	flags = xs->flags;
	if ((ccb = bha_get_ccb(sc, flags)) == NULL) {
		xs->error = XS_DRIVER_STUFFUP;
		return (TRY_AGAIN_LATER);
	}
	ccb->xs = xs;
	ccb->timeout = xs->timeout;

	/*
	 * Put all the arguments for the xfer in the ccb
	 */
	if (flags & SCSI_RESET) {
		ccb->opcode = BHA_RESET_CCB;
		ccb->scsi_cmd_length = 0;
	} else {
		/* can't use S/G if zero length */
		ccb->opcode = (xs->datalen ? BHA_INIT_SCAT_GATH_CCB
					   : BHA_INITIATOR_CCB);
		bcopy(xs->cmd, &ccb->scsi_cmd,
		    ccb->scsi_cmd_length = xs->cmdlen);
	}

	if (xs->datalen) {
		sg = ccb->scat_gath;
		seg = 0;
#ifdef	TFS
		if (flags & SCSI_DATA_UIO) {
			iovp = ((struct uio *)xs->data)->uio_iov;
			datalen = ((struct uio *)xs->data)->uio_iovcnt;
			xs->datalen = 0;
			while (datalen && seg < BHA_NSEG) {
				ltophys(iovp->iov_base, sg->seg_addr);
				ltophys(iovp->iov_len, sg->seg_len);
				xs->datalen += iovp->iov_len;
				SC_DEBUGN(sc_link, SDEV_DB4, ("(0x%x@0x%x)",
				    iovp->iov_len, iovp->iov_base));
				sg++;
				iovp++;
				seg++;
				datalen--;
			}
		} else
#endif	/* TFS */
		{
			/*
			 * Set up the scatter-gather block.
			 */
			SC_DEBUG(sc_link, SDEV_DB4,
			    ("%d @0x%x:- ", xs->datalen, xs->data));

			datalen = xs->datalen;
			thiskv = (u_long)xs->data;
			thisphys = KVTOPHYS(thiskv);

			while (datalen && seg < BHA_NSEG) {
				bytes_this_seg = 0;

				/* put in the base address */
				ltophys(thisphys, sg->seg_addr);

				SC_DEBUGN(sc_link, SDEV_DB4,
				    ("0x%x", thisphys));

				/* do it at least once */
				nextphys = thisphys;
				while (datalen && thisphys == nextphys) {
					/*
					 * This page is contiguous (physically)
					 * with the the last, just extend the
					 * length
					 */
					/* how far to the end of the page */
					nextphys =
					    (thisphys & ~PGOFSET) + NBPG;
					bytes_this_page = nextphys - thisphys;
					/**** or the data ****/
					bytes_this_page = min(bytes_this_page,
							      datalen);
					bytes_this_seg += bytes_this_page;
					datalen -= bytes_this_page;

					/* get more ready for the next page */
					thiskv = (thiskv & ~PGOFSET) + NBPG;
					if (datalen)
						thisphys = KVTOPHYS(thiskv);
				}
				/*
				 * next page isn't contiguous, finish the seg
				 */
				SC_DEBUGN(sc_link, SDEV_DB4,
				    ("(0x%x)", bytes_this_seg));
				ltophys(bytes_this_seg, sg->seg_len);
				sg++;
				seg++;
			}
		}
		/* end of iov/kv decision */
		SC_DEBUGN(sc_link, SDEV_DB4, ("\n"));
		if (datalen) {
			/*
			 * there's still data, must have run out of segs!
			 */
			printf("%s: bha_scsi_cmd, more than %d dma segs\n",
			    sc->sc_dev.dv_xname, BHA_NSEG);
			goto bad;
		}
		ltophys(KVTOPHYS(ccb->scat_gath), ccb->data_addr);
		ltophys(seg * sizeof(struct bha_scat_gath), ccb->data_length);
	} else {		/* No data xfer, use non S/G values */
		ltophys(0, ccb->data_addr);
		ltophys(0, ccb->data_length);
	}

	ccb->data_out = 0;
	ccb->data_in = 0;
	ccb->target = sc_link->target;
	ccb->lun = sc_link->lun;
	ltophys(KVTOPHYS(&ccb->scsi_sense), ccb->sense_ptr);
	ccb->req_sense_length = sizeof(ccb->scsi_sense);
	ccb->host_stat = 0x00;
	ccb->target_stat = 0x00;
	ccb->link_id = 0;
	ltophys(0, ccb->link_addr);

	s = splbio();
	bha_queue_ccb(sc, ccb);
	splx(s);

	/*
	 * Usually return SUCCESSFULLY QUEUED
	 */
	SC_DEBUG(sc_link, SDEV_DB3, ("cmd_sent\n"));
	if ((flags & SCSI_POLL) == 0)
		return (SUCCESSFULLY_QUEUED);

	/*
	 * If we can't use interrupts, poll on completion
	 */
	if (bha_poll(sc, xs, ccb->timeout)) {
		bha_timeout(ccb);
		if (bha_poll(sc, xs, ccb->timeout))
			bha_timeout(ccb);
	}
	return (COMPLETE);

bad:
	xs->error = XS_DRIVER_STUFFUP;
	bha_free_ccb(sc, ccb);
	return (COMPLETE);
}

/*
 * Poll a particular unit, looking for a particular xs
 */
int
bha_poll(sc, xs, count)
	struct bha_softc *sc;
	struct scsi_xfer *xs;
	int count;
{
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;

	/* timeouts are in msec, so we loop in 1000 usec cycles */
	while (count) {
		/*
		 * If we had interrupts enabled, would we
		 * have got an interrupt?
		 */
		if (bus_space_read_1(iot, ioh, BHA_INTR_PORT) &
		    BHA_INTR_ANYINTR)
			bha_intr(sc);
		if (xs->flags & ITSDONE)
			return (0);
		delay(1000);	/* only happens in boot so ok */
		count--;
	}
	return (1);
}

void
bha_timeout(arg)
	void *arg;
{
	struct bha_ccb *ccb = arg;
	struct scsi_xfer *xs = ccb->xs;
	struct scsi_link *sc_link = xs->sc_link;
	struct bha_softc *sc = sc_link->adapter_softc;
	int s;

	sc_print_addr(sc_link);
	printf("timed out");

	s = splbio();

#ifdef BHADIAG
	/*
	 * If the ccb's mbx is not free, then the board has gone Far East?
	 */
	bha_collect_mbo(sc);
	if (ccb->flags & CCB_SENDING) {
		printf("%s: not taking commands!\n", sc->sc_dev.dv_xname);
		Debugger();
	}
#endif

	/*
	 * If it has been through before, then
	 * a previous abort has failed, don't
	 * try abort again
	 */
	if (ccb->flags & CCB_ABORT) {
		/* abort timed out */
		printf(" AGAIN\n");
		/* XXX Must reset! */
	} else {
		/* abort the operation that has timed out */
		printf("\n");
		ccb->xs->error = XS_TIMEOUT;
		ccb->timeout = BHA_ABORT_TIMEOUT;
		ccb->flags |= CCB_ABORT;
		bha_queue_ccb(sc, ccb);
	}

	splx(s);
}
