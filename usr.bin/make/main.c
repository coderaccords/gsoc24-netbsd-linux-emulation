/*	$NetBSD: main.c,v 1.167 2009/03/01 01:49:17 christos Exp $	*/

/*
 * Copyright (c) 1988, 1989, 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Adam de Boor.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Copyright (c) 1989 by Berkeley Softworks
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Adam de Boor.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef MAKE_NATIVE
static char rcsid[] = "$NetBSD: main.c,v 1.167 2009/03/01 01:49:17 christos Exp $";
#else
#include <sys/cdefs.h>
#ifndef lint
__COPYRIGHT("@(#) Copyright (c) 1988, 1989, 1990, 1993\
 The Regents of the University of California.  All rights reserved.");
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "@(#)main.c	8.3 (Berkeley) 3/19/94";
#else
__RCSID("$NetBSD: main.c,v 1.167 2009/03/01 01:49:17 christos Exp $");
#endif
#endif /* not lint */
#endif

/*-
 * main.c --
 *	The main file for this entire program. Exit routines etc
 *	reside here.
 *
 * Utility functions defined in this file:
 *	Main_ParseArgLine	Takes a line of arguments, breaks them and
 *				treats them as if they were given when first
 *				invoked. Used by the parse module to implement
 *				the .MFLAGS target.
 *
 *	Error			Print a tagged error message. The global
 *				MAKE variable must have been defined. This
 *				takes a format string and two optional
 *				arguments for it.
 *
 *	Fatal			Print an error message and exit. Also takes
 *				a format string and two arguments.
 *
 *	Punt			Aborts all jobs and exits with a message. Also
 *				takes a format string and two arguments.
 *
 *	Finish			Finish things up by printing the number of
 *				errors which occurred, as passed to it, and
 *				exiting.
 */

#include <sys/types.h>
#include <sys/time.h>
#include <sys/param.h>
#include <sys/resource.h>
#include <sys/signal.h>
#include <sys/stat.h>
#ifdef MAKE_NATIVE
#include <sys/utsname.h>
#endif
#include <sys/wait.h>

#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "make.h"
#include "hash.h"
#include "dir.h"
#include "job.h"
#include "pathnames.h"
#include "trace.h"

#ifdef USE_IOVEC
#include <sys/uio.h>
#endif

#ifndef	DEFMAXLOCAL
#define	DEFMAXLOCAL DEFMAXJOBS
#endif	/* DEFMAXLOCAL */

Lst			create;		/* Targets to be made */
time_t			now;		/* Time at start of make */
GNode			*DEFAULT;	/* .DEFAULT node */
Boolean			allPrecious;	/* .PRECIOUS given on line by itself */

static Boolean		noBuiltins;	/* -r flag */
static Lst		makefiles;	/* ordered list of makefiles to read */
static Boolean		printVars;	/* print value of one or more vars */
static Lst		variables;	/* list of variables to print */
int			maxJobs;	/* -j argument */
static int		maxJobTokens;	/* -j argument */
Boolean			compatMake;	/* -B argument */
int			debug;		/* -d argument */
Boolean			noExecute;	/* -n flag */
Boolean			noRecursiveExecute;	/* -N flag */
Boolean			keepgoing;	/* -k flag */
Boolean			queryFlag;	/* -q flag */
Boolean			touchFlag;	/* -t flag */
Boolean			ignoreErrors;	/* -i flag */
Boolean			beSilent;	/* -s flag */
Boolean			oldVars;	/* variable substitution style */
Boolean			checkEnvFirst;	/* -e flag */
Boolean			parseWarnFatal;	/* -W flag */
Boolean			jobServer; 	/* -J flag */
static int jp_0 = -1, jp_1 = -1;	/* ends of parent job pipe */
Boolean			varNoExportEnv;	/* -X flag */
Boolean			doing_depend;	/* Set while reading .depend */
static Boolean		jobsRunning;	/* TRUE if the jobs might be running */
static const char *	tracefile;
static char *		Check_Cwd_av(int, char **, int);
static void		MainParseArgs(int, char **);
static int		ReadMakefile(const void *, const void *);
static void		usage(void);

static char curdir[MAXPATHLEN + 1];	/* startup directory */
static char objdir[MAXPATHLEN + 1];	/* where we chdir'ed to */
char *progname;				/* the program name */

Boolean forceJobs = FALSE;

extern Lst parseIncPath;

static void
parse_debug_options(const char *argvalue)
{
	const char *modules;
	const char *mode;
	char *fname;
	int len;

	for (modules = argvalue; *modules; ++modules) {
		switch (*modules) {
		case 'A':
			debug = ~0;
			break;
		case 'a':
			debug |= DEBUG_ARCH;
			break;
		case 'C':
			debug |= DEBUG_CWD;
			break;
		case 'c':
			debug |= DEBUG_COND;
			break;
		case 'd':
			debug |= DEBUG_DIR;
			break;
		case 'e':
			debug |= DEBUG_ERROR;
			break;
		case 'f':
			debug |= DEBUG_FOR;
			break;
		case 'g':
			if (modules[1] == '1') {
				debug |= DEBUG_GRAPH1;
				++modules;
			}
			else if (modules[1] == '2') {
				debug |= DEBUG_GRAPH2;
				++modules;
			}
			else if (modules[1] == '3') {
				debug |= DEBUG_GRAPH3;
				++modules;
			}
			break;
		case 'j':
			debug |= DEBUG_JOB;
			break;
		case 'l':
			debug |= DEBUG_LOUD;
			break;
		case 'm':
			debug |= DEBUG_MAKE;
			break;
		case 'n':
			debug |= DEBUG_SCRIPT;
			break;
		case 'p':
			debug |= DEBUG_PARSE;
			break;
		case 's':
			debug |= DEBUG_SUFF;
			break;
		case 't':
			debug |= DEBUG_TARG;
			break;
		case 'v':
			debug |= DEBUG_VAR;
			break;
		case 'x':
			debug |= DEBUG_SHELL;
			break;
		case 'F':
			if (debug_file != stdout && debug_file != stderr)
				fclose(debug_file);
			if (*++modules == '+')
				mode = "a";
			else
				mode = "w";
			if (strcmp(modules, "stdout") == 0) {
				debug_file = stdout;
				goto debug_setbuf;
			}
			if (strcmp(modules, "stderr") == 0) {
				debug_file = stderr;
				goto debug_setbuf;
			}
			len = strlen(modules);
			fname = malloc(len + 20);
			memcpy(fname, modules, len + 1);
			/* Let the filename be modified by the pid */
			if (strcmp(fname + len - 3, ".%d") == 0)
				snprintf(fname + len - 2, 20, "%d", getpid());
			debug_file = fopen(fname, mode);
			if (!debug_file) {
				fprintf(stderr, "Cannot open debug file %s\n",
				    fname);
				usage();
			}
			free(fname);
			goto debug_setbuf;
		default:
			(void)fprintf(stderr,
			    "%s: illegal argument to d option -- %c\n",
			    progname, *modules);
			usage();
		}
	}
debug_setbuf:
	/*
	 * Make the debug_file unbuffered, and make
	 * stdout line buffered (unless debugfile == stdout).
	 */
	setvbuf(debug_file, NULL, _IONBF, 0);
	if (debug_file != stdout) {
		setvbuf(stdout, NULL, _IOLBF, 0);
	}
}

/*-
 * MainParseArgs --
 *	Parse a given argument vector. Called from main() and from
 *	Main_ParseArgLine() when the .MAKEFLAGS target is used.
 *
 *	XXX: Deal with command line overriding .MAKEFLAGS in makefile
 *
 * Results:
 *	None
 *
 * Side Effects:
 *	Various global and local flags will be set depending on the flags
 *	given
 */
static void
MainParseArgs(int argc, char **argv)
{
	char *p;
	int c = '?';
	int arginc;
	char *argvalue;
	const char *getopt_def;
	char *optscan;
	Boolean inOption, dashDash = FALSE;
	char found_path[MAXPATHLEN + 1];	/* for searching for sys.mk */

#define OPTFLAGS "BD:I:J:NST:V:WXd:ef:ij:km:nqrst"
/* Can't actually use getopt(3) because rescanning is not portable */

	getopt_def = OPTFLAGS;
rearg:	
	inOption = FALSE;
	optscan = NULL;
	while(argc > 1) {
		char *getopt_spec;
		if(!inOption)
			optscan = argv[1];
		c = *optscan++;
		arginc = 0;
		if(inOption) {
			if(c == '\0') {
				++argv;
				--argc;
				inOption = FALSE;
				continue;
			}
		} else {
			if (c != '-' || dashDash)
				break;
			inOption = TRUE;
			c = *optscan++;
		}
		/* '-' found at some earlier point */
		getopt_spec = strchr(getopt_def, c);
		if(c != '\0' && getopt_spec != NULL && getopt_spec[1] == ':') {
			/* -<something> found, and <something> should have an arg */
			inOption = FALSE;
			arginc = 1;
			argvalue = optscan;
			if(*argvalue == '\0') {
				if (argc < 3)
					goto noarg;
				argvalue = argv[2];
				arginc = 2;
			}
		} else {
			argvalue = NULL; 
		}
		switch(c) {
		case '\0':
			arginc = 1;
			inOption = FALSE;
			break;
		case 'B':
			compatMake = TRUE;
			Var_Append(MAKEFLAGS, "-B", VAR_GLOBAL);
			break;
		case 'D':
			if (argvalue == NULL || argvalue[0] == 0) goto noarg;
			Var_Set(argvalue, "1", VAR_GLOBAL, 0);
			Var_Append(MAKEFLAGS, "-D", VAR_GLOBAL);
			Var_Append(MAKEFLAGS, argvalue, VAR_GLOBAL);
			break;
		case 'I':
			if (argvalue == NULL) goto noarg;
			Parse_AddIncludeDir(argvalue);
			Var_Append(MAKEFLAGS, "-I", VAR_GLOBAL);
			Var_Append(MAKEFLAGS, argvalue, VAR_GLOBAL);
			break;
		case 'J':
			if (argvalue == NULL) goto noarg;
			if (sscanf(argvalue, "%d,%d", &jp_0, &jp_1) != 2) {
			    (void)fprintf(stderr,
				"%s: internal error -- J option malformed (%s)\n",
				progname, argvalue);
				usage();
			}
			if ((fcntl(jp_0, F_GETFD, 0) < 0) ||
			    (fcntl(jp_1, F_GETFD, 0) < 0)) {
#if 0
			    (void)fprintf(stderr,
				"%s: ###### warning -- J descriptors were closed!\n",
				progname);
			    exit(2);
#endif
			    jp_0 = -1;
			    jp_1 = -1;
			    compatMake = TRUE;
			} else {
			    Var_Append(MAKEFLAGS, "-J", VAR_GLOBAL);
			    Var_Append(MAKEFLAGS, argvalue, VAR_GLOBAL);
			    jobServer = TRUE;
			}
			break;
		case 'N':
			noExecute = TRUE;
			noRecursiveExecute = TRUE;
			Var_Append(MAKEFLAGS, "-N", VAR_GLOBAL);
			break;
		case 'S':
			keepgoing = FALSE;
			Var_Append(MAKEFLAGS, "-S", VAR_GLOBAL);
			break;
		case 'T':
			if (argvalue == NULL) goto noarg;
			tracefile = bmake_strdup(argvalue);
			Var_Append(MAKEFLAGS, "-T", VAR_GLOBAL);
			Var_Append(MAKEFLAGS, argvalue, VAR_GLOBAL);
			break;
		case 'V':
			if (argvalue == NULL) goto noarg;
			printVars = TRUE;
			(void)Lst_AtEnd(variables, argvalue);
			Var_Append(MAKEFLAGS, "-V", VAR_GLOBAL);
			Var_Append(MAKEFLAGS, argvalue, VAR_GLOBAL);
			break;
		case 'W':
			parseWarnFatal = TRUE;
			break;
		case 'X':
			varNoExportEnv = TRUE;
			Var_Append(MAKEFLAGS, "-X", VAR_GLOBAL);
			break;
		case 'd':
			if (argvalue == NULL) goto noarg;
			/* If '-d-opts' don't pass to children */
			if (argvalue[0] == '-')
			    argvalue++;
			else {
			    Var_Append(MAKEFLAGS, "-d", VAR_GLOBAL);
			    Var_Append(MAKEFLAGS, argvalue, VAR_GLOBAL);
			}
			parse_debug_options(argvalue);
			break;
		case 'e':
			checkEnvFirst = TRUE;
			Var_Append(MAKEFLAGS, "-e", VAR_GLOBAL);
			break;
		case 'f':
			if (argvalue == NULL) goto noarg;
			(void)Lst_AtEnd(makefiles, argvalue);
			break;
		case 'i':
			ignoreErrors = TRUE;
			Var_Append(MAKEFLAGS, "-i", VAR_GLOBAL);
			break;
		case 'j':
			if (argvalue == NULL) goto noarg;
			forceJobs = TRUE;
			maxJobs = strtol(argvalue, &p, 0);
			if (*p != '\0' || maxJobs < 1) {
				(void)fprintf(stderr, "%s: illegal argument to -j -- must be positive integer!\n",
				    progname);
				exit(1);
			}
			Var_Append(MAKEFLAGS, "-j", VAR_GLOBAL);
			Var_Append(MAKEFLAGS, argvalue, VAR_GLOBAL);
			maxJobTokens = maxJobs;
			break;
		case 'k':
			keepgoing = TRUE;
			Var_Append(MAKEFLAGS, "-k", VAR_GLOBAL);
			break;
		case 'm':
			if (argvalue == NULL) goto noarg;
			/* look for magic parent directory search string */
			if (strncmp(".../", argvalue, 4) == 0) {
				if (!Dir_FindHereOrAbove(curdir, argvalue+4,
				    found_path, sizeof(found_path)))
					break;		/* nothing doing */
				(void)Dir_AddDir(sysIncPath, found_path);
				
			} else {
				(void)Dir_AddDir(sysIncPath, argvalue);
			}
			Var_Append(MAKEFLAGS, "-m", VAR_GLOBAL);
			Var_Append(MAKEFLAGS, argvalue, VAR_GLOBAL);
			break;
		case 'n':
			noExecute = TRUE;
			Var_Append(MAKEFLAGS, "-n", VAR_GLOBAL);
			break;
		case 'q':
			queryFlag = TRUE;
			/* Kind of nonsensical, wot? */
			Var_Append(MAKEFLAGS, "-q", VAR_GLOBAL);
			break;
		case 'r':
			noBuiltins = TRUE;
			Var_Append(MAKEFLAGS, "-r", VAR_GLOBAL);
			break;
		case 's':
			beSilent = TRUE;
			Var_Append(MAKEFLAGS, "-s", VAR_GLOBAL);
			break;
		case 't':
			touchFlag = TRUE;
			Var_Append(MAKEFLAGS, "-t", VAR_GLOBAL);
			break;
		case '-':
			dashDash = TRUE;
			break;
		default:
		case '?':
			usage();
		}
		argv += arginc;
		argc -= arginc;
	}

	oldVars = TRUE;

	/*
	 * See if the rest of the arguments are variable assignments and
	 * perform them if so. Else take them to be targets and stuff them
	 * on the end of the "create" list.
	 */
	for (; argc > 1; ++argv, --argc)
		if (Parse_IsVar(argv[1])) {
			Parse_DoVar(argv[1], VAR_CMD);
		} else {
			if (!*argv[1])
				Punt("illegal (null) argument.");
			if (*argv[1] == '-' && !dashDash)
				goto rearg;
			(void)Lst_AtEnd(create, bmake_strdup(argv[1]));
		}

	return;
noarg:
	(void)fprintf(stderr, "%s: option requires an argument -- %c\n",
	    progname, c);
	usage();
}

/*-
 * Main_ParseArgLine --
 *  	Used by the parse module when a .MFLAGS or .MAKEFLAGS target
 *	is encountered and by main() when reading the .MAKEFLAGS envariable.
 *	Takes a line of arguments and breaks it into its
 * 	component words and passes those words and the number of them to the
 *	MainParseArgs function.
 *	The line should have all its leading whitespace removed.
 *
 * Input:
 *	line		Line to fracture
 *
 * Results:
 *	None
 *
 * Side Effects:
 *	Only those that come from the various arguments.
 */
void
Main_ParseArgLine(const char *line)
{
	char **argv;			/* Manufactured argument vector */
	int argc;			/* Number of arguments in argv */
	char *args;			/* Space used by the args */
	char *buf, *p1;
	char *argv0 = Var_Value(".MAKE", VAR_GLOBAL, &p1);
	size_t len;

	if (line == NULL)
		return;
	for (; *line == ' '; ++line)
		continue;
	if (!*line)
		return;

	buf = bmake_malloc(len = strlen(line) + strlen(argv0) + 2);
	(void)snprintf(buf, len, "%s %s", argv0, line);
	if (p1)
		free(p1);

	argv = brk_string(buf, &argc, TRUE, &args);
	if (argv == NULL) {
		Error("Unterminated quoted string [%s]", buf);
		free(buf);
		return;
	}
	free(buf);
	MainParseArgs(argc, argv);

	free(args);
	free(argv);
}

Boolean
Main_SetObjdir(const char *path)
{
	struct stat sb;
	char *p = NULL;
	char buf[MAXPATHLEN + 1];
	Boolean rc = FALSE;

	/* expand variable substitutions */
	if (strchr(path, '$') != 0) {
		snprintf(buf, MAXPATHLEN, "%s", path);
		path = p = Var_Subst(NULL, buf, VAR_GLOBAL, 0);
	}

	if (path[0] != '/') {
		snprintf(buf, MAXPATHLEN, "%s/%s", curdir, path);
		path = buf;
	}

	/* look for the directory and try to chdir there */
	if (stat(path, &sb) == 0 && S_ISDIR(sb.st_mode)) {
		if (chdir(path)) {
			(void)fprintf(stderr, "make warning: %s: %s.\n",
				      path, strerror(errno));
		} else {
			strncpy(objdir, path, MAXPATHLEN);
			Var_Set(".OBJDIR", objdir, VAR_GLOBAL, 0);
			setenv("PWD", objdir, 1);
			Dir_InitDot();
			rc = TRUE;
		}
	}

	if (p)
		free(p);
	return rc;
}

/*-
 * ReadAllMakefiles --
 *	wrapper around ReadMakefile() to read all.
 *
 * Results:
 *	TRUE if ok, FALSE on error
 */
static int
ReadAllMakefiles(const void *p, const void *q)
{
	return (ReadMakefile(p, q) == 0);
}

#ifdef SIGINFO
/*ARGSUSED*/
static void
siginfo(int signo)
{
	char dir[MAXPATHLEN];
	char str[2 * MAXPATHLEN];
	int len;
	if (getcwd(dir, sizeof(dir)) == NULL)
		return;
	len = snprintf(str, sizeof(str), "%s: Working in: %s\n", getprogname(),
	    dir);
	if (len > 0)
		(void)write(STDERR_FILENO, str, (size_t)len);
}
#endif

/*-
 * main --
 *	The main function, for obvious reasons. Initializes variables
 *	and a few modules, then parses the arguments give it in the
 *	environment and on the command line. Reads the system makefile
 *	followed by either Makefile, makefile or the file given by the
 *	-f argument. Sets the .MAKEFLAGS PMake variable based on all the
 *	flags it has received by then uses either the Make or the Compat
 *	module to create the initial list of targets.
 *
 * Results:
 *	If -q was given, exits -1 if anything was out-of-date. Else it exits
 *	0.
 *
 * Side Effects:
 *	The program exits when done. Targets are created. etc. etc. etc.
 */
int
main(int argc, char **argv)
{
	Lst targs;	/* target nodes to create -- passed to Make_Init */
	Boolean outOfDate = FALSE; 	/* FALSE if all targets up to date */
	struct stat sb, sa;
	char *p1, *path, *pwd;
	char mdpath[MAXPATHLEN];
    	char *machine = getenv("MACHINE");
	const char *machine_arch = getenv("MACHINE_ARCH");
	char *syspath = getenv("MAKESYSPATH");
	Lst sysMkPath;			/* Path of sys.mk */
	char *cp = NULL, *start;
					/* avoid faults on read-only strings */
	static char defsyspath[] = _PATH_DEFSYSPATH;
	char found_path[MAXPATHLEN + 1];	/* for searching for sys.mk */
	struct timeval rightnow;		/* to initialize random seed */
#ifdef MAKE_NATIVE
	struct utsname utsname;
#endif

	/* default to writing debug to stderr */
	debug_file = stderr;

#ifdef SIGINFO
	(void)signal(SIGINFO, siginfo);
#endif
	/*
	 * Set the seed to produce a different random sequence
	 * on each program execution.
	 */
	gettimeofday(&rightnow, NULL);
	srandom(rightnow.tv_sec + rightnow.tv_usec);
	
	if ((progname = strrchr(argv[0], '/')) != NULL)
		progname++;
	else
		progname = argv[0];
#ifdef RLIMIT_NOFILE
	/*
	 * get rid of resource limit on file descriptors
	 */
	{
		struct rlimit rl;
		if (getrlimit(RLIMIT_NOFILE, &rl) != -1 &&
		    rl.rlim_cur != rl.rlim_max) {
			rl.rlim_cur = rl.rlim_max;
			(void)setrlimit(RLIMIT_NOFILE, &rl);
		}
	}
#endif
	/*
	 * Find where we are and take care of PWD for the automounter...
	 * All this code is so that we know where we are when we start up
	 * on a different machine with pmake.
	 */
	if (getcwd(curdir, MAXPATHLEN) == NULL) {
		(void)fprintf(stderr, "%s: %s.\n", progname, strerror(errno));
		exit(2);
	}

	if (stat(curdir, &sa) == -1) {
	    (void)fprintf(stderr, "%s: %s: %s.\n",
		 progname, curdir, strerror(errno));
	    exit(2);
	}

	/*
	 * Overriding getcwd() with $PWD totally breaks MAKEOBJDIRPREFIX
	 * since the value of curdir can very depending on how we got
	 * here.  Ie sitting at a shell prompt (shell that provides $PWD)
	 * or via subdir.mk in which case its likely a shell which does
	 * not provide it.
	 * So, to stop it breaking this case only, we ignore PWD if
	 * MAKEOBJDIRPREFIX is set or MAKEOBJDIR contains a transform.
	 */
	if ((pwd = getenv("PWD")) != NULL && getenv("MAKEOBJDIRPREFIX") == NULL) {
		const char *makeobjdir = getenv("MAKEOBJDIR");

		if (makeobjdir == NULL || !strchr(makeobjdir, '$')) {
			if (stat(pwd, &sb) == 0 && sa.st_ino == sb.st_ino &&
			    sa.st_dev == sb.st_dev)
				(void)strncpy(curdir, pwd, MAXPATHLEN);
		}
	}

	/*
	 * Get the name of this type of MACHINE from utsname
	 * so we can share an executable for similar machines.
	 * (i.e. m68k: amiga hp300, mac68k, sun3, ...)
	 *
	 * Note that both MACHINE and MACHINE_ARCH are decided at
	 * run-time.
	 */
	if (!machine) {
#ifdef MAKE_NATIVE
	    if (uname(&utsname) == -1) {
		(void)fprintf(stderr, "%s: uname failed (%s).\n", progname,
		    strerror(errno));
		exit(2);
	    }
	    machine = utsname.machine;
#else
#ifdef MAKE_MACHINE
	    machine = MAKE_MACHINE;
#else
	    machine = "unknown";
#endif
#endif
	}

	if (!machine_arch) {
#ifndef MACHINE_ARCH
#ifdef MAKE_MACHINE_ARCH
            machine_arch = MAKE_MACHINE_ARCH;
#else
	    machine_arch = "unknown";
#endif
#else
	    machine_arch = MACHINE_ARCH;
#endif
	}

	/*
	 * Just in case MAKEOBJDIR wants us to do something tricky.
	 */
	Var_Init();		/* Initialize the lists of variables for
				 * parsing arguments */
	Var_Set(".CURDIR", curdir, VAR_GLOBAL, 0);
	Var_Set("MACHINE", machine, VAR_GLOBAL, 0);
	Var_Set("MACHINE_ARCH", machine_arch, VAR_GLOBAL, 0);
#ifdef MAKE_VERSION
	Var_Set("MAKE_VERSION", MAKE_VERSION, VAR_GLOBAL, 0);
#endif
	Var_Set(".newline", "\n", VAR_GLOBAL, 0); /* handy for :@ loops */

	/*
	 * Find the .OBJDIR.  If MAKEOBJDIRPREFIX, or failing that,
	 * MAKEOBJDIR is set in the environment, try only that value
	 * and fall back to .CURDIR if it does not exist.
	 *
	 * Otherwise, try _PATH_OBJDIR.MACHINE, _PATH_OBJDIR, and
	 * finally _PATH_OBJDIRPREFIX`pwd`, in that order.  If none
	 * of these paths exist, just use .CURDIR.
	 */
	Dir_Init(curdir);
	(void)Main_SetObjdir(curdir);

	if ((path = getenv("MAKEOBJDIRPREFIX")) != NULL) {
		(void)snprintf(mdpath, MAXPATHLEN, "%s%s", path, curdir);
		(void)Main_SetObjdir(mdpath);
	} else if ((path = getenv("MAKEOBJDIR")) != NULL) {
		(void)Main_SetObjdir(path);
	} else {
		(void)snprintf(mdpath, MAXPATHLEN, "%s.%s", _PATH_OBJDIR, machine);
		if (!Main_SetObjdir(mdpath) && !Main_SetObjdir(_PATH_OBJDIR)) {
			(void)snprintf(mdpath, MAXPATHLEN, "%s%s", 
					_PATH_OBJDIRPREFIX, curdir);
			(void)Main_SetObjdir(mdpath);
		}
	}

	create = Lst_Init(FALSE);
	makefiles = Lst_Init(FALSE);
	printVars = FALSE;
	variables = Lst_Init(FALSE);
	beSilent = FALSE;		/* Print commands as executed */
	ignoreErrors = FALSE;		/* Pay attention to non-zero returns */
	noExecute = FALSE;		/* Execute all commands */
	noRecursiveExecute = FALSE;	/* Execute all .MAKE targets */
	keepgoing = FALSE;		/* Stop on error */
	allPrecious = FALSE;		/* Remove targets when interrupted */
	queryFlag = FALSE;		/* This is not just a check-run */
	noBuiltins = FALSE;		/* Read the built-in rules */
	touchFlag = FALSE;		/* Actually update targets */
	debug = 0;			/* No debug verbosity, please. */
	jobsRunning = FALSE;

	maxJobs = DEFMAXLOCAL;		/* Set default local max concurrency */
	maxJobTokens = maxJobs;
	compatMake = FALSE;		/* No compat mode */


	/*
	 * Initialize the parsing, directory and variable modules to prepare
	 * for the reading of inclusion paths and variable settings on the
	 * command line
	 */

	/*
	 * Initialize various variables.
	 *	MAKE also gets this name, for compatibility
	 *	.MAKEFLAGS gets set to the empty string just in case.
	 *	MFLAGS also gets initialized empty, for compatibility.
	 */
	Parse_Init();
	Var_Set("MAKE", argv[0], VAR_GLOBAL, 0);
	Var_Set(".MAKE", argv[0], VAR_GLOBAL, 0);
	Var_Set(MAKEFLAGS, "", VAR_GLOBAL, 0);
	Var_Set(MAKEOVERRIDES, "", VAR_GLOBAL, 0);
	Var_Set("MFLAGS", "", VAR_GLOBAL, 0);
	Var_Set(".ALLTARGETS", "", VAR_GLOBAL, 0);

	/*
	 * Set some other useful macros
	 */
	{
	    char tmp[64];

	    snprintf(tmp, sizeof(tmp), "%u", getpid());
	    Var_Set(".MAKE.PID", tmp, VAR_GLOBAL, 0);
	    snprintf(tmp, sizeof(tmp), "%u", getppid());
	    Var_Set(".MAKE.PPID", tmp, VAR_GLOBAL, 0);
	}
	Job_SetPrefix();

	/*
	 * First snag any flags out of the MAKE environment variable.
	 * (Note this is *not* MAKEFLAGS since /bin/make uses that and it's
	 * in a different format).
	 */
#ifdef POSIX
	Main_ParseArgLine(getenv("MAKEFLAGS"));
#else
	Main_ParseArgLine(getenv("MAKE"));
#endif

	MainParseArgs(argc, argv);

	/*
	 * Be compatible if user did not specify -j and did not explicitly
	 * turned compatibility on
	 */
	if (!compatMake && !forceJobs) {
		compatMake = TRUE;
	}
	
	/*
	 * Initialize archive, target and suffix modules in preparation for
	 * parsing the makefile(s)
	 */
	Arch_Init();
	Targ_Init();
	Suff_Init();
	Trace_Init(tracefile);

	DEFAULT = NULL;
	(void)time(&now);

	Trace_Log(MAKESTART, NULL);
	
	/*
	 * Set up the .TARGETS variable to contain the list of targets to be
	 * created. If none specified, make the variable empty -- the parser
	 * will fill the thing in with the default or .MAIN target.
	 */
	if (!Lst_IsEmpty(create)) {
		LstNode ln;

		for (ln = Lst_First(create); ln != NULL;
		    ln = Lst_Succ(ln)) {
			char *name = (char *)Lst_Datum(ln);

			Var_Append(".TARGETS", name, VAR_GLOBAL);
		}
	} else
		Var_Set(".TARGETS", "", VAR_GLOBAL, 0);


	/*
	 * If no user-supplied system path was given (through the -m option)
	 * add the directories from the DEFSYSPATH (more than one may be given
	 * as dir1:...:dirn) to the system include path.
	 */
	if (syspath == NULL || *syspath == '\0')
		syspath = defsyspath;
	else
		syspath = bmake_strdup(syspath);

	for (start = syspath; *start != '\0'; start = cp) {
		for (cp = start; *cp != '\0' && *cp != ':'; cp++)
			continue;
		if (*cp == ':') {
			*cp++ = '\0';
		}
		/* look for magic parent directory search string */
		if (strncmp(".../", start, 4) != 0) {
			(void)Dir_AddDir(defIncPath, start);
		} else {
			if (Dir_FindHereOrAbove(curdir, start+4, 
			    found_path, sizeof(found_path))) {
				(void)Dir_AddDir(defIncPath, found_path);
			}
		}
	}
	if (syspath != defsyspath)
		free(syspath);

	/*
	 * Read in the built-in rules first, followed by the specified
	 * makefile, if it was (makefile != NULL), or the default
	 * makefile and Makefile, in that order, if it wasn't.
	 */
	if (!noBuiltins) {
		LstNode ln;

		sysMkPath = Lst_Init(FALSE);
		Dir_Expand(_PATH_DEFSYSMK,
			   Lst_IsEmpty(sysIncPath) ? defIncPath : sysIncPath,
			   sysMkPath);
		if (Lst_IsEmpty(sysMkPath))
			Fatal("%s: no system rules (%s).", progname,
			    _PATH_DEFSYSMK);
		ln = Lst_Find(sysMkPath, NULL, ReadMakefile);
		if (ln == NULL)
			Fatal("%s: cannot open %s.", progname,
			    (char *)Lst_Datum(ln));
	}

	if (!Lst_IsEmpty(makefiles)) {
		LstNode ln;

		ln = Lst_Find(makefiles, NULL, ReadAllMakefiles);
		if (ln != NULL)
			Fatal("%s: cannot open %s.", progname, 
			    (char *)Lst_Datum(ln));
	} else if (ReadMakefile("makefile", NULL) != 0)
		(void)ReadMakefile("Makefile", NULL);

	/* In particular suppress .depend for '-r -V .OBJDIR -f /dev/null' */
	if (!noBuiltins || !printVars) {
		doing_depend = TRUE;
		(void)ReadMakefile(".depend", NULL);
		doing_depend = FALSE;
	}

	Var_Append("MFLAGS", Var_Value(MAKEFLAGS, VAR_GLOBAL, &p1), VAR_GLOBAL);
	if (p1)
	    free(p1);

	if (!compatMake)
	    Job_ServerStart(maxJobTokens, jp_0, jp_1);
	if (DEBUG(JOB))
	    fprintf(debug_file, "job_pipe %d %d, maxjobs %d, tokens %d, compat %d\n",
		jp_0, jp_1, maxJobs, maxJobTokens, compatMake);

	Main_ExportMAKEFLAGS(TRUE);	/* initial export */

	Check_Cwd_av(0, NULL, 0);	/* initialize it */
	

	/*
	 * For compatibility, look at the directories in the VPATH variable
	 * and add them to the search path, if the variable is defined. The
	 * variable's value is in the same format as the PATH envariable, i.e.
	 * <directory>:<directory>:<directory>...
	 */
	if (Var_Exists("VPATH", VAR_CMD)) {
		char *vpath, savec;
		/*
		 * GCC stores string constants in read-only memory, but
		 * Var_Subst will want to write this thing, so store it
		 * in an array
		 */
		static char VPATH[] = "${VPATH}";

		vpath = Var_Subst(NULL, VPATH, VAR_CMD, FALSE);
		path = vpath;
		do {
			/* skip to end of directory */
			for (cp = path; *cp != ':' && *cp != '\0'; cp++)
				continue;
			/* Save terminator character so know when to stop */
			savec = *cp;
			*cp = '\0';
			/* Add directory to search path */
			(void)Dir_AddDir(dirSearchPath, path);
			*cp = savec;
			path = cp + 1;
		} while (savec == ':');
		free(vpath);
	}

	/*
	 * Now that all search paths have been read for suffixes et al, it's
	 * time to add the default search path to their lists...
	 */
	Suff_DoPaths();

	/*
	 * Propagate attributes through :: dependency lists.
	 */
	Targ_Propagate();

	/* print the initial graph, if the user requested it */
	if (DEBUG(GRAPH1))
		Targ_PrintGraph(1);

	/* print the values of any variables requested by the user */
	if (printVars) {
		LstNode ln;

		for (ln = Lst_First(variables); ln != NULL;
		    ln = Lst_Succ(ln)) {
			char *var = (char *)Lst_Datum(ln);
			char *value;
			
			if (strchr(var, '$')) {
				value = p1 = Var_Subst(NULL, var, VAR_GLOBAL, 0);
			} else {
				value = Var_Value(var, VAR_GLOBAL, &p1);
			}
			printf("%s\n", value ? value : "");
			if (p1)
				free(p1);
		}
	} else {
		/*
		 * Have now read the entire graph and need to make a list of
		 * targets to create. If none was given on the command line,
		 * we consult the parsing module to find the main target(s)
		 * to create.
		 */
		if (Lst_IsEmpty(create))
			targs = Parse_MainName();
		else
			targs = Targ_FindList(create, TARG_CREATE);

		if (!compatMake) {
			/*
			 * Initialize job module before traversing the graph
			 * now that any .BEGIN and .END targets have been read.
			 * This is done only if the -q flag wasn't given
			 * (to prevent the .BEGIN from being executed should
			 * it exist).
			 */
			if (!queryFlag) {
				Job_Init();
				jobsRunning = TRUE;
			}

			/* Traverse the graph, checking on all the targets */
			outOfDate = Make_Run(targs);
		} else {
			/*
			 * Compat_Init will take care of creating all the
			 * targets as well as initializing the module.
			 */
			Compat_Run(targs);
		}
	}

#ifdef CLEANUP
	Lst_Destroy(targs, NULL);
	Lst_Destroy(variables, NULL);
	Lst_Destroy(makefiles, NULL);
	Lst_Destroy(create, (FreeProc *)free);
#endif

	/* print the graph now it's been processed if the user requested it */
	if (DEBUG(GRAPH2))
		Targ_PrintGraph(2);

	Trace_Log(MAKEEND, 0);

	Suff_End();
        Targ_End();
	Arch_End();
	Var_End();
	Parse_End();
	Dir_End();
	Job_End();
	Trace_End();

	return outOfDate ? 1 : 0;
}

/*-
 * ReadMakefile  --
 *	Open and parse the given makefile.
 *
 * Results:
 *	0 if ok. -1 if couldn't open file.
 *
 * Side Effects:
 *	lots
 */
static int
ReadMakefile(const void *p, const void *q __unused)
{
	const char *fname = p;		/* makefile to read */
	int fd;
	size_t len = MAXPATHLEN;
	char *name, *path = bmake_malloc(len);
	int setMAKEFILE;

	if (!strcmp(fname, "-")) {
		Parse_File("(stdin)", dup(fileno(stdin)));
		Var_Set("MAKEFILE", "", VAR_GLOBAL, 0);
	} else {
		setMAKEFILE = strcmp(fname, ".depend");

		/* if we've chdir'd, rebuild the path name */
		if (strcmp(curdir, objdir) && *fname != '/') {
			size_t plen = strlen(curdir) + strlen(fname) + 2;
			if (len < plen)
				path = bmake_realloc(path, len = 2 * plen);
			
			(void)snprintf(path, len, "%s/%s", curdir, fname);
			fd = open(path, O_RDONLY);
			if (fd != -1) {
				fname = path;
				goto found;
			}
			
			/* If curdir failed, try objdir (ala .depend) */
			plen = strlen(objdir) + strlen(fname) + 2;
			if (len < plen)
				path = bmake_realloc(path, len = 2 * plen);
			(void)snprintf(path, len, "%s/%s", objdir, fname);
			fd = open(path, O_RDONLY);
			if (fd != -1) {
				fname = path;
				goto found;
			}
		} else {
			fd = open(fname, O_RDONLY);
			if (fd != -1)
				goto found;
		}
		/* look in -I and system include directories. */
		name = Dir_FindFile(fname, parseIncPath);
		if (!name)
			name = Dir_FindFile(fname,
				Lst_IsEmpty(sysIncPath) ? defIncPath : sysIncPath);
		if (!name || (fd = open(name, O_RDONLY)) == -1) {
			if (name)
				free(name);
			free(path);
			return(-1);
		}
		fname = name;
		/*
		 * set the MAKEFILE variable desired by System V fans -- the
		 * placement of the setting here means it gets set to the last
		 * makefile specified, as it is set by SysV make.
		 */
found:
		if (setMAKEFILE)
			Var_Set("MAKEFILE", fname, VAR_GLOBAL, 0);
		Parse_File(fname, fd);
	}
	free(path);
	return(0);
}


/*
 * If MAKEOBJDIRPREFIX is in use, make ends up not in .CURDIR
 * in situations that would not arrise with ./obj (links or not).
 * This tends to break things like:
 *
 * build:
 * 	${MAKE} includes
 *
 * This function spots when ${.MAKE:T} or ${.MAKE} is a command (as
 * opposed to an argument) in a command line and if so returns
 * ${.CURDIR} so caller can chdir() so that the assumptions made by
 * the Makefile hold true.
 *
 * If ${.MAKE} does not contain any '/', then ${.MAKE:T} is skipped.
 *
 * The chdir() only happens in the child process, and does nothing if
 * MAKEOBJDIRPREFIX and MAKEOBJDIR are not in the environment so it
 * should not break anything.  Also if NOCHECKMAKECHDIR is set we
 * do nothing - to ensure historic semantics can be retained.
 */
static int  Check_Cwd_Off = 0;

static char *
Check_Cwd_av(int ac, char **av, int copy)
{
    static char *make[4];
    static char *cur_dir = NULL;
    char **mp;
    char *cp;
    int is_cmd, next_cmd;
    int i;
    int n;

    if (Check_Cwd_Off) {
	if (DEBUG(CWD))
	    fprintf(debug_file, "check_cwd: check is off.\n");
	return NULL;
    }
    
    if (make[0] == NULL) {
	if (Var_Exists("NOCHECKMAKECHDIR", VAR_GLOBAL)) {
	    Check_Cwd_Off = 1;
	    if (DEBUG(CWD))
		fprintf(debug_file, "check_cwd: turning check off.\n");
	    return NULL;
	}
	    
        make[1] = Var_Value(".MAKE", VAR_GLOBAL, &cp);
        if ((make[0] = strrchr(make[1], '/')) == NULL) {
            make[0] = make[1];
            make[1] = NULL;
        } else
            ++make[0];
        make[2] = NULL;
        cur_dir = Var_Value(".CURDIR", VAR_GLOBAL, &cp);
    }
    if (ac == 0 || av == NULL) {
	if (DEBUG(CWD))
	    fprintf(debug_file, "check_cwd: empty command.\n");
        return NULL;			/* initialization only */
    }

    if (getenv("MAKEOBJDIR") == NULL &&
        getenv("MAKEOBJDIRPREFIX") == NULL) {
	if (DEBUG(CWD))
	    fprintf(debug_file, "check_cwd: no obj dirs.\n");
        return NULL;
    }

    
    next_cmd = 1;
    for (i = 0; i < ac; ++i) {
	is_cmd = next_cmd;

	n = strlen(av[i]);
	cp = &(av[i])[n - 1];
	if (strspn(av[i], "|&;") == (size_t)n) {
	    next_cmd = 1;
	    continue;
	} else if (*cp == ';' || *cp == '&' || *cp == '|' || *cp == ')') {
	    next_cmd = 1;
	    if (copy) {
		do {
		    *cp-- = '\0';
		} while (*cp == ';' || *cp == '&' || *cp == '|' ||
			 *cp == ')' || *cp == '}') ;
	    } else {
		/*
		 * XXX this should not happen.
		 */
		fprintf(stderr, "%s: WARNING: raw arg ends in shell meta '%s'\n",
		    progname, av[i]);
	    }
	} else
	    next_cmd = 0;

	cp = av[i];
	if (*cp == ';' || *cp == '&' || *cp == '|')
	    is_cmd = 1;
	
	if (DEBUG(CWD))
	    fprintf(debug_file, "av[%d] == %s '%s'",
		i, (is_cmd) ? "cmd" : "arg", av[i]);
	if (is_cmd != 0) {
	    if (*cp == '(' || *cp == '{' ||
		*cp == ';' || *cp == '&' || *cp == '|') {
		do {
		    ++cp;
		} while (*cp == '(' || *cp == '{' ||
			 *cp == ';' || *cp == '&' || *cp == '|');
		if (*cp == '\0') {
		    next_cmd = 1;
		    continue;
		}
	    }
	    if (strcmp(cp, "cd") == 0 || strcmp(cp, "chdir") == 0) {
		if (DEBUG(CWD))
		    fprintf(debug_file, " == cd, done.\n");
		return NULL;
	    }
	    for (mp = make; *mp != NULL; ++mp) {
		n = strlen(*mp);
		if (strcmp(cp, *mp) == 0) {
		    if (DEBUG(CWD))
			fprintf(debug_file, " %s == '%s', chdir(%s)\n",
			    cp, *mp, cur_dir);
		    return cur_dir;
		}
	    }
	}
	if (DEBUG(CWD))
	    fprintf(debug_file, "\n");
    }
    return NULL;
}

char *
Check_Cwd_Cmd(const char *cmd)
{
    char *cp, *bp;
    char **av;
    int ac;

    if (Check_Cwd_Off)
	return NULL;
    
    if (cmd) {
	av = brk_string(cmd, &ac, TRUE, &bp);
	if (DEBUG(CWD))
	    fprintf(debug_file, "splitting: '%s' -> %d words\n",
		cmd, ac);
    } else {
	ac = 0;
	av = NULL;
	bp = NULL;
    }
    cp = Check_Cwd_av(ac, av, 1);
    if (bp)
	free(bp);
    if (av)
	free(av);
    return cp;
}

void
Check_Cwd(const char **argv)
{
    char *cp;
    int ac;
    
    if (Check_Cwd_Off)
	return;
    
    for (ac = 0; argv[ac] != NULL; ++ac)
	/* NOTHING */;
    if (ac == 3 && *argv[1] == '-') {
	cp =  Check_Cwd_Cmd(argv[2]);
    } else {
	cp = Check_Cwd_av(ac, UNCONST(argv), 0);
    }
    if (cp) {
	chdir(cp);
    }
}

/*-
 * Cmd_Exec --
 *	Execute the command in cmd, and return the output of that command
 *	in a string.
 *
 * Results:
 *	A string containing the output of the command, or the empty string
 *	If errnum is not NULL, it contains the reason for the command failure
 *
 * Side Effects:
 *	The string must be freed by the caller.
 */
char *
Cmd_Exec(const char *cmd, const char **errnum)
{
    const char	*args[4];   	/* Args for invoking the shell */
    int 	fds[2];	    	/* Pipe streams */
    int 	cpid;	    	/* Child PID */
    int 	pid;	    	/* PID from wait() */
    char	*res;		/* result */
    int		status;		/* command exit status */
    Buffer	buf;		/* buffer to store the result */
    char	*cp;
    int		cc;


    *errnum = NULL;

    if (!shellName)
	Shell_Init();
    /*
     * Set up arguments for shell
     */
    args[0] = shellName;
    args[1] = "-c";
    args[2] = cmd;
    args[3] = NULL;

    /*
     * Open a pipe for fetching its output
     */
    if (pipe(fds) == -1) {
	*errnum = "Couldn't create pipe for \"%s\"";
	goto bad;
    }

    /*
     * Fork
     */
    switch (cpid = vfork()) {
    case 0:
	/*
	 * Close input side of pipe
	 */
	(void)close(fds[0]);

	/*
	 * Duplicate the output stream to the shell's output, then
	 * shut the extra thing down. Note we don't fetch the error
	 * stream...why not? Why?
	 */
	(void)dup2(fds[1], 1);
	(void)close(fds[1]);

	Var_ExportVars();

	(void)execv(shellPath, UNCONST(args));
	_exit(1);
	/*NOTREACHED*/

    case -1:
	*errnum = "Couldn't exec \"%s\"";
	goto bad;

    default:
	/*
	 * No need for the writing half
	 */
	(void)close(fds[1]);

	Buf_Init(&buf, 0);

	do {
	    char   result[BUFSIZ];
	    cc = read(fds[0], result, sizeof(result));
	    if (cc > 0)
		Buf_AddBytes(&buf, cc, result);
	}
	while (cc > 0 || (cc == -1 && errno == EINTR));

	/*
	 * Close the input side of the pipe.
	 */
	(void)close(fds[0]);

	/*
	 * Wait for the process to exit.
	 */
	while(((pid = waitpid(cpid, &status, 0)) != cpid) && (pid >= 0))
	    continue;

	cc = Buf_Size(&buf);
	res = Buf_Destroy(&buf, FALSE);

	if (cc == 0)
	    *errnum = "Couldn't read shell's output for \"%s\"";

	if (status)
	    *errnum = "\"%s\" returned non-zero status";

	/*
	 * Null-terminate the result, convert newlines to spaces and
	 * install it in the variable.
	 */
	res[cc] = '\0';
	cp = &res[cc];

	if (cc > 0 && *--cp == '\n') {
	    /*
	     * A final newline is just stripped
	     */
	    *cp-- = '\0';
	}
	while (cp >= res) {
	    if (*cp == '\n') {
		*cp = ' ';
	    }
	    cp--;
	}
	break;
    }
    return res;
bad:
    res = bmake_malloc(1);
    *res = '\0';
    return res;
}

/*-
 * Error --
 *	Print an error message given its format.
 *
 * Results:
 *	None.
 *
 * Side Effects:
 *	The message is printed.
 */
/* VARARGS */
void
Error(const char *fmt, ...)
{
	va_list ap;
	FILE *err_file;

	err_file = debug_file;
	if (err_file == stdout)
		err_file = stderr;
	for (;;) {
		va_start(ap, fmt);
		fprintf(err_file, "%s: ", progname);
		(void)vfprintf(err_file, fmt, ap);
		va_end(ap);
		(void)fprintf(err_file, "\n");
		(void)fflush(err_file);
		if (err_file == stderr)
			break;
		err_file = stderr;
	}
}

/*-
 * Fatal --
 *	Produce a Fatal error message. If jobs are running, waits for them
 *	to finish.
 *
 * Results:
 *	None
 *
 * Side Effects:
 *	The program exits
 */
/* VARARGS */
void
Fatal(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	if (jobsRunning)
		Job_Wait();

	(void)vfprintf(stderr, fmt, ap);
	va_end(ap);
	(void)fprintf(stderr, "\n");
	(void)fflush(stderr);

	PrintOnError(NULL);

	if (DEBUG(GRAPH2) || DEBUG(GRAPH3))
		Targ_PrintGraph(2);
	Trace_Log(MAKEERROR, 0);
	exit(2);		/* Not 1 so -q can distinguish error */
}

/*
 * Punt --
 *	Major exception once jobs are being created. Kills all jobs, prints
 *	a message and exits.
 *
 * Results:
 *	None
 *
 * Side Effects:
 *	All children are killed indiscriminately and the program Lib_Exits
 */
/* VARARGS */
void
Punt(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	(void)fprintf(stderr, "%s: ", progname);
	(void)vfprintf(stderr, fmt, ap);
	va_end(ap);
	(void)fprintf(stderr, "\n");
	(void)fflush(stderr);

	PrintOnError(NULL);

	DieHorribly();
}

/*-
 * DieHorribly --
 *	Exit without giving a message.
 *
 * Results:
 *	None
 *
 * Side Effects:
 *	A big one...
 */
void
DieHorribly(void)
{
	if (jobsRunning)
		Job_AbortAll();
	if (DEBUG(GRAPH2))
		Targ_PrintGraph(2);
	Trace_Log(MAKEERROR, 0);
	exit(2);		/* Not 1, so -q can distinguish error */
}

/*
 * Finish --
 *	Called when aborting due to errors in child shell to signal
 *	abnormal exit.
 *
 * Results:
 *	None
 *
 * Side Effects:
 *	The program exits
 */
void
Finish(int errors)
	           	/* number of errors encountered in Make_Make */
{
	Fatal("%d error%s", errors, errors == 1 ? "" : "s");
}

/*
 * enunlink --
 *	Remove a file carefully, avoiding directories.
 */
int
eunlink(const char *file)
{
	struct stat st;

	if (lstat(file, &st) == -1)
		return -1;

	if (S_ISDIR(st.st_mode)) {
		errno = EISDIR;
		return -1;
	}
	return unlink(file);
}

/*
 * execError --
 *	Print why exec failed, avoiding stdio.
 */
void
execError(const char *af, const char *av)
{
#ifdef USE_IOVEC
	int i = 0;
	struct iovec iov[8];
#define IOADD(s) \
	(void)(iov[i].iov_base = UNCONST(s), \
	    iov[i].iov_len = strlen(iov[i].iov_base), \
	    i++)
#else
#define	IOADD(void)write(2, s, strlen(s))
#endif

	IOADD(progname);
	IOADD(": ");
	IOADD(af);
	IOADD("(");
	IOADD(av);
	IOADD(") failed (");
	IOADD(strerror(errno));
	IOADD(")\n");

#ifdef USE_IOVEC
	(void)writev(2, iov, 8);
#endif
}

/*
 * usage --
 *	exit with usage message
 */
static void
usage(void)
{
	(void)fprintf(stderr,
"usage: %s [-BeikNnqrstWX] [-D variable] [-d flags] [-f makefile]\n\
            [-I directory] [-J private] [-j max_jobs] [-m directory] [-T file]\n\
            [-V variable] [variable=value] [target ...]\n", progname);
	exit(2);
}


int
PrintAddr(void *a, void *b)
{
    printf("%lx ", (unsigned long) a);
    return b ? 0 : 0;
}



void
PrintOnError(const char *s)
{
    char tmp[64];
    char *cp;

    if (s)
	    printf("%s", s);
	
    printf("\n%s: stopped in %s\n", progname, curdir);
    strncpy(tmp, "${MAKE_PRINT_VAR_ON_ERROR:@v@$v='${$v}'\n@}",
	    sizeof(tmp) - 1);
    cp = Var_Subst(NULL, tmp, VAR_GLOBAL, 0);
    if (cp) {
	    if (*cp)
		    printf("%s", cp);
	    free(cp);
    }
}

void
Main_ExportMAKEFLAGS(Boolean first)
{
    static int once = 1;
    char tmp[64];
    char *s;

    if (once != first)
	return;
    once = 0;
    
    strncpy(tmp, "${.MAKEFLAGS} ${.MAKEOVERRIDES:O:u:@v@$v=${$v:Q}@}",
	    sizeof(tmp));
    s = Var_Subst(NULL, tmp, VAR_CMD, 0);
    if (s && *s) {
#ifdef POSIX
	setenv("MAKEFLAGS", s, 1);
#else
	setenv("MAKE", s, 1);
#endif
    }
}
