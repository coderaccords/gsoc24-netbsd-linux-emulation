/*	$NetBSD: btbc.c,v 1.3 2007/10/02 05:37:51 plunky Exp $	*/
/*
 * Copyright (c) 2007 KIYOHARA Takashi
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
/*
 * This driver is support to the AnyCom BlueCard.  written with reference to
 * Linux driver: (drivers/bluetooth/bluecard_cs.c)
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: btbc.c,v 1.3 2007/10/02 05:37:51 plunky Exp $");

#include <sys/param.h>
#include <sys/callout.h>
#include <sys/device.h>
#include <sys/errno.h>
#include <sys/kernel.h>
#include <sys/mbuf.h>
#include <sys/proc.h>

#include <machine/bus.h>
#include <machine/intr.h>

#include <dev/pcmcia/pcmciareg.h>
#include <dev/pcmcia/pcmciavar.h>
#include <dev/pcmcia/pcmciadevs.h>

#include <netbt/bluetooth.h>
#include <netbt/hci.h>

#include <dev/pcmcia/bluecardreg.h>


/* sc_state */				/* receiving */
#define BTBC_RECV_PKT_TYPE	0		/* packet type */
#define BTBC_RECV_ACL_HDR	1		/* acl header */
#define BTBC_RECV_SCO_HDR	2		/* sco header */
#define BTBC_RECV_EVENT_HDR	3		/* event header */
#define BTBC_RECV_ACL_DATA	4		/* acl packet data */
#define BTBC_RECV_SCO_DATA	5		/* sco packet data */
#define BTBC_RECV_EVENT_DATA	6		/* event packet data */

/* sc_flags */
#define BTBC_SLEEPING		(1 << 0)	/* but not with the fishes */

/* Default baud rate: 57600, 115200, 230400 or 460800 */
#define BTBC_DEFAULT_BAUDRATE	230400

struct btbc_softc {
	struct device sc_dev;			/* required */

	struct pcmcia_function *sc_pf;		/* our PCMCIA function */
	struct pcmcia_io_handle sc_pcioh;	/* PCMCIA i/o space info */
	void *sc_powerhook;			/* power hook descriptor */
	int sc_flags;				/* flags */

	struct hci_unit sc_unit;		/* Bluetooth HCI Unit */

	/* hardware interrupt */
	void *sc_intr;				/* cookie */
	int sc_state;				/* receive state */
	int sc_want;				/* how much we want */
	struct mbuf *sc_rxp;			/* incoming packet */
	struct mbuf *sc_txp;			/* outgoing packet */
	int sc_txstate;
#define TXBUF1_EMPTY	(1 << 0)
#define TXBUF2_EMPTY	(1 << 1)
#define TXBUF_MASK	(1 << 2)

	callout_t sc_ledch;			/* callout handler for LED */
	uint8_t sc_ctrlreg;			/* value for control register */
};

static int btbc_match(struct device *, struct cfdata *, void *);
static void btbc_attach(struct device *, struct device *, void *);
static int btbc_detach(struct device *, int);
static void btbc_power(int, void *);

static void btbc_activity_led_timeout(void *);
static void btbc_enable_activity_led(struct btbc_softc *);
static int btbc_read(struct btbc_softc *, uint32_t, uint8_t *, int);
static int btbc_write(struct btbc_softc *, uint32_t, uint8_t *, int);
static int btbc_set_baudrate(struct btbc_softc *, int);
static void btbc_receive(struct btbc_softc *, uint32_t);
static void btbc_transmit(struct btbc_softc *);
static int btbc_intr(void *);

static void btbc_start(struct hci_unit *);
static int btbc_enable(struct hci_unit *);
static void btbc_disable(struct hci_unit *);

CFATTACH_DECL(btbc, sizeof(struct btbc_softc),
    btbc_match, btbc_attach, btbc_detach, NULL);


/* ARGSUSED */
static int
btbc_match(struct device *parent, struct cfdata *match, void *aux)
{
	struct pcmcia_attach_args *pa = aux;

	if (pa->manufacturer == PCMCIA_VENDOR_ANYCOM)
		if ((pa->product == PCMCIA_PRODUCT_ANYCOM_LSE041) ||
		    (pa->product == PCMCIA_PRODUCT_ANYCOM_LSE039) ||
		    (pa->product == PCMCIA_PRODUCT_ANYCOM_LSE139))
			return 1;
	return 0;
}

static int
btbc_pcmcia_validate_config(struct pcmcia_config_entry *cfe)
{

	if (cfe->iftype != PCMCIA_IFTYPE_IO ||
	    cfe->num_iospace < 1 || cfe->num_iospace > 2)
		return EINVAL;
	return 0;
}

/* ARGSUSED */
static void
btbc_attach(struct device *parent, struct device *self, void *aux)
{
	struct btbc_softc *sc = (struct btbc_softc *)self;
	struct pcmcia_attach_args *pa = aux;
	struct pcmcia_config_entry *cfe;
	int error;

	sc->sc_pf = pa->pf;

	if ((error = pcmcia_function_configure(pa->pf,
	    btbc_pcmcia_validate_config)) != 0) {
		aprint_error("%s: configure failed, error=%d\n",
		    self->dv_xname, error);
		return;
	}

	cfe = pa->pf->cfe;
	sc->sc_pcioh = cfe->iospace[0].handle;

	/* Attach Bluetooth unit */
	sc->sc_unit.hci_softc = sc;
	sc->sc_unit.hci_devname = sc->sc_dev.dv_xname;
	sc->sc_unit.hci_enable = btbc_enable;
	sc->sc_unit.hci_disable = btbc_disable;
	sc->sc_unit.hci_start_cmd = btbc_start;
	sc->sc_unit.hci_start_acl = btbc_start;
	sc->sc_unit.hci_start_sco = btbc_start;
	sc->sc_unit.hci_ipl = makeiplcookie(IPL_TTY);
	hci_attach(&sc->sc_unit);

	/* establish a power change hook */
	sc->sc_powerhook = powerhook_establish(sc->sc_dev.dv_xname,
	    btbc_power, sc);

	callout_init(&sc->sc_ledch, 0);

	return;
}

/* ARGSUSED */
static int
btbc_detach(struct device *self, int flags)
{
	struct btbc_softc *sc = (struct btbc_softc *)self;
	int err = 0;

	btbc_disable(&sc->sc_unit);

	if (sc->sc_powerhook) {
		powerhook_disestablish(sc->sc_powerhook);
		sc->sc_powerhook = NULL;
	}

	callout_stop(&sc->sc_ledch);

	hci_detach(&sc->sc_unit);

	pcmcia_function_unconfigure(sc->sc_pf);

	return err;
}

static void
btbc_power(int why, void *arg)
{
	struct btbc_softc *sc = arg;

	switch(why) {
	case PWR_SUSPEND:
	case PWR_STANDBY:
		if (sc->sc_unit.hci_flags & BTF_RUNNING) {
			hci_detach(&sc->sc_unit);

			sc->sc_flags |= BTBC_SLEEPING;
			printf_nolog("%s: sleeping\n", sc->sc_dev.dv_xname);
		}
		break;

	case PWR_RESUME:
		if (sc->sc_flags & BTBC_SLEEPING) {
			printf_nolog("%s: waking up\n", sc->sc_dev.dv_xname);
			sc->sc_flags &= ~BTBC_SLEEPING;

			memset(&sc->sc_unit, 0, sizeof(sc->sc_unit));
			sc->sc_unit.hci_softc = sc;
			sc->sc_unit.hci_devname = sc->sc_dev.dv_xname;
			sc->sc_unit.hci_enable = btbc_enable;
			sc->sc_unit.hci_disable = btbc_disable;
			sc->sc_unit.hci_start_cmd = btbc_start;
			sc->sc_unit.hci_start_acl = btbc_start;
			sc->sc_unit.hci_start_sco = btbc_start;
			sc->sc_unit.hci_ipl = makeiplcookie(IPL_TTY);
			hci_attach(&sc->sc_unit);
		}
		break;

	case PWR_SOFTSUSPEND:
	case PWR_SOFTSTANDBY:
	case PWR_SOFTRESUME:
		break;
	}
}

static void
btbc_activity_led_timeout(void *arg)
{
	struct btbc_softc *sc = arg;
	uint8_t id;

	id = bus_space_read_1(sc->sc_pcioh.iot, sc->sc_pcioh.ioh,
	    BLUECARD_LEDCONTROL);
	if (id & 0x20)
		/* Disable activity LED */
		bus_space_write_1(sc->sc_pcioh.iot, sc->sc_pcioh.ioh,
		    BLUECARD_LEDCONTROL, 0x08 | 0x20);
	else
		/* Disable power LED */
		bus_space_write_1(sc->sc_pcioh.iot, sc->sc_pcioh.ioh,
		    BLUECARD_LEDCONTROL, 0x00);
}

static void
btbc_enable_activity_led(struct btbc_softc *sc)
{
	uint8_t id;

	id = bus_space_read_1(sc->sc_pcioh.iot, sc->sc_pcioh.ioh,
	    BLUECARD_LEDCONTROL);
	if (id & 0x20) {
		/* Enable activity LED */
		bus_space_write_1(sc->sc_pcioh.iot, sc->sc_pcioh.ioh,
		    BLUECARD_LEDCONTROL, 0x10 | 0x40);

		/* Stop the LED after hz/4 */
		callout_reset(&sc->sc_ledch, hz / 4,
		    btbc_activity_led_timeout, sc);
	} else {
		/* Enable power LED */
		bus_space_write_1(sc->sc_pcioh.iot, sc->sc_pcioh.ioh,
		    BLUECARD_LEDCONTROL, 0x08 | 0x20);

		/* Stop the LED after HZ/2 */
		callout_reset(&sc->sc_ledch, hz / 2,
		    btbc_activity_led_timeout, sc);
	}
}

static int
btbc_read(struct btbc_softc *sc, uint32_t offset, uint8_t *buf, int buflen)
{
	int i, n, len;

	bus_space_write_1(sc->sc_pcioh.iot, sc->sc_pcioh.ioh,
	    BLUECARD_COMMAND, BLUECARD_COMMAND_RXWIN1);
	len = bus_space_read_1(sc->sc_pcioh.iot, sc->sc_pcioh.ioh, offset);

	n = 0;
	i = 1;
	while (n < len) {
		if (i == 16) {
			bus_space_write_1(sc->sc_pcioh.iot, sc->sc_pcioh.ioh,
			    BLUECARD_COMMAND, BLUECARD_COMMAND_RXWIN2);
			i = 0;
		}

		buf[n] = bus_space_read_1(sc->sc_pcioh.iot, sc->sc_pcioh.ioh,
		    offset + i);
		i++;
		if (++n > buflen)
			break;
	}
	return len;
}

static int
btbc_write(struct btbc_softc *sc, uint32_t offset, uint8_t *buf, int buflen)
{
        int i, actual;

	actual = (buflen > 15) ? 15 : buflen;
	bus_space_write_1(sc->sc_pcioh.iot, sc->sc_pcioh.ioh, offset, actual);
	for (i = 0; i < actual; i++)
		bus_space_write_1(sc->sc_pcioh.iot, sc->sc_pcioh.ioh,
		    offset + i + 1, buf[i]);
	return actual;
}

/*
 * send Ericsson baud rate command
 */
static int
btbc_set_baudrate(struct btbc_softc *sc, int baud)
{
	struct hci_unit *unit = &sc->sc_unit;
	hci_cmd_hdr_t *p;
	struct mbuf *m;
	const uint16_t opcode = htole16(HCI_CMD_ERICSSON_SET_UART_BAUD_RATE);
	uint8_t param;

	m = m_gethdr(M_WAIT, MT_DATA);

	switch (baud) {
	case 460800:
		param = 0x00;
		break;

	case 230400:
		param = 0x01;
		break;

        case 115200:
		param = 0x02;
		break;

	case 57600:
	default:
		param = 0x03;
		break;
	}

	p = mtod(m, hci_cmd_hdr_t *);
	p->type = HCI_CMD_PKT;
	p->opcode = opcode;
	p->length = sizeof(param);
	m->m_pkthdr.len = m->m_len = sizeof(hci_cmd_hdr_t);
	m_copyback(m, sizeof(hci_cmd_hdr_t), p->length, &param);

	MBUFQ_ENQUEUE(&unit->hci_cmdq, m);
	btbc_start(unit);

	return 0;
}

static void
btbc_receive(struct btbc_softc *sc, uint32_t offset)
{
	struct mbuf *m = sc->sc_rxp;
	int count, space = 0, i;
	uint8_t buf[31];

	btbc_enable_activity_led(sc);

	/*
	 * If we already started a packet, find the
	 * trailing end of it.
	 */
	if (m) {
		while (m->m_next)
			m = m->m_next;

		space = M_TRAILINGSPACE(m);
	}

	count = btbc_read(sc, offset, buf, sizeof(buf));
	i = 0;

	while (i < count) {
		if (space == 0) {
			if (m == NULL) {
				/* new packet */
				MGETHDR(m, M_DONTWAIT, MT_DATA);
				if (m == NULL) {
					printf("%s: out of memory\n",
						sc->sc_dev.dv_xname);
					++sc->sc_unit.hci_stats.err_rx;
					return;		/* (lost sync) */
				}

				sc->sc_rxp = m;
				m->m_pkthdr.len = m->m_len = 0;
				space = MHLEN;

				sc->sc_state = BTBC_RECV_PKT_TYPE;
				sc->sc_want = 1;
			} else {
				/* extend mbuf */
				MGET(m->m_next, M_DONTWAIT, MT_DATA);
				if (m->m_next == NULL) {
					printf("%s: out of memory\n",
						sc->sc_dev.dv_xname);
					++sc->sc_unit.hci_stats.err_rx;
					return;		/* (lost sync) */
				}

				m = m->m_next;
				m->m_len = 0;
				space = MLEN;

				if (sc->sc_want > MINCLSIZE) {
					MCLGET(m, M_DONTWAIT);
					if (m->m_flags & M_EXT)
						space = MCLBYTES;
				}
			}
		}

		mtod(m, uint8_t *)[m->m_len++] = buf[i];
		space--;
		sc->sc_rxp->m_pkthdr.len++;
		sc->sc_unit.hci_stats.byte_rx++;

		sc->sc_want--;
		if (sc->sc_want > 0) {
			i++;
			continue; /* want more */
		}

		switch (sc->sc_state) {
		case BTBC_RECV_PKT_TYPE:		/* Got packet type */
			switch (buf[i]) {
			case 0x00:	/* init packet */
				m_freem(sc->sc_rxp);
				sc->sc_rxp = NULL;
				break;

			case HCI_ACL_DATA_PKT:
				sc->sc_state = BTBC_RECV_ACL_HDR;
				sc->sc_want = sizeof(hci_acldata_hdr_t) - 1;
				break;

			case HCI_SCO_DATA_PKT:
				sc->sc_state = BTBC_RECV_SCO_HDR;
				sc->sc_want = sizeof(hci_scodata_hdr_t) - 1;
				break;

			case HCI_EVENT_PKT:
				sc->sc_state = BTBC_RECV_EVENT_HDR;
				sc->sc_want = sizeof(hci_event_hdr_t) - 1;
				break;

			default:
				printf("%s: Unknown packet type=%#x!\n",
					sc->sc_dev.dv_xname, buf[i]);
				++sc->sc_unit.hci_stats.err_rx;
				m_freem(sc->sc_rxp);
				sc->sc_rxp = NULL;
				return;		/* (lost sync) */
			}

			break;

		/*
		 * we assume (correctly of course :) that the packet headers
		 * all fit into a single pkthdr mbuf
		 */
		case BTBC_RECV_ACL_HDR:		/* Got ACL Header */
			sc->sc_state = BTBC_RECV_ACL_DATA;
			sc->sc_want = mtod(m, hci_acldata_hdr_t *)->length;
			sc->sc_want = le16toh(sc->sc_want);
			break;

		case BTBC_RECV_SCO_HDR:		/* Got SCO Header */
			sc->sc_state = BTBC_RECV_SCO_DATA;
			sc->sc_want =  mtod(m, hci_scodata_hdr_t *)->length;
			break;

		case BTBC_RECV_EVENT_HDR:	/* Got Event Header */
			sc->sc_state = BTBC_RECV_EVENT_DATA;
			sc->sc_want =  mtod(m, hci_event_hdr_t *)->length;
			break;

		case BTBC_RECV_ACL_DATA:	/* ACL Packet Complete */
			hci_input_acl(&sc->sc_unit, sc->sc_rxp);
			sc->sc_unit.hci_stats.acl_rx++;
			sc->sc_rxp = m = NULL;
			space = 0;
			break;

		case BTBC_RECV_SCO_DATA:	/* SCO Packet Complete */
			hci_input_sco(&sc->sc_unit, sc->sc_rxp);
			sc->sc_unit.hci_stats.sco_rx++;
			sc->sc_rxp = m = NULL;
			space = 0;
			break;

		case BTBC_RECV_EVENT_DATA:	/* Event Packet Complete */
			sc->sc_unit.hci_stats.evt_rx++;
			hci_input_event(&sc->sc_unit, sc->sc_rxp);
			sc->sc_rxp = m = NULL;
			space = 0;
			break;

		default:
			panic("%s: invalid state %d!\n",
				sc->sc_dev.dv_xname, sc->sc_state);
		}
		i++;
	}
}

/*
 * write data from current packet to Transmit FIFO.
 * restart when done.
 */
static void
btbc_transmit(struct btbc_softc *sc)
{
	hci_cmd_hdr_t *p;
	struct mbuf *m;
	int count, rlen, set_baudrate, n, s;
	uint32_t offset, command;
	uint8_t *rptr;

	m = sc->sc_txp;
	if (m == NULL) {
		sc->sc_unit.hci_flags &= ~BTF_XMIT;
		btbc_start(&sc->sc_unit);
		return;
	}

	set_baudrate = 0;
	p = mtod(m, hci_cmd_hdr_t *);
	if ((void *)m->m_pktdat == (void *)p) {
		const uint16_t opcode =
		    htole16(HCI_CMD_ERICSSON_SET_UART_BAUD_RATE);

		if (p->type == HCI_CMD_PKT &&
		    p->opcode == opcode &&
		    p->length == 1) {
			set_baudrate = 1;
			sc->sc_txp = NULL;	/* safe reentrant */
		}
	}

	count = 0;
	rlen = 0;
	rptr = mtod(m, uint8_t *);
	for(;;) {
		if (rlen >= m->m_len) {
			m = m->m_next;
			if (m == NULL) {
				m = sc->sc_txp;
				sc->sc_txp = NULL;

				if (M_GETCTX(m, void *) == NULL)
					m_freem(m);
				else
					hci_complete_sco(&sc->sc_unit, m);

				break;
			}

			rlen = 0;
			rptr = mtod(m, uint8_t *);
			continue;
		}

		s = splhigh();
		if (sc->sc_txstate & TXBUF_MASK) {
			if (sc->sc_txstate & TXBUF2_EMPTY) {
				offset = BLUECARD_BUF2;
				command = BLUECARD_COMMAND_TXBUF2;
				sc->sc_txstate &= ~(TXBUF2_EMPTY | TXBUF_MASK);
			} else {
				splx(s);
				m_adj(m, rlen);
				break;
			}
		} else {
			if (sc->sc_txstate & TXBUF1_EMPTY) {
				offset = BLUECARD_BUF1;
				command = BLUECARD_COMMAND_TXBUF1;
				sc->sc_txstate &= ~TXBUF1_EMPTY;
				sc->sc_txstate |= TXBUF_MASK;
			} else {
				splx(s);
				m_adj(m, rlen);
				break;
			}
		}
		splx(s);

		if (set_baudrate) {
			/* Disable RTS */
			sc->sc_ctrlreg |= BLUECARD_CONTROL_RTS;
			bus_space_write_1(sc->sc_pcioh.iot, sc->sc_pcioh.ioh,
			    BLUECARD_CONTROL, sc->sc_ctrlreg);
		}

		/* Activate LED */
		btbc_enable_activity_led(sc);

		/* Send frame */
		n = btbc_write(sc, offset, rptr, m->m_len - rlen);
		count += n;
		rlen += n;
		rptr += n;

		/* Tell the FPGA to send the data */
		bus_space_write_1(sc->sc_pcioh.iot, sc->sc_pcioh.ioh,
		    BLUECARD_COMMAND, command);

		if (set_baudrate) {
			unsigned char baud_reg;

			switch (*(uint8_t *)(p + 1)) {
			case 0x00:	/* baud rate 460800 */
				baud_reg = BLUECARD_CONTROL_BAUDRATE_460800;
				break;
			case 0x01:	/* baud rate 230400 */
				baud_reg = BLUECARD_CONTROL_BAUDRATE_230400;
				break;
			case 0x02:	/* baud rate 115200 */
				baud_reg = BLUECARD_CONTROL_BAUDRATE_115200;
				break;
			case 0x03:	/* baud rate 57600 */
			default:
				baud_reg = BLUECARD_CONTROL_BAUDRATE_57600;
				break;
			}

			/* Wait until the command reaches the baseband */
			tsleep(sc, PCATCH, "btbc_wait", hz / 5);

			/* Set baud on baseband */
			sc->sc_ctrlreg &= ~BLUECARD_CONTROL_BAUDRATE_MASK;
			sc->sc_ctrlreg |= baud_reg;
			bus_space_write_1(sc->sc_pcioh.iot, sc->sc_pcioh.ioh,
			    BLUECARD_CONTROL, sc->sc_ctrlreg);

			/* Enable RTS */
			sc->sc_ctrlreg &= ~BLUECARD_CONTROL_RTS;
			bus_space_write_1(sc->sc_pcioh.iot, sc->sc_pcioh.ioh,
			    BLUECARD_CONTROL, sc->sc_ctrlreg);

			/* Wait before the next HCI packet can be send */
			tsleep(sc, PCATCH, "btbc_wait", hz);

			m_freem(m);
			break;
		}
	}
	sc->sc_unit.hci_stats.byte_tx += count;
}

static int
btbc_intr(void *arg)
{
	struct btbc_softc *sc = arg;
	int handled = 0;
	uint8_t isr;

	isr = bus_space_read_1(sc->sc_pcioh.iot, sc->sc_pcioh.ioh,
	    BLUECARD_INTERRUPT);
	if (isr != 0x00 && isr != 0xff) {
		if (isr & BLUECARD_INTERRUPT_RXBUF1) {
			isr &= ~BLUECARD_INTERRUPT_RXBUF1;
			handled = 1;
			btbc_receive(sc, BLUECARD_BUF1);
			bus_space_write_1(sc->sc_pcioh.iot, sc->sc_pcioh.ioh,
			    BLUECARD_INTERRUPT, BLUECARD_INTERRUPT_RXBUF1);
			bus_space_write_1(sc->sc_pcioh.iot, sc->sc_pcioh.ioh,
			    BLUECARD_COMMAND, BLUECARD_COMMAND_RXBUF1);
		}
		if (isr & BLUECARD_INTERRUPT_RXBUF2) {
			isr &= ~BLUECARD_INTERRUPT_RXBUF2;
			handled = 1;
			btbc_receive(sc, BLUECARD_BUF2);
			bus_space_write_1(sc->sc_pcioh.iot, sc->sc_pcioh.ioh,
			    BLUECARD_INTERRUPT, BLUECARD_INTERRUPT_RXBUF2);
			bus_space_write_1(sc->sc_pcioh.iot, sc->sc_pcioh.ioh,
			    BLUECARD_COMMAND, BLUECARD_COMMAND_RXBUF2);
		}
		if (isr & BLUECARD_INTERRUPT_TXBUF1) {
			isr &= ~BLUECARD_INTERRUPT_TXBUF1;
			handled = 1;
			sc->sc_txstate |= TXBUF1_EMPTY;
			bus_space_write_1(sc->sc_pcioh.iot, sc->sc_pcioh.ioh,
			    BLUECARD_INTERRUPT, BLUECARD_INTERRUPT_TXBUF1);
			btbc_transmit(sc);
		}
		if (isr & BLUECARD_INTERRUPT_TXBUF2) {
			isr &= ~BLUECARD_INTERRUPT_TXBUF2;
			handled = 1;
			sc->sc_txstate |= TXBUF2_EMPTY;
			bus_space_write_1(sc->sc_pcioh.iot, sc->sc_pcioh.ioh,
			    BLUECARD_INTERRUPT, BLUECARD_INTERRUPT_TXBUF2);
			btbc_transmit(sc);
		}

		if (isr & 0x40) {	/* card eject ? */
			printf("card eject?\n");
			isr &= ~0x40;
			bus_space_write_1(sc->sc_pcioh.iot, sc->sc_pcioh.ioh,
			    BLUECARD_INTERRUPT, 0x40);
		}
		if (isr != 0x00) {
			printf("unknwon intrrupt: isr=0x%x\n", isr);
			bus_space_write_1(sc->sc_pcioh.iot, sc->sc_pcioh.ioh,
			    BLUECARD_INTERRUPT, isr);
		}
	}

	return handled;
}

/*
 * start sending on btbc
 * this should be called only when BTF_XMIT is not set, and
 * we only send cmd packets that are clear to send
 */
static void
btbc_start(struct hci_unit *unit)
{
	struct btbc_softc *sc = unit->hci_softc;
	struct mbuf *m;

	KASSERT((unit->hci_flags & BTF_XMIT) == 0);
	KASSERT(sc->sc_txp == NULL);

	if (MBUFQ_FIRST(&unit->hci_cmdq)) {
		MBUFQ_DEQUEUE(&unit->hci_cmdq, m);
		unit->hci_stats.cmd_tx++;
		M_SETCTX(m, NULL);
		goto start;
	}

	if (MBUFQ_FIRST(&unit->hci_scotxq)) {
		MBUFQ_DEQUEUE(&unit->hci_scotxq, m);
		unit->hci_stats.sco_tx++;
		goto start;
	}

	if (MBUFQ_FIRST(&unit->hci_acltxq)) {
		MBUFQ_DEQUEUE(&unit->hci_acltxq, m);
		unit->hci_stats.acl_tx++;
		M_SETCTX(m, NULL);
		goto start;
	}

	/* Nothing to send */
	return;

start:
	sc->sc_txp = m;
	unit->hci_flags |= BTF_XMIT;
	btbc_transmit(sc);
}

static int
btbc_enable(struct hci_unit *unit)
{
	struct btbc_softc *sc = unit->hci_softc;
	int err;
	uint8_t id, ctrl;

	if (unit->hci_flags & BTF_RUNNING)
		return 0;

	sc->sc_txstate = TXBUF1_EMPTY | TXBUF2_EMPTY;
	sc->sc_intr = pcmcia_intr_establish(sc->sc_pf, IPL_TTY, btbc_intr, sc);
	if (sc->sc_intr == NULL) {
		err = EIO;
		goto fail1;
	}

	err = pcmcia_function_enable(sc->sc_pf);
	if (err)
		goto fail2;

	unit->hci_flags |= BTF_RUNNING;
	unit->hci_flags &= ~BTF_XMIT;

	/* Reset card */
	ctrl = BLUECARD_CONTROL_RESET | BLUECARD_CONTROL_CARDRESET;
	bus_space_write_1(sc->sc_pcioh.iot, sc->sc_pcioh.ioh, BLUECARD_CONTROL,
	    ctrl);

	/* Turn FPGA off */
	bus_space_write_1(sc->sc_pcioh.iot, sc->sc_pcioh.ioh,
	    BLUECARD_CARDRESET, 0x80);

	/* Wait some time */
	tsleep(sc, PCATCH, "btbc_reset", 1);

	/* Turn FPGA on */
	bus_space_write_1(sc->sc_pcioh.iot, sc->sc_pcioh.ioh,
	    BLUECARD_CARDRESET, 0x00);

	/* Activate card */
	ctrl = BLUECARD_CONTROL_ON | BLUECARD_CONTROL_RESPU;
	bus_space_write_1(sc->sc_pcioh.iot, sc->sc_pcioh.ioh, BLUECARD_CONTROL,
	    ctrl);

	tsleep(sc, PCATCH, "btbc_enable", 1);
	sc->sc_ctrlreg = ctrl;

	/* Enable interrupt */
	bus_space_write_1(sc->sc_pcioh.iot, sc->sc_pcioh.ioh,
	    BLUECARD_INTERRUPT, 0xff);
	sc->sc_ctrlreg |= BLUECARD_CONTROL_INTERRUPT;
	bus_space_write_1(sc->sc_pcioh.iot, sc->sc_pcioh.ioh, BLUECARD_CONTROL,
	    sc->sc_ctrlreg);

	id = bus_space_read_1(sc->sc_pcioh.iot,
	    sc->sc_pcioh.ioh, BLUECARD_LEDCONTROL);
	switch (id & 0x0f) {
	case 0x02:
		/* Enable LED */
		bus_space_write_1(sc->sc_pcioh.iot, sc->sc_pcioh.ioh,
		    BLUECARD_LEDCONTROL, 0x08 | 0x20);
		break;

	case 0x03:
		/* Disable RTS */
		ctrl |= BLUECARD_CONTROL_RTS;
		bus_space_write_1(sc->sc_pcioh.iot, sc->sc_pcioh.ioh,
		    BLUECARD_CONTROL, ctrl);

		/* Set baud rate */
		ctrl |= BLUECARD_CONTROL_BAUDRATE_460800;
		bus_space_write_1(sc->sc_pcioh.iot, sc->sc_pcioh.ioh,
		    BLUECARD_CONTROL, ctrl);

		/* Enable RTS */
		ctrl &= ~BLUECARD_CONTROL_RTS;
		bus_space_write_1(sc->sc_pcioh.iot, sc->sc_pcioh.ioh,
		    BLUECARD_CONTROL, ctrl);
		break;
	}

	/* Start the RX buffers */
	bus_space_write_1(sc->sc_pcioh.iot, sc->sc_pcioh.ioh,
	    BLUECARD_COMMAND, BLUECARD_COMMAND_RXBUF1);
	bus_space_write_1(sc->sc_pcioh.iot, sc->sc_pcioh.ioh,
	    BLUECARD_COMMAND, BLUECARD_COMMAND_RXBUF2);

	/* XXX: Control the point at which RTS is enabled */
	bus_space_write_1(sc->sc_pcioh.iot, sc->sc_pcioh.ioh,
	    BLUECARD_RXCONTROL, BLUECARD_RXCONTROL_RTSLEVEL(0x0f) | 1);

	/* Timeout before it is safe to send the first HCI packet */
	tsleep(sc, PCATCH, "btbc_enable", hz * 2);

	btbc_set_baudrate(sc, BTBC_DEFAULT_BAUDRATE);

	return 0;

fail2:
	pcmcia_intr_disestablish(sc->sc_pf, sc->sc_intr);
	sc->sc_intr = NULL;
fail1:
	return err;
}

static void
btbc_disable(struct hci_unit *unit)
{
	struct btbc_softc *sc = unit->hci_softc;

	if ((unit->hci_flags & BTF_RUNNING) == 0)
		return;

	pcmcia_function_disable(sc->sc_pf);

	if (sc->sc_intr) {
		pcmcia_intr_disestablish(sc->sc_pf, sc->sc_intr);
		sc->sc_intr = NULL;
	}

	if (sc->sc_rxp) {
		m_freem(sc->sc_rxp);
		sc->sc_rxp = NULL;
	}

	if (sc->sc_txp) {
		m_freem(sc->sc_txp);
		sc->sc_txp = NULL;
	}

	unit->hci_flags &= ~BTF_RUNNING;

	/* Disable LED */
	bus_space_write_1(sc->sc_pcioh.iot, sc->sc_pcioh.ioh,
	    BLUECARD_LEDCONTROL, 0x00);

	/* Reset card */
	sc->sc_ctrlreg = BLUECARD_CONTROL_RESET | BLUECARD_CONTROL_CARDRESET;
	bus_space_write_1(sc->sc_pcioh.iot, sc->sc_pcioh.ioh, BLUECARD_CONTROL,
	    sc->sc_ctrlreg);

	/* Turn FPGA off */
	bus_space_write_1(sc->sc_pcioh.iot, sc->sc_pcioh.ioh,
	    BLUECARD_CARDRESET, 0x80);
}
