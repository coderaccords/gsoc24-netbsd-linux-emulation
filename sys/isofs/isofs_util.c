/*
 *	$Id: isofs_util.c,v 1.6 1993/09/16 16:54:10 ws Exp $
 */
#include "param.h"
#include "systm.h"
#include "namei.h"
#include "resourcevar.h"
#include "kernel.h"
#include "file.h"
#include "stat.h"
#include "buf.h"
#include "proc.h"
#include "conf.h"
#include "mount.h"
#include "vnode.h"
#include "specdev.h"
#include "fifo.h"
#include "malloc.h"
#include "dir.h"

#include "iso.h"
#include "dirent.h"
#include "machine/endian.h"

#ifdef	__notanymore__
int
isonum_711 (p)
unsigned char *p;
{
	return (*p);
}

int
isonum_712 (p)
signed char *p;
{
	return (*p);
}

int
isonum_721 (p)
unsigned char *p;
{
	/* little endian short */
#if BYTE_ORDER != LITTLE_ENDIAN
	printf ("isonum_721 called on non little-endian machine!\n");
#endif

	return *(short *)p;
}

int
isonum_722 (p)
unsigned char *p;
{
        /* big endian short */
#if BYTE_ORDER != BIG_ENDIAN
        printf ("isonum_722 called on non big-endian machine!\n");
#endif

	return *(short *)p;
}

int
isonum_723 (p)
unsigned char *p;
{
#if BYTE_ORDER == BIG_ENDIAN
        return isonum_722 (p + 2);
#elif BYTE_ORDER == LITTLE_ENDIAN
	return isonum_721 (p);
#else
	printf ("isonum_723 unsupported byte order!\n");
	return 0;
#endif
}

int
isonum_731 (p)
unsigned char *p;
{
        /* little endian long */
#if BYTE_ORDER != LITTLE_ENDIAN
        printf ("isonum_731 called on non little-endian machine!\n");
#endif

	return *(long *)p;
}

int
isonum_732 (p)
unsigned char *p;
{
        /* big endian long */
#if BYTE_ORDER != BIG_ENDIAN
        printf ("isonum_732 called on non big-endian machine!\n");
#endif

	return *(long *)p;
}

int
isonum_733 (p)
unsigned char *p;
{
#if BYTE_ORDER == BIG_ENDIAN
        return isonum_732 (p + 4);
#elif BYTE_ORDER == LITTLE_ENDIAN
	return isonum_731 (p);
#else
	printf ("isonum_733 unsupported byte order!\n");
	return 0;
#endif
}
#endif	/* __notanymore__ */

/*
 * translate and compare a filename
 * Note: Version number plus ';' may be omitted.
 */
int
isofncmp(unsigned char *fn,int fnlen,unsigned char *isofn,int isolen)
{
	int i, j;
	char c;
	
	while (--fnlen >= 0) {
		if (--isolen < 0)
			return *fn;
		if ((c = *isofn++) == ';') {
			switch (*fn++) {
			default:
				return *--fn;
			case 0:
				return 0;
			case ';':
				break;
			}
			for (i = 0; --fnlen >= 0; i = i * 10 + *fn++ - '0') {
				if (*fn < '0' || *fn > '9') {
					return -1;
				}
			}
			for (j = 0; --isolen >= 0; j = j * 10 + *isofn++ - '0');
			return i - j;
		}
		if (c != *fn) {
			if (c >= 'A' && c <= 'Z') {
				if (c + ('a' - 'A') != *fn) {
					if (*fn >= 'a' && *fn <= 'z')
						return *fn - ('a' - 'A') - c;
					else
						return *fn - c;
				}
			} else
				return *fn - c;
		}
		fn++;
	}
	if (isolen > 0) {
		switch (*isofn) {
		default:
			return -1;
		case '.':
			if (isofn[1] != ';')
				return -1;
		case ';':
			return 0;
		}
	}
	return 0;
}

/*
 * translate a filename
 */
void
isofntrans(unsigned char *infn,int infnlen,
	   unsigned char *outfn,unsigned short *outfnlen,
	   int stripgen,int assoc)
{
	int fnidx;
	
	for (fnidx = 0; fnidx < infnlen; fnidx++) {
		char c = *infn++;
		
		if (c >= 'A' && c <= 'Z')
			*outfn++ = c + ('a' - 'A');
		else if (c == '.' && stripgen && *infn == ';')
			break;
		else if (c == ';' && stripgen)
			break;
		else
			*outfn++ = c;
	}
	if (assoc) {
		*outfn = ASSOCCHAR;
		fnidx++;
	}
	*outfnlen = fnidx;
}
