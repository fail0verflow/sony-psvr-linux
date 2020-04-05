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
#ifndef _DRV_APP_H_
#define _DRV_APP_H_
#include <linux/version.h>
#include <linux/types.h>
#include <linux/semaphore.h>
#include <linux/mm.h>
#include <linux/interrupt.h>

#if LINUX_VERSION_CODE >= KERNEL_VERSION (3,10,0)
#include <../drivers/staging/android/ion/ion.h>
#else
#include <linux/ion.h>
#endif
#if LINUX_VERSION_CODE >= KERNEL_VERSION (3,10,0)
#include <../drivers/staging/android/ion/ion.h>
#else
#include <linux/ion.h>
#endif

#include "amp_type.h"
#include "amp_ioctl.h"
#include "drv_msg.h"

#define APP_ISR_MSGQ_SIZE			16


typedef struct {
    struct ion_handle *ionh;
} APP_ION_HANDLE;

typedef struct _APP_CONTEXT_ {
    HWAPP_CMD_FIFO *p_app_fifo;
    APP_ION_HANDLE  gstAppIon;

    AMPMsgQ_t hAPPMsgQ;
    spinlock_t app_spinlock;
    struct semaphore app_sem;
	INT app_int_cnt;
} APP_CTX;

APP_CTX * drv_app_init(VOID);
VOID drv_app_exit(APP_CTX *hAppCtx);

VOID app_resume(VOID);
VOID send_msg2app(MV_CC_MSG_t *pMsg);
VOID app_start_cmd(APP_CTX *hAppCtx, HWAPP_CMD_FIFO * p_app_cmd_fifo);
VOID app_resume_cmd(APP_CTX *hAppCtx, HWAPP_CMD_FIFO * p_app_cmd_fifo);
#endif    //_DRV_APP_H_
