#	$NetBSD: Makefile.evbsh3.el,v 1.1 1999/09/13 10:30:23 itojun Exp $

# Makefile for NetBSD
#
# This makefile is constructed from a machine description:
#	config machineid
# Most changes should be made in the machine description
#	/sys/arch/sh3/conf/``machineid''
# after which you should do
#	config machineid
# Machine generic makefile changes should be made in
#	/sys/arch/sh3/conf/Makefile.sh3
# after which config should be rerun for all machines of that type.
#
# N.B.: NO DEPENDENCIES ON FOLLOWING FLAGS ARE VISIBLE TO MAKEFILE
#	IF YOU CHANGE THE DEFINITION OF ANY OF THESE RECOMPILE EVERYTHING
#
# -DTRACE	compile in kernel tracing hooks
# -DQUOTA	compile in file system quotas

# DEBUG is set to -g if debugging.
# PROF is set to -pg if profiling.

AR?=	ar
AS?=	as
CC?=	cc
CPP?=	cpp
LD?=	ld
LORDER?=lorder
MKDEP?=	mkdep
NM?=	nm
RANLIB?=ranlib
SIZE?=	size
STRIP?=	strip
TSORT?=	tsort -q

COPTS?=	-O2

# source tree is located via $S relative to the compilation directory
.ifndef S
S!=	cd ../../../..; pwd
.endif
SH3=	$S/arch/sh3
EVBSH3=	$S/arch/evbsh3

HAVE_GCC28!=	${CC} --version | egrep "^(2\.8|egcs)" ; echo
INCLUDES=	-I. -I$S/arch -I$S -nostdinc
CPPFLAGS=	${INCLUDES} ${IDENT} ${PARAM} -D_KERNEL \
		-Dsh3
CWARNFLAGS=	-Werror -Wall -Wmissing-prototypes -Wstrict-prototypes
CWARNFLAGS+=	-Wpointer-arith -Wno-parentheses
.if (${HAVE_GCC28} != "")
CWARNFLAGS+=	-Wno-main
.endif
CFLAGS=		${DEBUG} ${COPTS} ${CWARNFLAGS}
AFLAGS=		-x assembler-with-cpp -traditional-cpp -D_LOCORE
LINKFLAGS=	-e start -Map netbsd.map -T ../../conf/shl.x
LINKFLAGS2=	-e start -Map netbsd2.map -T ../../conf/shl.x.RAM
LINKFLAGS3=	-e start -Map netbsd3.map -T ../../conf/shl.x.ICE
STRIPFLAGS=	--strip-debug
MACHINE=evbsh3
MACHINE_ARCH=sh3

### find out what to use for libkern
.include "$S/lib/libkern/Makefile.inc"
.ifndef PROF
LIBKERN=	${KERNLIB}
.else
LIBKERN=	${KERNLIB_PROF}
.endif

### find out what to use for libcompat
.include "$S/compat/common/Makefile.inc"
.ifndef PROF
LIBCOMPAT=	${COMPATLIB}
.else
LIBCOMPAT=	${COMPATLIB_PROF}
.endif

# compile rules: rules are named ${TYPE}_${SUFFIX} where TYPE is NORMAL or
# HOSTED}, and SUFFIX is the file suffix, capitalized (e.g. C for a .c file).

NORMAL_C=	${CC} ${CFLAGS} ${CPPFLAGS} ${PROF} -c $<
NORMAL_S=	${CC} ${AFLAGS} ${CPPFLAGS} -c $<

%OBJS

%CFILES

%SFILES

# load lines for config "xxx" will be emitted as:
# xxx: ${SYSTEM_DEP} swapxxx.o
#	${SYSTEM_LD_HEAD}
#	${SYSTEM_LD} swapxxx.o
#	${SYSTEM_LD_TAIL}
MMEYEROOT=/usr/home/mmEye/root
SYSTEM_OBJ=	locore.o \
		param.o ioconf.o ${OBJS} ${LIBKERN} ${LIBCOMPAT}
SYSTEM_DEP=	Makefile ${SYSTEM_OBJ}
SYSTEM_LD_HEAD=	rm -f $@
SYSTEM_LD=	@echo ${LD} ${LINKFLAGS} -o $@.out '$${SYSTEM_OBJ}' vers.o; \
		${LD} ${LINKFLAGS} -o $@.out ${SYSTEM_OBJ} vers.o 
SYSTEM_LD_TAIL=	@echo ${LD} ${LINKFLAGS2} -o $@2.out '$${SYSTEM_OBJ}' vers.o; \
		${LD} ${LINKFLAGS2} -o $@2.out ${SYSTEM_OBJ} vers.o swapnetbsd.o ; \
		${LD} ${LINKFLAGS3} -o $@3.out ${SYSTEM_OBJ} vers.o swapnetbsd.o ; \
		cp $@.out $@; ${STRIP} $@ ; \
		${SIZE} $@; chmod 755 $@ ; \
		echo ${OBJCOPY} -O srec $@.out $@.s; ${OBJCOPY} -O srec $@.out $@.s



DEBUG?=
.if ${DEBUG} == "-g"
LINKFLAGS+=	-X
SYSTEM_LD_TAIL+=; \
		echo cp $@ $@.gdb; rm -f $@.gdb; cp $@ $@.gdb; \
		echo ${STRIP} ${STRIPFLAGS} $@; ${STRIP} ${STRIPFLAGS} $@
.else
LINKFLAGS+=	
.endif

%LOAD

assym.h: $S/kern/genassym.sh ${EVBSH3}/evbsh3/genassym.cf
	sh $S/kern/genassym.sh ${CC} ${CFLAGS} ${CPPFLAGS} ${PROF} \
	    < ${EVBSH3}/evbsh3/genassym.cf > assym.h.tmp && \
	mv -f assym.h.tmp assym.h

param.c: $S/conf/param.c
	rm -f param.c
	cp $S/conf/param.c .

param.o: param.c Makefile
	${NORMAL_C}

ioconf.o: ioconf.c
	${NORMAL_C}

newvers: ${SYSTEM_DEP} ${SYSTEM_SWAP_DEP}
	sh $S/conf/newvers.sh
	${CC} ${CFLAGS} ${CPPFLAGS} ${PROF} -c vers.c


__CLEANKERNEL: .USE
	@echo "${.TARGET}ing the kernel objects"
	rm -f eddep *netbsd netbsd netbsd.gdb tags *.[io] [a-z]*.s \
	    [Ee]rrs linterrs makelinks assym.h.tmp assym.h

__CLEANDEPEND: .USE
	rm -f .depend

clean: __CLEANKERNEL

cleandir: __CLEANKERNEL __CLEANDEPEND

lint:
	@lint -hbxncez -Dvolatile= ${CPPFLAGS} -UKGDB \
	    ${SH3}/sh3/Locore.c ${CFILES} \
	    ioconf.c param.c | \
	    grep -v 'static function .* unused'

tags:
	@echo "see $S/kern/Makefile for tags"

links:
	egrep '#if' ${CFILES} | sed -f $S/conf/defines | \
	  sed -e 's/:.*//' -e 's/\.c/.o/' | sort -u > dontlink
	echo ${CFILES} | tr -s ' ' '\12' | sed 's/\.c/.o/' | \
	  sort -u | comm -23 - dontlink | \
	  sed 's,../.*/\(.*.o\),rm -f \1; ln -s ../GENERIC/\1 \1,' > makelinks
	sh makelinks && rm -f dontlink

SRCS=	${EVBSH3}/evbsh3/locore.s \
	param.c ioconf.c ${CFILES} ${SFILES}
depend: .depend
.depend: ${SRCS} assym.h param.c
	${MKDEP} ${AFLAGS} ${CPPFLAGS} ${EVBSH3}/evbsh3/locore.s
	${MKDEP} -a ${CFLAGS} ${CPPFLAGS} param.c ioconf.c ${CFILES}
	sh $S/kern/genassym.sh ${MKDEP} -f assym.dep ${CFLAGS} \
	  ${CPPFLAGS} < ${EVBSH3}/evbsh3/genassym.cf
	@sed -e 's/.*\.o:.*\.c/assym.h:/' < assym.dep >> .depend
	@rm -f assym.dep


# depend on root or device configuration
autoconf.o conf.o: Makefile
 
# depend on network or filesystem configuration 
uipc_proto.o vfs_conf.o: Makefile 

# depend on maxusers
machdep.o: Makefile

# depend on CPU configuration 
locore.o machdep.o: Makefile


locore.o: ${EVBSH3}/evbsh3/locore.s assym.h
	${NORMAL_S}

%RULES
