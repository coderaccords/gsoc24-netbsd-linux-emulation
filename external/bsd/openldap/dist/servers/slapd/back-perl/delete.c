/*	$NetBSD: delete.c,v 1.1.1.3 2010/12/12 15:23:20 adam Exp $	*/

/* OpenLDAP: pkg/ldap/servers/slapd/back-perl/delete.c,v 1.20.2.5 2010/04/13 20:23:37 kurt Exp */
/* This work is part of OpenLDAP Software <http://www.openldap.org/>.
 *
 * Copyright 1999-2010 The OpenLDAP Foundation.
 * Portions Copyright 1999 John C. Quillan.
 * Portions Copyright 2002 myinternet Limited.
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

#include "perl_back.h"

int
perl_back_delete(
	Operation	*op,
	SlapReply	*rs )
{
	PerlBackend *perl_back = (PerlBackend *) op->o_bd->be_private;
	int count;

#if defined(HAVE_WIN32_ASPERL) || defined(USE_ITHREADS)
	PERL_SET_CONTEXT( PERL_INTERPRETER );
#endif
	ldap_pvt_thread_mutex_lock( &perl_interpreter_mutex );	

	{
		dSP; ENTER; SAVETMPS;

		PUSHMARK(sp);
		XPUSHs( perl_back->pb_obj_ref );
		XPUSHs(sv_2mortal(newSVpv( op->o_req_dn.bv_val , 0 )));

		PUTBACK;

#ifdef PERL_IS_5_6
		count = call_method("delete", G_SCALAR);
#else
		count = perl_call_method("delete", G_SCALAR);
#endif

		SPAGAIN;

		if (count != 1) {
			croak("Big trouble in perl-back_delete\n");
		}

		rs->sr_err = POPi;

		PUTBACK; FREETMPS; LEAVE;
	}

	ldap_pvt_thread_mutex_unlock( &perl_interpreter_mutex );	

	send_ldap_result( op, rs );

	Debug( LDAP_DEBUG_ANY, "Perl DELETE\n", 0, 0, 0 );
	return( 0 );
}
