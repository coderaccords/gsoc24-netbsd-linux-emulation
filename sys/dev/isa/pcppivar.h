/* $NetBSD: pcppivar.h,v 1.7 2005/12/11 12:22:03 christos Exp $ */

/*
 * Copyright (c) 1996 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: Chris G. Demetriou
 *
 * Permission to use, copy, modify and distribute this software and
 * its documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND
 * FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */

#ifndef _PCPPIVAR_H_
#define _PCPPIVAR_H_

typedef void *pcppi_tag_t;

struct pcppi_attach_args {
	pcppi_tag_t pa_cookie;
};

struct pcppi_softc {
        struct device sc_dv;  

        bus_space_tag_t sc_iot;
        bus_space_handle_t sc_ppi_ioh;
        struct attimer_softc *sc_timer;
        
        struct callout sc_bell_ch;

        int sc_bellactive, sc_bellpitch;
        int sc_slp;
        int sc_timeout;
};

void pcppi_attach(struct pcppi_softc *);

#define	PCPPI_BELL_SLEEP	0x01	/* synchronous; sleep for complete */
#define	PCPPI_BELL_POLL		0x02	/* synchronous; poll for complete */

void pcppi_bell(pcppi_tag_t, int, int, int);

#endif /* ! _PCPPIVAR_H_ */
