/*	$NetBSD: auconv.c,v 1.16 2006/03/18 13:12:15 jmcneill Exp $	*/

/*
 * Copyright (c) 1996 The NetBSD Foundation, Inc.
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
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: auconv.c,v 1.16 2006/03/18 13:12:15 jmcneill Exp $");

#include <sys/types.h>
#include <sys/audioio.h>
#include <sys/errno.h>
#include <sys/malloc.h>
#include <sys/null.h>
#include <sys/systm.h>
#include <dev/audio_if.h>
#include <dev/auconv.h>
#include <dev/mulaw.h>
#include <machine/limits.h>
#ifndef _KERNEL
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#endif

#include <aurateconv.h>		/* generated by config(8) */
#include <mulaw.h>		/* generated by config(8) */

/* #define AUCONV_DEBUG */
#ifdef AUCONV_DEBUG
# define DPRINTF(x)	printf x
#else
# define DPRINTF(x)
#endif

#if NAURATECONV > 0
static int auconv_rateconv_supportable(u_int, u_int, u_int);
static int auconv_rateconv_check_channels(const struct audio_format *, int,
					  int, const audio_params_t *,
					  stream_filter_list_t *);
static int auconv_rateconv_check_rates(const struct audio_format *, int,
				       int, const audio_params_t *,
				       audio_params_t *,
				       stream_filter_list_t *);
#endif
#ifdef AUCONV_DEBUG
static void auconv_dump_formats(const struct audio_format *, int);
#endif
static void auconv_dump_params(const audio_params_t *);
static int auconv_exact_match(const struct audio_format *, int, int,
			      const struct audio_params *);
static u_int auconv_normalize_encoding(u_int, u_int);
static int auconv_is_supported_rate(const struct audio_format *, u_int);
static int auconv_add_encoding(int, int, int, struct audio_encoding_set **,
			       int *);

#ifdef _KERNEL
#define AUCONV_MALLOC(size)	malloc(size, M_DEVBUF, M_NOWAIT)
#define AUCONV_REALLOC(p, size)	realloc(p, size, M_DEVBUF, M_NOWAIT)
#define AUCONV_FREE(p)		free(p, M_DEVBUF)
#else
#define AUCONV_MALLOC(size)	malloc(size)
#define AUCONV_REALLOC(p, size)	realloc(p, size)
#define AUCONV_FREE(p)		free(p)
#define FALSE			0
#define TRUE			1
#endif

struct audio_encoding_set {
	int size;
	audio_encoding_t items[1];
};
#define ENCODING_SET_SIZE(n)	(offsetof(struct audio_encoding_set, items) \
				+ sizeof(audio_encoding_t) * (n))

struct conv_table {
	u_int encoding;
	u_int validbits;
	u_int precision;
	stream_filter_factory_t *play_conv;
	stream_filter_factory_t *rec_conv;
};
/*
 * SLINEAR-16 or SLINEAR-24 should precede in a table because
 * aurateconv supports only SLINEAR.
 */
static const struct conv_table s8_table[] = {
	{AUDIO_ENCODING_SLINEAR_LE, 16, 16,
	 linear8_to_linear16, linear16_to_linear8},
	{AUDIO_ENCODING_SLINEAR_BE, 16, 16,
	 linear8_to_linear16, linear16_to_linear8},
	{AUDIO_ENCODING_ULINEAR_LE, 8, 8,
	 change_sign8, change_sign8},
	{0, 0, 0, NULL, NULL}};
static const struct conv_table u8_table[] = {
	{AUDIO_ENCODING_SLINEAR_LE, 16, 16,
	 linear8_to_linear16, linear16_to_linear8},
	{AUDIO_ENCODING_SLINEAR_BE, 16, 16,
	 linear8_to_linear16, linear16_to_linear8},
	{AUDIO_ENCODING_SLINEAR_LE, 8, 8,
	 change_sign8, change_sign8},
	{AUDIO_ENCODING_ULINEAR_LE, 16, 16,
	 linear8_to_linear16, linear16_to_linear8},
	{AUDIO_ENCODING_ULINEAR_BE, 16, 16,
	 linear8_to_linear16, linear16_to_linear8},
	{0, 0, 0, NULL, NULL}};
static const struct conv_table s16le_table[] = {
	{AUDIO_ENCODING_SLINEAR_BE, 16, 16,
	 swap_bytes, swap_bytes},
	{AUDIO_ENCODING_ULINEAR_LE, 16, 16,
	 change_sign16, change_sign16},
	{AUDIO_ENCODING_ULINEAR_BE, 16, 16,
	 swap_bytes_change_sign16, swap_bytes_change_sign16},
	{0, 0, 0, NULL, NULL}};
static const struct conv_table s16be_table[] = {
	{AUDIO_ENCODING_SLINEAR_LE, 16, 16,
	 swap_bytes, swap_bytes},
	{AUDIO_ENCODING_ULINEAR_BE, 16, 16,
	 change_sign16, change_sign16},
	{AUDIO_ENCODING_ULINEAR_LE, 16, 16,
	 swap_bytes_change_sign16, swap_bytes_change_sign16},
	{0, 0, 0, NULL, NULL}};
static const struct conv_table u16le_table[] = {
	{AUDIO_ENCODING_SLINEAR_LE, 16, 16,
	 change_sign16, change_sign16},
	{AUDIO_ENCODING_ULINEAR_BE, 16, 16,
	 swap_bytes, swap_bytes},
	{AUDIO_ENCODING_SLINEAR_BE, 16, 16,
	 swap_bytes_change_sign16, swap_bytes_change_sign16},
	{0, 0, 0, NULL, NULL}};
static const struct conv_table u16be_table[] = {
	{AUDIO_ENCODING_SLINEAR_BE, 16, 16,
	 change_sign16, change_sign16},
	{AUDIO_ENCODING_ULINEAR_LE, 16, 16,
	 swap_bytes, swap_bytes},
	{AUDIO_ENCODING_SLINEAR_LE, 16, 16,
	 swap_bytes_change_sign16, swap_bytes_change_sign16},
	{0, 0, 0, NULL, NULL}};
#if NMULAW > 0
static const struct conv_table mulaw_table[] = {
	{AUDIO_ENCODING_SLINEAR_LE, 16, 16,
	 mulaw_to_linear16, linear16_to_mulaw},
	{AUDIO_ENCODING_SLINEAR_BE, 16, 16,
	 mulaw_to_linear16, linear16_to_mulaw},
	{AUDIO_ENCODING_ULINEAR_LE, 16, 16,
	 mulaw_to_linear16, linear16_to_mulaw},
	{AUDIO_ENCODING_ULINEAR_BE, 16, 16,
	 mulaw_to_linear16, linear16_to_mulaw},
	{AUDIO_ENCODING_SLINEAR_LE, 8, 8,
	 mulaw_to_linear8, linear8_to_mulaw},
	{AUDIO_ENCODING_ULINEAR_LE, 8, 8,
	 mulaw_to_linear8, linear8_to_mulaw},
	{0, 0, 0, NULL, NULL}};
static const struct conv_table alaw_table[] = {
	{AUDIO_ENCODING_SLINEAR_LE, 16, 16,
	 alaw_to_linear16, linear16_to_alaw},
	{AUDIO_ENCODING_SLINEAR_BE, 16, 16,
	 alaw_to_linear16, linear16_to_alaw},
	{AUDIO_ENCODING_ULINEAR_LE, 16, 16,
	 alaw_to_linear16, linear16_to_alaw},
	{AUDIO_ENCODING_ULINEAR_BE, 16, 16,
	 alaw_to_linear16, linear16_to_alaw},
	{AUDIO_ENCODING_SLINEAR_LE, 8, 8,
	 alaw_to_linear8, linear8_to_alaw},
	{AUDIO_ENCODING_ULINEAR_LE, 8, 8,
	 alaw_to_linear8, linear8_to_alaw},
	{0, 0, 0, NULL, NULL}};
#endif
#ifdef AUCONV_DEBUG
static const char *encoding_dbg_names[] = {
	"none", AudioEmulaw, AudioEalaw, "pcm16",
	"pcm8", AudioEadpcm, AudioEslinear_le, AudioEslinear_be,
	AudioEulinear_le, AudioEulinear_be,
	AudioEslinear, AudioEulinear,
	AudioEmpeg_l1_stream, AudioEmpeg_l1_packets,
	AudioEmpeg_l1_system, AudioEmpeg_l2_stream,
	AudioEmpeg_l2_packets, AudioEmpeg_l2_system
};
#endif

void
stream_filter_set_fetcher(stream_filter_t *this, stream_fetcher_t *p)
{
	this->prev = p;
}

void
stream_filter_set_inputbuffer(stream_filter_t *this, audio_stream_t *stream)
{
	this->src = stream;
}

stream_filter_t *
auconv_nocontext_filter_factory(
	int (*fetch_to)(stream_fetcher_t *, audio_stream_t *, int))
{
	stream_filter_t *this;

	this = AUCONV_MALLOC(sizeof(stream_filter_t));
	if (this == NULL)
		return NULL;
	this->base.fetch_to = fetch_to;
	this->dtor = auconv_nocontext_filter_dtor;
	this->set_fetcher = stream_filter_set_fetcher;
	this->set_inputbuffer = stream_filter_set_inputbuffer;
	this->prev = NULL;
	this->src = NULL;
	return this;
}

void
auconv_nocontext_filter_dtor(struct stream_filter *this)
{
	if (this != NULL)
		AUCONV_FREE(this);
}

#define DEFINE_FILTER(name)	\
static int \
name##_fetch_to(stream_fetcher_t *, audio_stream_t *, int); \
stream_filter_t * \
name(struct audio_softc *sc, const audio_params_t *from, \
     const audio_params_t *to) \
{ \
	return auconv_nocontext_filter_factory(name##_fetch_to); \
} \
static int \
name##_fetch_to(stream_fetcher_t *self, audio_stream_t *dst, int max_used)

DEFINE_FILTER(change_sign8)
{
	stream_filter_t *this;
	int m, err;

	this = (stream_filter_t *)self;
	if ((err = this->prev->fetch_to(this->prev, this->src, max_used)))
		return err;
	m = dst->end - dst->start;
	m = min(m, max_used);
	FILTER_LOOP_PROLOGUE(this->src, 1, dst, 1, m) {
		*d = *s ^ 0x80;
	} FILTER_LOOP_EPILOGUE(this->src, dst);
	return 0;
}

DEFINE_FILTER(change_sign16)
{
	stream_filter_t *this;
	int m, err, enc;

	this = (stream_filter_t *)self;
	max_used = (max_used + 1) & ~1; /* round up to even */
	if ((err = this->prev->fetch_to(this->prev, this->src, max_used)))
		return err;
	m = (dst->end - dst->start) & ~1;
	m = min(m, max_used);
	enc = dst->param.encoding;
	if (enc == AUDIO_ENCODING_SLINEAR_LE
	    || enc == AUDIO_ENCODING_ULINEAR_LE) {
		FILTER_LOOP_PROLOGUE(this->src, 2, dst, 2, m) {
			d[0] = s[0];
			d[1] = s[1] ^ 0x80;
		} FILTER_LOOP_EPILOGUE(this->src, dst);
	} else {
		FILTER_LOOP_PROLOGUE(this->src, 2, dst, 2, m) {
			d[0] = s[0] ^ 0x80;
			d[1] = s[1];
		} FILTER_LOOP_EPILOGUE(this->src, dst);
	}
	return 0;
}

DEFINE_FILTER(swap_bytes)
{
	stream_filter_t *this;
	int m, err;

	this = (stream_filter_t *)self;
	max_used = (max_used + 1) & ~1; /* round up to even */
	if ((err = this->prev->fetch_to(this->prev, this->src, max_used)))
		return err;
	m = (dst->end - dst->start) & ~1;
	m = min(m, max_used);
	FILTER_LOOP_PROLOGUE(this->src, 2, dst, 2, m) {
		d[0] = s[1];
		d[1] = s[0];
	} FILTER_LOOP_EPILOGUE(this->src, dst);
	return 0;
}

DEFINE_FILTER(swap_bytes_change_sign16)
{
	stream_filter_t *this;
	int m, err, enc;

	this = (stream_filter_t *)self;
	max_used = (max_used + 1) & ~1; /* round up to even */
	if ((err = this->prev->fetch_to(this->prev, this->src, max_used)))
		return err;
	m = (dst->end - dst->start) & ~1;
	m = min(m, max_used);
	enc = dst->param.encoding;
	if (enc == AUDIO_ENCODING_SLINEAR_LE
	    || enc == AUDIO_ENCODING_ULINEAR_LE) {
		FILTER_LOOP_PROLOGUE(this->src, 2, dst, 2, m) {
			d[0] = s[1];
			d[1] = s[0] ^ 0x80;
		} FILTER_LOOP_EPILOGUE(this->src, dst);
	} else {
		FILTER_LOOP_PROLOGUE(this->src, 2, dst, 2, m) {
			d[0] = s[1] ^ 0x80;
			d[1] = s[0];
		} FILTER_LOOP_EPILOGUE(this->src, dst);
	}
	return 0;
}

DEFINE_FILTER(linear8_to_linear16)
{
	stream_filter_t *this;
	int m, err, enc_dst, enc_src;

	this = (stream_filter_t *)self;
	max_used = (max_used + 1) & ~1; /* round up to even */
	if ((err = this->prev->fetch_to(this->prev, this->src, max_used / 2)))
		return err;
	m = (dst->end - dst->start) & ~1;
	m = min(m, max_used);
	enc_dst = dst->param.encoding;
	enc_src = this->src->param.encoding;
	if ((enc_src == AUDIO_ENCODING_SLINEAR_LE
	     && enc_dst == AUDIO_ENCODING_SLINEAR_LE)
	    || (enc_src == AUDIO_ENCODING_ULINEAR_LE
		&& enc_dst == AUDIO_ENCODING_ULINEAR_LE)) {
		/*
		 * slinear8 -> slinear16_le
		 * ulinear8 -> ulinear16_le
		 */
		FILTER_LOOP_PROLOGUE(this->src, 1, dst, 2, m) {
			d[0] = 0;
			d[1] = s[0];
		} FILTER_LOOP_EPILOGUE(this->src, dst);
	} else if ((enc_src == AUDIO_ENCODING_SLINEAR_LE
		    && enc_dst == AUDIO_ENCODING_SLINEAR_BE)
		   || (enc_src == AUDIO_ENCODING_ULINEAR_LE
		       && enc_dst == AUDIO_ENCODING_ULINEAR_BE)) {
		/*
		 * slinear8 -> slinear16_be
		 * ulinear8 -> ulinear16_be
		 */
		FILTER_LOOP_PROLOGUE(this->src, 1, dst, 2, m) {
			d[0] = s[0];
			d[1] = 0;
		} FILTER_LOOP_EPILOGUE(this->src, dst);
	} else if ((enc_src == AUDIO_ENCODING_SLINEAR_LE
		    && enc_dst == AUDIO_ENCODING_ULINEAR_LE)
		   || (enc_src == AUDIO_ENCODING_ULINEAR_LE
		       && enc_dst == AUDIO_ENCODING_SLINEAR_LE)) {
		/*
		 * slinear8 -> ulinear16_le
		 * ulinear8 -> slinear16_le
		 */
		FILTER_LOOP_PROLOGUE(this->src, 1, dst, 2, m) {
			d[0] = 0;
			d[1] = s[0] ^ 0x80;
		} FILTER_LOOP_EPILOGUE(this->src, dst);
	} else {
		/*
		 * slinear8 -> ulinear16_be
		 * ulinear8 -> slinear16_be
		 */
		FILTER_LOOP_PROLOGUE(this->src, 1, dst, 2, m) {
			d[0] = s[0] ^ 0x80;
			d[1] = 0;
		} FILTER_LOOP_EPILOGUE(this->src, dst);
	}
	return 0;
}

DEFINE_FILTER(linear16_to_linear8)
{
	stream_filter_t *this;
	int m, err, enc_src, enc_dst;

	this = (stream_filter_t *)self;
	if ((err = this->prev->fetch_to(this->prev, this->src, max_used * 2)))
		return err;
	m = dst->end - dst->start;
	m = min(m, max_used);
	enc_dst = dst->param.encoding;
	enc_src = this->src->param.encoding;
	if ((enc_src == AUDIO_ENCODING_SLINEAR_LE
	     && enc_dst == AUDIO_ENCODING_SLINEAR_LE)
	    || (enc_src == AUDIO_ENCODING_ULINEAR_LE
		&& enc_dst == AUDIO_ENCODING_ULINEAR_LE)) {
		/*
		 * slinear16_le -> slinear8
		 * ulinear16_le -> ulinear8
		 */
		FILTER_LOOP_PROLOGUE(this->src, 2, dst, 1, m) {
			d[0] = s[1];
		} FILTER_LOOP_EPILOGUE(this->src, dst);
	} else if ((enc_src == AUDIO_ENCODING_SLINEAR_LE
		    && enc_dst == AUDIO_ENCODING_ULINEAR_LE)
		   || (enc_src == AUDIO_ENCODING_ULINEAR_LE
		       && enc_dst == AUDIO_ENCODING_SLINEAR_LE)) {
		/*
		 * slinear16_le -> ulinear8
		 * ulinear16_le -> slinear8
		 */
		FILTER_LOOP_PROLOGUE(this->src, 2, dst, 1, m) {
			d[0] = s[1] ^ 0x80;
		} FILTER_LOOP_EPILOGUE(this->src, dst);
	} else if ((enc_src == AUDIO_ENCODING_SLINEAR_BE
		    && enc_dst == AUDIO_ENCODING_SLINEAR_LE)
		   || (enc_src == AUDIO_ENCODING_ULINEAR_BE
		       && enc_dst == AUDIO_ENCODING_ULINEAR_LE)) {
		/*
		 * slinear16_be -> slinear8
		 * ulinear16_be -> ulinear8
		 */
		FILTER_LOOP_PROLOGUE(this->src, 2, dst, 1, m) {
			d[0] = s[0];
		} FILTER_LOOP_EPILOGUE(this->src, dst);
	} else {
		/*
		 * slinear16_be -> ulinear8
		 * ulinear16_be -> slinear8
		 */
		FILTER_LOOP_PROLOGUE(this->src, 2, dst, 1, m) {
			d[0] = s[0] ^ 0x80;
		} FILTER_LOOP_EPILOGUE(this->src, dst);
	}
	return 0;
}

DEFINE_FILTER(linear16_to_linear32)
{
	stream_filter_t *this;
	int m, err, enc_dst, enc_src;

	this = (stream_filter_t *)self;
	max_used = (max_used + 1) & ~1;
	if ((err = this->prev->fetch_to(this->prev, this->src, max_used / 2)))
		return err;
	m = (dst->end - dst->start) & ~1;
	m = min(m, max_used);
	enc_dst = dst->param.encoding;
	enc_src = this->src->param.encoding;
	if (enc_src == AUDIO_ENCODING_SLINEAR_LE &&
	    enc_dst == AUDIO_ENCODING_SLINEAR_LE) {
		FILTER_LOOP_PROLOGUE(this->src, 2, dst, 4, m) {
			d[0] = s[0];
			d[1] = 0;
			d[2] = s[1];
			d[3] = 0;
		} FILTER_LOOP_EPILOGUE(this->src, dst);
	}

	return 0;
}

DEFINE_FILTER(interleave32_2ch_to_8ch)
{
	stream_filter_t *this;
	int m, err;

	this = (stream_filter_t *)self;
	max_used = (max_used + 1) & ~1;
	if ((err = this->prev->fetch_to(this->prev, this->src, max_used / 2)))
		return err;
	m = (dst->end - dst->start) & ~1;
	m = min(m, max_used);

	FILTER_LOOP_PROLOGUE(this->src, 4, dst, 16, m) {
		memcpy(d, s, 4);
		memset(d, 0, 12);
	} FILTER_LOOP_EPILOGUE(this->src, dst);

	return 0;
}

/**
 * Set appropriate parameters in `param,' and return the index in
 * the hardware capability array `formats.'
 *
 * @param formats	[IN] An array of formats which a hardware can support.
 * @param nformats	[IN] The number of elements of the array.
 * @param mode		[IN] Either AUMODE_PLAY or AUMODE_RECORD.
 * @param param		[IN] Requested format.  param->sw_code may be set.
 * @param rateconv	[IN] TRUE if aurateconv may be used.
 * @param list		[OUT] stream_filters required for param.
 * @return The index of selected audio_format entry.  -1 if the device
 *	can not support the specified param.
 */
int
auconv_set_converter(const struct audio_format *formats, int nformats,
		     int mode, const audio_params_t *param, int rateconv,
		     stream_filter_list_t *list)
{
	audio_params_t work;
	const struct conv_table *table;
	stream_filter_factory_t *conv;
	int enc;
	int i, j;

#ifdef AUCONV_DEBUG
	DPRINTF(("%s: ENTER rateconv=%d\n", __func__, rateconv));
	auconv_dump_formats(formats, nformats);
#endif
	enc = auconv_normalize_encoding(param->encoding, param->precision);

	/* check support by native format */
	i = auconv_exact_match(formats, nformats, mode, param);
	if (i >= 0) {
		DPRINTF(("%s: LEAVE with %d (exact)\n", __func__, i));
		return i;
	}

#if NAURATECONV > 0
	/* native format with aurateconv */
	DPRINTF(("%s: native with aurateconv\n", __func__));
	if (rateconv
	    && auconv_rateconv_supportable(enc, param->precision,
					   param->validbits)) {
		i = auconv_rateconv_check_channels(formats, nformats,
						   mode, param, list);
		if (i >= 0) {
			DPRINTF(("%s: LEAVE with %d (aurateconv1)\n", __func__, i));
			return i;
		}
	}
#endif

	/* check for emulation */
	DPRINTF(("%s: encoding emulation\n", __func__));
	table = NULL;
	switch (enc) {
	case AUDIO_ENCODING_SLINEAR_LE:
		if (param->precision == 8)
			table = s8_table;
		else if (param->precision == 16)
			table = s16le_table;
		break;
	case AUDIO_ENCODING_SLINEAR_BE:
		if (param->precision == 8)
			table = s8_table;
		else if (param->precision == 16)
			table = s16be_table;
		break;
	case AUDIO_ENCODING_ULINEAR_LE:
		if (param->precision == 8)
			table = u8_table;
		else if (param->precision == 16)
			table = u16le_table;
		break;
	case AUDIO_ENCODING_ULINEAR_BE:
		if (param->precision == 8)
			table = u8_table;
		else if (param->precision == 16)
			table = u16be_table;
		break;
#if NMULAW > 0
	case AUDIO_ENCODING_ULAW:
		table = mulaw_table;
		break;
	case AUDIO_ENCODING_ALAW:
		table = alaw_table;
		break;
#endif
	}
	if (table == NULL) {
		DPRINTF(("%s: LEAVE with -1 (no-emultable)\n", __func__));
		return -1;
	}
	work = *param;
	for (j = 0; table[j].precision != 0; j++) {
		work.encoding = table[j].encoding;
		work.precision = table[j].precision;
		work.validbits = table[j].validbits;
		i = auconv_exact_match(formats, nformats, mode, &work);
		if (i >= 0) {
			conv = mode == AUMODE_PLAY
				? table[j].play_conv : table[j].rec_conv;
			list->append(list, conv, &work);
			DPRINTF(("%s: LEAVE with %d (emultable)\n", __func__, i));
			return i;
		}
	}
	/* not found */

#if NAURATECONV > 0
	/* emulation with aurateconv */
	DPRINTF(("%s: encoding emulation with aurateconv\n", __func__));
	if (!rateconv) {
		DPRINTF(("%s: LEAVE with -1 (no-rateconv)\n", __func__));
		return -1;
	}
	work = *param;
	for (j = 0; table[j].precision != 0; j++) {
		if (!auconv_rateconv_supportable(table[j].encoding,
						 table[j].precision,
						 table[j].validbits))
			continue;
		work.encoding = table[j].encoding;
		work.precision = table[j].precision;
		work.validbits = table[j].validbits;
		i = auconv_rateconv_check_channels(formats, nformats,
						   mode, &work, list);
		if (i >= 0) {
			/* work<=>hw conversion is already registered */
			conv = mode == AUMODE_PLAY
				? table[j].play_conv : table[j].rec_conv;
			/* register userland<=>work conversion */
			list->append(list, conv, &work);
			DPRINTF(("%s: LEAVE with %d (rateconv2)\n", __func__, i));
			return i;
		}
	}

#endif
	DPRINTF(("%s: LEAVE with -1 (bottom)\n", __func__));
	return -1;
}

#if NAURATECONV > 0
static int
auconv_rateconv_supportable(u_int encoding, u_int precision, u_int validbits)
{
	if (encoding != AUDIO_ENCODING_SLINEAR_LE
	    && encoding != AUDIO_ENCODING_SLINEAR_BE)
		return FALSE;
	if (precision != 16 && precision != 24 && precision != 32)
		return FALSE;
	if (precision < validbits)
		return FALSE;
	return TRUE;
}

static int
auconv_rateconv_check_channels(const struct audio_format *formats, int nformats,
			       int mode, const audio_params_t *param,
			       stream_filter_list_t *list)
{
	audio_params_t hw_param;
	int ind, n;

	hw_param = *param;
	/* check for the specified number of channels */
	ind = auconv_rateconv_check_rates(formats, nformats, mode, param,
					  &hw_param, list);
	if (ind >= 0)
		return ind;

	/* check for larger numbers */
	for (n = param->channels + 1; n <= AUDIO_MAX_CHANNELS; n++) {
		hw_param.channels = n;
		ind = auconv_rateconv_check_rates(formats, nformats, mode,
						  param, &hw_param, list);
		if (ind >= 0)
			return ind;
	}

	/* check for stereo:monaural conversion */
	if (param->channels == 2) {
		hw_param.channels = 1;
		ind = auconv_rateconv_check_rates(formats, nformats, mode,
						  param, &hw_param, list);
		if (ind >= 0)
			return ind;
	}
	return -1;
}

static int
auconv_rateconv_check_rates(const struct audio_format *formats, int nformats,
			    int mode, const audio_params_t *param,
			    audio_params_t *hw_param, stream_filter_list_t *list)
{
	int ind, i, j, enc, f_enc;
	u_int rate, minrate, maxrate, orig_rate;;

	/* exact match */
	ind = auconv_exact_match(formats, nformats, mode, hw_param);
	if (ind >= 0)
		goto found;

	/* determine min/max of specified encoding/precision/channels */
	minrate = UINT_MAX;
	maxrate = 0;
	enc = auconv_normalize_encoding(param->encoding,
					param->precision);
	for (i = 0; i < nformats; i++) {
		if (!AUFMT_IS_VALID(&formats[i]))
			continue;
		if ((formats[i].mode & mode) == 0)
			continue;
		f_enc = auconv_normalize_encoding(formats[i].encoding,
						  formats[i].precision);
		if (f_enc != enc)
			continue;
		if (formats[i].validbits != hw_param->validbits)
			continue;
		if (formats[i].precision != hw_param->precision)
			continue;
		if (formats[i].channels != hw_param->channels)
			continue;
		if (formats[i].frequency_type == 0) {
			if (formats[i].frequency[0] < minrate)
				minrate = formats[i].frequency[0];
			if (formats[i].frequency[1] > maxrate)
				maxrate = formats[i].frequency[1];
		} else {
			for (j = 0; j < formats[i].frequency_type; j++) {
				if (formats[i].frequency[j] < minrate)
					minrate = formats[i].frequency[j];
				if (formats[i].frequency[j] > maxrate)
					maxrate = formats[i].frequency[j];
			}
		}
	}
	if (maxrate == 0)
		return -1;

	/* try multiples of sample_rate */
	orig_rate = hw_param->sample_rate;
	for (i = 2; (rate = param->sample_rate * i) <= maxrate; i++) {
		hw_param->sample_rate = rate;
		ind = auconv_exact_match(formats, nformats, mode, hw_param);
		if (ind >= 0)
			goto found;
	}

	hw_param->sample_rate = param->sample_rate >= minrate
		? maxrate : minrate;
	ind = auconv_exact_match(formats, nformats, mode, hw_param);
	if (ind >= 0)
		goto found;
	hw_param->sample_rate = orig_rate;
	return -1;

 found:
	list->append(list, aurateconv, hw_param);
	return ind;
}
#endif /* NAURATECONV */

#ifdef AUCONV_DEBUG
static void
auconv_dump_formats(const struct audio_format *formats, int nformats)
{
	const struct audio_format *f;
	int i, j;

	for (i = 0; i < nformats; i++) {
		f = &formats[i];
		printf("[%2d]: mode=", i);
		if (!AUFMT_IS_VALID(f)) {
			printf("INVALID");
		} else if (f->mode == AUMODE_PLAY) {
			printf("PLAY");
		} else if (f->mode == AUMODE_RECORD) {
			printf("RECORD");
		} else if (f->mode == (AUMODE_PLAY | AUMODE_RECORD)) {
			printf("PLAY|RECORD");
		} else {
			printf("0x%x", f->mode);
		}
		printf(" enc=%s", encoding_dbg_names[f->encoding]);
		printf(" %u/%ubit", f->validbits, f->precision);
		printf(" %uch", f->channels);

		printf(" channel_mask=");
		if (f->channel_mask == AUFMT_MONAURAL) {
			printf("MONAURAL");
		} else if (f->channel_mask == AUFMT_STEREO) {
			printf("STEREO");
		} else if (f->channel_mask == AUFMT_SURROUND4) {
			printf("SURROUND4");
		} else if (f->channel_mask == AUFMT_DOLBY_5_1) {
			printf("DOLBY5.1");
		} else {
			printf("0x%x", f->channel_mask);
		}

		if (f->frequency_type == 0) {
			printf(" %uHz-%uHz", f->frequency[0],
			       f->frequency[1]);
		} else {
			printf(" %uHz", f->frequency[0]);
			for (j = 1; j < f->frequency_type; j++)
				printf(",%uHz", f->frequency[j]);
		}
		printf("\n");
	}
}

static void
auconv_dump_params(const audio_params_t *p)
{
	printf("enc=%s", encoding_dbg_names[p->encoding]);
	printf(" %u/%ubit", p->validbits, p->precision);
	printf(" %uch", p->channels);
	printf(" %uHz", p->sample_rate);
	printf("\n");
}
#else
static void
auconv_dump_params(const audio_params_t *p)
{
}
#endif /* AUCONV_DEBUG */

/**
 * a sub-routine for auconv_set_converter()
 */
static int
auconv_exact_match(const struct audio_format *formats, int nformats,
		   int mode, const audio_params_t *param)
{
	int i, enc, f_enc;

	DPRINTF(("%s: ENTER: mode=0x%x target:", __func__, mode));
	auconv_dump_params(param);
	enc = auconv_normalize_encoding(param->encoding,
					param->precision);
	DPRINTF(("%s: target normalized: %s\n", __func__,
	    encoding_dbg_names[enc]));
	for (i = 0; i < nformats; i++) {
		if (!AUFMT_IS_VALID(&formats[i]))
			continue;
		if ((formats[i].mode & mode) == 0)
			continue;
		f_enc = auconv_normalize_encoding(formats[i].encoding,
						  formats[i].precision);
		DPRINTF(("%s: format[%d] normalized: %s\n",
			 __func__, i, encoding_dbg_names[f_enc]));
		if (f_enc != enc)
			continue;
		/**
		 * XXX	we need encoding-dependent check.
		 * XXX	Is to check precision/channels meaningful for
		 *	MPEG encodings?
		 */
		if (formats[i].validbits != param->validbits)
			continue;
		if (formats[i].precision != param->precision)
			continue;
		if (formats[i].channels != param->channels)
			continue;
		if (!auconv_is_supported_rate(&formats[i],
					      param->sample_rate))
			continue;
		return i;
	}
	return -1;
}

/**
 * a sub-routine for auconv_set_converter()
 *   SLINEAR ==> SLINEAR_<host-endian>
 *   ULINEAR ==> ULINEAR_<host-endian>
 *   SLINEAR_BE 8bit ==> SLINEAR_LE 8bit
 *   ULINEAR_BE 8bit ==> ULINEAR_LE 8bit
 * This should be the same rule as audio_check_params()
 */
static u_int
auconv_normalize_encoding(u_int encoding, u_int precision)
{
	int enc;

	enc = encoding;
	if (enc == AUDIO_ENCODING_SLINEAR_LE)
		return enc;
	if (enc == AUDIO_ENCODING_ULINEAR_LE)
		return enc;
#if BYTE_ORDER == LITTLE_ENDIAN
	if (enc == AUDIO_ENCODING_SLINEAR)
		return AUDIO_ENCODING_SLINEAR_LE;
	else if (enc == AUDIO_ENCODING_ULINEAR)
		return AUDIO_ENCODING_ULINEAR_LE;
#else
	if (enc == AUDIO_ENCODING_SLINEAR)
		enc = AUDIO_ENCODING_SLINEAR_BE;
	else if (enc == AUDIO_ENCODING_ULINEAR)
		enc = AUDIO_ENCODING_ULINEAR_BE;
#endif
	if (precision == 8 && enc == AUDIO_ENCODING_SLINEAR_BE)
		return AUDIO_ENCODING_SLINEAR_LE;
	if (precision == 8 && enc == AUDIO_ENCODING_ULINEAR_BE)
		return AUDIO_ENCODING_ULINEAR_LE;
	return enc;
}

/**
 * a sub-routine for auconv_set_converter()
 */
static int
auconv_is_supported_rate(const struct audio_format *format, u_int rate)
{
	u_int i;

	if (format->frequency_type == 0) {
		return format->frequency[0] <= rate
			&& rate <= format->frequency[1];
	}
	for (i = 0; i < format->frequency_type; i++) {
		if (format->frequency[i] == rate)
			return TRUE;
	}
	return FALSE;
}

/**
 * Create an audio_encoding_set besed on hardware capability represented
 * by audio_format.
 *
 * Usage:
 *	foo_attach(...) {
 *		:
 *		if (auconv_create_encodings(formats, nformats,
 *			&sc->sc_encodings) != 0) {
 *			// attach failure
 *		}
 *
 * @param formats	[IN] An array of formats which a hardware can support.
 * @param nformats	[IN] The number of elements of the array.
 * @param encodings	[OUT] receives an address of an audio_encoding_set.
 * @return errno; 0 for success.
 */
int
auconv_create_encodings(const struct audio_format *formats, int nformats,
			struct audio_encoding_set **encodings)
{
	struct audio_encoding_set *buf;
	int capacity;
	int i;
	int err;

#define	ADD_ENCODING(enc, prec, flags)	do { \
	err = auconv_add_encoding(enc, prec, flags, &buf, &capacity); \
	if (err != 0) goto err_exit; \
} while (/*CONSTCOND*/0)

	capacity = 10;
	buf = AUCONV_MALLOC(ENCODING_SET_SIZE(capacity));
	if (buf == NULL) {
		err = ENOMEM;
		goto err_exit;
	}
	buf->size = 0;
	for (i = 0; i < nformats; i++) {
		if (!AUFMT_IS_VALID(&formats[i]))
			continue;
		switch (formats[i].encoding) {
		case AUDIO_ENCODING_SLINEAR_LE:
			ADD_ENCODING(formats[i].encoding,
				     formats[i].precision, 0);
			ADD_ENCODING(AUDIO_ENCODING_SLINEAR_BE,
				     formats[i].precision,
				     AUDIO_ENCODINGFLAG_EMULATED);
			ADD_ENCODING(AUDIO_ENCODING_ULINEAR_LE,
				     formats[i].precision,
				     AUDIO_ENCODINGFLAG_EMULATED);
			ADD_ENCODING(AUDIO_ENCODING_ULINEAR_BE,
				     formats[i].precision,
				     AUDIO_ENCODINGFLAG_EMULATED);
#if NMULAW > 0
			if (formats[i].precision == 8
			    || formats[i].precision == 16) {
				ADD_ENCODING(AUDIO_ENCODING_ULAW, 8,
					     AUDIO_ENCODINGFLAG_EMULATED);
				ADD_ENCODING(AUDIO_ENCODING_ALAW, 8,
					     AUDIO_ENCODINGFLAG_EMULATED);
			}
#endif
			break;
		case AUDIO_ENCODING_SLINEAR_BE:
			ADD_ENCODING(formats[i].encoding,
				     formats[i].precision, 0);
			ADD_ENCODING(AUDIO_ENCODING_SLINEAR_LE,
				     formats[i].precision,
				     AUDIO_ENCODINGFLAG_EMULATED);
			ADD_ENCODING(AUDIO_ENCODING_ULINEAR_LE,
				     formats[i].precision,
				     AUDIO_ENCODINGFLAG_EMULATED);
			ADD_ENCODING(AUDIO_ENCODING_ULINEAR_BE,
				     formats[i].precision,
				     AUDIO_ENCODINGFLAG_EMULATED);
#if NMULAW > 0
			if (formats[i].precision == 8
			    || formats[i].precision == 16) {
				ADD_ENCODING(AUDIO_ENCODING_ULAW, 8,
					     AUDIO_ENCODINGFLAG_EMULATED);
				ADD_ENCODING(AUDIO_ENCODING_ALAW, 8,
					     AUDIO_ENCODINGFLAG_EMULATED);
			}
#endif
			break;
		case AUDIO_ENCODING_ULINEAR_LE:
			ADD_ENCODING(formats[i].encoding,
				     formats[i].precision, 0);
			ADD_ENCODING(AUDIO_ENCODING_SLINEAR_BE,
				     formats[i].precision,
				     AUDIO_ENCODINGFLAG_EMULATED);
			ADD_ENCODING(AUDIO_ENCODING_SLINEAR_LE,
				     formats[i].precision,
				     AUDIO_ENCODINGFLAG_EMULATED);
			ADD_ENCODING(AUDIO_ENCODING_ULINEAR_BE,
				     formats[i].precision,
				     AUDIO_ENCODINGFLAG_EMULATED);
#if NMULAW > 0
			if (formats[i].precision == 8
			    || formats[i].precision == 16) {
				ADD_ENCODING(AUDIO_ENCODING_ULAW, 8,
					     AUDIO_ENCODINGFLAG_EMULATED);
				ADD_ENCODING(AUDIO_ENCODING_ALAW, 8,
					     AUDIO_ENCODINGFLAG_EMULATED);
			}
#endif
			break;
		case AUDIO_ENCODING_ULINEAR_BE:
			ADD_ENCODING(formats[i].encoding,
				     formats[i].precision, 0);
			ADD_ENCODING(AUDIO_ENCODING_SLINEAR_BE,
				     formats[i].precision,
				     AUDIO_ENCODINGFLAG_EMULATED);
			ADD_ENCODING(AUDIO_ENCODING_ULINEAR_LE,
				     formats[i].precision,
				     AUDIO_ENCODINGFLAG_EMULATED);
			ADD_ENCODING(AUDIO_ENCODING_SLINEAR_LE,
				     formats[i].precision,
				     AUDIO_ENCODINGFLAG_EMULATED);
#if NMULAW > 0
			if (formats[i].precision == 8
			    || formats[i].precision == 16) {
				ADD_ENCODING(AUDIO_ENCODING_ULAW, 8,
					     AUDIO_ENCODINGFLAG_EMULATED);
				ADD_ENCODING(AUDIO_ENCODING_ALAW, 8,
					     AUDIO_ENCODINGFLAG_EMULATED);
			}
#endif
			break;

		case AUDIO_ENCODING_ULAW:
		case AUDIO_ENCODING_ALAW:
		case AUDIO_ENCODING_ADPCM:
		case AUDIO_ENCODING_MPEG_L1_STREAM:
		case AUDIO_ENCODING_MPEG_L1_PACKETS:
		case AUDIO_ENCODING_MPEG_L1_SYSTEM:
		case AUDIO_ENCODING_MPEG_L2_STREAM:
		case AUDIO_ENCODING_MPEG_L2_PACKETS:
		case AUDIO_ENCODING_MPEG_L2_SYSTEM:
			ADD_ENCODING(formats[i].encoding,
				     formats[i].precision, 0);
			break;

		case AUDIO_ENCODING_SLINEAR:
		case AUDIO_ENCODING_ULINEAR:
		case AUDIO_ENCODING_LINEAR:
		case AUDIO_ENCODING_LINEAR8:
		default:
			printf("%s: invalid encoding value "
			       "for audio_format: %d\n",
			       __func__, formats[i].encoding);
			break;
		}
	}
	*encodings = buf;
	return 0;

 err_exit:
	if (buf != NULL)
		AUCONV_FREE(buf);
	*encodings = NULL;
	return err;
}

/**
 * a sub-routine for auconv_create_encodings()
 */
static int
auconv_add_encoding(int enc, int prec, int flags,
		    struct audio_encoding_set **buf, int *capacity)
{
#ifndef AUCONV_DEBUG 
	static const char *encoding_names[] = {
		NULL, AudioEmulaw, AudioEalaw, NULL,
		NULL, AudioEadpcm, AudioEslinear_le, AudioEslinear_be,
		AudioEulinear_le, AudioEulinear_be,
		AudioEslinear, AudioEulinear,
		AudioEmpeg_l1_stream, AudioEmpeg_l1_packets,
		AudioEmpeg_l1_system, AudioEmpeg_l2_stream,
		AudioEmpeg_l2_packets, AudioEmpeg_l2_system
	};
#endif
	struct audio_encoding_set *set;
	struct audio_encoding_set *new_buf;
	audio_encoding_t *e;
	int i;

	set = *buf;
	/* already has the same encoding? */
	e = set->items;
	for (i = 0; i < set->size; i++, e++) {
		if (e->encoding == enc && e->precision == prec) {
			/* overwrite EMULATED flag */
			if ((e->flags & AUDIO_ENCODINGFLAG_EMULATED)
			    && (flags & AUDIO_ENCODINGFLAG_EMULATED) == 0) {
				e->flags &= ~AUDIO_ENCODINGFLAG_EMULATED;
			}
			return 0;
		}
	}
	/* We don't have the specified one. */

	if (set->size >= *capacity) {
		new_buf = AUCONV_REALLOC(set,
					 ENCODING_SET_SIZE(*capacity + 10));
		if (new_buf == NULL)
			return ENOMEM;
		*buf = new_buf;
		set = new_buf;
		*capacity += 10;
	}

	e = &set->items[set->size];
	e->index = 0;
	strlcpy(e->name, encoding_names[enc], MAX_AUDIO_DEV_LEN);
	e->encoding = enc;
	e->precision = prec;
	e->flags = flags;
	set->size += 1;
	return 0;
}

/**
 * Delete an audio_encoding_set created by auconv_create_encodings().
 *
 * Usage:
 *	foo_detach(...) {
 *		:
 *		auconv_delete_encodings(sc->sc_encodings);
 *		:
 *	}
 *
 * @param encodings	[IN] An audio_encoding_set which was created by
 *			auconv_create_encodings().
 * @return errno; 0 for success.
 */
int auconv_delete_encodings(struct audio_encoding_set *encodings)
{
	if (encodings != NULL)
		AUCONV_FREE(encodings);
	return 0;
}

/**
 * Copy the element specified by aep->index.
 *
 * Usage:
 * int foo_query_encoding(void *v, audio_encoding_t *aep) {
 *	struct foo_softc *sc = (struct foo_softc *)v;
 *	return auconv_query_encoding(sc->sc_encodings, aep);
 * }
 *
 * @param encodings	[IN] An audio_encoding_set created by
 *			auconv_create_encodings().
 * @param aep		[IN/OUT] resultant audio_encoding_t.
 */
int
auconv_query_encoding(const struct audio_encoding_set *encodings,
		      audio_encoding_t *aep)
{
	if (aep->index >= encodings->size)
		return EINVAL;
	strlcpy(aep->name, encodings->items[aep->index].name,
		MAX_AUDIO_DEV_LEN);
	aep->encoding = encodings->items[aep->index].encoding;
	aep->precision = encodings->items[aep->index].precision;
	aep->flags = encodings->items[aep->index].flags;
	return 0;
}
