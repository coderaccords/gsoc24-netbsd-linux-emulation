/*	$NetBSD: identd.c,v 1.19 2003/09/14 22:38:23 christos Exp $	*/

/*
** identd.c                       A TCP/IP link identification protocol server
**
** This program is in the public domain and may be used freely by anyone
** who wants to. 
**
** Last update: 7 Oct 1993
**
** Please send bug fixes/bug reports to: Peter Eriksson <pen@lysator.liu.se>
*/

#if defined(IRIX) || defined(SVR4) || defined(NeXT) || (defined(sco) && sco >= 42) || defined(_AIX4) || defined(__NetBSD__) || defined(__FreeBSD__) || defined(ultrix)
#  define SIGRETURN_TYPE void
#  define SIGRETURN_TYPE_IS_VOID
#else
#  define SIGRETURN_TYPE int
#endif

#ifdef SVR4
#  define STRNET
#endif

#ifdef NeXT31
#  include <libc.h>
#endif

#ifdef sco
#  define USE_SIGALARM
#endif

#include <stdio.h>
#include <ctype.h>
#include <errno.h>
#include <netdb.h>
#include <signal.h>
#include <fcntl.h>
#include <poll.h>

#include <sys/types.h>
#include <sys/param.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#ifndef _AUX_SOURCE
#  include <sys/file.h>
#endif
#include <sys/time.h>
#include <sys/wait.h>

#include <pwd.h>
#include <grp.h>

#include <netinet/in.h>

#ifndef HPUX7
#  include <arpa/inet.h>
#endif

#if defined(MIPS) || defined(BSD43)
extern int errno;
#endif

#if defined(SOLARIS) || defined(__NetBSD__) || defined(__FreeBSD__) || defined(__linux__) || defined(_AIX)
#  include <unistd.h>
#  include <stdlib.h>
#  include <string.h>
#endif

#include "identd.h"
#include "error.h"
#include "paths.h"


/* Antique unixes do not have these things defined... */
#ifndef FD_SETSIZE
#  define FD_SETSIZE 256
#endif

#ifndef FD_SET
#  ifndef NFDBITS
#    define NFDBITS   	(sizeof(int) * NBBY)  /* bits per mask */
#  endif
#  define FD_SET(n, p)  ((p)->fds_bits[(n)/NFDBITS] |= (1 << ((n) % NFDBITS)))
#endif

#ifndef FD_ZERO
#  define FD_ZERO(p)        bzero((char *)(p), sizeof(*(p)))
#endif


char *path_unix = (char *) NULL;
char *path_kmem = (char *) NULL;

int verbose_flag = 0;
int debug_flag   = 0;
int syslog_flag  = 0;
int multi_flag   = 0;
int other_flag   = 0;
int unknown_flag = 0;
int noident_flag = 0;
int crypto_flag  = 0;
int liar_flag    = 0;

int lport = 0;
int fport = 0;

char *charset_name = (char *) NULL;
char *indirect_host = (char *) NULL;
char *indirect_password = (char *) NULL;
char *lie_string = (char *) NULL;

#ifdef ALLOW_FORMAT
    int format_flag = 0;
    char *format = "%u";
#endif

static int child_pid;

#ifdef LOG_DAEMON
static int syslog_facility = LOG_DAEMON;
#endif

static int comparemem __P((void *, void *, int));
char *clearmem __P((void *, int));
static SIGRETURN_TYPE child_handler __P((int));
int main __P((int, char *[]));

/*
** The structure passing convention for GCC is incompatible with
** Suns own C compiler, so we define our own inet_ntoa() function.
** (This should only affect GCC version 1 I think, a well, this works
** for version 2 also so why bother.. :-)
*/
#if defined(__GNUC__) && defined(__sparc__) && !defined(NeXT)

#ifdef inet_ntoa
#undef inet_ntoa
#endif

char *inet_ntoa(ad)
    struct in_addr ad;
{
    unsigned long int s_ad;
    int a, b, c, d;
    static char addr[20];
    
    s_ad = ad.s_addr;
    d = s_ad % 256;
    s_ad /= 256;
    c = s_ad % 256;
    s_ad /= 256;
    b = s_ad % 256;
    a = s_ad / 256;
    snprintf(addr, sizeof(addr), "%d.%d.%d.%d", a, b, c, d);
    
    return addr;
}
#endif

static int comparemem(vp1, vp2, len)
     void *vp1;
     void *vp2;
     int len;
{
    unsigned char *p1 = (unsigned char *) vp1;
    unsigned char *p2 = (unsigned char *) vp2;
    int c;

    while (len-- > 0)
	if ((c = (int) *p1++ - (int) *p2++) != 0)
	    return c;

    return 0;
}

/*
** Return the name of the connecting host, or the IP number as a string.
*/
char *gethost(addr)
    struct in_addr *addr;
{
    int i;
    struct hostent *hp;

  
    hp = gethostbyaddr((char *) addr, sizeof(struct in_addr), AF_INET);
    if (hp)
    {
	char *hname = strdup(hp->h_name);

	if (! hname) {
	    syslog(LOG_ERR, "strdup(%s): %m", hp->h_name);
	    exit(1);
	}
	/* Found a IP -> Name match, now try the reverse for security reasons */
	hp = gethostbyname(hname);
	(void) free(hname);
	if (hp)
#ifdef h_addr
	    for (i = 0; hp->h_addr_list[i]; i++)
		if (comparemem(hp->h_addr_list[i],
			       (unsigned char *) addr,
			       (int) sizeof(struct in_addr)) == 0)
		    return (char *) hp->h_name;
#else
	if (comparemem(hp->h_addr, addr, sizeof(struct in_addr)) == 0)
	    return hp->h_name;
#endif
  }

  return inet_ntoa(*addr);
}

#ifdef USE_SIGALARM
/*
** Exit cleanly after our time's up.
*/
static SIGRETURN_TYPE
alarm_handler(int s)
{
    if (syslog_flag)
	syslog(LOG_DEBUG, "SIGALRM triggered, exiting");
    
    exit(0);
}
#endif


#if !defined(hpux) && !defined(__hpux) && !defined(SVR4) && \
    !defined(_CRAY) && !defined(sco) && !defined(LINUX)
/*
** This is used to clean up zombie child processes
** if the -w or -b options are used.
*/
static SIGRETURN_TYPE
child_handler(dummy)
	int dummy;
{
#if defined(NeXT) || (defined(__sgi) && defined(__SVR3))
    union wait status;
#else
    int status;
#endif
    int saved_errno = errno;
    
    while (wait3(&status, WNOHANG, NULL) > 0)
	;

    errno = saved_errno;
    
#ifndef SIGRETURN_TYPE_IS_VOID
    return 0;
#endif
}
#endif


char *clearmem(vbp, len)
     void *vbp;
     int len;
{
    char *bp = (char *) vbp;
    char *cp;

    cp = bp;
    while (len-- > 0)
	*cp++ = 0;
    
    return bp;
}


/*
** Main entry point into this daemon
*/
int main(argc,argv)
    int argc;
    char *argv[];
{
    int i, len;
    struct sockaddr_in sin;
    struct in_addr laddr, faddr;
    int one = 1;

    int background_flag = 0;
    int timeout = 0;
    char *portno = "113";
    char *bind_address = (char *) NULL;
    int set_uid = 0;
    int set_gid = 0;
    int inhibit_default_config = 0;
    int opt_count = 0;		/* Count of option flags */
  
#ifdef __convex__
    argc--;    /* get rid of extra argument passed by inetd */
#endif


    if (isatty(0))
	background_flag = 1;
    
    /*
    ** Prescan the arguments for "-f<config-file>" switches
    */
    inhibit_default_config = 0;
    for (i = 1; i < argc && argv[i][0] == '-'; i++)
	if (argv[i][1] == 'f')
	    inhibit_default_config = 1;
    
    /*
    ** Parse the default config file - if it exists
    */
    if (!inhibit_default_config)
	parse_config(NULL, 1);
  
    /*
    ** Parse the command line arguments
    */
    for (i = 1; i < argc && argv[i][0] == '-'; i++) {
	opt_count++;
	switch (argv[i][1])
	{
	  case 'b':    /* Start as standalone daemon */
	    background_flag = 1;
	    break;
	    
	  case 'w':    /* Start from Inetd, wait mode */
	    background_flag = 2;
	    break;
	    
	  case 'i':    /* Start from Inetd, nowait mode */
	    background_flag = 0;
	    break;
	    
	  case 't':
	    timeout = atoi(argv[i]+2);
	    break;
	    
	  case 'p':
	    portno = argv[i]+2;
	    break;
	    
	  case 'a':
	    bind_address = argv[i]+2;
	    break;
	    
	  case 'u':
	    if (isdigit(argv[i][2]))
		set_uid = atoi(argv[i]+2);
	    else
	    {
		struct passwd *pwd;
		
		pwd = getpwnam(argv[i]+2);
		if (!pwd)
		    ERROR1("no such user (%s) for -u option", argv[i]+2);
		else
		{
		    set_uid = pwd->pw_uid;
		    set_gid = pwd->pw_gid;
		}
	    }
	    break;
	    
	  case 'g':
	    if (isdigit(argv[i][2]))
		set_gid = atoi(argv[i]+2);
	    else
	    {
		struct group *grp;
		
		grp = getgrnam(argv[i]+2);
		if (!grp)
		    ERROR1("no such group (%s) for -g option", argv[i]+2);
		else
		    set_gid = grp->gr_gid;
	    }
	    break;
	    
	  case 'c':
	    charset_name = argv[i]+2;
	    break;
	    
	  case 'r':
	    indirect_host = argv[i]+2;
	    break;
	    
	  case 'l':    /* Use the Syslog daemon for logging */
	    syslog_flag++;
	    break;
	    
	  case 'o':
	    other_flag = 1;
	    break;
	    
	  case 'e':
	    unknown_flag = 1;
	    break;
	    
	  case 'V':    /* Give version of this daemon */
	    printf("[in.identd, version %s]\r\n", version);
	    exit(0);
	    break;
	    
	  case 'v':    /* Be verbose */
	    verbose_flag++;
	    break;
	    
	  case 'd':    /* Enable debugging */
	    debug_flag++;
	    break;
	    
	  case 'm':    /* Enable multiline queries */
	    multi_flag++;
	    break;
	    
	  case 'N':    /* Enable users ".noident" files */
	    noident_flag++;
	    break;
	    
#ifdef INCLUDE_CRYPT
          case 'C':    /* Enable encryption. */
	    {
		FILE *keyfile;
		
		if (argv[i][2])
		    keyfile = fopen(argv[i]+2, "r");
		else
		    keyfile = fopen(PATH_DESKEY, "r");
		
		if (keyfile == NULL)
		{
		    ERROR("cannot open key file for option -C");
		}
		else
		{
		    char buf[1024];
		    
		    if (fgets(buf, 1024, keyfile) == NULL)
		    {
			ERROR("cannot read key file for option -C");
		    }
		    else
		    {
			init_encryption(buf);
			crypto_flag++;
		    }
		    fclose(keyfile);
		}
	    }
            break;
#endif

#ifdef ALLOW_FORMAT
	  case 'n': /* Compatibility flag - just send the user number */
	    format_flag = 1;
	    format = "%U";
	    break;
	    
          case 'F':    /* Output format */
	    format_flag = 1;
	    format = argv[i]+2;
	    break;
#endif

	  case 'L':	/* lie brazenly */
	    liar_flag = 1;
	    if (*(argv[i]+2) != '\0')
		lie_string = argv[i]+2;
	    else
#ifdef DEFAULT_LIE_USER
		lie_string = DEFAULT_LIE_USER;
#else
		ERROR("-L specified with no user name");
#endif
	    break;

	  default:
	    ERROR1("Bad option %s", argv[i]);
	    break;
	}
    }

#if defined(_AUX_SOURCE) || defined (SUNOS35)
    /* A/UX 2.0* & SunOS 3.5 calls us with an argument XXXXXXXX.YYYY
    ** where XXXXXXXXX is the hexadecimal version of the callers
    ** IP number, and YYYY is the port/socket or something.
    ** It seems to be impossible to pass arguments to a daemon started
    ** by inetd.
    **
    ** Just in case it is started from something else, then we only
    ** skip the argument if no option flags have been seen.
    */
    if (opt_count == 0)
	argc--;
#endif

    /*
    ** Path to kernel namelist file specified on command line
    */
    if (i < argc)
	path_unix = argv[i++];

    /*
    ** Path to kernel memory device specified on command line
    */
    if (i < argc)
	path_kmem = argv[i++];


    if (i < argc)
	ERROR1("Too many arguments: ignored from %s", argv[i]);


    /*
    ** We used to call k_open here. But then the file descriptor
    ** kd->fd open on /dev/kmem is shared by all child processes.
    ** From the fork(2) man page:
    **      o  The child process has its own copy of the parent's descriptors.  These
    **         descriptors reference the same underlying objects.  For instance, file
    **         pointers in file objects are shared between the child and the parent
    **         so that an lseek(2) on a descriptor in the child process can affect a
    **         subsequent read(2) or write(2) by the parent.
    ** Thus with concurrent (simultaneous) identd client processes,
    ** they step on each other's toes when they use kvm_read.
    ** 
    ** Calling k_open here was a mistake for another reason too: we
    ** did not yet honor -u and -g options. Presumably we are
    ** running as root (unless the in.identd file is setuid), and
    ** then we can open kmem regardless of -u and -g values.
    ** 
    ** 
    ** Open the kernel memory device and read the nlist table
    ** 
    **     if (k_open() < 0)
    ** 		ERROR("main: k_open");
    */
    
    /*
    ** Do the special handling needed for the "-b" flag
    */
    if (background_flag == 1)
    {
	struct sockaddr_in addr;
	struct servent *sp;
	int fd;
	

	if (!debug_flag)
	{
	    if (fork())
		exit(0);
	    
	    close(0);
	    close(1);
	    close(2);
	    
	    if (fork())
		exit(0);
	}
	
	fd = socket(AF_INET, SOCK_STREAM, 0);
	if (fd == -1)
	    ERROR("main: socket");
	
	if (fd != 0)
	    dup2(fd, 0);
	
	clearmem((void *) &addr, (int) sizeof(addr));
	
	addr.sin_family = AF_INET;
	if (bind_address == (char *) NULL)
	    addr.sin_addr.s_addr = htonl(INADDR_ANY);
	else
	{
	    if (isdigit(bind_address[0]))
		addr.sin_addr.s_addr = inet_addr(bind_address);
	    else
	    {
		struct hostent *hp;
		
		hp = gethostbyname(bind_address);
		if (!hp)
		    ERROR1("no such address (%s) for -a switch", bind_address);
		
		/* This is ugly, should use memcpy() or bcopy() but... */
		addr.sin_addr.s_addr = * (unsigned long *) (hp->h_addr);
	    }
	}
	
	if (isdigit(portno[0]))
	    addr.sin_port = htons(atoi(portno));
	else
	{
	    sp = getservbyname(portno, "tcp");
	    if (sp == (struct servent *) NULL)
		ERROR1("main: getservbyname: %s", portno);
	    addr.sin_port = sp->s_port;
	}

#ifdef SO_REUSEADDR
	setsockopt(0, SOL_SOCKET, SO_REUSEADDR, (void *) &one, sizeof(one));
#endif

	if (bind(0, (struct sockaddr *) &addr, sizeof(addr)) < 0)
	    ERROR("main: bind");
    }
    
    if (background_flag)
    {
      if (listen(0, 3) < 0)
	ERROR("main: listen");
    }
    
    if (set_gid)
    {
	if (setgid(set_gid) == -1)
	    ERROR("main: setgid");
	/* Call me paranoid... PSz */
	if (getgid() != set_gid)
	    ERROR2("main: setgid failed: wanted %d, got GID %d", set_gid, getgid());
	if (getegid() != set_gid)
	    ERROR2("main: setgid failed: wanted %d, got EGID %d", set_gid, getegid());
    }
    
    if (set_uid)
    {
	if (setuid(set_uid) == -1)
	    ERROR("main: setuid");
	/* Call me paranoid... PSz */
	if (getuid() != set_uid)
	    ERROR2("main: setuid failed: wanted %d, got UID %d", set_uid, getuid());
	if (geteuid() != set_uid)
	    ERROR2("main: setuid failed: wanted %d, got EUID %d", set_uid, geteuid());
    }
    
    if (syslog_flag) {
#ifdef LOG_DAEMON
	openlog("identd", LOG_PID, syslog_facility);
#else
	openlog("identd", LOG_PID);
#endif
    }
    (void)getpwnam("xyzzy");
    (void)gethostbyname("xyzzy");

    /*
    ** Do some special handling if the "-b" or "-w" flags are used
    */
    if (background_flag)
    {
	int nfds, fd;
	struct pollfd set[1];
	struct sockaddr sad;
	int sadlen;
	
	
	/*
	** Set up the SIGCHLD signal child termination handler so
	** that we can avoid zombie processes hanging around and
	** handle childs terminating before being able to complete the
	** handshake.
	*/
#if (defined(SVR4) || defined(hpux) || defined(__hpux) || defined(IRIX) || \
     defined(_CRAY) || defined(_AUX_SOURCE) || defined(sco) || \
	 defined(LINUX))
	signal(SIGCHLD, SIG_IGN);
#else
	signal(SIGCHLD, child_handler);
#endif
    
	set[0].fd = 0;
	set[0].events = POLLIN;

	/*
	** Loop and dispatch client handling processes
	*/
	do
	{
#ifdef USE_SIGALARM
	    /*
	    ** Terminate if we've been idle for 'timeout' seconds
	    */
	    if (background_flag == 2 && timeout)
	    {
		signal(SIGALRM, alarm_handler);
		alarm(timeout);
	    }
#endif
      
	    /*
	    ** Wait for a connection request to occur.
	    ** Ignore EINTR (Interrupted System Call).
	    */
	    do
	    {
#ifndef USE_SIGALARM
		if (timeout)
		    nfds = poll(set, 1, timeout * 1000);
		else
#endif
		nfds = poll(set, 1, INFTIM);
	    } while (nfds < 0  && errno == EINTR);
	    
	    /*
	    ** An error occurred in select? Just die
	    */
	    if (nfds < 0)
		ERROR("main: poll");
	    
	    /*
	    ** Timeout limit reached. Exit nicely
	    */
	    if (nfds == 0)
		exit(0);
      
#ifdef USE_SIGALARM
	    /*
	    ** Disable the alarm timeout
	    */
	    alarm(0);
#endif
      
	    /*
	    ** Accept the new client
	    */
	    sadlen = sizeof(sad);
	    errno = 0;
	    fd = accept(0, &sad, &sadlen);
	    if (fd == -1) {
		WARNING1("main: accept. errno = %d", errno);
		continue;
	    }
      
	    /*
	    ** And fork, then close the fd if we are the parent.
	    */
	    child_pid = fork();
	} while (child_pid && (close(fd), 1));

	/*
	** We are now in child, the parent has returned to "do" above.
	*/
	if (dup2(fd, 0) == -1)
	    ERROR("main: dup2: failed fd 0");
	
	if (dup2(fd, 1) == -1)
	    ERROR("main: dup2: failed fd 1");
	
	if (dup2(fd, 2) == -1)
	    ERROR("main: dup2: failed fd 2");
    }
    
    /*
    ** Get foreign internet address
    */
    len = sizeof(sin);
    if (getpeername(0, (struct sockaddr *) &sin, &len) == -1)
    {
	/*
	** A user has tried to start us from the command line or
	** the network link died, in which case this message won't
	** reach to other end anyway, so lets give the poor user some
	** errors.
	*/
	perror("in.identd: getpeername()");
	exit(1);
    }
    
    faddr = sin.sin_addr;
    
    
#ifdef STRONG_LOG
    if (syslog_flag)
	syslog(LOG_INFO, "Connection from %s", gethost(&faddr));
#endif

    
    /*
    ** Get local internet address
    */
    len = sizeof(sin);
#ifdef ATTSVR4
    if (t_getsockname(0, (struct sockaddr *) &sin, &len) == -1)
#else
    if (getsockname(0, (struct sockaddr *) &sin, &len) == -1)
#endif
    {
	/*
	** We can just die here, because if this fails then the
	** network has died and we haven't got anyone to return
	** errors to.
	*/
	exit(1);
    }
    laddr = sin.sin_addr;
    

    /*
    ** Get the local/foreign port pair from the luser
    */
    parse(stdin, &laddr, &faddr);

    exit(0);
}
