/*	$NetBSD: globals.c,v 1.1.1.3 2010/12/12 15:22:30 adam Exp $	*/

/* globals.c - various global variables */
/* OpenLDAP: pkg/ldap/servers/slapd/globals.c,v 1.15.2.5 2010/04/13 20:23:15 kurt Exp */
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

#include <ac/string.h>
#include "lber_pvt.h"

#include "slap.h"


/*
 * Global variables, in general, should be declared in the file
 * primarily responsible for its management.  Configurable globals
 * belong in config.c.  Variables declared here have no other
 * sensible home.
 */

const struct berval slap_empty_bv = BER_BVC("");
const struct berval slap_unknown_bv = BER_BVC("unknown");

/* normalized boolean values */
const struct berval slap_true_bv = BER_BVC("TRUE");
const struct berval slap_false_bv = BER_BVC("FALSE");

