/*++
/* NAME
/*	mail_server 3h
/* SUMMARY
/*	skeleton servers
/* SYNOPSIS
/*	#include <mail_server.h>
/* DESCRIPTION
/* .nf

 /*
  * Utility library.
  */
#include <vstream.h>

 /*
  * External interface. Tables are defined in mail_conf.h.
  */
#define MAIL_SERVER_INT_TABLE	1
#define MAIL_SERVER_STR_TABLE	2
#define MAIL_SERVER_BOOL_TABLE	3
#define MAIL_SERVER_RAW_TABLE	4

#define	MAIL_SERVER_PRE_INIT	10
#define MAIL_SERVER_POST_INIT	11
#define MAIL_SERVER_LOOP	12
#define MAIL_SERVER_EXIT	13
#define MAIL_SERVER_PRE_ACCEPT	14

typedef void (*MAIL_SERVER_INIT_FN) (char *, char **);
typedef int (*MAIL_SERVER_LOOP_FN) (char *, char **);
typedef void (*MAIL_SERVER_EXIT_FN) (char *, char **);
typedef void (*MAIL_SERVER_ACCEPT_FN) (char *, char **);

 /*
  * single_server.c
  */
typedef void (*SINGLE_SERVER_FN) (VSTREAM *, char *, char **);
extern NORETURN single_server_main(int, char **, SINGLE_SERVER_FN, ...);

 /*
  * multi_server.c
  */
typedef void (*MULTI_SERVER_FN) (VSTREAM *, char *, char **);
extern NORETURN multi_server_main(int, char **, MULTI_SERVER_FN,...);
extern void multi_server_disconnect(VSTREAM *);

 /*
  * trigger_server.c
  */
typedef void (*TRIGGER_SERVER_FN) (char *, int, char *, char **);
extern NORETURN trigger_server_main(int, char **, TRIGGER_SERVER_FN, ...);

#define TRIGGER_BUF_SIZE	1024

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
