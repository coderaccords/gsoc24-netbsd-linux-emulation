#	$NetBSD: bsd.nls.mk,v 1.41 2003/07/18 08:26:08 lukem Exp $

.include <bsd.init.mk>

##### Basic targets
.PHONY:		cleannls nlsinstall
cleandir:	cleannls
realinstall:	nlsinstall

##### Default values
NLSNAME?=	${PROG:Ulib${LIB}}

NLS?=

##### Build rules
.if ${MKNLS} != "no"

NLSALL=		${NLS:.msg=.cat}

realall:	${NLSALL}
.NOPATH:	${NLSALL}

.SUFFIXES: .cat .msg

.msg.cat:
	@rm -f ${.TARGET}
	${TOOL_GENCAT} ${.TARGET} ${.IMPSRC}

.endif # ${MKNLS} != "no"

##### Install rules
nlsinstall::	# ensure existence
.if ${MKNLS} != "no"

__nlsinstall: .USE
	${INSTALL_FILE} -o ${NLSOWN} -g ${NLSGRP} -m ${NLSMODE} \
		${SYSPKGTAG} ${.ALLSRC} ${.TARGET}

.for F in ${NLSALL:O:u}
_F:=		${DESTDIR}${NLSDIR}/${F:T:R}/${NLSNAME}.cat # installed path

.if ${MKUPDATE} == "no"
${_F}!		${F} __nlsinstall			# install rule
.if !defined(BUILD) && !make(all) && !make(${F})
${_F}!		.MADE					# no build at install
.endif
.else
${_F}:		${F} __nlsinstall			# install rule
.if !defined(BUILD) && !make(all) && !make(${F})
${_F}:		.MADE					# no build at install
.endif
.endif

nlsinstall::	${_F}
.PRECIOUS:	${_F}					# keep if install fails
.endfor

.undef _F
.endif # ${MKNLS} != "no"

##### Clean rules
cleannls:
.if ${MKNLS} != "no" && !empty(NLS)
	rm -f ${NLSALL}
.endif

##### Pull in related .mk logic
.include <bsd.obj.mk>
.include <bsd.sys.mk>
