/*
 * Copyright (c) 1993 Paul Kranenburg
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Paul Kranenburg.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software withough specific prior written permission
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
 *	$Id: md.c,v 1.1 1993/11/25 00:57:37 paulus Exp $
 */

#include <sys/param.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <fcntl.h>
#include <a.out.h>
#include <stab.h>
#include <string.h>

#include "ld.h"

/*
 * Get relocation addend corresponding to relocation record RP
 * from address ADDR
 */
long
md_get_addend(rp, addr)
struct relocation_info	*rp;
unsigned char		*addr;
{
	switch (RELOC_TARGET_SIZE(rp)) {
	case 0:
		return get_byte(addr);
		break;
	case 1:
		return get_short(addr);
		break;
	case 2:
		return get_long(addr);
		break;
	}
}

/*
 * Put RELOCATION at ADDR according to relocation record RP.
 */
void
md_relocate(rp, relocation, addr, relocatable_output)
struct relocation_info	*rp;
long			relocation;
unsigned char		*addr;
{
	switch (RELOC_TARGET_SIZE(rp)) {
	case 0:
		put_byte(addr, relocation);
		break;
	case 1:
		put_short(addr, relocation);
		break;
	case 2:
		put_long(addr, relocation);
		break;
	default:
		fatal("Unsupported relocation size: %x", RELOC_TARGET_SIZE(rp));
	}
}

/*
 * Initialize (output) exec header such that useful values are
 * obtained from subsequent N_*() macro evaluations.
 */
void
md_init_header(hp, magic, flags)
struct exec	*hp;
int		magic, flags;
{
	if (oldmagic)
		hp->a_midmag = oldmagic;
	else
		N_SETMAGIC((*hp), magic, MID_MACHINE, flags);

	/* TEXT_START depends on the value of outheader.a_entry.  */
	if (!(link_mode & SHAREABLE)) /*WAS: if (entry_symbol) */
		hp->a_entry = PAGSIZ;
}

/*
 * Machine dependent part of claim_rrs_reloc().
 * Set RRS relocation type.
 */
int
md_make_reloc(rp, r, type)
struct relocation_info	*rp, *r;
int			type;
{
	/* Relocation size */
	r->r_length = rp->r_length;

	if (RELOC_PCREL_P(rp))
		r->r_pcrel = 1;

	if (type & RELTYPE_RELATIVE)
		r->r_relative = 1;

	if (type & RELTYPE_COPY)
		r->r_copy = 1;

	return 0;
}

/*
 * Set up a transfer from jmpslot at OFFSET (relative to the PLT table)
 * to the binder slot (which is at offset 0 of the PLT).
 */
void
md_make_jmpslot(sp, offset, index)
jmpslot_t	*sp;
long		offset;
long		index;
{
	/*
	 * On m68k machines, a long branch offset is relative to
	 * the address of the offset.
	 */
	u_long	fudge = - (sizeof(sp->opcode) + offset);

	sp->opcode = BSRL;
	sp->addr[0] = fudge >> 16;
	sp->addr[1] = fudge;
	sp->reloc_index = index;
}

/*
 * Set up a "direct" transfer (ie. not through the run-time binder) from
 * jmpslot at OFFSET to ADDR. Used by `ld' when the SYMBOLIC flag is on,
 * and by `ld.so' after resolving the symbol.
 * On the m68k, we use the BRA instruction which is PC relative, so no
 * further RRS relocations will be necessary for such a jmpslot.
 */
void
md_fix_jmpslot(sp, offset, addr)
jmpslot_t	*sp;
long		offset;
u_long		addr;
{
	u_long	fudge = addr - (sizeof(sp->opcode) + offset);

	sp->opcode = BRAL;
	sp->addr[0] = fudge >> 16;
	sp->addr[1] = fudge;
	sp->reloc_index = 0;
}

/*
 * Update the relocation record for a RRS jmpslot.
 */
void
md_make_jmpreloc(rp, r, type)
struct relocation_info	*rp, *r;
int			type;
{
	jmpslot_t	*sp;

	/*
	 * Fix relocation address to point to the correct
	 * location within this jmpslot.
	 */
	r->r_address += sizeof(sp->opcode);

	/* Relocation size */
	r->r_length = 2;

	/* Set relocation type */
	r->r_jmptable = 1;
	if (type & RELTYPE_RELATIVE)
		r->r_relative = 1;

}

/*
 * Set relocation type for a RRS GOT relocation.
 */
void
md_make_gotreloc(rp, r, type)
struct relocation_info	*rp, *r;
int			type;
{
	r->r_baserel = 1;
	if (type & RELTYPE_RELATIVE)
		r->r_relative = 1;

	/* Relocation size */
	r->r_length = 2;
}

/*
 * Set relocation type for a RRS copy operation.
 */
void
md_make_cpyreloc(rp, r)
struct relocation_info	*rp, *r;
{
	/* Relocation size */
	r->r_length = 2;

	r->r_copy = 1;
}

void
md_set_breakpoint(where, savep)
long	where;
long	*savep;
{
	*savep = *(long *)where;
	*(short *)where = BPT;
}

#ifdef NEED_SWAP

/*
 * Byte swap routines for cross-linking.
 */

void
md_swapin_exec_hdr(h)
struct exec *h;
{
	int skip = 0;

	if (!N_BADMAG(*h))
		skip = 1;

	swap_longs((long *)h + skip, sizeof(*h)/sizeof(long) - skip);
}

void
md_swapout_exec_hdr(h)
struct exec *h;
{
	/* NetBSD: Always leave magic alone */
	int skip = 1;
#if 0
	if (N_GETMAGIC(*h) == OMAGIC)
		skip = 0;
#endif

	swap_longs((long *)h + skip, sizeof(*h)/sizeof(long) - skip);
}


void
md_swapin_reloc(r, n)
struct relocation_info *r;
int n;
{
	int	bits;

	for (; n; n--, r++) {
		r->r_address = md_swap_long(r->r_address);
		bits = ((int *)r)[1];
		r->r_symbolnum = md_swap_long(bits) & 0xffffff;
		bits = ((unsigned char *)r)[7];
		r->r_pcrel = ((bits >> 7) & 1);
		r->r_length = ((bits >> 5) & 3);
		r->r_extern = ((bits >> 4) & 1);
		r->r_baserel = ((bits >> 3) & 1);
		r->r_jmptable = ((bits >> 2) & 1);
		r->r_relative = ((bits >> 1) & 1);
#ifdef N_SIZE
		r->r_copy = (bits & 1);
#endif
	}
}

void
md_swapout_reloc(r, n)
struct relocation_info *r;
int n;
{
	int	bits;

	for (; n; n--, r++) {
		r->r_address = md_swap_long(r->r_address);
		((int *)r)[1] = md_swap_long(r->r_symbolnum & 0xffffff00);
		bits = ((r->r_pcrel << 7) & 0x80);
		bits |= ((r->r_length << 5) & 0x60);
		bits |= ((r->r_extern << 4) & 0x10);
		bits |= ((r->r_baserel << 3) & 8);
		bits |= ((r->r_jmptable << 2) & 4);
		bits |= ((r->r_relative << 1) & 2);
#ifdef N_SIZE
		bits |= (r->r_copy & 1);
#endif
		((unsigned char *)r)[7] = bits;
	}
}

void
md_swapout_jmpslot(j, n)
jmpslot_t	*j;
int		n;
{
	for (; n; n--, j++) {
		j->opcode = md_swap_short(j->opcode);
		j->addr[0] = md_swap_short(j->addr[0]);
		j->addr[1] = md_swap_short(j->addr[1]);
		j->reloc_index = md_swap_short(j->reloc_index);
	}
}

#endif /* NEED_SWAP */
