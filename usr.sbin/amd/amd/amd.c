/*
 * Copyright (c) 1997 Erez Zadok
 * Copyright (c) 1989 Jan-Simon Pendry
 * Copyright (c) 1989 Imperial College of Science, Technology & Medicine
 * Copyright (c) 1989 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Jan-Simon Pendry at Imperial College, London.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by the University of
 *      California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *      %W% (Berkeley) %G%
 *
 * $Id: amd.c,v 1.6 1997/09/26 16:59:47 christos Exp $
 *
 */

/*
 * Automounter
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif /* HAVE_CONFIG_H */
#include <am_defs.h>
#include <amd.h>

struct amu_global_options gopt;	/* where global options are stored */

char pid_fsname[16 + MAXHOSTNAMELEN];	/* "kiska.southseas.nz:(pid%d)" */
char *progname;			/* "amd" */
char *hostdomain = "unknown.domain";
char hostname[MAXHOSTNAMELEN] = "localhost";	/* Hostname */
char hostd[2 * MAXHOSTNAMELEN];	/* Host+domain */
char *endian = ARCH_ENDIAN;	/* Big or Little endian */
char *cpu = HOST_CPU;		/* CPU type */
char *PrimNetName;		/* name of primary network */
char *PrimNetNum;		/* number of primary network */
char *SubsNetName;		/* name of subsidiary network */
char *SubsNetNum;		/* number of subsidiary network */

int foreground = 1;		/* This is the top-level server */
int immediate_abort;		/* Should close-down unmounts be retried */
int orig_umask;
int select_intr_valid;

jmp_buf select_intr;
pid_t mypid;			/* Current process id */
serv_state amd_state;
struct amd_stats amd_stats;	/* Server statistics */
struct in_addr myipaddr;	/* (An) IP address of this host */
time_t do_mapc_reload = 0;	/* mapc_reload() call required? */

#ifdef HAVE_SIGACTION
sigset_t masked_sigs;
#endif /* HAVE_SIGACTION */


/*
 * Signal handler:
 * SIGINT - tells amd to do a full shutdown, including unmounting all
 *       filesystem.
 * SIGTERM - tells amd to shutdown now.  Just unmounts the automount nodes.
 */
static RETSIGTYPE
sigterm(int sig)
{
#ifdef REINSTALL_SIGNAL_HANDLER
  signal(sig, sigterm);
#endif /* REINSTALL_SIGNAL_HANDLER */

  switch (sig) {
  case SIGINT:
    immediate_abort = 15;
    break;

  case SIGTERM:
    immediate_abort = -1;
    /* fall through... */

  default:
    plog(XLOG_WARNING, "WARNING: automounter going down on signal %d", sig);
    break;
  }
  if (select_intr_valid)
    longjmp(select_intr, sig);
}


/*
 * Hook for cache reload.
 * When a SIGHUP arrives it schedules a call to mapc_reload
 */
static RETSIGTYPE
sighup(int sig)
{
#ifdef REINSTALL_SIGNAL_HANDLER
  signal(sig, sighup);
#endif /* REINSTALL_SIGNAL_HANDLER */

#ifdef DEBUG
  if (sig != SIGHUP)
    dlog("spurious call to sighup");
#endif /* DEBUG */
  /*
   * Force a reload by zero'ing the timer
   */
  if (amd_state == Run)
    do_mapc_reload = 0;
}


static RETSIGTYPE
parent_exit(int sig)
{
  exit(0);
}


static int
daemon_mode(void)
{
  int bgpid;

#ifdef HAVE_SIGACTION
  struct sigaction sa, osa;

  sa.sa_handler = parent_exit;
  sa.sa_flags = 0;
  sigemptyset(&(sa.sa_mask));
  sigaddset(&(sa.sa_mask), SIGQUIT);
  sigaction(SIGQUIT, &sa, &osa);
#else /* not HAVE_SIGACTION */
  signal(SIGQUIT, parent_exit);
#endif /* not HAVE_SIGACTION */

  bgpid = background();

  if (bgpid != 0) {
    /*
     * Now wait for the automount points to
     * complete.
     */
    for (;;)
      pause();
    /* should never reache here */
  }
#ifdef HAVE_SIGACTION
  sigaction(SIGQUIT, &osa, NULL);
#else /* not HAVE_SIGACTION */
  signal(SIGQUIT, SIG_DFL);
#endif /* not HAVE_SIGACTION */

  if (gopt.flags & CFM_PRINT_PID) {
    printf("%ld\n", (long) mypid);
    fflush(stdout);
    (void) fclose(stdout);
  }

  /*
   * Pretend we are in the foreground again
   */
  foreground = 1;

  /*
   * Dissociate from the controlling terminal
   */
#ifdef HAVE_SETPGID
  /*
   * XXX: does setpgid() work the same as setsid()?
   * I cannot use setsid here because linux had defined setsid and
   * setpgid to be the same functions, with two arguments instead of none,
   * and that causes compile time errors on linux.
   *
   * Note: on some systems setpgid(0,0) behaves the same as setsid().
   * Maybe I should use that instead? -Erez.
   */
  setpgid(mypid, mypid);
#else /* not HAVE_SETPGID */
  {
    int t = open("/dev/tty", O_RDWR);
    if (t < 0) {
      /* not an error if already no controlling tty */
      if (errno != ENXIO)
	plog(XLOG_WARNING, "Could not open controlling tty: %m");
    } else {
      if (ioctl(t, TIOCNOTTY, 0) < 0 && errno != ENOTTY)
	plog(XLOG_WARNING, "Could not disassociate tty (TIOCNOTTY): %m");
      close(t);
    }
  }
#endif /* not HAVE_SETPGID */

  return getppid();
}


/*
 * Initialize global options structure.
 */
static void
init_global_options(void)
{
#if defined(HAVE_SYS_UTSNAME_H) && defined(HAVE_UNAME)
  static struct utsname un;
#endif /* defined(HAVE_SYS_UTSNAME_H) && defined(HAVE_UNAME) */

  memset(&gopt, 0, sizeof(struct amu_global_options));

  /* name of current architecture */
  gopt.arch = HOST_ARCH;

  /* automounter temp dir */
  gopt.auto_dir = "/a";

  /* cluster name */
  gopt.cluster = NULL;

  /*
   * kernel architecture: this you must get from uname() if possible.
   */
#if defined(HAVE_SYS_UTSNAME_H) && defined(HAVE_UNAME)
  if (uname(&un) >= 0)
    gopt.karch = un.machine;
  else
#endif /* defined(HAVE_SYS_UTSNAME_H) && defined(HAVE_UNAME) */
    gopt.karch = HOST_ARCH;

  /* amd log file */
  gopt.logfile = NULL;

  /* operating system name */
  gopt.op_sys = HOST_OS_NAME;

  /* OS version */
  gopt.op_sys_ver = HOST_OS_VERSION;

  /* local domain */
  gopt.sub_domain = NULL;

  /* NFS retransmit counter */
  gopt.afs_retrans = -1;

  /* NFS retry interval */
  gopt.afs_timeo = -1;

  /* cache duration */
  gopt.am_timeo = AM_TTL;

  /* dismount interval */
  gopt.am_timeo_w = AM_TTL_W;

  /*
   * various CFM_* flags.
   * by default, only the "plock" option is on (if available).
   */
  gopt.flags = CFM_PROCESS_LOCK;

#ifdef HAVE_MAP_LDAP
  /* LDAP base */
  gopt.ldap_base = NULL;

  /* LDAP host ports */
  gopt.ldap_hostports = NULL;

  /* LDAP cache */
  gopt.ldap_cache_seconds = 0;
  gopt.ldap_cache_maxmem = 131072;
#endif /* HAVE_MAP_LDAP */

#ifdef HAVE_MAP_NIS
  /* YP domain name */
  gopt.nis_domain = NULL;
#endif /* HAVE_MAP_NIS */
}


int
main(int argc, char *argv[])
{
  char *domdot, *verstr;
  int ppid = 0;
  int error;
#ifdef HAVE_SIGACTION
  struct sigaction sa;
#endif /* HAVE_SIGACTION */

  /*
   * Make sure some built-in assumptions are true before we start
   */
  assert(sizeof(nfscookie) >= sizeof(u_int));
  assert(sizeof(int) >= 4);

  /*
   * Set processing status.
   */
  amd_state = Start;

  /*
   * Determine program name
   */
  if (argv[0]) {
    progname = strrchr(argv[0], '/');
    if (progname && progname[1])
      progname++;
    else
      progname = argv[0];
  }
  if (!progname)
    progname = "amd";

  /*
   * Initialise process id.  This is kept
   * cached since it is used for generating
   * and using file handles.
   */
  mypid = getpid();

  /*
   * Get local machine name
   */
  if (gethostname(hostname, sizeof(hostname)) < 0) {
    plog(XLOG_FATAL, "gethostname: %m");
    going_down(1);
  }

  /*
   * Check it makes sense
   */
  if (!*hostname) {
    plog(XLOG_FATAL, "host name is not set");
    going_down(1);
  }

  /*
   * Initialize global options structure.
   */
  init_global_options();

  /*
   * Partially initialise hostd[].  This
   * is completed in get_args().
   */
  if ((domdot = strchr(hostname, '.'))) {
    /*
     * Hostname already contains domainname.
     * Split out hostname and domainname
     * components
     */
    *domdot++ = '\0';
    hostdomain = domdot;
  }
  strcpy(hostd, hostname);

  /*
   * Trap interrupts for shutdowns.
   */
#ifdef HAVE_SIGACTION
  sa.sa_handler = sigterm;
  sa.sa_flags = 0;
  sigemptyset(&(sa.sa_mask));
  sigaddset(&(sa.sa_mask), SIGINT);
  sigaddset(&(sa.sa_mask), SIGTERM);
  sigaction(SIGINT, &sa, NULL);
  sigaction(SIGTERM, &sa, NULL);
#else /* not HAVE_SIGACTION */
  (void) signal(SIGINT, sigterm);
#endif /* not HAVE_SIGACTION */

  /*
   * Trap Terminate so that we can shutdown gracefully (some chance)
   */
#ifdef HAVE_SIGACTION
  sa.sa_handler = sigterm;
  sa.sa_flags = 0;
  sigemptyset(&(sa.sa_mask));
  sigaddset(&(sa.sa_mask), SIGTERM);
  sigaction(SIGTERM, &sa, NULL);
#else /* not HAVE_SIGACTION */
  (void) signal(SIGTERM, sigterm);
#endif /* not HAVE_SIGACTION */

  /*
   * Hangups tell us to reload the cache
   */
#ifdef HAVE_SIGACTION
  sa.sa_handler = sighup;
  sa.sa_flags = 0;
  sigemptyset(&(sa.sa_mask));
  sigaddset(&(sa.sa_mask), SIGHUP);
  sigaction(SIGHUP, &sa, NULL);
#else /* not HAVE_SIGACTION */
  (void) signal(SIGHUP, sighup);
#endif /* not HAVE_SIGACTION */

  /*
   * Trap Death-of-a-child.  These allow us to
   * pick up the exit status of backgrounded mounts.
   * See "sched.c".
   */
#ifdef HAVE_SIGACTION
  sa.sa_handler = sigchld;
  sa.sa_flags = 0;
  sigemptyset(&(sa.sa_mask));
  sigaddset(&(sa.sa_mask), SIGCHLD);
  sigaction(SIGCHLD, &sa, NULL);

  /*
   * construct global "masked_sigs" used in nfs_start.c
   */
  sigemptyset(&masked_sigs);
  sigaddset(&masked_sigs, SIGHUP);
  sigaddset(&masked_sigs, SIGCHLD);
  sigaddset(&masked_sigs, SIGTERM);
  sigaddset(&masked_sigs, SIGINT);
#else /* not HAVE_SIGACTION */
  (void) signal(SIGCHLD, sigchld);
#endif /* not HAVE_SIGACTION */

  /*
   * Fix-up any umask problems.  Most systems default
   * to 002 which is not too convenient for our purposes
   */
  orig_umask = umask(0);

  /*
   * Figure out primary network name
   */
  getwire(&PrimNetName, &PrimNetNum, &SubsNetName, &SubsNetNum);

  /*
   * Determine command-line arguments
   */
  get_args(argc, argv);

  /*
   * Log version information.
   */
  verstr = strtok(get_version_string(), "\n");
  plog(XLOG_INFO, "AM-UTILS VERSION INFORMATION:");
  while (verstr) {
    plog(XLOG_INFO, verstr);
    verstr = strtok(NULL, "\n");
  }

  /*
   * Get our own IP address so that we
   * can mount the automounter.
   */
  amu_get_myaddress(&myipaddr);
  plog(XLOG_INFO, "My ip addr is 0x%x", htonl(myipaddr.s_addr));

  /*
   * Now check we are root.
   */
  if (geteuid() != 0) {
    plog(XLOG_FATAL, "Must be root to mount filesystems (euid = %d)", geteuid());
    going_down(1);
  }

  /*
   * Lock process text and data segment in memory.
   */
#ifdef HAVE_PLOCK
  if (gopt.flags & CFM_PROCESS_LOCK)
    if (plock(PROCLOCK) != 0) {
      plog(XLOG_WARNING, "Couldn't lock process text and data segment in memory: %m");
    } else {
      plog(XLOG_INFO, "Locked process text and data segment in memory");
    }
#endif /* HAVE_PLOCK */

#ifdef HAVE_MAP_NIS
  /*
   * If the domain was specified then bind it here
   * to circumvent any default bindings that may
   * be done in the C library.
   */
  if (gopt.nis_domain && yp_bind(gopt.nis_domain)) {
    plog(XLOG_FATAL, "Can't bind to NIS domain \"%s\"", gopt.nis_domain);
    going_down(1);
  }
#endif /* HAVE_MAP_NIS */

#ifdef DEBUG
  amuDebug(D_DAEMON)
#endif /* DEBUG */
    ppid = daemon_mode();

  sprintf(pid_fsname, "%s:(pid%ld)", hostname, (long) mypid);

  do_mapc_reload = clocktime() + ONE_HOUR;

  /*
   * Register automounter with system.
   */
  error = mount_automounter(ppid);
  if (error && ppid)
    kill(SIGALRM, ppid);
  going_down(error);

  abort();
  return 1; /* should never get here */
}
