/*	$NetBSD: pciide_piix_reg.h,v 1.7 2002/04/23 20:41:18 bouyer Exp $	*/

/*
 * Copyright (c) 1998 Manuel Bouyer.
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
 *	This product includes software developed by Manuel Bouyer.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
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

/*
 * Registers definitions for Intel's PIIX serie PCI IDE controllers.
 * See Intel's
 * "82371FB (PIIX) and 82371SB (PIIX3) PCI ISA IDE XCELERATOR"
 * "82371AB PCI-TO-ISA / IDE XCELERATOR (PIIX4)" and
 * "Intel 82801AA (ICH) and Intel 82801AB (ICH0) I/O Controller Hub"
 * available from http://developers.intel.com/
 */

/*
 * Bus master interface base address register
 */
#define PIIX_BMIBA 0x20
#define PIIX_BMIBA_ADDR(x) (x & 0x0000FFFF0)
#define PIIX_BMIBA_RTE(x) (x & 0x000000001)
#define PIIX_BMIBA_RTE_IO 0x000000001 /* base addr maps to I/O space */

/*
 * IDE timing register 
 * 0x40/0x41 is for primary, 0x42/0x43 for secondary channel
 */
#define PIIX_IDETIM 0x40
#define PIIX_IDETIM_READ(x, channel) (((x) >> (16 * (channel))) & 0x0000FFFF)
#define PIIX_IDETIM_SET(x, bytes, channel) \
	((x) | ((bytes) << (16 * (channel))))
#define PIIX_IDETIM_CLEAR(x, bytes, channel) \
	((x) & ~((bytes) << (16 * (channel))))

#define PIIX_IDETIM_IDE		0x8000 /* PIIX decode IDE registers */
#define PIIX_IDETIM_SITRE	0x4000 /* slaves IDE timing registers
					enabled (PIIX3/4 only) */
#define PIIX_IDETIM_ISP_MASK	0x3000 /* IOrdy sample point */
#define PIIX_IDETIM_ISP_SHIFT	12
#define PIIX_IDETIM_ISP_SET(x)	((x) << PIIX_IDETIM_ISP_SHIFT)
#define PIIX_IDETIM_RTC_MASK	0x0300 /* recovery time */
#define PIIX_IDETIM_RTC_SHIFT	8
#define PIIX_IDETIM_RTC_SET(x)	((x) << PIIX_IDETIM_RTC_SHIFT)
#define PIIX_IDETIM_DTE(d)	(0x0008 << (4 * (d))) /* DMA timing only */
#define PIIX_IDETIM_PPE(d)	(0x0004 << (4 * (d))) /* prefetch/posting */
#define PIIX_IDETIM_IE(d)	(0x0002 << (4 * (d))) /* IORDY enable */
#define PIIX_IDETIM_TIME(d)	(0x0001 << (4 * (d))) /* Fast timing enable */
/*
 * Slave IDE timing register (PIIX3/4 only)
 * This register must be enabled via the PIIX_IDETIM_SITRE bit
 */
#define PIIX_SIDETIM 0x44
#define PIIX_SIDETIM_ISP_MASK(channel) (0x0c << ((channel) * 4))
#define PIIX_SIDETIM_ISP_SHIFT	2
#define PIIX_SIDETIM_ISP_SET(x, channel) \
	(x << (PIIX_SIDETIM_ISP_SHIFT + ((channel) * 4)))
#define PIIX_SIDETIM_RTC_MASK(channel) (0x03 << ((channel) * 4))
#define PIIX_SIDETIM_RTC_SHIFT	0
#define PIIX_SIDETIM_RTC_SET(x, channel) \
	(x << (PIIX_SIDETIM_RTC_SHIFT + ((channel) * 4)))

/*
 * Ultra DMA/33 register (PIIX4 only)
 */
#define PIIX_UDMAREG 0x48
/* Control register */
#define PIIX_UDMACTL_DRV_EN(channel, drive) (0x01 << ((channel) * 2 + (drive)))
/* Ultra DMA/33 timing register (PIIX4 only) */
#define PIIX_UDMATIM_SHIFT 16
#define PIIX_UDMATIM_SET(x, channel, drive) \
	(((x) << ((channel * 8) + (drive * 4))) << PIIX_UDMATIM_SHIFT)

/*
 * IDE config register (ICH/ICH0/ICH2 only)
 */
#define PIIX_CONFIG	0x54
#define PIIX_CONFIG_PINGPONG	0x0400
/* The following are only for the 82801AA (ICH) and 82801BA (ICH2) */
#define PIIX_CONFIG_CR(channel, drive) (0x0010 << ((channel) * 2 + (drive)))
#define PIIX_CONFIG_UDMA66(channel, drive) (0x0001 << ((channel) * 2 + (drive)))
/* The following are only for the 82801BA (ICH2) */
#define PIIX_CONFIG_UDMA100(channel, drive) (0x1000 << ((channel) * 2 + (drive)))

/*
 * these tables define the differents values to upload to the
 * ISP and RTC registers for the various PIO and DMA mode
 * (from the PIIX4 doc).
 */
static const int8_t piix_isp_pio[] __attribute__((__unused__)) =
    {0x00, 0x00, 0x01, 0x02, 0x02};
static const int8_t piix_rtc_pio[] __attribute__((__unused__)) =
    {0x00, 0x00, 0x00, 0x01, 0x03};
static const int8_t piix_isp_dma[] __attribute__((__unused__)) =
    {0x00, 0x02, 0x02};
static const int8_t piix_rtc_dma[] __attribute__((__unused__)) =
    {0x00, 0x02, 0x03};
static const int8_t piix4_sct_udma[] __attribute__((__unused__)) =
    {0x00, 0x01, 0x02, 0x01, 0x02, 0x01};
