/*	$NetBSD: cacheops.h,v 1.11 2005/12/11 12:17:53 christos Exp $	*/

/*-
 * Copyright (c) 1997 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Leo Weppelman
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

#ifndef _M68K_CACHEOPS_H_
#define	_M68K_CACHEOPS_H_

#if notyet /* XXX */
#include <machine/cpuconf.h>
#endif

#include <m68k/cacheops_20.h>
#include <m68k/cacheops_30.h>
#include <m68k/cacheops_40.h>
#include <m68k/cacheops_60.h>

#define M68K_CACHEOPS_NTYPES (defined(M68K_CACHEOPS_MACHDEP) + \
	defined(M68020) + defined(M68030) + defined(M68040) + defined(M68060))

#if M68K_CACHEOPS_NTYPES == 1

#if defined(M68020)

#define	DCIA()		DCIA_20()
#define	DCIAS(pa)	DCIAS_20((pa))
#define	DCIS()		DCIS_20()
#define	DCIU()		DCIU_20()
#define	ICIA()		ICIA_20()
#define	ICPA()		ICPA_20()
#define	PCIA()		PCIA_20()
#define	TBIA()		TBIA_20()
#define	TBIAS()		TBIAS_20()
#define	TBIAU()		TBIAU_20()
#define	TBIS(va)	TBIS_20((va))

#elif defined(M68030)

#define	DCIA()		DCIA_30()
#define	DCIAS(pa)	DCIAS_30((pa))
#define	DCIS()		DCIS_30()
#define	DCIU()		DCIU_30()
#define	ICIA()		ICIA_30()
#define	ICPA()		ICPA_30()
#define	PCIA()		PCIA_30()
#define	TBIA()		TBIA_30()
#define	TBIAS()		TBIAS_30()
#define	TBIAU()		TBIAU_30()
#define	TBIS(va)	TBIS_30((va))

#elif defined(M68040)

#define	DCFA()		DCFA_40()
#define	DCFL(pa)	DCFL_40((pa))
#define	DCFP(pa)	DCFP_40((pa))
#define	DCIA()		DCIA_40()
#define	DCIAS(pa)	DCIAS_40((pa))
#define	DCIS()		DCIS_40()
#define	DCIU()		DCIU_40()
#define	DCPA()		DCPA_40()
#define	DCPL(pa)	DCPL_40((pa))
#define	DCPP(pa)	DCPP_40((pa))
#define	ICIA()		ICIA_40()
#define	ICPA()		ICPA_40()
#define	ICPL(pa)	ICPL_40((pa))
#define	ICPP(pa)	ICPP_40((pa))
#define	PCIA()		PCIA_40()
#define	TBIA()		TBIA_40()
#define	TBIAS()		TBIAS_40()
#define	TBIAU()		TBIAU_40()
#define	TBIS(va)	TBIS_40((va))

#elif defined(M68060)

#define	DCFA()		DCFA_60()
#define	DCFL(pa)	DCFL_60((pa))
#define	DCFP(pa)	DCFP_60((pa))
#define	DCIA()		DCIA_60()
#define	DCIAS(pa)	DCIAS_60((pa))
#define	DCIS()		DCIS_60()
#define	DCIU()		DCIU_60()
#define	DCPA()		DCPA_60()
#define	DCPL(pa)	DCPL_60((pa))
#define	DCPP(pa)	DCPP_60((pa))
#define	ICIA()		ICIA_60()
#define	ICPA()		ICPA_60()
#define	ICPL(pa)	ICPL_60((pa))
#define	ICPP(pa)	ICPP_60((pa))
#define	PCIA()		PCIA_60()
#define	TBIA()		TBIA_60()
#define	TBIAS()		TBIAS_60()
#define	TBIAU()		TBIAU_60()
#define	TBIS(va)	TBIS_60((va))

#endif

#else /* M68K_CACHEOPS_NTYPES == 1 */

#define	DCFA()		_DCFA()
#define	DCFL(pa)	_DCFL((pa))
#define	DCFP(pa)	_DCFP((pa))
#define	DCIA()		_DCIA()
#define	DCIAS(pa)	_DCIAS((pa))
#define	DCIS()		_DCIS()
#define	DCIU()		_DCIU()
#define	DCPA()		_DCPA()
#define	DCPL(pa)	_DCPL((pa))
#define	DCPP(pa)	_DCPP((pa))
#define	ICIA()		_ICIA()
#define	ICPA()		_ICPA()
#define	ICPL(pa)	_ICPL((pa))
#define	ICPP(pa)	_ICPP((pa))
#define	PCIA()		_PCIA()
#define	TBIA()		_TBIA()
#define	TBIAS()		_TBIAS()
#define	TBIAU()		_TBIAU()
#define	TBIS(va)	_TBIS((va))

#endif /* M68K_CACHEOPS_NTYPES == 1 */

void	_DCFA(void);
void	_DCFL(paddr_t);
void	_DCFP(paddr_t);
void	_DCIA(void);
void	_DCIAS(paddr_t);
void	_DCIS(void);
void	_DCIU(void);
void	_DCPA(void);
void	_DCPL(paddr_t);
void	_DCPP(paddr_t);
void	_ICIA(void);
void	_ICPA(void);
void	_ICPL(paddr_t);
void	_ICPP(paddr_t);
void	_PCIA(void);
void	_TBIA(void);
void	_TBIAS(void);
void	_TBIAU(void);
void	_TBIS(vaddr_t);

#endif /* _M68K_CACHEOPS_H_ */
