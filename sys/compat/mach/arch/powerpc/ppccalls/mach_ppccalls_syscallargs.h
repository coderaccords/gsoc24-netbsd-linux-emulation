/* $NetBSD: mach_ppccalls_syscallargs.h,v 1.6 2005/12/11 12:20:22 christos Exp $ */

/*
 * System call argument lists.
 *
 * DO NOT EDIT-- this file is automatically generated.
 * created from	NetBSD: syscalls.master,v 1.3 2005/02/26 23:10:21 perry Exp
 */

#ifndef _MACH_PPCCALLS_SYS__SYSCALLARGS_H_
#define	_MACH_PPCCALLS_SYS__SYSCALLARGS_H_

#ifdef	syscallarg
#undef	syscallarg
#endif

#define	syscallarg(x)							\
	union {								\
		register_t pad;						\
		struct { x datum; } le;					\
		struct { /* LINTED zero array dimension */		\
			int8_t pad[  /* CONSTCOND */			\
				(sizeof (register_t) < sizeof (x))	\
				? 0					\
				: sizeof (register_t) - sizeof (x)];	\
			x datum;					\
		} be;							\
	}

/*
 * System call prototypes.
 */

#endif /* _MACH_PPCCALLS_SYS__SYSCALLARGS_H_ */
