/*	$NetBSD: clonetest.c,v 1.7 2002/07/20 08:36:24 grant Exp $	*/

/*
 * This file placed in the public domain.
 * Jason R. Thorpe, July 16, 2001.
 */

#include <sys/types.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <sys/resource.h>
#include <err.h>
#include <sched.h>
#include <signal.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>

static int	newclone(void *);
static int	stackdir(void *);
static void	test1(void);
static void	test2(void);
static void	test3(void);

int	main(int, char *[]);


#define	STACKSIZE	(8 * 1024)

#define	FROBVAL		41973
#define	CHILDEXIT	0xa5

int
main(int argc, char *argv[])
{
	test1();
	test2();
	test3();
	return 0;
}

static void
test1()
{
	sigset_t mask;
	void *stack;
	pid_t pid;
	__volatile long frobme[2];
	int stat;

	stack = mmap(NULL, STACKSIZE, PROT_READ|PROT_WRITE|PROT_EXEC,
	    MAP_PRIVATE|MAP_ANON, -1, (off_t) 0);

	if (stack == MAP_FAILED)
		err(1, "mmap stack");

	stack = (caddr_t) stack + STACKSIZE * stackdir(&stat);

	printf("parent: stack = %p, frobme = %p\n", stack, frobme);
	fflush(stdout);

	frobme[0] = (long)getpid();
	frobme[1] = (long)stack;

	sigemptyset(&mask);
	sigaddset(&mask, SIGUSR1);

	if (sigprocmask(SIG_BLOCK, &mask, NULL) == -1)
		err(1, "sigprocmask (SIGUSR1)");

	switch (pid = __clone(newclone, stack,
	    CLONE_VM|CLONE_FS|CLONE_FILES|CLONE_SIGHAND|SIGUSR1,
	    (void *)frobme)) {
	case 0:
		errx(1, "clone returned 0");
		/*NOTREACHED*/
	case -1:
		err(1, "clone");
		/*NOTREACHED*/
	default:
		while (waitpid(pid, &stat, __WCLONE) != pid)
			continue;
	}

	if (WIFEXITED(stat) == 0)
		errx(1, "child didn't exit");

	printf("parent: childexit = 0x%x, frobme = %ld\n",
	    WEXITSTATUS(stat), frobme[1]);

	if (WEXITSTATUS(stat) != CHILDEXIT || frobme[1] != FROBVAL)
		exit(1);
}

static void
test2()
{
	if (__clone(0, NULL,
	    CLONE_VM|CLONE_FS|CLONE_FILES|CLONE_SIGHAND|SIGCHLD,
	    (void *)NULL) != -1 || errno != EINVAL)
		errx(1, "clone did not return EINVAL on invalid arguments");
}

static void
test3()
{
	struct rlimit rl;

	/* Can't enforce resource limit on root */
	if (geteuid() == 0)
		return;

	if (getrlimit(RLIMIT_NPROC, &rl) == -1)
		err(1, "getrlimit");
	rl.rlim_cur = 0;
	rl.rlim_max = 0;
	if (setrlimit(RLIMIT_NPROC, &rl) == -1)
		err(1, "setrlimit");
	if (__clone(newclone, malloc(10240),
	    CLONE_VM|CLONE_FS|CLONE_FILES|CLONE_SIGHAND|SIGCHLD,
	    (void *)&rl) != -1 || errno != EAGAIN)
		errx(1, "clone did not return EAGAIN running out of procs");
}

static int
newclone(void *arg)
{
	long *frobp = arg, diff;

	printf("child: stack ~= %p, frobme = %p\n", &frobp, frobp);
	fflush(stdout);

	if (frobp[0] != getppid())
		errx(1, "argument does not contain parent's pid");

	if (frobp[0] == getpid())
		errx(1, "called in parent's pid");

	if (frobp[1] > (long)&frobp)
		diff = frobp[1] - (long)&frobp;
	else
		diff = (long)&frobp - frobp[1];

	if (diff > 1024)
		errx(1, "called with bad stack");

	frobp[1] = FROBVAL;

	return (CHILDEXIT);
}

/*
 * return 1 if the stack grows down, 0 if it grows up.
 */
static int
stackdir(void *p)
{
	void *q;
	return p < q ? 0 : 1;
}
