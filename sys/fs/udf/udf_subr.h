/* $NetBSD: udf_subr.h,v 1.6 2008/05/19 20:12:36 reinoud Exp $ */

/*
 * Copyright (c) 2006, 2008 Reinoud Zandijk
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
 * 
 */

#ifndef _FS_UDF_UDF_SUBR_H_
#define _FS_UDF_UDF_SUBR_H_

/* handies */
#define	VFSTOUDF(mp)	((struct udf_mount *)mp->mnt_data)


/* device information updating */
int udf_update_trackinfo(struct udf_mount *ump, struct mmc_trackinfo *trackinfo);
int udf_update_discinfo(struct udf_mount *ump);
int udf_search_tracks(struct udf_mount *ump, struct udf_args *args,
		  int *first_tracknr, int *last_tracknr);
int udf_search_writing_tracks(struct udf_mount *ump);
int udf_setup_writeparams(struct udf_mount *ump);
int udf_synchronise_caches(struct udf_mount *ump);

/* tags operations */
int udf_fidsize(struct fileid_desc *fid);
int udf_check_tag(void *blob);
int udf_check_tag_payload(void *blob, uint32_t max_length);
void udf_validate_tag_sum(void *blob);
void udf_validate_tag_and_crc_sums(void *blob);
int udf_tagsize(union dscrptr *dscr, uint32_t udf_sector_size);

/* read/write descriptors */
int udf_read_phys_dscr(
		struct udf_mount *ump,
		uint32_t sector,
		struct malloc_type *mtype,		/* where to allocate */
		union dscrptr **dstp);			/* out */

int udf_write_phys_dscr_sync(struct udf_mount *ump, struct udf_node *udf_node,
		int what, union dscrptr *dscr,
		uint32_t sector, uint32_t logsector);
int udf_write_phys_dscr_async(struct udf_mount *ump, struct udf_node *udf_node,
		      int what, union dscrptr *dscr,
		      uint32_t sector, uint32_t logsector,
		      void (*dscrwr_callback)(struct buf *));

/* read/write node descriptors */
int udf_create_logvol_dscr(struct udf_mount *ump, struct udf_node *udf_node,
	struct long_ad *icb, union dscrptr **dscrptr);
void udf_free_logvol_dscr(struct udf_mount *ump, struct long_ad *icb_loc,
	void *dscr);
int udf_read_logvol_dscr(struct udf_mount *ump, struct long_ad *icb,
	union dscrptr **dscrptr);
int udf_write_logvol_dscr(struct udf_node *udf_node, union dscrptr *dscr,
	struct long_ad *icb, int waitfor);


/* volume descriptors readers and checkers */
int udf_read_anchors(struct udf_mount *ump);
int udf_read_vds_space(struct udf_mount *ump);
int udf_process_vds(struct udf_mount *ump);
int udf_read_vds_tables(struct udf_mount *ump);
int udf_read_rootdirs(struct udf_mount *ump);

/* open/close and sync volumes */
int udf_open_logvol(struct udf_mount *ump);
int udf_close_logvol(struct udf_mount *ump, int mntflags);
int udf_writeout_vat(struct udf_mount *ump);
int udf_write_partition_spacetables(struct udf_mount *ump, int waitfor);
void udf_do_sync(struct udf_mount *ump, kauth_cred_t cred, int waitfor);

/* translation services */
int udf_translate_vtop(struct udf_mount *ump, struct long_ad *icb_loc,
		uint32_t *lb_numres, uint32_t *extres);
int udf_translate_file_extent(struct udf_node *node,
		uint32_t from, uint32_t num_lb, uint64_t *map);
void udf_get_adslot(struct udf_node *udf_node, int slot, struct long_ad *icb, int *eof);
int udf_append_adslot(struct udf_node *udf_node, int slot, struct long_ad *icb);

int udf_vat_read(struct udf_node *vat_node, uint8_t *blob, int size, uint32_t offset);
int udf_vat_write(struct udf_node *vat_node, uint8_t *blob, int size, uint32_t offset);

/* disc allocation */
void udf_late_allocate_buf(struct udf_mount *ump, struct buf *buf, uint64_t *lmapping, uint64_t *pmapping, struct long_ad *node_ad_cpy);
void udf_free_allocated_space(struct udf_mount *ump, uint32_t lb_num, uint16_t vpart_num, uint32_t num_lb);
int udf_pre_allocate_space(struct udf_mount *ump, int udf_c_type, int num_lb, uint16_t *alloc_partp, uint64_t *lmapping, uint64_t *pmapping);
int udf_grow_node(struct udf_node *node, uint64_t new_size);
int udf_shrink_node(struct udf_node *node, uint64_t new_size);

/* node readers and writers */
uint64_t udf_advance_uniqueid(struct udf_mount *ump);

#define UDF_LOCK_NODE(udf_node, flag) udf_lock_node(udf_node, (flag), __FILE__, __LINE__)
#define UDF_UNLOCK_NODE(udf_node, flag) udf_unlock_node(udf_node, (flag))
void udf_lock_node(struct udf_node *udf_node, int flag, char const *fname, const int lineno);
void udf_unlock_node(struct udf_node *udf_node, int flag);

int udf_get_node(struct udf_mount *ump, struct long_ad *icbloc, struct udf_node **noderes);
int udf_writeout_node(struct udf_node *udf_node, int waitfor);
int udf_dispose_node(struct udf_node *node);

/* node ops */
int udf_resize_node(struct udf_node *node, uint64_t new_size, int *extended);
int udf_extattr_search_intern(struct udf_node *node, uint32_t sattr, char const *sattrname, uint32_t *offsetp, uint32_t *lengthp);

/* node data buffer read/write */
void udf_read_filebuf(struct udf_node *node, struct buf *buf);
void udf_write_filebuf(struct udf_node *node, struct buf *buf);
void udf_fixup_fid_block(uint8_t *blob, int lb_size, int rfix_pos, int max_rfix_pos, uint32_t lb_num);
void udf_fixup_internal_extattr(uint8_t *blob, uint32_t lb_num);
void udf_fixup_node_internals(struct udf_mount *ump, uint8_t *blob, int udf_c_type);

/* device strategy */
void udf_discstrat_init(struct udf_mount *ump);
void udf_discstrat_finish(struct udf_mount *ump);
void udf_discstrat_queuebuf(struct udf_mount *ump, struct buf *nestbuf);

/* structure writers */
int udf_write_terminator(struct udf_mount *ump, uint32_t sector);

/* structure creators */
void udf_set_regid(struct regid *regid, char const *name);
void udf_add_domain_regid(struct udf_mount *ump, struct regid *regid);
void udf_add_udf_regid(struct udf_mount *ump, struct regid *regid);
void udf_add_impl_regid(struct udf_mount *ump, struct regid *regid);
void udf_add_app_regid(struct udf_mount *ump, struct regid *regid);

/* directory operations and helpers */
void udf_osta_charset(struct charspec *charspec);
int udf_read_fid_stream(struct vnode *vp, uint64_t *offset, struct fileid_desc *fid, struct dirent *dirent);
int udf_lookup_name_in_dir(struct vnode *vp, const char *name, int namelen, struct long_ad *icb_loc);
int udf_create_node(struct vnode *dvp, struct vnode **vpp, struct vattr *vap, struct componentname *cnp);
void udf_delete_node(struct udf_node *udf_node);

int udf_chsize(struct vnode *vp, u_quad_t newsize, kauth_cred_t cred);
int udf_dir_detach(struct udf_mount *ump, struct udf_node *dir_node, struct udf_node *udf_node, struct componentname *cnp);
int udf_dir_attach(struct udf_mount *ump, struct udf_node *dir_node, struct udf_node *udf_node, struct vattr *vap, struct componentname *cnp);

/* update and times */
void udf_add_to_dirtylist(struct udf_node *udf_node);
void udf_remove_from_dirtylist(struct udf_node *udf_node);
void udf_itimes(struct udf_node *udf_node, struct timespec *acc,
	struct timespec *mod, struct timespec *changed);
int  udf_update(struct vnode *node, struct timespec *acc,
	struct timespec *mod, int updflags);

/* helpers and converters */
long udf_calchash(struct long_ad *icbptr);    /* for `inode' numbering */
uint32_t udf_getaccessmode(struct udf_node *node);
void udf_setaccessmode(struct udf_node *udf_node, mode_t mode);
void udf_getownership(struct udf_node *udf_node, uid_t *uidp, gid_t *gidp);
void udf_setownership(struct udf_node *udf_node, uid_t uid, gid_t gid);

void udf_to_unix_name(char *result, int result_len, char *id, int len, struct charspec *chsp);
void unix_to_udf_name(char *result, uint8_t *result_len, char const *name, int name_len, struct charspec *chsp);

void udf_timestamp_to_timespec(struct udf_mount *ump, struct timestamp *timestamp, struct timespec *timespec);
void udf_timespec_to_timestamp(struct timespec *timespec, struct timestamp *timestamp);

/* vnode operations */
int udf_inactive(void *v);
int udf_reclaim(void *v);
int udf_readdir(void *v);
int udf_getattr(void *v);
int udf_setattr(void *v);
int udf_pathconf(void *v);
int udf_open(void *v);
int udf_close(void *v);
int udf_access(void *v);
int udf_read(void *v);
int udf_write(void *v);
int udf_trivial_bmap(void *v);
int udf_vfsstrategy(void *v);
int udf_lookup(void *v);
int udf_create(void *v);
int udf_mknod(void *v);
int udf_link(void *);
int udf_symlink(void *v);
int udf_readlink(void *v);
int udf_rename(void *v);
int udf_remove(void *v);
int udf_mkdir(void *v);
int udf_rmdir(void *v);
int udf_fsync(void *v);
int udf_advlock(void *v);

#endif	/* !_FS_UDF_UDF_SUBR_H_ */
