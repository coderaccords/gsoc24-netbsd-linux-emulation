/*	$NetBSD: fpgetround.c,v 1.2 2005/06/12 05:21:25 lukem Exp $	*/

/*	$OpenBSD: fpgetround.c,v 1.3 2002/10/21 18:41:05 mickey Exp $	*/

/*
 * Written by Miodrag Vallat.  Public domain
 */

#include <sys/cdefs.h>
#if defined(LIBC_SCCS) && !defined(lint)
__RCSID("$NetBSD: fpgetround.c,v 1.2 2005/06/12 05:21:25 lukem Exp $");
#endif /* LIBC_SCCS and not lint */

#include <sys/types.h>
#include <ieeefp.h>

fp_rnd
fpgetround(void)
{
	uint64_t fpsr;

	__asm__ __volatile__("fstd %%fr0,0(%1)" : "=m" (fpsr) : "r" (&fpsr));
	return ((fpsr >> 41) & 0x3);
}
