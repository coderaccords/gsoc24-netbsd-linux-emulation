/*	$NetBSD: irix_prctl.c,v 1.14 2002/08/02 23:02:51 manu Exp $ */

/*-
 * Copyright (c) 2001-2002 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Emmanuel Dreyfus.
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
 *	This product includes software developed by the NetBSD
 *	Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: irix_prctl.c,v 1.14 2002/08/02 23:02:51 manu Exp $");

#include <sys/errno.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/signal.h>
#include <sys/signalvar.h>
#include <sys/systm.h>
#include <sys/exec.h>
#include <sys/malloc.h>
#include <sys/pool.h>
#include <sys/lock.h>
#include <sys/filedesc.h>
#include <sys/vnode.h>
#include <sys/resourcevar.h>

#include <uvm/uvm_extern.h>
#include <uvm/uvm_map.h>

#include <machine/regnum.h>
#include <machine/vmparam.h>

#include <compat/svr4/svr4_types.h>

#include <compat/irix/irix_types.h>
#include <compat/irix/irix_exec.h>
#include <compat/irix/irix_prctl.h>
#include <compat/irix/irix_signal.h>
#include <compat/irix/irix_syscallargs.h>

struct irix_sproc_child_args {
	struct proc **isc_proc; 
	void *isc_entry;
	void *isc_arg;
	size_t isc_len;
	int isc_inh;
	struct proc *isc_parent;
	struct irix_share_group *isc_share_group;
}; 
static void irix_sproc_child __P((struct irix_sproc_child_args *));
static int irix_sproc __P((void *, unsigned int, void *, caddr_t, size_t, 
    pid_t, struct proc *, register_t *));

int
irix_sys_prctl(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct irix_sys_prctl_args /* {
		syscallarg(int) option;
		syscallarg(void *) arg1;
	} */ *uap = v;
	int option = SCARG(uap, option);

#ifdef DEBUG_IRIX
	printf("irix_sys_prctl(): option = %d\n", option);
#endif

	switch(option) {
	case IRIX_PR_GETSHMASK:	{	/* Get shared resources */
		struct proc *p2;
		int shmask = 0;
		struct irix_emuldata *ied;

		p2 = pfind((pid_t)SCARG(uap, arg1));

		if (p2 == p || SCARG(uap, arg1) == 0) {
			/* XXX return our own shmask */
			return 0;
		}

		if (p2 == NULL)
			return EINVAL;

		ied = (struct irix_emuldata *)p->p_emuldata;
		if (ied->ied_shareaddr)
			shmask |= IRIX_PR_SADDR;
		if (p->p_fd == p2->p_fd)
			shmask |= IRIX_PR_SFDS;
		if (p->p_cwdi == p2->p_cwdi);
			shmask |= (IRIX_PR_SDIR|IRIX_PR_SUMASK);

		*retval = (register_t)shmask;
		return 0;
		break;
	}
		
	case IRIX_PR_LASTSHEXIT:  	/* "Last sproc exit" */
		/* We no nothing */
		break;

	case IRIX_PR_GETNSHARE: {	/* Number of sproc share group memb.*/
		struct irix_emuldata *ied;
		struct irix_emuldata *iedp;
		struct irix_share_group *isg;
		int count;

		ied = (struct irix_emuldata *)p->p_emuldata;
		if ((isg = ied->ied_share_group) == NULL) {
			*retval = 0;
			return 0;
		}
			
		count = 0;
		(void)lockmgr(&isg->isg_lock, LK_SHARED, NULL);
		LIST_FOREACH(iedp, &isg->isg_head, ied_sglist)
			count++;
		(void)lockmgr(&isg->isg_lock, LK_RELEASE, NULL);

		*retval = count;
		return 0;
		break;
	}

	case IRIX_PR_TERMCHILD: {	/* Get SIGHUP when parent's exit */
		struct irix_emuldata *ied;

		ied = (struct irix_emuldata *)(p->p_emuldata);
		ied->ied_termchild = 1;
		break;
	}

	case IRIX_PR_ISBLOCKED: {	/* Is process blocked? */
		pid_t pid = (pid_t)SCARG(uap, arg1);
		struct irix_emuldata *ied;
		struct proc *target;
		struct pcred *pc;

		if (pid == 0)
			pid = p->p_pid;

		if ((target = pfind(pid)) == NULL)
			return ESRCH;

		if (irix_check_exec(target) == 0)
			return 0;

		pc = p->p_cred;
		if (!(pc->pc_ucred->cr_uid == 0 || \
		    pc->p_ruid == target->p_cred->p_ruid || \
		    pc->pc_ucred->cr_uid == target->p_cred->p_ruid || \
		    pc->p_ruid == target->p_ucred->cr_uid || \
		    pc->pc_ucred->cr_uid == target->p_ucred->cr_uid))
			return EPERM;

		ied = (struct irix_emuldata *)(target->p_emuldata);
		*retval = (ied->ied_procblk_count < 0);
		return 0;
		break;
	}

	default:
		printf("Warning: call to unimplemented prctl() command %d\n",
		    option);
		return EINVAL;
		break;
	}
	return 0;
}


int
irix_sys_pidsprocsp(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct irix_sys_pidsprocsp_args /* {
		syscallarg(void *) entry;
		syscallarg(unsigned) inh;
		syscallarg(void *) arg;
		syscallarg(caddr_t) sp;
		syscallarg(irix_size_t) len;
		syscallarg(irix_pid_t) pid;
	} */ *uap = v;
	
	/* pid is ignored for now */
	printf("Warning: unsupported pid argument to IRIX sproc\n");

	return irix_sproc(SCARG(uap, entry), SCARG(uap, inh), SCARG(uap, arg),
	    SCARG(uap, sp), SCARG(uap, len), SCARG(uap, pid), p, retval);
}

int
irix_sys_sprocsp(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct irix_sys_sprocsp_args /* {
		syscallarg(void *) entry;
		syscallarg(unsigned) inh;
		syscallarg(void *) arg;
		syscallarg(caddr_t) sp;
		syscallarg(irix_size_t) len;
	} */ *uap = v;

	return irix_sproc(SCARG(uap, entry), SCARG(uap, inh), SCARG(uap, arg),
	    SCARG(uap, sp), SCARG(uap, len), 0, p, retval);
}

int
irix_sys_sproc(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct irix_sys_sproc_args /* {
		syscallarg(void *) entry;
		syscallarg(unsigned) inh;
		syscallarg(void *) arg;
	} */ *uap = v;

	return irix_sproc(SCARG(uap, entry), SCARG(uap, inh), SCARG(uap, arg),
	    NULL, p->p_rlimit[RLIMIT_STACK].rlim_cur, 0, p, retval);
}


static int 
irix_sproc(entry, inh, arg, sp, len, pid, p, retval)
	void *entry;
	unsigned int inh;
	void *arg;
	caddr_t sp;
	size_t len;
	pid_t pid;
	struct proc *p;
	register_t *retval;
{
	int bsd_flags = 0;
	struct exec_vmcmd vmc;
	int error;
	struct proc *p2;
	struct irix_sproc_child_args *isc;	
	struct irix_emuldata *ied;
	struct irix_emuldata *iedp;
	struct irix_share_group *isg;
	segsz_t stacksize;

#ifdef DEBUG_IRIX
	printf("irix_sproc(): entry = %p, inh = %x, arg = %p, sp = 0x%08lx, len = 0x%08lx, pid = %d\n", entry, inh, arg, (u_long)sp, (u_long)len, pid);
#endif

	if (len == 0)
		return EINVAL;

	if (inh & IRIX_PR_SFDS)
		bsd_flags |= FORK_SHAREFILES;
	if (inh & (IRIX_PR_SUMASK|IRIX_PR_SDIR)) {
		bsd_flags |= FORK_SHARECWD;
		/* Forget them so that we don't get warning below */
		inh &= ~(IRIX_PR_SUMASK|IRIX_PR_SDIR);
	}
	/* We know how to do PR_SUMASK and PR_SDIR together only */
	if (inh & IRIX_PR_SUMASK)
		printf("Warning: unimplemented IRIX sproc flag PR_SUMASK\n");
	if (inh & IRIX_PR_SDIR)
		printf("Warning: unimplemented IRIX sproc flag PR_SDIR\n");

	/*
	 * If revelant, initialize the share group structure
	 */
	ied = (struct irix_emuldata *)(p->p_emuldata);
	if (ied->ied_share_group == NULL) {
		isg = malloc(sizeof(struct irix_share_group), 
		    M_EMULDATA, M_WAITOK);
		lockinit(&isg->isg_lock, PZERO|PCATCH, "sharegroup", 0, 0);
		isg->isg_refcount = 0;
	
		(void)lockmgr(&isg->isg_lock, LK_EXCLUSIVE, NULL);
		LIST_INIT(&isg->isg_head);
		LIST_INSERT_HEAD(&isg->isg_head, ied, ied_sglist);
		isg->isg_refcount++;
		(void)lockmgr(&isg->isg_lock, LK_RELEASE, NULL);

		ied->ied_share_group = isg;
	}

	/* 
	 * Setting up child stack 
	 */
	if (inh & IRIX_PR_SADDR) {
		if (sp == NULL) {
			/*
			 * All share group  members have vm_maxsaddr set 
			 * to the bottom of the lowest stack in address space,
			 * therefore we map the new stack there.
			 */
			sp = p->p_vmspace->vm_maxsaddr;

			/* Compute new stacks's bottom address */
			sp = (caddr_t)trunc_page((u_long)sp - len);
		}

		/* Now map the new stack */
		bzero(&vmc, sizeof(vmc));
		vmc.ev_addr = trunc_page((u_long)sp);
		vmc.ev_len = round_page(len);
		vmc.ev_prot = UVM_PROT_RWX;
		vmc.ev_flags = UVM_FLAG_COPYONW|UVM_FLAG_FIXED|UVM_FLAG_OVERLAY;
		vmc.ev_proc = vmcmd_map_zero;
#ifdef DEBUG_IRIX
		printf("irix_sproc(): new stack addr=0x%08lx, len=0x%08lx\n", 
		    (u_long)sp, (u_long)len);
#endif
		/* Eventually do map for a whole share group */
		if ((error = irix_sync_saddr_vmcmd(p, &vmc)) != 0)
			return error;

		/* Update stack parameters for the share group members */
		ied = (struct irix_emuldata *)p->p_emuldata;
		stacksize = (p->p_vmspace->vm_minsaddr - sp) / PAGE_SIZE;

		/* Normally it cannot be NULL since we just initialized it */
		if ((isg = ied->ied_share_group) == NULL)
			panic("irix_sproc: NULL ied->ied_share_group");

		(void)lockmgr(&isg->isg_lock, LK_EXCLUSIVE, NULL);
		LIST_FOREACH(iedp, &isg->isg_head, ied_sglist) {
			iedp->ied_p->p_vmspace->vm_maxsaddr = (caddr_t)sp;
			iedp->ied_p->p_vmspace->vm_ssize = stacksize;
		}
		(void)lockmgr(&isg->isg_lock, LK_RELEASE, NULL);
	}

	/*
	 * Arguments for irix_sproc_child()
	 * This will be freed by the child.
	 */
	isc = malloc(sizeof(*isc), M_TEMP, M_WAITOK);
	isc->isc_proc = &p2;
	isc->isc_entry = entry;
	isc->isc_arg = arg;
	isc->isc_len = len;
	isc->isc_inh = inh;
	isc->isc_parent = p;
	isc->isc_share_group = isg;

	if (inh & IRIX_PR_SADDR)
		ied->ied_shareaddr = 1;

	if ((error = fork1(p, bsd_flags, SIGCHLD, (void *)sp, len, 
	    (void *)irix_sproc_child, (void *)isc, retval, &p2)) != 0)
		return error;

	retval[0] = (register_t)p2->p_pid;
	retval[1] = 0;

	return 0;
}

static void
irix_sproc_child(isc)
	struct irix_sproc_child_args *isc;
{
	struct proc *p2 = *isc->isc_proc;
	int inh = isc->isc_inh;
	struct proc *parent = isc->isc_parent;
	struct frame *tf = (struct frame *)p2->p_md.md_regs;
	struct frame *ptf = (struct frame *)parent->p_md.md_regs;
	struct pcred *pc;
	struct plimit *pl;
	struct irix_emuldata *ied;
	struct irix_emuldata *parent_ied;

#ifdef DEBUG_IRIX
	printf("irix_sproc_child()\n");
#endif
	/*
	 * Handle shared VM space. The process private arena is not shared
	 */
	if (inh & IRIX_PR_SADDR) {
		int error;
		vaddr_t dstaddrp;
		vaddr_t vm_min;
		vsize_t vm_len;

		vm_min = vm_map_min(&parent->p_vmspace->vm_map);
		vm_len = vm_map_max(&parent->p_vmspace->vm_map) - vm_min;

		/* Drop the current VM space */
		uvm_unmap(&p2->p_vmspace->vm_map, vm_min, vm_min + vm_len);

		/* Clone the mapping from the parent */
		error = uvm_map_extract(&parent->p_vmspace->vm_map, 
		    vm_min, vm_len, &p2->p_vmspace->vm_map, &dstaddrp, 0);
		if (error != 0) {
			free(isc, M_TEMP);
			killproc(p2, "failed to initialize share group VM");
		}

		/* Unmap the process private arena (shared) */
		uvm_unmap(&p2->p_vmspace->vm_map, (vaddr_t)IRIX_PRDA,
		    (vaddr_t)((u_long)IRIX_PRDA + sizeof(struct irix_prda)));

		/* Remap the process private arena (unshared) */
		error = irix_prda_init(p2);
		if (error != 0) {
			free(isc, M_TEMP);
			killproc(p2, "failed to initialize share group VM");
		}
	}

	/*
	 * Handle shared process UID/GID
	 */
	if (inh & IRIX_PR_SID) {
		pc = p2->p_cred;
		parent->p_cred->p_refcnt++;
		p2->p_cred = parent->p_cred;
		if (--pc->p_refcnt == 0) {
			crfree(pc->pc_ucred);	
			pool_put(&pcred_pool, pc);
		}
	}

	/* 
	 * Handle shared process limits
	 */
	if (inh & IRIX_PR_SULIMIT) {
		pl = p2->p_limit;
		parent->p_limit->p_refcnt++;
		p2->p_limit = parent->p_limit;
		if(--pl->p_refcnt == 0)
			limfree(pl);
	}
		
	/* 
	 * Setup PC to return to the child entry point 
	 */
	tf->f_regs[PC] = (unsigned long)isc->isc_entry;
	tf->f_regs[RA] = 0;

	/* 
	 * Setup child arguments 
	 */
	tf->f_regs[A0] = (unsigned long)isc->isc_arg;
	tf->f_regs[A1] = 0;
	tf->f_regs[A2] = 0;
	tf->f_regs[A3] = 0;
	if (ptf->f_regs[S3] == (unsigned long)isc->isc_len) { 
		tf->f_regs[S0] = ptf->f_regs[S0];
		tf->f_regs[S1] = ptf->f_regs[S1];
		tf->f_regs[S2] = ptf->f_regs[S2];
		tf->f_regs[S3] = ptf->f_regs[S3];
	}

	/*
	 * Join the share group
	 */
	ied = (struct irix_emuldata *)(p2->p_emuldata);
	parent_ied = (struct irix_emuldata *)(parent->p_emuldata);
	ied->ied_share_group = parent_ied->ied_share_group;

	(void)lockmgr(&ied->ied_share_group->isg_lock, LK_EXCLUSIVE, NULL);
	LIST_INSERT_HEAD(&ied->ied_share_group->isg_head, ied, ied_sglist);
	ied->ied_share_group->isg_refcount++;
	(void)lockmgr(&ied->ied_share_group->isg_lock, LK_RELEASE, NULL);

	if (inh & IRIX_PR_SADDR) 
		ied->ied_shareaddr = 1;

	free(isc, M_TEMP);
	/*
	 * Return to userland for a newly created process
	 */
	child_return((void *)p2);
	return;
}

int
irix_sys_procblk(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct irix_sys_procblk_args /* {
		syscallarg(int) cmd;
		syscallarg(pid_t) pid;
		syscallarg(int) count;
	} */ *uap = v;
	int cmd = SCARG(uap, cmd);
	struct irix_emuldata *ied;
	struct irix_emuldata *iedp;
	struct irix_share_group *isg;
	struct proc *target;
	struct pcred *pc;
	int oldcount;
	int error, last_error;
	struct irix_sys_procblk_args cup;

	/* Find the process */
	if ((target = pfind(SCARG(uap, pid))) == NULL)
		return ESRCH;

	/* May we stop it? */
	pc = p->p_cred;
	if (!(pc->pc_ucred->cr_uid == 0 || \
	    pc->p_ruid == target->p_cred->p_ruid || \
	    pc->pc_ucred->cr_uid == target->p_cred->p_ruid || \
	    pc->p_ruid == target->p_ucred->cr_uid || \
	    pc->pc_ucred->cr_uid == target->p_ucred->cr_uid))
		return EPERM;

	/* Is it an IRIX process? */
	if (irix_check_exec(target) == 0)
		return EPERM;

	ied = (struct irix_emuldata *)(target->p_emuldata);
	oldcount = ied->ied_procblk_count;
	
	switch (cmd) {
	case IRIX_PROCBLK_BLOCK:
		ied->ied_procblk_count--;
		break;

	case IRIX_PROCBLK_UNBLOCK:
		ied->ied_procblk_count++;
		break;

	case IRIX_PROCBLK_COUNT:
		if (SCARG(uap, count) > IRIX_PR_MAXBLOCKCNT ||
		    SCARG(uap, count) < IRIX_PR_MINBLOCKCNT)
			return EINVAL;
		ied->ied_procblk_count = SCARG(uap, count);
		break;

	case IRIX_PROCBLK_BLOCKALL:
	case IRIX_PROCBLK_UNBLOCKALL:
	case IRIX_PROCBLK_COUNTALL:
		SCARG(&cup, cmd) = cmd -IRIX_PROCBLK_ONLYONE;
		SCARG(&cup, count) = SCARG(uap, count);
		last_error = 0;
		
		/* 
		 * If the process does not belong to a 
		 * share group, do it just for the process 
		 */
		if ((isg = ied->ied_share_group) == NULL) {
			SCARG(&cup, pid) = SCARG(uap, pid);
			return irix_sys_procblk(p, &cup, retval);
		}

		(void)lockmgr(&isg->isg_lock, LK_SHARED, NULL);
		LIST_FOREACH(iedp, &isg->isg_head, ied_sglist) {
			/* Recall procblk for this process */
			SCARG(&cup, pid) = iedp->ied_p->p_pid;
			error = irix_sys_procblk(iedp->ied_p, &cup, retval);
			if (error != 0) 
				last_error = error;
		}
		(void)lockmgr(&isg->isg_lock, LK_RELEASE, NULL);

		return last_error;
		break;
	default:
		printf("Warning: unimplemented IRIX procblk command %d\n", cmd);
		return EINVAL;
		break;
	}

	/* 
	 * We emulate the process block/unblock using SIGSTOP and SIGCONT
	 * signals. This is not very accurate, since on IRIX theses way
	 * of blocking a process are completely separated.
	 */ 
	if (oldcount >= 0 && ied->ied_procblk_count < 0) /* blocked */
		psignal(target, SIGSTOP);

	if (oldcount < 0 && ied->ied_procblk_count >= 0) /* unblocked */
		psignal(target, SIGCONT);

	return 0;
}

int
irix_prda_init(p)
	struct proc *p;
{
	int error;
	struct exec_vmcmd evc;

	bzero(&evc, sizeof(evc));
	evc.ev_addr = (u_long)IRIX_PRDA;
	evc.ev_len = sizeof(struct irix_prda);
	evc.ev_prot = UVM_PROT_RW;
	evc.ev_proc = *vmcmd_map_zero;
	error = (*evc.ev_proc)(p, &evc);
	return error;
}


int
irix_sync_saddr_syscall(p, v, retval, syscall)
        struct proc *p;
	void *v;
	register_t *retval;
	int (*syscall) __P((struct proc *, void *, register_t *));
{
	struct irix_emuldata *ied;
	struct irix_emuldata *iedp;
	int error; 
	int failures;

	/* 
	 * If we are not in a share group with shared VM, 
	 * just do the syscall for the process and return.
	 */
	ied = (struct irix_emuldata *)p->p_emuldata;
	if (ied->ied_share_group == NULL || ied->ied_shareaddr == 0) {
		error = (*syscall)(ied->ied_p, v, retval);
		return error;
	}

	/*
	 * We are in a share group with shared VM: the operation
	 * will be applied to all members. One failure of some but not
	 * all members, we will destroy the whole group. 
	 */
	lockmgr(&ied->ied_share_group->isg_lock, LK_EXCLUSIVE, NULL);
		
	failures = 0;
	LIST_FOREACH(iedp, &ied->ied_share_group->isg_head, ied_sglist) {
		if (iedp->ied_shareaddr == 1)
			if ((error = (*syscall)(iedp->ied_p, v, retval)) != 0)
				failures++;
	}

	/* Full success: the operation succeed or failed for all members */
	if (failures == 0 || failures == ied->ied_share_group->isg_refcount)
		error = 0;
	/* 
	 * Failure: the operation failed, but not on all members.
	 * This means that we failed to keep the VM in sync. 
	 * We will destroy the whole share group 
	 */
	if (error != 0) {
		LIST_FOREACH(iedp, &ied->ied_share_group->isg_head, ied_sglist)
			killproc(iedp->ied_p, "corrupted share group VM space");
	}

	(void)lockmgr(&ied->ied_share_group->isg_lock, LK_RELEASE, NULL);

	return error;
}

int
irix_sync_saddr_vmcmd(p, evc)
	struct proc *p;
	struct exec_vmcmd *evc;
{

	struct irix_emuldata *ied;
	struct irix_emuldata *iedp;
	int error; 
	void *addr;
	int len;
	int failures;

	/* 
	 * If we are not in a share group with shared VM, 
	 * just do the vmcmd for the process and return.
	 */
	ied = (struct irix_emuldata *)p->p_emuldata;
	if (ied->ied_share_group == NULL || ied->ied_shareaddr == 0) {
		error = (*evc->ev_proc)(p, evc);
		return error;
	}

	/*
	 * We are in a share group with shared VM: the operation
	 * will be applied to all members. One failure of some but not
	 * all members, we will destroy the whole group. 
	 */
	lockmgr(&ied->ied_share_group->isg_lock, LK_EXCLUSIVE, NULL);
		
	/*
	 * We do a shared VM operation.
	 * Check that this vmcmd does not operate on the private arena
	 */
	addr = (void *)evc->ev_addr;
	len = evc->ev_len;
	if ((u_long)addr >= (u_long)IRIX_PRDA &&
	    (u_long)addr + len < (u_long)IRIX_PRDA + sizeof(struct irix_prda))
		printf("Warning: shared vmcmd on process private arena\n");

	failures = 0;
	LIST_FOREACH(iedp, &ied->ied_share_group->isg_head, ied_sglist) {
		if (iedp->ied_shareaddr == 1)
			if ((error = (*evc->ev_proc)(iedp->ied_p, evc)) != 0) 
				failures++;
	}

	/* Full success: the operation succeed or failed for all members */
	if (failures == 0 || failures == ied->ied_share_group->isg_refcount)
		error = 0;
	/* 
	 * Failure: the operation failed, but not on all members.
	 * This means that we failed to keep the VM in sync. 
	 * We will destroy the whole share group 
	 */
	if (error != 0) {
		LIST_FOREACH(iedp, &ied->ied_share_group->isg_head, ied_sglist)
			killproc(iedp->ied_p, "corrupted share group VM space");
	}

	(void)lockmgr(&ied->ied_share_group->isg_lock, LK_RELEASE, NULL);

	return error;
}

