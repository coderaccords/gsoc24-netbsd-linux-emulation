/*	$NetBSD: md.h,v 1.24 2009/04/05 00:50:52 tsutsui Exp $	*/

/*
 * Copyright 1997 Piermont Information Systems Inc.
 * All rights reserved.
 *
 * Written by Philip A. Nelson for Piermont Information Systems Inc.
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
 *      This product includes software developed for the NetBSD Project by
 *      Piermont Information Systems Inc.
 * 4. The name of Piermont Information Systems Inc. may not be used to endorse
 *    or promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY PIERMONT INFORMATION SYSTEMS INC. ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL PIERMONT INFORMATION SYSTEMS INC. BE 
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR 
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF 
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF 
 * THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

/* md.h -- Machine specific definitions for the i386 */


#include <machine/cpu.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/bootblock.h>
#include <fcntl.h>
#include <unistd.h>

/* i386 uses the mbr code. */
#include "mbr.h"

/* constants and defines */
#define	DEFUSRSIZE	0
#define	DEFSWAPSIZE	(-1)

/* use UFS2 by default for ffs */
#define	DEFAULT_UFS2
#define	HAVE_UFS2_BOOT

/* Megs required for a full X installation. */
#define XNEEDMB 50


/*
 *  Default filesets to fetch and install during installation
 *  or upgrade. The standard sets are:
 *      base etc comp games man misc tests text xbase xcomp xetc xfont xserver
 *
 * x86_64 has the  MD set kern first, because generic kernels are  too
 * big to fit on install floppies.
 *
 * Third entry is the last extension name in the split sets for loading
 * from floppy.
 */
#define SET_KERNEL_1_NAME	"kern-GENERIC"

#define SET_KERNEL_GENERIC	SET_KERNEL_1

/*
 * Disk names accepted as valid targets for a from-scratch installation.
 *
 * On amd64, we allow "wd"  ST-506/IDE disks,  "sd" scsi disks, "ld" logical
 * disks and "raid" raidframe disks.
 */
#define DISK_NAMES "wd", "sd", "ld", "raid:no_mbr", "xbd:no_mbr"

/*
 * Machine-specific command to write a new label to a disk.
 * For example, i386  uses "/sbin/disklabel -w -r", just like i386
 * miniroot scripts, though this may leave a bogus incore label.
 * Sun ports should probably use  DISKLABEL_CMD "/sbin/disklabel -w"
 * to get incore  to ondisk inode translation for the Sun proms.
 * If not defined, we assume the port does not support disklabels and
 * hand-edited disklabel will NOT be written by MI code.
 *
 * On amd64, do what the 1.2 install scripts did. 
 */
#define DISKLABEL_CMD "disklabel -w -r"

#define _PATH_MBR	DEFAULT_BOOTDIR "/" DEFAULT_BOOTCODE
#define _PATH_BOOTSEL	DEFAULT_BOOTDIR "/" DEFAULT_BOOTSELCODE

extern struct mbr_bootsel *mbs;
 

/*
 *  prototypes for MD code.
 */


