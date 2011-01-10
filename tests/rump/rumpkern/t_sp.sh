#	$NetBSD: t_sp.sh,v 1.6 2011/01/10 19:51:37 pooka Exp $
#
# Copyright (c) 2010 The NetBSD Foundation, Inc.
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

test_case()
{
	local name="${1}"; shift
	local check_function="${1}"; shift

	atf_test_case "${name}" cleanup
	eval "${name}_head() {  }"
	eval "${name}_body() { \
		${check_function} " "${@}" "; \
	}"
        eval "${name}_cleanup() { \
		RUMP_SERVER=unix://commsock rump.halt
        }"
}

test_case basic basic
test_case stress_short stress 1
test_case stress_long stress 10
test_case fork_simple fork simple
test_case fork_pipecomm fork pipecomm
test_case fork_fakeauth fork fakeauth
test_case sigsafe sigsafe sigsafe

basic()
{
	export RUMP_SERVER=unix://commsock
	atf_check -s exit:0 rump_server ${RUMP_SERVER}
	atf_check -s exit:0 $(atf_get_srcdir)/h_client/h_simplecli
}

stress()
{

	export RUMP_SERVER=unix://commsock
	atf_check -s exit:0 rump_server ${RUMP_SERVER}
	atf_check -s exit:0 -e ignore $(atf_get_srcdir)/h_client/h_stresscli $1
}

fork()
{

	export RUMP_SERVER=unix://commsock
	atf_check -s exit:0 rump_server -lrumpvfs ${RUMP_SERVER}
	atf_check -s exit:0 $(atf_get_srcdir)/h_client/h_forkcli ${1}
}

sigsafe()
{

	export RUMP_SERVER=unix://commsock
	atf_check -s exit:0 rump_server ${RUMP_SERVER}
	atf_check -s exit:0 $(atf_get_srcdir)/h_client/h_sigcli
}

atf_init_test_cases()
{

	atf_add_test_case basic
	atf_add_test_case stress_short
	atf_add_test_case stress_long
	atf_add_test_case fork_simple
	atf_add_test_case fork_pipecomm
	atf_add_test_case fork_fakeauth
	atf_add_test_case sigsafe
}
