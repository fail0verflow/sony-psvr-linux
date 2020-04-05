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
#include "drv_zsp.h"
#include "drv_msg.h"
#include "drv_debug.h"
#include "api_dhub.h"
#include "api_avio_dhub.h"
#include "hal_dhub_wrap.h"
#include "zspWrapper.h"
#include "amp_memmap.h"

static ZSP_CTX zsp_ctx;

irqreturn_t amp_devices_zsp_isr(INT irq, VOID *dev_id)
{
    UNSG32 addr, v_id;
    T32ZspInt2Soc_status reg;
    MV_CC_MSG_t msg = {0};
    ZSP_CTX *hZspCtx = (ZSP_CTX *)dev_id;

    addr = MEMMAP_ZSP_REG_BASE + RA_ZspRegs_Int2Soc + RA_ZspInt2Soc_status;
    GA_REG_WORD32_READ(addr, &(reg.u32));

    addr = MEMMAP_ZSP_REG_BASE + RA_ZspRegs_Int2Soc + RA_ZspInt2Soc_clear;
    v_id = ADSP_ZSPINT2Soc_IRQ0;
    if ((reg.u32) & (1 << v_id)) {
        GA_REG_WORD32_WRITE(addr, v_id);
    }

    msg.m_MsgID = 1 << ADSP_ZSPINT2Soc_IRQ0;
    AMPMsgQ_Add(&hZspCtx->hZSPMsgQ, &msg);
    up(&hZspCtx->zsp_sem);

    return IRQ_HANDLED;
}

ZSP_CTX *drv_zsp_init(VOID)
{
    UINT err;
    ZSP_CTX *hZspCtx = &zsp_ctx;

    memset(hZspCtx, 0, sizeof(ZSP_CTX));
    sema_init(&hZspCtx->zsp_sem, 0);
    err = AMPMsgQ_Init(&hZspCtx->hZSPMsgQ, ZSP_ISR_MSGQ_SIZE);
    if (unlikely(err != S_OK)) {
        amp_error("drv_zsp_init: hZSPMsgQ init failed, err:%8x\n", err);
        return NULL;
    }

    return hZspCtx;
}

VOID drv_zsp_exit(ZSP_CTX *hZspCtx)
{
    UINT err;

    err = AMPMsgQ_Destroy(&hZspCtx->hZSPMsgQ);
    if (unlikely(err != S_OK)) {
        amp_error("drv_zsp_exit: ZSP MsgQ Destroy failed, err:%8x\n", err);
    }
}
