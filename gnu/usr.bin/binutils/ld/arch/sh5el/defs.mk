# This file is automatically generated.  DO NOT EDIT!
# Generated from: 	NetBSD: mknative,v 1.12 2003/03/05 06:17:17 mrg Exp 
#
G_DEFS=-DHAVE_CONFIG_H -I. -I${GNUHOSTDIST}/ld -I.
G_EMUL=shlelf32_nbsd
G_EMULATION_OFILES=eshlelf32_nbsd.o eshelf32_nbsd.o eshelf64_nbsd.o eshlelf64_nbsd.o eshelf_nbsd.o eshlelf_nbsd.o
G_INCLUDES=-D_GNU_SOURCE -I. -I${GNUHOSTDIST}/ld -I../bfd -I${GNUHOSTDIST}/ld/../bfd -I${GNUHOSTDIST}/ld/../include -I${GNUHOSTDIST}/ld/../intl -I../intl   -DLOCALEDIR="\"/usr/local/share/locale\""
G_OFILES=ldgram.o ldlex.o lexsup.o ldlang.o mri.o ldctor.o ldmain.o  ldwrite.o ldexp.o  ldemul.o ldver.o ldmisc.o  ldfile.o ldcref.o eshlelf32_nbsd.o eshelf32_nbsd.o eshelf64_nbsd.o eshlelf64_nbsd.o eshelf_nbsd.o eshlelf_nbsd.o 
G_STRINGIFY=astring.sed
G_TEXINFOS=ld.texinfo
G_target_alias=sh64le--netbsd
