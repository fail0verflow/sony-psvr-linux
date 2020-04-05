/*
 * Designware SPI core controller driver (refer pxa2xx_spi.c)
 *
 * Copyright (c) 2009, Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <linux/dma-mapping.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/highmem.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/spi/spi.h>

#include "spi-dw.h"
#include "api_dhub.h"
#include "dHub.h"
#include "pBridge.h"
#include "spi-dw-dhub.h"

#ifdef CONFIG_DEBUG_FS
#include <linux/debugfs.h>
#endif

#define START_STATE	((void *)0)
#define RUNNING_STATE	((void *)1)
#define DONE_STATE	((void *)2)
#define ERROR_STATE	((void *)-1)

#define QUEUE_RUNNING	0
#define QUEUE_STOPPED	1

#define MRST_SPI_DEASSERT	0
#define MRST_SPI_ASSERT		1


#define MEMMAP_PB_REG_BASE      0xF7D70000
#define MEMMAP_APBPERIF_REG_BASE    0xF7E80000
#define APB_SSI_INST1_BASE      (MEMMAP_APBPERIF_REG_BASE + 0x1C00) //0xF7FC6000//(MEMMAP_APBPERIF_REG_BASE + 0x1C00)


#define DMA_ALLOC_BUF_SIZE	8192
#define SPI_SYS_FIFO_SIZE	64

/* macros for pbridge registers read/write */
#define pb_writel(mi, off, val)	\
	__raw_writel((val), MEMMAP_PB_REG_BASE + (off))
#define pb_readl(mi, off)		\
	__raw_readl(MEMMAP_PB_REG_BASE + (off))

#define pb_get_phys_offset(mi, off)	\
	((APB_SSI_INST1_BASE + off) & 0x0FFFFFFF)

#define READ_DATA_CHAN_CMD_BASE		(0)
#define READ_DATA_CHAN_CMD_SIZE		(2 << 3)
#define READ_DATA_CHAN_DATA_BASE	(READ_DATA_CHAN_CMD_BASE + READ_DATA_CHAN_CMD_SIZE)
#define READ_DATA_CHAN_DATA_SIZE	(32 << 3)

#define WRITE_DATA_CHAN_CMD_BASE	(READ_DATA_CHAN_DATA_BASE + READ_DATA_CHAN_DATA_SIZE)
#define WRITE_DATA_CHAN_CMD_SIZE	(2 << 3)
#define WRITE_DATA_CHAN_DATA_BASE	(WRITE_DATA_CHAN_CMD_BASE + WRITE_DATA_CHAN_CMD_SIZE)
#define WRITE_DATA_CHAN_DATA_SIZE	(32 << 3)

#define DESCRIPTOR_CHAN_CMD_BASE	(WRITE_DATA_CHAN_DATA_BASE + WRITE_DATA_CHAN_DATA_SIZE)
#define DESCRIPTOR_CHAN_CMD_SIZE	(2 << 3)
#define DESCRIPTOR_CHAN_DATA_BASE	(DESCRIPTOR_CHAN_CMD_BASE + DESCRIPTOR_CHAN_CMD_SIZE)
#define DESCRIPTOR_CHAN_DATA_SIZE	(32 << 3)

#define NFC_DEV_CTL_CHAN_CMD_BASE	(DESCRIPTOR_CHAN_DATA_BASE + DESCRIPTOR_CHAN_DATA_SIZE)
#define NFC_DEV_CTL_CHAN_CMD_SIZE	(2 << 3)
#define NFC_DEV_CTL_CHAN_DATA_BASE	(NFC_DEV_CTL_CHAN_CMD_BASE + NFC_DEV_CTL_CHAN_CMD_SIZE)
#define NFC_DEV_CTL_CHAN_DATA_SIZE	(32 << 3)

#define SPI_DEV_CTL_CHAN_CMD_BASE	(NFC_DEV_CTL_CHAN_DATA_BASE + NFC_DEV_CTL_CHAN_DATA_SIZE)
#define SPI_DEV_CTL_CHAN_CMD_SIZE	(2 << 3)
#define SPI_DEV_CTL_CHAN_DATA_BASE	(SPI_DEV_CTL_CHAN_CMD_BASE + SPI_DEV_CTL_CHAN_CMD_SIZE)
#define SPI_DEV_CTL_CHAN_DATA_SIZE	(32 << 3)

#define IS_DMA_ALIGNED(x)	((((u32)(x)) & 0x07) == 0)

enum {
        READ_DATA_CHANNEL_ID = PBSemaMap_dHubSemID_dHubCh0_intr,
        WRITE_DATA_CHANNEL_ID = PBSemaMap_dHubSemID_dHubCh1_intr,
        DESCRIPTOR_CHANNEL_ID = PBSemaMap_dHubSemID_dHubCh2_intr,
        NFC_DEV_CTL_CHANNEL_ID = PBSemaMap_dHubSemID_dHubCh3_intr,
        SPI_DEV_CTL_CHANNEL_ID = PBSemaMap_dHubSemID_dHubCh4_intr
};

enum {
        MV_MTU_8_BYTE = 0,
        MV_MTU_32_BYTE,
        MV_MTU_128_BYTE,
        MV_MTU_1024_BYTE
};




/* Slave spi_dev related */
struct chip_data {
	u16 cr0;
	u8 cs;			/* chip select pin */
	u8 n_bytes;		/* current is a 1/2/4 byte op */
	u8 tmode;		/* TR/TO/RO/EEPROM */
	u8 type;		/* SPI/SSP/MicroWire */

	u8 poll_mode;		/* 1 means use poll mode */

	u32 dma_width;
	u32 rx_threshold;
	u32 tx_threshold;
	u8 enable_dma;
	u8 bits_per_word;
	u16 clk_div;		/* baud rate divider */
	u32 speed_hz;		/* baud rate */
	void (*cs_control)(u32 command);
};

#ifdef CONFIG_DEBUG_FS
#define SPI_REGS_BUFSIZE	1024
static ssize_t  dw_spi_show_regs(struct file *file, char __user *user_buf,
				size_t count, loff_t *ppos)
{
	struct dw_spi *dws;
	char *buf;
	u32 len = 0;
	ssize_t ret;

	dws = file->private_data;

	buf = kzalloc(SPI_REGS_BUFSIZE, GFP_KERNEL);
	if (!buf)
		return 0;

	len += snprintf(buf + len, SPI_REGS_BUFSIZE - len,
			"MRST SPI0 registers:\n");
	len += snprintf(buf + len, SPI_REGS_BUFSIZE - len,
			"=================================\n");
	len += snprintf(buf + len, SPI_REGS_BUFSIZE - len,
			"CTRL0: \t\t0x%08x\n", dw_readl(dws, DW_SPI_CTRL0));
	len += snprintf(buf + len, SPI_REGS_BUFSIZE - len,
			"CTRL1: \t\t0x%08x\n", dw_readl(dws, DW_SPI_CTRL1));
	len += snprintf(buf + len, SPI_REGS_BUFSIZE - len,
			"SSIENR: \t0x%08x\n", dw_readl(dws, DW_SPI_SSIENR));
	len += snprintf(buf + len, SPI_REGS_BUFSIZE - len,
			"SER: \t\t0x%08x\n", dw_readl(dws, DW_SPI_SER));
	len += snprintf(buf + len, SPI_REGS_BUFSIZE - len,
			"BAUDR: \t\t0x%08x\n", dw_readl(dws, DW_SPI_BAUDR));
	len += snprintf(buf + len, SPI_REGS_BUFSIZE - len,
			"TXFTLR: \t0x%08x\n", dw_readl(dws, DW_SPI_TXFLTR));
	len += snprintf(buf + len, SPI_REGS_BUFSIZE - len,
			"RXFTLR: \t0x%08x\n", dw_readl(dws, DW_SPI_RXFLTR));
	len += snprintf(buf + len, SPI_REGS_BUFSIZE - len,
			"TXFLR: \t\t0x%08x\n", dw_readl(dws, DW_SPI_TXFLR));
	len += snprintf(buf + len, SPI_REGS_BUFSIZE - len,
			"RXFLR: \t\t0x%08x\n", dw_readl(dws, DW_SPI_RXFLR));
	len += snprintf(buf + len, SPI_REGS_BUFSIZE - len,
			"SR: \t\t0x%08x\n", dw_readl(dws, DW_SPI_SR));
	len += snprintf(buf + len, SPI_REGS_BUFSIZE - len,
			"IMR: \t\t0x%08x\n", dw_readl(dws, DW_SPI_IMR));
	len += snprintf(buf + len, SPI_REGS_BUFSIZE - len,
			"ISR: \t\t0x%08x\n", dw_readl(dws, DW_SPI_ISR));
	len += snprintf(buf + len, SPI_REGS_BUFSIZE - len,
			"DMACR: \t\t0x%08x\n", dw_readl(dws, DW_SPI_DMACR));
	len += snprintf(buf + len, SPI_REGS_BUFSIZE - len,
			"DMATDLR: \t0x%08x\n", dw_readl(dws, DW_SPI_DMATDLR));
	len += snprintf(buf + len, SPI_REGS_BUFSIZE - len,
			"DMARDLR: \t0x%08x\n", dw_readl(dws, DW_SPI_DMARDLR));
	len += snprintf(buf + len, SPI_REGS_BUFSIZE - len,
			"=================================\n");

	ret =  simple_read_from_buffer(user_buf, count, ppos, buf, len);
	kfree(buf);
	return ret;
}

static const struct file_operations mrst_spi_regs_ops = {
	.owner		= THIS_MODULE,
	.open		= simple_open,
	.read		= dw_spi_show_regs,
	.llseek		= default_llseek,
};

static int mrst_spi_debugfs_init(struct dw_spi *dws)
{
	dws->debugfs = debugfs_create_dir("mrst_spi", NULL);
	if (!dws->debugfs)
		return -ENOMEM;

	debugfs_create_file("registers", S_IFREG | S_IRUGO,
		dws->debugfs, (void *)dws, &mrst_spi_regs_ops);
	return 0;
}

static void mrst_spi_debugfs_remove(struct dw_spi *dws)
{
	if (dws->debugfs)
		debugfs_remove_recursive(dws->debugfs);
}

#else
static inline int mrst_spi_debugfs_init(struct dw_spi *dws)
{
	return 0;
}

static inline void mrst_spi_debugfs_remove(struct dw_spi *dws)
{
}
#endif /* CONFIG_DEBUG_FS */




/* Return the max entries we can fill into tx fifo */
static inline u32 tx_max(struct dw_spi *dws)
{
	u32 tx_left, tx_room, rxtx_gap;

	tx_left = (dws->tx_end - dws->tx) / dws->n_bytes;
	tx_room = dws->fifo_len - dw_readw(dws, DW_SPI_TXFLR);

	/*
	 * Another concern is about the tx/rx mismatch, we
	 * though to use (dws->fifo_len - rxflr - txflr) as
	 * one maximum value for tx, but it doesn't cover the
	 * data which is out of tx/rx fifo and inside the
	 * shift registers. So a control from sw point of
	 * view is taken.
	 */
	rxtx_gap =  ((dws->rx_end - dws->rx) - (dws->tx_end - dws->tx))
			/ dws->n_bytes;

	return min3(tx_left, tx_room, (u32) (dws->fifo_len - rxtx_gap));
}

/* Return the max entries we should read out of rx fifo */
static inline u32 rx_max(struct dw_spi *dws)
{
	u32 rx_left = (dws->rx_end - dws->rx) / dws->n_bytes;

	return min(rx_left, (u32)dw_readw(dws, DW_SPI_RXFLR));
}

/*
 * Note: first step is the protocol driver prepares
 * a dma-capable memory, and this func just need translate
 * the virt addr to physical
 */
static int map_dma_buffers(struct dw_spi *dws)
{

	struct spi_message *msg = dws->cur_msg;
	struct device *dev = &msg->spi->dev;

	if (dws->cur_msg->is_dma_mapped
		|| !dws->dma_inited
		|| !dws->cur_chip->enable_dma
		|| !dws->dma_ops)
		return 0;

	if (!IS_DMA_ALIGNED(dws->rx) || !IS_DMA_ALIGNED(dws->tx))
		return 0;


	/* Stream map the tx buffer. Always do DMA_TO_DEVICE first
	 * so we flush the cache *before* invalidating it, in case
	 * the tx and rx buffers overlap.
	 */
	if (dws->tx) {
		dws->tx_dma = dma_map_single(dev, dws->tx,
						dws->len, DMA_TO_DEVICE);
		if (dma_mapping_error(dev, dws->tx_dma))
			return 0;
	}
	if (dws->rx) {
		/* Stream map the rx buffer */
		dws->rx_dma = dma_map_single(dev, dws->rx,
					dws->len, DMA_FROM_DEVICE);
		if (dma_mapping_error(dev, dws->rx_dma)) {
			dma_unmap_single(dev, dws->tx_dma,
						dws->len, DMA_TO_DEVICE);
			return 0;
		}
	}
	dws->cur_msg->is_dma_mapped = 1;
	return 1;
}

/* Caller already set message->status; dma and pio irqs are blocked */
static void giveback(struct dw_spi *dws)
{
	struct spi_transfer *last_transfer;
	unsigned long flags;
	struct spi_message *msg;

	spin_lock_irqsave(&dws->lock, flags);
	msg = dws->cur_msg;
	dws->cur_msg = NULL;
	dws->cur_transfer = NULL;
	dws->prev_chip = dws->cur_chip;
	dws->cur_chip = NULL;
	dws->dma_mapped = 0;
	queue_work(dws->workqueue, &dws->pump_messages);
	spin_unlock_irqrestore(&dws->lock, flags);

	last_transfer = list_entry(msg->transfers.prev,
					struct spi_transfer,
					transfer_list);

	if (!last_transfer->cs_change && dws->cs_control)
		dws->cs_control(MRST_SPI_DEASSERT);

	msg->state = NULL;
	if (msg->complete)
		msg->complete(msg->context);
}


static irqreturn_t dw_dhub_spi_irq(int irq, void *dev_id)
{
	struct dw_spi *mi = dev_id;
	unsigned int status;

	status = pb_readl(mi, RA_pBridge_dHub + RA_dHubReg2D_dHub +
			RA_dHubReg_SemaHub + RA_SemaHub_full);
	if (status & (1 << PBSemaMap_dHubSemID_APB0_DATA_CP)) {
		pb_writel(mi, RA_pBridge_dHub + RA_dHubReg2D_dHub +
				RA_dHubReg_SemaHub + RA_SemaHub_POP,
				0x100 | PBSemaMap_dHubSemID_APB0_DATA_CP);
		pb_writel(mi, RA_pBridge_dHub + RA_dHubReg2D_dHub +
				RA_dHubReg_SemaHub + RA_SemaHub_full,
				1 << PBSemaMap_dHubSemID_APB0_DATA_CP);
		dw_spi_xfer_done(mi);
		return IRQ_HANDLED;
	}

	return IRQ_NONE;
}


static void pump_transfers(unsigned long data)
{
	struct dw_spi *dws = (struct dw_spi *)data;
	struct spi_message *message = NULL;
	struct spi_transfer *transfer = NULL;
	struct spi_transfer *previous = NULL;
	struct spi_device *spi = NULL;
	struct chip_data *chip = NULL;
	u8 bits = 0;
	u8 cs_change = 0;
	u16 clk_div = 0;
	u32 speed = 0;
	u32 cr0 = 0;
	/* Get current state information */
	message = dws->cur_msg;
	transfer = dws->cur_transfer;
	chip = dws->cur_chip;
	spi = message->spi;

	if (unlikely(!chip->clk_div))
		chip->clk_div = dws->max_freq / chip->speed_hz;

	if (message->state == ERROR_STATE) {
		message->status = -EIO;
		goto early_exit;
	}

	/* Handle end of message */
	if (message->state == DONE_STATE) {
		message->status = 0;
		goto early_exit;
	}

	/* Delay if requested at end of transfer*/
	if (message->state == RUNNING_STATE) {
		previous = list_entry(transfer->transfer_list.prev,
					struct spi_transfer,
					transfer_list);
		if (previous->delay_usecs)
			udelay(previous->delay_usecs);
	}

	dws->n_bytes = chip->n_bytes;
	dws->dma_width = chip->dma_width;
	dws->cs_control = chip->cs_control;

	dws->rx_dma = transfer->rx_dma;
	dws->tx_dma = transfer->tx_dma;
	dws->tx = (void *)transfer->tx_buf;
	dws->tx_end = dws->tx + transfer->len;
	dws->rx = transfer->rx_buf;
	dws->rx_end = dws->rx + transfer->len;
	dws->cs_change = transfer->cs_change;
	dws->len = dws->cur_transfer->len;
	if (chip != dws->prev_chip)
		cs_change = 1;

	cr0 = chip->cr0;

	/* Handle per transfer options for bpw and speed */
	if (transfer->speed_hz) {
		speed = transfer->speed_hz;
		if (speed > dws->max_freq) {
			printk(KERN_ERR "MRST SPI0: unsupported"
				"freq: %dHz\n", speed);
			message->status = -EIO;
			goto early_exit;
		}

		/* clk_div doesn't support odd number */
		clk_div = dws->max_freq / speed;
		clk_div = (clk_div + 1) & 0xfffe;

		chip->speed_hz = speed;
		chip->clk_div = clk_div;
	}
	if (transfer->bits_per_word) {
		bits = transfer->bits_per_word;

		switch (bits) {
		case 8:
		case 16:
			dws->n_bytes = dws->dma_width = bits >> 3;
			break;
		default:
			printk(KERN_ERR "MRST SPI0: unsupported bits:"
				"%db\n", bits);
			message->status = -EIO;
			goto early_exit;
		}

		cr0 = (bits - 1)
			| (chip->type << SPI_FRF_OFFSET)
			| (spi->mode << SPI_MODE_OFFSET)
			| (chip->tmode << SPI_TMOD_OFFSET);
	}
	message->state = RUNNING_STATE;

	/*
	 * Adjust transfer mode if necessary. Requires platform dependent
	 * chipselect mechanism.
	 */
	if (dws->cs_control) {
		if (dws->rx && dws->tx)
			chip->tmode = SPI_TMOD_TR;
		else if (dws->rx)
			chip->tmode = SPI_TMOD_RO;
		else
			chip->tmode = SPI_TMOD_TO;

		cr0 &= ~SPI_TMOD_MASK;
		cr0 |= (chip->tmode << SPI_TMOD_OFFSET);
	}

	/* Check if current transfer is a DMA transaction */
	dws->dma_mapped = map_dma_buffers(dws);

	/* we just consider DHUB DMS mode */

	if (dws->dma_mapped)
		dws->dma_ops->dma_transfer(dws, cs_change);


	return;

early_exit:
	giveback(dws);
	return;
}

static void pump_messages(struct work_struct *work)
{
	struct dw_spi *dws =
		container_of(work, struct dw_spi, pump_messages);
	unsigned long flags;

	/* Lock queue and check for queue work */
	spin_lock_irqsave(&dws->lock, flags);
	if (list_empty(&dws->queue) || dws->run == QUEUE_STOPPED) {
		dws->busy = 0;
		spin_unlock_irqrestore(&dws->lock, flags);
		return;
	}

	/* Make sure we are not already running a message */
	if (dws->cur_msg) {
		spin_unlock_irqrestore(&dws->lock, flags);
		return;
	}

	/* Extract head of queue */
	dws->cur_msg = list_entry(dws->queue.next, struct spi_message, queue);
	list_del_init(&dws->cur_msg->queue);

	/* Initial message state*/
	dws->cur_msg->state = START_STATE;
	dws->cur_transfer = list_entry(dws->cur_msg->transfers.next,
						struct spi_transfer,
						transfer_list);
	dws->cur_chip = spi_get_ctldata(dws->cur_msg->spi);

	/* Mark as busy and launch transfers */
	tasklet_schedule(&dws->pump_transfers);

	dws->busy = 1;
	spin_unlock_irqrestore(&dws->lock, flags);
}

/* spi_device use this to queue in their spi_msg */
static int dw_spi_transfer(struct spi_device *spi, struct spi_message *msg)
{
	struct dw_spi *dws = spi_master_get_devdata(spi->master);
	unsigned long flags;

	spin_lock_irqsave(&dws->lock, flags);

	if (dws->run == QUEUE_STOPPED) {
		spin_unlock_irqrestore(&dws->lock, flags);
		return -ESHUTDOWN;
	}

	msg->actual_length = 0;
	msg->status = -EINPROGRESS;
	msg->state = START_STATE;

	list_add_tail(&msg->queue, &dws->queue);

	if (dws->run == QUEUE_RUNNING && !dws->busy) {

		if (dws->cur_transfer || dws->cur_msg)
			queue_work(dws->workqueue,
					&dws->pump_messages);
		else {
			/* If no other data transaction in air, just go */
			spin_unlock_irqrestore(&dws->lock, flags);
			pump_messages(&dws->pump_messages);
			return 0;
		}
	}

	spin_unlock_irqrestore(&dws->lock, flags);
	return 0;
}

/* This may be called twice for each spi dev */
static int dw_dhub_spi_setup(struct spi_device *spi)
{
	struct dw_spi_chip *chip_info = NULL;
	struct chip_data *chip;

	if (spi->bits_per_word != 8 && spi->bits_per_word != 16)
		return -EINVAL;

	/* Only alloc on first setup */
	chip = spi_get_ctldata(spi);
	if (!chip) {
		chip = kzalloc(sizeof(struct chip_data), GFP_KERNEL);
		if (!chip)
			return -ENOMEM;
	}

	/*
	 * Protocol drivers may change the chip settings, so...
	 * if chip_info exists, use it
	 */
	chip_info = spi->controller_data;

	/* chip_info doesn't always exist */
	if (chip_info) {
		if (chip_info->cs_control)
			chip->cs_control = chip_info->cs_control;

		chip->poll_mode = chip_info->poll_mode;
		chip->type = chip_info->type;

		chip->rx_threshold = 0;
		chip->tx_threshold = 0;

		chip->enable_dma = chip_info->enable_dma;
	}


	/* we force toset enable_dma here for dhub choice*/

	chip->enable_dma = 1;
	if (spi->bits_per_word <= 8) {
		chip->n_bytes = 1;
		chip->dma_width = 1;
	} else if (spi->bits_per_word <= 16) {
		chip->n_bytes = 2;
		chip->dma_width = 2;
	} else {
		/* Never take >16b case for MRST SPIC */
		dev_err(&spi->dev, "invalid wordsize\n");
		return -EINVAL;
	}
	chip->bits_per_word = spi->bits_per_word;

	if (!spi->max_speed_hz) {
		dev_err(&spi->dev, "No max speed HZ parameter\n");
		return -EINVAL;
	}
	chip->speed_hz = spi->max_speed_hz;

	chip->tmode = 0; /* Tx & Rx */
	/* Default SPI mode is SCPOL = 0, SCPH = 0 */
	chip->cr0 = (chip->bits_per_word - 1)
			| (chip->type << SPI_FRF_OFFSET)
			| (spi->mode  << SPI_MODE_OFFSET)
			| (chip->tmode << SPI_TMOD_OFFSET);

	spi_set_ctldata(spi, chip);
	return 0;
}

static void dw_spi_cleanup(struct spi_device *spi)
{
	struct chip_data *chip = spi_get_ctldata(spi);
	kfree(chip);
}

static int init_queue(struct dw_spi *dws)
{
	INIT_LIST_HEAD(&dws->queue);
	spin_lock_init(&dws->lock);

	dws->run = QUEUE_STOPPED;
	dws->busy = 0;

	tasklet_init(&dws->pump_transfers,
			pump_transfers,	(unsigned long)dws);

	INIT_WORK(&dws->pump_messages, pump_messages);
	dws->workqueue = create_singlethread_workqueue(
					dev_name(dws->master->dev.parent));
	if (dws->workqueue == NULL)
		return -EBUSY;

	return 0;
}

static int start_queue(struct dw_spi *dws)
{
	unsigned long flags;

	spin_lock_irqsave(&dws->lock, flags);

	if (dws->run == QUEUE_RUNNING || dws->busy) {
		spin_unlock_irqrestore(&dws->lock, flags);
		return -EBUSY;
	}

	dws->run = QUEUE_RUNNING;
	dws->cur_msg = NULL;
	dws->cur_transfer = NULL;
	dws->cur_chip = NULL;
	dws->prev_chip = NULL;
	spin_unlock_irqrestore(&dws->lock, flags);

	queue_work(dws->workqueue, &dws->pump_messages);

	return 0;
}

static int stop_queue(struct dw_spi *dws)
{
	unsigned long flags;
	unsigned limit = 50;
	int status = 0;

	spin_lock_irqsave(&dws->lock, flags);
	dws->run = QUEUE_STOPPED;
	while ((!list_empty(&dws->queue) || dws->busy) && limit--) {
		spin_unlock_irqrestore(&dws->lock, flags);
		msleep(10);
		spin_lock_irqsave(&dws->lock, flags);
	}

	if (!list_empty(&dws->queue) || dws->busy)
		status = -EBUSY;
	spin_unlock_irqrestore(&dws->lock, flags);

	return status;
}

static int destroy_queue(struct dw_spi *dws)
{
	int status;

	status = stop_queue(dws);
	if (status != 0)
		return status;
	destroy_workqueue(dws->workqueue);
	return 0;
}


struct mv_dhub_dma {
	struct mutex		lock;
	HDL_dhub		h_pb_dhub;
	dma_addr_t		buf_desc_dma;
	char			*buf_desc;
	int			desc_sz;
	int			fifo_sz;
};


static int dhub_dma_init(struct mv_dhub_dma *mi)
{
	HDL_semaphore *sema_handle;

	pb_writel(mi, RA_pBridge_BCM + RA_BCM_base,
			(APB_SSI_INST1_BASE & 0xF0000000));

	/* Initialize HDL_dhub with a $dHub BIU instance. */
	dhub_hdl(MEMMAP_PB_REG_BASE + RA_pBridge_tcm,		/* Base address of DTCM */
			MEMMAP_PB_REG_BASE + RA_pBridge_dHub,	/* Base address of a BIU instance of $dHub */
			&mi->h_pb_dhub);			/* Handle to HDL_dhub */

	/* Read Data Channel */
	dhub_channel_cfg(&mi->h_pb_dhub,		/* Handle to HDL_dhub */
			READ_DATA_CHANNEL_ID,		/* Channel ID in $dHubReg */
			READ_DATA_CHAN_CMD_BASE,	/* Channel FIFO base address (byte address) for cmdQ */
			READ_DATA_CHAN_DATA_BASE,	/* Channel FIFO base address (byte address) for dataQ */
			READ_DATA_CHAN_CMD_SIZE/8,	/* Channel FIFO depth for cmdQ, in 64b word */
			READ_DATA_CHAN_DATA_SIZE/8,	/* Channel FIFO depth for dataQ, in 64b word */
			MV_MTU_32_BYTE,			/* See 'dHubChannel.CFG.MTU', 0/1/2 for 8/32/128 bytes */
			0,				/* See 'dHubChannel.CFG.QoS' */
			0,				/* See 'dHubChannel.CFG.selfLoop' */
			1,				/* 0 to disable, 1 to enable */
			0);				/* Pass NULL to directly init dHub */

	/* Write Data Channel */
	dhub_channel_cfg(&mi->h_pb_dhub,
			WRITE_DATA_CHANNEL_ID,
			WRITE_DATA_CHAN_CMD_BASE,
			WRITE_DATA_CHAN_DATA_BASE,
			WRITE_DATA_CHAN_CMD_SIZE/8,
			WRITE_DATA_CHAN_DATA_SIZE/8,
			MV_MTU_32_BYTE,
			0, 0, 1, 0);

	/* Descriptor Channel */
	dhub_channel_cfg(&mi->h_pb_dhub,
			DESCRIPTOR_CHANNEL_ID,
			DESCRIPTOR_CHAN_CMD_BASE,
			DESCRIPTOR_CHAN_DATA_BASE,
			DESCRIPTOR_CHAN_CMD_SIZE/8,
			DESCRIPTOR_CHAN_DATA_SIZE/8,
			MV_MTU_32_BYTE,
			0, 0, 1, 0);

	/* NFC Device Control (function) Channel */
	dhub_channel_cfg(&mi->h_pb_dhub,
			NFC_DEV_CTL_CHANNEL_ID,
			NFC_DEV_CTL_CHAN_CMD_BASE,
			NFC_DEV_CTL_CHAN_DATA_BASE,
			NFC_DEV_CTL_CHAN_CMD_SIZE/8,
			NFC_DEV_CTL_CHAN_DATA_SIZE/8,
			MV_MTU_32_BYTE,
			0, 0, 1, 0);

	/* SPI Device Control (function) Channel */
	dhub_channel_cfg(&mi->h_pb_dhub,
			SPI_DEV_CTL_CHANNEL_ID,
			SPI_DEV_CTL_CHAN_CMD_BASE,
			SPI_DEV_CTL_CHAN_DATA_BASE,
			SPI_DEV_CTL_CHAN_CMD_SIZE/8,
			SPI_DEV_CTL_CHAN_DATA_SIZE/8,
			MV_MTU_32_BYTE,
			0, 0, 1, 0);

	sema_handle = dhub_semaphore(&mi->h_pb_dhub);

	/*configure the semaphore depth to be 1 */
	semaphore_cfg(sema_handle, READ_DATA_CHANNEL_ID, 1, 0);
	semaphore_cfg(sema_handle, WRITE_DATA_CHANNEL_ID, 1, 0);
	semaphore_cfg(sema_handle, DESCRIPTOR_CHANNEL_ID, 1, 0);
	semaphore_cfg(sema_handle, NFC_DEV_CTL_CHANNEL_ID, 1, 0);
	semaphore_cfg(sema_handle, SPI_DEV_CTL_CHANNEL_ID, 1, 0);

	semaphore_cfg(sema_handle, PBSemaMap_dHubSemID_dHub_NFCCmd, 1, 0);
	semaphore_cfg(sema_handle, PBSemaMap_dHubSemID_dHub_NFCDat, 1, 0);
	semaphore_cfg(sema_handle, PBSemaMap_dHubSemID_NFC_DATA_CP, 1, 0);
	semaphore_cfg(sema_handle, PBSemaMap_dHubSemID_dHub_APBRx2, 1, 0);
	semaphore_cfg(sema_handle, PBSemaMap_dHubSemID_APB0_DATA_CP, 1, 0);

	semaphore_intr_enable(sema_handle, PBSemaMap_dHubSemID_NFC_DATA_CP, 0, 1, 0, 0, 0);
	semaphore_intr_enable(sema_handle, PBSemaMap_dHubSemID_APB0_DATA_CP, 0, 1, 0, 0, 0);


	return 0;
}

static inline int dhub_dma_cfgw(struct dw_spi *mi, SIE_BCMCFGW *cfgw)
{
	int ndesc = 0;

	/* disable spi */
	cfgw->u_dat = 0;
	cfgw->u_devAdr = pb_get_phys_offset(mi, DW_SPI_SSIENR);
	cfgw->u_hdr = BCMINSFMT_hdr_CFGW;
	cfgw++;
	ndesc++;


	/* baudrate */
	cfgw->u_dat = mi->cur_chip->clk_div;
	cfgw->u_devAdr = pb_get_phys_offset(mi, DW_SPI_BAUDR);
	cfgw->u_hdr = BCMINSFMT_hdr_CFGW;
	cfgw++;
	ndesc++;

	/* slave select */
	cfgw->u_dat = 1;
	cfgw->u_devAdr = pb_get_phys_offset(mi, DW_SPI_SER);
	cfgw->u_hdr = BCMINSFMT_hdr_CFGW;
	cfgw++;
	ndesc++;

	/* dma control */
	cfgw->u_dat = 0x2;
	cfgw->u_devAdr = pb_get_phys_offset(mi, DW_SPI_DMACR);
	cfgw->u_hdr = BCMINSFMT_hdr_CFGW;
	cfgw++;
	ndesc++;

	/* dma transmit data level */
	cfgw->u_dat = 0;
	cfgw->u_devAdr = pb_get_phys_offset(mi, DW_SPI_DMATDLR);
	cfgw->u_hdr = BCMINSFMT_hdr_CFGW;
	cfgw++;
	ndesc++;

	/* enable spi */
	cfgw->u_dat = 1;
	cfgw->u_devAdr = pb_get_phys_offset(mi, DW_SPI_SSIENR);
	cfgw->u_hdr = BCMINSFMT_hdr_CFGW;
	cfgw++;
	ndesc++;

	return ndesc;
}

static inline void dhub_dma_sema(struct dw_spi *mi, SIE_BCMSEMA *sema)
{
	sema->u_pUpdId = 0;
	sema->u_pChkId = 0;
	sema->u_cUpdId = 0;
	sema->u_cChkId = PBSemaMap_dHubSemID_dHub_APBTx2;
	sema->u_rsvd0 = 0;
	sema->u_rsvd1 = 0;
	sema->u_hdr = BCMINSFMT_hdr_SEMA;
}

static inline void dhub_dma_rcmd(struct dw_spi *dws, SIE_BCMRCMD *rcmd, unsigned long offset, int update)
{
	struct mv_dhub_dma *mi =(struct mv_dhub_dma *)dws->dma_priv;

	rcmd->u_ddrAdr = dws->tx_dma + offset;
	rcmd->u_size = update?(dws->len - offset):mi->fifo_sz;
	rcmd->u_chkSemId = 0;
	rcmd->u_updSemId = 0;
	rcmd->u_rsvd = 0;
	rcmd->u_hdr = BCMINSFMT_hdr_RCMD;
}



static inline void dhub_dma_wcmd(struct dw_spi *dws, SIE_BCMRCMD *rcmd, unsigned long offset, int update)
{
	struct mv_dhub_dma *mi =(struct mv_dhub_dma *)dws->dma_priv;

	rcmd->u_ddrAdr = dws->rx_dma + offset;
	rcmd->u_size = update?(dws->len - offset):mi->fifo_sz;
	rcmd->u_chkSemId = 0;
	rcmd->u_updSemId = 0;
	rcmd->u_rsvd = 0;
	rcmd->u_hdr = BCMINSFMT_hdr_WCMD;
}

/*
 * u_size: size of data to be trasferred from ram
 * u_mode: 0/1/2, 8bit/16bit/32bit
 * u_endian: 0/1, little/big
 * u_last: indicate the arbiter to release the bus
 * u_cUpdId: semaphore for consumer to update
 * u_pUpdId: semaphore for producer to update
 * u_devAdr: address of device or mem that being trasferred to
 */
static inline void dhub_dma_rdat(struct dw_spi *dws, SIE_BCMRDAT *rdat, int last)
{
	struct mv_dhub_dma *mi =(struct mv_dhub_dma *)dws->dma_priv;
	rdat->u_size = (last && (dws->len % mi->fifo_sz))?(dws->len % mi->fifo_sz):mi->fifo_sz;
	rdat->u_mode = 0;
	rdat->u_endian = 0;
	rdat->u_last = 1;
	rdat->u_cUpdId = PBSemaMap_dHubSemID_dHub_APBTx2;
	rdat->u_pUpdId = last ? PBSemaMap_dHubSemID_APB0_DATA_CP : 0;
	rdat->u_rsvd0 = 0;
	rdat->u_devAdr = pb_get_phys_offset(mi, DW_SPI_DR);
	rdat->u_hdr = BCMINSFMT_hdr_RDAT;
}


/*
 * u_size: size of data to be trasferred from ram
 * u_mode: 0/1/2, 8bit/16bit/32bit
 * u_endian: 0/1, little/big
 * u_last: indicate the arbiter to release the bus
 * u_cUpdId: semaphore for consumer to update
 * u_pUpdId: semaphore for producer to update
 * u_devAdr: address of device or mem that being trasferred to
 */
static inline void dhub_dma_wdat(struct dw_spi *dws, SIE_BCMRDAT *rdat, int last)
{
	struct mv_dhub_dma *mi =(struct mv_dhub_dma *)dws->dma_priv;
	rdat->u_size = (last && (dws->len % mi->fifo_sz))?(dws->len % mi->fifo_sz):mi->fifo_sz;
	rdat->u_mode = 0;
	rdat->u_endian = 0;
	rdat->u_last = 1;
	rdat->u_cUpdId = PBSemaMap_dHubSemID_dHub_APBTx2;
	rdat->u_pUpdId = last ? PBSemaMap_dHubSemID_APB0_DATA_CP : 0;
	rdat->u_rsvd0 = 0;
	rdat->u_devAdr = pb_get_phys_offset(mi, DW_SPI_DR);
	rdat->u_hdr = BCMINSFMT_hdr_WDAT;
}


/*
 * 1 chunk == 64 bytes
 */
static void dhub_dma_deploy(struct dw_spi *dws, unsigned int nb)
{
	struct mv_dhub_dma *mi =(struct mv_dhub_dma *)dws->dma_priv;
	char *pb = mi->buf_desc;
	unsigned int ndesc;
	unsigned int i, chunks;

	ndesc = dhub_dma_cfgw(dws, (SIE_BCMCFGW *)pb);
	pb += sizeof(SIE_BCMCFGW) * ndesc;
	chunks = nb / mi->fifo_sz;
	if (nb % mi->fifo_sz > 0)
		chunks += 1;
	for (i = 0; i < chunks; i++) {
		dhub_dma_sema(dws, (SIE_BCMSEMA *)pb);
		pb += sizeof(SIE_BCMSEMA);
		if (i == chunks - 1) {
			if (dws->tx_dma)
				dhub_dma_rcmd(dws, (SIE_BCMRCMD *)pb, i*mi->fifo_sz, 1);
			if (dws->rx_dma)
				dhub_dma_wcmd(dws, (SIE_BCMRCMD *)pb, i*mi->fifo_sz, 1);
			pb += sizeof(SIE_BCMRCMD);

			if (dws->tx_dma)
				dhub_dma_rdat(dws, (SIE_BCMRDAT *)pb, 1);
			if (dws->rx_dma)
				dhub_dma_wdat(dws, (SIE_BCMRDAT *)pb, 1);
			pb += sizeof(SIE_BCMRDAT);
		} else {
			if (dws->tx_dma)
				dhub_dma_rcmd(dws, (SIE_BCMRCMD *)pb, i*mi->fifo_sz, 0);
			if (dws->rx_dma)
				dhub_dma_wcmd(dws, (SIE_BCMRCMD *)pb, i*mi->fifo_sz, 0);
			pb += sizeof(SIE_BCMRCMD);

			if (dws->tx_dma)
				dhub_dma_rdat(dws, (SIE_BCMRDAT *)pb, 0);
			if (dws->rx_dma)
				dhub_dma_wdat(dws, (SIE_BCMRDAT *)pb, 0);
			pb += sizeof(SIE_BCMRDAT);
		}
	}

	mi->desc_sz = (int)(pb - mi->buf_desc);
}

static int dhub_dma_do_xfer(struct dw_spi *dws)
{
	struct mv_dhub_dma *mi =(struct mv_dhub_dma *)dws->dma_priv;
	int ret = 0;
	wmb();
	ret = dhub_channel_write_cmd(
			&mi->h_pb_dhub,		/* Handle to HDL_dhub */
			SPI_DEV_CTL_CHANNEL_ID,		/* Channel ID in $dHubReg */
			mi->buf_desc_dma,	/* CMD: buffer address */
			mi->desc_sz,	/* CMD: number of bytes to transfer */
			0,	/* CMD: semaphore operation at CMD/MTU (0/1) */
			0,	/* CMD: non-zero to check semaphore */
			0,	/* CMD: non-zero to update semaphore */
			1,	/* CMD: raise interrupt at CMD finish */
			0,	/* Pass NULL to directly update dHub */
			0	/* Pass in current cmdQ pointer (in 64b word) */
			);
	if (ret > 0)
		dev_dbg(&dws->master->dev, "%s done, ret = %d\n", __func__, ret);
	else
		dev_err(&dws->master->dev, "%s, ret = %d\n", __func__, ret);
	return ret;
}


static int dhub_dma_xfer(struct dw_spi *dws, unsigned int nb)
{
	int ret = 0;

	dhub_dma_deploy(dws, nb);
	ret = dhub_dma_do_xfer(dws);
	if (ret <= 0)
		return -EIO;

	dev_dbg(&dws->master->dev, "%s %sdone, %d\n", __func__, ret?"NOT ":"", ret);

	return ret;
}

static int dhub_spi_dma_init(struct dw_spi *dws)
{
	struct mv_dhub_dma *mi = NULL;
	struct device *dev = &dws->master->dev;
	int ret = 0;

	dws->dma_priv = kzalloc(sizeof(struct mv_dhub_dma), GFP_KERNEL);
	if(!(dws->dma_priv)) {
		printk(KERN_ERR"dhub_spi_dma_init no memory\n");
		ret = -ENOMEM;
		return ret;
	}

	mi = (struct mv_dhub_dma *)dws->dma_priv;
	mi->buf_desc = dma_alloc_coherent(dws->parent_dev, DMA_ALLOC_BUF_SIZE,
					  &mi->buf_desc_dma, GFP_KERNEL);

	if (mi->buf_desc == NULL) {
		dev_err(dev, "failed to allocate dma buffer\n");
		ret = -ENOMEM;
		goto alloc_desc_fail;
	}


	dev_info(dev, "buf desc alloc: 0x%p, 0x%p (dma)\n",
		 mi->buf_desc, (void *)mi->buf_desc_dma);

	mi->fifo_sz = SPI_SYS_FIFO_SIZE;
	dhub_dma_init(mi);
	memset(mi->buf_desc, 0, DMA_ALLOC_BUF_SIZE);

	dev_info(dev, "%s, done. (mi: 0x%p)\n", __func__, mi);
	dws->dma_inited = 1;
	return 0;

alloc_desc_fail:
	kfree(dws->dma_priv);
	dws->dma_priv = NULL;
	return ret;

}


static void dhub_spi_dma_exit(struct dw_spi *dws)
{
	struct mv_dhub_dma * mi = (struct mv_dhub_dma *)dws->dma_priv;
	dma_free_coherent(dws->parent_dev, DMA_ALLOC_BUF_SIZE,
			  mi->buf_desc, mi->buf_desc_dma);
	kfree(dws->dma_priv);

}



static int dhub_spi_dma_transfer(struct dw_spi *dws, int cs_change)
{

	int ret ;
	ret = dhub_dma_xfer(dws, dws->len);
	return ret;
}


static struct dw_spi_dma_ops dhub_dma_ops = {
	.dma_init	= dhub_spi_dma_init,
	.dma_exit	= dhub_spi_dma_exit,
	.dma_transfer	= dhub_spi_dma_transfer,
};

int dw_dhub_spi_add_host(struct dw_spi *dws)
{
	struct spi_master *master;
	int ret;

	BUG_ON(dws == NULL);

	master = spi_alloc_master(dws->parent_dev, 0);
	if (!master) {
		ret = -ENOMEM;
		goto exit;
	}

	dws->master = master;
	dws->type = SSI_MOTO_SPI;
	dws->prev_chip = NULL;
	dws->dma_inited = 0;
	dws->dma_addr = (dma_addr_t)(dws->paddr + 0x60);
	snprintf(dws->name, sizeof(dws->name), "dw_spi%d",
			dws->bus_num);

	ret = request_irq(dws->irq, dw_dhub_spi_irq, IRQF_SHARED,
			dws->name, dws);
	if (ret < 0) {
		dev_err(&master->dev, "can not get IRQ\n");
		goto err_free_master;
	}

	master->mode_bits = SPI_CPOL | SPI_CPHA;
	master->bus_num = dws->bus_num;
	master->num_chipselect = dws->num_cs;
	master->cleanup = dw_spi_cleanup;
	master->setup = dw_dhub_spi_setup;
	master->transfer = dw_spi_transfer;
	master->dev.of_node = dws->parent_dev->of_node;
	/* Basic HW init */
	//spi_hw_init(dws);
	dws->dma_ops = &dhub_dma_ops;

	if (dws->dma_ops && dws->dma_ops->dma_init) {
		ret = dws->dma_ops->dma_init(dws);
		if (ret) {
			dev_err(&master->dev, "DMA init failed\n");
			dws->dma_inited = 0;
			goto err_diable_hw;
		}
	}

	/* Initial and start queue */
	ret = init_queue(dws);
	if (ret) {
		dev_err(&master->dev, "problem initializing queue\n");
		goto err_diable_hw;
	}
	ret = start_queue(dws);
	if (ret) {
		dev_err(&master->dev, "problem starting queue\n");
		goto err_diable_hw;
	}

	spi_master_set_devdata(master, dws);
	ret = spi_register_master(master);
	if (ret) {
		dev_err(&master->dev, "problem registering spi master\n");
		goto err_queue_alloc;
	}

	mrst_spi_debugfs_init(dws);
	return 0;

err_queue_alloc:
	destroy_queue(dws);
	if (dws->dma_ops && dws->dma_ops->dma_exit)
		dws->dma_ops->dma_exit(dws);
err_diable_hw:
	spi_enable_chip(dws, 0);
	free_irq(dws->irq, dws);
err_free_master:
	spi_master_put(master);
exit:
	return ret;
}
EXPORT_SYMBOL_GPL(dw_dhub_spi_add_host);

MODULE_AUTHOR("Hongzhan Chen <hzchen@marvell.com>");
MODULE_DESCRIPTION("Driver for DesignWare SPI controller core");
MODULE_LICENSE("GPL v2");
