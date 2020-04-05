/******************************************************************************
 * Copyright (c) 2013-2014 Marvell International Ltd. and its affiliates.
 * All rights reserved.
 *
 * This software file (the "File") is owned and distributed by Marvell
 * International Ltd. and/or its affiliates ("Marvell") under the following
 * licensing terms.
 ******************************************************************************
 * Marvell Commercial License Option
 *
 * If you received this File from Marvell and you have entered into a
 * commercial license agreement (a "Commercial License") with Marvell, the
 * File is licensed to you under the terms of the applicable Commercial
 * License.
 ******************************************************************************
 * Marvell GPL License Option
 *
 * If you received this File from Marvell, you may opt to use, redistribute
 * and/or modify this File in accordance with the terms and conditions of the
 * General Public License Version 2, June 1991 (the "GPL License"), a copy of
 * which is available along with the File in the license.txt file or by writing
 * to the Free Software Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 or on the worldwide web at http://www.gnu.org/licenses/gpl.txt.
 * THE FILE IS DISTRIBUTED AS-IS, WITHOUT WARRANTY OF ANY KIND, AND THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE ARE
 * EXPRESSLY DISCLAIMED. The GPL License provides additional details about this
 * warranty disclaimer.
 *******************************************************************************/

#ifndef _DRV_MORPHEUS_H_
#define _DRV_MORPHEUS_H_

#include <linux/init.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/sched.h>
#include <net/sock.h>
#include <linux/proc_fs.h>

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/of.h>
#include <linux/of_irq.h>

#include <linux/mm.h>
#include <linux/kdev_t.h>
#include <asm/page.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/version.h>
#include <linux/interrupt.h>
#include <linux/spinlock.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION (3,10,0)
#include <../drivers/staging/android/ion/ion.h>
#else
#include <linux/ion.h>
#endif
#include <asm/uaccess.h>

#include "galois_io.h"
#include "cinclude.h"
#include "api_dhub.h"
#include "avioDhub.h"
#include "api_avio_dhub.h"
#include "hal_dhub_wrap.h"

#include "amp_type.h"
#include "amp_ioctl.h"
#include "drv_msg.h"
#include "drv_debug.h"


#define MORPHEUS_TV_VIEW_MODE

#ifdef MORPHEUS_TV_VIEW_MODE
#define MODE_INVALID 0
#define MODE_TV_VIEW 1

#define AVIF_IOCTL_GET_AVIF_VPP_DRIFT_INFO 0xbeef6008
#define AVIF_IOCTL_SEND_MSG_TV_VIEW 0xbeef6009

#define AVIF_MSG_NOTIFY_STABLE 2
#define AVIF_MSG_SET_TV_VIEW_MODE 3
#define AVIF_MSG_SET_INVALID_MODE 4
#define AVIF_MSG_NOTIFY_VPP_RES_SET 5
#define AVIF_MSG_NOTIFY_VPP_DISCONNECT 6

#define MAX_TIMELINE	128
#define MAX_STAT	10
#define FAST_PASSTHRU_PROCFILE_STATUS           "passthru_stat"

typedef enum {
    TG_CHANGE_STATE_CHECK = 0,
    TG_CHANGE_STATE_SET_BACK_TO_NORMAL,
    TG_CHANGE_STATE_WAIT_EFFECT,
    TG_CHANGE_STATE_DONE,
} TG_CHANGE_STATE;

typedef struct
{
    int msg_type;
    int para0;
    int para1;
} MSG_TV_VIEW;

typedef struct DRIFT_INFO_T{
	int valid;
	unsigned long start_latency;
	int drift_count;
	long total_drift_count;
	UINT frame_count;
	int latency_in_the_expected_range;
}DRIFT_INFO;

int read_passthru_proc_stat(char *page, char **start, off_t offset,
			    int count, int *eof, void *data);
void reset_adjust(int saveIrq);
void adjust_inout_latency(void);
#endif

#endif    //_DRV_MORPHEUS_H_
