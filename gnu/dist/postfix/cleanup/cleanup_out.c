/*++
/* NAME
/*	cleanup_out 3
/* SUMMARY
/*	record output support
/* SYNOPSIS
/*	#include "cleanup.h"
/*
/*	int	CLEANUP_OUT_OK()
/*
/*	void	cleanup_out(type, data, len)
/*	int	type;
/*	char	*data;
/*	int	len;
/*
/*	void	cleanup_out_string(type, str)
/*	int	type;
/*	char	*str;
/*
/*	void	CLEANUP_OUT_BUF(type, buf)
/*	int	type;
/*	VSTRING	*buf;
/*
/*	void	cleanup_out_format(type, format, ...)
/*	int	type;
/*	char	*format;
/* DESCRIPTION
/*	This module writes records to the output stream.
/*
/*	CLEANUP_OUT_OK() is a macro that evaluates to non-zero
/*	as long as it makes sense to produce output. All output
/*	routines below check for this condition.
/*
/*	cleanup_out() is the main record output routine. It writes
/*	one record of the specified type, with the specified data
/*	and length to the output stream.
/*
/*	cleanup_out_string() outputs one string as a record.
/*
/*	CLEANUP_OUT_BUF() is an unsafe macro that outputs
/*	one string buffer as a record.
/*
/*	cleanup_out_format() formats its arguments and writes
/*	the result as a record.
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
#include <errno.h>
#include <stdlib.h>		/* 44BSD stdarg.h uses abort() */
#include <stdarg.h>
#include <string.h>

/* Utility library. */

#include <msg.h>
#include <vstring.h>
#include <vstream.h>

/* Global library. */

#include <record.h>
#include <rec_type.h>
#include <cleanup_user.h>

/* Application-specific. */

#include "cleanup.h"

/* cleanup_out - output one single record */

void    cleanup_out(int type, char *string, int len)
{
    if (CLEANUP_OUT_OK()) {
	if (rec_put(cleanup_dst, type, string, len) < 0) {
	    if (errno == EFBIG) {
		msg_warn("%s: queue file size limit exceeded",
			 cleanup_queue_id);
		cleanup_errs |= CLEANUP_STAT_SIZE;
	    } else {
		msg_warn("%s: write queue file: %m", cleanup_queue_id);
		cleanup_errs |= CLEANUP_STAT_WRITE;
	    }
	}
    }
}

/* cleanup_out_string - output string to one single record */

void    cleanup_out_string(int type, char *string)
{
    cleanup_out(type, string, strlen(string));
}

/* cleanup_out_format - output one formatted record */

void    cleanup_out_format(int type, char *fmt,...)
{
    static VSTRING *vp;
    va_list ap;

    if (vp == 0)
	vp = vstring_alloc(100);
    va_start(ap, fmt);
    vstring_vsprintf(vp, fmt, ap);
    va_end(ap);
    CLEANUP_OUT_BUF(type, vp);
}
