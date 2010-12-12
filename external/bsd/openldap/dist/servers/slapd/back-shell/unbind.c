/*	$NetBSD: unbind.c,v 1.1.1.3 2010/12/12 15:23:22 adam Exp $	*/

/* unbind.c - shell backend unbind function */
/* OpenLDAP: pkg/ldap/servers/slapd/back-shell/unbind.c,v 1.23.2.5 2010/04/13 20:23:40 kurt Exp */
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
/* Portions Copyright (c) 1995 Regents of the University of Michigan.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that this notice is preserved and that due credit is given
 * to the University of Michigan at Ann Arbor. The name of the University
 * may not be used to endorse or promote products derived from this
 * software without specific prior written permission. This software
 * is provided ``as is'' without express or implied warranty.
 */
/* ACKNOWLEDGEMENTS:
 * This work was originally developed by the University of Michigan
 * (as part of U-MICH LDAP).
 */

#include "portable.h"

#include <stdio.h>

#include <ac/socket.h>
#include <ac/string.h>

#include "slap.h"
#include "shell.h"

int
shell_back_unbind(
    Operation		*op,
    SlapReply		*rs
)
{
	struct shellinfo	*si = (struct shellinfo *) op->o_bd->be_private;
	FILE			*rfp, *wfp;

	if ( si->si_unbind == NULL ) {
		return 0;
	}

	if ( forkandexec( si->si_unbind, &rfp, &wfp ) == (pid_t)-1 ) {
		return 0;
	}

	/* write out the request to the unbind process */
	fprintf( wfp, "UNBIND\n" );
	fprintf( wfp, "msgid: %ld\n", (long) op->o_msgid );
	print_suffixes( wfp, op->o_bd );
	fprintf( wfp, "dn: %s\n", (op->o_conn->c_dn.bv_len ? op->o_conn->c_dn.bv_val : "") );
	fclose( wfp );

	/* no response to unbind */
	fclose( rfp );

	return 0;
}
