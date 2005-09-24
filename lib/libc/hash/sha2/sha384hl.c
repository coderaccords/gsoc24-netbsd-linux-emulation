/* $NetBSD: sha384hl.c,v 1.4 2005/09/24 18:49:18 elad Exp $ */

/*
 * Derived from code ritten by Jason R. Thorpe <thorpej@NetBSD.org>,
 * April 29, 1997.
 * Public domain.
 */

#include <sys/cdefs.h>
#if defined(LIBC_SCCS) && !defined(lint)
__RCSID("$NetBSD: sha384hl.c,v 1.4 2005/09/24 18:49:18 elad Exp $");
#endif /* LIBC_SCCS and not lint */

#define	HASH_ALGORITHM	SHA384
#define	HASH_FNPREFIX	SHA384_

#include "namespace.h"
#include <crypto/sha2.h>

#if HAVE_NBTOOL_CONFIG_H
#include "nbtool_config.h"
#endif

#include "../hash.c"
