/*	$NetBSD: kern_sysctl.c,v 1.155 2003/12/28 22:36:37 atatat Exp $	*/

/*-
 * Copyright (c) 2003 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Andrew Brown.
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
 *      This product includes software developed by the NetBSD
 *      Foundation, Inc. and its contributors.
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

/*-
 * Copyright (c) 1982, 1986, 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Mike Karels at Berkeley Software Design, Inc.
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
 *	@(#)kern_sysctl.c	8.9 (Berkeley) 5/20/95
 */

/*
 * sysctl system call.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: kern_sysctl.c,v 1.155 2003/12/28 22:36:37 atatat Exp $");

#include "opt_defcorename.h"
#include "opt_insecure.h"
#include "ksyms.h"

#include <sys/param.h>
#include <sys/sysctl.h>
#include <sys/systm.h>
#include <sys/buf.h>
#include <sys/ksyms.h>
#include <sys/malloc.h>
#include <sys/mount.h>
#include <sys/sa.h>
#include <sys/syscallargs.h>
#include <machine/stdarg.h>

MALLOC_DEFINE(M_SYSCTLNODE, "sysctlnode", "sysctl node structures");
MALLOC_DEFINE(M_SYSCTLDATA, "sysctldata", "misc sysctl data");

static int sysctl_mmap(SYSCTLFN_RWPROTO);
static int sysctl_alloc(struct sysctlnode *, int);
static int sysctl_realloc(struct sysctlnode *);

/*
 * the "root" of the new sysctl tree
 */
static struct sysctlnode sysctl_root = {
	.sysctl_flags = SYSCTL_ROOT|
	    SYSCTL_READWRITE|
	    CTLTYPE_NODE,
	.sysctl_num = 0,
	.sysctl_size = sizeof(struct sysctlnode),
	.sysctl_name = "(root)",
};

/*
 * link set of functions that add nodes at boot time (see also
 * sysctl_buildtree())
 */
__link_set_decl(sysctl_funcs, sysctl_setup_func);

/*
 * The `sysctl_lock' is intended to serialize access to the sysctl
 * tree.  Given that it is now (a) dynamic, and (b) most consumers of
 * sysctl are going to be copying data out, the old `sysctl_memlock'
 * has been `upgraded' to simply guard the whole tree.
 *
 * The two new data here are to keep track of the locked chunk of
 * memory, if there is one, so that it can be released more easily
 * from anywhere.
 */
struct lock sysctl_treelock;
caddr_t sysctl_memaddr;
size_t sysctl_memsize;

/*
 * Attributes stored in the kernel.
 */
char hostname[MAXHOSTNAMELEN];
int hostnamelen;

char domainname[MAXHOSTNAMELEN];
int domainnamelen;

long hostid;

#ifdef INSECURE
int securelevel = -1;
#else
int securelevel = 0;
#endif

#ifndef DEFCORENAME
#define	DEFCORENAME	"%n.core"
#endif
char defcorename[MAXPATHLEN] = DEFCORENAME;

/*
 * ********************************************************************
 * Section 0: Some simple glue
 * ********************************************************************
 * By wrapping copyin(), copyout(), and copyinstr() like this, we can
 * stop caring about who's calling us and simplify some code a bunch.
 * ********************************************************************
 */
static inline int
sysctl_copyin(const struct lwp *l, const void *uaddr, void *kaddr, size_t len)
{

	if (l != NULL)
		return (copyin(uaddr, kaddr, len));

	memcpy(kaddr, uaddr, len);

	return (0);
}

static inline int
sysctl_copyout(const struct lwp *l, const void *kaddr, void *uaddr, size_t len)
{

	if (l != NULL)
		return (copyout(kaddr, uaddr, len));

	memcpy(uaddr, kaddr, len);

	return (0);
}

static inline int
sysctl_copyinstr(const struct lwp *l, const void *uaddr, void *kaddr,
		 size_t len, size_t *done)
{

	if (l != NULL)
		return (copyinstr(uaddr, kaddr, len, done));
	else
		return (copystr(uaddr, kaddr, len, done));
}

/*
 * ********************************************************************
 * Initialize sysctl subsystem.
 * ********************************************************************
 */
void
sysctl_init(void)
{
	sysctl_setup_func **sysctl_setup, f;

	lockinit(&sysctl_treelock, PRIBIO|PCATCH, "sysctl", 0, 0);

	/*
	 * dynamic mib numbers start here
	 */
	sysctl_root.sysctl_num = CREATE_BASE;

        __link_set_foreach(sysctl_setup, sysctl_funcs) {
		/*
		 * XXX - why do i have to coerce the pointers like this?
		 */
		f = (void*)*sysctl_setup;
		(*f)();
	}

	/*
	 * setting this means no more permanent nodes can be added,
	 * trees that claim to be readonly at the root now are, and if
	 * the main tree is readonly, *everything* is.
	 */
	sysctl_root.sysctl_flags |= SYSCTL_PERMANENT;

}

/*
 * ********************************************************************
 * The main native sysctl system call itself.
 * ********************************************************************
 */
int
sys___sysctl(struct lwp *l, void *v, register_t *retval)
{
	struct sys___sysctl_args /* {
		syscallarg(int *) name;
		syscallarg(u_int) namelen;
		syscallarg(void *) old;
		syscallarg(size_t *) oldlenp;
		syscallarg(void *) new;
		syscallarg(size_t) newlen;
	} */ *uap = v;
	int error, nerror, name[CTL_MAXNAME];
	size_t oldlen, savelen, *oldlenp;

	/*
	 * get oldlen
	 */
	oldlen = 0;
	oldlenp = SCARG(uap, oldlenp);
	if (oldlenp != NULL) {
		error = copyin(oldlenp, &oldlen, sizeof(oldlen));
		if (error)
			return (error);
	}
	savelen = oldlen;

	/*
	 * top-level sysctl names may or may not be non-terminal, but
	 * we don't care
	 */
	if (SCARG(uap, namelen) > CTL_MAXNAME || SCARG(uap, namelen) < 1)
		return (EINVAL);
	error = copyin(SCARG(uap, name), &name,
		       SCARG(uap, namelen) * sizeof(int));
	if (error)
		return (error);

	/*
	 * wire old so that copyout() is less likely to fail?
	 */
	error = sysctl_lock(l, SCARG(uap, old), savelen);
	if (error)
		return (error);

	/*
	 * do sysctl work (NULL means main built-in default tree)
	 */
	error = sysctl_dispatch(&name[0], SCARG(uap, namelen),
				SCARG(uap, old), &oldlen,
				SCARG(uap, new), SCARG(uap, newlen),
				&name[0], l, NULL);

	/*
	 * release the sysctl lock
	 */
	sysctl_unlock(l);

	/*
	 * set caller's oldlen to new value even in the face of an
	 * error (if this gets an error and they didn't have one, they
	 * get this one)
	 */
	if (oldlenp) {
		nerror = copyout(&oldlen, oldlenp, sizeof(oldlen));
		if (error == 0)
			error = nerror;
	}

	/*
	 * if the only problem is that we weren't given enough space,
	 * that's an ENOMEM error
	 */
	if (error == 0 && SCARG(uap, old) != NULL && savelen < oldlen)
		error = ENOMEM;
	
	return (error);
}

/*
 * ********************************************************************
 * Section 1: How the tree is used
 * ********************************************************************
 * Implementations of sysctl for emulations should typically need only
 * these three functions in this order: lock the tree, dispatch
 * request into it, unlock the tree.
 * ********************************************************************
 */
int
sysctl_lock(struct lwp *l, void *oldp, size_t savelen)
{
	int error = 0;

	error = lockmgr(&sysctl_treelock, LK_EXCLUSIVE, NULL);
	if (error)
		return (error);

	if (l != NULL && oldp != NULL && savelen) {
		error = uvm_vslock(l->l_proc, oldp, savelen, VM_PROT_WRITE);
		if (error) {
			(void) lockmgr(&sysctl_treelock, LK_RELEASE, NULL);
			return (error);
		}
		sysctl_memaddr = oldp;
		sysctl_memsize = savelen;
	}

	return (0);
}

/*
 * ********************************************************************
 * the main sysctl dispatch routine.  scans the given tree and picks a
 * function to call based on what it finds.
 * ********************************************************************
 */
int
sysctl_dispatch(SYSCTLFN_RWARGS)
{
	int error;
	sysctlfn fn;
	int ni;

	fn = NULL;
	error = sysctl_locate(l, name, namelen, &rnode, &ni);

	/*
	 * the node we ended up at has a function, so call it.  it can
	 * hand off to query or create if it wants to.
	 */
	if (rnode->sysctl_func != NULL)
		fn = rnode->sysctl_func;

	/*
	 * we found the node they were looking for, so do a lookup.
	 */
	else if (error == 0)
		fn = (sysctlfn)sysctl_lookup; /* XXX may write to rnode */

	/*
	 * prospective parent node found, but the terminal node was
	 * not.  generic operations associate with the parent.
	 */
	else if (error == ENOENT && (ni + 1) == namelen && name[ni] < 0) {
		switch (name[ni]) {
		case CTL_QUERY:
			fn = sysctl_query;
			break;
		case CTL_CREATE:
#if NKSYMS > 0
		case CTL_CREATESYM:
#endif /* NKSYMS > 0 */
			fn = (sysctlfn)sysctl_create; /* we own the rnode */
			break;
		case CTL_DESTROY:
			fn = (sysctlfn)sysctl_destroy; /* we own the rnode */
			break;
		case CTL_MMAP:
			fn = (sysctlfn)sysctl_mmap; /* we own the rnode */
			break;
		default:
			error = EOPNOTSUPP;
			break;
		}
	}

	/*
	 * after all of that, maybe we found someone who knows how to
	 * get us what we want?
	 */
	if (fn != NULL)
		error = (*fn)(name + ni, namelen - ni, oldp, oldlenp,
			      newp, newlen, name, l, rnode);

	else if (error == 0)
		error = EOPNOTSUPP;

	return (error);
}

/*
 * ********************************************************************
 * Releases the tree lock.  Note that if uvm_vslock() was called when
 * the lock was taken, we release that memory now.  By keeping track
 * of where and how much by ourselves, the lock can be released much
 * more easily from anywhere.
 * ********************************************************************
 */
void
sysctl_unlock(struct lwp *l)
{

	if (l != NULL && sysctl_memsize != 0) {
		uvm_vsunlock(l->l_proc, sysctl_memaddr, sysctl_memsize);
		sysctl_memsize = 0;
	}

	(void) lockmgr(&sysctl_treelock, LK_RELEASE, NULL);
}

/*
 * ********************************************************************
 * Section 2: The main tree interfaces
 * ********************************************************************
 * This is how sysctl_dispatch() does its work, and you can too, by
 * calling these routines from helpers (though typically only
 * sysctl_lookup() will be used).  The tree MUST BE LOCKED when these
 * are called.
 * ********************************************************************
 */

/*
 * sysctl_locate -- Finds the node matching the given mib under the
 * given tree (via rv).  If no tree is given, we fall back to the
 * native tree.  The current process (via l) is used for access
 * control on the tree (some nodes may be traverable only by root) and
 * on return, nip will show how many numbers in the mib were consumed.
 */
int
sysctl_locate(struct lwp *l, const int *name, u_int namelen,
	      struct sysctlnode **rv, int *nip)
{
	struct sysctlnode *node, *pnode;
	int tn, si, ni, error, alias;

	/*
	 * basic checks and setup
	 */
	if (*rv == NULL)
		*rv = &sysctl_root;
	if (nip)
		*nip = 0;
	if (namelen < 0)
		return (EINVAL);
	if (namelen == 0)
		return (0);

	/*
	 * search starts from "root"
	 */
	pnode = *rv;
	node = pnode->sysctl_child;
	error = 0;

	/*
	 * scan for node to which new node should be attached
	 */
	for (ni = 0; ni < namelen; ni++) {
		/*
		 * walked off bottom of tree
		 */
		if (node == NULL) {
			if (SYSCTL_TYPE(pnode->sysctl_flags) == CTLTYPE_NODE)
				error = ENOENT;
			else
				error = ENOTDIR;
			break;
		}
		/*
		 * can anyone traverse this node or only root?
		 */
		if (l != NULL && (pnode->sysctl_flags & SYSCTL_PRIVATE) &&
		    (error = suser(l->l_proc->p_ucred, &l->l_proc->p_acflag))
		    != 0)
			return (error);
		/*
		 * find a child node with the right number
		 */
		tn = name[ni];
		alias = 0;
		for (si = 0; si < pnode->sysctl_clen; si++) {
			if (node[si].sysctl_num == tn ||
			    (tn >= 0 &&
			     node[si].sysctl_flags & SYSCTL_ANYNUMBER)) {
				if (node[si].sysctl_flags & SYSCTL_ALIAS) {
					if (alias++ == 4)
						si = pnode->sysctl_clen - 1;
					else {
						tn = node[si].sysctl_alias;
						si = -1;
					}
				}
				else
					break;
			}
		}
		/*
		 * if we ran off the end, it obviously doesn't exist
		 */
		if (si == pnode->sysctl_clen) {
			error = ENOENT;
			break;
		}
		/*
		 * so far so good, move on down the line
		 */
		pnode = &node[si];
		if (SYSCTL_TYPE(pnode->sysctl_flags) == CTLTYPE_NODE)
			node = node[si].sysctl_child;
		else
			node = NULL;
	}

	*rv = pnode;
	if (nip)
		*nip = ni;

	return (error);
}

/*
 * sysctl_query -- The auto-discovery engine.  Copies out the
 * descriptions on nodes under the given node and handles overlay
 * trees.
 */
int
sysctl_query(SYSCTLFN_ARGS)
{
	int error, ni, elim;
	size_t out, left, t;
	struct sysctlnode *enode, *onode;

	if (newp != NULL)
		return (EPERM);
	if (SYSCTL_TYPE(rnode->sysctl_flags) != CTLTYPE_NODE)
		return (ENOTDIR);
	if (namelen != 1 || name[0] != CTL_QUERY)
		return (EINVAL);

	error = 0;
	out = 0;
	left = *oldlenp;
	elim = 0;
	enode = NULL;

	/*
	 * process has overlay tree
	 */
	if (l && l->l_proc->p_emul->e_sysctlovly) {
		enode = (void*)l->l_proc->p_emul->e_sysctlovly;
		elim = (name - oname);
		error = sysctl_locate(l, oname, elim, &enode, NULL);
		if (error == 0) {
			/* ah, found parent in overlay */
			elim = enode->sysctl_clen;
			enode = enode->sysctl_child;
		}
		else {
			error = 0;
			elim = 0;
			enode = NULL;
		}
	}

	for (ni = 0; ni < rnode->sysctl_clen; ni++) {
		t = MIN(left, sizeof(struct sysctlnode));
		onode = &rnode->sysctl_child[ni];
		if (enode && enode->sysctl_num == onode->sysctl_num) {
			if (SYSCTL_TYPE(enode->sysctl_flags) !=
			    CTLTYPE_NODE)
				onode = enode;
			if (--elim > 0)
				enode++;
			else
				enode = NULL;
		}
		if (oldp != NULL && t > 0)
			error = sysctl_copyout(l, onode, (char*)oldp + out, t);
		if (error)
			return (error);
		out += sizeof(struct sysctlnode);
		left -= t;
	}

	/*
	 * overlay trees *MUST* be entirely consumed
	 */
	KASSERT(enode == NULL);

	*oldlenp = out;

	return (error);
}

#ifdef SYSCTL_DEBUG_CREATE
#undef sysctl_create
#endif /* SYSCTL_DEBUG_CREATE */

/*
 * sysctl_create -- Adds a node (the description of which is taken
 * from newp) to the tree, returning a copy of it in the space pointed
 * to by oldp.  In the event that the requested slot is already taken
 * (either by name or by number), the offending node is returned
 * instead.  Yes, this is complex, but we want to make sure everything
 * is proper.
 */
int
sysctl_create(SYSCTLFN_RWARGS)
{
	struct sysctlnode nnode, *node, *pnode;
	int error, ni, at, nm, type, sz, flags, rw, anum;
	void *own;

	error = 0;
	own = NULL;
	anum = -1;

	if (namelen != 1 || (name[namelen - 1] != CTL_CREATE
#if NKSYMS > 0
			     && name[namelen - 1] != CTL_CREATESYM
#endif /* NKSYMS > 0 */
			     ))
		return (EINVAL);

	/*
	 * processes can only add nodes at securelevel 0, must be
	 * root, and can't add nodes to a parent that's not writeable
	 */
	if (l != NULL) {
		if (securelevel > 0)
			return (EPERM);
		error = suser(l->l_proc->p_ucred, &l->l_proc->p_acflag);
		if (error)
			return (error);
#ifndef SYSCTL_DISALLOW_CREATE
		if (!(rnode->sysctl_flags & SYSCTL_READWRITE))
#endif /* SYSCTL_DISALLOW_CREATE */
			return (EPERM);
	}

	/*
	 * nothing can add a node if:
	 * we've finished initial set up and
	 * the tree itself is not writeable or
	 * the entire sysctl system is not writeable
	 */
	if ((sysctl_root.sysctl_flags & SYSCTL_PERMANENT) &&
	    (!(sysctl_rootof(rnode)->sysctl_flags & SYSCTL_READWRITE) ||
	     !(sysctl_root.sysctl_flags & SYSCTL_READWRITE)))
		return (EPERM);

	/*
	 * it must be a "node", not a "int" or something
	 */
	if (SYSCTL_TYPE(rnode->sysctl_flags) != CTLTYPE_NODE)
		return (ENOTDIR);
	pnode = rnode;

	if (newp == NULL || newlen != sizeof(struct sysctlnode))
		return (EINVAL);
	error = sysctl_copyin(l, newp, &nnode, sizeof(struct sysctlnode));
	if (error)
		return (error);

	/*
	 * nodes passed in don't *have* parents
	 */
	if (nnode.sysctl_parent != NULL)
		return (EINVAL);

	/*
	 * if we are indeed adding it, it should be a "good" name and
	 * number
	 */
	nm = nnode.sysctl_num;
#if NKSYMS > 0
	if (nm == CTL_CREATESYM)
		nm = CTL_CREATE;
#endif /* NKSYMS > 0 */
	if (nm < 0 && nm != CTL_CREATE)
		return (EINVAL);
	sz = 0;

	/*
	 * the name can't start with a digit
	 */
	if (nnode.sysctl_name[sz] >= '0' &&
	    nnode.sysctl_name[sz] <= '9')
		return (EINVAL);

	/*
	 * the name must be only alphanumerics or - or _, longer than
	 * 0 bytes and less that SYSCTL_NAMELEN
	 */
	while (sz < SYSCTL_NAMELEN && nnode.sysctl_name[sz] != '\0') {
		if ((nnode.sysctl_name[sz] >= '0' &&
		     nnode.sysctl_name[sz] <= '9') ||
		    (nnode.sysctl_name[sz] >= 'A' &&
		     nnode.sysctl_name[sz] <= 'Z') ||
		    (nnode.sysctl_name[sz] >= 'a' &&
		     nnode.sysctl_name[sz] <= 'z') ||
		    nnode.sysctl_name[sz] == '-' ||
		    nnode.sysctl_name[sz] == '_')
			sz++;
		else
			return (EINVAL);
	}
	if (sz == 0 || sz == SYSCTL_NAMELEN)
		return (EINVAL);

	/*
	 * various checks revolve around size vs type, etc
	 */
	type = SYSCTL_TYPE(nnode.sysctl_flags);
	flags = SYSCTL_FLAGS(nnode.sysctl_flags);
	rw = (flags & SYSCTL_READWRITE) ? B_WRITE : B_READ;
	sz = nnode.sysctl_size;

	/*
	 * find out if there's a collision, and if so, let the caller
	 * know what they collided with
	 */
	node = pnode->sysctl_child;
	if (((flags & SYSCTL_ANYNUMBER) && node) ||
	    (node && node->sysctl_flags & SYSCTL_ANYNUMBER))
		return (EINVAL);
	for (ni = at = 0; ni < pnode->sysctl_clen; ni++) {
		if (nm == node[ni].sysctl_num ||
		    strcmp(nnode.sysctl_name, node[ni].sysctl_name) == 0) {
			if (oldp != NULL) {
				/*
				 * ignore error here, since we
				 * are already fixed on EEXIST
				 */
				(void)sysctl_copyout(l, &node[ni], oldp,
				     MIN(*oldlenp, sizeof(struct sysctlnode)));
			}
			*oldlenp = sizeof(struct sysctlnode);
			return (EEXIST);
		}
		if (nm > node[ni].sysctl_num)
			at++;
	}

	/*
	 * use sysctl_ver to add to the tree iff it hasn't changed
	 */
	if (nnode.sysctl_ver != 0) {
		/*
		 * a specified value must match either the parent
		 * node's version or the root node's version
		 */
		if (nnode.sysctl_ver != sysctl_rootof(rnode)->sysctl_ver &&
		    nnode.sysctl_ver != rnode->sysctl_ver) {
			return (EINVAL);
		}
	}

	/*
	 * only the kernel can assign functions to entries
	 */
	if (l != NULL && nnode.sysctl_func != NULL)
		return (EPERM);

	/*
	 * only the kernel can create permanent entries, and only then
	 * before the kernel is finished setting itself up
	 */
	if (l != NULL && (flags & ~SYSCTL_USERFLAGS))
		return (EPERM);
	if ((flags & SYSCTL_PERMANENT) &
	    (sysctl_root.sysctl_flags & SYSCTL_PERMANENT))
		return (EPERM);
	if ((flags & (SYSCTL_OWNDATA | SYSCTL_IMMEDIATE)) == 
	    (SYSCTL_OWNDATA | SYSCTL_IMMEDIATE))
		return (EINVAL);
	if ((flags & SYSCTL_IMMEDIATE) &&
	    type != CTLTYPE_INT && type != CTLTYPE_QUAD)
		return (EINVAL);

	/*
	 * check size, or set it if unset and we can figure it out.
	 * kernel created nodes are allowed to have a function instead
	 * of a size (or a data pointer).
	 */
	switch (type) {
	case CTLTYPE_NODE:
		/*
		 * only *i* can assert the size of a node
		 */
		if (flags & SYSCTL_ALIAS) {
			anum = nnode.sysctl_alias;
			if (anum < 0)
				return (EINVAL);
			nnode.sysctl_alias = 0;
		}
		if (sz != 0 || nnode.sysctl_data != NULL)
			return (EINVAL);
		if (nnode.sysctl_csize != 0 ||
		    nnode.sysctl_clen != 0 ||
		    nnode.sysctl_child != 0)
			return (EINVAL);
		if (flags & SYSCTL_OWNDATA)
			return (EINVAL);
		sz = sizeof(struct sysctlnode);
		break;
	case CTLTYPE_INT:
		/*
		 * since an int is an int, if the size is not given or
		 * is wrong, we can "int-uit" it.
		 */
		if (sz != 0 && sz != sizeof(int))
			return (EINVAL);
		sz = sizeof(int);
		break;
	case CTLTYPE_STRING:
		/*
		 * strings are a little more tricky
		 */
		if (sz == 0) {
			if (l == NULL) {
				if (nnode.sysctl_func == NULL) {
					if (nnode.sysctl_data == NULL)
						return (EINVAL);
					else
						sz = strlen(nnode.sysctl_data) +
						    1;
				}
			}
			else if (nnode.sysctl_data == NULL &&
				 flags & SYSCTL_OWNDATA) {
				return (EINVAL);
			}
			else {
				char *v, *e;
				size_t s;

				/*
				 * arbitrary limit here...
				 */
				e = NULL; /* XXX: gcc on NetBSD/sparc */
				for (s = PAGE_SIZE, v = nnode.sysctl_data;
				     s < 32 * PAGE_SIZE;
				     s += PAGE_SIZE, v += PAGE_SIZE) {
/*
 * XXX @@@ the use of uvm_kernacc() can generate false negatives on
 * some ports, so this needs to be refined shortly.
 */
					if (!uvm_kernacc(v, PAGE_SIZE, rw))
						return (EFAULT);
					e = memchr(v, '\0', PAGE_SIZE);
					if (e != NULL)
						break;
				}
				if (s >= 32 * PAGE_SIZE)
					return (ERANGE);
				sz = e - ((char*)nnode.sysctl_data) + 1;
			}
		}
		break;
	case CTLTYPE_QUAD:
		if (sz != 0 && sz != sizeof(u_quad_t))
			return (EINVAL);
		sz = sizeof(u_quad_t);
		break;
	case CTLTYPE_STRUCT:
		if (sz == 0) {
			if (l != NULL || nnode.sysctl_func == NULL)
				return (EINVAL);
			if (flags & SYSCTL_OWNDATA)
				return (EINVAL);
		}
		break;
	default:
		return (EINVAL);
	}

	/*
	 * at this point, if sz is zero, we *must* have a
	 * function to go with it and we can't own it.
	 */

	/*
	 *  l  ptr own
	 *  0   0   0  -> EINVAL (if no func)
	 *  0   0   1  -> own
	 *  0   1   0  -> kptr
	 *  0   1   1  -> kptr
	 *  1   0   0  -> EINVAL
	 *  1   0   1  -> own
	 *  1   1   0  -> kptr, no own (check via uvm_kernacc)
	 *  1   1   1  -> uptr, own
	 */
	if (type != CTLTYPE_NODE) {
		if (sz != 0) {
			if (flags & SYSCTL_OWNDATA) {
				own = malloc(sz, M_SYSCTLDATA,
					     M_WAITOK|M_CANFAIL);
				if (nnode.sysctl_data == NULL)
					memset(own, 0, sz);
				else {
					error = sysctl_copyin(l,
					    nnode.sysctl_data, own, sz);
					if (error != 0) {
						FREE(own, M_SYSCTLDATA);
						return (error);
					}
				}
			}
			else if ((nnode.sysctl_data != NULL) &&
				 !(flags & SYSCTL_IMMEDIATE)) {
#if NKSYMS > 0
				if (name[namelen - 1] == CTL_CREATESYM) {
					char symname[128]; /* XXX enough? */
					u_long symaddr;
					size_t symlen;

					error = sysctl_copyinstr(l,
					    nnode.sysctl_data, symname,
					    sizeof(symname), &symlen);
					if (error)
						return (error);
					error = ksyms_getval_from_kernel(NULL,
					    symname, &symaddr, KSYMS_EXTERN);
					if (error)
						return (error); /* EINVAL? */
					nnode.sysctl_data = (void*)symaddr;
				}
#endif /* NKSYMS > 0 */
				if (!uvm_kernacc(nnode.sysctl_data, sz, rw)) {
#ifdef HAVE_SOLUTION_TO_UVM_KERNACC_PROBLEM
					/* XXX @@@ what is fix? */
					return (EFAULT);
#else /* HAVE_SOLUTION_TO_UVM_KERNACC_PROBLEM */
/*
 * XXX @@@ the use of uvm_kernacc() can generate false negatives on
 * some ports, so this needs to be refined shortly.  by checking here
 * to see if SYSCTL_PERMANENT is set in the root, we can differentiate
 * between nodes being created from sysctl_init() during bootstrap and
 * "other nodes", so we can at least allow the bootstrap to succeed by
 * simply "trusting" the kernel not to shoot itself in the foot right
 * from the start.
 */
					if ((sysctl_root.sysctl_flags &
					    SYSCTL_PERMANENT)) {
						printf("fault 2 %p %lu %d\n", nnode.sysctl_data, (unsigned long)sz, rw);
						return (EFAULT);
					}
#endif /* HAVE_SOLUTION_TO_UVM_KERNACC_PROBLEM */
				}
			}
		}
		else if (nnode.sysctl_func == NULL)
			return (EINVAL);
	}

	/*
	 * a process can't assign a function to a node, and the kernel
	 * can't create a node that has no function or data.
	 * (XXX somewhat redundant check)
	 */
	if (l != NULL || nnode.sysctl_func == NULL) {
		if (type != CTLTYPE_NODE &&
		    nnode.sysctl_data == NULL &&
		    !(flags & SYSCTL_IMMEDIATE) &&
		    own == NULL)
			return (EINVAL);
	}

#ifdef SYSCTL_DISALLOW_KWRITE
	/*
	 * a process can't create a writable node unless it refers to
	 * new data.
	 */
	if (l != NULL && own == NULL && type != CTLTYPE_NODE &&
	    (flags & SYSCTL_READWRITE) != SYSCTL_READONLY &&
	    !(flags & SYSCTL_IMMEDIATE))
		return (EPERM);
#endif /* SYSCTL_DISALLOW_KWRITE */

	/*
	 * make sure there's somewhere to put the new stuff.
	 */
	if (pnode->sysctl_child == NULL) {
		if (flags & SYSCTL_ANYNUMBER)
			error = sysctl_alloc(pnode, 1);
		else
			error = sysctl_alloc(pnode, 0);
		if (error)
			return (error);
	}
	node = pnode->sysctl_child;

	/*
	 * no collisions, so pick a good dynamic number if we need to.
	 */
	if (nm == CTL_CREATE) {
		nm = ++sysctl_root.sysctl_num;
		for (ni = 0; ni < pnode->sysctl_clen; ni++) {
			if (nm == node[ni].sysctl_num) {
				nm++;
				ni = -1;
			}
			else if (nm > node[ni].sysctl_num)
				at = ni + 1;
		}
	}

	/*
	 * oops...ran out of space
	 */
	if (pnode->sysctl_clen == pnode->sysctl_csize) {
		error = sysctl_realloc(pnode);
		if (error)
			return (error);
		node = pnode->sysctl_child;
	}

	/*
	 * insert new node data
	 */
	if (at < pnode->sysctl_clen) {
		int t;
		
		/*
		 * move the nodes that should come after the new one
		 */
		memmove(&node[at + 1], &node[at],
			(pnode->sysctl_clen - at) * sizeof(struct sysctlnode));
		memset(&node[at], 0, sizeof(struct sysctlnode));
		node[at].sysctl_parent = pnode;
		/*
		 * and...reparent any children of any moved nodes
		 */
		for (ni = at; ni <= pnode->sysctl_clen; ni++)
			if (SYSCTL_TYPE(node[ni].sysctl_flags) == CTLTYPE_NODE)
				for (t = 0; t < node[ni].sysctl_clen; t++)
					node[ni].sysctl_child[t].sysctl_parent =
						&node[ni];
	}
	node = &node[at];
	pnode->sysctl_clen++;

	strlcpy(node->sysctl_name, nnode.sysctl_name,
		sizeof(node->sysctl_name));
	node->sysctl_num = nm;
	node->sysctl_size = sz;
	node->sysctl_flags = type|flags;
	node->sysctl_csize = 0;
	node->sysctl_clen = 0;
	if (own) {
		node->sysctl_data = own;
		node->sysctl_flags |= SYSCTL_OWNDATA;
	}
	else if (flags & SYSCTL_ALIAS) {
		node->sysctl_alias = anum;
	}
	else if (flags & SYSCTL_IMMEDIATE) {
		switch (type) {
		case CTLTYPE_INT:
			node->sysctl_idata = nnode.sysctl_idata;
			break;
		case CTLTYPE_QUAD:
			node->sysctl_qdata = nnode.sysctl_qdata;
			break;
		}
	}
	else {
		node->sysctl_data = nnode.sysctl_data;
		node->sysctl_flags &= ~SYSCTL_OWNDATA;
	}
        node->sysctl_func = nnode.sysctl_func;
        node->sysctl_child = NULL;
	/* node->sysctl_parent should already be done */

	/*
	 * update "version" on path to "root"
	 */
	for (; rnode->sysctl_parent != NULL; rnode = rnode->sysctl_parent)
		;
	pnode = node;
	for (nm = rnode->sysctl_ver + 1; pnode != NULL;
	     pnode = pnode->sysctl_parent)
		pnode->sysctl_ver = nm;

	if (oldp != NULL)
		error = sysctl_copyout(l, node, oldp,
		    MIN(*oldlenp, sizeof(struct sysctlnode)));
	*oldlenp = sizeof(struct sysctlnode);

	return (error);
}

/*
 * ********************************************************************
 * A wrapper around sysctl_create() that prints the thing we're trying
 * to add.
 * ********************************************************************
 */
#ifdef SYSCTL_DEBUG_CREATE
int _sysctl_create(SYSCTLFN_RWPROTO);
int
_sysctl_create(SYSCTLFN_RWARGS)
{
	const struct sysctlnode *node;
	int k, rc, ni, nl = namelen + (name - oname);

	node = newp;

	printf("namelen %d (", nl);
	for (ni = 0; ni < nl - 1; ni++)
		printf(" %d", oname[ni]);
	printf(" %d )\t[%s]\tflags %08x (%08x %d %zu)\n",
	       k = node->sysctl_num,
	       node->sysctl_name,
	       node->sysctl_flags,
	       SYSCTL_FLAGS(node->sysctl_flags),
	       SYSCTL_TYPE(node->sysctl_flags),
	       node->sysctl_size);

	node = rnode;
	rc = sysctl_create(SYSCTLFN_CALL(rnode));

	printf("sysctl_create(");
	for (ni = 0; ni < nl - 1; ni++)
		printf(" %d", oname[ni]);
	printf(" %d ) returned %d\n", k, rc);

	return (rc);
}
#define sysctl_create _sysctl_create
#endif /* SYSCTL_DEBUG_CREATE */

/*
 * sysctl_destroy -- Removes a node (as described by newp) from the
 * given tree, returning (if successful) a copy of the dead node in
 * oldp.  Since we're removing stuff, there's not much to check.
 */
int
sysctl_destroy(SYSCTLFN_RWARGS)
{
	struct sysctlnode *node, *pnode, onode, nnode;
	int ni, error;

	error = 0;

	if (namelen != 1 || name[namelen - 1] != CTL_DESTROY)
		return (EINVAL);

	/*
	 * processes can only destroy nodes at securelevel 0, must be
	 * root, and can't remove nodes from a parent that's not
	 * writeable
	 */
	if (l != NULL) {
		if (securelevel > 0)
			return (EPERM);
		error = suser(l->l_proc->p_ucred, &l->l_proc->p_acflag);
		if (error)
			return (error);
		if (!(rnode->sysctl_flags & SYSCTL_READWRITE))
			return (EPERM);
	}

	/*
	 * nothing can remove a node if:
	 * the node is permanent (checked later) or
	 * the tree itself is not writeable or
	 * the entire sysctl system is not writeable
	 */
	if (!(sysctl_rootof(rnode)->sysctl_flags & SYSCTL_READWRITE) ||
	    !(sysctl_root.sysctl_flags & SYSCTL_READWRITE))
		return (EPERM);

	if (newp == NULL || newlen != sizeof(struct sysctlnode))
		return (EINVAL);
	error = sysctl_copyin(l, newp, &nnode, sizeof(struct sysctlnode));
	if (error)
		return (error);
	memset(&onode, 0, sizeof(struct sysctlnode));

	node = rnode->sysctl_child;
	for (ni = 0; ni < rnode->sysctl_clen; ni++) {
		if (nnode.sysctl_num == node[ni].sysctl_num) {
			/*
			 * if name specified, must match
			 */
			if (nnode.sysctl_name[0] != '\0' &&
			    strcmp(nnode.sysctl_name, node[ni].sysctl_name))
				continue;
			/*
			 * if version specified, must match
			 */
			if (nnode.sysctl_ver != 0 &&
			    nnode.sysctl_ver != node[ni].sysctl_ver)
				continue;
			/*
			 * this must be the one
			 */
			break;
		}
	}
	if (ni == rnode->sysctl_clen)
		return (ENOENT);
	node = &node[ni];
	pnode = node->sysctl_parent;

	/*
	 * if the kernel says permanent, it is, so there.  nyah.
	 */
	if (SYSCTL_FLAGS(node->sysctl_flags) & SYSCTL_PERMANENT)
		return (EPERM);

	/*
	 * can't delete non-empty nodes
	 */
	if (SYSCTL_TYPE(node->sysctl_flags) == CTLTYPE_NODE &&
	    node->sysctl_clen != 0)
		return (ENOTEMPTY);

	/*
	 * if the node "owns" data, release it now
	 */
	if (node->sysctl_flags & SYSCTL_OWNDATA) {
		if (node->sysctl_data != NULL)
			FREE(node->sysctl_data, M_SYSCTLDATA);
		node->sysctl_data = NULL;
	}

	/*
	 * if the node to be removed is not the last one on the list,
	 * move the remaining nodes up, and reparent any grandchildren
	 */
	onode = *node;
	if (ni < pnode->sysctl_clen - 1) {
		int t;

		memmove(&pnode->sysctl_child[ni], &pnode->sysctl_child[ni + 1],
			(pnode->sysctl_clen - ni - 1) *
			sizeof(struct sysctlnode));
		for (; ni < pnode->sysctl_clen - 1; ni++)
			if (SYSCTL_TYPE(pnode->sysctl_child[ni].sysctl_flags) ==
			    CTLTYPE_NODE)
				for (t = 0; t < pnode->sysctl_child[ni].sysctl_clen;
				     t++)
					pnode->sysctl_child[ni].sysctl_child[t].
						sysctl_parent =
						&pnode->sysctl_child[ni];
		ni = pnode->sysctl_clen - 1;
		node = &pnode->sysctl_child[ni];
	}

	/*
	 * reset the space we just vacated
	 */
	memset(node, 0, sizeof(struct sysctlnode));
	node->sysctl_parent = pnode;
	pnode->sysctl_clen--;

	/*
	 * if this parent just lost its last child, nuke the creche
	 */
	if (pnode->sysctl_clen == 0) {
		FREE(pnode->sysctl_child, M_SYSCTLNODE);
		pnode->sysctl_csize = 0;
		pnode->sysctl_child = NULL;
	}

	/*
	 * update "version" on path to "root"
	 */
        for (; rnode->sysctl_parent != NULL; rnode = rnode->sysctl_parent)
                ;
	for (ni = rnode->sysctl_ver + 1; pnode != NULL;
	     pnode = pnode->sysctl_parent)
		pnode->sysctl_ver = ni;

	if (oldp != NULL)
		error = sysctl_copyout(l, &onode, oldp,
		    MIN(*oldlenp, sizeof(struct sysctlnode)));
	*oldlenp = sizeof(struct sysctlnode);

	return (error);
}

/*
 * sysctl_lookup -- Handles copyin/copyout of new and old values.
 * Partial reads are globally allowed.  Only root can write to things
 * unless the node says otherwise.
 */
int
sysctl_lookup(SYSCTLFN_RWARGS)
{
	struct proc *p = l->l_proc;
	int error, rw;
	size_t sz, len;
	void *d;

	error = 0;

	/*
	 * you can't "look up" a node.  you can "query" it, but you
	 * can't "look it up".
	 */
	if (SYSCTL_TYPE(rnode->sysctl_flags) == CTLTYPE_NODE || namelen != 0)
		return (EINVAL);

	/*
	 * some nodes are private, so only root can look into them.
	 */
	if ((rnode->sysctl_flags & SYSCTL_PRIVATE) &&
	    (error = suser(p->p_ucred, &p->p_acflag)) != 0)
		return (error);

	/*
	 * if a node wants to be writable according to different rules
	 * other than "only root can write to stuff unless a flag is
	 * set", then it needs its own function which should have been
	 * called and not us.
	 */
	if (l != NULL && newp != NULL &&
	    !(rnode->sysctl_flags & SYSCTL_ANYWRITE) &&
	    (error = suser(l->l_proc->p_ucred, &l->l_proc->p_acflag)) != 0)
		return (error);

	/*
	 * is this node supposedly writable?
	 */
	rw = 0;
	switch (rnode->sysctl_flags & SYSCTL_READWRITE) {
	    case SYSCTL_READONLY1:
		rw = (securelevel < 1) ? 1 : 0;
		break;
	    case SYSCTL_READONLY2:
		rw = (securelevel < 2) ? 1 : 0;
		break;
	    case SYSCTL_READWRITE:
		rw = 1;
		break;
	}

	/*
	 * it appears not to be writable at this time, so if someone
	 * tried to write to it, we must tell them to go away
	 */
	if (!rw && newp != NULL)
		return (EPERM);

	/*
	 * step one, copy out the stuff we have presently
	 */
	if (rnode->sysctl_flags & SYSCTL_IMMEDIATE) {
		switch (SYSCTL_TYPE(rnode->sysctl_flags)) {
		case CTLTYPE_INT:
			d = &rnode->sysctl_idata;
			break;
		case CTLTYPE_QUAD:
			d = &rnode->sysctl_qdata;
			break;
		default:
			return (EINVAL);
		}
	}
	else
		d = rnode->sysctl_data;
	if (SYSCTL_TYPE(rnode->sysctl_flags) == CTLTYPE_STRING)
		sz = strlen(d) + 1;
	else
		sz = rnode->sysctl_size;
	if (oldp != NULL)
		error = sysctl_copyout(l, d, oldp, MIN(sz, *oldlenp));
	if (error)
		return (error);
	*oldlenp = sz;

	/*
	 * are we done?
	 */
	if (newp == NULL || newlen == 0)
		return (0);

	/*
	 * hmm...not done.  must now "copy in" new value.  re-adjust
	 * sz to maximum value (strings are "weird").
	 */
	sz = rnode->sysctl_size;
	switch (SYSCTL_TYPE(rnode->sysctl_flags)) {
	case CTLTYPE_INT:
	case CTLTYPE_QUAD:
	case CTLTYPE_STRUCT:
		/*
		 * these data must be *exactly* the same size coming
		 * in.
		 */
		if (newlen != sz)
			return (EINVAL);
		error = sysctl_copyin(l, newp, d, sz);
		break;
	case CTLTYPE_STRING: {
		/*
		 * strings, on the other hand, can be shorter, and we
		 * let userland be sloppy about the trailing nul.
		 */
		char *newbuf;

		/*
		 * too much new string?
		 */
		if (newlen > sz)
			return (EINVAL);

		/*
		 * temporary copy of new inbound string
		 */
		len = MIN(sz, newlen);
		newbuf = malloc(len, M_SYSCTLDATA, M_WAITOK|M_CANFAIL);
		if (newbuf == NULL)
			return (ENOMEM);
		error = sysctl_copyin(l, newp, newbuf, len);
		if (error) {
			FREE(newbuf, M_SYSCTLDATA);
			return (error);
		}

		/*
		 * did they null terminate it, or do we have space
		 * left to do it ourselves?
		 */
		if (newbuf[len - 1] != '\0' && len == sz) {
			FREE(newbuf, M_SYSCTLDATA);
			return (EINVAL);
		}

		/*
		 * looks good, so pop it into place and zero the rest.
		 */
		if (len > 0)
			memcpy(rnode->sysctl_data, newbuf, len);
		if (sz != len)
			memset((char*)rnode->sysctl_data + len, 0, sz - len);
		FREE(newbuf, M_SYSCTLDATA);
		break;
	}
	default:
		return (EINVAL);
	}

	return (error);
}

/*
 * sysctl_mmap -- Dispatches sysctl mmap requests to those nodes that
 * purport to handle it.  This interface isn't fully fleshed out yet,
 * unfortunately.
 */
static int
sysctl_mmap(SYSCTLFN_RWARGS)
{
	struct sysctlnode nnode, *node;
	int error;

	/*
	 * let's just pretend that didn't happen, m'kay?
	 */
	if (l == NULL)
		return (EPERM);

	/*
	 * is this a sysctlnode description of an mmap request?
	 */
	if (newp == NULL || newlen != sizeof(struct sysctlnode))
		return (EINVAL);
	error = sysctl_copyin(l, newp, &nnode, sizeof(struct sysctlnode));
	if (error)
		return (error);

	/*
	 * does the node they asked for exist?
	 */
	if (namelen != 1)
		return (EOPNOTSUPP);
	node = rnode;
        error = sysctl_locate(l, &nnode.sysctl_num, 1, &node, NULL);
	if (error)
		return (error);

	/*
	 * does this node that we have found purport to handle mmap?
	 */
	if (node->sysctl_func == NULL ||
	    !(node->sysctl_flags & SYSCTL_MMAP))
		return (EOPNOTSUPP);

	/*
	 * well...okay, they asked for it.
	 */
	return ((*node->sysctl_func)(SYSCTLFN_CALL(node)));
}

/*
 * ********************************************************************
 * Section 3: Create and destroy from inside the kernel
 * ********************************************************************
 * sysctl_createv() and sysctl_destroyv() are simpler-to-use
 * interfaces for the kernel to fling new entries into the mib and rip
 * them out later.  In the case of sysctl_createv(), the returned copy
 * of the node (see sysctl_create()) will be translated back into a
 * pointer to the actual node.
 *
 * Note that sysctl_createv() will return 0 if the create request
 * matches an existing node (ala mkdir -p), and that sysctl_destroyv()
 * will return 0 if the node to be destroyed already does not exist
 * (aka rm -f) or if it is a parent of other nodes.
 *
 * This allows two (or more) different subsystems to assert sub-tree
 * existence before populating their own nodes, and to remove their
 * own nodes without orphaning the others when they are done.
 * ********************************************************************
 */
int
sysctl_createv(int flags, int type,
	       const char *namep, struct sysctlnode **rnode,
	       sysctlfn func, u_quad_t qv, void *newp, size_t newlen,
	       ...)
{
	va_list ap;
	int error, ni, namelen, name[CTL_MAXNAME];
	struct sysctlnode *pnode, nnode, onode;
	size_t sz;

	/*
	 * what is it?
	 */
	flags = SYSCTL_TYPE(type)|SYSCTL_FLAGS(flags);

	/*
	 * where do we put it?
	 */
	va_start(ap, newlen);
	namelen = 0;
	ni = -1;
	do {
		if (++ni == CTL_MAXNAME)
			return (ENAMETOOLONG);
		name[ni] = va_arg(ap, int);
		/*
		 * sorry, this is not supported from here
		 */
		if (name[ni] == CTL_CREATESYM)
			return (EINVAL);
	} while (name[ni] != CTL_EOL && name[ni] != CTL_CREATE);
	namelen = ni + (name[ni] == CTL_CREATE ? 1 : 0);
	va_end(ap);

	/*
	 * what's it called
	 */
	if (strlcpy(nnode.sysctl_name, namep, sizeof(nnode.sysctl_name)) >
	    sizeof(nnode.sysctl_name))
		return (ENAMETOOLONG);

	/*
	 * cons up the description of the new node
	 */
	nnode.sysctl_num = name[namelen - 1];
	name[namelen - 1] = CTL_CREATE;
	nnode.sysctl_size = newlen;
	nnode.sysctl_flags = flags;
	if (type == CTLTYPE_NODE) {
		nnode.sysctl_csize = 0;
		nnode.sysctl_clen = 0;
		nnode.sysctl_child = NULL;
		if (flags & SYSCTL_ALIAS)
			nnode.sysctl_alias = qv;
	}
	else if (flags & SYSCTL_IMMEDIATE) {
		switch (type) {
		case CTLTYPE_INT:
			nnode.sysctl_idata = qv;
			break;
		case CTLTYPE_QUAD:
			nnode.sysctl_qdata = qv;
			break;
		default:
			return (EINVAL);
		}
	}
	else {
		nnode.sysctl_data = newp;
	}
	nnode.sysctl_func = func;
	nnode.sysctl_parent = NULL;
	nnode.sysctl_ver = 0;

	/*
	 * initialize lock state -- we need locks if the main tree has
	 * been marked as complete, but since we could be called from
	 * either there, or from a device driver (say, at device
	 * insertion), or from an lkm (at lkm load time, say), we
	 * don't really want to "wait"...
	 */
	error = sysctl_lock(NULL, NULL, 0);
	if (error)
		return (error);
	
	/*
	 * locate the prospective parent of the new node, and if we
	 * find it, add the new node.
	 */
	sz = sizeof(onode);
	pnode = (rnode != NULL) ? *rnode : NULL;
	error = sysctl_locate(NULL, &name[0], namelen - 1, &pnode, &ni);
	if (error == 0)
		error = sysctl_create(&name[ni], namelen - ni, &onode, &sz,
				      &nnode, sizeof(nnode), &name[0], NULL,
				      pnode);

	/*
	 * unfortunately the node we wanted to create is already
	 * there.  if the node that's already there is a reasonable
	 * facsimile of the node we wanted to create, just pretend
	 * (for the caller's benefit) that we managed to create the
	 * node they wanted.
	 */
	if (error == EEXIST) {
		/* name is the same as requested... */
		if (strcmp(nnode.sysctl_name, onode.sysctl_name) == 0 &&
		    /* they want the same function... */
		    nnode.sysctl_func == onode.sysctl_func &&
		    /* number is the same as requested, or... */
		    (nnode.sysctl_num == onode.sysctl_num ||
		     /* they didn't pick a number... */
		     nnode.sysctl_num == CTL_CREATE)) {
			/*
			 * collision here from trying to create
			 * something that already existed; let's give
			 * our customers a hand and tell them they got
			 * what they wanted.
			 */
#ifdef SYSCTL_DEBUG_CREATE
			printf("cleared\n");
#endif /* SYSCTL_DEBUG_CREATE */
			error = 0;
		}
	}

	/*
	 * if they want to know where the new node is, go find the
	 * address of the actual node, not the copy that
	 * sysctl_create() gave us.
	 */
	if (rnode != NULL && error == 0) {
		/*
		 * sysctl_create() gave us back a copy of the node,
		 * but we need to know where it actually is...
		 */
		name[namelen - 1] = onode.sysctl_num;
		pnode = *rnode;
		error = sysctl_locate(NULL, &name[0], namelen, &pnode, &ni);
		/*
		 * not expecting an error here, but...
		 */
		if (error == 0)
			*rnode = pnode;
	}

	/*
	 * now it should be safe to release the lock state.
	 */
	sysctl_unlock(NULL);

	if (error != 0) {
		printf("sysctl_createv: sysctl_create(%s) returned %d\n",
		       nnode.sysctl_name, error);
#if 0
		if (error != ENOENT)
			sysctl_dump(&onode);
#endif
	}

	return (error);
}

int
sysctl_destroyv(struct sysctlnode *rnode, ...)
{
	va_list ap;
	int error, name[CTL_MAXNAME], namelen, ni;
	struct sysctlnode *pnode, *node;

	va_start(ap, rnode);
	namelen = 0;
	ni = 0;
	do {
		if (ni == CTL_MAXNAME)
			return (ENAMETOOLONG);
		name[ni] = va_arg(ap, int);
	} while (name[ni++] != CTL_EOL);
	namelen = ni - 1;
	va_end(ap);

	/*
	 * i can't imagine why we'd be destroying a node when the tree
	 * wasn't complete, but who knows?
	 */
	error = sysctl_lock(NULL, NULL, 0);
	if (error)
		return (error);

	/*
	 * where is it?
	 */
	node = rnode;
        error = sysctl_locate(NULL, &name[0], namelen, &node, &ni);
	if (error) {
		/* they want it gone and it's not there, so... */
		sysctl_unlock(NULL);
		return (error == ENOENT ? 0 : error);
	}

	/*
	 * we found it, now let's nuke it
	 */
	name[namelen - 1] = CTL_DESTROY;
	pnode = node->sysctl_parent;
	error = sysctl_destroy(&name[namelen - 1], 1, NULL, NULL,
			       node, sizeof(*node), &name[0], NULL,
			       pnode);
	if (error == ENOTEMPTY)
		/*
		 * think of trying to delete "foo" when "foo.bar"
		 * (which someone else put there) is still in
		 * existence
		 */
		error = 0;

        sysctl_unlock(NULL);

	return (error);
}

#if 0
/*
 * ********************************************************************
 * the dump routine.  i haven't yet decided how (if at all) i'll call
 * this from userland when it's in the kernel.
 * ********************************************************************
 */
static const char *
sf(int f)
{
	static char s[256];
	char *c;

	s[0] = '\0';
	c = "";

#define print_flag(_f, _s, _c, _q, _x) \
	if (((_x) && (((_f) & (_x)) == (__CONCAT(SYSCTL_,_q)))) || \
	    (!(_x) && ((_f) & (__CONCAT(SYSCTL_,_q))))) { \
		strlcat((_s), (_c), sizeof(_s)); \
		strlcat((_s), __STRING(_q), sizeof(_s)); \
		(_c) = ","; \
		(_f) &= ~(__CONCAT(SYSCTL_,_q)|(_x)); \
	}
	print_flag(f, s, c, READONLY, SYSCTL_READWRITE);
	print_flag(f, s, c, READONLY1, SYSCTL_READWRITE);
	print_flag(f, s, c, READONLY2, SYSCTL_READWRITE);
	print_flag(f, s, c, READWRITE, SYSCTL_READWRITE);
	print_flag(f, s, c, ANYWRITE, 0);
	print_flag(f, s, c, PRIVATE, 0);
	print_flag(f, s, c, PERMANENT, 0);
	print_flag(f, s, c, OWNDATA, 0);
	print_flag(f, s, c, IMMEDIATE, 0);
	print_flag(f, s, c, HEX, 0);
	print_flag(f, s, c, ROOT, 0);
	print_flag(f, s, c, ANYNUMBER, 0);
	print_flag(f, s, c, HIDDEN, 0);
	print_flag(f, s, c, ALIAS, 0);
#undef print_flag

	if (f) {
		char foo[9];
		snprintf(foo, sizeof(foo), "%x", f);
		strlcat(s, c, sizeof(s));
		strlcat(s, foo, sizeof(s));
	}

	return (s);
}

static const char *
st(int t)
{

	switch (t) {
	case CTLTYPE_NODE:
		return "NODE";
	case CTLTYPE_INT:
		return "INT";
	case CTLTYPE_STRING:
		return "STRING";
	case CTLTYPE_QUAD:
		return "QUAD";
	case CTLTYPE_STRUCT:
		return "STRUCT";
	}

	return "???";
}

void
sysctl_dump(const struct sysctlnode *d)
{
	static char nmib[64], smib[256];
	static int indent;
	struct sysctlnode *n;
	char *np, *sp, tmp[20];
	int i;

	if (d == NULL)
		return;

	np = &nmib[strlen(nmib)];
	sp = &smib[strlen(smib)];

	if (!(d->sysctl_flags & SYSCTL_ROOT)) {
		snprintf(tmp, sizeof(tmp), "%d", d->sysctl_num);
		strcat(nmib, ".");
		strcat(smib, ".");
		strcat(nmib, tmp);
		strcat(smib, d->sysctl_name);
		printf("%s -> %s (%d)\n", &nmib[1], &smib[1],
		       SYSCTL_TYPE(d->sysctl_flags));
	}

	if (1) {
		printf("%*s%p:\tsysctl_name  [%s]\n", indent, "",
		       d, d->sysctl_name);
		printf("%*s\t\tsysctl_num    %d\n",   indent, "",
		       d->sysctl_num);
		printf("%*s\t\tsysctl_flags  %x (flags=%x<%s> type=%d<%s> "
		       "size=%zu)\n",
		       indent, "", d->sysctl_flags,
		       SYSCTL_FLAGS(d->sysctl_flags),
		       sf(SYSCTL_FLAGS(d->sysctl_flags)),
		       SYSCTL_TYPE(d->sysctl_flags),
		       st(SYSCTL_TYPE(d->sysctl_flags)),
		       d->sysctl_size);
		if (SYSCTL_TYPE(d->sysctl_flags) == CTLTYPE_NODE) {
			printf("%*s\t\tsysctl_csize  %d\n",   indent, "",
			       d->sysctl_csize);
			printf("%*s\t\tsysctl_clen   %d\n",   indent, "",
			       d->sysctl_clen);
			printf("%*s\t\tsysctl_child  %p\n",   indent, "",
			       d->sysctl_child);
		}
		else
			printf("%*s\t\tsysctl_data   %p\n",   indent, "",
			       d->sysctl_data);
		printf("%*s\t\tsysctl_func   %p\n",   indent, "",
		       d->sysctl_func);
		printf("%*s\t\tsysctl_parent %p\n",   indent, "",
		       d->sysctl_parent);
		printf("%*s\t\tsysctl_ver    %d\n",   indent, "",
		       d->sysctl_ver);
	}

	if (SYSCTL_TYPE(d->sysctl_flags) == CTLTYPE_NODE) {
		indent += 8;
		n = d->sysctl_child;
		for (i = 0; i < d->sysctl_clen; i++) {
			sysctl_dump(&n[i]);
		}
		indent -= 8;
	}

	np[0] = '\0';
	sp[0] = '\0';
}
#endif /* 0 */

/*
 * ********************************************************************
 * Deletes an entire n-ary tree.  Not recommended unless you know why
 * you're doing it.  Personally, I don't know why you'd even think
 * about it.
 * ********************************************************************
 */
void
sysctl_free(struct sysctlnode *rnode)
{
	struct sysctlnode *node, *pnode;

	if (rnode == NULL)
		rnode = &sysctl_root;
	pnode = rnode;

	node = pnode->sysctl_child;
	do {
		while (node != NULL && pnode->sysctl_csize > 0) {
			while (node <
			       &pnode->sysctl_child[pnode->sysctl_clen] &&
			       (SYSCTL_TYPE(node->sysctl_flags) !=
				CTLTYPE_NODE ||
				node->sysctl_csize == 0)) {
				if (SYSCTL_FLAGS(node->sysctl_flags) &
				    SYSCTL_OWNDATA) {
					if (node->sysctl_data != NULL) {
						FREE(node->sysctl_data,
						     M_SYSCTLDATA);
						node->sysctl_data = NULL;
					}
				}
				node++;
			}
			if (node < &pnode->sysctl_child[pnode->sysctl_clen]) {
				pnode = node;
				node = node->sysctl_child;
			}
			else
				break;
		}
		if (pnode->sysctl_child != NULL)
			FREE(pnode->sysctl_child, M_SYSCTLNODE);
		pnode->sysctl_clen = 0;
		pnode->sysctl_csize = 0;
		pnode->sysctl_child = NULL;
		node = pnode;
		pnode = node->sysctl_parent;
	} while (pnode != NULL && pnode != rnode);
}

/*
 * ********************************************************************
 * old_sysctl -- A routine to bridge old-style internal calls to the
 * new infrastructure.
 * ********************************************************************
 */
int
old_sysctl(int *name, u_int namelen, void *oldp, size_t *oldlenp,
	   void *newp, size_t newlen, struct lwp *l)
{
	int error;
	size_t savelen = *oldlenp;
	
	error = sysctl_lock(l, oldp, savelen);
	if (error)
		return (error);
	error = sysctl_dispatch(name, namelen, oldp, oldlenp,
				newp, newlen, name, l, NULL);
	sysctl_unlock(l);
	if (error == 0 && oldp != NULL && savelen < *oldlenp)
		error = ENOMEM;

	return (error);
}

/*
 * ********************************************************************
 * Section 4: Generic helper routines
 * ********************************************************************
 * "helper" routines that can do more finely grained access control,
 * construct structures from disparate information, create the
 * appearance of more nodes and sub-trees, etc.  for example, if
 * CTL_PROC wanted a helper function, it could respond to a CTL_QUERY
 * with a dynamically created list of nodes that represented the
 * currently running processes at that instant.
 * ********************************************************************
 */

/*
 * first, a few generic helpers that provide:
 *
 * sysctl_needfunc()		a readonly interface that emits a warning
 * sysctl_notavail()		returns EOPNOTSUPP (generic error)
 * sysctl_null()		an empty return buffer with no error
 */
int
sysctl_needfunc(SYSCTLFN_ARGS)
{
	int error;

	printf("!!SYSCTL_NEEDFUNC!!\n");

	if (newp != NULL || namelen != 0)
		return (EOPNOTSUPP);

	error = 0;
	if (oldp != NULL)
		error = sysctl_copyout(l, rnode->sysctl_data, oldp,
				       MIN(rnode->sysctl_size, *oldlenp));
	*oldlenp = rnode->sysctl_size;

	return (error);
}

int
sysctl_notavail(SYSCTLFN_ARGS)
{

	if (namelen == 1 && name[0] == CTL_QUERY)
		return (sysctl_query(SYSCTLFN_CALL(rnode)));

	return (EOPNOTSUPP);
}

int
sysctl_null(SYSCTLFN_ARGS)
{

	*oldlenp = 0;

	return (0);
}

/*
 * ********************************************************************
 * Section 5: The machinery that makes it all go
 * ********************************************************************
 * Memory "manglement" routines.  Not much to this, eh?
 * ********************************************************************
 */
static int
sysctl_alloc(struct sysctlnode *p, int x)
{
	int i;
	struct sysctlnode *n;

	assert(p->sysctl_child == NULL);

	if (x == 1)
		MALLOC(n, struct sysctlnode *,
		       sizeof(struct sysctlnode),
		       M_SYSCTLNODE, M_WAITOK|M_CANFAIL);
	else
		MALLOC(n, struct sysctlnode *,
		       SYSCTL_DEFSIZE * sizeof(struct sysctlnode),
		       M_SYSCTLNODE, M_WAITOK|M_CANFAIL);
	if (n == NULL)
		return (ENOMEM);

	if (x == 1) {
		memset(n, 0, sizeof(struct sysctlnode));
		p->sysctl_csize = 1;
	}
	else {
		memset(n, 0, SYSCTL_DEFSIZE * sizeof(struct sysctlnode));
		p->sysctl_csize = SYSCTL_DEFSIZE;
	}
	p->sysctl_clen = 0;

	for (i = 0; i < p->sysctl_csize; i++)
		n[i].sysctl_parent = p;

	p->sysctl_child = n;
	return (0);
}

static int
sysctl_realloc(struct sysctlnode *p)
{
	int i, j;
	struct sysctlnode *n;

	assert(p->sysctl_csize == p->sysctl_clen);

	/*
	 * how many do we have...how many should we make?
	 */
	i = p->sysctl_clen;
	n = malloc(2 * i * sizeof(struct sysctlnode), M_SYSCTLNODE,
		   M_WAITOK|M_CANFAIL);
	if (n == NULL)
		return (ENOMEM);

	/*
	 * move old children over...initialize new children
	 */
	memcpy(n, p->sysctl_child, i * sizeof(struct sysctlnode));
	memset(&n[i], 0, i * sizeof(struct sysctlnode));
	p->sysctl_csize = 2 * i;
	p->sysctl_clen = i;

	/*
	 * reattach moved (and new) children to parent; if a moved
	 * child node has children, reattach the parent pointers of
	 * grandchildren
	 */
        for (i = 0; i < p->sysctl_csize; i++) {
                n[i].sysctl_parent = p;
		if (n[i].sysctl_child != NULL) {
			for (j = 0; j < n[i].sysctl_csize; j++)
				n[i].sysctl_child[j].sysctl_parent = &n[i];
		}
	}

	/*
	 * get out with the old and in with the new
	 */
	FREE(p->sysctl_child, M_SYSCTLNODE);
	p->sysctl_child = n;

	return (0);
}
