/* -*- Mode: C; tab-width: 4 -*-
 *
 * Copyright (c) 2004, Apple Computer, Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without 
 * modification, are permitted provided that the following conditions are met:
 *
 * 1.  Redistributions of source code must retain the above copyright notice, 
 *     this list of conditions and the following disclaimer. 
 * 2.  Redistributions in binary form must reproduce the above copyright notice, 
 *     this list of conditions and the following disclaimer in the documentation 
 *     and/or other materials provided with the distribution. 
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of its 
 *     contributors may be used to endorse or promote products derived from this 
 *     software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY 
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED 
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE 
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY 
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES 
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; 
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND 
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT 
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS 
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

   Change History (most recent first):

Log: dnssd_clientlib.c,v $
Revision 1.21  2009/04/01 21:10:11  herscher
<rdar://problem/5925472> Current Bonjour code does not compile on Windows. Use _stricmp and _strnicmp.

Revision 1.20  2008/11/26 20:57:37  cheshire
For consistency with other similar macros, renamed mdnsIsDigit/mdnsIsLetter/mdnsValidHostChar
to mDNSIsDigit/mDNSIsLetter/mDNSValidHostChar

Revision 1.19  2008/11/04 21:15:18  cheshire
<rdar://problem/5969564> Potential buffer overflows in DNSServiceConstructFullName

Revision 1.18  2007/11/30 23:06:10  cheshire
Fixed compile warning: declaration of 'index' shadows a global declaration

Revision 1.17  2007/10/02 19:36:04  cheshire
<rdar://problem/5516444> TXTRecordGetValuePtr should be case-insenstive

Revision 1.16  2007/09/18 19:09:02  cheshire
<rdar://problem/5489549> mDNSResponderHelper (and other binaries) missing SCCS version strings

Revision 1.15  2007/07/28 00:00:43  cheshire
Renamed CompileTimeAssertionCheck structure for consistency with others

Revision 1.14  2007/03/20 17:07:16  cheshire
Rename "struct uDNS_TCPSocket_struct" to "TCPSocket", "struct uDNS_UDPSocket_struct" to "UDPSocket"

Revision 1.13  2007/02/27 00:25:03  cheshire
<rdar://problem/5010640> DNSServiceConstructFullName() doesn't handle empty string for instance name

Revision 1.12  2007/02/07 19:32:00  cheshire
<rdar://problem/4980353> All mDNSResponder components should contain version strings in SCCS-compatible format

Revision 1.11  2006/08/14 23:05:53  cheshire
Added "tab-width" emacs header line

Revision 1.10  2005/04/06 02:06:56  shersche
Add DNSSD_API macro to TXTRecord API calls

Revision 1.9  2004/10/06 02:22:19  cheshire
Changed MacRoman copyright symbol (should have been UTF-8 in any case :-) to ASCII-compatible "(c)"

Revision 1.8  2004/10/01 22:15:55  rpantos
rdar://problem/3824265: Replace APSL in client lib with BSD license.

Revision 1.7  2004/06/26 03:16:34  shersche
clean up warning messages on Win32 platform

Submitted by: herscher

Revision 1.6  2004/06/12 01:09:45  cheshire
To be callable from the broadest range of clients on Windows (e.g. Visual Basic, C#, etc.)
API routines have to be declared as "__stdcall", instead of the C default, "__cdecl"

Revision 1.5  2004/05/25 18:29:33  cheshire
Move DNSServiceConstructFullName() from dnssd_clientstub.c to dnssd_clientlib.c,
so that it's also accessible to dnssd_clientshim.c (single address space) clients.

Revision 1.4  2004/05/25 17:08:55  cheshire
Fix compiler warning (doesn't make sense for function return type to be const)

Revision 1.3  2004/05/21 21:41:35  cheshire
Add TXT record building and parsing APIs

Revision 1.2  2004/05/20 22:22:21  cheshire
Enable code that was bracketed by "#if 0"

Revision 1.1  2004/03/12 21:30:29  cheshire
Build a System-Context Shared Library from mDNSCore, for the benefit of developers
like Muse Research who want to be able to use mDNS/DNS-SD from GPL-licensed code.

 */

#include <stdlib.h>
#include <string.h>

#include "dns_sd.h"

#if MDNS_BUILDINGSHAREDLIBRARY || MDNS_BUILDINGSTUBLIBRARY
#pragma export on
#endif

#if defined(_WIN32)
// disable warning "conversion from <data> to uint16_t"
#pragma warning(disable:4244)
#define strncasecmp _strnicmp
#define strcasecmp _stricmp 
#endif

/*********************************************************************************************
 *
 *  Supporting Functions
 *
 *********************************************************************************************/

#define mDNSIsDigit(X)     ((X) >= '0' && (X) <= '9')

// DomainEndsInDot returns 1 if name ends with a dot, 0 otherwise
// (DNSServiceConstructFullName depends this returning 1 for true, rather than any non-zero value meaning true)

static int DomainEndsInDot(const char *dom)
	{
	while (dom[0] && dom[1])
		{
		if (dom[0] == '\\') // advance past escaped byte sequence
			{
			if (mDNSIsDigit(dom[1]) && mDNSIsDigit(dom[2]) && mDNSIsDigit(dom[3]))
				dom += 4;			// If "\ddd"    then skip four
			else dom += 2;			// else if "\x" then skip two
			}
		else dom++;					// else goto next character
		}
	return (dom[0] == '.');
	}

static uint8_t *InternalTXTRecordSearch
	(
	uint16_t         txtLen,
	const void       *txtRecord,
	const char       *key,
	unsigned long    *keylen
	)
	{
	uint8_t *p = (uint8_t*)txtRecord;
	uint8_t *e = p + txtLen;
	*keylen = (unsigned long) strlen(key);
	while (p<e)
		{
		uint8_t *x = p;
		p += 1 + p[0];
		if (p <= e && *keylen <= x[0] && !strncasecmp(key, (char*)x+1, *keylen))
			if (*keylen == x[0] || x[1+*keylen] == '=') return(x);
		}
	return(NULL);
	}

/*********************************************************************************************
 *
 *  General Utility Functions
 *
 *********************************************************************************************/

// Note: Need to make sure we don't write more than kDNSServiceMaxDomainName (1009) bytes to fullName
// In earlier builds this constant was defined to be 1005, so to avoid buffer overruns on clients
// compiled with that constant we'll actually limit the output to 1005 bytes.

DNSServiceErrorType DNSSD_API DNSServiceConstructFullName
	(
	char       *const fullName,
	const char *const service,      // May be NULL
	const char *const regtype,
	const char *const domain
	)
	{
	const size_t len = !regtype ? 0 : strlen(regtype) - DomainEndsInDot(regtype);
	char       *fn   = fullName;
	char *const lim  = fullName + 1005;
	const char *s    = service;
	const char *r    = regtype;
	const char *d    = domain;

	// regtype must be at least "x._udp" or "x._tcp"
	if (len < 6 || !domain || !domain[0]) return kDNSServiceErr_BadParam;
	if (strncasecmp((regtype + len - 4), "_tcp", 4) && strncasecmp((regtype + len - 4), "_udp", 4)) return kDNSServiceErr_BadParam;

	if (service && *service)
		{
		while (*s)
			{
			unsigned char c = *s++;				// Needs to be unsigned, or values like 0xFF will be interpreted as < 32
			if (c <= ' ')						// Escape non-printable characters
				{
				if (fn+4 >= lim) goto fail;
				*fn++ = '\\';
				*fn++ = '0' + (c / 100);
				*fn++ = '0' + (c /  10) % 10;
				c     = '0' + (c      ) % 10;
				}
			else if (c == '.' || (c == '\\'))	// Escape dot and backslash literals
				{
				if (fn+2 >= lim) goto fail;
				*fn++ = '\\';
				}
			else
				if (fn+1 >= lim) goto fail;
			*fn++ = (char)c;
			}
		*fn++ = '.';
		}

	while (*r) if (fn+1 >= lim) goto fail; else *fn++ = *r++;
	if (!DomainEndsInDot(regtype)) { if (fn+1 >= lim) goto fail; else *fn++ = '.'; }

	while (*d) if (fn+1 >= lim) goto fail; else *fn++ = *d++;
	if (!DomainEndsInDot(domain)) { if (fn+1 >= lim) goto fail; else *fn++ = '.'; }

	*fn = '\0';
	return kDNSServiceErr_NoError;

fail:
	*fn = '\0';
	return kDNSServiceErr_BadParam;
	}

/*********************************************************************************************
 *
 *   TXT Record Construction Functions
 *
 *********************************************************************************************/

typedef struct _TXTRecordRefRealType
	{
	uint8_t  *buffer;		// Pointer to data
	uint16_t buflen;		// Length of buffer
	uint16_t datalen;		// Length currently in use
	uint16_t malloced;	// Non-zero if buffer was allocated via malloc()
	} TXTRecordRefRealType;

#define txtRec ((TXTRecordRefRealType*)txtRecord)

// The opaque storage defined in the public dns_sd.h header is 16 bytes;
// make sure we don't exceed that.
struct CompileTimeAssertionCheck_dnssd_clientlib
	{
	char assert0[(sizeof(TXTRecordRefRealType) <= 16) ? 1 : -1];
	};

void DNSSD_API TXTRecordCreate
	(
	TXTRecordRef     *txtRecord,
	uint16_t         bufferLen,
	void             *buffer
	)
	{
	txtRec->buffer   = buffer;
	txtRec->buflen   = buffer ? bufferLen : (uint16_t)0;
	txtRec->datalen  = 0;
	txtRec->malloced = 0;
	}

void DNSSD_API TXTRecordDeallocate(TXTRecordRef *txtRecord)
	{
	if (txtRec->malloced) free(txtRec->buffer);
	}

DNSServiceErrorType DNSSD_API TXTRecordSetValue
	(
	TXTRecordRef     *txtRecord,
	const char       *key,
	uint8_t          valueSize,
	const void       *value
	)
	{
	uint8_t *start, *p;
	const char *k;
	unsigned long keysize, keyvalsize;

	for (k = key; *k; k++) if (*k < 0x20 || *k > 0x7E || *k == '=') return(kDNSServiceErr_Invalid);
	keysize = (unsigned long)(k - key);
	keyvalsize = 1 + keysize + (value ? (1 + valueSize) : 0);
	if (keysize < 1 || keyvalsize > 255) return(kDNSServiceErr_Invalid);
	(void)TXTRecordRemoveValue(txtRecord, key);
	if (txtRec->datalen + keyvalsize > txtRec->buflen)
		{
		unsigned char *newbuf;
		unsigned long newlen = txtRec->datalen + keyvalsize;
		if (newlen > 0xFFFF) return(kDNSServiceErr_Invalid);
		newbuf = malloc((size_t)newlen);
		if (!newbuf) return(kDNSServiceErr_NoMemory);
		memcpy(newbuf, txtRec->buffer, txtRec->datalen);
		if (txtRec->malloced) free(txtRec->buffer);
		txtRec->buffer = newbuf;
		txtRec->buflen = (uint16_t)(newlen);
		txtRec->malloced = 1;
		}
	start = txtRec->buffer + txtRec->datalen;
	p = start + 1;
	memcpy(p, key, keysize);
	p += keysize;
	if (value)
		{
		*p++ = '=';
		memcpy(p, value, valueSize);
		p += valueSize;
		}
	*start = (uint8_t)(p - start - 1);
	txtRec->datalen += p - start;
	return(kDNSServiceErr_NoError);
	}

DNSServiceErrorType DNSSD_API TXTRecordRemoveValue
	(
	TXTRecordRef     *txtRecord,
	const char       *key
	)
	{
	unsigned long keylen, itemlen, remainder;
	uint8_t *item = InternalTXTRecordSearch(txtRec->datalen, txtRec->buffer, key, &keylen);
	if (!item) return(kDNSServiceErr_NoSuchKey);
	itemlen   = (unsigned long)(1 + item[0]);
	remainder = (unsigned long)((txtRec->buffer + txtRec->datalen) - (item + itemlen));
	// Use memmove because memcpy behaviour is undefined for overlapping regions
	memmove(item, item + itemlen, remainder);
	txtRec->datalen -= itemlen;
	return(kDNSServiceErr_NoError);
	}

uint16_t DNSSD_API TXTRecordGetLength  (const TXTRecordRef *txtRecord) { return(txtRec->datalen); }
const void * DNSSD_API TXTRecordGetBytesPtr(const TXTRecordRef *txtRecord) { return(txtRec->buffer); }

/*********************************************************************************************
 *
 *   TXT Record Parsing Functions
 *
 *********************************************************************************************/

int DNSSD_API TXTRecordContainsKey
	(
	uint16_t         txtLen,
	const void       *txtRecord,
	const char       *key
	)
	{
	unsigned long keylen;
	return (InternalTXTRecordSearch(txtLen, txtRecord, key, &keylen) ? 1 : 0);
	}

const void * DNSSD_API TXTRecordGetValuePtr
	(
	uint16_t         txtLen,
	const void       *txtRecord,
	const char       *key,
	uint8_t          *valueLen
	)
	{
	unsigned long keylen;
	uint8_t *item = InternalTXTRecordSearch(txtLen, txtRecord, key, &keylen);
	if (!item || item[0] <= keylen) return(NULL);	// If key not found, or found with no value, return NULL
	*valueLen = (uint8_t)(item[0] - (keylen + 1));
	return (item + 1 + keylen + 1);
	}

uint16_t DNSSD_API TXTRecordGetCount
	(
	uint16_t         txtLen,
	const void       *txtRecord
	)
	{
	uint16_t count = 0;
	uint8_t *p = (uint8_t*)txtRecord;
	uint8_t *e = p + txtLen;
	while (p<e) { p += 1 + p[0]; count++; }
	return((p>e) ? (uint16_t)0 : count);
	}

DNSServiceErrorType DNSSD_API TXTRecordGetItemAtIndex
	(
	uint16_t         txtLen,
	const void       *txtRecord,
	uint16_t         itemIndex,
	uint16_t         keyBufLen,
	char             *key,
	uint8_t          *valueLen,
	const void       **value
	)
	{
	uint16_t count = 0;
	uint8_t *p = (uint8_t*)txtRecord;
	uint8_t *e = p + txtLen;
	while (p<e && count<itemIndex) { p += 1 + p[0]; count++; }	// Find requested item
	if (p<e && p + 1 + p[0] <= e)	// If valid
		{
		uint8_t *x = p+1;
		unsigned long len = 0;
		e = p + 1 + p[0];
		while (x+len<e && x[len] != '=') len++;
		if (len >= keyBufLen) return(kDNSServiceErr_NoMemory);
		memcpy(key, x, len);
		key[len] = 0;
		if (x+len<e)		// If we found '='
			{
			*value = x + len + 1;
			*valueLen = (uint8_t)(p[0] - (len + 1));
			}
		else
			{
			*value = NULL;
			*valueLen = 0;
			}
		return(kDNSServiceErr_NoError);
		}
	return(kDNSServiceErr_Invalid);
	}

/*********************************************************************************************
 *
 *   SCCS-compatible version string
 *
 *********************************************************************************************/

// For convenience when using the "strings" command, this is the last thing in the file

// Note: The C preprocessor stringify operator ('#') makes a string from its argument, without macro expansion
// e.g. If "version" is #define'd to be "4", then STRINGIFY_AWE(version) will return the string "version", not "4"
// To expand "version" to its value before making the string, use STRINGIFY(version) instead
#define STRINGIFY_ARGUMENT_WITHOUT_EXPANSION(s) #s
#define STRINGIFY(s) STRINGIFY_ARGUMENT_WITHOUT_EXPANSION(s)

// NOT static -- otherwise the compiler may optimize it out
// The "@(#) " pattern is a special prefix the "what" command looks for
const char VersionString_SCCS_libdnssd[] = "@(#) libdns_sd " STRINGIFY(mDNSResponderVersion);
