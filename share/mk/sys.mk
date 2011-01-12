#	$NetBSD: sys.mk,v 1.101 2011/01/12 23:12:11 joerg Exp $
#	@(#)sys.mk	8.2 (Berkeley) 3/21/94

unix?=		We run NetBSD.

.SUFFIXES: .a .o .ln .s .S .c .cc .cpp .cxx .C .f .F .r .p .l .y .sh

.LIBS:		.a

AR?=		ar
ARFLAGS?=	rl
RANLIB?=	ranlib

AS?=		as
AFLAGS?=
COMPILE.s?=	${CC} ${AFLAGS} -c
LINK.s?=	${CC} ${AFLAGS} ${LDFLAGS}
.if ${MACHINE_ARCH} == "i386" || ${MACHINE_ARCH} == "x86_64"
_ASM_TRADITIONAL_CPP=	-x assembler-with-cpp
.else
_ASM_TRADITIONAL_CPP=	-traditional-cpp
.endif
COMPILE.S?=	${CC} ${AFLAGS} ${CPPFLAGS} ${_ASM_TRADITIONAL_CPP} -c
LINK.S?=	${CC} ${AFLAGS} ${CPPFLAGS} ${LDFLAGS}

CC?=		cc
.if ${MACHINE_ARCH} == "alpha" || \
    ${MACHINE_ARCH} == "arm" || \
    ${MACHINE_ARCH} == "x86_64" || \
    ${MACHINE_ARCH} == "armeb" || \
    ${MACHINE_ARCH} == "hppa" || \
    ${MACHINE_ARCH} == "i386" || \
    ${MACHINE_ARCH} == "m68k" || \
    ${MACHINE_ARCH} == "mipsel" || ${MACHINE_ARCH} == "mipseb" || \
    ${MACHINE_ARCH} == "mips64el" || ${MACHINE_ARCH} == "mips64eb" || \
    ${MACHINE_ARCH} == "powerpc" || \
    ${MACHINE_ARCH} == "sparc" || \
    ${MACHINE_ARCH} == "sparc64"
DBG?=	-O2
.elif ${MACHINE_ARCH} == "sh3el" || ${MACHINE_ARCH} == "sh3eb"
# -O2 is too -falign-* zealous for low-memory sh3 machines
DBG?=	-Os -freorder-blocks
.elif ${MACHINE_ARCH} == "vax"
DBG?=	-O1 -fgcse -fstrength-reduce -fgcse-after-reload
.elif ${MACHINE_ARCH} == "m68000"
# see src/doc/HACKS for details
DBG?=	-O1
.else
DBG?=	-O
.endif
CFLAGS?=	${DBG}
LDFLAGS?=
COMPILE.c?=	${CC} ${CFLAGS} ${CPPFLAGS} -c
LINK.c?=	${CC} ${CFLAGS} ${CPPFLAGS} ${LDFLAGS}

# C Type Format data is required for DTrace
# XXX TBD VERSION is not defined
CTFFLAGS	?=	-L VERSION
CTFMFLAGS	?=	-t -L VERSION

.if defined(MKDTRACE) && ${MKDTRACE} != "no"
CTFCONVERT	?=	${TOOL_CTFCONVERT}
CTFMERGE	?=	${TOOL_CTFMERGE}
.if defined(CFLAGS) && (${CFLAGS:M-g} != "")
CTFFLAGS	+=	-g
CTFMFLAGS	+=	-g
.else
CFLAGS		+=	-g
.endif
.endif

CXX?=		c++
CXXFLAGS?=	${CFLAGS:N-Wno-traditional:N-Wstrict-prototypes:N-Wmissing-prototypes:N-Wno-pointer-sign:N-ffreestanding:N-std=gnu99}

__ALLSRC1=	${empty(DESTDIR):?${.ALLSRC}:${.ALLSRC:S|^${DESTDIR}|^destdir|}}
__ALLSRC2=	${empty(MAKEOBJDIR):?${__ALLSRC1}:${__ALLSRC1:S|^${MAKEOBJDIR}|^obj|}}
__ALLSRC3=	${empty(NETBSDSRCDIR):?${__ALLSRC2}:${__ALLSRC2:S|^${NETBSDSRCDIR}|^src|}}

_CXXSEED?=	${BUILDSEED:D-frandom-seed=${BUILDSEED:Q}/${__ALLSRC3:O:Q}/${.TARGET:Q}}

COMPILE.cc?=	${CXX} ${_CXXSEED} ${CXXFLAGS} ${CPPFLAGS} -c
LINK.cc?=	${CXX} ${CXXFLAGS} ${CPPFLAGS} ${LDFLAGS}

OBJC?=		${CC}
OBJCFLAGS?=	${CFLAGS}
COMPILE.m?=	${OBJC} ${OBJCFLAGS} ${CPPFLAGS} -c
LINK.m?=	${OBJC} ${OBJCFLAGS} ${CPPFLAGS} ${LDFLAGS}

CPP?=		cpp
CPPFLAGS?=

FC?=		f77
FFLAGS?=	-O
RFLAGS?=
COMPILE.f?=	${FC} ${FFLAGS} -c
LINK.f?=	${FC} ${FFLAGS} ${LDFLAGS}
COMPILE.F?=	${FC} ${FFLAGS} ${CPPFLAGS} -c
LINK.F?=	${FC} ${FFLAGS} ${CPPFLAGS} ${LDFLAGS}
COMPILE.r?=	${FC} ${FFLAGS} ${RFLAGS} -c
LINK.r?=	${FC} ${FFLAGS} ${RFLAGS} ${LDFLAGS}

INSTALL?=	install

LD?=		ld

LEX?=		lex
LFLAGS?=
LEX.l?=		${LEX} ${LFLAGS}

LINT?=		lint
LINTFLAGS?=	-chapbxzFS

LORDER?=	lorder

MAKE?=		make

NM?=		nm

PC?=		pc
PFLAGS?=
COMPILE.p?=	${PC} ${PFLAGS} ${CPPFLAGS} -c
LINK.p?=	${PC} ${PFLAGS} ${CPPFLAGS} ${LDFLAGS}

SHELL?=		sh

SIZE?=		size

TSORT?= 	tsort -q

YACC?=		yacc
YFLAGS?=
YACC.y?=	${YACC} ${YFLAGS}

# C
.c:
	${LINK.c} -o ${.TARGET} ${.IMPSRC} ${LDLIBS}
.if defined(CTFCONVERT)
	${CTFCONVERT} ${CTFFLAGS} ${.TARGET}
.endif
.c.o:
	${COMPILE.c} ${.IMPSRC}
.if defined(CTFCONVERT)
	${CTFCONVERT} ${CTFFLAGS} ${.TARGET}
.endif
.c.a:
	${COMPILE.c} ${.IMPSRC}
	${AR} ${ARFLAGS} ${.TARGET} ${.PREFIX}.o
	rm -f ${.PREFIX}.o
.c.ln:
	${LINT} ${LINTFLAGS} \
	    ${CPPFLAGS:C/-([IDU])[  ]*/-\1/Wg:M-[IDU]*} \
	    -i ${.IMPSRC}

# C++
.cc .cpp .cxx .C:
	${LINK.cc} -o ${.TARGET} ${.IMPSRC} ${LDLIBS}
.cc.o .cpp.o .cxx.o .C.o:
	${COMPILE.cc} ${.IMPSRC}
.cc.a .cpp.a .cxx.a .C.a:
	${COMPILE.cc} ${.IMPSRC}
	${AR} ${ARFLAGS} ${.TARGET} ${.PREFIX}.o
	rm -f ${.PREFIX}.o

# Fortran/Ratfor
.f:
	${LINK.f} -o ${.TARGET} ${.IMPSRC} ${LDLIBS}
.f.o:
	${COMPILE.f} ${.IMPSRC}
.f.a:
	${COMPILE.f} ${.IMPSRC}
	${AR} ${ARFLAGS} ${.TARGET} ${.PREFIX}.o
	rm -f ${.PREFIX}.o

.F:
	${LINK.F} -o ${.TARGET} ${.IMPSRC} ${LDLIBS}
.if defined(CTFCONVERT)
	${CTFCONVERT} ${CTFFLAGS} ${.TARGET}
.endif
.F.o:
	${COMPILE.F} ${.IMPSRC}
.if defined(CTFCONVERT)
	${CTFCONVERT} ${CTFFLAGS} ${.TARGET}
.endif
.F.a:
	${COMPILE.F} ${.IMPSRC}
	${AR} ${ARFLAGS} ${.TARGET} ${.PREFIX}.o
	rm -f ${.PREFIX}.o

.r:
	${LINK.r} -o ${.TARGET} ${.IMPSRC} ${LDLIBS}
.r.o:
	${COMPILE.r} ${.IMPSRC}
.r.a:
	${COMPILE.r} ${.IMPSRC}
	${AR} ${ARFLAGS} ${.TARGET} ${.PREFIX}.o
	rm -f ${.PREFIX}.o

# Pascal
.p:
	${LINK.p} -o ${.TARGET} ${.IMPSRC} ${LDLIBS}
.if defined(CTFCONVERT)
	${CTFCONVERT} ${CTFFLAGS} ${.TARGET}
.endif
.p.o:
	${COMPILE.p} ${.IMPSRC}
.if defined(CTFCONVERT)
	${CTFCONVERT} ${CTFFLAGS} ${.TARGET}
.endif
.p.a:
	${COMPILE.p} ${.IMPSRC}
	${AR} ${ARFLAGS} ${.TARGET} ${.PREFIX}.o
	rm -f ${.PREFIX}.o

# Assembly
.s:
	${LINK.s} -o ${.TARGET} ${.IMPSRC} ${LDLIBS}
.if defined(CTFCONVERT)
	${CTFCONVERT} ${CTFFLAGS} ${.TARGET}
.endif
.s.o:
	${COMPILE.s} ${.IMPSRC}
.if defined(CTFCONVERT)
	${CTFCONVERT} ${CTFFLAGS} ${.TARGET}
.endif
.s.a:
	${COMPILE.s} ${.IMPSRC}
	${AR} ${ARFLAGS} ${.TARGET} ${.PREFIX}.o
	rm -f ${.PREFIX}.o
.S:
	${LINK.S} -o ${.TARGET} ${.IMPSRC} ${LDLIBS}
.if defined(CTFCONVERT)
	${CTFCONVERT} ${CTFFLAGS} ${.TARGET}
.endif
.S.o:
	${COMPILE.S} ${.IMPSRC}
.if defined(CTFCONVERT)
	${CTFCONVERT} ${CTFFLAGS} ${.TARGET}
.endif
.S.a:
	${COMPILE.S} ${.IMPSRC}
	${AR} ${ARFLAGS} ${.TARGET} ${.PREFIX}.o
	rm -f ${.PREFIX}.o

# Lex
.l:
	${LEX.l} ${.IMPSRC}
	${LINK.c} -o ${.TARGET} lex.yy.c ${LDLIBS} -ll
	rm -f lex.yy.c
.l.c:
	${LEX.l} ${.IMPSRC}
	mv lex.yy.c ${.TARGET}
.l.o:
	${LEX.l} ${.IMPSRC}
	${COMPILE.c} -o ${.TARGET} lex.yy.c
	rm -f lex.yy.c

# Yacc
.y:
	${YACC.y} ${.IMPSRC}
	${LINK.c} -o ${.TARGET} y.tab.c ${LDLIBS}
	rm -f y.tab.c
.y.c:
	${YACC.y} ${.IMPSRC}
	mv y.tab.c ${.TARGET}
.y.o:
	${YACC.y} ${.IMPSRC}
	${COMPILE.c} -o ${.TARGET} y.tab.c
	rm -f y.tab.c

# Shell
.sh:
	rm -f ${.TARGET}
	cp ${.IMPSRC} ${.TARGET}
	chmod a+x ${.TARGET}
