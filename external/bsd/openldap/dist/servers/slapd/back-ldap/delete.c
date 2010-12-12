/*	$NetBSD: delete.c,v 1.1.1.3 2010/12/12 15:23:05 adam Exp $	*/

/* delete.c - ldap backend delete function */
/* OpenLDAP: pkg/ldap/servers/slapd/back-ldap/delete.c,v 1.46.2.7 2010/04/13 20:23:28 kurt Exp */
/* This work is part of OpenLDAP Software <http://www.openldap.org/>.
 *
 * Copyright 2003-2010 The OpenLDAP Foundation.
 * Portions Copyright 1999-2003 Howard Chu.
 * Portions Copyright 2000-2003 Pierangelo Masarati.
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
/* ACKNOWLEDGEMENTS:
 * This work was initially developed by the Howard Chu for inclusion
 * in OpenLDAP Software and subsequently enhanced by Pierangelo
 * Masarati.
 */

#include "portable.h"

#include <stdio.h>

#include <ac/string.h>
#include <ac/socket.h>

#include "slap.h"
#include "back-ldap.h"

int
ldap_back_delete(
		Operation	*op,
		SlapReply	*rs )
{
	ldapinfo_t		*li = (ldapinfo_t *)op->o_bd->be_private;

	ldapconn_t		*lc = NULL;
	ber_int_t		msgid;
	LDAPControl		**ctrls = NULL;
	ldap_back_send_t	retrying = LDAP_BACK_RETRYING;
	int			rc = LDAP_SUCCESS;

	if ( !ldap_back_dobind( &lc, op, rs, LDAP_BACK_SENDERR ) ) {
		return rs->sr_err;
	}

retry:
	ctrls = op->o_ctrls;
	rc = ldap_back_controls_add( op, rs, lc, &ctrls );
	if ( rc != LDAP_SUCCESS ) {
		send_ldap_result( op, rs );
		rc = rs->sr_err;
		goto cleanup;
	}

	rs->sr_err = ldap_delete_ext( lc->lc_ld, op->o_req_dn.bv_val,
			ctrls, NULL, &msgid );
	rc = ldap_back_op_result( lc, op, rs, msgid,
		li->li_timeout[ SLAP_OP_DELETE ],
		( LDAP_BACK_SENDRESULT | retrying ) );
	if ( rs->sr_err == LDAP_UNAVAILABLE && retrying ) {
		retrying &= ~LDAP_BACK_RETRYING;
		if ( ldap_back_retry( &lc, op, rs, LDAP_BACK_SENDERR ) ) {
			/* if the identity changed, there might be need to re-authz */
			(void)ldap_back_controls_free( op, rs, &ctrls );
			goto retry;
		}
	}

cleanup:
	(void)ldap_back_controls_free( op, rs, &ctrls );

	if ( lc != NULL ) {
		ldap_back_release_conn( li, lc );
	}

	return rc;
}
