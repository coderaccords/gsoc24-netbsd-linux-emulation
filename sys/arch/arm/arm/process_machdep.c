/*	$NetBSD: process_machdep.c,v 1.17 2005/12/11 12:16:41 christos Exp $	*/

/*
 * Copyright (c) 1993 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Jan-Simon Pendry.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * From:
 *	Id: procfs_i386.c,v 4.1 1993/12/17 10:47:45 jsp Rel
 */

/*
 * Copyright (c) 1995 Frank Lancaster.  All rights reserved.
 * Copyright (c) 1995 Tools GmbH.  All rights reserved.
 * All rights reserved.
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
 *      This product includes software developed by TooLs GmbH.
 * 4. The name of TooLs GmbH may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY TOOLS GMBH ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL TOOLS GMBH BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Copyright (c) 1995 Charles M. Hannum.  All rights reserved.
 * Copyright (c) 1993 Jan-Simon Pendry
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Jan-Simon Pendry.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * From:
 *	Id: procfs_i386.c,v 4.1 1993/12/17 10:47:45 jsp Rel
 */

/*
 * This file may seem a bit stylized, but that so that it's easier to port.
 * Functions to be implemented here are:
 *
 * process_read_regs(proc, regs)
 *	Get the current user-visible register set from the process
 *	and copy it into the regs structure (<machine/reg.h>).
 *	The process is stopped at the time read_regs is called.
 *
 * process_write_regs(proc, regs)
 *	Update the current register set from the passed in regs
 *	structure.  Take care to avoid clobbering special CPU
 *	registers or privileged bits in the PSL.
 *	The process is stopped at the time write_regs is called.
 *
 * process_sstep(proc, sstep)
 *	Arrange for the process to trap or not trap depending on sstep
 *	after executing a single instruction.
 *
 * process_set_pc(proc)
 *	Set the process's program counter.
 */

#include "opt_armfpe.h"

#include <sys/param.h>

__KERNEL_RCSID(0, "$NetBSD: process_machdep.c,v 1.17 2005/12/11 12:16:41 christos Exp $");

#include <sys/proc.h>
#include <sys/ptrace.h>
#include <sys/systm.h>
#include <sys/user.h>

#include <machine/frame.h>
#include <machine/pcb.h>
#include <machine/reg.h>

#include <arm/armreg.h>

#ifdef ARMFPE
#include <arm/fpe-arm/armfpe.h>
#endif

int
process_read_regs(struct lwp *l, struct reg *regs)
{
	struct trapframe *tf = process_frame(l);

	KASSERT(tf != NULL);
	bcopy((caddr_t)&tf->tf_r0, (caddr_t)regs->r, sizeof(regs->r));
	regs->r_sp = tf->tf_usr_sp;
	regs->r_lr = tf->tf_usr_lr;
	regs->r_pc = tf->tf_pc;
	regs->r_cpsr = tf->tf_spsr;

#ifdef THUMB_CODE
	if (tf->tf_spsr & PSR_T_bit)
		regs->r_pc |= 1;
#endif
#ifdef DIAGNOSTIC
	if ((tf->tf_spsr & PSR_MODE) == PSR_USR32_MODE
	    && tf->tf_spsr & I32_bit)
		panic("process_read_regs: Interrupts blocked in user process");
#endif

	return(0);
}

int
process_read_fpregs(struct lwp *l, struct fpreg *regs)
{
#ifdef ARMFPE
	arm_fpe_getcontext(p, regs);
	return(0);
#else	/* ARMFPE */
	/* No hardware FP support */
	memset(regs, 0, sizeof(struct fpreg));
	return(0);
#endif	/* ARMFPE */
}

int
process_write_regs(struct lwp *l, const struct reg *regs)
{
	struct trapframe *tf = process_frame(l);

	KASSERT(tf != NULL);
	bcopy(regs->r, &tf->tf_r0, sizeof(regs->r));
	tf->tf_usr_sp = regs->r_sp;
	tf->tf_usr_lr = regs->r_lr;
#ifdef __PROG32
	tf->tf_pc = regs->r_pc;
	tf->tf_spsr &=  ~(PSR_FLAGS | PSR_T_bit);
	tf->tf_spsr |= regs->r_cpsr & PSR_FLAGS;
#ifdef THUMB_CODE
	if ((regs->r_pc & 1) || (regs->r_cpsr & PSR_T_bit))
		tf->tf_spsr |= PSR_T_bit;
#endif
#ifdef DIAGNOSTIC
	if ((tf->tf_spsr & PSR_MODE) == PSR_USR32_MODE
	    && tf->tf_spsr & I32_bit)
		panic("process_write_regs: Interrupts blocked in user process");
#endif
#else /* __PROG26 */
	if ((regs->r_pc & (R15_MODE | R15_IRQ_DISABLE | R15_FIQ_DISABLE)) != 0)
		return EPERM;

	tf->tf_r15 = regs->r_pc;
#endif

	return(0);
}

int
process_write_fpregs(struct lwp *l, const struct fpreg *regs)
{
#ifdef ARMFPE
	arm_fpe_setcontext(p, regs);
	return(0);
#else	/* ARMFPE */
	/* No hardware FP support */
	return(0);
#endif	/* ARMFPE */
}

int
process_set_pc(struct lwp *l, caddr_t addr)
{
	struct trapframe *tf = process_frame(l);

	KASSERT(tf != NULL);
#ifdef __PROG32
	tf->tf_pc = (int)addr;
#ifdef THUMB_CODE
	if (((int)addr) & 1)
		tf->tf_spsr |= PSR_T_bit;
	else
		tf->tf_spsr &= ~PSR_T_bit;
#endif
#else /* __PROG26 */
	/* Only set the PC, not the PSR */
	if (((register_t)addr & R15_PC) != (register_t)addr)
		return EINVAL;
	tf->tf_r15 = (tf->tf_r15 & ~R15_PC) | (register_t)addr;
#endif

	return (0);
}
