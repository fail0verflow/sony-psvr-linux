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

extern int video_mode;
extern int vip_stable_isr;
extern int vip_stable;
extern int vpp_res_set;
extern int vpp_cpcb0_res;
extern int vpp_4k_res;
extern TG_CHANGE_STATE tg_changed;
extern long long vip_isr_count;
extern long long cur_vip_isr_count;
extern long long vpp_isr_count;
extern long long cur_vpp_isr_count;
extern spinlock_t drift_countlock;
extern DRIFT_INFO vip_vpp_drift_info;
#endif

// Copy from vpp_cfg.h and vpp_cfg.c
/* definition of VPP TG timing formats */
typedef enum {
    RES_INVALID   = -1,
    FIRST_RES     = 0,
    RES_NTSC_M    = 0,
    RES_NTSC_J    = 1,
    RES_PAL_M     = 2,
    RES_PAL_BGH   = 3,
    RES_525I60    = 4,
    RES_525I5994  = 5,
    RES_625I50    = 6,
    RES_525P60    = 7,
    RES_525P5994  = 8,
    RES_625P50    = 9,
    RES_720P30    = 10,
    RES_720P2997  = 11,
    RES_720P25    = 12,
    RES_720P60    = 13,
    RES_720P5994  = 14,
    RES_720P50    = 15,
    RES_1080I60   = 16,
    RES_1080I5994 = 17,
    RES_1080I50   = 18,
    RES_1080P30   = 19,
    RES_1080P2997 = 20,
    RES_1080P25   = 21,
    RES_1080P24   = 22,
    RES_1080P2398 = 23,
    RES_1080P60   = 24,
    RES_1080P5994 = 25,
    RES_1080P50   = 26,
    RES_LVDS_1080P48   = 27,
    RES_LVDS_1080P50   = 28,
    RES_LVDS_1080P60   = 29,
    RES_LVDS_2160P12   = 30,
    RES_VGA_480P60 = 31,
    RES_VGA_480P5994 = 32,
    FIRST_RES_3D = 33,
    RES_720P50_3D = 33,
    RES_720P60_3D = 34,
    RES_720P5994_3D = 35,
    RES_1080P24_3D = 36,
    RES_1080P2398_3D = 37,
    RES_1080P30_3D = 38,
    RES_1080P2997_3D = 39,
    RES_1080P25_3D = 40,
    RES_1080I60_FP = 41,
    RES_1080I5994_FP = 42,
    RES_1080I50_FP = 43,
    RES_LVDS_1920X540P60_3D = 44,
    RES_LVDS_1920X540P30_3D = 45,
    RES_LVDS_1920X540P24_3D = 46,
    RES_LVDS_720P100_3D = 47,
    RES_LVDS_720P120_3D = 48,
    RES_LVDS_1080P48_3D = 49,
    RES_LVDS_1080P50_3D = 50,
    RES_LVDS_1080P60_3D = 51,
    RES_LVDS_1920X540P100_3D = 52,
    RES_LVDS_1920X540P120_3D = 53,
    RES_LVDS_960X1080P100_3D = 54,
    RES_LVDS_960X1080P120_3D = 55, 
    MAX_NUM_RES_3D = 55,
    RES_MIN_4Kx2K      = 56,
    RES_4Kx2K2398      = 56,
    RES_4Kx2K24        = 57,
    RES_4Kx2K24_SMPTE  = 58,
    RES_4Kx2K25        = 59,
    RES_4Kx2K2997      = 60,
    RES_4Kx2K30        = 61,
    RES_4Kx2K50        = 62,
    RES_4Kx2K5994      = 63,
    RES_4Kx2K60        = 64,
    RES_4Kx2K30_HDMI   = 65,
    RES_4Kx1K120       = 66,
    RES_MAX_4Kx2K      = 66,
    RES_720P_4Kx1K120_3D = 67,
    RES_720P100        = 68,
    RES_720P11988      = 69,
    RES_720P120        = 70,
    RES_720P8991      = 71,
    RES_720P90        = 72,
    HFR_RES_MIN        = 73,
    RES_1080P100       = 73,
    RES_1080P11988     = 74,
    RES_1080P120       = 75,
    RES_1080P8991      = 76,
    RES_1080P90        = 77,
    HFR_RES_MAX        = 77,
    RES_4Kx2K2398_SMPTE = 78,
    RES_4Kx2K25_SMPTE   = 79,
    RES_4Kx2K2997_SMPTE = 80,
    RES_4Kx2K30_SMPTE   = 81,
    RES_4Kx2K50_SMPTE   = 82,
    RES_4Kx2K5994_SMPTE = 83,
    RES_4Kx2K60_SMPTE   = 84,
    RES_4Kx2K50_420     = 85,
    RES_4Kx2K5994_420   = 86,
    RES_4Kx2K60_420     = 87,
    RES_4Kx2K2398_3D    = 88,
    RES_4Kx2K24_3D      = 89,
    RES_4Kx2K25_3D      = 90,
    RES_4Kx2K2997_3D    = 91,
    RES_4Kx2K30_3D      = 92,
    RES_LVDS_1088P60    = 93, //Non Standard Resolution
    RES_RESET,

    MAX_NUM_RESS
}ENUM_CPCB_TG_RES;

#endif    //_DRV_MORPHEUS_H_
