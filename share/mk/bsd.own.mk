#	$NetBSD: bsd.own.mk,v 1.310 2002/09/17 23:18:30 thorpej Exp $

.if !defined(_BSD_OWN_MK_)
_BSD_OWN_MK_=1

MAKECONF?=	/etc/mk.conf
.-include "${MAKECONF}"

# CPU model, derived from MACHINE_ARCH
MACHINE_CPU=	${MACHINE_ARCH:C/mipse[bl]/mips/:C/sh3e[bl]/sh3/:C/sh5e[bl]/sh5/:S/m68000/m68k/:S/armeb/arm/}

# NEED_OWN_INSTALL_TARGET is set to "no" by pkgsrc/mk/bsd.pkg.mk to
# ensure that things defined by <bsd.own.mk> (default targets,
# INSTALL_FILE, etc.) are not conflicting with bsd.pkg.mk.
NEED_OWN_INSTALL_TARGET?=	yes

# This lists the platforms which do not have working in-tree toolchains.
.if ${MACHINE_CPU} == "hppa" || \
    ${MACHINE_CPU} == "ns32k" || \
    ${MACHINE_CPU} == "sh5" || \
    ${MACHINE_CPU} == "x86_64"
TOOLCHAIN_MISSING?=	yes
.else
TOOLCHAIN_MISSING=	no
.endif

# XXX TEMPORARY: If ns32k and not using an external toolchain, then we have
# to use -idirafter rather than -isystem, because the compiler is too old
# to use -isystem.
.if ${MACHINE_CPU} == "ns32k" && !defined(EXTERNAL_TOOLCHAIN)
CPPFLAG_ISYSTEM=	-idirafter
.else
CPPFLAG_ISYSTEM=	-isystem
.endif

.if empty(.MAKEFLAGS:M-V*)
PRINTOBJDIR=	${MAKE} -V .OBJDIR
.else
PRINTOBJDIR=	echo # prevent infinite recursion
.endif

# Determine if running in the NetBSD source tree by checking for the
# existence of build.sh and tools/ in the current or a parent directory,
# and setting _SRC_TOP_ to the result.
#
.if !defined(_SRC_TOP_)			# {
_SRC_TOP_!= cd ${.CURDIR}; while :; do \
		here=`pwd`; \
		[ -f build.sh  ] && [ -d tools ] && { echo $$here; break; }; \
		case $$here in /) echo ""; break;; esac; \
		cd ..; done 

.MAKEOVERRIDES+=	_SRC_TOP_

.endif					# }


# If _SRC_TOP != "", we're within the NetBSD source tree, so set
# defaults for NETBSDSRCDIR and _SRC_TOP_OBJ_.
#
.if (${_SRC_TOP_} != "")		# {

NETBSDSRCDIR?=	${_SRC_TOP_}

.if !defined(_SRC_TOP_OBJ_)
_SRC_TOP_OBJ_!=		cd ${_SRC_TOP_} && ${PRINTOBJDIR}
.MAKEOVERRIDES+=	_SRC_TOP_OBJ_
.endif

.endif	# _SRC_TOP != ""		# }


.if (${_SRC_TOP_} != "") && \
    (${TOOLCHAIN_MISSING} != "yes" || defined(EXTERNAL_TOOLCHAIN))
USETOOLS?=	yes
.endif
USETOOLS?=	no


.if ${MACHINE_ARCH} == "mips" || ${MACHINE_ARCH} == "sh3" || \
    ${MACHINE_ARCH} == "sh5"
.BEGIN:
	@echo "Must set MACHINE_ARCH to one of ${MACHINE_ARCH}eb or ${MACHINE_ARCH}el"
	@false
.elif defined(REQUIRETOOLS) && \
      (${TOOLCHAIN_MISSING} != "yes" || defined(EXTERNAL_TOOLCHAIN)) && \
      ${USETOOLS} == "no"
.BEGIN:
	@echo "USETOOLS=no, but this component requires a version-specific host toolchain"
	@false
.endif

# Host platform information; may be overridden
.if !defined(HOST_OSTYPE)
_HOST_OSNAME!=	uname -s
_HOST_OSREL!=	uname -r
_HOST_ARCH!=	uname -p 2>/dev/null || uname -m
HOST_OSTYPE:=	${_HOST_OSNAME}-${_HOST_OSREL:C/\([^\)]*\)//}-${_HOST_ARCH}
.MAKEOVERRIDES+= HOST_OSTYPE
.endif

.if ${USETOOLS} == "yes"						# {

# Provide a default for TOOLDIR.
.if !defined(TOOLDIR)
_TOOLOBJ!=	cd ${NETBSDSRCDIR}/tools && ${PRINTOBJDIR}
TOOLDIR:=	${_TOOLOBJ}/tools.${HOST_OSTYPE}
.MAKEOVERRIDES+= TOOLDIR
.endif

# Define default locations for common tools.
.if ${USETOOLS_BINUTILS:Uyes} == "yes"
AR=		${TOOLDIR}/bin/${MACHINE_GNU_PLATFORM}-ar
AS=		${TOOLDIR}/bin/${MACHINE_GNU_PLATFORM}-as
LD=		${TOOLDIR}/bin/${MACHINE_GNU_PLATFORM}-ld
NM=		${TOOLDIR}/bin/${MACHINE_GNU_PLATFORM}-nm
OBJCOPY=	${TOOLDIR}/bin/${MACHINE_GNU_PLATFORM}-objcopy
OBJDUMP=	${TOOLDIR}/bin/${MACHINE_GNU_PLATFORM}-objdump
RANLIB=		${TOOLDIR}/bin/${MACHINE_GNU_PLATFORM}-ranlib
SIZE=		${TOOLDIR}/bin/${MACHINE_GNU_PLATFORM}-size
STRIP=		${TOOLDIR}/bin/${MACHINE_GNU_PLATFORM}-strip
.endif

.if ${USETOOLS_GCC:Uyes} == "yes"
CC=		${TOOLDIR}/bin/${MACHINE_GNU_PLATFORM}-gcc
CPP=		${TOOLDIR}/bin/${MACHINE_GNU_PLATFORM}-cpp
CXX=		${TOOLDIR}/bin/${MACHINE_GNU_PLATFORM}-c++
FC=		${TOOLDIR}/bin/${MACHINE_GNU_PLATFORM}-g77
OBJC=		${TOOLDIR}/bin/${MACHINE_GNU_PLATFORM}-gcc
.endif

ASN1_COMPILE=	${TOOLDIR}/bin/nbasn1_compile
CAP_MKDB=	${TOOLDIR}/bin/nbcap_mkdb
CAT=		${TOOLDIR}/bin/nbcat
CKSUM=		${TOOLDIR}/bin/nbcksum
COMPILE_ET=	${TOOLDIR}/bin/nbcompile_et
CONFIG=		${TOOLDIR}/bin/nbconfig
CRUNCHGEN=	MAKE=${.MAKE:Q} ${TOOLDIR}/bin/nbcrunchgen
CTAGS=		${TOOLDIR}/bin/nbctags
DBSYM=		${TOOLDIR}/bin/${MACHINE_GNU_PLATFORM}-dbsym
ELF2ECOFF=	${TOOLDIR}/bin/nbmips-elf2ecoff
EQN=		${TOOLDIR}/bin/nbeqn
FGEN=		${TOOLDIR}/bin/nbfgen
GENCAT=		${TOOLDIR}/bin/nbgencat
#GRIND=		${TOOLDIR}/bin/nbvgrind -f
GROFF=		PATH=${TOOLDIR}/lib/groff:$${PATH} ${TOOLDIR}/bin/nbgroff
HOST_MKDEP=	${TOOLDIR}/bin/nbhost-mkdep
INDXBIB=	${TOOLDIR}/bin/nbindxbib
INSTALL=	STRIP=${STRIP:Q} ${TOOLDIR}/bin/nbinstall
INSTALLBOOT=	${TOOLDIR}/bin/nbinstallboot
INSTALL_INFO=	${TOOLDIR}/bin/nbinstall-info
LEX=		${TOOLDIR}/bin/nblex
LINT=		CC=${CC:Q} ${TOOLDIR}/bin/${MACHINE_GNU_PLATFORM}-lint
LORDER=		NM=${NM:Q} MKTEMP=${MKTEMP:Q} ${TOOLDIR}/bin/nblorder
M4=		${TOOLDIR}/bin/nbm4
MAKEFS=		${TOOLDIR}/bin/nbmakefs
MAKEINFO=	${TOOLDIR}/bin/nbmakeinfo
MAKEWHATIS=	${TOOLDIR}/bin/nbmakewhatis
MDSETIMAGE=	${TOOLDIR}/bin/${MACHINE_GNU_PLATFORM}-mdsetimage
MENUC=		MENUDEF=${TOOLDIR}/share/misc ${TOOLDIR}/bin/nbmenuc
MKDEP=		CC=${CC:Q} ${TOOLDIR}/bin/nbmkdep
MKLOCALE=	${TOOLDIR}/bin/nbmklocale
MKTEMP=		${TOOLDIR}/bin/nbmktemp
MSGC=		MSGDEF=${TOOLDIR}/share/misc ${TOOLDIR}/bin/nbmsgc
MTREE=		${TOOLDIR}/bin/nbmtree
PAX=		${TOOLDIR}/bin/nbpax
PIC=		${TOOLDIR}/bin/nbpic
PREPMKBOOTIMAGE=${TOOLDIR}/bin/nbprep-mkbootimage
PWD_MKDB=	${TOOLDIR}/bin/nbpwd_mkdb
REFER=		${TOOLDIR}/bin/nbrefer
RPCGEN=		CPP=${CPP:Q} ${TOOLDIR}/bin/nbrpcgen
SOELIM=		${TOOLDIR}/bin/nbsoelim
SUNLABEL=	${TOOLDIR}/bin/nbsunlabel
TBL=		${TOOLDIR}/bin/nbtbl
TSORT=		${TOOLDIR}/bin/nbtsort -q
UUDECODE=	${TOOLDIR}/bin/nbuudecode
YACC=		${TOOLDIR}/bin/nbyacc
ZIC=		${TOOLDIR}/bin/nbzic

.endif	# USETOOLS == yes						# }

# Targets to check if DESTDIR or RELEASEDIR is provided
#
.if !target(check_DESTDIR)
check_DESTDIR: .PHONY .NOTMAIN
.if !defined(DESTDIR)
	@echo "setenv DESTDIR before doing that!"
	@false
.else
	@true
.endif
.endif

.if !target(check_RELEASEDIR)
check_RELEASEDIR: .PHONY .NOTMAIN
.if !defined(RELEASEDIR)
	@echo "setenv RELEASEDIR before doing that!"
	@false
.else
	@true
.endif
.endif


.if ${USETOOLS} == "yes"
# Make sure DESTDIR is set, so that builds with these tools always
# get appropriate -nostdinc, -nostdlib, etc. handling.  The default is
# <empty string>, meaning start from /, the root directory.
DESTDIR?=
.endif

# Build a dynamically linked /bin and /sbin, with the necessary shared
# libraries moved from /usr/lib to /lib and the shared linker moved
# from /usr/libexec to /lib
#
MKDYNAMICROOT?=	no

# Where the system object and source trees are kept; can be configurable
# by the user in case they want them in ~/foosrc and ~/fooobj (for example).
#
BSDSRCDIR?=	/usr/src
BSDOBJDIR?=	/usr/obj
NETBSDSRCDIR?=	${BSDSRCDIR}

BINGRP?=	wheel
BINOWN?=	root
BINMODE?=	555
NONBINMODE?=	444

# Define MANZ to have the man pages compressed (gzip)
#MANZ=		1

MANDIR?=	/usr/share/man
MANGRP?=	wheel
MANOWN?=	root
MANMODE?=	${NONBINMODE}
MANINSTALL?=	maninstall catinstall

INFODIR?=	/usr/share/info
INFOGRP?=	wheel
INFOOWN?=	root
INFOMODE?=	${NONBINMODE}

LIBDIR?=	/usr/lib
.if ${MKDYNAMICROOT} == "no"
SHLIBDIR?=	/usr/lib
.else
SHLIBDIR?=	/lib
.endif
.if ${USE_SHLIBDIR:Uno} == "yes"
_LIBSODIR?=	${SHLIBDIR}
.else
_LIBSODIR?=	${LIBDIR}
.endif
.if ${MKDYNAMICROOT} == "no"
SHLINKDIR?=	/usr/libexec
.else
SHLINKDIR?=	/libexec
.endif
LINTLIBDIR?=	/usr/libdata/lint
LIBGRP?=	${BINGRP}
LIBOWN?=	${BINOWN}
LIBMODE?=	${NONBINMODE}

DOCDIR?=	/usr/share/doc
HTMLDOCDIR?=	/usr/share/doc/html
DOCGRP?=	wheel
DOCOWN?=	root
DOCMODE?=	${NONBINMODE}

NLSDIR?=	/usr/share/nls
NLSGRP?=	wheel
NLSOWN?=	root
NLSMODE?=	${NONBINMODE}

KMODDIR?=	/usr/lkm
KMODGRP?=	wheel
KMODOWN?=	root
KMODMODE?=	${NONBINMODE}

LOCALEDIR?=	/usr/share/locale
LOCALEGRP?=	wheel
LOCALEOWN?=	root
LOCALEMODE?=	${NONBINMODE}

COPY?=		-c
PRESERVE?=	${UPDATE:D-p}
RENAME?=	-r
HRDLINK?=	-l h
SYMLINK?=	-l s

METALOG?=	${DESTDIR}/METALOG
INSTPRIV?=	${UNPRIVED:D-U -M ${METALOG}}
STRIPFLAG?=	-s

.if ${NEED_OWN_INSTALL_TARGET} == "yes"
INSTALL_DIR?=		${INSTALL} ${INSTPRIV} -d
INSTALL_FILE?=		${INSTALL} ${INSTPRIV} ${COPY} ${PRESERVE} ${RENAME}
INSTALL_LINK?=		${INSTALL} ${INSTPRIV} ${HRDLINK} ${RENAME}
INSTALL_SYMLINK?=	${INSTALL} ${INSTPRIV} ${SYMLINK} ${RENAME}
HOST_INSTALL_FILE?=	${INSTALL} ${COPY} ${PRESERVE} ${RENAME}
.endif

# Define SYS_INCLUDE to indicate whether you want symbolic links to the system
# source (``symlinks''), or a separate copy (``copies''); (latter useful
# in environments where it's not possible to keep /sys publicly readable)
#SYS_INCLUDE= 	symlinks

# Data-driven table using make variables to control how 
# toolchain-dependent targets and shared libraries are built
# for different platforms and object formats.
#
# OBJECT_FMT:		currently either "ELF" or "a.out".
#
# All platforms are ELF, except for ns32k (which does not yet have
# an ELF BFD back-end).
.if ${MACHINE_ARCH} == "ns32k"
OBJECT_FMT?=	a.out		# allow overrides, to ease transition
.else
OBJECT_FMT=	ELF
.endif

# If this platform's toolchain is missing, we obviously cannot build it.
.if ${TOOLCHAIN_MISSING} == "yes"
MKBFD:= no
MKGDB:= no
MKGCC:= no
.endif

# If we are using an external toolchain, we can still build the target's
# BFD stuff, but we cannot build GCC's support libraries, since those are
# tightly-coupled to the version of GCC being used.
.if defined(EXTERNAL_TOOLCHAIN)
MKGCC:= no
.endif

# The sh3 port is incomplete.
.if ${MACHINE_CPU} == "sh3" && !defined(HAVE_GCC3)
NOPIC=		# defined
.endif

# The sh5 port is incomplete.
.if ${MACHINE_CPU} == "sh5" && !defined(HAVE_GCC3)
NOPIC=		# defined
NOPROFILE=	# defined
NOLINT=		# defined
NOGCCERROR=	# defined - The SuperH Gnu C compiler is too pedantic in places
.endif

# The m68000 port is incomplete.
.if ${MACHINE_ARCH} == "m68000"
NOPIC=		# defined
.endif

# The hppa port is incomplete.
.if ${MACHINE_ARCH} == "hppa"
NOLINT=		# defined
NOPROFILE=	# defined
.endif

# If the ns32k port is using an external toolchain, shared libraries
# are not yet supported.
.if ${MACHINE_ARCH} == "ns32k" && defined(EXTERNAL_TOOLCHAIN)
NOPIC=		# defined
.endif

# Location of the file that contains the major and minor numbers of the
# version of a shared library.  If this file exists a shared library
# will be built by <bsd.lib.mk>.
SHLIB_VERSION_FILE?= ${.CURDIR}/shlib_version

# GNU sources and packages sometimes see architecture names differently.
GNU_ARCH.m68000=m68010
GNU_ARCH.sh3eb=sh
GNU_ARCH.sh3el=shle
GNU_ARCH.sh5eb=sh64
GNU_ARCH.sh5el=sh64le
MACHINE_GNU_ARCH=${GNU_ARCH.${MACHINE_ARCH}:U${MACHINE_ARCH}}

# In order to identify NetBSD to GNU packages, we sometimes need
# an "elf" tag for historically a.out platforms.
.if ${OBJECT_FMT} == "ELF" && \
    (${MACHINE_GNU_ARCH} == "arm" || \
     ${MACHINE_GNU_ARCH} == "armeb" || \
     ${MACHINE_ARCH} == "ns32k" || \
     ${MACHINE_ARCH} == "i386" || \
     ${MACHINE_ARCH} == "m68k" || \
     ${MACHINE_ARCH} == "m68000" || \
     ${MACHINE_GNU_ARCH} == "sh" || \
     ${MACHINE_GNU_ARCH} == "shle" || \
     ${MACHINE_GNU_ARCH} == "sh64" || \
     ${MACHINE_GNU_ARCH} == "sh64le" || \
     ${MACHINE_ARCH} == "sparc" || \
     ${MACHINE_ARCH} == "vax")
MACHINE_GNU_PLATFORM=${MACHINE_GNU_ARCH}--netbsdelf
.else
MACHINE_GNU_PLATFORM=${MACHINE_GNU_ARCH}--netbsd
.endif

TARGETS+=	all clean cleandir depend dependall includes \
		install lint obj regress tags html installhtml cleanhtml
.PHONY:		all clean cleandir depend dependall distclean includes \
		install lint obj regress tags beforedepend afterdepend \
		beforeinstall afterinstall realinstall realdepend realall \
		html installhtml cleanhtml subdir-all subdir-install subdir-depend

.if ${NEED_OWN_INSTALL_TARGET} == "yes"
.if !target(install)
install:	.NOTMAIN beforeinstall subdir-install realinstall afterinstall
beforeinstall:	.NOTMAIN
subdir-install:	.NOTMAIN beforeinstall
realinstall:	.NOTMAIN beforeinstall
afterinstall:	.NOTMAIN subdir-install realinstall
.endif
all:		.NOTMAIN realall subdir-all
subdir-all:	.NOTMAIN
realall:	.NOTMAIN
depend:		.NOTMAIN realdepend subdir-depend
subdir-depend:	.NOTMAIN
realdepend:	.NOTMAIN
distclean:	.NOTMAIN cleandir
cleandir:	.NOTMAIN clean

dependall:	.NOTMAIN realdepend .MAKE
	@cd ${.CURDIR}; ${MAKE} realall
.endif

# Define MKxxx variables (which are either yes or no) for users
# to set in /etc/mk.conf and override on the make commandline.
# These should be tested with `== "no"' or `!= "no"'.
# The NOxxx variables should only be used by Makefiles.
#

# Supported NO* options (if defined, MK* will be forced to "no",
# regardless of user's mk.conf setting).
.for var in CRYPTO DOC LINKLIB LINT MAN NLS OBJ PIC PICINSTALL PROFILE SHARE
.if defined(NO${var})
MK${var}:=	no
.endif
.endfor

.if defined(NOMAN)
NOHTML=
.endif

# MK* options which default to "yes".
.for var in BFD CATPAGES CRYPTO DOC GCC GDB HESIOD IEEEFP INFO KERBEROS \
	LINKLIB LINT MAN NLS OBJ PIC PICINSTALL PROFILE SHARE SKEY YP
MK${var}?=	yes
.endfor

# MK* options which default to "no".
.for var in CRYPTO_IDEA CRYPTO_RC5 OBJDIRS SOFTFLOAT
MK${var}?=	no
.endfor

# Force some options off if their dependencies are off.
.if ${MKCRYPTO} == "no"
MKKERBEROS:=	no
.endif

.if ${MKLINKLIB} == "no"
MKPICINSTALL:=	no
MKPROFILE:=	no
.endif

.if ${MKMAN} == "no"
MKCATPAGES:=	no
.endif

.if ${MKOBJ} == "no"
MKOBJDIRS:=	no
.endif

.if ${MKSHARE} == "no"
MKCATPAGES:=	no
MKDOC:=		no
MKINFO:=	no
MKMAN:=		no
MKNLS:=		no
.endif

# Set defaults for the USE_xxx variables.  They all default to "yes"
# unless the corresponding MKxxx variable is set to "no".
.for var in HESIOD KERBEROS SKEY YP
.if (${MK${var}} == "no")
USE_${var}:= no
.else
USE_${var}?= yes
.endif
.endfor

# Use XFree86 4.x as default version on i386, x86_64, macppc and cats.
.if ${MACHINE_ARCH} == "i386" || ${MACHINE_ARCH} == "x86_64" || \
    ${MACHINE} == "macppc" || ${MACHINE} == "cats"
USE_XF86_4?=	yes
.endif

.endif		# _BSD_OWN_MK_
