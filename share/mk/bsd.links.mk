#	$NetBSD: bsd.links.mk,v 1.18 2002/03/21 12:59:56 pk Exp $

##### Basic targets
.PHONY:		linksinstall
install:	linksinstall
linksinstall::	realinstall

##### Default values
LINKS?=
SYMLINKS?=

##### Install rules
linksinstall::
.if !empty(SYMLINKS)
	@(set ${SYMLINKS}; \
	 while test $$# -ge 2; do \
		l=$$1; shift; \
		t=${DESTDIR}$$1; shift; \
		if [ -h $$t ]; then \
			cur=`ls -ld $$t | awk '{print $$NF}'` ; \
			if [ "$$cur" = "$$l" ]; then \
				continue ; \
			fi; \
		fi; \
		echo "$$t -> $$l"; \
		rm -rf $$t; ${INSTALL_SYMLINK} $$l $$t; \
	 done; )
.endif
.if !empty(LINKS)
	@(set ${LINKS}; \
	 echo ".include <bsd.own.mk>"; \
	 while test $$# -ge 2; do \
		l=${DESTDIR}$$1; shift; \
		t=${DESTDIR}$$1; shift; \
		echo "realall: $$t"; \
		echo "$$t!"; \
		echo "	@echo \"$$t -> $$l\""; \
		echo "	@rm -f $$t; ${INSTALL_LINK} $$l $$t"; \
	 done; \
	) | ${MAKE} -f- all
.endif
