/*	$NetBSD: process.c,v 1.10 2000/11/19 16:40:16 bouyer Exp $	*/

/*
 * Copyright (c) 1993-95 Mats O Jansson.  All rights reserved.
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
 *	This product includes software developed by Mats O Jansson.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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
 */

#include <sys/cdefs.h>
#ifndef lint
__RCSID("$NetBSD: process.c,v 1.10 2000/11/19 16:40:16 bouyer Exp $");
#endif

#include "os.h"
#include "cmp.h"
#include "common.h"
#include "dl.h"
#include "file.h"
#include "get.h"
#include "mopdef.h"
#include "nmadef.h"
#include "pf.h"
#include "print.h"
#include "put.h"
#include "rc.h"

extern u_char	buf[];
extern int	DebugFlag;

struct dllist dllist[MAXDL];		/* dump/load list		*/
extern char	dl_mcst[];		/* Dump/Load Multicast		*/
extern char	rc_mcst[];		/* Remote Console Multicast	*/

void	mopNextLoad __P((u_char *, u_char *, u_char, int));
void	mopProcessDL __P((FILE *, struct if_info *, u_char *, int *,
	    u_char *, u_char *, int, u_short));
void	mopProcessRC __P((FILE *, struct if_info *, u_char *, int *,
	    u_char *, u_char *, int, u_short));
void	mopProcessInfo __P((u_char *, int *, u_short, struct dllist *, int));
void	mopSendASV __P((u_char *, u_char *, struct if_info *, int));
void	mopStartLoad __P((u_char *, u_char *, struct dllist *, int));

void
mopProcessInfo(pkt, index, moplen, dl_rpr, trans)
	u_char  *pkt;
	int     *index;
	u_short moplen;
	struct  dllist  *dl_rpr;
	int	trans;
{
        u_short itype,tmps;
	u_char  ilen ,tmpc,device;
	u_char  uc1,uc2,uc3,*ucp;
	
	device = 0;

	switch(trans) {
	case TRANS_ETHER:
		moplen = moplen + 16;
		break;
	case TRANS_8023:
		moplen = moplen + 14;
		break;
	}

	itype = mopGetShort(pkt,index); 

	while (*index < (int)(moplen)) {
		ilen  = mopGetChar(pkt,index);
		switch (itype) {
		case 0:
			tmpc  = mopGetChar(pkt,index);
			*index = *index + tmpc;
			break;
		case MOP_K_INFO_VER:
			uc1 = mopGetChar(pkt,index);
			uc2 = mopGetChar(pkt,index);
			uc3 = mopGetChar(pkt,index);
			break;
		case MOP_K_INFO_MFCT:
			tmps = mopGetShort(pkt,index);
			break;
		case MOP_K_INFO_CNU:
			ucp = pkt + *index; *index = *index + 6;
			break;
		case MOP_K_INFO_RTM:
			tmps = mopGetShort(pkt,index);
			break;
		case MOP_K_INFO_CSZ:
			tmps = mopGetShort(pkt,index);
			break;
		case MOP_K_INFO_RSZ:
			tmps = mopGetShort(pkt,index);
			break;
		case MOP_K_INFO_HWA:
			ucp = pkt + *index; *index = *index + 6;
			break;
		case MOP_K_INFO_TIME:
			ucp = pkt + *index; *index = *index + 10;
			break;
		case MOP_K_INFO_SOFD:
			device = mopGetChar(pkt,index);
			break;
		case MOP_K_INFO_SFID:
			tmpc = mopGetChar(pkt,index);
			ucp = pkt + *index; *index = *index + tmpc;
			break;
		case MOP_K_INFO_PRTY:
			tmpc = mopGetChar(pkt,index);
			break;
		case MOP_K_INFO_DLTY:
			tmpc = mopGetChar(pkt,index);
			break;
		case MOP_K_INFO_DLBSZ:
			tmps = mopGetShort(pkt,index);
			dl_rpr->dl_bsz = tmps;
			break;
		default:
			if (((device = NMA_C_SOFD_LCS) ||   /* DECserver 100 */
			     (device = NMA_C_SOFD_DS2) ||   /* DECserver 200 */
			     (device = NMA_C_SOFD_DP2) ||   /* DECserver 250 */
			     (device = NMA_C_SOFD_DS3)) &&  /* DECserver 300 */
			    ((itype > 101) && (itype < 107)))
			{
				switch (itype) {
				case 102:
					ucp = pkt + *index;
					*index = *index + ilen;
					break;
				case 103:
					ucp = pkt + *index;
					*index = *index + ilen;
					break;
				case 104:
					tmps = mopGetShort(pkt,index);
					break;
				case 105:
					ucp = pkt + *index;
					*index = *index + ilen;
					break;
				case 106:
					ucp = pkt + *index;
					*index = *index + ilen;
					break;
				};
			} else {
				ucp = pkt + *index; *index = *index + ilen;
			};
		}
		itype = mopGetShort(pkt,index); 
        }
}

void
mopSendASV(dst, src, ii, trans)
	u_char	*dst,*src;
	struct if_info *ii;
	int	 trans;
{
        u_char	 pkt[200], *p;
	int	 index;
	u_char	 mopcode = MOP_K_CODE_ASV;
	u_short	 newlen = 0,ptype = MOP_K_PROTO_DL;

	index = 0;
	mopPutHeader(pkt, &index, dst, src, ptype, trans);

	p = &pkt[index];
	mopPutChar(pkt,&index,mopcode);
	
	mopPutLength(pkt, trans, index);
	newlen = mopGetLength(pkt, trans);

	if ((DebugFlag == DEBUG_ONELINE)) {
		mopPrintOneline(stdout, pkt, trans);
	}

	if ((DebugFlag >= DEBUG_HEADER)) {
		mopPrintHeader(stdout, pkt, trans);
		mopPrintMopHeader(stdout, pkt, trans);
	}
	
	if ((DebugFlag >= DEBUG_INFO)) {
		mopDumpDL(stdout, pkt, trans);
	}

	if (pfWrite(ii->fd, pkt, index, trans) != index) {
		if (DebugFlag) {
			(void)fprintf(stderr, "error pfWrite()\n");
		}
	}
}

#define MAX_ETH_PAYLOAD 1492

void
mopStartLoad(dst, src, dl_rpr, trans)
	u_char	*dst,*src;
	struct dllist *dl_rpr;
	int	 trans;
{
	int	 len;
	int	 i, slot;
	u_char	 pkt[BUFSIZE], *p;
	int	 index;
	u_char	 mopcode = MOP_K_CODE_MLD;
	u_short	 newlen,ptype = MOP_K_PROTO_DL;
	struct dllist *dle;

	slot = -1;
	
	/* Look if we have a non terminated load, if so, use it's slot */

	for (i = 0, dle = dllist; i < MAXDL; i++, dle++) {
		if (dle->status != DL_STATUS_FREE) {
			if (mopCmpEAddr(dle->eaddr, dst) == 0) {
				slot = i;
			}
		}
	}
	
	/* If no slot yet, then find first free */

	if (slot == -1) {
		for (i = 0, dle = dllist; i < MAXDL; i++, dle++) {
			if (dle->status == DL_STATUS_FREE) {
				if (slot == -1) {
					slot = i;
					memmove((char *)dle->eaddr,
					    (char *)dst, 6);
				}
			}
		}
	}

	/* If no slot yet, then return. No slot is free */
	
	if (slot == -1)
		return;
	
	/* Ok, save info from RPR */

	dllist[slot] = *dl_rpr;
	dle = &dllist[slot];
	dle->status = DL_STATUS_READ_IMGHDR;
	
	/* Get Load and Transfer Address. */

	GetFileInfo(dle->ldfd,
		    &dle->loadaddr,
		    &dle->xferaddr,
		    &dle->aout,
		    &dle->a_text, &dle->a_text_fill,
		    &dle->a_data, &dle->a_data_fill,
		    &dle->a_bss,  &dle->a_bss_fill);

	dle->nloadaddr = dle->loadaddr;
	dle->lseek     = lseek(dle->ldfd, 0L, SEEK_CUR);
	dle->a_lseek   = 0;

	dle->count     = 0;
	if (dle->dl_bsz >= MAX_ETH_PAYLOAD || dle->dl_bsz == 0)
		dle->dl_bsz = MAX_ETH_PAYLOAD;
	if (dle->dl_bsz == 1030)	/* VS/uVAX 2000 needs this */
		dle->dl_bsz = 1000;
	if (dle->dl_bsz == 0)		/* Needed by "big" VAXen */
		dle->dl_bsz = MAX_ETH_PAYLOAD;
	if (trans == TRANS_8023)
		dle->dl_bsz = dle->dl_bsz - 8;

	index = 0;
	mopPutHeader(pkt, &index, dst, src, ptype, trans);
	p = &pkt[index];
	mopPutChar (pkt, &index, mopcode);

	mopPutChar (pkt, &index, dle->count);
	mopPutLong (pkt, &index, dle->loadaddr);

	len = mopFileRead(dle, &pkt[index]);

	dle->nloadaddr = dle->loadaddr + len;
	index = index + len;

	mopPutLength(pkt, trans, index);
	newlen = mopGetLength(pkt, trans);

	if ((DebugFlag == DEBUG_ONELINE)) {
		mopPrintOneline(stdout, pkt, trans);
	}

	if ((DebugFlag >= DEBUG_HEADER)) {
		mopPrintHeader(stdout, pkt, trans);
		mopPrintMopHeader(stdout, pkt, trans);
	}
	
	if ((DebugFlag >= DEBUG_INFO)) {
		mopDumpDL(stdout, pkt, trans);
	}

	if (pfWrite(dle->ii->fd, pkt, index, trans) != index) {
		if (DebugFlag) {
			(void)fprintf(stderr, "error pfWrite()\n");
		}
	}

	dle->status = DL_STATUS_SENT_MLD;
}

void
mopNextLoad(dst, src, new_count, trans)
	u_char	*dst, *src, new_count;
	int	 trans;
{
	int	 len;
	int	 i, slot;
	u_char	 pkt[BUFSIZE], *p;
	int	 index, pindex;
	char	 line[100];
	u_short  newlen = 0,ptype = MOP_K_PROTO_DL;
	u_char	 mopcode;
	struct dllist *dle;

	slot = -1;
	
	for (i = 0, dle = dllist; i < MAXDL; i++, dle++) {
		if (dle->status != DL_STATUS_FREE) {
			if (mopCmpEAddr(dst, dle->eaddr) == 0)
				slot = i;
		}
	}

	/* If no slot yet, then return. No slot is free */
	
	if (slot == -1)
		return;

	dle = &dllist[slot];

	if ((new_count == ((dle->count+1) % 256))) {
		dle->loadaddr = dllist[slot].nloadaddr;
		dle->count    = new_count;
	} else if (new_count != (dle->count % 256)) {
		return;
	}

	if (dle->status == DL_STATUS_SENT_PLT) {
		close(dle->ldfd);
		dle->ldfd = -1;
		dle->status = DL_STATUS_FREE;
		sprintf(line,
			"%x:%x:%x:%x:%x:%x Load completed",
			dst[0],dst[1],dst[2],dst[3],dst[4],dst[5]);
		syslog(LOG_INFO, "%s", line);
		return;
	}

	dle->lseek     = lseek(dle->ldfd, 0L, SEEK_CUR);
	
	if (dle->dl_bsz >= MAX_ETH_PAYLOAD)
		dle->dl_bsz = MAX_ETH_PAYLOAD;
	
	index = 0;
	mopPutHeader(pkt, &index, dst, src, ptype, trans);
	p = &pkt[index];
	mopcode = MOP_K_CODE_MLD;
	pindex = index;
	mopPutChar (pkt,&index, mopcode);
	mopPutChar (pkt,&index, dle->count);
	mopPutLong (pkt,&index, dle->loadaddr);

	len = mopFileRead(dle, &pkt[index]);
	
	if (len > 0 ) {
			
		dle->nloadaddr = dle->loadaddr + len;
		index = index + len;

		mopPutLength(pkt, trans, index);
		newlen = mopGetLength(pkt, trans);
		
	} else {
		if (len == 0) {
			index = pindex;
			mopcode = MOP_K_CODE_PLT;
			mopPutChar (pkt, &index, mopcode);
			mopPutChar (pkt, &index, dle->count);
			mopPutChar (pkt, &index, MOP_K_PLTP_HSN);
 			mopPutChar (pkt, &index, 3);
			mopPutMulti(pkt, &index, "ipc", 3);
			mopPutChar (pkt, &index, MOP_K_PLTP_HSA);
			mopPutChar (pkt, &index, 6);
			mopPutMulti(pkt, &index, src, 6);
			mopPutChar (pkt, &index, MOP_K_PLTP_HST);
			mopPutTime (pkt, &index, 0);
			mopPutChar (pkt, &index, 0);
			mopPutLong (pkt, &index, dle->xferaddr);

			mopPutLength(pkt, trans, index);
			newlen = mopGetLength(pkt, trans);
		
			dle->status = DL_STATUS_SENT_PLT;
		} else {
			dle->status = DL_STATUS_FREE;
			return;
		}
	}

	if ((DebugFlag == DEBUG_ONELINE)) {
		mopPrintOneline(stdout, pkt, trans);
	}

	if ((DebugFlag >= DEBUG_HEADER)) {
		mopPrintHeader(stdout, pkt, trans);
		mopPrintMopHeader(stdout, pkt, trans);
	}
	
	if ((DebugFlag >= DEBUG_INFO)) {
		mopDumpDL(stdout, pkt, trans);
	}

	if (pfWrite(dle->ii->fd, pkt, index, trans) != index) {
		if (DebugFlag) {
			(void)fprintf(stderr, "error pfWrite()\n");
		}
	}
}

void
mopProcessDL(fd, ii, pkt, index, dst, src, trans, len)
	FILE	*fd;
	struct if_info *ii;
	u_char	*pkt;
	int	*index;
	u_char	*dst, *src;
	int	 trans;
	u_short	 len;
{
	u_char  tmpc;
	u_short moplen;
	u_char  pfile[129], mopcode;
	char    filename[FILENAME_MAX];
	char    line[100];
	int     i,nfd,iindex;
	struct dllist dl,*dl_rpr;
	u_char  rpr_pgty,load;

	if ((DebugFlag == DEBUG_ONELINE)) {
		mopPrintOneline(stdout, pkt, trans);
	}

	if ((DebugFlag >= DEBUG_HEADER)) {
		mopPrintHeader(stdout, pkt, trans);
		mopPrintMopHeader(stdout, pkt, trans);
	}
	
	if ((DebugFlag >= DEBUG_INFO)) {
		mopDumpDL(stdout, pkt, trans);
	}

	moplen  = mopGetLength(pkt, trans);
	mopcode = mopGetChar(pkt,index);

	switch (mopcode) {
	case MOP_K_CODE_MLT:
		break;
	case MOP_K_CODE_DCM:
		break;
	case MOP_K_CODE_MLD:
		break;
	case MOP_K_CODE_ASV:
		break;
	case MOP_K_CODE_RMD:
		break;
	case MOP_K_CODE_RPR:
		
		tmpc = mopGetChar(pkt,index);		/* Device Type */
		
		tmpc = mopGetChar(pkt,index);		/* Format Version */
		if ((tmpc != MOP_K_RPR_FORMAT) &&
		    (tmpc != MOP_K_RPR_FORMAT_V3)) {
			(void)fprintf(stderr,"mopd: Unknown RPR Format (%d) from ",tmpc);
			mopPrintHWA(stderr,src);
			(void)fprintf(stderr,"\n");
		}
		
		rpr_pgty = mopGetChar(pkt,index);	/* Program Type */
		
		tmpc = mopGetChar(pkt,index);		/* Software ID Len */
		if (tmpc > sizeof(pfile) - 1)
			return;
		for (i = 0; i < tmpc; i++) {
			pfile[i] = mopGetChar(pkt,index);
			pfile[i+1] = '\0';
		}

		if (tmpc == 0) {
			/* In a normal implementation of a MOP Loader this */
			/* would cause a question to NML (DECnet) if this  */
			/* node is known and if so what image to load. But */
			/* we don't have DECnet so we don't have anybody   */
			/* to ask. My solution is to use the ethernet addr */
			/* as filename. Implementing a database would be   */
			/* overkill.					   */
			sprintf(pfile,"%02x%02x%02x%02x%02x%02x%c",
				src[0],src[1],src[2],src[3],src[4],src[5],0);
		}
		
		tmpc = mopGetChar(pkt,index);		/* Processor */
	
		iindex = *index;
		dl_rpr = &dl;
		memset(dl_rpr, 0, sizeof(*dl_rpr));
		dl_rpr->ii = ii;
		memmove((char *)(dl_rpr->eaddr), (char *)src, 6);
		mopProcessInfo(pkt,index,moplen,dl_rpr,trans);

		sprintf(filename,"%s/%s.SYS", MOP_FILE_PATH, pfile);
		if ((mopCmpEAddr(dst,dl_mcst) == 0)) {
			if ((nfd = open(filename, O_RDONLY, 0)) != -1) {
				close(nfd);
				mopSendASV(src, ii->eaddr, ii, trans);
				sprintf(line,
					"%x:%x:%x:%x:%x:%x (%d) Do you have %s? (Yes)",
					src[0],src[1],src[2],
					src[3],src[4],src[5],trans,pfile);
			} else {
				sprintf(line,
					"%x:%x:%x:%x:%x:%x (%d) Do you have %s? (No)",
					src[0],src[1],src[2],
					src[3],src[4],src[5],trans,pfile);
			}
			syslog(LOG_INFO, "%s", line);
		} else {
			if ((mopCmpEAddr(dst,ii->eaddr) == 0)) {
				dl_rpr->ldfd = open(filename, O_RDONLY, 0);
				mopStartLoad(src, ii->eaddr, dl_rpr, trans);
				sprintf(line,
					"%x:%x:%x:%x:%x:%x Send me %s",
					src[0],src[1],src[2],
					src[3],src[4],src[5],pfile);
				syslog(LOG_INFO, "%s", line);
			}
		}
		
		break;
	case MOP_K_CODE_RML:
		
		load = mopGetChar(pkt,index);		/* Load Number	*/
		
		tmpc = mopGetChar(pkt,index);		/* Error	*/
		
		if ((mopCmpEAddr(dst,ii->eaddr) == 0)) {
			mopNextLoad(src, ii->eaddr, load, trans);
		}
		
		break;
	case MOP_K_CODE_RDS:
		break;
	case MOP_K_CODE_MDD:
		break;
	case MOP_K_CODE_CCP:
		break;
	case MOP_K_CODE_PLT:
		break;
	default:
		break;
	}
}

void
mopProcessRC(fd, ii, pkt, index, dst, src, trans, len)
	FILE	*fd;
	struct if_info *ii;
	u_char	*pkt;
	int	*index;
	u_char	*dst, *src;
	int	 trans;
	u_short	 len;
{
	u_char	 tmpc;
	u_short	 tmps, moplen = 0;
	u_char   mopcode;
	struct dllist dl,*dl_rpr;

	if ((DebugFlag == DEBUG_ONELINE)) {
		mopPrintOneline(stdout, pkt, trans);
	}

	if ((DebugFlag >= DEBUG_HEADER)) {
		mopPrintHeader(stdout, pkt, trans);
		mopPrintMopHeader(stdout, pkt, trans);
	}
	
	if ((DebugFlag >= DEBUG_INFO)) {
		mopDumpRC(stdout, pkt, trans);
	}

	moplen  = mopGetLength(pkt, trans);
	mopcode = mopGetChar(pkt,index);

	switch (mopcode) {
	case MOP_K_CODE_RID:
		break;
	case MOP_K_CODE_BOT:
		break;
	case MOP_K_CODE_SID:
		
		tmpc = mopGetChar(pkt,index);		/* Reserved */
		
		if ((DebugFlag >= DEBUG_INFO)) {
			(void)fprintf(stderr, "Reserved     :   %02x\n",tmpc);
		}
		
		tmps = mopGetShort(pkt,index);		/* Receipt # */
		if ((DebugFlag >= DEBUG_INFO)) {
			(void)fprintf(stderr, "Receipt Nbr  : %04x\n",tmpc);
		}
		
		dl_rpr = &dl;
		memset(dl_rpr, 0, sizeof(*dl_rpr));
		dl_rpr->ii = ii;
		memmove((char *)(dl_rpr->eaddr), (char *)src, 6);
		mopProcessInfo(pkt,index,moplen,dl_rpr,trans);
		
		break;
	case MOP_K_CODE_RQC:
		break;
	case MOP_K_CODE_CNT:
		break;
	case MOP_K_CODE_RVC:
		break;
	case MOP_K_CODE_RLC:
		break;
	case MOP_K_CODE_CCP:
		break;
	case MOP_K_CODE_CRA:
		break;
	default:
		break;
	}
}

