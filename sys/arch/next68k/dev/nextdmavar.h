/*	$NetBSD: nextdmavar.h,v 1.11 2002/09/11 01:46:32 mycroft Exp $	*/
/*
 * Copyright (c) 1998 Darrin B. Jewell
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
 *      This product includes software developed by Darrin B. Jewell
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
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


struct nextdma_channel {
	char			*nd_name;
	int			nd_base;
	int			nd_size;
	u_long			nd_intr;
	int			(*nd_intrfunc) __P((void *));
};

struct nextdma_config {
	/* This is called to get another map to dma */
	bus_dmamap_t		(*nd_continue_cb) __P((void *));
	/* This is called when a map has completed dma */
	void			(*nd_completed_cb) __P((bus_dmamap_t, void *));
	/* This is called when dma shuts down */
	void			(*nd_shutdown_cb)  __P((void *));
	/* callback argument */
	void			*nd_cb_arg;					
};

struct nextdma_status {
	bus_dmamap_t	nd_map;			/* map currently in dd_next */
	int		nd_idx;			/* idx of segment currently in dd_next */
	bus_dmamap_t	nd_map_cont;		/* map needed to continue DMA */
	int		nd_idx_cont;		/* segment index to continue DMA */
	int		nd_exception;
};

struct nextdma_softc {
	struct device		sc_dev;
	struct nextdma_channel	*sc_chan;
	bus_space_handle_t	sc_bsh;		/* bus space handle */
	bus_space_tag_t		sc_bst;		/* bus space tag */
	bus_dma_tag_t		sc_dmat;
	struct nextdma_config	sc_conf;
	struct nextdma_status	sc_stat;
	struct evcnt		sc_intrcnt;
};

#define nextdma_setconf(nsc, elem, val) nsc->sc_conf.nd_##elem = (val)

/* Configure the interface & initialize private structure vars */
void nextdma_config	__P((struct nextdma_softc *));
void nextdma_init	__P((struct nextdma_softc *));
void nextdma_start	__P((struct nextdma_softc *, u_long));

/* query to see if nextdma is finished */
int nextdma_finished	__P((struct nextdma_softc *));
void nextdma_reset	__P((struct nextdma_softc *));

void nextdma_print	__P((struct nextdma_softc *));

struct nextdma_softc *nextdma_findchannel __P((char *));
