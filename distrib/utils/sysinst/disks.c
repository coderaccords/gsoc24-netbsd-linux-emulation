/*	$NetBSD: disks.c,v 1.81 2004/04/25 17:15:27 dsl Exp $ */

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

/* disks.c -- routines to deal with finding disks and labeling disks. */


#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <util.h>

#include <sys/param.h>
#include <sys/swap.h>
#include <ufs/ufs/dinode.h>
#include <ufs/ffs/fs.h>
#define FSTYPENAMES
#define MOUNTNAMES
#define static
#include <sys/disklabel.h>
#undef static

#include "defs.h"
#include "md.h"
#include "msg_defs.h"
#include "menu_defs.h"
#include "txtwalk.h"

/* Disk descriptions */
#define MAX_DISKS 15
struct disk_desc {
	char dd_name[SSTRSIZE];
	struct disk_geom {
		int  dg_cyl;
		int  dg_head;
		int  dg_sec;
		int  dg_secsize;
		int  dg_totsec;
	} dg;
};
#define dd_cyl dg.dg_cyl
#define dd_head dg.dg_head
#define dd_sec dg.dg_sec
#define dd_secsize dg.dg_secsize
#define dd_totsec dg.dg_totsec

/* Local prototypes */
static int foundffs(struct data *, size_t);
static int mount_root(void);
static int fsck_preen(const char *, int, const char *);
static void fixsb(const char *, int);

#ifndef DISK_NAMES
#define DISK_NAMES "wd", "sd", "ld"
#endif

static const char *disk_names[] = { DISK_NAMES, "vnd", NULL };

static int
get_disks(struct disk_desc *dd)
{
	const char **xd;
	char d_name[SSTRSIZE];
	struct disklabel l;
	int i;
	int numdisks;

	/* initialize */
	numdisks = 0;

	for (xd = disk_names; *xd != NULL; xd++) {
		for (i = 0; i < MAX_DISKS; i++) {
			snprintf(d_name, sizeof d_name, "%s%d", *xd, i);
			if (!get_geom(d_name, &l)) {
				if (errno == ENOENT)
					break;
				continue;
			}
			strlcpy(dd->dd_name, d_name, sizeof dd->dd_name);
			dd->dd_cyl = l.d_ncylinders;
			dd->dd_head = l.d_ntracks;
			dd->dd_sec = l.d_nsectors;
			dd->dd_secsize = l.d_secsize;
			dd->dd_totsec = l.d_secperunit;
			dd++;
			numdisks++;
			if (numdisks >= MAX_DISKS)
				return numdisks;
		}
	}
	return numdisks;
}

static int
set_dsk_select(menudesc *m, void *arg)
{
	*(int *)arg = m->cursel;
	return 1;
}

int
find_disks(const char *doingwhat)
{
	struct disk_desc disks[MAX_DISKS];
	menu_ent dsk_menu[nelem(disks)];
	struct disk_desc *disk;
	int i;
	int numdisks;
	int selected_disk = 0;
	int menu_no;

	/* Find disks. */
	numdisks = get_disks(disks);

	/* need a redraw here, kernel messages hose everything */
	touchwin(stdscr);
	refresh();

	if (numdisks == 0) {
		/* No disks found! */
		msg_display(MSG_nodisk);
		process_menu(MENU_ok, NULL);
		/*endwin();*/
		return -1;
	}

	if (numdisks == 1) {
		/* One disk found! */
		msg_display(MSG_onedisk, disks[0].dd_name, doingwhat);
		process_menu(MENU_ok, NULL);
	} else {
		/* Multiple disks found! */
		for (i = 0; i < numdisks; i++) {
			dsk_menu[i].opt_name = disks[i].dd_name;
			dsk_menu[i].opt_menu = OPT_NOMENU;
			dsk_menu[i].opt_flags = OPT_EXIT;
			dsk_menu[i].opt_action = set_dsk_select;
		}
		menu_no = new_menu(MSG_Available_disks,
			dsk_menu, numdisks, -1, 4, 0, 0,
			MC_SCROLL | MC_NOEXITOPT,
			NULL, NULL, NULL, NULL, NULL);
		if (menu_no == -1)
			return -1;
		msg_display(MSG_ask_disk);
		process_menu(menu_no, &selected_disk);
		free_menu(menu_no);
	}

	disk = disks + selected_disk;
	strlcpy(diskdev, disk->dd_name, sizeof diskdev);

	sectorsize = disk->dd_secsize;
	dlcyl = disk->dd_cyl;
	dlhead = disk->dd_head;
	dlsec = disk->dd_sec;
	dlsize = disk->dd_totsec;
	if (dlsize == 0)
		dlsize = disk->dd_cyl * disk->dd_head * disk->dd_sec;
	dlcylsize = dlhead * dlsec;

	/* Get existing/default label */
	memset(&oldlabel, 0, sizeof oldlabel);
	incorelabel(diskdev, oldlabel);

	/* Set 'target' label to current label in case we don't change it */
	memcpy(&bsdlabel, &oldlabel, sizeof bsdlabel);

	return numdisks;
}

void
fmt_fspart(menudesc *m, int ptn, void *arg)
{
	int poffset, psize, pend;
	const char *desc;
	static const char *Yes, *No;
	partinfo *p = bsdlabel + ptn;

	if (Yes == NULL) {
		Yes = msg_string(MSG_Yes);
		No = msg_string(MSG_No);
	}

	poffset = p->pi_offset / sizemult;
	psize = p->pi_size / sizemult;
	if (psize == 0)
		pend = 0;
	else
		pend = (p->pi_offset + p->pi_size) / sizemult - 1;

	if (p->pi_fstype == FS_BSDFFS)
		if (p->pi_flags & PIF_FFSv2)
			desc = "FFSv2";
		else
			desc = "FFSv1";
	else
		desc = fstypenames[p->pi_fstype];

#ifdef PART_BOOT
	if (ptn == PART_BOOT)
		desc = msg_string(MSG_Boot_partition_cant_change);
#endif
	if (ptn == getrawpartition())
		desc = msg_string(MSG_Whole_disk_cant_change);
	else {
		if (ptn == PART_C)
			desc = msg_string(MSG_NetBSD_partition_cant_change);
	}

	wprintw(m->mw, msg_string(MSG_fspart_row),
			poffset, pend, psize, desc,
			p->pi_flags & PIF_NEWFS ? Yes : "",
			p->pi_flags & PIF_MOUNT ? Yes : "",
			p->pi_mount);
}

/*
 * Label a disk using an MD-specific string DISKLABEL_CMD for
 * to invoke disklabel.
 * if MD code does not define DISKLABEL_CMD, this is a no-op.
 *
 * i386 port uses "/sbin/disklabel -w -r", just like i386
 * miniroot scripts, though this may leave a bogus incore label.
 *
 * Sun ports should use DISKLABEL_CMD "/sbin/disklabel -w"
 * to get incore to ondisk inode translation for the Sun proms.
 */
int
write_disklabel (void)
{

#ifdef DISKLABEL_CMD
	/* disklabel the disk */
	return run_program(RUN_DISPLAY, "%s -f /tmp/disktab %s '%s'",
	    DISKLABEL_CMD, diskdev, bsddiskname);
#else
	return 0;
#endif
}


static int
ptn_sort(const void *a, const void *b)
{
	return strcmp(bsdlabel[*(const int *)a].pi_mount,
		      bsdlabel[*(const int *)b].pi_mount);
}

int
make_filesystems(void)
{
	int i;
	int ptn;
	int ptn_order[nelem(bsdlabel)];
	int error = 0;
	int maxpart = getmaxpartitions();
	char *newfs;
	const char *mnt_opts;
	const char *fsname;
	partinfo *lbl;

	if (maxpart > nelem(bsdlabel))
		maxpart = nelem(bsdlabel);

	/* Making new file systems and mounting them */

	/* sort to ensure /usr/local is mounted after /usr (etc) */
	for (i = 0; i < maxpart; i++)
		ptn_order[i] = i;
	qsort(ptn_order, maxpart, sizeof ptn_order[0], ptn_sort);

	for (i = 0; i < maxpart; i++) {
		/*
		 * newfs and mount. For now, process only BSD filesystems.
		 * but if this is the mounted-on root, has no mount
		 * point defined, or is marked preserve, don't touch it!
		 */
		ptn = ptn_order[i];
		lbl = bsdlabel + ptn;

		if (is_active_rootpart(diskdev, ptn))
			continue;

		if (*lbl->pi_mount == 0)
			/* No mount point */
			continue;

		newfs = NULL;
		mnt_opts = NULL;
		fsname = NULL;
		switch (lbl->pi_fstype) {
		case FS_APPLEUFS:
			asprintf(&newfs, "/sbin/newfs %s%.0d",
				lbl->pi_isize != 0 ? "-i" : "", lbl->pi_isize);
			mnt_opts = "-tffs -o async";
			fsname = "ffs";
			break;
		case FS_BSDFFS:
			asprintf(&newfs, "/sbin/newfs -O %d -b %d -f %d %s%.0d",
				lbl->pi_flags & PIF_FFSv2 ? 2 : 1,
				lbl->pi_fsize * lbl->pi_frag, lbl->pi_fsize,
				lbl->pi_isize != 0 ? "-i" : "", lbl->pi_isize);
			mnt_opts = "-tffs -o async";
			fsname = "ffs";
			break;
		case FS_BSDLFS:
			asprintf(&newfs, "/sbin/newfs_lfs -b %d",
				lbl->pi_fsize * lbl->pi_frag);
			mnt_opts = "-tlfs";
			fsname = "lfs";
			break;
		case FS_MSDOS:
			mnt_opts = "-tmsdos";
			fsname = "msdos";
			break;
		}
		if (lbl->pi_flags & PIF_NEWFS && newfs != NULL) {
			error = run_program(RUN_DISPLAY | RUN_PROGRESS,
			    "%s /dev/r%s%c", newfs, diskdev, 'a' + ptn);
		} else {
			/* We'd better check it isn't dirty */
			error = fsck_preen(diskdev, ptn, fsname);
		}
		free(newfs);
		if (error != 0)
			return error;

		if (lbl->pi_flags & PIF_MOUNT && mnt_opts != NULL) {
			make_target_dir(lbl->pi_mount);
			error = target_mount(mnt_opts, diskdev, ptn,
					    lbl->pi_mount);
			if (error) {
				msg_display(MSG_mountfail,
					    diskdev, 'a' + ptn, lbl->pi_mount);
				process_menu(MENU_ok, NULL);
				return error;
			}
		}
	}
	return 0;
}

int
make_fstab(void)
{
	FILE *f;
	int i, swap_dev = -1;

	/* Create the fstab. */
	make_target_dir("/etc");
	f = target_fopen("/etc/fstab", "w");
	if (logging)
		(void)fprintf(logfp,
		    "Creating %s/etc/fstab.\n", target_prefix());
	scripting_fprintf(NULL, "cat <<EOF >%s/etc/fstab\n", target_prefix());

	if (f == NULL) {
#ifndef DEBUG
		msg_display(MSG_createfstab);
		if (logging)
			(void)fprintf(logfp, "Failed to make /etc/fstab!\n");
		process_menu(MENU_ok, NULL);
		return 1;
#else
		f = stdout;
#endif
	}

	for (i = 0; i < getmaxpartitions(); i++) {
		const char *s = "";
		const char *mp = bsdlabel[i].pi_mount;
		const char *fstype = "ffs";
		int fsck_pass = 0, dump_freq = 0;

		if (!*mp) {
			/*
			 * No mount point specified, comment out line and
			 * use /mnt as a placeholder for the mount point.
			 */
			s = "# ";
			mp = "/mnt";
		}

		switch (bsdlabel[i].pi_fstype) {
		case FS_UNUSED:
			continue;
		case FS_BSDLFS:
			/* If there is no LFS, just comment it out. */
			if (check_lfs_progs())
				s = "# ";
			fstype = "lfs";
			/* FALLTHROUGH */
		case FS_BSDFFS:
			fsck_pass = (strcmp(mp, "/") == 0) ? 1 : 2;
			dump_freq = 1;
			break;
		case FS_MSDOS:
			fstype = "msdos";
			break;
		case FS_SWAP:
			if (swap_dev == -1)
				swap_dev = i;
			scripting_fprintf(f, "/dev/%s%c none swap sw 0 0\n",
				diskdev, 'a' + i);
			continue;
		default:
			fstype = "???";
			s = "# ";
			break;
		}
		scripting_fprintf(f, "%s/dev/%s%c %s %s rw%s%s%s%s%s%s%s%s %d %d\n",
		   s, diskdev, 'a' + i, mp, fstype,
		   bsdlabel[i].pi_flags & PIF_MOUNT ? "" : ",noauto",
		   bsdlabel[i].pi_flags & PIF_ASYNC ? ",async" : "",
		   bsdlabel[i].pi_flags & PIF_NOATIME ? ",noatime" : "",
		   bsdlabel[i].pi_flags & PIF_NODEV ? ",nodev" : "",
		   bsdlabel[i].pi_flags & PIF_NODEVMTIME ? ",nodevmtime" : "",
		   bsdlabel[i].pi_flags & PIF_NOEXEC ? ",noexec" : "",
		   bsdlabel[i].pi_flags & PIF_NOSUID ? ",nosuid" : "",
		   bsdlabel[i].pi_flags & PIF_SOFTDEP ? ",softdep" : "",
		   dump_freq, fsck_pass);
	}

	if (tmp_mfs_size != 0) {
		if (swap_dev != -1)
			scripting_fprintf(f, "/dev/%s%c /tmp mfs rw,-s=%d\n",
				diskdev, 'a' + swap_dev, tmp_mfs_size);
		else
			scripting_fprintf(f, "swap /tmp mfs rw,-s=%d\n",
				tmp_mfs_size);
	}

	/* Add /kern and /proc to fstab and make mountpoint. */
	scripting_fprintf(f, "kernfs /kern kernfs rw\n");
	scripting_fprintf(f, "procfs /proc procfs rw,noauto\n");
	make_target_dir("/kern");
	make_target_dir("/proc");

	scripting_fprintf(NULL, "EOF\n");

#ifndef DEBUG
	fclose(f);
	fflush(NULL);
#endif
	return 0;
}



static int
/*ARGSUSED*/
foundffs(struct data *list, size_t num)
{
	int error;

	if (num < 2 || strcmp(list[1].u.s_val, "/") == 0 ||
	    strstr(list[2].u.s_val, "noauto") != NULL)
		return 0;

	error = fsck_preen(list[0].u.s_val, ' '-'a', "ffs");
	if (error != 0)
		return error;

	error = target_mount("", list[0].u.s_val, ' '-'a', list[1].u.s_val);
	if (error != 0)
		return error;
	return 0;
}

/*
 * Do an fsck. On failure, inform the user by showing a warning
 * message and doing menu_ok() before proceeding.
 * Returns 0 on success, or nonzero return code from fsck() on failure.
 */
static int
fsck_preen(const char *disk, int ptn, const char *fsname)
{
	char *prog;
	int error;
	
	ptn += 'a';
	if (fsname == NULL)
		return 0;
	/* first check fsck program exists, if not assue ok */
	asprintf(&prog, "/sbin/fsck_%s", fsname);
	if (prog == NULL)
		return 0;
	if (access(prog, X_OK) != 0)
		return 0;
	if (!strcmp(fsname,"ffs"))
		fixsb(disk, ptn);
	error = run_program(0, "%s -p -q /dev/r%s%c", prog, disk, ptn);
	free(prog);
	if (error != 0) {
		msg_display(MSG_badfs, disk, ptn, error);
		process_menu(MENU_ok, NULL);
		/* XXX at this point maybe we should run a full fsck? */
	}
	return error;
}

/*
 * The import of FFSv2 was rather botched and many 1.6 'current' kernels
 * modified the superblock into a state part way between the 'old' format
 * and what an 'updated' superblock would have to look like.
 * This code attempts to detect, fix and reverse the error.
 * 1.6.1, 1.6.2 and the 2.0 release contain 'good' kernels...
 * This performs the same function as the etc/rc.d/fixsb script.
 * see NetBSD pr install/25138 for details.
 * (There are also problems with filesystems with 64k block size, which
 * fsck should fix itself.)
 */

static void
fixsb(const char *disk, int ptn)
{
	int fd;
	char diskpath[MAXPATHLEN];
	static char prog[] = "/sbin/fsck_ffs";
	int rval;
	uint64_t sblk[SBLOCKSIZE/sizeof(uint64_t)];
	struct fs *fs = (struct fs *)sblk;
	char *part;

	asprintf(&part, "%s%c", disk, ptn + 'a');
	fd = opendisk(part, O_RDONLY, diskpath, sizeof(diskpath), 0);
	free(part);
	if (fd == -1)
		return;

	/* Read ffsv1 main superblock */
	rval = pread(fd, sblk, sizeof sblk, SBLOCK_UFS1);
	close(fd);
	if (rval != sizeof sblk)
		return;

	if (fs->fs_magic != FS_UFS1_MAGIC &&
	    fs->fs_magic != FS_UFS1_MAGIC_SWAPPED)
		/* Not FFSv1 */
		return;
	if (fs->fs_old_flags & FS_FLAGS_UPDATED)
		/* properly updated fslevel 4 */
		return;
	if (fs->fs_bsize != fs->fs_maxbsize)
		/* not messed up */
		return;

	/*
	 * OK we have a munged fs, first 'upgrade' to fslevel 4,
	 * We specify -b16 in order to stop fsck bleating that the
	 * sb doesn't match the first alternate.
	 */
	if (run_program(0, "%s -p -q -b 16 -c 4 %s", prog, diskpath) != 0)
		return;
	/* Then downgrade to fslevel 3 */
	run_program(0, "%s -p -q -c 3 %s", prog, diskpath);
}

/*
 * fsck and mount the root partition.
 */
int
mount_root(void)
{
	int	error;

	error = fsck_preen(diskdev, rootpart, "ffs");
	if (error != 0)
		return error;

	/* Mount /dev/<diskdev>a on target's "".
	 * If we pass "" as mount-on, Prefixing will DTRT.
	 * for now, use no options.
	 * XXX consider -o remount in case target root is
	 * current root, still readonly from single-user?
	 */
	return target_mount("", diskdev, rootpart, "");
}

/* Get information on the file systems mounted from the root filesystem.
 * Offer to convert them into 4.4BSD inodes if they are not 4.4BSD
 * inodes.  Fsck them.  Mount them.
 */

int
mount_disks(void)
{
	char *fstab;
	int   fstabsize;
	int   i;
	int   error;

	static struct lookfor fstabbuf[] = {
		{"/dev/", "/dev/%s %s ffs %s", "c", NULL, 0, 0, foundffs},
		{"/dev/", "/dev/%s %s ufs %s", "c", NULL, 0, 0, foundffs},
	};
	static size_t numfstabbuf = sizeof(fstabbuf) / sizeof(struct lookfor);

	/* First the root device. */
	if (!target_already_root()) {
		error = mount_root();
		if (error != 0 && error != EBUSY)
			return 0;
	}

	/* Check the target /etc/fstab exists before trying to parse it. */
	if (target_dir_exists_p("/etc") == 0 ||
	    target_file_exists_p("/etc/fstab") == 0) {
		msg_display(MSG_noetcfstab, diskdev);
		process_menu(MENU_ok, NULL);
		return 0;
	}


	/* Get fstab entries from the target-root /etc/fstab. */
	fstabsize = target_collect_file(T_FILE, &fstab, "/etc/fstab");
	if (fstabsize < 0) {
		/* error ! */
		msg_display(MSG_badetcfstab, diskdev);
		process_menu(MENU_ok, NULL);
		return 0;
	}
	error = walk(fstab, (size_t)fstabsize, fstabbuf, numfstabbuf);
	free(fstab);

	return error;
}

int
set_swap(const char *disk, partinfo *pp)
{
	int i;
	char *cp;
	int rval;

	if (pp == NULL)
		pp = oldlabel;

	for (i = 0; i < MAXPARTITIONS; i++) {
		if (pp[i].pi_fstype != FS_SWAP)
			continue;
		asprintf(&cp, "/dev/%s%c", disk, 'a' + i);
		rval = swapctl(SWAP_ON, cp, 0);
		free(cp);
		if (rval != 0)
			return -1;
	}

	return 0;
}

int
check_swap(const char *disk, int remove_swap)
{
	struct swapent *swap;
	char *cp;
	int nswap;
	int l;
	int rval = 0;

	nswap = swapctl(SWAP_NSWAP, 0, 0);
	if (nswap <= 0)
		return 0;

	swap = malloc(nswap * sizeof *swap);
	if (swap == NULL)
		return -1;

	nswap = swapctl(SWAP_STATS, swap, nswap);
	if (nswap < 0)
		goto bad_swap;

	l = strlen(disk);
	while (--nswap >= 0) {
		/* Should we check the se_dev or se_path? */
		cp = swap[nswap].se_path;
		if (memcmp(cp, "/dev/", 5) != 0)
			continue;
		if (memcmp(cp + 5, disk, l) != 0)
			continue;
		if (!isalpha(*(unsigned char *)(cp + 5 + l)))
			continue;
		if (cp[5 + l + 1] != 0)
			continue;
		/* ok path looks like it is for this device */
		if (!remove_swap) {
			/* count active swap areas */
			rval++;
			continue;
		}
		if (swapctl(SWAP_OFF, cp, 0) == -1)
			rval = -1;
	}

    done:
	free(swap);
	return rval;

    bad_swap:
	rval = -1;
	goto done;
}
