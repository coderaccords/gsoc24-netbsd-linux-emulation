/* $NetBSD: cms.c,v 1.14 2006/11/16 01:33:00 christos Exp $ */

/*
 * Copyright (c) 2000 The NetBSD Foundation, Inc.
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
__KERNEL_RCSID(0, "$NetBSD: cms.c,v 1.14 2006/11/16 01:33:00 christos Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <sys/select.h>

#include <machine/bus.h>

#include <sys/audioio.h>
#include <dev/audio_if.h>
#include <dev/audiovar.h>

#include <sys/midiio.h>
#include <dev/midi_if.h>
#include <dev/midivar.h>
#include <dev/midisynvar.h>

#include <dev/isa/isareg.h>
#include <dev/isa/isavar.h>
#include <dev/isa/cmsreg.h>

#ifdef AUDIO_DEBUG
#define DPRINTF(x)	if (cmsdebug) printf x
int	cmsdebug = 0;
#else
#define DPRINTF(x)
#endif

struct cms_softc {
	struct midi_softc sc_mididev;

	bus_space_tag_t sc_iot;
	bus_space_handle_t sc_ioh;

	/* shadow registers for each chip */
	u_int8_t sc_shadowregs[32*2];
	midisyn sc_midisyn;
};

int	cms_probe(struct device *, struct cfdata *, void *);
void	cms_attach(struct device *, struct device *, void *);

CFATTACH_DECL(cms, sizeof(struct cms_softc),
    cms_probe, cms_attach, NULL, NULL);

int	cms_open(midisyn *, int);
void	cms_close(midisyn *);
void	cms_on(midisyn *, uint_fast16_t, midipitch_t, int16_t);
void	cms_off(midisyn *, uint_fast16_t, uint_fast8_t);

struct midisyn_methods midi_cms_hw = {
	.open     = cms_open,
	.close    = cms_close,
	.attackv  = cms_on,
	.releasev = cms_off,
};

static void cms_reset(struct cms_softc *);

static char cms_note_table[] = {
	/* A  */ 3,
	/* A# */ 31,
	/* B  */ 58,
	/* C  */ 83,
	/* C# */ 107,
	/* D  */ 130,
	/* D# */ 151,
	/* E  */ 172,
	/* F  */ 191,
	/* F# */ 209,
	/* G  */ 226,
	/* G# */ 242,
};

#define NOTE_TO_OCTAVE(note) (((note)-CMS_FIRST_NOTE)/12)
#define NOTE_TO_COUNT(note) cms_note_table[(((note)-CMS_FIRST_NOTE)%12)]

int
cms_probe(struct device *parent, struct cfdata *match,
    void *aux)
{
	struct isa_attach_args *ia = aux;
	bus_space_tag_t iot;
	bus_space_handle_t ioh;
	int found = 0;
	int i;

	DPRINTF(("cms_probe():\n"));

	iot = ia->ia_iot;

	if (ia->ia_nio < 1)
		return 0;

	if (ISA_DIRECT_CONFIG(ia))
		return 0;

	if (ia->ia_io[0].ir_addr == ISA_UNKNOWN_PORT)
		return 0;

	if (bus_space_map(iot, ia->ia_io[0].ir_addr, CMS_IOSIZE, 0, &ioh))
		return 0;

	bus_space_write_1(iot, ioh, CMS_WREG, 0xaa);
	if (bus_space_read_1(iot, ioh, CMS_RREG) != 0xaa)
		goto out;

	for (i = 0; i < 8; i++) {
		if (bus_space_read_1(iot, ioh, CMS_MREG) != 0x7f)
			goto out;
	}
	found = 1;

	ia->ia_nio = 1;
	ia->ia_io[0].ir_size = CMS_IOSIZE;

	ia->ia_niomem = 0;
	ia->ia_nirq = 0;
	ia->ia_ndrq = 0;

out:
	bus_space_unmap(iot, ioh, CMS_IOSIZE);

	return found;
}


void
cms_attach(struct device *parent, struct device *self, void *aux)
{
	struct cms_softc *sc = (struct cms_softc *)self;
	struct isa_attach_args *ia = aux;
	bus_space_tag_t iot;
	bus_space_handle_t ioh;
	midisyn *ms;
	struct audio_attach_args arg;

	printf("\n");

	DPRINTF(("cms_attach():\n"));

	iot = ia->ia_iot;

	if (bus_space_map(iot, ia->ia_io[0].ir_addr, CMS_IOSIZE, 0, &ioh)) {
		printf(": can't map i/o space\n");
		return;
	}

	sc->sc_iot = iot;
	sc->sc_ioh = ioh;

	/* now let's reset the chips */
	cms_reset(sc);

	ms = &sc->sc_midisyn;
	ms->mets = &midi_cms_hw;
	strcpy(ms->name, "Creative Music System");
	ms->nvoice = CMS_NVOICES;
	ms->data = sc;

	/* use the synthesiser */
	midisyn_attach(&sc->sc_mididev, ms);

	/* now attach the midi device to the synthesiser */
	arg.type = AUDIODEV_TYPE_MIDI;
	arg.hwif = sc->sc_mididev.hw_if;
	arg.hdl = sc->sc_mididev.hw_hdl;
	config_found((struct device *)&sc->sc_mididev, &arg, 0);
}


int
cms_open(midisyn *ms, int flag)
{
	struct cms_softc *sc = (struct cms_softc *)ms->data;

	cms_reset(sc);

	return 0;
}

void
cms_close(ms)
	midisyn *ms;
{
	struct cms_softc *sc = (struct cms_softc *)ms->data;

	cms_reset(sc);
}

void
cms_on(midisyn *ms, uint_fast16_t vidx, midipitch_t mp, int16_t level_cB)
{
	struct cms_softc *sc = (struct cms_softc *)ms->data;
	int chip = CHAN_TO_CHIP(vidx);
	int voice = CHAN_TO_VOICE(vidx);
	uint32_t note;
	u_int8_t octave;
	u_int8_t count;
	u_int8_t reg;
	u_int8_t vol;
	
	/*
	 * The next line is a regrettable hack, because it drops all pitch
	 * adjustment midisyn has supplied in miditune, so this synth will
	 * not respond to tuning, pitchbend, etc. It seems it ought to be
	 * possible to DTRT if the formula that generated the cms_note_table
	 * can be found, but I've had no luck, and a plot of the numbers in
	 * the table clearly curves the wrong way to be derived any obvious
	 * way from the equal tempered scale. (Or maybe the table's wrong?
	 * Has this device been playing flat up the scale? I don't have
	 * access to one to try.)
	 */
	note = MIDIPITCH_TO_KEY(mp);

	if (note < CMS_FIRST_NOTE)
		return;

	octave = NOTE_TO_OCTAVE(note);
	count = NOTE_TO_COUNT(note);

	DPRINTF(("chip=%d voice=%d octave=%d count=%d offset=%d shift=%d\n",
		chip, voice, octave, count, OCTAVE_OFFSET(voice),
		OCTAVE_SHIFT(voice)));

	/* write the count */
	CMS_WRITE(sc, chip, CMS_IREG_FREQ0 + voice, count);

	/* select the octave */
	reg = CMS_READ(sc, chip, CMS_IREG_OCTAVE_1_0 + OCTAVE_OFFSET(voice));
	reg &= ~(0x0f<<OCTAVE_SHIFT(voice));
	reg |= ((octave&0x7)<<OCTAVE_SHIFT(voice));
	CMS_WRITE(sc, chip, CMS_IREG_OCTAVE_1_0 + OCTAVE_OFFSET(voice), reg);

	/* set the volume */
	/* this may be the wrong curve but will do something. no docs! */
	vol = 15 + (level_cB > -75) ? level_cB/5 : -15;
	CMS_WRITE(sc, chip, CMS_IREG_VOL0 + voice, ((vol<<4)|vol));

	/* enable the voice */
	reg = CMS_READ(sc, chip, CMS_IREG_FREQ_CTL);
	reg |= (1<<voice);
	CMS_WRITE(sc, chip, CMS_IREG_FREQ_CTL, reg);
}

void
cms_off(midisyn *ms, uint_fast16_t vidx, uint_fast8_t vel)
{
	struct cms_softc *sc = (struct cms_softc *)ms->data;
	int chip = CHAN_TO_CHIP(vidx);
	int voice = CHAN_TO_VOICE(vidx);
	u_int8_t reg;

	/* disable the channel */
	reg = CMS_READ(sc, chip, CMS_IREG_FREQ_CTL);
	reg &= ~(1<<voice);
	CMS_WRITE(sc, chip, CMS_IREG_FREQ_CTL, reg);
}

static void
cms_reset(sc)
	struct cms_softc *sc;
{
	int i;

	DPRINTF(("cms_reset():\n"));

	for (i = 0; i < 6; i++) {
		CMS_WRITE(sc, 0, CMS_IREG_VOL0+i, 0x00);
		CMS_WRITE(sc, 1, CMS_IREG_VOL0+i, 0x00);

		CMS_WRITE(sc, 0, CMS_IREG_FREQ0+i, 0x00);
		CMS_WRITE(sc, 1, CMS_IREG_FREQ0+i, 0x00);
	}

	for (i = 0; i < 3; i++) {
		CMS_WRITE(sc, 0, CMS_IREG_OCTAVE_1_0+i, 0x00);
		CMS_WRITE(sc, 1, CMS_IREG_OCTAVE_1_0+i, 0x00);
	}

	CMS_WRITE(sc, 0, CMS_IREG_FREQ_CTL, 0x00);
	CMS_WRITE(sc, 1, CMS_IREG_FREQ_CTL, 0x00);

	CMS_WRITE(sc, 0, CMS_IREG_NOISE_CTL, 0x00);
	CMS_WRITE(sc, 1, CMS_IREG_NOISE_CTL, 0x00);

	CMS_WRITE(sc, 0, CMS_IREG_NOISE_BW, 0x00);
	CMS_WRITE(sc, 1, CMS_IREG_NOISE_BW, 0x00);

	/*
	 * These registers don't appear to be useful, but must be
	 * cleared otherwise certain voices don't work properly
	 */
	CMS_WRITE(sc, 0, 0x18, 0x00);
	CMS_WRITE(sc, 1, 0x18, 0x00);
	CMS_WRITE(sc, 0, 0x19, 0x00);
	CMS_WRITE(sc, 1, 0x19, 0x00);

	CMS_WRITE(sc, 0, CMS_IREG_SYS_CTL, CMS_IREG_SYS_RESET);
	CMS_WRITE(sc, 1, CMS_IREG_SYS_CTL, CMS_IREG_SYS_RESET);

	CMS_WRITE(sc, 0, CMS_IREG_SYS_CTL, CMS_IREG_SYS_ENBL);
	CMS_WRITE(sc, 1, CMS_IREG_SYS_CTL, CMS_IREG_SYS_ENBL);
}
