/* Kernel Object Display generic routines and callbacks
   Copyright 1998, 1999, 2000 Free Software Foundation, Inc.

   Written by Fernando Nasser <fnasser@cygnus.com> for Cygnus Solutions.

   This file is part of GDB.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.  */

#include "defs.h"
#include "command.h"
#include "gdbcmd.h"
#include "target.h"
#include "gdb_string.h"
#include "kod.h"

/* Prototypes for exported functions.  */
void _initialize_kod (void);

/* Prototypes for local functions.  */
static void info_kod_command (char *, int);
static void load_kod_library (char *);

/* Prototypes for callbacks.  These are passed into the KOD modules.  */
static void gdb_kod_display (char *);
static void gdb_kod_query (char *, char *, int *);

/* These functions are imported from the KOD module.
   
   gdb_kod_open - initiates the KOD connection to the remote.  The
   first argument is the display function the module should use to
   communicate with the user.  The second argument is the query
   function the display should use to communicate with the target.
   This should call error() if there is an error.  Otherwise it should
   return a malloc()d string of the form:
   
   NAME VERSION - DESCRIPTION
   
   Neither NAME nor VERSION should contain a hyphen.

   
   gdb_kod_request - This is used when the user enters an "info
   <module>" request.  The remaining arguments are passed as the first
   argument.  The second argument is the standard `from_tty'
   argument.

   
   gdb_kod_close - This is called when the KOD connection to the
   remote should be terminated.  */

static char *(*gdb_kod_open) (kod_display_callback_ftype *display,
			      kod_query_callback_ftype *query);
static void (*gdb_kod_request) (char *, int);
static void (*gdb_kod_close) ();


/* Name of inferior's operating system.  */
char *operating_system;

/* We save a copy of the OS so that we can properly reset when
   switching OS's.  */
static char *old_operating_system;

/* Print a line of data generated by the module.  */

static void
gdb_kod_display (char *arg)
{
  printf_filtered ("%s", arg);
}

/* Queries the target on behalf of the module.  */

static void
gdb_kod_query (char *arg, char *result, int *maxsiz)
{
  int bufsiz = 0;

  /* Check if current target has remote_query capabilities.
     If not, it does not have kod either.  */
  if (! current_target.to_query)
    {
      strcpy (result,
              "ERR: Kernel Object Display not supported by current target\n");
      return;
    }

  /* Just get the maximum buffer size.  */
  target_query ((int) 'K', 0, 0, &bufsiz);

  /* Check if *we* were called just for getting the buffer size.  */
  if (*maxsiz == 0)
    {
      *maxsiz = bufsiz;
      strcpy (result, "OK");
      return;
    }

  /* Check if caller can handle a buffer this large, if not, adjust.  */
  if (bufsiz > *maxsiz)
    bufsiz = *maxsiz;

  /* See if buffer can hold the query (usually it can, as the query is
     short).  */
  if (strlen (arg) >= bufsiz)
    error ("kod: query argument too long");

  /* Send actual request.  */
  if (target_query ((int) 'K', arg, result, &bufsiz))
    strcpy (result, "ERR: remote query failed");
}

/* Print name of kod command after selecting the appropriate kod
   formatting library module.  As a side effect we create a new "info"
   subcommand which is what the user actually uses to query the OS.  */

static void
kod_set_os (char *arg, int from_tty, struct cmd_list_element *command)
{
  char *p;

  /* NOTE: cagney/2002-03-17: The add_show_from_set() function clones
     the set command passed as a parameter.  The clone operation will
     include (BUG?) any ``set'' command callback, if present.
     Commands like ``info set'' call all the ``show'' command
     callbacks.  Unfortunatly, for ``show'' commands cloned from
     ``set'', this includes callbacks belonging to ``set'' commands.
     Making this worse, this only occures if add_show_from_set() is
     called after add_cmd_sfunc() (BUG?).  */

  if (cmd_type (command) != set_cmd)
    return;

  /* If we had already had an open OS, close it.  */
  if (gdb_kod_close)
    (*gdb_kod_close) ();

  /* Also remove the old OS's command.  */
  if (old_operating_system)
    {
      delete_cmd (old_operating_system, &infolist);
      xfree (old_operating_system);
    }

  if (! operating_system || ! *operating_system)
    {
      /* If user set operating system to empty, we want to forget we
	 had a module open.  Setting these variables is just nice for
	 debugging and clarity.  */
      gdb_kod_open = NULL;
      gdb_kod_request = NULL;
      gdb_kod_close = NULL;
    }
  else
    {
      char *kodlib;

      old_operating_system = xstrdup (operating_system);

      load_kod_library (operating_system);

      kodlib = (*gdb_kod_open) (gdb_kod_display, gdb_kod_query);

      /* Add kod related info commands to gdb.  */
      add_info (operating_system, info_kod_command,
		"Displays information about Kernel Objects.");

      p = strrchr (kodlib, '-');
      if (p != NULL)
	p++;
      else
	p = "Unknown KOD library";
      printf_filtered ("%s - %s\n", operating_system, p);

      xfree (kodlib);
    }
}

/* Print information about currently known kernel objects of the
   specified type or a list of all known kernel object types if
   argument is empty.  */

static void
info_kod_command (char *arg, int from_tty)
{
  (*gdb_kod_request) (arg, from_tty);
}

/* Print name of kod command after selecting the appropriate kod
   formatting library module.  */

static void
load_kod_library (char *lib)
{
#if 0
  /* FIXME: Don't have the eCos code here.  */
  if (! strcmp (lib, "ecos"))
    {
      gdb_kod_open = ecos_kod_open;
      gdb_kod_request = ecos_kod_request;
      gdb_kod_close = ecos_kod_close;
    }
  else
#endif /* 0 */
   if (! strcmp (lib, "cisco"))
    {
      gdb_kod_open = cisco_kod_open;
      gdb_kod_request = cisco_kod_request;
      gdb_kod_close = cisco_kod_close;
    }
  else
    error ("Unknown operating system: %s\n", operating_system);
}

void
_initialize_kod (void)
{
  struct cmd_list_element *c;

  c = add_set_cmd ("os", no_class, var_string,
		   (char *) &operating_system,
		   "Set operating system",
		   &setlist);
  set_cmd_sfunc (c, kod_set_os);
  add_show_from_set (c, &showlist);
}
