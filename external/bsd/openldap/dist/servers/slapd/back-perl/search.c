/*	$NetBSD: search.c,v 1.1.1.3 2010/12/12 15:23:21 adam Exp $	*/

/* OpenLDAP: pkg/ldap/servers/slapd/back-perl/search.c,v 1.31.2.5 2010/04/13 20:23:38 kurt Exp */
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
 * Search
 *
 **********************************************************/
int
perl_back_search(
	Operation *op,
	SlapReply *rs )
{
	PerlBackend *perl_back = (PerlBackend *)op->o_bd->be_private;
	int count ;
	AttributeName *an;
	Entry	*e;
	char *buf;
	int i;

#if defined(HAVE_WIN32_ASPERL) || defined(USE_ITHREADS)
	PERL_SET_CONTEXT( PERL_INTERPRETER );
#endif
	ldap_pvt_thread_mutex_lock( &perl_interpreter_mutex );	

	{
		dSP; ENTER; SAVETMPS;

		PUSHMARK(sp) ;
		XPUSHs( perl_back->pb_obj_ref );
		XPUSHs(sv_2mortal(newSVpv( op->o_req_ndn.bv_val , 0)));
		XPUSHs(sv_2mortal(newSViv( op->ors_scope )));
		XPUSHs(sv_2mortal(newSViv( op->ors_deref )));
		XPUSHs(sv_2mortal(newSViv( op->ors_slimit )));
		XPUSHs(sv_2mortal(newSViv( op->ors_tlimit )));
		XPUSHs(sv_2mortal(newSVpv( op->ors_filterstr.bv_val , 0)));
		XPUSHs(sv_2mortal(newSViv( op->ors_attrsonly )));

		for ( an = op->ors_attrs; an && an->an_name.bv_val; an++ ) {
			XPUSHs(sv_2mortal(newSVpv( an->an_name.bv_val , 0)));
		}
		PUTBACK;

#ifdef PERL_IS_5_6
		count = call_method("search", G_ARRAY );
#else
		count = perl_call_method("search", G_ARRAY );
#endif

		SPAGAIN;

		if (count < 1) {
			croak("Big trouble in back_search\n") ;
		}

		if ( count > 1 ) {
							 
			for ( i = 1; i < count; i++ ) {

				buf = POPp;

				if ( (e = str2entry( buf )) == NULL ) {
					Debug( LDAP_DEBUG_ANY, "str2entry(%s) failed\n", buf, 0, 0 );

				} else {
					int send_entry;

					if (perl_back->pb_filter_search_results)
						send_entry = (test_filter( op, e, op->ors_filter ) == LDAP_COMPARE_TRUE);
					else
						send_entry = 1;

					if (send_entry) {
						rs->sr_entry = e;
						rs->sr_attrs = op->ors_attrs;
						rs->sr_flags = REP_ENTRY_MODIFIABLE;
						rs->sr_err = LDAP_SUCCESS;
						rs->sr_err = send_search_entry( op, rs );
						if ( rs->sr_err == LDAP_SIZELIMIT_EXCEEDED ) {
							rs->sr_entry = NULL;
							goto done;
						}
					}

					entry_free( e );
				}
			}
		}

		/*
		 * We grab the return code last because the stack comes
		 * from perl in reverse order. 
		 *
		 * ex perl: return ( 0, $res_1, $res_2 );
		 *
		 * ex stack: <$res_2> <$res_1> <0>
		 */

		rs->sr_err = POPi;

done:;
		PUTBACK; FREETMPS; LEAVE;
	}

	ldap_pvt_thread_mutex_unlock( &perl_interpreter_mutex );	

	send_ldap_result( op, rs );

	return 0;
}

