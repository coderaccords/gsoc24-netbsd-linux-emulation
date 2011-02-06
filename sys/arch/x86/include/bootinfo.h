/*	$NetBSD: bootinfo.h,v 1.17 2011/02/06 23:14:04 jmcneill Exp $	*/

/*
 * Copyright (c) 1997
 *	Matthias Drochner.  All rights reserved.
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

#define BTINFO_BOOTPATH		0
#define BTINFO_ROOTDEVICE	1
#define BTINFO_BOOTDISK		3
#define BTINFO_NETIF		4
#define BTINFO_CONSOLE		6
#define BTINFO_BIOSGEOM		7
#define BTINFO_SYMTAB		8
#define BTINFO_MEMMAP		9
#define	BTINFO_BOOTWEDGE	10
#define BTINFO_MODULELIST	11
#define BTINFO_FRAMEBUFFER	12

#ifndef _LOCORE

struct btinfo_common {
	int len;
	int type;
};

struct btinfo_bootpath {
	struct btinfo_common common;
	char bootpath[80];
};

struct btinfo_rootdevice {
	struct btinfo_common common;
	char devname[16];
};

struct btinfo_bootdisk {
	struct btinfo_common common;
	int labelsector; /* label valid if != -1 */
	struct {
		uint16_t type, checksum;
		char packname[16];
	} label;
	int biosdev;
	int partition;
};

struct btinfo_bootwedge {
	struct btinfo_common common;
	int biosdev;
	daddr_t startblk;
	uint64_t nblks;
	daddr_t matchblk;
	uint64_t matchnblks;
	uint8_t matchhash[16];	/* MD5 hash */
} __packed;

struct btinfo_netif {
	struct btinfo_common common;
	char ifname[16];
	int bus;
#define BI_BUS_ISA 0
#define BI_BUS_PCI 1
	union {
		unsigned int iobase; /* ISA */
		unsigned int tag; /* PCI, BIOS format */
	} addr;
};

struct btinfo_console {
	struct btinfo_common common;
	char devname[16];
	int addr;
	int speed;
};

struct btinfo_symtab {
	struct btinfo_common common;
	int nsym;
	int ssym;
	int esym;
};

struct bi_memmap_entry {
	uint64_t addr;		/* beginning of block */	/* 8 */
	uint64_t size;		/* size of block */		/* 8 */
	uint32_t type;		/* type of block */		/* 4 */
} __packed;				/*	== 20 */

#define	BIM_Memory	1	/* available RAM usable by OS */
#define	BIM_Reserved	2	/* in use or reserved by the system */
#define	BIM_ACPI	3	/* ACPI Reclaim memory */
#define	BIM_NVS		4	/* ACPI NVS memory */

struct btinfo_memmap {
	struct btinfo_common common;
	int num;
	struct bi_memmap_entry entry[1]; /* var len */
};

#if HAVE_NBTOOL_CONFIG_H
#include <nbinclude/sys/bootblock.h>
#else
#include <sys/bootblock.h>
#endif /* HAVE_NBTOOL_CONFIG_H */

/*
 * Structure describing disk info as seen by the BIOS.
 */
struct bi_biosgeom_entry {
	int		sec, head, cyl;		/* geometry */
	uint64_t	totsec;			/* LBA sectors from ext int13 */
	int		flags, dev;		/* flags, BIOS device # */
#define BI_GEOM_INVALID		0x000001
#define BI_GEOM_EXTINT13	0x000002
#ifdef BIOSDISK_EXTINFO_V3
#define BI_GEOM_BADCKSUM	0x000004	/* v3.x checksum invalid */
#define BI_GEOM_BUS_MASK	0x00ff00	/* connecting bus type */
#define BI_GEOM_BUS_ISA		0x000100
#define BI_GEOM_BUS_PCI		0x000200
#define BI_GEOM_BUS_OTHER	0x00ff00
#define BI_GEOM_IFACE_MASK	0xff0000	/* interface type */
#define BI_GEOM_IFACE_ATA	0x010000
#define BI_GEOM_IFACE_ATAPI	0x020000
#define BI_GEOM_IFACE_SCSI	0x030000
#define BI_GEOM_IFACE_USB	0x040000
#define BI_GEOM_IFACE_1394	0x050000	/* Firewire */
#define BI_GEOM_IFACE_FIBRE	0x060000	/* Fibre channel */
#define BI_GEOM_IFACE_OTHER	0xff0000
	unsigned int	cksum;			/* MBR checksum */
	unsigned int	interface_path;		/* ISA iobase PCI bus/dev/fun */
	uint64_t	device_path;
	int		res0;			/* future expansion; 0 now */
#else
	unsigned int	cksum;			/* MBR checksum */
	int		res0, res1, res2, res3;	/* future expansion; 0 now */
#endif
	struct mbr_partition dosparts[MBR_PART_COUNT]; /* MBR itself */
} __packed;

struct btinfo_biosgeom {
	struct btinfo_common common;
	int num;
	struct bi_biosgeom_entry disk[1]; /* var len */
};

struct bi_modulelist_entry {
	char path[80];
	int type;
	int len;
	uint32_t base;
};
#define	BI_MODULE_NONE		0x00
#define	BI_MODULE_ELF		0x01
#define	BI_MODULE_IMAGE		0x02

struct btinfo_modulelist {
	struct btinfo_common common;
	int num;
	uint32_t endpa;
	/* bi_modulelist_entry list follows */
};

struct btinfo_framebuffer {
	struct btinfo_common common;
	uint64_t physaddr;
	uint32_t flags;
	uint32_t width;
	uint32_t height;
	uint16_t stride;
	uint8_t depth;
	uint8_t rnum;
	uint8_t gnum;
	uint8_t bnum;
	uint8_t rpos;
	uint8_t gpos;
	uint8_t bpos;
	uint16_t vbemode;
	uint8_t reserved[14];
};

#endif /* _LOCORE */

#ifdef _KERNEL

#define BOOTINFO_MAXSIZE 4096

#ifndef _LOCORE
/*
 * Structure that holds the information passed by the boot loader.
 */
struct bootinfo {
	/* Number of bootinfo_* entries in bi_data. */
	uint32_t	bi_nentries;

	/* Raw data of bootinfo entries.  The first one (if any) is
	 * found at bi_data[0] and can be casted to (bootinfo_common *).
	 * Once this is done, the following entry is found at 'len'
	 * offset as specified by the previous entry. */
	uint8_t		bi_data[BOOTINFO_MAXSIZE - sizeof(uint32_t)];
};

extern struct bootinfo bootinfo;

void *lookup_bootinfo(int);
#endif /* _LOCORE */

#endif /* _KERNEL */
