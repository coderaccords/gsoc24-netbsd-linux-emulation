/*	$NetBSD: ldif.h,v 1.1.1.3 2010/12/12 15:21:24 adam Exp $	*/

/* OpenLDAP: pkg/ldap/include/ldif.h,v 1.31.2.5 2010/04/13 20:22:49 kurt Exp */
/* This work is part of OpenLDAP Software <http://www.openldap.org/>.
 *
 * Copyright 1998-2010 The OpenLDAP Foundation.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted only as authorized by the OpenLDAP
 * Public License.
 *
 * A copy of this license is available in file LICENSE in the
 * top-level directory of the distribution or, alternatively, at
 * <http://www.OpenLDAP.org/license.html>.
 */
/* Portions Copyright (c) 1996 Regents of the University of Michigan.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that this notice is preserved and that due credit is given
 * to the University of Michigan at Ann Arbor. The name of the University
 * may not be used to endorse or promote products derived from this
 * software without specific prior written permission. This software
 * is provided ``as is'' without express or implied warranty.
 */

#ifndef _LDIF_H
#define _LDIF_H

#include <ldap_cdefs.h>

LDAP_BEGIN_DECL

/* This is NOT a bogus extern declaration (unlike ldap_debug) */
LDAP_LDIF_V (int) ldif_debug;

#define LDIF_LINE_WIDTH      76      /* maximum length of LDIF lines */

/*
 * Macro to calculate maximum number of bytes that the base64 equivalent
 * of an item that is "len" bytes long will take up.  Base64 encoding
 * uses one byte for every six bits in the value plus up to two pad bytes.
 */
#define LDIF_BASE64_LEN(len)	(((len) * 4 / 3 ) + 3)

/*
 * Macro to calculate maximum size that an LDIF-encoded type (length
 * tlen) and value (length vlen) will take up:  room for type + ":: " +
 * first newline + base64 value + continued lines.  Each continued line
 * needs room for a newline and a leading space character.
 */
#define LDIF_SIZE_NEEDED(nlen,vlen) \
    ((nlen) + 4 + LDIF_BASE64_LEN(vlen) \
    + ((LDIF_BASE64_LEN(vlen) + (nlen) + 3) / LDIF_LINE_WIDTH * 2 ))

LDAP_LDIF_F( int )
ldif_parse_line LDAP_P((
	LDAP_CONST char *line,
	char **name,
	char **value,
	ber_len_t *vlen ));

LDAP_LDIF_F( int )
ldif_parse_line2 LDAP_P((
	char *line,
	struct berval *type,
	struct berval *value,
	int *freeval ));

LDAP_LDIF_F( FILE * )
ldif_open_url LDAP_P(( LDAP_CONST char *urlstr ));

LDAP_LDIF_F( int )
ldif_fetch_url LDAP_P((
	LDAP_CONST char *line,
	char **value,
	ber_len_t *vlen ));

LDAP_LDIF_F( char * )
ldif_getline LDAP_P(( char **next ));

LDAP_LDIF_F( int )
ldif_countlines LDAP_P(( LDAP_CONST char *line ));

/* ldif_ropen, rclose, read_record - just for reading LDIF files,
 * no special open/close needed to write LDIF files.
 */
typedef struct LDIFFP {
	FILE *fp;
	struct LDIFFP *prev;
} LDIFFP;

LDAP_LDIF_F( LDIFFP * )
ldif_open LDAP_P(( LDAP_CONST char *file, LDAP_CONST char *mode ));

LDAP_LDIF_F( void )
ldif_close LDAP_P(( LDIFFP * ));

LDAP_LDIF_F( int )
ldif_read_record LDAP_P((
	LDIFFP *fp,
	int *lineno,
	char **bufp,
	int *buflen ));

LDAP_LDIF_F( int )
ldif_must_b64_encode_register LDAP_P((
	LDAP_CONST char *name,
	LDAP_CONST char *oid ));

LDAP_LDIF_F( void )
ldif_must_b64_encode_release LDAP_P(( void ));

#define LDIF_PUT_NOVALUE	0x0000	/* no value */
#define LDIF_PUT_VALUE		0x0001	/* value w/ auto detection */
#define LDIF_PUT_TEXT		0x0002	/* assume text */
#define	LDIF_PUT_BINARY		0x0004	/* assume binary (convert to base64) */
#define LDIF_PUT_B64		0x0008	/* pre-converted base64 value */

#define LDIF_PUT_COMMENT	0x0010	/* comment */
#define LDIF_PUT_URL		0x0020	/* url */
#define LDIF_PUT_SEP		0x0040	/* separator */

LDAP_LDIF_F( void )
ldif_sput LDAP_P((
	char **out,
	int type,
	LDAP_CONST char *name,
	LDAP_CONST char *val,
	ber_len_t vlen ));

LDAP_LDIF_F( char * )
ldif_put LDAP_P((
	int type,
	LDAP_CONST char *name,
	LDAP_CONST char *val,
	ber_len_t vlen ));

LDAP_LDIF_F( int )
ldif_is_not_printable LDAP_P((
	LDAP_CONST char *val,
	ber_len_t vlen ));

LDAP_END_DECL

#endif /* _LDIF_H */
