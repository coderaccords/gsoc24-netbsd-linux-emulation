/*	$NetBSD: mdreloc.c,v 1.12 2002/09/11 17:23:25 mycroft Exp $	*/

#include <sys/types.h>
#include <sys/stat.h>

#include "debug.h"
#include "rtld.h"

void _rtld_relocate_nonplt_self(Elf_Dyn *, Elf_Addr);

void
_rtld_setup_pltgot(const Obj_Entry *obj)
{
	obj->pltgot[1] = (Elf_Addr) obj;
	obj->pltgot[2] = (Elf_Addr) &_rtld_bind_start;
}

void
_rtld_relocate_nonplt_self(dynp, relocbase)
	Elf_Dyn *dynp;
	Elf_Addr relocbase;
{
	const Elf_Rel *rel = 0, *rellim;
	Elf_Addr relsz = 0;
	Elf_Addr *where;

	for (; dynp->d_tag != DT_NULL; dynp++) {
		switch (dynp->d_tag) {
		case DT_REL:
			rel = (const Elf_Rel *)(relocbase + dynp->d_un.d_ptr);
			break;
		case DT_RELSZ:
			relsz = dynp->d_un.d_val;
			break;
		}
	}
	rellim = (const Elf_Rel *)((caddr_t)rel + relsz);
	for (; rel < rellim; rel++) {
		where = (Elf_Addr *)(relocbase + rel->r_offset);
		*where += (Elf_Addr)relocbase;
	}
}

int
_rtld_relocate_nonplt_objects(obj, self, dodebug)
	const Obj_Entry *obj;
	bool self;
	bool dodebug;
{
	const Elf_Rel *rel;
#define COMBRELOC
#ifdef COMBRELOC
	unsigned long lastsym = -1;
#endif
	Elf_Addr target;

	if (self)
		return 0;

	for (rel = obj->rel; rel < obj->rellim; rel++) {
		Elf_Addr        *where;
		const Elf_Sym   *def;
		const Obj_Entry *defobj;
		Elf_Addr         tmp;
		unsigned long	 symnum;

		where = (Elf_Addr *)(obj->relocbase + rel->r_offset);
		symnum = ELF_R_SYM(rel->r_info);

		switch (ELF_R_TYPE(rel->r_info)) {
		case R_TYPE(NONE):
			break;

#if 1 /* XXX should not occur */
		case R_TYPE(PC32):
#ifdef COMBRELOC
			if (symnum != lastsym) {
#endif
				def = _rtld_find_symdef(symnum, obj, &defobj,
				    false);
				if (def == NULL)
					return -1;
				target = (Elf_Addr)(defobj->relocbase +
				    def->st_value);
#ifdef COMBRELOC
				lastsym = symnum;
			}
#endif

			*where += target - (Elf_Addr)where;
			rdbg(dodebug, ("PC32 %s in %s --> %p in %s",
			    obj->strtab + obj->symtab[symnum].st_name,
			    obj->path, (void *)*where, defobj->path));
			break;

		case R_TYPE(GOT32):
#endif
		case R_TYPE(32):
		case R_TYPE(GLOB_DAT):
#ifdef COMBRELOC
			if (symnum != lastsym) {
#endif
				def = _rtld_find_symdef(symnum, obj, &defobj,
				    false);
				if (def == NULL)
					return -1;
				target = (Elf_Addr)(defobj->relocbase +
				    def->st_value);
#ifdef COMBRELOC
				lastsym = symnum;
			}
#endif

			tmp = target + *where;
			if (*where != tmp)
				*where = tmp;
			rdbg(dodebug, ("32/GLOB_DAT %s in %s --> %p in %s",
			    obj->strtab + obj->symtab[symnum].st_name,
			    obj->path, (void *)*where, defobj->path));
			break;

		case R_TYPE(RELATIVE):
			*where += (Elf_Addr)obj->relocbase;
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
			if (obj->isdynamic) {
				_rtld_error(
			"%s: Unexpected R_COPY relocation in shared library",
				    obj->path);
				return -1;
			}
			rdbg(dodebug, ("COPY (avoid in main)"));
			break;

		default:
			rdbg(dodebug, ("sym = %lu, type = %lu, offset = %p, "
			    "contents = %p, symbol = %s",
			    symnum, (u_long)ELF_R_TYPE(rel->r_info),
			    (void *)rel->r_offset, (void *)*where,
			    obj->strtab + obj->symtab[symnum].st_name));
			_rtld_error("%s: Unsupported relocation type %ld "
			    "in non-PLT relocations\n",
			    obj->path, (u_long) ELF_R_TYPE(rel->r_info));
			return -1;
		}
	}
	return 0;
}

int
_rtld_relocate_plt_lazy(obj, dodebug)
	const Obj_Entry *obj;
	bool dodebug;
{
	const Elf_Rel *rel;

	if (!obj->isdynamic)
		return 0;

	for (rel = obj->pltrel; rel < obj->pltrellim; rel++) {
		Elf_Addr *where = (Elf_Addr *)(obj->relocbase + rel->r_offset);

		assert(ELF_R_TYPE(rel->r_info) == R_TYPE(JMP_SLOT));

		/* Just relocate the GOT slots pointing into the PLT */
		*where += (Elf_Addr)obj->relocbase;
		rdbg(dodebug, ("fixup !main in %s --> %p", obj->path,
		    (void *)*where));
	}

	return 0;
}

int
_rtld_relocate_plt_object(obj, rela, addrp, dodebug)
	const Obj_Entry *obj;
	const Elf_Rela *rela;
	caddr_t *addrp;
	bool dodebug;
{
	Elf_Addr *where = (Elf_Addr *)(obj->relocbase + rela->r_offset);
	Elf_Addr new_value;
	const Elf_Sym  *def;
	const Obj_Entry *defobj;

	assert(ELF_R_TYPE(rela->r_info) == R_TYPE(JMP_SLOT));

	def = _rtld_find_symdef(ELF_R_SYM(rela->r_info), obj, &defobj, true);
	if (def == NULL)
		return -1;

	new_value = (Elf_Addr)(defobj->relocbase + def->st_value);
	rdbg(dodebug, ("bind now/fixup in %s --> old=%p new=%p",
	    defobj->strtab + def->st_name, (void *)*where, (void *)new_value));
	if (*where != new_value)
		*where = new_value;

	*addrp = (caddr_t)new_value;
	return 0;
}
