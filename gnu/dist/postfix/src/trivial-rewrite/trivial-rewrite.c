/*	$NetBSD: trivial-rewrite.c,v 1.1.1.8 2004/07/28 22:49:30 heas Exp $	*/

/*++
/* NAME
/*	trivial-rewrite 8
/* SUMMARY
/*	Postfix address rewriting and resolving daemon
/* SYNOPSIS
/*	\fBtrivial-rewrite\fR [generic Postfix daemon options]
/* DESCRIPTION
/*	The \fBtrivial-rewrite\fR daemon processes three types of client
/*	service requests:
/* .IP \fBrewrite\fR
/*	Rewrite an address to standard form. The \fBtrivial-rewrite\fR
/*	daemon by default appends local domain information to unqualified
/*	addresses, swaps bang paths to domain form, and strips source
/*	routing information. This process is under control of several
/*	configuration parameters (see below).
/* .IP \fBresolve\fR
/*	Resolve an address to a (\fItransport\fR, \fInexthop\fR,
/*	\fIrecipient\fR) triple. The meaning of the results is as follows:
/* .RS
/* .IP \fItransport\fR
/*	The delivery agent to use. This is the first field of an entry
/*	in the \fBmaster.cf\fR file.
/* .IP \fInexthop\fR
/*	The host to send to and optional delivery method information.
/* .IP \fIrecipient\fR
/*	The envelope recipient address that is passed on to \fInexthop\fR.
/* .RE
/* .IP \fBverify\fR
/*	Resolve an address for address verification purposes.
/* SERVER PROCESS MANAGEMENT
/* .ad
/* .fi
/*	The trivial-rewrite servers run under control by the Postfix master
/*	server.  Each server can handle multiple simultaneous connections.
/*	When all servers are busy while a client connects, the master
/*	creates a new server process, provided that the trivial-rewrite
/*	server process limit is not exceeded.
/*	Each trivial-rewrite server terminates after
/*	serving at least \fB$max_use\fR clients of after \fB$max_idle\fR
/*	seconds of idle time.
/* STANDARDS
/* .ad
/* .fi
/*	None. The command does not interact with the outside world.
/* SECURITY
/* .ad
/* .fi
/*	The \fBtrivial-rewrite\fR daemon is not security sensitive.
/*	By default, this daemon does not talk to remote or local users.
/*	It can run at a fixed low privilege in a chrooted environment.
/* DIAGNOSTICS
/*	Problems and transactions are logged to \fBsyslogd\fR(8).
/* CONFIGURATION PARAMETERS
/* .ad
/* .fi
/*	On busy mail systems a long time may pass before a \fBmain.cf\fR
/*	change affecting trivial_rewrite(8) is picked up. Use the command
/*	"\fBpostfix reload\fR" to speed up a change.
/*
/*	The text below provides only a parameter summary. See
/*	postconf(5) for more details including examples.
/* COMPATIBILITY CONTROLS
/* .ad
/* .fi
/* .IP "\fBresolve_dequoted_address (yes)\fR"
/*	Resolve a recipient address safely instead of correctly, by
/*	looking inside quotes.
/* .IP "\fBresolve_null_domain (no)\fR"
/*	Resolve an address that ends in the "@" null domain as if the
/*	local hostname were specified, instead of rejecting the address as
/*	invalid.
/* ADDRESS REWRITING CONTROLS
/* .ad
/* .fi
/* .IP "\fBmyorigin ($myhostname)\fR"
/*	The default domain name that locally-posted mail appears to come
/*	from, and that locally posted mail is delivered to.
/* .IP "\fBallow_percent_hack (yes)\fR"
/*	Enable the rewriting of the form "user%domain" to "user@domain".
/* .IP "\fBappend_at_myorigin (yes)\fR"
/*	Append the string "@$myorigin" to mail addresses without domain
/*	information.
/* .IP "\fBappend_dot_mydomain (yes)\fR"
/*	Append the string ".$mydomain" to addresses that have no ".domain"
/*	information.
/* .IP "\fBrecipient_delimiter (empty)\fR"
/*	The separator between user names and address extensions (user+foo).
/* .IP "\fBswap_bangpath (yes)\fR"
/*	Enable the rewriting of "site!user" into "user@site".
/* ROUTING CONTROLS
/* .ad
/* .fi
/*	The following is applicable to Postfix version 2.0 and later.
/*	Earlier versions do not have support for: virtual_transport,
/*	relay_transport, virtual_alias_domains, virtual_mailbox_domains
/*	or proxy_interfaces.
/* .IP "\fBlocal_transport (local:$myhostname)\fR"
/*	The default mail delivery transport for domains that match
/*	$mydestination, $inet_interfaces or $proxy_interfaces.
/* .IP "\fBvirtual_transport (virtual)\fR"
/*	The default mail delivery transport for domains that match the
/*	$virtual_mailbox_domains parameter value.
/* .IP "\fBrelay_transport (relay)\fR"
/*	The default mail delivery transport and next-hop information for
/*	domains that match the $relay_domains parameter value.
/* .IP "\fBdefault_transport (smtp)\fR"
/*	The default mail delivery transport for domains that do not match
/*	$mydestination, $inet_interfaces, $proxy_interfaces,
/*	$virtual_alias_domains, $virtual_mailbox_domains, or $relay_domains.
/* .IP "\fBparent_domain_matches_subdomains (see 'postconf -d' output)\fR"
/*	What Postfix features match subdomains of "domain.tld" automatically,
/*	instead of requiring an explicit ".domain.tld" pattern.
/* .IP "\fBrelayhost (empty)\fR"
/*	The default host to send non-local mail to when no entry is matched
/*	in the optional transport(5) table.
/* .IP "\fBtransport_maps (empty)\fR"
/*	Optional lookup tables with mappings from recipient address to
/*	(message delivery transport, next-hop destination).
/* ADDRESS VERIFICATION CONTROLS
/* .ad
/* .fi
/*	Postfix version 2.1 introduces sender and recipient address verification.
/*	This feature is implemented by sending probe email messages that
/*	are not actually delivered.
/*	By default, address verification probes use the same route
/*	as regular mail. To override specific aspects of message
/*	routing for address verification probes, specify one or more
/*	of the following:
/* .IP "\fBaddress_verify_local_transport ($local_transport)\fR"
/*	Overrides the local_transport parameter setting for address
/*	verification probes.
/* .IP "\fBaddress_verify_virtual_transport ($virtual_transport)\fR"
/*	Overrides the virtual_transport parameter setting for address
/*	verification probes.
/* .IP "\fBaddress_verify_relay_transport ($relay_transport)\fR"
/*	Overrides the relay_transport parameter setting for address
/*	verification probes.
/* .IP "\fBaddress_verify_default_transport ($default_transport)\fR"
/*	Overrides the default_transport parameter setting for address
/*	verification probes.
/* .IP "\fBaddress_verify_relayhost ($relayhost)\fR"
/*	Overrides the relayhost parameter setting for address verification
/*	probes.
/* .IP "\fBaddress_verify_transport_maps ($transport_maps)\fR"
/*	Overrides the transport_maps parameter setting for address verification
/*	probes.
/* MISCELLANEOUS CONTROLS
/* .ad
/* .fi
/* .IP "\fBconfig_directory (see 'postconf -d' output)\fR"
/*	The default location of the Postfix main.cf and master.cf
/*	configuration files.
/* .IP "\fBdaemon_timeout (18000s)\fR"
/*	How much time a Postfix daemon process may take to handle a
/*	request before it is terminated by a built-in watchdog timer.
/* .IP "\fBempty_address_recipient (MAILER-DAEMON)\fR"
/*	The recipient of mail addressed to the null address.
/* .IP "\fBipc_timeout (3600s)\fR"
/*	The time limit for sending or receiving information over an internal
/*	communication channel.
/* .IP "\fBmax_idle (100s)\fR"
/*	The maximum amount of time that an idle Postfix daemon process
/*	waits for the next service request before exiting.
/* .IP "\fBmax_use (100)\fR"
/*	The maximal number of connection requests before a Postfix daemon
/*	process terminates.
/* .IP "\fBrelocated_maps (empty)\fR"
/*	Optional lookup tables with new contact information for users or
/*	domains that no longer exist.
/* .IP "\fBprocess_id (read-only)\fR"
/*	The process ID of a Postfix command or daemon process.
/* .IP "\fBprocess_name (read-only)\fR"
/*	The process name of a Postfix command or daemon process.
/* .IP "\fBqueue_directory (see 'postconf -d' output)\fR"
/*	The location of the Postfix top-level queue directory.
/* .IP "\fBshow_user_unknown_table_name (yes)\fR"
/*	Display the name of the recipient table in the "User unknown"
/*	responses.
/* .IP "\fBsyslog_facility (mail)\fR"
/*	The syslog facility of Postfix logging.
/* .IP "\fBsyslog_name (postfix)\fR"
/*	The mail system name that is prepended to the process name in syslog
/*	records, so that "smtpd" becomes, for example, "postfix/smtpd".
/* .PP
/*	Available in Postfix version 2.0 and later:
/* .IP "\fBhelpful_warnings (yes)\fR"
/*	Log warnings about problematic configuration settings, and provide
/*	helpful suggestions.
/* SEE ALSO
/*	postconf(5), configuration parameters
/*	transport(5), transport table format
/*	relocated(5), format of the "user has moved" table
/*	master(8), process manager
/*	syslogd(8), system logging
/* README FILES
/* .ad
/* .fi
/*	Use "\fBpostconf readme_directory\fR" or
/*	"\fBpostconf html_directory\fR" to locate this information.
/* .na
/* .nf
/*	ADDRESS_CLASS_README, Postfix address classes howto
/*	ADDRESS_VERIFICATION_README, Postfix address verification
/* LICENSE
/* .ad
/* .fi
/*	The Secure Mailer license must be distributed with this software.
/* AUTHOR(S)
/*	Wietse Venema
/*	IBM T.J. Watson Research
/*	P.O. Box 704
/*	Yorktown Heights, NY 10598, USA
/*--*/

/* System library. */

#include <sys_defs.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

/* Utility library. */

#include <msg.h>
#include <vstring.h>
#include <vstream.h>
#include <vstring_vstream.h>
#include <split_at.h>
#include <stringops.h>
#include <dict.h>

/* Global library. */

#include <mail_params.h>
#include <mail_proto.h>
#include <resolve_local.h>
#include <mail_conf.h>
#include <resolve_clnt.h>
#include <rewrite_clnt.h>
#include <tok822.h>
#include <mail_addr.h>

/* Multi server skeleton. */

#include <mail_server.h>

/* Application-specific. */

#include <trivial-rewrite.h>
#include <transport.h>

static VSTRING *command;

 /*
  * Tunable parameters.
  */
char   *var_transport_maps;
bool    var_swap_bangpath;
bool    var_append_dot_mydomain;
bool    var_append_at_myorigin;
bool    var_percent_hack;
char   *var_local_transport;
char   *var_virt_transport;
char   *var_relay_transport;
int     var_resolve_dequoted;
char   *var_virt_alias_maps;		/* XXX virtual_alias_domains */
char   *var_virt_mailbox_maps;		/* XXX virtual_mailbox_domains */
char   *var_virt_alias_doms;
char   *var_virt_mailbox_doms;
char   *var_relocated_maps;
char   *var_def_transport;
char   *var_empty_addr;
int     var_show_unk_rcpt_table;
int     var_resolve_nulldom;

 /*
  * Shadow personality for address verification.
  */
char   *var_vrfy_xport_maps;
char   *var_vrfy_local_xport;
char   *var_vrfy_virt_xport;
char   *var_vrfy_relay_xport;
char   *var_vrfy_def_xport;
char   *var_vrfy_relayhost;

 /*
  * Different resolver personalities depending on the kind of request.
  */
RES_CONTEXT resolve_regular = {
    VAR_LOCAL_TRANSPORT, &var_local_transport,
    VAR_VIRT_TRANSPORT, &var_virt_transport,
    VAR_RELAY_TRANSPORT, &var_relay_transport,
    VAR_DEF_TRANSPORT, &var_def_transport,
    VAR_RELAYHOST, &var_relayhost,
    VAR_TRANSPORT_MAPS, &var_transport_maps, 0
};

RES_CONTEXT resolve_verify = {
    VAR_VRFY_LOCAL_XPORT, &var_vrfy_local_xport,
    VAR_VRFY_VIRT_XPORT, &var_vrfy_virt_xport,
    VAR_VRFY_RELAY_XPORT, &var_vrfy_relay_xport,
    VAR_VRFY_DEF_XPORT, &var_vrfy_def_xport,
    VAR_VRFY_RELAYHOST, &var_vrfy_relayhost,
    VAR_VRFY_XPORT_MAPS, &var_vrfy_xport_maps, 0
};

/* rewrite_service - read request and send reply */

static void rewrite_service(VSTREAM *stream, char *unused_service, char **argv)
{
    int     status = -1;

    /*
     * Sanity check. This service takes no command-line arguments.
     */
    if (argv[0])
	msg_fatal("unexpected command-line argument: %s", argv[0]);

    /*
     * This routine runs whenever a client connects to the UNIX-domain socket
     * dedicated to address rewriting. All connection-management stuff is
     * handled by the common code in multi_server.c.
     */
    if (attr_scan(stream, ATTR_FLAG_STRICT | ATTR_FLAG_MORE,
		  ATTR_TYPE_STR, MAIL_ATTR_REQ, command,
		  ATTR_TYPE_END) == 1) {
	if (strcmp(vstring_str(command), REWRITE_ADDR) == 0) {
	    status = rewrite_proto(stream);
	} else if (strcmp(vstring_str(command), RESOLVE_REGULAR) == 0) {
	    status = resolve_proto(&resolve_regular, stream);
	} else if (strcmp(vstring_str(command), RESOLVE_VERIFY) == 0) {
	    status = resolve_proto(&resolve_verify, stream);
	} else {
	    msg_warn("bad command %.30s", printable(vstring_str(command), '?'));
	}
    }
    if (status < 0)
	multi_server_disconnect(stream);
}

/* pre_accept - see if tables have changed */

static void pre_accept(char *unused_name, char **unused_argv)
{
    const char *table;

    if ((table = dict_changed_name()) != 0) {
	msg_info("table %s has changed -- restarting", table);
	exit(0);
    }
}

/* pre_jail_init - initialize before entering chroot jail */

static void pre_jail_init(char *unused_name, char **unused_argv)
{
    command = vstring_alloc(100);
    rewrite_init();
    resolve_init();
    if (*RES_PARAM_VALUE(resolve_regular.transport_maps))
	resolve_regular.transport_info =
	    transport_pre_init(resolve_regular.transport_maps_name,
			   RES_PARAM_VALUE(resolve_regular.transport_maps));
    if (*RES_PARAM_VALUE(resolve_verify.transport_maps))
	resolve_verify.transport_info =
	    transport_pre_init(resolve_verify.transport_maps_name,
			    RES_PARAM_VALUE(resolve_verify.transport_maps));
}

/* post_jail_init - initialize after entering chroot jail */

static void post_jail_init(char *unused_name, char **unused_argv)
{
    if (resolve_regular.transport_info)
	transport_post_init(resolve_regular.transport_info);
    if (resolve_verify.transport_info)
	transport_post_init(resolve_verify.transport_info);
}

/* main - pass control to the multi-threaded skeleton code */

int     main(int argc, char **argv)
{
    static CONFIG_STR_TABLE str_table[] = {
	VAR_TRANSPORT_MAPS, DEF_TRANSPORT_MAPS, &var_transport_maps, 0, 0,
	VAR_LOCAL_TRANSPORT, DEF_LOCAL_TRANSPORT, &var_local_transport, 1, 0,
	VAR_VIRT_TRANSPORT, DEF_VIRT_TRANSPORT, &var_virt_transport, 1, 0,
	VAR_RELAY_TRANSPORT, DEF_RELAY_TRANSPORT, &var_relay_transport, 1, 0,
	VAR_DEF_TRANSPORT, DEF_DEF_TRANSPORT, &var_def_transport, 1, 0,
	VAR_VIRT_ALIAS_MAPS, DEF_VIRT_ALIAS_MAPS, &var_virt_alias_maps, 0, 0,
	VAR_VIRT_ALIAS_DOMS, DEF_VIRT_ALIAS_DOMS, &var_virt_alias_doms, 0, 0,
	VAR_VIRT_MAILBOX_MAPS, DEF_VIRT_MAILBOX_MAPS, &var_virt_mailbox_maps, 0, 0,
	VAR_VIRT_MAILBOX_DOMS, DEF_VIRT_MAILBOX_DOMS, &var_virt_mailbox_doms, 0, 0,
	VAR_RELOCATED_MAPS, DEF_RELOCATED_MAPS, &var_relocated_maps, 0, 0,
	VAR_EMPTY_ADDR, DEF_EMPTY_ADDR, &var_empty_addr, 1, 0,
	VAR_VRFY_XPORT_MAPS, DEF_VRFY_XPORT_MAPS, &var_vrfy_xport_maps, 0, 0,
	VAR_VRFY_LOCAL_XPORT, DEF_VRFY_LOCAL_XPORT, &var_vrfy_local_xport, 1, 0,
	VAR_VRFY_VIRT_XPORT, DEF_VRFY_VIRT_XPORT, &var_vrfy_virt_xport, 1, 0,
	VAR_VRFY_RELAY_XPORT, DEF_VRFY_RELAY_XPORT, &var_vrfy_relay_xport, 1, 0,
	VAR_VRFY_DEF_XPORT, DEF_VRFY_DEF_XPORT, &var_vrfy_def_xport, 1, 0,
	VAR_VRFY_RELAYHOST, DEF_VRFY_RELAYHOST, &var_vrfy_relayhost, 0, 0,
	0,
    };
    static CONFIG_BOOL_TABLE bool_table[] = {
	VAR_SWAP_BANGPATH, DEF_SWAP_BANGPATH, &var_swap_bangpath,
	VAR_APP_DOT_MYDOMAIN, DEF_APP_DOT_MYDOMAIN, &var_append_dot_mydomain,
	VAR_APP_AT_MYORIGIN, DEF_APP_AT_MYORIGIN, &var_append_at_myorigin,
	VAR_PERCENT_HACK, DEF_PERCENT_HACK, &var_percent_hack,
	VAR_RESOLVE_DEQUOTED, DEF_RESOLVE_DEQUOTED, &var_resolve_dequoted,
	VAR_SHOW_UNK_RCPT_TABLE, DEF_SHOW_UNK_RCPT_TABLE, &var_show_unk_rcpt_table,
	VAR_RESOLVE_NULLDOM, DEF_RESOLVE_NULLDOM, &var_resolve_nulldom,
	0,
    };

    multi_server_main(argc, argv, rewrite_service,
		      MAIL_SERVER_STR_TABLE, str_table,
		      MAIL_SERVER_BOOL_TABLE, bool_table,
		      MAIL_SERVER_PRE_INIT, pre_jail_init,
		      MAIL_SERVER_POST_INIT, post_jail_init,
		      MAIL_SERVER_PRE_ACCEPT, pre_accept,
		      0);
}
