/*
 * Copyright (c) 1992 Carnegie Mellon University
 * All Rights Reserved.
 * 
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND FOR
 * ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software_Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie Mellon
 * the rights to redistribute these changes.
 */
/*
 *	supcdefs.h -- Declarations shared by the collection of files
 *			that build the sup client.
 *
 **********************************************************************
 * HISTORY
 * 7-July-93  Nate Williams at Montana State University
 *	Modified SUP to use gzip based compression when sending files
 *	across the network to save BandWidth
 *
 * $Log: supcdefs.h,v $
 * Revision 1.4  1996/12/23 19:42:16  christos
 * - add missing prototypes.
 * - fix function call inconsistencies
 * - fix int <-> long and pointer conversions
 * It should run now on 64 bit machines...
 *
 * Revision 1.3  1996/09/05 16:50:07  christos
 * - for portability make sure that we never use "" as a pathname, always convert
 *   it to "."
 * - include sockio.h if needed to define SIOCGIFCONF (for svr4)
 * - use POSIX signals and wait macros
 * - add -S silent flag, so that the client does not print messages unless there
 *   is something wrong
 * - use flock or lockf as appropriate
 * - use fstatfs or fstatvfs to find out if a filesystem is mounted over nfs,
 *   don't depend on the major() = 255 hack; it only works on legacy systems.
 * - use gzip -cf to make sure that gzip compresses the file even when the file
 *   would expand.
 * - punt on defining vsnprintf if _IOSTRG is not defined; use sprintf...
 *
 * To compile sup on systems other than NetBSD, you'll need a copy of daemon.c,
 * vis.c, vis.h and sys/cdefs.h. Maybe we should keep those in the distribution?
 *
 * Revision 1.2  1993/08/04 17:46:16  brezak
 * Changes from nate for gzip'ed sup
 *
 * Revision 1.1.1.1  1993/05/21  14:52:18  cgd
 * initial import of CMU's SUP to NetBSD
 *
 * Revision 1.6  92/08/11  12:06:52  mrt
 * 	Added CFURELSUF  - use-release-suffix flag
 * 	Made rpause code conditional on MACH rather than CMUCS
 * 	[92/07/26            mrt]
 * 
 * Revision 1.5  92/02/08  18:23:57  mja
 * 	Added CFKEEP flag.
 * 	[92/01/17            vdelvecc]
 * 
 * 10-Feb-88  Glenn Marcy (gm0w) at Carnegie-Mellon University
 *	Added timeout for backoff.
 *
 * 28-Jun-87  Glenn Marcy (gm0w) at Carnegie-Mellon University
 *	Added Crelease for "release" support.
 *
 * 25-May-87  Doug Philips (dwp) at Carnegie-Mellon University
 *	Created.
 *
 **********************************************************************
 */

#include <libc.h>
#include <netdb.h>
#include <signal.h>
#include <setjmp.h>
#include <pwd.h>
#include <grp.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/file.h>
#include <sys/errno.h>
#if	MACH			/* used by resource pausing code only */
#include <sys/ioctl.h>
#include <sys/resource.h>
#endif	/* MACH */
#include <c.h>
#include "sup.h"
#include "supmsg.h"

extern int errno;
extern int PGMVERSION;

/*******************************************
 ***    D A T A   S T R U C T U R E S    ***
 *******************************************/

struct collstruct {			/* one per collection to be upgraded */
	char *Cname;			/* collection name */
	TREE *Chost;			/* attempted host for collection */
	TREE *Chtree;			/* possible hosts for collection */
	char *Cbase;			/* local base directory */
	char *Chbase;			/* remote base directory */
	char *Cprefix;			/* local collection pathname prefix */
	char *Crelease;			/* release name */
	char *Cnotify;			/* user to notify of status */
	char *Clogin;			/* remote login name */
	char *Cpswd;			/* remote password */
	char *Ccrypt;			/* data encryption key */
	int Ctimeout;			/* timeout for backoff */
	int Cflags;			/* collection flags */
	int Cnogood;			/* upgrade no good, "when" unchanged */
	int Clockfd;			/* >= 0 if collection is locked */
	struct collstruct *Cnext;	/* next collection */
};
typedef struct collstruct COLLECTION;

#define CFALL		00001
#define CFBACKUP	00002
#define CFDELETE	00004
#define CFEXECUTE	00010
#define CFLIST		00020
#define CFLOCAL		00040
#define CFMAIL		00100
#define CFOLD		00200
#define CFVERBOSE	00400
#define CFKEEP		01000
#define CFURELSUF	02000
#define CFCOMPRESS	04000
#define CFSILENT	10000

/*************************
 ***	M A C R O S    ***
 *************************/

#define vnotify	if (thisC->Cflags&CFVERBOSE)  notify
