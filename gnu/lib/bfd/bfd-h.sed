#	$NetBSD: bfd-h.sed,v 1.2 1998/08/22 20:24:31 tv Exp $
# Preparse bfd.h such that it can be used on multiple machines.

s/@VERSION@/2.8.1/
/@wordsize@/{
	i\
#if defined(__alpha__) || defined(__sparc__)\
#define BFD_ARCH_SIZE 64\
#else\
#define BFD_ARCH_SIZE 32\
#endif
	d
}
/@BFD_HOST_64BIT_LONG@/ {
	i\
#if defined(__alpha__)\
#define BFD_HOST_64BIT_LONG 1\
#else\
#define BFD_HOST_64BIT_LONG 1\
#endif\
#include <sys/types.h>
	d
}
s/@BFD_HOST_64_BIT_DEFINED@/1/
s/@BFD_HOST_64_BIT@/int64_t/
s/@BFD_HOST_U_64_BIT@/u_int64_t/
