/*	$NetBSD: iplang_y.y,v 1.2 1997/09/21 18:02:08 veego Exp $	*/

%{
/*
 * (C)opyright 1997 by Darren Reed.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that this notice is preserved and due credit is given
 * to the original author and the contributors.
 *
 * Id: iplang_y.y,v 2.0.2.9 1997/09/13 07:14:24 darrenr Exp 
 */
 
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#if !defined(__SVR4) && !defined(__svr4__)
#include <strings.h>
#else
#include <sys/byteorder.h>
#endif
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <sys/time.h>
#include <stdlib.h>
#include <unistd.h>
#include <stddef.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <netinet/ip_var.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>
#include <net/if.h>
#include <netinet/if_ether.h>
#include <netdb.h>
#include <arpa/nameser.h>
#include <arpa/inet.h>
#include <resolv.h>
#include <ctype.h>
#include "ipsend.h"
#include <netinet/ip_fil.h>
#include "ipf.h"
#include "iplang.h"

extern	struct ether_addr *ether_aton __P((char *));

extern	int	opts;
extern	struct ipopt_names ionames[];
extern	int	state, state, lineNum, token;
extern	int	yylineno;
extern	char	yytext[];
extern	FILE	*yyin;
int	yylex	__P((void));
/*#define	YYDEBUG 1*/
int	yydebug = 0;

iface_t *iflist = NULL, **iftail = &iflist;
iface_t *cifp = NULL;
arp_t *arplist = NULL, **arptail = &arplist, *carp = NULL;
struct in_addr defrouter;
send_t	sending;
char	*sclass = NULL;
u_short	c_chksum __P((u_short *, u_int, u_long));
u_long	p_chksum __P((u_short *, u_int));

u_long	ipbuffer[67584/sizeof(u_long)];		/* 66K */
aniphdr_t	*aniphead = NULL, *canip = NULL, **aniptail = &aniphead;
ip_t		*ip = NULL;
udphdr_t	*udp = NULL;
tcphdr_t	*tcp = NULL;
icmphdr_t	*icmp = NULL;

struct statetoopt {
	int	sto_st;
	int	sto_op;
};

struct	in_addr getipv4addr __P((char *arg));
u_short	getportnum __P((char *, char *));
struct	ether_addr *geteaddr __P((char *, struct ether_addr *));
void	*new_header __P((int));
void	free_aniplist __P((void));
void	inc_anipheaders __P((int));
void	new_data __P((void));
void	set_datalen __P((char **));
void	set_datafile __P((char **));
void	set_data __P((char **));
void	new_packet __P((void));
void	set_ipv4proto __P((char **));
void	set_ipv4src __P((char **));
void	set_ipv4dst __P((char **));
void	set_ipv4off __P((char **));
void	set_ipv4v __P((char **));
void	set_ipv4hl __P((char **));
void	set_ipv4ttl __P((char **));
void	set_ipv4tos __P((char **));
void	set_ipv4id __P((char **));
void	set_ipv4sum __P((char **));
void	set_ipv4len __P((char **));
void	new_tcpheader __P((void));
void	set_tcpsport __P((char **));
void	set_tcpdport __P((char **));
void	set_tcpseq __P((char **));
void	set_tcpack __P((char **));
void	set_tcpoff __P((char **));
void	set_tcpurp __P((char **));
void	set_tcpwin __P((char **));
void	set_tcpsum __P((char **));
void	set_tcpflags __P((char **));
void	set_tcpopt __P((int, char **));
void	end_tcpopt __P((void));
void	new_udpheader __P((void));
void	set_udplen __P((char **));
void	set_udpsum __P((char **));
void	prep_packet __P((void));
void	packet_done __P((void));
void	new_interface __P((void));
void	check_interface __P((void));
void	set_ifname __P((char **));
void	set_ifmtu __P((int));
void	set_ifv4addr __P((char **));
void	set_ifeaddr __P((char **));
void	new_arp __P((void));
void	set_arpeaddr __P((char **));
void	set_arpv4addr __P((char **));
void	reset_send __P((void));
void	set_sendif __P((char **));
void	set_sendvia __P((char **));
void	set_defaultrouter __P((char **));
void	new_icmpheader __P((void));
void	set_icmpcode __P((int));
void	set_icmptype __P((int));
void	set_icmpcodetok __P((char **));
void	set_icmptypetok __P((char **));
void	set_icmpid __P((int));
void	set_icmpseq __P((int));
void	set_icmpotime __P((int));
void	set_icmprtime __P((int));
void	set_icmpttime __P((int));
void	set_icmpmtu __P((int));
void	set_redir __P((int, char **));
void	new_ipv4opt __P((void));
void	set_icmppprob __P((int));
void	add_ipopt __P((int, void *));
void	end_ipopt __P((void));
void	set_secclass __P((char **));
void	free_anipheader __P((void));
void	end_ipv4 __P((void));
void	end_icmp __P((void));
void	end_udp __P((void));
void	end_tcp __P((void));
void	end_data __P((void));
void	yyerror __P((char *));
void	iplang __P((FILE *));
int	yyparse __P((void));
%}
%union {
	char	*str;
	int	num;
}
%token	<num> IL_NUMBER
%type	<num> number digits
%token	<string> IL_TOKEN
%type	<string> token
%token	IL_HEXDIGIT IL_COLON IL_DOT IL_EOF IL_COMMENT
%token	IL_LBRACE IL_RBRACE IL_SEMICOLON
%token	IL_INTERFACE IL_IFNAME IL_MTU IL_EADDR
%token	IL_IPV4 IL_V4PROTO IL_V4SRC IL_V4DST IL_V4OFF IL_V4V IL_V4HL IL_V4TTL
%token	IL_V4TOS IL_V4SUM IL_V4LEN IL_V4OPT IL_V4ID
%token	IL_TCP IL_SPORT IL_DPORT IL_TCPFL IL_TCPSEQ IL_TCPACK IL_TCPOFF
%token	IL_TCPWIN IL_TCPSUM IL_TCPURP IL_TCPOPT IL_TCPO_NOP IL_TCPO_EOL
%token	IL_TCPO_MSS IL_TCPO_WSCALE IL_TCPO_TS
%token	IL_UDP IL_UDPLEN IL_UDPSUM
%token	IL_ICMP IL_ICMPTYPE IL_ICMPCODE
%token	IL_SEND IL_VIA
%token	IL_ARP
%token	IL_DEFROUTER
%token	IL_SUM IL_OFF IL_LEN IL_V4ADDR IL_OPT
%token	IL_DATA IL_DLEN IL_DVALUE IL_DFILE
%token	IL_IPO_NOP IL_IPO_RR IL_IPO_ZSU IL_IPO_MTUP IL_IPO_MTUR IL_IPO_EOL
%token	IL_IPO_TS IL_IPO_TR IL_IPO_SEC IL_IPO_LSRR IL_IPO_ESEC
%token	IL_IPO_SATID IL_IPO_SSRR IL_IPO_ADDEXT IL_IPO_VISA IL_IPO_IMITD
%token	IL_IPO_EIP IL_IPO_FINN IL_IPO_SECCLASS IL_IPO_CIPSO IL_IPO_ENCODE
%token	IL_IPS_RESERV4 IL_IPS_TOPSECRET IL_IPS_SECRET IL_IPS_RESERV3
%token	IL_IPS_CONFID IL_IPS_UNCLASS IL_IPS_RESERV2 IL_IPS_RESERV1
%token	IL_ICMP_ECHOREPLY IL_ICMP_UNREACH IL_ICMP_UNREACH_NET
%token	IL_ICMP_UNREACH_HOST IL_ICMP_UNREACH_PROTOCOL IL_ICMP_UNREACH_PORT
%token	IL_ICMP_UNREACH_NEEDFRAG IL_ICMP_UNREACH_SRCFAIL
%token	IL_ICMP_UNREACH_NET_UNKNOWN IL_ICMP_UNREACH_HOST_UNKNOWN
%token	IL_ICMP_UNREACH_ISOLATED IL_ICMP_UNREACH_NET_PROHIB
%token	IL_ICMP_UNREACH_HOST_PROHIB IL_ICMP_UNREACH_TOSNET
%token	IL_ICMP_UNREACH_TOSHOST IL_ICMP_UNREACH_FILTER_PROHIB
%token	IL_ICMP_UNREACH_HOST_PRECEDENCE IL_ICMP_UNREACH_PRECEDENCE_CUTOFF
%token	IL_ICMP_SOURCEQUENCH IL_ICMP_REDIRECT IL_ICMP_REDIRECT_NET
%token	IL_ICMP_REDIRECT_HOST IL_ICMP_REDIRECT_TOSNET
%token	IL_ICMP_REDIRECT_TOSHOST IL_ICMP_ECHO IL_ICMP_ROUTERADVERT
%token	IL_ICMP_ROUTERSOLICIT IL_ICMP_TIMXCEED IL_ICMP_TIMXCEED_INTRANS
%token	IL_ICMP_TIMXCEED_REASS IL_ICMP_PARAMPROB IL_ICMP_PARAMPROB_OPTABSENT
%token	IL_ICMP_TSTAMP IL_ICMP_TSTAMPREPLY IL_ICMP_IREQ IL_ICMP_IREQREPLY
%token	IL_ICMP_MASKREQ IL_ICMP_MASKREPLY IL_ICMP_SEQ IL_ICMP_ID
%token	IL_ICMP_OTIME IL_ICMP_RTIME IL_ICMP_TTIME

%%
file:	line
	| line file
	| IL_COMMENT
	| IL_COMMENT file
	;

line:	iface
	| arp
	| send
	| defrouter
	| ipline
	;

iface:  ifhdr IL_LBRACE ifaceopts IL_RBRACE	{ check_interface(); }
	;

ifhdr:	IL_INTERFACE			{ new_interface(); }
	;

ifaceopts:
	ifaceopt
	| ifaceopt ifaceopts
	;

ifaceopt:
	IL_IFNAME token			{ set_ifname(&yylval.str); }
	| IL_MTU number			{ set_ifmtu(yylval.num); }
	| IL_V4ADDR token		{ set_ifv4addr(&yylval.str); }
	| IL_EADDR token		{ set_ifeaddr(&yylval.str); }
	;

send:   sendhdr IL_LBRACE sendbody IL_RBRACE	{ packet_done(); }
	| sendhdr IL_SEMICOLON			{ packet_done(); }
	;

sendhdr:
	IL_SEND				{ reset_send(); }
	;

sendbody:
	sendopt
	| sendbody sendopt
	;

sendopt:
	IL_IFNAME token			{ set_sendif(&yylval.str); }
	| IL_VIA token			{ set_sendvia(&yylval.str); }
	;

arp:    arphdr IL_LBRACE arpbody IL_RBRACE
	;

arphdr:	IL_ARP				{ new_arp(); }
	;

arpbody:
	arpopt
	| arpbody arpopt
	;

arpopt: IL_V4ADDR token			{ set_arpv4addr(&yylval.str); }
	| IL_EADDR token		{ set_arpeaddr(&yylval.str); }
	;

defrouter:
	IL_DEFROUTER token		{ set_defaultrouter(&yylval.str); }
	;

ipline:	ipv4 gotbody
	;

ipv4:	IL_IPV4				{ new_packet(); }

gotbody:	IL_LBRACE ipv4body IL_RBRACE	{ end_ipv4(); }
	;

ipv4body:
	ipv4type IL_SEMICOLON
	| ipv4type IL_SEMICOLON ipv4body
	| tcp tcpline
	| udp udpline
	| icmp icmpline
	| data dataline
	;

ipv4type:
	| IL_V4PROTO IL_TOKEN		{ set_ipv4proto(&yylval.str); }
	| IL_V4SRC IL_TOKEN		{ set_ipv4src(&yylval.str); }
	| IL_V4DST IL_TOKEN		{ set_ipv4dst(&yylval.str); }
	| IL_V4OFF IL_TOKEN		{ set_ipv4off(&yylval.str); }
	| IL_V4V IL_TOKEN		{ set_ipv4v(&yylval.str); }
	| IL_V4HL IL_TOKEN		{ set_ipv4hl(&yylval.str); }
	| IL_V4ID IL_TOKEN		{ set_ipv4id(&yylval.str); }
	| IL_V4TTL IL_TOKEN		{ set_ipv4ttl(&yylval.str); }
	| IL_V4TOS IL_TOKEN		{ set_ipv4tos(&yylval.str); }
	| IL_V4SUM IL_TOKEN		{ set_ipv4sum(&yylval.str); }
	| IL_V4LEN IL_TOKEN		{ set_ipv4len(&yylval.str); }
	| ipv4opt IL_LBRACE ipv4optlist IL_RBRACE	{ end_ipopt(); }
	| ipline
	;

tcp:	IL_TCP				{ new_tcpheader(); }
	;

tcpline:
	IL_LBRACE tcpheader IL_RBRACE	{ end_tcp(); }
	;

tcpheader:
	tcpbody  IL_SEMICOLON
	| tcpbody IL_SEMICOLON tcpheader
	;

tcpbody:
	| IL_SPORT IL_TOKEN		{ set_tcpsport(&yylval.str); }
	| IL_DPORT IL_TOKEN		{ set_tcpdport(&yylval.str); }
	| IL_TCPSEQ IL_TOKEN		{ set_tcpseq(&yylval.str); }
	| IL_TCPACK IL_TOKEN		{ set_tcpack(&yylval.str); }
	| IL_TCPOFF IL_TOKEN		{ set_tcpoff(&yylval.str); }
	| IL_TCPURP IL_TOKEN		{ set_tcpurp(&yylval.str); }
	| IL_TCPWIN IL_TOKEN		{ set_tcpwin(&yylval.str); }
	| IL_TCPSUM IL_TOKEN		{ set_tcpsum(&yylval.str); }
	| IL_TCPFL IL_TOKEN		{ set_tcpflags(&yylval.str); }
	| IL_TCPOPT IL_LBRACE tcpopts IL_RBRACE	{ end_tcpopt(); }
	| data dataline
	;

tcpopts:
	| tcpopt tcpopts
	;

tcpopt:	IL_TCPO_NOP IL_SEMICOLON	{ set_tcpopt(IL_TCPO_NOP, NULL); }
	| IL_TCPO_EOL IL_SEMICOLON	{ set_tcpopt(IL_TCPO_EOL, NULL); }
	| IL_TCPO_MSS optoken		{ set_tcpopt(IL_TCPO_MSS,&yylval.str);}
	| IL_TCPO_WSCALE optoken	{ set_tcpopt(IL_TCPO_MSS,&yylval.str);}
	| IL_TCPO_TS optoken		{ set_tcpopt(IL_TCPO_TS, &yylval.str);}
	;

udp:	IL_UDP				{ new_udpheader(); }
	;

udpline:	IL_LBRACE udpheader IL_RBRACE		{ end_udp(); }
	;


udpheader:
	udpbody IL_SEMICOLON
	| udpbody IL_SEMICOLON udpheader
	;

udpbody:
	| IL_SPORT IL_TOKEN		{ set_tcpsport(&yylval.str); }
	| IL_DPORT IL_TOKEN		{ set_tcpdport(&yylval.str); }
	| IL_UDPLEN IL_TOKEN		{ set_udplen(&yylval.str); }
	| IL_UDPSUM IL_TOKEN		{ set_udpsum(&yylval.str); }
	| data dataline
	;

icmp:	IL_ICMP				{ new_icmpheader(); }
	;

icmpline:
	IL_LBRACE icmpbody IL_RBRACE	{ end_icmp(); }
	;

icmpbody:
	icmptype icmpcode
	| icmptype
	| data dataline
	;

icmpcode:
	IL_ICMPCODE token		{ set_icmpcodetok(&yylval.str); }
	;


icmptype:
	IL_ICMP_ECHOREPLY		{ set_icmptype(ICMP_ECHOREPLY); }
	| IL_ICMP_ECHOREPLY IL_LBRACE icmpechoopts IL_RBRACE
	| unreach
	| IL_ICMP_SOURCEQUENCH		{ set_icmptype(ICMP_SOURCEQUENCH); }
	| redirect
	| IL_ICMP_ROUTERADVERT		{ set_icmptype(ICMP_ROUTERADVERT); }
	| IL_ICMP_ROUTERSOLICIT		{ set_icmptype(ICMP_ROUTERSOLICIT); }
	| IL_ICMP_ECHO			{ set_icmptype(ICMP_ECHO); }
	| IL_ICMP_ECHO IL_LBRACE icmpechoopts IL_RBRACE
	| IL_ICMP_TIMXCEED		{ set_icmptype(ICMP_TIMXCEED); }
	| IL_ICMP_TIMXCEED IL_LBRACE exceed IL_RBRACE
	| IL_ICMP_TSTAMP		{ set_icmptype(ICMP_TSTAMP); }
	| IL_ICMP_TSTAMPREPLY		{ set_icmptype(ICMP_TSTAMPREPLY); }
	| IL_ICMP_TSTAMPREPLY IL_LBRACE icmptsopts IL_RBRACE
	| IL_ICMP_IREQ			{ set_icmptype(ICMP_IREQ); }
	| IL_ICMP_IREQREPLY		{ set_icmptype(ICMP_IREQREPLY); }
	| IL_ICMP_IREQREPLY IL_LBRACE data dataline IL_RBRACE
	| IL_ICMP_MASKREQ		{ set_icmptype(ICMP_MASKREQ); }
	| IL_ICMP_MASKREPLY		{ set_icmptype(ICMP_MASKREPLY); }
	| IL_ICMP_MASKREPLY IL_LBRACE token IL_RBRACE
	| IL_ICMP_PARAMPROB		{ set_icmptype(ICMP_PARAMPROB); }
	| IL_ICMP_PARAMPROB IL_LBRACE paramprob IL_RBRACE
	| token				{ set_icmptypetok(&yylval.str); }
	;

icmpechoopts:
	| icmpechoopts icmpecho
	;

icmpecho:
	IL_ICMP_SEQ number 			{ set_icmpseq(yylval.num); }
	| IL_ICMP_ID number			{ set_icmpid(yylval.num); }
	;

icmptsopts:
	| icmptsopts icmpts
	;

icmpts: IL_ICMP_OTIME number 			{ set_icmpotime(yylval.num); }
	| IL_ICMP_RTIME number 			{ set_icmprtime(yylval.num); }
	| IL_ICMP_TTIME number 			{ set_icmpttime(yylval.num); }
	;

unreach:
	IL_ICMP_UNREACH
	| IL_ICMP_UNREACH IL_LBRACE unreachopts IL_RBRACE
	;

unreachopts:
	IL_ICMP_UNREACH_NET line
	| IL_ICMP_UNREACH_HOST line
	| IL_ICMP_UNREACH_PROTOCOL line
	| IL_ICMP_UNREACH_PORT line
	| IL_ICMP_UNREACH_NEEDFRAG number	{ set_icmpmtu(yylval.num); }
	| IL_ICMP_UNREACH_SRCFAIL line
	| IL_ICMP_UNREACH_NET_UNKNOWN line
	| IL_ICMP_UNREACH_HOST_UNKNOWN line
	| IL_ICMP_UNREACH_ISOLATED line
	| IL_ICMP_UNREACH_NET_PROHIB line
	| IL_ICMP_UNREACH_HOST_PROHIB line
	| IL_ICMP_UNREACH_TOSNET line
	| IL_ICMP_UNREACH_TOSHOST line
	| IL_ICMP_UNREACH_FILTER_PROHIB line
	| IL_ICMP_UNREACH_HOST_PRECEDENCE line
	| IL_ICMP_UNREACH_PRECEDENCE_CUTOFF line
	;

redirect:
	IL_ICMP_REDIRECT
	| IL_ICMP_REDIRECT IL_LBRACE redirectopts IL_RBRACE
	;

redirectopts:
	| IL_ICMP_REDIRECT_NET token		{ set_redir(0, &yylval.str); }
	| IL_ICMP_REDIRECT_HOST token		{ set_redir(1, &yylval.str); }
	| IL_ICMP_REDIRECT_TOSNET token		{ set_redir(2, &yylval.str); }
	| IL_ICMP_REDIRECT_TOSHOST token	{ set_redir(3, &yylval.str); }
	;

exceed:
	IL_ICMP_TIMXCEED_INTRANS line
	| IL_ICMP_TIMXCEED_REASS line
	;

paramprob:
	IL_ICMP_PARAMPROB_OPTABSENT
	| IL_ICMP_PARAMPROB_OPTABSENT paraprobarg

paraprobarg:
	IL_LBRACE number IL_RBRACE		{ set_icmppprob(yylval.num); }
	;

ipv4opt:	IL_V4OPT			{ new_ipv4opt(); }
	;

ipv4optlist:
	| ipv4opts ipv4optlist
	;

ipv4opts:
	IL_IPO_NOP IL_SEMICOLON		{ add_ipopt(IL_IPO_NOP, NULL); }
	| IL_IPO_RR optnumber		{ add_ipopt(IL_IPO_RR, &yylval.num); }
	| IL_IPO_ZSU IL_SEMICOLON	{ add_ipopt(IL_IPO_ZSU, NULL); }
	| IL_IPO_MTUP IL_SEMICOLON	{ add_ipopt(IL_IPO_MTUP, NULL); }
	| IL_IPO_MTUR IL_SEMICOLON	{ add_ipopt(IL_IPO_MTUR, NULL); }
	| IL_IPO_ENCODE IL_SEMICOLON	{ add_ipopt(IL_IPO_ENCODE, NULL); }
	| IL_IPO_TS IL_SEMICOLON	{ add_ipopt(IL_IPO_TS, NULL); }
	| IL_IPO_TR IL_SEMICOLON	{ add_ipopt(IL_IPO_TR, NULL); }
	| IL_IPO_SEC IL_SEMICOLON	{ add_ipopt(IL_IPO_SEC, NULL); }
	| IL_IPO_SECCLASS secclass	{ add_ipopt(IL_IPO_SECCLASS, sclass); }
	| IL_IPO_LSRR token		{ add_ipopt(IL_IPO_LSRR,&yylval.str); }
	| IL_IPO_ESEC IL_SEMICOLON	{ add_ipopt(IL_IPO_ESEC, NULL); }
	| IL_IPO_CIPSO IL_SEMICOLON	{ add_ipopt(IL_IPO_CIPSO, NULL); }
	| IL_IPO_SATID optnumber	{ add_ipopt(IL_IPO_SATID,&yylval.num);}
	| IL_IPO_SSRR token		{ add_ipopt(IL_IPO_SSRR,&yylval.str); }
	| IL_IPO_ADDEXT IL_SEMICOLON	{ add_ipopt(IL_IPO_ADDEXT, NULL); }
	| IL_IPO_VISA IL_SEMICOLON	{ add_ipopt(IL_IPO_VISA, NULL); }
	| IL_IPO_IMITD IL_SEMICOLON	{ add_ipopt(IL_IPO_IMITD, NULL); }
	| IL_IPO_EIP IL_SEMICOLON	{ add_ipopt(IL_IPO_EIP, NULL); }
	| IL_IPO_FINN IL_SEMICOLON	{ add_ipopt(IL_IPO_FINN, NULL); }
	;

secclass:
	IL_IPS_RESERV4				{ set_secclass(&yylval.str); }
	| IL_IPS_TOPSECRET			{ set_secclass(&yylval.str); }
	| IL_IPS_SECRET				{ set_secclass(&yylval.str); }
	| IL_IPS_RESERV3			{ set_secclass(&yylval.str); }
	| IL_IPS_CONFID				{ set_secclass(&yylval.str); }
	| IL_IPS_UNCLASS			{ set_secclass(&yylval.str); }
	| IL_IPS_RESERV2			{ set_secclass(&yylval.str); }
	| IL_IPS_RESERV1			{ set_secclass(&yylval.str); }
	;

data:	IL_DATA					{ new_data(); }
	;

dataline:
	IL_LBRACE databody IL_RBRACE		{ end_data(); }
	;

databody: dataopts IL_SEMICOLON
	| dataopts IL_SEMICOLON databody
	;

dataopts:
	IL_DLEN IL_TOKEN			{ set_datalen(&yylval.str); }
	| IL_DVALUE IL_TOKEN 			{ set_data(&yylval.str); }
	| IL_DFILE IL_TOKEN 			{ set_datafile(&yylval.str); }
	;

token: IL_TOKEN IL_SEMICOLON
	;

optoken: IL_SEMICOLON
	| token
	;

number: digits IL_SEMICOLON
	;

optnumber: IL_SEMICOLON
	| number
	;

digits:	IL_NUMBER
	| digits IL_NUMBER
	;
%%

struct	statetoopt	toipopts[] = {
	{ IL_IPO_NOP,		IPOPT_NOP },
	{ IL_IPO_RR,		IPOPT_RR },
	{ IL_IPO_ZSU,		IPOPT_ZSU },
	{ IL_IPO_MTUP,		IPOPT_MTUP },
	{ IL_IPO_MTUR,		IPOPT_MTUR },
	{ IL_IPO_ENCODE,	IPOPT_ENCODE },
	{ IL_IPO_TS,		IPOPT_TS },
	{ IL_IPO_TR,		IPOPT_TR },
	{ IL_IPO_SEC,		IPOPT_SECURITY },
	{ IL_IPO_SECCLASS,	IPOPT_SECURITY },
	{ IL_IPO_LSRR,		IPOPT_LSRR },
	{ IL_IPO_ESEC,		IPOPT_E_SEC },
	{ IL_IPO_CIPSO,		IPOPT_CIPSO },
	{ IL_IPO_SATID,		IPOPT_SATID },
	{ IL_IPO_SSRR,		IPOPT_SSRR },
	{ IL_IPO_ADDEXT,	IPOPT_ADDEXT },
	{ IL_IPO_VISA,		IPOPT_VISA },
	{ IL_IPO_IMITD,		IPOPT_IMITD },
	{ IL_IPO_EIP,		IPOPT_EIP },
	{ IL_IPO_FINN,		IPOPT_FINN },
	{ 0, 0 }
};

struct	statetoopt	tosecopts[] = {
	{ IL_IPS_RESERV4,	IPSO_CLASS_RES4 },
	{ IL_IPS_TOPSECRET,	IPSO_CLASS_TOPS },
	{ IL_IPS_SECRET,	IPSO_CLASS_SECR },
	{ IL_IPS_RESERV3,	IPSO_CLASS_RES3 },
	{ IL_IPS_CONFID,	IPSO_CLASS_CONF },
	{ IL_IPS_UNCLASS,	IPSO_CLASS_UNCL },
	{ IL_IPS_RESERV2,	IPSO_CLASS_RES2 },
	{ IL_IPS_RESERV1,	IPSO_CLASS_RES1 },
	{ 0, 0 }
};


struct in_addr getipv4addr(arg)
char *arg;
{
	struct hostent *hp;
	struct in_addr in;

	in.s_addr = 0xffffffff;

	if ((hp = gethostbyname(arg)))
		bcopy(hp->h_addr, &in.s_addr, sizeof(struct in_addr));
	else
		in.s_addr = inet_addr(arg);
	return in;
}


u_short getportnum(pr, name)
char *pr, *name;
{
	struct servent *sp;

	if (!(sp = getservbyname(name, pr)))
		return atoi(name);
	return sp->s_port;
}


struct ether_addr *geteaddr(arg, buf)
char *arg;
struct ether_addr *buf;
{
	struct ether_addr *e;

	e = ether_aton(arg);
	if (!e)
		fprintf(stderr, "Invalid ethernet address: %s\n", arg);
	else
#ifdef	__FreeBSD__
		bcopy(e->octet, buf->octet, sizeof(e->octet));
#else
		bcopy(e->ether_addr_octet, buf->ether_addr_octet,
		      sizeof(e->ether_addr_octet));
#endif
	return e;
}


void *new_header(type)
int type;
{
	aniphdr_t *aip, *oip = canip;
	int	sz = 0;

	aip = (aniphdr_t *)calloc(1, sizeof(*aip));
	*aniptail = aip;
	aniptail = &aip->ah_next;
	aip->ah_p = type;
	aip->ah_prev = oip;
	canip = aip;

	if (type == IPPROTO_UDP)
		sz = sizeof(udphdr_t);
	else if (type == IPPROTO_TCP)
		sz = sizeof(tcphdr_t);
	else if (type == IPPROTO_ICMP)
		sz = sizeof(icmphdr_t);
	else if (type == IPPROTO_IP)
		sz = sizeof(ip_t);

	if (oip)
		canip->ah_data = oip->ah_data + oip->ah_len;
	else
		canip->ah_data = (char *)ipbuffer;

	/*
	 * Increase the size fields in all wrapping headers.
	 */
	for (aip = aniphead; aip; aip = aip->ah_next) {
		aip->ah_len += sz;
		if (aip->ah_p == IPPROTO_IP)
			aip->ah_ip->ip_len += sz;
		else if (aip->ah_p == IPPROTO_UDP)
			aip->ah_udp->uh_ulen += sz;
	}
	return (void *)canip->ah_data;
}


void free_aniplist()
{
	aniphdr_t *aip, **aipp = &aniphead;

	while ((aip = *aipp)) {
		*aipp = aip->ah_next;
		free(aip);
	}
	aniptail = &aniphead;
}


void inc_anipheaders(inc)
int inc;
{
	aniphdr_t *aip;

	for (aip = aniphead; aip; aip = aip->ah_next) {
		aip->ah_len += inc;
		if (aip->ah_p == IPPROTO_IP)
			aip->ah_ip->ip_len += inc;
		else if (aip->ah_p == IPPROTO_UDP)
			aip->ah_udp->uh_ulen += inc;
	}
}


void new_data()
{
	(void) new_header(-1);
	canip->ah_len = 0;
}


void set_datalen(arg)
char **arg;
{
	int	len;

	len = strtol(*arg, NULL, 0);
	inc_anipheaders(len);
	free(*arg);
	*arg = NULL;
}


void set_data(arg)
char **arg;
{
	u_char *s = *arg, *t = canip->ah_data, c;
	int len = 0, todo = 0, quote = 0, val = 0;

	while ((c = *s++)) {
		if (todo) {
			if (isdigit(c)) {
				todo--;
				if (c > '7') {
					fprintf(stderr, "octal with %c!\n", c);
					break;
				}
				val <<= 3;
				val |= (c - '0');
			}
			if (!isdigit(c) || !todo) {
				*t++ = (u_char)(val & 0xff);
				todo = 0;
			}
		}
		if (quote) {
			if (isdigit(c)) {
				todo = 2;
				if (c > '7') {
					fprintf(stderr, "octal with %c!\n", c);
					break;
				}
				val = (c - '0');
			} else {
				switch (c)
				{
				case '\"' :
					*t++ = '\"';
					break;
				case '\\' :
					*t++ = '\\';
					break;
				case 'n' :
					*t++ = '\n';
					break;
				case 'r' :
					*t++ = '\r';
					break;
				case 't' :
					*t++ = '\t';
					break;
				}
				quote = 0;
			}
			continue;
		}

		if (c == '\\')
			quote = 1;
		else
			*t++ = c;
	}
	if (quote)
		*t++ = '\\';
	len = t - (u_char *)canip->ah_data;
	inc_anipheaders(len - canip->ah_len);
	canip->ah_len = len;
}


void set_datafile(arg)
char **arg;
{
	struct stat sb;
	char *file = *arg;
	int fd, len;

	if ((fd = open(file, O_RDONLY)) == -1) {
		perror("open");
		exit(-1);
	}

	if (fstat(fd, &sb) == -1) {
		perror("fstat");
		exit(-1);
	}

	if ((sb.st_size + aniphead->ah_len ) > 65535) {
		fprintf(stderr, "data file %s too big to include.\n", file);
		close(fd);
		return;
	}
	if ((len = read(fd, canip->ah_data, sb.st_size)) == -1) {
		perror("read");
		close(fd);
		return;
	}
	inc_anipheaders(len);
	canip->ah_len += len;
	close(fd);
}


void new_packet()
{
	static	u_short	id = 0;

	if (!aniphead)
		bzero((char *)ipbuffer, sizeof(ipbuffer));

	ip = (ip_t *)new_header(IPPROTO_IP);
	ip->ip_v = IPVERSION;
	ip->ip_hl = sizeof(ip_t) >> 2;
	ip->ip_len = sizeof(ip_t);
	ip->ip_ttl = 63;
	ip->ip_id = htons(id++);
}


void set_ipv4proto(arg)
char **arg;
{
	struct protoent *pr;

	if ((pr = getprotobyname(*arg)))
		ip->ip_p = pr->p_proto;
	else
		if (!(ip->ip_p = atoi(*arg)))
			fprintf(stderr, "unknown protocol %s\n", *arg);
	free(*arg);
	*arg = NULL;
}


void set_ipv4src(arg)
char **arg;
{
	ip->ip_src = getipv4addr(*arg);
	free(*arg);
	*arg = NULL;
}


void set_ipv4dst(arg)
char **arg;
{
	ip->ip_dst = getipv4addr(*arg);
	free(*arg);
	*arg = NULL;
}


void set_ipv4off(arg)
char **arg;
{
	ip->ip_off = strtol(*arg, NULL, 0);
	free(*arg);
	*arg = NULL;
}


void set_ipv4v(arg)
char **arg;
{
	ip->ip_v = strtol(*arg, NULL, 0);
	free(*arg);
	*arg = NULL;
}


void set_ipv4hl(arg)
char **arg;
{
	int newhl, inc;

	newhl = strtol(*arg, NULL, 0);
	inc = (newhl - ip->ip_hl) << 2;
	ip->ip_len += inc;
	ip->ip_hl = newhl;
	canip->ah_len += inc;
	free(*arg);
	*arg = NULL;
}


void set_ipv4ttl(arg)
char **arg;
{
	ip->ip_ttl = strtol(*arg, NULL, 0);
	free(*arg);
	*arg = NULL;
}


void set_ipv4tos(arg)
char **arg;
{
	ip->ip_tos = strtol(*arg, NULL, 0);
	free(*arg);
	*arg = NULL;
}


void set_ipv4id(arg)
char **arg;
{
	ip->ip_id = strtol(*arg, NULL, 0);
	free(*arg);
	*arg = NULL;
}


void set_ipv4sum(arg)
char **arg;
{
	ip->ip_sum = strtol(*arg, NULL, 0);
	free(*arg);
	*arg = NULL;
}


void set_ipv4len(arg)
char **arg;
{
	int len;

	len = strtol(*arg, NULL, 0);
	inc_anipheaders(len - ip->ip_len);
	ip->ip_len = len;
	free(*arg);
	*arg = NULL;
}


void new_tcpheader()
{

	if ((ip->ip_p) && (ip->ip_p != IPPROTO_TCP)) {
		fprintf(stderr, "protocol %d specified with TCP!\n", ip->ip_p);
		return;
	}
	ip->ip_p = IPPROTO_TCP;

	tcp = (tcphdr_t *)new_header(IPPROTO_TCP);
	tcp->th_win = 4096;
	tcp->th_off = sizeof(*tcp) >> 2;
}


void set_tcpsport(arg)
char **arg;
{
	u_short *port;
	char *pr;

	if (ip->ip_p == IPPROTO_UDP) {
		port = &udp->uh_sport;
		pr = "udp";
	} else {
		port = &tcp->th_sport;
		pr = "udp";
	}

	*port = getportnum(pr, *arg);
	free(*arg);
	*arg = NULL;
}


void set_tcpdport(arg)
char **arg;
{
	u_short *port;
	char *pr;

	if (ip->ip_p == IPPROTO_UDP) {
		port = &udp->uh_dport;
		pr = "udp";
	} else {
		port = &tcp->th_dport;
		pr = "udp";
	}

	*port = getportnum(pr, *arg);
	free(*arg);
	*arg = NULL;
}


void set_tcpseq(arg)
char **arg;
{
	tcp->th_seq = strtol(*arg, NULL, 0);
	free(*arg);
	*arg = NULL;
}


void set_tcpack(arg)
char **arg;
{
	tcp->th_ack = strtol(*arg, NULL, 0);
	free(*arg);
	*arg = NULL;
}


void set_tcpoff(arg)
char **arg;
{
	int	off;

	off = strtol(*arg, NULL, 0);
	inc_anipheaders((off - tcp->th_off) << 2);
	tcp->th_off = off;
	free(*arg);
	*arg = NULL;
}


void set_tcpurp(arg)
char **arg;
{
	tcp->th_urp = strtol(*arg, NULL, 0);
	free(*arg);
	*arg = NULL;
}


void set_tcpwin(arg)
char **arg;
{
	tcp->th_win = strtol(*arg, NULL, 0);
	free(*arg);
	*arg = NULL;
}


void set_tcpsum(arg)
char **arg;
{
	tcp->th_sum = strtol(*arg, NULL, 0);
	free(*arg);
	*arg = NULL;
}


void set_tcpflags(arg)
char **arg;
{
	static	char	flags[] = "ASURPF";
	static	int	flagv[] = { TH_ACK, TH_SYN, TH_URG, TH_RST, TH_PUSH,
				    TH_FIN } ;
	char *s, *t;

	for (s = *arg; *s; s++)
		if (!(t = strchr(flags, *s))) {
			if (s - *arg) {
				fprintf(stderr, "unknown TCP flag %c\n", *s);
				break;
			}
			tcp->th_flags = strtol(*arg, NULL, 0);
			break;
		} else
			tcp->th_flags |= flagv[t - flags];
	free(*arg);
	*arg = NULL;
}


void set_tcpopt(state, arg)
int state;
char **arg;
{
	u_char *s;
	int val, len, val2, pad, optval;

	if (arg && *arg)
		val = atoi(*arg);
	else
		val = 0;

	s = (u_char *)tcp + sizeof(*tcp) + canip->ah_optlen;
	switch (state)
	{
	case IL_TCPO_EOL :
		optval = 0;
		len = 1;
		break;
	case IL_TCPO_NOP :
		optval = 1;
		len = 1;
		break;
	case IL_TCPO_MSS :
		optval = 2;
		len = 4;
		break;
	case IL_TCPO_WSCALE :
		optval = 3;
		len = 3;
		break;
	case IL_TCPO_TS :
		optval = 8;
		len = 10;
		break;
	default :
		optval = 0;
		len = 0;
		break;
	}

	if (len > 1) {
		/*
		 * prepend padding - if required.
		 */
		if (len & 3)
			for (pad = 4 - (len & 3); pad; pad--) {
				*s++ = 1;
				canip->ah_optlen++;
			}
		/*
		 * build tcp option
		 */
		*s++ = (u_char)optval;
		*s++ = (u_char)len;
		if (len > 2) {
			if (len == 3) {		/* 1 byte - char */
				*s++ = (u_char)val;
			} else if (len == 4) {	/* 2 bytes - short */
				*s++ = (u_char)((val >> 8) & 0xff);
				*s++ = (u_char)(val & 0xff);
			} else if (len >= 6) {	/* 4 bytes - long */
				val2 = htonl(val);
				bcopy((char *)&val2, s, 4);
			}
			s += (len - 2);
		}
	} else
		*s++ = (u_char)optval;

	canip->ah_lastopt = optval;
	canip->ah_optlen += len;

	if (arg && *arg) {
		free(*arg);
		*arg = NULL;
	}
}


void end_tcpopt()
{
	int pad;
	char *s = (char *)tcp;

	s += sizeof(*tcp) + canip->ah_optlen;
	/*
	 * pad out so that we have a multiple of 4 bytes in size fo the
	 * options.  make sure last byte is EOL.
	 */
	if (canip->ah_optlen & 3) {
		if (canip->ah_lastopt != 1) {
			for (pad = 3 - (canip->ah_optlen & 3); pad; pad--) {
				*s++ = 1;
				canip->ah_optlen++;
			}
			canip->ah_optlen++;
		} else {
			s -= 1;

			for (pad = 3 - (canip->ah_optlen & 3); pad; pad--) {
				*s++ = 1;
				canip->ah_optlen++;
			}
		}
		*s++ = 0;
	}
	tcp->th_off = (sizeof(*tcp) + canip->ah_optlen) >> 2;
	inc_anipheaders(canip->ah_optlen);
}


void new_udpheader()
{
	if ((ip->ip_p) && (ip->ip_p != IPPROTO_UDP)) {
		fprintf(stderr, "protocol %d specified with UDP!\n", ip->ip_p);
		return;
	}
	ip->ip_p = IPPROTO_UDP;

	udp = (udphdr_t *)new_header(IPPROTO_UDP);
	udp->uh_ulen = sizeof(*udp);
}


void set_udplen(arg)
char **arg;
{
	int len;

	len = strtol(*arg, NULL, 0);
	inc_anipheaders(len - udp->uh_ulen);
	udp->uh_ulen = len;
	free(*arg);
	*arg = NULL;
}


void set_udpsum(arg)
char **arg;
{
	udp->uh_sum = strtol(*arg, NULL, 0);
	free(*arg);
	*arg = NULL;
}


void prep_packet()
{
	iface_t *ifp;
	struct in_addr gwip;

	ifp = sending.snd_if;
	if (!ifp) {
		fprintf(stderr, "no interface defined for sending!\n");
		return;
	}
	if (ifp->if_fd == -1)
		ifp->if_fd = initdevice(ifp->if_name, 0, 5);
	gwip = sending.snd_gw;
	if (!gwip.s_addr)
		gwip = aniphead->ah_ip->ip_dst;
	(void) send_ip(ifp->if_fd, ifp->if_MTU, (ip_t *)ipbuffer, gwip, 2);
}


void packet_done()
{
	char    outline[80];
	int     i, j, k;
	u_char  *s = (u_char *)ipbuffer, *t = (u_char *)outline;

	if (opts & OPT_VERBOSE) {
		for (i = ip->ip_len, j = 0; i; i--, j++, s++) {
			if (j && !(j & 0xf)) {
				*t++ = '\n';
				*t = '\0';
				fputs(outline, stdout);
				fflush(stdout);
				t = (u_char *)outline;
				*t = '\0';
			}
			sprintf(t, "%02x", *s & 0xff);
			t += 2;
			if (!((j + 1) & 0xf)) {
				s -= 15;
				sprintf(t, "        ");
				t += 8;
				for (k = 16; k; k--, s++)
					*t++ = (isprint(*s) ? *s : '.');
				s--;
			}

			if ((j + 1) & 0xf)
				*t++ = ' ';;
		}

		if (j & 0xf) {
			for (k = 16 - (j & 0xf); k; k--) {
				*t++ = ' ';
				*t++ = ' ';
				*t++ = ' ';
			}
			sprintf(t, "       ");
			t += 7;
			s -= j & 0xf;
			for (k = j & 0xf; k; k--, s++)
				*t++ = (isprint(*s) ? *s : '.');
			*t++ = '\n';
			*t = '\0';
		}
		fputs(outline, stdout);
		fflush(stdout);
	}

	prep_packet();
	free_aniplist();
}


void new_interface()
{
	cifp = (iface_t *)calloc(1, sizeof(iface_t));
	*iftail = cifp;
	iftail = &cifp->if_next;
	cifp->if_fd = -1;
}


void check_interface()
{
	if (!cifp->if_name || !*cifp->if_name)
		fprintf(stderr, "No interface name given!\n");
	if (!cifp->if_MTU || !*cifp->if_name)
		fprintf(stderr, "Interface %s has an MTU of 0!\n",
			cifp->if_name);
}


void set_ifname(arg)
char **arg;
{
	cifp->if_name = *arg;
	*arg = NULL;
}


void set_ifmtu(arg)
int arg;
{
	cifp->if_MTU = arg;
}


void set_ifv4addr(arg)
char **arg;
{
	cifp->if_addr = getipv4addr(*arg);
	free(*arg);
	*arg = NULL;
}


void set_ifeaddr(arg)
char **arg;
{
	(void) geteaddr(*arg, &cifp->if_eaddr);
	free(*arg);
	*arg = NULL;
}


void new_arp()
{
	carp = (arp_t *)calloc(1, sizeof(arp_t));
	*arptail = carp;
	arptail = &carp->arp_next;
}


void set_arpeaddr(arg)
char **arg;
{
	(void) geteaddr(*arg, &carp->arp_eaddr);
	free(*arg);
	*arg = NULL;
}


void set_arpv4addr(arg)
char **arg;
{
	carp->arp_addr = getipv4addr(*arg);
	free(*arg);
	*arg = NULL;
}


void reset_send()
{
	sending.snd_if = iflist;
	sending.snd_gw = defrouter;
}


void set_sendif(arg)
char **arg;
{
	iface_t	*ifp;

	for (ifp = iflist; ifp; ifp = ifp->if_next)
		if (ifp->if_name && !strcmp(ifp->if_name, *arg))
			break;
	sending.snd_if = ifp;
	if (!ifp)
		fprintf(stderr, "couldn't find interface %s\n", *arg);
	free(*arg);
	*arg = NULL;
}


void set_sendvia(arg)
char **arg;
{
	sending.snd_gw = getipv4addr(*arg);
	free(*arg);
	*arg = NULL;
}


void set_defaultrouter(arg)
char **arg;
{
	defrouter = getipv4addr(*arg);
	free(*arg);
	*arg = NULL;
}


void new_icmpheader()
{
	if ((ip->ip_p) && (ip->ip_p != IPPROTO_ICMP)) {
		fprintf(stderr, "protocol %d specified with ICMP!\n",
			ip->ip_p);
		return;
	}
	ip->ip_p = IPPROTO_ICMP;
	icmp = (icmphdr_t *)new_header(IPPROTO_ICMP);
}


void set_icmpcode(code)
int code;
{
	icmp->icmp_code = code;
}


void set_icmptype(type)
int type;
{
	icmp->icmp_type = type;
}


static	char	*icmpcodes[] = {
	"net-unr", "host-unr", "proto-unr", "port-unr", "needfrag", "srcfail",
	"net-unk", "host-unk", "isolate", "net-prohib", "host-prohib",
	"net-tos", "host-tos", NULL };

void set_icmpcodetok(code)
char **code;
{
	char	*s;
	int	i;

	for (i = 0; (s = icmpcodes[i]); i++)
		if (!strcmp(s, *code)) {
			icmp->icmp_code = i;
			break;
		}
	if (!s)
		fprintf(stderr, "unknown ICMP code %s\n", *code);
	free(*code);
	*code = NULL;
}


static	char	*icmptypes[] = {
	"echorep", (char *)NULL, (char *)NULL, "unreach", "squench",
	"redir", (char *)NULL, (char *)NULL, "echo", (char *)NULL,
	(char *)NULL, "timex", "paramprob", "timest", "timestrep",
	"inforeq", "inforep", "maskreq", "maskrep", "END"
};

void set_icmptypetok(type)
char **type;
{
	char	*s;
	int	i, done = 0;

	for (i = 0; !(s = icmptypes[i]) || strcmp(s, "END"); i++)
		if (s && !strcmp(s, *type)) {
			icmp->icmp_type = i;
			done = 1;
			break;
		}
	if (!done)
		fprintf(stderr, "unknown ICMP type %s\n", *type);
	free(*type);
	*type = NULL;
}


void set_icmpid(arg)
int arg;
{
	icmp->icmp_id = arg;
}


void set_icmpseq(arg)
int arg;
{
	icmp->icmp_seq = arg;
}


void set_icmpotime(arg)
int arg;
{
	icmp->icmp_otime = arg;
}


void set_icmprtime(arg)
int arg;
{
	icmp->icmp_rtime = arg;
}


void set_icmpttime(arg)
int arg;
{
	icmp->icmp_ttime = arg;
}


void set_icmpmtu(arg)
int arg;
{
#if	BSD >= 199306
	icmp->icmp_nextmtu = arg;
#endif
}


void set_redir(redir, arg)
int redir;
char **arg;
{
	icmp->icmp_code = redir;
	icmp->icmp_gwaddr = getipv4addr(*arg);
	free(*arg);
	*arg = NULL;
}


void set_icmppprob(num)
int num;
{
	icmp->icmp_pptr = num;
}


void new_ipv4opt()
{
	new_header(-2);
}


void add_ipopt(state, ptr)
int state;
void *ptr;
{
	struct ipopt_names *io;
	struct statetoopt *sto;
	char numbuf[16], *arg, **param = ptr;
	int inc, hlen;

	if (state == IL_IPO_RR || state == IL_IPO_SATID) {
		if (param)
			sprintf(numbuf, "%d", *(int *)param);
		else
			strcpy(numbuf, "0");
		arg = numbuf;
	} else
		arg = param ? *param : NULL;

	if (canip->ah_next) {
		fprintf(stderr, "cannot specify options after data body\n");
		return;
	}
	for (sto = toipopts; sto->sto_st; sto++)
		if (sto->sto_st == state)
			break;
	if (!sto || !sto->sto_st) {
		fprintf(stderr, "No mapping for state %d to IP option\n",
			state);
		return;
	}

	hlen = sizeof(ip_t) + canip->ah_optlen;
	for (io = ionames; io->on_name; io++)
		if (io->on_value == sto->sto_op)
			break;
	canip->ah_lastopt = io->on_value;

	if (io->on_name) {
		inc = addipopt((char *)ip + hlen, io, hlen - sizeof(ip_t),arg);
		if (inc > 0) {
			while (inc & 3) {
				((char *)ip)[sizeof(*ip) + inc] = IPOPT_NOP;
				canip->ah_lastopt = IPOPT_NOP;
				inc++;
			}
			hlen += inc;
		}
	}

	canip->ah_optlen = hlen - sizeof(ip_t);

	if (state != IL_IPO_RR && state != IL_IPO_SATID)
		if (param && *param) {
			free(*param);
			*param = NULL;
		}
	sclass = NULL;
}


void end_ipopt()
{
	int pad;
	char *s, *buf = (char *)ip;

	/*
	 * pad out so that we have a multiple of 4 bytes in size fo the
	 * options.  make sure last byte is EOL.
	 */
	if (canip->ah_lastopt == IPOPT_NOP) {
		buf[sizeof(*ip) + canip->ah_optlen - 1] = IPOPT_EOL;
	} else if (canip->ah_lastopt != IPOPT_EOL) {
		s = buf + sizeof(*ip) + canip->ah_optlen;

		for (pad = 3 - (canip->ah_optlen & 3); pad; pad--) {
			*s++ = IPOPT_NOP;
			*s = IPOPT_EOL;
			canip->ah_optlen++;
		}
		canip->ah_optlen++;
	} else {
		s = buf + sizeof(*ip) + canip->ah_optlen - 1;

		for (pad = 3 - (canip->ah_optlen & 3); pad; pad--) {
			*s++ = IPOPT_NOP;
			*s = IPOPT_EOL;
			canip->ah_optlen++;
		}
	}
	ip->ip_hl = (sizeof(*ip) + canip->ah_optlen) >> 2;
	inc_anipheaders(canip->ah_optlen);
	free_anipheader();
}


void set_secclass(arg)
char **arg;
{
	sclass = *arg;
	*arg = NULL;
}


void free_anipheader()
{
	aniphdr_t *aip;

	aip = canip;
	if ((canip = aip->ah_prev)) {
		canip->ah_next = NULL;
		aniptail = &canip->ah_next;
	}
	free(aip);
}


void end_ipv4()
{
	aniphdr_t *aip;

	ip->ip_sum = 0;
	ip->ip_sum = chksum((u_short *)ip, ip->ip_hl << 2);
	free_anipheader();
	for (aip = aniphead, ip = NULL; aip; aip = aip->ah_next)
		if (aip->ah_p == IPPROTO_IP)
			ip = aip->ah_ip;
}


void end_icmp()
{
	aniphdr_t *aip;

	icmp->icmp_cksum = 0;
	icmp->icmp_cksum = chksum((u_short *)icmp, canip->ah_len);
	free_anipheader();
	for (aip = aniphead, icmp = NULL; aip; aip = aip->ah_next)
		if (aip->ah_p == IPPROTO_ICMP)
			icmp = aip->ah_icmp;
}


void end_udp()
{
	u_long	sum;
	aniphdr_t *aip;
	ip_t	iptmp;

	bzero((char *)&iptmp, sizeof(iptmp));
        iptmp.ip_p = ip->ip_p;
        iptmp.ip_src = ip->ip_src;
        iptmp.ip_dst = ip->ip_dst;
	iptmp.ip_len = ip->ip_len - (ip->ip_hl << 2);
	sum = p_chksum((u_short *)&iptmp, (u_int)sizeof(iptmp));
	udp->uh_sum = c_chksum((u_short *)udp, (u_int)iptmp.ip_len, sum);
	free_anipheader();
	for (aip = aniphead, udp = NULL; aip; aip = aip->ah_next)
		if (aip->ah_p == IPPROTO_UDP)
			udp = aip->ah_udp;
}


void end_tcp()
{
	u_long	sum;
	aniphdr_t *aip;
	ip_t	iptmp;

	bzero((char *)&iptmp, sizeof(iptmp));
        iptmp.ip_p = ip->ip_p;
        iptmp.ip_src = ip->ip_src;
        iptmp.ip_dst = ip->ip_dst;
	iptmp.ip_len = ip->ip_len - (ip->ip_hl << 2);
	sum = p_chksum((u_short *)&iptmp, (u_int)sizeof(iptmp));
	tcp->th_sum = 0;
	tcp->th_sum = c_chksum((u_short *)tcp, (u_int)iptmp.ip_len, sum);
	free_anipheader();
	for (aip = aniphead, tcp = NULL; aip; aip = aip->ah_next)
		if (aip->ah_p == IPPROTO_TCP)
			tcp = aip->ah_tcp;
}


void end_data()
{
	free_anipheader();
}


void	yyerror(msg)
char	*msg;
{
	fprintf(stderr, "%s error at \"%s\", line %d\n", msg, yytext,
		lineNum + 1);
	exit(1);
}


void iplang(fp)
FILE *fp;
{
	yyin = fp;

	while (!feof(fp))
		yyparse();
}


u_short	c_chksum(buf, len, init)
u_short	*buf;
u_int	len;
u_long	init;
{
	u_long	sum = init;
	int	nwords = len >> 1;
 
	for(; nwords > 0; nwords--)
		sum += *buf++;
	sum = (sum>>16) + (sum & 0xffff);
	sum += (sum >>16);
	return (~sum);
}


u_long	p_chksum(buf,len)
u_short	*buf;
u_int	len;
{
	u_long	sum = 0;
	int	nwords = len >> 1;
 
	for(; nwords > 0; nwords--)
		sum += *buf++;
	return sum;
}
