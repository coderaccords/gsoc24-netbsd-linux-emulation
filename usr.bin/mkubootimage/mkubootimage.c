/* $NetBSD: mkubootimage.c,v 1.5 2011/01/31 03:37:28 matt Exp $ */

/*-
 * Copyright (c) 2010 Jared D. McNeill <jmcneill@invisible.ca>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#if HAVE_NBTOOL_CONFIG_H
#include "nbtool_config.h"
#endif

#include <sys/cdefs.h>
__RCSID("$NetBSD: mkubootimage.c,v 1.5 2011/01/31 03:37:28 matt Exp $");

#include <sys/mman.h>
#include <sys/stat.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "uboot.h"

#ifndef __arraycount
#define __arraycount(__x)	(sizeof(__x) / sizeof(__x[0]))
#endif

extern uint32_t crc32(const void *, size_t);

static enum uboot_image_arch image_arch = IH_ARCH_UNKNOWN;
static enum uboot_image_type image_type = IH_TYPE_UNKNOWN;
static enum uboot_image_comp image_comp = IH_COMP_NONE;
static uint32_t image_loadaddr = 0;
static uint32_t image_entrypoint = 0;
static char *image_name;

struct uboot_arch {
	enum uboot_image_arch arch;
	const char *name;
} uboot_arch[] = {
	{ IH_ARCH_ARM,		"arm" },
	{ IH_ARCH_MIPS,		"mips" },
	{ IH_ARCH_MIPS64,	"mips64" },
	{ IH_ARCH_PPC,		"powerpc" },
};

static enum uboot_image_arch
get_arch(const char *name)
{
	unsigned int i;

	for (i = 0; i < __arraycount(uboot_arch); i++) {
		if (strcmp(uboot_arch[i].name, name) == 0)
			return uboot_arch[i].arch;
	}

	return IH_ARCH_UNKNOWN;
}

struct uboot_type {
	enum uboot_image_type type;
	const char *name;
} uboot_type[] = {
	{ IH_TYPE_KERNEL,	"kernel" },
	{ IH_TYPE_RAMDISK,	"ramdisk" },
	{ IH_TYPE_FILESYSTEM,	"fs" },
};

static enum uboot_image_type
get_type(const char *name)
{
	unsigned int i;

	for (i = 0; i < __arraycount(uboot_type); i++) {
		if (strcmp(uboot_type[i].name, name) == 0)
			return uboot_type[i].type;
	}

	return IH_TYPE_UNKNOWN;
}

struct uboot_comp {
	enum uboot_image_comp comp;
	const char *name;
} uboot_comp[] = {
	{ IH_COMP_NONE,		"none" },
	{ IH_COMP_GZIP,		"gz" },
	{ IH_COMP_BZIP2,	"bz2" },
};

static enum uboot_image_comp
get_comp(const char *name)
{
	unsigned int i;

	for (i = 0; i < __arraycount(uboot_comp); i++) {
		if (strcmp(uboot_comp[i].name, name) == 0)
			return uboot_comp[i].comp;
	}

	return IH_TYPE_UNKNOWN;
}

static void
usage(void)
{
	fprintf(stderr, "usage: mkubootimage -A <arm|mips|mips64|powerpc>");
	fprintf(stderr, " -T <kernel|ramdisk|fs>");
	fprintf(stderr, " -C <none|gz|bz2>");
	fprintf(stderr, " -a <addr> [-e <ep>] -n <name>");
	fprintf(stderr, " <srcfile> <dstfile>\n");

	exit(EXIT_FAILURE);
}

static void
dump_header(struct uboot_image_header *hdr)
{
	time_t tm = ntohl(hdr->ih_time);

	printf(" magic:       0x%08x\n", ntohl(hdr->ih_magic));
	printf(" time:        %s", ctime(&tm));
	printf(" size:        %u\n", ntohl(hdr->ih_size));
	printf(" load addr:   0x%08x\n", ntohl(hdr->ih_load));
	printf(" entry point: 0x%08x\n", ntohl(hdr->ih_ep));
	printf(" data crc:    0x%08x\n", ntohl(hdr->ih_dcrc));
	printf(" os:          %d\n", hdr->ih_os);
	printf(" arch:        %d\n", hdr->ih_arch);
	printf(" type:        %d\n", hdr->ih_type);
	printf(" comp:        %d\n", hdr->ih_comp);
	printf(" name:        %s\n", hdr->ih_name);
	printf(" header crc:  0x%08x\n", hdr->ih_hcrc);
}

static int
generate_header(struct uboot_image_header *hdr, int kernel_fd)
{
	uint8_t *p;
	struct stat st;
	uint32_t crc;
	int error;

	error = fstat(kernel_fd, &st);
	if (error == -1) {
		perror("stat");
		return errno;
	}

	if (st.st_size + sizeof(*hdr) > UINT32_MAX) {
		fprintf(stderr, "fatal: kernel too big\n");
		return EINVAL;
	}

	p = mmap(0, st.st_size, PROT_READ, MAP_FILE|MAP_SHARED, kernel_fd, 0);
	if (p == MAP_FAILED) {
		perror("mmap kernel");
		return EINVAL;
	}
	crc = crc32(p, st.st_size);
	munmap(p, st.st_size);

	memset(hdr, 0, sizeof(*hdr));
	hdr->ih_magic = htonl(IH_MAGIC);
	hdr->ih_time = htonl(st.st_mtime);
	hdr->ih_size = htonl(st.st_size);
	hdr->ih_load = htonl(image_loadaddr);
	hdr->ih_ep = htonl(image_entrypoint);
	hdr->ih_dcrc = htonl(crc);
	hdr->ih_os = IH_OS_NETBSD;
	hdr->ih_arch = image_arch;
	hdr->ih_type = image_type;
	hdr->ih_comp = image_comp;
	strncpy((char *)hdr->ih_name, image_name, sizeof(hdr->ih_name));
	crc = crc32((void *)hdr, sizeof(*hdr));
	hdr->ih_hcrc = htonl(crc);

	dump_header(hdr);

	return 0;
}

static int
write_image(struct uboot_image_header *hdr, int kernel_fd, int image_fd)
{
	uint8_t buf[4096];
	ssize_t rlen, wlen;

	wlen = write(image_fd, hdr, sizeof(*hdr));
	if (wlen != sizeof(*hdr)) {
		perror("short write");
		return errno;
	}

	while ((rlen = read(kernel_fd, buf, sizeof(buf))) > 0) {
		wlen = write(image_fd, buf, rlen);
		if (wlen != rlen) {
			perror("short write");
			return errno;
		}
	}

	return 0;
}

int
main(int argc, char *argv[])
{
	struct uboot_image_header hdr;
	const char *src, *dest;
	char *ep;
	int kernel_fd, image_fd;
	int ch;
	unsigned long num;

	while ((ch = getopt(argc, argv, "A:C:T:a:e:hn:")) != -1) {
		switch (ch) {
		case 'A':	/* arch */
			image_arch = get_arch(optarg);
			break;
		case 'C':	/* comp */
			image_comp = get_comp(optarg);
			break;
		case 'T':	/* type */
			image_type = get_type(optarg);
			break;
		case 'a':	/* addr */
			errno = 0;
			num = strtoul(optarg, &ep, 0);
			if (*ep != '\0' || (errno == ERANGE &&
			    (num == ULONG_MAX || num == 0)))
				errx(1, "illegal number -- %s", optarg);
			image_loadaddr = (uint32_t)num;
			break;
		case 'e':	/* ep */
			errno = 0;
			num = strtoul(optarg, &ep, 0);
			if (*ep != '\0' || (errno == ERANGE &&
			    (num == ULONG_MAX || num == 0)))
				errx(1, "illegal number -- %s", optarg);
			image_entrypoint = (uint32_t)num;
			break;
		case 'n':	/* name */
			image_name = strdup(optarg);
			break;
		case 'h':
		default:
			usage();
			/* NOTREACHED */
		}
	}
	argc -= optind;
	argv += optind;

	if (argc != 2)
		usage();

	if (image_entrypoint == 0)
		image_entrypoint = image_loadaddr;

	if (image_arch == IH_ARCH_UNKNOWN ||
	    image_type == IH_TYPE_UNKNOWN ||
	    image_loadaddr == 0 ||
	    image_name == NULL)
		usage();

	src = argv[0];
	dest = argv[1];

	kernel_fd = open(src, O_RDONLY);
	if (kernel_fd == -1) {
		perror("open kernel");
		return EXIT_FAILURE;
	}
	image_fd = open(dest, O_WRONLY|O_CREAT, 0666);
	if (image_fd == -1) {
		perror("open image");
		return EXIT_FAILURE;
	}

	if (generate_header(&hdr, kernel_fd) != 0)
		return EXIT_FAILURE;

	if (write_image(&hdr, kernel_fd, image_fd) != 0)
		return EXIT_FAILURE;

	close(image_fd);
	close(kernel_fd);

	return EXIT_SUCCESS;
}
