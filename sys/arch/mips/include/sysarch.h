/*	$NetBSD: sysarch.h,v 1.5 2002/07/19 03:13:55 simonb Exp $ */

#ifndef _MIPS_SYSARCH_H_
#define _MIPS_SYSARCH_H_

/*
 * Architecture specific syscalls (mips)
 */
#define MIPS_CACHEFLUSH	0
#define MIPS_CACHECTL	1

struct mips_cacheflush_args {
	vaddr_t va;
	int nbytes;
	int whichcache;
};

struct mips_cachectl_args {
	vaddr_t va;
	int nbytes;
	int ctl;
};

#ifndef _KERNEL
int sysarch(int, void *);
#endif /* !_KERNEL */
#endif /* !_MIPS_SYSARCH_H_ */
