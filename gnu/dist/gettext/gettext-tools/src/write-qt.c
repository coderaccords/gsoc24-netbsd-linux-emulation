/* Writing Qt .qm files.
   Copyright (C) 2003, 2005 Free Software Foundation, Inc.
   Written by Bruno Haible <bruno@clisp.org>, 2003.

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
#include "write-qt.h"

#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "error.h"
#include "xerror.h"
#include "message.h"
#include "po-charset.h"
#include "msgl-iconv.h"
#include "hash-string.h"
#include "utf8-ucs4.h"
#include "xalloc.h"
#include "obstack.h"
#include "binary-io.h"
#include "fwriteerror.h"
#include "exit.h"
#include "gettext.h"

#define _(str) gettext (str)

/* Qt .qm files are read by the QTranslator::load() function and written
   by the Qt QTranslator::save() function.

   The Qt tool 'msg2qm' uses the latter function and can convert PO files
   to .qm files. But since 'msg2qm' is marked as an "old" tool in Qt 3.0.5's
   i18n.html documentation and therefore likely to disappear, we provide the
   same functionality here.

   The format of .qm files, as reverse engineered from the functions
     QTranslator::save(const QString& filename, SaveMode mode)
     QTranslator::squeeze(SaveMode mode)
     QTranslatorMessage::write(QDataStream& stream, bool strip, Prefix prefix)
     elfHash(const char* name)
   in qt-3.0.5, is as follows:

     It's a binary data format. Elements are u8 (byte), u16, u32. They are
     written in big-endian order.

     The file starts with a magic string of 16 bytes:
       3C B8 64 18 CA EF 9C 95 CD 21 1C BF 60 A1 BD DD

     Then come three sections. Each of the three sections is optional. Each
     has this structure:
       struct {
         u8 section_type; // 0x42 = hashes, 0x69 = messages, 0x2f = contexts
         u32 length; // number of bytes of the data
         u8 data[length];
       };

     In the first section, the hashes section, the data has the following
     structure:
       It's a sorted array of
         struct {
           u32 hashcode; // elfHash of the concatenation of msgid and
                         // disambiguating-comment
           u32 offset; // offset within the data[] of the messages section
         };
       It's sorted in ascending order by hashcode as primary sorting criteria
       and - when the hashcodes are the same - by offset as secondary criteria.

     In the second section, the messages section, the data has the following
     structure:
       It's a sequence of records, each representing a message, in no
       particular order. Each record is a sequence of subsections, each
       introduced by a particular subsection tag. The possible subsection tags
       are (and they usually occur in this order):
         - 03: Translation. Followed by the msgstr in UCS-2 or UTF-16 format:
               struct {
                 u32 length;
                 u16 chars[length/2];
               };
         - 08: Disambiguating-comment. Followed by the NUL-terminated,
               ISO-8859-1 encoded, disambiguating-comment string:
               struct {
                 u32 length;    // number of bytes including the NUL at the end
                 u8 chars[length];
               };
         - 06: SourceText, i.e. msgid. Followed by the NUL-terminated,
               ISO-8859-1 encoded, msgid:
               struct {
                 u32 length;    // number of bytes including the NUL at the end
                 u8 chars[length];
               };
         - 02: SourceText16, i.e. msgid. Encoded as UCS-2, but must actually
               be ISO-8859-1.
               struct {
                 u32 length;
                 u16 chars[length/2];
               };
               This subsection tag is obsoleted by SourceText.
         - 07: Context. Followed by the NUL-terminated, ISO-8859-1 encoded,
               context string (usually a C++ class name or empty):
               struct {
                 u32 length;    // number of bytes including the NUL at the end
                 u8 chars[length];
               };
         - 04: Context16. Encoded as UCS-2, but must actually be ISO-8859-1.
               struct {
                 u32 length;
                 u16 chars[length/2];
               };
               This subsection tag is obsoleted by Context.
         - 05: Hash. Followed by
               struct {
                 u32 hashcode; // elfHash of the concatenation of msgid and
                               // disambiguating-comment
               };
         - 01: End. Designates the end of the record. No further data.
       Usually the following subsections are written, but some of them are
       optional:
         - 03: Translation.
         - 08: Disambiguating-comment (optional).
         - 06: SourceText (optional).
         - 07: Context (optional).
         - 05: Hash.
         - 01: End.
       A subsection can be omitted if the value to be output is the same as
       for the previous record.

     In the third section, the contexts section, the data contains a hash
     table. Quite complicated.

     The elfHash function is the same as our hash_string function, except that
     at the end it maps a hash code of 0x00000000 to 0x00000001.

   When we convert from PO file format, all disambiguating-comments and
   contexts are empty, and therefore the contexts section can be omitted.  */


/* Write a u8 (a single byte) to the output stream.  */
static inline void
write_u8 (FILE *output_file, unsigned char value)
{
  putc (value, output_file);
}

/* Write a u16 (two bytes) to the output stream.  */
static inline void
write_u16 (FILE *output_file, unsigned short value)
{
  unsigned char data[2];

  data[0] = (value >> 8) & 0xff;
  data[1] = value & 0xff;

  fwrite (data, 2, 1, output_file);
}

/* Write a u32 (four bytes) to the output stream.  */
static inline void
write_u32 (FILE *output_file, unsigned int value)
{
  unsigned char data[4];

  data[0] = (value >> 24) & 0xff;
  data[1] = (value >> 16) & 0xff;
  data[2] = (value >> 8) & 0xff;
  data[3] = value & 0xff;

  fwrite (data, 4, 1, output_file);
}


#define obstack_chunk_alloc xmalloc
#define obstack_chunk_free free

/* Add a u8 (a single byte) to an obstack.  */
static void
append_u8 (struct obstack *mempool, unsigned char value)
{
  unsigned char data[1];

  data[0] = value;

  obstack_grow (mempool, data, 1);
}

/* Add a u16 (two bytes) to an obstack.  */
static void
append_u16 (struct obstack *mempool, unsigned short value)
{
  unsigned char data[2];

  data[0] = (value >> 8) & 0xff;
  data[1] = value & 0xff;

  obstack_grow (mempool, data, 2);
}

/* Add a u32 (four bytes) to an obstack.  */
static void
append_u32 (struct obstack *mempool, unsigned int value)
{
  unsigned char data[4];

  data[0] = (value >> 24) & 0xff;
  data[1] = (value >> 16) & 0xff;
  data[2] = (value >> 8) & 0xff;
  data[3] = value & 0xff;

  obstack_grow (mempool, data, 4);
}

/* Add an ISO-8859-1 encoded string to an obstack.  */
static void
append_base_string (struct obstack *mempool, const char *string)
{
  size_t length = strlen (string) + 1;
  append_u32 (mempool, length);
  obstack_grow (mempool, string, length);
}

/* Add an UTF-16 encoded string to an obstack.  */
static void
append_unicode_string (struct obstack *mempool, const unsigned short *string,
		       size_t length)
{
  append_u32 (mempool, length * 2);
  for (; length > 0; string++, length--)
    append_u16 (mempool, *string);
}

/* Retrieve a 4-byte integer from memory.  */
static inline unsigned int
peek_u32 (const unsigned char *p)
{
  return (p[0] << 24) | (p[1] << 16) | (p[2] << 8) | p[3];
}

/* Convert an UTF-8 string to ISO-8859-1, without error checking.  */
static char *
conv_to_iso_8859_1 (const char *string)
{
  size_t length = strlen (string);
  const char *str = string;
  const char *str_limit = string + length;
  /* Conversion to ISO-8859-1 can only reduce the number of bytes.  */
  char *result = (char *) xmalloc (length + 1);
  char *q = result;

  while (str < str_limit)
    {
      unsigned int uc;
      str += u8_mbtouc (&uc, (const unsigned char *) str, str_limit - str);
      /* It has already been verified that the string its in ISO-8859-1.  */
      if (!(uc < 0x100))
	abort ();
      /* Store as ISO-8859-1.  */
      *q++ = (unsigned char) uc;
    }
  *q = '\0';
  assert (q - result <= length);

  return result;
}

/* Convert an UTF-8 string to UTF-16, returning its size (number of UTF-16
   codepoints) in *SIZEP.  */
static unsigned short *
conv_to_utf16 (const char *string, size_t *sizep)
{
  size_t length = strlen (string);
  const char *str = string;
  const char *str_limit = string + length;
  /* Conversion to UTF-16 can at most double the number of bytes.  */
  unsigned short *result = (unsigned short *) xmalloc (2 * length);
  unsigned short *q = result;

  while (str < str_limit)
    {
      unsigned int uc;
      str += u8_mbtouc (&uc, (const unsigned char *) str, str_limit - str);
      if (uc < 0x10000)
	/* UCS-2 character.  */
	*q++ = (unsigned short) uc;
      else
	{
	  /* UTF-16 surrogate.  */
	  *q++ = 0xd800 + ((uc - 0x10000) >> 10);
	  *q++ = 0xdc00 + ((uc - 0x10000) & 0x3ff);
	}
    }
  assert (q - result <= 2 * length);

  *sizep = q - result;
  return result;
}

/* Return the Qt hash code of a string.  */
static unsigned int
string_hashcode (const char *str)
{
  unsigned int h;

  h = hash_string (str);
  if (h == 0)
    h = 1;
  return h;
}

/* Compare two entries of the hashes section.  */
static int
cmp_hashes (const void *va, const void *vb)
{
  const unsigned char *a = (const unsigned char *) va;
  const unsigned char *b = (const unsigned char *) vb;
  unsigned int a_hashcode = peek_u32 (a);
  unsigned int b_hashcode = peek_u32 (b);

  if (a_hashcode != b_hashcode)
    return (a_hashcode >= b_hashcode ? 1 : -1);
  else
    {
      unsigned int a_offset = peek_u32 (a + 4);
      unsigned int b_offset = peek_u32 (b + 4);

      if (a_offset != b_offset)
	return (a_offset >= b_offset ? 1 : -1);
      else
	return 0;
    }
}


/* Write a section to the output stream.  */
static void
write_section (FILE *output_file, unsigned char tag, void *data, size_t size)
{
  /* A section can be omitted if it is empty.  */
  if (size > 0)
    {
      write_u8 (output_file, tag);
      write_u32 (output_file, size);
      fwrite (data, size, 1, output_file);
    }
}


/* Write an entire .qm file.  */
static void
write_qm (FILE *output_file, message_list_ty *mlp)
{
  static unsigned char magic[16] =
    {
      0x3C, 0xB8, 0x64, 0x18, 0xCA, 0xEF, 0x9C, 0x95,
      0xCD, 0x21, 0x1C, 0xBF, 0x60, 0xA1, 0xBD, 0xDD
    };
  struct obstack hashes_pool;
  struct obstack messages_pool;
  size_t j;

  obstack_init (&hashes_pool);
  obstack_init (&messages_pool);

  /* Prepare the hashes section and the messages section.  */
  for (j = 0; j < mlp->nitems; j++)
    {
      message_ty *mp = mlp->item[j];

      /* No need to emit the header entry, it's not needed at runtime.  */
      if (mp->msgid[0] != '\0')
	{
	  char *msgid_as_iso_8859_1 = conv_to_iso_8859_1 (mp->msgid);
	  size_t msgstr_len;
	  unsigned short *msgstr_as_utf16 =
	    conv_to_utf16 (mp->msgstr, &msgstr_len);
	  unsigned int hashcode = string_hashcode (msgid_as_iso_8859_1);
	  unsigned int offset = obstack_object_size (&messages_pool);

	  /* Add a record to the hashes section.  */
	  append_u32 (&hashes_pool, hashcode);
	  append_u32 (&hashes_pool, offset);

	  /* Add a record to the messages section.  */

	  append_u8 (&messages_pool, 0x03);
	  append_unicode_string (&messages_pool, msgstr_as_utf16, msgstr_len);

	  append_u8 (&messages_pool, 0x08);
	  append_base_string (&messages_pool, "");

	  append_u8 (&messages_pool, 0x06);
	  append_base_string (&messages_pool, msgid_as_iso_8859_1);

	  append_u8 (&messages_pool, 0x07);
	  append_base_string (&messages_pool, "");

	  append_u8 (&messages_pool, 0x05);
	  append_u32 (&messages_pool, hashcode);

	  append_u8 (&messages_pool, 0x01);

	  free (msgstr_as_utf16);
	  free (msgid_as_iso_8859_1);
	}
    }

  /* Sort the hashes section.  */
  {
    size_t nstrings = obstack_object_size (&hashes_pool) / 8;
    if (nstrings > 0)
      qsort (obstack_base (&hashes_pool), nstrings, 8, cmp_hashes);
  }

  /* Write the magic number.  */
  fwrite (magic, sizeof (magic), 1, output_file);

  /* Write the hashes section.  */
  write_section (output_file, 0x42, obstack_base (&hashes_pool),
		 obstack_object_size (&hashes_pool));

  /* Write the messages section.  */
  write_section (output_file, 0x69, obstack_base (&messages_pool),
		 obstack_object_size (&messages_pool));

  /* Omit the contexts section.  */
#if 0
  write_section (output_file, 0x2f, ...);
#endif

  obstack_free (&messages_pool, NULL);
  obstack_free (&hashes_pool, NULL);
}


int
msgdomain_write_qt (message_list_ty *mlp, const char *canon_encoding,
		    const char *domain_name, const char *file_name)
{
  FILE *output_file;

  /* If no entry for this domain don't even create the file.  */
  if (mlp->nitems != 0)
    {
      /* Determine whether mlp has plural entries.  */
      {
	bool has_plural;
	size_t j;

	has_plural = false;
	for (j = 0; j < mlp->nitems; j++)
	  if (mlp->item[j]->msgid_plural != NULL)
	    has_plural = true;
	if (has_plural)
	  {
	    multiline_error (xstrdup (""),
			     xstrdup (_("\
message catalog has plural form translations\n\
but the Qt message catalog format doesn't support plural handling\n")));
	    return 1;
	  }
      }

      /* Convert the messages to Unicode.  */
      iconv_message_list (mlp, canon_encoding, po_charset_utf8, NULL);

      /* Determine whether mlp has non-ISO-8859-1 msgid entries.  */
      {
	size_t j;

	for (j = 0; j < mlp->nitems; j++)
	  {
	    const char *string = mlp->item[j]->msgid;

	    /* An UTF-8 encoded string fits in ISO-8859-1 if and only if all
	       its bytes are < 0xc4.  */
	    for (; *string; string++)
	      if ((unsigned char) *string >= 0xc4)
		{
		  multiline_error (xstrdup (""),
				   xstrdup (_("\
message catalog has msgid strings containing characters outside ISO-8859-1\n\
but the Qt message catalog format supports Unicode only in the translated\n\
strings, not in the untranslated strings\n")));
		  return 1;
		}
	  }
      }

      if (strcmp (domain_name, "-") == 0)
	{
	  output_file = stdout;
	  SET_BINARY (fileno (output_file));
	}
      else
	{
	  output_file = fopen (file_name, "wb");
	  if (output_file == NULL)
	    {
	      error (0, errno, _("error while opening \"%s\" for writing"),
		     file_name);
	      return 1;
	    }
	}

      if (output_file != NULL)
	{
	  write_qm (output_file, mlp);

	  /* Make sure nothing went wrong.  */
	  if (fwriteerror (output_file))
	    error (EXIT_FAILURE, errno, _("error while writing \"%s\" file"),
		   file_name);
	}
    }

  return 0;
}
