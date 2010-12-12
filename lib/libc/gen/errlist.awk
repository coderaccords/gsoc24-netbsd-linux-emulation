#! /usr/bin/awk -f
#	$NetBSD: errlist.awk,v 1.1 2010/12/12 20:08:27 christos Exp $
#
# Copyright (c) 2010 The NetBSD Foundation, Inc.
# All rights reserved.
#
# This code is derived from software contributed to The NetBSD Foundation
# by Christos Zoulas.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
# 3. All advertising materials mentioning features or use of this software
#    must display the following acknowledgement:
#        This product includes software developed by the NetBSD
#        Foundation, Inc. and its contributors.
# 4. Neither the name of The NetBSD Foundation nor the names of its
#    contributors may be used to endorse or promote products derived
#    from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
# ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
# TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
# PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
# BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
# CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
# SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
# CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
# ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
# POSSIBILITY OF SUCH DAMAGE.
#
function tabs(desc) {
	l = length(desc) + 3;	/* 3 = ["",] */
	if (l < 16)
		return "\t\t\t\t";
	else if (l < 24)
		return "\t\t\t";
	else if (l < 32)
		return "\t\t";
	else if (l < 40)
		return "\t";
	else
		return "";
}
function perror(name, number, desc)
{
	printf("\t\"%s\",%s/* %d - %s */\n", desc, tabs(desc), number, name);
}
BEGIN {
	printf("/* Automatically generated file; do not edit */\n");
	printf("#include <sys/cdefs.h>\n");
	printf("#if defined(LIBC_SCCS) && !defined(lint)\n");
	printf("#if 0\n");
	printf("static char sccsid[] = \"@(#)errlst.c	8.2 (Berkeley) 11/16/93\";\n");
	printf("#else\n");
	printf("__RCSID(\"$NetBSD: errlist.awk,v 1.1 2010/12/12 20:08:27 christos Exp $\");\n");
	printf("#endif\n");
	printf("#endif /* LIBC_SCCS and not lint */\n\n");
	printf("#include <errno.h>\n");
	printf("static const char *const errlist[] = {\n");
	perror("ENOERROR", 0, "Undefined error: 0");
	errno = 1;
}
/^#define/ {
	name = $2;
	if (name == "ELAST")
		next;
	number = $3;
	if (number < 0 || number == "EAGAIN")
		next;
	desc = $0;
	i1 = index(desc, "/*") + 3;
	l = length(desc);
	desc = substr(desc, i1, l - i1 - 2);
	if (number != errno) {
		printf("error number mismatch %d != %d\n", number, errno) > "/dev/stderr";
		exit(1);
	}
	perror(name, number, desc);
	errno++;
}
END {
	printf("};\n\n");
	printf("const int sys_nerr = sizeof(errlist) / sizeof(errlist[0]);\n");
	printf("const char * const *sys_errlist = errlist;\n");
}
