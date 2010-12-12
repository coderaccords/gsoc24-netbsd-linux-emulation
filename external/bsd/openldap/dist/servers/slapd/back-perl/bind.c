/*	$NetBSD: bind.c,v 1.1.1.3 2010/12/12 15:23:20 adam Exp $	*/

/* OpenLDAP: pkg/ldap/servers/slapd/back-perl/bind.c,v 1.24.2.6 2010/04/13 20:23:37 kurt Exp */
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


/**********************************************************
 *
 * Bind
 *
 **********************************************************/
int
perl_back_bind(
	Operation *op,
	SlapReply *rs )
{
	int count;

	PerlBackend *perl_back = (PerlBackend *) op->o_bd->be_private;

	/* allow rootdn as a means to auth without the need to actually
 	 * contact the proxied DSA */
	switch ( be_rootdn_bind( op, rs ) ) {
	case SLAP_CB_CONTINUE:
		break;

	default:
		return rs->sr_err;
	}

#if defined(HAVE_WIN32_ASPERL) || defined(USE_ITHREADS)
	PERL_SET_CONTEXT( PERL_INTERPRETER );
#endif

	ldap_pvt_thread_mutex_lock( &perl_interpreter_mutex );	

	{
		dSP; ENTER; SAVETMPS;

		PUSHMARK(SP);
		XPUSHs( perl_back->pb_obj_ref );
		XPUSHs(sv_2mortal(newSVpv( op->o_req_dn.bv_val , 0)));
		XPUSHs(sv_2mortal(newSVpv( op->orb_cred.bv_val , op->orb_cred.bv_len)));
		PUTBACK;

#ifdef PERL_IS_5_6
		count = call_method("bind", G_SCALAR);
#else
		count = perl_call_method("bind", G_SCALAR);
#endif

		SPAGAIN;

		if (count != 1) {
			croak("Big trouble in back_bind\n");
		}

		rs->sr_err = POPi;
							 

		PUTBACK; FREETMPS; LEAVE;
	}

	ldap_pvt_thread_mutex_unlock( &perl_interpreter_mutex );	

	Debug( LDAP_DEBUG_ANY, "Perl BIND returned 0x%04x\n", rs->sr_err, 0, 0 );

	/* frontend will send result on success (0) */
	if( rs->sr_err != LDAP_SUCCESS )
		send_ldap_result( op, rs );

	return ( rs->sr_err );
}
