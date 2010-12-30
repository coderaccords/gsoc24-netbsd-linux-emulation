/* $NetBSD: rump_syscalls.h,v 1.36 2010/12/30 20:11:07 pooka Exp $ */

/*
 * System call protos in rump namespace.
 *
 * DO NOT EDIT-- this file is automatically generated.
 * created from	NetBSD: syscalls.master,v 1.239 2010/11/11 14:47:41 pooka Exp
 */

#ifndef _RUMP_RUMP_SYSCALLS_H_
#define _RUMP_RUMP_SYSCALLS_H_

#ifdef _KERNEL
#error Interface not supported inside kernel
#endif /* _KERNEL */

#include <sys/types.h>
#include <sys/select.h>

#include <signal.h>

ssize_t rump_sys_read(int, void *, size_t);
ssize_t rump_sys_write(int, const void *, size_t);
int rump_sys_open(const char *, int, ...);
int rump_sys_close(int);
int rump_sys_link(const char *, const char *);
int rump_sys_unlink(const char *);
int rump_sys_chdir(const char *);
int rump_sys_fchdir(int);
int rump_sys_chmod(const char *, mode_t);
int rump_sys_chown(const char *, uid_t, gid_t);
pid_t rump_sys_getpid(void);
int rump_sys_unmount(const char *, int);
int rump_sys_setuid(uid_t);
uid_t rump_sys_getuid(void);
uid_t rump_sys_geteuid(void);
ssize_t rump_sys_recvmsg(int, struct msghdr *, int);
ssize_t rump_sys_sendmsg(int, const struct msghdr *, int);
ssize_t rump_sys_recvfrom(int, void *, size_t, int, struct sockaddr *, unsigned int *);
int rump_sys_accept(int, struct sockaddr *, unsigned int *);
int rump_sys_getpeername(int, struct sockaddr *, unsigned int *);
int rump_sys_getsockname(int, struct sockaddr *, unsigned int *);
int rump_sys_access(const char *, int);
int rump_sys_chflags(const char *, u_long);
int rump_sys_fchflags(int, u_long);
void rump_sys_sync(void);
pid_t rump_sys_getppid(void);
int rump_sys_dup(int);
gid_t rump_sys_getegid(void);
gid_t rump_sys_getgid(void);
int rump_sys___getlogin(char *, size_t);
int rump_sys___setlogin(const char *);
int rump_sys_ioctl(int, u_long, ...);
int rump_sys_revoke(const char *);
int rump_sys_symlink(const char *, const char *);
ssize_t rump_sys_readlink(const char *, char *, size_t);
mode_t rump_sys_umask(mode_t);
int rump_sys_chroot(const char *);
int rump_sys_getgroups(int, gid_t *);
int rump_sys_setgroups(int, const gid_t *);
int rump_sys_getpgrp(void);
int rump_sys_setpgid(int, int);
int rump_sys_dup2(int, int);
int rump_sys_fcntl(int, int, ...);
int rump_sys_fsync(int);
int rump_sys_connect(int, const struct sockaddr *, unsigned int);
int rump_sys_bind(int, const struct sockaddr *, unsigned int);
int rump_sys_setsockopt(int, int, int, const void *, unsigned int);
int rump_sys_listen(int, int);
int rump_sys_getsockopt(int, int, int, void *, unsigned int *);
ssize_t rump_sys_readv(int, const struct iovec *, int);
ssize_t rump_sys_writev(int, const struct iovec *, int);
int rump_sys_fchown(int, uid_t, gid_t);
int rump_sys_fchmod(int, mode_t);
int rump_sys_setreuid(uid_t, uid_t);
int rump_sys_setregid(gid_t, gid_t);
int rump_sys_rename(const char *, const char *);
int rump_sys_flock(int, int);
int rump_sys_mkfifo(const char *, mode_t);
ssize_t rump_sys_sendto(int, const void *, size_t, int, const struct sockaddr *, unsigned int);
int rump_sys_shutdown(int, int);
int rump_sys_socketpair(int, int, int, int *);
int rump_sys_mkdir(const char *, mode_t);
int rump_sys_rmdir(const char *);
int rump_sys_setsid(void);
int rump_sys_nfssvc(int, void *);
ssize_t rump_sys_pread(int, void *, size_t, off_t);
ssize_t rump_sys_pwrite(int, const void *, size_t, off_t);
int rump_sys_setgid(gid_t);
int rump_sys_setegid(gid_t);
int rump_sys_seteuid(uid_t);
long rump_sys_pathconf(const char *, int);
long rump_sys_fpathconf(int, int);
int rump_sys_getrlimit(int, struct rlimit *);
int rump_sys_setrlimit(int, const struct rlimit *);
off_t rump_sys_lseek(int, off_t, int);
int rump_sys_truncate(const char *, off_t);
int rump_sys_ftruncate(int, off_t);
int rump_sys___sysctl(const int *, u_int, void *, size_t *, const void *, size_t);
pid_t rump_sys_getpgid(pid_t);
int rump_sys_reboot(int, char *);
int rump_sys_poll(struct pollfd *, u_int, int);
int rump_sys_fdatasync(int);
int rump_sys_modctl(int, void *);
int rump_sys__ksem_init(unsigned int, intptr_t *);
int rump_sys__ksem_open(const char *, int, mode_t, unsigned int, intptr_t *);
int rump_sys__ksem_unlink(const char *);
int rump_sys__ksem_close(intptr_t);
int rump_sys__ksem_post(intptr_t);
int rump_sys__ksem_wait(intptr_t);
int rump_sys__ksem_trywait(intptr_t);
int rump_sys__ksem_getvalue(intptr_t, unsigned int *);
int rump_sys__ksem_destroy(intptr_t);
int rump_sys_lchmod(const char *, mode_t);
int rump_sys_lchown(const char *, uid_t, gid_t);
pid_t rump_sys_getsid(pid_t);
int rump_sys___getcwd(char *, size_t);
int rump_sys_fchroot(int);
int rump_sys_lchflags(const char *, u_long);
int rump_sys_issetugid(void);
int rump_sys_kqueue(void);
int rump_sys_fsync_range(int, int, off_t, off_t);
int rump_sys_getvfsstat(struct statvfs *, size_t, int);
int rump_sys_statvfs1(const char *, struct statvfs *, int);
int rump_sys_fstatvfs1(int, struct statvfs *, int);
int rump_sys_extattrctl(const char *, int, const char *, int, const char *);
int rump_sys_extattr_set_file(const char *, int, const char *, const void *, size_t);
ssize_t rump_sys_extattr_get_file(const char *, int, const char *, void *, size_t);
int rump_sys_extattr_delete_file(const char *, int, const char *);
int rump_sys_extattr_set_fd(int, int, const char *, const void *, size_t);
ssize_t rump_sys_extattr_get_fd(int, int, const char *, void *, size_t);
int rump_sys_extattr_delete_fd(int, int, const char *);
int rump_sys_extattr_set_link(const char *, int, const char *, const void *, size_t);
ssize_t rump_sys_extattr_get_link(const char *, int, const char *, void *, size_t);
int rump_sys_extattr_delete_link(const char *, int, const char *);
ssize_t rump_sys_extattr_list_fd(int, int, void *, size_t);
ssize_t rump_sys_extattr_list_file(const char *, int, void *, size_t);
ssize_t rump_sys_extattr_list_link(const char *, int, void *, size_t);
int rump_sys_setxattr(const char *, const char *, void *, size_t, int);
int rump_sys_lsetxattr(const char *, const char *, void *, size_t, int);
int rump_sys_fsetxattr(int, const char *, void *, size_t, int);
int rump_sys_getxattr(const char *, const char *, void *, size_t);
int rump_sys_lgetxattr(const char *, const char *, void *, size_t);
int rump_sys_fgetxattr(int, const char *, void *, size_t);
int rump_sys_listxattr(const char *, char *, size_t);
int rump_sys_llistxattr(const char *, char *, size_t);
int rump_sys_flistxattr(int, char *, size_t);
int rump_sys_removexattr(const char *, const char *);
int rump_sys_lremovexattr(const char *, const char *);
int rump_sys_fremovexattr(int, const char *);
int rump_sys_getdents(int, char *, size_t) __RENAME(rump_sys___getdents30);
int rump_sys_socket(int, int, int) __RENAME(rump_sys___socket30);
int rump_sys_getfh(const char *, void *, size_t *) __RENAME(rump_sys___getfh30);
int rump_sys_fhopen(const void *, size_t, int) __RENAME(rump_sys___fhopen40);
int rump_sys_fhstatvfs1(const void *, size_t, struct statvfs *, int) __RENAME(rump_sys___fhstatvfs140);
int rump_sys_mount(const char *, const char *, int, void *, size_t) __RENAME(rump_sys___mount50);
int rump_sys_posix_fadvise(int, off_t, off_t, int) __RENAME(rump_sys___posix_fadvise50);
int rump_sys_select(int, fd_set *, fd_set *, fd_set *, struct timeval *) __RENAME(rump_sys___select50);
int rump_sys_utimes(const char *, const struct timeval *) __RENAME(rump_sys___utimes50);
int rump_sys_futimes(int, const struct timeval *) __RENAME(rump_sys___futimes50);
int rump_sys_lutimes(const char *, const struct timeval *) __RENAME(rump_sys___lutimes50);
int rump_sys_kevent(int, const struct kevent *, size_t, struct kevent *, size_t, const struct timespec *) __RENAME(rump_sys___kevent50);
int rump_sys_pselect(int, fd_set *, fd_set *, fd_set *, const struct timespec *, const sigset_t *) __RENAME(rump_sys___pselect50);
int rump_sys_pollts(struct pollfd *, u_int, const struct timespec *, const sigset_t *) __RENAME(rump_sys___pollts50);
int rump_sys_stat(const char *, struct stat *) __RENAME(rump_sys___stat50);
int rump_sys_fstat(int, struct stat *) __RENAME(rump_sys___fstat50);
int rump_sys_lstat(const char *, struct stat *) __RENAME(rump_sys___lstat50);
int rump_sys_mknod(const char *, mode_t, dev_t) __RENAME(rump_sys___mknod50);
int rump_sys_fhstat(const void *, size_t, struct stat *) __RENAME(rump_sys___fhstat50);
int rump_sys_pipe(int *);

#include <rump/rump_syscalls_compat.h>

#endif /* _RUMP_RUMP_SYSCALLS_H_ */
