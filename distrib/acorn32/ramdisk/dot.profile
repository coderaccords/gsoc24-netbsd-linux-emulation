# $NetBSD: dot.profile,v 1.2 2002/01/30 23:02:22 reinoud Exp $
#
# Copyright (c) 1997 Perry E. Metzger
# Copyright (c) 1994 Christopher G. Demetriou
# All rights reserved.
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
#          This product includes software developed for the
#          NetBSD Project.  See http://www.netbsd.org/ for
#          information about NetBSD.
# 4. The name of the author may not be used to endorse or promote products
#    derived from this software without specific prior written permission.
# 
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
# IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
# OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
# IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
# INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
# NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
# THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
# 
# <<Id: LICENSE,v 1.2 2000/06/14 15:57:33 cgd Exp>>

PATH=/sbin:/bin:/usr/bin:/usr/sbin:/
export PATH
TERM=vt100
export TERM
HOME=/
export HOME

umask 022

ROOTDEV=/dev/md0a

if [ "X${DONEPROFILE}" = "X" ]; then
	DONEPROFILE=YES
	export DONEPROFILE

	# set up some sane defaults
	echo 'erase ^?, werase ^W, kill ^U, intr ^C'
	stty newcrt werase ^W intr ^C kill ^U erase ^? 9600
	echo ''

	# mount the ramdisk read write
	mount -u $ROOTDEV /

	# mount the kern_fs so that we can examine the dmesg state
	mount -t kernfs /kern /kern

	# run the installation or upgrade script.
	sysinst
fi
