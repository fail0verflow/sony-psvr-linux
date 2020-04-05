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
#include <linux/kernel.h>

#include "galois_io.h"
#include "cinclude.h"
#include "amp_type.h"
#include "amp_ioctl.h"
#include "drv_vpu.h"
#include "drv_msg.h"
#include "drv_debug.h"
#include "api_dhub.h"
#include "api_avio_dhub.h"
#include "hal_dhub_wrap.h"
#include "amp_driver.h"

static VPU_CTX vpu_ctx;

extern INT amp_irqs[IRQ_AMP_MAX];

irqreturn_t amp_devices_vdec_isr(int irq, VOID *dev_id)
{
    HRESULT ret = S_OK;
    MV_CC_MSG_t msg = {0};
    VPU_CTX *hVpuCtx;

    hVpuCtx = (VPU_CTX *)dev_id;
    if (hVpuCtx == NULL) {
        amp_error("amp_devices_vdec_isr: hVpuCtx=null!\n");
    }

    /* disable interrupt */
    disable_irq_nosync(irq);

    if (irq == amp_irqs[IRQ_DHUBINTRVPRO]) {
        msg.m_Param1 = ++hVpuCtx->vdec_int_cnt;
#ifdef CONFIG_MV_AMP_COMPONENT_VMETA_ENABLE
        ret = AMPMsgQ_Add(&hVpuCtx->hVPUMsgQ, &msg);
        if (ret != S_OK) {
            if (!atomic_read(&hVpuCtx->vdec_isr_msg_err_cnt)) {
                amp_error("[vdec isr] MsgQ full\n");
            }
            atomic_inc(&hVpuCtx->vdec_isr_msg_err_cnt);
            return IRQ_HANDLED;
        }
        up(&hVpuCtx->vdec_vmeta_sem);
#endif
    } else if (irq == V2G_IRQ_NUM) {
#ifdef CONFIG_MV_AMP_COMPONENT_V2G_ENABLE
        up(&hVpuCtx->vdec_v2g_sem);
#endif
    }

    return IRQ_HANDLED;
}

VPU_CTX * drv_vpu_init(VOID)
{
    VPU_CTX *hVpuCtx = &vpu_ctx;
    UINT err;

    memset(hVpuCtx, 0, sizeof(VPU_CTX));

    sema_init(&hVpuCtx->vdec_v2g_sem, 0);
    sema_init(&hVpuCtx->vdec_vmeta_sem, 0);

    err = AMPMsgQ_Init(&hVpuCtx->hVPUMsgQ, VDEC_ISR_MSGQ_SIZE);
    if (unlikely(err != S_OK)) {
        amp_error("drv_vpu_init: hVPUMsgQ init failed, err:%8x\n", err);
        return NULL;
    }

    return hVpuCtx;
}

VOID drv_vpu_exit(VPU_CTX *hVpuCtx)
{
    UINT err;
    err = AMPMsgQ_Destroy(&hVpuCtx->hVPUMsgQ);
    if (unlikely(err != S_OK)) {
        amp_error("drv_app_exit: failed, err:%8x\n", err);
        return;
    }
}
