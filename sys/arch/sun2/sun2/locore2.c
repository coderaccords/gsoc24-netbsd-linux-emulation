/*	$NetBSD: locore2.c,v 1.5 2001/06/15 00:32:38 fredette Exp $	*/

/*-
 * Copyright (c) 1996 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Adam Glass, Gordon W. Ross, and Matthew Fredette.
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
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include "opt_ddb.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/reboot.h>
#include <sys/user.h>
#define ELFSIZE 32
#include <sys/exec_elf.h>

#include <uvm/uvm_extern.h>

#include <machine/cpu.h>
#include <machine/db_machdep.h>
#include <machine/dvma.h>
#include <machine/idprom.h>
#include <machine/leds.h>
#include <machine/promlib.h>
#include <machine/pmap.h>
#include <machine/pte.h>

#include <machine/stdarg.h>

#include <sun2/sun2/control.h>
#include <sun2/sun2/machdep.h>
#include <sun68k/sun68k/vector.h>

/* This is defined in locore.s */
extern char kernel_text[];

/* These are defined by the linker */
extern char etext[], edata[], end[];
int nsym;
char *ssym, *esym;

/*
 * XXX: m68k common code needs these...
 * ... but this port does not need to deal with anything except
 * an mc68010, so these two variables are always ignored.
 */
int cputype = CPU_68010;
int mmutype = MMU_SUN;

/*
 * Now our own stuff.
 */

u_char cpu_machine_id = 0;
char *cpu_string = NULL;
int cpu_has_multibus = 0;
int cpu_has_vme = 0;

/*
 * XXX - Should empirically estimate the divisor...
 * Note that the value of delay_divisor is roughly
 * 2048 / cpuclock	(where cpuclock is in MHz).
 */
int delay_divisor = 82;		/* assume the fastest (3/260) */

extern int physmem;

struct user *proc0paddr;	/* proc[0] pcb address (u-area VA) */
extern struct pcb *curpcb;

/* First C code called by locore.s */
void _bootstrap __P((void));

static void _verify_hardware __P((void));
static void _vm_init __P((void));

#if defined(DDB) && !defined(SYMTAB_SPACE)
static void _save_symtab __P((void));

/*
 * Preserve DDB symbols and strings by setting esym.
 */
static void
_save_symtab()
{
	int i;
	Elf_Ehdr *ehdr;
	Elf_Shdr *shp;
	vaddr_t minsym, maxsym;
 
	/*
	 * Check the ELF headers.
	 */
 
	ehdr = (void *)end;
	if (memcmp(ehdr->e_ident, ELFMAG, SELFMAG) != 0 ||
	    ehdr->e_ident[EI_CLASS] != ELFCLASS32) {
		prom_printf("_save_symtab: bad ELF magic\n");
		return;
	}

	/*
	 * Find the end of the symbols and strings.
	 */

	maxsym = 0;
	minsym = ~maxsym;
	shp = (Elf_Shdr *)(end + ehdr->e_shoff);
	for (i = 0; i < ehdr->e_shnum; i++) {
		if (shp[i].sh_type != SHT_SYMTAB &&
		    shp[i].sh_type != SHT_STRTAB) {
			continue;
		}
		minsym = min(minsym, (vaddr_t)end + shp[i].sh_offset);
		maxsym = max(maxsym, (vaddr_t)end + shp[i].sh_offset +
			     shp[i].sh_size);
	}
 
	nsym = 1;
	ssym = (char *)ehdr;
	esym = (char *)maxsym;
}
#endif	/* DDB && !SYMTAB_SPACE */

/*
 * This function is called from _bootstrap() to initialize
 * pre-vm-sytem virtual memory.  All this really does is to
 * set virtual_avail to the first page following preloaded
 * data (i.e. the kernel and its symbol table) and special
 * things that may be needed very early (proc0 upages).
 * Once that is done, pmap_bootstrap() is called to do the
 * usual preparations for our use of the MMU.
 */
static void
_vm_init()
{
	vm_offset_t nextva;

	/*
	 * First preserve our symbol table, which might have been
	 * loaded after our BSS area by the boot loader.  However,
	 * if DDB is not part of this kernel, ignore the symbols.
	 */
	esym = end + 4;
#if defined(DDB) && !defined(SYMTAB_SPACE)
	/* This will advance esym past the symbols. */
	_save_symtab();
#endif

	/*
	 * Steal some special-purpose, already mapped pages.
	 * Note: msgbuf is setup in machdep.c:cpu_startup()
	 */
	nextva = m68k_round_page(esym);

	/*
	 * Setup the u-area pages (stack, etc.) for proc0.
	 * This is done very early (here) to make sure the
	 * fault handler works in case we hit an early bug.
	 * (The fault handler may reference proc0 stuff.)
	 */
	proc0paddr = (struct user *) nextva;
	nextva += USPACE;
	bzero((caddr_t)proc0paddr, USPACE);
	proc0.p_addr = proc0paddr;

	/*
	 * Now that proc0 exists, make it the "current" one.
	 */
	curproc = &proc0;
	curpcb = &proc0paddr->u_pcb;

	/* This does most of the real work. */
	pmap_bootstrap(nextva);
}

/*
 * Determine which Sun2 model we are running on.
 *
 * XXX: Just save idprom.idp_machtype here, and
 * XXX: move the rest of this to identifycpu().
 * XXX: Move cache_size stuff to cache.c.
 */
static void
_verify_hardware()
{
	unsigned char machtype;
	int cpu_match = 0;

	machtype = identity_prom.idp_machtype;
	if ((machtype & IDM_ARCH_MASK) != IDM_ARCH_SUN2) {
		prom_printf("Bad IDPROM arch!\n");
		prom_abort();
	}

	cpu_machine_id = machtype;
	switch (cpu_machine_id) {

	case SUN2_MACH_120 :
		cpu_match++;
		cpu_string = "{120,170}";
		delay_divisor = 205;	/* 10 MHz */
		cpu_has_multibus = TRUE;
		break;

	case SUN2_MACH_50 :
		cpu_match++;
		cpu_string = "50";
		delay_divisor = 205;	/* 10 MHz */
		cpu_has_vme = TRUE;
		break;

	default:
		prom_printf("unknown sun2 model\n");
		prom_abort();
	}
	if (!cpu_match) {
		prom_printf("kernel not configured for the Sun 2 model\n");
		prom_abort();
	}
}

/*
 * This is called from locore.s just after the kernel is remapped
 * to its proper address, but before the call to main().  The work
 * done here corresponds to various things done in locore.s on the
 * hp300 port (and other m68k) but which we prefer to do in C code.
 * Also do setup specific to the Sun PROM monitor and IDPROM here.
 */
void
_bootstrap()
{
	vm_offset_t va;

	/* First, Clear BSS. */
	bzero(edata, end - edata);

	/* Initialize the PROM. */
	prom_init();

	/* Copy the IDPROM from control space. */
	idprom_init();

	/* Validate the Sun2 model (from IDPROM). */
	_verify_hardware();

	/* Handle kernel mapping, pmap_bootstrap(), etc. */
	_vm_init();

	/*
	 * Point interrupts/exceptions to our vector table.
	 * (Until now, we use the one setup by the PROM.)
	 */
	setvbr((void **)vector_table);
	/* Interrupts are enabled later, after autoconfig. */

	/*
 	* Now unmap the PROM's physical/virtual pages zero through three.
 	*/
	for(va = 0; va < NBPG * 4; va += NBPG)
		set_pte(va, PG_INVAL);

	/*
	 * Turn on the LEDs so we know power is on.
	 * Needs idprom_init and obio_init earlier.
	 */
	leds_init();
}
