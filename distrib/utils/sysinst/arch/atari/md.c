/*	$NetBSD: md.c,v 1.7 2003/05/21 10:05:23 dsl Exp $ */

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

/* md.c -- Machine specific code for atari */

#include <stdio.h>
#include <util.h>
#include <sys/param.h>
#include <machine/cpu.h>
#include <sys/sysctl.h>
#include "defs.h"
#include "md.h"
#include "msg_defs.h"
#include "menu_defs.h"


int c1024_resp;
struct disklist *disklist = NULL;


/* prototypes */
int	make_bsd_partitions (void);



/*
 * md back-end code for menu-driven BSD disklabel editor.
 */
int
make_bsd_partitions(void)
{
	int i, part;
	int freepart;
	int remain;
	char isize[20];
	int maxpart = getmaxpartitions();
	struct disklabel l;
	int varsz = 0, swapsz = 0;
	int ptend;

	/*
	 * Initialize global variables that track space used on this disk.
	 * Standard 4.4BSD 8-partition labels always cover whole disk.
	 */
	ptsize = dlsize - ptstart;
	ptend = ptstart + ptsize;

	rammb = (ramsize + MEG - 1) / MEG;
	minfsdmb = (80 + 4*rammb) * (MEG / sectorsize);

	/* Ask for layout type -- standard or special */
	msg_display(MSG_md_layout,
		    (1.0*ptsize*sectorsize)/MEG,
		    (1.0*minfsdmb*sectorsize)/MEG,
		    (1.0*minfsdmb*sectorsize)/MEG+rammb+XNEEDMB);
	process_menu(MENU_md_layout);

	if (layoutkind == 3)
		ask_sizemult(dlcylsize);
	else
		md_set_sizemultname();

	/* Build standard partitions */
	emptylabel(bsdlabel);

	/* Set initial partition types to unused */
	for (part = 0 ; part < maxpart ; ++part)
		bsdlabel[part].pi_fstype = FS_UNUSED;

	/* We _always_ have a root partition */
	bsdlabel[PART_ROOT].pi_fstype = FS_BSDFFS;
	bsdlabel[PART_ROOT].pi_bsize  = 8192;
	bsdlabel[PART_ROOT].pi_fsize  = 1024;

	/* Whole disk partition */
	bsdlabel[PART_RAW].pi_offset = 0;
	bsdlabel[PART_RAW].pi_size = dlsize;

#ifdef PART_BSDRAW	/* BSD area of the disk - only on some (i386) ports */
	bsdlabel[PART_BSDRAW].pi_offset = ptstart;
	bsdlabel[PART_BSDRAW].pi_size = ptsize;
#endif

	switch (layoutkind) {
	case 1: /* standard: a root, b swap, c "unused", PART_USR /usr */
	case 2: /* standard X: as above, but with larger swap, and more
		 * space in /usr */
		partstart = ptstart;

		/*
		 * Defaults: 
		 *	root	- always (PART_ROOT)
		 *	swap	- ask, default (PART_SWAP)
		 *	/usr	- ask, default (PART_USR)
		 *	/var	- ask (PART_FIRST_FREE != PART_USR)
		 *	/home	- ask (PART_FIRST_FREE+1 != PART_USR)
		 *	/tmp	- ask mfs
		 */
		layout_swap = layout_usr = 1;
		layout_var = layout_home = layout_tmp = 0;
		freepart = PART_FIRST_FREE;
		process_menu(MENU_layoutparts);

		/* Root */
		if (layout_usr == 0 && layout_home == 0) {
			if (layout_swap) {
				i = NUMSEC(layoutkind * 2 *
				    (rammb < DEFSWAPRAM ? DEFSWAPRAM : rammb),
				    MEG/sectorsize, dlcylsize);
				swapsz = NUMSEC(i/(MEG/sectorsize)+1,
				    MEG/sectorsize, dlcylsize);
			}
			if (layout_var)
				varsz = NUMSEC(DEFVARSIZE, MEG/sectorsize,
				    dlcylsize);
			partsize = ptsize - swapsz - varsz;
		} else if (layout_usr)
			partsize = NUMSEC(DEFROOTSIZE, MEG/sectorsize,
			    dlcylsize);
		else if (layoutkind == 2)
			partsize = NUMSEC(STDNEEDMB + XNEEDMB, MEG/sectorsize,
			    dlcylsize);
		else
			partsize = NUMSEC(STDNEEDMB, MEG/sectorsize, dlcylsize);
		
		bsdlabel[PART_ROOT].pi_offset = partstart;
		bsdlabel[PART_ROOT].pi_size = partsize;
		bsdlabel[PART_ROOT].pi_bsize = 8192;
		bsdlabel[PART_ROOT].pi_fsize = 1024;
		strcpy(fsmount[PART_ROOT], "/");
		partstart += partsize;
#if notyet
		if (partstart > ptsize)
			error ... 
#endif

		/* swap */
		if (layout_swap) {
			if (swapsz)
				partsize = swapsz;
			else {
				i = NUMSEC(layoutkind * 2 *
				    (rammb < DEFSWAPRAM ? DEFSWAPRAM : rammb),
				    MEG/sectorsize, dlcylsize) + partstart;
				partsize = NUMSEC(i/(MEG/sectorsize)+1,
				    MEG/sectorsize, dlcylsize) - partstart;
			}
			bsdlabel[PART_SWAP].pi_fstype = FS_SWAP;
			bsdlabel[PART_SWAP].pi_offset = partstart;
			bsdlabel[PART_SWAP].pi_size = partsize;
			partstart += partsize;
#if notyet
			if (partstart > ptsize)
				error ... 
#endif
		}

		/* var */
		if (layout_var) {
			if (varsz)
				partsize = varsz;
			else
				partsize = NUMSEC(DEFVARSIZE, MEG/sectorsize,
				    dlcylsize);
			bsdlabel[freepart].pi_fstype = FS_BSDFFS;
			bsdlabel[freepart].pi_offset = partstart;
			bsdlabel[freepart].pi_size = partsize;
			bsdlabel[freepart].pi_bsize = 8192;
			bsdlabel[freepart].pi_fsize = 1024;
			strcpy(fsmount[freepart++], "/var");
			if (freepart == PART_USR)
				freepart++;
			partstart += partsize;
#if notyet
			if (partstart > ptsize)
				error ... 
#endif
		}

		/* /usr */
		if (layout_usr) {
			if (layout_home) {
				if (layoutkind == 2)
					partsize = NUMSEC(DEFUSRSIZE + XNEEDMB,
					    MEG/sectorsize, dlcylsize);
				else
					partsize = NUMSEC(DEFUSRSIZE,
					    MEG/sectorsize, dlcylsize);
			} else
				partsize = ptend - partstart;
			bsdlabel[PART_USR].pi_fstype = FS_BSDFFS;
			bsdlabel[PART_USR].pi_offset = partstart;
			bsdlabel[PART_USR].pi_size = partsize;
			bsdlabel[PART_USR].pi_bsize = 8192;
			bsdlabel[PART_USR].pi_fsize = 1024;
			strcpy(fsmount[PART_USR], "/usr");
			partstart += partsize;
#if notyet
			if (partstart > ptsize)
				error ... 
#endif
		}

		/* home */
		if (layout_home) {
			partsize = ptend - partstart;
			bsdlabel[freepart].pi_fstype = FS_BSDFFS;
			bsdlabel[freepart].pi_offset = partstart;
			bsdlabel[freepart].pi_size = partsize;
			bsdlabel[freepart].pi_bsize = 8192;
			bsdlabel[freepart].pi_fsize = 1024;
			strcpy(fsmount[freepart++], "/home");
			if (freepart == PART_USR)
				freepart++;
			partstart += partsize;
#if notyet
			if (partstart > ptsize)
				error ... 
#endif
		}

		break;

	case 3: /* custom: ask user for all sizes */
		ask_sizemult(dlcylsize);
		/* root */
		partstart = ptstart;
		remain = ptend - partstart;
		partsize = NUMSEC(DEFROOTSIZE, MEG/sectorsize, dlcylsize);
		snprintf(isize, 20, "%d", partsize/sizemult);
		msg_prompt(MSG_askfsroot, isize, isize, 20,
			    remain/sizemult, multname);
		partsize = NUMSEC(atoi(isize),sizemult, dlcylsize);
		/* If less than a 'unit' left, also use it */
		if (remain - partsize < sizemult)
			partsize = remain;
		bsdlabel[PART_ROOT].pi_offset = partstart;
		bsdlabel[PART_ROOT].pi_size = partsize;
		bsdlabel[PART_ROOT].pi_bsize = 8192;
		bsdlabel[PART_ROOT].pi_fsize = 1024;
		strcpy(fsmount[PART_ROOT], "/");
		partstart += partsize;

		/* swap */
		remain = ptend - partstart;
		if (remain > 0) {
			i = NUMSEC(2 *
				   (rammb < DEFSWAPRAM ? DEFSWAPRAM : rammb),
				   MEG/sectorsize, dlcylsize) + partstart;
			partsize = NUMSEC(i/(MEG/sectorsize)+1, MEG/sectorsize,
				   dlcylsize) - partstart;
			if (partsize > remain)
				partsize = remain;
			snprintf(isize, 20, "%d", partsize/sizemult);
			msg_prompt_add(MSG_askfsswap, isize, isize, 20,
				    remain/sizemult, multname);
			partsize = NUMSEC(atoi(isize),sizemult, dlcylsize);
			if (partsize) {
				/* If less than a 'unit' left, also use it */
				if (remain - partsize < sizemult)
					partsize = remain;
				bsdlabel[PART_SWAP].pi_fstype = FS_SWAP;
				bsdlabel[PART_SWAP].pi_offset = partstart;
				bsdlabel[PART_SWAP].pi_size = partsize;
				partstart += partsize;
			}
		}

		/* /usr */
		remain = ptend - partstart;
		if (remain > 0) {
			partsize = remain;
			snprintf(isize, 20, "%d", partsize/sizemult);
			msg_prompt_add(MSG_askfsusr, isize, isize, 20,
				    remain/sizemult, multname);
			partsize = NUMSEC(atoi(isize),sizemult, dlcylsize);
			if (partsize) {
				/* If less than a 'unit' left, also use it */
				if (remain - partsize < sizemult)
					partsize = remain;
				bsdlabel[PART_USR].pi_fstype = FS_BSDFFS;
				bsdlabel[PART_USR].pi_offset = partstart;
				bsdlabel[PART_USR].pi_size = partsize;
				bsdlabel[PART_USR].pi_bsize = 8192;
				bsdlabel[PART_USR].pi_fsize = 1024;
				strcpy(fsmount[PART_USR], "/usr");
				partstart += partsize;
			}
		}

		/* Others ... */
		remain = ptend - partstart;
		if (remain > 0)
			msg_display(MSG_otherparts);
		part = PART_FIRST_FREE;
		for (; remain > 0 && part < maxpart; ++part ) {
			if (bsdlabel[part].pi_fstype != FS_UNUSED)
				continue;
			partsize = ptend - partstart;
			snprintf(isize, 20, "%d", partsize/sizemult);
			msg_prompt_add(MSG_askfspart, isize, isize, 20,
					diskdev, partition_name(part),
					remain/sizemult, multname);
			partsize = NUMSEC(atoi(isize),sizemult, dlcylsize);
			/* If less than a 'unit' left, also use it */
			if (remain - partsize < sizemult)
				partsize = remain;
			bsdlabel[part].pi_fstype = FS_BSDFFS;
			bsdlabel[part].pi_offset = partstart;
			bsdlabel[part].pi_size = partsize;
			bsdlabel[part].pi_bsize = 8192;
			bsdlabel[part].pi_fsize = 1024;
			msg_prompt_add(MSG_mountpoint, NULL, fsmount[part],
				    20);
			partstart += partsize;
			remain = ptend - partstart;
		}

		break;

	case 4: /* use existing disklabel */

		if (get_real_geom(diskdev, &l) == 0) {
			msg_display(MSG_abort);	/* XXX more informative */
			return 0;
		}

		for (i = 0; i < maxpart; i++) {
#define p l.d_partitions[i]
			bsdlabel[i].pi_size = p.p_size;
			bsdlabel[i].pi_offset = p.p_offset;
			if ((i == PART_ROOT) && (p.p_size > 0))
				strcpy(fsmount[PART_ROOT], "/");
			if (i != RAW_PART) {
				bsdlabel[i].pi_fstype = p.p_fstype;
				bsdlabel[i].pi_bsize = p.p_fsize * p.p_frag;
				bsdlabel[i].pi_fsize = p.p_fsize;
				/* menu to get fsmount[] entry */
#undef p
			} else
				bsdlabel[i].pi_fstype = FS_UNUSED;
		}
		msg_display(MSG_postuseexisting);
		getchar();
		break;
	}

	/*
	 * OK, we have a partition table. Give the user the chance to
	 * edit it and verify it's OK, or abort altogether.
	 */
 edit_check:
	if (edit_and_check_label(bsdlabel, maxpart, RAW_PART, RAW_PART) == 0) {
		msg_display(MSG_abort);
		return 0;
	}
	if (md_check_partitions() == 0)
		goto edit_check;

	/* Disk name */
	msg_prompt(MSG_packname, "mydisk", bsddiskname, DISKNAME_SIZE);

	/*
	 * Setup the disktype so /etc/disktab gets proper info
	 */
	if (strncmp (disk->dd_name, "sd", 2) == 0)
	    disktype = "SCSI";
	else disktype = "ST506";

	/* save label to disk for MI code to update. */
	(void) savenewlabel(bsdlabel, maxpart);

	/* Everything looks OK. */
	return (1);
}

/*
 * any additional partition validation
 */
int
md_check_partitions(void)
{
	return 1;
}

int
md_get_info()
{
	return 1;
}

int
md_pre_disklabel()
{
	return 0;
}

int
md_post_disklabel(void)
{
	return 0;
}

int
md_post_newfs(void)
{
	/* boot blocks ... */
	msg_display(MSG_dobootblks, diskdev);
	return run_prog(RUN_DISPLAY, NULL,
	    "/usr/mdec/installboot -v /dev/r%sc",
	    diskdev);
}

int
md_copy_filesystem(void)
{
	return 0;
}

/*
 * md back-end code for menu-driven BSD disklabel editor.
 */
int
md_make_bsd_partitions(void)
{

	msg_display(MSG_infoahdilabel, diskdev);
	process_menu(MENU_md_noyes);
	if (yesno) {
		run_prog(RUN_DISPLAY, NULL, "ahdilabel /dev/r%sc", diskdev);
	}
	return(make_bsd_partitions());
}

/* Upgrade support */
int
md_update(void)
{
	move_aout_libs();
	endwin();
	md_copy_filesystem();
	md_post_newfs();
	wrefresh(curscr);
	wmove(stdscr, 0, 0);
	wclear(stdscr);
	wrefresh(stdscr);
	return 1;
}


void
md_cleanup_install(void)
{
	char realfrom[STRSIZE];
	char realto[STRSIZE];
	char sedcmd[STRSIZE];

	strncpy(realfrom, target_expand("/etc/rc.conf"), STRSIZE);
	strncpy(realto, target_expand("/etc/rc.conf.install"), STRSIZE);

	sprintf(sedcmd, "sed 's/rc_configured=NO/rc_configured=YES/' < %s > %s",
	    realfrom, realto);
	scripting_fprintf(logfp, "%s\n", sedcmd);
	do_system(sedcmd);

	run_prog(RUN_FATAL, NULL, "mv -f %s %s", realto, realfrom);
	run_prog(0, NULL, "rm -f %s", target_expand("/sysinst"));
	run_prog(0, NULL, "rm -f %s", target_expand("/.termcap"));
	run_prog(0, NULL, "rm -f %s", target_expand("/.profile"));
}

int
md_pre_update()
{
	return 1;
}

void
md_init()
{
}

void
md_set_sizemultname()
{

	set_sizemultname_meg();
}

void
md_set_no_x()
{

	toggle_getit(11);
	toggle_getit(12);
	toggle_getit(13);
	toggle_getit(14);
	toggle_getit(15);
	toggle_getit(16);
}
