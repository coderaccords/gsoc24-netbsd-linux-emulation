/* .eh_frame section optimization.
   Copyright 2001, 2002, 2003, 2004, 2005, 2006, 2007
   Free Software Foundation, Inc.
   Written by Jakub Jelinek <jakub@redhat.com>.

   This file is part of BFD, the Binary File Descriptor library.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street - Fifth Floor, Boston,
   MA 02110-1301, USA.  */

#include "sysdep.h"
#include "bfd.h"
#include "libbfd.h"
#include "elf-bfd.h"
#include "elf/dwarf2.h"

#define EH_FRAME_HDR_SIZE 8

struct cie
{
  unsigned int length;
  unsigned int hash;
  unsigned char version;
  unsigned char local_personality;
  char augmentation[20];
  bfd_vma code_align;
  bfd_signed_vma data_align;
  bfd_vma ra_column;
  bfd_vma augmentation_size;
  union {
    struct elf_link_hash_entry *h;
    bfd_vma val;
    unsigned int reloc_index;
  } personality;
  asection *output_sec;
  struct eh_cie_fde *cie_inf;
  unsigned char per_encoding;
  unsigned char lsda_encoding;
  unsigned char fde_encoding;
  unsigned char initial_insn_length;
  unsigned char can_make_lsda_relative;
  unsigned char initial_instructions[50];
};



/* If *ITER hasn't reached END yet, read the next byte into *RESULT and
   move onto the next byte.  Return true on success.  */

static inline bfd_boolean
read_byte (bfd_byte **iter, bfd_byte *end, unsigned char *result)
{
  if (*iter >= end)
    return FALSE;
  *result = *((*iter)++);
  return TRUE;
}

/* Move *ITER over LENGTH bytes, or up to END, whichever is closer.
   Return true it was possible to move LENGTH bytes.  */

static inline bfd_boolean
skip_bytes (bfd_byte **iter, bfd_byte *end, bfd_size_type length)
{
  if ((bfd_size_type) (end - *iter) < length)
    {
      *iter = end;
      return FALSE;
    }
  *iter += length;
  return TRUE;
}

/* Move *ITER over an leb128, stopping at END.  Return true if the end
   of the leb128 was found.  */

static bfd_boolean
skip_leb128 (bfd_byte **iter, bfd_byte *end)
{
  unsigned char byte;
  do
    if (!read_byte (iter, end, &byte))
      return FALSE;
  while (byte & 0x80);
  return TRUE;
}

/* Like skip_leb128, but treat the leb128 as an unsigned value and
   store it in *VALUE.  */

static bfd_boolean
read_uleb128 (bfd_byte **iter, bfd_byte *end, bfd_vma *value)
{
  bfd_byte *start, *p;

  start = *iter;
  if (!skip_leb128 (iter, end))
    return FALSE;

  p = *iter;
  *value = *--p;
  while (p > start)
    *value = (*value << 7) | (*--p & 0x7f);

  return TRUE;
}

/* Like read_uleb128, but for signed values.  */

static bfd_boolean
read_sleb128 (bfd_byte **iter, bfd_byte *end, bfd_signed_vma *value)
{
  bfd_byte *start, *p;

  start = *iter;
  if (!skip_leb128 (iter, end))
    return FALSE;

  p = *iter;
  *value = ((*--p & 0x7f) ^ 0x40) - 0x40;
  while (p > start)
    *value = (*value << 7) | (*--p & 0x7f);

  return TRUE;
}

/* Return 0 if either encoding is variable width, or not yet known to bfd.  */

static
int get_DW_EH_PE_width (int encoding, int ptr_size)
{
  /* DW_EH_PE_ values of 0x60 and 0x70 weren't defined at the time .eh_frame
     was added to bfd.  */
  if ((encoding & 0x60) == 0x60)
    return 0;

  switch (encoding & 7)
    {
    case DW_EH_PE_udata2: return 2;
    case DW_EH_PE_udata4: return 4;
    case DW_EH_PE_udata8: return 8;
    case DW_EH_PE_absptr: return ptr_size;
    default:
      break;
    }

  return 0;
}

#define get_DW_EH_PE_signed(encoding) (((encoding) & DW_EH_PE_signed) != 0)

/* Read a width sized value from memory.  */

static bfd_vma
read_value (bfd *abfd, bfd_byte *buf, int width, int is_signed)
{
  bfd_vma value;

  switch (width)
    {
    case 2:
      if (is_signed)
	value = bfd_get_signed_16 (abfd, buf);
      else
	value = bfd_get_16 (abfd, buf);
      break;
    case 4:
      if (is_signed)
	value = bfd_get_signed_32 (abfd, buf);
      else
	value = bfd_get_32 (abfd, buf);
      break;
    case 8:
      if (is_signed)
	value = bfd_get_signed_64 (abfd, buf);
      else
	value = bfd_get_64 (abfd, buf);
      break;
    default:
      BFD_FAIL ();
      return 0;
    }

  return value;
}

/* Store a width sized value to memory.  */

static void
write_value (bfd *abfd, bfd_byte *buf, bfd_vma value, int width)
{
  switch (width)
    {
    case 2: bfd_put_16 (abfd, value, buf); break;
    case 4: bfd_put_32 (abfd, value, buf); break;
    case 8: bfd_put_64 (abfd, value, buf); break;
    default: BFD_FAIL ();
    }
}

/* Return one if C1 and C2 CIEs can be merged.  */

static int
cie_eq (const void *e1, const void *e2)
{
  const struct cie *c1 = e1;
  const struct cie *c2 = e2;

  if (c1->hash == c2->hash
      && c1->length == c2->length
      && c1->version == c2->version
      && c1->local_personality == c2->local_personality
      && strcmp (c1->augmentation, c2->augmentation) == 0
      && strcmp (c1->augmentation, "eh") != 0
      && c1->code_align == c2->code_align
      && c1->data_align == c2->data_align
      && c1->ra_column == c2->ra_column
      && c1->augmentation_size == c2->augmentation_size
      && memcmp (&c1->personality, &c2->personality,
		 sizeof (c1->personality)) == 0
      && c1->output_sec == c2->output_sec
      && c1->per_encoding == c2->per_encoding
      && c1->lsda_encoding == c2->lsda_encoding
      && c1->fde_encoding == c2->fde_encoding
      && c1->initial_insn_length == c2->initial_insn_length
      && memcmp (c1->initial_instructions,
		 c2->initial_instructions,
		 c1->initial_insn_length) == 0)
    return 1;

  return 0;
}

static hashval_t
cie_hash (const void *e)
{
  const struct cie *c = e;
  return c->hash;
}

static hashval_t
cie_compute_hash (struct cie *c)
{
  hashval_t h = 0;
  h = iterative_hash_object (c->length, h);
  h = iterative_hash_object (c->version, h);
  h = iterative_hash (c->augmentation, strlen (c->augmentation) + 1, h);
  h = iterative_hash_object (c->code_align, h);
  h = iterative_hash_object (c->data_align, h);
  h = iterative_hash_object (c->ra_column, h);
  h = iterative_hash_object (c->augmentation_size, h);
  h = iterative_hash_object (c->personality, h);
  h = iterative_hash_object (c->output_sec, h);
  h = iterative_hash_object (c->per_encoding, h);
  h = iterative_hash_object (c->lsda_encoding, h);
  h = iterative_hash_object (c->fde_encoding, h);
  h = iterative_hash_object (c->initial_insn_length, h);
  h = iterative_hash (c->initial_instructions, c->initial_insn_length, h);
  c->hash = h;
  return h;
}

/* Return the number of extra bytes that we'll be inserting into
   ENTRY's augmentation string.  */

static INLINE unsigned int
extra_augmentation_string_bytes (struct eh_cie_fde *entry)
{
  unsigned int size = 0;
  if (entry->cie)
    {
      if (entry->add_augmentation_size)
	size++;
      if (entry->u.cie.add_fde_encoding)
	size++;
    }
  return size;
}

/* Likewise ENTRY's augmentation data.  */

static INLINE unsigned int
extra_augmentation_data_bytes (struct eh_cie_fde *entry)
{
  unsigned int size = 0;
  if (entry->add_augmentation_size)
    size++;
  if (entry->cie && entry->u.cie.add_fde_encoding)
    size++;
  return size;
}

/* Return the size that ENTRY will have in the output.  ALIGNMENT is the
   required alignment of ENTRY in bytes.  */

static unsigned int
size_of_output_cie_fde (struct eh_cie_fde *entry, unsigned int alignment)
{
  if (entry->removed)
    return 0;
  if (entry->size == 4)
    return 4;
  return (entry->size
	  + extra_augmentation_string_bytes (entry)
	  + extra_augmentation_data_bytes (entry)
	  + alignment - 1) & -alignment;
}

/* Assume that the bytes between *ITER and END are CFA instructions.
   Try to move *ITER past the first instruction and return true on
   success.  ENCODED_PTR_WIDTH gives the width of pointer entries.  */

static bfd_boolean
skip_cfa_op (bfd_byte **iter, bfd_byte *end, unsigned int encoded_ptr_width)
{
  bfd_byte op;
  bfd_vma length;

  if (!read_byte (iter, end, &op))
    return FALSE;

  switch (op & 0xc0 ? op & 0xc0 : op)
    {
    case DW_CFA_nop:
    case DW_CFA_advance_loc:
    case DW_CFA_restore:
    case DW_CFA_remember_state:
    case DW_CFA_restore_state:
    case DW_CFA_GNU_window_save:
      /* No arguments.  */
      return TRUE;

    case DW_CFA_offset:
    case DW_CFA_restore_extended:
    case DW_CFA_undefined:
    case DW_CFA_same_value:
    case DW_CFA_def_cfa_register:
    case DW_CFA_def_cfa_offset:
    case DW_CFA_def_cfa_offset_sf:
    case DW_CFA_GNU_args_size:
      /* One leb128 argument.  */
      return skip_leb128 (iter, end);

    case DW_CFA_val_offset:
    case DW_CFA_val_offset_sf:
    case DW_CFA_offset_extended:
    case DW_CFA_register:
    case DW_CFA_def_cfa:
    case DW_CFA_offset_extended_sf:
    case DW_CFA_GNU_negative_offset_extended:
    case DW_CFA_def_cfa_sf:
      /* Two leb128 arguments.  */
      return (skip_leb128 (iter, end)
	      && skip_leb128 (iter, end));

    case DW_CFA_def_cfa_expression:
      /* A variable-length argument.  */
      return (read_uleb128 (iter, end, &length)
	      && skip_bytes (iter, end, length));

    case DW_CFA_expression:
    case DW_CFA_val_expression:
      /* A leb128 followed by a variable-length argument.  */
      return (skip_leb128 (iter, end)
	      && read_uleb128 (iter, end, &length)
	      && skip_bytes (iter, end, length));

    case DW_CFA_set_loc:
      return skip_bytes (iter, end, encoded_ptr_width);

    case DW_CFA_advance_loc1:
      return skip_bytes (iter, end, 1);

    case DW_CFA_advance_loc2:
      return skip_bytes (iter, end, 2);

    case DW_CFA_advance_loc4:
      return skip_bytes (iter, end, 4);

    case DW_CFA_MIPS_advance_loc8:
      return skip_bytes (iter, end, 8);

    default:
      return FALSE;
    }
}

/* Try to interpret the bytes between BUF and END as CFA instructions.
   If every byte makes sense, return a pointer to the first DW_CFA_nop
   padding byte, or END if there is no padding.  Return null otherwise.
   ENCODED_PTR_WIDTH is as for skip_cfa_op.  */

static bfd_byte *
skip_non_nops (bfd_byte *buf, bfd_byte *end, unsigned int encoded_ptr_width,
	       unsigned int *set_loc_count)
{
  bfd_byte *last;

  last = buf;
  while (buf < end)
    if (*buf == DW_CFA_nop)
      buf++;
    else
      {
	if (*buf == DW_CFA_set_loc)
	  ++*set_loc_count;
	if (!skip_cfa_op (&buf, end, encoded_ptr_width))
	  return 0;
	last = buf;
      }
  return last;
}

/* Called before calling _bfd_elf_parse_eh_frame on every input bfd's
   .eh_frame section.  */

void
_bfd_elf_begin_eh_frame_parsing (struct bfd_link_info *info)
{
  struct eh_frame_hdr_info *hdr_info;

  hdr_info = &elf_hash_table (info)->eh_info;
  hdr_info->merge_cies = !info->relocatable;
}

/* Try to parse .eh_frame section SEC, which belongs to ABFD.  Store the
   information in the section's sec_info field on success.  COOKIE
   describes the relocations in SEC.  */

void
_bfd_elf_parse_eh_frame (bfd *abfd, struct bfd_link_info *info,
			 asection *sec, struct elf_reloc_cookie *cookie)
{
#define REQUIRE(COND)					\
  do							\
    if (!(COND))					\
      goto free_no_table;				\
  while (0)

  bfd_byte *ehbuf = NULL, *buf, *end;
  bfd_byte *last_fde;
  struct eh_cie_fde *this_inf;
  unsigned int hdr_length, hdr_id;
  unsigned int cie_count;
  struct cie *cie, *local_cies = NULL;
  struct elf_link_hash_table *htab;
  struct eh_frame_hdr_info *hdr_info;
  struct eh_frame_sec_info *sec_info = NULL;
  unsigned int ptr_size;
  unsigned int num_cies;
  unsigned int num_entries;
  elf_gc_mark_hook_fn gc_mark_hook;

  htab = elf_hash_table (info);
  hdr_info = &htab->eh_info;
  if (hdr_info->parsed_eh_frames)
    return;

  if (sec->size == 0)
    {
      /* This file does not contain .eh_frame information.  */
      return;
    }

  if (bfd_is_abs_section (sec->output_section))
    {
      /* At least one of the sections is being discarded from the
	 link, so we should just ignore them.  */
      return;
    }

  /* Read the frame unwind information from abfd.  */

  REQUIRE (bfd_malloc_and_get_section (abfd, sec, &ehbuf));

  if (sec->size >= 4
      && bfd_get_32 (abfd, ehbuf) == 0
      && cookie->rel == cookie->relend)
    {
      /* Empty .eh_frame section.  */
      free (ehbuf);
      return;
    }

  /* If .eh_frame section size doesn't fit into int, we cannot handle
     it (it would need to use 64-bit .eh_frame format anyway).  */
  REQUIRE (sec->size == (unsigned int) sec->size);

  ptr_size = (get_elf_backend_data (abfd)
	      ->elf_backend_eh_frame_address_size (abfd, sec));
  REQUIRE (ptr_size != 0);

  /* Go through the section contents and work out how many FDEs and
     CIEs there are.  */
  buf = ehbuf;
  end = ehbuf + sec->size;
  num_cies = 0;
  num_entries = 0;
  while (buf != end)
    {
      num_entries++;

      /* Read the length of the entry.  */
      REQUIRE (skip_bytes (&buf, end, 4));
      hdr_length = bfd_get_32 (abfd, buf - 4);

      /* 64-bit .eh_frame is not supported.  */
      REQUIRE (hdr_length != 0xffffffff);
      if (hdr_length == 0)
	break;

      REQUIRE (skip_bytes (&buf, end, 4));
      hdr_id = bfd_get_32 (abfd, buf - 4);
      if (hdr_id == 0)
	num_cies++;

      REQUIRE (skip_bytes (&buf, end, hdr_length - 4));
    }

  sec_info = bfd_zmalloc (sizeof (struct eh_frame_sec_info)
			  + (num_entries - 1) * sizeof (struct eh_cie_fde));
  REQUIRE (sec_info);

  /* We need to have a "struct cie" for each CIE in this section.  */
  local_cies = bfd_zmalloc (num_cies * sizeof (*local_cies));
  REQUIRE (local_cies);

#define ENSURE_NO_RELOCS(buf)				\
  REQUIRE (!(cookie->rel < cookie->relend		\
	     && (cookie->rel->r_offset			\
		 < (bfd_size_type) ((buf) - ehbuf))	\
	     && cookie->rel->r_info != 0))

#define SKIP_RELOCS(buf)				\
  while (cookie->rel < cookie->relend			\
	 && (cookie->rel->r_offset			\
	     < (bfd_size_type) ((buf) - ehbuf)))	\
    cookie->rel++

#define GET_RELOC(buf)					\
  ((cookie->rel < cookie->relend			\
    && (cookie->rel->r_offset				\
	== (bfd_size_type) ((buf) - ehbuf)))		\
   ? cookie->rel : NULL)

  buf = ehbuf;
  cie_count = 0;
  gc_mark_hook = get_elf_backend_data (abfd)->gc_mark_hook;
  while ((bfd_size_type) (buf - ehbuf) != sec->size)
    {
      char *aug;
      bfd_byte *start, *insns, *insns_end;
      bfd_size_type length;
      unsigned int set_loc_count;

      this_inf = sec_info->entry + sec_info->count;
      last_fde = buf;

      /* Read the length of the entry.  */
      REQUIRE (skip_bytes (&buf, ehbuf + sec->size, 4));
      hdr_length = bfd_get_32 (abfd, buf - 4);

      /* The CIE/FDE must be fully contained in this input section.  */
      REQUIRE ((bfd_size_type) (buf - ehbuf) + hdr_length <= sec->size);
      end = buf + hdr_length;

      this_inf->offset = last_fde - ehbuf;
      this_inf->size = 4 + hdr_length;
      this_inf->reloc_index = cookie->rel - cookie->rels;

      if (hdr_length == 0)
	{
	  /* A zero-length CIE should only be found at the end of
	     the section.  */
	  REQUIRE ((bfd_size_type) (buf - ehbuf) == sec->size);
	  ENSURE_NO_RELOCS (buf);
	  sec_info->count++;
	  break;
	}

      REQUIRE (skip_bytes (&buf, end, 4));
      hdr_id = bfd_get_32 (abfd, buf - 4);

      if (hdr_id == 0)
	{
	  unsigned int initial_insn_length;

	  /* CIE  */
	  this_inf->cie = 1;

	  /* Point CIE to one of the section-local cie structures.  */
	  cie = local_cies + cie_count++;

	  cie->cie_inf = this_inf;
	  cie->length = hdr_length;
	  cie->output_sec = sec->output_section;
	  start = buf;
	  REQUIRE (read_byte (&buf, end, &cie->version));

	  /* Cannot handle unknown versions.  */
	  REQUIRE (cie->version == 1 || cie->version == 3);
	  REQUIRE (strlen ((char *) buf) < sizeof (cie->augmentation));

	  strcpy (cie->augmentation, (char *) buf);
	  buf = (bfd_byte *) strchr ((char *) buf, '\0') + 1;
	  ENSURE_NO_RELOCS (buf);
	  if (buf[0] == 'e' && buf[1] == 'h')
	    {
	      /* GCC < 3.0 .eh_frame CIE */
	      /* We cannot merge "eh" CIEs because __EXCEPTION_TABLE__
		 is private to each CIE, so we don't need it for anything.
		 Just skip it.  */
	      REQUIRE (skip_bytes (&buf, end, ptr_size));
	      SKIP_RELOCS (buf);
	    }
	  REQUIRE (read_uleb128 (&buf, end, &cie->code_align));
	  REQUIRE (read_sleb128 (&buf, end, &cie->data_align));
	  if (cie->version == 1)
	    {
	      REQUIRE (buf < end);
	      cie->ra_column = *buf++;
	    }
	  else
	    REQUIRE (read_uleb128 (&buf, end, &cie->ra_column));
	  ENSURE_NO_RELOCS (buf);
	  cie->lsda_encoding = DW_EH_PE_omit;
	  cie->fde_encoding = DW_EH_PE_omit;
	  cie->per_encoding = DW_EH_PE_omit;
	  aug = cie->augmentation;
	  if (aug[0] != 'e' || aug[1] != 'h')
	    {
	      if (*aug == 'z')
		{
		  aug++;
		  REQUIRE (read_uleb128 (&buf, end, &cie->augmentation_size));
	  	  ENSURE_NO_RELOCS (buf);
		}

	      while (*aug != '\0')
		switch (*aug++)
		  {
		  case 'L':
		    REQUIRE (read_byte (&buf, end, &cie->lsda_encoding));
		    ENSURE_NO_RELOCS (buf);
		    REQUIRE (get_DW_EH_PE_width (cie->lsda_encoding, ptr_size));
		    break;
		  case 'R':
		    REQUIRE (read_byte (&buf, end, &cie->fde_encoding));
		    ENSURE_NO_RELOCS (buf);
		    REQUIRE (get_DW_EH_PE_width (cie->fde_encoding, ptr_size));
		    break;
		  case 'S':
		    break;
		  case 'P':
		    {
		      int per_width;

		      REQUIRE (read_byte (&buf, end, &cie->per_encoding));
		      per_width = get_DW_EH_PE_width (cie->per_encoding,
						      ptr_size);
		      REQUIRE (per_width);
		      if ((cie->per_encoding & 0xf0) == DW_EH_PE_aligned)
			{
			  length = -(buf - ehbuf) & (per_width - 1);
			  REQUIRE (skip_bytes (&buf, end, length));
			}
		      ENSURE_NO_RELOCS (buf);
		      /* Ensure we have a reloc here.  */
		      REQUIRE (GET_RELOC (buf));
		      cie->personality.reloc_index
			= cookie->rel - cookie->rels;
		      /* Cope with MIPS-style composite relocations.  */
		      do
			cookie->rel++;
		      while (GET_RELOC (buf) != NULL);
		      REQUIRE (skip_bytes (&buf, end, per_width));
		    }
		    break;
		  default:
		    /* Unrecognized augmentation. Better bail out.  */
		    goto free_no_table;
		  }
	    }

	  /* For shared libraries, try to get rid of as many RELATIVE relocs
	     as possible.  */
	  if (info->shared
	      && (get_elf_backend_data (abfd)
		  ->elf_backend_can_make_relative_eh_frame
		  (abfd, info, sec)))
	    {
	      if ((cie->fde_encoding & 0xf0) == DW_EH_PE_absptr)
		this_inf->make_relative = 1;
	      /* If the CIE doesn't already have an 'R' entry, it's fairly
		 easy to add one, provided that there's no aligned data
		 after the augmentation string.  */
	      else if (cie->fde_encoding == DW_EH_PE_omit
		       && (cie->per_encoding & 0xf0) != DW_EH_PE_aligned)
		{
		  if (*cie->augmentation == 0)
		    this_inf->add_augmentation_size = 1;
		  this_inf->u.cie.add_fde_encoding = 1;
		  this_inf->make_relative = 1;
		}
	    }

	  if (info->shared
	      && (get_elf_backend_data (abfd)
		  ->elf_backend_can_make_lsda_relative_eh_frame
		  (abfd, info, sec))
	      && (cie->lsda_encoding & 0xf0) == DW_EH_PE_absptr)
	    cie->can_make_lsda_relative = 1;

	  /* If FDE encoding was not specified, it defaults to
	     DW_EH_absptr.  */
	  if (cie->fde_encoding == DW_EH_PE_omit)
	    cie->fde_encoding = DW_EH_PE_absptr;

	  initial_insn_length = end - buf;
	  if (initial_insn_length <= sizeof (cie->initial_instructions))
	    {
	      cie->initial_insn_length = initial_insn_length;
	      memcpy (cie->initial_instructions, buf, initial_insn_length);
	    }
	  insns = buf;
	  buf += initial_insn_length;
	  ENSURE_NO_RELOCS (buf);

	  if (hdr_info->merge_cies)
	    this_inf->u.cie.u.full_cie = cie;
	  this_inf->u.cie.per_encoding_relative
	    = (cie->per_encoding & 0x70) == DW_EH_PE_pcrel;
	}
      else
	{
	  asection *rsec;

	  /* Find the corresponding CIE.  */
	  unsigned int cie_offset = this_inf->offset + 4 - hdr_id;
	  for (cie = local_cies; cie < local_cies + cie_count; cie++)
	    if (cie_offset == cie->cie_inf->offset)
	      break;

	  /* Ensure this FDE references one of the CIEs in this input
	     section.  */
	  REQUIRE (cie != local_cies + cie_count);
	  this_inf->u.fde.cie_inf = cie->cie_inf;
	  this_inf->make_relative = cie->cie_inf->make_relative;
	  this_inf->add_augmentation_size
	    = cie->cie_inf->add_augmentation_size;

	  ENSURE_NO_RELOCS (buf);
	  REQUIRE (GET_RELOC (buf));

	  /* Chain together the FDEs for each section.  */
	  rsec = _bfd_elf_gc_mark_rsec (info, sec, gc_mark_hook, cookie);
	  /* RSEC will be NULL if FDE was cleared out as it was belonging to
	     a discarded SHT_GROUP.  */
	  if (rsec)
	    {
	      REQUIRE (rsec->owner == abfd);
	      this_inf->u.fde.next_for_section = elf_fde_list (rsec);
	      elf_fde_list (rsec) = this_inf;
	    }

	  /* Skip the initial location and address range.  */
	  start = buf;
	  length = get_DW_EH_PE_width (cie->fde_encoding, ptr_size);
	  REQUIRE (skip_bytes (&buf, end, 2 * length));

	  /* Skip the augmentation size, if present.  */
	  if (cie->augmentation[0] == 'z')
	    REQUIRE (read_uleb128 (&buf, end, &length));
	  else
	    length = 0;

	  /* Of the supported augmentation characters above, only 'L'
	     adds augmentation data to the FDE.  This code would need to
	     be adjusted if any future augmentations do the same thing.  */
	  if (cie->lsda_encoding != DW_EH_PE_omit)
	    {
	      SKIP_RELOCS (buf);
	      if (cie->can_make_lsda_relative && GET_RELOC (buf))
		cie->cie_inf->u.cie.make_lsda_relative = 1;
	      this_inf->lsda_offset = buf - start;
	      /* If there's no 'z' augmentation, we don't know where the
		 CFA insns begin.  Assume no padding.  */
	      if (cie->augmentation[0] != 'z')
		length = end - buf;
	    }

	  /* Skip over the augmentation data.  */
	  REQUIRE (skip_bytes (&buf, end, length));
	  insns = buf;

	  buf = last_fde + 4 + hdr_length;

	  /* For NULL RSEC (cleared FDE belonging to a discarded section)
	     the relocations are commonly cleared.  We do not sanity check if
	     all these relocations are cleared as (1) relocations to
	     .gcc_except_table will remain uncleared (they will get dropped
	     with the drop of this unused FDE) and (2) BFD already safely drops
	     relocations of any type to .eh_frame by
	     elf_section_ignore_discarded_relocs.
	     TODO: The .gcc_except_table entries should be also filtered as
	     .eh_frame entries; or GCC could rather use COMDAT for them.  */
	  SKIP_RELOCS (buf);
	}

      /* Try to interpret the CFA instructions and find the first
	 padding nop.  Shrink this_inf's size so that it doesn't
	 include the padding.  */
      length = get_DW_EH_PE_width (cie->fde_encoding, ptr_size);
      set_loc_count = 0;
      insns_end = skip_non_nops (insns, end, length, &set_loc_count);
      /* If we don't understand the CFA instructions, we can't know
	 what needs to be adjusted there.  */
      if (insns_end == NULL
	  /* For the time being we don't support DW_CFA_set_loc in
	     CIE instructions.  */
	  || (set_loc_count && this_inf->cie))
	goto free_no_table;
      this_inf->size -= end - insns_end;
      if (insns_end != end && this_inf->cie)
	{
	  cie->initial_insn_length -= end - insns_end;
	  cie->length -= end - insns_end;
	}
      if (set_loc_count
	  && ((cie->fde_encoding & 0xf0) == DW_EH_PE_pcrel
	      || this_inf->make_relative))
	{
	  unsigned int cnt;
	  bfd_byte *p;

	  this_inf->set_loc = bfd_malloc ((set_loc_count + 1)
					  * sizeof (unsigned int));
	  REQUIRE (this_inf->set_loc);
	  this_inf->set_loc[0] = set_loc_count;
	  p = insns;
	  cnt = 0;
	  while (p < end)
	    {
	      if (*p == DW_CFA_set_loc)
		this_inf->set_loc[++cnt] = p + 1 - start;
	      REQUIRE (skip_cfa_op (&p, end, length));
	    }
	}

      this_inf->removed = 1;
      this_inf->fde_encoding = cie->fde_encoding;
      this_inf->lsda_encoding = cie->lsda_encoding;
      sec_info->count++;
    }
  BFD_ASSERT (sec_info->count == num_entries);
  BFD_ASSERT (cie_count == num_cies);

  elf_section_data (sec)->sec_info = sec_info;
  sec->sec_info_type = ELF_INFO_TYPE_EH_FRAME;
  if (hdr_info->merge_cies)
    {
      sec_info->cies = local_cies;
      local_cies = NULL;
    }
  goto success;

 free_no_table:
  (*info->callbacks->einfo)
    (_("%P: error in %B(%A); no .eh_frame_hdr table will be created.\n"),
     abfd, sec);
  hdr_info->table = FALSE;
  if (sec_info)
    free (sec_info);
 success:
  if (ehbuf)
    free (ehbuf);
  if (local_cies)
    free (local_cies);
#undef REQUIRE
}

/* Finish a pass over all .eh_frame sections.  */

void
_bfd_elf_end_eh_frame_parsing (struct bfd_link_info *info)
{
  struct eh_frame_hdr_info *hdr_info;

  hdr_info = &elf_hash_table (info)->eh_info;
  hdr_info->parsed_eh_frames = TRUE;
}

/* Mark all relocations against CIE or FDE ENT, which occurs in
   .eh_frame section SEC.  COOKIE describes the relocations in SEC;
   its "rel" field can be changed freely.  */

static bfd_boolean
mark_entry (struct bfd_link_info *info, asection *sec,
	    struct eh_cie_fde *ent, elf_gc_mark_hook_fn gc_mark_hook,
	    struct elf_reloc_cookie *cookie)
{
  for (cookie->rel = cookie->rels + ent->reloc_index;
       cookie->rel < cookie->relend
	 && cookie->rel->r_offset < ent->offset + ent->size;
       cookie->rel++)
    if (!_bfd_elf_gc_mark_reloc (info, sec, gc_mark_hook, cookie))
      return FALSE;

  return TRUE;
}

/* Mark all the relocations against FDEs that relate to code in input
   section SEC.  The FDEs belong to .eh_frame section EH_FRAME, whose
   relocations are described by COOKIE.  */

bfd_boolean
_bfd_elf_gc_mark_fdes (struct bfd_link_info *info, asection *sec,
		       asection *eh_frame, elf_gc_mark_hook_fn gc_mark_hook,
		       struct elf_reloc_cookie *cookie)
{
  struct eh_cie_fde *fde, *cie;

  for (fde = elf_fde_list (sec); fde; fde = fde->u.fde.next_for_section)
    {
      if (!mark_entry (info, eh_frame, fde, gc_mark_hook, cookie))
	return FALSE;

      /* At this stage, all cie_inf fields point to local CIEs, so we
	 can use the same cookie to refer to them.  */
      cie = fde->u.fde.cie_inf;
      if (!cie->u.cie.gc_mark)
	{
	  cie->u.cie.gc_mark = 1;
	  if (!mark_entry (info, eh_frame, cie, gc_mark_hook, cookie))
	    return FALSE;
	}
    }
  return TRUE;
}

/* Input section SEC of ABFD is an .eh_frame section that contains the
   CIE described by CIE_INF.  Return a version of CIE_INF that is going
   to be kept in the output, adding CIE_INF to the output if necessary.

   HDR_INFO is the .eh_frame_hdr information and COOKIE describes the
   relocations in REL.  */

static struct eh_cie_fde *
find_merged_cie (bfd *abfd, asection *sec,
		 struct eh_frame_hdr_info *hdr_info,
		 struct elf_reloc_cookie *cookie,
		 struct eh_cie_fde *cie_inf)
{
  unsigned long r_symndx;
  struct cie *cie, *new_cie;
  Elf_Internal_Rela *rel;
  void **loc;

  /* Use CIE_INF if we have already decided to keep it.  */
  if (!cie_inf->removed)
    return cie_inf;

  /* If we have merged CIE_INF with another CIE, use that CIE instead.  */
  if (cie_inf->u.cie.merged)
    return cie_inf->u.cie.u.merged_with;

  cie = cie_inf->u.cie.u.full_cie;

  /* Assume we will need to keep CIE_INF.  */
  cie_inf->removed = 0;
  cie_inf->u.cie.u.sec = sec;

  /* If we are not merging CIEs, use CIE_INF.  */
  if (cie == NULL)
    return cie_inf;

  if (cie->per_encoding != DW_EH_PE_omit)
    {
      /* Work out the address of personality routine, either as an absolute
	 value or as a symbol.  */
      rel = cookie->rels + cie->personality.reloc_index;
      memset (&cie->personality, 0, sizeof (cie->personality));
#ifdef BFD64
      if (elf_elfheader (abfd)->e_ident[EI_CLASS] == ELFCLASS64)
	r_symndx = ELF64_R_SYM (rel->r_info);
      else
#endif
	r_symndx = ELF32_R_SYM (rel->r_info);
      if (r_symndx >= cookie->locsymcount
	  || ELF_ST_BIND (cookie->locsyms[r_symndx].st_info) != STB_LOCAL)
	{
	  struct elf_link_hash_entry *h;

	  r_symndx -= cookie->extsymoff;
	  h = cookie->sym_hashes[r_symndx];

	  while (h->root.type == bfd_link_hash_indirect
		 || h->root.type == bfd_link_hash_warning)
	    h = (struct elf_link_hash_entry *) h->root.u.i.link;

	  cie->personality.h = h;
	}
      else
	{
	  Elf_Internal_Sym *sym;
	  asection *sym_sec;

	  sym = &cookie->locsyms[r_symndx];
	  sym_sec = bfd_section_from_elf_index (abfd, sym->st_shndx);
	  if (sym_sec == NULL)
	    return cie_inf;

	  if (sym_sec->kept_section != NULL)
	    sym_sec = sym_sec->kept_section;
	  if (sym_sec->output_section == NULL)
	    return cie_inf;

	  cie->local_personality = 1;
	  cie->personality.val = (sym->st_value
				  + sym_sec->output_offset
				  + sym_sec->output_section->vma);
	}
    }

  /* See if we can merge this CIE with an earlier one.  */
  cie->output_sec = sec->output_section;
  cie_compute_hash (cie);
  if (hdr_info->cies == NULL)
    {
      hdr_info->cies = htab_try_create (1, cie_hash, cie_eq, free);
      if (hdr_info->cies == NULL)
	return cie_inf;
    }
  loc = htab_find_slot_with_hash (hdr_info->cies, cie, cie->hash, INSERT);
  if (loc == NULL)
    return cie_inf;

  new_cie = (struct cie *) *loc;
  if (new_cie == NULL)
    {
      /* Keep CIE_INF and record it in the hash table.  */
      new_cie = malloc (sizeof (struct cie));
      if (new_cie == NULL)
	return cie_inf;

      memcpy (new_cie, cie, sizeof (struct cie));
      *loc = new_cie;
    }
  else
    {
      /* Merge CIE_INF with NEW_CIE->CIE_INF.  */
      cie_inf->removed = 1;
      cie_inf->u.cie.merged = 1;
      cie_inf->u.cie.u.merged_with = new_cie->cie_inf;
      if (cie_inf->u.cie.make_lsda_relative)
	new_cie->cie_inf->u.cie.make_lsda_relative = 1;
    }
  return new_cie->cie_inf;
}

/* This function is called for each input file before the .eh_frame
   section is relocated.  It discards duplicate CIEs and FDEs for discarded
   functions.  The function returns TRUE iff any entries have been
   deleted.  */

bfd_boolean
_bfd_elf_discard_section_eh_frame
   (bfd *abfd, struct bfd_link_info *info, asection *sec,
    bfd_boolean (*reloc_symbol_deleted_p) (bfd_vma, void *),
    struct elf_reloc_cookie *cookie)
{
  struct eh_cie_fde *ent;
  struct eh_frame_sec_info *sec_info;
  struct eh_frame_hdr_info *hdr_info;
  unsigned int ptr_size, offset;

  sec_info = (struct eh_frame_sec_info *) elf_section_data (sec)->sec_info;
  if (sec_info == NULL)
    return FALSE;

  hdr_info = &elf_hash_table (info)->eh_info;
  for (ent = sec_info->entry; ent < sec_info->entry + sec_info->count; ++ent)
    if (ent->size == 4)
      /* There should only be one zero terminator, on the last input
	 file supplying .eh_frame (crtend.o).  Remove any others.  */
      ent->removed = sec->map_head.s != NULL;
    else if (!ent->cie)
      {
	cookie->rel = cookie->rels + ent->reloc_index;
	BFD_ASSERT (cookie->rel < cookie->relend
		    && cookie->rel->r_offset == ent->offset + 8);
	if (!(*reloc_symbol_deleted_p) (ent->offset + 8, cookie))
	  {
	    if (info->shared
		&& (((ent->fde_encoding & 0xf0) == DW_EH_PE_absptr
		     && ent->make_relative == 0)
		    || (ent->fde_encoding & 0xf0) == DW_EH_PE_aligned))
	      {
		/* If a shared library uses absolute pointers
		   which we cannot turn into PC relative,
		   don't create the binary search table,
		   since it is affected by runtime relocations.  */
		hdr_info->table = FALSE;
		(*info->callbacks->einfo)
		  (_("%P: fde encoding in %B(%A) prevents .eh_frame_hdr"
		     " table being created.\n"), abfd, sec);
	      }
	    ent->removed = 0;
	    hdr_info->fde_count++;
	    ent->u.fde.cie_inf = find_merged_cie (abfd, sec, hdr_info, cookie,
						  ent->u.fde.cie_inf);
	  }
      }

  if (sec_info->cies)
    {
      free (sec_info->cies);
      sec_info->cies = NULL;
    }

  ptr_size = (get_elf_backend_data (sec->owner)
	      ->elf_backend_eh_frame_address_size (sec->owner, sec));
  offset = 0;
  for (ent = sec_info->entry; ent < sec_info->entry + sec_info->count; ++ent)
    if (!ent->removed)
      {
	ent->new_offset = offset;
	offset += size_of_output_cie_fde (ent, ptr_size);
      }

  sec->rawsize = sec->size;
  sec->size = offset;
  return offset != sec->rawsize;
}

/* This function is called for .eh_frame_hdr section after
   _bfd_elf_discard_section_eh_frame has been called on all .eh_frame
   input sections.  It finalizes the size of .eh_frame_hdr section.  */

bfd_boolean
_bfd_elf_discard_section_eh_frame_hdr (bfd *abfd, struct bfd_link_info *info)
{
  struct elf_link_hash_table *htab;
  struct eh_frame_hdr_info *hdr_info;
  asection *sec;

  htab = elf_hash_table (info);
  hdr_info = &htab->eh_info;

  if (hdr_info->cies != NULL)
    {
      htab_delete (hdr_info->cies);
      hdr_info->cies = NULL;
    }

  sec = hdr_info->hdr_sec;
  if (sec == NULL)
    return FALSE;

  sec->size = EH_FRAME_HDR_SIZE;
  if (hdr_info->table)
    sec->size += 4 + hdr_info->fde_count * 8;

  elf_tdata (abfd)->eh_frame_hdr = sec;
  return TRUE;
}

/* This function is called from size_dynamic_sections.
   It needs to decide whether .eh_frame_hdr should be output or not,
   because when the dynamic symbol table has been sized it is too late
   to strip sections.  */

bfd_boolean
_bfd_elf_maybe_strip_eh_frame_hdr (struct bfd_link_info *info)
{
  asection *o;
  bfd *abfd;
  struct elf_link_hash_table *htab;
  struct eh_frame_hdr_info *hdr_info;

  htab = elf_hash_table (info);
  hdr_info = &htab->eh_info;
  if (hdr_info->hdr_sec == NULL)
    return TRUE;

  if (bfd_is_abs_section (hdr_info->hdr_sec->output_section))
    {
      hdr_info->hdr_sec = NULL;
      return TRUE;
    }

  abfd = NULL;
  if (info->eh_frame_hdr)
    for (abfd = info->input_bfds; abfd != NULL; abfd = abfd->link_next)
      {
	/* Count only sections which have at least a single CIE or FDE.
	   There cannot be any CIE or FDE <= 8 bytes.  */
	o = bfd_get_section_by_name (abfd, ".eh_frame");
	if (o && o->size > 8 && !bfd_is_abs_section (o->output_section))
	  break;
      }

  if (abfd == NULL)
    {
      hdr_info->hdr_sec->flags |= SEC_EXCLUDE;
      hdr_info->hdr_sec = NULL;
      return TRUE;
    }

  hdr_info->table = TRUE;
  return TRUE;
}

/* Adjust an address in the .eh_frame section.  Given OFFSET within
   SEC, this returns the new offset in the adjusted .eh_frame section,
   or -1 if the address refers to a CIE/FDE which has been removed
   or to offset with dynamic relocation which is no longer needed.  */

bfd_vma
_bfd_elf_eh_frame_section_offset (bfd *output_bfd ATTRIBUTE_UNUSED,
				  struct bfd_link_info *info,
				  asection *sec,
				  bfd_vma offset)
{
  struct eh_frame_sec_info *sec_info;
  struct elf_link_hash_table *htab;
  struct eh_frame_hdr_info *hdr_info;
  unsigned int lo, hi, mid;

  if (sec->sec_info_type != ELF_INFO_TYPE_EH_FRAME)
    return offset;
  sec_info = elf_section_data (sec)->sec_info;

  if (offset >= sec->rawsize)
    return offset - sec->rawsize + sec->size;

  htab = elf_hash_table (info);
  hdr_info = &htab->eh_info;

  lo = 0;
  hi = sec_info->count;
  mid = 0;
  while (lo < hi)
    {
      mid = (lo + hi) / 2;
      if (offset < sec_info->entry[mid].offset)
	hi = mid;
      else if (offset
	       >= sec_info->entry[mid].offset + sec_info->entry[mid].size)
	lo = mid + 1;
      else
	break;
    }

  BFD_ASSERT (lo < hi);

  /* FDE or CIE was removed.  */
  if (sec_info->entry[mid].removed)
    return (bfd_vma) -1;

  /* If converting to DW_EH_PE_pcrel, there will be no need for run-time
     relocation against FDE's initial_location field.  */
  if (!sec_info->entry[mid].cie
      && sec_info->entry[mid].make_relative
      && offset == sec_info->entry[mid].offset + 8)
    return (bfd_vma) -2;

  /* If converting LSDA pointers to DW_EH_PE_pcrel, there will be no need
     for run-time relocation against LSDA field.  */
  if (!sec_info->entry[mid].cie
      && sec_info->entry[mid].u.fde.cie_inf->u.cie.make_lsda_relative
      && offset == (sec_info->entry[mid].offset + 8
		    + sec_info->entry[mid].lsda_offset))
    return (bfd_vma) -2;

  /* If converting to DW_EH_PE_pcrel, there will be no need for run-time
     relocation against DW_CFA_set_loc's arguments.  */
  if (sec_info->entry[mid].set_loc
      && sec_info->entry[mid].make_relative
      && (offset >= sec_info->entry[mid].offset + 8
		    + sec_info->entry[mid].set_loc[1]))
    {
      unsigned int cnt;

      for (cnt = 1; cnt <= sec_info->entry[mid].set_loc[0]; cnt++)
	if (offset == sec_info->entry[mid].offset + 8
		      + sec_info->entry[mid].set_loc[cnt])
	  return (bfd_vma) -2;
    }

  /* Any new augmentation bytes go before the first relocation.  */
  return (offset + sec_info->entry[mid].new_offset
	  - sec_info->entry[mid].offset
	  + extra_augmentation_string_bytes (sec_info->entry + mid)
	  + extra_augmentation_data_bytes (sec_info->entry + mid));
}

/* Write out .eh_frame section.  This is called with the relocated
   contents.  */

bfd_boolean
_bfd_elf_write_section_eh_frame (bfd *abfd,
				 struct bfd_link_info *info,
				 asection *sec,
				 bfd_byte *contents)
{
  struct eh_frame_sec_info *sec_info;
  struct elf_link_hash_table *htab;
  struct eh_frame_hdr_info *hdr_info;
  unsigned int ptr_size;
  struct eh_cie_fde *ent;

  if (sec->sec_info_type != ELF_INFO_TYPE_EH_FRAME)
    return bfd_set_section_contents (abfd, sec->output_section, contents,
				     sec->output_offset, sec->size);

  ptr_size = (get_elf_backend_data (abfd)
	      ->elf_backend_eh_frame_address_size (abfd, sec));
  BFD_ASSERT (ptr_size != 0);

  sec_info = elf_section_data (sec)->sec_info;
  htab = elf_hash_table (info);
  hdr_info = &htab->eh_info;

  if (hdr_info->table && hdr_info->array == NULL)
    hdr_info->array
      = bfd_malloc (hdr_info->fde_count * sizeof(*hdr_info->array));
  if (hdr_info->array == NULL)
    hdr_info = NULL;

  /* The new offsets can be bigger or smaller than the original offsets.
     We therefore need to make two passes over the section: one backward
     pass to move entries up and one forward pass to move entries down.
     The two passes won't interfere with each other because entries are
     not reordered  */
  for (ent = sec_info->entry + sec_info->count; ent-- != sec_info->entry;)
    if (!ent->removed && ent->new_offset > ent->offset)
      memmove (contents + ent->new_offset, contents + ent->offset, ent->size);

  for (ent = sec_info->entry; ent < sec_info->entry + sec_info->count; ++ent)
    if (!ent->removed && ent->new_offset < ent->offset)
      memmove (contents + ent->new_offset, contents + ent->offset, ent->size);

  for (ent = sec_info->entry; ent < sec_info->entry + sec_info->count; ++ent)
    {
      unsigned char *buf, *end;
      unsigned int new_size;

      if (ent->removed)
	continue;

      if (ent->size == 4)
	{
	  /* Any terminating FDE must be at the end of the section.  */
	  BFD_ASSERT (ent == sec_info->entry + sec_info->count - 1);
	  continue;
	}

      buf = contents + ent->new_offset;
      end = buf + ent->size;
      new_size = size_of_output_cie_fde (ent, ptr_size);

      /* Update the size.  It may be shrinked.  */
      bfd_put_32 (abfd, new_size - 4, buf);

      /* Filling the extra bytes with DW_CFA_nops.  */
      if (new_size != ent->size)
	memset (end, 0, new_size - ent->size);

      if (ent->cie)
	{
	  /* CIE */
	  if (ent->make_relative
	      || ent->u.cie.make_lsda_relative
	      || ent->u.cie.per_encoding_relative)
	    {
	      char *aug;
	      unsigned int action, extra_string, extra_data;
	      unsigned int per_width, per_encoding;

	      /* Need to find 'R' or 'L' augmentation's argument and modify
		 DW_EH_PE_* value.  */
	      action = ((ent->make_relative ? 1 : 0)
			| (ent->u.cie.make_lsda_relative ? 2 : 0)
			| (ent->u.cie.per_encoding_relative ? 4 : 0));
	      extra_string = extra_augmentation_string_bytes (ent);
	      extra_data = extra_augmentation_data_bytes (ent);

	      /* Skip length, id and version.  */
	      buf += 9;
	      aug = (char *) buf;
	      buf += strlen (aug) + 1;
	      skip_leb128 (&buf, end);
	      skip_leb128 (&buf, end);
	      skip_leb128 (&buf, end);
	      if (*aug == 'z')
		{
		  /* The uleb128 will always be a single byte for the kind
		     of augmentation strings that we're prepared to handle.  */
		  *buf++ += extra_data;
		  aug++;
		}

	      /* Make room for the new augmentation string and data bytes.  */
	      memmove (buf + extra_string + extra_data, buf, end - buf);
	      memmove (aug + extra_string, aug, buf - (bfd_byte *) aug);
	      buf += extra_string;
	      end += extra_string + extra_data;

	      if (ent->add_augmentation_size)
		{
		  *aug++ = 'z';
		  *buf++ = extra_data - 1;
		}
	      if (ent->u.cie.add_fde_encoding)
		{
		  BFD_ASSERT (action & 1);
		  *aug++ = 'R';
		  *buf++ = DW_EH_PE_pcrel;
		  action &= ~1;
		}

	      while (action)
		switch (*aug++)
		  {
		  case 'L':
		    if (action & 2)
		      {
			BFD_ASSERT (*buf == ent->lsda_encoding);
			*buf |= DW_EH_PE_pcrel;
			action &= ~2;
		      }
		    buf++;
		    break;
		  case 'P':
		    per_encoding = *buf++;
		    per_width = get_DW_EH_PE_width (per_encoding, ptr_size);
		    BFD_ASSERT (per_width != 0);
		    BFD_ASSERT (((per_encoding & 0x70) == DW_EH_PE_pcrel)
				== ent->u.cie.per_encoding_relative);
		    if ((per_encoding & 0xf0) == DW_EH_PE_aligned)
		      buf = (contents
			     + ((buf - contents + per_width - 1)
				& ~((bfd_size_type) per_width - 1)));
		    if (action & 4)
		      {
			bfd_vma val;

			val = read_value (abfd, buf, per_width,
					  get_DW_EH_PE_signed (per_encoding));
			val += (bfd_vma) ent->offset - ent->new_offset;
			val -= extra_string + extra_data;
			write_value (abfd, buf, val, per_width);
			action &= ~4;
		      }
		    buf += per_width;
		    break;
		  case 'R':
		    if (action & 1)
		      {
			BFD_ASSERT (*buf == ent->fde_encoding);
			*buf |= DW_EH_PE_pcrel;
			action &= ~1;
		      }
		    buf++;
		    break;
		  case 'S':
		    break;
		  default:
		    BFD_FAIL ();
		  }
	    }
	}
      else
	{
	  /* FDE */
	  bfd_vma value, address;
	  unsigned int width;
	  bfd_byte *start;
	  struct eh_cie_fde *cie;

	  /* Skip length.  */
	  cie = ent->u.fde.cie_inf;
	  buf += 4;
	  value = ((ent->new_offset + sec->output_offset + 4)
		   - (cie->new_offset + cie->u.cie.u.sec->output_offset));
	  bfd_put_32 (abfd, value, buf);
	  buf += 4;
	  width = get_DW_EH_PE_width (ent->fde_encoding, ptr_size);
	  value = read_value (abfd, buf, width,
			      get_DW_EH_PE_signed (ent->fde_encoding));
	  address = value;
	  if (value)
	    {
	      switch (ent->fde_encoding & 0xf0)
		{
		case DW_EH_PE_indirect:
		case DW_EH_PE_textrel:
		  BFD_ASSERT (hdr_info == NULL);
		  break;
		case DW_EH_PE_datarel:
		  {
		    asection *got = bfd_get_section_by_name (abfd, ".got");

		    BFD_ASSERT (got != NULL);
		    address += got->vma;
		  }
		  break;
		case DW_EH_PE_pcrel:
		  value += (bfd_vma) ent->offset - ent->new_offset;
		  address += (sec->output_section->vma
			      + sec->output_offset
			      + ent->offset + 8);
		  break;
		}
	      if (ent->make_relative)
		value -= (sec->output_section->vma
			  + sec->output_offset
			  + ent->new_offset + 8);
	      write_value (abfd, buf, value, width);
	    }

	  start = buf;

	  if (hdr_info)
	    {
	      hdr_info->array[hdr_info->array_count].initial_loc = address;
	      hdr_info->array[hdr_info->array_count++].fde
		= (sec->output_section->vma
		   + sec->output_offset
		   + ent->new_offset);
	    }

	  if ((ent->lsda_encoding & 0xf0) == DW_EH_PE_pcrel
	      || cie->u.cie.make_lsda_relative)
	    {
	      buf += ent->lsda_offset;
	      width = get_DW_EH_PE_width (ent->lsda_encoding, ptr_size);
	      value = read_value (abfd, buf, width,
				  get_DW_EH_PE_signed (ent->lsda_encoding));
	      if (value)
		{
		  if ((ent->lsda_encoding & 0xf0) == DW_EH_PE_pcrel)
		    value += (bfd_vma) ent->offset - ent->new_offset;
		  else if (cie->u.cie.make_lsda_relative)
		    value -= (sec->output_section->vma
			      + sec->output_offset
			      + ent->new_offset + 8 + ent->lsda_offset);
		  write_value (abfd, buf, value, width);
		}
	    }
	  else if (ent->add_augmentation_size)
	    {
	      /* Skip the PC and length and insert a zero byte for the
		 augmentation size.  */
	      buf += width * 2;
	      memmove (buf + 1, buf, end - buf);
	      *buf = 0;
	    }

	  if (ent->set_loc)
	    {
	      /* Adjust DW_CFA_set_loc.  */
	      unsigned int cnt, width;
	      bfd_vma new_offset;

	      width = get_DW_EH_PE_width (ent->fde_encoding, ptr_size);
	      new_offset = ent->new_offset + 8
			   + extra_augmentation_string_bytes (ent)
			   + extra_augmentation_data_bytes (ent);

	      for (cnt = 1; cnt <= ent->set_loc[0]; cnt++)
		{
		  bfd_vma value;
		  buf = start + ent->set_loc[cnt];

		  value = read_value (abfd, buf, width,
				      get_DW_EH_PE_signed (ent->fde_encoding));
		  if (!value)
		    continue;

		  if ((ent->fde_encoding & 0xf0) == DW_EH_PE_pcrel)
		    value += (bfd_vma) ent->offset + 8 - new_offset;
		  if (ent->make_relative)
		    value -= (sec->output_section->vma
			      + sec->output_offset
			      + new_offset + ent->set_loc[cnt]);
		  write_value (abfd, buf, value, width);
		}
	    }
	}
    }

  /* We don't align the section to its section alignment since the
     runtime library only expects all CIE/FDE records aligned at
     the pointer size. _bfd_elf_discard_section_eh_frame should
     have padded CIE/FDE records to multiple of pointer size with
     size_of_output_cie_fde.  */
  if ((sec->size % ptr_size) != 0)
    abort ();

  return bfd_set_section_contents (abfd, sec->output_section,
				   contents, (file_ptr) sec->output_offset,
				   sec->size);
}

/* Helper function used to sort .eh_frame_hdr search table by increasing
   VMA of FDE initial location.  */

static int
vma_compare (const void *a, const void *b)
{
  const struct eh_frame_array_ent *p = a;
  const struct eh_frame_array_ent *q = b;
  if (p->initial_loc > q->initial_loc)
    return 1;
  if (p->initial_loc < q->initial_loc)
    return -1;
  return 0;
}

/* Write out .eh_frame_hdr section.  This must be called after
   _bfd_elf_write_section_eh_frame has been called on all input
   .eh_frame sections.
   .eh_frame_hdr format:
   ubyte version		(currently 1)
   ubyte eh_frame_ptr_enc  	(DW_EH_PE_* encoding of pointer to start of
				 .eh_frame section)
   ubyte fde_count_enc		(DW_EH_PE_* encoding of total FDE count
				 number (or DW_EH_PE_omit if there is no
				 binary search table computed))
   ubyte table_enc		(DW_EH_PE_* encoding of binary search table,
				 or DW_EH_PE_omit if not present.
				 DW_EH_PE_datarel is using address of
				 .eh_frame_hdr section start as base)
   [encoded] eh_frame_ptr	(pointer to start of .eh_frame section)
   optionally followed by:
   [encoded] fde_count		(total number of FDEs in .eh_frame section)
   fde_count x [encoded] initial_loc, fde
				(array of encoded pairs containing
				 FDE initial_location field and FDE address,
				 sorted by increasing initial_loc).  */

bfd_boolean
_bfd_elf_write_section_eh_frame_hdr (bfd *abfd, struct bfd_link_info *info)
{
  struct elf_link_hash_table *htab;
  struct eh_frame_hdr_info *hdr_info;
  asection *sec;
  bfd_byte *contents;
  asection *eh_frame_sec;
  bfd_size_type size;
  bfd_boolean retval;
  bfd_vma encoded_eh_frame;

  htab = elf_hash_table (info);
  hdr_info = &htab->eh_info;
  sec = hdr_info->hdr_sec;
  if (sec == NULL)
    return TRUE;

  size = EH_FRAME_HDR_SIZE;
  if (hdr_info->array && hdr_info->array_count == hdr_info->fde_count)
    size += 4 + hdr_info->fde_count * 8;
  contents = bfd_malloc (size);
  if (contents == NULL)
    return FALSE;

  eh_frame_sec = bfd_get_section_by_name (abfd, ".eh_frame");
  if (eh_frame_sec == NULL)
    {
      free (contents);
      return FALSE;
    }

  memset (contents, 0, EH_FRAME_HDR_SIZE);
  contents[0] = 1;				/* Version.  */
  contents[1] = get_elf_backend_data (abfd)->elf_backend_encode_eh_address
    (abfd, info, eh_frame_sec, 0, sec, 4,
     &encoded_eh_frame);			/* .eh_frame offset.  */

  if (hdr_info->array && hdr_info->array_count == hdr_info->fde_count)
    {
      contents[2] = DW_EH_PE_udata4;		/* FDE count encoding.  */
      contents[3] = DW_EH_PE_datarel | DW_EH_PE_sdata4; /* Search table enc.  */
    }
  else
    {
      contents[2] = DW_EH_PE_omit;
      contents[3] = DW_EH_PE_omit;
    }
  bfd_put_32 (abfd, encoded_eh_frame, contents + 4);

  if (contents[2] != DW_EH_PE_omit)
    {
      unsigned int i;

      bfd_put_32 (abfd, hdr_info->fde_count, contents + EH_FRAME_HDR_SIZE);
      qsort (hdr_info->array, hdr_info->fde_count, sizeof (*hdr_info->array),
	     vma_compare);
      for (i = 0; i < hdr_info->fde_count; i++)
	{
	  bfd_put_32 (abfd,
		      hdr_info->array[i].initial_loc
		      - sec->output_section->vma,
		      contents + EH_FRAME_HDR_SIZE + i * 8 + 4);
	  bfd_put_32 (abfd,
		      hdr_info->array[i].fde - sec->output_section->vma,
		      contents + EH_FRAME_HDR_SIZE + i * 8 + 8);
	}
    }

  retval = bfd_set_section_contents (abfd, sec->output_section,
				     contents, (file_ptr) sec->output_offset,
				     sec->size);
  free (contents);
  return retval;
}

/* Return the width of FDE addresses.  This is the default implementation.  */

unsigned int
_bfd_elf_eh_frame_address_size (bfd *abfd, asection *sec ATTRIBUTE_UNUSED)
{
  return elf_elfheader (abfd)->e_ident[EI_CLASS] == ELFCLASS64 ? 8 : 4;
}

/* Decide whether we can use a PC-relative encoding within the given
   EH frame section.  This is the default implementation.  */

bfd_boolean
_bfd_elf_can_make_relative (bfd *input_bfd ATTRIBUTE_UNUSED,
			    struct bfd_link_info *info ATTRIBUTE_UNUSED,
			    asection *eh_frame_section ATTRIBUTE_UNUSED)
{
  return TRUE;
}

/* Select an encoding for the given address.  Preference is given to
   PC-relative addressing modes.  */

bfd_byte
_bfd_elf_encode_eh_address (bfd *abfd ATTRIBUTE_UNUSED,
			    struct bfd_link_info *info ATTRIBUTE_UNUSED,
			    asection *osec, bfd_vma offset,
			    asection *loc_sec, bfd_vma loc_offset,
			    bfd_vma *encoded)
{
  *encoded = osec->vma + offset -
    (loc_sec->output_section->vma + loc_sec->output_offset + loc_offset);
  return DW_EH_PE_pcrel | DW_EH_PE_sdata4;
}
