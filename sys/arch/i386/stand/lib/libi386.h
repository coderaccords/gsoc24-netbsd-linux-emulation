/*	$NetBSD: libi386.h,v 1.19 2005/06/13 11:37:41 junyoung Exp $	*/

/*
 * Copyright (c) 1996
 *	Matthias Drochner.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

typedef unsigned long physaddr_t;

/* this is in startup code */
void vpbcopy(const void *, void *, size_t);
void pvbcopy(const void *, void *, size_t);
void pbzero(void *, size_t);
physaddr_t vtophys(void *);

ssize_t pread(int, void *, size_t);
void startprog(physaddr_t, int, unsigned long *, physaddr_t);

int exec_netbsd(const char *, physaddr_t, int);

void delay(int);
int getbasemem(void);
int getextmemx(void);
int getextmem1(void);
int biosvideomode(void);
#ifdef CONSERVATIVE_MEMDETECT
#define getextmem() getextmem1()
#else
#define getextmem() getextmemx()
#endif
void printmemlist(void);
void reboot(void);
void gateA20(void);

void initio(int);
#define CONSDEV_PC 0
#define CONSDEV_COM0 1
#define CONSDEV_COM1 2
#define CONSDEV_COM2 3
#define CONSDEV_COM3 4
#define CONSDEV_COM0KBD 5
#define CONSDEV_COM1KBD 6
#define CONSDEV_COM2KBD 7
#define CONSDEV_COM3KBD 8
#define CONSDEV_AUTO (-1)
int iskey(int);
char awaitkey(int, int);

/* this is in "user code"! */
int parsebootfile(const char *, char **, char **, unsigned int *,
		  unsigned int *, const char **);

#ifdef XMS
physaddr_t ppbcopy(physaddr_t, physaddr_t, int);
int checkxms(void);
physaddr_t xmsalloc(int);
#endif

/* parseutils.c */
char *gettrailer(char *);
int parseopts(const char *, int *);
int parseboot(char *, char **, int *);

/* menuutils.c */
struct bootblk_command {
	const char *c_name;
	void (*c_fn)(char *);
};
void bootmenu(void);
void docommand(char *);

/* getsecs.c */
time_t getsecs(void);

/* in "user code": */
void command_help(char *);
extern const struct bootblk_command commands[];

/* asm bios/dos calls */
int biosdiskreset(int);
int biosextread(int, void *);
int biosgetrtc(u_long *);
int biosgetsystime(void);
int biosread(int, int, int, int, int, void *);
int comgetc(int);
void cominit(int);
int computc(int, int);
int comstatus(int);
int congetc(void);
int conisshift(void);
int coniskey(void);
void conputc(int);

int get_diskinfo(int);
int getextmem2(int *);
int getextmemps2(void *);
int getmementry(int *, int *);
int int13_extension(int);
struct biosdisk_ext13info;
void int13_getextinfo(int, struct biosdisk_ext13info *);
int pcibios_cfgread(unsigned int, int, int *);
int pcibios_cfgwrite(unsigned int, int, int);
int pcibios_finddev(int, int, int, unsigned int *);
int pcibios_present(int *);

void dosclose(int);
int dosopen(char *);
int dosread(int, char *, int);
int dosseek(int, int, int);
extern int doserrno;	/* in dos_file.S */
