#	$NetBSD: Makefile,v 1.44 1998/01/30 07:10:31 mellon Exp $

.include <bsd.own.mk>			# for configuration variables.

# NOTE THAT etc *DOES NOT* BELONG IN THE LIST BELOW

SUBDIR+= lib include bin libexec sbin usr.bin usr.sbin share

.if exists(games)
SUBDIR+= games
.endif

SUBDIR+= gnu

SUBDIR+= sys

.if exists(domestic) && !defined(EXPORTABLE_SYSTEM)
SUBDIR+= domestic
.endif

.if exists(regress)
.ifmake !(install)
SUBDIR+= regress
.endif

regression-tests:
	@echo Running regression tests...
	@(cd ${.CURDIR}/regress && ${MAKE} regress)
.endif

beforeinstall:
.ifndef DESTDIR
	(cd ${.CURDIR}/etc && ${MAKE} DESTDIR=/ distrib-dirs)
.else
	(cd ${.CURDIR}/etc && ${MAKE} distrib-dirs)
.endif

afterinstall:
.ifndef NOMAN
	(cd ${.CURDIR}/share/man && ${MAKE} makedb)
.endif

build: beforeinstall
	(cd ${.CURDIR}/share/mk && ${MAKE} install)
.if exists(domestic) && !defined (EXPORTABLE_SYSTEM)
	(cd ${.CURDIR}/domestic/usr.bin/compile_et && \
	    ${MAKE} depend && ${MAKE} && \
	    ${MAKE} install)
	(cd ${.CURDIR}/domestic/usr.bin/make_cmds && \
	    ${MAKE} depend && ${MAKE} && \
	    ${MAKE} install)
.endif
	${MAKE} includes
.if !defined(UPDATE)
	${MAKE} cleandir
.endif
	(cd ${.CURDIR}/lib/csu && ${MAKE} depend && ${MAKE} && ${MAKE} install)
	(cd ${.CURDIR}/lib && ${MAKE} depend && ${MAKE} && ${MAKE} install)
	(cd ${.CURDIR}/gnu/lib && ${MAKE} depend && ${MAKE} && ${MAKE} install)
	(cd ${.CURDIR}/usr.bin/lex &&\
	    ${MAKE} depend && ${MAKE} && ${MAKE} install)
	(cd ${.CURDIR}/usr.bin/yacc && \
	    ${MAKE} depend && ${MAKE} && ${MAKE} install)
	(cd ${.CURDIR}/usr.bin/xlint && \
	    ${MAKE} depend && ${MAKE} && ${MAKE} install)
.if exists(domestic) && !defined(EXPORTABLE_SYSTEM)
	(cd ${.CURDIR}/domestic/lib/libkrb && \
	    ${MAKE} depend && ${MAKE} && ${MAKE} install)
	(cd ${.CURDIR}/domestic/lib/ && ${MAKE} depend && ${MAKE} && \
	    ${MAKE} install)
.endif
	${MAKE} depend && ${MAKE} && ${MAKE} install

.include <bsd.subdir.mk>
