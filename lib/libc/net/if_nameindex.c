/*	$NetBSD: if_nameindex.c,v 1.4 2000/07/24 12:06:55 itojun Exp $	*/
/*	$KAME: if_nameindex.c,v 1.5 2000/07/24 12:03:31 itojun Exp $	*/

/*-
 * Copyright (c) 1997, 2000
 *	Berkeley Software Design, Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * THIS SOFTWARE IS PROVIDED BY Berkeley Software Design, Inc. ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL Berkeley Software Design, Inc. BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	BSDI Id: if_nameindex.c,v 2.3 2000/04/17 22:38:05 dab Exp
 */

#include <sys/cdefs.h>
#if defined(LIBC_SCCS) && !defined(lint)
__RCSID("$NetBSD: if_nameindex.c,v 1.4 2000/07/24 12:06:55 itojun Exp $");
#endif /* LIBC_SCCS and not lint */

#include "namespace.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <net/if_dl.h>
#include <net/if.h>
#include <ifaddrs.h>
#include <stdlib.h>
#include <string.h>

#ifdef __weak_alias
__weak_alias(if_nameindex,_if_nameindex)
__weak_alias(if_freenameindex,_if_freenameindex)
#endif
/*
 * From RFC 2133:
 *
 * 4.3.  Return All Interface Names and Indexes
 *
 *    The final function returns an array of if_nameindex structures, one
 *    structure per interface.
 *
 *        #include <net/if.h>
 *
 *        struct if_nameindex {
 *          unsigned int   if_index;  / * 1, 2, ... * /
 *          char          *if_name;   / * null terminated name: "le0", ... * /
 *        };
 *
 *        struct if_nameindex  *if_nameindex(void);
 *
 *    The end of the array of structures is indicated by a structure with
 *    an if_index of 0 and an if_name of NULL.  The function returns a NULL
 *    pointer upon an error.
 *
 *    The memory used for this array of structures along with the interface
 *    names pointed to by the if_name members is obtained dynamically.
 *    This memory is freed by the next function.
 *
 * 4.4.  Free Memory
 *
 *    The following function frees the dynamic memory that was allocated by
 *    if_nameindex().
 *
 *        #include <net/if.h>
 *
 *        void  if_freenameindex(struct if_nameindex *ptr);
 *
 *    The argument to this function must be a pointer that was returned by
 *    if_nameindex().
 */

struct if_nameindex *
if_nameindex(void)
{
	struct ifaddrs *ifaddrs, *ifa;
	unsigned int ni;
	int nbytes;
	struct if_nameindex *ifni, *ifni2;
	char *cp;

	if (getifaddrs(&ifaddrs) < 0)
		return(NULL);

	/*
	 * First, find out how many interfaces there are, and how
	 * much space we need for the string names.
	 */
	ni = 0;
	nbytes = 0;
	for (ifa = ifaddrs; ifa != NULL; ifa = ifa->ifa_next) {
		if (ifa->ifa_addr &&
		    ifa->ifa_addr->sa_family == AF_LINK) {
			nbytes += strlen(ifa->ifa_name) + 1;
			ni++;
		}
	}

	/*
	 * Next, allocate a chunk of memory, use the first part
	 * for the array of structures, and the last part for
	 * the strings.
	 */
	cp = malloc((ni + 1) * sizeof(struct if_nameindex) + nbytes);
	ifni = (struct if_nameindex *)cp;
	if (ifni == NULL)
		goto out;
	cp += (ni + 1) * sizeof(struct if_nameindex);

	/*
	 * Now just loop through the list of interfaces again,
	 * filling in the if_nameindex array and making copies
	 * of all the strings.
	 */
	ifni2 = ifni;
	for (ifa = ifaddrs; ifa != NULL; ifa = ifa->ifa_next) {
		if (ifa->ifa_addr &&
		    ifa->ifa_addr->sa_family == AF_LINK) {
			ifni2->if_index =
			    ((struct sockaddr_dl*)
			    (void *)ifa->ifa_addr)->sdl_index;
			ifni2->if_name = cp;
			strcpy(cp, ifa->ifa_name);
			ifni2++;
			cp += strlen(cp) + 1;
		}
	}
	/*
	 * Finally, don't forget to terminate the array.
	 */
	ifni2->if_index = 0;
	ifni2->if_name = NULL;
out:
	freeifaddrs(ifaddrs);
	return(ifni);
}

void
if_freenameindex(struct if_nameindex *ptr)
{
	free(ptr);
}
