/*	$NetBSD: esmvar.h,v 1.1 2001/01/08 19:54:31 rh Exp $	*/

/*-
 * Copyright (c) 2000, 2001 Rene Hexel <rh@netbsd.org>
 * All rights reserved.
 *
 * Copyright (c) 2000 Taku YAMAMOTO <taku@cent.saitama-u.ac.jp>
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * Taku Id: maestro.c,v 1.12 2000/09/06 03:32:34 taku Exp
 * FreeBSD: /c/ncvs/src/sys/dev/sound/pci/maestro.c,v 1.4 2000/12/18 01:36:35 cg Exp
 *
 */

/*
 * Credits:
 *
 * This code is based on the FreeBSD driver written by Taku YAMAMOTO
 *
 *
 * Original credits from the FreeBSD driver:
 *
 * Part of this code (especially in many magic numbers) was heavily inspired
 * by the Linux driver originally written by
 * Alan Cox <alan.cox@linux.org>, modified heavily by
 * Zach Brown <zab@zabbo.net>.
 *
 * busdma()-ize and buffer size reduction were suggested by
 * Cameron Grant <gandalf@vilnya.demon.co.uk>.
 * Also he showed me the way to use busdma() suite.
 *
 * Internal speaker problems on NEC VersaPro's and Dell Inspiron 7500
 * were looked at by
 * Munehiro Matsuda <haro@tk.kubota.co.jp>,
 * who brought patches based on the Linux driver with some simplification.
 */

/* IRQ timer fequency limits */
#define MAESTRO_MINFREQ	24
#define MAESTRO_MAXFREQ	48000

struct esm_dma {
	bus_dmamap_t		map;
	caddr_t			addr;
	bus_dma_segment_t	segs[1];
	int			nsegs;
	size_t			size;
	struct esm_dma		*next;
};

#define DMAADDR(p) ((p)->map->dm_segs[0].ds_addr)
#define KERNADDR(p) ((void *)((p)->addr))

struct esm_chinfo {
	u_int32_t		base;		/* DMA base */
	u_int32_t		blocksize;	/* block size in bytes */
	unsigned		num;		/* logical channel number */
	u_int16_t		aputype;	/* APU channel type */
	u_int16_t		apublk;		/* blk size in samples per ch */
	u_int16_t		apubuf;		/* buf size in samples per ch */
	u_int16_t		nextirq;	/* pos to trigger next IRQ at */
	u_int16_t		wcreg_tpl;	/* wavecache tag and format */
	u_int16_t		sample_rate;
};

struct esm_softc {
	struct device		sc_dev;

	bus_space_tag_t		st;
	bus_space_handle_t	sh;

	pcitag_t		tag;
	pci_chipset_tag_t	pc;
	bus_dma_tag_t		dmat;
	pcireg_t		subid;

	void			*ih;

	struct ac97_codec_if	*codec_if;
	struct ac97_host_if	host_if;

	struct esm_dma		*sc_dmas;

	int			pactive, ractive;
	struct esm_chinfo	pch;
	struct esm_chinfo	rch;

	void (*sc_pintr)(void *);
	void *sc_parg;

	void (*sc_rintr)(void *);
	void *sc_rarg;
};

int	esm_read_codec(void *, u_int8_t, u_int16_t *);
int	esm_write_codec(void *, u_int8_t, u_int16_t);
int	esm_attach_codec(void *, struct ac97_codec_if *);
void	esm_reset_codec(void *);

void	esm_power(struct esm_softc *, int);
void	esm_init(struct esm_softc *);
void	esm_initcodec(struct esm_softc *);

int	esm_init_output(void *, void *, int);
int	esm_trigger_output(void *, void *, void *, int, void (*)(void *),
	    void *, struct audio_params *);
int	esm_trigger_input(void *, void *, void *, int, void (*)(void *),
	    void *, struct audio_params *);
int	esm_halt_output(void *);
int	esm_halt_input(void *);
int	esm_open(void *, int);
void	esm_close(void *);
int	esm_getdev(void *, struct audio_device *);
int	esm_round_blocksize(void *, int);
int	esm_query_encoding(void *, struct audio_encoding *);
int	esm_set_params(void *, int, int, struct audio_params *,
	    struct audio_params *);
int	esm_set_port(void *, mixer_ctrl_t *);
int	esm_get_port(void *, mixer_ctrl_t *);
int	esm_query_devinfo(void *, mixer_devinfo_t *);
void	*esm_malloc(void *, int, size_t, int, int);
void	esm_free(void *, void *, int);
size_t	esm_round_buffersize(void *, int, size_t);
paddr_t	esm_mappage(void *, void *, off_t, int);
int	esm_get_props(void *);

int	esm_match(struct device *, struct cfdata *, void *);
void	esm_attach(struct device *, struct device *, void *);
int	esm_intr(void *);

int	esm_allocmem(struct esm_softc *, size_t, size_t,
	    struct esm_dma *);
int	esm_freemem(struct esm_softc *, struct esm_dma *);

int	esm_suspend(struct esm_softc *);
int	esm_resume(struct esm_softc *);
int	esm_shutdown(struct esm_softc *);
