#ifndef _MAIL_CONF_H_INCLUDED_
#define _MAIL_CONF_H_INCLUDED_

/*++
/* NAME
/*	mail_conf 3h
/* SUMMARY
/*	global configuration parameter management
/* SYNOPSIS
/*	#include <mail_conf.h>
/* DESCRIPTION
/* .nf

 /*
  * Well known names. These are not configurable. One has to start somewhere.
  */
#define CONFIG_DICT	"mail_dict"	/* global Postfix dictionary */

 /*
  * Environment variables.
  */
#define CONF_ENV_PATH	"MAIL_CONFIG"	/* config database */
#define CONF_ENV_VERB	"MAIL_VERBOSE"	/* verbose mode on */
#define CONF_ENV_DEBUG	"MAIL_DEBUG"	/* verbose mode on */

 /*
  * External representation for booleans.
  */
#define CONFIG_BOOL_YES	"yes"
#define CONFIG_BOOL_NO	"no"

 /*
  * Basic configuration management.
  */
extern void mail_conf_read(void);

extern void mail_conf_update(const char *, const char *);
extern const char *mail_conf_lookup(const char *);
extern const char *mail_conf_eval(const char *);
extern const char *mail_conf_lookup_eval(const char *);

 /*
  * Specific parameter lookup routines.
  */
extern char *get_mail_conf_str(const char *, const char *, int, int);
extern int get_mail_conf_int(const char *, int, int, int);
extern int get_mail_conf_bool(const char *, int);
extern char *get_mail_conf_raw(const char *, const char *, int, int);

extern int get_mail_conf_int2(const char *, const char *, int, int, int);

 /*
  * Lookup with function-call defaults.
  */
extern char *get_mail_conf_str_fn(const char *, const char *(*) (void), int, int);
extern int get_mail_conf_int_fn(const char *, int (*) (void), int, int);
extern int get_mail_conf_bool_fn(const char *, int (*) (void));
extern char *get_mail_conf_raw_fn(const char *, const char *(*) (void), int, int);

 /*
  * Update dictionary.
  */
extern void set_mail_conf_str(const char *, const char *);
extern void set_mail_conf_int(const char *, int);
extern void set_mail_conf_bool(const char *, int);

 /*
  * Tables that allow us to selectively copy values from the global
  * configuration file to global variables.
  */
typedef struct {
    const char *name;			/* config variable name */
    const char *defval;			/* default value or null */
    char  **target;			/* pointer to global variable */
    int     min;			/* min length or zero */
    int     max;			/* max length or zero */
} CONFIG_STR_TABLE;

typedef struct {
    const char *name;			/* config variable name */
    int     defval;			/* default value */
    int    *target;			/* pointer to global variable */
    int     min;			/* lower bound or zero */
    int     max;			/* upper bound or zero */
} CONFIG_INT_TABLE;

typedef struct {
    const char *name;			/* config variable name */
    int     defval;			/* default value */
    int    *target;			/* pointer to global variable */
} CONFIG_BOOL_TABLE;

extern void get_mail_conf_str_table(CONFIG_STR_TABLE *);
extern void get_mail_conf_int_table(CONFIG_INT_TABLE *);
extern void get_mail_conf_bool_table(CONFIG_BOOL_TABLE *);
extern void get_mail_conf_raw_table(CONFIG_STR_TABLE *);

 /*
  * Tables to initialize parameters from the global configuration file or
  * from function calls.
  */
typedef struct {
    const char *name;			/* config variable name */
    const char *(*defval) (void);	/* default value provider */
    char  **target;			/* pointer to global variable */
    int     min;			/* lower bound or zero */
    int     max;			/* upper bound or zero */
} CONFIG_STR_FN_TABLE;

typedef struct {
    const char *name;			/* config variable name */
    int     (*defval) (void);		/* default value provider */
    int    *target;			/* pointer to global variable */
    int     min;			/* lower bound or zero */
    int     max;			/* upper bound or zero */
} CONFIG_INT_FN_TABLE;

typedef struct {
    const char *name;			/* config variable name */
    int     (*defval) (void);		/* default value provider */
    int    *target;			/* pointer to global variable */
} CONFIG_BOOL_FN_TABLE;

extern void get_mail_conf_str_fn_table(CONFIG_STR_FN_TABLE *);
extern void get_mail_conf_int_fn_table(CONFIG_INT_FN_TABLE *);
extern void get_mail_conf_bool_fn_table(CONFIG_BOOL_FN_TABLE *);
extern void get_mail_conf_raw_fn_table(CONFIG_STR_FN_TABLE *);

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

#endif
