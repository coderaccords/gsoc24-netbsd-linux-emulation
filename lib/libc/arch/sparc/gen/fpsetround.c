/*	$NetBSD: fpsetround.c,v 1.4 2005/06/12 05:21:26 lukem Exp $	*/

/*
 * Written by J.T. Conklin, Apr 10, 1995
 * Public domain.
 */

#include <sys/cdefs.h>
#if defined(LIBC_SCCS) && !defined(lint)
__RCSID("$NetBSD: fpsetround.c,v 1.4 2005/06/12 05:21:26 lukem Exp $");
#endif /* LIBC_SCCS and not lint */

#include "namespace.h"

#include <ieeefp.h>

#ifdef __weak_alias
__weak_alias(fpsetround,_fpsetround)
#endif

fp_rnd
fpsetround(rnd_dir)
	fp_rnd rnd_dir;
{
	fp_rnd old;
	fp_rnd new;

	__asm__("st %%fsr,%0" : "=m" (*&old));

	new = old;
	new &= ~(0x03 << 30); 
	new |= ((rnd_dir & 0x03) << 30);

	__asm__("ld %0,%%fsr" : : "m" (*&new));

	return (old >> 30) & 0x03;
}
