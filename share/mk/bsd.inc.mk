#	$NetBSD: bsd.inc.mk,v 1.24 2003/07/18 08:26:07 lukem Exp $

.include <bsd.init.mk>

##### Basic targets
.PHONY:		incinstall
includes:	${INCS} incinstall

##### Install rules
incinstall::	# ensure existence

# -c is forced on here, in order to preserve modtimes for "make depend"
__incinstall: .USE
	@cmp -s ${.ALLSRC} ${.TARGET} > /dev/null 2>&1 || \
	    (echo "${INSTALL_FILE:N-c} -c -o ${BINOWN} -g ${BINGRP} \
		-m ${NONBINMODE} ${SYSPKGTAG} ${.ALLSRC} ${.TARGET}" && \
	     ${INSTALL_FILE:N-c} -c -o ${BINOWN} -g ${BINGRP} \
		-m ${NONBINMODE} ${SYSPKGTAG} ${.ALLSRC} ${.TARGET})

.for F in ${INCS:O:u}
_FDIR:=		${INCSDIR_${F:C,/,_,g}:U${INCSDIR}}	# dir override
_FNAME:=	${INCSNAME_${F:C,/,_,g}:U${INCSNAME:U${F}}} # name override
_F:=		${DESTDIR}${_FDIR}/${_FNAME}		# installed path

.if ${MKUPDATE} == "no"
${_F}!		${F} __incinstall			# install rule
.else
${_F}:		${F} __incinstall			# install rule
.endif

incinstall::	${_F}
.PRECIOUS:	${_F}					# keep if install fails
.endfor

.undef _FDIR
.undef _FNAME
.undef _F
