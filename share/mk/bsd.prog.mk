#	$NetBSD: bsd.prog.mk,v 1.64 1997/03/27 09:10:10 mycroft Exp $
#	@(#)bsd.prog.mk	5.26 (Berkeley) 6/25/91

.if exists(${.CURDIR}/../Makefile.inc)
.include "${.CURDIR}/../Makefile.inc"
.endif

.include <bsd.own.mk>

.SUFFIXES: .out .o .c .cc .C .y .l .s .8 .7 .6 .5 .4 .3 .2 .1 .0

CFLAGS+=	${COPTS}

LIBCRT0?=	${DESTDIR}/usr/lib/crt0.o
LIBC?=		${DESTDIR}/usr/lib/libc.a
LIBC_PIC?=	${DESTDIR}/usr/lib/libc_pic.a
LIBCOMPAT?=	${DESTDIR}/usr/lib/libcompat.a
LIBCRYPT?=	${DESTDIR}/usr/lib/libcrypt.a
LIBCURSES?=	${DESTDIR}/usr/lib/libcurses.a
LIBDBM?=	${DESTDIR}/usr/lib/libdbm.a
LIBDES?=	${DESTDIR}/usr/lib/libdes.a
LIBEDIT?=	${DESTDIR}/usr/lib/libedit.a
LIBGCC?=	${DESTDIR}/usr/lib/libgcc.a
LIBGNUMALLOC?=	${DESTDIR}/usr/lib/libgnumalloc.a
LIBKDB?=	${DESTDIR}/usr/lib/libkdb.a
LIBKRB?=	${DESTDIR}/usr/lib/libkrb.a
LIBKVM?=	${DESTDIR}/usr/lib/libkvm.a
LIBL?=		${DESTDIR}/usr/lib/libl.a
LIBM?=		${DESTDIR}/usr/lib/libm.a
LIBMP?=		${DESTDIR}/usr/lib/libmp.a
LIBPC?=		${DESTDIR}/usr/lib/libpc.a
LIBPCAP?=	${DESTDIR}/usr/lib/libpcap.a
LIBPLOT?=	${DESTDIR}/usr/lib/libplot.a
LIBRESOLV?=	${DESTDIR}/usr/lib/libresolv.a
LIBRPCSVC?=	${DESTDIR}/usr/lib/librpcsvc.a
LIBSKEY?=	${DESTDIR}/usr/lib/libskey.a
LIBTERMCAP?=	${DESTDIR}/usr/lib/libtermcap.a
LIBTELNET?=	${DESTDIR}/usr/lib/libtelnet.a
LIBUTIL?=	${DESTDIR}/usr/lib/libutil.a
LIBWRAP?=	${DESTDIR}/usr/lib/libwrap.a
LIBY?=		${DESTDIR}/usr/lib/liby.a
LIBZ?=		${DESTDIR}/usr/lib/libz.a

.if defined(SHAREDSTRINGS)
CLEANFILES+=strings
.c.o:
	${CC} -E ${CFLAGS} ${.IMPSRC} | xstr -c -
	@${CC} ${CFLAGS} -c x.c -o ${.TARGET}
	@rm -f x.c

.cc.o:
	${CXX} -E ${CXXFLAGS} ${.IMPSRC} | xstr -c -
	@mv -f x.c x.cc
	@${CXX} ${CXXFLAGS} -c x.cc -o ${.TARGET}
	@rm -f x.cc

.C.o:
	${CXX} -E ${CXXFLAGS} ${.IMPSRC} | xstr -c -
	@mv -f x.c x.C
	@${CXX} ${CXXFLAGS} -c x.C -o ${.TARGET}
	@rm -f x.C
.endif


.if defined(PROG)
SRCS?=	${PROG}.c
.if !empty(SRCS:N*.h:N*.sh)
OBJS+=	${SRCS:N*.h:N*.sh:R:S/$/.o/g}
LOBJS+=	${LSRCS:.c=.ln} ${SRCS:M*.c:.c=.ln}
.endif

.if defined(OBJS) && !empty(OBJS)
.if defined(DESTDIR)

${PROG}: ${LIBCRT0} ${OBJS} ${LIBC} ${DPADD}
	${CC} ${LDFLAGS} ${LDSTATIC} -o ${.TARGET} -nostdlib -L${DESTDIR}/usr/lib ${LIBCRT0} ${OBJS} ${LDADD} -lgcc -lc -lgcc

.else

${PROG}: ${LIBCRT0} ${OBJS} ${LIBC} ${DPADD}
	${CC} ${LDFLAGS} ${LDSTATIC} -o ${.TARGET} ${OBJS} ${LDADD}

.endif	# defined(DESTDIR)
.endif	# defined(OBJS) && !empty(OBJS)

.if	!defined(MAN)
MAN=	${PROG}.1
.endif	# !defined(MAN)
.endif	# defined(PROG)

.MAIN: all
all: ${PROG} _SUBDIRUSE

.if !target(clean)
cleanprog:
	rm -f a.out [Ee]rrs mklog core *.core \
	    ${PROG} ${OBJS} ${LOBJS} ${CLEANFILES}

clean: _SUBDIRUSE cleanprog
cleandir: _SUBDIRUSE cleanprog
.else
cleandir: _SUBDIRUSE clean
.endif

.if defined(SRCS)
afterdepend: .depend
	@(TMP=/tmp/_depend$$$$; \
	    sed -e 's/^\([^\.]*\).o[ ]*:/\1.o \1.ln:/' \
	      < .depend > $$TMP; \
	    mv $$TMP .depend)
.endif

.if !target(install)
.if !target(beforeinstall)
beforeinstall:
.endif
.if !target(afterinstall)
afterinstall:
.endif

.if !target(realinstall)

.if defined(PROG)
PROGNAME?= ${PROG}
proginstall:: ${DESTDIR}${BINDIR}/${PROGNAME}
.if !defined(UPDATE)
.PHONY: ${DESTDIR}${BINDIR}/${PROGNAME}
.endif
.if !defined(BUILD)
${DESTDIR}${BINDIR}/${PROGNAME}: .MADE
.endif

${DESTDIR}${BINDIR}/${PROGNAME}: ${PROG}
	${INSTALL} ${COPY} ${STRIP} -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    ${.ALLSRC} ${.TARGET}
.endif

.if defined(FILES)
FILESDIR?=${BINDIR}
FILESOWN?=${BINOWN}
FILESGRP?=${BINGRP}
FILESMODE?=${NONBINMODE}
.for F in ${FILES}
FILESDIR_${F}?=${FILESDIR}
FILESOWN_${F}?=${FILESOWN}
FILESGRP_${F}?=${FILESGRP}
FILESMODE_${F}?=${FILESMODE}
.if defined(FILESNAME)
FILESNAME_${F} ?= ${FILESNAME}
.else
FILESNAME_${F} ?= ${F:T}
.endif
FILESDIR_${F} ?= ${FILESDIR}
proginstall:: ${DESTDIR}${FILESDIR_${F}}/${FILESNAME_${F}}
.if !defined(UPDATE)
.PHONY: ${DESTDIR}${FILESDIR_${F}}/${FILESNAME_${F}}
.endif
.if !defined(BUILD)
${DESTDIR}${FILESDIR_${F}}/${FILESNAME_${F}}: .MADE
.endif

${DESTDIR}${FILESDIR_${F}}/${FILESNAME_${F}}: ${F}
	${INSTALL} ${COPY} -o ${FILESOWN_${F}} -g ${FILESGRP_${F}} \
		-m ${FILESMODE_${F}} ${.ALLSRC} ${.TARGET}
.endfor
.endif

.if defined(SCRIPTS)
.for S in ${SCRIPTS}
.if defined(SCRIPTSNAME)
SCRIPTSNAME_${S} ?= ${SCRIPTSNAME}
.else
SCRIPTSNAME_${S} ?= ${S:T:R}
.endif
proginstall:: ${DESTDIR}${BINDIR}/${SCRIPTSNAME_${S}}
.if !defined(UPDATE)
.PHONY: ${DESTDIR}${BINDIR}/${SCRIPTSNAME_${S}}
.endif
.if !defined(BUILD)
${DESTDIR}${BINDIR}/${SCRIPTSNAME_${S}}: .MADE
.endif

${DESTDIR}${BINDIR}/${SCRIPTSNAME_${S}}: ${S}
	${INSTALL} ${COPY} -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    ${.ALLSRC} ${.TARGET}
.endfor
.endif

.if target(proginstall)
realinstall: proginstall
.else
realinstall:
.endif
.endif

install: ${MANINSTALL} _SUBDIRUSE linksinstall

${MANINSTALL}: afterinstall
afterinstall: realinstall
realinstall: beforeinstall
.endif

.if !target(lint)
lint: ${LOBJS}
.if defined(LOBJS) && !empty(LOBJS)
	@${LINT} ${LINTFLAGS} ${LDFLAGS:M-L*} ${LOBJS} ${LDADD}
.endif
.endif

.if !defined(NOMAN)
.include <bsd.man.mk>
.endif

.if !defined(NONLS)
.include <bsd.nls.mk>
.endif

.include <bsd.obj.mk>
.include <bsd.links.mk>
.include <bsd.inc.mk>
.include <bsd.dep.mk>
.include <bsd.subdir.mk>
.include <bsd.sys.mk>
