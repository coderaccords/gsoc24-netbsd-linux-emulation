/* $NetBSD: stubs.c,v 1.4 2003/07/14 22:48:20 lukem Exp $ */
/*
 * stubs.c -- functions I haven't written yet
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: stubs.c,v 1.4 2003/07/14 22:48:20 lukem Exp $");

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/user.h>

#include <uvm/uvm_extern.h>

void
resettodr()
{

	printf("resettodr: not doing anything\n");
}

int
suibyte(base, c)
	void *base;
	int c;
{
	panic("suibyte not implemented");
}

int
susword(base, c)
	void *base;
	short c;
{
	panic("susword not implemented");
}

int
suisword(base, c)
	void *base;
	short c;
{
	panic("suisword not implemented");
}

int
suswintr(base, c)
	void *base;
	short c;
{

	return -1;
}

int
suiword(base, c)
	void *base;
	long c;
{
	panic("suiword not implemented");
}

int
fuibyte(base)
	const void *base;
{
	panic("fuibyte not implemented");
}

int
fusword(base)
	const void *base;
{
	panic("fusword not implemented");
}

int
fuisword(base)
	const void *base;
{
	panic("fuisword not implemented");
}

int
fuswintr(base)
	const void *base;
{

	return -1;
}

long
fuiword(base)
	const void *base;
{
	panic("fuiword not implemented");
}

void
pagemove(foo, bar, len)
	caddr_t foo, bar;
	size_t len;
{
	panic("pagemove not implemented");
}
