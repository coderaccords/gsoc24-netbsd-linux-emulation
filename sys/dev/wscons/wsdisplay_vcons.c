/*	$NetBSD: wsdisplay_vcons.c,v 1.21 2011/02/08 13:40:35 jmcneill Exp $ */

/*-
 * Copyright (c) 2005, 2006 Michael Lorenz
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

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: wsdisplay_vcons.c,v 1.21 2011/02/08 13:40:35 jmcneill Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/buf.h>
#include <sys/device.h>
#include <sys/ioctl.h>
#include <sys/malloc.h>
#include <sys/mman.h>
#include <sys/tty.h>
#include <sys/conf.h>
#include <sys/proc.h>
#include <sys/kthread.h>
#include <sys/tprintf.h>
#include <sys/atomic.h>

#include <dev/wscons/wsdisplayvar.h>
#include <dev/wscons/wsconsio.h>
#include <dev/wsfont/wsfont.h>
#include <dev/rasops/rasops.h>

#include <dev/wscons/wsdisplay_vconsvar.h>

#include "opt_wsemul.h"
#include "opt_wsdisplay_compat.h"
#include "opt_vcons.h"

#if defined(VCONS_DRAW_ASYNC) && defined(VCONS_DRAW_INTR)
#error VCONS_DRAW_ASYNC and VCONS_DRAW_INTR cannot be defined together
#endif

static void vcons_dummy_init_screen(void *, struct vcons_screen *, int, 
	    long *);

static int  vcons_ioctl(void *, void *, u_long, void *, int, struct lwp *);
static int  vcons_alloc_screen(void *, const struct wsscreen_descr *, void **, 
	    int *, int *, long *);
static void vcons_free_screen(void *, void *);
static int  vcons_show_screen(void *, void *, int, void (*)(void *, int, int),
	    void *);

#ifdef WSDISPLAY_SCROLLSUPPORT
static void vcons_scroll(void *, void *, int);
static void vcons_do_scroll(struct vcons_screen *);
#endif

static void vcons_do_switch(void *);

/* methods that work only on text buffers */
static void vcons_copycols_buffer(void *, int, int, int, int);
static void vcons_erasecols_buffer(void *, int, int, int, long);
static void vcons_copyrows_buffer(void *, int, int, int);
static void vcons_eraserows_buffer(void *, int, int, long);
static void vcons_putchar_buffer(void *, int, int, u_int, long);

#ifdef VCONS_DRAW_ASYNC
/* methods that work asynchronously */
static void vcons_copycols_async(void *, int, int, int, int);
static void vcons_erasecols_async(void *, int, int, int, long);
static void vcons_copyrows_async(void *, int, int, int);
static void vcons_eraserows_async(void *, int, int, long);
static void vcons_putchar_async(void *, int, int, u_int, long);
static void vcons_cursor_async(void *, int, int, int);
#endif

/*
 * actual wrapper methods which call both the _buffer ones above and the
 * driver supplied ones to do the drawing
 */
static void vcons_copycols(void *, int, int, int, int);
static void vcons_erasecols(void *, int, int, int, long);
static void vcons_copyrows(void *, int, int, int);
static void vcons_eraserows(void *, int, int, long);
static void vcons_putchar(void *, int, int, u_int, long);
static void vcons_cursor(void *, int, int, int);

/*
 * methods that avoid framebuffer reads
 */
static void vcons_copycols_noread(void *, int, int, int, int);
static void vcons_copyrows_noread(void *, int, int, int);


/* support for reading/writing text buffers. For wsmoused */
static int  vcons_putwschar(struct vcons_screen *, struct wsdisplay_char *);
static int  vcons_getwschar(struct vcons_screen *, struct wsdisplay_char *);

static void vcons_lock(struct vcons_screen *);
static void vcons_unlock(struct vcons_screen *);

#ifdef VCONS_DRAW_ASYNC
static void vcons_kthread(void *);
#endif
#ifdef VCONS_DRAW_INTR
static void vcons_intr(void *);
static void vcons_intr_work(struct work *, void *);
static void vcons_intr_enable(device_t);
#endif

int
vcons_init(struct vcons_data *vd, void *cookie, struct wsscreen_descr *def,
    struct wsdisplay_accessops *ao)
{

	/* zero out everything so we can rely on untouched fields being 0 */
	memset(vd, 0, sizeof(struct vcons_data));
	
	vd->cookie = cookie;

	vd->init_screen = vcons_dummy_init_screen;
	vd->show_screen_cb = NULL;

	/* keep a copy of the accessops that we replace below with our
	 * own wrappers */
	vd->ioctl = ao->ioctl;

	/* configure the accessops */
	ao->ioctl = vcons_ioctl;
	ao->alloc_screen = vcons_alloc_screen;
	ao->free_screen = vcons_free_screen;
	ao->show_screen = vcons_show_screen;
#ifdef WSDISPLAY_SCROLLSUPPORT
	ao->scroll = vcons_scroll;
#endif

	LIST_INIT(&vd->screens);
	vd->active = NULL;
	vd->wanted = NULL;
	vd->currenttype = def;
	callout_init(&vd->switch_callout, 0);
	callout_setfunc(&vd->switch_callout, vcons_do_switch, vd);

	/*
	 * a lock to serialize access to the framebuffer.
	 * when switching screens we need to make sure there's no rasops
	 * operation in progress
	 */
#ifdef DIAGNOSTIC
	vd->switch_poll_count = 0;
#endif
#ifdef VCONS_DRAW_ASYNC
	kthread_create(PRI_NONE, 0, NULL, vcons_kthread, vd,
	    &vd->drawing_thread, "vcons_draw");
#endif
#ifdef VCONS_DRAW_INTR
	workqueue_create(&vd->intr_wq, "vcons_draw",
	    vcons_intr_work, vd, PRI_KTHREAD, IPL_TTY, WQ_MPSAFE);
	callout_init(&vd->intr, 0);
	callout_setfunc(&vd->intr, vcons_intr, vd);

	/* XXX assume that the 'dev' arg is never dereferenced */
	config_interrupts((device_t)vd, vcons_intr_enable);
#endif
	return 0;
}

static void
vcons_lock(struct vcons_screen *scr)
{
#ifdef VCONS_PARANOIA
	int s;

	s = splhigh();
#endif
	SCREEN_BUSY(scr);
#ifdef VCONS_PARANOIA
	splx(s);
#endif
}

static void
vcons_unlock(struct vcons_screen *scr)
{
#ifdef VCONS_PARANOIA
	int s;

	s = splhigh();
#endif
	SCREEN_IDLE(scr);
#ifdef VCONS_PARANOIA
	splx(s);
#endif
}

static void
vcons_dummy_init_screen(void *cookie,
    struct vcons_screen *scr, int exists,
    long *defattr)
{

	/*
	 * default init_screen() method.
	 * Needs to be overwritten so we bitch and whine in case anyone ends
	 * up in here.
	 */
	printf("vcons_init_screen: dummy function called. Your driver is "
	       "supposed to supply a replacement for proper operation\n");
}

int
vcons_init_screen(struct vcons_data *vd, struct vcons_screen *scr,
    int existing, long *defattr)
{
	struct rasops_info *ri = &scr->scr_ri;
	int cnt, i;

	scr->scr_cookie = vd->cookie;
	scr->scr_vd = scr->scr_origvd = vd;
	scr->scr_busy = 0;
	
	/*
	 * call the driver-supplied init_screen function which is expected
	 * to set up rasops_info, override cursor() and probably others
	 */
	vd->init_screen(vd->cookie, scr, existing, defattr);

	/* 
	 * save the non virtual console aware rasops and replace them with 
	 * our wrappers
	 */
	vd->eraserows = ri->ri_ops.eraserows;
	vd->erasecols = ri->ri_ops.erasecols;
	vd->putchar   = ri->ri_ops.putchar;
	vd->cursor    = ri->ri_ops.cursor;

	if (scr->scr_flags & VCONS_NO_COPYCOLS) {
		vd->copycols  = vcons_copycols_noread;
	} else {
		vd->copycols = ri->ri_ops.copycols;
	}

	if (scr->scr_flags & VCONS_NO_COPYROWS) {
		vd->copyrows  = vcons_copyrows_noread;
	} else {
		vd->copyrows = ri->ri_ops.copyrows;
	}

	ri->ri_ops.eraserows = vcons_eraserows;	
	ri->ri_ops.erasecols = vcons_erasecols;	
	ri->ri_ops.putchar   = vcons_putchar;
	ri->ri_ops.cursor    = vcons_cursor;
	ri->ri_ops.copycols  = vcons_copycols;
	ri->ri_ops.copyrows  = vcons_copyrows;


	ri->ri_hw = scr;

	/* 
	 * we allocate both chars and attributes in one chunk, attributes first 
	 * because they have the (potentially) bigger alignment 
	 */
#ifdef WSDISPLAY_SCROLLSUPPORT
	cnt = (ri->ri_rows + WSDISPLAY_SCROLLBACK_LINES) * ri->ri_cols;
	scr->scr_lines_in_buffer = WSDISPLAY_SCROLLBACK_LINES;
	scr->scr_current_line = 0;
	scr->scr_line_wanted = 0;
	scr->scr_offset_to_zero = ri->ri_cols * WSDISPLAY_SCROLLBACK_LINES;
	scr->scr_current_offset = scr->scr_offset_to_zero;
#else
	cnt = ri->ri_rows * ri->ri_cols;
#endif
	scr->scr_attrs = (long *)malloc(cnt * (sizeof(long) + 
	    sizeof(uint16_t)), M_DEVBUF, M_WAITOK);
	if (scr->scr_attrs == NULL)
		return ENOMEM;

	scr->scr_chars = (uint16_t *)&scr->scr_attrs[cnt];

	ri->ri_ops.allocattr(ri, WS_DEFAULT_FG, WS_DEFAULT_BG, 0, defattr);
	scr->scr_defattr = *defattr;

	/* 
	 * fill the attribute buffer with *defattr, chars with 0x20 
	 * since we don't know if the driver tries to mimic firmware output or
	 * reset everything we do nothing to VRAM here, any driver that feels
	 * the need to clear screen or something will have to do it on its own
	 * Additional screens will start out in the background anyway so
	 * cleaning or not only really affects the initial console screen
	 */
	for (i = 0; i < cnt; i++) {
		scr->scr_attrs[i] = *defattr;
		scr->scr_chars[i] = 0x20;
	}

	if(vd->active == NULL) {
		vd->active = scr;
		SCREEN_VISIBLE(scr);
	}
	
	if (existing) {
		SCREEN_VISIBLE(scr);
		vd->active = scr;
	} else {
		SCREEN_INVISIBLE(scr);
	}
	
	LIST_INSERT_HEAD(&vd->screens, scr, next);
	return 0;
}

static void
vcons_do_switch(void *arg)
{
	struct vcons_data *vd = arg;
	struct vcons_screen *scr, *oldscr;

	scr = vd->wanted;
	if (!scr) {
		printf("vcons_switch_screen: disappeared\n");
		vd->switch_cb(vd->switch_cb_arg, EIO, 0);
		return;
	}
	oldscr = vd->active; /* can be NULL! */

	/* 
	 * if there's an old, visible screen we mark it invisible and wait
	 * until it's not busy so we can safely switch 
	 */
	if (oldscr != NULL) {
		SCREEN_INVISIBLE(oldscr);
		if (SCREEN_IS_BUSY(oldscr)) {
			callout_schedule(&vd->switch_callout, 1);
#ifdef DIAGNOSTIC
			/* bitch if we wait too long */
			vd->switch_poll_count++;
			if (vd->switch_poll_count > 100) {
				panic("vcons: screen still busy");
			}
#endif
			return;
		}
		/* invisible screen -> no visible cursor image */
		oldscr->scr_ri.ri_flg &= ~RI_CURSOR;
#ifdef DIAGNOSTIC
		vd->switch_poll_count = 0;
#endif
	}

	if (scr == oldscr)
		return;

#ifdef DIAGNOSTIC
	if (SCREEN_IS_VISIBLE(scr))
		printf("vcons_switch_screen: already active");
#endif

#ifdef notyet
	if (vd->currenttype != type) {
		vcons_set_screentype(vd, type);
		vd->currenttype = type;
	}
#endif

	SCREEN_VISIBLE(scr);
	vd->active = scr;
	vd->wanted = NULL;

	if (vd->show_screen_cb != NULL)
		vd->show_screen_cb(scr);

	if ((scr->scr_flags & VCONS_NO_REDRAW) == 0)
		vcons_redraw_screen(scr);

	if (vd->switch_cb)
		vd->switch_cb(vd->switch_cb_arg, 0, 0);
}

void
vcons_redraw_screen(struct vcons_screen *scr)
{
	uint16_t *charptr = scr->scr_chars;
	long *attrptr = scr->scr_attrs;
	struct rasops_info *ri = &scr->scr_ri;
	int i, j, offset;

	vcons_lock(scr);
	if (SCREEN_IS_VISIBLE(scr) && SCREEN_CAN_DRAW(scr)) {

		/*
		 * only clear the screen when RI_FULLCLEAR is set since we're
		 * going to overwrite every single character cell anyway
		 */
		if (ri->ri_flg & RI_FULLCLEAR) {
			scr->scr_vd->eraserows(ri, 0, ri->ri_rows,
			    scr->scr_defattr);
		}

		/* redraw the screen */
#ifdef WSDISPLAY_SCROLLSUPPORT
		offset = scr->scr_current_offset;
#else
		offset = 0;
#endif
		for (i = 0; i < ri->ri_rows; i++) {
			for (j = 0; j < ri->ri_cols; j++) {
				/*
				 * no need to use the wrapper function - we 
				 * don't change any characters or attributes
				 * and we already made sure the screen we're
				 * working on is visible
				 */
				scr->scr_vd->putchar(ri, i, j, 
				    charptr[offset], attrptr[offset]);
				offset++;
			}
		}
		ri->ri_flg &= ~RI_CURSOR;
		scr->scr_vd->cursor(ri, 1, ri->ri_crow, ri->ri_ccol);
	}
	vcons_unlock(scr);
}

static int
vcons_ioctl(void *v, void *vs, u_long cmd, void *data, int flag,
	struct lwp *l)
{
	struct vcons_data *vd = v;
	int error;

	switch (cmd) {
	case WSDISPLAYIO_GETWSCHAR:
		error = vcons_getwschar((struct vcons_screen *)vs,
			(struct wsdisplay_char *)data);
		break;

	case WSDISPLAYIO_PUTWSCHAR:
		error = vcons_putwschar((struct vcons_screen *)vs,
			(struct wsdisplay_char *)data);
		break;

	default:
		if (vd->ioctl != NULL)
			error = (*vd->ioctl)(v, vs, cmd, data, flag, l);
		else
			error = EPASSTHROUGH;
	}

	return error;
}

static int
vcons_alloc_screen(void *v, const struct wsscreen_descr *type, void **cookiep,
    int *curxp, int *curyp, long *defattrp)
{
	struct vcons_data *vd = v;
	struct vcons_screen *scr;
	int ret;

	scr = malloc(sizeof(struct vcons_screen), M_DEVBUF, M_WAITOK | M_ZERO);
	if (scr == NULL)
		return ENOMEM;

	scr->scr_flags = 0;		
	scr->scr_status = 0;
	scr->scr_busy = 0;
	scr->scr_type = type;

	ret = vcons_init_screen(vd, scr, 0, defattrp);
	if (ret != 0) {
		free(scr, M_DEVBUF);
		return ret;
	}

	if (vd->active == NULL) {
		SCREEN_VISIBLE(scr);
		vd->active = scr;
		vd->currenttype = type;
	}

	*cookiep = scr;
	*curxp = scr->scr_ri.ri_ccol;
	*curyp = scr->scr_ri.ri_crow;
	return 0;
}

static void
vcons_free_screen(void *v, void *cookie)
{
	struct vcons_data *vd = v;
	struct vcons_screen *scr = cookie;

	vcons_lock(scr);
	/* there should be no rasops activity here */

	LIST_REMOVE(scr, next);

	if ((scr->scr_flags & VCONS_SCREEN_IS_STATIC) == 0) {
		free(scr->scr_attrs, M_DEVBUF);
		free(scr, M_DEVBUF);
	} else {
		/*
		 * maybe we should just restore the old rasops_info methods
		 * and free the character/attribute buffer here?
		 */
#ifdef VCONS_DEBUG
		panic("vcons_free_screen: console");
#else
		printf("vcons_free_screen: console\n");
#endif
	}

	if (vd->active == scr)
		vd->active = NULL;
}

static int
vcons_show_screen(void *v, void *cookie, int waitok,
    void (*cb)(void *, int, int), void *cb_arg)
{
	struct vcons_data *vd = v;
	struct vcons_screen *scr;

	scr = cookie;
	if (scr == vd->active)
		return 0;

	vd->wanted = scr;
	vd->switch_cb = cb;
	vd->switch_cb_arg = cb_arg;
	if (cb) {
		callout_schedule(&vd->switch_callout, 0);
		return EAGAIN;
	}

	vcons_do_switch(vd);
	return 0;
}

/* wrappers for rasops_info methods */

static void
vcons_copycols_buffer(void *cookie, int row, int srccol, int dstcol, int ncols)
{
	struct rasops_info *ri = cookie;
	struct vcons_screen *scr = ri->ri_hw;
	int from = srccol + row * ri->ri_cols;
	int to = dstcol + row * ri->ri_cols;

#ifdef WSDISPLAY_SCROLLSUPPORT
	int offset;
	offset = scr->scr_offset_to_zero;

	memmove(&scr->scr_attrs[offset + to], &scr->scr_attrs[offset + from],
	    ncols * sizeof(long));
	memmove(&scr->scr_chars[offset + to], &scr->scr_chars[offset + from],
	    ncols * sizeof(uint16_t));
#else
	memmove(&scr->scr_attrs[to], &scr->scr_attrs[from],
	    ncols * sizeof(long));
	memmove(&scr->scr_chars[to], &scr->scr_chars[from],
	    ncols * sizeof(uint16_t));
#endif

#ifdef VCONS_DRAW_INTR
	scr->scr_dirty++;
#endif
}

static void
vcons_copycols(void *cookie, int row, int srccol, int dstcol, int ncols)
{
	struct rasops_info *ri = cookie;
	struct vcons_screen *scr = ri->ri_hw;

	vcons_copycols_buffer(cookie, row, srccol, dstcol, ncols);

#if defined(VCONS_DRAW_INTR)
	if (scr->scr_vd->use_intr)
		return;
#endif

	vcons_lock(scr);
	if (SCREEN_IS_VISIBLE(scr) && SCREEN_CAN_DRAW(scr)) {
#ifdef VCONS_DRAW_ASYNC
		struct vcons_data *vd = scr->scr_vd;
		if (vd->use_async) {
			vcons_copycols_async(cookie, row, srccol, dstcol, ncols);
		} else
#endif
			scr->scr_vd->copycols(cookie, row, srccol, dstcol, ncols);
	}
	vcons_unlock(scr);
}

static void
vcons_copycols_noread(void *cookie, int row, int srccol, int dstcol, int ncols)
{
	struct rasops_info *ri = cookie;
	struct vcons_screen *scr = ri->ri_hw;

	if (SCREEN_IS_VISIBLE(scr) && SCREEN_CAN_DRAW(scr)) {
		int pos, c, offset;

#ifdef WSDISPLAY_SCROLLSUPPORT
		offset = scr->scr_current_offset;
#else
		offset = 0;
#endif
		pos = ri->ri_cols * row + dstcol + offset;
		for (c = dstcol; c < (dstcol + ncols); c++) {
			scr->scr_vd->putchar(cookie, row, c, 
			   scr->scr_chars[pos], scr->scr_attrs[pos]);
			pos++;
		}
	}
}

static void
vcons_erasecols_buffer(void *cookie, int row, int startcol, int ncols, long fillattr)
{
	struct rasops_info *ri = cookie;
	struct vcons_screen *scr = ri->ri_hw;
	int start = startcol + row * ri->ri_cols;
	int end = start + ncols, i;

#ifdef WSDISPLAY_SCROLLSUPPORT
	int offset;
	offset = scr->scr_offset_to_zero;

	for (i = start; i < end; i++) {
		scr->scr_attrs[offset + i] = fillattr;
		scr->scr_chars[offset + i] = 0x20;
	}
#else
	for (i = start; i < end; i++) {
		scr->scr_attrs[i] = fillattr;
		scr->scr_chars[i] = 0x20;
	}
#endif

#ifdef VCONS_DRAW_INTR
	scr->scr_dirty++;
#endif
}

static void
vcons_erasecols(void *cookie, int row, int startcol, int ncols, long fillattr)
{
	struct rasops_info *ri = cookie;
	struct vcons_screen *scr = ri->ri_hw;

	vcons_erasecols_buffer(cookie, row, startcol, ncols, fillattr);

#if defined(VCONS_DRAW_INTR)
	if (scr->scr_vd->use_intr)
		return;
#endif

	vcons_lock(scr);
	if (SCREEN_IS_VISIBLE(scr) && SCREEN_CAN_DRAW(scr)) {
#ifdef VCONS_DRAW_ASYNC
		struct vcons_data *vd = scr->scr_vd;
		if (vd->use_async) {
			vcons_erasecols_async(cookie, row, startcol, ncols, 
			    fillattr);
		} else
#endif
			scr->scr_vd->erasecols(cookie, row, startcol, ncols, 
			    fillattr);
	}
	vcons_unlock(scr);
}

static void
vcons_copyrows_buffer(void *cookie, int srcrow, int dstrow, int nrows)
{
	struct rasops_info *ri = cookie;
	struct vcons_screen *scr = ri->ri_hw;
	int from, to, len;

#ifdef WSDISPLAY_SCROLLSUPPORT
	int offset;
	offset = scr->scr_offset_to_zero;

	/* do we need to scroll the back buffer? */
	if (dstrow == 0) {
		from = ri->ri_cols * srcrow;
		to = ri->ri_cols * dstrow;

		memmove(&scr->scr_attrs[to], &scr->scr_attrs[from],
		    scr->scr_offset_to_zero * sizeof(long));
		memmove(&scr->scr_chars[to], &scr->scr_chars[from],
		    scr->scr_offset_to_zero * sizeof(uint16_t));
	}
	from = ri->ri_cols * srcrow + offset;
	to = ri->ri_cols * dstrow + offset;
	len = ri->ri_cols * nrows;		
		
#else
	from = ri->ri_cols * srcrow;
	to = ri->ri_cols * dstrow;
	len = ri->ri_cols * nrows;
#endif
	memmove(&scr->scr_attrs[to], &scr->scr_attrs[from],
	    len * sizeof(long));
	memmove(&scr->scr_chars[to], &scr->scr_chars[from],
	    len * sizeof(uint16_t));

#ifdef VCONS_DRAW_INTR
	scr->scr_dirty++;
#endif
}

static void
vcons_copyrows(void *cookie, int srcrow, int dstrow, int nrows)
{
	struct rasops_info *ri = cookie;
	struct vcons_screen *scr = ri->ri_hw;

	vcons_copyrows_buffer(cookie, srcrow, dstrow, nrows);

#if defined(VCONS_DRAW_INTR)
	if (scr->scr_vd->use_intr)
		return;
#endif

	vcons_lock(scr);
	if (SCREEN_IS_VISIBLE(scr) && SCREEN_CAN_DRAW(scr)) {
#ifdef VCONS_DRAW_ASYNC
		struct vcons_data *vd = scr->scr_vd;
		if (vd->use_async) {
			vcons_copyrows_async(cookie, srcrow, dstrow, nrows);
		} else
#endif
			scr->scr_vd->copyrows(cookie, srcrow, dstrow, nrows);
	}
	vcons_unlock(scr);
}

static void
vcons_copyrows_noread(void *cookie, int srcrow, int dstrow, int nrows)
{
	struct rasops_info *ri = cookie;
	struct vcons_screen *scr = ri->ri_hw;

	if (SCREEN_IS_VISIBLE(scr) && SCREEN_CAN_DRAW(scr)) {
		int pos, l, c, offset;

#ifdef WSDISPLAY_SCROLLSUPPORT
		offset = scr->scr_current_offset;
#else
		offset = 0;
#endif
		pos = ri->ri_cols * dstrow + offset;
		for (l = dstrow; l < (dstrow + nrows); l++) {
			for (c = 0; c < ri->ri_cols; c++) {
				scr->scr_vd->putchar(cookie, l, c, 
				   scr->scr_chars[pos], scr->scr_attrs[pos]);
				pos++;
			}
		}
	}
}

static void
vcons_eraserows_buffer(void *cookie, int row, int nrows, long fillattr)
{
	struct rasops_info *ri = cookie;
	struct vcons_screen *scr = ri->ri_hw;
	int start, end, i;

#ifdef WSDISPLAY_SCROLLSUPPORT
	int offset;
	offset = scr->scr_offset_to_zero;

	start = ri->ri_cols * row + offset;
	end = ri->ri_cols * (row + nrows) + offset;
#else
	start = ri->ri_cols * row;
	end = ri->ri_cols * (row + nrows);
#endif

	for (i = start; i < end; i++) {
		scr->scr_attrs[i] = fillattr;
		scr->scr_chars[i] = 0x20;
	}

#ifdef VCONS_DRAW_INTR
	scr->scr_dirty++;
#endif
}

static void
vcons_eraserows(void *cookie, int row, int nrows, long fillattr)
{
	struct rasops_info *ri = cookie;
	struct vcons_screen *scr = ri->ri_hw;

	vcons_eraserows_buffer(cookie, row, nrows, fillattr);

#if defined(VCONS_DRAW_INTR)
	if (scr->scr_vd->use_intr)
		return;
#endif

	vcons_lock(scr);
	if (SCREEN_IS_VISIBLE(scr) && SCREEN_CAN_DRAW(scr)) {
#ifdef VCONS_DRAW_ASYNC
		struct vcons_data *vd = scr->scr_vd;
		if (vd->use_async) {
			vcons_eraserows_async(cookie, row, nrows, fillattr);
		} else
#endif
			scr->scr_vd->eraserows(cookie, row, nrows, fillattr);
	}
	vcons_unlock(scr);
}

static void
vcons_putchar_buffer(void *cookie, int row, int col, u_int c, long attr)
{
	struct rasops_info *ri = cookie;
	struct vcons_screen *scr = ri->ri_hw;
	int pos;
	
#ifdef WSDISPLAY_SCROLLSUPPORT
	int offset;
	offset = scr->scr_offset_to_zero;

	if ((row >= 0) && (row < ri->ri_rows) && (col >= 0) && 
	     (col < ri->ri_cols)) {
		pos = col + row * ri->ri_cols;
		scr->scr_attrs[pos + offset] = attr;
		scr->scr_chars[pos + offset] = c;
	}
#else
	if ((row >= 0) && (row < ri->ri_rows) && (col >= 0) && 
	     (col < ri->ri_cols)) {
		pos = col + row * ri->ri_cols;
		scr->scr_attrs[pos] = attr;
		scr->scr_chars[pos] = c;
	}
#endif

#ifdef VCONS_DRAW_INTR
	scr->scr_dirty++;
#endif
}

static void
vcons_putchar(void *cookie, int row, int col, u_int c, long attr)
{
	struct rasops_info *ri = cookie;
	struct vcons_screen *scr = ri->ri_hw;
	
	vcons_putchar_buffer(cookie, row, col, c, attr);

#if defined(VCONS_DRAW_INTR)
	if (scr->scr_vd->use_intr)
		return;
#endif

	vcons_lock(scr);
	if (SCREEN_IS_VISIBLE(scr) && SCREEN_CAN_DRAW(scr)) {
#ifdef VCONS_DRAW_ASYNC
		struct vcons_data *vd = scr->scr_vd;
		if (vd->use_async) {
			vcons_putchar_async(cookie, row, col, c, attr);
		} else
#endif
			scr->scr_vd->putchar(cookie, row, col, c, attr);
	}
	vcons_unlock(scr);
}

static void
vcons_cursor(void *cookie, int on, int row, int col)
{
	struct rasops_info *ri = cookie;
	struct vcons_screen *scr = ri->ri_hw;


#if defined(VCONS_DRAW_INTR)
	if (scr->scr_vd->use_intr) {
		vcons_lock(scr);
		if (scr->scr_ri.ri_crow != row || scr->scr_ri.ri_ccol != col) {
			scr->scr_ri.ri_crow = row;
			scr->scr_ri.ri_ccol = col;
			scr->scr_dirty++;
		}
		vcons_unlock(scr);
		return;
	}
#endif

	vcons_lock(scr);

	if (SCREEN_IS_VISIBLE(scr) && SCREEN_CAN_DRAW(scr)) {
#ifdef VCONS_DRAW_ASYNC
		struct vcons_data *vd = scr->scr_vd;
		if (vd->use_async) {
			vcons_cursor_async(cookie, on, row, col);
		} else
#endif
			scr->scr_vd->cursor(cookie, on, row, col);
	} else {
		scr->scr_ri.ri_crow = row;
		scr->scr_ri.ri_ccol = col;
	}
	vcons_unlock(scr);
}

/* methods to read/write characters via ioctl() */

static int
vcons_putwschar(struct vcons_screen *scr, struct wsdisplay_char *wsc)
{
	long attr;
	struct rasops_info *ri;

	KASSERT(scr != NULL && wsc != NULL);

	ri = &scr->scr_ri;

	if (__predict_false((unsigned int)wsc->col > ri->ri_cols ||
	    (unsigned int)wsc->row > ri->ri_rows))
			return (EINVAL);
	
	if ((wsc->row >= 0) && (wsc->row < ri->ri_rows) && (wsc->col >= 0) && 
	     (wsc->col < ri->ri_cols)) {

		ri->ri_ops.allocattr(ri, wsc->foreground, wsc->background,
		    wsc->flags, &attr);
		vcons_putchar(ri, wsc->row, wsc->col, wsc->letter, attr);
#ifdef VCONS_DEBUG
		printf("vcons_putwschar(%d, %d, %x, %lx\n", wsc->row, wsc->col,
		    wsc->letter, attr);
#endif
		return 0;
	} else
		return EINVAL;
}

static int
vcons_getwschar(struct vcons_screen *scr, struct wsdisplay_char *wsc)
{
	int offset;
	long attr;
	struct rasops_info *ri;

	KASSERT(scr != NULL && wsc != NULL);

	ri = &scr->scr_ri;

	if ((wsc->row >= 0) && (wsc->row < ri->ri_rows) && (wsc->col >= 0) && 
	     (wsc->col < ri->ri_cols)) {

		offset = ri->ri_cols * wsc->row + wsc->col;
#ifdef WSDISPLAY_SCROLLSUPPORT
		offset += scr->scr_offset_to_zero;
#endif
		wsc->letter = scr->scr_chars[offset];
		attr = scr->scr_attrs[offset];

		/* 
		 * this is ugly. We need to break up an attribute into colours and
		 * flags but there's no rasops method to do that so we must rely on
		 * the 'canonical' encoding.
		 */
#ifdef VCONS_DEBUG
		printf("vcons_getwschar: %d, %d, %x, %lx\n", wsc->row,
		    wsc->col, wsc->letter, attr);
#endif
		wsc->foreground = (attr >> 24) & 0xff;
		wsc->background = (attr >> 16) & 0xff;
		wsc->flags      = attr & 0xff;
		return 0;
	} else
		return EINVAL;
}

#ifdef WSDISPLAY_SCROLLSUPPORT

static void
vcons_scroll(void *cookie, void *vs, int where)
{
	struct vcons_screen *scr = vs;

	if (where == 0) {
		scr->scr_line_wanted = 0;
	} else {
		scr->scr_line_wanted = scr->scr_line_wanted - where;
		if (scr->scr_line_wanted < 0)
			scr->scr_line_wanted = 0;
		if (scr->scr_line_wanted > scr->scr_lines_in_buffer)
			scr->scr_line_wanted = scr->scr_lines_in_buffer;
	}

	if (scr->scr_line_wanted != scr->scr_current_line) {

		vcons_do_scroll(scr);
	}
}

static void
vcons_do_scroll(struct vcons_screen *scr)
{
	int dist, from, to, num;
	int r_offset, r_start;
	int i, j;

	if (scr->scr_line_wanted == scr->scr_current_line)
		return;
	dist = scr->scr_line_wanted - scr->scr_current_line;
	scr->scr_current_line = scr->scr_line_wanted;
	scr->scr_current_offset = scr->scr_ri.ri_cols *
	    (scr->scr_lines_in_buffer - scr->scr_current_line);
	if (abs(dist) >= scr->scr_ri.ri_rows) {
		vcons_redraw_screen(scr);
		return;
	}
	/* scroll and redraw only what we really have to */
	if (dist > 0) {
		/* we scroll down */
		from = 0;
		to = dist;
		num = scr->scr_ri.ri_rows - dist;
		/* now the redraw parameters */
		r_offset = scr->scr_current_offset;
		r_start = 0;
	} else {
		/* scrolling up */
		to = 0;
		from = -dist;
		num = scr->scr_ri.ri_rows + dist;
		r_offset = scr->scr_current_offset + num * scr->scr_ri.ri_cols;
		r_start = num;
	}
	scr->scr_vd->copyrows(scr, from, to, num);
	for (i = 0; i < abs(dist); i++) {
		for (j = 0; j < scr->scr_ri.ri_cols; j++) {
			scr->scr_vd->putchar(scr, i + r_start, j,
			    scr->scr_chars[r_offset],
			    scr->scr_attrs[r_offset]);
			r_offset++;
		}
	}

	if (scr->scr_line_wanted == 0) {
		/* this was a reset - need to draw the cursor */
		scr->scr_ri.ri_flg &= ~RI_CURSOR;
		scr->scr_vd->cursor(scr, 1, scr->scr_ri.ri_crow,
		    scr->scr_ri.ri_ccol);
	}
}

#endif /* WSDISPLAY_SCROLLSUPPORT */

/* async drawing using a kernel thread */

#ifdef VCONS_DRAW_ASYNC

static inline uint32_t
vcons_words_in_buffer(struct vcons_data *vd)
{
	int len = vd->rb_write - vd->rb_read;

	if (len < 0) len += VCONS_RING_BUFFER_LENGTH;
	if (len < 0) vd->use_async = 0;
	if (len >= VCONS_RING_BUFFER_LENGTH) vd->use_async = 0;
	return (uint32_t)len;
}

static inline int
vcons_wait_buffer(struct vcons_data *vd, uint32_t words)
{
	int bail = 0;

	mutex_enter(&vd->go_buffer_il);
	while (((VCONS_RING_BUFFER_LENGTH - vcons_words_in_buffer(vd)) < words)
	    && (bail < 3)) {
		if (cv_timedwait(&vd->go_buffer, &vd->go_buffer_il, hz)
		    == EWOULDBLOCK)
			bail++;
	}
	if (bail >= 3) {
		/*
		 * waited too long, something is wrong so fall back to sync
		 * we should probably kill the kthread here and try to empty
		 * the command buffer as well
		 */
		vd->use_async = 0;
	}
	return 0;
}

#define VRB_NEXT(idx) ((idx + 1) >= VCONS_RING_BUFFER_LENGTH) ? 0 : idx + 1

static void
vcons_copycols_async(void *cookie, int row, int srccol, int dstcol, int ncols)
{
	struct rasops_info *ri = cookie;
	struct vcons_screen *scr = ri->ri_hw;
	struct vcons_data *vd = scr->scr_vd;
	int idx;

	vcons_wait_buffer(vd, 5);
	mutex_enter(&vd->drawing_mutex);
	mutex_exit(&vd->go_buffer_il);
	idx = vd->rb_write;
	vd->rb_buffer[idx] = VCMD_COPYCOLS;
	idx = VRB_NEXT(idx);
	vd->rb_buffer[idx] = row;
	idx = VRB_NEXT(idx);
	vd->rb_buffer[idx] = srccol;
	idx = VRB_NEXT(idx);
	vd->rb_buffer[idx] = dstcol;
	idx = VRB_NEXT(idx);
	vd->rb_buffer[idx] = ncols;
	idx = VRB_NEXT(idx);
	membar_producer();
	vd->rb_write = idx;
	membar_enter();
	mutex_exit(&vd->drawing_mutex);
	cv_signal(&vd->go_draw);
}
	
static void
vcons_erasecols_async(void *cookie, int row, int startcol, int ncols,
    long fillattr)
{
	struct rasops_info *ri = cookie;
	struct vcons_screen *scr = ri->ri_hw;
	struct vcons_data *vd = scr->scr_vd;
	int idx;

	vcons_wait_buffer(vd, 5);
	mutex_enter(&vd->drawing_mutex);
	mutex_exit(&vd->go_buffer_il);
	idx = vd->rb_write;
	vd->rb_buffer[idx] = VCMD_ERASECOLS;
	idx = VRB_NEXT(idx);
	vd->rb_buffer[idx] = row;
	idx = VRB_NEXT(idx);
	vd->rb_buffer[idx] = startcol;
	idx = VRB_NEXT(idx);
	vd->rb_buffer[idx] = ncols;
	idx = VRB_NEXT(idx);
	/* 
	 * XXX all drivers I've seen use 32bit attributes although fillattr is
	 * a 64bit value on LP64
	 */
	vd->rb_buffer[idx] = (uint32_t)fillattr;
	idx = VRB_NEXT(idx);
	membar_producer();
	vd->rb_write = idx;
	membar_enter();
	mutex_exit(&vd->drawing_mutex);
	cv_signal(&vd->go_draw);
}

static void
vcons_copyrows_async(void *cookie, int srcrow, int dstrow, int nrows)
{
	struct rasops_info *ri = cookie;
	struct vcons_screen *scr = ri->ri_hw;
	struct vcons_data *vd = scr->scr_vd;
	int idx;

	vcons_wait_buffer(vd, 4);
	mutex_enter(&vd->drawing_mutex);
	mutex_exit(&vd->go_buffer_il);
	idx = vd->rb_write;
	vd->rb_buffer[idx] = VCMD_COPYROWS;
	idx = VRB_NEXT(idx);
	vd->rb_buffer[idx] = srcrow;
	idx = VRB_NEXT(idx);
	vd->rb_buffer[idx] = dstrow;
	idx = VRB_NEXT(idx);
	vd->rb_buffer[idx] = nrows;
	idx = VRB_NEXT(idx);
	membar_producer();
	vd->rb_write = idx;
	membar_enter();
	mutex_exit(&vd->drawing_mutex);
	cv_signal(&vd->go_draw);
}

static void
vcons_eraserows_async(void *cookie, int row, int nrows, long fillattr)
{
	struct rasops_info *ri = cookie;
	struct vcons_screen *scr = ri->ri_hw;
	struct vcons_data *vd = scr->scr_vd;
	int idx;

	vcons_wait_buffer(vd, 4);
	mutex_enter(&vd->drawing_mutex);
	mutex_exit(&vd->go_buffer_il);
	idx = vd->rb_write;
	vd->rb_buffer[idx] = VCMD_ERASEROWS;
	idx = VRB_NEXT(idx);
	vd->rb_buffer[idx] = row;
	idx = VRB_NEXT(idx);
	vd->rb_buffer[idx] = nrows;
	idx = VRB_NEXT(idx);
	vd->rb_buffer[idx] = (uint32_t)fillattr;
	idx = VRB_NEXT(idx);
	membar_producer();
	vd->rb_write = idx;
	membar_enter();
	mutex_exit(&vd->drawing_mutex);
	cv_signal(&vd->go_draw);
}

static void
vcons_putchar_async(void *cookie, int row, int col, u_int c, long attr)
{
	struct rasops_info *ri = cookie;
	struct vcons_screen *scr = ri->ri_hw;
	struct vcons_data *vd = scr->scr_vd;
	int idx;

#ifdef VCONS_ASYNC_DEBUG
	/* mess with the background attribute so we can see if we draw async */
	attr &= 0xff00ffff;
	attr |= (WSCOL_LIGHT_BROWN << 16);
#endif

	vcons_wait_buffer(vd, 5);
	mutex_enter(&vd->drawing_mutex);
	mutex_exit(&vd->go_buffer_il);
	idx = vd->rb_write;
	vd->rb_buffer[idx] = VCMD_PUTCHAR;
	idx = VRB_NEXT(idx);
	vd->rb_buffer[idx] = row;
	idx = VRB_NEXT(idx);
	vd->rb_buffer[idx] = col;
	idx = VRB_NEXT(idx);
	vd->rb_buffer[idx] = c;
	idx = VRB_NEXT(idx);
	vd->rb_buffer[idx] = (uint32_t)attr;
	idx = VRB_NEXT(idx);
	membar_producer();
	vd->rb_write = idx;
	membar_enter();
	mutex_exit(&vd->drawing_mutex);
	cv_signal(&vd->go_draw);
}

static void
vcons_cursor_async(void *cookie, int on, int row, int col)
{
	struct rasops_info *ri = cookie;
	struct vcons_screen *scr = ri->ri_hw;
	struct vcons_data *vd = scr->scr_vd;
	int idx;

	vcons_wait_buffer(vd, 4);
	mutex_enter(&vd->drawing_mutex);
	mutex_exit(&vd->go_buffer_il);
	idx = vd->rb_write;
	vd->rb_buffer[idx] = VCMD_CURSOR;
	idx = VRB_NEXT(idx);
	vd->rb_buffer[idx] = on;
	idx = VRB_NEXT(idx);
	vd->rb_buffer[idx] = row;
	idx = VRB_NEXT(idx);
	vd->rb_buffer[idx] = col;
	idx = VRB_NEXT(idx);
	membar_producer();
	vd->rb_write = idx;
	membar_enter();
	mutex_exit(&vd->drawing_mutex);
	cv_signal(&vd->go_draw);
}

static int
vcons_copy_params(struct vcons_data *vd, int len, uint32_t *buf)
{
	int idx = vd->rb_read, i;

	for (i = 0; i < len; i++) {
		buf[i] = vd->rb_buffer[idx];
		idx = VRB_NEXT(idx);
	}
	return idx;
}


static void
vcons_process_command(struct vcons_data *vd)
{
	/* we take a command out of the buffer, run it and return */
	void *cookie;
	int idx = vd->rb_read;
	uint32_t cmd = vd->rb_buffer[idx];
	uint32_t params[10];

	KASSERT(vd->active != NULL);
	cookie = &vd->active->scr_ri;

	switch (cmd) {
		case VCMD_COPYCOLS:
			idx = vcons_copy_params(vd, 5, params);
			vd->rb_read = idx;
			membar_producer();
			vd->copycols(cookie, params[1], params[2], params[3], params[4]);
			break;
		case VCMD_ERASECOLS:
			idx = vcons_copy_params(vd, 5, params);
			vd->rb_read = idx;
			membar_producer();
			vd->erasecols(cookie, params[1], params[2], params[3], params[4]);
			break;
		case VCMD_COPYROWS:
			idx = vcons_copy_params(vd, 4, params);
			vd->rb_read = idx;
			membar_producer();
			vd->copyrows(cookie, params[1], params[2], params[3]);
			break;
		case VCMD_ERASEROWS:
			idx = vcons_copy_params(vd, 4, params);
			vd->rb_read = idx;
			membar_producer();
			vd->eraserows(cookie, params[1], params[2], params[3]);
			break;
		case VCMD_PUTCHAR:
			idx = vcons_copy_params(vd, 5, params);
			vd->rb_read = idx;
			membar_producer();
			vd->putchar(cookie, params[1], params[2], params[3], params[4]);
			break;
		case VCMD_CURSOR:
			idx = vcons_copy_params(vd, 4, params);
			vd->rb_read = idx;
			membar_producer();
			vd->cursor(cookie, params[1], params[2], params[3]);
			break;
		default:
			/*
			 * invalid command, something is wrong so we fall back
			 * to synchronous operations
			 */
			vd->use_async = 0;
			vd->rb_read = 0;
			vd->rb_write = 0;
	}
}
		
static void vcons_cursor(void *, int, int, int);

static void
vcons_kthread(void *cookie)
{
	struct vcons_data *vd = cookie;

	/* initialize the synchronization goo */
	cv_init(&vd->go_draw, "go_draw");
	cv_init(&vd->go_buffer, "go_buffer");
	mutex_init(&vd->drawing_mutex, MUTEX_DEFAULT, IPL_NONE);
	mutex_init(&vd->go_draw_il, MUTEX_DEFAULT, IPL_NONE);
	mutex_init(&vd->go_buffer_il, MUTEX_DEFAULT, IPL_NONE);
	vd->rb_read = 1000;
	vd->rb_write = 2;
	printf("%d\n", vcons_words_in_buffer(vd));
	vd->rb_read = 0;
	vd->rb_write = 0;
	printf("%d\n", vcons_words_in_buffer(vd));
	printf("%d %d\n", VRB_NEXT(1), VRB_NEXT(1023));
	/* now we're good to go */
	vd->use_async = 1;

	while (1) {

		while (vcons_words_in_buffer(vd) > 0) {
			vcons_process_command(vd);
			cv_signal(&vd->go_buffer);
		}
		/*
		 * We don't really need the interlock here since there is no
		 * need for serializing access to the buffer - we're the only
		 * consumer. All we want is to sleep until someone gives us
		 * something to so.
		 */
		mutex_enter(&vd->go_draw_il);
		cv_timedwait(&vd->go_draw, &vd->go_draw_il, hz);
		mutex_exit(&vd->go_draw_il);
	}
}
#endif /* VCONS_DRAW_ASYNC */

#ifdef VCONS_DRAW_INTR
static void
vcons_intr(void *cookie)
{
	struct vcons_data *vd = cookie;

	workqueue_enqueue(vd->intr_wq, &vd->wk, NULL);
}

static void
vcons_intr_work(struct work *wk, void *cookie)
{
	struct vcons_data *vd = cookie;
	struct vcons_screen *scr = vd->active;

	if (scr) {
		if (!SCREEN_IS_BUSY(scr) && scr->scr_dirty > 0) {
			if ((scr->scr_flags & VCONS_NO_REDRAW) == 0)
				vcons_redraw_screen(scr);
			scr->scr_dirty = 0;
		}
	}

	callout_schedule(&vd->intr, mstohz(33));
}

static void
vcons_intr_enable(device_t dev)
{
	/* the 'dev' arg we pass to config_interrupts isn't a device_t */
	struct vcons_data *vd = (struct vcons_data *)dev;
	vd->use_intr = 1;
	callout_schedule(&vd->intr, mstohz(33));
}
#endif /* VCONS_DRAW_INTR */
