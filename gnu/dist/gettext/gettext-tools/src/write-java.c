/* Writing Java ResourceBundles.
   Copyright (C) 2001-2003, 2005 Free Software Foundation, Inc.
   Written by Bruno Haible <haible@clisp.cons.org>, 2001.

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
#include <alloca.h>

/* Specification.  */
#include "write-java.h"

#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <sys/stat.h>
#if STAT_MACROS_BROKEN
# undef S_ISDIR
#endif
#if !defined S_ISDIR && defined S_IFDIR
# define S_ISDIR(mode) (((mode) & S_IFMT) == S_IFDIR)
#endif
#if !S_IRUSR && S_IREAD
# define S_IRUSR S_IREAD
#endif
#if !S_IRUSR
# define S_IRUSR 00400
#endif
#if !S_IWUSR && S_IWRITE
# define S_IWUSR S_IWRITE
#endif
#if !S_IWUSR
# define S_IWUSR 00200
#endif
#if !S_IXUSR && S_IEXEC
# define S_IXUSR S_IEXEC
#endif
#if !S_IXUSR
# define S_IXUSR 00100
#endif

#if HAVE_UNISTD_H
# include <unistd.h>
#endif

#ifdef __MINGW32__
/* mingw's mkdir() function has 1 argument, but we pass 2 arguments.
   Therefore we have to disable the argument count checking.  */
# define mkdir ((int (*)()) mkdir)
#endif

#include "c-ctype.h"
#include "error.h"
#include "javacomp.h"
#include "message.h"
#include "mkdtemp.h"
#include "msgfmt.h"
#include "msgl-iconv.h"
#include "pathmax.h"
#include "plural-exp.h"
#include "po-charset.h"
#include "xalloc.h"
#include "xallocsa.h"
#include "pathname.h"
#include "fatal-signal.h"
#include "fwriteerror.h"
#include "tmpdir.h"
#include "utf8-ucs4.h"
#include "gettext.h"

#define _(str) gettext (str)


/* Check that the resource name is a valid Java class name.  To simplify
   things, we allow only ASCII characters in the class name.
   Return the number of dots in the class name, or -1 if not OK.  */
static int
check_resource_name (const char *name)
{
  int ndots = 0;
  const char *p = name;

  for (;;)
    {
      /* First character, see Character.isJavaIdentifierStart.  */
      if (!(c_isalpha (*p) || (*p == '$') || (*p == '_')))
	return -1;
      /* Following characters, see Character.isJavaIdentifierPart.  */
      do
	p++;
      while (c_isalpha (*p) || (*p == '$') || (*p == '_') || c_isdigit (*p));
      if (*p == '\0')
	break;
      if (*p != '.')
	return -1;
      p++;
      ndots++;
    }
  return ndots;
}


/* Return the Java hash code of a string mod 2^31.
   The Java String.hashCode() function returns the same values across
   Java implementations.
   (See http://www.javasoft.com/docs/books/jls/clarify.html)
   It returns a signed 32-bit integer.  We add a mod 2^31 afterwards;
   this removes one bit but greatly simplifies the following "mod hash_size"
   and "mod (hash_size - 2)" operations.  */
static unsigned int
string_hashcode (const char *str)
{
  const char *str_limit = str + strlen (str);
  int hash = 0;
  while (str < str_limit)
    {
      unsigned int uc;
      str += u8_mbtouc (&uc, (const unsigned char *) str, str_limit - str);
      if (uc < 0x10000)
	/* Single UCS-2 'char'.  */
	hash = 31 * hash + uc;
      else
	{
	  /* UTF-16 surrogate: two 'char's.  */
	  unsigned int uc1 = 0xd800 + ((uc - 0x10000) >> 10);
	  unsigned int uc2 = 0xdc00 + ((uc - 0x10000) & 0x3ff);
	  hash = 31 * hash + uc1;
	  hash = 31 * hash + uc2;
	}
    }
  return hash & 0x7fffffff;
}


/* Compute a good hash table size for the given set of msgids.  */
static unsigned int
compute_hashsize (message_list_ty *mlp, bool *collisionp)
{
  /* This is an O(n^2) algorithm, but should be sufficient because few
     programs have more than 1000 messages in a single domain.  */
#define XXN 3  /* can be tweaked */
#define XXS 3  /* can be tweaked */
  unsigned int n = mlp->nitems;
  unsigned int *hashcodes =
    (unsigned int *) xallocsa (n * sizeof (unsigned int));
  unsigned int hashsize;
  unsigned int best_hashsize;
  unsigned int best_score;
  size_t j;

  for (j = 0; j < n; j++)
    hashcodes[j] = string_hashcode (mlp->item[j]->msgid);

  /* Try all numbers between n and 3*n.  The score depends on the size of the
     table -- the smaller the better -- and the number of collision lookups,
     i.e. total number of times that 1 + (hashcode % (hashsize - 2))
     is added to the index during lookup.  If there are collisions, only odd
     hashsize values are allowed.  */
  best_hashsize = 0;
  best_score = UINT_MAX;
  for (hashsize = n; hashsize <= XXN * n; hashsize++)
    {
      char *bitmap;
      unsigned int score;

      /* Premature end of the loop if all future scores are known to be
	 larger than the already reached best_score.  This relies on the
	 ascending loop and on the fact that score >= hashsize.  */
      if (hashsize >= best_score)
	break;

      bitmap = (char *) xmalloc (hashsize);
      memset (bitmap, 0, hashsize);

      score = 0;
      for (j = 0; j < n; j++)
	{
	  unsigned int idx = hashcodes[j] % hashsize;

	  if (bitmap[idx] != 0)
	    {
	      /* Collision.  Cannot deal with it if hashsize is even.  */
	      if ((hashsize % 2) == 0)
		/* Try next hashsize.  */
		goto bad_hashsize;
	      else
		{
		  unsigned int idx0 = idx;
		  unsigned int incr = 1 + (hashcodes[j] % (hashsize - 2));
		  score += 2;	/* Big penalty for the additional division */
		  do
		    {
		      score++;	/* Small penalty for each loop round */
		      idx += incr;
		      if (idx >= hashsize)
			idx -= hashsize;
		      if (idx == idx0)
			/* Searching for a hole, we performed a whole round
			   across the table.  This happens particularly
			   frequently if gcd(hashsize,incr) > 1.  Try next
			   hashsize.  */
			goto bad_hashsize;
		    }
		  while (bitmap[idx] != 0);
		}
	    }
	  bitmap[idx] = 1;
	}

      /* Big hashsize also gives a penalty.  */
      score = XXS * score + hashsize;

      /* If for any incr between 1 and hashsize - 2, an whole round
	 (idx0, idx0 + incr, ...) is occupied, and the lookup function
	 must deal with collisions, then some inputs would lead to
	 an endless loop in the lookup function.  */
      if (score > hashsize)
	{
	  unsigned int incr;

	  /* Since the set { idx0, idx0 + incr, ... } depends only on idx0
	     and gcd(hashsize,incr), we only need to conside incr that
	     divides hashsize.  */
	  for (incr = 1; incr <= hashsize / 2; incr++)
	    if ((hashsize % incr) == 0)
	      {
		unsigned int idx0;

		for (idx0 = 0; idx0 < incr; idx0++)
		  {
		    bool full = true;
		    unsigned int idx;

		    for (idx = idx0; idx < hashsize; idx += incr)
		      if (bitmap[idx] == 0)
			{
			  full = false;
			  break;
			}
		    if (full)
		      /* A whole round is occupied.  */
		      goto bad_hashsize;
		  }
	      }
	}

      if (false)
	bad_hashsize:
	score = UINT_MAX;

      free (bitmap);

      if (score < best_score)
	{
	  best_score = score;
	  best_hashsize = hashsize;
	}
    }
  if (best_hashsize == 0 || best_score < best_hashsize)
    abort ();

  freesa (hashcodes);

  /* There are collisions if and only if best_score > best_hashsize.  */
  *collisionp = (best_score > best_hashsize);
  return best_hashsize;
}


struct table_item { unsigned int index; message_ty *mp; };

static int
compare_index (const void *pval1, const void *pval2)
{
  return (int)((const struct table_item *) pval1)->index
	 - (int)((const struct table_item *) pval2)->index;
}

/* Compute the list of messages and table indices, sorted according to the
   indices.  */
static struct table_item *
compute_table_items (message_list_ty *mlp, unsigned int hashsize)
{
  unsigned int n = mlp->nitems;
  struct table_item *arr =
    (struct table_item *) xmalloc (n * sizeof (struct table_item));
  char *bitmap;
  size_t j;

  bitmap = (char *) xmalloc (hashsize);
  memset (bitmap, 0, hashsize);

  for (j = 0; j < n; j++)
    {
      unsigned int hashcode = string_hashcode (mlp->item[j]->msgid);
      unsigned int idx = hashcode % hashsize;

      if (bitmap[idx] != 0)
	{
	  unsigned int incr = 1 + (hashcode % (hashsize - 2));
	  do
	    {
	      idx += incr;
	      if (idx >= hashsize)
		idx -= hashsize;
	    }
	  while (bitmap[idx] != 0);
	}
      bitmap[idx] = 1;

      arr[j].index = idx;
      arr[j].mp = mlp->item[j];
    }

  free (bitmap);

  qsort (arr, n, sizeof (arr[0]), compare_index);

  return arr;
}


/* Write a string in Java Unicode notation to the given stream.  */
static void
write_java_string (FILE *stream, const char *str)
{
  static const char hexdigit[] = "0123456789abcdef";
  const char *str_limit = str + strlen (str);

  fprintf (stream, "\"");
  while (str < str_limit)
    {
      unsigned int uc;
      str += u8_mbtouc (&uc, (const unsigned char *) str, str_limit - str);
      if (uc < 0x10000)
	{
	  /* Single UCS-2 'char'.  */
	  if (uc == 0x000a)
	    fprintf (stream, "\\n");
	  else if (uc == 0x000d)
	    fprintf (stream, "\\r");
	  else if (uc == 0x0022)
	    fprintf (stream, "\\\"");
	  else if (uc == 0x005c)
	    fprintf (stream, "\\\\");
	  else if (uc >= 0x0020 && uc < 0x007f)
	    fprintf (stream, "%c", uc);
	  else
	    fprintf (stream, "\\u%c%c%c%c",
		     hexdigit[(uc >> 12) & 0x0f], hexdigit[(uc >> 8) & 0x0f],
		     hexdigit[(uc >> 4) & 0x0f], hexdigit[uc & 0x0f]);
	}
      else
	{
	  /* UTF-16 surrogate: two 'char's.  */
	  unsigned int uc1 = 0xd800 + ((uc - 0x10000) >> 10);
	  unsigned int uc2 = 0xdc00 + ((uc - 0x10000) & 0x3ff);
	  fprintf (stream, "\\u%c%c%c%c",
		   hexdigit[(uc1 >> 12) & 0x0f], hexdigit[(uc1 >> 8) & 0x0f],
		   hexdigit[(uc1 >> 4) & 0x0f], hexdigit[uc1 & 0x0f]);
	  fprintf (stream, "\\u%c%c%c%c",
		   hexdigit[(uc2 >> 12) & 0x0f], hexdigit[(uc2 >> 8) & 0x0f],
		   hexdigit[(uc2 >> 4) & 0x0f], hexdigit[uc2 & 0x0f]);
	}
    }
  fprintf (stream, "\"");
}


/* Write Java code that returns the value for a message.  If the message
   has plural forms, it is an expression of type String[], otherwise it is
   an expression of type String.  */
static void
write_java_msgstr (FILE *stream, message_ty *mp)
{
  if (mp->msgid_plural != NULL)
    {
      bool first;
      const char *p;

      fprintf (stream, "new java.lang.String[] { ");
      for (p = mp->msgstr, first = true;
	   p < mp->msgstr + mp->msgstr_len;
	   p += strlen (p) + 1, first = false)
	{
	  if (!first)
	    fprintf (stream, ", ");
	  write_java_string (stream, p);
	}
      fprintf (stream, " }");
    }
  else
    {
      if (mp->msgstr_len != strlen (mp->msgstr) + 1)
	abort ();

      write_java_string (stream, mp->msgstr);
    }
}


/* Writes the body of the function which returns the local value for a key
   named 'msgid'.  */
static void
write_lookup_code (FILE *stream, unsigned int hashsize, bool collisions)
{
  fprintf (stream, "    int hash_val = msgid.hashCode() & 0x7fffffff;\n");
  fprintf (stream, "    int idx = (hash_val %% %d) << 1;\n", hashsize);
  if (collisions)
    {
      fprintf (stream, "    {\n");
      fprintf (stream, "      java.lang.Object found = table[idx];\n");
      fprintf (stream, "      if (found == null)\n");
      fprintf (stream, "        return null;\n");
      fprintf (stream, "      if (msgid.equals(found))\n");
      fprintf (stream, "        return table[idx + 1];\n");
      fprintf (stream, "    }\n");
      fprintf (stream, "    int incr = ((hash_val %% %d) + 1) << 1;\n",
	       hashsize - 2);
      fprintf (stream, "    for (;;) {\n");
      fprintf (stream, "      idx += incr;\n");
      fprintf (stream, "      if (idx >= %d)\n", 2 * hashsize);
      fprintf (stream, "        idx -= %d;\n", 2 * hashsize);
      fprintf (stream, "      java.lang.Object found = table[idx];\n");
      fprintf (stream, "      if (found == null)\n");
      fprintf (stream, "        return null;\n");
      fprintf (stream, "      if (msgid.equals(found))\n");
      fprintf (stream, "        return table[idx + 1];\n");
      fprintf (stream, "    }\n");
    }
  else
    {
      fprintf (stream, "    java.lang.Object found = table[idx];\n");
      fprintf (stream, "    if (found != null && msgid.equals(found))\n");
      fprintf (stream, "      return table[idx + 1];\n");
      fprintf (stream, "    return null;\n");
    }
}


/* Tests whether a plural expression, evaluated according to the C rules,
   can only produce the values 0 and 1.  */
static bool
is_expression_boolean (struct expression *exp)
{
  switch (exp->operation)
    {
    case var:
    case mult:
    case divide:
    case module:
    case plus:
    case minus:
      return false;
    case lnot:
    case less_than:
    case greater_than:
    case less_or_equal:
    case greater_or_equal:
    case equal:
    case not_equal:
    case land:
    case lor:
      return true;
    case num:
      return (exp->val.num == 0 || exp->val.num == 1);
    case qmop:
      return is_expression_boolean (exp->val.args[1])
	     && is_expression_boolean (exp->val.args[2]);
    default:
      abort ();
    }
}


/* Write Java code that evaluates a plural expression according to the C rules.
   The variable is called 'n'.  */
static void
write_java_expression (FILE *stream, struct expression *exp, bool as_boolean)
{
  /* We use parentheses everywhere.  This frees us from tracking the priority
     of arithmetic operators.  */
  if (as_boolean)
    {
      /* Emit a Java expression of type 'boolean'.  */
      switch (exp->operation)
	{
	case num:
	  fprintf (stream, "%s", exp->val.num ? "true" : "false");
	  return;
	case lnot:
	  fprintf (stream, "(!");
	  write_java_expression (stream, exp->val.args[0], true);
	  fprintf (stream, ")");
	  return;
	case less_than:
	  fprintf (stream, "(");
	  write_java_expression (stream, exp->val.args[0], false);
	  fprintf (stream, " < ");
	  write_java_expression (stream, exp->val.args[1], false);
	  fprintf (stream, ")");
	  return;
	case greater_than:
	  fprintf (stream, "(");
	  write_java_expression (stream, exp->val.args[0], false);
	  fprintf (stream, " > ");
	  write_java_expression (stream, exp->val.args[1], false);
	  fprintf (stream, ")");
	  return;
	case less_or_equal:
	  fprintf (stream, "(");
	  write_java_expression (stream, exp->val.args[0], false);
	  fprintf (stream, " <= ");
	  write_java_expression (stream, exp->val.args[1], false);
	  fprintf (stream, ")");
	  return;
	case greater_or_equal:
	  fprintf (stream, "(");
	  write_java_expression (stream, exp->val.args[0], false);
	  fprintf (stream, " >= ");
	  write_java_expression (stream, exp->val.args[1], false);
	  fprintf (stream, ")");
	  return;
	case equal:
	  fprintf (stream, "(");
	  write_java_expression (stream, exp->val.args[0], false);
	  fprintf (stream, " == ");
	  write_java_expression (stream, exp->val.args[1], false);
	  fprintf (stream, ")");
	  return;
	case not_equal:
	  fprintf (stream, "(");
	  write_java_expression (stream, exp->val.args[0], false);
	  fprintf (stream, " != ");
	  write_java_expression (stream, exp->val.args[1], false);
	  fprintf (stream, ")");
	  return;
	case land:
	  fprintf (stream, "(");
	  write_java_expression (stream, exp->val.args[0], true);
	  fprintf (stream, " && ");
	  write_java_expression (stream, exp->val.args[1], true);
	  fprintf (stream, ")");
	  return;
	case lor:
	  fprintf (stream, "(");
	  write_java_expression (stream, exp->val.args[0], true);
	  fprintf (stream, " || ");
	  write_java_expression (stream, exp->val.args[1], true);
	  fprintf (stream, ")");
	  return;
	case qmop:
	  if (is_expression_boolean (exp->val.args[1])
	      && is_expression_boolean (exp->val.args[2]))
	    {
	      fprintf (stream, "(");
	      write_java_expression (stream, exp->val.args[0], true);
	      fprintf (stream, " ? ");
	      write_java_expression (stream, exp->val.args[1], true);
	      fprintf (stream, " : ");
	      write_java_expression (stream, exp->val.args[2], true);
	      fprintf (stream, ")");
	      return;
	    }
	  /*FALLTHROUGH*/
	case var:
	case mult:
	case divide:
	case module:
	case plus:
	case minus:
	  fprintf (stream, "(");
	  write_java_expression (stream, exp, false);
	  fprintf (stream, " != 0)");
	  return;
	default:
	  abort ();
	}
    }
  else
    {
      /* Emit a Java expression of type 'long'.  */
      switch (exp->operation)
	{
	case var:
	  fprintf (stream, "n");
	  return;
	case num:
	  fprintf (stream, "%lu", exp->val.num);
	  return;
	case mult:
	  fprintf (stream, "(");
	  write_java_expression (stream, exp->val.args[0], false);
	  fprintf (stream, " * ");
	  write_java_expression (stream, exp->val.args[1], false);
	  fprintf (stream, ")");
	  return;
	case divide:
	  fprintf (stream, "(");
	  write_java_expression (stream, exp->val.args[0], false);
	  fprintf (stream, " / ");
	  write_java_expression (stream, exp->val.args[1], false);
	  fprintf (stream, ")");
	  return;
	case module:
	  fprintf (stream, "(");
	  write_java_expression (stream, exp->val.args[0], false);
	  fprintf (stream, " %% ");
	  write_java_expression (stream, exp->val.args[1], false);
	  fprintf (stream, ")");
	  return;
	case plus:
	  fprintf (stream, "(");
	  write_java_expression (stream, exp->val.args[0], false);
	  fprintf (stream, " + ");
	  write_java_expression (stream, exp->val.args[1], false);
	  fprintf (stream, ")");
	  return;
	case minus:
	  fprintf (stream, "(");
	  write_java_expression (stream, exp->val.args[0], false);
	  fprintf (stream, " - ");
	  write_java_expression (stream, exp->val.args[1], false);
	  fprintf (stream, ")");
	  return;
	case qmop:
	  fprintf (stream, "(");
	  write_java_expression (stream, exp->val.args[0], true);
	  fprintf (stream, " ? ");
	  write_java_expression (stream, exp->val.args[1], false);
	  fprintf (stream, " : ");
	  write_java_expression (stream, exp->val.args[2], false);
	  fprintf (stream, ")");
	  return;
	case lnot:
	case less_than:
	case greater_than:
	case less_or_equal:
	case greater_or_equal:
	case equal:
	case not_equal:
	case land:
	case lor:
	  fprintf (stream, "(");
	  write_java_expression (stream, exp, true);
	  fprintf (stream, " ? 1 : 0)");
	  return;
	default:
	  abort ();
	}
    }
}


/* Write the Java code for the ResourceBundle subclass to the given stream.
   Note that we use fully qualified class names and no "import" statements,
   because applications can have their own classes called X.Y.ResourceBundle
   or X.Y.String.  */
static void
write_java_code (FILE *stream, const char *class_name, message_list_ty *mlp,
		 bool assume_java2)
{
  const char *last_dot;
  unsigned int plurals;
  size_t j;

  fprintf (stream,
	   "/* Automatically generated by GNU msgfmt.  Do not modify!  */\n");
  last_dot = strrchr (class_name, '.');
  if (last_dot != NULL)
    {
      fprintf (stream, "package ");
      fwrite (class_name, 1, last_dot - class_name, stream);
      fprintf (stream, ";\npublic class %s", last_dot + 1);
    }
  else
    fprintf (stream, "public class %s", class_name);
  fprintf (stream, " extends java.util.ResourceBundle {\n");

  /* Determine whether there are plural messages.  */
  plurals = 0;
  for (j = 0; j < mlp->nitems; j++)
    if (mlp->item[j]->msgid_plural != NULL)
      plurals++;

  if (assume_java2)
    {
      unsigned int hashsize;
      bool collisions;
      struct table_item *table_items;
      const char *table_eltype;

      /* Determine the hash table size and whether it leads to collisions.  */
      hashsize = compute_hashsize (mlp, &collisions);

      /* Determines which indices in the table contain a message.  The others
	 are null.  */
      table_items = compute_table_items (mlp, hashsize);

      /* Emit the table of pairs (msgid, msgstr).  If there are plurals,
	 it is of type Object[], otherwise of type String[].  We use a static
	 code block because that makes less code:  The Java compilers also
	 generate code for the 'null' entries, which is dumb.  */
      table_eltype = (plurals ? "java.lang.Object" : "java.lang.String");
      fprintf (stream, "  private static final %s[] table;\n", table_eltype);
      fprintf (stream, "  static {\n");
      fprintf (stream, "    %s[] t = new %s[%d];\n", table_eltype, table_eltype,
	       2 * hashsize);
      for (j = 0; j < mlp->nitems; j++)
	{
	  struct table_item *ti = &table_items[j];

	  fprintf (stream, "    t[%d] = ", 2 * ti->index);
	  write_java_string (stream, ti->mp->msgid);
	  fprintf (stream, ";\n");
	  fprintf (stream, "    t[%d] = ", 2 * ti->index + 1);
	  write_java_msgstr (stream, ti->mp);
	  fprintf (stream, ";\n");
	}
      fprintf (stream, "    table = t;\n");
      fprintf (stream, "  }\n");

      /* Emit the msgid_plural strings.  Only used by msgunfmt.  */
      if (plurals)
	{
	  bool first;
	  fprintf (stream, "  public static final java.lang.String[] get_msgid_plural_table () {\n");
	  fprintf (stream, "    return new java.lang.String[] { ");
	  first = true;
	  for (j = 0; j < mlp->nitems; j++)
	    {
	      struct table_item *ti = &table_items[j];
	      if (ti->mp->msgid_plural != NULL)
		{
		  if (!first)
		    fprintf (stream, ", ");
		  write_java_string (stream, ti->mp->msgid_plural);
		  first = false;
		}
	    }
	  fprintf (stream, " };\n");
	  fprintf (stream, "  }\n");
	}

      if (plurals)
	{
	  /* Emit the lookup function.  It is a common subroutine for
	     handleGetObject and ngettext.  */
	  fprintf (stream, "  public java.lang.Object lookup (java.lang.String msgid) {\n");
	  write_lookup_code (stream, hashsize, collisions);
	  fprintf (stream, "  }\n");
	}

      /* Emit the handleGetObject function.  It is declared abstract in
	 ResourceBundle.  It implements a local version of gettext.  */
      fprintf (stream, "  public java.lang.Object handleGetObject (java.lang.String msgid) throws java.util.MissingResourceException {\n");
      if (plurals)
	{
	  fprintf (stream, "    java.lang.Object value = lookup(msgid);\n");
	  fprintf (stream, "    return (value instanceof java.lang.String[] ? ((java.lang.String[])value)[0] : value);\n");
	}
      else
	write_lookup_code (stream, hashsize, collisions);
      fprintf (stream, "  }\n");

      /* Emit the getKeys function.  It is declared abstract in ResourceBundle.
	 The inner class is not avoidable.  */
      fprintf (stream, "  public java.util.Enumeration getKeys () {\n");
      fprintf (stream, "    return\n");
      fprintf (stream, "      new java.util.Enumeration() {\n");
      fprintf (stream, "        private int idx = 0;\n");
      fprintf (stream, "        { while (idx < %d && table[idx] == null) idx += 2; }\n",
	       2 * hashsize);
      fprintf (stream, "        public boolean hasMoreElements () {\n");
      fprintf (stream, "          return (idx < %d);\n", 2 * hashsize);
      fprintf (stream, "        }\n");
      fprintf (stream, "        public java.lang.Object nextElement () {\n");
      fprintf (stream, "          java.lang.Object key = table[idx];\n");
      fprintf (stream, "          do idx += 2; while (idx < %d && table[idx] == null);\n",
	       2 * hashsize);
      fprintf (stream, "          return key;\n");
      fprintf (stream, "        }\n");
      fprintf (stream, "      };\n");
      fprintf (stream, "  }\n");
    }
  else
    {
      /* Java 1.1.x uses a different hash function.  If compatibility with
	 this Java version is required, the hash table must be built at run time,
	 not at compile time.  */
      fprintf (stream, "  private static final java.util.Hashtable table;\n");
      fprintf (stream, "  static {\n");
      fprintf (stream, "    java.util.Hashtable t = new java.util.Hashtable();\n");
      for (j = 0; j < mlp->nitems; j++)
	{
	  fprintf (stream, "    t.put(");
	  write_java_string (stream, mlp->item[j]->msgid);
	  fprintf (stream, ",");
	  write_java_msgstr (stream, mlp->item[j]);
	  fprintf (stream, ");\n");
	}
      fprintf (stream, "    table = t;\n");
      fprintf (stream, "  }\n");

      /* Emit the msgid_plural strings.  Only used by msgunfmt.  */
      if (plurals)
	{
	  fprintf (stream, "  public static final java.util.Hashtable get_msgid_plural_table () {\n");
	  fprintf (stream, "    java.util.Hashtable p = new java.util.Hashtable();\n");
	  for (j = 0; j < mlp->nitems; j++)
	    if (mlp->item[j]->msgid_plural != NULL)
	      {
		fprintf (stream, "    p.put(");
		write_java_string (stream, mlp->item[j]->msgid);
		fprintf (stream, ",");
		write_java_string (stream, mlp->item[j]->msgid_plural);
		fprintf (stream, ");\n");
	      }
	  fprintf (stream, "    return p;\n");
	  fprintf (stream, "  }\n");
	}

      if (plurals)
	{
	  /* Emit the lookup function.  It is a common subroutine for
	     handleGetObject and ngettext.  */
	  fprintf (stream, "  public java.lang.Object lookup (java.lang.String msgid) {\n");
	  fprintf (stream, "    return table.get(msgid);\n");
	  fprintf (stream, "  }\n");
	}

      /* Emit the handleGetObject function.  It is declared abstract in
	 ResourceBundle.  It implements a local version of gettext.  */
      fprintf (stream, "  public java.lang.Object handleGetObject (java.lang.String msgid) throws java.util.MissingResourceException {\n");
      if (plurals)
	{
	  fprintf (stream, "    java.lang.Object value = table.get(msgid);\n");
	  fprintf (stream, "    return (value instanceof java.lang.String[] ? ((java.lang.String[])value)[0] : value);\n");
	}
      else
	fprintf (stream, "    return table.get(msgid);\n");
      fprintf (stream, "  }\n");

      /* Emit the getKeys function.  It is declared abstract in
	 ResourceBundle.  */
      fprintf (stream, "  public java.util.Enumeration getKeys () {\n");
      fprintf (stream, "    return table.keys();\n");
      fprintf (stream, "  }\n");
    }

  /* Emit the pluralEval function.  It is a subroutine for ngettext.  */
  if (plurals)
    {
      message_ty *header_entry;
      struct expression *plural;
      unsigned long int nplurals;

      header_entry = message_list_search (mlp, "");
      extract_plural_expression (header_entry ? header_entry->msgstr : NULL,
				 &plural, &nplurals);

      fprintf (stream, "  public static long pluralEval (long n) {\n");
      fprintf (stream, "    return ");
      write_java_expression (stream, plural, false);
      fprintf (stream, ";\n");
      fprintf (stream, "  }\n");
    }

  /* Emit the getParent function.  It is a subroutine for ngettext.  */
  fprintf (stream, "  public java.util.ResourceBundle getParent () {\n");
  fprintf (stream, "    return parent;\n");
  fprintf (stream, "  }\n");

  fprintf (stream, "}\n");
}


/* Asynchronously cleaning up temporary files, when we receive any of the
   usually occurring signals whose default action is to terminate the
   program.  */

static struct
{
  const char *tmpdir;
  unsigned int subdir_count;
  const char * const *subdir;
  const char *file_name;
} cleanup_list;

/* The signal handler.  It gets called asynchronously.  */
static void
cleanup ()
{
  unsigned int i;

  /* First cleanup the files in the subdirectory.  */
  {
    const char *filename = cleanup_list.file_name;

    if (filename != NULL)
      unlink (filename);
  }

  /* Then cleanup the subdirectories.  */
  for (i = cleanup_list.subdir_count; i > 0; )
    {
      const char *filename = cleanup_list.subdir[--i];

      rmdir (filename);
    }

  /* Then cleanup the main temporary directory.  */
  {
    const char *filename = cleanup_list.tmpdir;

    if (filename != NULL)
      rmdir (filename);
  }
}


int
msgdomain_write_java (message_list_ty *mlp, const char *canon_encoding,
		      const char *resource_name, const char *locale_name,
		      const char *directory,
		      bool assume_java2)
{
  int retval;
  char *template;
  char *tmpdir;
  int ndots;
  char *class_name;
  char **subdirs;
  char *java_file_name;
  FILE *java_file;
  const char *java_sources[1];

  /* If no entry for this resource/domain, don't even create the file.  */
  if (mlp->nitems == 0)
    return 0;

  retval = 1;

  /* Convert the messages to Unicode.  */
  iconv_message_list (mlp, canon_encoding, po_charset_utf8, NULL);

  cleanup_list.tmpdir = NULL;
  cleanup_list.subdir_count = 0;
  cleanup_list.file_name = NULL;
  {
    static bool cleanup_already_registered = false;
    if (!cleanup_already_registered)
      {
	at_fatal_signal (&cleanup);
	cleanup_already_registered = true;
      }
  }

  /* Create a temporary directory where we can put the Java file.  */
  template = (char *) xallocsa (PATH_MAX);
  if (path_search (template, PATH_MAX, NULL, "msg", 1))
    {
      error (0, errno,
	     _("cannot find a temporary directory, try setting $TMPDIR"));
      goto quit1;
    }
  block_fatal_signals ();
  tmpdir = mkdtemp (template);
  cleanup_list.tmpdir = tmpdir;
  unblock_fatal_signals ();
  if (tmpdir == NULL)
    {
      error (0, errno,
	     _("cannot create a temporary directory using template \"%s\""),
	     template);
      goto quit1;
    }

  /* Assign a default value to the resource name.  */
  if (resource_name == NULL)
    resource_name = "Messages";

  /* Prepare the list of subdirectories.  */
  ndots = check_resource_name (resource_name);
  if (ndots < 0)
    {
      error (0, 0, _("not a valid Java class name: %s"), resource_name);
      goto quit2;
    }

  if (locale_name != NULL)
    {
      class_name =
	(char *) xmalloc (strlen (resource_name) + 1 + strlen (locale_name) + 1);
      sprintf (class_name, "%s_%s", resource_name, locale_name);
    }
  else
    class_name = xstrdup (resource_name);

  subdirs = (ndots > 0 ? (char **) xallocsa (ndots * sizeof (char *)) : NULL);
  {
    const char *p;
    const char *last_dir;
    int i;

    last_dir = tmpdir;
    p = resource_name;
    for (i = 0; i < ndots; i++)
      {
	const char *q = strchr (p, '.');
	size_t n = q - p;
	char *part = (char *) xallocsa (n + 1);
	memcpy (part, p, n);
	part[n] = '\0';
	subdirs[i] = concatenated_pathname (last_dir, part, NULL);
	freesa (part);
	last_dir = subdirs[i];
	p = q + 1;
      }

    if (locale_name != NULL)
      {
	char *suffix = (char *) xmalloc (1 + strlen (locale_name) + 5 + 1);
	sprintf (suffix, "_%s.java", locale_name);
	java_file_name = concatenated_pathname (last_dir, p, suffix);
	free (suffix);
      }
    else
      java_file_name = concatenated_pathname (last_dir, p, ".java");
  }

  /* Create the subdirectories.  This is needed because some older Java
     compilers verify that the source of class A.B.C really sits in a
     directory whose name ends in /A/B.  */
  {
    int i;

    cleanup_list.subdir = (const char * const *) subdirs;
    cleanup_list.subdir_count = 0;
    for (i = 0; i < ndots; i++)
      {
	cleanup_list.subdir_count = i + 1;
	if (mkdir (subdirs[i], S_IRUSR | S_IWUSR | S_IXUSR) < 0)
	  {
	    error (0, errno, _("failed to create \"%s\""), subdirs[i]);
	    while (i-- > 0)
	      rmdir (subdirs[i]);
	    goto quit3;
	  }
      }
  }

  /* Create the Java file.  */
  cleanup_list.file_name = java_file_name;
  java_file = fopen (java_file_name, "w");
  if (java_file == NULL)
    {
      error (0, errno, _("failed to create \"%s\""), java_file_name);
      goto quit4;
    }

  write_java_code (java_file, class_name, mlp, assume_java2);

  if (fwriteerror (java_file))
    {
      error (0, errno, _("error while writing \"%s\" file"), java_file_name);
      fclose (java_file);
      goto quit5;
    }

  /* Compile the Java file to a .class file.
     directory must be non-NULL, because when the -d option is omitted, the
     Java compilers create the class files in the source file's directory -
     which is in a temporary directory in our case.  */
  java_sources[0] = java_file_name;
  if (compile_java_class (java_sources, 1, NULL, 0, directory, true, false,
			  true, verbose))
    {
      error (0, 0, _("\
compilation of Java class failed, please try --verbose or set $JAVAC"));
      goto quit5;
    }

  retval = 0;

 quit5:
  unlink (java_file_name);
 quit4:
  cleanup_list.file_name = NULL;
  {
    int i;
    for (i = ndots; i-- > 0; )
      rmdir (subdirs[i]);
  }
 quit3:
  cleanup_list.subdir_count = 0;
  {
    int i;
    free (java_file_name);
    for (i = 0; i < ndots; i++)
      free (subdirs[i]);
  }
  freesa (subdirs);
  free (class_name);
 quit2:
  rmdir (tmpdir);
 quit1:
  cleanup_list.tmpdir = NULL;
  freesa (template);
  /* Here we could unregister the cleanup() handler.  */
  return retval;
}
