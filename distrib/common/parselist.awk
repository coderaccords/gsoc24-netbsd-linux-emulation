#	$NetBSD: parselist.awk,v 1.6 2002/03/07 01:36:34 lukem Exp $
#
# Copyright (c) 2002 The NetBSD Foundation, Inc.
# All rights reserved.
#
# This code is derived from software contributed to The NetBSD Foundation
# by Luke Mewburn of Wasabi Systems.
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

#
# awk -f parselist.awk -v mode=MODE [var=val ...] file1 [...]
#
#	Parse list files file1 [...], generating different output,
#	depending upon the setting of MODE:
#	    crunch	crunchgen(1) config
#	    mtree	mtree(8) specfile
#	    populate	sh(1) commands to populate ${TARGDIR} from ${CURDIR}
#
# 	Each line of the input is either a comment (starts with `#'),
#	or contains one of the following keywords and arguments.
#	In general, keywords in lowercase are crunchgen(1) keywords which
#	might be also supported for the other operations.
#
#	Before each line is parsed, the following strings are replaced with
#	the appropriate value which is passed in on the command line:
#
#	string		value
#	------		-----
#	@MACHINE_ARCH@	MACHINE_ARCH
#	@MACHINE@	MACHINE
#
#	mode key	operation
#	--------	---------
#	C		crunch
#	M		mtree
#	P		populate
#
#	mode	keyword arg1 [...]	description
#	----	------------------	-----------
#
#	C	ARGVLN	prog link	as per crunchgen(1) `ln'
#
#	P	CMD	arg1 [...]	run CMD as a shell command
#
#	M P	COPY	src dest [mode]	copy src to dest
#
#	M P	COPYDIR	src dest	recursively copy files under src to
#					dest.  for M, the directories in src
#					are listed first.
#
#	C	LIBS	libspec ...	as per crunchgen(1) `libs'
#
#	M P	LINK	src d1 [d2 ...]	hard link src to d1, d2, ...
#
#	M	MTREE	arg1 [...]	output arguments `as-is' to specfile
#
#	C M P	PROG	prog [links...]	program(s) to crunch/mtree/populate.
#					for M and P, the first prog listed
#					is copied from ${OBJDIR}/${CRUNCHBIN}
#					and then used as the name to link
#					all other PROG entries to.
#
#	C	SPECIAL	prog cmd ...	as per crunchgen(1) `special'
#
#	C	SRCDIRS	dirname ...	as per crunchgen(1) `srcdirs'
#
#	M P	SYMLINK src dest [...]	symlink src to dest, [...]
#

BEGIN \
{
	errexit = 0;
	crunchprog = "";

	if (mode != "crunch" && mode != "mtree" && mode != "populate")
		err("Unknown parselist mode '" mode "'");
	print "#";
	print "# This file is automatically generated by";
	print "#\tparselist mode=" mode;
	print "#";
	print "";
	if (mode == "populate") {
		print "checkvarisset()";
		print "{";
		print "	eval _v=\\$${1}";
		print "	if [ -z \"$_v\" ]; then";
		print "		echo 1>&2 \"Error: $1 is not defined\"";
		print "		exit 1";
		print "	fi";
		print "}";
		print;
		print "checkvarisset CURDIR";
		print "checkvarisset TARGDIR";
		print "checkvarisset OBJDIR";
		print "checkvarisset CRUNCHBIN";
		print "cd ${CURDIR}";
		print;
	} else if (mode == "mtree") {
		print "/unset\tall";
		print "/set\ttype=file uname=root gname=wheel";
		print;
	}
}

/^$/ || /^#/ \
{
	print;
	next;
}

/@MACHINE(_ARCH)?@/ \
{
	gsub(/@MACHINE_ARCH@/, MACHINE_ARCH);
	gsub(/@MACHINE@/, MACHINE);
}

$1 == "COPY" \
{
	if (NF < 3 || NF > 4)
		err("Usage: COPY src dest [mode]");
	if (mode == "populate" || mode == "mtree")
		copy($2, $3, $4);
	next;
}

$1 == "COPYDIR" \
{
	if (NF != 3)
		err("Usage: COPYDIR src dest");
	srcdir=$2;
	destdir=$3;
	if (mode == "mtree") {
		printf("./%s type=dir mode=755\n", destdir);
		command="cd " srcdir " ; find . -type d -print"
		while (command | getline dir) {
			gsub(/^\.\//, "", dir);
			if (dir == ".")
				continue;
			printf("./%s/%s type=dir mode=755\n", destdir, dir);
		}
		close(command);
	}
	if (mode == "populate" || mode == "mtree") {
		command="cd " srcdir " ; find . -type f -print"
		while (command | getline srcfile) {
			gsub(/^\.\//, "", srcfile);
			copy(srcdir "/" srcfile, destdir "/" srcfile, "");
		}
		close(command);
	}
	next;
}

$1 == "LIBS" || $1 == "SPECIAL" || $1 == "SRCDIRS" \
{
	if (NF < 2)
		err("Usage: " $1 " args...");
	if (mode == "crunch") {
		$1 = tolower($1);
		print;
	}
	next;
}

$1 == "PROG" \
{
	if (NF < 2)
		err("Usage: PROG prog [link ...]");
	if (mode == "crunch") {
		prog = basename($2);
		print "progs " prog;
		for (i = 3; i <= NF; i++)
			print "ln " prog " " basename($i);
	} else {
		for (i = 2; i <= NF; i++) {
			if (crunchprog == "") {
				crunchprog = $i;
				copy("${OBJDIR}/${CRUNCHBIN}", crunchprog);
				continue;
			}
			link(crunchprog, $i);
		}
	}
	next;
}

$1 == "ARGVLN" \
{
	if (NF != 3)
		err("Usage: ARGVLN prog link");
	if (mode == "crunch") {
		$1 = "ln";
		print;
	}
	next;
}

$1 == "LINK" \
{
	if (NF < 3)
		err("Usage: LINK prog link [...]");
	if (mode == "populate" || mode == "mtree") {
		for (i = 3; i <= NF; i++)
			link($2, $i);
	}
	next;
}

$1 == "SYMLINK" \
{
	if (NF < 3)
		err("Usage: SYMLINK prog link [...]");
	if (mode == "populate" || mode == "mtree") {
		for (i = 3; i <= NF; i++)
			symlink($2, $i);
	}
	next;
}

$1 == "CMD" \
{
	if (NF < 2)
		err("Usage: CMD ...");
	if (mode == "populate") {
		printf("(cd ${TARGDIR};");
		for (i = 2; i <= NF; i++)
			printf(" %s", $i);
		print ")";
	}
	next;
}

$1 == "MTREE" \
{
	if (NF < 2)
		err("Usage: MTREE ...");
	if (mode == "mtree") {
		sub(/^[^ \t]+[ \t]+/, "");	# strip first word ("MTREE")
		print;
	}
	next;
}


{
	err("Unknown keyword '" $1 "'");
}


function basename (file) \
{
	gsub(/[^\/]+\//, "", file);
	return file;
}

function copy (src, dest, perm) \
{
	if (mode == "mtree") {
		printf("./%s%s\n", dest, perm != "" ? " mode=" perm : "");
	} else {
		printf("rm -rf ${TARGDIR}/%s\n", dest);
		printf("cp %s ${TARGDIR}/%s\n", src, dest);
		if (perm != "")
			printf("chmod %s ${TARGDIR}/%s\n", perm, dest);
	}
}

function link (src, dest) \
{
	if (mode == "mtree") {
		printf("./%s\n", dest);
	} else {
		printf("rm -rf ${TARGDIR}/%s\n", dest);
		printf("(cd ${TARGDIR}; ln %s %s)\n", src, dest);
	}
}

function symlink (src, dest) \
{
	if (mode == "mtree") {
		printf("./%s type=link link=%s\n", dest, src);
	} else {
		printf("rm -rf ${TARGDIR}/%s\n", dest);
		printf("(cd ${TARGDIR}; ln -s %s %s)\n", src, dest);
	}
}

function err(msg) \
{
	printf("%s at line %d of input.\n", msg, NR) >"/dev/stderr";
	errexit=1;
	exit 1;
}
