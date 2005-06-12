/*	$NetBSD: fpgetsticky.c,v 1.4 2005/06/12 05:21:26 lukem Exp $	*/

/*
 * Written by J.T. Conklin, Apr 11, 1995
 * Public domain.
 */

#include <sys/cdefs.h>
#if defined(LIBC_SCCS) && !defined(lint)
__RCSID("$NetBSD: fpgetsticky.c,v 1.4 2005/06/12 05:21:26 lukem Exp $");
#endif /* LIBC_SCCS and not lint */

#include "namespace.h"

#include <ieeefp.h>

#ifdef __weak_alias
__weak_alias(fpgetsticky,_fpgetsticky)
#endif

fp_except
fpgetsticky()
{
	int x;

	__asm__("cfc1 %0,$31" : "=r" (x));
	return (x >> 2) & 0x1f;
}
