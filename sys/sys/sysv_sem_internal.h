#ifndef SYSV_SEM_INTERNAL_H
#define SYSV_SEM_INTERNAL_H

#include <sys/types.h>
#include <sys/sem.h>
#include <sys/lwp.h>

/*
 * Prototype for do_semop.
 * This function performs semaphore operations.
 */
int do_semop(struct lwp *l, int usemid, struct sembuf *usops,
            size_t nsops, struct timespec *timeout, register_t *retval);

#endif /* SYSV_SEM_INTERNAL_H */