/*++
/* NAME
/*	set_eugid 3
/* SUMMARY
/*	set effective user and group attributes
/* SYNOPSIS
/*	#include <set_eugid.h>
/*
/*	void	set_eugid(euid, egid)
/*	uid_t	euid;
/*	gid_t	egid;
/* DESCRIPTION
/*	set_eugid() sets the effective user and group process attributes
/*	and updates the process group access list to be just the specified
/*	effective group id.
/* DIAGNOSTICS
/*	All system call errors are fatal.
/* SEE ALSO
/*	seteuid(2), setegid(2), setgroups(2)
/* LICENSE
/* .ad
/* .fi
/*	The Secure Mailer license must be distributed with this software.
/* AUTHOR(S)
/*	Wietse Venema
/*	IBM T.J. Watson Research
/*	P.O. Box 704
/*	Yorktown Heights, NY 10598, USA
/*--*/

/* System library. */

#include <sys_defs.h>
#include <unistd.h>
#include <grp.h>

/* Utility library. */

#include "msg.h"
#include "set_eugid.h"

/* set_eugid - set effective user and group attributes */

void    set_eugid(uid_t euid, gid_t egid)
{
    if (geteuid() != 0)
	if (seteuid(0))
	    msg_fatal("set_eugid: seteuid(0): %m");
    if (setegid(egid) < 0)
	msg_fatal("set_eugid: setegid(%d): %m", egid);
    if (setgroups(1, &egid) < 0)
	msg_fatal("set_eugid: setgroups(%d): %m", egid);
    if (euid != 0 && seteuid(euid) < 0)
	msg_fatal("set_eugid: seteuid(%d): %m", euid);
    if (msg_verbose)
	msg_info("set_eugid: euid %d egid %d", euid, egid);
}
