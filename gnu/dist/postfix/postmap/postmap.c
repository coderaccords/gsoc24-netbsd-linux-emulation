/*++
/* NAME
/*	postmap 1
/* SUMMARY
/*	Postfix lookup table management
/* SYNOPSIS
/* .fi
/*	\fBpostmap\fR [\fB-Ninvw\fR] [\fB-c \fIconfig_dir\fR] [\fB-q \fIkey\fR]
/*		[\fIfile_type\fR:]\fIfile_name\fR ...
/* DESCRIPTION
/*	The \fBpostmap\fR command creates or queries one or more Postfix
/*	lookup tables, or updates an existing one. The input and output
/*	file formats are expected to be compatible with:
/*
/* .ti +4
/*	\fBmakemap \fIfile_type\fR \fIfile_name\fR < \fIfile_name\fR
/*
/*	While the table update is in progress, signal delivery is
/*	postponed, and an exclusive, advisory, lock is placed on the
/*	entire table, in order to avoid surprises in spectator
/*	programs.
/*
/*	The format of a lookup table input file is as follows:
/* .IP \(bu
/*	Blank lines are ignored. So are lines beginning with `#'.
/* .IP \(bu
/*	A table entry has the form
/* .sp
/* .ti +5
/*	\fIkey\fR whitespace \fIvalue\fR
/* .IP \(bu
/*	A line that starts with whitespace continues the preceding line.
/* .PP
/*	The \fIkey\fR and \fIvalue\fR are processed as is, except that
/*	surrounding white space is stripped off. Unlike with Postfix alias
/*	databases, quotes cannot be used to protect lookup keys that contain
/*	special characters such as `#' or whitespace. The \fIkey\fR is mapped
/*	to lowercase to make mapping lookups case insensitive.
/*
/*	Options:
/* .IP \fB-N\fR
/*	Include the terminating null character that terminates lookup keys
/*	and values. By default, Postfix does whatever is the default for
/*	the host operating system.
/* .IP "\fB-c \fIconfig_dir\fR"
/*	Read the \fBmain.cf\fR configuration file in the named directory.
/* .IP \fB-i\fR
/*	Incremental mode. Read entries from standard input and do not
/*	truncate an existing database. By default, \fBpostmap\fR creates
/*	a new database from the entries in \fBfile_name\fR.
/* .IP \fB-n\fR
/*	Don't include the terminating null character that terminates lookup
/*	keys and values. By default, Postfix does whatever is the default for
/*	the host operating system.
/* .IP "\fB-q \fIkey\fR"
/*	Search the specified maps for \fIkey\fR and print the first value
/*	found on the standard output stream. The exit status is non-zero
/*	if the requested information was not found.
/* .IP \fB-v\fR
/*	Enable verbose logging for debugging purposes. Multiple \fB-v\fR
/*	options make the software increasingly verbose.
/* .IP \fB-w\fR
/*	Do not warn about duplicate entries; silently ignore them.
/* .PP
/*	Arguments:
/* .IP \fIfile_type\fR
/*	The type of database to be produced.
/* .RS
/* .IP \fBbtree\fR
/*	The output file is a btree file, named \fIfile_name\fB.db\fR.
/*	This is available only on systems with support for \fBdb\fR databases.
/* .IP \fBdbm\fR
/*	The output consists of two files, named \fIfile_name\fB.pag\fR and
/*	\fIfile_name\fB.dir\fR.
/*	This is available only on systems with support for \fBdbm\fR databases.
/* .IP \fBhash\fR
/*	The output file is a hashed file, named \fIfile_name\fB.db\fR.
/*	This is available only on systems with support for \fBdb\fR databases.
/* .PP
/*	When no \fIfile_type\fR is specified, the software uses the database
/*	type specified via the \fBdatabase_type\fR configuration parameter.
/* .RE
/* .IP \fIfile_name\fR
/*	The name of the lookup table source file when rebuilding a database.
/* DIAGNOSTICS
/*	Problems and transactions are logged to the standard error
/*	stream. No output means no problems. Duplicate entries are
/*	skipped and are flagged with a warning.
/* ENVIRONMENT
/* .ad
/* .fi
/* .IP \fBMAIL_CONFIG\fR
/*	Directory with Postfix configuration files.
/* .IP \fBMAIL_VERBOSE\fR
/*	Enable verbose logging for debugging purposes.
/* CONFIGURATION PARAMETERS
/* .ad
/* .fi
/* .IP \fBdatabase_type\fR
/*	Default output database type.
/*	On many UNIX systems, the default database type is either \fBhash\fR
/*	or \fBdbm\fR.
/* LICENSE
/* .ad
/* .fi
/*	The Secure Mailer license must be distributed with this software.
/* AUTHOR(S)
/*	Wietse Venema
/*	IBM T.J. Watson Research
/*	P.O. Box 704
/*	Yorktown Heights, NY 10598, USA
/*--*/

/* System library. */

#include <sys_defs.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <ctype.h>
#include <string.h>

/* Utility library. */

#include <msg.h>
#include <mymalloc.h>
#include <vstring.h>
#include <vstream.h>
#include <msg_vstream.h>
#include <readlline.h>
#include <stringops.h>
#include <split_at.h>

/* Global library. */

#include <mail_conf.h>
#include <mail_params.h>
#include <mkmap.h>

/* Application-specific. */

#define STR	vstring_str

/* postmap - create or update mapping database */

static void postmap(char *map_type, char *path_name,
		            int open_flags, int dict_flags)
{
    VSTREAM *source_fp;
    VSTRING *line_buffer;
    MKMAP  *mkmap;
    int     lineno;
    char   *key;
    char   *value;

    /*
     * Initialize.
     */
    line_buffer = vstring_alloc(100);
    if ((open_flags & O_TRUNC) == 0) {
	source_fp = VSTREAM_IN;
	vstream_control(source_fp, VSTREAM_CTL_PATH, "stdin", VSTREAM_CTL_END);
    } else if ((source_fp = vstream_fopen(path_name, O_RDONLY, 0)) == 0) {
	msg_fatal("open %s: %m", path_name);
    }

    /*
     * Open the database, optionally create it when it does not exist,
     * optionally truncate it when it does exist, and lock out any
     * spectators.
     */
    mkmap = mkmap_open(map_type, path_name, open_flags, dict_flags);

    /*
     * Add records to the database.
     */
    lineno = 0;
    while (readlline(line_buffer, source_fp, &lineno, READLL_STRIPNL)) {

	/*
	 * Skip comments.
	 */
	if (*STR(line_buffer) == '#')
	    continue;

	/*
	 * Split on the first whitespace character, then trim leading and
	 * trailing whitespace from key and value.
	 */
	key = STR(line_buffer);
	value = STR(line_buffer) + strcspn(STR(line_buffer), " \t\r\n");
	if (*value)
	    *value++ = 0;
	while (ISSPACE(*key))
	    key++;
	while (ISSPACE(*value))
	    value++;
	trimblanks(key, 0)[0] = 0;
	trimblanks(value, 0)[0] = 0;

	/*
	 * Skip empty lines, or lines with whitespace characters only.
	 */
	if (*key == 0 && *value == 0)
	    continue;

	/*
	 * Enforce the "key whitespace value" format. Disallow missing keys
	 * or missing values.
	 */
	if (*key == 0 || *value == 0) {
	    msg_warn("%s, line %d: expected format: key whitespace value",
		     VSTREAM_PATH(source_fp), lineno);
	    continue;
	}
	if (key[strlen(key) - 1] == ':')
	    msg_warn("%s, line %d: record is in \"key: value\" format; is this an alias file?",
		     VSTREAM_PATH(source_fp), lineno);

	/*
	 * Store the value under a case-insensitive key.
	 */
	lowercase(key);
	mkmap_append(mkmap, key, value);
    }

    /*
     * Close the mapping database, and release the lock.
     */
    mkmap_close(mkmap);

    /*
     * Cleanup. We're about to terminate, but it is a good sanity check.
     */
    vstring_free(line_buffer);
    if (source_fp != VSTREAM_IN)
	vstream_fclose(source_fp);
}

/* postmap_query - query a map and print the result to stdout */

static int postmap_query(const char *map_type, const char *map_name,
			         const char *key)
{
    DICT   *dict;
    const char *value;

    dict = dict_open3(map_type, map_name, O_RDONLY, DICT_FLAG_LOCK);
    if ((value = dict_get(dict, key)) != 0) {
	vstream_printf("%s\n", value);
	vstream_fflush(VSTREAM_OUT);
    }
    dict_close(dict);
    return (value != 0);
}

/* usage - explain */

static NORETURN usage(char *myname)
{
    msg_fatal("usage: %s [-Ninvw] [-c config_dir] [-q key] [map_type:]file...",
	      myname);
}

int     main(int argc, char **argv)
{
    char   *path_name;
    int     ch;
    int     fd;
    char   *slash;
    struct stat st;
    int     open_flags = O_RDWR | O_CREAT | O_TRUNC;
    int     dict_flags = DICT_FLAG_DUP_WARN;
    char   *query = 0;
    int     found;

    /*
     * Be consistent with file permissions.
     */
    umask(022);

    /*
     * To minimize confusion, make sure that the standard file descriptors
     * are open before opening anything else. XXX Work around for 44BSD where
     * fstat can return EBADF on an open file descriptor.
     */
    for (fd = 0; fd < 3; fd++)
	if (fstat(fd, &st) == -1
	    && (close(fd), open("/dev/null", O_RDWR, 0)) != fd)
	    msg_fatal("open /dev/null: %m");

    /*
     * Process environment options as early as we can. We are not set-uid,
     * and we are supposed to be running in a controlled environment.
     */
    if (getenv(CONF_ENV_VERB))
	msg_verbose = 1;

    /*
     * Initialize. Set up logging, read the global configuration file and
     * extract configuration information.
     */
    if ((slash = strrchr(argv[0], '/')) != 0)
	argv[0] = slash + 1;
    msg_vstream_init(argv[0], VSTREAM_ERR);

    /*
     * Parse JCL.
     */
    while ((ch = GETOPT(argc, argv, "Nc:inq:vw")) > 0) {
	switch (ch) {
	default:
	    usage(argv[0]);
	    break;
	case 'N':
	    dict_flags |= DICT_FLAG_TRY1NULL;
	    dict_flags &= ~DICT_FLAG_TRY0NULL;
	    break;
	case 'c':
	    if (setenv(CONF_ENV_PATH, optarg, 1) < 0)
		msg_fatal("out of memory");
	    break;
	case 'i':
	    open_flags &= ~O_TRUNC;
	    break;
	case 'n':
	    dict_flags |= DICT_FLAG_TRY0NULL;
	    dict_flags &= ~DICT_FLAG_TRY1NULL;
	    break;
	case 'q':
	    query = optarg;
	    break;
	case 'v':
	    msg_verbose++;
	    break;
	case 'w':
	    dict_flags &= ~DICT_FLAG_DUP_WARN;
	    dict_flags |= DICT_FLAG_DUP_IGNORE;
	    break;
	}
    }
    mail_conf_read();

    /*
     * Use the map type specified by the user, or fall back to a default
     * database type.
     */
    if (query == 0) {				/* create/update map(s) */
	if (optind + 1 > argc)
	    usage(argv[0]);
	while (optind < argc) {
	    if ((path_name = split_at(argv[optind], ':')) != 0) {
		postmap(argv[optind], path_name, open_flags, dict_flags);
	    } else {
		postmap(var_db_type, argv[optind], open_flags, dict_flags);
	    }
	    optind++;
	}
	exit(0);
    } else {					/* query map(s) */
	if (optind + 1 > argc)
	    usage(argv[0]);
	while (optind < argc) {
	    if ((path_name = split_at(argv[optind], ':')) != 0) {
		found = postmap_query(argv[optind], path_name, query);
	    } else {
		found = postmap_query(var_db_type, argv[optind], query);
	    }
	    if (found)
		exit(0);
	    optind++;
	}
	exit(1);
    }
}
