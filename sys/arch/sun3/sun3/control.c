/*
 * Copyright (c) 1993 Adam Glass
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
 *	This product includes software developed by Adam Glass.
 * 4. The name of the Author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY Adam Glass ``AS IS'' AND
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
 * $Header: /cvsroot/src/sys/arch/sun3/sun3/control.c,v 1.6 1994/03/01 08:23:04 glass Exp $
 */
#include <sys/systm.h>
#include <sys/types.h>

#include <machine/pte.h>
#include <machine/control.h>

#define CONTROL_ALIGN(x) (x & CONTROL_ADDR_MASK)
#define CONTROL_ADDR_BUILD(space, va) (CONTROL_ALIGN(va)|space)

static vm_offset_t temp_seg_va = NULL;

int get_context()
{
    int c;
    
    c = get_control_byte((char *) CONTEXT_REG);
    return c & CONTEXT_MASK;
}

void set_context(int c)
{
    set_control_byte((char *) CONTEXT_REG, c & CONTEXT_MASK);
}

vm_offset_t get_pte(va)
     vm_offset_t va;
{
    return (vm_offset_t)
	get_control_word((char *) CONTROL_ADDR_BUILD(PGMAP_BASE, va));
}
void set_pte(va, pte)
     vm_offset_t va, pte;
{
    set_control_word((char *) CONTROL_ADDR_BUILD(PGMAP_BASE, va),
		     (unsigned int) pte);
}

unsigned char get_segmap(va)
     vm_offset_t va;
{
    return get_control_byte((char *) CONTROL_ADDR_BUILD(SEGMAP_BASE, va));
}
void set_segmap(va, sme)
     vm_offset_t va;
     unsigned char sme;
{
    set_control_byte((char *) CONTROL_ADDR_BUILD(SEGMAP_BASE, va), sme);
}

void set_temp_seg_addr(va)
     vm_offset_t va;
{
    if (va)
	temp_seg_va = va;
}

vm_offset_t get_pte_pmeg(pmeg_num, page_num)
     unsigned char pmeg_num;
     unsigned int page_num;
{
    vm_offset_t pte, va;

    set_segmap(temp_seg_va, pmeg_num);
    va += NBPG*page_num;
    pte = get_pte(va);
    set_segmap(temp_seg_va, SEGINV);
    return pte;
}

void set_pte_pmeg(pmeg_num, page_num,pte)
     unsigned char pmeg_num;
     unsigned int page_num;
     vm_offset_t pte;
{
    vm_offset_t va;

    va = temp_seg_va;
    set_segmap(temp_seg_va, pmeg_num);
    va += NBPG*page_num;
    set_pte(va, pte);
    set_segmap(temp_seg_va, SEGINV);
}

