/*	$NetBSD: operational.c,v 1.1.1.3 2010/12/12 15:23:16 adam Exp $	*/

/* operational.c - monitor backend operational attributes function */
/* OpenLDAP: pkg/ldap/servers/slapd/back-monitor/operational.c,v 1.17.2.6 2010/04/13 20:23:33 kurt Exp */
/* This work is part of OpenLDAP Software <http://www.openldap.org/>.
 *
 * Copyright 2001-2010 The OpenLDAP Foundation.
 * Portions Copyright 2001-2003 Pierangelo Masarati.
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
/* ACKNOWLEDGEMENTS:
 * This work was initially developed by Pierangelo Masarati for inclusion
 * in OpenLDAP Software.
 */

#include "portable.h"

#include <stdio.h>

#include <ac/string.h>
#include <ac/socket.h>

#include "slap.h"
#include "back-monitor.h"
#include "proto-back-monitor.h"

/*
 * sets the supported operational attributes (if required)
 */

int
monitor_back_operational(
	Operation	*op,
	SlapReply	*rs )
{
	Attribute	**ap;

	assert( rs->sr_entry != NULL );

	for ( ap = &rs->sr_operational_attrs; *ap; ap = &(*ap)->a_next )
		/* just count */ ;

	if ( SLAP_OPATTRS( rs->sr_attr_flags ) ||
			ad_inlist( slap_schema.si_ad_hasSubordinates, rs->sr_attrs ) )
	{
		int			hs;
		monitor_entry_t	*mp;

		mp = ( monitor_entry_t * )rs->sr_entry->e_private;

		assert( mp != NULL );

		hs = MONITOR_HAS_CHILDREN( mp );
		*ap = slap_operational_hasSubordinate( hs );
		assert( *ap != NULL );
		ap = &(*ap)->a_next;
	}
	
	return LDAP_SUCCESS;
}

