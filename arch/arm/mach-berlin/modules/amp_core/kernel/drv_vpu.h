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
#ifndef _DRV_VPU_H_
#define _DRV_VPU_H_
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

#define VDEC_ISR_MSGQ_SIZE			16

#if (BERLIN_CHIP_VERSION != BERLIN_BG4_CD)
#define VMETA_IRQ_NUM               34
#define V2G_IRQ_NUM                 88
#else
#define VMETA_IRQ_NUM
#define V2G_IRQ_NUM            (1 + 32)
#endif

enum VPU_HW_ID
{
    VPU_VMETA = 0,
    VPU_V2G   = 1,
    VPU_G2    = 2,
    VPU_G1    = 3,
    VPU_H1_0  = 4,
    VPU_H1_1  = 5,
    VPU_HW_IP_NUM,
};

typedef struct _VPU_CONTEXT_ {
    AMPMsgQ_t hVPUMsgQ;

    UINT vdec_int_cnt;
    UINT vdec_enable_int_cnt;
    atomic_t vdec_isr_msg_err_cnt;

    struct semaphore vdec_v2g_sem;
    struct semaphore vdec_vmeta_sem;
	INT app_int_cnt;
} VPU_CTX;

VPU_CTX * drv_vpu_init(VOID);
VOID drv_vpu_exit(VPU_CTX *hVpuCtx);
irqreturn_t amp_devices_vdec_isr(INT irq, VOID *dev_id);

#endif    //_DRV_VPU_H_