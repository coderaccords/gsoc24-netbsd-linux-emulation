/* $NetBSD: kern_auth.c,v 1.51 2007/07/06 17:33:31 dsl Exp $ */

/*-
 * Copyright (c) 2005, 2006 Elad Efrat <elad@NetBSD.org>
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
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: kern_auth.c,v 1.51 2007/07/06 17:33:31 dsl Exp $");

#include <sys/types.h>
#include <sys/param.h>
#include <sys/queue.h>
#include <sys/proc.h>
#include <sys/ucred.h>
#include <sys/pool.h>
#include <sys/kauth.h>
#include <sys/kmem.h>
#include <sys/rwlock.h>
#include <sys/sysctl.h>		/* for pi_[p]cread */
#include <sys/mutex.h>
#include <sys/specificdata.h>

/*
 * Secmodel-specific credentials.
 */
struct kauth_key {
	const char *ks_secmodel;	/* secmodel */
	specificdata_key_t ks_key;	/* key */
};

/* 
 * Credentials.
 *
 * A subset of this structure is used in kvm(3) (src/lib/libkvm/kvm_proc.c)
 * and should be synchronized with this structure when the update is
 * relevant.
 */
struct kauth_cred {
	kmutex_t cr_lock;		/* lock on cr_refcnt */
	u_int cr_refcnt;		/* reference count */
	uid_t cr_uid;			/* user id */
	uid_t cr_euid;			/* effective user id */
	uid_t cr_svuid;			/* saved effective user id */
	gid_t cr_gid;			/* group id */
	gid_t cr_egid;			/* effective group id */
	gid_t cr_svgid;			/* saved effective group id */
	u_int cr_ngroups;		/* number of groups */
	gid_t cr_groups[NGROUPS];	/* group memberships */
	specificdata_reference cr_sd;	/* specific data */
};

/*
 * Listener.
 */
struct kauth_listener {
	kauth_scope_callback_t		func;		/* callback */
	kauth_scope_t			scope;		/* scope backpointer */
	u_int				refcnt;		/* reference count */
	SIMPLEQ_ENTRY(kauth_listener)	listener_next;	/* listener list */
};

/*
 * Scope.
 */
struct kauth_scope {
	const char		       *id;		/* scope name */
	void			       *cookie;		/* user cookie */
	u_int				nlisteners;	/* # of listeners */
	SIMPLEQ_HEAD(, kauth_listener)	listenq;	/* listener list */
	SIMPLEQ_ENTRY(kauth_scope)	next_scope;	/* scope list */
};

static int kauth_cred_hook(kauth_cred_t, kauth_action_t, void *, void *);

static POOL_INIT(kauth_cred_pool, sizeof(struct kauth_cred), 0, 0, 0,
    "kauthcredpl", &pool_allocator_nointr, IPL_NONE);

/* List of scopes and its lock. */
static SIMPLEQ_HEAD(, kauth_scope) scope_list;

/* Built-in scopes: generic, process. */
static kauth_scope_t kauth_builtin_scope_generic;
static kauth_scope_t kauth_builtin_scope_system;
static kauth_scope_t kauth_builtin_scope_process;
static kauth_scope_t kauth_builtin_scope_network;
static kauth_scope_t kauth_builtin_scope_machdep;
static kauth_scope_t kauth_builtin_scope_device;
static kauth_scope_t kauth_builtin_scope_cred;

static unsigned int nsecmodels = 0;

static specificdata_domain_t kauth_domain;
krwlock_t	kauth_lock;

/* Allocate new, empty kauth credentials. */
kauth_cred_t
kauth_cred_alloc(void)
{
	kauth_cred_t cred;

	cred = pool_get(&kauth_cred_pool, PR_WAITOK);
	memset(cred, 0, sizeof(*cred));
	mutex_init(&cred->cr_lock, MUTEX_DEFAULT, IPL_NONE);
	cred->cr_refcnt = 1;
	specificdata_init(kauth_domain, &cred->cr_sd);
	kauth_cred_hook(cred, KAUTH_CRED_INIT, NULL, NULL);

	return (cred);
}

/* Increment reference count to cred. */
void
kauth_cred_hold(kauth_cred_t cred)
{
	KASSERT(cred != NULL);
	KASSERT(cred->cr_refcnt > 0);

        mutex_enter(&cred->cr_lock);
        cred->cr_refcnt++;
        mutex_exit(&cred->cr_lock);
}

/* Decrease reference count to cred. If reached zero, free it. */
void
kauth_cred_free(kauth_cred_t cred)
{
	u_int refcnt;

	KASSERT(cred != NULL);
	KASSERT(cred->cr_refcnt > 0);

	mutex_enter(&cred->cr_lock);
	refcnt = --cred->cr_refcnt;
	mutex_exit(&cred->cr_lock);

	if (refcnt == 0) {
		kauth_cred_hook(cred, KAUTH_CRED_FREE, NULL, NULL);
		specificdata_fini(kauth_domain, &cred->cr_sd);
		mutex_destroy(&cred->cr_lock);
		pool_put(&kauth_cred_pool, cred);
	}
}

static void
kauth_cred_clone1(kauth_cred_t from, kauth_cred_t to, bool copy_groups)
{
	KASSERT(from != NULL);
	KASSERT(to != NULL);
	KASSERT(from->cr_refcnt > 0);

	to->cr_uid = from->cr_uid;
	to->cr_euid = from->cr_euid;
	to->cr_svuid = from->cr_svuid;
	to->cr_gid = from->cr_gid;
	to->cr_egid = from->cr_egid;
	to->cr_svgid = from->cr_svgid;
	if (copy_groups) {
		to->cr_ngroups = from->cr_ngroups;
		memcpy(to->cr_groups, from->cr_groups, sizeof(to->cr_groups));
	}

	kauth_cred_hook(from, KAUTH_CRED_COPY, to, NULL);
}

void
kauth_cred_clone(kauth_cred_t from, kauth_cred_t to)
{
	kauth_cred_clone1(from, to, true);
}

/*
 * Duplicate cred and return a new kauth_cred_t.
 */
kauth_cred_t
kauth_cred_dup(kauth_cred_t cred)
{
	kauth_cred_t new_cred;

	KASSERT(cred != NULL);
	KASSERT(cred->cr_refcnt > 0);

	new_cred = kauth_cred_alloc();

	kauth_cred_clone(cred, new_cred);

	return (new_cred);
}

/*
 * Similar to crcopy(), only on a kauth_cred_t.
 * XXX: Is this even needed? [kauth_cred_copy]
 */
kauth_cred_t
kauth_cred_copy(kauth_cred_t cred)
{
	kauth_cred_t new_cred;

	KASSERT(cred != NULL);
	KASSERT(cred->cr_refcnt > 0);

	/* If the provided credentials already have one reference, use them. */
	if (cred->cr_refcnt == 1)
		return (cred);

	new_cred = kauth_cred_alloc();

	kauth_cred_clone(cred, new_cred);

	kauth_cred_free(cred);

	return (new_cred);
}

void
kauth_proc_fork(struct proc *parent, struct proc *child)
{

	mutex_enter(&parent->p_mutex);
	kauth_cred_hold(parent->p_cred);
	child->p_cred = parent->p_cred;
	mutex_exit(&parent->p_mutex);

	/* XXX: relies on parent process stalling during fork() */
	kauth_cred_hook(parent->p_cred, KAUTH_CRED_FORK, parent,
	    child);
}

uid_t
kauth_cred_getuid(kauth_cred_t cred)
{
	KASSERT(cred != NULL);

	return (cred->cr_uid);
}

uid_t
kauth_cred_geteuid(kauth_cred_t cred)
{
	KASSERT(cred != NULL);

	return (cred->cr_euid);
}

uid_t
kauth_cred_getsvuid(kauth_cred_t cred)
{
	KASSERT(cred != NULL);

	return (cred->cr_svuid);
}

gid_t
kauth_cred_getgid(kauth_cred_t cred)
{
	KASSERT(cred != NULL);

	return (cred->cr_gid);
}

gid_t
kauth_cred_getegid(kauth_cred_t cred)
{
	KASSERT(cred != NULL);

	return (cred->cr_egid);
}

gid_t
kauth_cred_getsvgid(kauth_cred_t cred)
{
	KASSERT(cred != NULL);

	return (cred->cr_svgid);
}

void
kauth_cred_setuid(kauth_cred_t cred, uid_t uid)
{
	KASSERT(cred != NULL);
	KASSERT(cred->cr_refcnt == 1);

	cred->cr_uid = uid;
}

void
kauth_cred_seteuid(kauth_cred_t cred, uid_t uid)
{
	KASSERT(cred != NULL);
	KASSERT(cred->cr_refcnt == 1);

	cred->cr_euid = uid;
}

void
kauth_cred_setsvuid(kauth_cred_t cred, uid_t uid)
{
	KASSERT(cred != NULL);
	KASSERT(cred->cr_refcnt == 1);

	cred->cr_svuid = uid;
}

void
kauth_cred_setgid(kauth_cred_t cred, gid_t gid)
{
	KASSERT(cred != NULL);
	KASSERT(cred->cr_refcnt == 1);

	cred->cr_gid = gid;
}

void
kauth_cred_setegid(kauth_cred_t cred, gid_t gid)
{
	KASSERT(cred != NULL);
	KASSERT(cred->cr_refcnt == 1);

	cred->cr_egid = gid;
}

void
kauth_cred_setsvgid(kauth_cred_t cred, gid_t gid)
{
	KASSERT(cred != NULL);
	KASSERT(cred->cr_refcnt == 1);

	cred->cr_svgid = gid;
}

/* Checks if gid is a member of the groups in cred. */
int
kauth_cred_ismember_gid(kauth_cred_t cred, gid_t gid, int *resultp)
{
	int i;

	KASSERT(cred != NULL);
	KASSERT(resultp != NULL);

	*resultp = 0;

	for (i = 0; i < cred->cr_ngroups; i++)
		if (cred->cr_groups[i] == gid) {
			*resultp = 1;
			break;
		}

	return (0);
}

u_int
kauth_cred_ngroups(kauth_cred_t cred)
{
	KASSERT(cred != NULL);

	return (cred->cr_ngroups);
}

/*
 * Return the group at index idx from the groups in cred.
 */
gid_t
kauth_cred_group(kauth_cred_t cred, u_int idx)
{
	KASSERT(cred != NULL);
	KASSERT(idx < cred->cr_ngroups);

	return (cred->cr_groups[idx]);
}

/* XXX elad: gmuid is unused for now. */
int
kauth_cred_setgroups(kauth_cred_t cred, const gid_t *grbuf, size_t len,
    uid_t gmuid, unsigned int flags)
{
	int error = 0;

	KASSERT(cred != NULL);
	KASSERT(cred->cr_refcnt == 1);

	if (len > sizeof(cred->cr_groups) / sizeof(cred->cr_groups[0]))
		return EINVAL;

	if (len) {
		if ((flags & (UIO_USERSPACE | UIO_SYSSPACE)) == UIO_SYSSPACE)
			memcpy(cred->cr_groups, grbuf,
			    len * sizeof(cred->cr_groups[0]));
		else {
			error = copyin(grbuf, cred->cr_groups,
			    len * sizeof(cred->cr_groups[0]));
			if (error != 0)
				len = 0;
		}
	}
	memset(cred->cr_groups + len, 0xff,
	    sizeof(cred->cr_groups) - (len * sizeof(cred->cr_groups[0])));

	cred->cr_ngroups = len;

	return error;
}

/* This supports sys_setgroups() */
int
kauth_proc_setgroups(struct lwp *l, kauth_cred_t ncred)
{
	kauth_cred_t cred;
	int error;

	/*
	 * At this point we could delete duplicate groups from ncred,
	 * and plausibly sort the list - but in general the later is
	 * a bad idea.
	 */
	proc_crmod_enter();
	/* Maybe we should use curproc here ? */
	cred = l->l_proc->p_cred;

	kauth_cred_clone1(cred, ncred, false);

	error = kauth_authorize_process(cred, KAUTH_PROCESS_SETID,
	    l->l_proc, NULL, NULL, NULL);
	if (error != 0) {
		proc_crmod_leave(cred, ncred, false);
			return error;
	}

	/* Broadcast our credentials to the process and other LWPs. */
 	proc_crmod_leave(ncred, cred, true);
	return 0;
}

int
kauth_cred_getgroups(kauth_cred_t cred, gid_t *grbuf, size_t len,
    unsigned int flags)
{
	KASSERT(cred != NULL);

	if (len > cred->cr_ngroups)
		return EINVAL;

	if ((flags & (UIO_USERSPACE | UIO_SYSSPACE)) == UIO_USERSPACE)
		return copyout(cred->cr_groups, grbuf, sizeof(*grbuf) * len);
	memcpy(grbuf, cred->cr_groups, sizeof(*grbuf) * len);

	return 0;
}

int
kauth_register_key(const char *secmodel, kauth_key_t *result)
{
	kauth_key_t k;
	specificdata_key_t key;
	int error;

	KASSERT(result != NULL);

	error = specificdata_key_create(kauth_domain, &key, NULL);
	if (error)
		return (error);

	k = kmem_alloc(sizeof(*k), KM_SLEEP);
	k->ks_secmodel = secmodel;
	k->ks_key = key;

	*result = k;

	return (0);
}

int
kauth_deregister_key(kauth_key_t key)
{
	KASSERT(key != NULL);

	specificdata_key_delete(kauth_domain, key->ks_key);
	kmem_free(key, sizeof(*key));

	return (0);
}

void *
kauth_cred_getdata(kauth_cred_t cred, kauth_key_t key)
{
	KASSERT(cred != NULL);
	KASSERT(key != NULL);

	return (specificdata_getspecific(kauth_domain, &cred->cr_sd,
	    key->ks_key));
}

void
kauth_cred_setdata(kauth_cred_t cred, kauth_key_t key, void *data)
{
	KASSERT(cred != NULL);
	KASSERT(key != NULL);

	specificdata_setspecific(kauth_domain, &cred->cr_sd, key->ks_key, data);
}

/*
 * Match uids in two credentials.
 */
int
kauth_cred_uidmatch(kauth_cred_t cred1, kauth_cred_t cred2)
{
	KASSERT(cred1 != NULL);
	KASSERT(cred2 != NULL);

	if (cred1->cr_uid == cred2->cr_uid ||
	    cred1->cr_euid == cred2->cr_uid ||
	    cred1->cr_uid == cred2->cr_euid ||
	    cred1->cr_euid == cred2->cr_euid)
		return (1);

	return (0);
}

u_int
kauth_cred_getrefcnt(kauth_cred_t cred)
{
	KASSERT(cred != NULL);

	return (cred->cr_refcnt);
}

/*
 * Convert userland credentials (struct uucred) to kauth_cred_t.
 * XXX: For NFS & puffs
 */
void    
kauth_uucred_to_cred(kauth_cred_t cred, const struct uucred *uuc)
{       
	KASSERT(cred != NULL);
	KASSERT(uuc != NULL);
 
	cred->cr_refcnt = 1;
	cred->cr_uid = uuc->cr_uid;
	cred->cr_euid = uuc->cr_uid;
	cred->cr_svuid = uuc->cr_uid;
	cred->cr_gid = uuc->cr_gid;
	cred->cr_egid = uuc->cr_gid;
	cred->cr_svgid = uuc->cr_gid;
	cred->cr_ngroups = min(uuc->cr_ngroups, NGROUPS);
	kauth_cred_setgroups(cred, __UNCONST(uuc->cr_groups),
	    cred->cr_ngroups, -1, UIO_SYSSPACE);
}

/*
 * Convert kauth_cred_t to userland credentials (struct uucred).
 * XXX: For NFS & puffs
 */
void    
kauth_cred_to_uucred(struct uucred *uuc, const kauth_cred_t cred)
{       
	KASSERT(cred != NULL);
	KASSERT(uuc != NULL);
	int ng;

	ng = min(cred->cr_ngroups, NGROUPS);
	uuc->cr_uid = cred->cr_euid;  
	uuc->cr_gid = cred->cr_egid;  
	uuc->cr_ngroups = ng;
	kauth_cred_getgroups(cred, uuc->cr_groups, ng, UIO_SYSSPACE);
}

/*
 * Compare kauth_cred_t and uucred credentials.
 * XXX: Modelled after crcmp() for NFS.
 */
int
kauth_cred_uucmp(kauth_cred_t cred, const struct uucred *uuc)
{
	KASSERT(cred != NULL);
	KASSERT(uuc != NULL);

	if (cred->cr_euid == uuc->cr_uid &&
	    cred->cr_egid == uuc->cr_gid &&
	    cred->cr_ngroups == uuc->cr_ngroups) {
		int i;

		/* Check if all groups from uuc appear in cred. */
		for (i = 0; i < uuc->cr_ngroups; i++) {
			int ismember;

			ismember = 0;
			if (kauth_cred_ismember_gid(cred, uuc->cr_groups[i],
			    &ismember) != 0 || !ismember)
				return (1);
		}

		return (0);
	}

	return (1);
}

/*
 * Make a struct ucred out of a kauth_cred_t.  For compatibility.
 */
void
kauth_cred_toucred(kauth_cred_t cred, struct ki_ucred *uc)
{
	KASSERT(cred != NULL);
	KASSERT(uc != NULL);

	uc->cr_ref = cred->cr_refcnt;
	uc->cr_uid = cred->cr_euid;
	uc->cr_gid = cred->cr_egid;
	uc->cr_ngroups = min(cred->cr_ngroups,
			     sizeof(uc->cr_groups) / sizeof(uc->cr_groups[0]));
	memcpy(uc->cr_groups, cred->cr_groups,
	       uc->cr_ngroups * sizeof(uc->cr_groups[0]));
}

/*
 * Make a struct pcred out of a kauth_cred_t.  For compatibility.
 */
void
kauth_cred_topcred(kauth_cred_t cred, struct ki_pcred *pc)
{
	KASSERT(cred != NULL);
	KASSERT(pc != NULL);

	pc->p_pad = NULL;
	pc->p_ruid = cred->cr_uid;
	pc->p_svuid = cred->cr_svuid;
	pc->p_rgid = cred->cr_gid;
	pc->p_svgid = cred->cr_svgid;
	pc->p_refcnt = cred->cr_refcnt;
}

/*
 * Return kauth_cred_t for the current LWP.
 */
kauth_cred_t
kauth_cred_get(void)
{
	return (curlwp->l_cred);
}

/*
 * Returns a scope matching the provided id.
 * Requires the scope list lock to be held by the caller.
 */
static kauth_scope_t
kauth_ifindscope(const char *id)
{
	kauth_scope_t scope;

	KASSERT(rw_lock_held(&kauth_lock));

	scope = NULL;
	SIMPLEQ_FOREACH(scope, &scope_list, next_scope) {
		if (strcmp(scope->id, id) == 0)
			break;
	}

	return (scope);
}

/*
 * Register a new scope.
 *
 * id - identifier for the scope
 * callback - the scope's default listener
 * cookie - cookie to be passed to the listener(s)
 */
kauth_scope_t
kauth_register_scope(const char *id, kauth_scope_callback_t callback,
    void *cookie)
{
	kauth_scope_t scope;
	kauth_listener_t listener = NULL; /* XXX gcc */

	/* Sanitize input */
	if (id == NULL)
		return (NULL);

	/* Allocate space for a new scope and listener. */
	scope = kmem_alloc(sizeof(*scope), KM_SLEEP);
	if (scope == NULL)
		return NULL;
	if (callback != NULL) {
		listener = kmem_alloc(sizeof(*listener), KM_SLEEP);
		if (listener == NULL) {
			kmem_free(scope, sizeof(*scope));
			return (NULL);
		}
	}

	/*
	 * Acquire scope list lock.
	 */
	rw_enter(&kauth_lock, RW_WRITER);

	/* Check we don't already have a scope with the same id */
	if (kauth_ifindscope(id) != NULL) {
		rw_exit(&kauth_lock);

		kmem_free(scope, sizeof(*scope));
		if (callback != NULL)
			kmem_free(listener, sizeof(*listener));

		return (NULL);
	}

	/* Initialize new scope with parameters */
	scope->id = id;
	scope->cookie = cookie;
	scope->nlisteners = 1;

	SIMPLEQ_INIT(&scope->listenq);

	/* Add default listener */
	if (callback != NULL) {
		listener->func = callback;
		listener->scope = scope;
		listener->refcnt = 0;
		SIMPLEQ_INSERT_HEAD(&scope->listenq, listener, listener_next);
	}

	/* Insert scope to scopes list */
	SIMPLEQ_INSERT_TAIL(&scope_list, scope, next_scope);

	rw_exit(&kauth_lock);

	return (scope);
}

/*
 * Initialize the kernel authorization subsystem.
 *
 * Initialize the scopes list lock.
 * Create specificdata domain.
 * Register the credentials scope, used in kauth(9) internally.
 * Register built-in scopes: generic, system, process, network, machdep, device.
 */
void
kauth_init(void)
{
	SIMPLEQ_INIT(&scope_list);
	rw_init(&kauth_lock);

	/* Create specificdata domain. */
	kauth_domain = specificdata_domain_create();

	/* Register credentials scope. */
	kauth_builtin_scope_cred =
	    kauth_register_scope(KAUTH_SCOPE_CRED, NULL, NULL);

	/* Register generic scope. */
	kauth_builtin_scope_generic = kauth_register_scope(KAUTH_SCOPE_GENERIC,
	    NULL, NULL);

	/* Register system scope. */
	kauth_builtin_scope_system = kauth_register_scope(KAUTH_SCOPE_SYSTEM,
	    NULL, NULL);

	/* Register process scope. */
	kauth_builtin_scope_process = kauth_register_scope(KAUTH_SCOPE_PROCESS,
	    NULL, NULL);

	/* Register network scope. */
	kauth_builtin_scope_network = kauth_register_scope(KAUTH_SCOPE_NETWORK,
	    NULL, NULL);

	/* Register machdep scope. */
	kauth_builtin_scope_machdep = kauth_register_scope(KAUTH_SCOPE_MACHDEP,
	    NULL, NULL);

	/* Register device scope. */
	kauth_builtin_scope_device = kauth_register_scope(KAUTH_SCOPE_DEVICE,
	    NULL, NULL);
}

/*
 * Deregister a scope.
 * Requires scope list lock to be held by the caller.
 *
 * scope - the scope to deregister
 */
void
kauth_deregister_scope(kauth_scope_t scope)
{
	if (scope != NULL) {
		/* Remove scope from list */
		SIMPLEQ_REMOVE(&scope_list, scope, kauth_scope, next_scope);
		kmem_free(scope, sizeof(*scope));
	}
}

/*
 * Register a listener.
 *
 * id - scope identifier.
 * callback - the callback routine for the listener.
 * cookie - cookie to pass unmoidfied to the callback.
 */
kauth_listener_t
kauth_listen_scope(const char *id, kauth_scope_callback_t callback,
   void *cookie)
{
	kauth_scope_t scope;
	kauth_listener_t listener;

	listener = kmem_alloc(sizeof(*listener), KM_SLEEP);
	if (listener == NULL)
		return (NULL);

	rw_enter(&kauth_lock, RW_WRITER);

	/*
	 * Find scope struct.
	 */
	scope = kauth_ifindscope(id);
	if (scope == NULL) {
		rw_exit(&kauth_lock);
		kmem_free(listener, sizeof(*listener));
		return (NULL);
	}

	/* Allocate listener */

	/* Initialize listener with parameters */
	listener->func = callback;
	listener->refcnt = 0;

	/* Add listener to scope */
	SIMPLEQ_INSERT_TAIL(&scope->listenq, listener, listener_next);

	/* Raise number of listeners on scope. */
	scope->nlisteners++;
	listener->scope = scope;

	rw_exit(&kauth_lock);

	return (listener);
}

/*
 * Deregister a listener.
 *
 * listener - listener reference as returned from kauth_listen_scope().
 */
void
kauth_unlisten_scope(kauth_listener_t listener)
{

	if (listener != NULL) {
		rw_enter(&kauth_lock, RW_WRITER);
		SIMPLEQ_REMOVE(&listener->scope->listenq, listener,
		    kauth_listener, listener_next);
		listener->scope->nlisteners--;
		rw_exit(&kauth_lock);
		kmem_free(listener, sizeof(*listener));
	}
}

/*
 * Authorize a request.
 *
 * scope - the scope of the request as defined by KAUTH_SCOPE_* or as
 *	   returned from kauth_register_scope().
 * credential - credentials of the user ("actor") making the request.
 * action - request identifier.
 * arg[0-3] - passed unmodified to listener(s).
 */
int
kauth_authorize_action(kauth_scope_t scope, kauth_cred_t cred,
		       kauth_action_t action, void *arg0, void *arg1,
		       void *arg2, void *arg3)
{
	kauth_listener_t listener;
	int error, allow, fail;

#if 0 /* defined(LOCKDEBUG) */
	spinlock_switchcheck();
	simple_lock_only_held(NULL, "kauth_authorize_action");
#endif

	KASSERT(cred != NULL);
	KASSERT(action != 0);

	/* Short-circuit requests coming from the kernel. */
	if (cred == NOCRED || cred == FSCRED)
		return (0);

	KASSERT(scope != NULL);

	fail = 0;
	allow = 0;

	/* rw_enter(&kauth_lock, RW_READER); XXX not yet */
	SIMPLEQ_FOREACH(listener, &scope->listenq, listener_next) {
		error = listener->func(cred, action, scope->cookie, arg0,
		    arg1, arg2, arg3);

		if (error == KAUTH_RESULT_ALLOW)
			allow = 1;
		else if (error == KAUTH_RESULT_DENY)
			fail = 1;
	}
	/* rw_exit(&kauth_lock); */

	if (fail)
		return (EPERM);

	if (allow)
		return (0);

	if (!nsecmodels)
		return (0);

	return (EPERM);
};

/*
 * Generic scope authorization wrapper.
 */
int
kauth_authorize_generic(kauth_cred_t cred, kauth_action_t action, void *arg0)
{
	return (kauth_authorize_action(kauth_builtin_scope_generic, cred, 
	    action, arg0, NULL, NULL, NULL));
}

/*
 * System scope authorization wrapper.
 */
int
kauth_authorize_system(kauth_cred_t cred, kauth_action_t action,
    enum kauth_system_req req, void *arg1, void *arg2, void *arg3)
{
	return (kauth_authorize_action(kauth_builtin_scope_system, cred,
	    action, (void *)req, arg1, arg2, arg3));
}

/*
 * Process scope authorization wrapper.
 */
int
kauth_authorize_process(kauth_cred_t cred, kauth_action_t action,
    struct proc *p, void *arg1, void *arg2, void *arg3)
{
	return (kauth_authorize_action(kauth_builtin_scope_process, cred,
	    action, p, arg1, arg2, arg3));
}

/*
 * Network scope authorization wrapper.
 */
int
kauth_authorize_network(kauth_cred_t cred, kauth_action_t action,
    enum kauth_network_req req, void *arg1, void *arg2, void *arg3)
{
	return (kauth_authorize_action(kauth_builtin_scope_network, cred,
	    action, (void *)req, arg1, arg2, arg3));
}

int
kauth_authorize_machdep(kauth_cred_t cred, kauth_action_t action,
    void *arg0, void *arg1, void *arg2, void *arg3)
{
	return (kauth_authorize_action(kauth_builtin_scope_machdep, cred,
	    action, arg0, arg1, arg2, arg3));
}

int
kauth_authorize_device(kauth_cred_t cred, kauth_action_t action,
    void *arg0, void *arg1, void *arg2, void *arg3)
{
	return (kauth_authorize_action(kauth_builtin_scope_device, cred,
	    action, arg0, arg1, arg2, arg3));
}

int
kauth_authorize_device_tty(kauth_cred_t cred, kauth_action_t action,
    struct tty *tty)
{
	return (kauth_authorize_action(kauth_builtin_scope_device, cred,
	    action, tty, NULL, NULL, NULL));
}

int
kauth_authorize_device_spec(kauth_cred_t cred, enum kauth_device_req req,
    struct vnode *vp)
{
	return (kauth_authorize_action(kauth_builtin_scope_device, cred,
	    KAUTH_DEVICE_RAWIO_SPEC, (void *)req, vp, NULL, NULL));
}

int
kauth_authorize_device_passthru(kauth_cred_t cred, dev_t dev, u_long bits,
    void *data)
{
	return (kauth_authorize_action(kauth_builtin_scope_device, cred,
	    KAUTH_DEVICE_RAWIO_PASSTHRU, (void *)bits, (void *)(u_long)dev,
	    data, NULL));
}

static int
kauth_cred_hook(kauth_cred_t cred, kauth_action_t action, void *arg0,
    void *arg1)
{
	int r;

	r = kauth_authorize_action(kauth_builtin_scope_cred, cred, action,
	    arg0, arg1, NULL, NULL);

#ifdef DIAGNOSTIC
	if (!SIMPLEQ_EMPTY(&kauth_builtin_scope_cred->listenq))
		KASSERT(r == 0);
#endif /* DIAGNOSTIC */

	return (r);
}

void
secmodel_register(void)
{
	KASSERT(nsecmodels + 1 != 0);

	rw_enter(&kauth_lock, RW_WRITER);
	nsecmodels++;
	rw_exit(&kauth_lock);
}

void
secmodel_deregister(void)
{
	KASSERT(nsecmodels != 0);

	rw_enter(&kauth_lock, RW_WRITER);
	nsecmodels--;
	rw_exit(&kauth_lock);
}
