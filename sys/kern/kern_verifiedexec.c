/*	$NetBSD: kern_verifiedexec.c,v 1.18 2005/05/28 16:37:20 elad Exp $	*/

/*-
 * Copyright 2005 Elad Efrat <elad@bsd.org.il>
 * Copyright 2005 Brett Lymn <blymn@netbsd.org>
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Brett Lymn and Elad Efrat
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Neither the name of The NetBSD Foundation nor the names of its
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
__KERNEL_RCSID(0, "$NetBSD: kern_verifiedexec.c,v 1.18 2005/05/28 16:37:20 elad Exp $");

#include <sys/param.h>
#include <sys/mount.h>
#include <sys/malloc.h>
#include <sys/vnode.h>
#include <sys/namei.h>
#include <sys/exec.h>
#include <sys/proc.h>
#include <sys/syslog.h>
#include <sys/sysctl.h>
#define VERIEXEC_NEED_NODE
#include <sys/verified_exec.h>
#if defined(__FreeBSD__)
# include <sys/systm.h>
# include <sys/imgact.h>
# include <crypto/sha1.h>
#else
# include <sys/sha1.h>
#endif
#include "crypto/sha2/sha2.h"
#include "crypto/ripemd160/rmd160.h"
#include <sys/md5.h>

int veriexec_verbose = 0;
int veriexec_strict = 0;

char *veriexec_fp_names = NULL;
unsigned int veriexec_name_max = 0;

struct sysctlnode *veriexec_count_node = NULL;

/* Veriexecs table of hash types and their associated information. */
LIST_HEAD(veriexec_ops_head, veriexec_fp_ops) veriexec_ops_list;

/*
 * Add fingerprint names to the global list.
 */
static void
veriexec_add_fp_name(char *name)
{
	char *newp;
	unsigned int new_max;

	if (name == NULL)
		return;

	if (veriexec_fp_names == NULL) {
		veriexec_name_max = (VERIEXEC_TYPE_MAXLEN + 1) * 6;
		veriexec_fp_names = malloc(veriexec_name_max, M_TEMP, M_WAITOK);
		memset(veriexec_fp_names, 0, veriexec_name_max);
	}

	if ((strlen(veriexec_fp_names) + VERIEXEC_TYPE_MAXLEN + 1) >=
	    veriexec_name_max) {
		new_max = veriexec_name_max + 4 * (VERIEXEC_TYPE_MAXLEN + 1);
		newp = realloc(veriexec_fp_names, new_max, M_TEMP, M_WAITOK);
		veriexec_fp_names = newp;
		veriexec_name_max = new_max;
	}

	strlcat(veriexec_fp_names, name, veriexec_name_max);
	strlcat(veriexec_fp_names, " ", veriexec_name_max);
}

/*
 * Add ops to the fignerprint ops vector list.
 */
int veriexec_add_fp_ops(struct veriexec_fp_ops *ops)
{
	if (ops == NULL)
		return (EFAULT);

	if ((ops->init == NULL) ||
	    (ops->update == NULL) ||
	    (ops->final == NULL))
		return (EFAULT);

	ops->type[sizeof(ops->type) - 1] = '\0';

#ifdef DIAGNOSTIC
	if (veriexec_find_ops(ops->type) != NULL)
		return (EEXIST);
#endif /* DIAGNOSTIC */

	LIST_INSERT_HEAD(&veriexec_ops_list, ops, entries);
	veriexec_add_fp_name(ops->type);

	return (0);
}

/*
 * Initialise the internal "default" fingerprint ops vector list.
 */
void
veriexec_init_fp_ops(void)
{
	struct veriexec_fp_ops *ops = NULL;

	LIST_INIT(&veriexec_ops_list);

#ifdef VERIFIED_EXEC_FP_RMD160
	ops = (struct veriexec_fp_ops *) malloc(sizeof(*ops), M_TEMP, M_WAITOK);
	VERIEXEC_OPINIT(ops, "RMD160", RMD160_DIGEST_LENGTH,
			sizeof(RMD160_CTX), RMD160Init, RMD160Update,
			RMD160Final);
	(void) veriexec_add_fp_ops(ops);
#endif /* VERIFIED_EXEC_FP_RMD160 */

#ifdef VERIFIED_EXEC_FP_SHA256
	ops = (struct veriexec_fp_ops *) malloc(sizeof(*ops), M_TEMP, M_WAITOK);
	VERIEXEC_OPINIT(ops, "SHA256", SHA256_DIGEST_LENGTH,
			sizeof(SHA256_CTX), SHA256_Init, SHA256_Update,
			SHA256_Final);
	(void) veriexec_add_fp_ops(ops);
#endif /* VERIFIED_EXEC_FP_SHA256 */

#ifdef VERIFIED_EXEC_FP_SHA384
	ops = (struct veriexec_fp_ops *) malloc(sizeof(*ops), M_TEMP, M_WAITOK);
	VERIEXEC_OPINIT(ops, "SHA384", SHA384_DIGEST_LENGTH,
			sizeof(SHA384_CTX), SHA384_Init, SHA384_Update,
			SHA384_Final);
	(void) veriexec_add_fp_ops(ops);
#endif /* VERIFIED_EXEC_FP_SHA384 */

#ifdef VERIFIED_EXEC_FP_SHA512
	ops = (struct veriexec_fp_ops *) malloc(sizeof(*ops), M_TEMP, M_WAITOK);
	VERIEXEC_OPINIT(ops, "SHA512", SHA512_DIGEST_LENGTH,
			sizeof(SHA512_CTX), SHA512_Init, SHA512_Update,
			SHA512_Final);
	(void) veriexec_add_fp_ops(ops);
#endif /* VERIFIED_EXEC_FP_SHA512 */

#ifdef VERIFIED_EXEC_FP_SHA1
	ops = (struct veriexec_fp_ops *) malloc(sizeof(*ops), M_TEMP, M_WAITOK);
	VERIEXEC_OPINIT(ops, "SHA1", SHA1_DIGEST_LENGTH,
			sizeof(SHA1_CTX), SHA1Init, SHA1Update,
			SHA1Final);
	(void) veriexec_add_fp_ops(ops);
#endif /* VERIFIED_EXEC_FP_SHA1 */

#ifdef VERIFIED_EXEC_FP_MD5
	ops = (struct veriexec_fp_ops *) malloc(sizeof(*ops), M_TEMP, M_WAITOK);
	VERIEXEC_OPINIT(ops, "MD5", MD5_DIGEST_LENGTH, sizeof(MD5_CTX),
			MD5Init, MD5Update, MD5Final);
	(void) veriexec_add_fp_ops(ops);
#endif /* VERIFIED_EXEC_FP_MD5 */
}

struct veriexec_fp_ops *
veriexec_find_ops(u_char *name)
{
	struct veriexec_fp_ops *ops;

	name[VERIEXEC_TYPE_MAXLEN] = '\0';
	
	LIST_FOREACH(ops, &veriexec_ops_list, entries) {
		if ((strlen(name) == strlen(ops->type)) &&
		    (strncasecmp(name, ops->type, sizeof(ops->type) - 1)
		     == 0))
			return (ops);
	}

	return (NULL);
}

/*
 * Calculate fingerprint. Information on hash length and routines used is
 * extracted from veriexec_hash_list according to the hash type.
 */
int
veriexec_fp_calc(struct proc *p, struct vnode *vp,
		 struct veriexec_hash_entry *vhe, uint64_t size, u_char *fp)
{
	void *ctx = NULL;
	u_char *buf = NULL;
	off_t offset, len;
	size_t resid;
	int error = 0;

	/* XXX: This should not happen. Print more details? */
	if (vhe->ops == NULL) {
		panic("veriexec: Operations vector is NULL");
	}

	bzero(fp, vhe->ops->hash_len);


	ctx = (void *) malloc(vhe->ops->context_size, M_TEMP, M_WAITOK);
	buf = (u_char *) malloc(PAGE_SIZE, M_TEMP, M_WAITOK);

	(vhe->ops->init)(ctx); /* init the fingerprint context */

	/*
	 * The vnode is locked. sys_execve() does it for us; We have our
	 * own locking in vn_open().
	 */
	for (offset = 0; offset < size; offset += PAGE_SIZE) {
		len = ((size - offset) < PAGE_SIZE) ? (size - offset)
			: PAGE_SIZE;

		error = vn_rdwr(UIO_READ, vp, buf, len, offset, 
				UIO_SYSSPACE,
#ifdef __FreeBSD__
				IO_NODELOCKED,
#else
				0,
#endif
				p->p_ucred, &resid, NULL);

		if (error)
			goto bad;

		  /* calculate fingerprint for each chunk */
		(vhe->ops->update)(ctx, buf, (unsigned int) len);
	}

	  /* finalise the fingerprint calculation */
	(vhe->ops->final)(fp, ctx);

bad:
	free(ctx, M_TEMP);
	free(buf, M_TEMP);

	return (error);
}
	
/* Compare two fingerprints of the same type. */
int
veriexec_fp_cmp(struct veriexec_hash_entry *vhe, u_char *digest)
{
#ifdef VERIFIED_EXEC_DEBUG
	int i;

	if (veriexec_verbose > 1) {
		printf("comparing hashes...\n");
		printf("vhe->fp: ");
		for (i = 0; i < vhe->ops->hash_len; i++) {
			printf("%x", vhe->fp[i]);
		}
		printf("\ndigest: ");
		for (i = 0; i < vhe->ops->hash_len; i++) {
			printf("%x", digest[i]);
		}
		printf("\n");
	}
#endif

	return (memcmp(vhe->fp, digest, vhe->ops->hash_len));
}

/* Get the hash table for the specified device. */
struct veriexec_hashtbl *
veriexec_tblfind(dev_t device) {
	struct veriexec_hashtbl *tbl;

	LIST_FOREACH(tbl, &veriexec_tables, hash_list) {
		if (tbl->hash_dev == device)
			return (tbl);
	}

	return (NULL);
}

/* Perform a lookup on a hash table. */
struct veriexec_hash_entry *
veriexec_lookup(dev_t device, ino_t inode)
{
	struct veriexec_hashtbl *tbl;
	struct veriexec_hashhead *tble;
	struct veriexec_hash_entry *e;
	size_t indx;

	tbl = veriexec_tblfind(device);
	if (tbl == NULL)
		return (NULL);

	indx = VERIEXEC_HASH(tbl, inode);
	tble = &(tbl->hash_tbl[indx & VERIEXEC_HASH_MASK(tbl)]);

	LIST_FOREACH(e, tble, entries) {
		if ((e != NULL) && (e->inode == inode))
			return (e);
	}

	return (NULL);
}

/*
 * Add an entry to a hash table. If a collision is found, handle it.
 * The passed entry is allocated in kernel memory.
 */
int
veriexec_hashadd(struct veriexec_hashtbl *tbl, struct veriexec_hash_entry *e)
{
	struct veriexec_hashhead *vhh;
	size_t indx;

	if (tbl == NULL)
		return (EFAULT);

	indx = VERIEXEC_HASH(tbl, e->inode);
	vhh = &(tbl->hash_tbl[indx]);

	if (vhh == NULL)
		panic("veriexec: veriexec_hashadd: vhh is NULL.");

	LIST_INSERT_HEAD(vhh, e, entries);

	tbl->hash_count++;

	return (0);
}

/*
 * Verify the fingerprint of the given file. If we're called directly from
 * sys_execve(), 'flag' will be VERIEXEC_DIRECT. If we're called from
 * exec_script(), 'flag' will be VERIEXEC_INDIRECT.  If we are called from
 * vn_open(), 'flag' will be VERIEXEC_FILE.
 */
int
veriexec_verify(struct proc *p, struct vnode *vp, struct vattr *va,
		const u_char *name, int flag)
{
	struct veriexec_hash_entry *vhe;
        u_char *digest;
        int error = 0;

	/* Evaluate fingerprint if needed and set the status on the vp. */
	if (vp->fp_status == FINGERPRINT_NOTEVAL) {
		vhe = veriexec_lookup(va->va_fsid, va->va_fileid);
		if (vhe == NULL) {
			vp->fp_status = FINGERPRINT_NOENTRY;
			goto out;
		}
 
		veriexec_dprintf(("veriexec: veriexec_verify: Got entry for "
				  "%s. (dev=%d, inode=%u)\n", name,
				  va->va_fsid, va->va_fileid));

		/* Calculate fingerprint for the inode. */
		digest = (u_char *) malloc(vhe->ops->hash_len, M_TEMP,
					   M_WAITOK);
		error = veriexec_fp_calc(p, vp, vhe, va->va_size, digest);
		
		if (error) {
			veriexec_dprintf(("veriexec: veriexec_verify: "
					  "Calculation error.\n"));
			free(digest, M_TEMP);
			return (error);
		}

		if (veriexec_fp_cmp(vhe, digest) == 0) {
			if (vhe->type == VERIEXEC_INDIRECT) {
				vp->fp_status = FINGERPRINT_INDIRECT;
			} else {
				vp->fp_status = FINGERPRINT_VALID;
			}
		} else {
			vp->fp_status = FINGERPRINT_NOMATCH;
		}
		free(digest, M_TEMP);
	}

out:
        switch (vp->fp_status) {
	case FINGERPRINT_NOTEVAL:
		/* Should not happen. */
		veriexec_report("veriexec_verify: Not-evaluated status "
		    "post evaluation; inconsistency detected.", name, va,
		    NULL, REPORT_NOVERBOSE, REPORT_NOALARM, REPORT_PANIC);

	case FINGERPRINT_VALID:
		/* Valid fingerprint. */
		veriexec_report("veriexec_verify: Match.", name, va, NULL,
		    REPORT_VERBOSE, REPORT_NOALARM, REPORT_NOPANIC);

		break;

	case FINGERPRINT_INDIRECT:
		/* Fingerprint is okay; Make sure it's indirect execution. */
		veriexec_report("veriexec_verify: Match. [indirect]",
		    name, va, NULL, REPORT_VERBOSE, REPORT_NOALARM,
		    REPORT_NOPANIC);

		if (flag == VERIEXEC_DIRECT) {
			veriexec_report("veriexec_verify: Direct "
			    "execution.", name, va, NULL,
			    REPORT_NOVERBOSE, REPORT_ALARM,
			    REPORT_NOPANIC);

			if (veriexec_strict > 0)
				error = EPERM;
		}

		break;

	case FINGERPRINT_NOMATCH:
		/* Fingerprint mismatch. Deny execution. */
		veriexec_report("veriexec_verify: Mismatch.", name, va,
		    NULL, REPORT_NOVERBOSE, REPORT_ALARM, REPORT_NOPANIC);

		if (veriexec_strict > 0)
			error = EPERM;

		break;

	case FINGERPRINT_NOENTRY:
		/* No entry in the list. */
		veriexec_report("veriexec_verify: No entry.", name, va,
		    p, REPORT_VERBOSE, REPORT_NOALARM, REPORT_NOPANIC);

		/* We don't care about these in learning mode. */
		if (veriexec_strict < 1) {
			break;
		}

		/*
		 * Deny access to files with no entry if
		 *   - File is being executed, and we're in strict
		 *     level 1; or
		 *   - File is being accessed, and we're in strict
		 *     level 2.
		 */
		if ((veriexec_strict > 1) ||
		    ((veriexec_strict == 1) &&
		    ((flag == VERIEXEC_FILE) || (flag == VERIEXEC_INDIRECT))))
			error = EPERM;

		break;

	default:
		/*
		 * Should never happen.
		 * XXX: Print vnode/process?
		 */
		veriexec_report("veriexec_verify: Invalid stats "
		    "post evaluation.", name, va, NULL, REPORT_NOVERBOSE,
		    REPORT_NOALARM, REPORT_PANIC);
        }

	return (error);
}

/*
 * Veriexec remove policy code. If we have an entry for the file in our
 * tables, we disallow removing if the securelevel is high or we're in
 * strict mode.
 */
int
veriexec_removechk(struct proc *p, struct vnode *vp, const char *pathbuf)
{
	struct veriexec_hashtbl *tbl;
	struct veriexec_hash_entry *vhe = NULL;
	struct vattr va;
	int error;

	error = VOP_GETATTR(vp, &va, p->p_ucred, p);
	if (error)
		return (error);

	/*
	 * Evaluate fingerprint to eliminate FINGERPRINT_NOTEVAL.
	 * The flag here should have no affect on the return value.
	 */
	error = veriexec_verify(p, vp, &va, pathbuf, VERIEXEC_FILE);
	if (error) {
		return (error);
	}

	switch (vp->fp_status) {
	case FINGERPRINT_VALID:
	case FINGERPRINT_INDIRECT:
	case FINGERPRINT_NOMATCH:
		if (veriexec_strict > 0) {
			veriexec_report("veriexec_removechk: Denying "
			    "unlink.", pathbuf, &va, p, REPORT_NOVERBOSE,
			    REPORT_ALARM, REPORT_NOPANIC);

			error = EPERM;
		} else {
			veriexec_report("veriexec_removechk: Removing "
			    "entry.", pathbuf, &va, NULL,
			    REPORT_NOVERBOSE, REPORT_NOALARM,
			    REPORT_NOPANIC);
			
			goto veriexec_rm;
		}

		break;

	case FINGERPRINT_NOENTRY:
		if (veriexec_strict > 1) {
			veriexec_report("veriexec_removechk: Denying "
			    "unlink. [strict]", pathbuf, &va, p,
			    REPORT_NOVERBOSE, REPORT_ALARM, REPORT_NOPANIC);

			error = EPERM;
		}

		break;

	default:
		veriexec_report("veriexec_removechk: Invalid status post "
		    "evaluation; inconsistency detected.", pathbuf, &va,
		    NULL, REPORT_NOVERBOSE, REPORT_NOALARM, REPORT_PANIC);
	}

	return (error);

veriexec_rm:
	vhe = veriexec_lookup(va.va_fsid, va.va_fileid);
	if (vhe == NULL) {
		veriexec_report("veriexec_removechk: Inconsistency "
		    "detected: Trying to remove entry without having one.",
		    pathbuf, &va, NULL, REPORT_NOVERBOSE, REPORT_NOALARM,
		    REPORT_PANIC);
	}

	tbl = veriexec_tblfind(va.va_fsid);
	if (tbl == NULL) {
		veriexec_report("veriexec_removechk: Inconsistency "
		    "detected: Could not get table for file in lists.",
		    pathbuf, &va, NULL, REPORT_NOVERBOSE, REPORT_NOALARM,
		    REPORT_PANIC);
	}

	LIST_REMOVE(vhe, entries);
	free(vhe->fp, M_TEMP);
	free(vhe, M_TEMP);
	tbl->hash_count--;

	return (error);
}

/*
 * Routine for maintaining mostly consistent message formats in Verified
 * Exec.
 *
 * 'verbose_only' - if 1, the message will be printed only if veriexec is
 * in verbose mode.
 * 'alarm' - if 1, the message is considered an alarm and will be printed
 * at all times along with pid and user credentials.
 * 'die' - if 1, the system will call panic() instead of printf().
 */
void
veriexec_report(const u_char *msg, const u_char *filename,
		struct vattr *va, struct proc *p, int verbose_only,
		int alarm, int die)
{
	void (*f)(const char *, ...);

	if (msg == NULL || filename == NULL || va == NULL)
		return;

	if (die)
		f = panic;
	else
		f = (void (*)(const char *, ...)) printf;

	if (!verbose_only || veriexec_verbose) {
		if (!alarm || p == NULL)
			f("veriexec: %s [%s, %d:%u%s", msg, filename,
			    va->va_fsid, va->va_fileid,
			    die ? "]" : "]\n");
		else
			f("veriexec: %s [%s, %d:%u, pid=%u, uid=%u, "
			    "gid=%u%s", msg, filename, va->va_fsid,
			    va->va_fileid, p->p_pid, p->p_cred->p_ruid,
			    p->p_cred->p_rgid, die ? "]" : "]\n");
	}
}
