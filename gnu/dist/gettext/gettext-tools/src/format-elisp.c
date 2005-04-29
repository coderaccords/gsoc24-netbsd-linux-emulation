/* Emacs Lisp format strings.
   Copyright (C) 2001-2004 Free Software Foundation, Inc.
   Written by Bruno Haible <haible@clisp.cons.org>, 2002.

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

#include <stdbool.h>
#include <stdlib.h>

#include "format.h"
#include "c-ctype.h"
#include "xalloc.h"
#include "xerror.h"
#include "format-invalid.h"
#include "gettext.h"

#define _(str) gettext (str)

/* Emacs Lisp format strings are implemented in emacs-21.1/src/editfns.c,
   xemacs-21.1.14/src/editfns.c and xemacs-21.1.14/src/doprnt.c.
   A directive
   - starts with '%' or '%m$' where m is a positive integer,
   - is optionally followed by any of the characters '#', '0', '-', ' ', '+',
     each of which acts as a flag,
   - is optionally followed by a width specification: '*' (reads an argument)
     or a nonempty digit sequence,
   - is optionally followed by '.' and a precision specification: '*' (reads
     an argument) or a nonempty digit sequence,
   - is finished by a specifier
       - '%', that needs no argument,
       - 'c', that need a character argument,
       - 'd', 'i', 'x', 'X', 'o', that need an integer argument,
       - 'e', 'E', 'f', 'g', 'G', that need a floating-point argument,
       - 's', that need an argument and prints it using princ,
       - 'S', that need an argument and prints it using prin1.
   Numbered ('%m$') and unnumbered argument specifications can be used in the
   same string. The effect of '%m$' is to set the current argument number to
   m. The current argument number is incremented after processing a directive.
 */

enum format_arg_type
{
  FAT_NONE,
  FAT_CHARACTER,
  FAT_INTEGER,
  FAT_FLOAT,
  FAT_OBJECT_PRETTY,
  FAT_OBJECT
};

struct numbered_arg
{
  unsigned int number;
  enum format_arg_type type;
};

struct spec
{
  unsigned int directives;
  unsigned int numbered_arg_count;
  unsigned int allocated;
  struct numbered_arg *numbered;
};

/* Locale independent test for a decimal digit.
   Argument can be  'char' or 'unsigned char'.  (Whereas the argument of
   <ctype.h> isdigit must be an 'unsigned char'.)  */
#undef isdigit
#define isdigit(c) ((unsigned int) ((c) - '0') < 10)


static int
numbered_arg_compare (const void *p1, const void *p2)
{
  unsigned int n1 = ((const struct numbered_arg *) p1)->number;
  unsigned int n2 = ((const struct numbered_arg *) p2)->number;

  return (n1 > n2 ? 1 : n1 < n2 ? -1 : 0);
}

static void *
format_parse (const char *format, bool translated, char **invalid_reason)
{
  struct spec spec;
  struct spec *result;
  unsigned int number;

  spec.directives = 0;
  spec.numbered_arg_count = 0;
  spec.allocated = 0;
  spec.numbered = NULL;
  number = 1;

  for (; *format != '\0';)
    if (*format++ == '%')
      {
	/* A directive.  */
	enum format_arg_type type;

	spec.directives++;

	if (isdigit (*format))
	  {
	    const char *f = format;
	    unsigned int m = 0;

	    do
	      {
		m = 10 * m + (*f - '0');
		f++;
	      }
	    while (isdigit (*f));

	    if (*f == '$' && m > 0)
	      {
		number = m;
		format = ++f;
	      }
	  }

	/* Parse flags.  */
	while (*format == ' ' || *format == '+' || *format == '-'
	       || *format == '#' || *format == '0')
	  format++;

	/* Parse width.  */
	if (*format == '*')
	  {
	    format++;

	    if (spec.allocated == spec.numbered_arg_count)
	      {
		spec.allocated = 2 * spec.allocated + 1;
		spec.numbered = (struct numbered_arg *) xrealloc (spec.numbered, spec.allocated * sizeof (struct numbered_arg));
	      }
	    spec.numbered[spec.numbered_arg_count].number = number;
	    spec.numbered[spec.numbered_arg_count].type = FAT_INTEGER;
	    spec.numbered_arg_count++;

	    number++;
	  }
	else if (isdigit (*format))
	  {
	    do format++; while (isdigit (*format));
	  }

	/* Parse precision.  */
	if (*format == '.')
	  {
	    format++;

	    if (*format == '*')
	      {
		format++;

		if (spec.allocated == spec.numbered_arg_count)
		  {
		    spec.allocated = 2 * spec.allocated + 1;
		    spec.numbered = (struct numbered_arg *) xrealloc (spec.numbered, spec.allocated * sizeof (struct numbered_arg));
		  }
		spec.numbered[spec.numbered_arg_count].number = number;
		spec.numbered[spec.numbered_arg_count].type = FAT_INTEGER;
		spec.numbered_arg_count++;

		number++;
	      }
	    else if (isdigit (*format))
	      {
		do format++; while (isdigit (*format));
	      }
	  }

	switch (*format)
	  {
	  case '%':
	    type = FAT_NONE;
	    break;
	  case 'c':
	    type = FAT_CHARACTER;
	    break;
	  case 'd': case 'i': case 'x': case 'X': case 'o':
	    type = FAT_INTEGER;
	    break;
	  case 'e': case 'E': case 'f': case 'g': case 'G':
	    type = FAT_FLOAT;
	    break;
	  case 's':
	    type = FAT_OBJECT_PRETTY;
	    break;
	  case 'S':
	    type = FAT_OBJECT;
	    break;
	  default:
	    *invalid_reason =
	      (*format == '\0'
	       ? INVALID_UNTERMINATED_DIRECTIVE ()
	       : INVALID_CONVERSION_SPECIFIER (spec.directives, *format));
	    goto bad_format;
	  }

	if (type != FAT_NONE)
	  {
	    if (spec.allocated == spec.numbered_arg_count)
	      {
		spec.allocated = 2 * spec.allocated + 1;
		spec.numbered = (struct numbered_arg *) xrealloc (spec.numbered, spec.allocated * sizeof (struct numbered_arg));
	      }
	    spec.numbered[spec.numbered_arg_count].number = number;
	    spec.numbered[spec.numbered_arg_count].type = type;
	    spec.numbered_arg_count++;

	    number++;
	  }

	format++;
      }

  /* Sort the numbered argument array, and eliminate duplicates.  */
  if (spec.numbered_arg_count > 1)
    {
      unsigned int i, j;
      bool err;

      qsort (spec.numbered, spec.numbered_arg_count,
	     sizeof (struct numbered_arg), numbered_arg_compare);

      /* Remove duplicates: Copy from i to j, keeping 0 <= j <= i.  */
      err = false;
      for (i = j = 0; i < spec.numbered_arg_count; i++)
	if (j > 0 && spec.numbered[i].number == spec.numbered[j-1].number)
	  {
	    enum format_arg_type type1 = spec.numbered[i].type;
	    enum format_arg_type type2 = spec.numbered[j-1].type;
	    enum format_arg_type type_both;

	    if (type1 == type2)
	      type_both = type1;
	    else
	      {
		/* Incompatible types.  */
		type_both = FAT_NONE;
		if (!err)
		  *invalid_reason =
		    INVALID_INCOMPATIBLE_ARG_TYPES (spec.numbered[i].number);
		err = true;
	      }

	    spec.numbered[j-1].type = type_both;
	  }
	else
	  {
	    if (j < i)
	      {
		spec.numbered[j].number = spec.numbered[i].number;
		spec.numbered[j].type = spec.numbered[i].type;
	      }
	    j++;
	  }
      spec.numbered_arg_count = j;
      if (err)
	/* *invalid_reason has already been set above.  */
	goto bad_format;
    }

  result = (struct spec *) xmalloc (sizeof (struct spec));
  *result = spec;
  return result;

 bad_format:
  if (spec.numbered != NULL)
    free (spec.numbered);
  return NULL;
}

static void
format_free (void *descr)
{
  struct spec *spec = (struct spec *) descr;

  if (spec->numbered != NULL)
    free (spec->numbered);
  free (spec);
}

static int
format_get_number_of_directives (void *descr)
{
  struct spec *spec = (struct spec *) descr;

  return spec->directives;
}

static bool
format_check (void *msgid_descr, void *msgstr_descr, bool equality,
	      formatstring_error_logger_t error_logger,
	      const char *pretty_msgstr)
{
  struct spec *spec1 = (struct spec *) msgid_descr;
  struct spec *spec2 = (struct spec *) msgstr_descr;
  bool err = false;

  if (spec1->numbered_arg_count + spec2->numbered_arg_count > 0)
    {
      unsigned int i, j;
      unsigned int n1 = spec1->numbered_arg_count;
      unsigned int n2 = spec2->numbered_arg_count;

      /* Check the argument names are the same.
	 Both arrays are sorted.  We search for the first difference.  */
      for (i = 0, j = 0; i < n1 || j < n2; )
	{
	  int cmp = (i >= n1 ? 1 :
		     j >= n2 ? -1 :
		     spec1->numbered[i].number > spec2->numbered[j].number ? 1 :
		     spec1->numbered[i].number < spec2->numbered[j].number ? -1 :
		     0);

	  if (cmp > 0)
	    {
	      if (error_logger)
		error_logger (_("a format specification for argument %u, as in '%s', doesn't exist in 'msgid'"),
			      spec2->numbered[j].number, pretty_msgstr);
	      err = true;
	      break;
	    }
	  else if (cmp < 0)
	    {
	      if (equality)
		{
		  if (error_logger)
		    error_logger (_("a format specification for argument %u doesn't exist in '%s'"),
				  spec1->numbered[i].number, pretty_msgstr);
		  err = true;
		  break;
		}
	      else
		i++;
	    }
	  else
	    j++, i++;
	}
      /* Check the argument types are the same.  */
      if (!err)
	for (i = 0, j = 0; j < n2; )
	  {
	    if (spec1->numbered[i].number == spec2->numbered[j].number)
	      {
		if (spec1->numbered[i].type != spec2->numbered[j].type)
		  {
		    if (error_logger)
		      error_logger (_("format specifications in 'msgid' and '%s' for argument %u are not the same"),
				    pretty_msgstr, spec2->numbered[j].number);
		    err = true;
		    break;
		  }
		j++, i++;
	      }
	    else
	      i++;
	  }
    }

  return err;
}


struct formatstring_parser formatstring_elisp =
{
  format_parse,
  format_free,
  format_get_number_of_directives,
  format_check
};


#ifdef TEST

/* Test program: Print the argument list specification returned by
   format_parse for strings read from standard input.  */

#include <stdio.h>
#include "getline.h"

static void
format_print (void *descr)
{
  struct spec *spec = (struct spec *) descr;
  unsigned int last;
  unsigned int i;

  if (spec == NULL)
    {
      printf ("INVALID");
      return;
    }

  printf ("(");
  last = 1;
  for (i = 0; i < spec->numbered_arg_count; i++)
    {
      unsigned int number = spec->numbered[i].number;

      if (i > 0)
	printf (" ");
      if (number < last)
	abort ();
      for (; last < number; last++)
	printf ("_ ");
      switch (spec->numbered[i].type)
	{
	case FAT_CHARACTER:
	  printf ("c");
	  break;
	case FAT_INTEGER:
	  printf ("i");
	  break;
	case FAT_FLOAT:
	  printf ("f");
	  break;
	case FAT_OBJECT_PRETTY:
	  printf ("s");
	  break;
	case FAT_OBJECT:
	  printf ("*");
	  break;
	default:
	  abort ();
	}
      last = number + 1;
    }
  printf (")");
}

int
main ()
{
  for (;;)
    {
      char *line = NULL;
      size_t line_size = 0;
      int line_len;
      char *invalid_reason;
      void *descr;

      line_len = getline (&line, &line_size, stdin);
      if (line_len < 0)
	break;
      if (line_len > 0 && line[line_len - 1] == '\n')
	line[--line_len] = '\0';

      invalid_reason = NULL;
      descr = format_parse (line, false, &invalid_reason);

      format_print (descr);
      printf ("\n");
      if (descr == NULL)
	printf ("%s\n", invalid_reason);

      free (invalid_reason);
      free (line);
    }

  return 0;
}

/*
 * For Emacs M-x compile
 * Local Variables:
 * compile-command: "/bin/sh ../libtool --mode=link gcc -o a.out -static -O -g -Wall -I.. -I../lib -I../intl -DHAVE_CONFIG_H -DTEST format-elisp.c ../lib/libgettextlib.la"
 * End:
 */

#endif /* TEST */
