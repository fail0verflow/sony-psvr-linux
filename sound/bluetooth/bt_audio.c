/*
 * Bluetooth ALSA driver
 *
 * Copyright (c) 2012-2013 Marvell
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License.
 */
#include <linux/module.h>
#include <linux/version.h>
#include <linux/init.h>
#include <linux/jiffies.h>
#include <linux/slab.h>
#include <linux/time.h>
#include <linux/wait.h>
#include <linux/socket.h>
#include <linux/file.h>
#include <linux/completion.h>
#include <net/sock.h>
#include <net/bluetooth/bluetooth.h>

#include <sound/core.h>
#include <sound/control.h>
#include <sound/pcm.h>
#include <sound/rawmidi.h>
#include <sound/hwdep.h>
#define SNDRV_GET_ID
#include <sound/initval.h>

#define MAX_BUFFER_SIZE (32 * 1024)


typedef enum
{
	SND_BT_IOCTL_CREATE_DEV = 0x100,
	SND_BT_IOCTL_DESTORY_DEV = 0x110,
}SND_BT_IOCTL_CMD;

struct snd_bt_card_pcm;

struct snd_bt_chip {
	struct snd_card *card;
	struct snd_bt_card_pcm *capture;
};

struct snd_bt_card_pcm {
	unsigned int pcm_size;
	unsigned int pcm_count;
	unsigned int pcm_bps;	/* bytes per second */
	unsigned int pcm_buf_pos;	/* position in buffer */
	unsigned int pcm_buf_count;
	struct snd_pcm_substream *substream;
};

static struct snd_card *snd_bt_card;
static int reference_count;

static int snd_bt_card_capture_state;
static DECLARE_WAIT_QUEUE_HEAD(snd_bt_wait);

static inline void snd_card_bt_capture_trigger_start(void)
{
	snd_bt_card_capture_state = 1;
	wake_up_interruptible(&snd_bt_wait);
}

static inline void snd_card_bt_capture_trigger_stop(void)
{
	snd_bt_card_capture_state = 0;
	wake_up_interruptible(&snd_bt_wait);
}

static DEFINE_MUTEX(snd_bt_mutex);

static inline void snd_bt_lock(void)
{
	mutex_lock(&snd_bt_mutex);
}

static inline void snd_bt_unlock(void)
{
	mutex_unlock(&snd_bt_mutex);
}

static const struct snd_pcm_hardware snd_card_bt_capture = {
	.info = (SNDRV_PCM_INFO_MMAP
		| SNDRV_PCM_INFO_INTERLEAVED
		| SNDRV_PCM_INFO_MMAP_VALID),
	.formats = SNDRV_PCM_FMTBIT_S16_LE,
	.rates = SNDRV_PCM_RATE_16000,
	.rate_min = 16000,
	.rate_max = 16000,
	.channels_min = 1,
	.channels_max = 1, /* set in pcm_open, depending on capture/playback */
	.buffer_bytes_max = MAX_BUFFER_SIZE,
	.period_bytes_min = 256,
	.period_bytes_max = MAX_BUFFER_SIZE / 2,
	.periods_min = 1,
	.periods_max = 4 * 16000 / 24,
	.fifo_size = 0
};

static void snd_card_bt_runtime_free(struct snd_pcm_runtime *runtime)
{
	struct snd_bt_card_pcm *btpcm = runtime->private_data;
	kfree(btpcm);
}

static int snd_card_bt_capture_open(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct snd_bt_card_pcm *btpcm;

	snd_printk("capture_open\n");

	btpcm = kzalloc(sizeof(struct snd_bt_card_pcm), GFP_KERNEL);
	if (btpcm == NULL)
		return -ENOMEM;

	if ((runtime->dma_area =
	     snd_malloc_pages(MAX_BUFFER_SIZE, GFP_KERNEL)) == NULL) {
		kfree(btpcm);
		return -ENOMEM;
	}
	runtime->dma_bytes = MAX_BUFFER_SIZE;
	memset(runtime->dma_area, 0, runtime->dma_bytes);
	btpcm->substream = substream;
	runtime->private_data = btpcm;
	runtime->private_free = snd_card_bt_runtime_free;
	runtime->hw = snd_card_bt_capture;

	return 0;
}

static int snd_card_bt_capture_close(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;

	snd_free_pages(runtime->dma_area, runtime->dma_bytes);
	return 0;
}

static int snd_card_bt_capture_prepare(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct snd_bt_card_pcm *btpcm = runtime->private_data;
	unsigned int bps;

	bps = runtime->rate * runtime->channels;
	bps *= snd_pcm_format_width(runtime->format);
	bps /= 8;
	if (bps <= 0)
		return -EINVAL;
	btpcm->pcm_bps = bps;
	btpcm->pcm_size = snd_pcm_lib_buffer_bytes(substream);
	btpcm->pcm_count = snd_pcm_lib_period_bytes(substream);
	btpcm->pcm_buf_pos = 0;
	btpcm->pcm_buf_count = 0;
	snd_printk("prepare ok bps: %d size: %d count: %d\n",
		btpcm->pcm_bps, btpcm->pcm_size, btpcm->pcm_count);
	return 0;
}

static int snd_card_bt_capture_trigger(struct snd_pcm_substream *substream, int cmd)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct snd_bt_card_pcm *btpcm = runtime->private_data;
	struct snd_bt_chip *chip = snd_pcm_substream_chip(substream);

	if (cmd == SNDRV_PCM_TRIGGER_START) {
		chip->capture = btpcm;
		snd_card_bt_capture_trigger_start();
		snd_printk("capture setting done\n");
	} else if (cmd == SNDRV_PCM_TRIGGER_STOP) {
		chip->capture = NULL;
		snd_card_bt_capture_trigger_stop();
		snd_printk("capture end\n");
	} else {
		return -EINVAL;
	}
	return 0;
}

static snd_pcm_uframes_t
snd_card_bt_capture_pointer(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct snd_bt_card_pcm *btpcm = runtime->private_data;

	return bytes_to_frames(runtime, btpcm->pcm_buf_pos);
}

static ssize_t snd_card_bt_pcm_receive(const char __user *data, unsigned int len)
{
	struct snd_bt_chip *chip = (struct snd_bt_chip *)snd_bt_card->private_data;
	struct snd_bt_card_pcm *btpcm = chip->capture;
	unsigned int oldptr;
	unsigned int pos = 0;

	if(btpcm == NULL) {
		return -EFAULT;
	}

	oldptr = btpcm->pcm_buf_pos;
	btpcm->pcm_buf_pos += len;
	btpcm->pcm_buf_count += len;
	pos = btpcm->pcm_buf_count;
	btpcm->pcm_buf_pos %= btpcm->pcm_size;

	/* copy the data chunk */
	if (oldptr + len > btpcm->pcm_size) {
		unsigned int cnt = btpcm->pcm_size - oldptr;
		if(copy_from_user(btpcm->substream->runtime->dma_area + oldptr, data, cnt)) {
			return -EFAULT;
		}
		if(copy_from_user(btpcm->substream->runtime->dma_area, data + cnt, len - cnt)) {
			return -EFAULT;
		}
	} else {
		if(copy_from_user(btpcm->substream->runtime->dma_area + oldptr, data, len)) {
			return -EFAULT;
		}
	}

	/* update the pointer */
	if (pos >= btpcm->pcm_count) {
		snd_pcm_period_elapsed(btpcm->substream);
		btpcm->pcm_buf_count %= btpcm->pcm_count;
	}


	return len;
}

static struct snd_pcm_ops snd_card_bt_capture_ops = {
	.open = snd_card_bt_capture_open,
	.close = snd_card_bt_capture_close,
	.ioctl = snd_pcm_lib_ioctl,
	.prepare = snd_card_bt_capture_prepare,
	.trigger = snd_card_bt_capture_trigger,
	.pointer = snd_card_bt_capture_pointer,
};

static int snd_bt_card_new_pcm(struct snd_bt_chip *chip)
{
	struct snd_pcm *pcm;
	int err;

	if ((err =
	     snd_pcm_new(chip->card, "Bluetooth ALSA PCM", 0, 0, 1, &pcm)) < 0)
		return err;

	snd_pcm_set_ops(pcm, SNDRV_PCM_STREAM_CAPTURE,
			&snd_card_bt_capture_ops);
	pcm->private_data = chip;
	pcm->info_flags = 0;
	strcpy(pcm->name, "BT ALSA PCM");
	return 0;
}

static void snd_card_bt_private_free(struct snd_card *card)
{
	struct snd_bt_chip *chip = card->private_data;

	kfree(chip);
}

static int snd_bt_card_probe(int dev)
{
	int ret;
	char id[16];
	struct snd_bt_chip *chip;
	int err;
	struct snd_hwdep *hw;

	snd_printk("bt snd card is probed\n");
	sprintf(id, "MRVLRCMIC");
	ret = snd_card_create(-1, id,
			 THIS_MODULE, 0, &snd_bt_card);
	if (snd_bt_card == NULL)
		return -ENOMEM;

	chip = kzalloc(sizeof(struct snd_bt_chip), GFP_KERNEL);
	if(chip == NULL)
		return -ENOMEM;

	snd_bt_card->private_data = chip;
	snd_bt_card->private_free = snd_card_bt_private_free;

	chip->card = snd_bt_card;

	if ((err = snd_bt_card_new_pcm(chip)) < 0)
		goto __nodev;

	strcpy(snd_bt_card->driver, "Bluetooth Alsa");
	strcpy(snd_bt_card->shortname, "BT Alsa");
	sprintf(snd_bt_card->longname, "BT Alsa %i", dev + 1);

	err = snd_hwdep_new(snd_bt_card, "BTALSA", 0, &hw);
	if (err < 0)
		goto __nodev;

	if ((err = snd_card_register(snd_bt_card)) == 0) {
		snd_printk("bt snd card device driver registered\n");
		return 0;
	}

__nodev:
	snd_card_free(snd_bt_card);
	return err;
}

static int snd_bt_open(struct inode *inode, struct file *filp)
{
	if (try_module_get(THIS_MODULE) == 0)
		return -EFAULT;
	nonseekable_open(inode, filp);
	filp->f_version = 0;
	return 0;
}

static int snd_bt_release(struct inode *inode, struct file *filp)
{
	module_put(THIS_MODULE);
	return 0;
}

static ssize_t snd_bt_read(struct file *filp, char __user *buf,
				size_t count, loff_t *ppos)
{
	int state = filp->f_version;
	if (unlikely(count < sizeof(state)))
		return -EINVAL;
	if (copy_to_user(buf, &state, sizeof(state)))
		return -EFAULT;
	return sizeof(state);
}

static ssize_t snd_bt_write(struct file *file, const char __user *buf,
				size_t count, loff_t *ppos)
{
	ssize_t r;
	snd_bt_lock();
	r = (snd_bt_card) ? snd_card_bt_pcm_receive(buf, count) : -ENODEV;
	snd_bt_unlock();
	return r;
}

static long snd_bt_ioctl(struct file *file,
			unsigned int cmd, unsigned long arg)
{
	int r = 0;
	snd_bt_lock();
	switch(cmd) {
	case SND_BT_IOCTL_CREATE_DEV:
		if (snd_bt_card == NULL) {
			if (snd_bt_card_probe(0) < 0) {
				printk(KERN_ERR
					"Bluetooth soundcard not found or device busy\n");
				r = -ENODEV;
				break;
			}
		}
		reference_count++;
		break;
	case SND_BT_IOCTL_DESTORY_DEV:
		if (snd_bt_card == NULL)
			break;
		if (--reference_count == 0) {
			snd_card_free(snd_bt_card);
			snd_bt_card = NULL;
			snd_printk("bt snd card device removed\n");
		}
		break;
	default:
		r = -EINVAL;
		break;
	}
	snd_bt_unlock();
	return r;
}

static unsigned int snd_bt_poll(struct file *filp, struct poll_table_struct *wait)
{
	poll_wait(filp, &snd_bt_wait, wait);
	if (filp->f_version != snd_bt_card_capture_state) {
		filp->f_version = snd_bt_card_capture_state;
		return POLLIN | POLLRDNORM;
	}
	return 0;
}

static struct file_operations snd_bt_fops = {
	.owner			= THIS_MODULE,
	.read			= snd_bt_read,
	.write			= snd_bt_write,
	.open			= snd_bt_open,
	.release		= snd_bt_release,
	.unlocked_ioctl		= snd_bt_ioctl,
	.poll			= snd_bt_poll
};

static struct miscdevice snd_bt_dev = {
	.minor	= 130,
	.name	= "snd_bt",
	.fops	= &snd_bt_fops,
};

static int __init snd_bt_init(void)
{
	printk("snd_bt_init\n");
	return misc_register(&snd_bt_dev);
}

static void __exit snd_bt_exit(void)
{
	printk("snd_bt_exit\n");
	misc_deregister(&snd_bt_dev);
}
MODULE_LICENSE("GPL");
module_init(snd_bt_init);
module_exit(snd_bt_exit);
