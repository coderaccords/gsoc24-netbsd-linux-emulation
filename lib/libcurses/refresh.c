/*	$NetBSD: refresh.c,v 1.20 2000/04/18 22:47:01 jdc Exp $	*/

/*
 * Copyright (c) 1981, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
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
 */

#include <sys/cdefs.h>
#ifndef lint
#if 0
static char sccsid[] = "@(#)refresh.c	8.7 (Berkeley) 8/13/94";
#else
__RCSID("$NetBSD: refresh.c,v 1.20 2000/04/18 22:47:01 jdc Exp $");
#endif
#endif				/* not lint */

#include <string.h>

#include "curses.h"
#include "curses_private.h"

static int curwin;
static short ly, lx;

static void	domvcur __P((int, int, int, int));
static int	makech __P((WINDOW *, int));
static void	quickch __P((WINDOW *));
static void	scrolln __P((WINDOW *, int, int, int, int, int));
static void	unsetattr __P((int));

#ifndef _CURSES_USE_MACROS

/*
 * refresh --
 *	Make the current screen look like "stdscr" over the area covered by
 *	stdscr.
 */
int
refresh(void)
{
	return wrefresh(stdscr);
}

#endif

/*
 * wrefresh --
 *	Make the current screen look like "win" over the area coverd by
 *	win.
 */
int
wrefresh(WINDOW *win)
{
	__LINE *wlp;
	int	retval;
	short	wy;
	int	dnum;

	/* Check if we need to restart ... */
	if (__endwin) {
		__endwin = 0;
		__restartwin();
	}

	/* Initialize loop parameters. */
	ly = curscr->cury;
	lx = curscr->curx;
	wy = 0;
	curwin = (win == curscr);

	if (!curwin)
		for (wy = 0; wy < win->maxy; wy++) {
			wlp = win->lines[wy];
			if (wlp->flags & __ISDIRTY)
				wlp->hash = __hash((char *)(void *)wlp->line,
				    (int) (win->maxx * __LDATASIZE));
		}

	if ((win->flags & __CLEAROK) || (curscr->flags & __CLEAROK) || curwin) {
		if ((win->flags & __FULLWIN) || curscr->flags & __CLEAROK) {
			if ((!(win->battr & __COLOR) ||
			    ((win->battr & __COLOR) && BE)) &&
			    win->bch == ' ') {
				if (win->battr & __COLOR) {
					if ((win->battr & __COLOR) !=
				    	(curscr->wattr & __COLOR)) {
						__set_color(win->battr);
						curscr->wattr &= ~__COLOR;
						curscr->wattr |= win->battr &
						    __COLOR;
					}
				} else if (curscr->wattr & __COLOR) {
					if (OC != NULL && CC == NULL)
						tputs(OC, 0, __cputchar);
					if (OP != NULL) {
						tputs(OP, 0, __cputchar);
						if (SE != NULL &&
						    !strcmp(OP, SE))
							curscr->wattr &=
							    ~__STANDOUT;
						if (UE != NULL &&
						    !strcmp(OP, UE))
							curscr->wattr &=
							    ~__UNDERSCORE;
						if (ME != NULL &&
						    !strcmp(OP, ME))
							curscr->wattr &=
							    ~__TERMATTR;
					}
					curscr->wattr &= ~__COLOR;
				}
				tputs(CL, 0, __cputchar);
				ly = 0;
				lx = 0;
				if (!curwin) {
					curscr->flags &= ~__CLEAROK;
					curscr->cury = 0;
					curscr->curx = 0;
					werase(curscr);
				}
				__touchwin(win);
			} else {
				if (!curwin)
					curscr->flags &= ~__CLEAROK;
				touchwin(win);
			}
		}
		win->flags &= ~__CLEAROK;
	}
	if (!CA) {
		if (win->curx != 0)
			putchar('\n');
		if (!curwin)
			werase(curscr);
	}
#ifdef DEBUG
	__CTRACE("wrefresh: (%0.2o): curwin = %d\n", win, curwin);
	__CTRACE("wrefresh: \tfirstch\tlastch\n");
#endif

	if ((win->flags & __FULLWIN) && !curwin) {
		/*
		 * Invoke quickch() only if more than a quarter of the lines
		 * in the window are dirty.
		 */
		for (wy = 0, dnum = 0; wy < win->maxy; wy++)
			if (win->lines[wy]->flags & (__ISDIRTY | __FORCEPAINT))
				dnum++;
		if (!__noqch && dnum > (int) win->maxy / 4)
			quickch(win);
	}

#ifdef DEBUG
	{
		int	i, j;

		__CTRACE("#####################################\n");
		for (i = 0; i < curscr->maxy; i++) {
			__CTRACE("C: %d:", i);
			__CTRACE(" 0x%x \n", curscr->lines[i]->hash);
			for (j = 0; j < curscr->maxx; j++)
				__CTRACE("%c", curscr->lines[i]->line[j].ch);
			__CTRACE("\n");
			__CTRACE(" attr:");
			for (j = 0; j < curscr->maxx; j++)
				__CTRACE(" %x", curscr->lines[i]->line[j].attr);
			__CTRACE("\n");
			__CTRACE("W: %d:", i);
			/* Handle small windows */
			if (i >= win->begy && i < (win->begy + win->maxy)) {
				__CTRACE(" 0x%x \n",
				    win->lines[i - win->begy]->hash);
				__CTRACE(" 0x%x ",
				    win->lines[i - win->begy]->flags);
				for (j = 0; j < win->maxx; j++)
					__CTRACE("%c", win->lines[i - win
					    ->begy]->line[j].ch);
				__CTRACE("\n");
				__CTRACE(" attr:");
				for (j = 0; j < win->maxx; j++)
					__CTRACE(" %x", win->lines[i - win
					    ->begy]->line[j].attr);
				__CTRACE("\n");
				__CTRACE(" battr:");
				for (j = 0; j < win->maxx; j++)
					__CTRACE(" %x", win->lines[i - win
					    ->begy]->line[j].battr);
				__CTRACE("\n");
			}
		}
	}
#endif				/* DEBUG */

	for (wy = 0; wy < win->maxy; wy++) {
#ifdef DEBUG
		__CTRACE("wy %d\tf: %d\tl:%d\tflags %x\n",
		    wy, *win->lines[wy]->firstchp, *win->lines[wy]->lastchp,
		    win->lines[wy]->flags);
#endif
		if (!curwin)
			curscr->lines[wy]->hash = win->lines[wy]->hash;
		if (win->lines[wy]->flags & (__ISDIRTY | __FORCEPAINT)) {
			if (makech(win, wy) == ERR)
				return (ERR);
			else {
				if (*win->lines[wy]->firstchp >= win->ch_off)
					*win->lines[wy]->firstchp = win->maxx +
					    win->ch_off;
				if (*win->lines[wy]->lastchp < win->maxx +
				    win->ch_off)
					*win->lines[wy]->lastchp = win->ch_off;
				if (*win->lines[wy]->lastchp <
				    *win->lines[wy]->firstchp) {
#ifdef DEBUG
					__CTRACE("wrefresh: line %d notdirty \n", wy);
#endif
					win->lines[wy]->flags &= ~__ISDIRTY;
				}
			}

		}
#ifdef DEBUG
		__CTRACE("\t%d\t%d\n", *win->lines[wy]->firstchp,
		    *win->lines[wy]->lastchp);
#endif
	}

#ifdef DEBUG
	__CTRACE("refresh: ly=%d, lx=%d\n", ly, lx);
#endif

	if (win == curscr)
		domvcur(ly, lx, (int) win->cury, (int) win->curx);
	else {
		if (win->flags & __LEAVEOK) {
			curscr->cury = ly;
			curscr->curx = lx;
			ly -= win->begy;
			lx -= win->begx;
			if (ly >= 0 && ly < win->maxy && lx >= 0 &&
			    lx < win->maxx) {
				win->cury = ly;
				win->curx = lx;
			} else
				win->cury = win->curx = 0;
		} else {
			domvcur(ly, lx, (int) (win->cury + win->begy),
			    (int) (win->curx + win->begx));
			curscr->cury = win->cury + win->begy;
			curscr->curx = win->curx + win->begx;
		}
	}
	retval = OK;

	(void) fflush(stdout);
	return (retval);
}

/*
 * makech --
 *	Make a change on the screen.
 */
static int
makech(win, wy)
	WINDOW *win;
	int	wy;
{
	static __LDATA blank = {' ', 0};
	__LDATA *nsp, *csp, *cp, *cep;
	u_int	force;
	int	clsp, nlsp;	/* Last space in lines. */
	int	lch, wx, y;
	char	*ce;
	attr_t	lspb;		/* Last space background colour */

#ifdef __GNUC__
	nlsp = 0;		/* XXX gcc -Wuninitialized */
#endif
	/* Is the cursor still on the end of the last line? */
	if (wy > 0 && win->lines[wy - 1]->flags & __ISPASTEOL) {
		domvcur(ly, lx, ly + 1, 0);
		ly++;
		lx = 0;
	}
	wx = *win->lines[wy]->firstchp - win->ch_off;
	if (wx < 0)
		wx = 0;
	else
		if (wx >= win->maxx)
			return (OK);
	lch = *win->lines[wy]->lastchp - win->ch_off;
	if (lch < 0)
		return (OK);
	else
		if (lch >= (int) win->maxx)
			lch = win->maxx - 1;
	y = wy + win->begy;

	if (curwin)
		csp = &blank;
	else
		csp = &curscr->lines[wy + win->begy]->line[wx + win->begx];

	nsp = &win->lines[wy]->line[wx];
	force = win->lines[wy]->flags & __FORCEPAINT;
	win->lines[wy]->flags &= ~__FORCEPAINT;
	/* XXX: Check for background character here */
	if (CE && !curwin) {
		cp = &win->lines[wy]->line[win->maxx - 1];
		if (cp->attr & __COLOR)
			lspb = cp->attr & __COLOR;
		else
			lspb = cp->battr & __COLOR;
		while (cp->ch == ' ' && cp->attr == lspb && cp->battr == lspb)
			if (cp-- <= win->lines[wy]->line)
				break;
		nlsp = cp - win->lines[wy]->line;
	}
	if (!curwin)
		ce = CE;
	else
		ce = NULL;

	if (force) {
		if (CM)
			tputs(tgoto(CM, lx, ly), 0, __cputchar);
		else {
			tputs(HO, 0, __cputchar);
			__mvcur(0, 0, ly, lx, 1);
		}
	}

	while (wx <= lch) {
		if (!force && memcmp(nsp, csp, sizeof(__LDATA)) == 0) {
			if (wx <= lch) {
				while (wx <= lch &&
				    memcmp(nsp, csp, sizeof(__LDATA)) == 0) {
					nsp++;
					if (!curwin)
						++csp;
					++wx;
				}
				continue;
			}
			break;
		}
		domvcur(ly, lx, y, (int) (wx + win->begx));

#ifdef DEBUG
		__CTRACE("makech: 1: wx = %d, ly= %d, lx = %d, newy = %d, newx = %d, force =%d\n",
		    wx, ly, lx, y, wx + win->begx, force);
#endif
		ly = y;
		lx = wx + win->begx;
		while ((force || memcmp(nsp, csp, sizeof(__LDATA)) != 0)
		    && wx <= lch) {

			if (ce != NULL &&
			    win->maxx + win->begx == curscr->maxx &&
			    wx >= nlsp && nsp->ch == ' ' && nsp->attr == 0) {
				/* Check for clear to end-of-line. */
				cep = &curscr->lines[wy]->line[win->maxx - 1];
				while (cep->ch == ' ' && cep->attr == lspb)
					if (cep-- <= csp)
						break;
				clsp = cep - curscr->lines[wy]->line -
				    win->begx * __LDATASIZE;
#ifdef DEBUG
				__CTRACE("makech: clsp = %d, nlsp = %d\n",
				    clsp, nlsp);
#endif
				if (((clsp - nlsp >= strlen(CE) &&
				    clsp < win->maxx * __LDATASIZE) ||
				    wy == win->maxy - 1) &&
				    (!(lspb & __COLOR) ||
				    ((lspb & __COLOR) && BE)) &&
				    win->bch == ' ') {
					unsetattr(0);
					if ((lspb & __COLOR) !=
					    (curscr->wattr & __COLOR)) {
						__set_color(lspb);
						curscr->wattr &= ~__COLOR;
						curscr->wattr |= lspb & __COLOR;
					}
					tputs(CE, 0, __cputchar);
					lx = wx + win->begx;
					while (wx++ <= clsp) {
						csp->ch = ' ';
						csp->attr = 0;
						csp++;
					}
					return (OK);
				}
				ce = NULL;
			}

			/*
			 * Unset colour if appropriate.  Check to see
			 * if we also turn off standout, underscore and
			 * attributes.
			 */
			if (!(nsp->attr & __COLOR) &&
			    !(nsp->battr & __COLOR) &&
			    (curscr->wattr & __COLOR)) {
					if (OC != NULL && CC == NULL)
						tputs(OC, 0, __cputchar);
					if (OP != NULL) {
						tputs(OP, 0, __cputchar);
						if (SE != NULL &&
						    !strcmp(OP, SE))
							curscr->wattr &=
							    ~__STANDOUT;
						if (UE != NULL &&
						    !strcmp(OP, UE))
							curscr->wattr &=
							    ~__UNDERSCORE;
						if (ME != NULL &&
						    !strcmp(OP, ME))
							curscr->wattr &=
							    ~__TERMATTR;
					}
					curscr->wattr &= ~__COLOR;
				}

			/*
			 * Unset attributes as appropriate.  Unset first
			 * so that the relevant attributes can be reset
			 * (because 'me' unsets 'mb', 'md', 'mh', 'mk',
			 * 'mp' and 'mr').  Check to see if we also turn off
			 * standout, attributes and colour.
			 */
			if (((nsp->attr & __TERMATTR) |
			    (nsp->battr & __TERMATTR)) !=
			    (curscr->wattr & __TERMATTR)) {
				tputs(ME, 0, __cputchar);
				curscr->wattr &= ~__TERMATTR;
				if (SE != NULL && !strcmp(ME, SE))
					curscr->wattr &= ~__STANDOUT;
				if (UE != NULL && !strcmp(ME, UE))
					curscr->wattr &= ~__UNDERSCORE;
				if (OP != NULL && !strcmp(ME, OP))
					curscr->wattr &= ~__COLOR;
			}

			/*
			 * Exit underscore mode if appropriate.
			 * Check to see if we also turn off standout,
			 * attributes and colour.
			 */
			if (!(nsp->attr & __UNDERSCORE) &&
			    !(nsp->battr & __UNDERSCORE) &&
			    (curscr->wattr & __UNDERSCORE)) {
				tputs(UE, 0, __cputchar);
				curscr->wattr &= ~__UNDERSCORE;
				if (SE != NULL && !strcmp(UE, SE))
					curscr->wattr &= ~__STANDOUT;
				if (ME != NULL && !strcmp(UE, ME))
					curscr->wattr &= ~__TERMATTR;
				if (OP != NULL && !strcmp(UE, OP))
					curscr->wattr &= ~__COLOR;
			}

			/*
			 * Enter/exit standout mode as appropriate.
			 * Check to see if we also turn off underscore,
			 * attributes and colour.
			 * XXX
			 * Should use UC if SO/SE not available.
			 */
			if ((nsp->attr & __STANDOUT) ||
			    (nsp->battr & __STANDOUT)) {
				if (!(curscr->wattr & __STANDOUT) &&
				    SO != NULL && SE != NULL) {
					tputs(SO, 0, __cputchar);
					curscr->wattr |= __STANDOUT;
				}
			} else {
				if (curscr->wattr & __STANDOUT) {
					tputs(SE, 0, __cputchar);
					curscr->wattr &= ~__STANDOUT;
					if (UE != NULL && !strcmp(SE, UE))
						curscr->wattr &=
						    ~__UNDERSCORE;
					if (ME != NULL && !strcmp(SE, ME))
						curscr->wattr &= ~__TERMATTR;
					if (OP != NULL && !strcmp(SE, OP))
						curscr->wattr &= ~__COLOR;
				}
			}

			/*
			 * Enter underscore mode if appropriate.
			 * XXX
			 * Should use UC if US/UE not available.
			 */
			if (((nsp->attr & __UNDERSCORE) ||
			    (nsp->battr & __UNDERSCORE)) &&
			    !(curscr->wattr & __UNDERSCORE) &&
			    US != NULL && UE != NULL) {
				tputs(US, 0, __cputchar);
				curscr->wattr |= __UNDERSCORE;
			}

			/*
			 * Set other attributes as appropriate.
			 */
			if (((nsp->attr & __BLINK) ||
			    (nsp->battr & __BLINK)) &&
			    !(curscr->wattr & __BLINK) &&
			    MB != NULL && ME != NULL) {
				tputs(MB, 0, __cputchar);
				curscr->wattr |= __BLINK;
			}
			if (((nsp->attr & __BOLD) ||
			    (nsp->battr & __BOLD)) &&
			    !(curscr->wattr & __BOLD) &&
			    MD != NULL && ME != NULL) {
				tputs(MD, 0, __cputchar);
				curscr->wattr |= __BOLD;
			}
			if (((nsp->attr & __DIM) ||
			    (nsp->battr & __DIM)) &&
			    !(curscr->wattr & __DIM) &&
			    MH != NULL && ME != NULL) {
				tputs(MH, 0, __cputchar);
				curscr->wattr |= __DIM;
			}
			if (((nsp->attr & __BLANK) ||
			    (nsp->battr & __BLANK)) &&
			    !(curscr->wattr & __BLANK) &&
			    MK != NULL && ME != NULL) {
				tputs(MK, 0, __cputchar);
				curscr->wattr |= __BLANK;
			}
			if (((nsp->attr & __PROTECT) ||
			    (nsp->battr & __PROTECT)) &&
			    !(curscr->wattr & __PROTECT) &&
			    MP != NULL && ME != NULL) {
				tputs(MP, 0, __cputchar);
				curscr->wattr |= __PROTECT;
			}
			if (((nsp->attr & __REVERSE) ||
			    (nsp->battr & __REVERSE)) &&
			    !(curscr->wattr & __REVERSE) &&
			    MR != NULL && ME != NULL) {
				tputs(MR, 0, __cputchar);
				curscr->wattr |= __REVERSE;
			}

			/* Set/change colour as appropriate. */
			if (((nsp->attr & __COLOR) ||
			    (nsp->battr & __COLOR)) &&
			    cO != NULL && (OC != NULL || OP != NULL)) {
				if (nsp->attr & __COLOR) {
					if ((nsp->attr & __COLOR) !=
					    (curscr->wattr & __COLOR)) {
						__set_color(nsp->attr);
						curscr->wattr &= ~__COLOR;
						curscr->wattr |= nsp->attr &
						    __COLOR;
					}
				} else if (nsp->battr & __COLOR) {
					if ((nsp->battr & __COLOR) !=
					    (curscr->wattr & __COLOR)) {
						__set_color(nsp->battr);
						curscr->wattr &= ~__COLOR;
						curscr->wattr |= nsp->battr &
						    __COLOR;
					}
				}
			}

			/* Enter/exit altcharset mode as appropriate. */
			if ((nsp->attr & __ALTCHARSET) ||
			    (nsp->battr & __ALTCHARSET)) {
				if (!(curscr->wattr & __ALTCHARSET) &&
				    AS != NULL && AE != NULL) {
					tputs(AS, 0, __cputchar);
					curscr->wattr |= __ALTCHARSET;
				}
			} else {
				if (curscr->wattr & __ALTCHARSET) {
					tputs(AE, 0, __cputchar);
					curscr->wattr &= ~__ALTCHARSET;
				}
			}

			wx++;
			if (wx >= win->maxx && wy == win->maxy - 1 && !curwin)
				if (win->flags & __SCROLLOK) {
					if (win->flags & __ENDLINE){
						unsetattr(1);
					}
					if (!(win->flags & __SCROLLWIN)) {
						if (!curwin) {
							csp->attr = nsp->attr |
							    (nsp->battr &
							    ~__COLOR);
							if (!(nsp->attr &
							    __COLOR) &&
							    (nsp->battr &
							    __COLOR))
								csp->attr |=
								    nsp->battr
								    & __COLOR;
							if (nsp->ch == ' ' &&
							    nsp->bch != ' ')
								putchar((int)
								    (csp->ch =
								    nsp->bch));
							else
								putchar((int)
								    (csp->ch =
								    nsp->ch));
						} else
							putchar((int) nsp->ch);
					}
					if (wx + win->begx < curscr->maxx) {
						domvcur(ly, (int) (wx + win->begx),
						    (int) (win->begy + win->maxy - 1),
						    (int) (win->begx + win->maxx - 1));
					}
					ly = win->begy + win->maxy - 1;
					lx = win->begx + win->maxx - 1;
					return (OK);
				}
			if (wx < win->maxx || wy < win->maxy - 1 ||
			    !(win->flags & __SCROLLWIN)) {
				if (!curwin) {
					csp->attr = nsp->attr | (nsp->battr &
					    ~__COLOR);
					if (!(nsp->attr & __COLOR) &&
					    (nsp->battr & __COLOR))
						csp->attr |= nsp->battr &
						    __COLOR;
					if (nsp->ch == ' ' && nsp->bch != ' ')
						putchar((int) (csp->ch =
						    nsp->bch));
					else
						putchar((int) (csp->ch =
						    nsp->ch));
					csp++;
				} else
					putchar((int) nsp->ch);
			}
#ifdef DEBUG
			__CTRACE("makech: putchar(%c)\n", nsp->ch & 0177);
#endif
			if (UC && ((nsp->attr & __STANDOUT) ||
			    (nsp->attr & __UNDERSCORE))) {
				putchar('\b');
				tputs(UC, 0, __cputchar);
			}
			nsp++;
#ifdef DEBUG
			__CTRACE("makech: 2: wx = %d, lx = %d\n", wx, lx);
#endif
		}
		if (lx == wx + win->begx)	/* If no change. */
			break;
		lx = wx + win->begx;
		if (lx >= COLS && AM)
			lx = COLS - 1;
		else
			if (wx >= win->maxx) {
				domvcur(ly, lx, ly, (int) (win->maxx + win->begx - 1));
				lx = win->maxx + win->begx - 1;
			}
#ifdef DEBUG
		__CTRACE("makech: 3: wx = %d, lx = %d\n", wx, lx);
#endif
	}

	/*
	 * Don't leave the screen with attributes set.
	 * XXX Should this be at the end of wrefresh() and not here?
	 */
	unsetattr(0);
	return (OK);
}

/*
 * domvcur --
 *	Do a mvcur, leaving attributes if necessary.
 */
static void
domvcur(oy, ox, ny, nx)
	int	oy, ox, ny, nx;
{
	unsetattr(1);
	__mvcur(oy, ox, ny, nx, 1);
}

/*
 * Quickch() attempts to detect a pattern in the change of the window
 * in order to optimize the change, e.g., scroll n lines as opposed to
 * repainting the screen line by line.
 */

static void
quickch(win)
	WINDOW *win;
{
#define THRESH		(int) win->maxy / 4

	__LINE *clp, *tmp1, *tmp2;
	int	bsize, curs, curw, starts, startw, i, j;
	int	n, target, cur_period, bot, top, sc_region;
	__LDATA buf[1024];
	u_int	blank_hash;

#ifdef __GNUC__
	curs = curw = starts = startw = 0;	/* XXX gcc -Wuninitialized */
#endif
	/*
	 * Find how many lines from the top of the screen are unchanged.
	 */
	for (top = 0; top < win->maxy; top++)
		if (win->lines[top]->flags & __FORCEPAINT ||
		    win->lines[top]->hash != curscr->lines[top]->hash
		    || memcmp(win->lines[top]->line,
			curscr->lines[top]->line,
			(size_t) win->maxx * __LDATASIZE) != 0)
			break;
		else
			win->lines[top]->flags &= ~__ISDIRTY;
	/*
	 * Find how many lines from bottom of screen are unchanged.
	 */
	for (bot = win->maxy - 1; bot >= 0; bot--)
		if (win->lines[bot]->flags & __FORCEPAINT ||
		    win->lines[bot]->hash != curscr->lines[bot]->hash
		    || memcmp(win->lines[bot]->line,
			curscr->lines[bot]->line,
			(size_t) win->maxx * __LDATASIZE) != 0)
			break;
		else
			win->lines[bot]->flags &= ~__ISDIRTY;

#ifdef NO_JERKINESS
	/*
	 * If we have a bottom unchanged region return.  Scrolling the
	 * bottom region up and then back down causes a screen jitter.
	 * This will increase the number of characters sent to the screen
	 * but it looks better.
	 */
	if (bot < win->maxy - 1)
		return;
#endif				/* NO_JERKINESS */

	/*
	 * Search for the largest block of text not changed.
	 * Invariants of the loop:
	 * - Startw is the index of the beginning of the examined block in win.
	 * - Starts is the index of the beginning of the examined block in
	 *    curscr.
	 * - Curs is the index of one past the end of the exmined block in win.
	 * - Curw is the index of one past the end of the exmined block in
	 *   curscr.
	 * - bsize is the current size of the examined block.
	*/
	for (bsize = bot - top; bsize >= THRESH; bsize--) {
		for (startw = top; startw <= bot - bsize; startw++)
			for (starts = top; starts <= bot - bsize;
			    starts++) {
				for (curw = startw, curs = starts;
				    curs < starts + bsize; curw++, curs++)
					if (win->lines[curw]->flags &
					    __FORCEPAINT ||
					    (win->lines[curw]->hash !=
						curscr->lines[curs]->hash ||
						memcmp(win->lines[curw]->line,
						    curscr->lines[curs]->line,
						    (size_t) win->maxx * __LDATASIZE) != 0))
						break;
				if (curs == starts + bsize)
					goto done;
			}
	}
done:
	/* Did not find anything */
	if (bsize < THRESH)
		return;

#ifdef DEBUG
	__CTRACE("quickch:bsize=%d,starts=%d,startw=%d,curw=%d,curs=%d,top=%d,bot=%d\n",
	    bsize, starts, startw, curw, curs, top, bot);
#endif

	/*
	 * Make sure that there is no overlap between the bottom and top
	 * regions and the middle scrolled block.
	 */
	if (bot < curs)
		bot = curs - 1;
	if (top > starts)
		top = starts;

	n = startw - starts;

#ifdef DEBUG
	__CTRACE("#####################################\n");
	for (i = 0; i < curscr->maxy; i++) {
		__CTRACE("C: %d:", i);
		__CTRACE(" 0x%x \n", curscr->lines[i]->hash);
		for (j = 0; j < curscr->maxx; j++)
			__CTRACE("%c", curscr->lines[i]->line[j].ch);
		__CTRACE("\n");
		__CTRACE(" attr:");
		for (j = 0; j < curscr->maxx; j++)
			__CTRACE(" %x", curscr->lines[i]->line[j].attr);
		__CTRACE("\n");
		__CTRACE("W: %d:", i);
		__CTRACE(" 0x%x \n", win->lines[i]->hash);
		__CTRACE(" 0x%x ", win->lines[i]->flags);
		for (j = 0; j < win->maxx; j++)
			__CTRACE("%c", win->lines[i]->line[j].ch);
		__CTRACE("\n");
		__CTRACE(" attr:");
		for (j = 0; j < win->maxx; j++)
			__CTRACE(" %x", win->lines[i]->line[j].attr);
		__CTRACE("\n");
	}
#endif

	/* So we don't have to call __hash() each time */
	for (i = 0; i < win->maxx; i++) {
		buf[i].ch = ' ';
		buf[i].bch = ' ';
		buf[i].attr = 0;
		buf[i].battr = 0;
	}
	blank_hash = __hash((char *)(void *)buf,
	    (int) (win->maxx * __LDATASIZE));

	/*
	 * Perform the rotation to maintain the consistency of curscr.
	 * This is hairy since we are doing an *in place* rotation.
	 * Invariants of the loop:
	 * - I is the index of the current line.
	 * - Target is the index of the target of line i.
	 * - Tmp1 points to current line (i).
	 * - Tmp2 and points to target line (target);
	 * - Cur_period is the index of the end of the current period.
	 *   (see below).
	 *
	 * There are 2 major issues here that make this rotation non-trivial:
	 * 1.  Scrolling in a scrolling region bounded by the top
	 *     and bottom regions determined (whose size is sc_region).
	 * 2.  As a result of the use of the mod function, there may be a
	 *     period introduced, i.e., 2 maps to 4, 4 to 6, n-2 to 0, and
	 *     0 to 2, which then causes all odd lines not to be rotated.
	 *     To remedy this, an index of the end ( = beginning) of the
	 *     current 'period' is kept, cur_period, and when it is reached,
	 *     the next period is started from cur_period + 1 which is
	 *     guaranteed not to have been reached since that would mean that
	 *     all records would have been reached. (think about it...).
	 *
	 * Lines in the rotation can have 3 attributes which are marked on the
	 * line so that curscr is consistent with the visual screen.
	 * 1.  Not dirty -- lines inside the scrolled block, top region or
	 *                  bottom region.
	 * 2.  Blank lines -- lines in the differential of the scrolling
	 *		      region adjacent to top and bot regions
	 *                    depending on scrolling direction.
	 * 3.  Dirty line -- all other lines are marked dirty.
	 */
	sc_region = bot - top + 1;
	i = top;
	tmp1 = curscr->lines[top];
	cur_period = top;
	for (j = top; j <= bot; j++) {
		target = (i - top + n + sc_region) % sc_region + top;
		tmp2 = curscr->lines[target];
		curscr->lines[target] = tmp1;
		/* Mark block as clean and blank out scrolled lines. */
		clp = curscr->lines[target];
#ifdef DEBUG
		__CTRACE("quickch: n=%d startw=%d curw=%d i = %d target=%d ",
		    n, startw, curw, i, target);
#endif
		if ((target >= startw && target < curw) || target < top
		    || target > bot) {
#ifdef DEBUG
			__CTRACE("-- notdirty");
#endif
			win->lines[target]->flags &= ~__ISDIRTY;
		} else
			if ((n > 0 && target >= top && target < top + n) ||
			    (n < 0 && target <= bot && target > bot + n)) {
				if (clp->hash != blank_hash || memcmp(clp->line,
				    buf, (size_t) win->maxx * __LDATASIZE) !=0) {
					(void)memcpy(clp->line,  buf,
					    (size_t) win->maxx * __LDATASIZE);
#ifdef DEBUG
					__CTRACE("-- blanked out: dirty");
#endif
					clp->hash = blank_hash;
					__touchline(win, target, 0, (int) win->maxx - 1, 0);
				} else {
					__touchline(win, target, 0, (int) win->maxx - 1, 0);
#ifdef DEBUG
					__CTRACE(" -- blank line already: dirty");
#endif
				}
			} else {
#ifdef DEBUG
				__CTRACE(" -- dirty");
#endif
				__touchline(win, target, 0, (int) win->maxx - 1, 0);
			}
#ifdef DEBUG
		__CTRACE("\n");
#endif
		if (target == cur_period) {
			i = target + 1;
			tmp1 = curscr->lines[i];
			cur_period = i;
		} else {
			tmp1 = tmp2;
			i = target;
		}
	}
#ifdef DEBUG
	__CTRACE("$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$\n");
	for (i = 0; i < curscr->maxy; i++) {
		__CTRACE("C: %d:", i);
		for (j = 0; j < curscr->maxx; j++)
			__CTRACE("%c", curscr->lines[i]->line[j].ch);
		__CTRACE("\n");
		__CTRACE("W: %d:", i);
		for (j = 0; j < win->maxx; j++)
			__CTRACE("%c", win->lines[i]->line[j].ch);
		__CTRACE("\n");
	}
#endif
	if (n != 0) {
		WINDOW *wp;
		scrolln(win, starts, startw, curs, bot, top);
		/*
		 * Need to repoint any subwindow lines to the rotated
		 * line structured.
		 */
		for (wp = win->nextp; wp != win; wp = wp->nextp)
			__set_subwin(win, wp);
	}
}

/*
 * scrolln --
 *	Scroll n lines, where n is starts - startw.
 */
static void /* ARGSUSED */
scrolln(win, starts, startw, curs, bot, top)
	WINDOW *win;
	int	starts, startw, curs, bot, top;
{
	int	i, oy, ox, n;

	oy = curscr->cury;
	ox = curscr->curx;
	n = starts - startw;

	/*
	 * XXX
	 * The initial tests that set __noqch don't let us reach here unless
	 * we have either CS + HO + SF/sf/SR/sr, or AL + DL.  SF/sf and SR/sr
	 * scrolling can only shift the entire scrolling region, not just a
	 * part of it, which means that the quickch() routine is going to be
	 * sadly disappointed in us if we don't have CS as well.
	 *
	 * If CS, HO and SF/sf are set, can use the scrolling region.  Because
	 * the cursor position after CS is undefined, we need HO which gives us
	 * the ability to move to somewhere without knowledge of the current
	 * location of the cursor.  Still call __mvcur() anyway, to update its
	 * idea of where the cursor is.
	 *
	 * When the scrolling region has been set, the cursor has to be at the
	 * last line of the region to make the scroll happen.
	 *
	 * Doing SF/SR or AL/DL appears faster on the screen than either sf/sr
	 * or al/dl, and, some terminals have AL/DL, sf/sr, and CS, but not
	 * SF/SR.  So, if we're scrolling almost all of the screen, try and use
	 * AL/DL, otherwise use the scrolling region.  The "almost all" is a
	 * shameless hack for vi.
	 */
	/* XXX: check for background colour and bce here */
	if (n > 0) {
		if (CS != NULL && HO != NULL && (SF != NULL ||
		    ((AL == NULL || DL == NULL ||
		    top > 3 || bot + 3 < win->maxy) && sf != NULL))) {
			tputs(__tscroll(CS, top, bot + 1), 0, __cputchar);
			__mvcur(oy, ox, 0, 0, 1);
			tputs(HO, 0, __cputchar);
			__mvcur(0, 0, bot, 0, 1);
			if (SF != NULL)
				tputs(__tscroll(SF, n, 0), 0, __cputchar);
			else
				for (i = 0; i < n; i++)
					tputs(sf, 0, __cputchar);
			tputs(__tscroll(CS, 0, (int) win->maxy), 0, __cputchar);
			__mvcur(bot, 0, 0, 0, 1);
			tputs(HO, 0, __cputchar);
			__mvcur(0, 0, oy, ox, 1);
			return;
		}

		/* Scroll up the block. */
		if (SF != NULL && top == 0) {
			__mvcur(oy, ox, bot, 0, 1);
			tputs(__tscroll(SF, n, 0), 0, __cputchar);
		} else
			if (DL != NULL) {
				__mvcur(oy, ox, top, 0, 1);
				tputs(__tscroll(DL, n, 0), 0, __cputchar);
			} else
				if (dl != NULL) {
					__mvcur(oy, ox, top, 0, 1);
					for (i = 0; i < n; i++)
						tputs(dl, 0, __cputchar);
				} else
					if (sf != NULL && top == 0) {
						__mvcur(oy, ox, bot, 0, 1);
						for (i = 0; i < n; i++)
							tputs(sf, 0, __cputchar);
					} else
						abort();

		/* Push down the bottom region. */
		__mvcur(top, 0, bot - n + 1, 0, 1);
		if (AL != NULL)
			tputs(__tscroll(AL, n, 0), 0, __cputchar);
		else
			if (al != NULL)
				for (i = 0; i < n; i++)
					tputs(al, 0, __cputchar);
			else
				abort();
		__mvcur(bot - n + 1, 0, oy, ox, 1);
	} else {
		/*
		 * !!!
		 * n < 0
		 *
		 * If CS, HO and SR/sr are set, can use the scrolling region.
		 * See the above comments for details.
		 */
		if (CS != NULL && HO != NULL && (SR != NULL ||
		    ((AL == NULL || DL == NULL ||
		    top > 3 || bot + 3 < win->maxy) && sr != NULL))) {
			tputs(__tscroll(CS, top, bot + 1), 0, __cputchar);
			__mvcur(oy, ox, 0, 0, 1);
			tputs(HO, 0, __cputchar);
			__mvcur(0, 0, top, 0, 1);

			if (SR != NULL)
				tputs(__tscroll(SR, -n, 0), 0, __cputchar);
			else
				for (i = n; i < 0; i++)
					tputs(sr, 0, __cputchar);
			tputs(__tscroll(CS, 0, (int) win->maxy), 0, __cputchar);
			__mvcur(top, 0, 0, 0, 1);
			tputs(HO, 0, __cputchar);
			__mvcur(0, 0, oy, ox, 1);
			return;
		}

		/* Preserve the bottom lines. */
		__mvcur(oy, ox, bot + n + 1, 0, 1);
		if (SR != NULL && bot == win->maxy)
			tputs(__tscroll(SR, -n, 0), 0, __cputchar);
		else
			if (DL != NULL)
				tputs(__tscroll(DL, -n, 0), 0, __cputchar);
			else
				if (dl != NULL)
					for (i = n; i < 0; i++)
						tputs(dl, 0, __cputchar);
				else
					if (sr != NULL && bot == win->maxy)
						for (i = n; i < 0; i++)
							tputs(sr, 0, __cputchar);
					else
						abort();

		/* Scroll the block down. */
		__mvcur(bot + n + 1, 0, top, 0, 1);
		if (AL != NULL)
			tputs(__tscroll(AL, -n, 0), 0, __cputchar);
		else
			if (al != NULL)
				for (i = n; i < 0; i++)
					tputs(al, 0, __cputchar);
			else
				abort();
		__mvcur(top, 0, oy, ox, 1);
	}
}

/*
 * unsetattr --
 *	Unset attributes on curscr.  Leave standout, attribute and colour
 *	modes if necessary (!MS).  Always leave altcharset (xterm at least
 *	ignores a cursor move if we don't).
 */
static void /* ARGSUSED */
unsetattr(int checkms)
{
	int	isms;

	if (checkms)
		if (!MS) {
			isms = 1;
		} else {
			isms = 0;
		}
	else
		isms = 1;
#ifdef DEBUG
	__CTRACE("unsetattr: checkms = %d, MS = %s, wattr = %08x\n",
	    checkms, MS ? "TRUE" : "FALSE", curscr->wattr);
#endif
		
	/*
         * Don't leave the screen in standout mode (check against MS).  Check
	 * to see if we also turn off underscore, attributes and colour.
	 */
	if (curscr->wattr & __STANDOUT && isms) {
		tputs(SE, 0, __cputchar);
		curscr->wattr &= ~__STANDOUT;
		if (UE != NULL && !strcmp(SE, UE))
			curscr->wattr &= ~__UNDERSCORE;
		if (ME != NULL && !strcmp(SE, ME))
			curscr->wattr &= ~__TERMATTR;
		if (OP != NULL && !strcmp(SE, OP))
			curscr->wattr &= ~__COLOR;
	}
	/*
	 * Don't leave the screen in underscore mode (check against MS).
	 * Check to see if we also turn off attributes and colour.
	 */
	if (curscr->wattr & __UNDERSCORE && isms) {
		tputs(UE, 0, __cputchar);
		curscr->wattr &= ~__UNDERSCORE;
		if (ME != NULL && !strcmp(UE, ME))
			curscr->wattr &= ~__TERMATTR;
		if (OP != NULL && !strcmp(UE, OP))
			curscr->wattr &= ~__COLOR;
	}
	/*
	 * Don't leave the screen with attributes set (check against MS).
	 * Check to see if we also turn off colour.
	 */
	if (curscr->wattr & __TERMATTR && isms) {
		tputs(ME, 0, __cputchar);
			curscr->wattr &= ~__TERMATTR;
		if (OP != NULL && !strcmp(ME, OP))
			curscr->wattr &= ~__COLOR;
	}
	/* Don't leave the screen with altcharset set (don't check MS). */
	if (curscr->wattr & __ALTCHARSET) {
		tputs(AE, 0, __cputchar);
		curscr->wattr &= ~__ALTCHARSET;
	}
	/* Don't leave the screen with colour set (check against MS). */
	if (curscr->wattr & __COLOR && isms) {
		if (OC != NULL && CC == NULL)
			tputs(OC, 0, __cputchar);
		if (OP != NULL)
			tputs(OP, 0, __cputchar);
		curscr->wattr &= ~__COLOR;
	}
}
