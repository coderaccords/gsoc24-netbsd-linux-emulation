/*	$NetBSD: ntp_io.c,v 1.5 2011/01/09 14:49:40 kardel Exp $	*/

/*
 * ntp_io.c - input/output routines for ntpd.	The socket-opening code
 *		   was shamelessly stolen from ntpd.
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <stdio.h>
#include <signal.h>
#ifdef HAVE_SYS_PARAM_H
# include <sys/param.h>
#endif
#ifdef HAVE_SYS_IOCTL_H
# include <sys/ioctl.h>
#endif
#ifdef HAVE_SYS_SOCKIO_H	/* UXPV: SIOC* #defines (Frank Vance <fvance@waii.com>) */
# include <sys/sockio.h>
#endif
#ifdef HAVE_SYS_UIO_H
# include <sys/uio.h>
#endif

#include "ntp_machine.h"
#include "ntpd.h"
#include "ntp_io.h"
#include "iosignal.h"
#include "ntp_lists.h"
#include "ntp_refclock.h"
#include "ntp_stdlib.h"
#include "ntp_request.h"
#include "ntp.h"
#include "ntp_unixtime.h"
#include "ntp_assert.h"
#include "ntpd-opts.h"

/* Don't include ISC's version of IPv6 variables and structures */
#define ISC_IPV6_H 1
#include <isc/mem.h>
#include <isc/interfaceiter.h>
#include <isc/netaddr.h>
#include <isc/result.h>
#include <isc/sockaddr.h>

#ifdef SIM
#include "ntpsim.h"
#endif

#ifdef HAS_ROUTING_SOCKET
# include <net/route.h>
# ifdef HAVE_RTNETLINK
#  include <linux/rtnetlink.h>
# endif
#endif


/*
 * setsockopt does not always have the same arg declaration
 * across all platforms. If it's not defined we make it empty
 */

#ifndef SETSOCKOPT_ARG_CAST
#define SETSOCKOPT_ARG_CAST
#endif

extern int listen_to_virtual_ips;

/*
 * NIC rule entry
 */
typedef struct nic_rule_tag nic_rule;

struct nic_rule_tag {
	nic_rule *	next;
	nic_rule_action	action;
	nic_rule_match	match_type;
	char *		if_name;
	isc_netaddr_t	netaddr;
	int		prefixlen;
};

/*
 * NIC rule listhead.  Entries are added at the head so that the first
 * match in the list is the last matching rule specified.
 */
nic_rule *nic_rule_list;


#if defined(SO_TIMESTAMP) && defined(SCM_TIMESTAMP)
#if defined(CMSG_FIRSTHDR)
#define HAVE_TIMESTAMP
#define USE_TIMESTAMP_CMSG
#ifndef TIMESTAMP_CTLMSGBUF_SIZE
#define TIMESTAMP_CTLMSGBUF_SIZE 1536 /* moderate default */
#endif
#else
/* fill in for old/other timestamp interfaces */
#endif
#endif

#if defined(SYS_WINNT)
#include <transmitbuff.h>
#include <isc/win32os.h>
/*
 * Windows C runtime ioctl() can't deal properly with sockets, 
 * map to ioctlsocket for this source file.
 */
#define ioctl(fd, opt, val)  ioctlsocket((fd), (opt), (u_long *)(val))
#endif  /* SYS_WINNT */

/*
 * We do asynchronous input using the SIGIO facility.  A number of
 * recvbuf buffers are preallocated for input.	In the signal
 * handler we poll to see which sockets are ready and read the
 * packets from them into the recvbuf's along with a time stamp and
 * an indication of the source host and the interface it was received
 * through.  This allows us to get as accurate receive time stamps
 * as possible independent of other processing going on.
 *
 * We watch the number of recvbufs available to the signal handler
 * and allocate more when this number drops below the low water
 * mark.  If the signal handler should run out of buffers in the
 * interim it will drop incoming frames, the idea being that it is
 * better to drop a packet than to be inaccurate.
 */


/*
 * Other statistics of possible interest
 */
volatile u_long packets_dropped;	/* total number of packets dropped on reception */
volatile u_long packets_ignored;	/* packets received on wild card interface */
volatile u_long packets_received;	/* total number of packets received */
	 u_long packets_sent;		/* total number of packets sent */
	 u_long packets_notsent;	/* total number of packets which couldn't be sent */

volatile u_long handler_calls;	/* number of calls to interrupt handler */
volatile u_long handler_pkts;	/* number of pkts received by handler */
u_long io_timereset;		/* time counters were reset */

/*
 * Interface stuff
 */
struct interface *any_interface;	/* default ipv4 interface */
struct interface *any6_interface;	/* default ipv6 interface */
struct interface *loopback_interface;	/* loopback ipv4 interface */

isc_boolean_t broadcast_client_enabled;	/* is broadcast client enabled */
int ninterfaces;			/* Total number of interfaces */

int disable_dynamic_updates;		/* scan interfaces once only */

#ifdef REFCLOCK
/*
 * Refclock stuff.	We keep a chain of structures with data concerning
 * the guys we are doing I/O for.
 */
static	struct refclockio *refio;
#endif /* REFCLOCK */

#if defined(HAVE_IPTOS_SUPPORT)
/* set IP_TOS to minimize packet delay */
# if defined(IPTOS_PREC_INTERNETCONTROL)
	unsigned int qos = IPTOS_PREC_INTERNETCONTROL;
# else
	 unsigned int qos = IPTOS_LOWDELAY;
# endif
#endif

/*
 * File descriptor masks etc. for call to select
 * Not needed for I/O Completion Ports
 */
fd_set activefds;
int maxactivefd;
/*
 * bit alternating value to detect verified interfaces during an update cycle
 */
static  u_short		sys_interphase = 0;

static struct interface *new_interface	(struct interface *);
static void		add_interface	(struct interface *);
static int		update_interfaces(u_short, interface_receiver_t, void *);
static void		remove_interface(struct interface *);
static struct interface *create_interface(u_short, struct interface *);

static int	move_fd			(SOCKET);
static int	is_wildcard_addr	(sockaddr_u *);
static int	is_wildcard_netaddr	(const isc_netaddr_t *);

/*
 * Multicast functions
 */
static	isc_boolean_t	addr_ismulticast	(sockaddr_u *);
/*
 * Not all platforms support multicast
 */
#ifdef MCAST
static	isc_boolean_t	socket_multicast_enable	(struct interface *, int, sockaddr_u *);
static	isc_boolean_t	socket_multicast_disable(struct interface *, sockaddr_u *);
#endif

#ifdef DEBUG
static void interface_dump	(struct interface *);
static void sockaddr_dump	(sockaddr_u *psau);
static void print_interface	(struct interface *, const char *, const char *);
#define DPRINT_INTERFACE(level, args) do { if (debug >= (level)) { print_interface args; } } while (0)
#else
#define DPRINT_INTERFACE(level, args) do {} while (0)
#endif

typedef struct vsock vsock_t;
enum desc_type { FD_TYPE_SOCKET, FD_TYPE_FILE };

struct vsock {
	vsock_t	*	link;
	SOCKET		fd;
	enum desc_type	type;
};

vsock_t	*fd_list;

#if !defined(HAVE_IO_COMPLETION_PORT) && defined(HAS_ROUTING_SOCKET)
/*
 * async notification processing (e. g. routing sockets)
 */
/*
 * support for receiving data on fd that is not a refclock or a socket
 * like e. g. routing sockets
 */
struct asyncio_reader {
	struct asyncio_reader *link;		    /* the list this is being kept in */
	SOCKET fd;				    /* fd to be read */
	void  *data;				    /* possibly local data */
	void (*receiver)(struct asyncio_reader *);  /* input handler */
};

struct asyncio_reader *asyncio_reader_list;

static void delete_asyncio_reader (struct asyncio_reader *);
static struct asyncio_reader *new_asyncio_reader (void);
static void add_asyncio_reader (struct asyncio_reader *, enum desc_type);
static void remove_asyncio_reader (struct asyncio_reader *);

#endif /* !defined(HAVE_IO_COMPLETION_PORT) && defined(HAS_ROUTING_SOCKET) */

static void init_async_notifications (void);

static	int create_sockets	(u_short);
static	SOCKET	open_socket	(sockaddr_u *, int, int, struct interface *);
static	char *	fdbits		(int, fd_set *);
static	void	set_reuseaddr	(int);
static	isc_boolean_t	socket_broadcast_enable	 (struct interface *, SOCKET, sockaddr_u *);
static	isc_boolean_t	socket_broadcast_disable (struct interface *, sockaddr_u *);

typedef struct remaddr remaddr_t;

struct remaddr {
	remaddr_t *		link;
	sockaddr_u		addr;
	struct interface *	interface;
};

remaddr_t *		remoteaddr_list;

struct interface *	inter_list;

static struct interface *wildipv4 = NULL;
static struct interface *wildipv6 = NULL;

static void		add_fd_to_list		(SOCKET, 
						 enum desc_type);
static struct interface *find_addr_in_list	(sockaddr_u *);
static struct interface *find_samenet_addr_in_list(sockaddr_u *);
static struct interface *find_flagged_addr_in_list(sockaddr_u *, int);
static void		delete_addr_from_list	(sockaddr_u *);
static void		delete_interface_from_list(struct interface *);
static void		close_and_delete_fd_from_list(SOCKET);
static void		add_addr_to_list	(sockaddr_u *, 
						 struct interface *);
static void		create_wildcards	(u_short);
#ifdef DEBUG
static const char *	action_text		(nic_rule_action);
#endif
static nic_rule_action	interface_action(char *, isc_netaddr_t *,
					 isc_uint32_t);
static void		convert_isc_if	(isc_interface_t *,
					 struct interface *, u_short);
static struct interface *getinterface	(sockaddr_u *, int);
static struct interface *getsamenetinterface	(sockaddr_u *, int);
static struct interface *findlocalinterface	(sockaddr_u *, int, int);
static struct interface *findlocalcastinterface	(sockaddr_u *);

/*
 * Routines to read the ntp packets
 */
#if !defined(HAVE_IO_COMPLETION_PORT)
static inline int     read_network_packet	(SOCKET, struct interface *, l_fp);
static inline int     read_refclock_packet	(SOCKET, struct refclockio *, l_fp);
#endif


#ifdef SYS_WINNT
/*
 * Windows 2000 systems incorrectly cause UDP sockets using WASRecvFrom
 * to not work correctly, returning a WSACONNRESET error when a WSASendTo
 * fails with an "ICMP port unreachable" response and preventing the
 * socket from using the WSARecvFrom in subsequent operations.
 * The function below fixes this, but requires that Windows 2000
 * Service Pack 2 or later be installed on the system.  NT 4.0
 * systems are not affected by this and work correctly.
 * See Microsoft Knowledge Base Article Q263823 for details of this.
 */
void
connection_reset_fix(
	SOCKET		fd,
	sockaddr_u *	addr
	)
{
	DWORD dw;
	BOOL  bNewBehavior = FALSE;
	DWORD status;

	/*
	 * disable bad behavior using IOCTL: SIO_UDP_CONNRESET
	 * NT 4.0 has no problem
	 */
	if (isc_win32os_majorversion() >= 5) {
		status = WSAIoctl(fd, SIO_UDP_CONNRESET, &bNewBehavior,
				  sizeof(bNewBehavior), NULL, 0,
				  &dw, NULL, NULL);
		if (SOCKET_ERROR == status)
			msyslog(LOG_ERR,
				"connection_reset_fix() failed for address %s: %m", 
				stoa(addr));
	}
}
#endif

/*
 * on Unix systems the stdio library typically
 * makes use of file descriptors in the lower
 * integer range.  stdio usually will make use
 * of the file descriptors in the range of
 * [0..FOPEN_MAX)
 * in order to keep this range clean, for socket
 * file descriptors we attempt to move them above
 * FOPEN_MAX. This is not as easy as it sounds as
 * FOPEN_MAX changes from implementation to implementation
 * and may exceed to current file decriptor limits.
 * We are using following strategy:
 * - keep a current socket fd boundary initialized with
 *   max(0, min(getdtablesize() - FD_CHUNK, FOPEN_MAX))
 * - attempt to move the descriptor to the boundary or
 *   above.
 *   - if that fails and boundary > 0 set boundary
 *     to min(0, socket_fd_boundary - FD_CHUNK)
 *     -> retry
 *     if failure and boundary == 0 return old fd
 *   - on success close old fd return new fd
 *
 * effects:
 *   - fds will be moved above the socket fd boundary
 *     if at all possible.
 *   - the socket boundary will be reduced until
 *     allocation is possible or 0 is reached - at this
 *     point the algrithm will be disabled
 */
static int
move_fd(
	SOCKET fd
	)
{
#if !defined(SYS_WINNT) && defined(F_DUPFD)
#ifndef FD_CHUNK
#define FD_CHUNK	10
#endif
/*
 * number of fds we would like to have for
 * stdio FILE* available.
 * we can pick a "low" number as our use of
 * FILE* is limited to log files and temporarily
 * to data and config files. Except for log files
 * we don't keep the other FILE* open beyond the
 * scope of the function that opened it.
 */
#ifndef FD_PREFERRED_SOCKBOUNDARY
#define FD_PREFERRED_SOCKBOUNDARY 48
#endif

#ifndef HAVE_GETDTABLESIZE
/*
 * if we have no idea about the max fd value set up things
 * so we will start at FOPEN_MAX
 */
#define getdtablesize() (FOPEN_MAX+FD_CHUNK)
#endif

#ifndef FOPEN_MAX
#define FOPEN_MAX	20	/* assume that for the lack of anything better */
#endif
	static SOCKET socket_boundary = -1;
	SOCKET newfd;

	NTP_REQUIRE((int)fd >= 0);

	/*
	 * check whether boundary has be set up
	 * already
	 */
	if (socket_boundary == -1) {
		socket_boundary = max(0, min(getdtablesize() - FD_CHUNK, 
					     min(FOPEN_MAX, FD_PREFERRED_SOCKBOUNDARY)));
#ifdef DEBUG
		msyslog(LOG_DEBUG,
			"ntp_io: estimated max descriptors: %d, initial socket boundary: %d",
			getdtablesize(), socket_boundary);
#endif
	}

	/*
	 * Leave a space for stdio to work in. potentially moving the
	 * socket_boundary lower until allocation succeeds.
	 */
	do {
		if (fd >= 0 && fd < socket_boundary) {
			/* inside reserved range: attempt to move fd */
			newfd = fcntl(fd, F_DUPFD, socket_boundary);
			
			if (newfd != -1) {
				/* success: drop the old one - return the new one */
				close(fd);
				return newfd;
			}
		} else {
			/* outside reserved range: no work - return the original one */
			return fd;
		}
		socket_boundary = max(0, socket_boundary - FD_CHUNK);
#ifdef DEBUG
		msyslog(LOG_DEBUG,
			"ntp_io: selecting new socket boundary: %d",
			socket_boundary);
#endif
	} while (socket_boundary > 0);
#else
	NTP_REQUIRE((int)fd >= 0);
#endif /* !defined(SYS_WINNT) && defined(F_DUPFD) */
	return fd;
}

#ifdef DEBUG_TIMING
/*
 * collect timing information for various processing
 * paths. currently we only pass then on to the file
 * for later processing. this could also do histogram
 * based analysis in other to reduce the load (and skew)
 * dur to the file output
 */
void
collect_timing(struct recvbuf *rb, const char *tag, int count, l_fp *dts)
{
	char buf[256];

	snprintf(buf, sizeof(buf), "%s %d %s %s", 
		 (rb != NULL)
		     ? ((rb->dstadr != NULL)
			    ? stoa(&rb->recv_srcadr)
			    : "-REFCLOCK-")
		     : "-",
		 count, lfptoa(dts, 9), tag);
	record_timing_stats(buf);
}
#endif

/*
 * About dynamic interfaces, sockets, reception and more...
 *
 * the code solves following tasks:
 *
 *   - keep a current list of active interfaces in order
 *     to bind to to the interface address on NTP_PORT so that
 *     all wild and specific bindings for NTP_PORT are taken by ntpd
 *     to avoid other daemons messing with the time or sockets.
 *   - all interfaces keep a list of peers that are referencing 
 *     the interface in order to quickly re-assign the peers to
 *     new interface in case an interface is deleted (=> gone from system or
 *     down)
 *   - have a preconfigured socket ready with the right local address
 *     for transmission and reception
 *   - have an address list for all destination addresses used within ntpd
 *     to find the "right" preconfigured socket.
 *   - facilitate updating the internal interface list with respect to
 *     the current kernel state
 *
 * special issues:
 *
 *   - mapping of multicast addresses to the interface affected is not always
 *     one to one - especially on hosts with multiple interfaces
 *     the code here currently allocates a separate interface entry for those
 *     multicast addresses
 *     iff it is able to bind to a *new* socket with the multicast address (flags |= MCASTIF)
 *     in case of failure the multicast address is bound to an existing interface.
 *   - on some systems it is perfectly legal to assign the same address to
 *     multiple interfaces. Therefore this code does not keep a list of interfaces
 *     but a list of interfaces that represent a unique address as determined by the kernel
 *     by the procedure in findlocalinterface. Thus it is perfectly legal to see only
 *     one representative of a group of real interfaces if they share the same address.
 * 
 * Frank Kardel 20050910
 */

/*
 * init_io - initialize I/O data structures and call socket creation routine
 */
void
init_io(void)
{
	/*
	 * Init buffer free list and stat counters
	 */
	init_recvbuff(RECV_INIT);

#ifdef SYS_WINNT
	init_io_completion_port();
#endif /* SYS_WINNT */

#if defined(HAVE_SIGNALED_IO)
	(void) set_signal();
#endif
}


/*
 * io_open_sockets - call socket creation routine
 */
void
io_open_sockets(void)
{
	static int already_opened;

	if (already_opened || HAVE_OPT( SAVECONFIGQUIT ))
		return;

	already_opened = 1;

	/*
	 * Create the sockets
	 */
	BLOCKIO();
	create_sockets(NTP_PORT);
	UNBLOCKIO();

	init_async_notifications();

	DPRINTF(3, ("io_open_sockets: maxactivefd %d\n", maxactivefd));
}


#ifdef DEBUG
/*
 * function to dump the contents of the interface structure
 * for debugging use only.
 */
void
interface_dump(struct interface *itf)
{
	printf("Dumping interface: %p\n", itf);
	printf("fd = %d\n", itf->fd);
	printf("bfd = %d\n", itf->bfd);
	printf("sin = %s,\n", stoa(&itf->sin));
	sockaddr_dump(&itf->sin);
	printf("bcast = %s,\n", stoa(&itf->bcast));
	sockaddr_dump(&itf->bcast);
	printf("mask = %s,\n", stoa(&itf->mask));
	sockaddr_dump(&itf->mask);
	printf("name = %s\n", itf->name);
	printf("flags = 0x%08x\n", itf->flags);
	printf("last_ttl = %d\n", itf->last_ttl);
	printf("addr_refid = %08x\n", itf->addr_refid);
	printf("num_mcast = %d\n", itf->num_mcast);
	printf("received = %ld\n", itf->received);
	printf("sent = %ld\n", itf->sent);
	printf("notsent = %ld\n", itf->notsent);
	printf("scopeid = %u\n", itf->scopeid);
	printf("peercnt = %u\n", itf->peercnt);
	printf("phase = %u\n", itf->phase);
}

/*
 * sockaddr_dump - hex dump the start of a sockaddr_u
 */
static void
sockaddr_dump(sockaddr_u *psau)
{
	/* Limit the size of the sockaddr_storage hex dump */
	const int maxsize = min(32, sizeof(psau->sas));
	u_char *	cp;
	int		i;

	cp = (u_char *)&psau->sas;

	for(i = 0; i < maxsize; i++)
	{
		printf("%02x", *cp++);
		if (!((i + 1) % 4))
			printf(" ");
	}
	printf("\n");
}

/*
 * print_interface - helper to output debug information
 */
static void
print_interface(struct interface *iface, const char *pfx, const char *sfx)
{
	printf("%sinterface #%d: fd=%d, bfd=%d, name=%s, flags=0x%x, scope=%d, sin=%s",
	       pfx,
	       iface->ifnum,
	       iface->fd,
	       iface->bfd,
	       iface->name,
	       iface->flags,
	       iface->scopeid,
	       stoa(&iface->sin));
	if (AF_INET == iface->family) {
		if (iface->flags & INT_BROADCAST)
			printf(", bcast=%s", stoa(&iface->bcast));
		printf(", mask=%s", stoa(&iface->mask));
	}
	printf(", %s:%s",
	       (iface->ignore_packets) 
		   ? "Disabled"
		   : "Enabled",
	       sfx);
	if (debug > 4)	/* in-depth debugging only */
		interface_dump(iface);
}
#endif

#if !defined(HAVE_IO_COMPLETION_PORT) && defined(HAS_ROUTING_SOCKET)
/*
 * create an asyncio_reader structure
 */
static struct asyncio_reader *
new_asyncio_reader(void)
{
	struct asyncio_reader *reader;

	reader = emalloc(sizeof(*reader));

	memset(reader, 0, sizeof(*reader));
	reader->fd = INVALID_SOCKET;
	return reader;
}

/*
 * delete a reader
 */
static void
delete_asyncio_reader(
	struct asyncio_reader *reader
	)
{
	free(reader);
}

/*
 * add asynchio_reader
 */
static void
add_asyncio_reader(
	struct asyncio_reader *	reader, 
	enum desc_type		type)
{
	LINK_SLIST(asyncio_reader_list, reader, link);
	add_fd_to_list(reader->fd, type);
}
	
/*
 * remove asynchio_reader
 */
static void
remove_asyncio_reader(
	struct asyncio_reader *reader
	)
{
	struct asyncio_reader *unlinked;

	UNLINK_SLIST(unlinked, asyncio_reader_list, reader, link,
	    struct asyncio_reader);

	if (reader->fd != INVALID_SOCKET)
		close_and_delete_fd_from_list(reader->fd);

	reader->fd = INVALID_SOCKET;
}
#endif /* !defined(HAVE_IO_COMPLETION_PORT) && defined(HAS_ROUTING_SOCKET) */

/*
 * Code to tell if we have an IP address
 * If we have then return the sockaddr structure
 * and set the return value
 * see the bind9/getaddresses.c for details
 */
isc_boolean_t
is_ip_address(
	const char *	host,
	isc_netaddr_t *	addr
	)
{
	struct in_addr in4;
	struct in6_addr in6;
	char tmpbuf[128];
	char *pch;

	NTP_REQUIRE(host != NULL);
	NTP_REQUIRE(addr != NULL);

	/*
	 * Try IPv4, then IPv6.  In order to handle the extended format
	 * for IPv6 scoped addresses (address%scope_ID), we'll use a local
	 * working buffer of 128 bytes.  The length is an ad-hoc value, but
	 * should be enough for this purpose; the buffer can contain a string
	 * of at least 80 bytes for scope_ID in addition to any IPv6 numeric
	 * addresses (up to 46 bytes), the delimiter character and the
	 * terminating NULL character.
	 */
	if (inet_pton(AF_INET, host, &in4) == 1) {
		isc_netaddr_fromin(addr, &in4);
		return (ISC_TRUE);
	} else if (sizeof(tmpbuf) > strlen(host)) {
		if ('[' == host[0]) {
			strncpy(tmpbuf, &host[1], sizeof(tmpbuf));
			pch = strchr(tmpbuf, ']');
			if (pch != NULL)
				*pch = '\0';
		} else
			strncpy(tmpbuf, host, sizeof(tmpbuf));
		pch = strchr(tmpbuf, '%');
		if (pch != NULL)
			*pch = '\0';

		if (inet_pton(AF_INET6, tmpbuf, &in6) == 1) {
			isc_netaddr_fromin6(addr, &in6);
			return (ISC_TRUE);
		}
	}
	/*
	 * If we got here it was not an IP address
	 */
	return (ISC_FALSE);
}


/*
 * interface list enumerator - visitor pattern
 */
void
interface_enumerate(
	interface_receiver_t	receiver, 
	void *			data
	)
{
	interface_info_t ifi;

	ifi.action = IFS_EXISTS;
	
	for (ifi.interface = inter_list;
	     ifi.interface != NULL;
	     ifi.interface = ifi.interface->link)
		(*receiver)(data, &ifi);
}

/*
 * do standard initialization of interface structure
 */
static void
init_interface(
	struct interface *iface
	)
{
	memset(iface, 0, sizeof(*iface));
	iface->fd = INVALID_SOCKET;
	iface->bfd = INVALID_SOCKET;
	iface->phase = sys_interphase;
}


/*
 * create new interface structure initialize from
 * template structure or via standard initialization
 * function
 */
static struct interface *
new_interface(
	struct interface *interface
	)
{
	static u_int		sys_ifnum = 0;
	struct interface *	iface;

	iface = emalloc(sizeof(*iface));

	if (NULL == interface)
		init_interface(iface);
	else				/* use the template */
		memcpy(iface, interface, sizeof(*iface));

	/* count every new instance of an interface in the system */
	iface->ifnum = sys_ifnum++;
	iface->starttime = current_time;

	return iface;
}


/*
 * return interface storage into free memory pool
 */
static inline void
delete_interface(
	struct interface *interface
	)
{
	free(interface);
}


/*
 * link interface into list of known interfaces
 */
static void
add_interface(
	struct interface *interface
	)
{
	/*
	 * Calculate the address hash
	 */
	interface->addr_refid = addr2refid(&interface->sin);

	LINK_SLIST(inter_list, interface, link);
	ninterfaces++;
}


/*
 * remove interface from known interface list and clean up
 * associated resources
 */
static void
remove_interface(
	struct interface *iface
	)
{
	struct interface *unlinked;
	sockaddr_u resmask;

	UNLINK_SLIST(unlinked, inter_list, iface, link, struct
	    interface);

	delete_interface_from_list(iface);
  
	if (iface->fd != INVALID_SOCKET) {
		msyslog(LOG_INFO,
			"Deleting interface #%d %s, %s#%d, interface stats: received=%ld, sent=%ld, dropped=%ld, active_time=%ld secs",
			iface->ifnum,
			iface->name,
			stoa(&iface->sin),
			SRCPORT(&iface->sin),
			iface->received,
			iface->sent,
			iface->notsent,
			current_time - iface->starttime);

		close_and_delete_fd_from_list(iface->fd);
	}

	if (iface->bfd != INVALID_SOCKET) {
		msyslog(LOG_INFO,
			"Deleting broadcast address %s#%d from interface #%d %s",
			stoa(&iface->bcast),
			SRCPORT(&iface->bcast),
			iface->ifnum,
			iface->name);

		close_and_delete_fd_from_list(iface->bfd);
	}

	ninterfaces--;
	ntp_monclearinterface(iface);

	/* remove restrict interface entry */
	SET_HOSTMASK(&resmask, AF(&iface->sin));
	hack_restrict(RESTRICT_REMOVEIF, &iface->sin, &resmask,
		      RESM_NTPONLY | RESM_INTERFACE, RES_IGNORE);
}


static void
list_if_listening(
	struct interface *	iface
	)
{
	msyslog(LOG_INFO, "%s on %d %s %s UDP %d",
		(iface->ignore_packets) 
		    ? "Listen and drop"
		    : "Listen normally",
		iface->ifnum,
		iface->name,
		stoa(&iface->sin),
		SRCPORT(&iface->sin));
}


static void
create_wildcards(
	u_short	port
	)
{
	int			v4wild;
#ifdef INCLUDE_IPV6_SUPPORT
	int                     v6wild;
#endif
	sockaddr_u		wildaddr;
	isc_netaddr_t		wnaddr;
	nic_rule_action		action;
	struct interface *	wildif;

	/*
	 * silence "potentially uninitialized" warnings from VC9
	 * failing to follow the logic.  Ideally action could remain
	 * uninitialized, and the memset be the first statement under
	 * the first if (v4wild).
	 */
	action = ACTION_LISTEN;
	memset(&wildaddr, 0, sizeof(wildaddr));
	
	/*
	 * create pseudo-interface with wildcard IPv4 address
	 */
	v4wild = ipv4_works;
	if (v4wild) {
		/* set wildaddr to the v4 wildcard address 0.0.0.0 */
		AF(&wildaddr) = AF_INET;
		SET_ADDR4(&wildaddr, INADDR_ANY);
		SET_PORT(&wildaddr, port);

		/* make an libisc-friendly copy */
		isc_netaddr_fromin(&wnaddr, &wildaddr.sa4.sin_addr);

		/* check for interface/nic rules affecting the wildcard */
		action = interface_action(NULL, &wnaddr, 0);
		v4wild = (ACTION_IGNORE != action);
	}
	if (v4wild) {
		wildif = new_interface(NULL);

		strncpy(wildif->name, "v4wildcard", sizeof(wildif->name));
		memcpy(&wildif->sin, &wildaddr, sizeof(wildif->sin));
		wildif->family = AF_INET;
		AF(&wildif->mask) = AF_INET;
		SET_ONESMASK(&wildif->mask);

		wildif->flags = INT_BROADCAST | INT_UP | INT_WILDCARD;
		wildif->ignore_packets = (ACTION_DROP == action);
#if defined(MCAST)
		/*
		 * enable multicast reception on the broadcast socket
		 */
		AF(&wildif->bcast) = AF_INET;
		SET_ADDR4(&wildif->bcast, INADDR_ANY);
		SET_PORT(&wildif->bcast, port);
#endif /* MCAST */
		wildif->fd = open_socket(&wildif->sin, 0, 1, wildif);

		if (wildif->fd != INVALID_SOCKET) {
			wildipv4 = wildif;
			any_interface = wildif;
			
			add_addr_to_list(&wildif->sin, wildif);
			add_interface(wildif);
			list_if_listening(wildif);
		} else {
			msyslog(LOG_ERR, 
				"unable to bind to wildcard address %s - another process may be running - EXITING",
				stoa(&wildif->sin));
			exit(1);
		}
		DPRINT_INTERFACE(2, (wildif, "created ", "\n"));
	}

#ifdef INCLUDE_IPV6_SUPPORT
	/*
	 * create pseudo-interface with wildcard IPv6 address
	 */
	v6wild = ipv6_works;
	if (v6wild) {
		/* set wildaddr to the v6 wildcard address :: */
		memset(&wildaddr, 0, sizeof(wildaddr));
		AF(&wildaddr) = AF_INET6;
		SET_ADDR6N(&wildaddr, in6addr_any);
		SET_PORT(&wildaddr, port);
		SET_SCOPE(&wildaddr, 0);

		/* make an libisc-friendly copy */
		isc_netaddr_fromin(&wnaddr, &wildaddr.sa4.sin_addr);

		/* check for interface/nic rules affecting the wildcard */
		action = interface_action(NULL, &wnaddr, 0);
		v6wild = (ACTION_IGNORE != action);
	}
	if (v6wild) {
		wildif = new_interface(NULL);

		strncpy(wildif->name, "v6wildcard", sizeof(wildif->name));
		memcpy(&wildif->sin, &wildaddr, sizeof(wildif->sin));
		wildif->family = AF_INET6;
		AF(&wildif->mask) = AF_INET6;
		SET_ONESMASK(&wildif->mask);

		wildif->flags = INT_UP | INT_WILDCARD;
		wildif->ignore_packets = (ACTION_DROP == action);

		wildif->fd = open_socket(&wildif->sin, 0, 1, wildif);

		if (wildif->fd != INVALID_SOCKET) {
			wildipv6 = wildif;
			any6_interface = wildif;
			add_addr_to_list(&wildif->sin, wildif);
			add_interface(wildif);
			list_if_listening(wildif);
		} else {
			msyslog(LOG_ERR,
				"unable to bind to wildcard address %s - another process may be running - EXITING",
				stoa(&wildif->sin));
			exit(1);
		}
		DPRINT_INTERFACE(2, (wildif, "created ", "\n"));
	}
#endif
}


/*
 * add_nic_rule() -- insert a rule entry at the head of nic_rule_list.
 */
void
add_nic_rule(
	nic_rule_match	match_type,
	const char *	if_name,	/* interface name or numeric address */
	int		prefixlen,
	nic_rule_action	action
	)
{
	nic_rule *	rule;
	isc_boolean_t	is_ip;

	rule = emalloc(sizeof(*rule));
	memset(rule, 0, sizeof(*rule));
	rule->match_type = match_type;
	rule->prefixlen = prefixlen;
	rule->action = action;
	
	if (MATCH_IFNAME == match_type) {
		NTP_REQUIRE(NULL != if_name);
		rule->if_name = estrdup(if_name);
	} else if (MATCH_IFADDR == match_type) {
		NTP_REQUIRE(NULL != if_name);
		/* set rule->netaddr */
		is_ip = is_ip_address(if_name, &rule->netaddr);
		NTP_REQUIRE(is_ip);
	} else
		NTP_REQUIRE(NULL == if_name);

	LINK_SLIST(nic_rule_list, rule, next);
}


#ifdef DEBUG
static const char *
action_text(
	nic_rule_action	action
	)
{
	const char *t;

	switch (action) {

	default:
		t = "ERROR";	/* quiet uninit warning */
		DPRINTF(1, ("fatal: unknown nic_rule_action %d\n",
			    action));
		NTP_ENSURE(0);
		break;

	case ACTION_LISTEN:
		t = "listen";
		break;

	case ACTION_IGNORE:
		t = "ignore";
		break;

	case ACTION_DROP:
		t = "drop";
		break;
	}

	return t;
}
#endif	/* DEBUG */


static nic_rule_action
interface_action(
	char *		if_name,
	isc_netaddr_t *	if_netaddr,
	isc_uint32_t	if_flags
	)
{
	nic_rule *rule;
	int isloopback;
	int iswildcard;

	DPRINTF(4, ("interface_action: interface %s ",
		    (if_name != NULL) ? if_name : "wildcard"));

	iswildcard = is_wildcard_netaddr(if_netaddr);

	/*
	 * Always listen on 127.0.0.1 - required by ntp_intres
	 */
	if (if_flags & INTERFACE_F_LOOPBACK) {
		isloopback = 1;
		if (AF_INET == if_netaddr->family) {
			DPRINTF(4, ("IPv4 loopback - listen\n"));
			return ACTION_LISTEN;
		}
	} else
		isloopback = 0;

	/*
	 * Find any matching NIC rule from --interface / -I or ntp.conf
	 * interface/nic rules.
	 */
	for (rule = nic_rule_list; rule != NULL; rule = rule->next) {

		switch (rule->match_type) {

		case MATCH_ALL:
			/* loopback and wildcard excluded from "all" */
			if (isloopback || iswildcard)
				break;
			DPRINTF(4, ("nic all %s\n",
			    action_text(rule->action)));
			return rule->action;

		case MATCH_IPV4:
			if (AF_INET == if_netaddr->family) {
				DPRINTF(4, ("nic ipv4 %s\n",
				    action_text(rule->action)));
				return rule->action;
			}
			break;

		case MATCH_IPV6:
			if (AF_INET6 == if_netaddr->family) {
				DPRINTF(4, ("nic ipv6 %s\n",
				    action_text(rule->action)));
				return rule->action;
			}
			break;

		case MATCH_WILDCARD:
			if (iswildcard) {
				DPRINTF(4, ("nic wildcard %s\n",
				    action_text(rule->action)));
				return rule->action;
			}
			break;

		case MATCH_IFADDR:
			if (rule->prefixlen != -1) {
				if (isc_netaddr_eqprefix(if_netaddr,
				    &rule->netaddr, rule->prefixlen)) {

					DPRINTF(4, ("subnet address match - %s\n",
					    action_text(rule->action)));
					return rule->action;
				}
			} else
				if (isc_netaddr_equal(if_netaddr,
				    &rule->netaddr)) {

					DPRINTF(4, ("address match - %s\n",
					    action_text(rule->action)));
					return rule->action;
				}
			break;

		case MATCH_IFNAME:
			if (if_name != NULL
			    && !strcasecmp(if_name, rule->if_name)) {

				DPRINTF(4, ("interface name match - %s\n",
				    action_text(rule->action)));
				return rule->action;
			}
			break;
		}
	}

	/*
	 * Unless explicitly disabled such as with "nic ignore ::1"
	 * listen on loopback addresses.  Since ntpq and ntpdc query
	 * "localhost" by default, which typically resolves to ::1 and
	 * 127.0.0.1, it's useful to default to listening on both.
	 */
	if (isloopback) {
		DPRINTF(4, ("default loopback listen\n"));
		return ACTION_LISTEN;
	}

	/*
	 * Treat wildcard addresses specially.  If there is no explicit
	 * "nic ... wildcard" or "nic ... 0.0.0.0" or "nic ... ::" rule
	 * default to drop.
	 */
	if (iswildcard) {
		DPRINTF(4, ("default wildcard drop\n"));
		return ACTION_DROP;
	}

	/*
	 * Check for "virtual IP" (colon in the interface name) after
	 * the rules so that "ntpd --interface eth0:1 -novirtualips"
	 * does indeed listen on eth0:1's addresses.
	 */
	if (!listen_to_virtual_ips && if_name != NULL
	    && (strchr(if_name, ':') != NULL)) {

		DPRINTF(4, ("virtual ip - ignore\n"));
		return ACTION_IGNORE;
	}

	/*
	 * If there are no --interface/-I command-line options and no
	 * interface/nic rules in ntp.conf, the default action is to
	 * listen.  In the presence of rules from either, the default
	 * is to ignore.  This implements ntpd's traditional listen-
	 * every default with no interface listen configuration, and
	 * ensures a single -I eth0 or "nic listen eth0" means do not
	 * listen on any other addresses.
	 */
	if (NULL == nic_rule_list) {
		DPRINTF(4, ("default listen\n"));
		return ACTION_LISTEN;
	}

	DPRINTF(4, ("implicit ignore\n"));
	return ACTION_IGNORE;
}


static void
convert_isc_if(
	isc_interface_t *isc_if,
	struct interface *itf,
	u_short port
	)
{
	strncpy(itf->name, isc_if->name, sizeof(itf->name));
	itf->name[sizeof(itf->name) - 1] = 0; /* strncpy may not */
	itf->family = (u_short)isc_if->af;
	AF(&itf->sin) = itf->family;
	AF(&itf->mask) = itf->family;
	AF(&itf->bcast) = itf->family;
	SET_PORT(&itf->sin, port);
	SET_PORT(&itf->mask, port);
	SET_PORT(&itf->bcast, port);

	if (IS_IPV4(&itf->sin)) {
		NSRCADR(&itf->sin) = isc_if->address.type.in.s_addr;
		NSRCADR(&itf->mask) = isc_if->netmask.type.in.s_addr;

		if (isc_if->flags & INTERFACE_F_BROADCAST) {
			itf->flags |= INT_BROADCAST;
			NSRCADR(&itf->bcast) = 
			    isc_if->broadcast.type.in.s_addr;
		}
	}
#ifdef INCLUDE_IPV6_SUPPORT
	else if (IS_IPV6(&itf->sin)) {
		SET_ADDR6N(&itf->sin, isc_if->address.type.in6);
		SET_ADDR6N(&itf->mask, isc_if->netmask.type.in6);

		itf->scopeid = isc_netaddr_getzone(&isc_if->address);
		SET_SCOPE(&itf->sin, itf->scopeid);
	}
#endif /* INCLUDE_IPV6_SUPPORT */


	/* Process the rest of the flags */

	itf->flags |=
		  ((INTERFACE_F_UP & isc_if->flags)
			 ? INT_UP : 0)
		| ((INTERFACE_F_LOOPBACK & isc_if->flags) 
			 ? INT_LOOPBACK : 0)
		| ((INTERFACE_F_POINTTOPOINT & isc_if->flags) 
			 ? INT_PPP : 0)
		| ((INTERFACE_F_MULTICAST & isc_if->flags) 
			 ? INT_MULTICAST : 0)
		;
}


/*
 * refresh_interface
 *
 * some OSes have been observed to keep
 * cached routes even when more specific routes
 * become available.
 * this can be mitigated by re-binding
 * the socket.
 */
static int
refresh_interface(
	struct interface * interface
	)
{
#ifdef  OS_MISSES_SPECIFIC_ROUTE_UPDATES
	if (interface->fd != INVALID_SOCKET) {
		close_and_delete_fd_from_list(interface->fd);
		interface->fd = open_socket(&interface->sin,
					    0, 0, interface);
		 /*
		  * reset TTL indication so TTL is is set again 
		  * next time around
		  */
		interface->last_ttl = 0;
		return (interface->fd != INVALID_SOCKET);
	} else
		return 0;	/* invalid sockets are not refreshable */
#else /* !OS_MISSES_SPECIFIC_ROUTE_UPDATES */
	return (interface->fd != INVALID_SOCKET);
#endif /* !OS_MISSES_SPECIFIC_ROUTE_UPDATES */
}

/*
 * interface_update - externally callable update function
 */
void
interface_update(
	interface_receiver_t	receiver,
	void *			data)
{
	int new_interface_found;

	if (disable_dynamic_updates)
		return;

	BLOCKIO();
	new_interface_found = update_interfaces(NTP_PORT, receiver, data);
	UNBLOCKIO();

	if (!new_interface_found)
		return;

#ifdef DEBUG
	msyslog(LOG_DEBUG, "new interface(s) found: waking up resolver");
#endif
#ifdef SYS_WINNT
	/* wake up the resolver thread */
	if (ResolverEventHandle != NULL)
		SetEvent(ResolverEventHandle);
#else
	/* write any single byte to the pipe to wake up the resolver process */
	write( resolver_pipe_fd[1], &new_interface_found, 1 );
#endif
}


/*
 * sau_from_netaddr() - convert network address on-wire formats.
 * Convert from libisc's isc_netaddr_t to NTP's sockaddr_u
 */
void
sau_from_netaddr(
	sockaddr_u *psau,
	const isc_netaddr_t *pna
	)
{
	memset(psau, 0, sizeof(*psau));
	AF(psau) = (u_short)pna->family;
	switch (pna->family) {

	case AF_INET:
		memcpy(&psau->sa4.sin_addr, &pna->type.in,
		       sizeof(psau->sa4.sin_addr));
		break;

	case AF_INET6:
		memcpy(&psau->sa6.sin6_addr, &pna->type.in6,
		       sizeof(psau->sa6.sin6_addr));
		break;
	}
}


static int
is_wildcard_addr(
	sockaddr_u *psau
	)
{
	if (IS_IPV4(psau) && !NSRCADR(psau))
		return 1;

#ifdef INCLUDE_IPV6_SUPPORT
	if (IS_IPV6(psau) && S_ADDR6_EQ(psau, &in6addr_any))
		return 1;
#endif

	return 0;
}


static int
is_wildcard_netaddr(
	const isc_netaddr_t *pna
	)
{
	sockaddr_u sau;

	sau_from_netaddr(&sau, pna);

	return is_wildcard_addr(&sau);
}


#ifdef OS_NEEDS_REUSEADDR_FOR_IFADDRBIND
/*
 * enable/disable re-use of wildcard address socket
 */
static void
set_wildcard_reuse(
	u_short	family,
	int	on
	)
{
	struct interface *any;
	SOCKET fd = INVALID_SOCKET;

	any = ANY_INTERFACE_BYFAM(family);
	if (any != NULL)
		fd = any->fd;

	if (fd != INVALID_SOCKET) {
		if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR,
			       (char *)&on, sizeof(on)))
			msyslog(LOG_ERR,
				"set_wildcard_reuse: setsockopt(SO_REUSEADDR, %s) failed: %m",
				on ? "on" : "off");

		DPRINTF(4, ("set SO_REUSEADDR to %s on %s\n", 
			    on ? "on" : "off",
			    stoa(&any->sin)));
	}
}
#endif /* OS_NEEDS_REUSEADDR_FOR_IFADDRBIND */

/*
 * update_interface strategy
 *
 * toggle configuration phase
 *
 * Phase 1:
 * forall currently existing interfaces
 *   if address is known:
 *	drop socket - rebind again
 *
 *   if address is NOT known:
 *	attempt to create a new interface entry
 *
 * Phase 2:
 * forall currently known non MCAST and WILDCARD interfaces
 *   if interface does not match configuration phase (not seen in phase 1):
 *	remove interface from known interface list
 *	forall peers associated with this interface
 *         disconnect peer from this interface
 *
 * Phase 3:
 *   attempt to re-assign interfaces to peers
 *
 */

static int
update_interfaces(
	u_short			port,
	interface_receiver_t	receiver,
	void *			data
	)
{
	isc_mem_t *		mctx = (void *)-1;
	interface_info_t	ifi;
	isc_interfaceiter_t *	iter;
	isc_result_t		result;
	isc_interface_t		isc_if;
	int			new_interface_found;
	unsigned int		family;
	struct interface	interface;
	struct interface *	iface;
	struct interface *	next;

	DPRINTF(3, ("update_interfaces(%d)\n", port));

	/*
	 * phase one - scan interfaces
	 * - create those that are not found
	 * - update those that are found
	 */

	new_interface_found = 0;
	iter = NULL;
	result = isc_interfaceiter_create(mctx, &iter);

	if (result != ISC_R_SUCCESS)
		return 0;

	/* 
	 * Toggle system interface scan phase to find untouched
	 * interfaces to be deleted.
	 */
	sys_interphase ^= 0x1;
	
	for (result = isc_interfaceiter_first(iter);
	     ISC_R_SUCCESS == result;
	     result = isc_interfaceiter_next(iter)) {

		result = isc_interfaceiter_current(iter, &isc_if);

		if (result != ISC_R_SUCCESS)
			break;

		/* See if we have a valid family to use */
		family = isc_if.address.family;
		if (AF_INET != family && AF_INET6 != family)
			continue;
		if (AF_INET == family && !ipv4_works)
			continue;
		if (AF_INET6 == family && !ipv6_works)
			continue;

		/* create prototype */
		init_interface(&interface);

		convert_isc_if(&isc_if, &interface, port);

		/* 
		 * Check if and how we are going to use the interface.
		 */
		switch (interface_action(isc_if.name, &isc_if.address,
					 isc_if.flags)) {

		case ACTION_IGNORE:
			continue;

		case ACTION_LISTEN:
			interface.ignore_packets = ISC_FALSE;
			break;

		case ACTION_DROP:
			interface.ignore_packets = ISC_TRUE;
			break;
		}

		DPRINT_INTERFACE(4, (&interface, "examining ", "\n"));

		 /* interfaces must be UP to be usable */
		if (!(interface.flags & INT_UP)) {
			DPRINTF(4, ("skipping interface %s (%s) - DOWN\n",
				    interface.name, stoa(&interface.sin)));
			continue;
		}

		/*
		 * skip any interfaces UP and bound to a wildcard
		 * address - some dhcp clients produce that in the
		 * wild
		 */
		if (is_wildcard_addr(&interface.sin))
			continue;

		/*
		 * map to local *address* in order to map all duplicate
		 * interfaces to an interface structure with the
		 * appropriate socket.  Our name space is (ip-address), 
		 * NOT (interface name, ip-address).
		 */
		iface = getinterface(&interface.sin, INT_WILDCARD);
		
		if (iface != NULL && refresh_interface(iface)) {
			/*
			 * found existing and up to date interface -
			 * mark present.
			 */
			if (iface->phase != sys_interphase) {
				/*
				 * On a new round we reset the name so
				 * the interface name shows up again if
				 * this address is no longer shared.
				 * The same reasoning goes for the
				 * ignore_packets flag.
				 */
				strncpy(iface->name, interface.name,
					sizeof(iface->name));
				iface->ignore_packets = 
					interface.ignore_packets;
			} else
				/* name collision - rename interface */
				strncpy(iface->name, "*multiple*",
					sizeof(iface->name));

			DPRINT_INTERFACE(4, (iface, "updating ", 
					     " present\n"));

			if (iface->ignore_packets != 
			    interface.ignore_packets) {
				/*
				 * We have conflicting configurations
				 * for the interface address. This is
				 * caused by using -I <interfacename>
				 * for an interface that shares its 
				 * address with other interfaces. We
				 * can not disambiguate incoming
				 * packets delivered to this socket
				 * without extra syscalls/features.
				 * These are not (commonly) available.
				 * Note this is a more unusual
				 * configuration where several
				 * interfaces share an address but
				 * filtering via interface name is
				 * attempted.  We resolve the
				 * configuration conflict by disabling
				 * the processing of received packets.
				 * This leads to no service on the
				 * interface address where the conflict
				 * occurs.
				 */
				msyslog(LOG_ERR,
					"WARNING: conflicting enable configuration for interfaces %s and %s for address %s - unsupported configuration - address DISABLED",
					interface.name, iface->name,
					stoa(&interface.sin));

				iface->ignore_packets = ISC_TRUE;				
			}

			iface->phase = sys_interphase;

			ifi.action = IFS_EXISTS;
			ifi.interface = iface;
			if (receiver != NULL)
				(*receiver)(data, &ifi);
		} else {
			/*
			 * This is new or refreshing failed - add to
			 * our interface list.  If refreshing failed we
			 * will delete the interface structure in phase
			 * 2 as the interface was not marked current.
			 * We can bind to the address as the refresh
			 * code already closed the offending socket
			 */
			iface = create_interface(port, &interface);

			if (iface != NULL) {
				ifi.action = IFS_CREATED;
				ifi.interface = iface;
				if (receiver != NULL)
					(*receiver)(data, &ifi);

				new_interface_found = 1;

				DPRINT_INTERFACE(3,
					(iface, "updating ",
					 " new - created\n"));
			} else {
				DPRINT_INTERFACE(3, 
					(&interface, "updating ",
					 " new - creation FAILED"));
			
				msyslog(LOG_INFO,
					"failed to init interface for address %s", 
					stoa(&interface.sin));
				continue;
			}
		}
	}

	isc_interfaceiter_destroy(&iter);

	/*
	 * phase 2 - delete gone interfaces - reassigning peers to
	 * other interfaces
	 */
	iface = inter_list;

	while (iface != NULL) {
		next = iface->link;
		  
		if (!(iface->flags & (INT_WILDCARD | INT_MCASTIF))) {
			/*
			 * if phase does not match sys_phase this
			 * interface was not enumerated during the last
			 * interface scan - so it is gone and will be
			 * deleted here unless it is solely an MCAST or
			 * WILDCARD interface.
			 */
			if (iface->phase != sys_interphase) {
				DPRINT_INTERFACE(3, 
					(iface, "updating ",
					 "GONE - deleting\n"));
				remove_interface(iface);

				ifi.action = IFS_DELETED;
				ifi.interface = iface;
				if (receiver != NULL)
					(*receiver)(data, &ifi);

				/*
				 * disconnect peers from deleted
				 * interface
				 */
				while (iface->peers != NULL)
					set_peerdstadr(iface->peers, NULL);

				/*
				 * update globals in case we lose 
				 * a loopback interface
				 */
				if (iface == loopback_interface)
					loopback_interface = NULL;

				delete_interface(iface);
			}
		}
		iface = next;
	}

	/*
	 * phase 3 - re-configure as the world has changed if necessary
	 */
	refresh_all_peerinterfaces();
	return new_interface_found;
}


/*
 * create_sockets - create a socket for each interface plus a default
 *			socket for when we don't know where to send
 */
static int
create_sockets(
	u_short port
	)
{
#ifndef HAVE_IO_COMPLETION_PORT
	/*
	 * I/O Completion Ports don't care about the select and FD_SET
	 */
	maxactivefd = 0;
	FD_ZERO(&activefds);
#endif

	DPRINTF(2, ("create_sockets(%d)\n", port));

	create_wildcards(port);

	update_interfaces(port, NULL, NULL);
	
	/*
	 * Now that we have opened all the sockets, turn off the reuse
	 * flag for security.
	 */
	set_reuseaddr(0);

	DPRINTF(2, ("create_sockets: Total interfaces = %d\n", ninterfaces));

	return ninterfaces;
}

/*
 * create_interface - create a new interface for a given prototype
 *		      binding the socket.
 */
static struct interface *
create_interface(
	u_short			port,
	struct interface *	protot
	)
{
	sockaddr_u resmask;
	struct interface *iface;

	DPRINTF(2, ("create_interface(%s#%d)\n", stoa(&protot->sin),
		    port));

	/* build an interface */
	iface = new_interface(protot);
	
	/*
	 * create socket
	 */
	iface->fd = open_socket(&iface->sin, 0, 0, iface);

	if (iface->fd != INVALID_SOCKET)
		list_if_listening(iface);

	if ((INT_BROADCAST & iface->flags)
	    && iface->bfd != INVALID_SOCKET)
		msyslog(LOG_INFO, "Listening on broadcast address %s#%d",
			stoa((&iface->bcast)), port);

	if (INVALID_SOCKET == iface->fd
	    && INVALID_SOCKET == iface->bfd) {
		msyslog(LOG_ERR, "unable to create socket on %s (%d) for %s#%d",
			iface->name,
			iface->ifnum,
			stoa((&iface->sin)),
			port);
		delete_interface(iface);
		return NULL;
	}
	
	/*
	 * Blacklist our own addresses, no use talking to ourself
	 */
	SET_HOSTMASK(&resmask, AF(&iface->sin));
	hack_restrict(RESTRICT_FLAGS, &iface->sin, &resmask,
		      RESM_NTPONLY | RESM_INTERFACE, RES_IGNORE);
	  
	/*
	 * set globals with the first found
	 * loopback interface of the appropriate class
	 */
	if (NULL == loopback_interface && AF_INET == iface->family
	    && (INT_LOOPBACK & iface->flags))
		loopback_interface = iface;

	/*
	 * put into our interface list
	 */
	add_addr_to_list(&iface->sin, iface);
	add_interface(iface);

	DPRINT_INTERFACE(2, (iface, "created ", "\n"));
	return iface;
}


#ifdef SO_EXCLUSIVEADDRUSE
static void
set_excladdruse(
	SOCKET fd
	)
{
	int one = 1;
	int failed;
#ifdef SYS_WINNT
	DWORD err;
#endif

	failed = setsockopt(fd, SOL_SOCKET, SO_EXCLUSIVEADDRUSE,
			    (char *)&one, sizeof(one));

	if (!failed)
		return;

#ifdef SYS_WINNT
	/*
	 * Prior to Windows XP setting SO_EXCLUSIVEADDRUSE can fail with
	 * error WSAINVAL depending on service pack level and whether
	 * the user account is in the Administrators group.  Do not 
	 * complain if it fails that way on versions prior to XP (5.1).
	 */
	err = GetLastError();

	if (isc_win32os_versioncheck(5, 1, 0, 0) < 0	/* < 5.1/XP */
	    && WSAEINVAL == err)
		return;

	SetLastError(err);
#endif
	msyslog(LOG_ERR, 
		"setsockopt(%d, SO_EXCLUSIVEADDRUSE, on): %m",
		(int)fd);
}
#endif  /* SO_EXCLUSIVEADDRUSE */


/*
 * set_reuseaddr() - set/clear REUSEADDR on all sockets
 *			NB possible hole - should we be doing this on broadcast
 *			fd's also?
 */
static void
set_reuseaddr(
	int flag
	)
{
#ifndef SO_EXCLUSIVEADDRUSE
	struct interface *interf;

	for (interf = inter_list;
	     interf != NULL;
	     interf = interf->link) {

		if (interf->flags & INT_WILDCARD)
			continue;
	  
		/*
		 * if interf->fd  is INVALID_SOCKET, we might have a adapter
		 * configured but not present
		 */
		DPRINTF(4, ("setting SO_REUSEADDR on %.16s@%s to %s\n",
			    interf->name, stoa(&interf->sin),
			    flag ? "on" : "off"));
		
		if (interf->fd != INVALID_SOCKET) {
			if (setsockopt(interf->fd, SOL_SOCKET,
					SO_REUSEADDR, (char *)&flag,
					sizeof(flag))) {
				msyslog(LOG_ERR, "set_reuseaddr: setsockopt(SO_REUSEADDR, %s) failed: %m", flag ? "on" : "off");
			}
		}
	}
#endif /* ! SO_EXCLUSIVEADDRUSE */
}

/*
 * This is just a wrapper around an internal function so we can
 * make other changes as necessary later on
 */
void
enable_broadcast(
	struct interface *	iface, 
	sockaddr_u *		baddr
	)
{
#ifdef OPEN_BCAST_SOCKET 
	socket_broadcast_enable(iface, iface->fd, baddr);
#endif
}

#ifdef OPEN_BCAST_SOCKET 
/*
 * Enable a broadcast address to a given socket
 * The socket is in the inter_list all we need to do is enable
 * broadcasting. It is not this function's job to select the socket
 */
static isc_boolean_t
socket_broadcast_enable(
	struct interface *	iface, 
	SOCKET			fd, 
	sockaddr_u *		baddr
	)
{
#ifdef SO_BROADCAST
	int on = 1;

	if (IS_IPV4(baddr)) {
		/* if this interface can support broadcast, set SO_BROADCAST */
		if (setsockopt(fd, SOL_SOCKET, SO_BROADCAST,
			       (char *)&on, sizeof(on)))
			msyslog(LOG_ERR,
				"setsockopt(SO_BROADCAST) enable failure on address %s: %m",
				stoa(baddr));
		else
			DPRINTF(2, ("Broadcast enabled on socket %d for address %s\n",
				    fd, stoa(baddr)));
	}
	iface->flags |= INT_BCASTOPEN;
	broadcast_client_enabled = ISC_TRUE;
	return ISC_TRUE;
#else
	return ISC_FALSE;
#endif /* SO_BROADCAST */
}

/*
 * Remove a broadcast address from a given socket
 * The socket is in the inter_list all we need to do is disable
 * broadcasting. It is not this function's job to select the socket
 */
static isc_boolean_t
socket_broadcast_disable(
	struct interface *	iface, 
	sockaddr_u *		baddr
	)
{
#ifdef SO_BROADCAST
	int off = 0;	/* This seems to be OK as an int */

	if (IS_IPV4(baddr) && setsockopt(iface->fd, SOL_SOCKET,
	    SO_BROADCAST, (char *)&off, sizeof(off)))
		msyslog(LOG_ERR,
			"setsockopt(SO_BROADCAST) disable failure on address %s: %m",
			stoa(baddr));

	iface->flags &= ~INT_BCASTOPEN;
	broadcast_client_enabled = ISC_FALSE;
	return ISC_TRUE;
#else
	return ISC_FALSE;
#endif /* SO_BROADCAST */
}

#endif /* OPEN_BCAST_SOCKET */

/*
 * return the broadcast client flag value
 */
isc_boolean_t
get_broadcastclient_flag(void)
{
	return (broadcast_client_enabled);
}
/*
 * Check to see if the address is a multicast address
 */
static isc_boolean_t
addr_ismulticast(
	sockaddr_u *maddr
	)
{
	isc_boolean_t result;

#ifndef INCLUDE_IPV6_MULTICAST_SUPPORT
	/*
	 * If we don't have IPV6 support any IPV6 addr is not multicast
	 */
	if (IS_IPV6(maddr))
		result = ISC_FALSE;
	else
#endif
		result = IS_MCAST(maddr);

	if (!result)
		DPRINTF(4, ("address %s is not multicast\n",
			    stoa(maddr)));

	return result;
}

/*
 * Multicast servers need to set the appropriate Multicast interface
 * socket option in order for it to know which interface to use for
 * send the multicast packet.
 */
void
enable_multicast_if(
	struct interface *	iface,
	sockaddr_u *		maddr
	)
{
#ifdef MCAST
	TYPEOF_IP_MULTICAST_LOOP off = 0;

	NTP_REQUIRE(AF(maddr) == AF(&iface->sin));

	switch (AF(&iface->sin)) {

	case AF_INET:
		if (setsockopt(iface->fd, IPPROTO_IP, IP_MULTICAST_IF,
			       (void *)&NSRCADR(&iface->sin),
			       sizeof(NSRCADR(&iface->sin)))) {

			msyslog(LOG_ERR,
				"setsockopt IP_MULTICAST_IF failed: %m on socket %d, addr %s for multicast address %s",
				iface->fd, stoa(&iface->sin),
				stoa(maddr));
			return;
		}
#ifdef IP_MULTICAST_LOOP
		/*
		 * Don't send back to itself, but allow failure to set
		 */
		if (setsockopt(iface->fd, IPPROTO_IP,
			       IP_MULTICAST_LOOP,
			       SETSOCKOPT_ARG_CAST &off,
			       sizeof(off))) {

			msyslog(LOG_ERR,
				"setsockopt IP_MULTICAST_LOOP failed: %m on socket %d, addr %s for multicast address %s",
				iface->fd, stoa(&iface->sin), 
				stoa(maddr));
		}
#endif
		DPRINTF(4, ("Added IPv4 multicast interface on socket %d, addr %s for multicast address %s\n",
			    iface->fd, stoa(&iface->sin),
			    stoa(maddr)));
		break;

	case AF_INET6:
#ifdef INCLUDE_IPV6_MULTICAST_SUPPORT
		if (setsockopt(iface->fd, IPPROTO_IPV6,
			       IPV6_MULTICAST_IF,
			       (char *)&iface->scopeid,
			       sizeof(iface->scopeid))) {

			msyslog(LOG_ERR,
				"setsockopt IPV6_MULTICAST_IF failed: %m on socket %d, addr %s, scope %d for multicast address %s",
				iface->fd, stoa(&iface->sin),
				iface->scopeid, stoa(maddr));
			return;
		}
#ifdef IPV6_MULTICAST_LOOP
		/*
		 * Don't send back to itself, but allow failure to set
		 */
		if (setsockopt(iface->fd, IPPROTO_IPV6,
			       IPV6_MULTICAST_LOOP,
			       (char *) &off, sizeof(off))) {

			msyslog(LOG_ERR,
				"setsockopt IP_MULTICAST_LOOP failed: %m on socket %d, addr %s for multicast address %s",
				iface->fd, stoa(&iface->sin),
				stoa(maddr));
		}
#endif
		DPRINTF(4, ("Added IPv6 multicast interface on socket %d, addr %s, scope %d for multicast address %s\n",
			    iface->fd,  stoa(&iface->sin),
			    iface->scopeid, stoa(maddr)));
		break;
#else
		return;
#endif	/* INCLUDE_IPV6_MULTICAST_SUPPORT */
	}
	return;
#endif
}

/*
 * Add a multicast address to a given socket
 * The socket is in the inter_list all we need to do is enable
 * multicasting. It is not this function's job to select the socket
 */
#ifdef MCAST
static isc_boolean_t
socket_multicast_enable(
	struct interface *	iface,
	int			lscope,
	sockaddr_u *		maddr
	)
{
	struct ip_mreq		mreq;
#ifdef INCLUDE_IPV6_MULTICAST_SUPPORT
	struct ipv6_mreq	mreq6;
#endif

	if (find_addr_in_list(maddr) != NULL) {
		DPRINTF(4, ("socket_multicast_enable(%s): already enabled\n",
			    stoa(maddr)));
		return ISC_TRUE;
	}

	switch (AF(maddr)) {

	case AF_INET:
		memset(&mreq, 0, sizeof(mreq));
		mreq.imr_multiaddr = SOCK_ADDR4(maddr);
		mreq.imr_interface.s_addr = htonl(INADDR_ANY);
		if (setsockopt(iface->fd,
			       IPPROTO_IP,
			       IP_ADD_MEMBERSHIP,
			       (char *)&mreq, 
			       sizeof(mreq))) {
			msyslog(LOG_ERR,
				"setsockopt IP_ADD_MEMBERSHIP failed: %m on socket %d, addr %s for %x / %x (%s)",
				iface->fd, stoa(&iface->sin),
				mreq.imr_multiaddr.s_addr,
				mreq.imr_interface.s_addr,
				stoa(maddr));
			return ISC_FALSE;
		}
		DPRINTF(4, ("Added IPv4 multicast membership on socket %d, addr %s for %x / %x (%s)\n",
			    iface->fd, stoa(&iface->sin),
			    mreq.imr_multiaddr.s_addr,
			    mreq.imr_interface.s_addr, stoa(maddr)));
		break;

	case AF_INET6:
#ifdef INCLUDE_IPV6_MULTICAST_SUPPORT
		/*
		 * Enable reception of multicast packets.
		 * If the address is link-local we can get the
		 * interface index from the scope id. Don't do this
		 * for other types of multicast addresses. For now let
		 * the kernel figure it out.
		 */
		memset(&mreq6, 0, sizeof(mreq6));
		mreq6.ipv6mr_multiaddr = SOCK_ADDR6(maddr);
		mreq6.ipv6mr_interface = lscope;

		if (setsockopt(iface->fd, IPPROTO_IPV6,
			       IPV6_JOIN_GROUP, (char *)&mreq6, 
			       sizeof(mreq6))) {
			msyslog(LOG_ERR,
				"setsockopt IPV6_JOIN_GROUP failed: %m on socket %d, addr %s for interface %d (%s)",
				iface->fd, stoa(&iface->sin),
				mreq6.ipv6mr_interface, stoa(maddr));
			return ISC_FALSE;
		}
		DPRINTF(4, ("Added IPv6 multicast group on socket %d, addr %s for interface %d(%s)\n",
			    iface->fd, stoa(&iface->sin),
			    mreq6.ipv6mr_interface, stoa(maddr)));
#else
		return ISC_FALSE;
#endif	/* INCLUDE_IPV6_MULTICAST_SUPPORT */
	}
	iface->flags |= INT_MCASTOPEN;
	iface->num_mcast++;
	add_addr_to_list(maddr, iface);
	return ISC_TRUE;
}

/*
 * Remove a multicast address from a given socket
 * The socket is in the inter_list all we need to do is disable
 * multicasting. It is not this function's job to select the socket
 */
static isc_boolean_t
socket_multicast_disable(
	struct interface *	iface,
	sockaddr_u *		maddr
	)
{
#ifdef INCLUDE_IPV6_MULTICAST_SUPPORT
	struct ipv6_mreq mreq6;
#endif
	struct ip_mreq mreq;

	memset(&mreq, 0, sizeof(mreq));

	if (find_addr_in_list(maddr) == NULL) {
		DPRINTF(4, ("socket_multicast_disable(%s): not found\n", 
			    stoa(maddr)));
		return ISC_TRUE;
	}

	switch (AF(maddr)) {

	case AF_INET:
		mreq.imr_multiaddr = SOCK_ADDR4(maddr);
		mreq.imr_interface = SOCK_ADDR4(&iface->sin);
		if (setsockopt(iface->fd, IPPROTO_IP,
			       IP_DROP_MEMBERSHIP, (char *)&mreq,
			       sizeof(mreq))) {

			msyslog(LOG_ERR,
				"setsockopt IP_DROP_MEMBERSHIP failed: %m on socket %d, addr %s for %x / %x (%s)",
				iface->fd, stoa(&iface->sin),
				SRCADR(maddr), SRCADR(&iface->sin),
				stoa(maddr));
			return ISC_FALSE;
		}
		break;
	case AF_INET6:
#ifdef INCLUDE_IPV6_MULTICAST_SUPPORT
		/*
		 * Disable reception of multicast packets
		 * If the address is link-local we can get the
		 * interface index from the scope id.  Don't do this
		 * for other types of multicast addresses. For now let
		 * the kernel figure it out.
		 */
		mreq6.ipv6mr_multiaddr = SOCK_ADDR6(maddr);
		mreq6.ipv6mr_interface = iface->scopeid;

		if (setsockopt(iface->fd, IPPROTO_IPV6,
			       IPV6_LEAVE_GROUP, (char *)&mreq6,
			       sizeof(mreq6))) {

			msyslog(LOG_ERR,
				"setsockopt IPV6_LEAVE_GROUP failure: %m on socket %d, addr %s for %d (%s)",
				iface->fd, stoa(&iface->sin),
				iface->scopeid, stoa(maddr));
			return ISC_FALSE;
		}
		break;
#else
		return ISC_FALSE;
#endif	/* INCLUDE_IPV6_MULTICAST_SUPPORT */
	}

	iface->num_mcast--;
	if (!iface->num_mcast)
		iface->flags &= ~INT_MCASTOPEN;

	return ISC_TRUE;
}
#endif	/* MCAST */

/*
 * io_setbclient - open the broadcast client sockets
 */
void
io_setbclient(void)
{
#ifdef OPEN_BCAST_SOCKET 
	struct interface *	interf;
	int			nif;
	isc_boolean_t		jstatus; 
	SOCKET			fd;

	nif = 0;
	set_reuseaddr(1);

	for (interf = inter_list;
	     interf != NULL;
	     interf = interf->link) {

		if (interf->flags & (INT_WILDCARD | INT_LOOPBACK))
			continue;
	  
		/* use only allowed addresses */
		if (interf->ignore_packets)
			continue;


		/* Need a broadcast-capable interface */
		if (!(interf->flags & INT_BROADCAST))
			continue;

		/* Only IPv4 addresses are valid for broadcast */
		NTP_REQUIRE(IS_IPV4(&interf->sin));

		/* Do we already have the broadcast address open? */
		if (interf->flags & INT_BCASTOPEN) {
			/* 
			 * account for already open interfaces to avoid
			 * misleading warning below 
			 */
			nif++;
			continue;
		}

		/*
		 * Try to open the broadcast address
		 */
		interf->family = AF_INET;
		interf->bfd = open_socket(&interf->bcast, 1, 0, interf);

		/*
		 * If we succeeded then we use it otherwise enable
		 * broadcast on the interface address
		 */
		if (interf->bfd != INVALID_SOCKET) {
			fd = interf->bfd;
			jstatus = ISC_TRUE;
		} else {
			fd = interf->fd;
			jstatus = socket_broadcast_enable(interf, fd,
					&interf->sin);
		}

		/* Enable Broadcast on socket */
		if (jstatus) {
			nif++;
			msyslog(LOG_INFO,
				"io_setbclient: Opened broadcast client on interface #%d %s",
				interf->ifnum, interf->name);
			interf->addr_refid = addr2refid(&interf->sin);
		}
	}
	set_reuseaddr(0);
	if (nif > 0)
		DPRINTF(1, ("io_setbclient: Opened broadcast clients\n"));
	else if (!nif)
		msyslog(LOG_ERR,
			"Unable to listen for broadcasts, no broadcast interfaces available");
#else
	msyslog(LOG_ERR,
		"io_setbclient: Broadcast Client disabled by build");
#endif	/* OPEN_BCAST_SOCKET */
}

/*
 * io_unsetbclient - close the broadcast client sockets
 */
void
io_unsetbclient(void)
{
	struct interface *interf;

	for (interf = inter_list;
	     NULL != interf;
	     interf = interf->link)
	{
		if (interf->flags & INT_WILDCARD)
			continue;
	  
		if (!(interf->flags & INT_BCASTOPEN))
			continue;

		socket_broadcast_disable(interf, &interf->sin);
	}
}

/*
 * io_multicast_add() - add multicast group address
 */
void
io_multicast_add(
	sockaddr_u *addr
	)
{
#ifdef MCAST
	struct interface *interface;
#ifndef MULTICAST_NONEWSOCKET
	struct interface *iface;
#endif
	int lscope = 0;
	
	/*
	 * Check to see if this is a multicast address
	 */
	if (!addr_ismulticast(addr))
		return;

	/* If we already have it we can just return */
	if (NULL != find_flagged_addr_in_list(addr, INT_MCASTOPEN)) {
		msyslog(LOG_INFO, 
			"Duplicate request found for multicast address %s",
			stoa(addr));
		return;
	}

#ifndef MULTICAST_NONEWSOCKET
	interface = new_interface(NULL);
	
	/*
	 * Open a new socket for the multicast address
	 */
	interface->family = 
		AF(&interface->sin) = 
		AF(&interface->mask) = AF(addr);
	SET_PORT(&interface->sin, NTP_PORT);
	SET_ONESMASK(&interface->mask);

	switch(AF(addr)) {

	case AF_INET:
		NSRCADR(&interface->sin) = NSRCADR(addr);
		break;

	case AF_INET6:
#ifdef INCLUDE_IPV6_MULTICAST_SUPPORT
		SET_ADDR6N(&interface->sin, SOCK_ADDR6(addr));
		lscope = SCOPE(addr);
		SET_SCOPE(&interface->sin, lscope); 
#endif
		iface = findlocalcastinterface(addr);
		if (iface != NULL)
			DPRINTF(4, ("Found interface #%d %s, scope %d for address %s\n",
				    iface->ifnum, iface->name, lscope,
				    stoa(addr)));
	}
		
	set_reuseaddr(1);
	interface->bfd = INVALID_SOCKET;
	interface->fd = open_socket(&interface->sin, INT_MULTICAST, 0,
				    interface);

	if (interface->fd != INVALID_SOCKET) {
		interface->bfd = INVALID_SOCKET;
		interface->ignore_packets = ISC_FALSE;
		interface->flags |= INT_MCASTIF;
		
		strncpy(interface->name, "multicast",
			sizeof(interface->name));
		DPRINT_INTERFACE(2, (interface, "multicast add ", "\n"));
		/*
		 * socket_multicast_enable() will add this address to
		 * the addresslist
		 */
		add_interface(interface);
		list_if_listening(interface);
	} else {
		/* bind failed, re-use wildcard interface */
		delete_interface(interface);

		if (IS_IPV4(addr))
			interface = wildipv4;
		else if (IS_IPV6(addr))
			interface = wildipv6;
		else
			interface = NULL;

		if (interface != NULL) {
			/* HACK ! -- stuff in an address */
			/* because we don't bind addr? DH */
			interface->bcast = *addr;
			msyslog(LOG_ERR,
				"multicast address %s using wildcard interface #%d %s",
				 stoa(addr), interface->ifnum,
				 interface->name);
		} else {
			msyslog(LOG_ERR,
				"No multicast socket available to use for address %s",
				stoa(addr));
			return;
		}
	}
#else
	/*
	 * For the case where we can't use a separate socket
	 */
	interface = findlocalcastinterface(addr);
	/*
	 * If we don't have a valid socket, just return
	 */
	if (NULL == interface) {
		msyslog(LOG_ERR,
			"Can not add multicast address %s: no multicast interface found",
			stoa(addr));
		return;
	}

#endif
	if (socket_multicast_enable(interface, lscope, addr))
		msyslog(LOG_INFO, 
			"Added Multicast Listener %s on interface #%d %s",
			stoa(addr), interface->ifnum, interface->name);
	else
		msyslog(LOG_ERR, "Failed to add Multicast Listener %s",
			stoa(addr));
#else /* MCAST */
	msyslog(LOG_ERR,
		"Can not add multicast address %s: no multicast support",
		stoa(addr));
#endif /* MCAST */
	return;
}


/*
 * io_multicast_del() - delete multicast group address
 */
void
io_multicast_del(
	sockaddr_u *	addr
	)
{
#ifdef MCAST
	struct interface *iface;

	/*
	 * Check to see if this is a multicast address
	 */
	if (!addr_ismulticast(addr)) {
		msyslog(LOG_ERR, "invalid multicast address %s",
			stoa(addr));
		return;
	}

	/*
	 * Disable reception of multicast packets
	 */
	while ((iface = find_flagged_addr_in_list(addr, INT_MCASTOPEN))
	       != NULL)
		socket_multicast_disable(iface, addr);

	delete_addr_from_list(addr);

#else /* not MCAST */
	msyslog(LOG_ERR,
		"Can not delete multicast address %s: no multicast support",
		stoa(addr));
#endif /* not MCAST */
}


/*
 * init_nonblocking_io() - set up descriptor to be non blocking
 */
static void init_nonblocking_io(
	SOCKET fd
	)
{
	/*
	 * set non-blocking,
	 */

#ifdef USE_FIONBIO
	/* in vxWorks we use FIONBIO, but the others are defined for old systems, so
	 * all hell breaks loose if we leave them defined
	 */
#undef O_NONBLOCK
#undef FNDELAY
#undef O_NDELAY
#endif

#if defined(O_NONBLOCK) /* POSIX */
	if (fcntl(fd, F_SETFL, O_NONBLOCK) < 0) {
		msyslog(LOG_ERR,
			"fcntl(O_NONBLOCK) fails on fd #%d: %m", fd);
		exit(1);
	}
#elif defined(FNDELAY)
	if (fcntl(fd, F_SETFL, FNDELAY) < 0) {
		msyslog(LOG_ERR, "fcntl(FNDELAY) fails on fd #%d: %m",
			fd);
		exit(1);
	}
#elif defined(O_NDELAY) /* generally the same as FNDELAY */
	if (fcntl(fd, F_SETFL, O_NDELAY) < 0) {
		msyslog(LOG_ERR, "fcntl(O_NDELAY) fails on fd #%d: %m",
			fd);
		exit(1);
	}
#elif defined(FIONBIO)
	{
		int on = 1;

		if (ioctl(fd, FIONBIO, &on) < 0) {
			msyslog(LOG_ERR,
				"ioctl(FIONBIO) fails on fd #%d: %m",
				fd);
			exit(1);
		}
	}
#elif defined(FIOSNBIO)
	if (ioctl(fd, FIOSNBIO, &on) < 0) {
		msyslog(LOG_ERR,
			"ioctl(FIOSNBIO) fails on fd #%d: %m", fd);
		exit(1);
	}
#else
# include "Bletch: Need non-blocking I/O!"
#endif
}

/*
 * open_socket - open a socket, returning the file descriptor
 */

static SOCKET
open_socket(
	sockaddr_u *		addr,
	int			bcast,
	int			turn_off_reuse,
	struct interface *	interf
	)
{
	SOCKET	fd;
	int	errval;
	char	scopetext[16];
	/*
	 * int is OK for REUSEADR per 
	 * http://www.kohala.com/start/mcast.api.txt
	 */
	int	on = 1;
	int	off = 0;

	if (IS_IPV6(addr) && !ipv6_works)
		return INVALID_SOCKET;

	/* create a datagram (UDP) socket */
	fd = socket(AF(addr), SOCK_DGRAM, 0);
	if (INVALID_SOCKET == fd) {
#ifndef SYS_WINNT
		errval = errno;
#else
		errval = WSAGetLastError();
#endif
		msyslog(LOG_ERR, 
			"socket(AF_INET%s, SOCK_DGRAM, 0) failed on address %s: %m",
			IS_IPV6(addr) ? "6" : "", stoa(addr));

		if (errval == EPROTONOSUPPORT || 
		    errval == EAFNOSUPPORT ||
		    errval == EPFNOSUPPORT)
			return (INVALID_SOCKET);

		errno = errval;
		msyslog(LOG_ERR,
			"unexpected socket() error %m code %d (not EPROTONOSUPPORT nor EAFNOSUPPORT nor EPFNOSUPPORT) - exiting",
			errno);
		exit(1);
	}

#ifdef SYS_WINNT
	connection_reset_fix(fd, addr);
#endif
	/*
	 * Fixup the file descriptor for some systems
	 * See bug #530 for details of the issue.
	 */
	fd = move_fd(fd);

	/*
	 * set SO_REUSEADDR since we will be binding the same port
	 * number on each interface according to turn_off_reuse.
	 * This is undesirable on Windows versions starting with
	 * Windows XP (numeric version 5.1).
	 */
#ifdef SYS_WINNT
	if (isc_win32os_versioncheck(5, 1, 0, 0) < 0)  /* before 5.1 */
#endif
		if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR,
			       (char *)((turn_off_reuse) 
					    ? &off 
					    : &on), 
			       sizeof(on))) {

			msyslog(LOG_ERR,
				"setsockopt SO_REUSEADDR %s fails for address %s: %m",
				(turn_off_reuse)
				    ? "off" 
				    : "on", 
				stoa(addr));
			closesocket(fd);
			return INVALID_SOCKET;
		}
#ifdef SO_EXCLUSIVEADDRUSE
	/*
	 * setting SO_EXCLUSIVEADDRUSE on the wildcard we open
	 * first will cause more specific binds to fail.
	 */
	if (!(interf->flags & INT_WILDCARD))
		set_excladdruse(fd);
#endif

	/*
	 * IPv4 specific options go here
	 */
	if (IS_IPV4(addr)) {
#if defined(HAVE_IPTOS_SUPPORT)
		if (setsockopt(fd, IPPROTO_IP, IP_TOS, (char *)&qos,
			       sizeof(qos)))
			msyslog(LOG_ERR,
				"setsockopt IP_TOS (%02x) fails on address %s: %m",
				qos, stoa(addr));
#endif /* HAVE_IPTOS_SUPPORT */
		if (bcast)
			socket_broadcast_enable(interf, fd, addr);
	}

	/*
	 * IPv6 specific options go here
	 */
	if (IS_IPV6(addr)) {
#if defined(IPV6_V6ONLY)
		if (isc_net_probe_ipv6only() == ISC_R_SUCCESS
		    && setsockopt(fd, IPPROTO_IPV6, IPV6_V6ONLY,
		    (char*)&on, sizeof(on)))
			msyslog(LOG_ERR,
				"setsockopt IPV6_V6ONLY on fails on address %s: %m",
				stoa(addr));
#endif /* IPV6_V6ONLY */
#if defined(IPV6_BINDV6ONLY)
		if (setsockopt(fd, IPPROTO_IPV6, IPV6_BINDV6ONLY,
		    (char*)&on, sizeof(on)))
			msyslog(LOG_ERR,
				"setsockopt IPV6_BINDV6ONLY on fails on address %s: %m",
				stoa(addr));
#endif /* IPV6_BINDV6ONLY */
	}

#ifdef OS_NEEDS_REUSEADDR_FOR_IFADDRBIND
	/*
	 * some OSes don't allow binding to more specific
	 * addresses if a wildcard address already bound
	 * to the port and SO_REUSEADDR is not set
	 */
	if (!is_wildcard_addr(addr))
		set_wildcard_reuse(AF(addr), 1);
#endif

	/*
	 * bind the local address.
	 */
	errval = bind(fd, &addr->sa, SOCKLEN(addr));

#ifdef OS_NEEDS_REUSEADDR_FOR_IFADDRBIND
	if (!is_wildcard_addr(addr))
		set_wildcard_reuse(AF(addr), 0);
#endif

	if (errval < 0) {
		/*
		 * Don't log this under all conditions
		 */
		if (turn_off_reuse == 0
#ifdef DEBUG
		    || debug > 1
#endif
		    ) {
			if (SCOPE(addr))
				snprintf(scopetext, sizeof(scopetext),
					 "%%%d", SCOPE(addr));
			else
				scopetext[0] = 0;

			msyslog(LOG_ERR,
				"bind(%d) AF_INET%s %s%s#%d%s flags 0x%x failed: %m",
				fd, IS_IPV6(addr) ? "6" : "",
				stoa(addr), scopetext, SRCPORT(addr),
				IS_MCAST(addr) ? " (multicast)" : "",
				interf->flags);
		}

		closesocket(fd);
		
		return INVALID_SOCKET;
	}

#ifdef HAVE_TIMESTAMP
	{
		if (setsockopt(fd, SOL_SOCKET, SO_TIMESTAMP,
			       (char*)&on, sizeof(on)))
			msyslog(LOG_DEBUG,
				"setsockopt SO_TIMESTAMP on fails on address %s: %m",
				stoa(addr));
		else
			DPRINTF(4, ("setsockopt SO_TIMESTAMP enabled on fd %d address %s\n",
				    fd, stoa(addr)));
	}	
#endif
	DPRINTF(4, ("bind(%d) AF_INET%s, addr %s%%%d#%d, flags 0x%x\n",
		   fd, IS_IPV6(addr) ? "6" : "", stoa(addr),
		   SCOPE(addr), SRCPORT(addr), interf->flags));

	init_nonblocking_io(fd);
	
#ifdef HAVE_SIGNALED_IO
	init_socket_sig(fd);
#endif /* not HAVE_SIGNALED_IO */

	add_fd_to_list(fd, FD_TYPE_SOCKET);

#if !defined(SYS_WINNT) && !defined(VMS)
	DPRINTF(4, ("flags for fd %d: 0x%x\n", fd,
		    fcntl(fd, F_GETFL, 0)));
#endif /* SYS_WINNT || VMS */

#if defined (HAVE_IO_COMPLETION_PORT)
/*
 * Add the socket to the completion port
 */
	if (io_completion_port_add_socket(fd, interf)) {
		msyslog(LOG_ERR, "unable to set up io completion port - EXITING");
		exit(1);
	}
#endif
	return fd;
}

/* XXX ELIMINATE sendpkt similar in ntpq.c, ntpdc.c, ntp_io.c, ntptrace.c */
/*
 * sendpkt - send a packet to the specified destination. Maintain a
 * send error cache so that only the first consecutive error for a
 * destination is logged.
 */
void
sendpkt(
	sockaddr_u *dest,
	struct interface *inter,
	int ttl,
	struct pkt *pkt,
	int len
	)
{
	int cc;

	if (NULL == inter) {
		/*
		 * unbound peer - drop request and wait for better
		 * network conditions 
		 */
		DPRINTF(2, ("%ssendpkt(dst=%s, ttl=%d, len=%d): no interface - IGNORED\n",
			    (ttl > 0) ? "\tMCAST\t***** " : "",
			    stoa(dest), ttl, len));
		return;
	}

	DPRINTF(2, ("%ssendpkt(%d, dst=%s, src=%s, ttl=%d, len=%d)\n",
		    (ttl > 0) ? "\tMCAST\t***** " : "",
		    inter->fd, stoa(dest), stoa(&inter->sin),
		    ttl, len));

#ifdef MCAST
	/*
	 * for the moment we use the bcast option to set multicast ttl
	 */
	if (ttl > 0 && ttl != inter->last_ttl) {
		/*
		 * set the multicast ttl for outgoing packets
		 */
		int	rtc;
		u_char	cttl;
#ifdef INCLUDE_IPV6_SUPPORT
		u_int	uttl;
#endif
		
		switch (AF(&inter->sin)) {
			
		case AF_INET :
			cttl = (u_char)ttl;
			rtc = setsockopt(inter->fd, IPPROTO_IP, 
					 IP_MULTICAST_TTL,
					 (void *)&cttl, sizeof(cttl));
			break;
			
#ifdef INCLUDE_IPV6_SUPPORT
		case AF_INET6 :
			uttl = (u_int)ttl;
			rtc = setsockopt(inter->fd, IPPROTO_IPV6,
					 IPV6_MULTICAST_HOPS,
					 (void *)&uttl, sizeof(uttl));
			break;
#endif /* INCLUDE_IPV6_SUPPORT */

		default:	/* just NOP if not supported */
			DPRINTF(1, ("sendpkt unknown AF %d", 
				    AF(&inter->sin)));
			rtc = 0;
		}
		
		if (!rtc)
			inter->last_ttl = ttl;
		else
			msyslog(LOG_ERR, 
				"setsockopt IP_MULTICAST_TTL/IPV6_MULTICAST_HOPS fails on address %s: %m",
				stoa(&inter->sin));
	}

#endif /* MCAST */

#if defined(HAVE_IO_COMPLETION_PORT)
	cc = io_completion_port_sendto(inter, pkt, len, dest);
	if (cc != ERROR_SUCCESS) {
#else
#ifdef SIM
	cc = simulate_server(dest, inter, pkt);
#else /* SIM */
	cc = sendto(inter->fd, (char *)pkt, (unsigned int)len, 0, 
		    (struct sockaddr *)dest, SOCKLEN(dest));
#endif /* SIM */
	if (cc == -1) {
#endif
		inter->notsent++;
		packets_notsent++;
	} else	{
		inter->sent++;
		packets_sent++;
	}
}


#if !defined(HAVE_IO_COMPLETION_PORT)
/*
 * fdbits - generate ascii representation of fd_set (FAU debug support)
 * HFDF format - highest fd first.
 */
static char *
fdbits(
	int count,
	fd_set *set
	)
{
	static char buffer[256];
	char * buf = buffer;

	count = min(count,  255);

	while (count >= 0) {
		*buf++ = FD_ISSET(count, set) ? '#' : '-';
		count--;
	}
	*buf = '\0';

	return buffer;
}

/*
 * Routine to read the refclock packets for a specific interface
 * Return the number of bytes read. That way we know if we should
 * read it again or go on to the next one if no bytes returned
 */
static inline int
read_refclock_packet(SOCKET fd, struct refclockio *rp, l_fp ts)
{
	int i;
	int buflen;
	register struct recvbuf *rb;

	rb = get_free_recv_buffer();

	if (NULL == rb) {
		/*
		 * No buffer space available - just drop the packet
		 */
		char buf[RX_BUFF_SIZE];

		buflen = read(fd, buf, sizeof buf);
		packets_dropped++;
		return (buflen);
	}

	i = (rp->datalen == 0
	     || rp->datalen > (int)sizeof(rb->recv_space))
		? (int)sizeof(rb->recv_space)
		: rp->datalen;
	buflen = read(fd, (char *)&rb->recv_space, (unsigned)i);

	if (buflen < 0) {
		if (errno != EINTR && errno != EAGAIN)
			msyslog(LOG_ERR, "clock read fd %d: %m", fd);
		freerecvbuf(rb);
		return (buflen);
	}

	/*
	 * Got one. Mark how and when it got here,
	 * put it on the full list and do bookkeeping.
	 */
	rb->recv_length = buflen;
	rb->recv_srcclock = rp->srcclock;
	rb->dstadr = 0;
	rb->fd = fd;
	rb->recv_time = ts;
	rb->receiver = rp->clock_recv;

	if (rp->io_input) {
		/*
		 * have direct input routine for refclocks
		 */
		if (rp->io_input(rb) == 0) {
			/*
			 * data was consumed - nothing to pass up
			 * into block input machine
			 */
			freerecvbuf(rb);
			return (buflen);
		}
	}
	
	add_full_recv_buffer(rb);

	rp->recvcount++;
	packets_received++;
	return (buflen);
}


#ifdef HAVE_TIMESTAMP
/*
 * extract timestamps from control message buffer
 */
static l_fp
fetch_timestamp(
	struct recvbuf *	rb,
	struct msghdr *		msghdr, 
	l_fp			ts
	)
{
#ifdef USE_TIMESTAMP_CMSG
	struct cmsghdr *cmsghdr;

	cmsghdr = CMSG_FIRSTHDR(msghdr);
	while (cmsghdr != NULL) {
		switch (cmsghdr->cmsg_type)
		{
		case SCM_TIMESTAMP:
		{
			struct timeval *tvp;
			double dtemp;
			l_fp nts;

			tvp = (struct timeval *)CMSG_DATA(cmsghdr);
			DPRINTF(4, ("fetch_timestamp: system network time stamp: %lld.%06ld\n",
				    (long long)tvp->tv_sec, (long)tvp->tv_usec));
			nts.l_i = tvp->tv_sec + JAN_1970;
			dtemp = (tvp->tv_usec 
				 + (ntp_random() * 2. / FRAC)) / 1e6;
			nts.l_uf = (u_int32)(dtemp * FRAC);
#ifdef DEBUG_TIMING
			{
				l_fp dts;

				dts = ts;
				L_SUB(&dts, &nts);
				collect_timing(rb, 
					       "input processing delay",
					       1, &dts);
				DPRINTF(4, ("fetch_timestamp: timestamp delta: %s (incl. prec fuzz)\n",
					    lfptoa(&dts, 9)));
			}
#endif
			ts = nts;  /* network time stamp */
			break;
		}
		default:
			DPRINTF(4, ("fetch_timestamp: skipping control message 0x%x\n",
				    cmsghdr->cmsg_type));
		}
		cmsghdr = CMSG_NXTHDR(msghdr, cmsghdr);
	}
#endif
	return ts;
}
#endif


/*
 * Routine to read the network NTP packets for a specific interface
 * Return the number of bytes read. That way we know if we should
 * read it again or go on to the next one if no bytes returned
 */
static inline int
read_network_packet(
	SOCKET			fd,
	struct interface *	itf,
	l_fp			ts
	)
{
	GETSOCKNAME_SOCKLEN_TYPE fromlen;
	int buflen;
	register struct recvbuf *rb;
#ifdef HAVE_TIMESTAMP
	struct msghdr msghdr;
	struct iovec iovec;
	char control[TIMESTAMP_CTLMSGBUF_SIZE];
#endif

	/*
	 * Get a buffer and read the frame.  If we
	 * haven't got a buffer, or this is received
	 * on a disallowed socket, just dump the
	 * packet.
	 */

	rb = get_free_recv_buffer();
	if (NULL == rb || itf->ignore_packets) {
		char buf[RX_BUFF_SIZE];
		sockaddr_u from;

		if (rb != NULL)
			freerecvbuf(rb);

		fromlen = sizeof(from);
		buflen = recvfrom(fd, buf, sizeof(buf), 0,
				  &from.sa, &fromlen);
		DPRINTF(4, ("%s on (%lu) fd=%d from %s\n",
			(itf->ignore_packets)
			    ? "ignore"
			    : "drop",
			free_recvbuffs(), fd, stoa(&from)));
		if (itf->ignore_packets)
			packets_ignored++;
		else
			packets_dropped++;
		return (buflen);
	}

	fromlen = sizeof(rb->recv_srcadr);

#ifndef HAVE_TIMESTAMP
	rb->recv_length = recvfrom(fd, (char *)&rb->recv_space,
				   sizeof(rb->recv_space), 0,
				   &rb->recv_srcadr.sa, &fromlen);
#else
	iovec.iov_base        = &rb->recv_space;
	iovec.iov_len         = sizeof(rb->recv_space);
	msghdr.msg_name       = &rb->recv_srcadr;
	msghdr.msg_namelen    = fromlen;
	msghdr.msg_iov        = &iovec;
	msghdr.msg_iovlen     = 1;
	msghdr.msg_control    = (void *)&control;
	msghdr.msg_controllen = sizeof(control);
	msghdr.msg_flags      = 0;
	rb->recv_length       = recvmsg(fd, &msghdr, 0);
#endif

	buflen = rb->recv_length;

	if (buflen == 0 || (buflen == -1 && 
	    (EWOULDBLOCK == errno
#ifdef EAGAIN
	     || EAGAIN == errno
#endif
	     ))) {
		freerecvbuf(rb);
		return (buflen);
	} else if (buflen < 0) {
		msyslog(LOG_ERR, "recvfrom(%s) fd=%d: %m",
			stoa(&rb->recv_srcadr), fd);
		DPRINTF(5, ("read_network_packet: fd=%d dropped (bad recvfrom)\n",
			    fd));
		freerecvbuf(rb);
		return (buflen);
	}

	DPRINTF(3, ("read_network_packet: fd=%d length %d from %s\n",
		    fd, buflen, stoa(&rb->recv_srcadr)));

	/*
	 * Got one.  Mark how and when it got here,
	 * put it on the full list and do bookkeeping.
	 */
	rb->dstadr = itf;
	rb->fd = fd;
#ifdef HAVE_TIMESTAMP
	/* pick up a network time stamp if possible */
	ts = fetch_timestamp(rb, &msghdr, ts);
#endif
	rb->recv_time = ts;
	rb->receiver = receive;

	add_full_recv_buffer(rb);

	itf->received++;
	packets_received++;
	return (buflen);
}


/*
 * input_handler - receive packets asynchronously
 */
void
input_handler(
	l_fp *cts
	)
{
	int buflen;
	int n;
	int doing;
	SOCKET fd;
	struct timeval tvzero;
	l_fp ts;		/* Timestamp at BOselect() gob */
#ifdef DEBUG_TIMING
	l_fp ts_e;		/* Timestamp at EOselect() gob */
#endif
	fd_set fds;
	int select_count = 0;
	struct interface *interface;
#if defined(HAS_ROUTING_SOCKET)
	struct asyncio_reader *asyncio_reader;
#endif

	handler_calls++;

	/*
	 * If we have something to do, freeze a timestamp.
	 * See below for the other cases (nothing left to do or error)
	 */
	ts = *cts;

	/*
	 * Do a poll to see who has data
	 */

	fds = activefds;
	tvzero.tv_sec = tvzero.tv_usec = 0;

	n = select(maxactivefd + 1, &fds, (fd_set *)0, (fd_set *)0,
		   &tvzero);

	/*
	 * If there are no packets waiting just return
	 */
	if (n < 0) {
		int err = errno;
		/*
		 * extended FAU debugging output
		 */
		if (err != EINTR)
			msyslog(LOG_ERR,
				"select(%d, %s, 0L, 0L, &0.0) error: %m",
				maxactivefd + 1,
				fdbits(maxactivefd, &activefds));
		if (err == EBADF) {
			int j, b;
			fds = activefds;
			for (j = 0; j <= maxactivefd; j++)
				if ((FD_ISSET(j, &fds)
				    && (read(j, &b, 0) == -1)))
					msyslog(LOG_ERR,
						"Bad file descriptor %d",
						j);
		}
		return;
	}
	else if (n == 0)
		return;

	++handler_pkts;

#ifdef REFCLOCK
	/*
	 * Check out the reference clocks first, if any
	 */

	if (refio != NULL) {
		register struct refclockio *rp;

		for (rp = refio; rp != NULL; rp = rp->next) {
			fd = rp->fd;

			if (FD_ISSET(fd, &fds))
				do {
					++select_count;
					buflen = read_refclock_packet(
							fd, rp, ts);
				} while (buflen > 0);
		}
	}
#endif /* REFCLOCK */

	/*
	 * Loop through the interfaces looking for data to read.
	 */
	for (interface = inter_list;
	     interface != NULL;
	     interface = interface->link) {

		for (doing = 0; (doing < 2); doing++) {
			if (!doing)
				fd = interface->fd;
			else {
				if (!(interface->flags & INT_BCASTOPEN))
					break;
				fd = interface->bfd;
			}
			if (fd < 0)
				continue;
			if (FD_ISSET(fd, &fds))
				do {
					++select_count;
					buflen = read_network_packet(
							fd, interface,
							ts);
				} while (buflen > 0);
			/* Check more interfaces */
		}
	}

#ifdef HAS_ROUTING_SOCKET
	/*
	 * scan list of asyncio readers - currently only used for routing sockets
	 */
	asyncio_reader = asyncio_reader_list;

	while (asyncio_reader != NULL) {
		struct asyncio_reader *next = asyncio_reader->link;

		if (FD_ISSET(asyncio_reader->fd, &fds)) {
			++select_count;
			(asyncio_reader->receiver)(asyncio_reader);
		}

		asyncio_reader = next;
	}
#endif /* HAS_ROUTING_SOCKET */
	
	/*
	 * Done everything from that select.
	 */

	/*
	 * If nothing to do, just return.
	 * If an error occurred, complain and return.
	 */
	if (select_count == 0) { /* We really had nothing to do */
#ifdef DEBUG
		if (debug)
			msyslog(LOG_DEBUG, "input_handler: select() returned 0");
#endif
		return;
	}
	/* We've done our work */
#ifdef DEBUG_TIMING
	get_systime(&ts_e);
	/*
	 * (ts_e - ts) is the amount of time we spent
	 * processing this gob of file descriptors.  Log
	 * it.
	 */
	L_SUB(&ts_e, &ts);
	collect_timing(NULL, "input handler", 1, &ts_e);
	if (debug > 3)
		msyslog(LOG_DEBUG,
			"input_handler: Processed a gob of fd's in %s msec",
			lfptoms(&ts_e, 6));
#endif
	/* just bail. */
	return;
}
#endif

/*
 * findinterface - find local interface corresponding to address
 */
struct interface *
findinterface(
	sockaddr_u *addr
	)
{
	struct interface *iface;
	
	iface = findlocalinterface(addr, INT_WILDCARD, 0);

	if (NULL == iface) {
		DPRINTF(4, ("Found no interface for address %s - returning wildcard\n",
			    stoa(addr)));

		iface = ANY_INTERFACE_CHOOSE(addr);
	} else
		DPRINTF(4, ("Found interface #%d %s for address %s\n",
			    iface->ifnum, iface->name, stoa(addr)));

	return iface;
}

/*
 * findlocalinterface - find local interface corresponding to addr,
 * which does not have any of flags set.  If bast is nonzero, addr is
 * a broadcast address.
 *
 * This code attempts to find the local sending address for an outgoing
 * address by connecting a new socket to destinationaddress:NTP_PORT
 * and reading the sockname of the resulting connect.
 * the complicated sequence simulates the routing table lookup
 * for to first hop without duplicating any of the routing logic into
 * ntpd. preferably we would have used an API call - but its not there -
 * so this is the best we can do here short of duplicating to entire routing
 * logic in ntpd which would be a silly and really unportable thing to do.
 *
 */
static struct interface *
findlocalinterface(
	sockaddr_u *	addr,
	int		flags,
	int		bcast
	)
{
	GETSOCKNAME_SOCKLEN_TYPE	sockaddrlen;
	struct interface *		iface;
	sockaddr_u			saddr;
	SOCKET				s;
	int				rtn;
	int				on;

	DPRINTF(4, ("Finding interface for addr %s in list of addresses\n",
		    stoa(addr)));
	
	s = socket(AF(addr), SOCK_DGRAM, 0);
	if (INVALID_SOCKET == s)
		return NULL;

	/*
	 * If we are looking for broadcast interface we need to set this
	 * socket to allow broadcast
	 */
	if (bcast) {
		on = 1;
		setsockopt(s, SOL_SOCKET, SO_BROADCAST,
			   (char *)&on, sizeof(on));
	}

	rtn = connect(s, &addr->sa, SOCKLEN(addr));
	if (SOCKET_ERROR == rtn) {
		closesocket(s);
		return NULL;
	}

	sockaddrlen = sizeof(saddr);
	rtn = getsockname(s, &saddr.sa, &sockaddrlen);
	closesocket(s);

	if (SOCKET_ERROR == rtn)
		return NULL;

	DPRINTF(4, ("findlocalinterface: kernel maps %s to %s\n",
		    stoa(addr), stoa(&saddr)));
	
	iface = getinterface(&saddr, flags);

	/* 
	 * if we didn't find an exact match on saddr check for an
	 * interface on the same subnet as saddr.  This handles the
	 * case of the address suggested by the kernel being
	 * excluded by the user's -I and -L options to ntpd, when
	 * another address is enabled on the same subnet.
	 * See http://bugs.ntp.org/1184 for more detail.
	 */
	if (NULL == iface || iface->ignore_packets)
		iface = getsamenetinterface(&saddr, flags);

	/* Don't use an interface which will ignore replies */
	if (iface != NULL && iface->ignore_packets)
		iface = NULL;

	return iface;
}


/*
 * fetch an interface structure the matches the
 * address and has the given flags NOT set
 */
static struct interface *
getinterface(
	sockaddr_u *	addr, 
	int		flags
	)
{
	struct interface *iface;
	
	iface = find_addr_in_list(addr);

	if (iface != NULL && (iface->flags & flags))
		iface = NULL;
	
	return iface;
}


/*
 * fetch an interface structure with a local address on the same subnet
 * as addr which has the given flags NOT set
 */
static struct interface *
getsamenetinterface(
	sockaddr_u *	addr,
	int		flags
	)
{
	struct interface *iface;

	iface = find_samenet_addr_in_list(addr);

	if (iface != NULL && (iface->flags & flags))
		iface = NULL;

	return iface;
}


/*
 * findlocalcastinterface - find local *cast interface for addr
 */
static struct interface *
findlocalcastinterface(
	sockaddr_u *	addr
	)
{
	struct interface *	iface;
	struct interface *	nif;
#ifdef INCLUDE_IPV6_MULTICAST_SUPPORT
	isc_boolean_t		want_linklocal;
#endif 

	NTP_REQUIRE(addr_ismulticast(addr));

	/*
	 * see how kernel maps the mcast address
	 */
	nif = findlocalinterface(addr, 0, 0);

	if (nif != NULL && !nif->ignore_packets) {
		DPRINTF(2, ("findlocalcastinterface: kernel recommends interface #%d %s for %s\n",
			    nif->ifnum, nif->name, stoa(addr)));
		return nif;
	}

#ifdef INCLUDE_IPV6_MULTICAST_SUPPORT
	want_linklocal = (IN6_IS_ADDR_MC_LINKLOCAL(PSOCK_ADDR6(addr))
		       || IN6_IS_ADDR_MC_SITELOCAL(PSOCK_ADDR6(addr)));
#endif

	for (iface = inter_list;
	     iface != NULL;
	     iface = iface->link) 
	  {
		/* use only allowed addresses */
		if (iface->ignore_packets)
			continue;

		/* Skip the loopback and wildcard addresses */
		if (iface->flags & (INT_LOOPBACK | INT_WILDCARD))
			continue;

		/* Skip if different family */
		if (AF(&iface->sin) != AF(addr))
			continue;

		/* Is it multicast capable? */
		if (!(iface->flags & INT_MULTICAST))
			continue;

#ifdef INCLUDE_IPV6_MULTICAST_SUPPORT
		if (want_linklocal && IS_IPV6(&iface->sin) &&
		    IN6_IS_ADDR_LINKLOCAL(PSOCK_ADDR6(&iface->sin))) {
			nif = iface;
			break;
		}
		/* If we want a linklocal address, skip */
		if (want_linklocal)
			continue;
#endif
		nif = iface;
		break;
	}	/* for loop over interfaces */

	if (nif != NULL)
		DPRINTF(3, ("findlocalcastinterface: found interface #%d %s for %s\n",
			    nif->ifnum, nif->name, stoa(addr)));
	else
		DPRINTF(3, ("findlocalcastinterface: no interface found for %s\n",
			    stoa(addr)));
	return nif;
}


/*
 * findbcastinter - find broadcast interface corresponding to address
 */
struct interface *
findbcastinter(
	sockaddr_u *addr
	)
{
#if !defined(MPE) && (defined(SIOCGIFCONF) || defined(SYS_WINNT))
	struct interface *iface;
	
	
	DPRINTF(4, ("Finding broadcast/multicast interface for addr %s in list of addresses\n",
		    stoa(addr)));

	iface = findlocalinterface(addr, INT_LOOPBACK | INT_WILDCARD,
				   1);	
	if (iface != NULL) {
		DPRINTF(4, ("Easily found bcast-/mcast- interface index #%d %s\n",
			    iface->ifnum, iface->name));
		return iface;
	}

	/*
	 * plan B - try to find something reasonable in our lists in
	 * case kernel lookup doesn't help
	 */
	for (iface = inter_list;
	     iface != NULL;
	     iface = iface->link) 
	{
		if (iface->flags & INT_WILDCARD)
			continue;
		
		/* Don't bother with ignored interfaces */
		if (iface->ignore_packets)
			continue;
		
		/*
		 * First look if this is the correct family
		 */
		if(AF(&iface->sin) != AF(addr))
			continue;

		/* Skip the loopback addresses */
		if (iface->flags & INT_LOOPBACK)
			continue;

		/*
		 * If we are looking to match a multicast address and
		 * this interface is one...
		 */
		if (addr_ismulticast(addr) 
		    && (iface->flags & INT_MULTICAST)) {
#ifdef INCLUDE_IPV6_SUPPORT
			/*
			 * ...it is the winner unless we're looking for
			 * an interface to use for link-local multicast
			 * and its address is not link-local.
			 */
			if (IS_IPV6(addr)
			    && IN6_IS_ADDR_MC_LINKLOCAL(PSOCK_ADDR6(addr))
			    && !IN6_IS_ADDR_LINKLOCAL(PSOCK_ADDR6(&iface->sin)))
				continue;
#endif
			break;
		}

		/*
		 * We match only those interfaces marked as
		 * broadcastable and either the explicit broadcast
		 * address or the network portion of the IP address.
		 * Sloppy.
		 */
		if (IS_IPV4(addr)) {
			if (SOCK_EQ(&iface->bcast, addr))
				break;

			if ((NSRCADR(&iface->sin) & NSRCADR(&iface->mask)) 
			    == (NSRCADR(addr)	  & NSRCADR(&iface->mask)))
				break;
		}
#ifdef INCLUDE_IPV6_SUPPORT
		else if(IS_IPV6(addr)) {
			if (SOCK_EQ(&iface->bcast, addr))
				break;

			if (SOCK_EQ(netof(&iface->sin), netof(addr)))
				break;
		}
#endif
	}
#endif /* SIOCGIFCONF */
	if (NULL == iface) {
		DPRINTF(4, ("No bcast interface found for %s\n",
			    stoa(addr)));
		iface = ANY_INTERFACE_CHOOSE(addr);
	} else
		DPRINTF(4, ("Found bcast-/mcast- interface index #%d %s\n",
			    iface->ifnum, iface->name));
	return iface;
}


/*
 * io_clr_stats - clear I/O module statistics
 */
void
io_clr_stats(void)
{
	packets_dropped = 0;
	packets_ignored = 0;
	packets_received = 0;
	packets_sent = 0;
	packets_notsent = 0;

	handler_calls = 0;
	handler_pkts = 0;
	io_timereset = current_time;
}


#ifdef REFCLOCK
/*
 * io_addclock - add a reference clock to the list and arrange that we
 *				 get SIGIO interrupts from it.
 */
int
io_addclock(
	struct refclockio *rio
	)
{
	BLOCKIO();

	/*
	 * Stuff the I/O structure in the list and mark the descriptor
	 * in use.  There is a harmless (I hope) race condition here.
	 */
	rio->next = refio;

# ifdef HAVE_SIGNALED_IO
	if (init_clock_sig(rio)) {
		UNBLOCKIO();
		return 0;
	}
# elif defined(HAVE_IO_COMPLETION_PORT)
	if (io_completion_port_add_clock_io(rio)) {
		UNBLOCKIO();
		return 0;
	}
# endif

	/*
	 * enqueue
	 */
	refio = rio;

	/*
	 * register fd
	 */
	add_fd_to_list(rio->fd, FD_TYPE_FILE);

	UNBLOCKIO();
	return 1;
}

/*
 * io_closeclock - close the clock in the I/O structure given
 */
void
io_closeclock(
	struct refclockio *rio
	)
{
	register struct refclockio *rp;

	BLOCKIO();

	/*
	 * Remove structure from the list
	 */
	if (refio == rio)
		refio = rio->next;
	else {
		for (rp = refio; rp != NULL; rp = rp->next)
			if (rp->next == rio) {
				rp->next = rio->next;
				break;
			}

		if (NULL == rp) {
			UNBLOCKIO();
			return;
		}
	}

	/*
	 * Close the descriptor.
	 */
	close_and_delete_fd_from_list(rio->fd);
	UNBLOCKIO();
}
#endif	/* REFCLOCK */

/*
 * On NT a SOCKET is an unsigned int so we cannot possibly keep it in
 * an array. So we use one of the ISC_LIST functions to hold the
 * socket value and use that when we want to enumerate it.
 *
 * This routine is called by the forked intres child process to close
 * all open sockets.  On Windows there's no need as intres runs in
 * the same process as a thread.
 */
#ifndef SYS_WINNT
void
kill_asyncio(int startfd)
{
	BLOCKIO();

	/*
	 * In the child process we do not maintain activefds and
	 * maxactivefd.  Zeroing maxactivefd disables code which
	 * maintains it in close_and_delete_fd_from_list().
	 */
	maxactivefd = 0;

	while (fd_list != NULL)
		close_and_delete_fd_from_list(fd_list->fd);

	UNBLOCKIO();
}
#endif	/* !SYS_WINNT */

/*
 * Add and delete functions for the list of open sockets
 */
static void
add_fd_to_list(
	SOCKET fd,
	enum desc_type type
	)
{
	vsock_t *lsock = emalloc(sizeof(*lsock));

	lsock->fd = fd;
	lsock->type = type;

	LINK_SLIST(fd_list, lsock, link);
	/*
	 * I/O Completion Ports don't care about the select and FD_SET
	 */
#ifndef HAVE_IO_COMPLETION_PORT
	if (fd < 0 || fd >= FD_SETSIZE) {
		msyslog(LOG_ERR,
			"Too many sockets in use, FD_SETSIZE %d exceeded",
			FD_SETSIZE);
		exit(1);
	}
	/*
	 * keep activefds in sync
	 */
	maxactivefd = max(fd, maxactivefd);

	FD_SET(fd, &activefds);
#endif
}

static void
close_and_delete_fd_from_list(
	SOCKET fd
	) 
{
	vsock_t *lsock;

	UNLINK_EXPR_SLIST(lsock, fd_list, fd == 
	    UNLINK_EXPR_SLIST_CURRENT()->fd, link, vsock_t);

	if (lsock != NULL) {
		switch (lsock->type) {
		case FD_TYPE_SOCKET:
			closesocket(lsock->fd);
			break;

		case FD_TYPE_FILE:
			close(lsock->fd);
			break;

		default:
			msyslog(LOG_ERR,
				"internal error - illegal descriptor type %d - EXITING",
				(int)lsock->type);
			exit(1);
		}

		free(lsock);
		/*
		 * I/O Completion Ports don't care about select and fd_set
		 */
#ifndef HAVE_IO_COMPLETION_PORT
		/*
		 * remove from activefds
		 */
		FD_CLR(fd, &activefds);
		
		if (fd == maxactivefd && maxactivefd) {
			int i;
			NTP_INSIST(maxactivefd - 1 < FD_SETSIZE);
			for (i = maxactivefd - 1; i >= 0; i--)
				if (FD_ISSET(i, &activefds)) {
					maxactivefd = i;
					break;
				}
			NTP_INSIST(fd != maxactivefd);
		}
#endif
	}
}

static void
add_addr_to_list(
	sockaddr_u *addr,
	struct interface *interface
	)
{
	remaddr_t *laddr;

#ifdef DEBUG
	if (find_addr_in_list(addr) == NULL) {
#endif
		/* not there yet - add to list */
		laddr = emalloc(sizeof(*laddr));
		memcpy(&laddr->addr, addr, sizeof(laddr->addr));
		laddr->interface = interface;
		
		LINK_SLIST(remoteaddr_list, laddr, link);
		
		DPRINTF(4, ("Added addr %s to list of addresses\n",
			    stoa(addr)));
#ifdef DEBUG
	} else
		DPRINTF(4, ("WARNING: Attempt to add duplicate addr %s to address list\n",
			    stoa(addr)));
#endif
}


static void
delete_addr_from_list(
	sockaddr_u *addr
	) 
{
	remaddr_t *unlinked;
	
	UNLINK_EXPR_SLIST(unlinked, remoteaddr_list, SOCK_EQ(addr,
		&(UNLINK_EXPR_SLIST_CURRENT()->addr)), link, remaddr_t);

	if (unlinked != NULL) {
		DPRINTF(4, ("Deleted addr %s from list of addresses\n",
			stoa(addr)));
		free(unlinked);
	}
}


static void
delete_interface_from_list(
	struct interface *iface
	)
{
	remaddr_t *unlinked;

	do {
		UNLINK_EXPR_SLIST(unlinked, remoteaddr_list, iface ==
		    UNLINK_EXPR_SLIST_CURRENT()->interface, link,
		    remaddr_t);

		if (unlinked != NULL) {
			DPRINTF(4, ("Deleted addr %s for interface #%d %s from list of addresses\n",
				stoa(&unlinked->addr), iface->ifnum,
				iface->name));
			if (addr_ismulticast(&unlinked->addr))
				/* find a new interface to use */
				io_multicast_add(&unlinked->addr);
			free(unlinked);
		}
	} while (unlinked != NULL);
}


static struct interface *
find_addr_in_list(
	sockaddr_u *addr
	) 
{
	remaddr_t *entry;
	
	DPRINTF(4, ("Searching for addr %s in list of addresses - ",
		    stoa(addr)));

	for (entry = remoteaddr_list;
	     entry != NULL;
	     entry = entry->link)
		if (SOCK_EQ(&entry->addr, addr)) {
			DPRINTF(4, ("FOUND\n"));
			return entry->interface;
		}

	DPRINTF(4, ("NOT FOUND\n"));
	return NULL;
}

static inline isc_boolean_t
same_network_v4(
	struct sockaddr_in *addr1,
	struct sockaddr_in *mask,
	struct sockaddr_in *addr2
	)
{
	return (addr1->sin_addr.s_addr & mask->sin_addr.s_addr)
	       == (addr2->sin_addr.s_addr & mask->sin_addr.s_addr);
}

#ifdef INCLUDE_IPV6_SUPPORT
static inline isc_boolean_t
same_network_v6(
	struct sockaddr_in6 *addr1,
	struct sockaddr_in6 *mask,
	struct sockaddr_in6 *addr2
	)
{
	size_t i;

	for (i = 0; 
	     i < sizeof(addr1->sin6_addr.s6_addr) / 
	         sizeof(addr1->sin6_addr.s6_addr[0]);
	     i++)

		if ((addr1->sin6_addr.s6_addr[i] &
		     mask->sin6_addr.s6_addr[i]) 
		    !=
		    (addr2->sin6_addr.s6_addr[i] &
		     mask->sin6_addr.s6_addr[i]))

			return ISC_FALSE;

	return ISC_TRUE;
}
#endif	/* INCLUDE_IPV6_SUPPORT */


static isc_boolean_t
same_network(
	sockaddr_u *a1,
	sockaddr_u *mask,
	sockaddr_u *a2
	)
{
	isc_boolean_t sn;

	if (AF(a1) != AF(a2))
		sn = ISC_FALSE;
	else if (IS_IPV4(a1))
		sn = same_network_v4(&a1->sa4, &mask->sa4, &a2->sa4);
#ifdef INCLUDE_IPV6_SUPPORT
	else if (IS_IPV6(a1))
		sn = same_network_v6(&a1->sa6, &mask->sa6, &a2->sa6);
#endif
	else
		sn = ISC_FALSE;

	return sn;
}

/*
 * Find an address in the list on the same network as addr which is not
 * addr.
 */
static struct interface *
find_samenet_addr_in_list(
	sockaddr_u *addr
	) 
{
	remaddr_t *entry;

	DPRINTF(4, ("Searching for addr with same subnet as %s in list of addresses - ",
		    stoa(addr)));

	for (entry = remoteaddr_list;
	     entry != NULL;
	     entry = entry->link)

		if (!SOCK_EQ(addr, &entry->addr)
		    && same_network(&entry->addr, 
				    &entry->interface->mask,
				    addr)) {
			DPRINTF(4, ("FOUND\n"));
			return entry->interface;
		}

	DPRINTF(4, ("NOT FOUND\n"));
	return NULL;
}


/*
 * Find the given address with the all given flags set in the list
 */
static struct interface *
find_flagged_addr_in_list(
	sockaddr_u *	addr,
	int		flags
	)
{
	remaddr_t *entry;
	
	DPRINTF(4, ("Finding addr %s with flags %d in list: ",
		    stoa(addr), flags));

	for (entry = remoteaddr_list;
	     entry != NULL;
	     entry = entry->link)

		if (SOCK_EQ(&entry->addr, addr)
		    && (entry->interface->flags & flags) == flags) {

			DPRINTF(4, ("FOUND\n"));
			return entry->interface;
		}

	DPRINTF(4, ("NOT FOUND\n"));
	return NULL;
}


#ifdef HAS_ROUTING_SOCKET
# ifndef UPDATE_GRACE
#  define UPDATE_GRACE	2	/* wait UPDATE_GRACE seconds before scanning */
# endif

static void
process_routing_msgs(struct asyncio_reader *reader)
{
	char buffer[5120];
	int cnt, msg_type;
#ifdef HAVE_RTNETLINK
	struct nlmsghdr *nh;
#else
	struct rt_msghdr *rtm;
	char *p;
#endif
	
	if (disable_dynamic_updates) {
		/*
		 * discard ourselves if we are not needed any more
		 * usually happens when running unprivileged
		 */
		remove_asyncio_reader(reader);
		delete_asyncio_reader(reader);
		return;
	}

	cnt = read(reader->fd, buffer, sizeof(buffer));
	
	if (cnt < 0) {
		msyslog(LOG_ERR,
			"i/o error on routing socket %m - disabling");
		remove_asyncio_reader(reader);
		delete_asyncio_reader(reader);
		return;
	}

	/*
	 * process routing message
	 */
#ifdef HAVE_RTNETLINK
	for (nh = (struct nlmsghdr *)buffer;
	     NLMSG_OK(nh, cnt);
	     nh = NLMSG_NEXT(nh, cnt)) {
		msg_type = nh->nlmsg_type;
#else
	for (p = buffer;
	     (p + sizeof(struct rt_msghdr)) <= (buffer + cnt);
	     p += rtm->rtm_msglen) {
		rtm = (struct rt_msghdr *)p;
		if (rtm->rtm_version != RTM_VERSION) {
			msyslog(LOG_ERR,
				"version mismatch (got %d - expected %d) on routing socket - disabling",
				rtm->rtm_version, RTM_VERSION);

			remove_asyncio_reader(reader);
			delete_asyncio_reader(reader);
			return;
		}
		msg_type = rtm->rtm_type;
#endif
		switch (msg_type) {
#ifdef RTM_NEWADDR
		case RTM_NEWADDR:
#endif
#ifdef RTM_DELADDR
		case RTM_DELADDR:
#endif
#ifdef RTM_ADD
		case RTM_ADD:
#endif
#ifdef RTM_DELETE
		case RTM_DELETE:
#endif
#ifdef RTM_REDIRECT
		case RTM_REDIRECT:
#endif
#ifdef RTM_CHANGE
		case RTM_CHANGE:
#endif
#ifdef RTM_LOSING
		case RTM_LOSING:
#endif
#ifdef RTM_IFINFO
		case RTM_IFINFO:
#endif
#ifdef RTM_IFANNOUNCE
		case RTM_IFANNOUNCE:
#endif
#ifdef RTM_NEWLINK
		case RTM_NEWLINK:
#endif
#ifdef RTM_DELLINK
		case RTM_DELLINK:
#endif
#ifdef RTM_NEWROUTE
		case RTM_NEWROUTE:
#endif
#ifdef RTM_DELROUTE
		case RTM_DELROUTE:
#endif
			/*
			 * we are keen on new and deleted addresses and
			 * if an interface goes up and down or routing
			 * changes
			 */
			DPRINTF(3, ("routing message op = %d: scheduling interface update\n",
				    msg_type));
			timer_interfacetimeout(current_time + UPDATE_GRACE);
			break;
#ifdef HAVE_RTNETLINK
		case NLMSG_DONE:
			/* end of multipart message */
			return;
#endif
		default:
			/*
			 * the rest doesn't bother us.
			 */
			DPRINTF(4, ("routing message op = %d: ignored\n",
				    msg_type));
			break;
		}
	}
}

/*
 * set up routing notifications
 */
static void
init_async_notifications()
{
	struct asyncio_reader *reader;
#ifdef HAVE_RTNETLINK
	int fd = socket(PF_NETLINK, SOCK_RAW, NETLINK_ROUTE);
	struct sockaddr_nl sa;
#else
	int fd = socket(PF_ROUTE, SOCK_RAW, 0);
#endif
	if (fd < 0) {
		msyslog(LOG_ERR,
			"unable to open routing socket (%m) - using polled interface update");
		return;
	}

	fd = move_fd(fd);
#ifdef HAVE_RTNETLINK
	memset(&sa, 0, sizeof(sa));
	sa.nl_family = PF_NETLINK;
	sa.nl_groups = RTMGRP_LINK | RTMGRP_IPV4_IFADDR
		       | RTMGRP_IPV6_IFADDR | RTMGRP_IPV4_ROUTE
		       | RTMGRP_IPV4_MROUTE | RTMGRP_IPV6_ROUTE
		       | RTMGRP_IPV6_MROUTE;
	if (bind(fd, (struct sockaddr *)&sa, sizeof(sa)) < 0) {
		msyslog(LOG_ERR,
			"bind failed on routing socket (%m) - using polled interface update");
		return;
	}
#endif
	init_nonblocking_io(fd);
#if defined(HAVE_SIGNALED_IO)
	init_socket_sig(fd);
#endif /* HAVE_SIGNALED_IO */
	
	reader = new_asyncio_reader();

	reader->fd = fd;
	reader->receiver = process_routing_msgs;
	
	add_asyncio_reader(reader, FD_TYPE_SOCKET);
	msyslog(LOG_INFO,
		"Listening on routing socket on fd #%d for interface updates",
		fd);
}
#else
/* HAS_ROUTING_SOCKET not defined */
static void
init_async_notifications(void)
{
}
#endif
