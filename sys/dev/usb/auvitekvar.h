/* $NetBSD: auvitekvar.h,v 1.1 2010/12/27 15:42:11 jmcneill Exp $ */

/*-
 * Copyright (c) 2010 Jared D. McNeill <jmcneill@invisible.ca>
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

#ifndef _AUVITEKVAR_H
#define _AUVITEKVAR_H

#include <sys/mutex.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usbdi_util.h>
#include <dev/i2c/i2cvar.h>

#include <dev/i2c/au8522var.h>
#include <dev/i2c/xc5kvar.h>
#include <dev/i2c/xc5kreg.h>

struct auvitek_softc;

enum auvitek_board {
	AUVITEK_BOARD_HVR_950Q,
};

#define	AUVITEK_NXFERS		8
#define AUVITEK_XFER_ALTNO	5

struct auvitek_isoc {
	struct auvitek_xfer	*i_ax;
	usbd_xfer_handle	i_xfer;
	uint8_t			*i_buf;
	uint16_t		*i_frlengths;
};

struct auvitek_videobuf {
	uint8_t			av_buf[720*480*2];
	uint32_t		av_el, av_eb;
	uint32_t		av_ol, av_ob;
	uint32_t		av_stride;
};

struct auvitek_xfer {
	struct auvitek_softc	*ax_sc;
	int			ax_endpt;
	uint16_t		ax_maxpktlen;
	usbd_pipe_handle	ax_pipe;
	struct auvitek_isoc	ax_i[AUVITEK_NXFERS];
	uint32_t		ax_nframes;
	uint32_t		ax_uframe_len;
	uint8_t			ax_frinfo;
	int			ax_frno;
	struct auvitek_videobuf	ax_av;
};

struct auvitek_softc {
	device_t		sc_dev;
	device_t		sc_videodev, sc_audiodev;
	struct i2c_controller	sc_i2c;
	kmutex_t		sc_i2c_lock;

	usbd_device_handle	sc_udev;
	int			sc_uport;
	usbd_interface_handle	sc_iface;

	char			sc_running;
	char			sc_dying;

	enum auvitek_board	sc_board;
	const char		*sc_descr;
	uint8_t			sc_i2c_clkdiv;

	struct au8522		*sc_au8522;
	struct xc5k		*sc_xc5k;
	kmutex_t		sc_subdev_lock;

	unsigned int		sc_ainput, sc_vinput;
	uint32_t		sc_curfreq;

	struct auvitek_xfer	sc_ax;

	char			sc_businfo[32];
};

/* auvitek.c */
uint8_t	auvitek_read_1(struct auvitek_softc *, uint16_t);
void	auvitek_write_1(struct auvitek_softc *, uint16_t, uint8_t);

/* auvitek_audio.c */
int	auvitek_audio_attach(struct auvitek_softc *);
int	auvitek_audio_detach(struct auvitek_softc *, int);
void	auvitek_audio_childdet(struct auvitek_softc *, device_t);

/* auvitek_board.c */
void	auvitek_board_init(struct auvitek_softc *);
int	auvitek_board_tuner_reset(struct auvitek_softc *);

/* auvitek_i2c.c */
int	auvitek_i2c_attach(struct auvitek_softc *);
int	auvitek_i2c_detach(struct auvitek_softc *, int);

/* auvitek_video.c */
int	auvitek_video_attach(struct auvitek_softc *);
int	auvitek_video_detach(struct auvitek_softc *, int);
void	auvitek_video_childdet(struct auvitek_softc *, device_t);

#endif /* !_AUVITEKVAR_H */
