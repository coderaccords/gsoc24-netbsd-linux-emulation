/*	$NetBSD: emuxki.c,v 1.39 2005/01/10 22:01:37 kent Exp $	*/

/*-
 * Copyright (c) 2001 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Yannick Montulet.
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
 *	This product includes software developed by the NetBSD
 *	Foundation, Inc. and its contributors.
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
 * Driver for Creative Labs SBLive! series and probably PCI512.
 * 
 * Known bugs:
 * - inversed stereo at ac97 codec level
 *   (XXX jdolecek - don't see the problem? maybe because auvia(4) has
 *    it swapped too?)
 * - bass disappear when you plug rear jack-in on Cambridge FPS2000 speakers
 *   (and presumably all speakers that support front and rear jack-in)
 *
 * TODO:
 * - Digital Outputs
 * - (midi/mpu),joystick support
 * - Multiple voices play (problem with /dev/audio architecture)
 * - Multiple sources recording (Pb with audio(4))
 * - Independent modification of each channel's parameters (via mixer ?)
 * - DSP FX patches (to make fx like chipmunk)
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: emuxki.c,v 1.39 2005/01/10 22:01:37 kent Exp $");

#include <sys/param.h>
#include <sys/device.h>
#include <sys/errno.h>
#include <sys/malloc.h>
#include <sys/systm.h>
#include <sys/audioio.h>
#include <sys/select.h>
#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>
#include <dev/audio_if.h>
#include <dev/audiovar.h>
#include <dev/auconv.h>
#include <dev/mulaw.h>
#include <dev/ic/ac97reg.h>
#include <dev/ic/ac97var.h>

#include <dev/pci/emuxkireg.h>
#include <dev/pci/emuxkivar.h>

/* autoconf goo */
static int  emuxki_match(struct device *, struct cfdata *, void *);
static void emuxki_attach(struct device *, struct device *, void *);
static int  emuxki_detach(struct device *, int);

/* DMA mem mgmt */
static struct dmamem *dmamem_alloc(bus_dma_tag_t, size_t, bus_size_t,
				 int, struct malloc_type *, int);
static void           dmamem_free(struct dmamem *, struct malloc_type *);

/* Emu10k1 init & shutdown */
static int  emuxki_init(struct emuxki_softc *);
static void emuxki_shutdown(struct emuxki_softc *);

/* Emu10k1 mem mgmt */
static void   *emuxki_pmem_alloc(struct emuxki_softc *, size_t,
		struct malloc_type *,int);
static void   *emuxki_rmem_alloc(struct emuxki_softc *, size_t,
		struct malloc_type *,int);

/*
 * Emu10k1 channels funcs : There is no direct access to channels, everything
 * is done through voices I will at least provide channel based fx params
 * modification, later...
 */

/* Emu10k1 voice mgmt */
static struct emuxki_voice *emuxki_voice_new(struct emuxki_softc *,
					     u_int8_t);
static void   emuxki_voice_delete(struct emuxki_voice *);
static int    emuxki_voice_set_audioparms(struct emuxki_voice *, u_int8_t,
					  u_int8_t, u_int32_t);
/* emuxki_voice_set_fxparms will come later, it'll need channel distinction */
static int emuxki_voice_set_bufparms(struct emuxki_voice *,
				     void *, u_int32_t, u_int16_t);
static void emuxki_voice_commit_parms(struct emuxki_voice *);
static int emuxki_voice_adc_rate(struct emuxki_voice *);
static u_int32_t emuxki_voice_curaddr(struct emuxki_voice *);
static void emuxki_voice_start(struct emuxki_voice *,
			       void (*) (void *), void *);
static void emuxki_voice_halt(struct emuxki_voice *);

/*
 * Emu10k1 stream mgmt : not done yet
 */
#if 0
static struct emuxki_stream *emuxki_stream_new(struct emu10k1 *);
static void   emuxki_stream_delete(struct emuxki_stream *);
static int    emuxki_stream_set_audio_params(struct emuxki_stream *, u_int8_t,
					     u_int8_t, u_int8_t, u_int16_t);
static void   emuxki_stream_start(struct emuxki_stream *);
static void   emuxki_stream_halt(struct emuxki_stream *);
#endif

/* audio interface callbacks */

static int	emuxki_open(void *, int);
static void	emuxki_close(void *);

static int	emuxki_query_encoding(void *, struct audio_encoding *);
static int	emuxki_set_params(void *, int, int, audio_params_t *,
				  audio_params_t *, stream_filter_list_t *,
				  stream_filter_list_t *);

static int	emuxki_round_blocksize(void *, int, int, const audio_params_t *);
static size_t	emuxki_round_buffersize(void *, int, size_t);

static int	emuxki_trigger_output(void *, void *, void *, int,
				      void (*)(void *), void *,
				      const audio_params_t *);
static int	emuxki_trigger_input(void *, void *, void *, int,
				     void (*) (void *), void *,
				     const audio_params_t *);
static int	emuxki_halt_output(void *);
static int	emuxki_halt_input(void *);

static int	emuxki_getdev(void *, struct audio_device *);
static int	emuxki_set_port(void *, mixer_ctrl_t *);
static int	emuxki_get_port(void *, mixer_ctrl_t *);
static int	emuxki_query_devinfo(void *, mixer_devinfo_t *);

static void    *emuxki_allocm(void *, int, size_t, struct malloc_type *, int);
static void	emuxki_freem(void *, void *, struct malloc_type *);

static paddr_t	emuxki_mappage(void *, void *, off_t, int);
static int	emuxki_get_props(void *);

/* Interrupt handler */
static int  emuxki_intr(void *);

/* Emu10k1 AC97 interface callbacks */
static int  emuxki_ac97_attach(void *, struct ac97_codec_if *);
static int  emuxki_ac97_read(void *, u_int8_t, u_int16_t *);
static int  emuxki_ac97_write(void *, u_int8_t, u_int16_t);
static int  emuxki_ac97_reset(void *);
static enum ac97_host_flags emuxki_ac97_flags(void *);

/*
 * Autoconfig goo.
 */
CFATTACH_DECL(emuxki, sizeof(struct emuxki_softc),
    emuxki_match, emuxki_attach, emuxki_detach, NULL);

static const struct audio_hw_if emuxki_hw_if = {
	emuxki_open,
	emuxki_close,
	NULL,			/* drain */
	emuxki_query_encoding,
	emuxki_set_params,
	emuxki_round_blocksize,
	NULL,			/* commit settings */
	NULL,			/* init_output */
	NULL,			/* init_input */
	NULL,			/* start_output */
	NULL,			/* start_input */
	emuxki_halt_output,
	emuxki_halt_input,
	NULL,			/* speaker_ctl */
	emuxki_getdev,
	NULL,			/* setfd */
	emuxki_set_port,
	emuxki_get_port,
	emuxki_query_devinfo,
	emuxki_allocm,
	emuxki_freem,
	emuxki_round_buffersize,
	emuxki_mappage,
	emuxki_get_props,
	emuxki_trigger_output,
	emuxki_trigger_input,
	NULL,			/* dev_ioctl */
};

#if 0
static const int emuxki_recsrc_intrmasks[EMU_NUMRECSRCS] =
    { EMU_INTE_MICBUFENABLE, EMU_INTE_ADCBUFENABLE, EMU_INTE_EFXBUFENABLE };
#endif
static const u_int32_t emuxki_recsrc_bufaddrreg[EMU_NUMRECSRCS] =
    { EMU_MICBA, EMU_ADCBA, EMU_FXBA };
static const u_int32_t emuxki_recsrc_szreg[EMU_NUMRECSRCS] =
    { EMU_MICBS, EMU_ADCBS, EMU_FXBS };
static const int emuxki_recbuf_sz[] = {
	0, 384, 448, 512, 640, 768, 896, 1024, 1280, 1536, 1792,
	2048, 2560, 3072, 3584, 4096, 5120, 6144, 7168, 8192, 10240,
	12288, 14366, 16384, 20480, 24576, 28672, 32768, 40960, 49152,
	57344, 65536
};

#define EMUXKI_NFORMATS	4
static const struct audio_format emuxki_formats[EMUXKI_NFORMATS] = {
	{NULL, AUMODE_PLAY | AUMODE_RECORD, AUDIO_ENCODING_SLINEAR_LE, 16, 16,
	 2, AUFMT_STEREO, 0, {4000, 48000}},
	{NULL, AUMODE_PLAY | AUMODE_RECORD, AUDIO_ENCODING_SLINEAR_LE, 16, 16,
	 1, AUFMT_MONAURAL, 0, {4000, 48000}},
	{NULL, AUMODE_PLAY, AUDIO_ENCODING_ULINEAR_LE, 8, 8,
	 2, AUFMT_STEREO, 0, {4000, 48000}},
	{NULL, AUMODE_PLAY, AUDIO_ENCODING_ULINEAR_LE, 8, 8,
	 1, AUFMT_MONAURAL, 0, {4000, 48000}},
};

/*
 * DMA memory mgmt
 */

static void
dmamem_delete(struct dmamem *mem, struct malloc_type *type)
{
	free(mem->segs, type);
	free(mem, type);
}

static struct dmamem *
dmamem_alloc(bus_dma_tag_t dmat, size_t size, bus_size_t align,
	     int nsegs, struct malloc_type *type, int flags)
{
	struct dmamem	*mem;
	int		bus_dma_flags;

	/* Allocate memory for structure */
	if ((mem = malloc(sizeof(*mem), type, flags)) == NULL)
		return (NULL);
	mem->dmat = dmat;
	mem->size = size;
	mem->align = align;
	mem->nsegs = nsegs;
	mem->bound = 0;

	mem->segs = malloc(mem->nsegs * sizeof(*(mem->segs)), type, flags);
	if (mem->segs == NULL) {
		free(mem, type);
		return (NULL);
	}

	bus_dma_flags = (flags & M_NOWAIT) ? BUS_DMA_NOWAIT : BUS_DMA_WAITOK;
	if (bus_dmamem_alloc(dmat, mem->size, mem->align, mem->bound,
			     mem->segs, mem->nsegs, &(mem->rsegs),
			     bus_dma_flags)) {
		dmamem_delete(mem, type);
		return (NULL);
	}

	if (bus_dmamem_map(dmat, mem->segs, mem->nsegs, mem->size,
			   &(mem->kaddr), bus_dma_flags | BUS_DMA_COHERENT)) {
		bus_dmamem_free(dmat, mem->segs, mem->nsegs);
		dmamem_delete(mem, type);
		return (NULL);
	}

	if (bus_dmamap_create(dmat, mem->size, mem->nsegs, mem->size,
			      mem->bound, bus_dma_flags, &(mem->map))) {
		bus_dmamem_unmap(dmat, mem->kaddr, mem->size);
		bus_dmamem_free(dmat, mem->segs, mem->nsegs);
		dmamem_delete(mem, type);
		return (NULL);
	}

	if (bus_dmamap_load(dmat, mem->map, mem->kaddr, 
			    mem->size, NULL, bus_dma_flags)) {
		bus_dmamap_destroy(dmat, mem->map);
		bus_dmamem_unmap(dmat, mem->kaddr, mem->size);
		bus_dmamem_free(dmat, mem->segs, mem->nsegs);
		dmamem_delete(mem, type);
		return (NULL);
	}

	return (mem);
}

static void
dmamem_free(struct dmamem *mem, struct malloc_type *type)
{
	bus_dmamap_unload(mem->dmat, mem->map);
	bus_dmamap_destroy(mem->dmat, mem->map);
	bus_dmamem_unmap(mem->dmat, mem->kaddr, mem->size);
	bus_dmamem_free(mem->dmat, mem->segs, mem->nsegs);
	dmamem_delete(mem, type);
}


/*
 * Autoconf device callbacks : attach and detach
 */

static void
emuxki_pci_shutdown(struct emuxki_softc *sc)
{
	if (sc->sc_ih != NULL)
		pci_intr_disestablish(sc->sc_pc, sc->sc_ih);
	if (sc->sc_ios)
		bus_space_unmap(sc->sc_iot, sc->sc_ioh, sc->sc_ios);
}

static int
emuxki_scinit(struct emuxki_softc *sc)
{
	int             err;

	bus_space_write_4(sc->sc_iot, sc->sc_ioh, EMU_HCFG,
		EMU_HCFG_LOCKSOUNDCACHE | EMU_HCFG_LOCKTANKCACHE_MASK |
		EMU_HCFG_MUTEBUTTONENABLE);
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, EMU_INTE,
		EMU_INTE_SAMPLERATER | EMU_INTE_PCIERRENABLE);

	if ((err = emuxki_init(sc)))
		return (err);

	if (sc->sc_type & EMUXKI_AUDIGY2) {
		bus_space_write_4(sc->sc_iot, sc->sc_ioh, EMU_HCFG,
			EMU_HCFG_AUDIOENABLE | EMU_HCFG_AC3ENABLE_CDSPDIF |
			EMU_HCFG_AC3ENABLE_GPSPDIF | EMU_HCFG_AUTOMUTE);
	} else if (sc->sc_type & EMUXKI_AUDIGY) {
		bus_space_write_4(sc->sc_iot, sc->sc_ioh, EMU_HCFG,
			EMU_HCFG_AUDIOENABLE | EMU_HCFG_AUTOMUTE);
	} else {
		bus_space_write_4(sc->sc_iot, sc->sc_ioh, EMU_HCFG,
			EMU_HCFG_AUDIOENABLE |
			EMU_HCFG_LOCKTANKCACHE_MASK | EMU_HCFG_AUTOMUTE);
	}
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, EMU_INTE,
		bus_space_read_4(sc->sc_iot, sc->sc_ioh, EMU_INTE) |
		EMU_INTE_VOLINCRENABLE | EMU_INTE_VOLDECRENABLE |
		EMU_INTE_MUTEENABLE);
	if (sc->sc_type & EMUXKI_AUDIGY2) {
		bus_space_write_4(sc->sc_iot, sc->sc_ioh, EMU_A_IOCFG,
			EMU_A_IOCFG_GPOUT0 |
			bus_space_read_4(sc->sc_iot, sc->sc_ioh, EMU_A_IOCFG));
	}

	/* No multiple voice support for now */
	sc->pvoice = sc->rvoice = NULL;

	return (0);
}

static int
emuxki_ac97_init(struct emuxki_softc *sc)
{
	sc->hostif.arg = sc;
	sc->hostif.attach = emuxki_ac97_attach;
	sc->hostif.read = emuxki_ac97_read;
	sc->hostif.write = emuxki_ac97_write;
	sc->hostif.reset = emuxki_ac97_reset;
	sc->hostif.flags = emuxki_ac97_flags;
	return ac97_attach(&sc->hostif, &sc->sc_dev);
}

static int
emuxki_match(struct device *parent, struct cfdata *match, void *aux)
{
	struct pci_attach_args *pa = aux;

	if (PCI_VENDOR(pa->pa_id) != PCI_VENDOR_CREATIVELABS)
		return 0;

	switch (PCI_PRODUCT(pa->pa_id)) {
	case PCI_PRODUCT_CREATIVELABS_SBLIVE:
	case PCI_PRODUCT_CREATIVELABS_SBLIVE2:
 	case PCI_PRODUCT_CREATIVELABS_AUDIGY:
 		return 1;
	default:
		return 0;
	}
}

static void
emuxki_attach(struct device *parent, struct device *self, void *aux)
{
	struct emuxki_softc *sc = (struct emuxki_softc *) self;
	struct pci_attach_args *pa = aux;
	char            devinfo[256];
	pci_intr_handle_t ih;
	const char     *intrstr;

	aprint_naive(": Audio controller\n");

	if (pci_mapreg_map(pa, EMU_PCI_CBIO, PCI_MAPREG_TYPE_IO, 0,
	    &(sc->sc_iot), &(sc->sc_ioh), &(sc->sc_iob),
			   &(sc->sc_ios))) {
		aprint_error(": can't map iospace\n");
		return;
	}
	pci_devinfo(pa->pa_id, pa->pa_class, 1, devinfo, sizeof(devinfo));
	aprint_normal(": %s\n", devinfo);

	sc->sc_pc   = pa->pa_pc;
	sc->sc_dmat = pa->pa_dmat;
	pci_conf_write(pa->pa_pc, pa->pa_tag, PCI_COMMAND_STATUS_REG,
		pci_conf_read(pa->pa_pc, pa->pa_tag,
		(PCI_COMMAND_STATUS_REG) | PCI_COMMAND_MASTER_ENABLE));

	if (pci_intr_map(pa, &ih)) {
		aprint_error("%s: couldn't map interrupt\n",
			sc->sc_dev.dv_xname);
		bus_space_unmap(sc->sc_iot, sc->sc_ioh, sc->sc_ios);
		return;
	}

	intrstr = pci_intr_string(pa->pa_pc, ih);
	sc->sc_ih = pci_intr_establish(pa->pa_pc, ih, IPL_AUDIO, emuxki_intr,
		sc);
	if (sc->sc_ih == NULL) {
		aprint_error("%s: couldn't establish interrupt",
		    sc->sc_dev.dv_xname);
		if (intrstr != NULL)
			aprint_normal(" at %s", intrstr);
		aprint_normal("\n");
		bus_space_unmap(sc->sc_iot, sc->sc_ioh, sc->sc_ios);
		return;
	}

 /* XXX it's unknown whether APS is made from Audigy as well */
        if (PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_CREATIVELABS_AUDIGY) {
                sc->sc_type = EMUXKI_AUDIGY;
                if (PCI_REVISION(pa->pa_class) == 0x04) {
                        sc->sc_type |= EMUXKI_AUDIGY2;
                        strlcpy(sc->sc_audv.name, "Audigy2", sizeof sc->sc_audv.name);
                } else {
                        strlcpy(sc->sc_audv.name, "Audigy", sizeof sc->sc_audv.name);
                }
        } else if (pci_conf_read(pa->pa_pc, pa->pa_tag,
            PCI_SUBSYS_ID_REG) == EMU_SUBSYS_APS) {
                sc->sc_type = EMUXKI_APS;
                strlcpy(sc->sc_audv.name, "E-mu APS", sizeof sc->sc_audv.name);
        } else {
                sc->sc_type = EMUXKI_SBLIVE;
                strlcpy(sc->sc_audv.name, "SB Live!", sizeof sc->sc_audv.name);
        }
        snprintf(sc->sc_audv.version, sizeof sc->sc_audv.version, "0x%02x",
                 PCI_REVISION(pa->pa_class));
        strlcpy(sc->sc_audv.config, "emuxki", sizeof sc->sc_audv.config);

	if (emuxki_scinit(sc) || emuxki_ac97_init(sc) ||
	    (sc->sc_audev = audio_attach_mi(&emuxki_hw_if, sc, self)) == NULL) {
		emuxki_pci_shutdown(sc);
		return;
	}
	aprint_normal("%s: interrupting at %s\n", sc->sc_dev.dv_xname, intrstr);
#if 0
	sc->rsourcectl.dev =
	    sc->codecif->vtbl->get_portnum_by_name(sc->codec_if, AudioCrecord,
						   AudioNsource, NULL);
	sc->rsourcectl.cp = AUDIO_MIXER_ENUM;
#endif
}

static int
emuxki_detach(struct device *self, int flags)
{
	struct emuxki_softc *sc = (struct emuxki_softc *) self;

        if (sc->sc_audev != NULL) /* Test in case audio didn't attach */
	        config_detach(sc->sc_audev, 0);

	/* All voices should be stopped now but add some code here if not */

	bus_space_write_4(sc->sc_iot, sc->sc_ioh, EMU_HCFG,
		EMU_HCFG_LOCKSOUNDCACHE | EMU_HCFG_LOCKTANKCACHE_MASK |
		EMU_HCFG_MUTEBUTTONENABLE);
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, EMU_INTE, 0);

	emuxki_shutdown(sc);

	emuxki_pci_shutdown(sc);

	return (0);
}


/* Misc stuff relative to emu10k1 */

static u_int32_t
emuxki_rate_to_pitch(u_int32_t rate)
{
	static const u_int32_t logMagTable[128] = {
		0x00000, 0x02dfc, 0x05b9e, 0x088e6, 0x0b5d6, 0x0e26f, 0x10eb3,
		0x13aa2, 0x1663f, 0x1918a, 0x1bc84, 0x1e72e, 0x2118b, 0x23b9a,
		0x2655d, 0x28ed5, 0x2b803, 0x2e0e8, 0x30985, 0x331db, 0x359eb,
		0x381b6, 0x3a93d, 0x3d081, 0x3f782, 0x41e42, 0x444c1, 0x46b01,
		0x49101, 0x4b6c4, 0x4dc49, 0x50191, 0x5269e, 0x54b6f, 0x57006,
		0x59463, 0x5b888, 0x5dc74, 0x60029, 0x623a7, 0x646ee, 0x66a00,
		0x68cdd, 0x6af86, 0x6d1fa, 0x6f43c, 0x7164b, 0x73829, 0x759d4,
		0x77b4f, 0x79c9a, 0x7bdb5, 0x7dea1, 0x7ff5e, 0x81fed, 0x8404e,
		0x86082, 0x88089, 0x8a064, 0x8c014, 0x8df98, 0x8fef1, 0x91e20,
		0x93d26, 0x95c01, 0x97ab4, 0x9993e, 0x9b79f, 0x9d5d9, 0x9f3ec,
		0xa11d8, 0xa2f9d, 0xa4d3c, 0xa6ab5, 0xa8808, 0xaa537, 0xac241,
		0xadf26, 0xafbe7, 0xb1885, 0xb3500, 0xb5157, 0xb6d8c, 0xb899f,
		0xba58f, 0xbc15e, 0xbdd0c, 0xbf899, 0xc1404, 0xc2f50, 0xc4a7b,
		0xc6587, 0xc8073, 0xc9b3f, 0xcb5ed, 0xcd07c, 0xceaec, 0xd053f,
		0xd1f73, 0xd398a, 0xd5384, 0xd6d60, 0xd8720, 0xda0c3, 0xdba4a,
		0xdd3b4, 0xded03, 0xe0636, 0xe1f4e, 0xe384a, 0xe512c, 0xe69f3,
		0xe829f, 0xe9b31, 0xeb3a9, 0xecc08, 0xee44c, 0xefc78, 0xf148a,
		0xf2c83, 0xf4463, 0xf5c2a, 0xf73da, 0xf8b71, 0xfa2f0, 0xfba57,
		0xfd1a7, 0xfe8df
	};
	static const u_int8_t logSlopeTable[128] = {
		0x5c, 0x5c, 0x5b, 0x5a, 0x5a, 0x59, 0x58, 0x58,
		0x57, 0x56, 0x56, 0x55, 0x55, 0x54, 0x53, 0x53,
		0x52, 0x52, 0x51, 0x51, 0x50, 0x50, 0x4f, 0x4f,
		0x4e, 0x4d, 0x4d, 0x4d, 0x4c, 0x4c, 0x4b, 0x4b,
		0x4a, 0x4a, 0x49, 0x49, 0x48, 0x48, 0x47, 0x47,
		0x47, 0x46, 0x46, 0x45, 0x45, 0x45, 0x44, 0x44,
		0x43, 0x43, 0x43, 0x42, 0x42, 0x42, 0x41, 0x41,
		0x41, 0x40, 0x40, 0x40, 0x3f, 0x3f, 0x3f, 0x3e,
		0x3e, 0x3e, 0x3d, 0x3d, 0x3d, 0x3c, 0x3c, 0x3c,
		0x3b, 0x3b, 0x3b, 0x3b, 0x3a, 0x3a, 0x3a, 0x39,
		0x39, 0x39, 0x39, 0x38, 0x38, 0x38, 0x38, 0x37,
		0x37, 0x37, 0x37, 0x36, 0x36, 0x36, 0x36, 0x35,
		0x35, 0x35, 0x35, 0x34, 0x34, 0x34, 0x34, 0x34,
		0x33, 0x33, 0x33, 0x33, 0x32, 0x32, 0x32, 0x32,
		0x32, 0x31, 0x31, 0x31, 0x31, 0x31, 0x30, 0x30,
		0x30, 0x30, 0x30, 0x2f, 0x2f, 0x2f, 0x2f, 0x2f
	};
	int8_t          i;

	if (rate == 0)
		return 0;	/* Bail out if no leading "1" */
	rate *= 11185;		/* Scale 48000 to 0x20002380 */
	for (i = 31; i > 0; i--) {
		if (rate & 0x80000000) {	/* Detect leading "1" */
			return (((u_int32_t) (i - 15) << 20) +
				logMagTable[0x7f & (rate >> 24)] +
				(0x7f & (rate >> 17)) *
				logSlopeTable[0x7f & (rate >> 24)]);
		}
		rate <<= 1;
	}

	return 0;		/* Should never reach this point */
}

/* Emu10k1 Low level */

static u_int32_t
emuxki_read(struct emuxki_softc *sc, u_int16_t chano, u_int32_t reg)
{
	u_int32_t       ptr, mask = 0xffffffff;
	u_int8_t        size, offset = 0;
	int             s;

	ptr = ((((u_int32_t) reg) << 16) &
		(sc->sc_type & EMUXKI_AUDIGY ?
			EMU_A_PTR_ADDR_MASK : EMU_PTR_ADDR_MASK)) |
		(chano & EMU_PTR_CHNO_MASK);
	if (reg & 0xff000000) {
		size = (reg >> 24) & 0x3f;
		offset = (reg >> 16) & 0x1f;
		mask = ((1 << size) - 1) << offset;
	}

	s = splaudio();
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, EMU_PTR, ptr);
	ptr = (bus_space_read_4(sc->sc_iot, sc->sc_ioh, EMU_DATA) & mask)
		>> offset;
	splx(s);

	return (ptr);
}

static void
emuxki_write(struct emuxki_softc *sc, u_int16_t chano,
	      u_int32_t reg, u_int32_t data)
{
	u_int32_t       ptr, mask;
	u_int8_t        size, offset;
	int             s;

	ptr = ((((u_int32_t) reg) << 16) &
		(sc->sc_type & EMUXKI_AUDIGY ?
			EMU_A_PTR_ADDR_MASK : EMU_PTR_ADDR_MASK)) |
		(chano & EMU_PTR_CHNO_MASK);
	if (reg & 0xff000000) {
		size = (reg >> 24) & 0x3f;
		offset = (reg >> 16) & 0x1f;
		mask = ((1 << size) - 1) << offset;
		data = ((data << offset) & mask) |
			(emuxki_read(sc, chano, reg & 0xffff) & ~mask);
	}

	s = splaudio();
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, EMU_PTR, ptr);
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, EMU_DATA, data);
	splx(s);
}

/* Microcode should this go in /sys/dev/microcode ? */

static void
emuxki_write_micro(struct emuxki_softc *sc, u_int32_t pc, u_int32_t data)
{
	emuxki_write(sc, 0,
		(sc->sc_type & EMUXKI_AUDIGY ?
			EMU_A_MICROCODEBASE : EMU_MICROCODEBASE) + pc,
		 data);
}

static void
emuxki_dsp_addop(struct emuxki_softc *sc, u_int16_t *pc, u_int8_t op,
		  u_int16_t r, u_int16_t a, u_int16_t x, u_int16_t y)
{
	if (sc->sc_type & EMUXKI_AUDIGY) {
		emuxki_write_micro(sc, *pc << 1,
			((x << 12) & EMU_A_DSP_LOWORD_OPX_MASK) |
			(y & EMU_A_DSP_LOWORD_OPY_MASK));
		emuxki_write_micro(sc, (*pc << 1) + 1,
			((op << 24) & EMU_A_DSP_HIWORD_OPCODE_MASK) |
			((r << 12) & EMU_A_DSP_HIWORD_RESULT_MASK) |
			(a & EMU_A_DSP_HIWORD_OPA_MASK));
	} else {
		emuxki_write_micro(sc, *pc << 1,
			((x << 10) & EMU_DSP_LOWORD_OPX_MASK) |
			(y & EMU_DSP_LOWORD_OPY_MASK));
		emuxki_write_micro(sc, (*pc << 1) + 1,
			((op << 20) & EMU_DSP_HIWORD_OPCODE_MASK) |
			((r << 10) & EMU_DSP_HIWORD_RESULT_MASK) |
			(a & EMU_DSP_HIWORD_OPA_MASK));
	}
  	(*pc)++;
}

/* init and shutdown */

static void
emuxki_initfx(struct emuxki_softc *sc)
{
	u_int16_t       pc;

	/* Set all GPRs to 0 */
	for (pc = 0; pc < 256; pc++)
		emuxki_write(sc, 0, EMU_DSP_GPR(pc), 0);
	for (pc = 0; pc < 160; pc++) {
		emuxki_write(sc, 0, EMU_TANKMEMDATAREGBASE + pc, 0);
		emuxki_write(sc, 0, EMU_TANKMEMADDRREGBASE + pc, 0);
	}
	pc = 0;

	if (sc->sc_type & EMUXKI_AUDIGY) {
		/* AC97 Out (l/r) = AC97 In (l/r) + FX[0/1] * 4 */
		emuxki_dsp_addop(sc, &pc, EMU_DSP_OP_MACINTS,
				  EMU_A_DSP_OUTL(EMU_A_DSP_OUT_A_FRONT),
				  EMU_A_DSP_CST(0),
				  EMU_DSP_FX(0), EMU_A_DSP_CST(4));
		emuxki_dsp_addop(sc, &pc, EMU_DSP_OP_MACINTS,
				  EMU_A_DSP_OUTR(EMU_A_DSP_OUT_A_FRONT),
				  EMU_A_DSP_CST(0),
				  EMU_DSP_FX(1), EMU_A_DSP_CST(4));

		/* Rear channel OUT (l/r) = FX[2/3] * 4 */
#if 0
		emuxki_dsp_addop(sc, &pc, EMU_DSP_OP_MACINTS,
				  EMU_A_DSP_OUTL(EMU_A_DSP_OUT_A_REAR),
				  EMU_A_DSP_OUTL(EMU_A_DSP_OUT_A_FRONT),
				  EMU_DSP_FX(0), EMU_A_DSP_CST(4));
		emuxki_dsp_addop(sc, &pc, EMU_DSP_OP_MACINTS,
				  EMU_A_DSP_OUTR(EMU_A_DSP_OUT_A_REAR),
				  EMU_A_DSP_OUTR(EMU_A_DSP_OUT_A_FRONT),
				  EMU_DSP_FX(1), EMU_A_DSP_CST(4));
#endif
		/* ADC recording (l/r) = AC97 In (l/r) */
		emuxki_dsp_addop(sc, &pc, EMU_DSP_OP_ACC3,
				  EMU_A_DSP_OUTL(EMU_A_DSP_OUT_ADC),
				  EMU_A_DSP_INL(EMU_DSP_IN_AC97),
				  EMU_A_DSP_CST(0), EMU_A_DSP_CST(0));
		emuxki_dsp_addop(sc, &pc, EMU_DSP_OP_ACC3,
				  EMU_A_DSP_OUTR(EMU_A_DSP_OUT_ADC),
				  EMU_A_DSP_INR(EMU_DSP_IN_AC97),
				  EMU_A_DSP_CST(0), EMU_A_DSP_CST(0));

		/* zero out the rest of the microcode */
		while (pc < 512)
			emuxki_dsp_addop(sc, &pc, EMU_DSP_OP_ACC3,
					  EMU_A_DSP_CST(0), EMU_A_DSP_CST(0),
					  EMU_A_DSP_CST(0), EMU_A_DSP_CST(0));

		emuxki_write(sc, 0, EMU_A_DBG, 0);	/* Is it really necessary ? */
	} else {
		/* AC97 Out (l/r) = AC97 In (l/r) + FX[0/1] * 4 */
		emuxki_dsp_addop(sc, &pc, EMU_DSP_OP_MACINTS,
				  EMU_DSP_OUTL(EMU_DSP_OUT_A_FRONT),
				  EMU_DSP_CST(0),
				  EMU_DSP_FX(0), EMU_DSP_CST(4));
		emuxki_dsp_addop(sc, &pc, EMU_DSP_OP_MACINTS,
				  EMU_DSP_OUTR(EMU_DSP_OUT_A_FRONT),
				  EMU_DSP_CST(0),
				  EMU_DSP_FX(1), EMU_DSP_CST(4));

		/* Rear channel OUT (l/r) = FX[2/3] * 4 */
#if 0
		emuxki_dsp_addop(sc, &pc, EMU_DSP_OP_MACINTS,
				  EMU_DSP_OUTL(EMU_DSP_OUT_AD_REAR),
				  EMU_DSP_OUTL(EMU_DSP_OUT_A_FRONT),
				  EMU_DSP_FX(0), EMU_DSP_CST(4));
		emuxki_dsp_addop(sc, &pc, EMU_DSP_OP_MACINTS,
				  EMU_DSP_OUTR(EMU_DSP_OUT_AD_REAR),
				  EMU_DSP_OUTR(EMU_DSP_OUT_A_FRONT),
				  EMU_DSP_FX(1), EMU_DSP_CST(4));
#endif
		/* ADC recording (l/r) = AC97 In (l/r) */
		emuxki_dsp_addop(sc, &pc, EMU_DSP_OP_ACC3,
				  EMU_DSP_OUTL(EMU_DSP_OUT_ADC),
				  EMU_DSP_INL(EMU_DSP_IN_AC97),
				  EMU_DSP_CST(0), EMU_DSP_CST(0));
		emuxki_dsp_addop(sc, &pc, EMU_DSP_OP_ACC3,
				  EMU_DSP_OUTR(EMU_DSP_OUT_ADC),
				  EMU_DSP_INR(EMU_DSP_IN_AC97),
				  EMU_DSP_CST(0), EMU_DSP_CST(0));

		/* zero out the rest of the microcode */
		while (pc < 512)
			emuxki_dsp_addop(sc, &pc, EMU_DSP_OP_ACC3,
					  EMU_DSP_CST(0), EMU_DSP_CST(0),
					  EMU_DSP_CST(0), EMU_DSP_CST(0));

		emuxki_write(sc, 0, EMU_DBG, 0);	/* Is it really necessary ? */
	}
}

static int
emuxki_init(struct emuxki_softc *sc)
{
	u_int16_t       i;
	u_int32_t       spcs, *ptb;
	bus_addr_t      silentpage;

	/* disable any channel interrupt */
	emuxki_write(sc, 0, EMU_CLIEL, 0);
	emuxki_write(sc, 0, EMU_CLIEH, 0);
	emuxki_write(sc, 0, EMU_SOLEL, 0);
	emuxki_write(sc, 0, EMU_SOLEH, 0);

	/* Set recording buffers sizes to zero */
	emuxki_write(sc, 0, EMU_MICBS, EMU_RECBS_BUFSIZE_NONE);
	emuxki_write(sc, 0, EMU_MICBA, 0);
	emuxki_write(sc, 0, EMU_FXBS, EMU_RECBS_BUFSIZE_NONE);
	emuxki_write(sc, 0, EMU_FXBA, 0);
	emuxki_write(sc, 0, EMU_ADCBS, EMU_RECBS_BUFSIZE_NONE);
	emuxki_write(sc, 0, EMU_ADCBA, 0);

        if(sc->sc_type & EMUXKI_AUDIGY) {
                emuxki_write(sc, 0, EMU_SPBYPASS, EMU_SPBYPASS_24_BITS);
                emuxki_write(sc, 0, EMU_AC97SLOT, EMU_AC97SLOT_CENTER | EMU_AC97SLOT_LFE);
        }

	/* Initialize all channels to stopped and no effects */
	for (i = 0; i < EMU_NUMCHAN; i++) {
		emuxki_write(sc, i, EMU_CHAN_DCYSUSV, 0);
		emuxki_write(sc, i, EMU_CHAN_IP, 0);
		emuxki_write(sc, i, EMU_CHAN_VTFT, 0xffff);
		emuxki_write(sc, i, EMU_CHAN_CVCF, 0xffff);
		emuxki_write(sc, i, EMU_CHAN_PTRX, 0);
		emuxki_write(sc, i, EMU_CHAN_CPF, 0);
		emuxki_write(sc, i, EMU_CHAN_CCR, 0);
		emuxki_write(sc, i, EMU_CHAN_PSST, 0);
		emuxki_write(sc, i, EMU_CHAN_DSL, 0x10);	/* Why 16 ? */
		emuxki_write(sc, i, EMU_CHAN_CCCA, 0);
		emuxki_write(sc, i, EMU_CHAN_Z1, 0);
		emuxki_write(sc, i, EMU_CHAN_Z2, 0);
		emuxki_write(sc, i, EMU_CHAN_FXRT, 0x32100000);
		emuxki_write(sc, i, EMU_CHAN_ATKHLDM, 0);
		emuxki_write(sc, i, EMU_CHAN_DCYSUSM, 0);
		emuxki_write(sc, i, EMU_CHAN_IFATN, 0xffff);
		emuxki_write(sc, i, EMU_CHAN_PEFE, 0);
		emuxki_write(sc, i, EMU_CHAN_FMMOD, 0);
		emuxki_write(sc, i, EMU_CHAN_TREMFRQ, 24);
		emuxki_write(sc, i, EMU_CHAN_FM2FRQ2, 24);
		emuxki_write(sc, i, EMU_CHAN_TEMPENV, 0);

		/* these are last so OFF prevents writing */
		emuxki_write(sc, i, EMU_CHAN_LFOVAL2, 0);
		emuxki_write(sc, i, EMU_CHAN_LFOVAL1, 0);
		emuxki_write(sc, i, EMU_CHAN_ATKHLDV, 0);
		emuxki_write(sc, i, EMU_CHAN_ENVVOL, 0);
		emuxki_write(sc, i, EMU_CHAN_ENVVAL, 0);
	}

	/* set digital outputs format */
	spcs = (EMU_SPCS_CLKACCY_1000PPM | EMU_SPCS_SAMPLERATE_48 |
	      EMU_SPCS_CHANNELNUM_LEFT | EMU_SPCS_SOURCENUM_UNSPEC |
		EMU_SPCS_GENERATIONSTATUS | 0x00001200 /* Cat code. */ |
		0x00000000 /* IEC-958 Mode */ | EMU_SPCS_EMPHASIS_NONE |
		EMU_SPCS_COPYRIGHT);
	emuxki_write(sc, 0, EMU_SPCS0, spcs);
	emuxki_write(sc, 0, EMU_SPCS1, spcs);
	emuxki_write(sc, 0, EMU_SPCS2, spcs);

        if(sc->sc_type & EMUXKI_AUDIGY2) {
                emuxki_write(sc, 0, EMU_A2_SPDIF_SAMPLERATE, EMU_A2_SPDIF_UNKNOWN);

                bus_space_write_4(sc->sc_iot, sc->sc_ioh, EMU_A2_PTR, EMU_A2_SRCSEL);
                bus_space_write_4(sc->sc_iot, sc->sc_ioh, EMU_A2_DATA, 
                        EMU_A2_SRCSEL_ENABLE_SPDIF | EMU_A2_SRCSEL_ENABLE_SRCMULTI);

                bus_space_write_4(sc->sc_iot, sc->sc_ioh, EMU_A2_PTR, EMU_A2_SRCMULTI);
                bus_space_write_4(sc->sc_iot, sc->sc_ioh, EMU_A2_DATA, EMU_A2_SRCMULTI_ENABLE_INPUT);
        }


	/* Let's play with sound processor */
	emuxki_initfx(sc);

	/* Here is our Page Table */
	if ((sc->ptb = dmamem_alloc(sc->sc_dmat,
	    EMU_MAXPTE * sizeof(u_int32_t),
	    EMU_DMA_ALIGN, EMU_DMAMEM_NSEG,
	    M_DEVBUF, M_WAITOK)) == NULL)
		return (ENOMEM);

	/* This is necessary unless you like Metallic noise... */
	if ((sc->silentpage = dmamem_alloc(sc->sc_dmat, EMU_PTESIZE,
	    EMU_DMA_ALIGN, EMU_DMAMEM_NSEG, M_DEVBUF, M_WAITOK))==NULL){
		dmamem_free(sc->ptb, M_DEVBUF);
		return (ENOMEM);
	}

	/* Zero out the silent page */
	/* This might not be always true, it might be 128 for 8bit channels */
	memset(KERNADDR(sc->silentpage), 0, DMASIZE(sc->silentpage));

	/*
	 * Set all the PTB Entries to the silent page We shift the physical
	 * address by one and OR it with the page number. I don't know what
	 * the ORed index is for, might be a very useful unused feature...
	 */
	silentpage = DMAADDR(sc->silentpage) << 1;
	ptb = KERNADDR(sc->ptb);
	for (i = 0; i < EMU_MAXPTE; i++)
		ptb[i] = htole32(silentpage | i);

	/* Write PTB address and set TCB to none */
	emuxki_write(sc, 0, EMU_PTB, DMAADDR(sc->ptb));
	emuxki_write(sc, 0, EMU_TCBS, 0);	/* This means 16K TCB */
	emuxki_write(sc, 0, EMU_TCB, 0);	/* No TCB use for now */

	/*
	 * Set channels MAPs to the silent page.
	 * I don't know what MAPs are for.
	 */
	silentpage |= EMU_CHAN_MAP_PTI_MASK;
	for (i = 0; i < EMU_NUMCHAN; i++) {
		emuxki_write(sc, i, EMU_CHAN_MAPA, silentpage);
		emuxki_write(sc, i, EMU_CHAN_MAPB, silentpage);
		sc->channel[i] = NULL;
	}

	/* Init voices list */
	LIST_INIT(&(sc->voices));

	/* Timer is stopped */
	sc->timerstate &= ~EMU_TIMER_STATE_ENABLED;
	return (0);
}

static void
emuxki_shutdown(struct emuxki_softc *sc)
{
	u_int32_t       i;

	/* Disable any Channels interrupts */
	emuxki_write(sc, 0, EMU_CLIEL, 0);
	emuxki_write(sc, 0, EMU_CLIEH, 0);
	emuxki_write(sc, 0, EMU_SOLEL, 0);
	emuxki_write(sc, 0, EMU_SOLEH, 0);

	/*
	 * Should do some voice(stream) stopping stuff here, that's what will
	 * stop and deallocate all channels.
	 */

	/* Stop all channels */
	/* XXX This shouldn't be necessary, I'll remove once everything works */
	for (i = 0; i < EMU_NUMCHAN; i++)
		emuxki_write(sc, i, EMU_CHAN_DCYSUSV, 0);
	for (i = 0; i < EMU_NUMCHAN; i++) {
		emuxki_write(sc, i, EMU_CHAN_VTFT, 0);
		emuxki_write(sc, i, EMU_CHAN_CVCF, 0);
		emuxki_write(sc, i, EMU_CHAN_PTRX, 0);
		emuxki_write(sc, i, EMU_CHAN_CPF, 0);
	}

	/*
	 * Deallocate Emu10k1 caches and recording buffers. Again it will be
	 * removed because it will be done in voice shutdown.
	 */
	emuxki_write(sc, 0, EMU_MICBS, EMU_RECBS_BUFSIZE_NONE);
	emuxki_write(sc, 0, EMU_MICBA, 0);
	emuxki_write(sc, 0, EMU_FXBS, EMU_RECBS_BUFSIZE_NONE);
	emuxki_write(sc, 0, EMU_FXBA, 0);
        if(sc->sc_type & EMUXKI_AUDIGY) {
                emuxki_write(sc, 0, EMU_A_FXWC1, 0);
                emuxki_write(sc, 0, EMU_A_FXWC2, 0);
        } else {
                emuxki_write(sc, 0, EMU_FXWC, 0);
        }
	emuxki_write(sc, 0, EMU_ADCBS, EMU_RECBS_BUFSIZE_NONE);
	emuxki_write(sc, 0, EMU_ADCBA, 0);

	/*
	 * XXX I don't know yet how I will handle tank cache buffer,
	 * I don't even clearly  know what it is for.
	 */
	emuxki_write(sc, 0, EMU_TCB, 0);	/* 16K again */
	emuxki_write(sc, 0, EMU_TCBS, 0);

	emuxki_write(sc, 0, EMU_DBG, 0x8000);	/* necessary ? */

	dmamem_free(sc->silentpage, M_DEVBUF);
	dmamem_free(sc->ptb, M_DEVBUF);
}

/* Emu10k1 Memory management */

static struct emuxki_mem *
emuxki_mem_new(struct emuxki_softc *sc, int ptbidx,
		size_t size, struct malloc_type *type, int flags)
{
	struct emuxki_mem *mem;

	if ((mem = malloc(sizeof(*mem), type, flags)) == NULL)
		return (NULL);

	mem->ptbidx = ptbidx;
	if ((mem->dmamem = dmamem_alloc(sc->sc_dmat, size, EMU_DMA_ALIGN,
	    EMU_DMAMEM_NSEG, type, flags)) == NULL) {
		free(mem, type);
		return (NULL);
	}
	return (mem);
}

static void
emuxki_mem_delete(struct emuxki_mem *mem, struct malloc_type *type)
{
	dmamem_free(mem->dmamem, type);
	free(mem, type);
}

static void *
emuxki_pmem_alloc(struct emuxki_softc *sc, size_t size,
    struct malloc_type *type, int flags)
{
	int             i, j, s;
	size_t          numblocks;
	struct emuxki_mem *mem;
	u_int32_t      *ptb, silentpage;

	ptb = KERNADDR(sc->ptb);
	silentpage = DMAADDR(sc->silentpage) << 1;
	numblocks = size / EMU_PTESIZE;
	if (size % EMU_PTESIZE)
		numblocks++;

	for (i = 0; i < EMU_MAXPTE; i++)
		if ((le32toh(ptb[i]) & EMU_CHAN_MAP_PTE_MASK) == silentpage) {
			/* We look for a free PTE */
			s = splaudio();
			for (j = 0; j < numblocks; j++)
				if ((le32toh(ptb[i + j])
				    & EMU_CHAN_MAP_PTE_MASK) != silentpage)
					break;
			if (j == numblocks) {
				if ((mem = emuxki_mem_new(sc, i,
						size, type, flags)) == NULL) {
					splx(s);
					return (NULL);
				}
				for (j = 0; j < numblocks; j++)
					ptb[i + j] =
					    htole32((((DMAADDR(mem->dmamem) +
					    j * EMU_PTESIZE)) << 1) | (i + j));
				LIST_INSERT_HEAD(&(sc->mem), mem, next);
				splx(s);
				return (KERNADDR(mem->dmamem));
			} else
				i += j;
			splx(s);
		}
	return (NULL);
}

static void *
emuxki_rmem_alloc(struct emuxki_softc *sc, size_t size,
    struct malloc_type *type, int flags)
{
	struct emuxki_mem *mem;
	int             s;

	mem = emuxki_mem_new(sc, EMU_RMEM, size, type, flags);
	if (mem == NULL)
		return (NULL);

	s = splaudio();
	LIST_INSERT_HEAD(&(sc->mem), mem, next);
	splx(s);

	return (KERNADDR(mem->dmamem));
}

/*
 * emuxki_channel_* : Channel management functions
 * emuxki_chanparms_* : Channel parameters modification functions
 */

/*
 * is splaudio necessary here, can the same voice be manipulated by two
 * different threads at a time ?
 */
static void
emuxki_chanparms_set_defaults(struct emuxki_channel *chan)
{
	chan->fxsend.a.level = chan->fxsend.b.level =
	chan->fxsend.c.level = chan->fxsend.d.level =
	/* for audigy */
	chan->fxsend.e.level = chan->fxsend.f.level =
	chan->fxsend.g.level = chan->fxsend.h.level =
		chan->voice->sc->sc_type & EMUXKI_AUDIGY ?
			0xc0 : 0xff;	/* not max */

	chan->fxsend.a.dest = 0x0;
	chan->fxsend.b.dest = 0x1;
	chan->fxsend.c.dest = 0x2;
	chan->fxsend.d.dest = 0x3;
	/* for audigy */
	chan->fxsend.e.dest = 0x4;
	chan->fxsend.f.dest = 0x5;
	chan->fxsend.g.dest = 0x6;
	chan->fxsend.h.dest = 0x7;

	chan->pitch.initial = 0x0000;	/* shouldn't it be 0xE000 ? */
	chan->pitch.current = 0x0000;	/* should it be 0x0400 */
	chan->pitch.target = 0x0000;	/* the unity pitch shift ? */
	chan->pitch.envelope_amount = 0x00;	/* none */

	chan->initial_attenuation = 0x00;	/* no attenuation */
	chan->volume.current = 0x0000;	/* no volume */
	chan->volume.target = 0xffff;
	chan->volume.envelope.current_state = 0x8000;	/* 0 msec delay */
	chan->volume.envelope.hold_time = 0x7f;	/* 0 msec */
	chan->volume.envelope.attack_time = 0x7F;	/* 5.5msec */
	chan->volume.envelope.sustain_level = 0x7F;	/* full  */
	chan->volume.envelope.decay_time = 0x7F;	/* 22msec  */

	chan->filter.initial_cutoff_frequency = 0xff;	/* no filter */
	chan->filter.current_cutoff_frequency = 0xffff;	/* no filtering */
	chan->filter.target_cutoff_frequency = 0xffff;	/* no filtering */
	chan->filter.lowpass_resonance_height = 0x0;
	chan->filter.interpolation_ROM = 0x1;	/* full band */
	chan->filter.envelope_amount = 0x7f;	/* none */
	chan->filter.LFO_modulation_depth = 0x00;	/* none */

	chan->loop.start = 0x000000;
	chan->loop.end = 0x000010;	/* Why ? */

	chan->modulation.envelope.current_state = 0x8000;
	chan->modulation.envelope.hold_time = 0x00;	/* 127 better ? */
	chan->modulation.envelope.attack_time = 0x00;	/* infinite */
	chan->modulation.envelope.sustain_level = 0x00;	/* off */
	chan->modulation.envelope.decay_time = 0x7f;	/* 22 msec */
	chan->modulation.LFO_state = 0x8000;

	chan->vibrato_LFO.state = 0x8000;
	chan->vibrato_LFO.modulation_depth = 0x00;	/* none */
	chan->vibrato_LFO.vibrato_depth = 0x00;
	chan->vibrato_LFO.frequency = 0x00;	/* Why set to 24 when
						 * initialized ? */

	chan->tremolo_depth = 0x00;
}

/* only call it at splaudio */
static struct emuxki_channel *
emuxki_channel_new(struct emuxki_voice *voice, u_int8_t num)
{
	struct emuxki_channel *chan;

	chan = malloc(sizeof(struct emuxki_channel), M_DEVBUF, M_WAITOK);
	if (chan == NULL)
		return (NULL);

	chan->voice = voice;
	chan->num = num;
	emuxki_chanparms_set_defaults(chan);
	chan->voice->sc->channel[num] = chan;
	return (chan);
}

/* only call it at splaudio */
static void
emuxki_channel_delete(struct emuxki_channel *chan)
{
	chan->voice->sc->channel[chan->num] = NULL;
	free(chan, M_DEVBUF);
}

static void
emuxki_channel_set_fxsend(struct emuxki_channel *chan,
			   struct emuxki_chanparms_fxsend *fxsend)
{
	/* Could do a memcpy ...*/
	chan->fxsend.a.level = fxsend->a.level;
	chan->fxsend.b.level = fxsend->b.level;
	chan->fxsend.c.level = fxsend->c.level;
	chan->fxsend.d.level = fxsend->d.level;
	chan->fxsend.a.dest = fxsend->a.dest;
	chan->fxsend.b.dest = fxsend->b.dest;
	chan->fxsend.c.dest = fxsend->c.dest;
	chan->fxsend.d.dest = fxsend->d.dest;

	/* for audigy */
	chan->fxsend.e.level = fxsend->e.level;
	chan->fxsend.f.level = fxsend->f.level;
	chan->fxsend.g.level = fxsend->g.level;
	chan->fxsend.h.level = fxsend->h.level;
	chan->fxsend.e.dest = fxsend->e.dest;
	chan->fxsend.f.dest = fxsend->f.dest;
	chan->fxsend.g.dest = fxsend->g.dest;
	chan->fxsend.h.dest = fxsend->h.dest;
}

static void
emuxki_channel_set_srate(struct emuxki_channel *chan, u_int32_t srate)
{
	chan->pitch.target = (srate << 8) / 375;
	chan->pitch.target = (chan->pitch.target >> 1) +
		(chan->pitch.target & 1);
	chan->pitch.target &= 0xffff;
	chan->pitch.current = chan->pitch.target;
	chan->pitch.initial =
		(emuxki_rate_to_pitch(srate) >> 8) & EMU_CHAN_IP_MASK;
}

/* voice params must be set before calling this */
static void
emuxki_channel_set_bufparms(struct emuxki_channel *chan,
			     u_int32_t start, u_int32_t end)
{
	chan->loop.start = start & EMU_CHAN_PSST_LOOPSTARTADDR_MASK;
	chan->loop.end = end & EMU_CHAN_DSL_LOOPENDADDR_MASK;
}

static void
emuxki_channel_commit_fx(struct emuxki_channel *chan)
{
	struct emuxki_softc *sc = chan->voice->sc;
        u_int8_t	chano = chan->num;
        
        if(sc->sc_type & EMUXKI_AUDIGY) {
                emuxki_write(sc, chano, EMU_A_CHAN_FXRT1,
                              (chan->fxsend.d.dest << 24) |
                              (chan->fxsend.c.dest << 16) |
                              (chan->fxsend.b.dest << 8) |
                              (chan->fxsend.a.dest));
                emuxki_write(sc, chano, EMU_A_CHAN_FXRT2,
                              (chan->fxsend.h.dest << 24) |
                              (chan->fxsend.g.dest << 16) |
                              (chan->fxsend.f.dest << 8) |
                              (chan->fxsend.e.dest));
                emuxki_write(sc, chano, EMU_A_CHAN_SENDAMOUNTS,
                              (chan->fxsend.e.level << 24) |
                              (chan->fxsend.f.level << 16) |
                              (chan->fxsend.g.level << 8) |
                              (chan->fxsend.h.level));
        } else {
                emuxki_write(sc, chano, EMU_CHAN_FXRT,
                              (chan->fxsend.d.dest << 28) |
                              (chan->fxsend.c.dest << 24) |
                              (chan->fxsend.b.dest << 20) |
                              (chan->fxsend.a.dest << 16));
        }
        
        emuxki_write(sc, chano, 0x10000000 | EMU_CHAN_PTRX,
                      (chan->fxsend.a.level << 8) | chan->fxsend.b.level);
        emuxki_write(sc, chano, EMU_CHAN_DSL,
                      (chan->fxsend.d.level << 24) | chan->loop.end);
        emuxki_write(sc, chano, EMU_CHAN_PSST,
                      (chan->fxsend.c.level << 24) | chan->loop.start);
}

static void
emuxki_channel_commit_parms(struct emuxki_channel *chan)
{
	struct emuxki_voice *voice = chan->voice;
	struct emuxki_softc *sc = voice->sc;
	u_int32_t start, mapval;
	u_int8_t chano = chan->num;
	int s;

	start = chan->loop.start +
		(voice->stereo ? 28 : 30) * (voice->b16 + 1);
	mapval = DMAADDR(sc->silentpage) << 1 | EMU_CHAN_MAP_PTI_MASK;

	s = splaudio();
	emuxki_write(sc, chano, EMU_CHAN_CPF_STEREO, voice->stereo);

	emuxki_channel_commit_fx(chan);

	emuxki_write(sc, chano, EMU_CHAN_CCCA,
		(chan->filter.lowpass_resonance_height << 28) |
		(chan->filter.interpolation_ROM << 25) |
		(voice->b16 ? 0 : EMU_CHAN_CCCA_8BITSELECT) | start);
	emuxki_write(sc, chano, EMU_CHAN_Z1, 0);
	emuxki_write(sc, chano, EMU_CHAN_Z2, 0);
	emuxki_write(sc, chano, EMU_CHAN_MAPA, mapval);
	emuxki_write(sc, chano, EMU_CHAN_MAPB, mapval);
	emuxki_write(sc, chano, EMU_CHAN_CVCF_CURRFILTER,
		chan->filter.current_cutoff_frequency);
	emuxki_write(sc, chano, EMU_CHAN_VTFT_FILTERTARGET,
		chan->filter.target_cutoff_frequency);
	emuxki_write(sc, chano, EMU_CHAN_ATKHLDM,
		(chan->modulation.envelope.hold_time << 8) |
		chan->modulation.envelope.attack_time);
	emuxki_write(sc, chano, EMU_CHAN_DCYSUSM,
		(chan->modulation.envelope.sustain_level << 8) |
		chan->modulation.envelope.decay_time);
	emuxki_write(sc, chano, EMU_CHAN_LFOVAL1,
		chan->modulation.LFO_state);
	emuxki_write(sc, chano, EMU_CHAN_LFOVAL2,
		chan->vibrato_LFO.state);
	emuxki_write(sc, chano, EMU_CHAN_FMMOD,
		(chan->vibrato_LFO.modulation_depth << 8) |
		chan->filter.LFO_modulation_depth);
	emuxki_write(sc, chano, EMU_CHAN_TREMFRQ,
		(chan->tremolo_depth << 8));
	emuxki_write(sc, chano, EMU_CHAN_FM2FRQ2,
		(chan->vibrato_LFO.vibrato_depth << 8) |
		chan->vibrato_LFO.frequency);
	emuxki_write(sc, chano, EMU_CHAN_ENVVAL,
		chan->modulation.envelope.current_state);
	emuxki_write(sc, chano, EMU_CHAN_ATKHLDV,
		(chan->volume.envelope.hold_time << 8) |
		chan->volume.envelope.attack_time);
	emuxki_write(sc, chano, EMU_CHAN_ENVVOL,
		chan->volume.envelope.current_state);
	emuxki_write(sc, chano, EMU_CHAN_PEFE,
		(chan->pitch.envelope_amount << 8) |
		chan->filter.envelope_amount);
	splx(s);
}

static void
emuxki_channel_start(struct emuxki_channel *chan)
{
	struct emuxki_voice *voice = chan->voice;
	struct emuxki_softc *sc = voice->sc;
	u_int8_t        cache_sample, cache_invalid_size, chano = chan->num;
	u_int32_t       sample;
	int             s;

	cache_sample = voice->stereo ? 4 : 2;
	sample = voice->b16 ? 0x00000000 : 0x80808080;
	cache_invalid_size = (voice->stereo ? 28 : 30) * (voice->b16 + 1);

	s = splaudio();
	while (cache_sample--) {
		emuxki_write(sc, chano, EMU_CHAN_CD0 + cache_sample,
			sample);
	}
	emuxki_write(sc, chano, EMU_CHAN_CCR_CACHEINVALIDSIZE, 0);
	emuxki_write(sc, chano, EMU_CHAN_CCR_READADDRESS, 64);
	emuxki_write(sc, chano, EMU_CHAN_CCR_CACHEINVALIDSIZE,
		cache_invalid_size);
	emuxki_write(sc, chano, EMU_CHAN_IFATN,
		(chan->filter.target_cutoff_frequency << 8) |
		chan->initial_attenuation);
	emuxki_write(sc, chano, EMU_CHAN_VTFT_VOLUMETARGET,
		chan->volume.target);
	emuxki_write(sc, chano, EMU_CHAN_CVCF_CURRVOL,
		chan->volume.current);
	emuxki_write(sc, 0,
		EMU_MKSUBREG(1, chano, EMU_SOLEL + (chano >> 5)),
		0);	/* Clear stop on loop */
	emuxki_write(sc, 0,
		EMU_MKSUBREG(1, chano, EMU_CLIEL + (chano >> 5)),
		0);	/* Clear loop interrupt */
	emuxki_write(sc, chano, EMU_CHAN_DCYSUSV,
		(chan->volume.envelope.sustain_level << 8) |
		chan->volume.envelope.decay_time);
	emuxki_write(sc, chano, EMU_CHAN_PTRX_PITCHTARGET,
		chan->pitch.target);
	emuxki_write(sc, chano, EMU_CHAN_CPF_PITCH,
		chan->pitch.current);
	emuxki_write(sc, chano, EMU_CHAN_IP, chan->pitch.initial);

	splx(s);
}

static void
emuxki_channel_stop(struct emuxki_channel *chan)
{
	int s;
	u_int8_t chano = chan->num;
	struct emuxki_softc *sc = chan->voice->sc;

	s = splaudio();
	emuxki_write(sc, chano, EMU_CHAN_PTRX_PITCHTARGET, 0);
	emuxki_write(sc, chano, EMU_CHAN_CPF_PITCH, 0);
	emuxki_write(sc, chano, EMU_CHAN_IFATN_ATTENUATION, 0xff);
	emuxki_write(sc, chano, EMU_CHAN_VTFT_VOLUMETARGET, 0);
	emuxki_write(sc, chano, EMU_CHAN_CVCF_CURRVOL, 0);
	emuxki_write(sc, chano, EMU_CHAN_IP, 0);
	splx(s);
}

/*
 * Voices management
 * emuxki_voice_dataloc : use(play or rec) independent dataloc union helpers
 * emuxki_voice_channel_* : play part of dataloc union helpers
 * emuxki_voice_recsrc_* : rec part of dataloc union helpers
 */

/* Allocate channels for voice in case of play voice */
static int
emuxki_voice_channel_create(struct emuxki_voice *voice)
{
	struct emuxki_channel **channel = voice->sc->channel;
	u_int8_t i, stereo = voice->stereo;
	int s;

	for (i = 0; i < EMU_NUMCHAN; i += stereo + 1) {
		if ((stereo && (channel[i + 1] != NULL)) ||
		    (channel[i] != NULL))	/* Looking for free channels */
			continue;
		s = splaudio();
		if (stereo) {
			voice->dataloc.chan[1] =
				emuxki_channel_new(voice, i + 1);
			if (voice->dataloc.chan[1] == NULL) {
				splx(s);
				return (ENOMEM);
			}
		}
		voice->dataloc.chan[0] = emuxki_channel_new(voice, i);
		if (voice->dataloc.chan[0] == NULL) {
			if (stereo) {
				emuxki_channel_delete(voice->dataloc.chan[1]);
				voice->dataloc.chan[1] = NULL;
			}
			splx(s);
			return (ENOMEM);
		}
		splx(s);
		return (0);
	}
	return (EAGAIN);
}

/* When calling this function we assume no one can access the voice */
static void
emuxki_voice_channel_destroy(struct emuxki_voice *voice)
{
	emuxki_channel_delete(voice->dataloc.chan[0]);
	voice->dataloc.chan[0] = NULL;
	if (voice->stereo)
		emuxki_channel_delete(voice->dataloc.chan[1]);
	voice->dataloc.chan[1] = NULL;
}

/*
 * Will come back when used in voice_dataloc_create
 */
static int
emuxki_recsrc_reserve(struct emuxki_voice *voice, emuxki_recsrc_t source)
{
	if (source < 0 || source >= EMU_NUMRECSRCS) {
#ifdef EMUXKI_DEBUG
		printf("Tried to reserve invalid source: %d\n", source);
#endif
		return (EINVAL);
	}
	if (voice->sc->recsrc[source] == voice)
		return (0);			/* XXX */
	if (voice->sc->recsrc[source] != NULL)
		return (EBUSY);
	voice->sc->recsrc[source] = voice;
	return (0);
}

/* When calling this function we assume the voice is stopped */
static void
emuxki_voice_recsrc_release(struct emuxki_softc *sc, emuxki_recsrc_t source)
{
	sc->recsrc[source] = NULL;
}

static int
emuxki_voice_dataloc_create(struct emuxki_voice *voice)
{
	int             error;

	if (voice->use & EMU_VOICE_USE_PLAY) {
		if ((error = emuxki_voice_channel_create(voice)))
			return (error);
	} else {
		if ((error =
		    emuxki_recsrc_reserve(voice, voice->dataloc.source)))
			return (error);
	}
	return (0);
}

static void
emuxki_voice_dataloc_destroy(struct emuxki_voice *voice)
{
	if (voice->use & EMU_VOICE_USE_PLAY) {
		if (voice->dataloc.chan[0] != NULL)
			emuxki_voice_channel_destroy(voice);
	} else {
		if (voice->dataloc.source != EMU_RECSRC_NOTSET) {
			emuxki_voice_recsrc_release(voice->sc,
						     voice->dataloc.source);
			voice->dataloc.source = EMU_RECSRC_NOTSET;
		}
	}
}

static struct emuxki_voice *
emuxki_voice_new(struct emuxki_softc *sc, u_int8_t use)
{
	struct emuxki_voice *voice;
	int             s;

	s = splaudio();
	voice = sc->lvoice;
	sc->lvoice = NULL;
	splx(s);

	if (!voice) {
		if (!(voice = malloc(sizeof(*voice), M_DEVBUF, M_WAITOK)))
			return (NULL);
	} else if (voice->use != use) 
		emuxki_voice_dataloc_destroy(voice);
	else
		goto skip_initialize;

	voice->sc = sc;
	voice->state = !EMU_VOICE_STATE_STARTED;
	voice->stereo = EMU_VOICE_STEREO_NOTSET;
	voice->b16 = 0;
	voice->sample_rate = 0;
	if (use & EMU_VOICE_USE_PLAY)
		voice->dataloc.chan[0] = voice->dataloc.chan[1] = NULL;
	else
		voice->dataloc.source = EMU_RECSRC_NOTSET;
	voice->buffer = NULL;
	voice->blksize = 0;
	voice->trigblk = 0;
	voice->blkmod = 0;
	voice->inth = NULL;
	voice->inthparam = NULL;
	voice->use = use;

skip_initialize:
	s = splaudio();
	LIST_INSERT_HEAD((&sc->voices), voice, next);
	splx(s);

	return (voice);
}

static void
emuxki_voice_delete(struct emuxki_voice *voice)
{
	struct emuxki_softc *sc = voice->sc;
	struct emuxki_voice *lvoice;
	int s;

	if (voice->state & EMU_VOICE_STATE_STARTED)
		emuxki_voice_halt(voice);

	s = splaudio();
	LIST_REMOVE(voice, next);
	lvoice = sc->lvoice;
	sc->lvoice = voice;
	splx(s);

	if (lvoice) {
		emuxki_voice_dataloc_destroy(lvoice);
		free(lvoice, M_DEVBUF);
	}
}

static int
emuxki_voice_set_stereo(struct emuxki_voice *voice, u_int8_t stereo)
{
	int	error;
	emuxki_recsrc_t source = 0; /* XXX: gcc */
	struct emuxki_chanparms_fxsend fxsend;

	if (! (voice->use & EMU_VOICE_USE_PLAY))
		source = voice->dataloc.source;
	emuxki_voice_dataloc_destroy(voice);
	if (! (voice->use & EMU_VOICE_USE_PLAY))
		voice->dataloc.source = source;
	voice->stereo = stereo;
	if ((error = emuxki_voice_dataloc_create(voice)))
	  return (error);
	if (voice->use & EMU_VOICE_USE_PLAY) {
		fxsend.a.dest = 0x0;
		fxsend.b.dest = 0x1;
		fxsend.c.dest = 0x2;
		fxsend.d.dest = 0x3;
		/* for audigy */
		fxsend.e.dest = 0x4;
		fxsend.f.dest = 0x5;
		fxsend.g.dest = 0x6;
		fxsend.h.dest = 0x7;
		if (voice->stereo) {
			fxsend.a.level = fxsend.c.level = 0xc0;
			fxsend.b.level = fxsend.d.level = 0x00;
			fxsend.e.level = fxsend.g.level = 0xc0;
			fxsend.f.level = fxsend.h.level = 0x00;
			emuxki_channel_set_fxsend(voice->dataloc.chan[0],
						   &fxsend);
			fxsend.a.level = fxsend.c.level = 0x00;
			fxsend.b.level = fxsend.d.level = 0xc0;
			fxsend.e.level = fxsend.g.level = 0x00;
			fxsend.f.level = fxsend.h.level = 0xc0;
			emuxki_channel_set_fxsend(voice->dataloc.chan[1],
						   &fxsend);
		} /* No else : default is good for mono */	
	}
	return (0);
}

static int
emuxki_voice_set_srate(struct emuxki_voice *voice, u_int32_t srate)
{
	if (voice->use & EMU_VOICE_USE_PLAY) {
		if ((srate < 4000) || (srate > 48000))
			return (EINVAL);
		voice->sample_rate = srate;
		emuxki_channel_set_srate(voice->dataloc.chan[0], srate);
		if (voice->stereo)
			emuxki_channel_set_srate(voice->dataloc.chan[1],
						  srate);
	} else {
		if ((srate < 8000) || (srate > 48000))
			return (EINVAL);
		voice->sample_rate = srate;
		if (emuxki_voice_adc_rate(voice) < 0) {
			voice->sample_rate = 0;
			return (EINVAL);
		}
	}
	return (0);
}

static int
emuxki_voice_set_audioparms(struct emuxki_voice *voice, u_int8_t stereo,
			     u_int8_t b16, u_int32_t srate)
{
	int             error = 0;

	if (voice->stereo == stereo && voice->b16 == b16 &&
	    voice->sample_rate == srate)
		return (0);

#ifdef EMUXKI_DEBUG
	printf("Setting %s voice params : %s, %u bits, %u Hz\n",
	       (voice->use & EMU_VOICE_USE_PLAY) ? "play" : "record",
	       stereo ? "stereo" : "mono", (b16 + 1) * 8, srate);
#endif
	
	if (voice->stereo != stereo) {
		if ((error = emuxki_voice_set_stereo(voice, stereo)))
			return (error);
	 }
	voice->b16 = b16;
	if (voice->sample_rate != srate)
		error = emuxki_voice_set_srate(voice, srate);
	return error;
}

/* voice audio parms (see just before) must be set prior to this */
static int
emuxki_voice_set_bufparms(struct emuxki_voice *voice, void *ptr,
			   u_int32_t bufsize, u_int16_t blksize)
{
	struct emuxki_mem *mem;
	struct emuxki_channel **chan;
	u_int32_t start, end;
	u_int8_t sample_size;
	int idx;
	int error = EFAULT;

	LIST_FOREACH(mem, &voice->sc->mem, next) {
		if (KERNADDR(mem->dmamem) != ptr)
			continue;

		voice->buffer = mem;
		sample_size = (voice->b16 + 1) * (voice->stereo + 1);
		voice->trigblk = 0;	/* This shouldn't be needed */
		voice->blkmod = bufsize / blksize;
		if (bufsize % blksize) 	  /* This should not happen */
			voice->blkmod++;
		error = 0;

		if (voice->use & EMU_VOICE_USE_PLAY) {
			voice->blksize = blksize / sample_size;
			chan = voice->dataloc.chan;
			start = mem->ptbidx << 12;
			end = start + bufsize / sample_size;
			emuxki_channel_set_bufparms(chan[0],
						     start, end);
			if (voice->stereo)
				emuxki_channel_set_bufparms(chan[1],
				     start, end);
			voice->timerate = (u_int32_t) 48000 *
			                voice->blksize / voice->sample_rate;
			if (voice->timerate < 5)
				error = EINVAL;
		} else {
			voice->blksize = blksize;
			for(idx = sizeof(emuxki_recbuf_sz) /
			    sizeof(emuxki_recbuf_sz[0]); --idx >= 0;)
				if (emuxki_recbuf_sz[idx] == bufsize)
					break;
			if (idx < 0) {
#ifdef EMUXKI_DEBUG
				printf("Invalid bufsize: %d\n", bufsize);
#endif
				return (EINVAL);
			}
			emuxki_write(voice->sc, 0,
			    emuxki_recsrc_szreg[voice->dataloc.source], idx);
			emuxki_write(voice->sc, 0,
			    emuxki_recsrc_bufaddrreg[voice->dataloc.source],
			    DMAADDR(mem->dmamem));

			/* Use timer to emulate DMA completion interrupt */
			voice->timerate = (u_int32_t) 48000 * blksize /
			    (voice->sample_rate * sample_size);
			if (voice->timerate < 5) {
#ifdef EMUXKI_DEBUG
				printf("Invalid timerate: %d, blksize %d\n",
				    voice->timerate, blksize);
#endif
				error = EINVAL;
			}
		}

		break;
	}

	return (error);
}

static void
emuxki_voice_commit_parms(struct emuxki_voice *voice)
{
	if (voice->use & EMU_VOICE_USE_PLAY) {
		emuxki_channel_commit_parms(voice->dataloc.chan[0]);
		if (voice->stereo)
			emuxki_channel_commit_parms(voice->dataloc.chan[1]);
	}
}

static u_int32_t
emuxki_voice_curaddr(struct emuxki_voice *voice)
{
	int idxreg = 0;

	/* XXX different semantics in these cases */
	if (voice->use & EMU_VOICE_USE_PLAY) {
		/* returns number of samples (an l/r pair counts 1) */
		return (emuxki_read(voice->sc,
				     voice->dataloc.chan[0]->num,
				     EMU_CHAN_CCCA_CURRADDR) -
			voice->dataloc.chan[0]->loop.start);
	} else {
		/* returns number of bytes */
		switch (voice->dataloc.source) {
			case EMU_RECSRC_MIC:
				idxreg = (voice->sc->sc_type & EMUXKI_AUDIGY) ?
					EMU_A_MICIDX : EMU_MICIDX;
				break;
			case EMU_RECSRC_ADC:
				idxreg = (voice->sc->sc_type & EMUXKI_AUDIGY) ?
					EMU_A_ADCIDX : EMU_ADCIDX;
				break;
			case EMU_RECSRC_FX:
				idxreg = EMU_FXIDX;
				break;
			default:
#ifdef EMUXKI_DEBUG
				printf("emu: bad recording source!\n");
#endif
				break;
		}
		return (emuxki_read(voice->sc, 0, EMU_RECIDX(idxreg))
				& EMU_RECIDX_MASK);
	}
	return (0);
}

static void
emuxki_resched_timer(struct emuxki_softc *sc)
{
	struct emuxki_voice *voice;
	u_int16_t       timerate = 1024;
	u_int8_t	active = 0;
	int s;

	s = splaudio();
	LIST_FOREACH(voice, &sc->voices, next) {
		if ((voice->state & EMU_VOICE_STATE_STARTED) == 0)
			continue;
		active = 1;
		if (voice->timerate < timerate)
			timerate = voice->timerate;
	}

	if (timerate & ~EMU_TIMER_RATE_MASK)
		timerate = 0;
	bus_space_write_2(sc->sc_iot, sc->sc_ioh, EMU_TIMER, timerate);
	if (!active && (sc->timerstate & EMU_TIMER_STATE_ENABLED)) {
		bus_space_write_4(sc->sc_iot, sc->sc_ioh, EMU_INTE,
			bus_space_read_4(sc->sc_iot, sc->sc_ioh, EMU_INTE) &
			~EMU_INTE_INTERTIMERENB);
		sc->timerstate &= ~EMU_TIMER_STATE_ENABLED;
	} else if (active && !(sc->timerstate & EMU_TIMER_STATE_ENABLED)) {
		bus_space_write_4(sc->sc_iot, sc->sc_ioh, EMU_INTE,
			bus_space_read_4(sc->sc_iot, sc->sc_ioh, EMU_INTE) |
			EMU_INTE_INTERTIMERENB);
		sc->timerstate |= EMU_TIMER_STATE_ENABLED;
	}
	splx(s);
}

static int
emuxki_voice_adc_rate(struct emuxki_voice *voice)
{
	switch(voice->sample_rate) {
		case 48000:
			return EMU_ADCCR_SAMPLERATE_48;
			break;
		case 44100:
			return EMU_ADCCR_SAMPLERATE_44;
			break;
		case 32000:
			return EMU_ADCCR_SAMPLERATE_32;
			break;
		case 24000:
			return EMU_ADCCR_SAMPLERATE_24;
			break;
		case 22050:
			return EMU_ADCCR_SAMPLERATE_22;
			break;
		case 16000:
			return EMU_ADCCR_SAMPLERATE_16;
			break;
		case 12000:
			if(voice->sc->sc_type & EMUXKI_AUDIGY)
				return EMU_A_ADCCR_SAMPLERATE_12;
			else {
#ifdef EMUXKI_DEBUG
				printf("recording sample_rate not supported : %u\n", voice->sample_rate);
#endif
				return (-1);
			}
			break;
		case 11000:
			if(voice->sc->sc_type & EMUXKI_AUDIGY)
				return EMU_A_ADCCR_SAMPLERATE_11;
			else
				return EMU_ADCCR_SAMPLERATE_11;
			break;
		case 8000:
			if(voice->sc->sc_type & EMUXKI_AUDIGY)
				return EMU_A_ADCCR_SAMPLERATE_8;
			else
				return EMU_ADCCR_SAMPLERATE_8;
			break;
		default:
#ifdef EMUXKI_DEBUG
				printf("recording sample_rate not supported : %u\n", voice->sample_rate);
#endif
				return (-1);
	}
	return (-1);  /* shouldn't get here */
}


static void
emuxki_voice_start(struct emuxki_voice *voice,
		    void (*inth) (void *), void *inthparam)
{
	u_int32_t val;

	voice->inth = inth;
	voice->inthparam = inthparam;
	if (voice->use & EMU_VOICE_USE_PLAY) {
		voice->trigblk = 1;
		emuxki_channel_start(voice->dataloc.chan[0]);
		if (voice->stereo)
			emuxki_channel_start(voice->dataloc.chan[1]);
	} else {
		voice->trigblk = 1;
		switch (voice->dataloc.source) {
		case EMU_RECSRC_ADC:
			/* XXX need to program DSP to output L+R
			 * XXX in monaural case? */
			if (voice->sc->sc_type & EMUXKI_AUDIGY) {
				val = EMU_A_ADCCR_LCHANENABLE;
				if (voice->stereo)
					val |= EMU_A_ADCCR_RCHANENABLE;
			} else {
				val = EMU_ADCCR_LCHANENABLE;
				if (voice->stereo)
					val |= EMU_ADCCR_RCHANENABLE;
			}
			val |= emuxki_voice_adc_rate(voice);
			emuxki_write(voice->sc, 0, EMU_ADCCR, 0);
			emuxki_write(voice->sc, 0, EMU_ADCCR, val);
			break;
		case EMU_RECSRC_MIC:
		case EMU_RECSRC_FX:
			printf("unimplemented\n");
			break;
		case EMU_RECSRC_NOTSET:
		default:
		        break;
		}
#if 0
		/* DMA completion interrupt is useless; use timer */
		int s;
		s = splaudio();
		val = emu_rd(sc, INTE, 4);
		val |= emuxki_recsrc_intrmasks[voice->dataloc.source];
		emu_wr(sc, INTE, val, 4);
		splx(s);
#endif
	}
	voice->state |= EMU_VOICE_STATE_STARTED;
	emuxki_resched_timer(voice->sc);
}

static void
emuxki_voice_halt(struct emuxki_voice *voice)
{
	if (voice->use & EMU_VOICE_USE_PLAY) {
		emuxki_channel_stop(voice->dataloc.chan[0]);
		if (voice->stereo)
			emuxki_channel_stop(voice->dataloc.chan[1]);
	} else {
		switch (voice->dataloc.source) {
		case EMU_RECSRC_ADC:
			emuxki_write(voice->sc, 0, EMU_ADCCR, 0);
			break;
		case EMU_RECSRC_FX:
		case EMU_RECSRC_MIC:
			printf("unimplemented\n");
			break;
		case EMU_RECSRC_NOTSET:
			printf("Bad dataloc.source\n");
		}
		/* This should reset buffer pointer */
		emuxki_write(voice->sc, 0,
		    emuxki_recsrc_szreg[voice->dataloc.source],
		    EMU_RECBS_BUFSIZE_NONE);
#if 0
		int s;
		s = splaudio();
		val = emu_rd(sc, INTE, 4);
		val &= ~emuxki_recsrc_intrmasks[voice->dataloc.source];
		emu_wr(sc, INTE, val, 4);
		splx(s);
#endif
	}
	voice->state &= ~EMU_VOICE_STATE_STARTED;
	emuxki_resched_timer(voice->sc);
}

/*
 * The interrupt handler
 */
static int
emuxki_intr(void *arg)
{
	struct emuxki_softc *sc = arg;
	u_int32_t       ipr, curblk;
	struct emuxki_voice *voice;
	int claim = 0;

	while ((ipr = bus_space_read_4(sc->sc_iot, sc->sc_ioh, EMU_IPR))) {
		if (ipr & EMU_IPR_INTERVALTIMER) {
			LIST_FOREACH(voice, &sc->voices, next) {
				if ((voice->state &
				      EMU_VOICE_STATE_STARTED) == 0)
					continue;

				curblk = emuxki_voice_curaddr(voice) /
				       voice->blksize;
#if 0
				if (curblk == voice->trigblk) {
					voice->inth(voice->inthparam);
					voice->trigblk++;
					voice->trigblk %= voice->blkmod;
				}
#else
				while ((curblk >= voice->trigblk &&
				    curblk < (voice->trigblk + voice->blkmod / 2)) ||
				    ((int)voice->trigblk - (int)curblk) >
				    (voice->blkmod / 2 + 1)) {
					voice->inth(voice->inthparam);
					voice->trigblk++;
					voice->trigblk %= voice->blkmod;
				}
#endif
			}
		}

		/* Got interrupt */
		bus_space_write_4(sc->sc_iot, sc->sc_ioh, EMU_IPR, ipr);

		claim = 1;
	}

	return (claim);
}


/*
 * Audio Architecture callbacks
 */

static int
emuxki_open(void *addr, int flags)
{
	struct emuxki_softc *sc = addr;

#ifdef EMUXKI_DEBUG
	printf("%s: emuxki_open called\n", sc->sc_dev.dv_xname);
#endif

	/*
	 * Multiple voice support would be added as soon as I find a way to
	 * trick the audio arch into supporting multiple voices.
	 * Or I might integrate a modified audio arch supporting
	 * multiple voices.
	 */

	/*
	 * I did this because i have problems identifying the selected
	 * recording source(s) which is necessary when setting recording
	 * params This will be addressed very soon
	 */
	if (flags & AUOPEN_READ) {
		sc->rvoice = emuxki_voice_new(sc, 0 /* EMU_VOICE_USE_RECORD */);
		if (sc->rvoice == NULL)
			return (EBUSY);

		/* XXX Hardcode RECSRC_ADC for now */
		sc->rvoice->dataloc.source = EMU_RECSRC_ADC;
	}

	if (flags & AUOPEN_WRITE) {
		sc->pvoice = emuxki_voice_new(sc, EMU_VOICE_USE_PLAY);
		if (sc->pvoice == NULL) {
			if (sc->rvoice) {
				emuxki_voice_delete(sc->rvoice);
				sc->rvoice = NULL;
			}
			return (EBUSY);
		}
	}

	return (0);
}

static void
emuxki_close(void *addr)
{
	struct emuxki_softc *sc = addr;

#ifdef EMUXKI_DEBUG
	printf("%s: emu10K1_close called\n", sc->sc_dev.dv_xname);
#endif

	/* No multiple voice support for now */
	if (sc->rvoice != NULL) {
		emuxki_voice_delete(sc->rvoice);
		sc->rvoice = NULL;
	}
	if (sc->pvoice != NULL) {
		emuxki_voice_delete(sc->pvoice);
		sc->pvoice = NULL;
	}
}

static int
emuxki_query_encoding(void *addr, struct audio_encoding *fp)
{
#ifdef EMUXKI_DEBUG
	struct emuxki_softc *sc = addr;

	printf("%s: emuxki_query_encoding called\n", sc->sc_dev.dv_xname);
#endif

	switch (fp->index) {
	case 0:
		strcpy(fp->name, AudioEulinear);
		fp->encoding = AUDIO_ENCODING_ULINEAR;
		fp->precision = 8;
		fp->flags = 0;
		break;
	case 1:
		strcpy(fp->name, AudioEmulaw);
		fp->encoding = AUDIO_ENCODING_ULAW;
		fp->precision = 8;
		fp->flags = AUDIO_ENCODINGFLAG_EMULATED;
		break;
	case 2:
		strcpy(fp->name, AudioEalaw);
		fp->encoding = AUDIO_ENCODING_ALAW;
		fp->precision = 8;
		fp->flags = AUDIO_ENCODINGFLAG_EMULATED;
		break;
	case 3:
		strcpy(fp->name, AudioEslinear);
		fp->encoding = AUDIO_ENCODING_SLINEAR;
		fp->precision = 8;
		fp->flags = AUDIO_ENCODINGFLAG_EMULATED;
		break;
	case 4:
		strcpy(fp->name, AudioEslinear_le);
		fp->encoding = AUDIO_ENCODING_SLINEAR_LE;
		fp->precision = 16;
		fp->flags = 0;
		break;
	case 5:
		strcpy(fp->name, AudioEulinear_le);
		fp->encoding = AUDIO_ENCODING_ULINEAR_LE;
		fp->precision = 16;
		fp->flags = AUDIO_ENCODINGFLAG_EMULATED;
		break;
	case 6:
		strcpy(fp->name, AudioEslinear_be);
		fp->encoding = AUDIO_ENCODING_SLINEAR_BE;
		fp->precision = 16;
		fp->flags = AUDIO_ENCODINGFLAG_EMULATED;
		break;
	case 7:
		strcpy(fp->name, AudioEulinear_be);
		fp->encoding = AUDIO_ENCODING_ULINEAR_BE;
		fp->precision = 16;
		fp->flags = AUDIO_ENCODINGFLAG_EMULATED;
		break;
	default:
		return (EINVAL);
	}
	return (0);
}

static int
emuxki_set_vparms(struct emuxki_voice *voice, const audio_params_t *p,
		  stream_filter_list_t *fil)
{
	int mode, i;

	mode = (voice->use & EMU_VOICE_USE_PLAY) ?
		AUMODE_PLAY : AUMODE_RECORD;
	i = auconv_set_converter(emuxki_formats, EMUXKI_NFORMATS,
				 mode, p, FALSE, fil);
	if (i < 0)
		return EINVAL;
	if (fil->req_size > 0)
		p = &fil->filters[0].param;
	return emuxki_voice_set_audioparms
		(voice, p->channels == 2, p->precision == 16, p->sample_rate);
}

static int
emuxki_set_params(void *addr, int setmode, int usemode,
		  audio_params_t *play, audio_params_t *rec,
		  stream_filter_list_t *pfil, stream_filter_list_t *rfil)
{
	struct emuxki_softc *sc = addr;
	int	     mode, error;
	struct audio_params *p;
	struct emuxki_voice *v;
	stream_filter_list_t *fil;

	for (mode = AUMODE_RECORD; mode != -1;
	     mode = mode == AUMODE_RECORD ? AUMODE_PLAY : -1) {
		if ((usemode & setmode & mode) == 0)
			continue;

		if (mode == AUMODE_PLAY) {
			p = play;
			fil = pfil;
			v = sc->pvoice;
		} else {
			p = rec;
			fil = rfil;
			v = sc->rvoice;
		}

		if (v == NULL) {
			continue;
		}

		/* No multiple voice support for now */
		if ((error = emuxki_set_vparms(v, p, fil)))
			return (error);
	}

	return (0);
}

static int
emuxki_halt_output(void *addr)
{
	struct emuxki_softc *sc = addr;

	/* No multiple voice support for now */
	if (sc->pvoice == NULL)
		return (ENXIO);

	emuxki_voice_halt(sc->pvoice);
	return (0);
}

static int
emuxki_halt_input(void *addr)
{
	struct emuxki_softc *sc = addr;

#ifdef EMUXKI_DEBUG
	printf("%s: emuxki_halt_input called\n", sc->sc_dev.dv_xname);
#endif

	/* No multiple voice support for now */
	if (sc->rvoice == NULL)
		return (ENXIO);
	emuxki_voice_halt(sc->rvoice);
	return (0);
}

static int
emuxki_getdev(void *addr, struct audio_device *dev)
{
	struct emuxki_softc *sc = addr;
	*dev = sc->sc_audv;

	return (0);
}

static int
emuxki_set_port(void *addr, mixer_ctrl_t *mctl)
{
	struct emuxki_softc *sc = addr;

	return sc->codecif->vtbl->mixer_set_port(sc->codecif, mctl);
}

static int
emuxki_get_port(void *addr, mixer_ctrl_t *mctl)
{
	struct emuxki_softc *sc = addr;

	return sc->codecif->vtbl->mixer_get_port(sc->codecif, mctl);
}

static int
emuxki_query_devinfo(void *addr, mixer_devinfo_t *minfo)
{
	struct emuxki_softc *sc = addr;

	return sc->codecif->vtbl->query_devinfo(sc->codecif, minfo);
}

static void *
emuxki_allocm(void *addr, int direction, size_t size,
    struct malloc_type *type, int flags)
{
	struct emuxki_softc *sc = addr;

	if (direction == AUMODE_PLAY)
		return emuxki_pmem_alloc(sc, size, type, flags);
	else
		return emuxki_rmem_alloc(sc, size, type, flags);
}

static void
emuxki_freem(void *addr, void *ptr, struct malloc_type *type)
{
	struct emuxki_softc *sc = addr;
	int	     i, s;
	struct emuxki_mem *mem;
	size_t	  numblocks;
	u_int32_t      *ptb, silentpage;

	ptb = KERNADDR(sc->ptb);
	silentpage = DMAADDR(sc->silentpage) << 1;
	LIST_FOREACH(mem, &sc->mem, next) {
		if (KERNADDR(mem->dmamem) != ptr)
			continue;

		s = splaudio();
		if (mem->ptbidx != EMU_RMEM) {
			numblocks = DMASIZE(mem->dmamem) / EMU_PTESIZE;
			if (DMASIZE(mem->dmamem) % EMU_PTESIZE)
				numblocks++;
			for (i = 0; i < numblocks; i++)
				ptb[mem->ptbidx + i] =
				    htole32(silentpage | (mem->ptbidx + i));
		}
		LIST_REMOVE(mem, next);
		splx(s);

		emuxki_mem_delete(mem, type);
		break;
	}
}

/* blocksize should be a divisor of allowable buffersize */
/* XXX probably this could be done better */
static int
emuxki_round_blocksize(void *addr, int blksize,
		       int mode, const audio_params_t* param)
{
#if 0
	struct emuxki_softc *sc = addr;
	struct audio_softc *au;
#endif
	int bufsize;
#if 0
	if (sc == NULL)
		return blksize;

	au = (void *)sc->sc_audev;
	if (au == NULL)
		return blksize;

	bufsize = emuxki_round_buffersize(sc, AUMODE_RECORD,
	    au->sc_rr.bufsize);
#else
	bufsize = 65536;
#endif

	while (bufsize > blksize)
		bufsize /= 2;

	return bufsize;
}
	
static size_t
emuxki_round_buffersize(void *addr, int direction, size_t bsize)
{

	if (direction == AUMODE_PLAY) {
		if (bsize < EMU_PTESIZE)
			bsize = EMU_PTESIZE;
		else if (bsize > (EMU_PTESIZE * EMU_MAXPTE))
			bsize = EMU_PTESIZE * EMU_MAXPTE;
		/* Would be better if set to max available */
		else if (bsize % EMU_PTESIZE)
			bsize = bsize -
				(bsize % EMU_PTESIZE) +
				EMU_PTESIZE;
	} else {
		int idx;

		/* find nearest lower recbuf size */
		for(idx = sizeof(emuxki_recbuf_sz) /
		    sizeof(emuxki_recbuf_sz[0]); --idx >= 0; ) {
			if (bsize >= emuxki_recbuf_sz[idx]) {
				bsize = emuxki_recbuf_sz[idx];
				break;
			}
		}

		if (bsize == 0)
			bsize = 384;
	}

	return (bsize);
}

static paddr_t
emuxki_mappage(void *addr, void *ptr, off_t off, int prot)
{
	struct emuxki_softc *sc = addr;
	struct emuxki_mem *mem;

	LIST_FOREACH(mem, &sc->mem, next) {
		if (KERNADDR(mem->dmamem) == ptr) {
			struct dmamem *dm = mem->dmamem;

			return bus_dmamem_mmap(dm->dmat, dm->segs, dm->nsegs,
			       off, prot, BUS_DMA_WAITOK);
		}
	}

	return (-1);
}

static int
emuxki_get_props(void *addr)
{
	return (AUDIO_PROP_MMAP | AUDIO_PROP_INDEPENDENT |
		AUDIO_PROP_FULLDUPLEX);
}

static int
emuxki_trigger_output(void *addr, void *start, void *end, int blksize,
		       void (*inth) (void *), void *inthparam,
		       const audio_params_t *params)
{
	struct emuxki_softc *sc = addr;
	/* No multiple voice support for now */
	struct emuxki_voice *voice = sc->pvoice;
	int	     error;

	if (voice == NULL)
		return (ENXIO);
	if ((error = emuxki_voice_set_audioparms(voice, params->channels == 2,
						 params->precision == 16,
						 params->sample_rate)))
		return (error);
	if ((error = emuxki_voice_set_bufparms(voice, start,
				(caddr_t)end - (caddr_t)start, blksize)))
		return (error);
	emuxki_voice_commit_parms(voice);
	emuxki_voice_start(voice, inth, inthparam);

	return (0);
}

static int
emuxki_trigger_input(void *addr, void *start, void *end, int blksize,
		      void (*inth) (void *), void *inthparam,
		      const audio_params_t *params)
{
	struct emuxki_softc *sc = addr;
	/* No multiple voice support for now */
	struct emuxki_voice *voice = sc->rvoice;
	int	error;

	if (voice == NULL)
		return (ENXIO);
	if ((error = emuxki_voice_set_audioparms(voice, params->channels == 2,
						 params->precision == 16,
						 params->sample_rate)))
		return (error);
	if ((error = emuxki_voice_set_bufparms(voice, start,
						(caddr_t)end - (caddr_t)start,
						blksize)))
		return (error);
	emuxki_voice_start(voice, inth, inthparam);

	return (0);
}


/*
 * AC97 callbacks
 */

static int
emuxki_ac97_attach(void *arg, struct ac97_codec_if *codecif)
{
	struct emuxki_softc *sc = arg;

	sc->codecif = codecif;
	return (0);
}

static int
emuxki_ac97_read(void *arg, u_int8_t reg, u_int16_t *val)
{
	struct emuxki_softc *sc = arg;
	int s;

	s = splaudio();
	bus_space_write_1(sc->sc_iot, sc->sc_ioh, EMU_AC97ADDR, reg);
	*val = bus_space_read_2(sc->sc_iot, sc->sc_ioh, EMU_AC97DATA);
	splx(s);

	return (0);
}

static int
emuxki_ac97_write(void *arg, u_int8_t reg, u_int16_t val)
{
	struct emuxki_softc *sc = arg;
	int s;

	s = splaudio();
	bus_space_write_1(sc->sc_iot, sc->sc_ioh, EMU_AC97ADDR, reg);
	bus_space_write_2(sc->sc_iot, sc->sc_ioh, EMU_AC97DATA, val);
	splx(s);

	return (0);
}

static int
emuxki_ac97_reset(void *arg)
{
	return 0;
}

enum ac97_host_flags
emuxki_ac97_flags(void *arg)
{
  return (AC97_HOST_SWAPPED_CHANNELS);
}
