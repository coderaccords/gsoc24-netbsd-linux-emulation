/*	$NetBSD: gtidmacvar.h,v 1.1 2010/04/28 13:51:56 kiyohara Exp $	*/
/*
 * Copyright (c) 2008, 2009 KIYOHARA Takashi
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

#ifndef _GTIDMACVAR_H_
#define _GTIDMACVAR_H_

void *gtidmac_tag_get(void);

int gtidmac_chan_alloc(void *, bus_dmamap_t **, bus_dmamap_t **, void *);
void gtidmac_chan_free(void *, int);
int gtidmac_setup(void *, int, int, bus_dmamap_t *, bus_dmamap_t *, bus_size_t);
void gtidmac_start(void *, int,
	 	   void (*)(void *, int, bus_dmamap_t *, bus_dmamap_t *, int));

int mvxore_chan_alloc(void *, bus_dmamap_t **, bus_dmamap_t **, void *);
void mvxore_chan_free(void *, int);
int mvxore_setup(void *, int, int, bus_dmamap_t *, bus_dmamap_t *, bus_size_t);
void mvxore_start(void *, int,
		  void (*)(void *, int, bus_dmamap_t *, bus_dmamap_t *, int));

#endif	/* _GTIDMACVAR_H_ */
