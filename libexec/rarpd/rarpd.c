/*
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that: (1) source code distributions
 * retain the above copyright notice and this paragraph in its entirety, (2)
 * distributions including binary code include the above copyright notice and
 * this paragraph in its entirety in the documentation or other materials
 * provided with the distribution, and (3) all advertising materials mentioning
 * features or use of this software display the following acknowledgement:
 * ``This product includes software developed by the University of California,
 * Lawrence Berkeley Laboratory and its contributors.'' Neither the name of
 * the University nor the names of its contributors may be used to endorse
 * or promote products derived from this software without specific prior
 * written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */
#ifndef lint
char    copyright[] =
"@(#) Copyright (c) 1990 The Regents of the University of California.\n\
 All rights reserved.\n";
#endif				/* not lint */

#ifndef lint
static char rcsid[] =
"@(#) $Id: rarpd.c,v 1.1 1993/09/07 03:21:32 cassidy Exp $";
#endif


/*
 * rarpd - Reverse ARP Daemon
 *
 * Usage:	rarpd -a [ -d -f ]
 *		rarpd [ -d -f ] interface
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <syslog.h>
#include <string.h>
#include <strings.h>
#include <sys/types.h>
#include <unistd.h>
/* SunOS 4.x defines this while 3.x does not. */
#ifdef __sys_types_h
#define SUNOS4
#endif
#include <sys/time.h>
#include <net/bpf.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <netinet/in.h>
#include <netinet/if_ether.h>
#include <sys/errno.h>
#include <sys/file.h>
#include <netdb.h>
#include <arpa/inet.h>

#if defined (SUNOS4) || defined (i386) || defined (__i386__)
#include <dirent.h>
#else
#include <sys/dir.h>
#endif

/*
 * Map field names in ether_arp struct.  What a pain in the neck.
 * XXX This logic is brain-dead and needs to be cleaned up.
 */
#if !defined(i386) || !defined(__i386__)
#ifndef SUNOS4
#undef arp_sha
#undef arp_spa
#undef arp_tha
#undef arp_tpa
#define arp_sha arp_xsha
#define arp_spa arp_xspa
#define arp_tha arp_xtha
#define arp_tpa arp_xtpa
#endif
#endif				/* !i386 */

/* define these if missing */
#ifndef ETHERTYPE_REVARP	/* reverse address resolution protocol */
#define ETHERTYPE_REVARP 0x8035
#endif
#ifndef REVARP_REPLY		/* reverse address resolution reply */
#define REVARP_REPLY	4
#endif
#ifndef REVARP_REQUEST		/* reverse address resolution request */
#define REVARP_REQUEST	3
#endif

#ifndef __GNUC__
#define inline
#endif

#define FATAL		1	/* fatal error occurred */
#define NONFATAL	0	/* non fatal error occurred */

extern int errno;

/*
 * The structure for each interface.
 */
struct if_info {
	int     ii_fd;		/* BPF file descriptor */
	u_char  ii_eaddr[6];	/* Ethernet address of this interface */
	u_long  ii_ipaddr;	/* IP address of this interface */
	u_long  ii_netmask;	/* subnet or net mask */
	struct if_info *ii_next;
};
/*
 * The list of all interfaces that are being listened to.  rarp_loop()
 * "selects" on the descriptors in this list.
 */
struct if_info *iflist;

int    rarp_open     __P((char *));
int    rarp_bootable __P((u_long));
char   *intoa        __P((u_long));
long   tell          __P((int));
void   init_one      __P((char *));
void   init_all      __P((void));
void   rarp_loop     __P((void));
void   lookup_eaddr  __P((int, u_char *));
void   lookup_ipaddr __P((char *, u_long *, u_long *));
void   usage         __P((void));
void   rarp_process  __P((struct if_info *, u_char *));
void   rarp_reply    __P((struct if_info *, struct ether_header *, u_long));
void   update_arptab __P((u_char *, u_long));
void   err           __P((int, const char *,...));
void   debug         __P((const char *,...));
u_long ipaddrtonetmask __P((u_long));

int     aflag = 0;		/* listen on "all" interfaces  */
int     dflag = 0;		/* print debugging messages */
int     fflag = 0;		/* don't fork */

void
main(argc, argv)
	int     argc;
	char  **argv;
{
	int     op, pid, devnull, f;
	char   *ifname, *hostname, *name;

	extern char *optarg;
	extern int optind, opterr;

	if (name = strrchr(argv[0], '/'))
		++name;
	else
		name = argv[0];
	if (*name == '-')
		++name;

	/* All error reporting is done through syslogs. */
	openlog(name, LOG_PID | LOG_CONS, LOG_DAEMON);

	opterr = 0;
	while ((op = getopt(argc, argv, "adf")) != EOF) {
		switch (op) {
		case 'a':
			++aflag;
			break;

		case 'd':
			++dflag;
			break;

		case 'f':
			++fflag;
			break;

		default:
			usage();
			/* NOTREACHED */
		}
	}
	ifname = argv[optind++];
	hostname = ifname ? argv[optind] : 0;
	if ((aflag && ifname) || (!aflag && ifname == 0))
		usage();

	if (aflag)
		init_all();
	else
		init_one(ifname);

	if ((!fflag) || (!dflag)) {
		pid = fork();
		if (pid > 0)
			/* Parent exits, leaving child in background. */
			exit(0);
		else
			if (pid == -1) {
				err(FATAL, "cannot fork");
				/* NOTREACHED */
			}
		/* Fade into the background */
		f = open("/dev/tty", O_RDWR);
		if (f >= 0) {
			if (ioctl(f, TIOCNOTTY, 0) < 0) {
				err(FATAL, "TIOCNOTTY: %s", strerror(errno));
				/* NOTREACHED */
			}
			(void) close(f);
		}
		(void) chdir("/");
		(void) setpgrp(0, getpid());
		devnull = open("/dev/null", O_RDWR);
		if (devnull >= 0) {
			(void) dup2(devnull, 0);
			(void) dup2(devnull, 1);
			(void) dup2(devnull, 2);
			if (devnull > 2)
				(void) close(devnull);
		}
	}
	rarp_loop();
}
/*
 * Add 'ifname' to the interface list.  Lookup its IP address and network
 * mask and Ethernet address, and open a BPF file for it.
 */
void
init_one(ifname)
	char   *ifname;
{
	struct if_info *p;


	p = (struct if_info *) malloc(sizeof(*p));
	if (p == 0) {
		err(FATAL, "malloc: %s", strerror(errno));
		/* NOTREACHED */
	}
	p->ii_next = iflist;
	iflist = p;

	p->ii_fd = rarp_open(ifname);
	lookup_eaddr(p->ii_fd, p->ii_eaddr);
	lookup_ipaddr(ifname, &p->ii_ipaddr, &p->ii_netmask);
}
/*
 * Initialize all "candidate" interfaces that are in the system
 * configuration list.  A "candidate" is up, not loopback and not
 * point to point.
 */
void
init_all()
{
	int     fd;
	int     n;
	struct ifreq ibuf[8], *ifrp;
	struct ifconf ifc;

	if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		err(FATAL, "socket: %s", strerror(errno));
		/* NOTREACHED */
	}
	ifc.ifc_len = sizeof ibuf;
	ifc.ifc_buf = (caddr_t) ibuf;
	if (ioctl(fd, SIOCGIFCONF, (char *) &ifc) < 0 ||
	    ifc.ifc_len < sizeof(struct ifreq)) {
		err(FATAL, "SIOCGIFCONF: %s", strerror(errno));
		/* NOTREACHED */
	}
	ifrp = ibuf;
	n = ifc.ifc_len / sizeof(*ifrp);
	for (; --n >= 0; ++ifrp) {
		if (ioctl(fd, SIOCGIFFLAGS, (char *) ifrp) < 0) {
			err(FATAL, "SIOCGIFFLAGS: %s", strerror(errno));
			/* NOTREACHED */
		}
		if ((ifrp->ifr_flags & IFF_UP) == 0 ||
		    ifrp->ifr_flags & IFF_LOOPBACK ||
		    ifrp->ifr_flags & IFF_POINTOPOINT)
			continue;
		init_one(ifrp->ifr_name);
	}
	(void) close(fd);
}

void
usage()
{
	(void) fprintf(stderr, "usage: rarpd -a [ -d -f ]\n");
	(void) fprintf(stderr, "       rarpd [ -d -f ] interface\n");
	exit(1);
}

static int
bpf_open()
{
	int     fd;
	int     n = 0;
	char    device[sizeof "/dev/bpf000"];

	/* Go through all the minors and find one that isn't in use. */
	do {
		(void) sprintf(device, "/dev/bpf%d", n++);
		fd = open(device, O_RDWR);
	} while (fd < 0 && errno == EBUSY);

	if (fd < 0) {
		err(FATAL, "%s: %s", device, strerror(errno));
		/* NOTREACHED */
	}
	return fd;
}
/*
 * Open a BPF file and attach it to the interface named 'device'.
 * Set immediate mode, and set a filter that accepts only RARP requests.
 */
int
rarp_open(device)
	char   *device;
{
	int     fd;
	struct ifreq ifr;
	u_int   dlt;
	int     immediate;

	static struct bpf_insn insns[] = {
		BPF_STMT(BPF_LD | BPF_H | BPF_ABS, 12),
		BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, ETHERTYPE_REVARP, 0, 3),
		BPF_STMT(BPF_LD | BPF_H | BPF_ABS, 20),
		BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, REVARP_REQUEST, 0, 1),
		BPF_STMT(BPF_RET | BPF_K, sizeof(struct ether_arp) +
		    sizeof(struct ether_header)),
		BPF_STMT(BPF_RET | BPF_K, 0),
	};
	static struct bpf_program filter = {
		sizeof insns / sizeof(insns[0]),
		insns
	};

	fd = bpf_open();

	/* Set immediate mode so packets are processed as they arrive. */
	immediate = 1;
	if (ioctl(fd, BIOCIMMEDIATE, &immediate) < 0) {
		err(FATAL, "BIOCIMMEDIATE: %s", strerror(errno));
		/* NOTREACHED */
	}
	(void) strncpy(ifr.ifr_name, device, sizeof ifr.ifr_name);
	if (ioctl(fd, BIOCSETIF, (caddr_t) & ifr) < 0) {
		err(FATAL, "BIOCSETIF: %s", strerror(errno));
		/* NOTREACHED */
	}
	/* Check that the data link layer is an Ethernet; this code won't work
	 * with anything else. */
	if (ioctl(fd, BIOCGDLT, (caddr_t) & dlt) < 0) {
		err(FATAL, "BIOCGDLT: %s", strerror(errno));
		/* NOTREACHED */
	}
	if (dlt != DLT_EN10MB) {
		err(FATAL, "%s is not an ethernet", device);
		/* NOTREACHED */
	}
	/* Set filter program. */
	if (ioctl(fd, BIOCSETF, (caddr_t) & filter) < 0) {
		err(FATAL, "BIOCSETF: %s", strerror(errno));
		/* NOTREACHED */
	}
	return fd;
}
/*
 * Perform various sanity checks on the RARP request packet.  Return
 * false on failure and log the reason.
 */
static int
rarp_check(p, len)
	u_char *p;
	int     len;
{
	struct ether_header *ep = (struct ether_header *) p;
	struct ether_arp *ap = (struct ether_arp *) (p + sizeof(*ep));

	(void) debug("got a packet");

	if (len < sizeof(*ep) + sizeof(*ap)) {
		err(NONFATAL, "truncated request");
		return 0;
	}
	/* XXX This test might be better off broken out... */
	if (ep->ether_type != ETHERTYPE_REVARP ||
	    ap->arp_hrd != ARPHRD_ETHER ||
	    ap->arp_op != REVARP_REQUEST ||
	    ap->arp_pro != ETHERTYPE_IP ||
	    ap->arp_hln != 6 || ap->arp_pln != 4) {
		err(NONFATAL, "request fails sanity check");
		return 0;
	}
	if (bcmp((char *) &ep->ether_shost, (char *) &ap->arp_sha, 6) != 0) {
		err(NONFATAL, "ether/arp sender address mismatch");
		return 0;
	}
	if (bcmp((char *) &ap->arp_sha, (char *) &ap->arp_tha, 6) != 0) {
		err(NONFATAL, "ether/arp target address mismatch");
		return 0;
	}
	return 1;
}
#ifndef FD_SETSIZE
#define FD_SET(n, fdp) ((fdp)->fds_bits[0] |= (1 << (n)))
#define FD_ISSET(n, fdp) ((fdp)->fds_bits[0] & (1 << (n)))
#define FD_ZERO(fdp) ((fdp)->fds_bits[0] = 0)
#endif

/*
 * Loop indefinitely listening for RARP requests on the
 * interfaces in 'iflist'.
 */
void
rarp_loop()
{
	u_char *buf, *bp, *ep;
	int     cc, fd;
	fd_set  fds, listeners;
	int     bufsize, maxfd = 0;
	struct if_info *ii;

	if (iflist == 0) {
		err(FATAL, "no interfaces");
		/* NOTREACHED */
	}
	if (ioctl(iflist->ii_fd, BIOCGBLEN, (caddr_t) & bufsize) < 0) {
		err(FATAL, "BIOCGBLEN: %s", strerror(errno));
		/* NOTREACHED */
	}
	buf = (u_char *) malloc((unsigned) bufsize);
	if (buf == 0) {
		err(FATAL, "malloc: %s", strerror(errno));
		/* NOTREACHED */
	}
	/*
         * Find the highest numbered file descriptor for select().
         * Initialize the set of descriptors to listen to.
         */
	FD_ZERO(&fds);
	for (ii = iflist; ii; ii = ii->ii_next) {
		FD_SET(ii->ii_fd, &fds);
		if (ii->ii_fd > maxfd)
			maxfd = ii->ii_fd;
	}
	while (1) {
		listeners = fds;
		if (select(maxfd + 1, &listeners, (struct fd_set *) 0,
			(struct fd_set *) 0, (struct timeval *) 0) < 0) {
			err(FATAL, "select: %s", strerror(errno));
			/* NOTREACHED */
		}
		for (ii = iflist; ii; ii = ii->ii_next) {
			fd = ii->ii_fd;
			if (!FD_ISSET(fd, &listeners))
				continue;
	again:
			cc = read(fd, (char *) buf, bufsize);
			/* Don't choke when we get ptraced */
			if (cc < 0 && errno == EINTR)
				goto again;
			/* Due to a SunOS bug, after 2^31 bytes, the file
			 * offset overflows and read fails with EINVAL.  The
			 * lseek() to 0 will fix things. */
			if (cc < 0) {
				if (errno == EINVAL &&
				    (long) (tell(fd) + bufsize) < 0) {
					(void) lseek(fd, 0, 0);
					goto again;
				}
				err(FATAL, "read: %s", strerror(errno));
				/* NOTREACHED */
			}
			/* Loop through the packet(s) */
#define bhp ((struct bpf_hdr *)bp)
			bp = buf;
			ep = bp + cc;
			while (bp < ep) {
				register int caplen, hdrlen;

				caplen = bhp->bh_caplen;
				hdrlen = bhp->bh_hdrlen;
				if (rarp_check(bp + hdrlen, caplen))
					rarp_process(ii, bp + hdrlen);
				bp += BPF_WORDALIGN(hdrlen + caplen);
			}
		}
	}
}
#ifndef TFTP_DIR
#define TFTP_DIR "/tftpboot"
#endif

/*
 * True if this server can boot the host whose IP address is 'addr'.
 * This check is made by looking in the tftp directory for the
 * configuration file.
 */
int
rarp_bootable(addr)
	u_long  addr;
{
#if defined (SUNOS4) || defined (i386) || defined (__i386__)
	register struct dirent *dent;
#else
	register struct direct *dent;
#endif
	register DIR *d;
	char    ipname[9];
	static DIR *dd = 0;

	(void) sprintf(ipname, "%08X", addr);
	/* If directory is already open, rewind it.  Otherwise, open it. */
	if (d = dd)
		rewinddir(d);
	else {
		if (chdir(TFTP_DIR) == -1) {
			err(FATAL, "chdir: %s", strerror(errno));
			/* NOTREACHED */
		}
		d = opendir(".");
		if (d == 0) {
			err(FATAL, "opendir: %s", strerror(errno));
			/* NOTREACHED */
		}
		dd = d;
	}
	while (dent = readdir(d))
		if (strncmp(dent->d_name, ipname, 8) == 0)
			return 1;
	return 0;
}
/*
 * Given a list of IP addresses, 'alist', return the first address that
 * is on network 'net'; 'netmask' is a mask indicating the network portion
 * of the address.
 */
u_long
choose_ipaddr(alist, net, netmask)
	u_long **alist;
	u_long  net;
	u_long  netmask;
{
	for (; *alist; ++alist) {
		if ((**alist & netmask) == net)
			return **alist;
	}
	return 0;
}
/*
 * Answer the RARP request in 'pkt', on the interface 'ii'.  'pkt' has
 * already been checked for validity.  The reply is overlaid on the request.
 */
void
rarp_process(ii, pkt)
	struct if_info *ii;
	u_char *pkt;
{
	struct ether_header *ep;
	struct hostent *hp;
	u_long  target_ipaddr;
	char    ename[256];

	ep = (struct ether_header *) pkt;

#ifdef XXX_TEMP_HACK
	if (ether_ntohost(ename, &ep->ether_shost) != 0 ||
	    (hp = gethostbyname(ename)) == 0)
		return;
#endif
	/* Choose correct address from list. */
	if (hp->h_addrtype != AF_INET) {
		err(FATAL, "cannot handle non IP addresses");
		/* NOTREACHED */
	}
	target_ipaddr = choose_ipaddr((u_long **) hp->h_addr_list,
	    ii->ii_ipaddr & ii->ii_netmask,
	    ii->ii_netmask);

	if (target_ipaddr == 0) {
		err(NONFATAL, "cannot find %s on net %s\n",
		    ename, intoa(ii->ii_ipaddr & ii->ii_netmask));
		return;
	}
	if (rarp_bootable(target_ipaddr))
		rarp_reply(ii, ep, target_ipaddr);
}
/*
 * Lookup the ethernet address of the interface attached to the BPF
 * file descriptor 'fd'; return it in 'eaddr'.
 */
void
lookup_eaddr(fd, eaddr)
	int     fd;
	u_char *eaddr;
{
	struct ifreq ifr;

	/* Use BPF descriptor to get ethernet address. */
	if (ioctl(fd, SIOCGIFADDR, (char *) &ifr) < 0) {
		err(FATAL, "lookup_eaddr: SIOCGIFADDR: %s", strerror(errno));
		/* NOTREACHED */
	}
	bcopy((char *) &ifr.ifr_addr.sa_data[0], (char *) eaddr, 6);
}
/*
 * Lookup the IP address and network mask of the interface named 'ifname'.
 */
void
lookup_ipaddr(ifname, addrp, netmaskp)
	char   *ifname;
	u_long *addrp;
	u_long *netmaskp;
{
	int     fd;
	struct ifreq ifr;

	/* Use datagram socket to get IP address. */
	if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		err(FATAL, "socket: %s", strerror(errno));
		/* NOTREACHED */
	}
	(void) strncpy(ifr.ifr_name, ifname, sizeof ifr.ifr_name);
	if (ioctl(fd, SIOCGIFADDR, (char *) &ifr) < 0) {
		err(FATAL, "SIOCGIFADDR: %s", strerror(errno));
		/* NOTREACHED */
	}
	*addrp = ((struct sockaddr_in *) & ifr.ifr_addr)->sin_addr.s_addr;
	if (ioctl(fd, SIOCGIFNETMASK, (char *) &ifr) < 0) {
		perror("SIOCGIFNETMASK");
		exit(1);
	}
	*netmaskp = ((struct sockaddr_in *) & ifr.ifr_addr)->sin_addr.s_addr;
	/* If SIOCGIFNETMASK didn't work, figure out a mask from the IP
	 * address class. */
	if (*netmaskp == 0)
		*netmaskp = ipaddrtonetmask(*addrp);

	(void) close(fd);
}
/*
 * Poke the kernel arp tables with the ethernet/ip address combinataion
 * given.  When processing a reply, we must do this so that the booting
 * host (i.e. the guy running rarpd), won't try to ARP for the hardware
 * address of the guy being booted (he cannot answer the ARP).
 */
void
update_arptab(ep, ipaddr)
	u_char *ep;
	u_long  ipaddr;
{
	int     s;
	struct arpreq request;
	struct sockaddr_in *sin;

	request.arp_flags = 0;
	sin = (struct sockaddr_in *) & request.arp_pa;
	sin->sin_family = AF_INET;
	sin->sin_addr.s_addr = ipaddr;
	request.arp_ha.sa_family = AF_UNSPEC;
	bcopy((char *) ep, (char *) request.arp_ha.sa_data, 6);

	s = socket(AF_INET, SOCK_DGRAM, 0);
	if (ioctl(s, SIOCSARP, (caddr_t) & request) < 0) {
		err(NONFATAL, "SIOCSARP: %s", strerror(errno));
	}
	(void) close(s);
}
/*
 * Build a reverse ARP packet and sent it out on the interface.
 * 'ep' points to a valid REVARP_REQUEST.  The REVARP_REPLY is built
 * on top of the request, then written to the network.
 *
 * RFC 903 defines the ether_arp fields as follows.  The following comments
 * are taken (more or less) straight from this document.
 *
 * REVARP_REQUEST
 *
 * arp_sha is the hardware address of the sender of the packet.
 * arp_spa is undefined.
 * arp_tha is the 'target' hardware address.
 *   In the case where the sender wishes to determine his own
 *   protocol address, this, like arp_sha, will be the hardware
 *   address of the sender.
 * arp_tpa is undefined.
 *
 * REVARP_REPLY
 *
 * arp_sha is the hardware address of the responder (the sender of the
 *   reply packet).
 * arp_spa is the protocol address of the responder (see the note below).
 * arp_tha is the hardware address of the target, and should be the same as
 *   that which was given in the request.
 * arp_tpa is the protocol address of the target, that is, the desired address.
 *
 * Note that the requirement that arp_spa be filled in with the responder's
 * protocol is purely for convenience.  For instance, if a system were to use
 * both ARP and RARP, then the inclusion of the valid protocol-hardware
 * address pair (arp_spa, arp_sha) may eliminate the need for a subsequent
 * ARP request.
 */
void
rarp_reply(ii, ep, ipaddr)
	struct if_info *ii;
	struct ether_header *ep;
	u_long  ipaddr;
{
	int     n;
	struct ether_arp *ap = (struct ether_arp *) (ep + 1);
	int     len;

	update_arptab((u_char *) & ap->arp_sha, ipaddr);

	/* Build the rarp reply by modifying the rarp request in place. */
	ap->arp_op = REVARP_REPLY;

	bcopy((char *) &ap->arp_sha, (char *) &ep->ether_dhost, 6);
	bcopy((char *) ii->ii_eaddr, (char *) &ep->ether_shost, 6);
	bcopy((char *) ii->ii_eaddr, (char *) &ap->arp_sha, 6);

	bcopy((char *) &ipaddr, (char *) ap->arp_tpa, 4);
	/* Target hardware is unchanged. */
	bcopy((char *) &ii->ii_ipaddr, (char *) ap->arp_spa, 4);

	len = sizeof(*ep) + sizeof(*ap);
	n = write(ii->ii_fd, (char *) ep, len);
	if (n != len) {
		err(NONFATAL, "write: only %d of %d bytes written", n, len);
	}
}
/*
 * Get the netmask of an IP address.  This routine is used if
 * SIOCGIFNETMASK doesn't work.
 */
u_long
ipaddrtonetmask(addr)
	u_long  addr;
{
	if (IN_CLASSA(addr))
		return IN_CLASSA_NET;
	if (IN_CLASSB(addr))
		return IN_CLASSB_NET;
	if (IN_CLASSC(addr))
		return IN_CLASSC_NET;
	err(FATAL, "unknown IP address class: %08X", addr);
	/* NOTREACHED */
}
/*
 * A faster replacement for inet_ntoa().
 */
char   *
intoa(addr)
	u_long  addr;
{
	register char *cp;
	register u_int byte;
	register int n;
	static char buf[sizeof(".xxx.xxx.xxx.xxx")];

	cp = &buf[sizeof buf];
	*--cp = '\0';

	n = 4;
	do {
		byte = addr & 0xff;
		*--cp = byte % 10 + '0';
		byte /= 10;
		if (byte > 0) {
			*--cp = byte % 10 + '0';
			byte /= 10;
			if (byte > 0)
				*--cp = byte + '0';
		}
		*--cp = '.';
		addr >>= 8;
	} while (--n > 0);

	return cp + 1;
}
/*
 * substitute for the obsolete function tell()
 */
long
tell(fd)
	int     fd;
{
	return lseek(fd, 0L, SEEK_CUR);
}
/* ARGSUSED */

#if __STDC__
#include <stdarg.h>
#else
#include <varargs.h>
#endif

void
#if __STDC__
err(int fatal, const char *fmt,...)
#else
err(fmt, va_alist)
	int     fatal;
	char   *fmt;
va_dcl
#endif
{
	va_list ap;
#if __STDC__
	va_start(ap, fmt);
#else
	va_start(ap);
#endif
	if (dflag) {
		if (fatal)
			(void) fprintf(stderr, "rarpd: error: ");
		else
			(void) fprintf(stderr, "rarpd: warning: ");
		(void) vfprintf(stderr, fmt, ap);
		(void) fprintf(stderr, "\n");
	}
	vsyslog(LOG_ERR, fmt, ap);
	va_end(ap);
	if (fatal)
		exit(1);
	/* NOTREACHED */
}

void
#if __STDC__
debug(const char *fmt,...)
#else
debug(fmt, va_alist)
	char   *fmt;
va_dcl
#endif
{
	va_list ap;

	if (dflag) {
#if __STDC__
		va_start(ap, fmt);
#else
		va_start(ap);
#endif
		(void) fprintf(stderr, "rarpd: ");
		(void) vfprintf(stderr, fmt, ap);
		va_end(ap);
		(void) fprintf(stderr, "\n");
	}
}
