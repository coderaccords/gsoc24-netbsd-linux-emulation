/*
 * Copyright (c) 1993, 1994, 1995, 1996, 1997
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that: (1) source code distributions
 * retain the above copyright notice and this paragraph in its entirety, (2)
 * distributions including binary code include the above copyright notice and
 * this paragraph in its entirety in the documentation or other materials
 * provided with the distribution, and (3) all advertising materials mentioning
 * features or use of this software display the following acknowledgement:
 * ``This product includes software developed by the University of California,
 * Lawrence Berkeley Laboratory and its contributors.'' Neither the name of
 * the University nor the names of its contributors may be used to endorse
 * or promote products derived from this software without specific prior
 * written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 * @(#) Header: /tcpdump/master/libpcap/lbl/os-osf5.h,v 1.2 2002-08-02 03:24:15 guy Exp (LBL)
 */

/*
 * Prototypes missing in Tru64 UNIX 5.x
 * XXX - "snprintf()" and "vsnprintf()" aren't missing, but you have to
 * #define the right value to get them defined by <stdio.h>.
 */
int	snprintf(char *, size_t, const char *, ...);
int	vsnprintf(char *, size_t, const char *, va_list);
int	pfopen(char *, int);
   
