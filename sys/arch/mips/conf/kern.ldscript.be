/* $NetBSD: kern.ldscript.be,v 1.3 2001/06/01 03:55:30 thorpej Exp $ */

/*  ldscript for NetBSD/mipseb kernels */
OUTPUT_FORMAT("elf32-bigmips", "elf32-bigmips",
	      "elf32-littlemips")
OUTPUT_ARCH(mips)
ENTRY(_start)
SEARCH_DIR(/lib);
/* Do we need any of these?
   __DYNAMIC = 0;    */
_DYNAMIC_LINK = 0;
SECTIONS
{
  /*  Read-only sections, merged into text segment.  Assumes the
      kernel Makefile sets the start address via -Ttext.  */
  .text      :
  {
    _ftext = . ;
    *(.text)
    *(.gnu.warning)
  } =0
  _etext = .;
  PROVIDE (etext = .);
  .rodata    : { *(.rodata)  }
  .reginfo : { *(.reginfo) }
/*  . = . + 0x1000; */
  .data    :
  {
    _fdata = . ;
    *(.data)
    CONSTRUCTORS
  }
  _gp = ALIGN(16) + 0x7ff0;
  .lit8 : { *(.lit8) }
  .lit4 : { *(.lit4) }
  .sdata     : { *(.sdata) }
  _edata  =  .;
  PROVIDE (edata = .);
  __bss_start = .;
  _fbss = .;
  .sbss      : { *(.sbss) *(.scommon) }
  .bss       :
  {
   *(.bss)
   *(COMMON)
  }
  _end = . ;
  PROVIDE (end = .);
  /* These are needed for ELF backends which have not yet been
     converted to the new style linker.  */
  .stab 0 : { *(.stab) }
  .stabstr 0 : { *(.stabstr) }
  /* DWARF debug sections.
     Symbols in the .debug DWARF section are relative to the beginning of the
     section so we begin .debug at 0.  It's not clear yet what needs to happen
     for the others.   */
  .debug          0 : { *(.debug) }
  .debug_srcinfo  0 : { *(.debug_srcinfo) }
  .debug_aranges  0 : { *(.debug_aranges) }
  .debug_pubnames 0 : { *(.debug_pubnames) }
  .debug_sfnames  0 : { *(.debug_sfnames) }
  .line           0 : { *(.line) }
  /* These must appear regardless of  .  */
  .gptab.sdata : { *(.gptab.data) *(.gptab.sdata) }
  .gptab.sbss : { *(.gptab.bss) *(.gptab.sbss) }
}
