/*	$NetBSD: fdset.h,v 1.1.1.3 2010/12/12 15:21:26 adam Exp $	*/

/* redefine FD_SET */
/* OpenLDAP: pkg/ldap/include/ac/fdset.h,v 1.5.2.5 2010/04/13 20:22:51 kurt Exp */
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

/*
 * This header is to be included by portable.h to ensure
 * tweaking of FD_SETSIZE is done early enough to be effective.
 */

#ifndef _AC_FDSET_H
#define _AC_FDSET_H

#if !defined( OPENLDAP_FD_SETSIZE ) && !defined( FD_SETSIZE )
#  define OPENLDAP_FD_SETSIZE 4096
#endif

#ifdef OPENLDAP_FD_SETSIZE
    /* assume installer desires to enlarge fd_set */
#  ifdef HAVE_BITS_TYPES_H
#    include <bits/types.h>
#  endif
#  ifdef __FD_SETSIZE
#    undef __FD_SETSIZE
#    define __FD_SETSIZE OPENLDAP_FD_SETSIZE
#  else
#    define FD_SETSIZE OPENLDAP_FD_SETSIZE
#  endif
#endif

#endif /* _AC_FDSET_H */
