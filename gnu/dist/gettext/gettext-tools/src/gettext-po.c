/* Public API for GNU gettext PO files.
   Copyright (C) 2003-2005 Free Software Foundation, Inc.
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
#include "gettext-po.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

#include "message.h"
#include "xalloc.h"
#include "read-po.h"
#include "write-po.h"
#include "error.h"
#include "xerror.h"
#include "po-error.h"
#include "vasprintf.h"
#include "format.h"
#include "gettext.h"

#define _(str) gettext(str)


struct po_file
{
  msgdomain_list_ty *mdlp;
  const char *real_filename;
  const char *logical_filename;
  const char **domains;
};

struct po_message_iterator
{
  po_file_t file;
  char *domain;
  message_list_ty *mlp;
  size_t index;
};

/* A po_message_t is actually a 'struct message_ty *'.  */

/* A po_filepos_t is actually a 'lex_pos_ty *'.  */


/* Version number: (major<<16) + (minor<<8) + subminor */
int libgettextpo_version = LIBGETTEXTPO_VERSION;


/* Create an empty PO file representation in memory.  */

po_file_t
po_file_create (void)
{
  po_file_t file;

  file = (struct po_file *) xmalloc (sizeof (struct po_file));
  file->mdlp = msgdomain_list_alloc (false);
  file->real_filename = _("<unnamed>");
  file->logical_filename = file->real_filename;
  file->domains = NULL;
  return file;
}


/* Read a PO file into memory.
   Return its contents.  Upon failure, return NULL and set errno.  */

po_file_t
po_file_read (const char *filename, po_error_handler_t handler)
{
  FILE *fp;
  po_file_t file;

  if (strcmp (filename, "-") == 0 || strcmp (filename, "/dev/stdin") == 0)
    {
      filename = _("<stdin>");
      fp = stdin;
    }
  else
    {
      fp = fopen (filename, "r");
      if (fp == NULL)
	return NULL;
    }

  /* Establish error handler around read_po().  */
  po_error             = handler->error;
  po_error_at_line     = handler->error_at_line;
  po_multiline_warning = handler->multiline_warning;
  po_multiline_error   = handler->multiline_error;

  file = (struct po_file *) xmalloc (sizeof (struct po_file));
  file->real_filename = filename;
  file->logical_filename = filename;
  file->mdlp = read_po (fp, file->real_filename, file->logical_filename);
  file->domains = NULL;

  /* Restore error handler.  */
  po_error             = error;
  po_error_at_line     = error_at_line;
  po_multiline_warning = multiline_warning;
  po_multiline_error   = multiline_error;

  if (fp != stdin)
    fclose (fp);
  return file;
}
#undef po_file_read

/* Older version for binary backward compatibility.  */
po_file_t
po_file_read (const char *filename)
{
  FILE *fp;
  po_file_t file;

  if (strcmp (filename, "-") == 0 || strcmp (filename, "/dev/stdin") == 0)
    {
      filename = _("<stdin>");
      fp = stdin;
    }
  else
    {
      fp = fopen (filename, "r");
      if (fp == NULL)
	return NULL;
    }

  file = (struct po_file *) xmalloc (sizeof (struct po_file));
  file->real_filename = filename;
  file->logical_filename = filename;
  file->mdlp = read_po (fp, file->real_filename, file->logical_filename);
  file->domains = NULL;

  if (fp != stdin)
    fclose (fp);
  return file;
}


/* Write an in-memory PO file to a file.
   Upon failure, return NULL and set errno.  */

po_file_t
po_file_write (po_file_t file, const char *filename, po_error_handler_t handler)
{
  /* Establish error handler around msgdomain_list_print().  */
  po_error             = handler->error;
  po_error_at_line     = handler->error_at_line;
  po_multiline_warning = handler->multiline_warning;
  po_multiline_error   = handler->multiline_error;

  msgdomain_list_print (file->mdlp, filename, true, false);

  /* Restore error handler.  */
  po_error             = error;
  po_error_at_line     = error_at_line;
  po_multiline_warning = multiline_warning;
  po_multiline_error   = multiline_error;

  return file;
}


/* Free a PO file from memory.  */

void
po_file_free (po_file_t file)
{
  msgdomain_list_free (file->mdlp);
  if (file->domains != NULL)
    free (file->domains);
  free (file);
}


/* Return the names of the domains covered by a PO file in memory.  */

const char * const *
po_file_domains (po_file_t file)
{
  if (file->domains == NULL)
    {
      size_t n = file->mdlp->nitems;
      const char **domains =
	(const char **) xmalloc ((n + 1) * sizeof (const char *));
      size_t j;

      for (j = 0; j < n; j++)
	domains[j] = file->mdlp->item[j]->domain;
      domains[n] = NULL;

      file->domains = domains;
    }

  return file->domains;
}


/* Return the header entry of a domain of a PO file in memory.
   The domain NULL denotes the default domain.
   Return NULL if there is no header entry.  */

const char *
po_file_domain_header (po_file_t file, const char *domain)
{
  message_list_ty *mlp;
  size_t j;

  if (domain == NULL)
    domain = MESSAGE_DOMAIN_DEFAULT;
  mlp = msgdomain_list_sublist (file->mdlp, domain, false);
  if (mlp != NULL)
    for (j = 0; j < mlp->nitems; j++)
      if (mlp->item[j]->msgid[0] == '\0' && !mlp->item[j]->obsolete)
	{
	  const char *header = mlp->item[j]->msgstr;

	  if (header != NULL)
	    return xstrdup (header);
	  else
	    return NULL;
	}
  return NULL;
}


/* Return the value of a field in a header entry.
   The return value is either a freshly allocated string, to be freed by the
   caller, or NULL.  */

char *
po_header_field (const char *header, const char *field)
{
  size_t field_len = strlen (field);
  const char *line;

  for (line = header;;)
    {
      if (strncmp (line, field, field_len) == 0
	  && line[field_len] == ':' && line[field_len + 1] == ' ')
	{
	  const char *value_start;
	  const char *value_end;
	  char *value;

	  value_start = line + field_len + 2;
	  value_end = strchr (value_start, '\n');
	  if (value_end == NULL)
	    value_end = value_start + strlen (value_start);

	  value = (char *) xmalloc (value_end - value_start + 1);
	  memcpy (value, value_start, value_end - value_start);
	  value[value_end - value_start] = '\0';

	  return value;
	}

      line = strchr (line, '\n');
      if (line != NULL)
	line++;
      else
	break;
    }

  return NULL;
}


/* Return the header entry with a given field set to a given value.  The field
   is added if necessary.
   The return value is a freshly allocated string.  */

char *
po_header_set_field (const char *header, const char *field, const char *value)
{
  size_t header_len = strlen (header);
  size_t field_len = strlen (field);
  size_t value_len = strlen (value);

  {
    const char *line;

    for (line = header;;)
      {
	if (strncmp (line, field, field_len) == 0
	    && line[field_len] == ':' && line[field_len + 1] == ' ')
	  {
	    const char *oldvalue_start;
	    const char *oldvalue_end;
	    size_t oldvalue_len;
	    size_t header_part1_len;
	    size_t header_part3_len;
	    size_t result_len;
	    char *result;

	    oldvalue_start = line + field_len + 2;
	    oldvalue_end = strchr (oldvalue_start, '\n');
	    if (oldvalue_end == NULL)
	      oldvalue_end = oldvalue_start + strlen (oldvalue_start);
	    oldvalue_len = oldvalue_end - oldvalue_start;

	    header_part1_len = oldvalue_start - header;
	    header_part3_len = header + header_len - oldvalue_end;
	    result_len = header_part1_len + value_len + header_part3_len;
		    /* = header_len - oldvalue_len + value_len */
	    result = (char *) xmalloc (result_len + 1);
	    memcpy (result, header, header_part1_len);
	    memcpy (result + header_part1_len, value, value_len);
	    memcpy (result + header_part1_len + value_len, oldvalue_end,
		    header_part3_len);
	    *(result + result_len) = '\0';

	    return result;
	  }

	line = strchr (line, '\n');
	if (line != NULL)
	  line++;
	else
	  break;
      }
  }
  {
    size_t newline;
    size_t result_len;
    char *result;

    newline = (header_len > 0 && header[header_len - 1] != '\n' ? 1 : 0);
    result_len = header_len + newline + field_len + 2 + value_len + 1;
    result = (char *) xmalloc (result_len + 1);
    memcpy (result, header, header_len);
    if (newline)
      *(result + header_len) = '\n';
    memcpy (result + header_len + newline, field, field_len);
    *(result + header_len + newline + field_len) = ':';
    *(result + header_len + newline + field_len + 1) = ' ';
    memcpy (result + header_len + newline + field_len + 2, value, value_len);
    *(result + header_len + newline + field_len + 2 + value_len) = '\n';
    *(result + result_len) = '\0';

    return result;
  }
}


/* Create an iterator for traversing a domain of a PO file in memory.
   The domain NULL denotes the default domain.  */

po_message_iterator_t
po_message_iterator (po_file_t file, const char *domain)
{
  po_message_iterator_t iterator;

  if (domain == NULL)
    domain = MESSAGE_DOMAIN_DEFAULT;

  iterator =
    (struct po_message_iterator *)
    xmalloc (sizeof (struct po_message_iterator));
  iterator->file = file;
  iterator->domain = xstrdup (domain);
  iterator->mlp = msgdomain_list_sublist (file->mdlp, domain, false);
  iterator->index = 0;

  return iterator;
}


/* Free an iterator.  */

void
po_message_iterator_free (po_message_iterator_t iterator)
{
  free (iterator->domain);
  free (iterator);
}


/* Return the next message, and advance the iterator.
   Return NULL at the end of the message list.  */

po_message_t
po_next_message (po_message_iterator_t iterator)
{
  if (iterator->mlp != NULL && iterator->index < iterator->mlp->nitems)
    return (po_message_t) iterator->mlp->item[iterator->index++];
  else
    return NULL;
}


/* Insert a message in a PO file in memory, in the domain and at the position
   indicated by the iterator.  The iterator thereby advances past the freshly
   inserted message.  */

void
po_message_insert (po_message_iterator_t iterator, po_message_t message)
{
  message_ty *mp = (message_ty *) message;

  if (iterator->mlp == NULL)
    /* Now we need to allocate a sublist corresponding to the iterator.  */
    iterator->mlp =
      msgdomain_list_sublist (iterator->file->mdlp, iterator->domain, true);
  /* Insert the message.  */
  message_list_insert_at (iterator->mlp, iterator->index, mp);
  /* Advance the iterator.  */
  iterator->index++;
}


/* Return a freshly constructed message.
   To finish initializing the message, you must set the msgid and msgstr.  */

po_message_t
po_message_create (void)
{
  lex_pos_ty pos = { NULL, 0 };

  return (po_message_t) message_alloc (NULL, NULL, NULL, 0, &pos);
}


/* Return the msgid (untranslated English string) of a message.  */

const char *
po_message_msgid (po_message_t message)
{
  message_ty *mp = (message_ty *) message;

  return mp->msgid;
}


/* Change the msgid (untranslated English string) of a message.  */

void
po_message_set_msgid (po_message_t message, const char *msgid)
{
  message_ty *mp = (message_ty *) message;

  if (msgid != mp->msgid)
    {
      char *old_msgid = (char *) mp->msgid;

      mp->msgid = xstrdup (msgid);
      if (old_msgid != NULL)
	free (old_msgid);
    }
}


/* Return the msgid_plural (untranslated English plural string) of a message,
   or NULL for a message without plural.  */

const char *
po_message_msgid_plural (po_message_t message)
{
  message_ty *mp = (message_ty *) message;

  return mp->msgid_plural;
}


/* Change the msgid_plural (untranslated English plural string) of a message.
   NULL means a message without plural.  */

void
po_message_set_msgid_plural (po_message_t message, const char *msgid_plural)
{
  message_ty *mp = (message_ty *) message;

  if (msgid_plural != mp->msgid_plural)
    {
      char *old_msgid_plural = (char *) mp->msgid_plural;

      mp->msgid_plural = (msgid_plural != NULL ? xstrdup (msgid_plural) : NULL);
      if (old_msgid_plural != NULL)
	free (old_msgid_plural);
    }
}


/* Return the msgstr (translation) of a message.
   Return the empty string for an untranslated message.  */

const char *
po_message_msgstr (po_message_t message)
{
  message_ty *mp = (message_ty *) message;

  return mp->msgstr;
}


/* Change the msgstr (translation) of a message.
   Use an empty string to denote an untranslated message.  */

void
po_message_set_msgstr (po_message_t message, const char *msgstr)
{
  message_ty *mp = (message_ty *) message;

  if (msgstr != mp->msgstr)
    {
      char *old_msgstr = (char *) mp->msgstr;

      mp->msgstr = xstrdup (msgstr);
      mp->msgstr_len = strlen (mp->msgstr) + 1;
      if (old_msgstr != NULL)
	free (old_msgstr);
    }
}


/* Return the msgstr[index] for a message with plural handling, or
   NULL when the index is out of range or for a message without plural.  */

const char *
po_message_msgstr_plural (po_message_t message, int index)
{
  message_ty *mp = (message_ty *) message;

  if (mp->msgid_plural != NULL && index >= 0)
    {
      const char *p;
      const char *p_end = mp->msgstr + mp->msgstr_len;

      for (p = mp->msgstr; ; p += strlen (p) + 1, index--)
	{
	  if (p >= p_end)
	    return NULL;
	  if (index == 0)
	    break;
	}
      return p;
    }
  else
    return NULL;
}


/* Change the msgstr[index] for a message with plural handling.
   Use a NULL value at the end to reduce the number of plural forms.  */

void
po_message_set_msgstr_plural (po_message_t message, int index, const char *msgstr)
{
  message_ty *mp = (message_ty *) message;

  if (mp->msgid_plural != NULL && index >= 0)
    {
      char *p = (char *) mp->msgstr;
      char *p_end = (char *) mp->msgstr + mp->msgstr_len;
      char *copied_msgstr;

      /* Special care must be taken of the case that msgstr points into the
	 mp->msgstr string list, because mp->msgstr may be relocated before we
	 are done with msgstr.  */
      if (msgstr >= p && msgstr < p_end)
	msgstr = copied_msgstr = xstrdup (msgstr);
      else
	copied_msgstr = NULL;

      for (; ; p += strlen (p) + 1, index--)
	{
	  if (p >= p_end)
	    {
	      /* Append at the end.  */
	      if (msgstr != NULL)
		{
		  size_t new_msgstr_len = mp->msgstr_len + index + strlen (msgstr) + 1;

		  mp->msgstr =
		    (char *) xrealloc ((char *) mp->msgstr, new_msgstr_len);
		  p = (char *) mp->msgstr + mp->msgstr_len;
		  for (; index > 0; index--)
		    *p++ = '\0';
		  memcpy (p, msgstr, strlen (msgstr) + 1);
		  mp->msgstr_len = new_msgstr_len;
		}
	      if (copied_msgstr != NULL)
		free (copied_msgstr);
	      return;
	    }
	  if (index == 0)
	    break;
	}
      if (msgstr == NULL)
	{
	  if (p + strlen (p) + 1 >= p_end)
	    {
	      /* Remove the string that starts at p.  */
	      mp->msgstr_len = p - mp->msgstr;
	      return;
	    }
	  /* It is not possible to remove an element of the string list
	     except the last one.  So just replace it with the empty string.
	     That's the best we can do here.  */
	  msgstr = "";
	}
      {
	/* Replace the string that starts at p.  */
	size_t i1 = p - mp->msgstr;
	size_t i2before = i1 + strlen (p);
	size_t i2after = i1 + strlen (msgstr);
	size_t new_msgstr_len = mp->msgstr_len - i2before + i2after;

	if (i2after > i2before)
	  mp->msgstr = (char *) xrealloc ((char *) mp->msgstr, new_msgstr_len);
	memmove ((char *) mp->msgstr + i2after, mp->msgstr + i2before,
		 mp->msgstr_len - i2before);
	memcpy ((char *) mp->msgstr + i1, msgstr, i2after - i1);
	mp->msgstr_len = new_msgstr_len;
      }
      if (copied_msgstr != NULL)
	free (copied_msgstr);
    }
}


/* Return the comments for a message.  */

const char *
po_message_comments (po_message_t message)
{
  /* FIXME: memory leak.  */
  message_ty *mp = (message_ty *) message;

  if (mp->comment == NULL || mp->comment->nitems == 0)
    return "";
  else
    return string_list_join (mp->comment, '\n', '\n', true);
}


/* Change the comments for a message.
   comments should be a multiline string, ending in a newline, or empty.  */

void
po_message_set_comments (po_message_t message, const char *comments)
{
  message_ty *mp = (message_ty *) message;
  string_list_ty *slp = string_list_alloc ();

  {
    char *copy = xstrdup (comments);
    char *rest;

    rest = copy;
    while (*rest != '\0')
      {
	char *newline = strchr (rest, '\n');

	if (newline != NULL)
	  {
	    *newline = '\0';
	    string_list_append (slp, rest);
	    rest = newline + 1;
	  }
	else
	  {
	    string_list_append (slp, rest);
	    break;
	  }
      }
    free (copy);
  }

  if (mp->comment != NULL)
    string_list_free (mp->comment);

  mp->comment = slp;
}


/* Return the extracted comments for a message.  */

const char *
po_message_extracted_comments (po_message_t message)
{
  /* FIXME: memory leak.  */
  message_ty *mp = (message_ty *) message;

  if (mp->comment_dot == NULL || mp->comment_dot->nitems == 0)
    return "";
  else
    return string_list_join (mp->comment_dot, '\n', '\n', true);
}


/* Change the extracted comments for a message.
   comments should be a multiline string, ending in a newline, or empty.  */

void
po_message_set_extracted_comments (po_message_t message, const char *comments)
{
  message_ty *mp = (message_ty *) message;
  string_list_ty *slp = string_list_alloc ();

  {
    char *copy = xstrdup (comments);
    char *rest;

    rest = copy;
    while (*rest != '\0')
      {
	char *newline = strchr (rest, '\n');

	if (newline != NULL)
	  {
	    *newline = '\0';
	    string_list_append (slp, rest);
	    rest = newline + 1;
	  }
	else
	  {
	    string_list_append (slp, rest);
	    break;
	  }
      }
    free (copy);
  }

  if (mp->comment_dot != NULL)
    string_list_free (mp->comment_dot);

  mp->comment_dot = slp;
}


/* Return the i-th file position for a message, or NULL if i is out of
   range.  */

po_filepos_t
po_message_filepos (po_message_t message, int i)
{
  message_ty *mp = (message_ty *) message;

  if (i >= 0 && (size_t)i < mp->filepos_count)
    return (po_filepos_t) &mp->filepos[i];
  else
    return NULL;
}


/* Remove the i-th file position from a message.
   The indices of all following file positions for the message are decremented
   by one.  */

void
po_message_remove_filepos (po_message_t message, int i)
{
  message_ty *mp = (message_ty *) message;

  if (i >= 0)
    {
      size_t j = (size_t)i;
      size_t n = mp->filepos_count;

      if (j < n)
	{
	  mp->filepos_count = n = n - 1;
	  free ((char *) mp->filepos[j].file_name);
	  for (; j < n; j++)
	    mp->filepos[j] = mp->filepos[j + 1];
	}
    }
}


/* Add a file position to a message, if it is not already present for the
   message.
   file is the file name.
   start_line is the line number where the string starts, or (size_t)(-1) if no
   line number is available.  */

void
po_message_add_filepos (po_message_t message, const char *file, size_t start_line)
{
  message_ty *mp = (message_ty *) message;

  message_comment_filepos (mp, file, start_line);
}


/* Return true if the message is marked obsolete.  */

int
po_message_is_obsolete (po_message_t message)
{
  message_ty *mp = (message_ty *) message;

  return (mp->obsolete ? 1 : 0);
}


/* Change the obsolete mark of a message.  */

void
po_message_set_obsolete (po_message_t message, int obsolete)
{
  message_ty *mp = (message_ty *) message;

  mp->obsolete = obsolete;
}


/* Return true if the message is marked fuzzy.  */

int
po_message_is_fuzzy (po_message_t message)
{
  message_ty *mp = (message_ty *) message;

  return (mp->is_fuzzy ? 1 : 0);
}


/* Change the fuzzy mark of a message.  */

void
po_message_set_fuzzy (po_message_t message, int fuzzy)
{
  message_ty *mp = (message_ty *) message;

  mp->is_fuzzy = fuzzy;
}


/* Return true if the message is marked as being a format string of the given
   type (e.g. "c-format").  */

int
po_message_is_format (po_message_t message, const char *format_type)
{
  message_ty *mp = (message_ty *) message;
  size_t len = strlen (format_type);
  size_t i;

  if (len >= 7 && memcmp (format_type + len - 7, "-format", 7) == 0)
    for (i = 0; i < NFORMATS; i++)
      if (strlen (format_language[i]) == len - 7
	  && memcmp (format_language[i], format_type, len - 7) == 0)
	/* The given format_type corresponds to (enum format_type) i.  */
	return (possible_format_p (mp->is_format[i]) ? 1 : 0);
  return 0;
}


/* Change the format string mark for a given type of a message.  */

void
po_message_set_format (po_message_t message, const char *format_type, /*bool*/int value)
{
  message_ty *mp = (message_ty *) message;
  size_t len = strlen (format_type);
  size_t i;

  if (len >= 7 && memcmp (format_type + len - 7, "-format", 7) == 0)
    for (i = 0; i < NFORMATS; i++)
      if (strlen (format_language[i]) == len - 7
	  && memcmp (format_language[i], format_type, len - 7) == 0)
	/* The given format_type corresponds to (enum format_type) i.  */
	mp->is_format[i] = (value ? yes : no);
}


/* An error logger based on the po_error function pointer.  */
static void
po_error_logger (const char *format, ...)
{
  va_list args;
  char *error_message;

  va_start (args, format);
  if (vasprintf (&error_message, format, args) < 0)
    error (EXIT_FAILURE, 0, _("memory exhausted"));
  va_end (args);
  po_error (0, 0, "%s", error_message);
  free (error_message);
}

/* Test whether the message translation is a valid format string if the message
   is marked as being a format string.  If it is invalid, pass the reasons to
   the handler.  */
void
po_message_check_format (po_message_t message, po_error_handler_t handler)
{
  message_ty *mp = (message_ty *) message;

  /* Establish error handler for po_error_logger().  */
  po_error = handler->error;

  check_msgid_msgstr_format (mp->msgid, mp->msgid_plural,
			     mp->msgstr, mp->msgstr_len,
			     mp->is_format, po_error_logger);

  /* Restore error handler.  */
  po_error = error;
}


/* Return the file name.  */

const char *
po_filepos_file (po_filepos_t filepos)
{
  lex_pos_ty *pp = (lex_pos_ty *) filepos;

  return pp->file_name;
}


/* Return the line number where the string starts, or (size_t)(-1) if no line
   number is available.  */

size_t
po_filepos_start_line (po_filepos_t filepos)
{
  lex_pos_ty *pp = (lex_pos_ty *) filepos;

  return pp->line_number;
}
