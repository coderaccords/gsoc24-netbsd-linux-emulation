# This file is automatically generated.  DO NOT EDIT!
# Generated from: 	NetBSD: mknative-gcc,v 1.7 2003/08/22 00:24:46 mrg Exp 
#
G_CXX_EXTRA_HEADERS=
G_CXX_LIB2FUNCS=
G_CXX_LIB2SRCS=
G_INCLUDES=-I. -I. -I${GNUHOSTDIST}/gcc -I${GNUHOSTDIST}/gcc/.  -I${GNUHOSTDIST}/gcc/config -I${GNUHOSTDIST}/gcc/../include
G_LIB2ADD=
G_LIB2ADDEH=${GNUHOSTDIST}/gcc/unwind-dw2.c ${GNUHOSTDIST}/gcc/unwind-dw2-fde.c  ${GNUHOSTDIST}/gcc/unwind-sjlj.c ${GNUHOSTDIST}/gcc/unwind-c.c
G_LIB2FUNCS_1=_muldi3 _negdi2 _lshrdi3 _ashldi3 _ashrdi3 _ffsdi2 _clz  _cmpdi2 _ucmpdi2 _floatdidf _floatdisf _fixunsdfsi _fixunssfsi  _fixunsdfdi _fixdfdi _fixunssfdi _fixsfdi _fixxfdi _fixunsxfdi
G_LIB2FUNCS_2=_floatdixf _fixunsxfsi _fixtfdi _fixunstfdi _floatditf  _clear_cache _trampoline __main _exit _absvsi2 _absvdi2 _addvsi3  _addvdi3 _subvsi3 _subvdi3 _mulvsi3 _mulvdi3 _negvsi2 _negvdi2 _ctors
G_LIB2_DIVMOD_FUNCS=_divdi3 _moddi3 _udivdi3 _umoddi3 _udiv_w_sdiv _udivmoddi4
G_LIB2FUNCS_ST=_eprintf _bb __gcc_bcmp
G_LIBGCC2_CFLAGS=-O2  -DIN_GCC   -W -Wall -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -isystem ./include  -fpic -g -DHAVE_GTHR_DEFAULT -DIN_LIBGCC2 -D__GCC_FLOAT_NOT_NEEDED
G_MAYBE_USE_COLLECT2=
G_tm_defines=NETBSD_ENABLE_PTHREADS
G_xm_file=ansidecl.h  sh/little.h sh/sh.h dbxelf.h elfos.h sh/elf.h netbsd.h netbsd-elf.h sh/netbsd-elf.h defaults.h
G_xm_defines=POSIX
