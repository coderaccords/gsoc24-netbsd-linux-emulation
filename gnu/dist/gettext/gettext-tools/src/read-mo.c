/* Reading binary .mo files.
   Copyright (C) 1995-1998, 2000-2004 Free Software Foundation, Inc.
   Written by Ulrich Drepper <drepper@gnu.ai.mit.edu>, April 1995.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

/* Specification.  */
#include "read-mo.h"

#include <errno.h>
#include <stdio.h>
#include <stddef.h>
#include <string.h>

/* This include file describes the main part of binary .mo format.  */
#include "gmo.h"

#include "error.h"
#include "xalloc.h"
#include "binary-io.h"
#include "exit.h"
#include "message.h"
#include "gettext.h"

#define _(str) gettext (str)


/* We read the file completely into memory.  This is more efficient than
   lots of lseek().  This struct represents the .mo file in memory.  */
struct binary_mo_file
{
  const char *filename;
  char *data;
  size_t size;
  enum { MO_LITTLE_ENDIAN, MO_BIG_ENDIAN } endian;
};


/* Read the contents of the given input stream.  */
static void
read_binary_mo_file (struct binary_mo_file *bfp,
		     FILE *fp, const char *filename)
{
  char *buf = NULL;
  size_t alloc = 0;
  size_t size = 0;
  size_t count;

  while (!feof (fp))
    {
      const size_t increment = 4096;
      if (size + increment > alloc)
	{
	  alloc = alloc + alloc / 2;
	  if (alloc < size + increment)
	    alloc = size + increment;
	  buf = (char *) xrealloc (buf, alloc);
	}
      count = fread (buf + size, 1, increment, fp);
      if (count == 0)
	{
	  if (ferror (fp))
	    error (EXIT_FAILURE, errno, _("error while reading \"%s\""),
		   filename);
	}
      else
	size += count;
    }
  buf = (char *) xrealloc (buf, size);
  bfp->filename = filename;
  bfp->data = buf;
  bfp->size = size;
}

/* Get a 32-bit number from the file, at the given file position.  */
static nls_uint32
get_uint32 (const struct binary_mo_file *bfp, size_t offset)
{
  nls_uint32 b0, b1, b2, b3;

  if (offset + 4 > bfp->size)
    error (EXIT_FAILURE, 0, _("file \"%s\" is truncated"), bfp->filename);

  b0 = *(unsigned char *) (bfp->data + offset + 0);
  b1 = *(unsigned char *) (bfp->data + offset + 1);
  b2 = *(unsigned char *) (bfp->data + offset + 2);
  b3 = *(unsigned char *) (bfp->data + offset + 3);
  if (bfp->endian == MO_LITTLE_ENDIAN)
    return b0 | (b1 << 8) | (b2 << 16) | (b3 << 24);
  else
    return (b0 << 24) | (b1 << 16) | (b2 << 8) | b3;
}

/* Get a static string from the file, at the given file position.  */
static char *
get_string (const struct binary_mo_file *bfp, size_t offset, size_t *lengthp)
{
  /* See 'struct string_desc'.  */
  nls_uint32 s_length = get_uint32 (bfp, offset);
  nls_uint32 s_offset = get_uint32 (bfp, offset + 4);

  if (s_offset + s_length + 1 > bfp->size)
    error (EXIT_FAILURE, 0, _("file \"%s\" is truncated"), bfp->filename);
  if (bfp->data[s_offset + s_length] != '\0')
    error (EXIT_FAILURE, 0,
	   _("file \"%s\" contains a not NUL terminated string"),
	   bfp->filename);

  *lengthp = s_length + 1;
  return bfp->data + s_offset;
}

/* Get a system dependent string from the file, at the given file position.  */
static char *
get_sysdep_string (const struct binary_mo_file *bfp, size_t offset,
		   const struct mo_file_header *header, size_t *lengthp)
{
  /* See 'struct sysdep_string'.  */
  size_t length;
  char *string;
  size_t i;
  char *p;
  nls_uint32 s_offset;

  /* Compute the length.  */
  length = 0;
  for (i = 4; ; i += 8)
    {
      nls_uint32 segsize = get_uint32 (bfp, offset + i);
      nls_uint32 sysdepref = get_uint32 (bfp, offset + i + 4);
      nls_uint32 sysdep_segment_offset;
      nls_uint32 ss_length;
      nls_uint32 ss_offset;
      size_t n;

      length += segsize;

      if (sysdepref == SEGMENTS_END)
	break;
      if (sysdepref >= header->n_sysdep_segments)
	/* Invalid.  */
	  error (EXIT_FAILURE, 0, _("file \"%s\" is not in GNU .mo format"),
		 bfp->filename);
      /* See 'struct sysdep_segment'.  */
      sysdep_segment_offset = header->sysdep_segments_offset + sysdepref * 8;
      ss_length = get_uint32 (bfp, sysdep_segment_offset);
      ss_offset = get_uint32 (bfp, sysdep_segment_offset + 4);
      if (ss_offset + ss_length > bfp->size)
	error (EXIT_FAILURE, 0, _("file \"%s\" is truncated"), bfp->filename);
      if (!(ss_length > 0 && bfp->data[ss_offset + ss_length - 1] == '\0'))
	{
	  char location[30];
	  sprintf (location, "sysdep_segment[%u]", (unsigned int) sysdepref);
	  error (EXIT_FAILURE, 0,
		 _("file \"%s\" contains a not NUL terminated string, at %s"),
		 bfp->filename, location);
	}
      n = strlen (bfp->data + ss_offset);
      length += (n > 1 ? 1 + n + 1 : n);
    }

  /* Allocate and fill the string.  */
  string = (char *) xmalloc (length);
  p = string;
  s_offset = get_uint32 (bfp, offset);
  for (i = 4; ; i += 8)
    {
      nls_uint32 segsize = get_uint32 (bfp, offset + i);
      nls_uint32 sysdepref = get_uint32 (bfp, offset + i + 4);
      nls_uint32 sysdep_segment_offset;
      nls_uint32 ss_length;
      nls_uint32 ss_offset;
      size_t n;

      if (s_offset + segsize > bfp->size)
	error (EXIT_FAILURE, 0, _("file \"%s\" is truncated"), bfp->filename);
      memcpy (p, bfp->data + s_offset, segsize);
      p += segsize;
      s_offset += segsize;

      if (sysdepref == SEGMENTS_END)
	break;
      if (sysdepref >= header->n_sysdep_segments)
	abort ();
      /* See 'struct sysdep_segment'.  */
      sysdep_segment_offset = header->sysdep_segments_offset + sysdepref * 8;
      ss_length = get_uint32 (bfp, sysdep_segment_offset);
      ss_offset = get_uint32 (bfp, sysdep_segment_offset + 4);
      if (ss_offset + ss_length > bfp->size)
	abort ();
      if (!(ss_length > 0 && bfp->data[ss_offset + ss_length - 1] == '\0'))
	abort ();
      n = strlen (bfp->data + ss_offset);
      if (n > 1)
	*p++ = '<';
      memcpy (p, bfp->data + ss_offset, n);
      p += n;
      if (n > 1)
	*p++ = '>';
    }

  if (p != string + length)
    abort ();

  *lengthp = length;
  return string;
}

/* Reads an existing .mo file and adds the messages to mlp.  */
void
read_mo_file (message_list_ty *mlp, const char *filename)
{
  FILE *fp;
  struct binary_mo_file bf;
  struct mo_file_header header;
  unsigned int i;
  static lex_pos_ty pos = { __FILE__, __LINE__ };

  if (strcmp (filename, "-") == 0 || strcmp (filename, "/dev/stdin") == 0)
    {
      fp = stdin;
      SET_BINARY (fileno (fp));
    }
  else
    {
      fp = fopen (filename, "rb");
      if (fp == NULL)
	error (EXIT_FAILURE, errno,
	       _("error while opening \"%s\" for reading"), filename);
    }

  /* Read the file contents into memory.  */
  read_binary_mo_file (&bf, fp, filename);

  /* Get a 32-bit number from the file header.  */
# define GET_HEADER_FIELD(field) \
    get_uint32 (&bf, offsetof (struct mo_file_header, field))

  /* We must grope the file to determine which endian it is.
     Perversity of the universe tends towards maximum, so it will
     probably not match the currently executing architecture.  */
  bf.endian = MO_BIG_ENDIAN;
  header.magic = GET_HEADER_FIELD (magic);
  if (header.magic != _MAGIC)
    {
      bf.endian = MO_LITTLE_ENDIAN;
      header.magic = GET_HEADER_FIELD (magic);
      if (header.magic != _MAGIC)
	{
	unrecognised:
	  error (EXIT_FAILURE, 0, _("file \"%s\" is not in GNU .mo format"),
		 filename);
	}
    }

  header.revision = GET_HEADER_FIELD (revision);

  /* We support only the major revisions 0 and 1.  */
  switch (header.revision >> 16)
    {
    case 0:
    case 1:
      /* Fill the header parts that apply to major revisions 0 and 1.  */
      header.nstrings = GET_HEADER_FIELD (nstrings);
      header.orig_tab_offset = GET_HEADER_FIELD (orig_tab_offset);
      header.trans_tab_offset = GET_HEADER_FIELD (trans_tab_offset);
      header.hash_tab_size = GET_HEADER_FIELD (hash_tab_size);
      header.hash_tab_offset = GET_HEADER_FIELD (hash_tab_offset);

      for (i = 0; i < header.nstrings; i++)
	{
	  message_ty *mp;
	  char *msgid;
	  size_t msgid_len;
	  char *msgstr;
	  size_t msgstr_len;

	  /* Read the msgid.  */
	  msgid = get_string (&bf, header.orig_tab_offset + i * 8,
			      &msgid_len);

	  /* Read the msgstr.  */
	  msgstr = get_string (&bf, header.trans_tab_offset + i * 8,
			       &msgstr_len);

	  mp = message_alloc (msgid,
			      (strlen (msgid) + 1 < msgid_len
			       ? msgid + strlen (msgid) + 1
			       : NULL),
			      msgstr, msgstr_len,
			      &pos);
	  message_list_append (mlp, mp);
	}

      switch (header.revision & 0xffff)
	{
	case 0:
	  break;
	case 1:
	default:
	  /* Fill the header parts that apply to minor revision >= 1.  */
	  header.n_sysdep_segments = GET_HEADER_FIELD (n_sysdep_segments);
	  header.sysdep_segments_offset =
	    GET_HEADER_FIELD (sysdep_segments_offset);
	  header.n_sysdep_strings = GET_HEADER_FIELD (n_sysdep_strings);
	  header.orig_sysdep_tab_offset =
	    GET_HEADER_FIELD (orig_sysdep_tab_offset);
	  header.trans_sysdep_tab_offset =
	    GET_HEADER_FIELD (trans_sysdep_tab_offset);

	  for (i = 0; i < header.n_sysdep_strings; i++)
	    {
	      message_ty *mp;
	      char *msgid;
	      size_t msgid_len;
	      char *msgstr;
	      size_t msgstr_len;
	      nls_uint32 offset;

	      /* Read the msgid.  */
	      offset = get_uint32 (&bf, header.orig_sysdep_tab_offset + i * 4);
	      msgid = get_sysdep_string (&bf, offset, &header, &msgid_len);

	      /* Read the msgstr.  */
	      offset = get_uint32 (&bf, header.trans_sysdep_tab_offset + i * 4);
	      msgstr = get_sysdep_string (&bf, offset, &header, &msgstr_len);

	      mp = message_alloc (msgid,
				  (strlen (msgid) + 1 < msgid_len
				   ? msgid + strlen (msgid) + 1
				   : NULL),
				  msgstr, msgstr_len,
				  &pos);
	      message_list_append (mlp, mp);
	    }
	  break;
	}
      break;

    default:
      goto unrecognised;
    }

  if (fp != stdin)
    fclose (fp);
}
