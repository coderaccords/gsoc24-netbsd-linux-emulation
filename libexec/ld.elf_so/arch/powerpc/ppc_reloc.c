/*	$NetBSD: ppc_reloc.c,v 1.17 2002/09/06 03:05:37 mycroft Exp $	*/

/*-
 * Copyright (C) 1998	Tsubai Masanari
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
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <machine/cpu.h>

#include "debug.h"
#include "rtld.h"

caddr_t _rtld_bind_powerpc __P((Obj_Entry *, Elf_Word));
void _rtld_powerpc_pltcall __P((Elf_Word));
void _rtld_powerpc_pltresolve __P((Elf_Word, Elf_Word));

#define ha(x) ((((u_int32_t)(x) & 0x8000) ? \
			((u_int32_t)(x) + 0x10000) : (u_int32_t)(x)) >> 16)
#define l(x) ((u_int32_t)(x) & 0xffff)

/*
 * Bind a pltgot slot indexed by reloff.
 */
caddr_t
_rtld_bind_powerpc(obj, reloff)
	Obj_Entry *obj;
	Elf_Word reloff;
{
	const Elf_Rela *rela;
	caddr_t		addr;

	if (reloff < 0 || reloff >= 0x8000) {
		dbg(("_rtld_bind_powerpc: broken reloff %x", reloff));
		_rtld_die();
	}

	rela = obj->pltrela + reloff;

	if (_rtld_relocate_plt_object(obj, rela, &addr, true) < 0)
		_rtld_die();

	return addr;
}

int
_rtld_relocate_plt_object(
	Obj_Entry *obj,
	const Elf_Rela *rela,
	caddr_t *addrp,
	bool dodebug)
{
	Elf_Word *where = (Elf_Word *)(obj->relocbase + rela->r_offset);
	int distance;

	const Elf_Sym *def;
	const Obj_Entry *defobj;
	Elf_Addr value;

	assert(ELF_R_TYPE(rela->r_info) == R_TYPE(JMP_SLOT));

	def = _rtld_find_symdef(ELF_R_SYM(rela->r_info), obj, &defobj,
	    true);
	if (def == NULL)
		return (-1);

	value = (Elf_Addr)(defobj->relocbase + def->st_value);
	distance = value - (Elf_Addr)where;

	if (abs(distance) < 32*1024*1024) {	/* inside 32MB? */
		/* b	value	# branch directly */
		*where = 0x48000000 | (distance & 0x03fffffc);
		__syncicache(where, 4);
	} else {
		Elf_Addr *pltcall, *jmptab;
		int N = obj->pltrelalim - obj->pltrela;
		int reloff = rela - obj->pltrela;
	
		if (reloff < 0 || reloff >= 0x8000)
			return (-1);
	
		pltcall = obj->pltgot;
	
		jmptab = pltcall + 18 + N * 2;
		jmptab[reloff] = value;

		distance = (Elf_Addr)pltcall - (Elf_Addr)(where + 1);
	
		/* li	r11,reloff */
		/* b	pltcall		# use pltcall routine */
		where[0] = 0x39600000 | reloff;
		where[1] = 0x48000000 | (distance & 0x03fffffc);
		__syncicache(where, 8);
	}

	*addrp = (caddr_t)value;
	return (0);
}

/*
 * Setup the plt glue routines.
 */
#define PLTCALL_SIZE	20
#define PLTRESOLVE_SIZE	24

void
_rtld_setup_pltgot(obj)
	const Obj_Entry *obj;
{
	Elf_Word *pltcall, *pltresolve;
	Elf_Word *jmptab;
	int N = obj->pltrelalim - obj->pltrela;

	pltcall = obj->pltgot;

	memcpy(pltcall, _rtld_powerpc_pltcall, PLTCALL_SIZE);
	jmptab = pltcall + 18 + N * 2;
	pltcall[1] |= ha(jmptab);
	pltcall[2] |= l(jmptab);

	pltresolve = obj->pltgot + 8;

	memcpy(pltresolve, _rtld_powerpc_pltresolve, PLTRESOLVE_SIZE);
	pltresolve[0] |= ha(_rtld_bind_start);
	pltresolve[1] |= l(_rtld_bind_start);
	pltresolve[3] |= ha(obj);
	pltresolve[4] |= l(obj);

	__syncicache(pltcall, 72 + N * 8);
}

int
_rtld_relocate_nonplt_objects(obj, dodebug)
	Obj_Entry *obj;
	bool dodebug;
{
	const Elf_Rela *rela;

	for (rela = obj->rela; rela < obj->relalim; rela++) {
		Elf_Addr        *where;
		const Elf_Sym   *def;
		const Obj_Entry *defobj;
		Elf_Addr         tmp;
		unsigned long	 symnum;

		where = (Elf_Addr *)(obj->relocbase + rela->r_offset);
		symnum = ELF_R_SYM(rela->r_info);

		switch (ELF_R_TYPE(rela->r_info)) {
		case R_TYPE(NONE):
			break;

		case R_TYPE(32):	/* word32 S + A */
		case R_TYPE(GLOB_DAT):	/* word32 S + A */
			def = _rtld_find_symdef(symnum, obj, &defobj, false);
			if (def == NULL)
				return -1;

			tmp = (Elf_Addr)(defobj->relocbase + def->st_value +
			    rela->r_addend);
			if (*where != tmp)
				*where = tmp;
			rdbg(dodebug, ("32/GLOB_DAT %s in %s --> %p in %s",
			    obj->strtab + obj->symtab[symnum].st_name,
			    obj->path, (void *)*where, defobj->path));
			break;

		case R_TYPE(RELATIVE):	/* word32 B + A */
			tmp = (Elf_Addr)(obj->relocbase + rela->r_addend);
			if (*where != tmp)
				*where = tmp;
			rdbg(dodebug, ("RELATIVE in %s --> %p", obj->path,
			    (void *)*where));
			break;

		case R_TYPE(COPY):
			/*
			 * These are deferred until all other relocations have
			 * been done.  All we do here is make sure that the
			 * COPY relocation is not in a shared library.  They
			 * are allowed only in executable files.
			 */
			if (!obj->mainprog) {
				_rtld_error(
			"%s: Unexpected R_COPY relocation in shared library",
				    obj->path);
				return -1;
			}
			rdbg(dodebug, ("COPY (avoid in main)"));
			break;

		default:
			rdbg(dodebug, ("sym = %lu, type = %lu, offset = %p, "
			    "addend = %p, contents = %p, symbol = %s",
			    symnum, (u_long)ELF_R_TYPE(rela->r_info),
			    (void *)rela->r_offset, (void *)rela->r_addend,
			    (void *)*where,
			    obj->strtab + obj->symtab[symnum].st_name));
			_rtld_error("%s: Unsupported relocation type %ld "
			    "in non-PLT relocations\n",
			    obj->path, (u_long) ELF_R_TYPE(rela->r_info));
			return -1;
		}
	}
	return 0;
}

int
_rtld_relocate_plt_lazy(obj, dodebug)
	Obj_Entry *obj;
	bool dodebug;
{
	const Elf_Rela *rela;

	for (rela = obj->pltrela; rela < obj->pltrelalim; rela++) {
		Elf_Word *where = (Elf_Word *)(obj->relocbase + rela->r_offset);
		int distance;
		Elf_Addr *pltresolve;
		int reloff = rela - obj->pltrela;

		assert(ELF_R_TYPE(rela->r_info) == R_TYPE(JMP_SLOT));

		if (reloff < 0 || reloff >= 0x8000)
			return (-1);

		pltresolve = obj->pltgot + 8;

		distance = (Elf_Addr)pltresolve - (Elf_Addr)(where + 1);

       		/* li	r11,reloff */
		/* b	pltresolve   */
		where[0] = 0x39600000 | reloff;
		where[1] = 0x48000000 | (distance & 0x03fffffc);
		/* __syncicache(where, 8); */
	}

	return 0;
}
