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
#include "drv_app.h"
#include "drv_msg.h"
#include "drv_debug.h"
#include "api_dhub.h"
#include "api_avio_dhub.h"
#include "hal_dhub_wrap.h"

static APP_CTX app_ctx;

static INT HwAPPFifoCheckKernelFullness(HWAPP_CMD_FIFO * p_app_cmd_fifo)
{
    INT full;
    full = p_app_cmd_fifo->wr_offset - p_app_cmd_fifo->kernel_rd_offset;
    if (full < 0) {
        full += p_app_cmd_fifo->size;
    }
    return full;
}

static VOID *HwAPPFifoGetKernelInCoefRdCmdBuf(HWAPP_CMD_FIFO * p_app_cmd_fifo)
{
    VOID *pHandle;
    pHandle = &(p_app_cmd_fifo->in_coef_cmd[p_app_cmd_fifo->kernel_rd_offset]);
    return pHandle;
}

static VOID *HwAPPFifoGetKernelOutCoefRdCmdBuf(HWAPP_CMD_FIFO * p_app_cmd_fifo)
{
    VOID *pHandle;
    pHandle = &(p_app_cmd_fifo->out_coef_cmd[p_app_cmd_fifo->kernel_rd_offset]);
    return pHandle;
}

static VOID *HwAPPFifoGetKernelInDataRdCmdBuf(HWAPP_CMD_FIFO * p_app_cmd_fifo)
{
    VOID *pHandle;
    pHandle = &(p_app_cmd_fifo->in_data_cmd[p_app_cmd_fifo->kernel_rd_offset]);
    return pHandle;
}

static VOID *HwAPPFifoGetKernelOutDataRdCmdBuf(HWAPP_CMD_FIFO * p_app_cmd_fifo)
{
    VOID *pHandle;
    pHandle = &(p_app_cmd_fifo->out_data_cmd[p_app_cmd_fifo->kernel_rd_offset]);
    return pHandle;
}

static VOID HwAPPFifoUpdateIdleFlag(HWAPP_CMD_FIFO * p_app_cmd_fifo, INT flag)
{
    p_app_cmd_fifo->kernel_idle = flag;
}

static VOID HwAPPFifoKernelRdUpdate(HWAPP_CMD_FIFO * p_app_cmd_fifo, INT adv)
{
    p_app_cmd_fifo->kernel_rd_offset += adv;
    p_app_cmd_fifo->kernel_rd_offset %= p_app_cmd_fifo->size;
}

VOID app_start_cmd(APP_CTX *hAppCtx, HWAPP_CMD_FIFO * p_app_cmd_fifo)
{
	APP_CMD_BUFFER *p_in_coef_cmd, *p_out_coef_cmd;
	APP_CMD_BUFFER *p_in_data_cmd, *p_out_data_cmd;
	UINT chanId, PA, cmdSize;

	if (HwAPPFifoCheckKernelFullness(p_app_cmd_fifo)) {
#if ((BERLIN_CHIP_VERSION != BERLIN_BG4_CD) &&( BERLIN_CHIP_VERSION != BERLIN_BG4_CT))
		HwAPPFifoUpdateIdleFlag(p_app_cmd_fifo, 0);
		p_in_coef_cmd =
		    (APP_CMD_BUFFER *)
		    HwAPPFifoGetKernelInCoefRdCmdBuf(p_app_cmd_fifo);
		chanId = avioDhubChMap_ag_APPCMD_R;
		if (p_in_coef_cmd->cmd_len) {
			PA = p_in_coef_cmd->cmd_buffer_hw_base;
			cmdSize = p_in_coef_cmd->cmd_len * sizeof(INT);
			dhub_channel_write_cmd(&AG_dhubHandle.dhub, chanId, PA,
					       cmdSize, 0, 0, 0, 0, 0, 0);
		}
		p_in_data_cmd =
		    (APP_CMD_BUFFER *)
		    HwAPPFifoGetKernelInDataRdCmdBuf(p_app_cmd_fifo);
		if (p_in_data_cmd->cmd_len) {
			PA = p_in_data_cmd->cmd_buffer_hw_base;
			cmdSize = p_in_data_cmd->cmd_len * sizeof(INT);
			dhub_channel_write_cmd(&AG_dhubHandle.dhub, chanId, PA,
					       cmdSize, 0, 0, 0, 0, 0, 0);
		}
		p_out_coef_cmd =
		    (APP_CMD_BUFFER *)
		    HwAPPFifoGetKernelOutCoefRdCmdBuf(p_app_cmd_fifo);
		chanId = avioDhubChMap_ag_APPCMD_R;
		if (p_out_coef_cmd->cmd_len) {
			PA = p_out_coef_cmd->cmd_buffer_hw_base;
			cmdSize = p_out_coef_cmd->cmd_len * sizeof(INT);
			dhub_channel_write_cmd(&AG_dhubHandle.dhub, chanId, PA,
					       cmdSize, 0, 0, 0, 0, 0, 0);
		}
		p_out_data_cmd =
		    (APP_CMD_BUFFER *)
		    HwAPPFifoGetKernelOutDataRdCmdBuf(p_app_cmd_fifo);
		if (p_out_data_cmd->cmd_len) {
			PA = p_out_data_cmd->cmd_buffer_hw_base;
			cmdSize = p_out_data_cmd->cmd_len * sizeof(INT);
			dhub_channel_write_cmd(&AG_dhubHandle.dhub, chanId, PA,
					       cmdSize, 0, 0, 0, 0, 0, 0);
		}
#endif
	} else {
		HwAPPFifoUpdateIdleFlag(p_app_cmd_fifo, 1);
	}
}

VOID app_resume_cmd(APP_CTX *hAppCtx, HWAPP_CMD_FIFO * p_app_cmd_fifo)
{
	hAppCtx->app_int_cnt++;
	HwAPPFifoKernelRdUpdate(p_app_cmd_fifo, 1);
	app_start_cmd(hAppCtx, p_app_cmd_fifo);
}

VOID app_resume(VOID)
{
    app_resume_cmd(&app_ctx, app_ctx.p_app_fifo);
}

VOID send_msg2app(MV_CC_MSG_t *pMsg)
{
	AMPMsgQ_Add(&app_ctx.hAPPMsgQ, pMsg);
	up(&app_ctx.app_sem);
}

APP_CTX * drv_app_init(VOID)
{
    APP_CTX *hAppCtx = &app_ctx;
    UINT err;

    memset(hAppCtx, 0, sizeof(APP_CTX));

    spin_lock_init(&hAppCtx->app_spinlock);
    sema_init(&hAppCtx->app_sem, 0);
    err = AMPMsgQ_Init(&hAppCtx->hAPPMsgQ, APP_ISR_MSGQ_SIZE);
    if (unlikely(err != S_OK)) {
        amp_error("drv_app_init: hAPPMsgQ init failed, err:%8x\n", err);
        return NULL;
    }

    return hAppCtx;
}

VOID drv_app_exit(APP_CTX *hAppCtx)
{
    UINT err;
    err = AMPMsgQ_Destroy(&hAppCtx->hAPPMsgQ);
    if (unlikely(err != S_OK)) {
        amp_error("app MsgQ Destroy: failed, err:%8x\n", err);
        return;
    }
}
