/* xgettext Perl backend.
   Copyright (C) 2002-2004 Free Software Foundation, Inc.

   This file was written by Guido Flohr <guido@imperia.net>, 2002-2003.

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
# include "config.h"
#endif

#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "message.h"
#include "xgettext.h"
#include "x-perl.h"
#include "error.h"
#include "error-progname.h"
#include "xalloc.h"
#include "exit.h"
#include "po-charset.h"
#include "ucs4-utf8.h"
#include "uniname.h"
#include "getline.h"
#include "gettext.h"

#define _(s) gettext(s)

/* The Perl syntax is defined in perlsyn.pod.  Try the command
   "man perlsyn" or "perldoc perlsyn".
   Also, the syntax after the 'sub' keyword is specified in perlsub.pod.
   Try the command "man perlsub" or "perldoc perlsub".  */

#define DEBUG_PERL 0


/* ====================== Keyword set customization.  ====================== */

/* If true extract all strings.  */
static bool extract_all = false;

static hash_table keywords;
static bool default_keywords = true;


void
x_perl_extract_all ()
{
  extract_all = true;
}


void
x_perl_keyword (const char *name)
{
  if (name == NULL)
    default_keywords = false;
  else
    {
      const char *end;
      int argnum1;
      int argnum2;
      const char *colon;

      if (keywords.table == NULL)
	init_hash (&keywords, 100);

      split_keywordspec (name, &end, &argnum1, &argnum2);

      /* The characters between name and end should form a valid C identifier.
	 A colon means an invalid parse in split_keywordspec().  */
      colon = strchr (name, ':');
      if (colon == NULL || colon >= end)
	{
	  if (argnum1 == 0)
	    argnum1 = 1;
	  insert_entry (&keywords, name, end - name,
			(void *) (long) (argnum1 + (argnum2 << 10)));
	}
    }
}

/* Finish initializing the keywords hash table.
   Called after argument processing, before each file is processed.  */
static void
init_keywords ()
{
  if (default_keywords)
    {
      x_perl_keyword ("gettext");
      x_perl_keyword ("%gettext");
      x_perl_keyword ("$gettext");
      x_perl_keyword ("dgettext:2");
      x_perl_keyword ("dcgettext:2");
      x_perl_keyword ("ngettext:1,2");
      x_perl_keyword ("dngettext:2,3");
      x_perl_keyword ("dcngettext:2,3");
      x_perl_keyword ("gettext_noop");
#if 0
      x_perl_keyword ("__");
      x_perl_keyword ("$__");
      x_perl_keyword ("%__");
      x_perl_keyword ("__x");
      x_perl_keyword ("__n:1,2");
      x_perl_keyword ("__nx:1,2");
      x_perl_keyword ("__xn:1,2");
      x_perl_keyword ("N__");
#endif
      default_keywords = false;
    }
}

void
init_flag_table_perl ()
{
  xgettext_record_flag ("gettext:1:pass-perl-format");
  xgettext_record_flag ("gettext:1:pass-perl-brace-format");
  xgettext_record_flag ("%gettext:1:pass-perl-format");
  xgettext_record_flag ("%gettext:1:pass-perl-brace-format");
  xgettext_record_flag ("$gettext:1:pass-perl-format");
  xgettext_record_flag ("$gettext:1:pass-perl-brace-format");
  xgettext_record_flag ("dgettext:2:pass-perl-format");
  xgettext_record_flag ("dgettext:2:pass-perl-brace-format");
  xgettext_record_flag ("dcgettext:2:pass-perl-format");
  xgettext_record_flag ("dcgettext:2:pass-perl-brace-format");
  xgettext_record_flag ("ngettext:1:pass-perl-format");
  xgettext_record_flag ("ngettext:2:pass-perl-format");
  xgettext_record_flag ("ngettext:1:pass-perl-brace-format");
  xgettext_record_flag ("ngettext:2:pass-perl-brace-format");
  xgettext_record_flag ("dngettext:2:pass-perl-format");
  xgettext_record_flag ("dngettext:3:pass-perl-format");
  xgettext_record_flag ("dngettext:2:pass-perl-brace-format");
  xgettext_record_flag ("dngettext:3:pass-perl-brace-format");
  xgettext_record_flag ("dcngettext:2:pass-perl-format");
  xgettext_record_flag ("dcngettext:3:pass-perl-format");
  xgettext_record_flag ("dcngettext:2:pass-perl-brace-format");
  xgettext_record_flag ("dcngettext:3:pass-perl-brace-format");
  xgettext_record_flag ("gettext_noop:1:pass-perl-format");
  xgettext_record_flag ("gettext_noop:1:pass-perl-brace-format");
  xgettext_record_flag ("printf:1:perl-format"); /* argument 1 or 2 ?? */
  xgettext_record_flag ("sprintf:1:perl-format");
#if 0
  xgettext_record_flag ("__:1:pass-perl-format");
  xgettext_record_flag ("__:1:pass-perl-brace-format");
  xgettext_record_flag ("%__:1:pass-perl-format");
  xgettext_record_flag ("%__:1:pass-perl-brace-format");
  xgettext_record_flag ("$__:1:pass-perl-format");
  xgettext_record_flag ("$__:1:pass-perl-brace-format");
  xgettext_record_flag ("__x:1:perl-brace-format");
  xgettext_record_flag ("__n:1:pass-perl-format");
  xgettext_record_flag ("__n:2:pass-perl-format");
  xgettext_record_flag ("__n:1:pass-perl-brace-format");
  xgettext_record_flag ("__n:2:pass-perl-brace-format");
  xgettext_record_flag ("__nx:1:perl-brace-format");
  xgettext_record_flag ("__nx:2:perl-brace-format");
  xgettext_record_flag ("__xn:1:perl-brace-format");
  xgettext_record_flag ("__xn:2:perl-brace-format");
  xgettext_record_flag ("N__:1:pass-perl-format");
  xgettext_record_flag ("N__:1:pass-perl-brace-format");
#endif
}


/* ======================== Reading of characters.  ======================== */

/* Real filename, used in error messages about the input file.  */
static const char *real_file_name;

/* Logical filename and line number, used to label the extracted messages.  */
static char *logical_file_name;
static int line_number;

/* The input file stream.  */
static FILE *fp;

/* The current line buffer.  */
static char *linebuf;

/* The size of the current line.  */
static int linesize;

/* The position in the current line.  */
static int linepos;

/* The size of the input buffer.  */
static size_t linebuf_size;

/* Number of lines eaten for here documents.  */
static int here_eaten;

/* Paranoia: EOF marker for __END__ or __DATA__.  */
static bool end_of_file;


/* 1. line_number handling.  */

/* Returns the next character from the input stream or EOF.  */
static int
phase1_getc ()
{
  line_number += here_eaten;
  here_eaten = 0;

  if (end_of_file)
    return EOF;

  if (linepos >= linesize)
    {
      linesize = getline (&linebuf, &linebuf_size, fp);

      if (linesize < 0)
	{
	  if (ferror (fp))
	    error (EXIT_FAILURE, errno, _("error while reading \"%s\""),
		   real_file_name);
	  end_of_file = true;
	  return EOF;
	}

      linepos = 0;
      ++line_number;

      /* Undosify.  This is important for catching the end of <<EOF and
	 <<'EOF'.  We could rely on stdio doing this for us but you
	 it is not uncommon to to come across Perl scripts with CRLF
	 newline conventions on systems that do not follow this
	 convention.  */
      if (linesize >= 2 && linebuf[linesize - 1] == '\n'
	  && linebuf[linesize - 2] == '\r')
	{
	  linebuf[linesize - 2] = '\n';
	  linebuf[linesize - 1] = '\0';
	  --linesize;
	}
    }

  return linebuf[linepos++];
}

/* Supports only one pushback character.  */
static void
phase1_ungetc (int c)
{
  if (c != EOF)
    {
      if (linepos == 0)
	/* Attempt to ungetc across line boundary.  Shouldn't happen.
	   No two phase1_ungetc calls are permitted in a row.  */
	abort ();

      --linepos;
    }
}

/* Read a here document and return its contents.
   The delimiter is an UTF-8 encoded string; the resulting string is UTF-8
   encoded as well.  */

static char *
get_here_document (const char *delimiter)
{
  /* Accumulator for the entire here document, including a NUL byte
     at the end.  */
  static char *buffer;
  static size_t bufmax = 0;
  size_t bufpos = 0;
  /* Current line being appended.  */
  static char *my_linebuf = NULL;
  static size_t my_linebuf_size = 0;

  /* Allocate the initial buffer.  Later on, bufmax > 0.  */
  if (bufmax == 0)
    {
      buffer = xrealloc (NULL, 1);
      buffer[0] = '\0';
      bufmax = 1;
    }

  for (;;)
    {
      int read_bytes = getline (&my_linebuf, &my_linebuf_size, fp);
      char *my_line_utf8;
      bool chomp;

      if (read_bytes < 0)
	{
	  if (ferror (fp))
	    {
	      error (EXIT_FAILURE, errno, _("error while reading \"%s\""),
		     real_file_name);
	    }
	  else
	    {
	      error_with_progname = false;
	      error (EXIT_SUCCESS, 0, _("\
%s:%d: can't find string terminator \"%s\" anywhere before EOF"),
		     real_file_name, line_number, delimiter);
	      error_with_progname = true;

	      break;
	    }
	}

      ++here_eaten;

      /* Convert to UTF-8.  */
      my_line_utf8 =
	from_current_source_encoding (my_linebuf, logical_file_name,
				      line_number + here_eaten);
      if (my_line_utf8 != my_linebuf)
	{
	  if (strlen (my_line_utf8) >= my_linebuf_size)
	    {
	      my_linebuf_size = strlen (my_line_utf8) + 1;
	      my_linebuf = xrealloc (my_linebuf, my_linebuf_size);
	    }
	  strcpy (my_linebuf, my_line_utf8);
	  free (my_line_utf8);
	}

      /* Undosify.  This is important for catching the end of <<EOF and
	 <<'EOF'.  We could rely on stdio doing this for us but you
	 it is not uncommon to to come across Perl scripts with CRLF
	 newline conventions on systems that do not follow this
	 convention.  */
      if (read_bytes >= 2 && my_linebuf[read_bytes - 1] == '\n'
	  && my_linebuf[read_bytes - 2] == '\r')
	{
	  my_linebuf[read_bytes - 2] = '\n';
	  my_linebuf[read_bytes - 1] = '\0';
	  --read_bytes;
	}

      /* Temporarily remove the trailing newline from my_linebuf.  */
      chomp = false;
      if (read_bytes >= 1 && my_linebuf[read_bytes - 1] == '\n')
	{
	  chomp = true;
	  my_linebuf[read_bytes - 1] = '\0';
	}

      /* See whether this line terminates the here document.  */
      if (strcmp (my_linebuf, delimiter) == 0)
	break;

      /* Add back the trailing newline to my_linebuf.  */
      if (chomp)
	my_linebuf[read_bytes - 1] = '\n';

      /* Ensure room for read_bytes + 1 bytes.  */
      if (bufpos + read_bytes >= bufmax)
	{
	  do
	    bufmax = 2 * bufmax + 10;
	  while (bufpos + read_bytes >= bufmax);
	  buffer = xrealloc (buffer, bufmax);
	}
      /* Append this line to the accumulator.  */
      strcpy (buffer + bufpos, my_linebuf);
      bufpos += read_bytes;
    }

  /* Done accumulating the here document.  */
  return xstrdup (buffer);
}

/* Skips pod sections.  */
static void
skip_pod ()
{
  line_number += here_eaten;
  here_eaten = 0;
  linepos = 0;

  for (;;)
    {
      linesize = getline (&linebuf, &linebuf_size, fp);

      if (linesize < 0)
	{
	  if (ferror (fp))
	    error (EXIT_FAILURE, errno, _("error while reading \"%s\""),
		   real_file_name);
	  return;
	}

      ++line_number;

      if (strncmp ("=cut", linebuf, 4) == 0)
	{
	  /* Force reading of a new line on next call to phase1_getc().  */
	  linepos = linesize;
	  return;
	}
    }
}


/* These are for tracking whether comments count as immediately before
   keyword.  */
static int last_comment_line;
static int last_non_comment_line;


/* 2. Replace each comment that is not inside a string literal or regular
   expression with a newline character.  We need to remember the comment
   for later, because it may be attached to a keyword string.  */

static int
phase2_getc ()
{
  static char *buffer;
  static size_t bufmax;
  size_t buflen;
  int lineno;
  int c;
  char *utf8_string;

  c = phase1_getc ();
  if (c == '#')
    {
      buflen = 0;
      lineno = line_number;
      /* Skip leading whitespace.  */
      for (;;)
	{
	  c = phase1_getc ();
	  if (c == EOF)
	    break;
	  if (c != ' ' && c != '\t' && c != '\r' && c != '\f')
	    {
	      phase1_ungetc (c);
	      break;
	    }
	}
      /* Accumulate the comment.  */
      for (;;)
	{
	  c = phase1_getc ();
	  if (c == '\n' || c == EOF)
	    break;
	  if (buflen >= bufmax)
	    {
	      bufmax = 2 * bufmax + 10;
	      buffer = xrealloc (buffer, bufmax);
	    }
	  buffer[buflen++] = c;
	}
      if (buflen >= bufmax)
	{
	  bufmax = 2 * bufmax + 10;
	  buffer = xrealloc (buffer, bufmax);
	}
      buffer[buflen] = '\0';
      /* Convert it to UTF-8.  */
      utf8_string =
	from_current_source_encoding (buffer, logical_file_name, lineno);
      /* Save it until we encounter the corresponding string.  */
      xgettext_current_source_encoding = po_charset_utf8;
      xgettext_comment_add (utf8_string);
      xgettext_current_source_encoding = xgettext_global_source_encoding;
      last_comment_line = lineno;
    }
  return c;
}

/* Supports only one pushback character.  */
static void
phase2_ungetc (int c)
{
  if (c != EOF)
    phase1_ungetc (c);
}

/* Whitespace recognition.  */

#define case_whitespace \
  case ' ': case '\t': case '\r': case '\n': case '\f'

static inline bool
is_whitespace (int c)
{
  return (c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\f');
}


/* ========================== Reading of tokens.  ========================== */


enum token_type_ty
{
  token_type_eof,
  token_type_lparen,		/* ( */
  token_type_rparen,		/* ) */
  token_type_comma,		/* , */
  token_type_fat_comma,		/* => */
  token_type_dereference,	/* , */
  token_type_semicolon,         /* ; */
  token_type_lbrace,            /* { */
  token_type_rbrace,            /* } */
  token_type_lbracket,          /* [ */
  token_type_rbracket,          /* ] */
  token_type_string,		/* quote-like */
  token_type_named_op,          /* if, unless, while, ... */
  token_type_variable,          /* $... */
  token_type_symbol,		/* symbol, number */
  token_type_regex_op,		/* s, tr, y, m.  */
  token_type_dot,               /* . */
  token_type_other,		/* regexp, misc. operator */
  /* The following are not really token types, but variants used by
     the parser.  */
  token_type_keyword_symbol	/* keyword symbol */
};
typedef enum token_type_ty token_type_ty;

/* Subtypes for strings, important for interpolation.  */
enum string_type_ty
{
  string_type_verbatim,     /* "<<'EOF'", "m'...'", "s'...''...'",
			       "tr/.../.../", "y/.../.../".  */
  string_type_q,            /* "'..'", "q/.../".  */
  string_type_qq,           /* '"..."', "`...`", "qq/.../", "qx/.../",
			       "<file*glob>".  */
  string_type_qr            /* Not supported.  */
};

/* Subtypes for symbols, important for dollar interpretation.  */
enum symbol_type_ty
{
  symbol_type_none,         /* Nothing special.  */
  symbol_type_sub,          /* 'sub'.  */
  symbol_type_function      /* Function name after 'sub'.  */
};

typedef struct token_ty token_ty;
struct token_ty
{
  token_type_ty type;
  int sub_type;			/* for token_type_string, token_type_symbol */
  char *string;			/* for:			in encoding:
				   token_type_named_op	ASCII
				   token_type_string	UTF-8
				   token_type_symbol	ASCII
				   token_type_variable	global_source_encoding
				 */
  int line_number;
};

#if DEBUG_PERL
static const char *
token2string (const token_ty *token)
{
  switch (token->type)
    {
    case token_type_eof:
      return "token_type_eof";
    case token_type_lparen:
      return "token_type_lparen";
    case token_type_rparen:
      return "token_type_rparen";
    case token_type_comma:
      return "token_type_comma";
    case token_type_fat_comma:
      return "token_type_fat_comma";
    case token_type_dereference:
      return "token_type_dereference";
    case token_type_semicolon:
      return "token_type_semicolon";
    case token_type_lbrace:
      return "token_type_lbrace";
    case token_type_rbrace:
      return "token_type_rbrace";
    case token_type_lbracket:
      return "token_type_lbracket";
    case token_type_rbracket:
      return "token_type_rbracket";
    case token_type_string:
      return "token_type_string";
    case token_type_named_op:
      return "token_type_named_op";
    case token_type_variable:
      return "token_type_variable";
    case token_type_symbol:
      return "token_type_symbol";
    case token_type_regex_op:
      return "token_type_regex_op";
    case token_type_dot:
      return "token_type_dot";
    case token_type_other:
      return "token_type_other";
    default:
      return "unknown";
    }
}
#endif

/* Free the memory pointed to by a 'struct token_ty'.  */
static inline void
free_token (token_ty *tp)
{
  switch (tp->type)
    {
    case token_type_named_op:
    case token_type_string:
    case token_type_symbol:
    case token_type_variable:
      free (tp->string);
      break;
    default:
      break;
    }
  free (tp);
}

/* Pass 1 of extracting quotes: Find the end of the string, regardless
   of the semantics of the construct.  Return the complete string,
   including the starting and the trailing delimiter, with backslashes
   removed where appropriate.  */
static char *
extract_quotelike_pass1 (int delim)
{
  /* This function is called recursively.  No way to allocate stuff
     statically.  Also alloca() is inappropriate due to limited stack
     size on some platforms.  So we use malloc().  */
  int bufmax = 10;
  char *buffer = (char *) xmalloc (bufmax);
  int bufpos = 0;
  bool nested = true;
  int counter_delim;

  buffer[bufpos++] = delim;

  /* Find the closing delimiter.  */
  switch (delim)
    {
    case '(':
      counter_delim = ')';
      break;
    case '{':
      counter_delim = '}';
      break;
    case '[':
      counter_delim = ']';
      break;
    case '<':
      counter_delim = '>';
      break;
    default: /* "..." or '...' or |...| etc. */
      nested = false;
      counter_delim = delim;
      break;
    }

  for (;;)
    {
      int c = phase1_getc ();

      /* This round can produce 1 or 2 bytes.  Ensure room for 2 bytes.  */
      if (bufpos + 2 > bufmax)
	{
	  bufmax = 2 * bufmax + 10;
	  buffer = xrealloc (buffer, bufmax);
	}

      if (c == counter_delim || c == EOF)
	{
	  buffer[bufpos++] = counter_delim; /* will be stripped off later */
	  buffer[bufpos++] = '\0';
#if DEBUG_PERL
	  fprintf (stderr, "PASS1: %s\n", buffer);
#endif
	  return buffer;
	}

      if (nested && c == delim)
	{
	  char *inner = extract_quotelike_pass1 (delim);
	  size_t len = strlen (inner);

	  /* Ensure room for len + 1 bytes.  */
	  if (bufpos + len >= bufmax)
	    {
	      do
		bufmax = 2 * bufmax + 10;
	      while (bufpos + len >= bufmax);
	      buffer = xrealloc (buffer, bufmax);
	    }
	  strcpy (buffer + bufpos, inner);
	  free (inner);
	  bufpos += len;
	}
      else if (c == '\\')
	{
	  c = phase1_getc ();
	  if (c == '\\')
	    {
	      buffer[bufpos++] = '\\';
	      buffer[bufpos++] = '\\';
	    }
	  else if (c == delim || c == counter_delim)
	    {
	      /* This is pass2 in Perl.  */
	      buffer[bufpos++] = c;
	    }
	  else
	    {
	      buffer[bufpos++] = '\\';
	      phase1_ungetc (c);
	    }
	}
      else
	{
	  buffer[bufpos++] = c;
	}
    }
}

/* Like extract_quotelike_pass1, but return the complete string in UTF-8
   encoding.  */
static char *
extract_quotelike_pass1_utf8 (int delim)
{
  char *string = extract_quotelike_pass1 (delim);
  char *utf8_string =
    from_current_source_encoding (string, logical_file_name, line_number);
  if (utf8_string != string)
    free (string);
  return utf8_string;
}


/* ========= Reading of tokens and commands.  Extracting strings.  ========= */


/* There is an ambiguity about '/': It can start a division operator ('/' or
   '/=') or it can start a regular expression.  The distinction is important
   because inside regular expressions, '#' loses its special meaning.
   The distinction is possible depending on the parsing state: After a
   variable or simple expression, it's a division operator; at the beginning
   of an expression, it's a regexp.  */
static bool prefer_division_over_regexp;

/* Context lookup table.  */
static flag_context_list_table_ty *flag_context_list_table;


/* Forward declaration of local functions.  */
static void interpolate_keywords (message_list_ty *mlp, const char *string,
				  int lineno);
static token_ty *x_perl_lex (message_list_ty *mlp);
static void x_perl_unlex (token_ty *tp);
static bool extract_balanced (message_list_ty *mlp, int state,
			      token_type_ty delim,
			      flag_context_ty outer_context,
			      flag_context_list_iterator_ty context_iter,
			      int arg_sg, int arg_pl);


/* Extract an unsigned hexadecimal number from STRING, considering at
   most LEN bytes and place the result in *RESULT.  Returns a pointer
   to the first character past the hexadecimal number.  */
static const char *
extract_hex (const char *string, size_t len, unsigned int *result)
{
  size_t i;

  *result = 0;

  for (i = 0; i < len; i++)
    {
      char c = string[i];
      int number;

      if (c >= 'A' && c <= 'F')
	number = c - 'A' + 10;
      else if (c >= 'a' && c <= 'f')
	number = c - 'a' + 10;
      else if (c >= '0' && c <= '9')
	number = c - '0';
      else
	break;

      *result <<= 4;
      *result |= number;
    }

  return string + i;
}

/* Extract an unsigned octal number from STRING, considering at
   most LEN bytes and place the result in *RESULT.  Returns a pointer
   to the first character past the octal number.  */
static const char *
extract_oct (const char *string, size_t len, unsigned int *result)
{
  size_t i;

  *result = 0;

  for (i = 0; i < len; i++)
    {
      char c = string[i];
      int number;

      if (c >= '0' && c <= '7')
	number = c - '0';
      else
	break;

      *result <<= 3;
      *result |= number;
    }

  return string + i;
}

/* Extract the various quotelike constructs except for <<EOF.  See the
   section "Gory details of parsing quoted constructs" in perlop.pod.
   Return the resulting token in *tp; tp->type == token_type_string.  */
static void
extract_quotelike (token_ty *tp, int delim)
{
  char *string = extract_quotelike_pass1_utf8 (delim);
  size_t len = strlen (string);

  tp->type = token_type_string;
  /* Take the string without the delimiters at the start and at the end.  */
  if (!(len >= 2))
    abort ();
  string[len - 1] = '\0';
  tp->string = xstrdup (string + 1);
  free (string);
}

/* Extract the quotelike constructs with double delimiters, like
   s/[SEARCH]/[REPLACE]/.  This function does not eat up trailing
   modifiers (left to the caller).
   Return the resulting token in *tp; tp->type == token_type_regex_op.  */
static void
extract_triple_quotelike (message_list_ty *mlp, token_ty *tp, int delim,
			  bool interpolate)
{
  char *string;

  tp->type = token_type_regex_op;

  string = extract_quotelike_pass1_utf8 (delim);
  if (interpolate)
    interpolate_keywords (mlp, string, line_number);
  free (string);

  if (delim == '(' || delim == '<' || delim == '{' || delim == '[')
    {
      /* The delimiter for the second string can be different, e.g.
	 s{SEARCH}{REPLACE} or s{SEARCH}/REPLACE/.  See "man perlrequick".  */
      delim = phase1_getc ();
      while (is_whitespace (delim))
	{
	  /* The hash-sign is not a valid delimiter after whitespace, ergo
	     use phase2_getc() and not phase1_getc() now.  */
	  delim = phase2_getc ();
	}
    }
  string = extract_quotelike_pass1_utf8 (delim);
  if (interpolate)
    interpolate_keywords (mlp, string, line_number);
  free (string);
}

/* Perform pass 3 of quotelike extraction (interpolation).
   *tp is a token of type token_type_string.
   This function replaces tp->string.  */
/* FIXME: Currently may writes null-bytes into the string.  */
static void
extract_quotelike_pass3 (token_ty *tp, int error_level)
{
  static char *buffer;
  static int bufmax = 0;
  int bufpos = 0;
  const char *crs;
  bool uppercase;
  bool lowercase;
  bool quotemeta;

#if DEBUG_PERL
  switch (tp->sub_type)
    {
    case string_type_verbatim:
      fprintf (stderr, "Interpolating string_type_verbatim:\n");
      break;
    case string_type_q:
      fprintf (stderr, "Interpolating string_type_q:\n");
      break;
    case string_type_qq:
      fprintf (stderr, "Interpolating string_type_qq:\n");
      break;
    case string_type_qr:
      fprintf (stderr, "Interpolating string_type_qr:\n");
      break;
    }
  fprintf (stderr, "%s\n", tp->string);
  if (tp->sub_type == string_type_verbatim)
    fprintf (stderr, "---> %s\n", tp->string);
#endif

  if (tp->sub_type == string_type_verbatim)
    return;

  /* Loop over tp->string, accumulating the expansion in buffer.  */
  crs = tp->string;
  uppercase = false;
  lowercase = false;
  quotemeta = false;
  while (*crs)
    {
      bool backslashed;

      /* Ensure room for 7 bytes, 6 (multi-)bytes plus a leading backslash
	 if \Q modifier is present.  */
      if (bufpos + 7 > bufmax)
	{
	  bufmax = 2 * bufmax + 10;
	  buffer = xrealloc (buffer, bufmax);
	}

      if (tp->sub_type == string_type_q)
	{
	  switch (*crs)
	    {
	    case '\\':
	      if (crs[1] == '\\')
		{
		  crs += 2;
		  buffer[bufpos++] = '\\';
		  break;
		}
	      /* FALLTHROUGH */
	    default:
	      buffer[bufpos++] = *crs++;
	      break;
	    }
	  continue;
	}

      /* We only get here for double-quoted strings or regular expressions.
	 Unescape escape sequences.  */
      if (*crs == '\\')
	{
	  switch (crs[1])
	    {
	    case 't':
	      crs += 2;
	      buffer[bufpos++] = '\t';
	      continue;
	    case 'n':
	      crs += 2;
	      buffer[bufpos++] = '\n';
	      continue;
	    case 'r':
	      crs += 2;
	      buffer[bufpos++] = '\r';
	      continue;
	    case 'f':
	      crs += 2;
	      buffer[bufpos++] = '\f';
	      continue;
	    case 'b':
	      crs += 2;
	      buffer[bufpos++] = '\b';
	      continue;
	    case 'a':
	      crs += 2;
	      buffer[bufpos++] = '\a';
	      continue;
	    case 'e':
	      crs += 2;
	      buffer[bufpos++] = 0x1b;
	      continue;
	    case '0': case '1': case '2': case '3':
	    case '4': case '5': case '6': case '7':
	      {
		unsigned int oct_number;
		int length;

		crs = extract_oct (crs + 1, 3, &oct_number);

		/* FIXME: If one of the variables UPPERCASE or LOWERCASE is
		   true, the character should be converted to its uppercase
		   resp. lowercase equivalent.  I don't know if the necessary
		   facilities are already included in gettext.  For US-Ascii
		   the conversion can be already be done, however.  */
		if (uppercase && oct_number >= 'a' && oct_number <= 'z')
		  {
		    oct_number = oct_number - 'a' + 'A';
		  }
		else if (lowercase && oct_number >= 'A' && oct_number <= 'Z')
		  {
		    oct_number = oct_number - 'A' + 'a';
		  }


		/* Yes, octal escape sequences in the range 0x100..0x1ff are
		   valid.  */
		length = u8_uctomb ((unsigned char *) (buffer + bufpos),
				    oct_number, 2);
		if (length > 0)
		  bufpos += length;
	      }
	      continue;
	    case 'x':
	      {
		unsigned int hex_number = 0;
		int length;

		crs += 2;
		if (*crs == '{')
		  {
		    const char *end = strchr (crs, '}');
		    if (end == NULL)
		      {
			error_with_progname = false;
			error (error_level, 0, _("\
%s:%d: missing right brace on \\x{HEXNUMBER}"), real_file_name, line_number);
			error_with_progname = true;
			++crs;
			continue;
		      }
		    else
		      {
			++crs;
			(void) extract_hex (crs, end - crs, &hex_number);
			crs = end + 1;
		      }
		  }
		else
		  {
		    crs = extract_hex (crs, 2, &hex_number);
		  }

		/* FIXME: If one of the variables UPPERCASE or LOWERCASE is
		   true, the character should be converted to its uppercase
		   resp. lowercase equivalent.  I don't know if the necessary
		   facilities are already included in gettext.  For US-Ascii
		   the conversion can be already be done, however.  */
		if (uppercase && hex_number >= 'a' && hex_number <= 'z')
		  {
		    hex_number = hex_number - 'a' + 'A';
		  }
		else if (lowercase && hex_number >= 'A' && hex_number <= 'Z')
		  {
		    hex_number = hex_number - 'A' + 'a';
		  }

		length = u8_uctomb ((unsigned char *) (buffer + bufpos),
				    hex_number, 6);

		if (length > 0)
		  bufpos += length;
	      }
	      continue;
	    case 'c':
	      /* Perl's notion of control characters.  */
	      crs += 2;
	      if (*crs)
		{
		  int the_char = (unsigned char) *crs;
		  if (the_char >= 'a' || the_char <= 'z')
		    the_char = the_char - 'a' + 'A';
		  buffer[bufpos++] = the_char ^ 0x40;
		}
	      continue;
	    case 'N':
	      crs += 2;
	      if (*crs == '{')
		{
		  const char *end = strchr (crs + 1, '}');
		  if (end != NULL)
		    {
		      char *name;
		      unsigned int unicode;

		      name = (char *) xmalloc (end - (crs + 1) + 1);
		      memcpy (name, crs + 1, end - (crs + 1));
		      name[end - (crs + 1)] = '\0';

		      unicode = unicode_name_character (name);
		      if (unicode != UNINAME_INVALID)
			{
			  /* FIXME: Convert to upper/lowercase if the
			     corresponding flag is set to true.  */
			  int length =
			    u8_uctomb ((unsigned char *) (buffer + bufpos),
				       unicode, 6);
			  if (length > 0)
			    bufpos += length;
			}

		      free (name);

		      crs = end + 1;
		    }
		}
	      continue;
	    }
	}

      /* No escape sequence, go on.  */
      if (*crs == '\\')
	{
	  ++crs;
	  switch (*crs)
	    {
	    case 'E':
	      uppercase = false;
	      lowercase = false;
	      quotemeta = false;
	      ++crs;
	      continue;
	    case 'L':
	      uppercase = false;
	      lowercase = true;
	      ++crs;
	      continue;
	    case 'U':
	      uppercase = true;
	      lowercase = false;
	      ++crs;
	      continue;
	    case 'Q':
	      quotemeta = true;
	      ++crs;
	      continue;
	    case 'l':
	      ++crs;
	      if (*crs >= 'A' && *crs <= 'Z')
		{
		  buffer[bufpos++] = *crs - 'A' + 'a';
		}
	      else if ((unsigned char) *crs >= 0x80)
		{
		  error_with_progname = false;
		  error (error_level, 0, _("\
%s:%d: invalid interpolation (\"\\l\") of 8bit character \"%c\""),
			 real_file_name, line_number, *crs);
		  error_with_progname = true;
		}
	      else
	        {
		  buffer[bufpos++] = *crs;
		}
	      ++crs;
	      continue;
	    case 'u':
	      ++crs;
	      if (*crs >= 'a' && *crs <= 'z')
		{
		  buffer[bufpos++] = *crs - 'a' + 'A';
		}
	      else if ((unsigned char) *crs >= 0x80)
		{
		  error_with_progname = false;
		  error (error_level, 0, _("\
%s:%d: invalid interpolation (\"\\u\") of 8bit character \"%c\""),
			 real_file_name, line_number, *crs);
		  error_with_progname = true;
		}
	      else
	        {
		  buffer[bufpos++] = *crs;
		}
	      ++crs;
	      continue;
	    case '\\':
	      buffer[bufpos++] = *crs;
	      ++crs;
	      continue;
	    default:
	      backslashed = true;
	      break;
	    }
	}
      else
	backslashed = false;

      if (quotemeta
	  && !((*crs >= 'A' && *crs <= 'Z') || (*crs >= 'A' && *crs <= 'z')
	       || (*crs >= '0' && *crs <= '9') || *crs == '_'))
	{
	  buffer[bufpos++] = '\\';
	  backslashed = true;
	}

      if (!backslashed && !extract_all && (*crs == '$' || *crs == '@'))
	{
	  error_with_progname = false;
	  error (error_level, 0, _("\
%s:%d: invalid variable interpolation at \"%c\""),
		 real_file_name, line_number, *crs);
	  error_with_progname = true;
	  ++crs;
	}
      else if (lowercase)
	{
	  if (*crs >= 'A' && *crs <= 'Z')
	    buffer[bufpos++] = *crs - 'A' + 'a';
	  else if ((unsigned char) *crs >= 0x80)
	    {
	      error_with_progname = false;
	      error (error_level, 0, _("\
%s:%d: invalid interpolation (\"\\L\") of 8bit character \"%c\""),
		     real_file_name, line_number, *crs);
	      error_with_progname = true;
	      buffer[bufpos++] = *crs;
	    }
	  else
	    buffer[bufpos++] = *crs;
	  ++crs;
	}
      else if (uppercase)
	{
	  if (*crs >= 'a' && *crs <= 'z')
	    buffer[bufpos++] = *crs - 'a' + 'A';
	  else if ((unsigned char) *crs >= 0x80)
	    {
	      error_with_progname = false;
	      error (error_level, 0, _("\
%s:%d: invalid interpolation (\"\\U\") of 8bit character \"%c\""),
		     real_file_name, line_number, *crs);
	      error_with_progname = true;
	      buffer[bufpos++] = *crs;
	    }
	  else
	    buffer[bufpos++] = *crs;
	  ++crs;
	}
      else
	{
	  buffer[bufpos++] = *crs++;
	}
    }

  /* Ensure room for 1 more byte.  */
  if (bufpos >= bufmax)
    {
      bufmax = 2 * bufmax + 10;
      buffer = xrealloc (buffer, bufmax);
    }

  buffer[bufpos++] = '\0';

#if DEBUG_PERL
  fprintf (stderr, "---> %s\n", buffer);
#endif

  /* Replace tp->string.  */
  free (tp->string);
  tp->string = xstrdup (buffer);
}

/* Parse a variable.  This is done in several steps:
     1) Consume all leading occurencies of '$', '@', '%', and '*'.
     2) Determine the name of the variable from the following input.
     3) Parse possible following hash keys or array indexes.
 */
static void
extract_variable (message_list_ty *mlp, token_ty *tp, int first)
{
  static char *buffer;
  static int bufmax = 0;
  int bufpos = 0;
  int c = first;
  size_t varbody_length = 0;
  bool maybe_hash_deref = false;
  bool maybe_hash_value = false;

  tp->type = token_type_variable;

#if DEBUG_PERL
  fprintf (stderr, "%s:%d: extracting variable type '%c'\n",
	   real_file_name, line_number, first);
#endif

  /*
   * 1) Consume dollars and so on (not euros ...).  Unconditionally
   *    accepting the hash sign (#) will maybe lead to inaccurate
   *    results.  FIXME!
   */
  while (c == '$' || c == '*' || c == '#' || c == '@' || c == '%')
    {
      if (bufpos >= bufmax)
	{
	  bufmax = 2 * bufmax + 10;
	  buffer = xrealloc (buffer, bufmax);
	}
      buffer[bufpos++] = c;
      c = phase1_getc ();
    }

  if (c == EOF)
    {
      tp->type = token_type_eof;
      return;
    }

  /* Hash references are treated in a special way, when looking for
     our keywords.  */
  if (buffer[0] == '$')
    {
      if (bufpos == 1)
	maybe_hash_value = true;
      else if (bufpos == 2 && buffer[1] == '$')
	{
	  if (!(c == '{'
		|| (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')
		|| (c >= '0' && c <= '9')
		|| c == '_' || c == ':' || c == '\'' || c >= 0x80))
	    {
	      /* Special variable $$ for pid.  */
	      if (bufpos >= bufmax)
		{
		  bufmax = 2 * bufmax + 10;
		  buffer = xrealloc (buffer, bufmax);
		}
	      buffer[bufpos++] = '\0';
	      tp->string = xstrdup (buffer);
#if DEBUG_PERL
	      fprintf (stderr, "%s:%d: is PID ($$)\n",
		       real_file_name, line_number);
#endif

	      phase1_ungetc (c);
	      return;
	    }

	  maybe_hash_deref = true;
	  bufpos = 1;
	}
    }

  /*
   * 2) Get the name of the variable.  The first character is practically
   *    arbitrary.  Punctuation and numbers automagically put a variable
   *    in the global namespace but that subtle difference is not interesting
   *    for us.
   */
  if (bufpos >= bufmax)
    {
      bufmax = 2 * bufmax + 10;
      buffer = xrealloc (buffer, bufmax);
    }
  if (c == '{')
    {
      /* Yuck, we cannot accept ${gettext} as a keyword...  Except for
       * debugging purposes it is also harmless, that we suppress the
       * real name of the variable.
       */
#if DEBUG_PERL
      fprintf (stderr, "%s:%d: braced {variable_name}\n",
	       real_file_name, line_number);
#endif

      if (extract_balanced (mlp, 0, token_type_rbrace,
			    null_context, null_context_list_iterator, -1, -1))
	return;
      buffer[bufpos++] = c;
    }
  else
    {
      while ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')
	     || (c >= '0' && c <= '9')
	     || c == '_' || c == ':' || c == '\'' || c >= 0x80)
	{
	  ++varbody_length;
	  if (bufpos >= bufmax)
	    {
	      bufmax = 2 * bufmax + 10;
	      buffer = xrealloc (buffer, bufmax);
	    }
	  buffer[bufpos++] = c;
	  c = phase1_getc ();
	}
      phase1_ungetc (c);
    }

  /* Probably some strange Perl variable like $`.  */
  if (varbody_length == 0)
    {
      c = phase1_getc ();
      if (c == EOF || is_whitespace (c))
	phase1_ungetc (c);  /* Loser.  */
      else
	{
	  if (bufpos >= bufmax)
	    {
	      bufmax = 2 * bufmax + 10;
	      buffer = xrealloc (buffer, bufmax);
	    }
	  buffer[bufpos++] = c;
	}
    }

  if (bufpos >= bufmax)
    {
      bufmax = 2 * bufmax + 10;
      buffer = xrealloc (buffer, bufmax);
    }
  buffer[bufpos++] = '\0';

  tp->string = xstrdup (buffer);

#if DEBUG_PERL
  fprintf (stderr, "%s:%d: complete variable name: %s\n",
	   real_file_name, line_number, tp->string);
#endif

  prefer_division_over_regexp = true;

  /*
   * 3) If the following looks strange to you, this is valid Perl syntax:
   *
   *      $var = $$hashref    # We can place a
   *                          # comment here and then ...
   *             {key_into_hashref};
   *
   *    POD sections are not allowed but we leave complaints about
   *    that to the compiler/interpreter.
   */
  /* We only extract strings from the first hash key (if present).  */

  if (maybe_hash_deref || maybe_hash_value)
    {
      bool is_dereference = false;
      int c;

      do
	c = phase2_getc ();
      while (is_whitespace (c));

      if (c == '-')
	{
	  int c2 = phase1_getc ();

	  if (c2 == '>')
	    {
	      is_dereference = true;

	      do
		c = phase2_getc ();
	      while (is_whitespace (c));
	    }
	  else if (c2 != '\n')
	    {
	      /* Discarding the newline is harmless here.  The only
		 special character recognized after a minus is greater-than
		 for dereference.  However, the sequence "-\n>" that we
	         treat incorrectly here, is a syntax error.  */
	      phase1_ungetc (c2);
	    }
	}

      if (maybe_hash_value && is_dereference)
	{
#if DEBUG_PERL
	  fprintf (stderr, "%s:%d: first keys preceded by \"->\"\n",
		   real_file_name, line_number);
#endif
	}
      else if (maybe_hash_value)
	{
	  /* Fake it into a hash.  */
	  tp->string[0] = '%';
	}

      /* Do NOT change that into else if (see above).  */
      if ((maybe_hash_value || maybe_hash_deref) && c == '{')
	{
	  void *keyword_value;

#if DEBUG_PERL
	  fprintf (stderr, "%s:%d: first keys preceded by '{'\n",
		   real_file_name, line_number);
#endif

	  if (find_entry (&keywords, tp->string, strlen (tp->string),
			  &keyword_value) == 0)
	    {
	      /* Extract a possible string from the key.  Before proceeding
		 we check whether the open curly is followed by a symbol and
		 then by a right curly.  */
	      flag_context_list_iterator_ty context_iter =
		flag_context_list_iterator (
		  flag_context_list_table_lookup (
		    flag_context_list_table,
		    tp->string, strlen (tp->string)));
	      token_ty *t1 = x_perl_lex (mlp);

#if DEBUG_PERL
	      fprintf (stderr, "%s:%d: extracting string key\n",
		       real_file_name, line_number);
#endif

	      if (t1->type == token_type_symbol
		  || t1->type == token_type_named_op)
		{
		  token_ty *t2 = x_perl_lex (mlp);
		  if (t2->type == token_type_rbrace)
		    {
		      flag_context_ty context;
		      lex_pos_ty pos;

		      context =
			inherited_context (null_context,
					   flag_context_list_iterator_advance (
					     &context_iter));

		      pos.line_number = line_number;
		      pos.file_name = logical_file_name;

		      xgettext_current_source_encoding = po_charset_utf8;
		      remember_a_message (mlp, xstrdup (t1->string), context, &pos);
		      xgettext_current_source_encoding = xgettext_global_source_encoding;
		      free_token (t2);
		      free_token (t1);
		    }
		  else
		    {
		      x_perl_unlex (t2);
		    }
		}
	      else
		{
		  x_perl_unlex (t1);
		  if (extract_balanced (mlp, 1, token_type_rbrace,
					null_context, context_iter, 1, -1))
		    return;
		}
	    }
	  else
	    {
	      phase2_ungetc (c);
	    }
	}
      else
	{
	  phase2_ungetc (c);
	}
    }

  /* Now consume "->", "[...]", and "{...}".  */
  for (;;)
    {
      int c = phase2_getc ();
      int c2;

      switch (c)
	{
	case '{':
#if DEBUG_PERL
	  fprintf (stderr, "%s:%d: extracting balanced '{' after varname\n",
		   real_file_name, line_number);
#endif
	  extract_balanced (mlp, 0, token_type_rbrace,
			    null_context, null_context_list_iterator, -1, -1);
	  break;

	case '[':
#if DEBUG_PERL
	  fprintf (stderr, "%s:%d: extracting balanced '[' after varname\n",
		   real_file_name, line_number);
#endif
	  extract_balanced (mlp, 0, token_type_rbracket,
			    null_context, null_context_list_iterator, -1, -1);
	  break;

	case '-':
	  c2 = phase1_getc ();
	  if (c2 == '>')
	    {
#if DEBUG_PERL
	      fprintf (stderr, "%s:%d: another \"->\" after varname\n",
		       real_file_name, line_number);
#endif
	      break;
	    }
	  else if (c2 != '\n')
	    {
	      /* Discarding the newline is harmless here.  The only
		 special character recognized after a minus is greater-than
		 for dereference.  However, the sequence "-\n>" that we
	         treat incorrectly here, is a syntax error.  */
	      phase1_ungetc (c2);
	    }
	  /* FALLTHROUGH */

	default:
#if DEBUG_PERL
	  fprintf (stderr, "%s:%d: variable finished\n",
		   real_file_name, line_number);
#endif
	  phase2_ungetc (c);
	  return;
	}
    }
}

/* Actually a simplified version of extract_variable().  It searches for
   variables inside a double-quoted string that may interpolate to
   some keyword hash (reference).  The string is UTF-8 encoded.  */
static void
interpolate_keywords (message_list_ty *mlp, const char *string, int lineno)
{
  static char *buffer;
  static int bufmax = 0;
  int bufpos = 0;
  flag_context_ty context;
  int c;
  bool maybe_hash_deref = false;
  enum parser_state
    {
      initial,
      one_dollar,
      two_dollars,
      identifier,
      minus,
      wait_lbrace,
      wait_quote,
      dquote,
      squote,
      barekey,
      wait_rbrace
    } state;
  token_ty token;

  lex_pos_ty pos;

  /* States are:
   *
   * initial:      initial
   * one_dollar:   dollar sign seen in state INITIAL
   * two_dollars:  another dollar-sign has been seen in state ONE_DOLLAR
   * identifier:   a valid identifier character has been seen in state
   *               ONE_DOLLAR or TWO_DOLLARS
   * minus:        a minus-sign has been seen in state IDENTIFIER
   * wait_lbrace:  a greater-than has been seen in state MINUS
   * wait_quote:   a left brace has been seen in state IDENTIFIER or in
   *               state WAIT_LBRACE
   * dquote:       a double-quote has been seen in state WAIT_QUOTE
   * squote:       a single-quote has been seen in state WAIT_QUOTE
   * barekey:      an bareword character has been seen in state WAIT_QUOTE
   * wait_rbrace:  closing quote has been seen in state DQUOTE or SQUOTE
   *
   * In the states initial...identifier the context is null_context; in the
   * states minus...wait_rbrace the context is the one suitable for the first
   * argument of the last seen identifier.
   */
  state = initial;
  context = null_context;

  token.type = token_type_string;
  token.sub_type = string_type_qq;
  token.line_number = line_number;
  pos.file_name = logical_file_name;
  pos.line_number = lineno;

  while ((c = (unsigned char) *string++) != '\0')
    {
      void *keyword_value;

      if (state == initial)
	bufpos = 0;

      if (c == '\n')
	lineno++;

      if (bufpos + 1 >= bufmax)
	{
	  bufmax = 2 * bufmax + 10;
	  buffer = xrealloc (buffer, bufmax);
	}

      switch (state)
	{
	case initial:
	  switch (c)
	    {
	    case '\\':
	      c = (unsigned char) *string++;
	      if (c == '\0')
		return;
	      break;
	    case '$':
	      buffer[bufpos++] = '$';
	      maybe_hash_deref = false;
	      state = one_dollar;
	      break;
	    default:
	      break;
	    }
	  break;
	case one_dollar:
	  switch (c)
	    {
	    case '$':
	      /*
	       * This is enough to make us believe later that we dereference
	       * a hash reference.
	       */
	      maybe_hash_deref = true;
	      state = two_dollars;
	      break;
	    default:
	      if (c == '_' || c == ':' || c == '\'' || c >= 0x80
		  || (c >= 'A' && c <= 'Z')
		  || (c >= 'a' && c <= 'z')
		  || (c >= '0' && c <= '9'))
		{
		  buffer[bufpos++] = c;
		  state = identifier;
		}
	      else
		state = initial;
	      break;
	    }
	  break;
	case two_dollars:
	  if (c == '_' || c == ':' || c == '\'' || c >= 0x80
	      || (c >= 'A' && c <= 'Z')
	      || (c >= 'a' && c <= 'z')
	      || (c >= '0' && c <= '9'))
	    {
	      buffer[bufpos++] = c;
	      state = identifier;
	    }
	  else
	    state = initial;
	  break;
	case identifier:
	  switch (c)
	    {
	    case '-':
	      if (find_entry (&keywords, buffer, bufpos, &keyword_value) == 0)
		{
		  flag_context_list_iterator_ty context_iter =
		    flag_context_list_iterator (
		      flag_context_list_table_lookup (
			flag_context_list_table,
			buffer, bufpos));
		  context =
		    inherited_context (null_context,
				       flag_context_list_iterator_advance (
					 &context_iter));
		  state = minus;
		}
	      else
		state = initial;
	      break;
	    case '{':
	      if (!maybe_hash_deref)
		buffer[0] = '%';
	      if (find_entry (&keywords, buffer, bufpos, &keyword_value) == 0)
		{
		  flag_context_list_iterator_ty context_iter =
		    flag_context_list_iterator (
		      flag_context_list_table_lookup (
			flag_context_list_table,
			buffer, bufpos));
		  context =
		    inherited_context (null_context,
				       flag_context_list_iterator_advance (
					 &context_iter));
		  state = wait_quote;
		}
	      else
		state = initial;
	      break;
	    default:
	      if (c == '_' || c == ':' || c == '\'' || c >= 0x80
		  || (c >= 'A' && c <= 'Z')
		  || (c >= 'a' && c <= 'z')
		  || (c >= '0' && c <= '9'))
		{
		  buffer[bufpos++] = c;
		}
	      else
		state = initial;
	      break;
	    }
	  break;
	case minus:
	  switch (c)
	    {
	    case '>':
	      state = wait_lbrace;
	      break;
	    default:
	      context = null_context;
	      state = initial;
	      break;
	    }
	  break;
	case wait_lbrace:
	  switch (c)
	    {
	    case '{':
	      state = wait_quote;
	      break;
	    default:
	      context = null_context;
	      state = initial;
	      break;
	    }
	  break;
	case wait_quote:
	  switch (c)
	    {
	    case_whitespace:
	      break;
	    case '\'':
	      pos.line_number = lineno;
	      bufpos = 0;
	      state = squote;
	      break;
	    case '"':
	      pos.line_number = lineno;
	      bufpos = 0;
	      state = dquote;
	      break;
	    default:
	      if (c == '_' || (c >= '0' && c <= '9') || c >= 0x80
		  || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z'))
		{
		  pos.line_number = lineno;
		  bufpos = 0;
		  buffer[bufpos++] = c;
		  state = barekey;
		}
	      else
		{
		  context = null_context;
		  state = initial;
		}
	      break;
	    }
	  break;
	case dquote:
	  switch (c)
	    {
	    case '"':
	      /* The resulting string has to be interpolated twice.  */
	      buffer[bufpos] = '\0';
	      token.string = xstrdup (buffer);
	      extract_quotelike_pass3 (&token, EXIT_FAILURE);
	      /* The string can only shrink with interpolation (because
		 we ignore \Q).  */
	      if (!(strlen (token.string) <= bufpos))
		abort ();
	      strcpy (buffer, token.string);
	      free (token.string);
	      state = wait_rbrace;
	      break;
	    case '\\':
	      if (string[0] == '\"')
		{
		  buffer[bufpos++] = string++[0];
		}
	      else if (string[0])
		{
		  buffer[bufpos++] = '\\';
		  buffer[bufpos++] = string++[0];
		}
	      else
		{
		  context = null_context;
		  state = initial;
		}
	      break;
	    default:
	      buffer[bufpos++] = c;
	      break;
	    }
	  break;
	case squote:
	  switch (c)
	    {
	    case '\'':
	      state = wait_rbrace;
	      break;
	    case '\\':
	      if (string[0] == '\'')
		{
		  buffer[bufpos++] = string++[0];
		}
	      else if (string[0])
		{
		  buffer[bufpos++] = '\\';
		  buffer[bufpos++] = string++[0];
		}
	      else
		{
		  context = null_context;
		  state = initial;
		}
	      break;
	    default:
	      buffer[bufpos++] = c;
	      break;
	    }
	  break;
	case barekey:
	  if (c == '_' || (c >= '0' && c <= '9') || c >= 0x80
	      || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z'))
	    {
	      buffer[bufpos++] = c;
	      break;
	    }
	  else if (is_whitespace (c))
	    {
	      state = wait_rbrace;
	      break;
	    }
	  else if (c != '}')
	    {
	      context = null_context;
	      state = initial;
	      break;
	    }
	  /* Must be right brace.  */
	  /* FALLTHROUGH */
	case wait_rbrace:
	  switch (c)
	    {
	    case_whitespace:
	      break;
	    case '}':
	      buffer[bufpos] = '\0';
	      token.string = xstrdup (buffer);
	      extract_quotelike_pass3 (&token, EXIT_FAILURE);
	      xgettext_current_source_encoding = po_charset_utf8;
	      remember_a_message (mlp, token.string, context, &pos);
	      xgettext_current_source_encoding = xgettext_global_source_encoding;
	      /* FALLTHROUGH */
	    default:
	      context = null_context;
	      state = initial;
	      break;
	    }
	  break;
	}
    }
}

/* The last token seen in the token stream.  This is important for the
   interpretation of '?' and '/'.  */
static token_type_ty last_token;

/* Combine characters into tokens.  Discard whitespace.  */

static void
x_perl_prelex (message_list_ty *mlp, token_ty *tp)
{
  static char *buffer;
  static int bufmax;
  int bufpos;
  int c;

  for (;;)
    {
      c = phase2_getc ();
      tp->line_number = line_number;

      switch (c)
	{
	case EOF:
	  tp->type = token_type_eof;
	  return;

	case '\n':
	  if (last_non_comment_line > last_comment_line)
	    xgettext_comment_reset ();
	  /* FALLTHROUGH */
	case '\t':
	case ' ':
	  /* Ignore whitespace.  */
	  continue;

	case '%':
	case '@':
	case '*':
	case '$':
	  if (!extract_all)
	    {
	      extract_variable (mlp, tp, c);
	      prefer_division_over_regexp = true;
	      return;
	    }
	  break;
	}

      last_non_comment_line = tp->line_number;

      switch (c)
	{
	case '.':
	  {
	    int c2 = phase1_getc ();
	    phase1_ungetc (c2);
	    if (c2 == '.')
	      {
		tp->type = token_type_other;
		prefer_division_over_regexp = false;
		return;
	      }
	    else if (c2 >= '0' && c2 <= '9')
	      {
		prefer_division_over_regexp = false;
	      }
	    else
	      {
		tp->type = token_type_dot;
		prefer_division_over_regexp = true;
		return;
	      }
	  }
	  /* FALLTHROUGH */
	case 'A': case 'B': case 'C': case 'D': case 'E': case 'F':
	case 'G': case 'H': case 'I': case 'J': case 'K': case 'L':
	case 'M': case 'N': case 'O': case 'P': case 'Q': case 'R':
	case 'S': case 'T': case 'U': case 'V': case 'W': case 'X':
	case 'Y': case 'Z':
	case '_':
	case 'a': case 'b': case 'c': case 'd': case 'e': case 'f':
	case 'g': case 'h': case 'i': case 'j': case 'k': case 'l':
	case 'm': case 'n': case 'o': case 'p': case 'q': case 'r':
	case 's': case 't': case 'u': case 'v': case 'w': case 'x':
	case 'y': case 'z':
	case '0': case '1': case '2': case '3': case '4':
	case '5': case '6': case '7': case '8': case '9':
	  /* Symbol, or part of a number.  */
	  prefer_division_over_regexp = true;
	  bufpos = 0;
	  for (;;)
	    {
	      if (bufpos >= bufmax)
		{
		  bufmax = 2 * bufmax + 10;
		  buffer = xrealloc (buffer, bufmax);
		}
	      buffer[bufpos++] = c;
	      c = phase1_getc ();
	      switch (c)
		{
		case 'A': case 'B': case 'C': case 'D': case 'E': case 'F':
		case 'G': case 'H': case 'I': case 'J': case 'K': case 'L':
		case 'M': case 'N': case 'O': case 'P': case 'Q': case 'R':
		case 'S': case 'T': case 'U': case 'V': case 'W': case 'X':
		case 'Y': case 'Z':
		case '_':
		case 'a': case 'b': case 'c': case 'd': case 'e': case 'f':
		case 'g': case 'h': case 'i': case 'j': case 'k': case 'l':
		case 'm': case 'n': case 'o': case 'p': case 'q': case 'r':
		case 's': case 't': case 'u': case 'v': case 'w': case 'x':
		case 'y': case 'z':
		case '0': case '1': case '2': case '3': case '4':
		case '5': case '6': case '7': case '8': case '9':
		  continue;

		default:
		  phase1_ungetc (c);
		  break;
		}
	      break;
	    }
	  if (bufpos >= bufmax)
	    {
	      bufmax = 2 * bufmax + 10;
	      buffer = xrealloc (buffer, bufmax);
	    }
	  buffer[bufpos] = '\0';

	  if (strcmp (buffer, "__END__") == 0
	      || strcmp (buffer, "__DATA__") == 0)
	    {
	      end_of_file = true;
	      tp->type = token_type_eof;
	      return;
	    }
	  else if (strcmp (buffer, "and") == 0
		   || strcmp (buffer, "cmp") == 0
		   || strcmp (buffer, "eq") == 0
		   || strcmp (buffer, "if") == 0
		   || strcmp (buffer, "ge") == 0
		   || strcmp (buffer, "gt") == 0
		   || strcmp (buffer, "le") == 0
		   || strcmp (buffer, "lt") == 0
		   || strcmp (buffer, "ne") == 0
		   || strcmp (buffer, "not") == 0
		   || strcmp (buffer, "or") == 0
		   || strcmp (buffer, "unless") == 0
		   || strcmp (buffer, "while") == 0
		   || strcmp (buffer, "xor") == 0)
	    {
	      tp->type = token_type_named_op;
	      tp->string = xstrdup (buffer);
	      prefer_division_over_regexp = false;
	      return;
	    }
	  else if (strcmp (buffer, "s") == 0
		 || strcmp (buffer, "y") == 0
		 || strcmp (buffer, "tr") == 0)
	    {
	      int delim = phase1_getc ();

	      while (is_whitespace (delim))
		delim = phase2_getc ();

	      if (delim == EOF)
		{
		  tp->type = token_type_eof;
		  return;
		}
	      if ((delim >= '0' && delim <= '9')
		  || (delim >= 'A' && delim <= 'Z')
		  || (delim >= 'a' && delim <= 'z'))
		{
		  /* False positive.  */
		  phase2_ungetc (delim);
		  tp->type = token_type_symbol;
		  tp->sub_type = symbol_type_none;
		  tp->string = xstrdup (buffer);
		  prefer_division_over_regexp = true;
		  return;
		}
	      extract_triple_quotelike (mlp, tp, delim,
					buffer[0] == 's' && delim != '\'');

	      /* Eat the following modifiers.  */
	      do
		c = phase1_getc ();
	      while (c >= 'a' && c <= 'z');
	      phase1_ungetc (c);
	      return;
	    }
	  else if (strcmp (buffer, "m") == 0)
	    {
	      int delim = phase1_getc ();

	      while (is_whitespace (delim))
		delim = phase2_getc ();

	      if (delim == EOF)
		{
		  tp->type = token_type_eof;
		  return;
		}
	      if ((delim >= '0' && delim <= '9')
		  || (delim >= 'A' && delim <= 'Z')
		  || (delim >= 'a' && delim <= 'z'))
		{
		  /* False positive.  */
		  phase2_ungetc (delim);
		  tp->type = token_type_symbol;
		  tp->sub_type = symbol_type_none;
		  tp->string = xstrdup (buffer);
		  prefer_division_over_regexp = true;
		  return;
		}
	      extract_quotelike (tp, delim);
	      if (delim != '\'')
		interpolate_keywords (mlp, tp->string, line_number);
	      free (tp->string);
	      tp->type = token_type_regex_op;
	      prefer_division_over_regexp = true;

	      /* Eat the following modifiers.  */
	      do
		c = phase1_getc ();
	      while (c >= 'a' && c <= 'z');
	      phase1_ungetc (c);
	      return;
	    }
	  else if (strcmp (buffer, "qq") == 0
		   || strcmp (buffer, "q") == 0
		   || strcmp (buffer, "qx") == 0
		   || strcmp (buffer, "qw") == 0
		   || strcmp (buffer, "qr") == 0)
	    {
	      /* The qw (...) construct is not really a string but we
		 can treat in the same manner and then pretend it is
		 a symbol.  Rationale: Saying "qw (foo bar)" is the
		 same as "my @list = ('foo', 'bar'); @list;".  */

	      int delim = phase1_getc ();

	      while (is_whitespace (delim))
		delim = phase2_getc ();

	      if (delim == EOF)
		{
		  tp->type = token_type_eof;
		  return;
		}
	      prefer_division_over_regexp = true;

	      if ((delim >= '0' && delim <= '9')
		  || (delim >= 'A' && delim <= 'Z')
		  || (delim >= 'a' && delim <= 'z'))
		{
		  /* False positive.  */
		  phase2_ungetc (delim);
		  tp->type = token_type_symbol;
		  tp->sub_type = symbol_type_none;
		  tp->string = xstrdup (buffer);
		  prefer_division_over_regexp = true;
		  return;
		}

	      extract_quotelike (tp, delim);

	      switch (buffer[1])
		{
		case 'q':
		case 'x':
		  tp->type = token_type_string;
		  tp->sub_type = string_type_qq;
		  interpolate_keywords (mlp, tp->string, line_number);
		  break;
		case 'r':
		  tp->type = token_type_regex_op;
		  break;
		case 'w':
		  tp->type = token_type_symbol;
		  tp->sub_type = symbol_type_none;
		  break;
		case '\0':
		  tp->type = token_type_string;
		  tp->sub_type = string_type_q;
		  break;
		default:
		  abort ();
		}
	      return;
	    }
	  else if (strcmp (buffer, "grep") == 0
		   || strcmp (buffer, "split") == 0)
	    {
	      prefer_division_over_regexp = false;
	    }
	  tp->type = token_type_symbol;
	  tp->sub_type = (strcmp (buffer, "sub") == 0
			  ? symbol_type_sub
			  : symbol_type_none);
	  tp->string = xstrdup (buffer);
	  return;

	case '"':
	  prefer_division_over_regexp = true;
	  extract_quotelike (tp, c);
	  tp->sub_type = string_type_qq;
	  interpolate_keywords (mlp, tp->string, line_number);
	  return;

	case '`':
	  prefer_division_over_regexp = true;
	  extract_quotelike (tp, c);
	  tp->sub_type = string_type_qq;
	  interpolate_keywords (mlp, tp->string, line_number);
	  return;

	case '\'':
	  prefer_division_over_regexp = true;
	  extract_quotelike (tp, c);
	  tp->sub_type = string_type_q;
	  return;

	case '(':
	  c = phase2_getc ();
	  if (c == ')')
	    /* Ignore empty list.  */
	    continue;
	  else
	    phase2_ungetc (c);
	  tp->type = token_type_lparen;
	  prefer_division_over_regexp = false;
	  return;

	case ')':
	  tp->type = token_type_rparen;
	  prefer_division_over_regexp = true;
	  return;

	case '{':
	  tp->type = token_type_lbrace;
	  prefer_division_over_regexp = false;
	  return;

	case '}':
	  tp->type = token_type_rbrace;
	  prefer_division_over_regexp = false;
	  return;

	case '[':
	  tp->type = token_type_lbracket;
	  prefer_division_over_regexp = false;
	  return;

	case ']':
	  tp->type = token_type_rbracket;
	  prefer_division_over_regexp = false;
	  return;

	case ';':
	  tp->type = token_type_semicolon;
	  prefer_division_over_regexp = false;
	  return;

	case ',':
	  tp->type = token_type_comma;
	  prefer_division_over_regexp = false;
	  return;

	case '=':
	  /* Check for fat comma.  */
	  c = phase1_getc ();
	  if (c == '>')
	    {
	      tp->type = token_type_fat_comma;
	      return;
	    }
	  else if (linepos == 2
		   && (last_token == token_type_semicolon
		       || last_token == token_type_rbrace)
		   && ((c >= 'A' && c <='Z')
		       || (c >= 'a' && c <= 'z')))
	    {
#if DEBUG_PERL
	      fprintf (stderr, "%s:%d: start pod section\n",
		       real_file_name, line_number);
#endif
	      skip_pod ();
#if DEBUG_PERL
	      fprintf (stderr, "%s:%d: end pod section\n",
		       real_file_name, line_number);
#endif
	      continue;
	    }
	  phase1_ungetc (c);
	  tp->type = token_type_other;
	  prefer_division_over_regexp = false;
	  return;

	case '<':
	  /* Check for <<EOF and friends.  */
	  prefer_division_over_regexp = false;
	  c = phase1_getc ();
	  if (c == '<')
	    {
	      c = phase1_getc ();
	      if (c == '\'')
		{
		  char *string;
		  extract_quotelike (tp, c);
		  string = get_here_document (tp->string);
		  free (tp->string);
		  tp->string = string;
		  tp->type = token_type_string;
		  tp->sub_type = string_type_verbatim;
		  tp->line_number = line_number + 1;
		  return;
		}
	      else if (c == '"')
		{
		  char *string;
		  extract_quotelike (tp, c);
		  string = get_here_document (tp->string);
		  free (tp->string);
		  tp->string = string;
		  tp->type = token_type_string;
		  tp->sub_type = string_type_qq;
		  tp->line_number = line_number + 1;
		  interpolate_keywords (mlp, tp->string, line_number + 1);
		  return;
		}
	      else if ((c >= 'A' && c <= 'Z')
		       || (c >= 'a' && c <= 'z')
		       || c == '_')
		{
		  bufpos = 0;
		  while ((c >= 'A' && c <= 'Z')
			 || (c >= 'a' && c <= 'z')
			 || (c >= '0' && c <= '9')
			 || c == '_' || c >= 0x80)
		    {
		      if (bufpos >= bufmax)
			{
			  bufmax = 2 * bufmax + 10;
			  buffer = xrealloc (buffer, bufmax);
			}
		      buffer[bufpos++] = c;
		      c = phase1_getc ();
		    }
		  if (c == EOF)
		    {
		      tp->type = token_type_eof;
		      return;
		    }
		  else
		    {
		      char *string;
		      phase1_ungetc (c);
		      if (bufpos >= bufmax)
			{
			  bufmax = 2 * bufmax + 10;
			  buffer = xrealloc (buffer, bufmax);
			}
		      buffer[bufpos++] = '\0';
		      string = get_here_document (buffer);
		      tp->string = string;
		      tp->type = token_type_string;
		      tp->sub_type = string_type_qq;
		      tp->line_number = line_number + 1;
		      interpolate_keywords (mlp, tp->string, line_number + 1);
		      return;
		    }
		}
	      else
		{
		  tp->type = token_type_other;
		  return;
		}
	    }
	  else
	    {
	      phase1_ungetc (c);
	      tp->type = token_type_other;
	    }
	  return;  /* End of case '>'.  */

	case '-':
	  /* Check for dereferencing operator.  */
	  c = phase1_getc ();
	  if (c == '>')
	    {
	      tp->type = token_type_dereference;
	      return;
	    }
	  else if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z'))
	    {
	      /* One of the -X (filetest) functions.  We play safe
		 and accept all alphabetical characters here.  */
	      tp->type = token_type_other;
	      return;
	    }
	  phase1_ungetc (c);
	  tp->type = token_type_other;
	  prefer_division_over_regexp = false;
	  return;

	case '/':
	case '?':
	  if (!prefer_division_over_regexp)
	    {
	      extract_quotelike (tp, c);
	      interpolate_keywords (mlp, tp->string, line_number);
	      free (tp->string);
	      tp->type = token_type_other;
	      prefer_division_over_regexp = true;
	      /* Eat the following modifiers.  */
	      do
		c = phase1_getc ();
	      while (c >= 'a' && c <= 'z');
	      phase1_ungetc (c);
	      return;
	    }
	  /* FALLTHROUGH */

	default:
	  /* We could carefully recognize each of the 2 and 3 character
	     operators, but it is not necessary, as we only need to recognize
	     gettext invocations.  Don't bother.  */
	  tp->type = token_type_other;
	  prefer_division_over_regexp = false;
	  return;
	}
    }
}


/* A token stack used as a lookahead buffer.  */

typedef struct token_stack_ty token_stack_ty;
struct token_stack_ty
{
  token_ty **items;
  size_t nitems;
  size_t nitems_max;
};

static struct token_stack_ty token_stack;

#if DEBUG_PERL
/* Dumps all resources allocated by stack STACK.  */
static int
token_stack_dump (token_stack_ty *stack)
{
  size_t i;

  fprintf (stderr, "BEGIN STACK DUMP\n");
  for (i = 0; i < stack->nitems; i++)
    {
      token_ty *token = stack->items[i];
      fprintf (stderr, "  [%s]\n", token2string (token));
      switch (token->type)
	{
	case token_type_named_op:
	case token_type_string:
	case token_type_symbol:
	case token_type_variable:
	  fprintf (stderr, "    string: %s\n", token->string);
	  break;
	}
    }
  fprintf (stderr, "END STACK DUMP\n");
  return 0;
}
#endif

/* Pushes the token TOKEN onto the stack STACK.  */
static inline void
token_stack_push (token_stack_ty *stack, token_ty *token)
{
  if (stack->nitems >= stack->nitems_max)
    {
      size_t nbytes;

      stack->nitems_max = 2 * stack->nitems_max + 4;
      nbytes = stack->nitems_max * sizeof (token_ty *);
      stack->items = xrealloc (stack->items, nbytes);
    }
  stack->items[stack->nitems++] = token;
}

/* Pops the most recently pushed token from the stack STACK and returns it.
   Returns NULL if the stack is empty.  */
static inline token_ty *
token_stack_pop (token_stack_ty *stack)
{
  if (stack->nitems > 0)
    return stack->items[--(stack->nitems)];
  else
    return NULL;
}

/* Return the top of the stack without removing it from the stack, or
   NULL if the stack is empty.  */
static inline token_ty *
token_stack_peek (const token_stack_ty *stack)
{
  if (stack->nitems > 0)
    return stack->items[stack->nitems - 1];
  else
    return NULL;
}

/* Frees all resources allocated by stack STACK.  */
static inline void
token_stack_free (token_stack_ty *stack)
{
  size_t i;

  for (i = 0; i < stack->nitems; i++)
    free_token (stack->items[i]);
  free (stack->items);
}


static token_ty *
x_perl_lex (message_list_ty *mlp)
{
#if DEBUG_PERL
  int dummy = token_stack_dump (&token_stack);
#endif
  token_ty *tp = token_stack_pop (&token_stack);

  if (!tp)
    {
      tp = (token_ty *) xmalloc (sizeof (token_ty));
      x_perl_prelex (mlp, tp);
#if DEBUG_PERL
      fprintf (stderr, "%s:%d: x_perl_prelex returned %s\n",
	       real_file_name, line_number, token2string (tp));
#endif
    }
#if DEBUG_PERL
  else
    {
      fprintf (stderr, "%s:%d: %s recycled from stack\n",
	       real_file_name, line_number, token2string (tp));
    }
#endif

  /* A symbol followed by a fat comma is really a single-quoted string.
     Function definitions or forward declarations also need a special
     handling because the dollars and at signs inside the parentheses
     must not be interpreted as the beginning of a variable ')'.  */
  if (tp->type == token_type_symbol || tp->type == token_type_named_op)
    {
      token_ty *next = token_stack_peek (&token_stack);

      if (!next)
	{
#if DEBUG_PERL
	  fprintf (stderr, "%s:%d: pre-fetching next token\n",
		   real_file_name, line_number);
#endif
	  next = x_perl_lex (mlp);
	  x_perl_unlex (next);
#if DEBUG_PERL
	  fprintf (stderr, "%s:%d: unshifted next token\n",
		   real_file_name, line_number);
#endif
	}

#if DEBUG_PERL
      fprintf (stderr, "%s:%d: next token is %s\n",
	       real_file_name, line_number, token2string (next));
#endif

      if (next->type == token_type_fat_comma)
	{
	  tp->type = token_type_string;
	  tp->sub_type = string_type_q;
#if DEBUG_PERL
	  fprintf (stderr,
		   "%s:%d: token %s mutated to token_type_string\n",
		   real_file_name, line_number, token2string (tp));
#endif
	}
      else if (tp->type == token_type_symbol && tp->sub_type == symbol_type_sub
	       && next->type == token_type_symbol)
        {
	  /* Start of a function declaration or definition.  Mark this
	     symbol as a function name, so that we can later eat up
	     possible prototype information.  */
#if DEBUG_PERL
	  fprintf (stderr, "%s:%d: subroutine declaration/definition '%s'\n",
		   real_file_name, line_number, next->string);
#endif
	  next->sub_type = symbol_type_function;
	}
      else if (tp->type == token_type_symbol
	       && (tp->sub_type == symbol_type_sub
		   || tp->sub_type == symbol_type_function)
	       && next->type == token_type_lparen)
        {
	  /* For simplicity we simply consume everything up to the
	     closing parenthesis.  Actually only a limited set of
	     characters is allowed inside parentheses but we leave
	     complaints to the interpreter and are prepared for
	     future extensions to the Perl syntax.  */
	  int c;

#if DEBUG_PERL
	  fprintf (stderr, "%s:%d: consuming prototype information\n",
		   real_file_name, line_number);
#endif

	  do
	    {
	      c = phase1_getc ();
#if DEBUG_PERL
	      fprintf (stderr, "  consuming character '%c'\n", c);
#endif
	    }
	  while (c != EOF && c != ')');
	  phase1_ungetc (c);
	}
    }

  return tp;
}

static void
x_perl_unlex (token_ty *tp)
{
  token_stack_push (&token_stack, tp);
}


/* ========================= Extracting strings.  ========================== */

/* Assuming TP is a string token, this function accumulates all subsequent
   . string2 . string3 ... to the string.  (String concatenation.)  */

static char *
collect_message (message_list_ty *mlp, token_ty *tp, int error_level)
{
  char *string;
  size_t len;

  extract_quotelike_pass3 (tp, error_level);
  string = xstrdup (tp->string);
  len = strlen (tp->string) + 1;

  for (;;)
    {
      int c;

      do
	c = phase2_getc ();
      while (is_whitespace (c));

      if (c != '.')
	{
	  phase2_ungetc (c);
	  return string;
	}

      do
	c = phase2_getc ();
      while (is_whitespace (c));

      phase2_ungetc (c);

      if (c == '"' || c == '\'' || c == '`'
	  || (!prefer_division_over_regexp && (c == '/' || c == '?'))
	  || c == 'q')
	{
	  token_ty *qstring = x_perl_lex (mlp);
	  if (qstring->type != token_type_string)
	    {
	      /* assert (qstring->type == token_type_symbol) */
	      x_perl_unlex (qstring);
	      return string;
	    }

	  extract_quotelike_pass3 (qstring, error_level);
	  len += strlen (qstring->string);
	  string = xrealloc (string, len);
	  strcat (string, qstring->string);
	  free_token (qstring);
	}
    }
}

/* The file is broken into tokens.  Scan the token stream, looking for
   a keyword, followed by a left paren, followed by a string.  When we
   see this sequence, we have something to remember.  We assume we are
   looking at a valid C or C++ program, and leave the complaints about
   the grammar to the compiler.

     Normal handling: Look for
       keyword ( ... msgid ... )
     Plural handling: Look for
       keyword ( ... msgid ... msgid_plural ... )

   We use recursion because the arguments before msgid or between msgid
   and msgid_plural can contain subexpressions of the same form.  */

/* Extract messages until the next balanced closing parenthesis.
   Extracted messages are added to MLP.

   When specific arguments shall be extracted, ARG_SG and ARG_PL are
   set to the corresponding argument number or -1 if not applicable.

   Returns true for EOF, false otherwise.

   States are:

   0 - initial state
   1 - keyword has been seen
   2 - extractable string has been seen
   3 - a dot operator after an extractable string has been seen

   States 2 and 3 are "fragile", the parser will remain in state 2
   as long as only opening parentheses are seen, a transition to
   state 3 is done on appearance of a dot operator, all other tokens
   will cause the parser to fall back to state 1 or 0, eventually
   with an error message about invalid intermixing of constant and
   non-constant strings.

   Likewise, state 3 is fragile.  The parser will remain in state 3
   as long as only closing parentheses are seen, a transition to state
   2 is done on appearance of another (literal!) string, all other
   tokens will cause a warning.  */

static bool
extract_balanced (message_list_ty *mlp, int state, token_type_ty delim,
		  flag_context_ty outer_context,
		  flag_context_list_iterator_ty context_iter,
		  int arg_sg, int arg_pl)
{
  /* Remember the message containing the msgid, for msgid_plural.  */
  message_ty *plural_mp = NULL;

  /* The current argument for a possibly extracted keyword.  Counting
     starts with 1.  */
  int arg_count = 1;

  /* Number of left parentheses seen.  */
  int paren_seen = 0;

  /* Whether to implicitly assume the next tokens are arguments even without
     a '('.  */
  bool next_is_argument = false;

  /* Context iterator that will be used if the next token is a '('.  */
  flag_context_list_iterator_ty next_context_iter =
    passthrough_context_list_iterator;
  /* Current context.  */
  flag_context_ty inner_context =
    inherited_context (outer_context,
		       flag_context_list_iterator_advance (&context_iter));

#if DEBUG_PERL
  static int nesting_level = 0;

  ++nesting_level;
#endif

  last_token = token_type_semicolon;  /* Safe assumption.  */
  prefer_division_over_regexp = false;

  for (;;)
    {
      int my_last_token = last_token;
      /* The current token.  */
      token_ty *tp;

      tp = x_perl_lex (mlp);

      last_token = tp->type;

      if (delim == tp->type)
	{
#if DEBUG_PERL
	  fprintf (stderr, "%s:%d: extract_balanced finished (%d)\n",
		   logical_file_name, tp->line_number, --nesting_level);
#endif
	  free_token (tp);
	  return false;
	}

      if (next_is_argument && tp->type != token_type_lparen)
	{
	  /* An argument list starts, even though there is no '('.  */
	  context_iter = next_context_iter;
	  outer_context = inner_context;
	  inner_context =
	    inherited_context (outer_context,
			       flag_context_list_iterator_advance (
				 &context_iter));
	}

      switch (tp->type)
	{
	case token_type_symbol:
#if DEBUG_PERL
	  fprintf (stderr, "%s:%d: type symbol (%d) \"%s\"\n",
		   logical_file_name, tp->line_number, nesting_level,
		   tp->string);
#endif

	  {
	    void *keyword_value;

	    if (find_entry (&keywords, tp->string, strlen (tp->string),
			    &keyword_value) == 0)
	      {
		last_token = token_type_keyword_symbol;

		arg_sg = (int) (long) keyword_value & ((1 << 10) - 1);
		arg_pl = (int) (long) keyword_value >> 10;
		arg_count = 1;

		state = 2;
	      }
	  }
	  next_is_argument = true;
	  next_context_iter =
	    flag_context_list_iterator (
	      flag_context_list_table_lookup (
		flag_context_list_table,
		tp->string, strlen (tp->string)));
	  break;

	case token_type_variable:
#if DEBUG_PERL
	  fprintf (stderr, "%s:%d: type variable (%d) \"%s\"\n",
		   logical_file_name, tp->line_number, nesting_level, tp->string);
#endif
	  prefer_division_over_regexp = true;
	  next_is_argument = false;
	  next_context_iter = null_context_list_iterator;
	  break;

	case token_type_lparen:
#if DEBUG_PERL
	  fprintf (stderr, "%s:%d: type left parentheses (%d)\n",
		   logical_file_name, tp->line_number, nesting_level);
#endif
	  ++paren_seen;

	  if (extract_balanced (mlp, state, token_type_rparen,
				inner_context, next_context_iter,
				arg_sg - arg_count + 1, arg_pl - arg_count + 1))
	    {
	      free_token (tp);
	      return true;
	    }
	  if (my_last_token == token_type_keyword_symbol)
	    arg_sg = arg_pl = -1;
	  next_is_argument = false;
	  next_context_iter = null_context_list_iterator;
	  break;

	case token_type_rparen:
#if DEBUG_PERL
	  fprintf (stderr, "%s:%d: type right parentheses(%d)\n",
		   logical_file_name, tp->line_number, nesting_level);
#endif
	  --paren_seen;
	  next_is_argument = false;
	  next_context_iter = null_context_list_iterator;
	  break;

	case token_type_comma:
	case token_type_fat_comma:
#if DEBUG_PERL
	  fprintf (stderr, "%s:%d: type comma (%d)\n",
		   logical_file_name, tp->line_number, nesting_level);
#endif
	  ++arg_count;
	  if (arg_count > arg_sg && arg_count > arg_pl)
	    {
	      /* We have missed the argument.  */
	      arg_sg = arg_pl = -1;
	      arg_count = 0;
	    }
#if DEBUG_PERL
	  fprintf (stderr, "%s:%d: arg_count: %d, arg_sg: %d, arg_pl: %d\n",
		   real_file_name, tp->line_number,
		   arg_count, arg_sg, arg_pl);
#endif
	  inner_context =
	    inherited_context (outer_context,
			       flag_context_list_iterator_advance (
				 &context_iter));
	  next_is_argument = false;
	  next_context_iter = passthrough_context_list_iterator;
	  break;

	case token_type_string:
#if DEBUG_PERL
	  fprintf (stderr, "%s:%d: type string (%d): \"%s\"\n",
		   logical_file_name, tp->line_number, nesting_level,
		   tp->string);
#endif

	  if (extract_all)
	    {
	      lex_pos_ty pos;
	      char *string;

	      pos.file_name = logical_file_name;
	      pos.line_number = tp->line_number;
	      string = collect_message (mlp, tp, EXIT_SUCCESS);
	      xgettext_current_source_encoding = po_charset_utf8;
	      remember_a_message (mlp, string, inner_context, &pos);
	      xgettext_current_source_encoding = xgettext_global_source_encoding;
	    }
	  else if (state)
	    {
	      lex_pos_ty pos;
	      char *string;

	      pos.file_name = logical_file_name;
	      pos.line_number = tp->line_number;

	      if (arg_count == arg_sg)
		{
		  string = collect_message (mlp, tp, EXIT_FAILURE);
		  xgettext_current_source_encoding = po_charset_utf8;
		  plural_mp = remember_a_message (mlp, string, inner_context, &pos);
		  xgettext_current_source_encoding = xgettext_global_source_encoding;
		  arg_sg = -1;
		}
	      else if (arg_count == arg_pl)
		{
		  if (plural_mp == NULL)
		    error (EXIT_FAILURE, 0, _("\
%s:%d: fatal: plural message seen before singular message\n"),
			   real_file_name, tp->line_number);

		  string = collect_message (mlp, tp, EXIT_FAILURE);
		  xgettext_current_source_encoding = po_charset_utf8;
		  remember_a_message_plural (plural_mp, string, inner_context, &pos);
		  xgettext_current_source_encoding = xgettext_global_source_encoding;
		  arg_pl = -1;
		}
	    }

	  if (arg_sg == -1 && arg_pl == -1)
	    {
	      state = 0;
	      plural_mp = NULL;
	    }

	  next_is_argument = false;
	  next_context_iter = null_context_list_iterator;
	  break;

	case token_type_eof:
#if DEBUG_PERL
	  fprintf (stderr, "%s:%d: type EOF (%d)\n",
		   logical_file_name, tp->line_number, nesting_level);
#endif
	  free_token (tp);
	  return true;

	case token_type_lbrace:
#if DEBUG_PERL
	  fprintf (stderr, "%s:%d: type lbrace (%d)\n",
		   logical_file_name, tp->line_number, nesting_level);
#endif
	  if (extract_balanced (mlp, 0, token_type_rbrace,
				null_context, null_context_list_iterator,
				-1, -1))
	    {
	      free_token (tp);
	      return true;
	    }
	  next_is_argument = false;
	  next_context_iter = null_context_list_iterator;
	  break;

	case token_type_rbrace:
#if DEBUG_PERL
	  fprintf (stderr, "%s:%d: type rbrace (%d)\n",
		   logical_file_name, tp->line_number, nesting_level);
#endif
	  next_is_argument = false;
	  next_context_iter = null_context_list_iterator;
	  state = 0;
	  break;

	case token_type_lbracket:
#if DEBUG_PERL
	  fprintf (stderr, "%s:%d: type lbracket (%d)\n",
		   logical_file_name, tp->line_number, nesting_level);
#endif
	  if (extract_balanced (mlp, 0, token_type_rbracket,
				null_context, null_context_list_iterator,
				-1, -1))
	    {
	      free_token (tp);
	      return true;
	    }
	  next_is_argument = false;
	  next_context_iter = null_context_list_iterator;
	  break;

	case token_type_rbracket:
#if DEBUG_PERL
	  fprintf (stderr, "%s:%d: type rbracket (%d)\n",
		   logical_file_name, tp->line_number, nesting_level);
#endif
	  next_is_argument = false;
	  next_context_iter = null_context_list_iterator;
	  state = 0;
	  break;

	case token_type_semicolon:
#if DEBUG_PERL
	  fprintf (stderr, "%s:%d: type semicolon (%d)\n",
		   logical_file_name, tp->line_number, nesting_level);
#endif
	  state = 0;

	  /* The ultimate sign.  */
	  arg_sg = arg_pl = -1;

	  /* FIXME: Instead of resetting outer_context here, it may be better
	     to recurse in the next_is_argument handling above, waiting for
	     the next semicolon or other statement terminator.  */
	  outer_context = null_context;
	  context_iter = null_context_list_iterator;
	  next_is_argument = false;
	  next_context_iter = passthrough_context_list_iterator;
	  inner_context =
	    inherited_context (outer_context,
			       flag_context_list_iterator_advance (
				 &context_iter));
	  break;

	case token_type_dereference:
#if DEBUG_PERL
	  fprintf (stderr, "%s:%d: type dereference (%d)\n",
		   logical_file_name, tp->line_number, nesting_level);
#endif
	  next_is_argument = false;
	  next_context_iter = null_context_list_iterator;
	  break;

	case token_type_dot:
#if DEBUG_PERL
	  fprintf (stderr, "%s:%d: type dot (%d)\n",
		   logical_file_name, tp->line_number, nesting_level);
#endif
	  next_is_argument = false;
	  next_context_iter = null_context_list_iterator;
	  state = 0;
	  break;

	case token_type_named_op:
#if DEBUG_PERL
	  fprintf (stderr, "%s:%d: type named operator (%d): %s\n",
		   logical_file_name, tp->line_number, nesting_level,
		   tp->string);
#endif
	  next_is_argument = false;
	  next_context_iter = null_context_list_iterator;
	  state = 0;
	  break;

	case token_type_regex_op:
#if DEBUG_PERL
	  fprintf (stderr, "%s:%d: type regex operator (%d)\n",
		   logical_file_name, tp->line_number, nesting_level);
#endif
	  next_is_argument = false;
	  next_context_iter = null_context_list_iterator;
	  break;

	case token_type_other:
#if DEBUG_PERL
	  fprintf (stderr, "%s:%d: type other (%d)\n",
		   logical_file_name, tp->line_number, nesting_level);
#endif
	  next_is_argument = false;
	  next_context_iter = null_context_list_iterator;
	  state = 0;
	  break;

	default:
	  fprintf (stderr, "%s:%d: unknown token type %d\n",
		   real_file_name, tp->line_number, tp->type);
	  abort ();
	}

      free_token (tp);
    }
}

void
extract_perl (FILE *f, const char *real_filename, const char *logical_filename,
	      flag_context_list_table_ty *flag_table,
	      msgdomain_list_ty *mdlp)
{
  message_list_ty *mlp = mdlp->item[0]->messages;

  fp = f;
  real_file_name = real_filename;
  logical_file_name = xstrdup (logical_filename);
  line_number = 0;

  last_comment_line = -1;
  last_non_comment_line = -1;

  flag_context_list_table = flag_table;

  init_keywords ();

  token_stack.items = NULL;
  token_stack.nitems = 0;
  token_stack.nitems_max = 0;
  linesize = 0;
  linepos = 0;
  here_eaten = 0;
  end_of_file = false;

  /* Eat tokens until eof is seen.  When extract_balanced returns
     due to an unbalanced closing brace, just restart it.  */
  while (!extract_balanced (mlp, 0, token_type_rbrace,
			    null_context, null_context_list_iterator,
			    -1, -1))
    ;

  fp = NULL;
  real_file_name = NULL;
  free (logical_file_name);
  logical_file_name = NULL;
  line_number = 0;
  last_token = token_type_semicolon;
  token_stack_free (&token_stack);
  here_eaten = 0;
  end_of_file = true;
}
