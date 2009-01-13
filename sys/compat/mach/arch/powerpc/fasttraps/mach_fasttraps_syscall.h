/* $NetBSD: mach_fasttraps_syscall.h,v 1.13 2009/01/13 22:33:11 pooka Exp $ */

/*
 * System call numbers.
 *
 * DO NOT EDIT-- this file is automatically generated.
 * created from	NetBSD: syscalls.master,v 1.8 2009/01/13 22:27:43 pooka Exp
 */

#ifndef _MACH_FASTTRAPS_SYS_SYSCALL_H_
#define	_MACH_FASTTRAPS_SYS_SYSCALL_H_

#define	MACH_FASTTRAPS_SYS_MAXSYSARGS	8

/* syscall: "cthread_set_self" ret: "void" args: "mach_cproc_t" */
#define	MACH_FASTTRAPS_SYS_cthread_set_self	1

/* syscall: "cthread_self" ret: "mach_cproc_t" args: */
#define	MACH_FASTTRAPS_SYS_cthread_self	2

/* syscall: "processor_facilities_used" ret: "int" args: */
#define	MACH_FASTTRAPS_SYS_processor_facilities_used	3

/* syscall: "load_msr" ret: "void" args: */
#define	MACH_FASTTRAPS_SYS_load_msr	4

#define	MACH_FASTTRAPS_SYS_MAXSYSCALL	16
#define	MACH_FASTTRAPS_SYS_NSYSENT	16
#endif /* _MACH_FASTTRAPS_SYS_SYSCALL_H_ */
