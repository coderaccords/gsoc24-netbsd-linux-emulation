/* $NetBSD: elf2ecoff.c,v 1.3 1996/10/16 00:27:08 jonathan Exp $ */

/*
 * Copyright (c) 1995
 *	Ted Lemon (hereinafter referred to as the author)
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/* elf2ecoff.c

   This program converts an elf executable to an ECOFF executable.
   No symbol table is retained.   This is useful primarily in building
   net-bootable kernels for machines (e.g., DECstation and Alpha) which
   only support the ECOFF object file format. */

#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/exec_elf.h>
#include <sys/exec_aout.h>
#include <stdio.h>
#include <sys/exec_ecoff.h>
#include <sys/errno.h>
#include <string.h>
#include <limits.h>


/* Elf Program segment permissions, in program header flags field */

#define PF_X            (1 << 0)        /* Segment is executable */
#define PF_W            (1 << 1)        /* Segment is writable */
#define PF_R            (1 << 2)        /* Segment is readable */
#define PF_MASKPROC     0xF0000000      /* Processor-specific reserved bits */

struct sect {
  unsigned long vaddr;
  unsigned long len;
};

int phcmp ();
char *saveRead (int file, off_t offset, off_t len, char *name);
int copy (int, int, off_t, off_t);
int translate_syms (int, int, off_t, off_t, off_t, off_t);
extern int errno;
int *symTypeTable;

main (int argc, char **argv, char **envp)
{
  Elf32_Ehdr ex;
  Elf32_Phdr *ph;
  Elf32_Shdr *sh;
  Elf32_Sym *symtab;
  char *shstrtab;
  int strtabix, symtabix;
  int i, pad;
  struct sect text, data, bss;
  struct ecoff_exechdr ep;
  struct ecoff_scnhdr esecs [3];
  int infile, outfile;
  unsigned long cur_vma = ULONG_MAX;
  int symflag = 0;

  text.len = data.len = bss.len = 0;
  text.vaddr = data.vaddr = bss.vaddr = 0;

  /* Check args... */
  if (argc < 3 || argc > 4)
    {
    usage:
      fprintf (stderr,
	       "usage: elf2aout <elf executable> <a.out executable> [-s]\n");
      exit (1);
    }
  if (argc == 4)
    {
      if (strcmp (argv [3], "-s"))
	goto usage;
      symflag = 1;
    }

  /* Try the input file... */
  if ((infile = open (argv [1], O_RDONLY)) < 0)
    {
      fprintf (stderr, "Can't open %s for read: %s\n",
	       argv [1], strerror (errno));
      exit (1);
    }

  /* Read the header, which is at the beginning of the file... */
  i = read (infile, &ex, sizeof ex);
  if (i != sizeof ex)
    {
      fprintf (stderr, "ex: %s: %s.\n",
	       argv [1], i ? strerror (errno) : "End of file reached");
      exit (1);
    }

  /* Read the program headers... */
  ph = (Elf32_Phdr *)saveRead (infile, ex.e_phoff,
				ex.e_phnum * sizeof (Elf32_Phdr), "ph");
  /* Read the section headers... */
  sh = (Elf32_Shdr *)saveRead (infile, ex.e_shoff,
				ex.e_shnum * sizeof (Elf32_Shdr), "sh");
  /* Read in the section string table. */
  shstrtab = saveRead (infile, sh [ex.e_shstrndx].sh_offset,
		       sh [ex.e_shstrndx].sh_size, "shstrtab");

  /* Figure out if we can cram the program header into an ECOFF
     header...  Basically, we can't handle anything but loadable
     segments, but we can ignore some kinds of segments.  We can't
     handle holes in the address space.  Segments may be out of order,
     so we sort them first. */

  qsort (ph, ex.e_phnum, sizeof (Elf32_Phdr), phcmp);

  for (i = 0; i < ex.e_phnum; i++)
    {
      /* Section types we can ignore... */
      if (ph [i].p_type == Elf_pt_null || ph [i].p_type == Elf_pt_note ||
	  ph [i].p_type == Elf_pt_phdr || ph [i].p_type == Elf_pt_mips_reginfo)
	continue;
      /* Section types we can't handle... */
      else if (ph [i].p_type != Elf_pt_load)
        {
	  fprintf (stderr, "Program header %d type %d can't be converted.\n");
	  exit (1);
	}
      /* Writable (data) segment? */
      if (ph [i].p_flags & PF_W)
	{
	  struct sect ndata, nbss;

	  ndata.vaddr = ph [i].p_vaddr;
	  ndata.len = ph [i].p_filesz;
	  nbss.vaddr = ph [i].p_vaddr + ph [i].p_filesz;
	  nbss.len = ph [i].p_memsz - ph [i].p_filesz;

	  combine (&data, &ndata, 0);
	  combine (&bss, &nbss, 1);
	}
      else
	{
	  struct sect ntxt;

	  ntxt.vaddr = ph [i].p_vaddr;
	  ntxt.len = ph [i].p_filesz;

	  combine (&text, &ntxt);
	}
      /* Remember the lowest segment start address. */
      if (ph [i].p_vaddr < cur_vma)
	cur_vma = ph [i].p_vaddr;
    }

  /* Sections must be in order to be converted... */
  if (text.vaddr > data.vaddr || data.vaddr > bss.vaddr ||
      text.vaddr + text.len > data.vaddr || data.vaddr + data.len > bss.vaddr)
    {
      fprintf (stderr, "Sections ordering prevents a.out conversion.\n");
      exit (1);
    }

  /* If there's a data section but no text section, then the loader
     combined everything into one section.   That needs to be the
     text section, so just make the data section zero length following
     text. */
  if (data.len && !text.len)
    {
      text = data;
      data.vaddr = text.vaddr + text.len;
      data.len = 0;
    }

  /* If there is a gap between text and data, we'll fill it when we copy
     the data, so update the length of the text segment as represented in
     a.out to reflect that, since a.out doesn't allow gaps in the program
     address space. */
  if (text.vaddr + text.len < data.vaddr)
    text.len = data.vaddr - text.vaddr;

  /* We now have enough information to cons up an a.out header... */
  ep.a.magic = ECOFF_OMAGIC;
  ep.a.vstamp = 200;
  ep.a.tsize = text.len;
  ep.a.dsize = data.len;
  ep.a.bsize = bss.len;
  ep.a.entry = ex.e_entry;
  ep.a.text_start = text.vaddr;
  ep.a.data_start = data.vaddr;
  ep.a.bss_start = bss.vaddr;
  ep.a.gprmask = 0xf3fffffe;
  bzero (&ep.a.cprmask, sizeof ep.a.cprmask);
  ep.a.gp_value = 0; /* unused. */

  ep.f.f_magic = ECOFF_MAGIC_MIPSEL;
  ep.f.f_nscns = 3;
  ep.f.f_timdat = 0;	/* bogus */
  ep.f.f_symptr = 0;
  ep.f.f_nsyms = 0;
  ep.f.f_opthdr = sizeof ep.a;
  ep.f.f_flags = 0x100f; /* Stripped, not sharable. */

  strcpy (esecs [0].s_name, ".text");
  strcpy (esecs [1].s_name, ".data");
  strcpy (esecs [2].s_name, ".bss");
  esecs [0].s_paddr = esecs [0].s_vaddr = ep.a.text_start;
  esecs [1].s_paddr = esecs [1].s_vaddr = ep.a.data_start;
  esecs [2].s_paddr = esecs [2].s_vaddr = ep.a.bss_start;
  esecs [0].s_size = ep.a.tsize;
  esecs [1].s_size = ep.a.dsize;
  esecs [2].s_size = ep.a.bsize;

  esecs [0].s_scnptr = ECOFF_TXTOFF (&ep);
  esecs [1].s_scnptr = ECOFF_DATOFF (&ep);
  esecs [2].s_scnptr = esecs [1].s_scnptr +
	  ECOFF_ROUND (esecs [1].s_size, ECOFF_SEGMENT_ALIGNMENT (&ep));
  esecs [0].s_relptr = esecs [1].s_relptr
	  = esecs [2].s_relptr = 0;
  esecs [0].s_lnnoptr = esecs [1].s_lnnoptr
	  = esecs [2].s_lnnoptr = 0;
  esecs [0].s_nreloc = esecs [1].s_nreloc = esecs [2].s_nreloc = 0;
  esecs [0].s_nlnno = esecs [1].s_nlnno = esecs [2].s_nlnno = 0;
  esecs [0].s_flags = 0x20;
  esecs [1].s_flags = 0x40;
  esecs [2].s_flags = 0x82;

  /* Make the output file... */
  if ((outfile = open (argv [2], O_WRONLY | O_CREAT, 0777)) < 0)
    {
      fprintf (stderr, "Unable to create %s: %s\n", argv [2], strerror (errno));
      exit (1);
    }

  /* Write the headers... */
  i = write (outfile, &ep.f, sizeof ep.f);
  if (i != sizeof ep.f)
    {
      perror ("ep.f: write");
      exit (1);

  for (i = 0; i < 6; i++)
    {
      printf ("Section %d: %s phys %x  size %x  file offset %x\n",
	      i, esecs [i].s_name, esecs [i].s_paddr,
	      esecs [i].s_size, esecs [i].s_scnptr);
    }
    }
  fprintf (stderr, "wrote %d byte file header.\n", i);

  i = write (outfile, &ep.a, sizeof ep.a);
  if (i != sizeof ep.a)
    {
      perror ("ep.a: write");
      exit (1);
    }
  fprintf (stderr, "wrote %d byte a.out header.\n", i);

  i = write (outfile, &esecs, sizeof esecs);
  if (i != sizeof esecs)
    {
      perror ("esecs: write");
      exit (1);
    }
  fprintf (stderr, "wrote %d bytes of section headers.\n", i);

  if (pad = ((sizeof ep.f + sizeof ep.a + sizeof esecs) & 15))
    {
      pad = 16 - pad;
      i = write (outfile, "\0\0\0\0\0\0\0\0\0\0\0\0\0\0", pad);
      if (i < 0)
	{
	  perror ("ipad: write");
	  exit (1);
	}
      fprintf (stderr, "wrote %d byte pad.\n", i);
    }

  /* Copy the loadable sections.   Zero-fill any gaps less than 64k;
     complain about any zero-filling, and die if we're asked to zero-fill
     more than 64k. */
  for (i = 0; i < ex.e_phnum; i++)
    {
      /* Unprocessable sections were handled above, so just verify that
	 the section can be loaded before copying. */
      if (ph [i].p_type == Elf_pt_load && ph [i].p_filesz)
	{
	  if (cur_vma != ph [i].p_vaddr)
	    {
	      unsigned long gap = ph [i].p_vaddr - cur_vma;
	      char obuf [1024];
	      if (gap > 65536)
		{
		  fprintf (stderr, "Intersegment gap (%d bytes) too large.\n",
			   gap);
		  exit (1);
		}
	      fprintf (stderr, "Warning: %d byte intersegment gap.\n", gap);
	      memset (obuf, 0, sizeof obuf);
	      while (gap)
		{
		  int count = write (outfile, obuf, (gap > sizeof obuf
						     ? sizeof obuf : gap));
		  if (count < 0)
		    {
		      fprintf (stderr, "Error writing gap: %s\n",
			       strerror (errno));
		      exit (1);
		    }
		  gap -= count;
		}
	    }
fprintf (stderr, "writing %d bytes...\n", ph [i].p_filesz);
	  copy (outfile, infile, ph [i].p_offset, ph [i].p_filesz);
	  cur_vma = ph [i].p_vaddr + ph [i].p_filesz;
	}
    }

  /* Looks like we won... */
  exit (0);
}

copy (out, in, offset, size)
     int out, in;
     off_t offset, size;
{
  char ibuf [4096];
  int remaining, cur, count;

  /* Go the the start of the ELF symbol table... */
  if (lseek (in, offset, SEEK_SET) < 0)
    {
      perror ("copy: lseek");
      exit (1);
    }

  remaining = size;
  while (remaining)
    {
      cur = remaining;
      if (cur > sizeof ibuf)
	cur = sizeof ibuf;
      remaining -= cur;
      if ((count = read (in, ibuf, cur)) != cur)
	{
	  fprintf (stderr, "copy: read: %s\n",
		   count ? strerror (errno) : "premature end of file");
	  exit (1);
	}
      if ((count = write (out, ibuf, cur)) != cur)
	{
	  perror ("copy: write");
	  exit (1);
	}
    }
}

/* Combine two segments, which must be contiguous.   If pad is true, it's
   okay for there to be padding between. */
combine (base, new, pad)
     struct sect *base, *new;
     int pad;
{
  if (!base -> len)
    *base = *new;
  else if (new -> len)
    {
      if (base -> vaddr + base -> len != new -> vaddr)
	{
	  if (pad)
	    base -> len = new -> vaddr - base -> vaddr;
	  else
	    {
	      fprintf (stderr,
		       "Non-contiguous data can't be converted.\n");
	      exit (1);
	    }
	}
      base -> len += new -> len;
    }
}

int
phcmp (h1, h2)
     Elf32_Phdr *h1, *h2;
{
  if (h1 -> p_vaddr > h2 -> p_vaddr)
    return 1;
  else if (h1 -> p_vaddr < h2 -> p_vaddr)
    return -1;
  else
    return 0;
}

char *saveRead (int file, off_t offset, off_t len, char *name)
{
  char *tmp;
  int count;
  off_t off;
  if ((off = lseek (file, offset, SEEK_SET)) < 0)
    {
      fprintf (stderr, "%s: fseek: %s\n", name, strerror (errno));
      exit (1);
    }
  if (!(tmp = (char *)malloc (len)))
    {
      fprintf (stderr, "%s: Can't allocate %d bytes.\n", name, len);
      exit (1);
    }
  count = read (file, tmp, len);
  if (count != len)
    {
      fprintf (stderr, "%s: read: %s.\n",
	       name, count ? strerror (errno) : "End of file reached");
      exit (1);
    }
  return tmp;
}
