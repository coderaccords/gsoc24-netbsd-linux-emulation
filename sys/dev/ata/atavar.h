/*	$NetBSD: atavar.h,v 1.66 2005/01/26 21:51:40 jmcneill Exp $	*/

/*
 * Copyright (c) 1998, 2001 Manuel Bouyer.
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

#ifndef _DEV_ATA_ATAVAR_H_
#define	_DEV_ATA_ATAVAR_H_

#include <sys/lock.h>
#include <sys/queue.h>

/* XXX For scsipi_adapter and scsipi_channel. */
#include <dev/scsipi/scsipi_all.h>
#include <dev/scsipi/atapiconf.h>

/*
 * Max number of drives per channel.
 */
#define	ATA_MAXDRIVES		2

/*
 * Description of a command to be handled by an ATA controller.  These
 * commands are queued in a list.
 */
struct ata_xfer {
	__volatile u_int c_flags;	/* command state flags */
	
	/* Channel and drive that are to process the request. */
	struct ata_channel *c_chp;
	int	c_drive;

	void	*c_cmd;			/* private request structure pointer */
	void	*c_databuf;		/* pointer to data buffer */
	int	c_bcount;		/* byte count left */
	int	c_skip;			/* bytes already transferred */
	int	c_dscpoll;		/* counter for dsc polling (ATAPI) */

	/* Link on the command queue. */
	TAILQ_ENTRY(ata_xfer) c_xferchain;

	/* Low-level protocol handlers. */
	void	(*c_start)(struct ata_channel *, struct ata_xfer *);
	int	(*c_intr)(struct ata_channel *, struct ata_xfer *, int);
	void	(*c_kill_xfer)(struct ata_channel *, struct ata_xfer *, int);
};

/* flags in c_flags */
#define	C_ATAPI		0x0001		/* xfer is ATAPI request */
#define	C_TIMEOU	0x0002		/* xfer processing timed out */
#define	C_POLL		0x0004		/* command is polled */
#define	C_DMA		0x0008		/* command uses DMA */
#define C_WAIT		0x0010		/* can use tsleep */
#define C_WAITACT	0x0020		/* wakeup when active */
#define C_FREE		0x0040		/* call ata_free_xfer() asap */

/* reasons for c_kill_xfer() */
#define KILL_GONE 1 /* device is gone */
#define KILL_RESET 2 /* xfer was reset */

/* Per-channel queue of ata_xfers.  May be shared by multiple channels. */
struct ata_queue {
	TAILQ_HEAD(, ata_xfer) queue_xfer; /* queue of pending commands */
	int queue_freeze; /* freeze count for the queue */
	struct ata_xfer *active_xfer; /* active command */
};

/* ATA bus instance state information. */
struct atabus_softc {
	struct device sc_dev;
	struct ata_channel *sc_chan;
	int sc_flags;
#define ATABUSCF_OPEN	0x01
	void *sc_powerhook;
	int sc_sleeping;
};

/*
 * A queue of atabus instances, used to ensure the same bus probe order
 * for a given hardware configuration at each boot.
 */
struct atabus_initq {
	TAILQ_ENTRY(atabus_initq) atabus_initq;
	struct atabus_softc *atabus_sc;
};

#ifdef _KERNEL
TAILQ_HEAD(atabus_initq_head, atabus_initq);
extern struct atabus_initq_head atabus_initq_head;
extern struct simplelock atabus_interlock;
#endif /* _KERNEL */

/* High-level functions and structures used by both ATA and ATAPI devices */
struct ataparams;

/* Datas common to drives and controller drivers */
struct ata_drive_datas {
	u_int8_t drive;		/* drive number */
	int8_t ata_vers;	/* ATA version supported */
	u_int16_t drive_flags;	/* bitmask for drives present/absent and cap */

#define	DRIVE_ATA	0x0001
#define	DRIVE_ATAPI	0x0002
#define	DRIVE_OLD	0x0004 
#define	DRIVE		(DRIVE_ATA|DRIVE_ATAPI|DRIVE_OLD)
#define	DRIVE_CAP32	0x0008
#define	DRIVE_DMA	0x0010 
#define	DRIVE_UDMA	0x0020
#define	DRIVE_MODE	0x0040	/* the drive reported its mode */
#define	DRIVE_RESET	0x0080	/* reset the drive state at next xfer */
#define	DRIVE_WAITDRAIN	0x0100	/* device is waiting for the queue to drain */
#define	DRIVE_ATAPIST	0x0200	/* device is an ATAPI tape drive */
#define	DRIVE_NOSTREAM	0x0400	/* no stream methods on this drive */

	/*
	 * Current setting of drive's PIO, DMA and UDMA modes.
	 * Is initialised by the disks drivers at attach time, and may be
	 * changed later by the controller's code if needed
	 */
	u_int8_t PIO_mode;	/* Current setting of drive's PIO mode */
	u_int8_t DMA_mode;	/* Current setting of drive's DMA mode */
	u_int8_t UDMA_mode;	/* Current setting of drive's UDMA mode */

	/* Supported modes for this drive */
	u_int8_t PIO_cap;	/* supported drive's PIO mode */
	u_int8_t DMA_cap;	/* supported drive's DMA mode */
	u_int8_t UDMA_cap;	/* supported drive's UDMA mode */

	/*
	 * Drive state.
	 * This is reset to 0 after a channel reset.
	 */
	u_int8_t state;

#define RESET          0
#define READY          1

	/* numbers of xfers and DMA errs. Used by ata_dmaerr() */
	u_int8_t n_dmaerrs;
	u_int32_t n_xfers;

	/* Downgrade after NERRS_MAX errors in at most NXFER xfers */
#define NERRS_MAX 4
#define NXFER 4000

	/* Callbacks into the drive's driver. */
	void	(*drv_done)(void *);	/* transfer is done */

	struct device *drv_softc;	/* ATA drives softc, if any */
	void *chnl_softc;		/* channel softc */
};

/* User config flags that force (or disable) the use of a mode */
#define ATA_CONFIG_PIO_MODES	0x0007
#define ATA_CONFIG_PIO_SET	0x0008
#define ATA_CONFIG_PIO_OFF	0
#define ATA_CONFIG_DMA_MODES	0x0070
#define ATA_CONFIG_DMA_SET	0x0080
#define ATA_CONFIG_DMA_DISABLE	0x0070
#define ATA_CONFIG_DMA_OFF	4
#define ATA_CONFIG_UDMA_MODES	0x0700
#define ATA_CONFIG_UDMA_SET	0x0800
#define ATA_CONFIG_UDMA_DISABLE	0x0700
#define ATA_CONFIG_UDMA_OFF	8

/*
 * Parameters/state needed by the controller to perform an ATA bio.
 */
struct ata_bio {
	volatile u_int16_t flags;/* cmd flags */
#define	ATA_NOSLEEP	0x0001	/* Can't sleep */   
#define	ATA_POLL	0x0002	/* poll for completion */
#define	ATA_ITSDONE	0x0004	/* the transfer is as done as it gets */
#define	ATA_SINGLE	0x0008	/* transfer must be done in singlesector mode */
#define	ATA_LBA		0x0010	/* transfer uses LBA addressing */
#define	ATA_READ	0x0020	/* transfer is a read (otherwise a write) */
#define	ATA_CORR	0x0040	/* transfer had a corrected error */
#define	ATA_LBA48	0x0080	/* transfer uses 48-bit LBA addressing */
	int		multi;	/* # of blocks to transfer in multi-mode */
	struct disklabel *lp;	/* pointer to drive's label info */
	daddr_t		blkno;	/* block addr */
	daddr_t		blkdone;/* number of blks transferred */
	daddr_t		nblks;	/* number of block currently transferring */
	int		nbytes;	/* number of bytes currently transferring */
	long		bcount;	/* total number of bytes */
	char		*databuf;/* data buffer address */
	volatile int	error;
#define	NOERROR 	0	/* There was no error (r_error invalid) */
#define	ERROR		1	/* check r_error */
#define	ERR_DF		2	/* Drive fault */
#define	ERR_DMA		3	/* DMA error */
#define	TIMEOUT		4	/* device timed out */
#define	ERR_NODEV	5	/* device has been gone */
#define ERR_RESET	6	/* command was terminated by channel reset */
	u_int8_t	r_error;/* copy of error register */
	daddr_t		badsect[127];/* 126 plus trailing -1 marker */
};

/*
 * ATA/ATAPI commands description 
 *
 * This structure defines the interface between the ATA/ATAPI device driver
 * and the controller for short commands. It contains the command's parameter,
 * the len of data's to read/write (if any), and a function to call upon
 * completion.
 * If no sleep is allowed, the driver can poll for command completion.
 * Once the command completed, if the error registed is valid, the flag
 * AT_ERROR is set and the error register value is copied to r_error .
 * A separate interface is needed for read/write or ATAPI packet commands
 * (which need multiple interrupts per commands).
 */
struct ata_command {
	u_int8_t r_command;	/* Parameters to upload to registers */
	u_int8_t r_head;
	u_int16_t r_cyl;
	u_int8_t r_sector;
	u_int8_t r_count;
	u_int8_t r_features;
	u_int8_t r_st_bmask;	/* status register mask to wait for before
				   command */
	u_int8_t r_st_pmask;	/* status register mask to wait for after
				   command */
	u_int8_t r_error;	/* error register after command done */
	volatile u_int16_t flags;

#define AT_READ     0x0001 /* There is data to read */
#define AT_WRITE    0x0002 /* There is data to write (excl. with AT_READ) */
#define AT_WAIT     0x0008 /* wait in controller code for command completion */
#define AT_POLL     0x0010 /* poll for command completion (no interrupts) */
#define AT_DONE     0x0020 /* command is done */
#define AT_XFDONE   0x0040 /* data xfer is done */
#define AT_ERROR    0x0080 /* command is done with error */
#define AT_TIMEOU   0x0100 /* command timed out */
#define AT_DF       0x0200 /* Drive fault */
#define AT_RESET    0x0400 /* command terminated by channel reset */
#define AT_GONE     0x0800 /* command terminated because device is gone */
#define AT_READREG  0x1000 /* Read registers on completion */

	int timeout;		/* timeout (in ms) */
	void *data;		/* Data buffer address */
	int bcount;		/* number of bytes to transfer */
	void (*callback)(void *); /* command to call once command completed */
	void *callback_arg;	/* argument passed to *callback() */
};

/*
 * ata_bustype.  The first field must be compatible with scsipi_bustype,
 * as it's used for autoconfig by both ata and atapi drivers.
 */
struct ata_bustype {
	int	bustype_type;	/* symbolic name of type */
	int	(*ata_bio)(struct ata_drive_datas *, struct ata_bio *);
	void	(*ata_reset_drive)(struct ata_drive_datas *, int);
	void	(*ata_reset_channel)(struct ata_channel *, int);
/* extra flags for ata_reset_*(), in addition to AT_* */
#define AT_RST_EMERG 0x10000 /* emergency - e.g. for a dump */
#define	AT_RST_NOCMD 0x20000 /* XXX has to go - temporary until we have tagged queuing */

	int	(*ata_exec_command)(struct ata_drive_datas *,
				    struct ata_command *);

#define	ATACMD_COMPLETE		0x01
#define	ATACMD_QUEUED		0x02
#define	ATACMD_TRY_AGAIN	0x03

	int	(*ata_get_params)(struct ata_drive_datas *, u_int8_t,
				  struct ataparams *);
	int	(*ata_addref)(struct ata_drive_datas *);
	void	(*ata_delref)(struct ata_drive_datas *);
	void	(*ata_killpending)(struct ata_drive_datas *);
};

/* bustype_type */	/* XXX XXX XXX */
/* #define SCSIPI_BUSTYPE_SCSI	0 */
/* #define SCSIPI_BUSTYPE_ATAPI	1 */
#define	SCSIPI_BUSTYPE_ATA	2

/*
 * Describe an ATA device.  Has to be compatible with scsipi_channel, so
 * start with a pointer to ata_bustype.
 */
struct ata_device {
	const struct ata_bustype *adev_bustype;
	int adev_channel;
	int adev_openings;
	struct ata_drive_datas *adev_drv_data;
};

/*
 * Per-channel data
 */
struct ata_channel {
	struct callout ch_callout;	/* callout handle */
	int ch_channel;			/* location */
	struct atac_softc *ch_atac;	/* ATA controller softc */

	/* Our state */
	volatile int ch_flags;
#define ATACH_SHUTDOWN 0x02	/* channel is shutting down */
#define ATACH_IRQ_WAIT 0x10	/* controller is waiting for irq */
#define ATACH_DMA_WAIT 0x20	/* controller is waiting for DMA */
#define	ATACH_DISABLED 0x80	/* channel is disabled */
#define ATACH_TH_RUN   0x100	/* the kenrel thread is working */
#define ATACH_TH_RESET 0x200	/* someone ask the thread to reset */
	u_int8_t ch_status;	/* copy of status register */
	u_int8_t ch_error;	/* copy of error register */

	/* for the reset callback */
	int ch_reset_flags;

	/* per-drive info */
	int ch_ndrive;
	struct ata_drive_datas ch_drive[ATA_MAXDRIVES];

	struct device *atabus;	/* self */

	/* ATAPI children */
	struct device *atapibus;
	struct scsipi_channel ch_atapi_channel;

	/* ATA children */
	struct device *ata_drives[ATA_MAXDRIVES];

	/*
	 * Channel queues.  May be the same for all channels, if hw
	 * channels are not independent.
	 */
	struct ata_queue *ch_queue;

	/* The channel kernel thread */
	struct proc *ch_thread;
};

/*
 * ATA controller softc.
 *
 * This contains a bunch of generic info that all ATA controllers need
 * to have.
 *
 * XXX There is still some lingering wdc-centricity here.
 */
struct atac_softc {
	struct device atac_dev;		/* generic device info */

	int	atac_cap;		/* controller capabilities */

#define	ATAC_CAP_DATA16	0x0001		/* can do 16-bit data access */
#define	ATAC_CAP_DATA32	0x0002		/* can do 32-bit data access */
#define	ATAC_CAP_DMA	0x0008		/* can do ATA DMA modes */
#define	ATAC_CAP_UDMA	0x0010		/* can do ATA Ultra DMA modes */
#define	ATAC_CAP_ATA_NOSTREAM 0x0040	/* don't use stream funcs on ATA */
#define	ATAC_CAP_ATAPI_NOSTREAM 0x0080	/* don't use stream funcs on ATAPI */
#define	ATAC_CAP_NOIRQ	0x1000		/* controller never interrupts */
#define	ATAC_CAP_RAID	0x4000		/* controller "supports" RAID */

	uint8_t	atac_pio_cap;		/* highest PIO mode supported */
	uint8_t	atac_dma_cap;		/* highest DMA mode supported */
	uint8_t	atac_udma_cap;		/* highest UDMA mode supported */

	/* Array of pointers to channel-specific data. */
	struct ata_channel **atac_channels;
	int		     atac_nchannels;

	const struct ata_bustype *atac_bustype_ata;

	/*
	 * Glue between ATA and SCSIPI for the benefit of ATAPI.
	 *
	 * Note: The reference count here is used for both ATA and ATAPI
	 * devices.
	 */
	struct atapi_adapter atac_atapi_adapter;
	void (*atac_atapibus_attach)(struct atabus_softc *);

	/* Driver callback to probe for drives. */
	void (*atac_probe)(struct ata_channel *);

	/* Optional callbacks to lock/unlock hardware. */
	int  (*atac_claim_hw)(struct ata_channel *, int);
	void (*atac_free_hw)(struct ata_channel *);

	/*
	 * Optional callbacks to set drive mode.  Required for anything
	 * but basic PIO operation.
	 */
	void (*atac_set_modes)(struct ata_channel *);
};

#ifdef _KERNEL
void	ata_channel_attach(struct ata_channel *);
int	atabusprint(void *aux, const char *);
int	ataprint(void *aux, const char *);

struct ataparams;
int	ata_get_params(struct ata_drive_datas *, u_int8_t, struct ataparams *);
int	ata_set_mode(struct ata_drive_datas *, u_int8_t, u_int8_t);
/* return code for these cmds */
#define CMD_OK    0
#define CMD_ERR   1
#define CMD_AGAIN 2

struct ata_xfer *ata_get_xfer(int);
void	ata_free_xfer(struct ata_channel *, struct ata_xfer *);
#define	ATAXF_CANSLEEP	0x00
#define	ATAXF_NOSLEEP	0x01

void	ata_exec_xfer(struct ata_channel *, struct ata_xfer *);
void	ata_kill_pending(struct ata_drive_datas *);
void	ata_reset_channel(struct ata_channel *, int);

int	ata_addref(struct ata_channel *);
void	ata_delref(struct ata_channel *);
void	atastart(struct ata_channel *);
void	ata_print_modes(struct ata_channel *);
int	ata_downgrade_mode(struct ata_drive_datas *, int);
void	ata_probe_caps(struct ata_drive_datas *);

void	ata_dmaerr(struct ata_drive_datas *, int);
#endif /* _KERNEL */

#endif /* _DEV_ATA_ATAVAR_H_ */
