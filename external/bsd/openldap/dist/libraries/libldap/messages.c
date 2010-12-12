/*	$NetBSD: messages.c,v 1.1.1.3 2010/12/12 15:21:33 adam Exp $	*/

/* messages.c */
/* OpenLDAP: pkg/ldap/libraries/libldap/messages.c,v 1.17.2.5 2010/04/13 20:22:58 kurt Exp */
/* This work is part of OpenLDAP Software <http://www.openldap.org/>.
 *
 * Copyright 1998-2010 The OpenLDAP Foundation.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted only as authorized by the OpenLDAP
 * Public License.
 *
 * A copy of this license is available in the file LICENSE in the
 * top-level directory of the distribution or, alternatively, at
 * <http://www.OpenLDAP.org/license.html>.
 */

#include "portable.h"

#include <stdio.h>

#include <ac/stdlib.h>

#include <ac/socket.h>
#include <ac/string.h>
#include <ac/time.h>

#include "ldap-int.h"

LDAPMessage *
ldap_first_message( LDAP *ld, LDAPMessage *chain )
{
	assert( ld != NULL );
	assert( LDAP_VALID( ld ) );
	assert( chain != NULL );

  	return chain;
}

LDAPMessage *
ldap_next_message( LDAP *ld, LDAPMessage *msg )
{
	assert( ld != NULL );
	assert( LDAP_VALID( ld ) );
	assert( msg != NULL );

	return msg->lm_chain;
}

int
ldap_count_messages( LDAP *ld, LDAPMessage *chain )
{
	int	i;

	assert( ld != NULL );
	assert( LDAP_VALID( ld ) );

	for ( i = 0; chain != NULL; chain = chain->lm_chain ) {
		i++;
	}

	return( i );
}

BerElement*
ldap_get_message_ber( LDAPMessage *ld )
{
	return ld->lm_ber;
}
