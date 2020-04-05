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
#ifndef _DRV_AOUT_H_
#define _DRV_AOUT_H_
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

#include "amp_type.h"
#include "amp_ioctl.h"
#include "drv_msg.h"

#define AOUT_ISR_MSGQ_SIZE			128

typedef struct _AOUT_CONTEXT_ {
	INT aout_int_cnt[MAX_OUTPUT_AUDIO];

    AOUT_ION_HANDLE gstAoutIon;
    AOUT_PATH_CMD_FIFO *p_hdmi_fifo;
	AOUT_PATH_CMD_FIFO *p_spdif_fifo;
	AOUT_PATH_CMD_FIFO *p_sa_fifo;
	AOUT_PATH_CMD_FIFO *p_ma_fifo;
	AOUT_PATH_CMD_FIFO *p_arc_fifo;

    spinlock_t aout_spinlock;
	struct semaphore aout_sem;
	spinlock_t aout_msg_spinlock;
	AMPMsgQ_t hAoutMsgQ;
} AOUT_CTX;

AOUT_CTX * drv_aout_init(VOID);
VOID drv_aout_exit(AOUT_CTX *hAoutCtx);
VOID send_msg2aout(MV_CC_MSG_t *pMsg);
VOID aout_resume(INT path_id);

VOID aout_start_cmd(AOUT_CTX *hAoutCtx, INT *aout_info, VOID *param);
VOID aout_stop_cmd(AOUT_CTX *hAoutCtx, INT path_id);
VOID aout_resume_cmd(AOUT_CTX *hAoutCtx, INT path_id);
irqreturn_t amp_devices_aout_isr(int irq, void *dev_id);

#endif    //_DRV_AOUT_H_
