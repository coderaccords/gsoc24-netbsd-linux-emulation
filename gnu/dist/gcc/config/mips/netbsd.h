/* Definitions for DECstation running BSD as target machine for GNU compiler.
   Copyright (C) 1993, 1995, 1996 Free Software Foundation, Inc.

This file is part of GNU CC.

GNU CC is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2, or (at your option)
any later version.

GNU CC is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with GNU CC; see the file COPYING.  If not, write to
the Free Software Foundation, 59 Temple Place - Suite 330,
Boston, MA 02111-1307, USA.  */

/* Define default target values. */

#define TARGET_ENDIAN_DEFAULT 0
#define TARGET_DEFAULT MASK_GAS

/* Get generic mips ELF definitions. */

#include <mips/elf.h>

/* Get generic NetBSD definitions. */

#define NETBSD_ELF
#include <netbsd.h>

/* Define mips-specific netbsd predefines... */

#undef CPP_PREDEFINES
#define CPP_PREDEFINES \
 "-D__ANSI_COMPAT -DMIPSEL -DR3000 -DSYSTYPE_BSD -D_SYSTYPE_BSD \
  -D__NetBSD__ -Dmips -D__NO_LEADING_UNDERSCORES__ -D__GP_SUPPORT__ \
  -Dunix -D_R3000 -Asystem(unix) -Asystem(NetBSD) -Amachine(mips)"

#undef SUBTARGET_CPP_SPEC
#define SUBTARGET_CPP_SPEC "%{posix:-D_POSIX_SOURCE}"

#undef MACHINE_TYPE
#define MACHINE_TYPE "NetBSD/mips"

/* Always uses gas.  */

#undef ASM_SPEC
#define ASM_SPEC \
 "%{G*} %{EB} %{EL} %{mips1} %{mips2} %{mips3} %{v} \
  %{noasmopt:-O0} \
  %{!noasmopt:%{O:-O2} %{O1:-O2} %{O2:-O2} %{O3:-O3}} \
  %{g} %{g0} %{g1} %{g2} %{g3} \
  %{ggdb:-g} %{ggdb0:-g0} %{ggdb1:-g1} %{ggdb2:-g2} %{ggdb3:-g3} \
  %{gstabs:-g} %{gstabs0:-g0} %{gstabs1:-g1} %{gstabs2:-g2} %{gstabs3:-g3} \
  %{gstabs+:-g} %{gstabs+0:-g0} %{gstabs+1:-g1} %{gstabs+2:-g2} %{gstabs+3:-g3} \
  %{gcoff:-g} %{gcoff0:-g0} %{gcoff1:-g1} %{gcoff2:-g2} %{gcoff3:-g3} \
  %{membedded-pic} %{fPIC:-KPIC}"

/* Provide a LINK_SPEC appropriate for NetBSD.  Here we provide support
   for the special GCC options -static, -assert, and -nostdlib.  */
 
#undef LINK_SPEC
#define LINK_SPEC \
  "%{G*} %{EB} %{EL} %{mips1} %{mips2} %{mips3} \
   %{bestGnum} %{shared} %{non_shared} \
   %{call_shared} %{no_archive} %{exact_version} \
   %{!shared: %{!non_shared: %{!call_shared: -non_shared}}} \
   %{!dynamic-linker:-dynamic-linker /usr/libexec/ld.elf_so} \
   %{nostdlib:-nostdlib} %{!nostdlib:%{!r*:%{!e*:-e __start}}} -dc -dp \
   %{static:-Bstatic} %{!static:-Bdynamic} %{assert*}"

/* Provide CC1_SPEC appropriate for NetBSD/mips ELF platforms */

#undef CC1_SPEC
#define CC1_SPEC \
 "%{gline:%{!g:%{!g0:%{!g1:%{!g2: -g1}}}}} \
  %{mips1:-mfp32 -mgp32}%{mips2:-mfp32 -mgp32}\
  %{mips3:%{!msingle-float:%{!m4650:-mfp64}} -mgp64} \
  %{mips4:%{!msingle-float:%{!m4650:-mfp64}} -mgp64} \
  %{mfp64:%{msingle-float:%emay not use both -mfp64 and -msingle-float}} \
  %{mfp64:%{m4650:%emay not use both -mfp64 and -m4650}} \
  %{m4650:-mcpu=r4650} \
  %{G*} %{EB:-meb} %{EL:-mel} %{EB:%{EL:%emay not use both -EB and -EL}} \
  %{pic-none:   -mno-half-pic} \
  %{pic-lib:    -mhalf-pic} \
  %{pic-extern: -mhalf-pic} \
  %{pic-calls:  -mhalf-pic} \
  %{save-temps: } \
  %{!mno-abicalls:    -mabicalls}"

#undef CPP_SPEC
#define CPP_SPEC \
 "%{posix:-D_POSIX_SOURCE} \
  %{mlong64:-D__SIZE_TYPE__=long\\ unsigned\\ int -D__PTRDIFF_TYPE__=long\\ int} \
  %{!mlong64:-D__SIZE_TYPE__=unsigned\\ int -D__PTRDIFF_TYPE__=int} \
  %{mips3:-U__mips -D__mips=3 -D__mips64} \
  %{mgp32:-U__mips64} %{mgp64:-D__mips64}"

/* We have atexit(3).  */

#define HAVE_ATEXIT

/* Trampoline code for closures should call _cacheflush()
    to ensure I-cache consistency after writing trampoline code.  */

#define MIPS_CACHEFLUSH_FUNC "_cacheflush"

/*
 * Some imports from svr4.h in support of shared libraries.
 * Currently, we need the DECLARE_OBJECT_SIZE stuff.
 */

/* Define the strings used for the special svr4 .type and .size directives.
   These strings generally do not vary from one system running svr4 to
   another, but if a given system (e.g. m88k running svr) needs to use
   different pseudo-op names for these, they may be overridden in the
   file which includes this one.  */

#undef TYPE_ASM_OP
#undef SIZE_ASM_OP
#undef WEAK_ASM_OP
#define TYPE_ASM_OP	".type"
#define SIZE_ASM_OP	".size"
#define WEAK_ASM_OP	".weak"

/* The following macro defines the format used to output the second
   operand of the .type assembler directive.  Different svr4 assemblers
   expect various different forms for this operand.  The one given here
   is just a default.  You may need to override it in your machine-
   specific tm.h file (depending upon the particulars of your assembler).  */

#undef TYPE_OPERAND_FMT
#define TYPE_OPERAND_FMT	"@%s"

/* Write the extra assembler code needed to declare a function's result.
   Most svr4 assemblers don't require any special declaration of the
   result value, but there are exceptions.  */

#ifndef ASM_DECLARE_RESULT
#define ASM_DECLARE_RESULT(FILE, RESULT)
#endif

/* These macros generate the special .type and .size directives which
   are used to set the corresponding fields of the linker symbol table
   entries in an ELF object file under SVR4.  These macros also output
   the starting labels for the relevant functions/objects.  */

/* Write the extra assembler code needed to declare a function properly.
   Some svr4 assemblers need to also have something extra said about the
   function's return value.  We allow for that here.  */

#undef ASM_DECLARE_FUNCTION_NAME
#define ASM_DECLARE_FUNCTION_NAME(FILE, NAME, DECL)			\
  do {									\
    fprintf (FILE, "\t%s\t ", TYPE_ASM_OP);				\
    assemble_name (FILE, NAME);						\
    putc (',', FILE);							\
    fprintf (FILE, TYPE_OPERAND_FMT, "function");			\
    putc ('\n', FILE);							\
    ASM_DECLARE_RESULT (FILE, DECL_RESULT (DECL));			\
  } while (0)

/* Write the extra assembler code needed to declare an object properly.  */

#undef ASM_DECLARE_OBJECT_NAME
#define ASM_DECLARE_OBJECT_NAME(FILE, NAME, DECL)			\
  do {									\
    fprintf (FILE, "\t%s\t ", TYPE_ASM_OP);				\
    assemble_name (FILE, NAME);						\
    putc (',', FILE);							\
    fprintf (FILE, TYPE_OPERAND_FMT, "object");				\
    putc ('\n', FILE);							\
    size_directive_output = 0;						\
    if (!flag_inhibit_size_directive && DECL_SIZE (DECL))		\
      {									\
	size_directive_output = 1;					\
	fprintf (FILE, "\t%s\t ", SIZE_ASM_OP);				\
	assemble_name (FILE, NAME);					\
	fprintf (FILE, ",%d\n",  int_size_in_bytes (TREE_TYPE (DECL)));	\
      }									\
    ASM_OUTPUT_LABEL(FILE, NAME);					\
  } while (0)

/* Output the size directive for a decl in rest_of_decl_compilation
   in the case where we did not do so before the initializer.
   Once we find the error_mark_node, we know that the value of
   size_directive_output was set
   by ASM_DECLARE_OBJECT_NAME when it was run for the same decl.  */

#undef ASM_FINISH_DECLARE_OBJECT
#define ASM_FINISH_DECLARE_OBJECT(FILE, DECL, TOP_LEVEL, AT_END)	 \
do {									 \
     char *name = XSTR (XEXP (DECL_RTL (DECL), 0), 0);			 \
     if (!flag_inhibit_size_directive && DECL_SIZE (DECL)		 \
         && ! AT_END && TOP_LEVEL					 \
	 && DECL_INITIAL (DECL) == error_mark_node			 \
	 && !size_directive_output)					 \
       {								 \
	 size_directive_output = 1;					 \
	 fprintf (FILE, "\t%s\t ", SIZE_ASM_OP);			 \
	 assemble_name (FILE, name);					 \
	 fprintf (FILE, ",%d\n",  int_size_in_bytes (TREE_TYPE (DECL))); \
       }								 \
   } while (0)

/* This is how to declare the size of a function.  */

#undef ASM_DECLARE_FUNCTION_SIZE
#define ASM_DECLARE_FUNCTION_SIZE(FILE, FNAME, DECL)			\
  do {									\
    if (!flag_inhibit_size_directive)					\
      {									\
        char label[256];						\
	static int labelno;						\
	labelno++;							\
	ASM_GENERATE_INTERNAL_LABEL (label, "Lfe", labelno);		\
	ASM_OUTPUT_INTERNAL_LABEL (FILE, "Lfe", labelno);		\
	fprintf (FILE, "\t%s\t ", SIZE_ASM_OP);				\
	assemble_name (FILE, (FNAME));					\
        fprintf (FILE, ",");						\
	assemble_name (FILE, label);					\
        fprintf (FILE, "-");						\
	assemble_name (FILE, (FNAME));					\
	putc ('\n', FILE);						\
      }									\
  } while (0)

/*
 A C statement to output something to the assembler file to switch to section
 NAME for object DECL which is either a FUNCTION_DECL, a VAR_DECL or
 NULL_TREE.  Some target formats do not support arbitrary sections.  Do not
 define this macro in such cases.
*/
#define ASM_OUTPUT_SECTION_NAME(F, DECL, NAME, RELOC)                        \
do {                                                                         \
  extern FILE *asm_out_text_file;                                            \
  if ((DECL) && TREE_CODE (DECL) == FUNCTION_DECL)                           \
    fprintf (asm_out_text_file, "\t.section %s,\"ax\",@progbits\n", (NAME)); \
  else if ((DECL) && DECL_READONLY_SECTION (DECL, RELOC))                    \
    fprintf (F, "\t.section %s,\"a\",@progbits\n", (NAME));                  \
  else                                                                       \
    fprintf (F, "\t.section %s,\"aw\",@progbits\n", (NAME));                 \
} while (0)

/* Since gas and gld are standard on NetBSD, we don't need these */
#undef ASM_FINAL_SPEC
