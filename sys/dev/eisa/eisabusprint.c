
#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: eisabusprint.c,v 1.3 2005/12/11 12:21:20 christos Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <dev/eisa/eisavar.h>

int
eisabusprint(void *vea, const char *pnp)
{
#if 0
	struct eisabus_attach_args *ea = vea;
#endif
	if (pnp)
		aprint_normal("eisa at %s", pnp);
	return (UNCONF);
}
