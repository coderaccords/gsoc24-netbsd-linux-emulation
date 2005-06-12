/*	$NetBSD: flt_rounds.c,v 1.2 2005/06/12 05:21:25 lukem Exp $	*/

/*	$OpenBSD: flt_rounds.c,v 1.3 2002/10/21 18:41:05 mickey Exp $	*/

/*
 * Written by Miodrag Vallat.  Public domain.
 */

#include <sys/cdefs.h>
#if defined(LIBC_SCCS) && !defined(lint)
__RCSID("$NetBSD: flt_rounds.c,v 1.2 2005/06/12 05:21:25 lukem Exp $");
#endif /* LIBC_SCCS and not lint */

#include <sys/types.h>
#include <machine/float.h>

static const int map[] = {
	1,	/* round to nearest */
	0,	/* round to zero */
	2,	/* round to positive infinity */
	3	/* round to negative infinity */
};

int
__flt_rounds(void)
{
	uint64_t fpsr;

	__asm__ __volatile__("fstd %%fr0,0(%1)" : "=m" (fpsr) : "r" (&fpsr));
	return map[(fpsr >> 41) & 0x03];
}
