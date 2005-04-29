/* Message list charset and locale charset handling.
   Copyright (C) 2001-2003 Free Software Foundation, Inc.
   Written by Bruno Haible <haible@clisp.cons.org>, 2001.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

#ifndef _MSGL_CHARSET_H
#define _MSGL_CHARSET_H

#include "message.h"


#ifdef __cplusplus
extern "C" {
#endif


extern void
       compare_po_locale_charsets (const msgdomain_list_ty *mdlp);


#ifdef __cplusplus
}
#endif


#endif /* _MSGL_CHARSET_H */
