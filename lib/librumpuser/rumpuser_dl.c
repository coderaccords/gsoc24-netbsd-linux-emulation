/*      $NetBSD: rumpuser_dl.c,v 1.3 2010/03/05 18:47:50 pooka Exp $	*/

/*
 * Copyright (c) 2009 Antti Kantee.  All Rights Reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Load all module link sets and feed symbol table to the kernel.
 * Called during rump bootstrap.
 */

#include <sys/cdefs.h>
__RCSID("$NetBSD: rumpuser_dl.c,v 1.3 2010/03/05 18:47:50 pooka Exp $");

#include <sys/types.h>
#include <sys/time.h>

#include <assert.h>
#include <dlfcn.h>
#include <elf.h>
#include <errno.h>
#include <fcntl.h>
#include <link.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <rump/rumpuser.h>

#if defined(__ELF__) && (defined(__NetBSD__) || defined(__FreeBSD__)	\
    || (defined(__sun__) && defined(__svr4__)))
static size_t symtabsize = 0, strtabsize = 0;
static size_t symtaboff = 0, strtaboff = 0;
static uint8_t *symtab = NULL;
static char *strtab = NULL;
static unsigned char eident;

static void *
reservespace(void *store, size_t *storesize,
	size_t storeoff, size_t required)
{
	size_t chunk, newsize;

	assert(storeoff <= *storesize);
	chunk = *storesize - storeoff;

	if (chunk >= required)
		return store;

	newsize = *storesize + ((size_t)required - chunk);
	store = realloc(store, newsize);
	if (store == NULL) {
		return NULL;
	}
	*((uint8_t *)store + storeoff) = '\0';
	*storesize = newsize;

	return store;
}

/*
 * Macros to make handling elf32/64 in the code a little saner.
 */

#define EHDR_GETMEMBER(base, thevar, result)				\
do {									\
	if (eident == ELFCLASS32) {					\
		Elf32_Ehdr *ehdr = base;				\
		/*LINTED*/						\
		result = ehdr->thevar;					\
	} else {							\
		Elf64_Ehdr *ehdr = base;				\
		/*LINTED*/						\
		result = ehdr->thevar;					\
	}								\
} while (/*CONSTCOND*/0)

#define SHDRn_GETMEMBER(base, n, thevar, result)			\
do {									\
	if (eident == ELFCLASS32) {					\
		Elf32_Shdr *shdr = base;				\
		/*LINTED*/						\
		result = shdr[n].thevar;				\
	} else {							\
		Elf64_Shdr *shdr = base;				\
		/*LINTED*/						\
		result = shdr[n].thevar;				\
	}								\
} while (/*CONSTCOND*/0)

#define DYNn_GETMEMBER(base, n, thevar, result)				\
do {									\
	if (eident == ELFCLASS32) {					\
		Elf32_Dyn *dyn = base;					\
		/*LINTED*/						\
		result = dyn[n].thevar;					\
	} else {							\
		Elf64_Dyn *dyn = base;					\
		/*LINTED*/						\
		result = dyn[n].thevar;					\
	}								\
} while (/*CONSTCOND*/0)

#define SYMn_GETMEMBER(base, n, thevar, result)				\
do {									\
	if (eident == ELFCLASS32) {					\
		Elf32_Sym *sym = base;					\
		/*LINTED*/						\
		result = sym[n].thevar;					\
	} else {							\
		Elf64_Sym *sym = base;					\
		/*LINTED*/						\
		result = sym[n].thevar;					\
	}								\
} while (/*CONSTCOND*/0)

#define SYMn_SETMEMBER(base, n, thevar, value)				\
do {									\
	if (eident == ELFCLASS32) {					\
		Elf32_Sym *sym = base;					\
		/*LINTED*/						\
		sym[n].thevar = value;					\
	} else {							\
		Elf64_Sym *sym = base;					\
		/*LINTED*/						\
		sym[n].thevar = value;					\
	}								\
} while (/*CONSTCOND*/0)

#define SYM_GETSIZE() ((eident==ELFCLASS32)?sizeof(Elf32_Sym):sizeof(Elf64_Sym))

static int
getsymbols(struct link_map *map)
{
	void *libbase = map->l_addr;
	int i = 0, fd;
	char *str_base;
	void *syms_base = NULL; /* XXXgcc */
	size_t cursymsize, curstrsize;
	void *shdr_base;
	size_t shsize;
	int shnum;
	uint64_t shoff;
	void *ed_base;
	uint64_t ed_tag;
	int sverrno;

	if (memcmp(libbase, ELFMAG, SELFMAG) != 0)
		return ENOEXEC;
	eident = *(unsigned char *)(map->l_addr + EI_CLASS);
	if (eident != ELFCLASS32 && eident != ELFCLASS64)
		return ENOEXEC;

	/* read the section headers from disk to determine size of dynsym */
	fd = open(map->l_name, O_RDONLY);
	if (fd == -1) {
		sverrno = errno;
		fprintf(stderr, "open %s failed\n", map->l_name);
		return sverrno;
	}

	EHDR_GETMEMBER(libbase, e_shnum, shnum);
	EHDR_GETMEMBER(libbase, e_shentsize, shsize);
	EHDR_GETMEMBER(libbase, e_shoff, shoff);
	shdr_base = malloc(shnum * shsize);
	if (pread(fd, shdr_base, shnum * shsize, (off_t)shoff)
	    != (ssize_t)(shnum*shsize)){
		sverrno = errno;
		fprintf(stderr, "read section headers for %s failed\n",
		    map->l_name);
		free(shdr_base);
		close(fd);
		return sverrno;
	}
	cursymsize = (size_t)-1;
	for (i = 1; i <= shnum; i++) {
		int shtype;

		SHDRn_GETMEMBER(shdr_base, i, sh_type, shtype);
		if (shtype != SHT_DYNSYM)
			continue;
		SHDRn_GETMEMBER(shdr_base, i, sh_size, cursymsize);
		break;
	}
	free(shdr_base);
	close(fd);
	if (cursymsize == (size_t)-1) {
		fprintf(stderr, "could not find dynsym size from %s\n",
		    map->l_name);
		return ENOEXEC;
	}

	/* find symtab, strtab and strtab size */
	str_base = NULL;
	curstrsize = (size_t)-1;
	ed_base = map->l_ld;
	i = 0;
	DYNn_GETMEMBER(ed_base, i, d_tag, ed_tag);
	while (ed_tag != DT_NULL) {
		uintptr_t edptr;
		size_t edval;

		switch (ed_tag) {
		case DT_SYMTAB:
			DYNn_GETMEMBER(ed_base, i, d_un.d_ptr, edptr);
			syms_base = map->l_addr + edptr;
			break;
		case DT_STRTAB:
			DYNn_GETMEMBER(ed_base, i, d_un.d_ptr, edptr);
			str_base = map->l_addr + edptr;
			break;
		case DT_STRSZ:
			DYNn_GETMEMBER(ed_base, i, d_un.d_val, edval);
			curstrsize = edval;
			break;
		default:
			break;
		}
		i++;
		DYNn_GETMEMBER(ed_base, i, d_tag, ed_tag);
	} while (ed_tag != DT_NULL);

	if (str_base == NULL || syms_base == NULL || curstrsize == (size_t)-1) {
		fprintf(stderr, "could not find strtab, symtab or strtab size "
		    "in %s\n", map->l_name);
		return ENOEXEC;
	}

	/*
	 * Make sure we have enough space for the contents of the symbol
	 * and string tables we are currently processing.  The total used
	 * space will be smaller due to undefined symbols we are not
	 * interested in.
	 */
	symtab = reservespace(symtab, &symtabsize, symtaboff, cursymsize);
	strtab = reservespace(strtab, &strtabsize, strtaboff, curstrsize);
	if (symtab == NULL || strtab == NULL) {
		fprintf(stderr, "failed to reserve memory");
		return ENOMEM;
	}

	/* iterate over all symbols in current symtab */
	for (i = 0; i * SYM_GETSIZE() < cursymsize; i++) {
		char *cursymname;
		int shndx, name;
		uintptr_t value;
		void *csym;

		SYMn_GETMEMBER(syms_base, i, st_shndx, shndx);
		SYMn_GETMEMBER(syms_base, i, st_value, value);
		if (shndx == SHN_UNDEF || value == 0)
			continue;

		/* get symbol name */
		SYMn_GETMEMBER(syms_base, i, st_name, name);
		cursymname = name + str_base;
		memcpy(symtab + symtaboff,
		    (uint8_t *)syms_base + i*SYM_GETSIZE(), SYM_GETSIZE());

		/*
		 * set name to point at new strtab, offset symbol value
		 * with lib base address.
		 */
		csym = symtab + symtaboff;
		SYMn_SETMEMBER(csym, 0, st_name, strtaboff);
		SYMn_GETMEMBER(csym, 0, st_value, value);
		SYMn_SETMEMBER(csym, 0, st_value,(intptr_t)(value+map->l_addr));
		symtaboff += SYM_GETSIZE();

		strcpy(strtab + strtaboff, cursymname);
		strtaboff += strlen(cursymname)+1;
	}

	return 0;
}

static void
process(const char *soname, rump_modinit_fn domodinit)
{
	void *handle;
	const struct modinfo *const *mi_start, *const *mi_end;

	if (strstr(soname, "librump") == NULL)
		return;

	handle = dlopen(soname, RTLD_LAZY);
	if (handle == NULL)
		return;

	mi_start = dlsym(handle, "__start_link_set_modules");
	if (!mi_start)
		goto out;
	mi_end = dlsym(handle, "__stop_link_set_modules");
	if (!mi_end)
		goto out;

	domodinit(mi_start, (size_t)(mi_end-mi_start));

 out:
	dlclose(handle);
}

/*
 * Get the linkmap from the dynlinker.  Try to load kernel modules
 * from all objects in the linkmap.
 */
void
rumpuser_dl_bootstrap(rump_modinit_fn domodinit,
	rump_symload_fn symload)
{
	struct link_map *map, *origmap;
	int error;

	if (dlinfo(RTLD_SELF, RTLD_DI_LINKMAP, &origmap) == -1) {
		fprintf(stderr, "warning: rumpuser module bootstrap "
		    "failed: %s\n", dlerror());
		return;
	}
	/*
	 * Process last->first because that's the most probable
	 * order for dependencies
	 */
	for (; origmap->l_next; origmap = origmap->l_next)
		continue;

	/*
	 * Build symbol table to hand to the rump kernel.  Do this by
	 * iterating over all rump libraries and collecting symbol
	 * addresses and relocation info.
	 */
	error = 0;
	for (map = origmap; map && !error; map = map->l_prev) {
		if (map->l_addr == NULL)
			continue;
		if (strstr(map->l_name, "librump") != NULL)
			error = getsymbols(map);
	}

	if (error == 0) {
		void *trimmedsym, *trimmedstr;

		/*
		 * Allocate optimum-sized memory for storing tables
		 * and feed to kernel.  If memory allocation fails,
		 * just give the ones with extra context (although
		 * I'm pretty sure we'll die moments later due to
		 * memory running out).
		 */
		if ((trimmedsym = malloc(symtaboff)) != NULL) {
			memcpy(trimmedsym, symtab, symtaboff);
		} else {
			trimmedsym = symtab;
			symtab = NULL;
		}
		if ((trimmedstr = malloc(strtaboff)) != NULL) {
			memcpy(trimmedstr, strtab, strtaboff);
		} else {
			trimmedstr = strtab;
			strtab = NULL;
		}
		symload(trimmedsym, symtaboff, trimmedstr, strtaboff);
	}
	free(symtab);
	free(strtab);

	/*
	 * Next, load modules from dynlibs.
	 */
	for (map = origmap; map; map = map->l_prev)
		process(map->l_name, domodinit);
}

void
rumpuser_dl_component_init(int type, rump_component_init_fn compinit)
{
	struct link_map *map;

	if (dlinfo(RTLD_SELF, RTLD_DI_LINKMAP, &map) == -1) {
		fprintf(stderr, "warning: rumpuser module bootstrap "
		    "failed: %s\n", dlerror());
		return;
	}

	for (; map->l_next; map = map->l_next)
		continue;
	for (; map; map = map->l_prev) {
		if (strstr(map->l_name, "librump") != NULL) {
			void *handle;
			struct rump_component **rc, **rc_end;

			handle = dlopen(map->l_name, RTLD_LAZY);
			if (handle == NULL)
				continue;

			rc = dlsym(handle,
			    "__start_link_set_rump_components");
			if (!rc)
				goto loop;
			rc_end = dlsym(handle,
			    "__stop_link_set_rump_components");
			if (!rc_end)
				goto loop;

			for (; rc < rc_end; rc++)
				compinit(*rc, type);
			assert(rc == rc_end);
 loop:
			dlclose(handle);
		}
	}
}
#else
void
rumpuser_dl_bootstrap(rump_modinit_fn domodinit,
	rump_symload_fn symload)
{

	fprintf(stderr, "Warning, dlinfo() unsupported on host?\n");
}

void
rumpuser_dl_component_init(int type, rump_component_init_fn compinit)
{

	fprintf(stderr, "Warning, dlinfo() unsupported on host?\n");
}
#endif
