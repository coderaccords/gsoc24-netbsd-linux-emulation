/*	$NetBSD: stp4020.c,v 1.38 2004/07/05 10:48:29 pk Exp $ */

/*-
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Paul Kranenburg.
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

/*
 * STP4020: SBus/PCMCIA bridge supporting two Type-3 PCMCIA cards.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: stp4020.c,v 1.38 2004/07/05 10:48:29 pk Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/malloc.h>
#include <sys/extent.h>
#include <sys/proc.h>
#include <sys/kernel.h>
#include <sys/kthread.h>
#include <sys/device.h>

#include <dev/pcmcia/pcmciareg.h>
#include <dev/pcmcia/pcmciavar.h>
#include <dev/pcmcia/pcmciachip.h>

#include <machine/bus.h>
#include <machine/intr.h>

#include <dev/sbus/sbusvar.h>
#include <dev/sbus/stp4020reg.h>

#define STP4020_DEBUG 1	/* XXX-temp */

/*
 * We use the three available windows per socket in a simple, fixed
 * arrangement. Each window maps (at full 1 MB size) one of the pcmcia
 * spaces into sbus space.
 */
#define STP_WIN_ATTR	0	/* index of the attribute memory space window */
#define	STP_WIN_MEM	1	/* index of the common memory space window */
#define	STP_WIN_IO	2	/* index of the io space window */


#if defined(STP4020_DEBUG)
int stp4020_debug = 0;
#define DPRINTF(x)	do { if (stp4020_debug) printf x; } while(0)
#else
#define DPRINTF(x)
#endif

/*
 * Event queue; events detected in an interrupt context go here
 * awaiting attention from our event handling thread.
 */
struct stp4020_event {
	SIMPLEQ_ENTRY(stp4020_event) se_q;
	int	se_type;
	int	se_sock;
};
/* Defined event types */
#define STP4020_EVENT_INSERTION	0
#define STP4020_EVENT_REMOVAL	1

/*
 * Per socket data.
 */
struct stp4020_socket {
	struct stp4020_softc	*sc;	/* Back link */
	int		flags;
#define STP4020_SOCKET_BUSY	0x0001
	int		sock;		/* Socket number (0 or 1) */
	int		sbus_intno;	/* Do we use first (0) or second (1)
					   interrupt? */
	int		int_enable;	/* ICR0 value for interrupt enabled */
	int		int_disable;	/* ICR0 value for interrupt disabled */
	bus_space_tag_t	tag;		/* socket control io	*/
	bus_space_handle_t	regs;	/*  space		*/
	bus_space_tag_t	pcmciat;	/* io space for pcmcia  */
	struct device	*pcmcia;	/* Associated PCMCIA device */
	int		(*intrhandler)	/* Card driver interrupt handler */
			    __P((void *));
	void		*intrarg;	/* Card interrupt handler argument */
	void		*softint;	/* cookie for the softintr */

	struct {
		bus_space_handle_t	winaddr;/* this window's address */
	} windows[STP4020_NWIN];

};

struct stp4020_softc {
	struct device	sc_dev;		/* Base device */
	struct sbusdev	sc_sd;		/* SBus device */
	pcmcia_chipset_tag_t	sc_pct;	/* Chipset methods */

	struct proc	*event_thread;		/* event handling thread */
	SIMPLEQ_HEAD(, stp4020_event)	events;	/* Pending events for thread */

	struct stp4020_socket sc_socks[STP4020_NSOCK];
};


static int	stp4020print	__P((void *, const char *));
static int	stp4020match	__P((struct device *, struct cfdata *, void *));
static void	stp4020attach	__P((struct device *, struct device *, void *));
static int	stp4020_intr	__P((void *));
static void	stp4020_map_window(struct stp4020_socket *h, int win, int speed);
static void	stp4020_calc_speed(int bus_speed, int ns, int *length, int *delay);
static void	stp4020_intr_dispatch(void *arg);

CFATTACH_DECL(nell, sizeof(struct stp4020_softc),
    stp4020match, stp4020attach, NULL, NULL);

#ifdef STP4020_DEBUG
static void	stp4020_dump_regs __P((struct stp4020_socket *));
#endif

static int	stp4020_rd_sockctl __P((struct stp4020_socket *, int));
static void	stp4020_wr_sockctl __P((struct stp4020_socket *, int, int));
static int	stp4020_rd_winctl __P((struct stp4020_socket *, int, int));
static void	stp4020_wr_winctl __P((struct stp4020_socket *, int, int, int));

void	stp4020_delay __P((unsigned int));
void	stp4020_attach_socket __P((struct stp4020_socket *, int));
void	stp4020_create_event_thread __P((void *));
void	stp4020_event_thread __P((void *));
void	stp4020_queue_event __P((struct stp4020_softc *, int, int));

int	stp4020_chip_mem_alloc __P((pcmcia_chipset_handle_t, bus_size_t,
				    struct pcmcia_mem_handle *));
void	stp4020_chip_mem_free __P((pcmcia_chipset_handle_t,
				   struct pcmcia_mem_handle *));
int	stp4020_chip_mem_map __P((pcmcia_chipset_handle_t, int, bus_addr_t,
				  bus_size_t, struct pcmcia_mem_handle *,
				  bus_size_t *, int *));
void	stp4020_chip_mem_unmap __P((pcmcia_chipset_handle_t, int));

int	stp4020_chip_io_alloc __P((pcmcia_chipset_handle_t,
				   bus_addr_t, bus_size_t, bus_size_t,
				   struct pcmcia_io_handle *));
void	stp4020_chip_io_free __P((pcmcia_chipset_handle_t,
				  struct pcmcia_io_handle *));
int	stp4020_chip_io_map __P((pcmcia_chipset_handle_t, int, bus_addr_t,
				 bus_size_t, struct pcmcia_io_handle *, int *));
void	stp4020_chip_io_unmap __P((pcmcia_chipset_handle_t, int));

void	stp4020_chip_socket_enable __P((pcmcia_chipset_handle_t));
void	stp4020_chip_socket_disable __P((pcmcia_chipset_handle_t));
void	*stp4020_chip_intr_establish __P((pcmcia_chipset_handle_t,
					  struct pcmcia_function *, int,
					  int (*) __P((void *)), void *));
void	stp4020_chip_intr_disestablish __P((pcmcia_chipset_handle_t, void *));

/* Our PCMCIA chipset methods */
static struct pcmcia_chip_functions stp4020_functions = {
	stp4020_chip_mem_alloc,
	stp4020_chip_mem_free,
	stp4020_chip_mem_map,
	stp4020_chip_mem_unmap,

	stp4020_chip_io_alloc,
	stp4020_chip_io_free,
	stp4020_chip_io_map,
	stp4020_chip_io_unmap,

	stp4020_chip_intr_establish,
	stp4020_chip_intr_disestablish,

	stp4020_chip_socket_enable,
	stp4020_chip_socket_disable
};


static __inline__ int
stp4020_rd_sockctl(h, idx)
	struct stp4020_socket *h;
	int idx;
{
	int o = ((STP4020_SOCKREGS_SIZE * (h->sock)) + idx);
	return (bus_space_read_2(h->tag, h->regs, o));
}

static __inline__ void
stp4020_wr_sockctl(h, idx, v)
	struct stp4020_socket *h;
	int idx;
	int v;
{
	int o = (STP4020_SOCKREGS_SIZE * (h->sock)) + idx;
	bus_space_write_2(h->tag, h->regs, o, v);
}

static __inline__ int
stp4020_rd_winctl(h, win, idx)
	struct stp4020_socket *h;
	int win;
	int idx;
{
	int o = (STP4020_SOCKREGS_SIZE * (h->sock)) +
		(STP4020_WINREGS_SIZE * win) + idx;
	return (bus_space_read_2(h->tag, h->regs, o));
}

static __inline__ void
stp4020_wr_winctl(h, win, idx, v)
	struct stp4020_socket *h;
	int win;
	int idx;
	int v;
{
	int o = (STP4020_SOCKREGS_SIZE * (h->sock)) +
		(STP4020_WINREGS_SIZE * win) + idx;

	bus_space_write_2(h->tag, h->regs, o, v);
}

#ifndef SUN4U	/* XXX - move to SBUS machdep function? */

static	u_int16_t stp4020_read_2(bus_space_tag_t,
				 bus_space_handle_t,
				 bus_size_t);
static	u_int32_t stp4020_read_4(bus_space_tag_t,
				 bus_space_handle_t,
				 bus_size_t);
static	u_int64_t stp4020_read_8(bus_space_tag_t,
				 bus_space_handle_t,
				 bus_size_t);
static	void	stp4020_write_2(bus_space_tag_t,
				bus_space_handle_t,
				bus_size_t,
				u_int16_t);
static	void	stp4020_write_4(bus_space_tag_t,
				bus_space_handle_t,
				bus_size_t,
				u_int32_t);
static	void	stp4020_write_8(bus_space_tag_t,
				bus_space_handle_t,
				bus_size_t,
				u_int64_t);

static u_int16_t
stp4020_read_2(space, handle, offset)
	bus_space_tag_t space;
	bus_space_handle_t handle;
	bus_size_t offset;
{
	return (le16toh(*(volatile u_int16_t *)(handle + offset)));
}

static u_int32_t
stp4020_read_4(space, handle, offset)
	bus_space_tag_t space;
	bus_space_handle_t handle;
	bus_size_t offset;
{
	return (le32toh(*(volatile u_int32_t *)(handle + offset)));
}

static u_int64_t
stp4020_read_8(space, handle, offset)
	bus_space_tag_t space;
	bus_space_handle_t handle;
	bus_size_t offset;
{
	return (le64toh(*(volatile u_int64_t *)(handle + offset)));
}

static void
stp4020_write_2(space, handle, offset, value)
	bus_space_tag_t space;
	bus_space_handle_t handle;
	bus_size_t offset;
	u_int16_t value;
{
	(*(volatile u_int16_t *)(handle + offset)) = htole16(value);
}

static void
stp4020_write_4(space, handle, offset, value)
	bus_space_tag_t space;
	bus_space_handle_t handle;
	bus_size_t offset;
	u_int32_t value;
{
	(*(volatile u_int32_t *)(handle + offset)) = htole32(value);
}

static void
stp4020_write_8(space, handle, offset, value)
	bus_space_tag_t space;
	bus_space_handle_t handle;
	bus_size_t offset;
	u_int64_t value;
{
	(*(volatile u_int64_t *)(handle + offset)) = htole64(value);
}
#endif	/* SUN4U */

int
stp4020print(aux, busname)
	void *aux;
	const char *busname;
{
	struct pcmciabus_attach_args *paa = aux;
	struct stp4020_socket *h = paa->pch;

	aprint_normal(" socket %d", h->sock);
	return (UNCONF);
}

int
stp4020match(parent, cf, aux)
	struct device *parent;
	struct cfdata *cf;
	void *aux;
{
	struct sbus_attach_args *sa = aux;

	return (strcmp("SUNW,pcmcia", sa->sa_name) == 0);
}

/*
 * Attach all the sub-devices we can find
 */
void
stp4020attach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct sbus_attach_args *sa = aux;
	struct stp4020_softc *sc = (void *)self;
	bus_space_tag_t tag;
	int rev;
	int i, sbus_intno;
	bus_space_handle_t bh;

	/* lsb of our config flags decides which interrupt we use */
	sbus_intno = sc->sc_dev.dv_cfdata->cf_flags & 1;

	/* Transfer bus tags */
#ifdef SUN4U
	tag = sa->sa_bustag;
#else
	tag = bus_space_tag_alloc(sa->sa_bustag, sc);
	if (tag == NULL) {
		printf("%s: attach: out of memory\n", self->dv_xname);
		return;
	}
	tag->sparc_read_2 = stp4020_read_2;
	tag->sparc_read_4 = stp4020_read_4;
	tag->sparc_read_8 = stp4020_read_8;
	tag->sparc_write_2 = stp4020_write_2;
	tag->sparc_write_4 = stp4020_write_4;
	tag->sparc_write_8 = stp4020_write_8;
#endif	/* SUN4U */

	/* Set up per-socket static initialization */
	sc->sc_socks[0].sc = sc->sc_socks[1].sc = sc;
	sc->sc_socks[0].tag = sc->sc_socks[1].tag = sa->sa_bustag;
	/*
	 * XXX we rely on "tag" accepting the same handle-domain
	 * as sa->sa_bustag.
	 */
	sc->sc_socks[0].pcmciat = sc->sc_socks[1].pcmciat = tag;
	sc->sc_socks[0].sbus_intno =
		sc->sc_socks[1].sbus_intno = sbus_intno;

	if (sa->sa_nreg < 8) {
		printf("%s: only %d register sets\n",
			self->dv_xname, sa->sa_nreg);
		return;
	}

	if (sa->sa_nintr != 2) {
		printf("%s: expect 2 interrupt Sbus levels; got %d\n",
			self->dv_xname, sa->sa_nintr);
		return;
	}

#define STP4020_BANK_PROM	0
#define STP4020_BANK_CTRL	4
	for (i = 0; i < 8; i++) {

		/*
		 * STP4020 Register address map:
		 *	bank  0:   Forth PROM
		 *	banks 1-3: socket 0, windows 0-2
		 *	bank  4:   control registers
		 *	banks 5-7: socket 1, windows 0-2
		 */

		if (i == STP4020_BANK_PROM)
			/* Skip the PROM */
			continue;

		if (sbus_bus_map(sa->sa_bustag,
				 sa->sa_reg[i].oa_space,
				 sa->sa_reg[i].oa_base,
				 sa->sa_reg[i].oa_size,
				 0, &bh) != 0) {
			printf("%s: attach: cannot map registers\n",
				self->dv_xname);
			return;
		}		

		if (i == STP4020_BANK_CTRL) {
			/*
			 * Copy tag and handle to both socket structures
			 * for easy access in control/status IO functions.
			 */
			sc->sc_socks[0].regs = sc->sc_socks[1].regs = bh;
		} else if (i < STP4020_BANK_CTRL) {
			/* banks 1-3 */
			sc->sc_socks[0].windows[i-1].winaddr = bh;
		} else {
			/* banks 5-7 */
			sc->sc_socks[1].windows[i-5].winaddr = bh;
		}
	}

	sbus_establish(&sc->sc_sd, &sc->sc_dev);

	/* We only use one interrupt level. */
	if (sa->sa_nintr > sbus_intno) {
		bus_intr_establish(sa->sa_bustag,
		    sa->sa_intr[sbus_intno].oi_pri,
		    IPL_NONE, stp4020_intr, sc);
	}

	rev = stp4020_rd_sockctl(&sc->sc_socks[0], STP4020_ISR1_IDX) &
		STP4020_ISR1_REV_M;
	printf(": rev %x\n", rev);

	sc->sc_pct = (pcmcia_chipset_tag_t)&stp4020_functions;

	/*
	 * Arrange that a kernel thread be created to handle
	 * insert/removal events.
	 */
	SIMPLEQ_INIT(&sc->events);
	kthread_create(stp4020_create_event_thread, sc);

	for (i = 0; i < STP4020_NSOCK; i++) {
		struct stp4020_socket *h = &sc->sc_socks[i];
		h->sock = i;
		h->sc = sc;
#ifdef STP4020_DEBUG
		if (stp4020_debug)
			stp4020_dump_regs(h);
#endif
		stp4020_attach_socket(h, sa->sa_frequency);
	}
}

void
stp4020_attach_socket(h, speed)
	struct stp4020_socket *h;
	int speed;
{
	struct pcmciabus_attach_args paa;
	int v;

	/* no interrupt handlers yet */
	h->intrhandler = NULL;
	h->intrarg = NULL;
	h->softint = NULL;
	h->int_enable = 0;
	h->int_disable = 0;

	/* Map all three windows */
	stp4020_map_window(h, STP_WIN_ATTR, speed);
	stp4020_map_window(h, STP_WIN_MEM, speed);
	stp4020_map_window(h, STP_WIN_IO, speed);

	/* Configure one pcmcia device per socket */
	paa.paa_busname = "pcmcia";
	paa.pct = (pcmcia_chipset_tag_t)h->sc->sc_pct;
	paa.pch = (pcmcia_chipset_handle_t)h;
	paa.iobase = 0;
	paa.iosize = STP4020_WINDOW_SIZE;

	h->pcmcia = config_found(&h->sc->sc_dev, &paa, stp4020print);

	if (h->pcmcia == NULL)
		return;

	/*
	 * There's actually a pcmcia bus attached; initialize the slot.
	 */

	/*
	 * Clear things up before we enable status change interrupts.
	 * This seems to not be fully initialized by the PROM.
	 */
	stp4020_wr_sockctl(h, STP4020_ICR1_IDX, 0);
	stp4020_wr_sockctl(h, STP4020_ICR0_IDX, 0);
	stp4020_wr_sockctl(h, STP4020_ISR1_IDX, 0x3fff);
	stp4020_wr_sockctl(h, STP4020_ISR0_IDX, 0x3fff);

	/*
	 * Enable socket status change interrupts.
	 * We only use one common interrupt for status change
	 * and IO, to avoid locking issues.
	 */
	v = STP4020_ICR0_ALL_STATUS_IE
	    | (h->sbus_intno ? STP4020_ICR0_SCILVL_SB1
			     : STP4020_ICR0_SCILVL_SB0);
	stp4020_wr_sockctl(h, STP4020_ICR0_IDX, v);

	/* Get live status bits from ISR0 and clear pending interrupts */
	v = stp4020_rd_sockctl(h, STP4020_ISR0_IDX);
	stp4020_wr_sockctl(h, STP4020_ISR0_IDX, v);

	if ((v & (STP4020_ISR0_CD1ST|STP4020_ISR0_CD2ST)) == 0)
		return;

	pcmcia_card_attach(h->pcmcia);
	h->flags |= STP4020_SOCKET_BUSY;
}


/*
 * Deferred thread creation callback.
 */
void
stp4020_create_event_thread(arg)
	void *arg;
{
	struct stp4020_softc *sc = arg;
	const char *name = sc->sc_dev.dv_xname;

	if (kthread_create1(stp4020_event_thread, sc, &sc->event_thread,
			   "%s", name)) {
		panic("%s: unable to create event thread", name);
	}
}

/*
 * The actual event handling thread.
 */
void
stp4020_event_thread(arg)
	void *arg;
{
	struct stp4020_softc *sc = arg;
	struct stp4020_event *e;
	int s;

	while (1) {
		struct stp4020_socket *h;
		int n;

		s = splhigh();
		if ((e = SIMPLEQ_FIRST(&sc->events)) == NULL) {
			splx(s);
			(void)tsleep(&sc->events, PWAIT, "pcicev", 0);
			continue;
		}
		SIMPLEQ_REMOVE_HEAD(&sc->events, se_q);
		splx(s);

		n = e->se_sock;
		if (n < 0 || n >= STP4020_NSOCK)
			panic("stp4020_event_thread: wayward socket number %d",
			      n);

		h = &sc->sc_socks[n];
		switch (e->se_type) {
		case STP4020_EVENT_INSERTION:
			pcmcia_card_attach(h->pcmcia);
			break;
		case STP4020_EVENT_REMOVAL:
			pcmcia_card_detach(h->pcmcia, DETACH_FORCE);
			break;
		default:
			panic("stp4020_event_thread: unknown event type %d",
			      e->se_type);
		}
		free(e, M_TEMP);
	}
}

void
stp4020_queue_event(sc, sock, event)
	struct stp4020_softc *sc;
	int sock, event;
{
	struct stp4020_event *e;
	int s;

	e = malloc(sizeof(*e), M_TEMP, M_NOWAIT);
	if (e == NULL)
		panic("stp4020_queue_event: can't allocate event");

	e->se_type = event;
	e->se_sock = sock;
	s = splhigh();
	SIMPLEQ_INSERT_TAIL(&sc->events, e, se_q);
	splx(s);
	wakeup(&sc->events);
}

/*
 * Softinterrupt called to invoke the real driver interrupt handler.
 */
static void
stp4020_intr_dispatch(arg)
	void *arg;
{
	struct stp4020_socket *h = arg;
	int s;

	/* invoke driver handler */
	h->intrhandler(h->intrarg);

	/* enable SBUS interrupts for pcmcia interrupts again */
	s = splhigh();
	stp4020_wr_sockctl(h, STP4020_ICR0_IDX, h->int_enable);
	splx(s);
}

int
stp4020_intr(arg)
	void *arg;
{
	struct stp4020_softc *sc = arg;
	int i, s, r = 0, cd_change = 0;


	/* protect hardware access by splhigh against softint */
	s = splhigh();

	/*
	 * Check each socket for pending requests.
	 */
	for (i = 0 ; i < STP4020_NSOCK; i++) {
		struct stp4020_socket *h;
		int v;

		h = &sc->sc_socks[i];

		v = stp4020_rd_sockctl(h, STP4020_ISR0_IDX);

		/* Ack all interrupts at once. */
		stp4020_wr_sockctl(h, STP4020_ISR0_IDX, v);

#ifdef STP4020_DEBUG
		if (stp4020_debug != 0) {
			char bits[64];
			bitmask_snprintf(v, STP4020_ISR0_IOBITS,
					 bits, sizeof(bits));
			printf("stp4020_statintr: ISR0=%s\n", bits);
		}
#endif

		if ((v & STP4020_ISR0_CDCHG) != 0) {
			/*
			 * Card status change detect
			 */
			cd_change = 1;
			r = 1;
			if ((v & (STP4020_ISR0_CD1ST|STP4020_ISR0_CD2ST)) == (STP4020_ISR0_CD1ST|STP4020_ISR0_CD2ST)){
				if ((h->flags & STP4020_SOCKET_BUSY) == 0) {
					stp4020_queue_event(sc, i,
						STP4020_EVENT_INSERTION);
					h->flags |= STP4020_SOCKET_BUSY;
				}
			}
			if ((v & (STP4020_ISR0_CD1ST|STP4020_ISR0_CD2ST)) == 0){
				if ((h->flags & STP4020_SOCKET_BUSY) != 0) {
					stp4020_queue_event(sc, i,
						STP4020_EVENT_REMOVAL);
					h->flags &= ~STP4020_SOCKET_BUSY;
				}
			}
		}
		
		if ((v & STP4020_ISR0_IOINT) != 0) {
			/* we can not deny this is ours, no matter what the
			   card driver says. */
			r = 1;

			/* It's a card interrupt */
			if ((h->flags & STP4020_SOCKET_BUSY) == 0) {
				printf("stp4020[%d]: spurious interrupt?\n",
					h->sock);
				continue;
			}

			/*
			 * Schedule softint to invoke driver interrupt 
			 * handler
			 */
			if (h->softint != NULL)
				softintr_schedule(h->softint);
			/*
			 * Disable this sbus interrupt, until the soft-int
			 * handler had a chance to run
			 */
			stp4020_wr_sockctl(h, STP4020_ICR0_IDX, h->int_disable);
		}

		/* informational messages */
		if ((v & STP4020_ISR0_BVD1CHG) != 0) {
			/* ignore if this is caused by insert or removal */
			if (!cd_change)
				printf("stp4020[%d]: Battery change 1\n", h->sock);
			r = 1;
		}

		if ((v & STP4020_ISR0_BVD2CHG) != 0) {
			/* ignore if this is caused by insert or removal */
			if (!cd_change)
				printf("stp4020[%d]: Battery change 2\n", h->sock);
			r = 1;
		}

		if ((v & STP4020_ISR0_SCINT) != 0) {
			DPRINTF(("stp4020[%d]: status change\n", h->sock));
			r = 1;
		}

		if ((v & STP4020_ISR0_RDYCHG) != 0) {
			DPRINTF(("stp4020[%d]: Ready/Busy change\n", h->sock));
			r = 1;
		}

		if ((v & STP4020_ISR0_WPCHG) != 0) {
			DPRINTF(("stp4020[%d]: Write protect change\n", h->sock));
			r = 1;
		}

		if ((v & STP4020_ISR0_PCTO) != 0) {
			DPRINTF(("stp4020[%d]: Card access timeout\n", h->sock));
			r = 1;
		}

		if ((v & ~STP4020_ISR0_LIVE) && r == 0)
			printf("stp4020[%d]: unhandled interrupt: 0x%x\n", h->sock, v);

	}
	splx(s);

	return (r);
}

/*
 * The function gets the sbus speed and a access time and calculates
 * values for the CMDLNG and CMDDLAY registers.
 */
static void
stp4020_calc_speed(int bus_speed, int ns, int *length, int *delay)
{
	int result;

	if (ns < STP4020_MEM_SPEED_MIN)
		ns = STP4020_MEM_SPEED_MIN;
	else if (ns > STP4020_MEM_SPEED_MAX)
		ns = STP4020_MEM_SPEED_MAX;
	result = ns*(bus_speed/1000);
	if (result % 1000000)
		result = result/1000000 + 1;
	else
		result /= 1000000;
	*length = result;

	/* the sbus frequency range is limited, so we can keep this simple */
	*delay = ns <= STP4020_MEM_SPEED_MIN? 1 : 2;
}

static void
stp4020_map_window(struct stp4020_socket *h, int win, int speed)
{
	int v, length, delay;

	/*
	 * According to the PC Card standard 300ns access timing should be
	 * used for attribute memory access. Our pcmcia framework does not
	 * seem to propagate timing information, so we use that
	 * everywhere.
	 */
	stp4020_calc_speed(speed, (win==STP_WIN_ATTR)? 300 : 100, &length, &delay);

	/*
	 * Fill in the Address Space Select and Base Address
	 * fields of this windows control register 0.
	 */
	v = ((delay << STP4020_WCR0_CMDDLY_S)&STP4020_WCR0_CMDDLY_M)
	    | ((length << STP4020_WCR0_CMDLNG_S)&STP4020_WCR0_CMDLNG_M);
	switch (win) {
	case STP_WIN_ATTR:
		v |= STP4020_WCR0_ASPSEL_AM;
		break;
	case STP_WIN_MEM:
		v |= STP4020_WCR0_ASPSEL_CM;
		break;
	case STP_WIN_IO:
		v |= STP4020_WCR0_ASPSEL_IO;
		break;
	}
	v |= (STP4020_ADDR2PAGE(0) & STP4020_WCR0_BASE_M);
	stp4020_wr_winctl(h, win, STP4020_WCR0_IDX, v);
	stp4020_wr_winctl(h, win, STP4020_WCR1_IDX, 1<<STP4020_WCR1_WAITREQ_S);
}

int
stp4020_chip_mem_alloc(pch, size, pcmhp)
	pcmcia_chipset_handle_t pch;
	bus_size_t size;
	struct pcmcia_mem_handle *pcmhp;
{
	struct stp4020_socket *h = (struct stp4020_socket *)pch;

	/* we can not do much here, defere work to _mem_map */
	pcmhp->memt = h->pcmciat;
	pcmhp->size = size;
	pcmhp->addr = 0;
	pcmhp->mhandle = 0;
	pcmhp->realsize = size;

	return (0);
}

void
stp4020_chip_mem_free(pch, pcmhp)
	pcmcia_chipset_handle_t pch;
	struct pcmcia_mem_handle *pcmhp;
{
}

int
stp4020_chip_mem_map(pch, kind, card_addr, size, pcmhp, offsetp, windowp)
	pcmcia_chipset_handle_t pch;
	int kind;
	bus_addr_t card_addr;
	bus_size_t size;
	struct pcmcia_mem_handle *pcmhp;
	bus_size_t *offsetp;
	int *windowp;
{
	struct stp4020_socket *h = (struct stp4020_socket *)pch;
	int win = (kind&PCMCIA_MEM_ATTR)? STP_WIN_ATTR : STP_WIN_MEM;

	pcmhp->memt = h->pcmciat;
	bus_space_subregion(h->pcmciat, h->windows[win].winaddr, card_addr, size, &pcmhp->memh);
#ifdef SUN4U
	if ((u_int8_t)pcmhp->memh._asi == ASI_PHYS_NON_CACHED)
		pcmhp->memh._asi = ASI_PHYS_NON_CACHED_LITTLE;
	else if ((u_int8_t)pcmhp->memh._asi == ASI_PRIMARY)
		pcmhp->memh._asi = ASI_PRIMARY_LITTLE;
#endif
	pcmhp->size = size;
	pcmhp->realsize = STP4020_WINDOW_SIZE - card_addr;
	*offsetp = 0;
	*windowp = 0;

	return (0);
}

void
stp4020_chip_mem_unmap(pch, win)
	pcmcia_chipset_handle_t pch;
	int win;
{
}

int
stp4020_chip_io_alloc(pch, start, size, align, pcihp)
	pcmcia_chipset_handle_t pch;
	bus_addr_t start;
	bus_size_t size;
	bus_size_t align;
	struct pcmcia_io_handle *pcihp;
{
	struct stp4020_socket *h = (struct stp4020_socket *)pch;

	pcihp->iot = h->pcmciat;
	pcihp->ioh = h->windows[STP_WIN_IO].winaddr;
	return 0;
}

void
stp4020_chip_io_free(pch, pcihp)
	pcmcia_chipset_handle_t pch;
	struct pcmcia_io_handle *pcihp;
{
}

int
stp4020_chip_io_map(pch, width, offset, size, pcihp, windowp)
	pcmcia_chipset_handle_t pch;
	int width;
	bus_addr_t offset;
	bus_size_t size;
	struct pcmcia_io_handle *pcihp;
	int *windowp;
{
	struct stp4020_socket *h = (struct stp4020_socket *)pch;

	pcihp->iot = h->pcmciat;
	bus_space_subregion(h->pcmciat, h->windows[STP_WIN_IO].winaddr, offset, size, &pcihp->ioh);
#ifdef SUN4U
	if ((u_int8_t)pcihp->ioh._asi == ASI_PHYS_NON_CACHED)
		pcihp->ioh._asi = ASI_PHYS_NON_CACHED_LITTLE;
	else if ((u_int8_t)pcihp->ioh._asi == ASI_PRIMARY)
		pcihp->ioh._asi = ASI_PRIMARY_LITTLE;
#endif
	*windowp = 0;
	return 0;
}

void
stp4020_chip_io_unmap(pch, win)
	pcmcia_chipset_handle_t pch;
	int win;
{
}

void
stp4020_chip_socket_enable(pch)
	pcmcia_chipset_handle_t pch;
{
	struct stp4020_socket *h = (struct stp4020_socket *)pch;
	int i, v;

	/* this bit is mostly stolen from pcic_attach_card */

	/* Power down the socket to reset it, clear the card reset pin */
	stp4020_wr_sockctl(h, STP4020_ICR1_IDX, 0);

	/*
	 * wait 300ms until power fails (Tpf).  Then, wait 100ms since
	 * we are changing Vcc (Toff).
	 */
	stp4020_delay((300 + 100) * 1000);

	/* Power up the socket */
	v = STP4020_ICR1_MSTPWR;
	stp4020_wr_sockctl(h, STP4020_ICR1_IDX, v);

	/*
	 * wait 100ms until power raise (Tpr) and 20ms to become
	 * stable (Tsu(Vcc)).
	 */
	stp4020_delay((100 + 20) * 1000);

	v |= STP4020_ICR1_PCIFOE|STP4020_ICR1_VPP1_VCC;
	stp4020_wr_sockctl(h, STP4020_ICR1_IDX, v);

	/*
	 * hold RESET at least 10us.
	 */
	delay(10);

	/* Clear reset flag */
	v = stp4020_rd_sockctl(h, STP4020_ICR0_IDX);
	v &= ~STP4020_ICR0_RESET;
	stp4020_wr_sockctl(h, STP4020_ICR0_IDX, v);

	/* wait 20ms as per pc card standard (r2.01) section 4.3.6 */
	stp4020_delay(20000);

	/* Wait for the chip to finish initializing (5 seconds max) */
	for (i = 10000; i > 0; i--) {
		v = stp4020_rd_sockctl(h, STP4020_ISR0_IDX);
		if ((v & STP4020_ISR0_RDYST) != 0)
			break;
		delay(500);
	}
	if (i <= 0) {
		char bits[64];
		bitmask_snprintf(stp4020_rd_sockctl(h, STP4020_ISR0_IDX),
				 STP4020_ISR0_IOBITS, bits, sizeof(bits));
		printf("stp4020_chip_socket_enable: not ready: status %s\n",
			bits);
		return;
	}

	v = stp4020_rd_sockctl(h, STP4020_ICR0_IDX);

	/*
	 * Check the card type.
	 * Enable socket I/O interrupts for IO cards.
	 */
	if (pcmcia_card_gettype(h->pcmcia) == PCMCIA_IFTYPE_IO) {
		v &= ~(STP4020_ICR0_IOILVL|STP4020_ICR0_IFTYPE);
		v |= STP4020_ICR0_IFTYPE_IO|STP4020_ICR0_IOIE
		    |STP4020_ICR0_SPKREN;
		v |= h->sbus_intno ? STP4020_ICR0_IOILVL_SB1
				   : STP4020_ICR0_IOILVL_SB0;
		h->int_enable = v;
		h->int_disable = v & ~STP4020_ICR0_IOIE;
		DPRINTF(("%s: configuring card for IO useage\n", h->sc->sc_dev.dv_xname));
	} else {
		v &= ~(STP4020_ICR0_IOILVL|STP4020_ICR0_IFTYPE
		    |STP4020_ICR0_SPKREN);
		v |= STP4020_ICR0_IFTYPE_MEM;
		h->int_enable = h->int_disable = v;
		DPRINTF(("%s: configuring card for IO useage\n", h->sc->sc_dev.dv_xname));
		DPRINTF(("%s: configuring card for MEM ONLY useage\n", h->sc->sc_dev.dv_xname));
	}
	stp4020_wr_sockctl(h, STP4020_ICR0_IDX, v);
}

void
stp4020_chip_socket_disable(pch)
	pcmcia_chipset_handle_t pch;
{
	struct stp4020_socket *h = (struct stp4020_socket *)pch;
	int v;

	/*
	 * Disable socket I/O interrupts.
	 */
	v = stp4020_rd_sockctl(h, STP4020_ICR0_IDX);
	v &= ~(STP4020_ICR0_IOIE | STP4020_ICR0_IOILVL);
	stp4020_wr_sockctl(h, STP4020_ICR0_IDX, v);

	/* Power down the socket */
	stp4020_wr_sockctl(h, STP4020_ICR1_IDX, 0);

	/*
	 * wait 300ms until power fails (Tpf).
	 */
	stp4020_delay(300 * 1000);
}

void *
stp4020_chip_intr_establish(pch, pf, ipl, handler, arg)
	pcmcia_chipset_handle_t pch;
	struct pcmcia_function *pf;
	int ipl;
	int (*handler) __P((void *));
	void *arg;
{
	struct stp4020_socket *h = (struct stp4020_socket *)pch;

	/* only one interrupt handler per slot */
	if (h->intrhandler != NULL) return NULL;

	h->intrhandler = handler;
	h->intrarg = arg;
	h->softint = softintr_establish(ipl, stp4020_intr_dispatch, h);
	return h->softint;
}

void
stp4020_chip_intr_disestablish(pch, ih)
	pcmcia_chipset_handle_t pch;
	void *ih;
{
	struct stp4020_socket *h = (struct stp4020_socket *)pch;

	h->intrhandler = NULL;
	h->intrarg = NULL;
	if (h->softint) {
		softintr_disestablish(h->softint);
		h->softint = NULL;
	}
}

/*
 * Delay and possibly yield CPU.
 * XXX - assumes a context
 */
void
stp4020_delay(ms)
	unsigned int ms;
{
	unsigned int ticks;

	/* Convert to ticks */
	ticks = (ms * hz ) / 1000000;

	if (cold || ticks == 0) {
		delay(ms);
		return;
	}

#ifdef DIAGNOSTIC
	if (ticks > 60*hz)
		panic("stp4020: preposterous delay: %u", ticks);
#endif
	tsleep(&ticks, 0, "stp4020_delay", ticks);
}

#ifdef STP4020_DEBUG
void
stp4020_dump_regs(h)
	struct stp4020_socket *h;
{
	char bits[64];
	/*
	 * Dump control and status registers.
	 */
	printf("socket[%d] registers:\n", h->sock);
	bitmask_snprintf(stp4020_rd_sockctl(h, STP4020_ICR0_IDX),
			 STP4020_ICR0_BITS, bits, sizeof(bits));
	printf("\tICR0=%s\n", bits);

	bitmask_snprintf(stp4020_rd_sockctl(h, STP4020_ICR1_IDX),
			 STP4020_ICR1_BITS, bits, sizeof(bits));
	printf("\tICR1=%s\n", bits);

	bitmask_snprintf(stp4020_rd_sockctl(h, STP4020_ISR0_IDX),
			 STP4020_ISR0_IOBITS, bits, sizeof(bits));
	printf("\tISR0=%s\n", bits);

	bitmask_snprintf(stp4020_rd_sockctl(h, STP4020_ISR1_IDX),
			 STP4020_ISR1_BITS, bits, sizeof(bits));
	printf("\tISR1=%s\n", bits);
}
#endif /* STP4020_DEBUG */
