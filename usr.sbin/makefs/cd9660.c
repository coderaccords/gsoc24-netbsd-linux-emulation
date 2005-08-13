/*	$NetBSD: cd9660.c,v 1.1 2005/08/13 01:53:01 fvdl Exp $	*/

/*
 * Copyright (c) 2005 Daniel Watt, Walter Deignan, Ryan Gabrys, Alan
 * Perez-Rathke and Ram Vedam.  All rights reserved.
 *
 * This code was written by Daniel Watt, Walter Deignan, Ryan Gabrys,
 * Alan Perez-Rathke and Ram Vedam.
 *
 * Redistribution and use in source and binary forms, with or
 * without modification, are permitted provided that the following
 * conditions are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials provided
 *    with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY DANIEL WATT, WALTER DEIGNAN, RYAN
 * GABRYS, ALAN PEREZ-RATHKE AND RAM VEDAM ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE 
 * DISCLAIMED.  IN NO EVENT SHALL DANIEL WATT, WALTER DEIGNAN, RYAN
 * GABRYS, ALAN PEREZ-RATHKE AND RAM VEDAM BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 * USE,DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY
 * OF SUCH DAMAGE.
 */
/*
 * Copyright (c) 2001 Wasabi Systems, Inc.
 * All rights reserved.
 *
 * Written by Luke Mewburn for Wasabi Systems, Inc.
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
 *      Wasabi Systems, Inc.
 * 4. The name of Wasabi Systems, Inc. may not be used to endorse
 *    or promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY WASABI SYSTEMS, INC. ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL WASABI SYSTEMS, INC
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
/*
 * Copyright (c) 1982, 1986, 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
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
  */

#if HAVE_NBTOOL_CONFIG_H
#include "nbtool_config.h"
#endif

#include <sys/cdefs.h>
#if defined(__RCSID) && !defined(__lint)
__RCSID("$NetBSD: cd9660.c,v 1.1 2005/08/13 01:53:01 fvdl Exp $");
#endif  /* !__lint */

#include <string.h>
#include <ctype.h>
#include <sys/cdefs.h>
#include <sys/param.h>
#include <sys/queue.h>

#if !HAVE_NBTOOL_CONFIG_H
#include <sys/mount.h>
#endif

#include "makefs.h"
#include "cd9660.h"
#include "cd9660/iso9660_rrip.h"

/*
 * Global variables
 */
iso9660_disk diskStructure;

static void cd9660_finalize_PVD(void);
static cd9660node *cd9660_allocate_cd9660node(void);
static void cd9660_set_defaults(void);
static int cd9660_arguments_set_string(const char *, const char *, int,
    char, char *);
static void cd9660_populate_iso_dir_record(
    struct _iso_directory_record_cd9660 *, u_char, u_char, u_char,
    const char *);
static void cd9660_setup_root_node(void);
static int cd9660_setup_volume_descriptors(void);
#if 0
static int cd9660_fill_extended_attribute_record(cd9660node *);
#endif
static int cd9960_translate_node_common(cd9660node *);
static int cd9660_translate_node(fsnode *, cd9660node *);
static int cd9660_compare_filename(const char *, const char *);
static cd9660node *cd9660_sorted_node_insert(cd9660node *, cd9660node *);
static int cd9660_handle_collisions(cd9660node *, int);
static cd9660node *cd9660_rename_filename(cd9660node *, int, int);
static void cd9660_copy_filenames(cd9660node *);
static void cd9660_sorting_nodes(cd9660node *);
static void cd9660_move_in_order(cd9660node *);
static int cd9660_count_collisions(cd9660node *);
static cd9660node *cd9660_rrip_move_directory(cd9660node *);
static int cd9660_add_dot_records(cd9660node *);

static cd9660node *cd9660_convert_structure(fsnode *, cd9660node *, int,
    int *, int *);
static void cd9660_free_structure(cd9660node *);
static int cd9660_generate_path_table(void);
static int cd9660_level1_convert_filename(const char *, char *, int);
static int cd9660_level2_convert_filename(const char *, char *, int);
#if 0
static int cd9660_joliet_convert_filename(const char *, char *, int);
#endif
static int cd9660_convert_filename(const char *, char *, int);
static void cd9660_populate_dot_records(cd9660node *);
static int cd9660_compute_offsets(cd9660node *, int);
#if 0
static int cd9660_copy_stat_info(cd9660node *, cd9660node *, int);
#endif
static cd9660node *cd9660_create_virtual_entry(const char *, cd9660node *, int,
    int);
static cd9660node *cd9660_create_file(const char *, cd9660node *);
static cd9660node *cd9660_create_directory(const char *, cd9660node *);
static cd9660node *cd9660_create_special_directory(u_char, cd9660node *);


/*
 * Allocate and initalize a cd9660node
 * @returns struct cd9660node * Pointer to new node, or NULL on error
 */
static cd9660node *
cd9660_allocate_cd9660node(void)
{
	cd9660node *temp;

	if ((temp = malloc(sizeof(cd9660node))) == NULL) {
		err(1,"Memory allocation error");
		return NULL;
	} else {
		temp->next = temp->prev = temp->child = 
			temp->parent = temp->last = 
			temp->dot_record = temp->dot_dot_record = 0;
		temp->ptnext = temp->ptprev = temp->ptlast = 0;
		temp->node = 0;
		temp->isoDirRecord = 0;
		temp->isoExtAttributes = 0;
		temp->rr_real_parent = temp->rr_relocated = 0;
		return temp;
	}
}

int cd9660_defaults_set = 0;

/**
* Set default values for cd9660 extension to makefs
*/
static void
cd9660_set_defaults(void)
{
	/*Fix the sector size for now, though the spec allows for other sizes*/
	diskStructure.sectorSize = 2048;
	
	/* Set up defaults in our own structure */
	diskStructure.verbose_level = 2;
	diskStructure.keep_bad_images = 0;
	diskStructure.follow_sym_links = 0;
	diskStructure.isoLevel = 2;

	diskStructure.rock_ridge_enabled = 0;
	diskStructure.rock_ridge_renamed_dir_name = 0;
	diskStructure.rock_ridge_move_count = 0;
	diskStructure.rr_moved_dir = 0;
	
	diskStructure.include_padding_areas = 1;
	
	/* Spec breaking functionality */
	diskStructure.allow_deep_trees = 
		diskStructure.allow_start_dot =
		diskStructure.allow_max_name =
		diskStructure.allow_illegal_chars =
		diskStructure.allow_lowercase =
		diskStructure.allow_multidot =
		diskStructure.omit_trailing_period = 0;
	
	/* Make sure the PVD is clear */
	memset(&diskStructure.primaryDescriptor, 0, 2048);
	
	memset(diskStructure.primaryDescriptor.volume_set_id,	0x20,32);
	memset(diskStructure.primaryDescriptor.publisher_id,	0x20,128);
	memset(diskStructure.primaryDescriptor.preparer_id,	0x20,128);
	memset(diskStructure.primaryDescriptor.application_id,	0x20,128);
	memset(diskStructure.primaryDescriptor.copyright_file_id, 0x20,128);
	memset(diskStructure.primaryDescriptor.abstract_file_id, 0x20,128);
	memset(diskStructure.primaryDescriptor.bibliographic_file_id, 0x20,128);
	
	strcpy(diskStructure.primaryDescriptor.system_id,"NetBSD");
	
	cd9660_defaults_set = 1;
	
	/* Boot support: Initially disabled */
	diskStructure.boot_image_directory = 0;
	/*memset(diskStructure.boot_descriptor, 0, 2048);*/
	
	diskStructure.is_bootable = 0;
	LIST_INIT(&diskStructure.boot_images);
	LIST_INIT(&diskStructure.boot_entries);
}

void
cd9660_prep_opts(fsinfo_t *fsopts)
{
	cd9660_set_defaults();
}

void
cd9660_cleanup_opts(fsinfo_t *fsopts)
{

}

static int
cd9660_arguments_set_string(const char *val, const char *fieldtitle, int length,
			    char testmode, char * dest)
{
	int len, test;

	if (val == NULL)
		warnx("error: The %s requires a string argument", fieldtitle);
	else if ((len = strlen(val)) <= length) {
		if (testmode == 'd')
			test = cd9660_valid_d_chars(val);
		else
			test = cd9660_valid_a_chars(val);
		if (test) {
			memcpy(dest, val, len);
			if (test == 2)
				cd9660_uppercase_characters(dest, len);
			return 1;
		} else
			warnx("error: The %s must be composed of "
			      "%c-characters", fieldtitle, testmode);
	} else
		warnx("error: The %s must be at most 32 characters long",
		    fieldtitle);
	return 0;
}

/*
 * Command-line parsing function
 */

int
cd9660_parse_opts(const char *option, fsinfo_t *fsopts)
{
	char *var, *val;
	int	rv;

	if (cd9660_defaults_set == 0)
		cd9660_set_defaults();

	/* Set up allowed options - integer options ONLY */
	option_t cd9660_options[] = {
		{ "l", &diskStructure.isoLevel, 1, 3, "ISO Level" },
		{ "isolevel", &diskStructure.isoLevel, 1, 3, "ISO Level" },
		{ "verbose",  &diskStructure.verbose_level, 0, 2,
		  "Turns on verbose output" },
		{ "v", &diskStructure.verbose_level, 0 , 2,
		  "Turns on verbose output"},
		{ NULL }
	};
	
	/*
	 * Todo : finish implementing this, and make a function that
	 * parses them
	 */
	/*
	string_option_t cd9660_string_options[] = {
		{ "L", "Label", &diskStructure.primaryDescriptor.volume_id, 1, 32, "Disk Label", ISO_STRING_FILTER_DCHARS },
		{ NULL }
	}
	*/
	
	assert(option != NULL);
	assert(fsopts != NULL);

	if (debug & DEBUG_FS_PARSE_OPTS)
		printf("cd9660_parse_opts: got `%s'\n", option);

	if ((var = strdup(option)) == NULL)
		err(1, "allocating memory for copy of option string");
	rv = 1;

	val = strchr(var, '=');
	if (val != NULL)
		*val++ = '\0';

	/* First handle options with no parameters */
	if (strcmp(var, "h") == 0) {
		diskStructure.displayHelp = 1;
		rv = 1;
	} else if (CD9660_IS_COMMAND_ARG_DUAL(var, "S", "follow-symlinks")) {
		/* this is not handled yet */
		diskStructure.follow_sym_links = 1;
		rv = 1;
	} else if (CD9660_IS_COMMAND_ARG_DUAL(var, "L", "label")) {
		rv = cd9660_arguments_set_string(val, "Disk Label", 32, 'd',
			diskStructure.primaryDescriptor.volume_id);
	} else if (CD9660_IS_COMMAND_ARG_DUAL(var, "A", "applicationid")) {
		rv = cd9660_arguments_set_string(val, "Application Identifier", 128, 'a',
			diskStructure.primaryDescriptor.application_id);
	} else if(CD9660_IS_COMMAND_ARG_DUAL(var, "P", "publisher")) {
		rv = cd9660_arguments_set_string(val, "Publisher Identifier",
			128, 'a', diskStructure.primaryDescriptor.publisher_id);
	} else if (CD9660_IS_COMMAND_ARG_DUAL(var, "p", "preparer")) {
		rv = cd9660_arguments_set_string(val, "Preparer Identifier",
		    128, 'a', diskStructure.primaryDescriptor.preparer_id);
	} else if (CD9660_IS_COMMAND_ARG_DUAL(var, "V", "volumeid")) {
		rv = cd9660_arguments_set_string(val, "Volume Set Identifier",
		    128, 'a', diskStructure.primaryDescriptor.volume_set_id);
	/* Boot options */
	} else if (CD9660_IS_COMMAND_ARG_DUAL(var, "B", "bootimage")) {
		if (val == NULL)
			warnx("error: The Boot Image parameter requires a valid boot information string");
		else
			rv = cd9660_add_boot_disk(val);
	} else if (CD9660_IS_COMMAND_ARG(var, "bootimagedir")) {
		/*
		 * XXXfvdl this is unused.
		 */
		if (val == NULL)
			errx(1, "error: The Boot Image Directory parameter"
			     " requires a directory name\n");
		else {
			if ((diskStructure.boot_image_directory =
			     malloc(strlen(val) + 1)) == NULL) {
				CD9660_MEM_ALLOC_ERROR("cd9660_parse_opts");
				exit(1);
			}
			
			/* BIG TODO: Add the max length function here */
			cd9660_arguments_set_string(val, "Boot Image Directory",
			    12 , 'd', diskStructure.boot_image_directory);
		}
	} else if (CD9660_IS_COMMAND_ARG(var, "no-trailing-padding"))
		diskStructure.include_padding_areas = 0;
	/* RRIP */
	else if (CD9660_IS_COMMAND_ARG_DUAL(var, "R", "rockridge"))
		diskStructure.rock_ridge_enabled = 1;
	else if (CD9660_IS_COMMAND_ARG_DUAL(var, "K", "keep-bad-images"))
		diskStructure.keep_bad_images = 1;
	else if (CD9660_IS_COMMAND_ARG(var, "allow-deep-trees"))
		diskStructure.allow_deep_trees = 1;
	else if (CD9660_IS_COMMAND_ARG(var, "allow-max-name"))
		diskStructure.allow_max_name = 1;
	else if (CD9660_IS_COMMAND_ARG(var, "allow-illegal-chars"))
		diskStructure.allow_illegal_chars = 1;
	else if (CD9660_IS_COMMAND_ARG(var, "allow-lowercase"))
		diskStructure.allow_lowercase = 1;
	else if (CD9660_IS_COMMAND_ARG(var,"allow-multidot"))
		diskStructure.allow_multidot = 1;
	else if (CD9660_IS_COMMAND_ARG(var, "omit-trailing-period"))
		diskStructure.omit_trailing_period = 0;
	else if (CD9660_IS_COMMAND_ARG(var, "no-emul-boot") ||
		    CD9660_IS_COMMAND_ARG(var, "no-boot") ||
		    CD9660_IS_COMMAND_ARG(var, "hard-disk-boot") ||
		    CD9660_IS_COMMAND_ARG(var, "boot-load-size") ||
		    CD9660_IS_COMMAND_ARG(var, "boot-load-segment")) {
		cd9660_eltorito_add_boot_option(var, val);
	}
		/* End of flag variables */
	else if (val == NULL) {
		warnx("Option `%s' doesn't contain a value", var);
		rv = 0;
	} else
		rv = set_option(cd9660_options, var, val);
		
	if (var)
		free(var);
	return (rv);
}

/*
 * Main function for cd9660_makefs
 * Builds the ISO image file
 * @param const char *image The image filename to create
 * @param const char *dir The directory that is being read
 * @param struct fsnode *root The root node of the filesystem tree
 * @param struct fsinfo_t *fsopts Any options
 */
void
cd9660_makefs(const char *image, const char *dir, fsnode *root,
	      fsinfo_t *fsopts)
{
	int startoffset;
	int numDirectories;
	int pathTableSectors;
	int firstAvailableSector;
	int totalSpace;
	int error;
	cd9660node *real_root;
	
	if (diskStructure.verbose_level > 0)
		printf("cd9660_makefs: ISO level is %i\n",
		    diskStructure.isoLevel);

	assert(image != NULL);
	assert(dir != NULL);
	assert(root != NULL);
	assert(fsopts != NULL);
	
	if (diskStructure.displayHelp) {
		/*
		 * Display help here - probably want to put it in
		 * a separate function
		 */
		return;
	}
	
	diskStructure.rootFilesystemPath = dir;
	
	if (diskStructure.verbose_level > 0)
		printf("cd9660_makefs: image %s directory %s root %p\n",
		    image, dir, root);

	/* Set up some constants. Later, these will be defined with options */

	/* Counter needed for path tables */
	numDirectories = 0;

	/* Convert tree to our own format */
	/* Actually, we now need to add the REAL root node, at level 0 */

	real_root = cd9660_allocate_cd9660node();
	if ((real_root->isoDirRecord = 
		malloc( sizeof(iso_directory_record_cd9660) )) == NULL) {
		CD9660_MEM_ALLOC_ERROR("cd9660_makefs");
		exit(1);
	}

	/* Leave filename blank for root */
	memset(real_root->isoDirRecord->name, 0,
	    ISO_FILENAME_MAXLENGTH_WITH_PADDING);

	real_root->level = 0;
	diskStructure.rootNode = real_root;
	real_root->type = CD9660_TYPE_DIR;
	error = 0;
	cd9660_convert_structure(root, real_root, 1, &numDirectories, &error);
	
	if (real_root->child == NULL) {
		errx(1, "cd9660_makefs: converted directory is empty. "
			"Tree conversion failed\n");
	} else if (error != 0) {
		errx(1, "cd9660_makefs: tree conversion failed\n");
	} else {
		if (diskStructure.verbose_level > 0)
			printf("cd9660_makefs: tree converted\n");
	}
		
	/* Add the dot and dot dot records */
	cd9660_add_dot_records(real_root);

	cd9660_setup_root_node();

	if (diskStructure.verbose_level > 0)
		printf("cd9660_makefs: done converting tree\n");

	/* Rock ridge / SUSP init pass */
	if (diskStructure.rock_ridge_enabled)
		cd9660_susp_initialize(diskStructure.rootNode);

	/* Build path table structure */
	diskStructure.pathTableLength = cd9660_generate_path_table();

	pathTableSectors = CD9660_BLOCKS(diskStructure.sectorSize,
		diskStructure.pathTableLength);
	
	firstAvailableSector = cd9660_setup_volume_descriptors();
	if (diskStructure.is_bootable) {
		firstAvailableSector = cd9660_setup_boot(firstAvailableSector);
		if (firstAvailableSector < 0)
			errx(1, "setup_boot failed");
	}
	/* LE first, then BE */
	diskStructure.primaryLittleEndianTableSector = firstAvailableSector;
	diskStructure.primaryBigEndianTableSector = 
		diskStructure.primaryLittleEndianTableSector + pathTableSectors;
	
	/* Set the secondary ones to -1, not going to use them for now */
	diskStructure.secondaryBigEndianTableSector = -1;
	diskStructure.secondaryLittleEndianTableSector = -1;
	
	diskStructure.dataFirstSector =
	    diskStructure.primaryBigEndianTableSector + pathTableSectors;
	if (diskStructure.verbose_level > 0)
		printf("cd9660_makefs: Path table conversion complete. "
		       "Each table is %i bytes, or %i sectors.\n",
		    diskStructure.pathTableLength, pathTableSectors);
	
	startoffset = diskStructure.sectorSize*diskStructure.dataFirstSector;

	totalSpace = cd9660_compute_offsets(real_root, startoffset);
	
	diskStructure.totalSectors = diskStructure.dataFirstSector +
		CD9660_BLOCKS(diskStructure.sectorSize, totalSpace);

	/* Disabled until pass 1 is done */
	if (diskStructure.rock_ridge_enabled) {
		diskStructure.susp_continuation_area_start_sector =
		    diskStructure.totalSectors;
		diskStructure.totalSectors +=
		    CD9660_BLOCKS(diskStructure.sectorSize,
			diskStructure.susp_continuation_area_size);
		cd9660_susp_finalize(diskStructure.rootNode);
	}


	cd9660_finalize_PVD();
	
	/* Add padding sectors, just for testing purposes right now */
	/* diskStructure.totalSectors+=150; */
	
	/* Debugging output */
	if (diskStructure.verbose_level > 0) {
		printf("cd9660_makefs: Sectors 0-15 reserved\n");
		printf("cd9660_makefs: Primary path tables starts in sector %i\n",
			diskStructure.primaryLittleEndianTableSector);	
		printf("cd9660_makefs: File data starts in sector %i\n",
			diskStructure.dataFirstSector);
		printf("cd9660_makefs: Total sectors: %i\n",diskStructure.totalSectors);
	}

	/*
	 * Add padding sectors at the end 
	 * TODO: Clean this up and separate padding
	 */
	if (diskStructure.include_padding_areas)
		diskStructure.totalSectors += 150;
	
	cd9660_write_image(image);

	if (diskStructure.verbose_level > 1) {
		debug_print_volume_descriptor_information();
		debug_print_tree(real_root,0);
		debug_print_path_tree(real_root);
	}
	
	/* Clean up data structures */
	cd9660_free_structure(real_root);

	if (diskStructure.verbose_level > 0)
		printf("cd9660_makefs: done\n");
}

/* Generic function pointer - implement later */
typedef int (*cd9660node_func)(cd9660node *);

static void
cd9660_finalize_PVD(void)
{
	time_t tim;
	unsigned char *temp;
	 
	/* Copy the root directory record */
	temp = (unsigned char *) &diskStructure.primaryDescriptor;
	
	/* root should be a fixed size of 34 bytes since it has no name */
	memcpy(diskStructure.primaryDescriptor.root_directory_record,
		diskStructure.rootNode->dot_record->isoDirRecord, 34);

	/* In RRIP, this might be longer than 34 */
	diskStructure.primaryDescriptor.root_directory_record[0] = 34;
	
	/* Set up all the important numbers in the PVD */
	cd9660_bothendian_dword(diskStructure.totalSectors,
	    (unsigned char *)diskStructure.primaryDescriptor.volume_space_size);
	cd9660_bothendian_word(1,
	    (unsigned char *)diskStructure.primaryDescriptor.volume_set_size);
	cd9660_bothendian_word(1,
	    (unsigned char *)
		diskStructure.primaryDescriptor.volume_sequence_number);
	cd9660_bothendian_word(diskStructure.sectorSize,
	    (unsigned char *)
		diskStructure.primaryDescriptor.logical_block_size);
	cd9660_bothendian_dword(diskStructure.pathTableLength,
	    (unsigned char *)diskStructure.primaryDescriptor.path_table_size);

	cd9660_731(diskStructure.primaryLittleEndianTableSector,
		(u_char *)diskStructure.primaryDescriptor.type_l_path_table);
	cd9660_732(diskStructure.primaryBigEndianTableSector,
		(u_char *)diskStructure.primaryDescriptor.type_m_path_table);
	
	diskStructure.primaryDescriptor.file_structure_version[0] = 1;
	
	/* Pad all strings with spaces instead of nulls */
	cd9660_pad_string_spaces(diskStructure.primaryDescriptor.volume_id, 32);
	cd9660_pad_string_spaces(diskStructure.primaryDescriptor.system_id, 32);
	cd9660_pad_string_spaces(diskStructure.primaryDescriptor.volume_set_id,
	    128);
	cd9660_pad_string_spaces(diskStructure.primaryDescriptor.publisher_id,
	    128);
	cd9660_pad_string_spaces(diskStructure.primaryDescriptor.preparer_id,
	    128);
	cd9660_pad_string_spaces(diskStructure.primaryDescriptor.application_id,
	    128);
	cd9660_pad_string_spaces(
	    diskStructure.primaryDescriptor.copyright_file_id, 128);
	cd9660_pad_string_spaces(
		diskStructure.primaryDescriptor.abstract_file_id, 128);
	cd9660_pad_string_spaces(
		diskStructure.primaryDescriptor.bibliographic_file_id, 128);
	
	/* Setup dates */
	time(&tim);
	cd9660_time_8426(
	    (unsigned char *)diskStructure.primaryDescriptor.creation_date,
	    tim);
	cd9660_time_8426(
	    (unsigned char *)diskStructure.primaryDescriptor.modification_date,
	    tim);

	/*
	cd9660_set_date(diskStructure.primaryDescriptor.expiration_date, now);
	*/
	
	memset(diskStructure.primaryDescriptor.expiration_date, '0' ,17);
	cd9660_time_8426(
	    (unsigned char *)diskStructure.primaryDescriptor.effective_date,
	    tim);
}

static void
cd9660_populate_iso_dir_record(struct _iso_directory_record_cd9660 *record,
			       u_char ext_attr_length, u_char flags,
			       u_char name_len, const char * name)
{
	record->ext_attr_length[0] = ext_attr_length;
	record->flags[0] = ISO_FLAG_CLEAR | flags;
	record->file_unit_size[0] = 0;
	record->interleave[0] = 0;
	cd9660_bothendian_word(1, record->volume_sequence_number);
	record->name_len[0] = name_len;
	memcpy(record->name, name, name_len);
	record->length[0] = 33 + name_len;

	/* Todo : better rounding */
	record->length[0] += (record->length[0] & 1) ? 1 : 0;
}

static void
cd9660_setup_root_node(void)
{
	cd9660_populate_iso_dir_record(diskStructure.rootNode->isoDirRecord,
	    0, ISO_FLAG_DIRECTORY, 1, "\0");
	
}

/*********** SUPPORT FUNCTIONS ***********/
static int
cd9660_setup_volume_descriptors(void)
{
	/* Boot volume descriptor should come second */
	int sector = 16;
	/* For now, a fixed 2 : PVD and terminator */
	volume_descriptor *temp, *t;

	/* Set up the PVD */
	if ((temp = malloc(sizeof(volume_descriptor))) == NULL) {
		CD9660_MEM_ALLOC_ERROR("cd9660_setup_volume_descriptors");
		exit(1);
	}

	temp->volumeDescriptorData =
	   (unsigned char *)&diskStructure.primaryDescriptor;
	temp->volumeDescriptorData[0] = ISO_VOLUME_DESCRIPTOR_PVD;
	temp->volumeDescriptorData[6] = 1;
	temp->sector = sector;
	memcpy(temp->volumeDescriptorData + 1,
	    ISO_VOLUME_DESCRIPTOR_STANDARD_ID, 5);
	diskStructure.firstVolumeDescriptor = temp;
	
	sector++;
	/* Set up boot support if enabled. BVD must reside in sector 17 */
	if (diskStructure.is_bootable) {
		if ((t = malloc(sizeof(volume_descriptor))) == NULL) {
			CD9660_MEM_ALLOC_ERROR(
			    "cd9660_setup_volume_descriptors");
			exit(1);
		}
		if ((t->volumeDescriptorData = malloc(2048)) == NULL) {
			CD9660_MEM_ALLOC_ERROR(
			    "cd9660_setup_volume_descriptors");
			exit(1);
		}
		temp->next = t;
		temp = t;
		memset(t->volumeDescriptorData, 0, 2048);
		t->sector = 17;
		if (diskStructure.verbose_level > 0)
			printf("Setting up boot volume descriptor\n");
		cd9660_setup_boot_volume_descritpor(t);
		sector++;
	}
	
	/* Set up the terminator */
	if ((t = malloc(sizeof(volume_descriptor))) == NULL) {
		CD9660_MEM_ALLOC_ERROR("cd9660_setup_volume_descriptors");
		exit(1);
	}
	if ((t->volumeDescriptorData = malloc(2048)) == NULL) {
		CD9660_MEM_ALLOC_ERROR("cd9660_setup_volume_descriptors");
		exit(1);
	}
	
	temp->next = t;
	memset(t->volumeDescriptorData, 0, 2048);
	t->volumeDescriptorData[0] = ISO_VOLUME_DESCRIPTOR_TERMINATOR;
	t->next = 0;
	t->volumeDescriptorData[6] = 1;
	t->sector = sector;
	memcpy(t->volumeDescriptorData + 1,
	    ISO_VOLUME_DESCRIPTOR_STANDARD_ID, 5);
	
	sector++;
	return sector;
}

#if 0
/*
 * Populate EAR at some point. Not required, but is used by NetBSD's
 * cd9660 support
 */
static int
cd9660_fill_extended_attribute_record(cd9660node *node)
{
	if ((node->isoExtAttributes =
	    malloc(sizeof(struct iso_extended_attributes))) == NULL) {
		CD9660_MEM_ALLOC_ERROR("cd9660_fill_extended_attribute_record");
		exit(1);
	};
	
	return 1;
}
#endif

static int
cd9960_translate_node_common(cd9660node *newnode)
{
	time_t tim;
	int test;
	u_char flag;
	char temp[ISO_FILENAME_MAXLENGTH_WITH_PADDING];

	/* Now populate the isoDirRecord structure */
	memset(temp, 0, ISO_FILENAME_MAXLENGTH_WITH_PADDING);

	test = cd9660_convert_filename(newnode->node->name,	
		temp, !(S_ISDIR(newnode->node->type)));
	
	flag = ISO_FLAG_CLEAR;
	if (S_ISDIR(newnode->node->type))
		flag |= ISO_FLAG_DIRECTORY;

	cd9660_populate_iso_dir_record(newnode->isoDirRecord, 0,
	    flag, strlen(temp), temp);
	
	/* Set the various dates */

	/* If we want to use the current date and time */
	time(&tim);

	cd9660_time_915(newnode->isoDirRecord->date, tim);

	cd9660_bothendian_dword(newnode->fileDataLength,
	    newnode->isoDirRecord->size);
	/* If the file is a link, we want to set the size to 0 */
	if (S_ISLNK(newnode->node->type))
		newnode->fileDataLength = 0;

	return 1;
}

/*
 * Translate fsnode to cd9960node
 * Translate filenames and other metadata, including dates, sizes,
 * permissions, etc
 * @param struct fsnode * The node generated by makefs
 * @param struct cd9660node * The intermediate node to be written to
 * @returns int 0 on failure, 1 on success
 */
static int
cd9660_translate_node(fsnode *node, cd9660node *newnode)
{
	if (node == NULL) {
		if (diskStructure.verbose_level > 0)
			printf("cd9660_translate_node: NULL node passed, "
			       "returning\n");
		return 0;
	}
	if ((newnode->isoDirRecord =
		malloc(sizeof(iso_directory_record_cd9660))) == NULL) {
		CD9660_MEM_ALLOC_ERROR("cd9660_translate_node");
		return 0;
	}

	/* Set the node pointer */
	newnode->node = node;

	/* Set the size */
	if (!(S_ISDIR(node->type)))
		newnode->fileDataLength = node->inode->st.st_size;
	
	if (cd9960_translate_node_common(newnode) == 0)
		return 0;

	/* Finally, overwrite some of the values that are set by default */
	cd9660_time_915(newnode->isoDirRecord->date, node->inode->st.st_mtime);
	
	return 1;
}

/*
 * Compares two ISO filenames
 * @param const char * The first file name
 * @param const char * The second file name
 * @returns : -1 if first is less than second, 0 if they are the same, 1 if
 * 	the second is greater than the first
 */
static int
cd9660_compare_filename(const char *first, const char *second)
{
	/*
	 * This can be made more optimal once it has been tested
	 * (the extra character, for example, is for testing)
	 */

	int p1 = 0;
	int p2 = 0;
	char c1, c2;
	/* First, on the filename */

	while (p1 < ISO_FILENAME_MAXLENGTH_BEFORE_VERSION-1
		&& p2 < ISO_FILENAME_MAXLENGTH_BEFORE_VERSION-1) {
		c1 = first[p1];
		c2 = second[p2];
		if (c1 == '.' && c2 =='.')
			break;
		else if (c1 == '.') {
			p2++;
			c1 = ' ';
		} else if (c2 == '.') {
			p1++;
			c2 = ' ';
		} else {
			p1++;
			p2++;
		}

		if (c1 < c2)
			return -1;
		else if (c1 > c2) {
			return 1;
		}
	}

	if (first[p1] == '.' && second[p2] == '.') {
		p1++;
		p2++;
		while (p1 < ISO_FILENAME_MAXLENGTH_BEFORE_VERSION - 1
			&& p2 < ISO_FILENAME_MAXLENGTH_BEFORE_VERSION - 1) {
			c1 = first[p1];
			c2 = second[p2];
			if (c1 == ';' && c2 == ';')
				break;
			else if (c1 == ';') {
				p2++;
				c1 = ' ';
			} else if (c2 == ';') {
				p1++;
				c2 = ' ';
			} else {
				p1++;
				p2++;
			}

			if (c1 < c2)
				return -1;
			else if (c1 > c2)
				return 1;
		}
	}
	return 0;
}

/*
 * Insert a node into list with ISO sorting rules
 * @param cd9660node * The head node of the list
 * @param cd9660node * The node to be inserted
 */
static cd9660node * 
cd9660_sorted_node_insert(cd9660node *head, cd9660node *newnode)
{
	cd9660node *temp;
	int compare;

	/* TODO: Optimize? */
	newnode->parent = head->parent;
	
	if (head == newnode) {
		head->parent->child = head;
		return head;
	}
	
	/* skip over the . and .. records */

	/*
	first = head->parent->child;
	if (first != 0) {
		if ((first->type & CD9660_TYPE_DOT) && (first->next != 0)) {
			if (first->next->type & CD9660_TYPE_DOTDOT) {
				printf("First is now DOTDOT\n");
				first = first->next;
			}
		}
	}
	*/
	
	/*
	 * first will either be 0, the . or the ..
	 * if . or .., this means no other entry may be written before first
	 * if 0, the new node may be inserted at the head
	 */
	
	temp = head;

	while (1) {
		/*
		 * Dont insert a node twice -
		 * that would cause an infinite loop
		 */
		assert(newnode != temp);
		
		compare = cd9660_compare_filename(newnode->isoDirRecord->name,
			temp->isoDirRecord->name);
		
		if (compare == 0)
			compare = cd9660_compare_filename(newnode->node->name,
				temp->node->name);
		
		if (compare < 0) {
			/*
			if (temp == first) {
				newnode->next = first->next;
				if (first->next != 0)
					first->next->prev = newnode;
				first->next = newnode;
				newnode->prev = first;
			} else {
			*/
				newnode->prev = temp->prev;
				if (temp->prev != 0)
					temp->prev->next = newnode;
				newnode->next = temp;
				temp->prev = newnode;
				
				/*test for inserting before the first child*/
				if (temp == head) {
					if (head->parent != 0)
						head->parent->child = newnode;
					return newnode;
				}
			/* } */
			return head;
		}
		if (temp->next == NULL) {
			temp->next = newnode;
			newnode->prev = temp;
			return head;
		}
		temp = temp->next;
	}
	return head;
}

/*
 * Called After cd9660_sorted_node_insert
 * handles file collisions by suffixing each filname with ~n
 * where n represents the files respective place in the ordering
 */
static int
cd9660_handle_collisions(cd9660node *colliding, int past)
{
	cd9660node *iter = colliding;
	int skip;
	int delete_chars = 0;
	int temp_past = past;
	int temp_skip;
	int flag = 0;
	cd9660node *end_of_range;
	
	while ((iter != 0) && (iter->next != 0)) {
		if ( strcmp(iter->isoDirRecord->name,
			iter->next->isoDirRecord->name) == 0) {
			flag = 1;
			temp_skip = skip = cd9660_count_collisions(iter);
			end_of_range = iter;
			while (temp_skip > 0){
				temp_skip--;
				end_of_range = end_of_range->next;
			}
			temp_past = past;
			while (temp_past > 0){
				if (end_of_range->next != 0)
					end_of_range = end_of_range->next;
				else if (iter->prev != 0)
					iter = iter->prev;
				else
					delete_chars++;
				temp_past--;
			}
			skip += past;
			iter = cd9660_rename_filename(iter, skip, delete_chars);
		} else
			iter = iter->next;
	}
	return flag;
}
 

static cd9660node *
cd9660_rename_filename(cd9660node *iter, int num, int delete_chars)
{
	int i = 0;
	int numbts, dot, semi, digit, digits, temp, powers, multiplier, count;
	char *naming;
	int maxlength;
        char *tmp;

	if (diskStructure.verbose_level > 0)
		printf("Rename_filename called\n");	

	/* TODO : A LOT of chanes regarding 8.3 filenames */
	if (diskStructure.isoLevel == 1)
		maxlength = 8;
	else if (diskStructure.isoLevel == 2)
		maxlength = 31;
	else
		maxlength = ISO_FILENAME_MAXLENGTH_BEFORE_VERSION;

	tmp = malloc(maxlength + 1);

	while (i < num) {
		powers = 1;
		count = 0;
		digits = 1;
		multiplier = 1;
		while (((int)(i / powers) ) >= 10) {
			digits++;
			powers = powers * 10;
		}
	
		naming = iter->o_name;

		/*
		while ((*naming != '.') && (*naming != ';')) {
			naming++;
			count++;
		}
		*/

		dot = -1;
		semi = -1;
		while (count < maxlength) {
			if (*naming == '.')
				dot = count;
			else if (*naming == ';') {
				semi = count;
				break;
			}
			naming++;
			count++;
		}
		
		if ((count + digits) < maxlength)
			numbts = count;
		else
			numbts = maxlength - (digits);
		numbts -= delete_chars;

		/* 8.3 rules - keep the extension, add before the dot */

		/*
		 * This code makes a bunch of assumptions.
		 * See if you can spot them all :)
		 */

		/*
		if (diskStructure.isoLevel == 1) {
			numbts = 8 - digits - delete_chars;
			if (dot < 0) {
				
			} else {
				if (dot < 8) {
					memmove(&tmp[numbts],&tmp[dot],4);
				}
			}
		}
		*/
		
		/* (copying just the filename before the '.' */
		memcpy(tmp, (iter->o_name), numbts);
	
		/* adding the appropriate number following the name */
		temp = i;
		while (digits > 0) {
			digit = (int)(temp / powers);
			temp = temp - digit * powers;
			sprintf(&tmp[numbts] , "%d", digit);
			digits--;
			numbts++;
			powers = powers / 10;
		}
	
		while ((*naming != ';')  && (numbts < maxlength)) {
			tmp[numbts] = (*naming);
			naming++;
			numbts++;
		}

		tmp[numbts] = ';';
		tmp[numbts+1] = '1';
	
		/*
		 * now tmp has exactly the identifier
		 * we want so we'll copy it back to record
		 */
		memcpy((iter->isoDirRecord->name), tmp, numbts + 2);  
	
		iter = iter->next;
		i++;
	}

	free(tmp);
	return iter;
}

/* Todo: Figure out why these functions are nec. */
static void 
cd9660_copy_filenames(cd9660node *node)
{
	cd9660node *temp;

	if (node == NULL)
		return;

	if (node->isoDirRecord == NULL) {
		debug_print_tree(diskStructure.rootNode, 0);
		exit(1);
	}

	temp = node;
	while (temp != NULL) {
		cd9660_copy_filenames(temp->child);
		memcpy(temp->o_name, temp->isoDirRecord->name,
		    ISO_FILENAME_MAXLENGTH_WITH_PADDING);
		temp = temp->next;
	}
}

static void
cd9660_sorting_nodes(cd9660node *node)
{
	cd9660node *temp;

	if (node == NULL)
		return;

	temp = node;
	while (temp != 0) {
		cd9660_sorting_nodes(temp->child);
		cd9660_move_in_order(temp);
		temp = temp->next;
	}
}

static void
cd9660_move_in_order(cd9660node *node)
{
	cd9660node * nxt;

	while (node->next != 0) {
		nxt = node->next;
		if (strcmp(nxt->isoDirRecord->name, node->isoDirRecord->name)
		    < 0) {
			if (node->parent->child == node)
				node->parent->child = nxt;
			if (node->prev != 0)
				node->prev->next = nxt;
			if (nxt->next != 0)
				nxt->next->prev = node;
			nxt->prev = node->prev;
				node->next = nxt->next;
			nxt->next = node;
			node->prev = nxt;
		} else
			return;
	}
	return;
}

static int
cd9660_count_collisions(cd9660node *copy)
{
	int count = 0;
	cd9660node *temp = copy;

	while (temp->next != 0) {
		if (cd9660_compare_filename(temp->isoDirRecord->name,
			temp->next->isoDirRecord->name) == 0)
				count ++;
		else
			return count;
		temp = temp->next;
	}
#if 0
	if (temp->next != NULL) {
		printf("cd9660_recurse_on_collision: count is %i \n", count);     
		compare = cd9660_compare_filename(temp->isoDirRecord->name,
			temp->next->isoDirRecord->name);
		if (compare == 0) {
			count++;
			return cd9660_recurse_on_collision(temp->next, count);
		} else
			return count;
	}
#endif
	return count;
}

static cd9660node *
cd9660_rrip_move_directory(cd9660node *dir)
{
	char newname[9];
	cd9660node *tfile;

	/*
	 * This function needs to:
	 * 1) Create an empty virtual file in place of the old directory
	 * 2) Point the virtual file to the new directory
	 * 3) Point the relocated directory to its old parent
	 * 4) Move the directory specified by dir into rr_moved_dir,
	 * and rename it to "diskStructure.rock_ridge_move_count" (as a string)
	 */

	/* First see if the moved directory even exists */
	if (diskStructure.rr_moved_dir == NULL) {
		diskStructure.rr_moved_dir =
			cd9660_create_directory(ISO_RRIP_DEFAULT_MOVE_DIR_NAME,
				diskStructure.rootNode);
		if (diskStructure.rr_moved_dir == NULL)
			return 0;
	}

	/* Create a file with the same ORIGINAL name */
	tfile = cd9660_create_file(dir->node->name, dir->parent);
	if (tfile == NULL)
		return NULL;
	
	diskStructure.rock_ridge_move_count++;
	sprintf(newname,"%08i", diskStructure.rock_ridge_move_count);

	/* Point to old parent */
	dir->rr_real_parent = dir->parent;

	/* Place the placeholder file */
	if (dir->rr_real_parent->child == NULL) {
		dir->rr_real_parent->child = tfile;
	} else {
		cd9660_sorted_node_insert(dir->rr_real_parent->child, tfile);
	}
	
	/* Point to new parent */
	dir->parent = diskStructure.rr_moved_dir;

	/* Point the file to the moved directory */
	tfile->rr_relocated = dir;

	/* Actually move the directory */
	if (diskStructure.rr_moved_dir->child == NULL) {
		diskStructure.rr_moved_dir->child = dir;
	} else {
		cd9660_sorted_node_insert(diskStructure.rr_moved_dir->child,
		    dir);
	}

	/* TODO: Inherit permissions / ownership (basically the entire inode) */

	/* Set the new name */
	memset(dir->isoDirRecord->name, 0, ISO_FILENAME_MAXLENGTH_WITH_PADDING);
	strncpy(dir->isoDirRecord->name, newname, 8);

	return dir;
}

static int
cd9660_add_dot_records(cd9660node *node)
{
	cd9660node *temp;

	temp = node;
	while (temp != 0) {
		if (temp->type & CD9660_TYPE_DIR) {
			/* Recursion first */
			if (temp->child != 0)
				cd9660_add_dot_records(temp->child);
			cd9660_create_special_directory(CD9660_TYPE_DOT, temp);
			cd9660_create_special_directory(CD9660_TYPE_DOTDOT,
			    temp);
		}
		temp = temp->next;
	}
	return 1;
}

/*
 * Convert node to cd9660 structure
 * This function is designed to be called recursively on the root node of
 * the filesystem
 * Lots of recursion going on here, want to make sure it is efficient
 * @param struct fsnode * The root node to be converted
 * @param struct cd9660* The parent node (should not be NULL)
 * @param int Current directory depth
 * @param int* Running count of the number of directories that are being created
 */
static cd9660node *
cd9660_convert_structure(fsnode *root, cd9660node *parent_node, int level,
			 int *numDirectories, int *error)
{
	cd9660node *first_node = 0;
	fsnode *iterator = root;
	cd9660node *temp_node;
	int working_level;
	int add;
	int flag = 0;
	int counter = 0;
	
	/*
	 * Newer, more efficient method, reduces recursion depth
	 */
	if (root == NULL) {
		printf("cd9660_convert_structure: root is null\n");
		return 0;
	}

	/* Test for an empty directory - makefs still gives us the . record */
	if ((S_ISDIR(root->type)) && (root->name[0] == '.')
		&& (root->name[1] == '\0')) {
		root = root->next;
		if (root == NULL) {
			return NULL;
		}
	}
	if ((first_node = cd9660_allocate_cd9660node()) == NULL) {
		CD9660_MEM_ALLOC_ERROR("cd9660_convert_structure");
		exit(1);
	}
	
	/*
	 * To reduce the number of recursive calls, we will iterate over
	 * the next pointers to the right.
	 */
	temp_node = first_node;
	parent_node->child = 0;
	while (iterator != 0) {
		add = 1;
		/*
		 * Increment the directory count if this is a directory
		 * Ignore "." entries. We will generate them later
		 */
		if (!((S_ISDIR(iterator->type)) &&
			(iterator->name[0] == '.' &&
			    iterator->name[1] == '\0'))) {
			
			if (S_ISDIR(iterator->type))
				(*numDirectories)++;

			/* Translate the node, including its filename */
			temp_node->parent = parent_node;
			cd9660_translate_node(iterator, temp_node);
			temp_node->level = level;

			if (S_ISDIR(iterator->type)) {
				temp_node->type = CD9660_TYPE_DIR;
				working_level = level + 1;

				/*
				 * If at level 8, directory would be at 8
				 * and have children at 9 which is not	
				 * allowed as per ISO spec
				 */
				if (level == 8) {
					if ((!diskStructure.allow_deep_trees) &&
					  (!diskStructure.rock_ridge_enabled)) {
						warnx("error: found entry "
						     "with depth greater "
						     "than 8.");
						(*error) = 1;
						return 0;
					} else if (diskStructure.
						   rock_ridge_enabled) {
						working_level = 3;
						/*	
						 * Moved directory is actually
						 * at level 2.
						 */
						temp_node->level =	
						    working_level - 1;
						if (cd9660_rrip_move_directory(
							temp_node) == 0) {
							warnx("Failure in "
							      "cd9660_rrip_"
							      "move_directory"
							);
							(*error) = 1;
							return 0;
						}
						add = 0;
					}
				}

				/* Do the recursive call on the children */
				if (iterator->child != 0) {
					cd9660_convert_structure(	
					    iterator->child, temp_node,
						working_level,
						numDirectories, error);
						
					if ((*error) == 1) {
						warnx("cd9660_convert_"
						       "structure: Error on "
						       "recursive call");
						return 0;
					}
				}
				
			} else {
				/* Only directories should have children */ 
				assert(iterator->child == NULL);
				
				temp_node->type = CD9660_TYPE_FILE;
			}
			
			/*
			 * Finally, do a sorted insert
			 */
			if (add) {
				/*
				 * Recompute firstnode, as a recursive call
				 * _might_ have changed it.
				 */

				/*
				 * We can probably get rid of this first_node
				 * variable.
				 */
				if (parent_node->child != 0)
					first_node = parent_node->child;
				first_node = cd9660_sorted_node_insert(
				    first_node, temp_node);
			}

			/*Allocate new temp_node */
			if (iterator->next != 0) {
				temp_node = cd9660_allocate_cd9660node();
				if (temp_node == NULL) {
					CD9660_MEM_ALLOC_ERROR("cd9660_convert_"
							       "structure");
					exit(1);
				}
			}
		}
		iterator = iterator->next;
	}

	/* cd9660_handle_collisions(first_node); */
	
	/* TODO: need cleanup */
	temp_node = first_node;
	cd9660_copy_filenames(first_node);
	
	do {
		flag = cd9660_handle_collisions(first_node, counter);
		counter++;
		cd9660_sorting_nodes(first_node);
	} while ((flag == 1) && (counter < 100));
	
	return first_node;
}

/*
 * Clean up the cd9660node tree
 * This is designed to be called recursively on the root node
 * @param struct cd9660node *root The node to free
 * @returns void
 */
static void
cd9660_free_structure(cd9660node *root)
{
#if 0
	cd9660node *temp = root;
	cd9660node *temp2;

	while (temp != NULL)
	{
		if (temp->child != NULL)
		{
			cd9660_free_structure(temp->child);
			free(temp->child);
		}
		temp2 = temp->next;
		if (temp != root)
		{
			free(temp);
		}
		temp = temp2;
	}
#endif
}

/*
 * Be a little more memory conservative:
 * instead of having the TAILQ_ENTRY as part of the cd9660node,
 * just create a temporary structure
 */
struct ptq_entry
{
	TAILQ_ENTRY(ptq_entry) ptq;
	cd9660node *node;
} *n;

#define PTQUEUE_NEW(n,s,r,t){\
	n = malloc(sizeof(struct s));	\
	if (n == NULL)	\
		return r; \
	n->node = t;\
}

/*
 * Generate the path tables
 * The specific implementation of this function is left as an exercise to the
 * programmer. It could be done recursively. Make sure you read how the path
 * table has to be laid out, it has levels.
 * @param struct iso9660_disk *disk The disk image
 * @returns int The number of built path tables (between 1 and 4), 0 on failure
 */
static int
cd9660_generate_path_table(void)
{
	cd9660node *dirNode = diskStructure.rootNode;
	cd9660node *last = dirNode;
	int pathTableSize = 0;	/* computed as we go */
	int counter = 1;	/* root gets a count of 0 */
	int parentRecNum = 0;	/* root's parent is '0' */
		
	TAILQ_HEAD(cd9660_pt_head, ptq_entry) pt_head;
	TAILQ_INIT(&pt_head);
	
	PTQUEUE_NEW(n, ptq_entry, -1, diskStructure.rootNode);

	/* Push the root node */
	TAILQ_INSERT_HEAD(&pt_head, n, ptq);
	
	/* Breadth-first traversal of file structure */
	while (pt_head.tqh_first != 0) {
		n = pt_head.tqh_first;
		dirNode = n->node;
		TAILQ_REMOVE(&pt_head, pt_head.tqh_first, ptq);
		free(n);

		/* Update the size */
		pathTableSize += ISO_PATHTABLE_ENTRY_BASESIZE
		    + dirNode->isoDirRecord->name_len[0]+
			(dirNode->isoDirRecord->name_len[0] % 2 == 0 ? 0 : 1);
			/* includes the padding bit */
		
		dirNode->ptnumber=counter;
		if (dirNode != last) {
			last->ptnext = dirNode;
			dirNode->ptprev = last;
		}
		last = dirNode;

		parentRecNum = 1;
		if (dirNode->parent != 0)
			parentRecNum = dirNode->parent->ptnumber;

		/* Push children onto queue */
		dirNode = dirNode->child;
		while (dirNode != 0) {
			/*
			 * Dont add the DOT and DOTDOT types to the path
			 * table.
			 */
			if ((dirNode->type != CD9660_TYPE_DOT)
				&& (dirNode->type != CD9660_TYPE_DOTDOT)) {
				
				if (S_ISDIR(dirNode->node->type)) {
					PTQUEUE_NEW(n, ptq_entry, -1, dirNode);
					TAILQ_INSERT_TAIL(&pt_head, n, ptq);
				}
			}
			dirNode = dirNode->next;
		}
		counter++;
	}
	
	return pathTableSize;
}

void
cd9660_compute_full_filename(cd9660node *node, char *buf, int level)
{
	cd9660node *parent;

	parent = (node->rr_real_parent == NULL ?
		  node->parent : node->rr_real_parent);
	if (parent != NULL) {
		cd9660_compute_full_filename(parent, buf, level + 1);
		strcat(buf, node->node->name);
	} else {
		/* We are at the root */
		strcat(buf, diskStructure.rootFilesystemPath);
		if (buf[strlen(buf) - 1] == '/')
			buf[strlen(buf) - 1] = '\0';
	}

	if (level != 0)
		strcat(buf, "/");
}

/* NEW filename conversion method */
typedef int(*cd9660_filename_conversion_functor)(const char *, char *, int);


/*
 * TODO: These two functions are almost identical.
 * Some code cleanup is possible here
 */
static int
cd9660_level1_convert_filename(const char *oldname, char *newname, int is_file)
{
	/*
	 * ISO 9660 : 10.1
	 * File Name shall not contain more than 8 d or d1 characters
	 * File Name Extension shall not contain more than 3 d or d1 characters
	 * Directory Identifier shall not contain more than 8 d or d1 characters
	 */
	int namelen = 0;
	int extlen = 0;
	int found_ext = 0;

	while (*oldname != '\0') {
		/* Handle period first, as it is special */
		if (*oldname == '.') {
			if (found_ext) {
				*(newname++) = '_';
				extlen ++;
			}
			else {
				*(newname++) = '.';
				found_ext = 1;
			}
		} else {
			/* Enforce 12.3 / 8 */
			if (((namelen == 8) && !found_ext) ||
			    (found_ext && extlen == 3)) {
				break;
			}

			if (islower((unsigned char)*oldname))
				*(newname++) = toupper((unsigned char)*oldname);
			else if (isupper((unsigned char)*oldname)
				    || isdigit((unsigned char)*oldname))
				*(newname++) = *oldname;
			else
				*(newname++) = '_';

			if (found_ext)
				extlen++;
			else
				namelen++;
		}
		oldname ++;
	}
	/* Add version */
	*(newname++) = ';';
	sprintf((newname++), "%i", 1);
	return namelen + extlen + found_ext;
}

static int
cd9660_level2_convert_filename(const char *oldname, char *newname, int is_file)
{
	/*
	 * ISO 9660 : 7.5.1
	 * File name : 0+ d or d1 characters
	 * separator 1 (.)
	 * File name extension : 0+ d or d1 characters
	 * separator 2 (;)
	 * File version number (5 characters, 1-32767)
	 * 1 <= Sum of File name and File name extension <= 30
	 */
	int namelen = 0;
	int extlen = 0;
	int found_ext = 0;

	while (*oldname != '\0') {
		/* Handle period first, as it is special */
		if (*oldname == '.') {
			if (found_ext) {
				*(newname++) = '_';
				extlen ++;
			}
			else {
				*(newname++) = '.';
				found_ext = 1;
			}
		} else {
			if ((namelen + extlen) == 30)
				break;

			 if (islower((unsigned char)*oldname))
				*(newname++) = toupper((unsigned char)*oldname);
			else if (isupper((unsigned char)*oldname) ||
			    isdigit((unsigned char)*oldname))
				*(newname++) = *oldname;
			else 
				*(newname++) = '_';

			if (found_ext)
				extlen++;
			else
				namelen++;
		}
		oldname ++;
	}

	/* Add version */
	*(newname++) = ';';
	sprintf((newname++), "%i", 1);
	return namelen + extlen + found_ext;
}

#if 0
static int
cd9660_joliet_convert_filename(const char *oldname, char *newname, int is_file)
{
	/* TODO: implement later, move to cd9660_joliet.c ?? */
}
#endif


/*
 * Convert a file name to ISO compliant file name
 * @param char * oldname The original filename
 * @param char ** newname The new file name, in the appropriate character
 *                        set and of appropriate length
 * @param int 1 if file, 0 if directory
 * @returns int The length of the new string
 */
static int
cd9660_convert_filename(const char *oldname, char *newname, int is_file)
{
	/* NEW */
	cd9660_filename_conversion_functor conversion_function = 0;
	if (diskStructure.isoLevel == 1)
		conversion_function = &cd9660_level1_convert_filename;
	else if (diskStructure.isoLevel == 2)
		conversion_function = &cd9660_level2_convert_filename;
	return (*conversion_function)(oldname, newname, is_file);
}

int
cd9660_compute_record_size(cd9660node *node)
{
	int size = node->isoDirRecord->length[0];

	if (diskStructure.rock_ridge_enabled)
		size += node->susp_entry_size;
	return size;
}

static void
cd9660_populate_dot_records(cd9660node *node)
{
	node->dot_record->fileDataSector = node->fileDataSector;
	memcpy(node->dot_record->isoDirRecord,node->isoDirRecord, 34);
	node->dot_record->isoDirRecord->name_len[0] = 1;
	node->dot_record->isoDirRecord->name[0] = 0;
	node->dot_record->isoDirRecord->name[1] = 0;
	node->dot_record->isoDirRecord->length[0] = 34;
	node->dot_record->fileRecordSize =
	    cd9660_compute_record_size(node->dot_record);
	
	if (node == diskStructure.rootNode) {
		node->dot_dot_record->fileDataSector = node->fileDataSector;
		memcpy(node->dot_dot_record->isoDirRecord,node->isoDirRecord,
		    34);
	} else {
		node->dot_dot_record->fileDataSector =
		    node->parent->fileDataSector;
		memcpy(node->dot_dot_record->isoDirRecord,
		    node->parent->isoDirRecord,34);
	}
	node->dot_dot_record->isoDirRecord->name_len[0] = 1;
	node->dot_dot_record->isoDirRecord->name[0] = 1;
	node->dot_dot_record->isoDirRecord->name[1] = 0;
	node->dot_dot_record->isoDirRecord->length[0] = 34;
	node->dot_dot_record->fileRecordSize =
	    cd9660_compute_record_size(node->dot_dot_record);
}

/*
 * @param struct cd9660node *node The node
 * @param int The offset (in bytes) - SHOULD align to the beginning of a sector
 * @returns int The total size of files and directory entries (should be
 *              a multiple of sector size)
*/
static int
cd9660_compute_offsets(cd9660node *node, int startOffset)
{
	/*
	 * This function needs to compute the size of directory records and
	 * runs, file lengths, and set the appropriate variables both in
	 * cd9660node and isoDirEntry
	 */
	int used_bytes = 0;
	int current_sector_usage = 0;
	cd9660node *child;
	int r;

	assert(node != NULL);


	/*
	 * NOTE : There needs to be some special case detection for
	 * the "real root" node, since for it, node->node is undefined
	 */
	
	node->fileDataSector = -1;
	
	if (node->type & CD9660_TYPE_DIR) {
		node->fileRecordSize = cd9660_compute_record_size(node);
		/*Set what sector this directory starts in*/
		node->fileDataSector =
		    CD9660_BLOCKS(diskStructure.sectorSize,startOffset);
		
		cd9660_bothendian_dword(node->fileDataSector,
		    node->isoDirRecord->extent);
		
		/*
		 * First loop over children, need to know the size of
		 * their directory records
		 */
		child = node->child;
		node->fileSectorsUsed = 1;
		for (child = node->child; child != 0; child = child->next) {
			
			node->fileDataLength +=
			    cd9660_compute_record_size(child);
			if ((cd9660_compute_record_size(child) +
			    current_sector_usage) >=
		 	    diskStructure.sectorSize) {
				current_sector_usage = 0;
				node->fileSectorsUsed++;
			}
			
			current_sector_usage +=
			    cd9660_compute_record_size(child);
		}
		
		cd9660_bothendian_dword(node->fileSectorsUsed *
			diskStructure.sectorSize,node->isoDirRecord->size);

		/*
		 * This should point to the sector after the directory
		 * record (or, the first byte in that sector)
		 */
		used_bytes += node->fileSectorsUsed * diskStructure.sectorSize;
		
		
		for (child = node->dot_dot_record->next; child != NULL;
		     child = child->next) {
			/* Directories need recursive call */
			if (S_ISDIR(child->node->type)) {
				r = cd9660_compute_offsets(child,
				    used_bytes + startOffset);
				
				if (r != -1)
					used_bytes += r;
				else
					return -1;
			}
		}
		
		/* Explicitly set the . and .. records */
		cd9660_populate_dot_records(node);
		
		/* Finally, do another iteration to write the file data*/
		child = node->dot_dot_record->next;
		while (child != 0) {
			/* Files need extent set */
			if (!(S_ISDIR(child->node->type))) {
				child->fileRecordSize =
				    cd9660_compute_record_size(child);

				/* For 0 byte files */
				if (child->fileDataLength == 0)
					child->fileSectorsUsed = 0;
				else 
					child->fileSectorsUsed =
					    CD9660_BLOCKS(
						diskStructure.sectorSize,
						child->fileDataLength);
				
				child->fileDataSector =	
					CD9660_BLOCKS(diskStructure.sectorSize,
					    used_bytes + startOffset);
				cd9660_bothendian_dword(child->fileDataSector,
					child->isoDirRecord->extent);
				used_bytes += child->fileSectorsUsed *
				    diskStructure.sectorSize;
			}
			child = child->next;
		}
	}

	return used_bytes;
}

#if 0
/* Might get rid of this func */
static int
cd9660_copy_stat_info(cd9660node *from, cd9660node *to, int file)
{
	to->node->inode->st.st_dev = 0;
	to->node->inode->st.st_ino = 0;
	to->node->inode->st.st_size = 0;
	to->node->inode->st.st_blksize = from->node->inode->st.st_blksize;
	to->node->inode->st.st_atime = from->node->inode->st.st_atime;
	to->node->inode->st.st_mtime = from->node->inode->st.st_mtime;
	to->node->inode->st.st_ctime = from->node->inode->st.st_ctime;
	to->node->inode->st.st_uid = from->node->inode->st.st_uid;
	to->node->inode->st.st_gid = from->node->inode->st.st_gid;
	to->node->inode->st.st_mode = from->node->inode->st.st_mode;
	/* Clear out type */
	to->node->inode->st.st_mode = to->node->inode->st.st_mode & ~(S_IFMT);
	if (file)
		to->node->inode->st.st_mode |= S_IFREG;
	else
		to->node->inode->st.st_mode |= S_IFDIR;
	return 1;
}
#endif

static cd9660node *
cd9660_create_virtual_entry(const char *name, cd9660node *parent, int file,
			    int insert)
{
	cd9660node *temp;
	fsnode * tfsnode;
	
	assert(parent != NULL);
	
	temp = cd9660_allocate_cd9660node();
	if (temp == NULL)
		return NULL;

	if ((tfsnode = malloc(sizeof(fsnode))) == NULL) {
		CD9660_MEM_ALLOC_ERROR("cd9660_create_virtual_entry");
		return NULL;
	}

	/* Assume for now name is a valid length */
	if ((tfsnode->name = malloc(strlen(name) + 1)) == NULL) {
		CD9660_MEM_ALLOC_ERROR("cd9660_create_virtual_entry");
		return NULL;
	}

	if ((temp->isoDirRecord =
	    malloc(sizeof(iso_directory_record_cd9660))) == NULL) {
		CD9660_MEM_ALLOC_ERROR("cd9660_create_virtual_entry");
		return NULL;
	}
	
	strcpy(tfsnode->name, name);
	
	cd9660_convert_filename(tfsnode->name, temp->isoDirRecord->name, file);
	
	temp->node = tfsnode;
	temp->parent = parent;

	if (insert) {
		if (temp->parent != NULL) {
			temp->level = temp->parent->level + 1;
			if (temp->parent->child != NULL)
				cd9660_sorted_node_insert(temp->parent->child,
				    temp);
			else
				temp->parent->child = temp;
		}
	}

	if (parent->node != NULL) {
		tfsnode->type = parent->node->type;
	}

	/* Clear out file type bits */
	tfsnode->type &= ~(S_IFMT);
	if (file)
		tfsnode->type |= S_IFREG;
	else
		tfsnode->type |= S_IFDIR;

	/* Indicate that there is no spec entry (inode) */
	tfsnode->flags &= ~(FSNODE_F_HASSPEC);
#if 0
	cd9660_copy_stat_info(parent, temp, file);
#endif
	return temp;
}

static cd9660node *
cd9660_create_file(const char * name, cd9660node *parent)
{
	cd9660node *temp;

	temp = cd9660_create_virtual_entry(name,parent,1,1);
	if (temp == NULL)
		return NULL;

	temp->fileDataLength = 0;
	
	temp->type = CD9660_TYPE_FILE | CD9660_TYPE_VIRTUAL;
	
	if (cd9960_translate_node_common(temp) == 0)
		return NULL;
	return temp;
}

/*
 * Create a new directory which does not exist on disk
 * @param const char * name The name to assign to the directory
 * @param const char * parent Pointer to the parent directory
 * @returns cd9660node * Pointer to the new directory
 */
static cd9660node *
cd9660_create_directory(const char *name, cd9660node *parent)
{
	cd9660node *temp;

	temp = cd9660_create_virtual_entry(name,parent,0,1);
	if (temp == NULL)
		return NULL;
	temp->node->type |= S_IFDIR;
	
	temp->type = CD9660_TYPE_DIR | CD9660_TYPE_VIRTUAL;
	
	if (cd9960_translate_node_common(temp) == 0)
		return NULL;
	return temp;
}

static cd9660node *
cd9660_create_special_directory(u_char type, cd9660node *parent)
{
	cd9660node *temp;
	char na[2];
	
	assert(parent != NULL);
	
	if (type == CD9660_TYPE_DOT)
		na[0] = 0;
	else if (type == CD9660_TYPE_DOTDOT)
		na[0] = 1;
	else
		return 0;

	na[1] = 0;
	if ((temp = cd9660_create_virtual_entry(na, parent, 0, 0)) == NULL)
		return NULL;

	temp->parent = parent;
	temp->type = type;
	temp->isoDirRecord->length[0] = 34;
	/* Dot record is always first */
	if (type == CD9660_TYPE_DOT) {
		parent->dot_record = temp;
		if (parent->child != NULL) {
			parent->child->prev = temp;
			temp->next = parent->child;
			parent->child = temp;
		} else
			parent->child = temp;
	/* DotDot should be second */
	} else if (type == CD9660_TYPE_DOTDOT) {
		parent->dot_dot_record = temp;
		if (parent->child != NULL) {
			/*
			 * If the first child is the dot record,
			 * insert this second.
			 */
			if (parent->child->type & CD9660_TYPE_DOT) {
				if (parent->child->next != 0)
					parent->child->next->prev = temp;
				temp->next = parent->child->next;
				temp->prev = parent->child;
				parent->child->next = temp;
			} else {
				/*
				 * Else, insert it first. Dot record
				 * will be added later.
				 */
				parent->child->prev = temp;
				temp->next = parent->child;
				parent->child = temp;
			}
		} else
			parent->child = temp;
	}

	return temp;
}

