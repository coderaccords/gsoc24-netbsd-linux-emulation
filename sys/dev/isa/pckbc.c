/* $NetBSD: pckbc.c,v 1.2 1998/04/07 15:57:48 drochner Exp $ */

/*
 * Copyright (c) 1998
 *	Matthias Drochner.  All rights reserved.
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
 *	This product includes software developed for the NetBSD Project
 *	by Matthias Drochner.
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
 *
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/device.h>
#include <sys/malloc.h>
#include <sys/errno.h>
#include <sys/queue.h>
#include <sys/lock.h>
#include <sys/pool.h>

#include <machine/bus.h>

#include <dev/isa/isareg.h>
#include <dev/isa/isavar.h>
#include <dev/isa/pckbcvar.h>

#include <dev/ic/i8042reg.h>

#ifdef __HAVE_NWSCONS /* XXX: this port uses sys/dev/pckbc */
#include "pckbd.h"
#else /* ie: only md drivers attach to pckbc */
#define NPCKBD 0
#endif
#if (NPCKBD > 0)
#include <dev/pckbc/pckbdvar.h>
#endif

/* descriptor for one device command */
struct pckbc_devcmd {
	TAILQ_ENTRY(pckbc_devcmd) next;
	int flags;
#define KBC_CMDFLAG_SYNC 1 /* give descriptor back to caller */
#define KBC_CMDFLAG_SLOW 2
	u_char cmd[4];
	int cmdlen, cmdidx, retries;
	u_char response[4];
	int status, responselen, responseidx;
};

struct pckbc_softc {
	struct device sc_dv;
	isa_chipset_tag_t sc_ic;

	struct pckbc_internal *id;

	pckbc_inputfcn inputhandler[2];
	void *inputarg[2];
};

/* data per slave device */
struct pckbc_slotdata {
	int polling; /* don't read data port in interrupt handler */
	TAILQ_HEAD(, pckbc_devcmd) cmdqueue; /* active commands */
	pool_handle_t cmdpool; /* freelist for descriptors */
#define NCMD 5
	char cmdpool_storage[POOL_STORAGE_SIZE(sizeof(struct pckbc_devcmd),
					       NCMD)];
};

#define CMD_IN_QUEUE(q) (TAILQ_FIRST(&(q)->cmdqueue) != NULL)

/*
 * external representation (pckbc_tag_t),
 * needed early for console operation
 */
struct pckbc_internal {
	bus_space_tag_t t_iot;
	bus_space_handle_t t_ioh_d, t_ioh_c; /* data port, cmd port */
	u_char t_cmdbyte; /* shadow */

	int t_haveaux; /* controller has an aux port */
	struct pckbc_slotdata *t_slotdata[2];

	struct pckbc_softc *t_sc; /* back pointer */
};

int pckbc_match __P((struct device *, struct cfdata *, void *));
void pckbc_attach __P((struct device *, struct device *, void *));
void pckbc_init_slotdata __P((struct pckbc_slotdata *));
int pckbc_attach_slot __P((struct pckbc_softc *, pckbc_slot_t));
int pckbc_submatch __P((struct device *, struct cfdata *, void *));
int pckbcprint __P((void *, const char *));

struct cfattach pckbc_ca = {
	sizeof(struct pckbc_softc), pckbc_match, pckbc_attach,
};

static int pckbc_console, pckbc_console_attached;
static struct pckbc_internal pckbc_consdata;
static struct pckbc_slotdata pckbc_cons_slotdata;
static int pckbc_is_console __P((bus_space_tag_t));

static int pckbc_wait_output __P((bus_space_tag_t, bus_space_handle_t));
static int pckbc_send_cmd __P((bus_space_tag_t, bus_space_handle_t, u_char));
static int pckbc_poll_data1 __P((bus_space_tag_t,
				 bus_space_handle_t, bus_space_handle_t,
				 pckbc_slot_t, int));

static int pckbc_get8042cmd __P((struct pckbc_internal *));
static int pckbc_put8042cmd __P((struct pckbc_internal *));
static int pckbc_send_devcmd __P((struct pckbc_internal *, pckbc_slot_t,
				  u_char));
static void pckbc_poll_cmd1 __P((struct pckbc_internal *, pckbc_slot_t,
				 struct pckbc_devcmd *));

void pckbc_cleanqueue __P((struct pckbc_slotdata *));
void pckbc_cleanup __P((void *));
int pckbc_cmdresponse __P((struct pckbc_internal *, pckbc_slot_t, u_char));
void pckbc_start __P((struct pckbc_internal *, pckbc_slot_t));

int pckbcintr __P((void *));

#define KBC_DEVCMD_ACK 0xfa
#define KBC_DEVCMD_RESEND 0xfe

#define	KBD_DELAY	DELAY(8)

static inline int
pckbc_wait_output(iot, ioh_c)
	bus_space_tag_t iot;
	bus_space_handle_t ioh_c;
{
	u_int i;

	for (i = 100000; i; i--)
		if (!(bus_space_read_1(iot, ioh_c, 0) & KBS_IBF)) {
			KBD_DELAY;
			return (1);
		}
	return (0);
}

static int
pckbc_send_cmd(iot, ioh_c, val)
	bus_space_tag_t iot;
	bus_space_handle_t ioh_c;
	u_char val;
{
	if (!pckbc_wait_output(iot, ioh_c))
		return (0);
	bus_space_write_1(iot, ioh_c, 0, val);
	return (1);
}

static int pckbc_poll_data1(iot, ioh_d, ioh_c, slot, checkaux)
	bus_space_tag_t iot;
	bus_space_handle_t ioh_d, ioh_c;
	pckbc_slot_t slot;
	int checkaux;
{
	int i;
	u_char stat;

	/* if 1 port read takes 1us (?), this polls for 100ms */
	for (i = 100000; i; i--) {
		stat = bus_space_read_1(iot, ioh_c, 0);
		if (stat & KBS_DIB) {
			register u_char c;

			KBD_DELAY;
			c = bus_space_read_1(iot, ioh_d, 0);
			if (checkaux && (stat & 0x20)) { /* aux data */
				if (slot != PCKBC_AUX_SLOT) {
#ifdef PCKBCDEBUG
					printf("lost aux 0x%x\n", c);
#endif
					continue;
				}
			} else {
				if (slot == PCKBC_AUX_SLOT) {
#ifdef PCKBCDEBUG
					printf("lost kbd 0x%x\n", c);
#endif
					continue;
				}
			}
			return (c);
		}
	}
	return (-1);
}

/*
 * Get the current command byte.
 */
static int
pckbc_get8042cmd(t)
	struct pckbc_internal *t;
{
	bus_space_tag_t iot = t->t_iot;
	bus_space_handle_t ioh_d = t->t_ioh_d;
	bus_space_handle_t ioh_c = t->t_ioh_c;
	int data;

	if (!pckbc_send_cmd(iot, ioh_c, K_RDCMDBYTE))
		return (0);
	data = pckbc_poll_data1(iot, ioh_d, ioh_c, PCKBC_KBD_SLOT,
				t->t_haveaux);
	if (data == -1)
		return (0);
	t->t_cmdbyte = data;
	return (1);
}

/*
 * Pass command byte to keyboard controller (8042).
 */
static int
pckbc_put8042cmd(t)
	struct pckbc_internal *t;
{
	bus_space_tag_t iot = t->t_iot;
	bus_space_handle_t ioh_d = t->t_ioh_d;
	bus_space_handle_t ioh_c = t->t_ioh_c;

	if (!pckbc_send_cmd(iot, ioh_c, K_LDCMDBYTE))
		return (0);
	if (!pckbc_wait_output(iot, ioh_c))
		return (0);
	bus_space_write_1(iot, ioh_d, 0, t->t_cmdbyte);
	return (1);
}

static int
pckbc_send_devcmd(t, slot, val)
	struct pckbc_internal *t;
	pckbc_slot_t slot;
	u_char val;
{
	bus_space_tag_t iot = t->t_iot;
	bus_space_handle_t ioh_d = t->t_ioh_d;
	bus_space_handle_t ioh_c = t->t_ioh_c;

	if (slot == PCKBC_AUX_SLOT) {
		if (!pckbc_send_cmd(iot, ioh_c, KBC_AUXWRITE))
			return (0);
	}
	if (!pckbc_wait_output(iot, ioh_c))
		return (0);
	bus_space_write_1(iot, ioh_d, 0, val);
	return (1);
}

static int
pckbc_is_console(iot)
	bus_space_tag_t iot;
{
	if (pckbc_console && !pckbc_console_attached &&
	    pckbc_consdata.t_iot == iot)
		return (1);
	return (0);
}

int
pckbc_match(parent, match, aux)
	struct device *parent;
	struct cfdata *match;
	void *aux;
{
	struct isa_attach_args *ia = aux;
	bus_space_tag_t iot = ia->ia_iot;
	bus_space_handle_t ioh_d, ioh_c;
	int res, ok = 1;

	/* If values are hardwired to something that they can't be, punt. */
	if ((ia->ia_iobase != IOBASEUNK && ia->ia_iobase != IO_KBD) ||
	    ia->ia_maddr != MADDRUNK ||
	    (ia->ia_irq != IRQUNK && ia->ia_irq != 1 /* XXX */) ||
	    ia->ia_drq != DRQUNK)
		return (0);

	if (!pckbc_is_console(iot)) {
		if (bus_space_map(iot, IO_KBD + KBDATAP, 1, 0, &ioh_d))
			return (0);
		if (bus_space_map(iot, IO_KBD + KBCMDP, 1, 0, &ioh_c)) {
			bus_space_unmap(iot, ioh_d, 1);
			return (0);
		}

		/* flush KBC */
		(void) pckbc_poll_data1(iot, ioh_d, ioh_c, PCKBC_KBD_SLOT, 0);

		/* KBC selftest */
		if (!pckbc_send_cmd(iot, ioh_c, KBC_SELFTEST)) {
			ok = 0;
			goto out;
		}
		res = pckbc_poll_data1(iot, ioh_d, ioh_c, PCKBC_KBD_SLOT, 0);
		if (res != 0x55) {
			printf("kbc selftest: %x\n", res);
			ok = 0;
		}
out:
		bus_space_unmap(iot, ioh_d, 1);
		bus_space_unmap(iot, ioh_c, 1);
	}

	if (ok) {
		ia->ia_iobase = IO_KBD;
		ia->ia_iosize = 5;
		ia->ia_msize = 0x0;
	}
	return (ok);
}

int
pckbc_submatch(parent, cf, aux)
	struct device *parent;
	struct cfdata *cf;
	void *aux;
{
	struct pckbc_attach_args *pa = aux;

	if (cf->cf_loc[PCKBCCF_SLOT] != PCKBCCF_SLOT_DEFAULT &&
	    cf->cf_loc[PCKBCCF_SLOT] != pa->pa_slot)
		return (0);
	return ((*cf->cf_attach->ca_match)(parent, cf, aux));
}

int pckbc_attach_slot(sc, slot)
	struct pckbc_softc *sc;
	pckbc_slot_t slot;
{
	struct pckbc_internal *t = sc->id;
	struct pckbc_attach_args pa;
	int found;

	pa.pa_tag = t;
	pa.pa_slot = slot;
	found = (config_found_sm((struct device *)sc, &pa,
				 pckbcprint, pckbc_submatch) != NULL);

	if (found && !t->t_slotdata[slot]) {
		t->t_slotdata[slot] = malloc(sizeof(struct pckbc_slotdata),
					     M_DEVBUF, M_NOWAIT);
		pckbc_init_slotdata(t->t_slotdata[slot]);
	}
	return (found);
}

void
pckbc_attach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct pckbc_softc *sc = (struct pckbc_softc *)self;
	struct isa_attach_args *ia = aux;
	struct pckbc_internal *t;
	bus_space_tag_t iot;
	bus_space_handle_t ioh_d, ioh_c;
	int res;
	u_char cmdbits = 0;

	sc->sc_ic = ia->ia_ic;
	iot = ia->ia_iot;

	if (pckbc_is_console(iot)) {
		t = &pckbc_consdata;
		ioh_d = t->t_ioh_d;
		ioh_c = t->t_ioh_c;
		pckbc_console_attached = 1;
	} else {
		if (bus_space_map(iot, IO_KBD + KBDATAP, 1, 0, &ioh_d) ||
		    bus_space_map(iot, IO_KBD + KBCMDP, 1, 0, &ioh_c))
			panic("pckbc_attach: couldn't map");

		t = malloc(sizeof(struct pckbc_internal), M_DEVBUF, M_WAITOK);
		bzero(t, sizeof(struct pckbc_internal));
		t->t_iot = iot;
		t->t_ioh_d = ioh_d;
		t->t_ioh_c = ioh_c;
	}

	t->t_sc = sc;
	sc->id = t;

	printf("\n");

	/* flush */
	(void) pckbc_poll_data1(iot, ioh_d, ioh_c, PCKBC_KBD_SLOT, 0);

	/* Enable ports */
	t->t_cmdbyte = KC8_CPU;
	if (!pckbc_put8042cmd(t)) {
		printf("kbc: cmd word write error\n");
		return;
	}

	/*
	 * check kbd port ok
	 */
	if (!pckbc_send_cmd(iot, ioh_c, KBC_KBDTEST))
		return;
	res = pckbc_poll_data1(iot, ioh_d, ioh_c, PCKBC_KBD_SLOT, 0);

	if (res == 0) {
		if (pckbc_attach_slot(sc, PCKBC_KBD_SLOT))
			cmdbits |= KC8_KENABLE;
	} else {
		printf("kbc: kbd port test: %x\n", res);
		return;
	}

	/*
	 * check aux port ok
	 */
	if (!pckbc_send_cmd(iot, ioh_c, KBC_AUXTEST))
		return;
	res = pckbc_poll_data1(iot, ioh_d, ioh_c, PCKBC_KBD_SLOT, 0);

	if (res == 0) {
		t->t_haveaux = 1;
		if (pckbc_attach_slot(sc, PCKBC_AUX_SLOT))
			cmdbits |= KC8_MENABLE;
	} else
		printf("kbc: aux port test: %x\n", res);

	/* enable needed interrupts */
	t->t_cmdbyte |= cmdbits;
	if (!pckbc_put8042cmd(t))
		printf("kbc: cmd word write error\n");
}

int
pckbcprint(aux, pnp)
	void *aux;
	const char *pnp;
{
	struct pckbc_attach_args *pa = aux;

	if (!pnp)
		printf(" (%s slot)",
		       pa->pa_slot == PCKBC_KBD_SLOT ? "kbd" : "aux");
	return (QUIET);
}

void
pckbc_init_slotdata(q)
	struct pckbc_slotdata *q;
{
	TAILQ_INIT(&q->cmdqueue);
	q->cmdpool = pool_create(sizeof(struct pckbc_devcmd), NCMD,
				 "kbccmdget", 0, q->cmdpool_storage);
	q->polling = 0;
}

void
pckbc_flush(self, slot)
	pckbc_tag_t self;
	pckbc_slot_t slot;
{
	struct pckbc_internal *t = self;

	(void) pckbc_poll_data1(t->t_iot, t->t_ioh_d, t->t_ioh_c,
				slot, t->t_haveaux);
}

int
pckbc_poll_data(self, slot)
	pckbc_tag_t self;
	pckbc_slot_t slot;
{
	struct pckbc_internal *t = self;
	struct pckbc_slotdata *q = t->t_slotdata[slot];
	int c;

	c = pckbc_poll_data1(t->t_iot, t->t_ioh_d, t->t_ioh_c,
			     slot, t->t_haveaux);
	if (c != -1 && q && CMD_IN_QUEUE(q)) {
		/* we jumped into a running command - try to
		 deliver the response */
		if (pckbc_cmdresponse(t, slot, c))
			return (-1);
	}
	return (c);
}

/*
 * switch scancode translation on / off
 * return nonzero on success
 */
int
pckbc_xt_translation(self, slot, on)
	pckbc_tag_t self;
	pckbc_slot_t slot;
	int on;
{
	struct pckbc_internal *t = self;
	int ison;

	if (slot != PCKBC_KBD_SLOT) {
		/* translation only for kbd slot */
		if (on)
			return (0);
		else
			return (1);
	}

	ison = t->t_cmdbyte & KC8_TRANS;
	if ((on && ison) || (!on && !ison))
		return (1);

	t->t_cmdbyte ^= KC8_TRANS;
	if (!pckbc_put8042cmd(t))
		return (0);

	/* read back to be sure */
	if (!pckbc_get8042cmd(t))
		return (0);

	ison = t->t_cmdbyte & KC8_TRANS;
	if ((on && ison) || (!on && !ison))
		return (1);
	return (0);
}

static struct pckbc_portcmd {
	u_char cmd_en, cmd_dis;
} pckbc_portcmd[2] = {
	{
		KBC_KBDENABLE, KBC_KBDDISABLE,
	}, {
		KBC_AUXENABLE, KBC_AUXDISABLE,
	}
};

void
pckbc_slot_enable(self, slot, on)
	pckbc_tag_t self;
	pckbc_slot_t slot;
	int on;
{
	struct pckbc_internal *t = (struct pckbc_internal *)self;
	struct pckbc_portcmd *cmd;

	cmd = &pckbc_portcmd[slot];

	if (!pckbc_send_cmd(t->t_iot, t->t_ioh_c,
			    on ? cmd->cmd_en : cmd->cmd_dis))
		printf("pckbc_slot_enable(%d) failed\n", on);
}

void
pckbc_set_poll(self, slot, on)
	pckbc_tag_t self;
	pckbc_slot_t slot;
	int on;
{
	struct pckbc_internal *t = (struct pckbc_internal *)self;

	t->t_slotdata[slot]->polling = on;

	if (!on) {
                int s;

                /*
                 * If disabling polling on a device that's been configured,
                 * make sure there are no bytes left in the FIFO, holding up
                 * the interrupt line.  Otherwise we won't get any further
                 * interrupts.
                 */
		if (t->t_sc) {
			s = spltty();
			pckbcintr(t->t_sc);
			splx(s);
		}
	}
}

/*
 * Pass command to device, poll for ACK and data.
 * to be called at spltty()
 */
static void
pckbc_poll_cmd1(t, slot, cmd)
	struct pckbc_internal *t;
	pckbc_slot_t slot;
	struct pckbc_devcmd *cmd;
{
	bus_space_tag_t iot = t->t_iot;
	bus_space_handle_t ioh_d = t->t_ioh_d;
	bus_space_handle_t ioh_c = t->t_ioh_c;
	int i, c = 0;

	while (cmd->cmdidx < cmd->cmdlen) {
		if (!pckbc_send_devcmd(t, slot, cmd->cmd[cmd->cmdidx])) {
			printf("pckbc_cmd: send error\n");
			cmd->status = EIO;
			return;
		}
		for (i = 10; i; i--) { /* 1s ??? */
			c = pckbc_poll_data1(iot, ioh_d, ioh_c, slot,
					     t->t_haveaux);
			if (c != -1)
				break;
		}

		if (c == KBC_DEVCMD_ACK) {
			cmd->cmdidx++;
			continue;
		}
		if (c == KBC_DEVCMD_RESEND) {
#ifdef PCKBCDEBUG
			printf("pckbc_cmd: RESEND\n");
#endif
			if (cmd->retries++ < 5)
				continue;
			else {
#ifdef DIAGNOSTIC
				printf("pckbc: cmd failed\n");
#endif
				cmd->status = EIO;
				return;
			}
		}
		if (c == -1) {
#ifdef DIAGNOSTIC
			printf("pckbc_cmd: timeout\n");
#endif
			cmd->status = EIO;
			return;
		}
#ifdef PCKBCDEBUG
		printf("pckbc_cmd: lost 0x%x\n", c);
#endif
	}

	while (cmd->responseidx < cmd->responselen) {
		if (cmd->flags & KBC_CMDFLAG_SLOW)
			i = 100; /* 10s ??? */
		else
			i = 10; /* 1s ??? */
		while (i--) {
			c = pckbc_poll_data1(iot, ioh_d, ioh_c, slot,
					     t->t_haveaux);
			if (c != -1)
				break;
		}
		if (c == -1) {
#ifdef DIAGNOSTIC
			printf("pckbc_cmd: no data\n");
#endif
			cmd->status = ETIMEDOUT;
			return;
		} else
			cmd->response[cmd->responseidx++] = c;
	}
}

/* for use in autoconfiguration */
int
pckbc_poll_cmd(self, slot, cmd, len, responselen, respbuf, slow)
	pckbc_tag_t self;
	pckbc_slot_t slot;
	u_char *cmd;
	int len, responselen;
	u_char *respbuf;
	int slow;
{
	struct pckbc_internal *t = self;
	struct pckbc_devcmd nc;

	if ((len > 4) || (responselen > 4))
		return (EINVAL);

	bzero(&nc, sizeof(nc));
	bcopy(cmd, nc.cmd, len);
	nc.cmdlen = len;
	nc.responselen = responselen;
	nc.flags = (slow ? KBC_CMDFLAG_SLOW : 0);

	pckbc_poll_cmd1(t, slot, &nc);

	if (nc.status == 0 && respbuf)
		bcopy(nc.response, respbuf, responselen);

	return (nc.status);
}

/*
 * Clean up a command queue, throw away everything.
 */
void
pckbc_cleanqueue(q)
	struct pckbc_slotdata *q;
{
	struct pckbc_devcmd *cmd;
#ifdef PCKBCDEBUG
	int i;
#endif

	while ((cmd = TAILQ_FIRST(&q->cmdqueue))) {
		TAILQ_REMOVE(&q->cmdqueue, cmd, next);
#ifdef PCKBCDEBUG
		printf("pckbc_cleanqueue: removing");
		for (i = 0; i < cmd->cmdlen; i++)
			printf(" %02x", cmd->cmd[i]);
		printf("\n");
#endif
		pool_put(q->cmdpool, cmd);
	}
}

/*
 * Timeout error handler: clean queues and data port.
 * XXX could be less invasive.
 */
void
pckbc_cleanup(self)
	void *self;
{
	struct pckbc_internal *t = self;
	int s;

	printf("pckbc: command timeout\n");

	s = spltty();

	if (t->t_slotdata[PCKBC_KBD_SLOT])
		pckbc_cleanqueue(t->t_slotdata[PCKBC_KBD_SLOT]);
	if (t->t_slotdata[PCKBC_AUX_SLOT])
		pckbc_cleanqueue(t->t_slotdata[PCKBC_AUX_SLOT]);

	while (bus_space_read_1(t->t_iot, t->t_ioh_c, 0) & KBS_DIB) {
		KBD_DELAY;
		(void) bus_space_read_1(t->t_iot, t->t_ioh_d, 0);
	}

	/* reset KBC? */

	splx(s);
}

/*
 * Pass command to device during normal operation.
 * to be called at spltty()
 */
void
pckbc_start(t, slot)
	struct pckbc_internal *t;
	pckbc_slot_t slot;
{
	struct pckbc_slotdata *q = t->t_slotdata[slot];
	struct pckbc_devcmd *cmd = TAILQ_FIRST(&q->cmdqueue);

	if (q->polling) {
		do {
			pckbc_poll_cmd1(t, slot, cmd);
			if (cmd->status)
				printf("pckbc_start: command error\n");

			TAILQ_REMOVE(&q->cmdqueue, cmd, next);
			if (cmd->flags & KBC_CMDFLAG_SYNC)
				wakeup(cmd);
			else {
				untimeout(pckbc_cleanup, t);
				pool_put(q->cmdpool, cmd);
			}
			cmd = TAILQ_FIRST(&q->cmdqueue);
		} while (cmd);
		return;
	}

	if (!pckbc_send_devcmd(t, slot, cmd->cmd[cmd->cmdidx])) {
		printf("pckbc_start: send error\n");
		/* XXX what now? */
		return;
	}
}

/*
 * Handle command responses coming in asynchonously,
 * return nonzero if valid response.
 * to be called at spltty()
 */
int
pckbc_cmdresponse(t, slot, data)
	struct pckbc_internal *t;
	pckbc_slot_t slot;
	u_char data;
{
	struct pckbc_slotdata *q = t->t_slotdata[slot];
	struct pckbc_devcmd *cmd = TAILQ_FIRST(&q->cmdqueue);
#ifdef DIAGNOSTIC
	if (!cmd)
		panic("pckbc_cmdresponse: no active command");
#endif
	if (cmd->cmdidx < cmd->cmdlen) {
		if (data != KBC_DEVCMD_ACK && data != KBC_DEVCMD_RESEND)
			return (0);

		if (data == KBC_DEVCMD_RESEND) {
			if (cmd->retries++ < 5) {
				/* try again last command */
				goto restart;
			} else {
				printf("pckbc: cmd failed\n");
				cmd->status = EIO;
				/* dequeue */
			}
		} else {
			if (++cmd->cmdidx < cmd->cmdlen)
				goto restart;
			if (cmd->responselen)
				return (1);
			/* else dequeue */
		}
	} else if (cmd->responseidx < cmd->responselen) {
		cmd->response[cmd->responseidx++] = data;
		if (cmd->responseidx < cmd->responselen)
			return (1);
		/* else dequeue */
	} else
		return (0);

	/* dequeue: */
	TAILQ_REMOVE(&q->cmdqueue, cmd, next);
	if (cmd->flags & KBC_CMDFLAG_SYNC)
		wakeup(cmd);
	else {
		untimeout(pckbc_cleanup, t);
		pool_put(q->cmdpool, cmd);
	}
	if (!CMD_IN_QUEUE(q))
		return (1);
restart:
	pckbc_start(t, slot);
	return (1);
}

/*
 * Put command into the device's command queue, return zero or errno.
 */
int
pckbc_enqueue_cmd(self, slot, cmd, len, responselen, sync, respbuf)
	pckbc_tag_t self;
	pckbc_slot_t slot;
	u_char *cmd;
	int len, responselen, sync;
	u_char *respbuf;
{
	struct pckbc_internal *t = self;
	struct pckbc_slotdata *q = t->t_slotdata[slot];
	struct pckbc_devcmd *nc;
	int s, isactive, res = 0;

	if ((len > 4) || (responselen > 4))
		return (EINVAL);
	s = spltty();
	nc = pool_get(q->cmdpool, 0);
	splx(s);
	if (!nc)
		return (ENOMEM);

	bzero(nc, sizeof(*nc));
	bcopy(cmd, nc->cmd, len);
	nc->cmdlen = len;
	nc->responselen = responselen;
	nc->flags = (sync ? KBC_CMDFLAG_SYNC : 0);

	s = spltty();

	if (q->polling && sync) {
		/*
		 * XXX We should poll until the queue is empty.
		 * But we don't come here normally, so make
		 * it simple and throw away everything.
		 */
		pckbc_cleanqueue(q);
	}

	isactive = CMD_IN_QUEUE(q);
	TAILQ_INSERT_TAIL(&q->cmdqueue, nc, next);
	if (!isactive)
		pckbc_start(t, slot);

	if (q->polling)
		res = (sync ? nc->status : 0);
	else if (sync) {
		if ((res = tsleep(nc, 0, "kbccmd", 1*hz))) {
			TAILQ_REMOVE(&q->cmdqueue, nc, next);
			pckbc_cleanup(t);
		} else
			res = nc->status;
	} else
		timeout(pckbc_cleanup, t, 1*hz);

	if (sync) {
		if (respbuf)
			bcopy(nc->response, respbuf, responselen);
		pool_put(q->cmdpool, nc);
	}

	splx(s);

	return (res);
}

void
pckbc_set_inputhandler(self, slot, func, arg)
	pckbc_tag_t self;
	pckbc_slot_t slot;
	pckbc_inputfcn func;
	void *arg;
{
	struct pckbc_internal *t = (struct pckbc_internal *)self;
	struct pckbc_softc *sc = t->t_sc;
	void *rv;

	/* XXX use machdep hook? */

	switch (slot) {
	    case PCKBC_KBD_SLOT:
		rv = isa_intr_establish(sc->sc_ic, 1, IST_EDGE, IPL_TTY,
					pckbcintr, sc);
		break;
	    case PCKBC_AUX_SLOT:
		rv = isa_intr_establish(sc->sc_ic, 12, IST_EDGE, IPL_TTY,
					pckbcintr, sc);
		/* XXX irq 9 on alpha AXP150 "Jensen" */
		break;
	    default:
		panic("pckbc_set_inputhandler: bad slot %d", slot);
	}

	sc->inputhandler[slot] = func;
	sc->inputarg[slot] = arg;
}

int
pckbcintr(vsc)
	void *vsc;
{
	struct pckbc_softc *sc = (struct pckbc_softc *)vsc;
	struct pckbc_internal *t = sc->id;
	u_char stat;
	pckbc_slot_t slot;
	struct pckbc_slotdata *q;
	int served = 0, data;

	for(;;) {
		stat = bus_space_read_1(t->t_iot, t->t_ioh_c, 0);
		if (!(stat & KBS_DIB))
			break;

		served = 1;

		slot = (t->t_haveaux && (stat & 0x20)) ?
		    PCKBC_AUX_SLOT : PCKBC_KBD_SLOT;
		q = t->t_slotdata[slot];

		if (!q) {
			/* XXX do something for live insertion? */
			printf("pckbcintr: no dev for slot %d\n", slot);
			KBD_DELAY;
			(void) bus_space_read_1(t->t_iot, t->t_ioh_d, 0);
			continue;
		}

		if (q->polling)
			break; /* pckbc_poll_data() will get it */

		KBD_DELAY;
		data = bus_space_read_1(t->t_iot, t->t_ioh_d, 0);

		if (CMD_IN_QUEUE(q) && pckbc_cmdresponse(t, slot, data))
			continue;

		if (sc->inputhandler[slot])
			(*sc->inputhandler[slot])(sc->inputarg[slot], data);
#ifdef PCKBCDEBUG
		else
			printf("pckbcintr: slot %d lost %d\n", slot, data);
#endif
	}

	return (served);
}

int
pckbc_cnattach(iot, slot)
	bus_space_tag_t iot;
	pckbc_slot_t slot;
{
	int res;

	pckbc_consdata.t_iot = iot;
	if (bus_space_map(iot, IO_KBD + KBDATAP, 1, 0,
			  &pckbc_consdata.t_ioh_d))
                return (ENXIO);
	if (bus_space_map(iot, IO_KBD + KBCMDP, 1, 0,
			  &pckbc_consdata.t_ioh_c)) {
		bus_space_unmap(iot, pckbc_consdata.t_ioh_d, 1);
                return (ENXIO);
	}

#if (NPCKBD > 0)
	res = pckbd_cnattach(&pckbc_consdata, slot);
#else
	res = pckbc_machdep_cnattach(&pckbc_consdata, slot);
#endif
	if (res) {
		bus_space_unmap(iot, pckbc_consdata.t_ioh_d, 1);
		bus_space_unmap(iot, pckbc_consdata.t_ioh_c, 1);
	} else {
		pckbc_consdata.t_slotdata[slot] = &pckbc_cons_slotdata;
		pckbc_init_slotdata(&pckbc_cons_slotdata);
		pckbc_console = 1;
	}

	return (res);
}
