/*	$NetBSD: overlay_vnops.c,v 1.3 2001/01/22 12:17:39 jdolecek Exp $	*/

/*
 * Copyright (c) 1999, 2000 National Aeronautics & Space Administration
 * All rights reserved.
 *
 * This software was written by William Studenmund of the
 * Numerical Aerospace Similation Facility, NASA Ames Research Center.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the National Aeronautics & Space Administration
 *    nor the names of its contributors may be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NATIONAL AERONAUTICS & SPACE ADMINISTRATION
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE ADMINISTRATION OR CONTRIB-
 * UTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
/*
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * John Heidemann of the UCLA Ficus project.
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
 *	@(#)null_vnops.c	8.6 (Berkeley) 5/27/95
 *
 * Ancestors:
 *	@(#)lofs_vnops.c	1.2 (Berkeley) 6/18/92
 *	$Id: overlay_vnops.c,v 1.3 2001/01/22 12:17:39 jdolecek Exp $
 *	...and...
 *	@(#)null_vnodeops.c 1.20 92/07/07 UCLA Ficus project
 */

/*
 * Overlay Layer
 *
 * (See mount_overlay(8) for more information.)
 *
 * The overlay layer has two purposes.  First, it serves as a demonstration
 * of layering by proving a layer which really does nothing.  (the null
 * layer makes the underlying files appear elsewhere in the file hierarchy)
 * Second, the overlay layer can serve as a prototype layer. Since it
 * provides all necessary layer framework, new file system layers can be
 * created very easily be starting with an overlay layer.
 *
 * The remainder of this comment examines the overlay layer as a basis
 * for constructing new layers.
 *
 *
 * INSTANTIATING NEW OVERLAY LAYERS
 *
 * New null layers are created with mount_overlay(8).
 * Mount_overlay(8) takes two arguments, an ignored string
 * and the pathname which the overlay will mount over. After
 * the overlay layer is put into place, all access to the mount
 * point path will proceede through the overlay layer.
 *
 *
 * OPERATION OF AN OVERLAY LAYER
 *
 * The operation of an overlay layer is identical to that of a null
 * layer. See the null layer (and layerfs) documentation for more info.
 *
 *
 * CREATING OTHER FILE SYSTEM LAYERS
 *
 * One of the easiest ways to construct new file system layers is to make
 * a copy of either the null layer or the overlay layer, rename all files
 * and variables, and then begin modifing the copy.  Sed can be used to
 * easily rename all variables.
 *
 * The choice between using a null and an overlay layer depends on
 * the desirability of retaining access to the underlying filestore.
 * For instance, the umap filesystem presents both a uid-translated and an
 * untranslaged view of the underlying files, and so it is based off of
 * the null layer. However a layer implimenting Access Controll Lists
 * might prefer to block access to the underlying filestore, for which
 * the overlay layer is a better basis.
 *
 *
 * INVOKING OPERATIONS ON LOWER LAYERS
 *
 * See the null layer documentation.
 *
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/vnode.h>
#include <sys/mount.h>
#include <sys/namei.h>
#include <sys/malloc.h>
#include <sys/buf.h>
#include <miscfs/genfs/genfs.h>
#include <miscfs/overlay/overlay.h>
#include <miscfs/genfs/layer_extern.h>

/*
 * Global vfs data structures
 */
int (**overlay_vnodeop_p) __P((void *));
const struct vnodeopv_entry_desc overlay_vnodeop_entries[] = {
	{ &vop_default_desc,  layer_bypass },

	{ &vop_lookup_desc,   layer_lookup },
	{ &vop_setattr_desc,  layer_setattr },
	{ &vop_getattr_desc,  layer_getattr },
	{ &vop_access_desc,   layer_access },
	{ &vop_lock_desc,     layer_lock },
	{ &vop_unlock_desc,   layer_unlock },
	{ &vop_islocked_desc, layer_islocked },
	{ &vop_fsync_desc,    layer_fsync },
	{ &vop_inactive_desc, layer_inactive },
	{ &vop_reclaim_desc,  layer_reclaim },
	{ &vop_print_desc,    layer_print },

	{ &vop_open_desc,     layer_open },	/* mount option handling */

	{ &vop_strategy_desc, layer_strategy },
	{ &vop_bwrite_desc,   layer_bwrite },
	{ &vop_bmap_desc,     layer_bmap },

	{ (struct vnodeop_desc*)NULL, (int(*)__P((void *)))NULL }
};
const struct vnodeopv_desc overlay_vnodeop_opv_desc =
	{ &overlay_vnodeop_p, overlay_vnodeop_entries };
