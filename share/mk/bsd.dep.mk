#	$NetBSD: bsd.dep.mk,v 1.24 1999/02/03 21:08:05 tv Exp $

.PHONY:		cleandepend
cleandir distclean: cleandepend

MKDEP?=		mkdep

# some of the rules involve .h sources, so remove them from mkdep line
depend: beforedepend
.if defined(SRCS)
depend: .depend
	@true # hack to prevent "make depend" from using implicit rules
.NOPATH: .depend
.depend: ${SRCS} ${DPSRCS}
	@rm -f .depend
	@files="${.ALLSRC:M*.s} ${.ALLSRC:M*.S}"; \
	if [ "$$files" != " " ]; then \
	  echo ${MKDEP} -a ${MKDEPFLAGS} \
	    ${CFLAGS:M-[ID]*} ${CPPFLAGS} ${AINC} $$files; \
	  ${MKDEP} -a ${MKDEPFLAGS} \
	    ${CFLAGS:M-[ID]*} ${CPPFLAGS} ${AINC} $$files; \
	fi
	@files="${.ALLSRC:M*.c}"; \
	if [ "$$files" != "" ]; then \
	  echo ${MKDEP} -a ${MKDEPFLAGS} \
	    ${CFLAGS:M-[ID]*} ${CPPFLAGS} $$files; \
	  ${MKDEP} -a ${MKDEPFLAGS} \
	    ${CFLAGS:M-[ID]*} ${CPPFLAGS} $$files; \
	fi
	@files="${.ALLSRC:M*.m}"; \
	if [ "$$files" != "" ]; then \
	  echo ${MKDEP} -a ${MKDEPFLAGS} \
	    ${OBJCFLAGS:M-[ID]*} ${CPPFLAGS} $$files; \
	  ${MKDEP} -a ${MKDEPFLAGS} \
	    ${OBJCFLAGS:M-[ID]*} ${CPPFLAGS} $$files; \
	fi
	@files="${.ALLSRC:M*.cc} ${.ALLSRC:M*.C} ${.ALLSRC:M*.cxx}"; \
	if [ "$$files" != "  " ]; then \
	  echo ${MKDEP} -a ${MKDEPFLAGS} \
	    ${CXXFLAGS:M-[ID]*} ${CPPFLAGS} $$files; \
	  ${MKDEP} -a ${MKDEPFLAGS} \
	    ${CXXFLAGS:M-[ID]*} ${CPPFLAGS} $$files; \
	fi
cleandepend:
	rm -f .depend ${.CURDIR}/tags
.else
cleandepend:
.endif
depend: afterdepend

beforedepend:
afterdepend:

.if !target(tags)
.if defined(SRCS)
tags: ${SRCS}
	-cd ${.CURDIR}; ctags -f /dev/stdout ${.ALLSRC:N*.h} | \
	    sed "s;\${.CURDIR}/;;" > tags
.else
tags:
.endif
.endif
