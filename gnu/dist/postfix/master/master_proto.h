/*++
/* NAME
/*	master_proto 3h
/* SUMMARY
/*	master process protocol
/* SYNOPSIS
/*	#include <master_proto.h>
/* DESCRIPTION
/* .nf

 /*
  * Transport names. The master passes the transport name on the command
  * line, and thus the name is part of the master to child protocol.
  */
#define MASTER_XPORT_NAME_UNIX	"unix"	/* local IPC */
#define MASTER_XPORT_NAME_FIFO	"fifo"	/* local IPC */
#define MASTER_XPORT_NAME_INET	"inet"	/* non-local IPC */

 /*
  * Format of a status message sent by a child process to the process
  * manager. Since this is between processes on the same machine we need not
  * worry about byte order and word length.
  */
typedef struct MASTER_STATUS {
    int     pid;			/* process ID */
    int     avail;			/* availability */
} MASTER_STATUS;

#define MASTER_STAT_TAKEN	0	/* this one is occupied */
#define MASTER_STAT_AVAIL	1	/* this process is idle */

extern int master_notify(int, int);	/* encapsulate status msg */

 /*
  * File descriptors inherited from the master process. All processes that
  * provide a given service share the same status file descriptor, and listen
  * on the same service socket(s). The kernel decides what process gets the
  * next connection. Usually the number of listening processes is small, so
  * one connection will not cause a "thundering herd" effect. When no process
  * listens on a given socket, the master process will. MASTER_LISTEN_FD is
  * actually the lowest-numbered descriptor of a sequence of descriptors to
  * listen on.
  */
#define MASTER_STATUS_FD	3	/* shared channel to parent */
#define MASTER_LISTEN_FD	4	/* accept connections here */

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

