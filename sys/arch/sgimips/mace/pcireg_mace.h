/*	$NetBSD: pcireg_mace.h,v 1.2 2005/12/11 12:18:54 christos Exp $	*/

/*
 * Copyright (c) 2000 Soren S. Jorvang
 * All rights reserved.
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
 *          This product includes software developed for the
 *          NetBSD Project.  See http://www.NetBSD.org/ for
 *          information about NetBSD.
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

#define MACEPCI_ERROR_ADDR	0x0000
#define MACEPCI_ERROR_FLAGS	0x0004
#define FLAGS_66MHZ_CAP		0x00000001
#define FLAGS_BACKTOBACK_CAP	0x00000002
#define FLAGS_DEVSEL_FAST	0x00000000
#define FLAGS_DEVSEL_MEDIUM	0x00000004
#define FLAGS_DEVSEL_SLOW	0x00000008
#define FLAGS_DEVSEL_MASK	0x0000000c
#define FLAGS_SIG_TARGET_ABORT	0x00000010
#define FLAGS_ADDR_RETRY_ERROR	0x00010000
#define FLAGS_ADDR_DATA_PARITY	0x00020000
#define FLAGS_ADDR_TARGET_ABORT	0x00040000
#define FLAGS_ADDR_MASTER_ABORT	0x00080000
#define FLAGS_ADDR_CONFIG_SPACE	0x00100000
#define FLAGS_ADDR_MEMORY_SPACE	0x00200000
#define FLAGS_SIG_SYSTEM_ERROR	0x00400000
#define FLAGS_READ_BUF_OVERRUN	0x00800000
#define FLAGS_PARITY_ERROR	0x01000000
#define FLAGS_INTERRUPT_TEST	0x02000000
#define FLAGS_SYSTEM_ERROR	0x04000000
#define FLAGS_ILL_HOST_TRANSACT	0x08000000
#define FLAGS_RETRY_ERROR	0x10000000
#define FLAGS_DATA_PARITY	0x20000000
#define FLAGS_TARGET_ABORT	0x40000000
#define FLAGS_MASTER_ABORT	0x80000000
#define MACEPCI_CONTROL		0x0008
#define CONTROL_INT0_SCSI0	0x00000001
#define CONTROL_INT1_SCSI1	0x00000002
#define CONTROL_INT2_SLOT0_A	0x00000004
#define CONTROL_INT3_SLOT1_A	0x00000008
#define CONTROL_INT4_SLOT2_A	0x00000010
#define CONTROL_INT5_SLOTS_BCD	0x00000020
#define CONTROL_INT6_SLOTS_CDB	0x00000040
#define CONTROL_INT7_SLOTS_DBC	0x00000080
#define CONTROL_INT_MASK	0x000000ff
#define CONTROL_SERR_N_ENABLE	0x00000100
#define CONTROL_REQ_N6_PRIORITY	0x00000200		/* Revision 1 only */
#define CONTROL_PARITY_ERROR	0x00000400
#define CONTROL_MRM_READAHEAD	0x00000800
#define CONTROL_REG_N3_PRIORITY	0x00001000
#define CONTROL_REG_N4_PRIORITY	0x00002000
#define CONTROL_REG_N5_PRIORITY	0x00004000
#define CONTROL_PARKING_ON_LAST	0x00008000
#define CONTROL_INT0_INVAL_BUFS	0x00010000
#define CONTROL_INT1_INVAL_BUFS	0x00020000
#define CONTROL_INT2_INVAL_BUFS	0x00040000
#define CONTROL_INT3_INVAL_BUFS	0x00080000
#define CONTROL_INT4_INVAL_BUFS	0x00100000
#define CONTROL_INT5_INVAL_BUFS	0x00200000
#define CONTROL_INT6_INVAL_BUFS	0x00400000
#define CONTROL_INT7_INVAL_BUFS	0x00800000
#define CONTROL_OVERRUN_COND_I	0x01000000
#define CONTROL_PARITY_ERROR_I	0x02000000
#define CONTROL_SYSTEM_ERROR_I	0x04000000
#define CONTROL_ILL_TRANS_I	0x08000000
#define CONTROL_RETRY_ERROR_I	0x10000000
#define CONTROL_DATA_PARITY_I	0x20000000
#define CONTROL_TARGET_ABORT_I	0x40000000
#define CONTROL_MASTER_ABORT_I	0x80000000
#define MACEPCI_REVISION	0x000c
#define MACEPCI_WBFLUSH		0x000c
#define MACEPCI_CONFIG_ADDR	0x0cf8
#define MACEPCI_CONFIG_DATA	0x0cfc

#define MACEPCI_MEM_KSEG	0x1a000000
#define MACEPCI_IO_KSEG		0x18000000
#define MACEPCI_MEM_XKSEG	0x000000280000000
#define MACEPCI_IO_XKSEG	0x000000100000000
