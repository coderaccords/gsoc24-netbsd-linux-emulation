/*
 * Copyright (c) 1991-1993 Regents of the University of California.
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the Computer Systems
 *	Engineering Group at Lawrence Berkeley Laboratory.
 * 4. Neither the name of the University nor of the Laboratory may be used
 *    to endorse or promote products derived from this software without
 *    specific prior written permission.
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
 *	$Id: sb.c,v 1.4 1994/03/02 16:23:10 hpeyerl Exp $
 */

#include "sb.h"
#if NSB > 0

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/ioctl.h>
#include <sys/syslog.h>

#include <machine/cpu.h>
#include <machine/pio.h>

#include <i386/isa/isa.h>
#include <i386/isa/isa_device.h>
#include <i386/isa/icu.h>

#include "sbreg.h"

/*
 * Software state, per SoundBlaster card.
 * The soundblaster has multiple functionality, which we must demultiplex.
 * One approach is to have one major device number for the soundblaster card,
 * and use different minor numbers to indicate which hardware function
 * we want.  This would make for one large driver.  Instead our approach
 * is to partition the design into a set of drivers that share an underlying
 * piece of hardware.  Most things are hard to share, for example, the audio
 * and midi ports.  For audio, we might want to mix two processes' signals,
 * and for midi we might want to merge streams (this is hard due to
 * running status).  Moreover, we should be able to re-use the high-level
 * modules with other kinds of hardware.  In this module, we only handle the
 * most basic communications with the sb card.
 */
struct sb_softc {
#ifdef NEWCONFIG
	struct device sc_dev;		/* base device */
	struct isadev sc_id;		/* ISA device */
	struct  intrhand sc_ih;		/* interrupt vectoring */
#endif
	u_short	sc_open;		/* reference count of open calls */
	u_short sc_dmachan;		/* dma channel */
	u_long	sc_locked;		/* true when doing HS DMA  */
	u_long	sc_base;		/* I/O port base address */
 	u_short	sc_adacmode;		/* low/high speed mode indicator */
#define SB_ADAC_LS 0
#define SB_ADAC_HS 1
 	u_short	sc_adactc;		/* current adac time constant */
	u_long	sc_interrupts;		/* number of interrupts taken */
	void	(*sc_intr)(void*);	/* dma completion intr handler */
	void	(*sc_mintr)(void*, int);/* midi input intr handler */
	void	*sc_arg;		/* arg for sc_intr() */
};

int	sbreset(u_long);

void sb_spkron(struct sb_softc *);
void sb_spkroff(struct sb_softc *);

static int wdsp(u_long base, int v);
static int rdsp(u_long base);

/* XXX */
#define splsb splhigh
/* XXX */
struct sb_softc *sb_softc;

#ifndef NEWCONFIG
struct sb_softc sb_softcs[NSB];
#define at_dma(flags, ptr, cc, chan)	isa_dmastart(flags, ptr, cc, chan)
#endif

struct {
	int wdsp;
	int rdsp;
	int wmidi;
} sberr;

#ifdef NEWCONFIG
int	sbintr(struct sb_softc *);
int	sbprobe(struct device *, struct cfdata *, void *);
void	sbattach(struct device *, struct device *, void *);
void	sbforceintr(void *);

struct cfdriver sbcd =
	{ NULL, "sb", sbprobe, sbattach, sizeof(struct sb_softc) };

int
sbprobe(struct device *parent, struct cfdata *cf,  void *aux)
{
	register struct isa_attach_args *ia = (struct isa_attach_args *)aux;
	register int base = ia->ia_iobase;

	if (!SB_BASE_VALID(base)) {
		printf("sb: configured dma chan %d invalid\n", ia->ia_drq);
		return (0);
	}
	ia->ia_iosize = SB_NPORT;
	if (sbreset(base) < 0) {
		printf("sb: couldn't reset card\n");
		return (0);
	}
	/*
	 * Cannot auto-discover DMA channel.
	 */
	if (!SB_DRQ_VALID(ia->ia_drq)) {
		printf("sb: configured dma chan %d invalid\n", ia->ia_drq);
		return (0);
	}
	/*
	 * If the IRQ wasn't compiled in, auto-detect it.
	 */
	if (ia->ia_irq == IRQUNK) {
		ia->ia_irq = isa_discoverintr(sbforceintr, aux);
		sbreset(base);
		if (!SB_IRQ_VALID(ia->ia_irq)) {
			printf("sb: couldn't auto-detect interrupt");
			return (0);
		}
	} else if (!SB_IRQ_VALID(ia->ia_irq)) {
		int irq = ffs(ia->ia_irq) - 1;
		printf("sb: configured irq %d invalid\n", irq);
	}
	return (15);
}

void
sbforceintr(void *arg)
{
	static char dmabuf;
	struct isa_attach_args *ia = (struct isa_attach_args *)arg;
	int base = ia->ia_iobase;
	/*
	 * Set up a DMA read of one byte.
	 * XXX Note that at this point we haven't called 
	 * at_setup_dmachan().  This is okay because it just
	 * allocates a buffer in case it needs to make a copy,
	 * and it won't need to make a copy for a 1 byte buffer.
	 * (I think that calling at_setup_dmachan() should be optional;
	 * if you don't call it, it will be called the first time
	 * it is needed (and you pay the latency).  Also, you might
	 * never need the buffer anyway.)
	 */
	at_dma(1, &dmabuf, 1, ia->ia_drq);
	if (wdsp(base, SB_DSP_RDMA) == 0) {
		(void)wdsp(base, 0);
		(void)wdsp(base, 0);
	}
}

void
sbattach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	register struct sb_softc *sc = (struct sb_softc *)self;
	struct isa_attach_args *ia = (struct isa_attach_args *)aux;
	register int base = ia->ia_iobase;
	register int vers;

	/* XXX */
	sb_softc = sc;

	sc->sc_base = base;
	sc->sc_dmachan = ia->ia_drq;
	sc->sc_locked = 0;
	isa_establish(&sc->sc_id, &sc->sc_dev);
	sc->sc_ih.ih_fun = sbintr;
	sc->sc_ih.ih_arg = (void *)sc;
	/* XXX DV_TAPE? */
	intr_establish(ia->ia_irq, &sc->sc_ih, DV_TAPE);

	/*
	 * We limit DMA transfers to a page, and use the generic DMA handling
	 * code in isa.c.  This code can end up copying a buffer, but since
	 * the audio driver uses relative small buffers this isn't likely.
	 *
	 * This allocation scheme means that the maximum transfer is limited
	 * by the page size (rather than 64k).  This is reasonable.  For 4K
	 * pages, the transfer time at 48KHz is 4096 / 48000 = 85ms.  This
	 * is plenty long enough to amortize any fixed time overhead.
	 */
	at_setup_dmachan(sc->sc_dmachan, NBPG);

	vers = sbversion(base);
	printf(" dsp v%d.%d\n", vers >> 8, vers & 0xff);
}
#endif

#ifndef NEWCONFIG
int	sbintr(int unit);
int	sbprobe(struct isa_device *dev);
int	sbattach(struct isa_device *dev);

struct isa_driver sbdriver = { sbprobe, sbattach, "sb" };

int
sbprobe(struct isa_device *dev)
{
	register int base = dev->id_iobase;

	if (!SB_BASE_VALID(base)) {
		printf("sb: configured dma chan %d invalid\n", dev->id_drq);
		return (0);
	}
	if (sbreset(base) < 0) {
		printf("sb: couldn't reset card\n");
		return (0);
	}
	/*
	 * Cannot auto-discover DMA channel.
	 */
	if (!SB_DRQ_VALID(dev->id_drq)) {
		printf("sb: configured dma chan %d invalid\n", dev->id_drq);
		return (0);
	}
	/*
	 * If the IRQ wasn't compiled in, auto-detect it.
	 */
	if (dev->id_irq == 0) {
		printf("sb: no irq configured\n");
		return (0);
	} else if (!SB_IRQ_VALID(dev->id_irq)) {
		int irq = ffs(dev->id_irq) - 1;
		printf("sb: configured irq %d invalid\n", irq);
		return (0);
	}
	return (15);
}

#define	UNIT(x)		(minor(x) & 0xf)

int
sbattach(struct isa_device *dev)
{
	int unit = UNIT(dev->id_unit);
	register struct sb_softc *sc = &sb_softcs[unit];
	register int base = dev->id_iobase;
	register int vers;

	/* XXX */
	sb_softc = sc;

	sc->sc_base = base;
	sc->sc_dmachan = dev->id_drq;
	sc->sc_locked = 0;

	vers = sbversion(base);
	printf("sb%d: dsp v%d.%d\n", unit, vers >> 8, vers & 0xff);
}
#endif

struct sb_softc *
sbopen()
{
	struct sb_softc *sc = sb_softc;

	if (sc == 0)
		return 0;

	if (sc->sc_open == 0 && sbreset(sc->sc_base) == 0) {
		sc->sc_open = 1;
		sc->sc_mintr = 0;
		sc->sc_intr = 0;
		return (sc);
	}
	return (0);
}

void
sbclose(struct sb_softc *sc)
{
	sc->sc_open = 0;
	sb_spkroff(sc);
	sc->sc_intr = 0;
	sc->sc_mintr = 0;
	/* XXX this will turn off any dma */
	sbreset(sc->sc_base);
}

/*
 * Write a byte to the dsp.
 * XXX We are at the mercy of the card as we use a
 * polling loop and wait until it can take the byte.
 */
static int
wdsp(u_long base, int v)
{
	register int i;

	for (i = 100; --i >= 0; ) {
		if ((inb(base + SBP_DSP_WSTAT) & SB_DSP_BUSY) != 0)
			continue;
		outb(base + SBP_DSP_WRITE, v);
		return (0);
	}
	++sberr.wdsp;
	return (-1);
}

/*
 * Read a byte from the DSP, using polling.
 */
int
rdsp(u_long base)
{
	register int i;

	for (i = 100; --i >= 0; ) {
		if ((inb(base + SBP_DSP_RSTAT) & SB_DSP_READY) == 0)
			continue;
		return (inb(base + SBP_DSP_READ));
	}
	++sberr.rdsp;
	return (-1);
}

/*
 * Reset the card.
 * Return non-zero if the card isn't detected.
 */
int
sbreset(register u_long base)
{
	register int i;
	/*
	 * See SBK, section 11.3.
	 * We pulse a reset signal into the card.
	 * Gee, what a brilliant hardware design.
	 */
	outb(base + SBP_DSP_RESET, 1);
	DELAY(3);
	outb(base + SBP_DSP_RESET, 0);
	if (rdsp(base) != SB_MAGIC)
		return (-1);
	return (0);
}

/*
 * Turn on the speaker.  The SBK documention says this operation
 * can take up to 1/10 of a second.  Higher level layers should
 * probably let the task sleep for this amount of time after
 * calling here.  Otherwise, things might not work (because
 * wdsp() and rdsp() will probably timeout.)
 *
 * These engineers had their heads up their ass when
 * they designed this card.
 */
void
sb_spkron(struct sb_softc *sc)
{
	(void)wdsp(sc->sc_base, SB_DSP_SPKR_ON);
	DELAY(1000);
}

/*
 * Turn off the speaker; see comment above.
 */
void
sb_spkroff(struct sb_softc *sc)
{
	(void)wdsp(sc->sc_base, SB_DSP_SPKR_OFF);
}

/*
 * Read the version number out of the card.  Return major code
 * in high byte, and minor code in low byte.
 */
int
sbversion(register u_long base)
{
	int v;

	if (wdsp(base, SB_DSP_VERSION) < 0)
		return (0);
	v = rdsp(base) << 8;
	v |= rdsp(base);
	return ((v >= 0) ? v : 0);
}

/*
 * Halt a DMA in progress.  A low-speed transfer can be
 * resumed with sb_contdma().
 */
void
sb_haltdma(struct sb_softc *sc)
{
	if (sc->sc_locked)
		sbreset(sc->sc_base);
	else
		(void)wdsp(sc->sc_base, SB_DSP_HALT);
}

void
sb_contdma(struct sb_softc *sc)
{
	(void)wdsp(sc->sc_base, SB_DSP_CONT);
}

/*
 * Time constant routines follow.  See SBK, section 12.
 * Although they don't come out and say it (in the docs),
 * the card clearly uses a 1MHz countdown timer, as the
 * low-speed formula (p. 12-4) is:
 *	tc = 256 - 10^6 / sr
 * In high-speed mode, the constant is the upper byte of a 16-bit counter,
 * and a 256MHz clock is used:
 *	tc = 65536 - 256 * 10^ 6 / sr
 * Since we can only use the upper byte of the HS TC, the two formulae
 * are equivalent.  (Why didn't they say so?)  E.g.,
 * 	(65536 - 256 * 10 ^ 6 / x) >> 8 = 256 - 10^6 / x
 *
 * The crossover point (from low- to high-speed modes) is different
 * for the SBPRO and SB20.  The table on p. 12-5 gives the following data:
 *
 *				SBPRO			SB20
 *				-----			--------
 * input ls min			4	KHz		4	HJz
 * input ls max			23	KHz		13	KHz
 * input hs max			44.1	KHz		15	KHz
 * output ls min		4	KHz		4	KHz
 * output ls max		23	KHz		23	KHz
 * output hs max		44.1	KHz		44.1	KHz
 */
#define SB_LS_MIN	0x06	/* 4000 Hz */
#ifdef SBPRO
#define SB_ADC_LS_MAX	0xd4	/* 22727 Hz */
#define SB_ADC_HS_MAX	0xe9	/* 43478 Hz */
#else
#define SB_ADC_LS_MAX	0xb3	/* 12987 Hz */
#define SB_ADC_HS_MAX	0xbd	/* 14925 Hz */
#endif
#define SB_DAC_LS_MAX	0xd4	/* 22727 Hz */
#define SB_DAC_HS_MAX	0xe9	/* 43478 Hz */

/*
 * Convert a linear sampling rate into the DAC time constant.
 * Set *mode to indicate the high/low-speed DMA operation.
 * Because of limitations of the card, not all rates are possible.
 * We return the time constant of the closest possible rate.
 * The sampling rate limits are different for the DAC and ADC,
 * so isdac indicates output, and !isdac indicates input.
 */
int
sb_srtotc(int sr, int *mode, int isdac)
{
	register int tc = 256 - 1000000 / sr;

	if (tc < SB_LS_MIN) {
		tc = SB_LS_MIN;
		*mode = SB_ADAC_LS;
	} else if (isdac) {
		if (tc < SB_DAC_LS_MAX)
			*mode = SB_ADAC_LS;
		else {
			*mode = SB_ADAC_HS;
			if (tc > SB_DAC_HS_MAX)
				tc = SB_DAC_HS_MAX;
		}
	} else {
		if (tc < SB_ADC_LS_MAX)
			*mode = SB_ADAC_LS;
		else {
			*mode = SB_ADAC_HS;
			if (tc > SB_ADC_HS_MAX)
				tc = SB_ADC_HS_MAX;
		}
	}
	return (tc);
}

/*
 * Convert a DAC time constant to a sampling rate.
 * See SBK, section 12.
 */
int
sb_tctosr(int tc)
{
	return (1000000 / (256 - tc));
}

int
sb_set_sr(register struct sb_softc *sc, u_long *sr, int isdac)
{
	register int tc;
	int mode;

	tc = sb_srtotc(*sr, &mode, isdac);
	if (wdsp(sc->sc_base, SB_DSP_TIMECONST) < 0 ||
	    wdsp(sc->sc_base, tc) < 0)
		return (-1);

	*sr = sb_tctosr(tc);
	sc->sc_adacmode = mode;
	sc->sc_adactc = tc;

	return (0);
}

int
sb_round_sr(u_long sr, int isdac)
{
	int mode, tc;

	tc = sb_srtotc(sr, &mode, isdac);
	return (sb_tctosr(tc));
}

int
sb_dma_input(struct sb_softc *sc, void *p, int cc, void (*intr)(), void *arg)
{
	register int base;

	at_dma(1, p, cc, sc->sc_dmachan);
	sc->sc_intr = intr;
	sc->sc_arg = arg;
	base = sc->sc_base;
	--cc;
	if (sc->sc_adacmode == SB_ADAC_LS) {
		if (wdsp(base, SB_DSP_RDMA) < 0 ||
		    wdsp(base, cc) < 0 ||
		    wdsp(base, cc >> 8) < 0) {
			sbreset(sc->sc_base);
			return (EIO);
		}
	} else {
		if (wdsp(base, SB_DSP_BLOCKSIZE) < 0 ||
		    wdsp(base, cc) < 0 ||
		    wdsp(base, cc >> 8) < 0 ||
		    wdsp(base, SB_DSP_HS_INPUT) < 0) {
			sbreset(sc->sc_base);
			return (EIO);
		}
		sc->sc_locked = 1;
	}
	return (0);
}

int
sb_dma_output(struct sb_softc *sc, void *p, int cc, void (*intr)(), void *arg)
{
	register int base;

	at_dma(0, p, cc, sc->sc_dmachan);
	sc->sc_intr = intr;
	sc->sc_arg = arg;
	base = sc->sc_base;
	--cc;
	if (sc->sc_adacmode == SB_ADAC_LS) {
		if (wdsp(base, SB_DSP_WDMA) < 0 ||
		    wdsp(base, cc) < 0 ||
		    wdsp(base, cc >> 8) < 0) {
			sbreset(sc->sc_base);
			return (EIO);
		}
	} else {
		if (wdsp(base, SB_DSP_BLOCKSIZE) < 0 ||
		    wdsp(base, cc) < 0 ||
		    wdsp(base, cc >> 8) < 0 ||
		    wdsp(base, SB_DSP_HS_OUTPUT) < 0) {
			sbreset(sc->sc_base);
			return (EIO);
		}
		sc->sc_locked = 1;
	}
	return (0);
}

/*
 * Only the DSP unit on the sound blaster generates interrupts.
 * There are three cases of interrupt: reception of a midi byte
 * (when mode is enabled), completion of dma transmission, or 
 * completion of a dma reception.  The three modes are mutually
 * exclusive so we know a priori which event has occurred.
 */
#ifdef NEWCONFIG
int
sbintr(struct sb_softc *sc)
{
#else
int
sbintr(int unit)
{
	register struct sb_softc *sc = &sb_softcs[UNIT(unit)];
#endif

	sc->sc_locked = 0;
	/* clear interrupt */
	inb(sc->sc_base + SBP_DSP_RSTAT);
	if (sc->sc_mintr != 0) {
		int c = rdsp(sc->sc_base);
		(*sc->sc_mintr)(sc->sc_arg, c);
	} else if(sc->sc_intr != 0) {
		(*sc->sc_intr)(sc->sc_arg);
	} else
		return (0);
	return (1);
}

/*
 * Enter midi uart mode and arrange for read interrupts
 * to vector to `intr'.  This puts the card in a mode
 * which allows only midi I/O; the card must be reset
 * to leave this mode.  Unfortunately, the card does not
 * use transmit interrupts, so bytes must be output


 * using polling.  To keep the polling overhead to a
 * minimum, output should be driven off a timer.
 * This is a little tricky since only 320us separate
 * consecutive midi bytes.
 */
void
sb_set_midi_mode(struct sb_softc *sc, void (*intr)(), void *arg)
{
	wdsp(sc->sc_base, SB_MIDI_UART_INTR);
	sc->sc_mintr = intr;
	sc->sc_intr = 0;
	sc->sc_arg = arg;
}

/*
 * Write a byte to the midi port, when in midi uart mode.
 */
void
sb_midi_output(struct sb_softc *sc, int v)
{
	if (wdsp(sc->sc_base, v) < 0)
		++sberr.wmidi;
}
#endif
