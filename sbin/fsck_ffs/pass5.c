/*	$NetBSD: pass5.c,v 1.19 1998/03/18 17:01:24 bouyer Exp $	*/

/*
 * Copyright (c) 1980, 1986, 1993
 *	The Regents of the University of California.  All rights reserved.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
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
 */

#include <sys/cdefs.h>
#ifndef lint
#if 0
static char sccsid[] = "@(#)pass5.c	8.9 (Berkeley) 4/28/95";
#else
__RCSID("$NetBSD: pass5.c,v 1.19 1998/03/18 17:01:24 bouyer Exp $");
#endif
#endif /* not lint */

#include <sys/param.h>
#include <sys/time.h>

#include <ufs/ufs/dinode.h>
#include <ufs/ffs/fs.h>
#include <ufs/ffs/ffs_extern.h>
#include <ufs/ufs/ufs_bswap.h>

#include <err.h>
#include <string.h>
#include <malloc.h>

#include "fsutil.h"
#include "fsck.h"
#include "extern.h"

void print_bmap __P((u_char *,u_int32_t));

void
pass5()
{
	int c, blk, frags, basesize, sumsize, mapsize, savednrpos = 0;
	struct fs *fs = sblock;
	ufs_daddr_t dbase, dmax;
	ufs_daddr_t d;
	long i, j;
	struct csum *cs;
	struct csum cstotal;
	struct inodesc idesc[3];
	char buf[MAXBSIZE];
	struct cg *newcg = (struct cg *)buf;
	struct ocg *ocg = (struct ocg *)buf;
	struct cg *cg;

	cg = cgrp = malloc(fs->fs_cgsize);
	if (cg == NULL)
		errx(EEXIT, "cannot allocate space for cylinder group");

	statemap[WINO] = USTATE;
	memset(newcg, 0, (size_t)fs->fs_cgsize);
	newcg->cg_niblk = fs->fs_ipg;
	if (cvtlevel >= 3) {
		if (fs->fs_maxcontig < 2 && fs->fs_contigsumsize > 0) {
			if (preen)
				pwarn("DELETING CLUSTERING MAPS\n");
			if (preen || reply("DELETE CLUSTERING MAPS")) {
				fs->fs_contigsumsize = 0;
				doinglevel1 = 1;
				sbdirty();
			}
		}
		if (fs->fs_maxcontig > 1) {
			char *doit = 0;

			if (fs->fs_contigsumsize < 1) {
				doit = "CREAT";
			} else if (fs->fs_contigsumsize < fs->fs_maxcontig &&
				   fs->fs_contigsumsize < FS_MAXCONTIG) {
				doit = "EXPAND";
			}
			if (doit) {
				i = fs->fs_contigsumsize;
				fs->fs_contigsumsize =
				    MIN(fs->fs_maxcontig, FS_MAXCONTIG);
				if (CGSIZE(fs) > fs->fs_bsize) {
					pwarn("CANNOT %s CLUSTER MAPS\n", doit);
					fs->fs_contigsumsize = i;
				} else if (preen ||
				    reply("CREATE CLUSTER MAPS")) {
					if (preen)
						pwarn("%sING CLUSTER MAPS\n",
						    doit);
					fs->fs_cgsize =
					    fragroundup(fs, CGSIZE(fs));
					doinglevel1 = 1;
					sbdirty();
				}
			}
		}
	}
	switch ((int)fs->fs_postblformat) {

	case FS_42POSTBLFMT:
		basesize = (char *)(&ocg->cg_btot[0]) -
		    (char *)(&ocg->cg_firstfield);
		sumsize = &ocg->cg_iused[0] - (u_int8_t *)(&ocg->cg_btot[0]);
		mapsize = &ocg->cg_free[howmany(fs->fs_fpg, NBBY)] -
			(u_char *)&ocg->cg_iused[0];
		ocg->cg_magic = CG_MAGIC;
		savednrpos = fs->fs_nrpos;
		fs->fs_nrpos = 8;
		break;

	case FS_DYNAMICPOSTBLFMT:
		newcg->cg_btotoff =
		     &newcg->cg_space[0] - (u_char *)(&newcg->cg_firstfield);
		newcg->cg_boff =
		    newcg->cg_btotoff + fs->fs_cpg * sizeof(int32_t);
		newcg->cg_iusedoff = newcg->cg_boff + 
		    fs->fs_cpg * fs->fs_nrpos * sizeof(int16_t);
		newcg->cg_freeoff =
		    newcg->cg_iusedoff + howmany(fs->fs_ipg, NBBY);
		if (fs->fs_contigsumsize <= 0) {
			newcg->cg_nextfreeoff = newcg->cg_freeoff +
			    howmany(fs->fs_cpg * fs->fs_spc / NSPF(fs), NBBY);
		} else {
			newcg->cg_clustersumoff = newcg->cg_freeoff +
			    howmany(fs->fs_cpg * fs->fs_spc / NSPF(fs), NBBY) -
			    sizeof(int32_t);
			newcg->cg_clustersumoff =
			    roundup(newcg->cg_clustersumoff, sizeof(int32_t));
			newcg->cg_clusteroff = newcg->cg_clustersumoff +
			    (fs->fs_contigsumsize + 1) * sizeof(int32_t);
			newcg->cg_nextfreeoff = newcg->cg_clusteroff +
			    howmany(fs->fs_cpg * fs->fs_spc / NSPB(fs), NBBY);
		}
		newcg->cg_magic = CG_MAGIC;
		basesize = &newcg->cg_space[0] -
		    (u_char *)(&newcg->cg_firstfield);
		sumsize = newcg->cg_iusedoff - newcg->cg_btotoff;
		mapsize = newcg->cg_nextfreeoff - newcg->cg_iusedoff;
		break;

	default:
		sumsize = 0;	/* keep lint happy */
		errx(EEXIT, "UNKNOWN ROTATIONAL TABLE FORMAT %d",
			fs->fs_postblformat);
	}
	memset(&idesc[0], 0, sizeof idesc);
	for (i = 0; i < 3; i++) {
		idesc[i].id_type = ADDR;
		if (doinglevel2)
			idesc[i].id_fix = FIX;
	}
	memset(&cstotal, 0, sizeof(struct csum));
	j = blknum(fs, fs->fs_size + fs->fs_frag - 1);
	for (i = fs->fs_size; i < j; i++)
		setbmap(i);
	for (c = 0; c < fs->fs_ncg; c++) {
		getblk(&cgblk, cgtod(fs, c), fs->fs_cgsize);
		memcpy(cg, cgblk.b_un.b_cg, fs->fs_cgsize);
		if((doswap && !needswap) || (!doswap && needswap))
			swap_cg(cgblk.b_un.b_cg, cg);
		if (!cg_chkmagic(cg, 0))
			pfatal("CG %d: BAD MAGIC NUMBER\n", c);
		if(doswap)
			cgdirty();
		dbase = cgbase(fs, c);
		dmax = dbase + fs->fs_fpg;
		if (dmax > fs->fs_size)
			dmax = fs->fs_size;
		newcg->cg_time = cg->cg_time;
		newcg->cg_cgx = c;
		if (c == fs->fs_ncg - 1)
			newcg->cg_ncyl = fs->fs_ncyl % fs->fs_cpg;
		else
			newcg->cg_ncyl = fs->fs_cpg;
		newcg->cg_ndblk = dmax - dbase;
		if (fs->fs_contigsumsize > 0)
			newcg->cg_nclusterblks = newcg->cg_ndblk / fs->fs_frag;
		newcg->cg_cs.cs_ndir = 0;
		newcg->cg_cs.cs_nffree = 0;
		newcg->cg_cs.cs_nbfree = 0;
		newcg->cg_cs.cs_nifree = fs->fs_ipg;
		if (cg->cg_rotor < newcg->cg_ndblk)
			newcg->cg_rotor = cg->cg_rotor;
		else
			newcg->cg_rotor = 0;
		if (cg->cg_frotor < newcg->cg_ndblk)
			newcg->cg_frotor = cg->cg_frotor;
		else
			newcg->cg_frotor = 0;
		if (cg->cg_irotor < newcg->cg_niblk)
			newcg->cg_irotor = cg->cg_irotor;
		else
			newcg->cg_irotor = 0;
		memset(&newcg->cg_frsum[0], 0, sizeof newcg->cg_frsum);
		memset(&cg_blktot(newcg, 0)[0], 0,
		      (size_t)(sumsize + mapsize));
		if (fs->fs_postblformat == FS_42POSTBLFMT)
			ocg->cg_magic = CG_MAGIC;
		j = fs->fs_ipg * c;
		for (i = 0; i < fs->fs_ipg; j++, i++) {
			switch (statemap[j]) {

			case USTATE:
				break;

			case DSTATE:
			case DCLEAR:
			case DFOUND:
				newcg->cg_cs.cs_ndir++;
				/* fall through */

			case FSTATE:
			case FCLEAR:
				newcg->cg_cs.cs_nifree--;
				setbit(cg_inosused(newcg, 0), i);
				break;

			default:
				if (j < ROOTINO)
					break;
				errx(EEXIT, "BAD STATE %d FOR INODE I=%ld",
				    statemap[j], (long)j);
			}
		}
		if (c == 0)
			for (i = 0; i < ROOTINO; i++) {
				setbit(cg_inosused(newcg, 0), i);
				newcg->cg_cs.cs_nifree--;
			}
		for (i = 0, d = dbase;
		     d < dmax;
		     d += fs->fs_frag, i += fs->fs_frag) {
			frags = 0;
			for (j = 0; j < fs->fs_frag; j++) {
				if (testbmap(d + j))
					continue;
				setbit(cg_blksfree(newcg, 0), i + j);
				frags++;
			}
			if (frags == fs->fs_frag) {
				newcg->cg_cs.cs_nbfree++;
				j = cbtocylno(fs, i);
				cg_blktot(newcg, 0)[j]++;
				cg_blks(fs, newcg, j, 0)[cbtorpos(fs, i)]++;
				if (fs->fs_contigsumsize > 0)
					setbit(cg_clustersfree(newcg, 0),
					    i / fs->fs_frag);
			} else if (frags > 0) {
				newcg->cg_cs.cs_nffree += frags;
				blk = blkmap(fs, cg_blksfree(newcg, 0), i);
				ffs_fragacct(fs, blk, newcg->cg_frsum, 1, 0);
			}
		}
		if (fs->fs_contigsumsize > 0) {
			int32_t *sump = cg_clustersum(newcg, 0);
			u_char *mapp = cg_clustersfree(newcg, 0);
			int map = *mapp++;
			int bit = 1;
			int run = 0;

			for (i = 0; i < newcg->cg_nclusterblks; i++) {
				if ((map & bit) != 0) {
					run++;
				} else if (run != 0) {
					if (run > fs->fs_contigsumsize)
						run = fs->fs_contigsumsize;
					sump[run]++;
					run = 0;
				}
				if ((i & (NBBY - 1)) != (NBBY - 1)) {
					bit <<= 1;
				} else {
					map = *mapp++;
					bit = 1;
				}
			}
			if (run != 0) {
				if (run > fs->fs_contigsumsize)
					run = fs->fs_contigsumsize;
				sump[run]++;
			}
		}
		cstotal.cs_nffree += newcg->cg_cs.cs_nffree;
		cstotal.cs_nbfree += newcg->cg_cs.cs_nbfree;
		cstotal.cs_nifree += newcg->cg_cs.cs_nifree;
		cstotal.cs_ndir += newcg->cg_cs.cs_ndir;
		cs = &fs->fs_cs(fs, c);
		if (memcmp(&newcg->cg_cs, cs, sizeof *cs) != 0)
			if (dofix(&idesc[0], "FREE BLK COUNT(S) WRONG IN SUPERBLK")) {
			memmove(cs, &newcg->cg_cs, sizeof *cs);
			sbdirty();
			} else
				markclean = 0;
		if (doinglevel1) {
			memmove(cg, newcg, (size_t)fs->fs_cgsize);
			cgdirty();
			continue;
		}
		if (memcmp(cg_inosused(newcg, 0), cg_inosused(cg, 0), mapsize) != 0) {
			if (debug) {
				printf("newcg map:\n");
				print_bmap(cg_inosused(newcg, 0), mapsize);
				printf("\ncg map:\n");
				print_bmap(cg_inosused(cg, 0), mapsize);
			}
		    if (dofix(&idesc[1], "BLK(S) MISSING IN BIT MAPS")) {
			memmove(cg_inosused(cg, 0), cg_inosused(newcg, 0),
			      (size_t)mapsize);
			cgdirty();
			} else
				markclean = 0;
		}
		if (memcmp(newcg, cg, basesize) != 0 ||
		     memcmp(&cg_blktot(newcg, 0)[0],
				&cg_blktot(cg, 0)[0], sumsize) != 0) 
		    if (dofix(&idesc[2], "SUMMARY INFORMATION BAD")) {
			memmove(cg, newcg, (size_t)basesize);
			memmove(&cg_blktot(cg, 0)[0],
			       &cg_blktot(newcg, 0)[0], (size_t)sumsize);
			cgdirty();
			} else markclean = 0;

	}
	if (fs->fs_postblformat == FS_42POSTBLFMT)
		fs->fs_nrpos = savednrpos;
	if (memcmp(&cstotal, &fs->fs_cstotal, sizeof *cs) != 0)
	    if(dofix(&idesc[0], "FREE BLK COUNT(S) WRONG IN SUPERBLK")) {
		memmove(&fs->fs_cstotal, &cstotal, sizeof *cs);
		fs->fs_ronly = 0;
		fs->fs_fmod = 0;
		sbdirty();
		} else
			markclean = 0;
}

void 
print_bmap(map, size)
	u_char *map;
	u_int32_t size;
{
	int i, j;

	i = 0;
	while (i < size) {
		printf("%u: ",i);
		for (j = 0; j < 16; j++, i++)
			printf("%2x ", (u_int)map[i] & 0xff);
		printf("\n");
	}
}
