#	$NetBSD: Makefile.cats,v 1.2 2001/05/29 02:20:22 mrg Exp $

# Makefile for NetBSD
#
# This makefile is constructed from a machine description:
#	config machineid
# Most changes should be made in the machine description
#	/sys/arch/cats/conf/``machineid''
# after which you should do
#	config machineid
# Machine generic makefile changes should be made in
#	/sys/arch/cats/conf/Makefile.cats
# after which config should be rerun for all machines of that type.

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
ARM32=		$S/arch/arm32
THISARM=	$S/arch/cats

HAVE_EGCS!=	${CC} --version | egrep "^(2\.[89]|egcs)" ; echo 
INCLUDES=	-I. -I$S/arch -I$S -nostdinc
CPPFLAGS=	${INCLUDES} ${IDENT} ${PARAM} -D_KERNEL -D_KERNEL_OPT -Dcats
CWARNFLAGS?=	-Werror -Wall -Wcomment -Wpointer-arith
# XXX Delete -Wuninitialized for now, since the compiler doesn't
# XXX always get it right.  --thorpej 
CWARNFLAGS+=	-Wno-uninitialized
.if (${HAVE_EGCS} != "")
CWARNFLAGS+=	-Wno-main
.endif
CFLAGS=		${DEBUG} ${COPTS} ${CWARNFLAGS}
AFLAGS=		-x assembler-with-cpp -D_LOCORE

LOADADDRESS=	0xF0000000
LINKFLAGS=	-Ttext ${LOADADDRESS} -e start
STRIPFLAGS=	-g

%INCLUDES

HOSTED_CC=	${CC}
HOSTED_CPPFLAGS=${CPPFLAGS:S/^-nostdinc$//}
HOSTED_CFLAGS=	${CFLAGS}

### find out what to use for libkern
KERN_AS=	obj
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
NOPROF_C=	${CC} ${CFLAGS} ${CPPFLAGS} -c $<
NORMAL_S=	${CC} ${AFLAGS} ${CPPFLAGS} -c $<

HOSTED_C=	${HOSTED_CC} ${HOSTED_CFLAGS} ${HOSTED_CPPFLAGS} -c $<

%OBJS

%CFILES

%SFILES

# load lines for config "xxx" will be emitted as:
# xxx: ${SYSTEM_DEP} swapxxx.o
#	${SYSTEM_LD_HEAD}
#	${SYSTEM_LD} swapxxx.o
#	${SYSTEM_LD_TAIL}
SYSTEM_OBJ=	locore.o \
		param.o ioconf.o ${OBJS} ${LIBCOMPAT} ${LIBKERN}
SYSTEM_DEP=	Makefile ${SYSTEM_OBJ}
SYSTEM_LD_HEAD=	rm -f $@
SYSTEM_LD=	@echo ${LD} ${LINKFLAGS} -o $@ '$${SYSTEM_OBJ}' vers.o; \
		${LD} ${LINKFLAGS} -o $@ ${SYSTEM_OBJ} vers.o
SYSTEM_LD_TAIL=	@${SIZE} $@; chmod 755 $@

DEBUG?=
.if ${DEBUG} == "-g"
LINKFLAGS+=	-X
SYSTEM_LD_TAIL+=; \
		echo mv -f $@ $@.gdb; mv -f $@ $@.gdb; \
		echo ${STRIP} ${STRIPFLAGS} -o $@ $@.gdb; \
		${STRIP} ${STRIPFLAGS} -o $@ $@.gdb
.else
#LINKFLAGS+=	-S
LINKFLAGS+=	-x
.endif

%LOAD

assym.h: $S/kern/genassym.sh ${ARM32}/arm32/genassym.cf
	sh $S/kern/genassym.sh ${CC} ${CFLAGS} ${CPPFLAGS} ${PROF} \
	    < ${ARM32}/arm32/genassym.cf > assym.h.tmp && \
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
	rm -f eddep *netbsd netbsd.gdb tags *.[io] [a-z]*.s \
	    [Ee]rrs linterrs makelinks assym.h.tmp assym.h

__CLEANDEPEND: .USE
	rm -f .depend

clean: __CLEANKERNEL

cleandir distclean: __CLEANKERNEL __CLEANDEPEND

lint:
	@lint -hbxncez -Dvolatile= ${CPPFLAGS} -UKGDB \
	    ${ARM32}/arm32/Locore.c ${CFILES}  \
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

SRCS=	${ARM32}/arm32/locore.S param.c ioconf.c ${CFILES} ${SFILES}
depend: .depend
.depend: ${SRCS} assym.h param.c
	${MKDEP} ${AFLAGS} ${CPPFLAGS} ${ARM32}/arm32/locore.S
	${MKDEP} -a ${CFLAGS} ${CPPFLAGS} param.c ioconf.c ${CFILES}
	test -z "${SFILES}" || ${MKDEP} -a ${AFLAGS} ${CPPFLAGS} ${SFILES}
	sh $S/kern/genassym.sh ${MKDEP} -f assym.dep ${CFLAGS} \
	    ${CPPFLAGS} < ${ARM32}/arm32/genassym.cf
	@sed -e 's/.*\.o:.*\.c/assym.h:/' < assym.dep >> .depend
	@rm -f assym.dep

dependall: depend all


# depend on root or device configuration
autoconf.o conf.o: Makefile
 
# depend on network
uipc_proto.o: Makefile 

# depend on maxusers
assym.h: Makefile

# depend on CPU configuration 
cpufunc.o cpufunc_asm.o: Makefile

# depend on DIAGNOSTIC etc.
cpuswitch.o fault.o machdep.o: Makefile


locore.o: ${ARM32}/arm32/locore.S assym.h
	${NORMAL_S}

# The install target can be redefined by putting a
# install-kernel-${MACHINE_NAME} target into /etc/mk.conf
MACHINE_NAME!=  uname -n
install: install-kernel-${MACHINE_NAME}
.if !target(install-kernel-${MACHINE_NAME}})
install-kernel-${MACHINE_NAME}:
	rm -f /onetbsd
	ln /netbsd /onetbsd
	cp netbsd /nnetbsd
	mv /nnetbsd /netbsd
.endif

%RULES
