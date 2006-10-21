/* Prototypes of target machine for GNU compiler.  MIPS version.
   Copyright (C) 1989, 1990, 1991, 1992, 1993, 1994, 1995, 1996, 1997, 1998,
   1999, 2001, 2002, 2003, 2004, 2005 Free Software Foundation, Inc.
   Contributed by A. Lichnewsky (lich@inria.inria.fr).
   Changed by Michael Meissner	(meissner@osf.org).
   64 bit r4000 support by Ian Lance Taylor (ian@cygnus.com) and
   Brendan Eich (brendan@microunity.com).

This file is part of GCC.

GCC is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2, or (at your option)
any later version.

GCC is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with GCC; see the file COPYING.  If not, write to
the Free Software Foundation, 51 Franklin Street, Fifth Floor,
Boston, MA 02110-1301, USA.  */

#ifndef GCC_MIPS_PROTOS_H
#define GCC_MIPS_PROTOS_H

/* Classifies a SYMBOL_REF, LABEL_REF or UNSPEC address.

   SYMBOL_GENERAL
       Used when none of the below apply.

   SYMBOL_SMALL_DATA
       The symbol refers to something in a small data section.

   SYMBOL_CONSTANT_POOL
       The symbol refers to something in the mips16 constant pool.

   SYMBOL_GOT_LOCAL
       The symbol refers to local data that will be found using
       the global offset table.

   SYMBOL_GOT_GLOBAL
       Likewise non-local data.

   SYMBOL_GOTOFF_PAGE
       An UNSPEC wrapper around a SYMBOL_GOT_LOCAL.  It represents the
       offset from _gp of a GOT page entry.

   SYMBOL_GOTOFF_GLOBAL
       An UNSPEC wrapper around a SYMBOL_GOT_GLOBAL.  It represents the
       the offset from _gp of the symbol's GOT entry.

   SYMBOL_GOTOFF_CALL
       Like SYMBOL_GOTOFF_GLOBAL, but used when calling a global function.
       The GOT entry is allowed to point to a stub rather than to the
       function itself.

   SYMBOL_GOTOFF_LOADGP
       An UNSPEC wrapper around a function's address.  It represents the
       offset of _gp from the start of the function.

   SYMBOL_TLS
       A thread-local symbol.

   SYMBOL_TLSGD
   SYMBOL_TLSLDM
   SYMBOL_DTPREL
   SYMBOL_GOTTPREL
   SYMBOL_TPREL
       UNSPEC wrappers around SYMBOL_TLS, corresponding to the
       thread-local storage relocation operators.

   SYMBOL_64_HIGH
       For a 64-bit symbolic address X, this is the value of
       (%highest(X) << 16) + %higher(X).

   SYMBOL_64_MID
       For a 64-bit symbolic address X, this is the value of
       (%higher(X) << 16) + %hi(X).

   SYMBOL_64_LOW
       For a 64-bit symbolic address X, this is the value of
       (%hi(X) << 16) + %lo(X).  */
enum mips_symbol_type {
  SYMBOL_GENERAL,
  SYMBOL_SMALL_DATA,
  SYMBOL_CONSTANT_POOL,
  SYMBOL_GOT_LOCAL,
  SYMBOL_GOT_GLOBAL,
  SYMBOL_GOTOFF_PAGE,
  SYMBOL_GOTOFF_GLOBAL,
  SYMBOL_GOTOFF_CALL,
  SYMBOL_GOTOFF_LOADGP,
  SYMBOL_TLS,
  SYMBOL_TLSGD,
  SYMBOL_TLSLDM,
  SYMBOL_DTPREL,
  SYMBOL_GOTTPREL,
  SYMBOL_TPREL,
  SYMBOL_64_HIGH,
  SYMBOL_64_MID,
  SYMBOL_64_LOW
};
#define NUM_SYMBOL_TYPES (SYMBOL_64_LOW + 1)

extern bool mips_symbolic_constant_p (rtx, enum mips_symbol_type *);
extern bool mips_atomic_symbolic_constant_p (rtx);
extern int mips_regno_mode_ok_for_base_p (int, enum machine_mode, int);
extern bool mips_stack_address_p (rtx, enum machine_mode);
extern int mips_address_insns (rtx, enum machine_mode);
extern int mips_const_insns (rtx);
extern int mips_fetch_insns (rtx);
extern int mips_idiv_insns (void);
extern int fp_register_operand (rtx, enum machine_mode);
extern int lo_operand (rtx, enum machine_mode);
extern bool mips_legitimate_address_p (enum machine_mode, rtx, int);
extern rtx mips_unspec_address (rtx, enum mips_symbol_type);
extern bool mips_legitimize_address (rtx *, enum machine_mode);
extern bool mips_legitimize_move (enum machine_mode, rtx, rtx);

extern int m16_uimm3_b (rtx, enum machine_mode);
extern int m16_simm4_1 (rtx, enum machine_mode);
extern int m16_nsimm4_1 (rtx, enum machine_mode);
extern int m16_simm5_1 (rtx, enum machine_mode);
extern int m16_nsimm5_1 (rtx, enum machine_mode);
extern int m16_uimm5_4 (rtx, enum machine_mode);
extern int m16_nuimm5_4 (rtx, enum machine_mode);
extern int m16_simm8_1 (rtx, enum machine_mode);
extern int m16_nsimm8_1 (rtx, enum machine_mode);
extern int m16_uimm8_1 (rtx, enum machine_mode);
extern int m16_nuimm8_1 (rtx, enum machine_mode);
extern int m16_uimm8_m1_1 (rtx, enum machine_mode);
extern int m16_uimm8_4 (rtx, enum machine_mode);
extern int m16_nuimm8_4 (rtx, enum machine_mode);
extern int m16_simm8_8 (rtx, enum machine_mode);
extern int m16_nsimm8_8 (rtx, enum machine_mode);

extern rtx mips_subword (rtx, int);
extern bool mips_split_64bit_move_p (rtx, rtx);
extern void mips_split_64bit_move (rtx, rtx);
extern const char *mips_output_move (rtx, rtx);
extern void mips_restore_gp (void);
#ifdef RTX_CODE
extern bool mips_emit_scc (enum rtx_code, rtx);
extern void gen_conditional_branch (rtx *, enum rtx_code);
#endif
extern void gen_conditional_move (rtx *);
extern void mips_gen_conditional_trap (rtx *);
extern void mips_expand_call (rtx, rtx, rtx, rtx, int);
extern void mips_emit_fcc_reload (rtx, rtx, rtx);
extern void mips_set_return_address (rtx, rtx);
extern bool mips_expand_block_move (rtx, rtx, rtx);

extern void init_cumulative_args (CUMULATIVE_ARGS *, tree, rtx);
extern void function_arg_advance (CUMULATIVE_ARGS *, enum machine_mode,
				  tree, int);
extern struct rtx_def *function_arg (const CUMULATIVE_ARGS *,
				     enum machine_mode, tree, int);
extern int function_arg_boundary (enum machine_mode, tree);
extern bool mips_pad_arg_upward (enum machine_mode, tree);
extern bool mips_pad_reg_upward (enum machine_mode, tree);
extern void mips_va_start (tree, rtx);

extern bool mips_expand_unaligned_load (rtx, rtx, unsigned int, int);
extern bool mips_expand_unaligned_store (rtx, rtx, unsigned int, int);
extern bool mips_mem_fits_mode_p (enum machine_mode mode, rtx x);
extern void override_options (void);
extern void mips_conditional_register_usage (void);
extern void mips_order_regs_for_local_alloc (void);
extern HOST_WIDE_INT mips_debugger_offset (rtx, HOST_WIDE_INT);

extern void print_operand (FILE *, rtx, int);
extern void print_operand_address (FILE *, rtx);
extern int mips_output_external (FILE *, tree, const char *);
extern void mips_output_filename (FILE *, const char *);
extern void mips_output_ascii (FILE *, const char *, size_t, const char *);
extern void mips_output_aligned_bss (FILE *, tree, const char *,
				     unsigned HOST_WIDE_INT, int);
extern void mips_output_aligned_decl_common (FILE *, tree, const char *,
					     unsigned HOST_WIDE_INT,
					     unsigned int);
extern void mips_declare_common_object (FILE *, const char *,
					const char *, unsigned HOST_WIDE_INT,
					unsigned int, bool);
extern void mips_declare_object (FILE *, const char *, const char *,
				 const char *, ...) ATTRIBUTE_PRINTF_4;
extern void mips_declare_object_name (FILE *, const char *, tree);
extern void mips_finish_declare_object (FILE *, tree, int, int);

extern bool mips_small_data_pattern_p (rtx);
extern rtx mips_rewrite_small_data (rtx);
extern HOST_WIDE_INT compute_frame_size (HOST_WIDE_INT);
extern HOST_WIDE_INT mips_initial_elimination_offset (int, int);
extern rtx mips_return_addr (int, rtx);
extern void mips_expand_prologue (void);
extern void mips_expand_epilogue (int);
extern int mips_can_use_return_insn (void);
extern struct rtx_def *mips_function_value (tree, tree, enum machine_mode);

extern bool mips_cannot_change_mode_class (enum machine_mode,
					   enum machine_mode, enum reg_class);
extern bool mips_dangerous_for_la25_p (rtx);
extern enum reg_class mips_preferred_reload_class (rtx, enum reg_class);
extern enum reg_class mips_secondary_reload_class (enum reg_class,
						   enum machine_mode,
						   rtx, int);
extern int mips_class_max_nregs (enum reg_class, enum machine_mode);
extern int build_mips16_call_stub (rtx, rtx, rtx, int);
extern int mips_register_move_cost (enum machine_mode, enum reg_class,
				    enum reg_class);

extern int mips_adjust_insn_length (rtx, int);
extern const char *mips_output_load_label (void);
extern const char *mips_output_conditional_branch (rtx, rtx *, int, int,
						   int, int);
extern const char *mips_output_division (const char *, rtx *);
extern unsigned int mips_hard_regno_nregs (int, enum machine_mode);
extern bool mips_linked_madd_p (rtx, rtx);
extern rtx mips_prefetch_cookie (rtx, rtx);

extern void irix_asm_output_align (FILE *, unsigned);
extern const char *current_section_name (void);
extern unsigned int current_section_flags (void);
extern bool mips_use_ins_ext_p (rtx, rtx, rtx);

#endif /* ! GCC_MIPS_PROTOS_H */
