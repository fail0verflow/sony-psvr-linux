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
#include "drv_aout.h"
#include "drv_msg.h"
#include "drv_debug.h"
#include "api_dhub.h"
#include "api_avio_dhub.h"
#include "hal_dhub_wrap.h"
#include "drv_avif.h"
#ifdef CONFIG_MV_AMP_COMPONENT_HWAPP_ENABLE
#include "drv_app.h"
#endif

static INT32 pri_audio_chanId[4] = {
#if ((BERLIN_CHIP_VERSION == BERLIN_BG4_CD) || (BERLIN_CHIP_VERSION == BERLIN_BG4_CT))
    avioDhubChMap_aio64b_MA0_R,
    avioDhubChMap_aio64b_MA1_R,
    avioDhubChMap_aio64b_MA2_R,
    avioDhubChMap_aio64b_MA3_R,
#else
    avioDhubChMap_ag_MA0_R,
    avioDhubChMap_ag_MA1_R,
    avioDhubChMap_ag_MA2_R,
    avioDhubChMap_ag_MA3_R,
#endif
};

static AOUT_CTX aout_ctx;
static VOID *AoutFifoGetKernelRdDMAInfo(AOUT_PATH_CMD_FIFO * p_aout_cmd_fifo,	INT pair)
{
	VOID *pHandle;
	INT rd_offset = p_aout_cmd_fifo->kernel_rd_offset;
	if ((unsigned)rd_offset >= AOUT_PATH_CMD_FIFO_COUNT) {
		INT i = 0, fifo_cmd_size = sizeof(AOUT_PATH_CMD_FIFO) >> 2;
		INT *temp = (INT *)p_aout_cmd_fifo;
		amp_trace("AOUT FIFO memory %p is corrupted! corrupted data :\n",
				  p_aout_cmd_fifo);
		for (i = 0; i < fifo_cmd_size; i++) {
			amp_trace("0x%x\n", *temp++);
		}
		rd_offset = 0;
	}
	pHandle = &(p_aout_cmd_fifo->aout_dma_info[pair][rd_offset]);

	return pHandle;
}

static VOID AoutFifoKernelReset ( AOUT_PATH_CMD_FIFO * p_aout_cmd_fifo )
{
       p_aout_cmd_fifo->kernel_rd_offset = 0;
}
static VOID AoutFifoKernelRdUpdate(AOUT_PATH_CMD_FIFO * p_aout_cmd_fifo, INT adv)
{
	p_aout_cmd_fifo->kernel_rd_offset
		= (adv + p_aout_cmd_fifo->kernel_rd_offset) & (AOUT_PATH_CMD_FIFO_COUNT-1);
}

static INT AoutFifoCheckKernelFullness(AOUT_PATH_CMD_FIFO * p_aout_cmd_fifo)
{
	return (p_aout_cmd_fifo->wr_offset - p_aout_cmd_fifo->kernel_rd_offset) & (AOUT_PATH_CMD_FIFO_COUNT-1);
}

irqreturn_t amp_devices_aout_isr(int irq, void *dev_id)
{
	int instat;
	UNSG32 chanId;
	HDL_semaphore *pSemHandle;
    AOUT_CTX *hAoutCtx = (AOUT_CTX *)dev_id;

	pSemHandle = dhub_semaphore(&AG_dhubHandle.dhub);
	instat = semaphore_chk_full(pSemHandle, -1);
#if ((BERLIN_CHIP_VERSION == BERLIN_BG4_CD) || (BERLIN_CHIP_VERSION == BERLIN_BG4_CT))
	for (chanId = avioDhubChMap_aio64b_MA0_R; chanId <= avioDhubChMap_aio64b_MA3_R; chanId++)
#else
    for (chanId = avioDhubChMap_ag_MA0_R; chanId <= avioDhubChMap_ag_MA3_R; chanId++)
#endif
    {
		if (bTST(instat, chanId)) {
			semaphore_pop(pSemHandle, chanId, 1);
			semaphore_clr_full(pSemHandle, chanId);
#if ((BERLIN_CHIP_VERSION == BERLIN_BG4_CD) || (BERLIN_CHIP_VERSION == BERLIN_BG4_CT))
	        if (chanId == avioDhubChMap_aio64b_MA0_R)
#else
            if (chanId == avioDhubChMap_ag_MA0_R)
#endif
            {
				MV_CC_MSG_t msg = {0, 0, 0};
				aout_resume_cmd(hAoutCtx, MULTI_PATH);
				msg.m_MsgID = 1 << chanId;
				spin_lock(&hAoutCtx->aout_msg_spinlock);
				AMPMsgQ_Add(&hAoutCtx->hAoutMsgQ, &msg);
				spin_unlock(&hAoutCtx->aout_msg_spinlock);
				up(&hAoutCtx->aout_sem);
			}
		}
	}

/* HDMI is part of AG DHUB for BG4 CT/CD */
#if ((BERLIN_CHIP_VERSION == BERLIN_BG4_CD) || (BERLIN_CHIP_VERSION == BERLIN_BG4_CT))
	chanId = avioDhubChMap_aio64b_HDMI_R;
	if (bTST(instat, chanId)) {
	    MV_CC_MSG_t msg = {0, 0, 0};
		semaphore_pop(pSemHandle, chanId, 1);
		semaphore_clr_full(pSemHandle, chanId);
		aout_resume_cmd(hAoutCtx, HDMI_PATH);
		msg.m_MsgID = 1 << chanId;
		spin_lock(&hAoutCtx->aout_msg_spinlock);
		AMPMsgQ_Add(&hAoutCtx->hAoutMsgQ, &msg);
		spin_unlock(&hAoutCtx->aout_msg_spinlock);
		up(&hAoutCtx->aout_sem);
	}
#endif
#ifdef CONFIG_MV_AMP_COMPONENT_AIP_ENABLE
#if (BERLIN_CHIP_VERSION == BERLIN_BG2_DTV_A0)
	chanId = avioDhubChMap_ag_PDM_MIC_ch1;
	if (bTST(instat, chanId)) {
		MV_CC_MSG_t msg = {0 , 0, 0};
		semaphore_pop(pSemHandle, chanId, 1);
		semaphore_clr_full(pSemHandle, chanId);
		aip_resume();
		send_msg2avif(&msg);
	}
#endif
#endif

#ifdef CONFIG_MV_AMP_AUDIO_PATH_20_ENABLE
#if ((BERLIN_CHIP_VERSION == BERLIN_BG4_CT) || (BERLIN_CHIP_VERSION == BERLIN_BG4_CD))
    chanId = avioDhubChMap_aio64b_SA_R;
#else
    chanId = avioDhubChMap_ag_SA_R;
#endif
	if (bTST(instat, chanId)) {
		MV_CC_MSG_t msg = {0, 0, 0};
		semaphore_pop(pSemHandle, chanId, 1);
		semaphore_clr_full(pSemHandle, chanId);
		aout_resume_cmd(hAoutCtx, LoRo_PATH);

		msg.m_MsgID = 1 << chanId;
		spin_lock(&hAoutCtx->aout_msg_spinlock);
		AMPMsgQ_Add(&hAoutCtx->hAoutMsgQ, &msg);
		spin_unlock(&hAoutCtx->aout_msg_spinlock);
		up(&hAoutCtx->aout_sem);
	}
#endif

#ifdef CONFIG_MV_AMP_AUDIO_PATH_SPDIF_ENABLE
#if (BERLIN_CHIP_VERSION == BERLIN_BG4_CT)
    chanId = avioDhubChMap_aio64b_SPDIF_TX_R;
#else
    chanId = avioDhubChMap_ag_SPDIF_R;
#endif
	if (bTST(instat, chanId)) {
		MV_CC_MSG_t msg = {0, 0, 0};
		semaphore_pop(pSemHandle, chanId, 1);
		semaphore_clr_full(pSemHandle, chanId);
		aout_resume_cmd(hAoutCtx, SPDIF_PATH);

		msg.m_MsgID = 1 << chanId;
		spin_lock(&hAoutCtx->aout_msg_spinlock);
		AMPMsgQ_Add(&hAoutCtx->hAoutMsgQ, &msg);
		spin_unlock(&hAoutCtx->aout_msg_spinlock);
		up(&hAoutCtx->aout_sem);
	}
#endif

#ifdef CONFIG_MV_AMP_COMPONENT_HWAPP_ENABLE
	chanId = avioDhubSemMap_ag_app_intr2;
	if (bTST(instat, chanId)) {
		MV_CC_MSG_t msg = {0, 0, 0};

                semaphore_pop(pSemHandle, chanId, 1);
		semaphore_clr_full(pSemHandle, chanId);
		app_resume();

		msg.m_MsgID = (1 << avioDhubSemMap_ag_app_intr2);
		send_msg2app(&msg);
	}
#endif

	return IRQ_HANDLED;
}

VOID send_msg2aout(MV_CC_MSG_t *pMsg)
{
    AOUT_CTX *hAoutCtx = &aout_ctx;

    spin_lock(&hAoutCtx->aout_msg_spinlock);
    AMPMsgQ_Add(&hAoutCtx->hAoutMsgQ, pMsg);
    spin_unlock(&hAoutCtx->aout_msg_spinlock);
    up(&hAoutCtx->aout_sem);
}

VOID aout_resume(INT path_id)
{
    aout_resume_cmd(&aout_ctx, path_id);
}

VOID aout_start_cmd(AOUT_CTX *hAoutCtx, INT *aout_info, VOID *param)
{
	INT *p = aout_info;
	INT i, chanId;
	AOUT_DMA_INFO *p_dma_info;

	if (*p == MULTI_PATH) {
		hAoutCtx->p_ma_fifo = (AOUT_PATH_CMD_FIFO *) param;
		AoutFifoKernelReset ( hAoutCtx->p_ma_fifo );
		for (i = 0; i < 4; i++) {
			p_dma_info =
			    (AOUT_DMA_INFO *)
			    AoutFifoGetKernelRdDMAInfo(hAoutCtx->p_ma_fifo, i);
			chanId = pri_audio_chanId[i];
			dhub_channel_write_cmd(&AG_dhubHandle.dhub, chanId,
					       p_dma_info->addr0,
					       p_dma_info->size0, 0, 0, 0,
					       i == 0 ? 1 : 0, 0, 0);
		}
	}
#ifdef CONFIG_MV_AMP_AUDIO_PATH_20_ENABLE
	else if (*p == LoRo_PATH) {
		hAoutCtx->p_sa_fifo = (AOUT_PATH_CMD_FIFO *) param;
		p_dma_info =
		    (AOUT_DMA_INFO *) AoutFifoGetKernelRdDMAInfo(hAoutCtx->p_sa_fifo, 0);
#if ((BERLIN_CHIP_VERSION == BERLIN_BG4_CT) || (BERLIN_CHIP_VERSION == BERLIN_BG4_CD))
        chanId = avioDhubChMap_aio64b_SA_R;
#else
        chanId = avioDhubChMap_ag_SA_R;
#endif
		dhub_channel_write_cmd(&AG_dhubHandle.dhub, chanId,
				       p_dma_info->addr0, p_dma_info->size0, 0,
				       0, 0, 1, 0, 0);
	}
#endif
#ifdef CONFIG_MV_AMP_AUDIO_PATH_SPDIF_ENABLE
	else if (*p == SPDIF_PATH) {
		hAoutCtx->p_spdif_fifo = (AOUT_PATH_CMD_FIFO *) param;
		p_dma_info =
		    (AOUT_DMA_INFO *) AoutFifoGetKernelRdDMAInfo(hAoutCtx->p_spdif_fifo,
								 0);
#if (BERLIN_CHIP_VERSION == BERLIN_BG4_CT)
		chanId = avioDhubChMap_aio64b_SPDIF_TX_R;
#else
		chanId = avioDhubChMap_ag_SPDIF_R;
#endif
		dhub_channel_write_cmd(&AG_dhubHandle.dhub, chanId,
				       p_dma_info->addr0, p_dma_info->size0, 0,
				       0, 0, 1, 0, 0);
	}
#endif
#ifdef CONFIG_MV_AMP_AUDIO_PATH_HDMI_ENABLE
	else if (*p == HDMI_PATH) {
		hAoutCtx->p_hdmi_fifo = (AOUT_PATH_CMD_FIFO *) param;
		AoutFifoKernelReset ( hAoutCtx->p_hdmi_fifo );
		p_dma_info =
		    (AOUT_DMA_INFO *) AoutFifoGetKernelRdDMAInfo(hAoutCtx->p_hdmi_fifo,
								 0);
#if ((BERLIN_CHIP_VERSION == BERLIN_BG4_CT) || (BERLIN_CHIP_VERSION == BERLIN_BG4_CD))
        chanId = avioDhubChMap_aio64b_HDMI_R;
		wrap_dhub_channel_write_cmd(&AG_dhubHandle.dhub, chanId,
				       p_dma_info->addr0, p_dma_info->size0, 0,
				       0, 0, 1, 0, 0);
#else
        chanId = avioDhubChMap_vpp_HDMI_R;
		wrap_dhub_channel_write_cmd(&VPP_dhubHandle.dhub, chanId,
				       p_dma_info->addr0, p_dma_info->size0, 0,
				       0, 0, 1, 0, 0);
#endif
	}
#endif
}

VOID aout_stop_cmd(AOUT_CTX *hAoutCtx, INT path_id)
{
	if (path_id == MULTI_PATH) {
		hAoutCtx->p_ma_fifo = NULL;
	}
#ifdef CONFIG_MV_AMP_AUDIO_PATH_20_ENABLE
	else if (path_id == LoRo_PATH) {
		hAoutCtx->p_sa_fifo = NULL;
	}
#endif
#ifdef CONFIG_MV_AMP_AUDIO_PATH_SPDIF_ENABLE
	else if (path_id == SPDIF_PATH) {
		hAoutCtx->p_spdif_fifo = NULL;
	}
#endif
#ifdef CONFIG_MV_AMP_AUDIO_PATH_HDMI_ENABLE
	else if (path_id == HDMI_PATH) {
		hAoutCtx->p_hdmi_fifo = NULL;
	}
#endif
}

VOID aout_resume_cmd(AOUT_CTX *hAoutCtx, INT path_id)
{
	AOUT_DMA_INFO *p_dma_info;
	UINT i, chanId;
	int kernel_full;

	if (path_id >= MAX_OUTPUT_AUDIO) {
		return;
	}

	if (!hAoutCtx) {
		amp_error("hAoutCtx == NULL!\n");
	}

	hAoutCtx->aout_int_cnt[path_id]++;
	if (path_id == MULTI_PATH) {
		AOUT_PATH_CMD_FIFO *p_ma_fifo = hAoutCtx->p_ma_fifo;
		if (!p_ma_fifo) return;

		if (!p_ma_fifo->fifo_underflow){
			AoutFifoKernelRdUpdate(p_ma_fifo, 1);
		}
		kernel_full = AoutFifoCheckKernelFullness(p_ma_fifo);
		if (kernel_full) {
			p_ma_fifo->fifo_underflow = 0;
			for (i = 0; i < 4; i++) {
				p_dma_info =
				    (AOUT_DMA_INFO *)
				    AoutFifoGetKernelRdDMAInfo(p_ma_fifo, i);
				chanId = pri_audio_chanId[i];
				dhub_channel_write_cmd(&AG_dhubHandle.dhub,
						       chanId,
						       p_dma_info->addr0,
						       p_dma_info->size0, 0, 0,
						       0,
						       (p_dma_info->size1 == 0
							&& i == 0) ? 1 : 0, 0,
						       0);
				if (p_dma_info->size1)
					dhub_channel_write_cmd(&AG_dhubHandle.dhub,
					               chanId,
							       p_dma_info->addr1,
							       p_dma_info->size1, 0, 0, 0,
							       (i == 0) ? 1 : 0,
							       0, 0);
			}
		} else {
			p_ma_fifo->fifo_underflow = 1;
			for (i = 0; i < 4; i++) {
				chanId = pri_audio_chanId[i];
				dhub_channel_write_cmd(&AG_dhubHandle.dhub,
						       chanId,
						       p_ma_fifo->zero_buffer,
						       p_ma_fifo->zero_buffer_size, 0, 0,
						       0, i == 0 ? 1 : 0, 0, 0);
			}
		}
	}
#ifdef CONFIG_MV_AMP_AUDIO_PATH_20_ENABLE
	else if (path_id == LoRo_PATH) {
		AOUT_PATH_CMD_FIFO *p_sa_fifo = hAoutCtx->p_sa_fifo;
		if (!p_sa_fifo) return;

		if (!p_sa_fifo->fifo_underflow)
			AoutFifoKernelRdUpdate(p_sa_fifo, 1);

		if (AoutFifoCheckKernelFullness(p_sa_fifo)) {
			p_sa_fifo->fifo_underflow = 0;
			p_dma_info =
			    (AOUT_DMA_INFO *)
			    AoutFifoGetKernelRdDMAInfo(p_sa_fifo, 0);
#if ((BERLIN_CHIP_VERSION == BERLIN_BG4_CT) || (BERLIN_CHIP_VERSION == BERLIN_BG4_CD))
            chanId = avioDhubChMap_aio64b_SA_R;
#else
            chanId = avioDhubChMap_ag_SA_R;
#endif
			dhub_channel_write_cmd(&AG_dhubHandle.dhub, chanId,
					       p_dma_info->addr0,
					       p_dma_info->size0, 0, 0, 0,
					       p_dma_info->size1 ? 0 : 1, 0, 0);
			if (p_dma_info->size1)
				dhub_channel_write_cmd(&AG_dhubHandle.dhub,
						       chanId,
						       p_dma_info->addr1,
						       p_dma_info->size1, 0, 0,
						       0, 1, 0, 0);
		} else {
			p_sa_fifo->fifo_underflow = 1;
#if ((BERLIN_CHIP_VERSION == BERLIN_BG4_CT) || (BERLIN_CHIP_VERSION == BERLIN_BG4_CD))
            chanId = avioDhubChMap_aio64b_SA_R;
#else
            chanId = avioDhubChMap_ag_SA_R;
#endif
			dhub_channel_write_cmd(&AG_dhubHandle.dhub, chanId,
					       p_sa_fifo->zero_buffer,
					       p_sa_fifo->zero_buffer_size, 0,
					       0, 0, 1, 0, 0);
		}
	}
#endif
#ifdef CONFIG_MV_AMP_AUDIO_PATH_SPDIF_ENABLE
	else if (path_id == SPDIF_PATH) {
		AOUT_PATH_CMD_FIFO *p_spdif_fifo = hAoutCtx->p_spdif_fifo;

		if ((!p_spdif_fifo) || (!hAoutCtx->p_arc_fifo)) return;
	    memcpy(hAoutCtx->p_arc_fifo, p_spdif_fifo, sizeof(AOUT_PATH_CMD_FIFO));
		if (!p_spdif_fifo->fifo_underflow)
			AoutFifoKernelRdUpdate(p_spdif_fifo, 1);

		if (AoutFifoCheckKernelFullness(p_spdif_fifo)) {
			p_spdif_fifo->fifo_underflow = 0;
			p_dma_info =
			    (AOUT_DMA_INFO *)
			    AoutFifoGetKernelRdDMAInfo(p_spdif_fifo, 0);
#if (BERLIN_CHIP_VERSION == BERLIN_BG4_CT)
            chanId = avioDhubChMap_aio64b_SPDIF_TX_R;
#else
            chanId = avioDhubChMap_ag_SPDIF_R;
#endif
			dhub_channel_write_cmd(&AG_dhubHandle.dhub, chanId,
					       p_dma_info->addr0,
					       p_dma_info->size0, 0, 0, 0,
					       p_dma_info->size1 ? 0 : 1, 0, 0);
			if (p_dma_info->size1)
				dhub_channel_write_cmd(&AG_dhubHandle.dhub,
						       chanId,
						       p_dma_info->addr1,
						       p_dma_info->size1, 0, 0,
						       0, 1, 0, 0);
		} else {
			p_spdif_fifo->fifo_underflow = 1;
#if (BERLIN_CHIP_VERSION == BERLIN_BG4_CT)
            chanId = avioDhubChMap_aio64b_SPDIF_TX_R;
#else
            chanId = avioDhubChMap_ag_SPDIF_R;
#endif
			dhub_channel_write_cmd(&AG_dhubHandle.dhub, chanId,
					       p_spdif_fifo->zero_buffer,
					       p_spdif_fifo->zero_buffer_size,
					       0, 0, 0, 1, 0, 0);
		}
	}
#endif
#ifdef CONFIG_MV_AMP_AUDIO_PATH_HDMI_ENABLE

	else if (path_id == HDMI_PATH) {
		AOUT_PATH_CMD_FIFO *p_hdmi_fifo = hAoutCtx->p_hdmi_fifo;
		if (!p_hdmi_fifo) return;

		if (!p_hdmi_fifo->fifo_underflow)
			AoutFifoKernelRdUpdate(p_hdmi_fifo, 1);

		if (AoutFifoCheckKernelFullness(p_hdmi_fifo)) {
			p_hdmi_fifo->fifo_underflow = 0;
			p_dma_info =
			    (AOUT_DMA_INFO *)
			    AoutFifoGetKernelRdDMAInfo(p_hdmi_fifo, 0);

#if ((BERLIN_CHIP_VERSION == BERLIN_BG4_CT) || (BERLIN_CHIP_VERSION == BERLIN_BG4_CD))
            chanId = avioDhubChMap_aio64b_HDMI_R;
			wrap_dhub_channel_write_cmd(&AG_dhubHandle.dhub, chanId,
					       p_dma_info->addr0,
					       p_dma_info->size0, 0, 0, 0,
					       p_dma_info->size1 ? 0 : 1, 0, 0);
			if (p_dma_info->size1)
				wrap_dhub_channel_write_cmd(&AG_dhubHandle.dhub,
						       chanId,
						       p_dma_info->addr1,
						       p_dma_info->size1, 0, 0,
						       0, 1, 0, 0);
#else
            chanId = avioDhubChMap_vpp_HDMI_R;
			wrap_dhub_channel_write_cmd(&VPP_dhubHandle.dhub, chanId,
					       p_dma_info->addr0,
					       p_dma_info->size0, 0, 0, 0,
					       p_dma_info->size1 ? 0 : 1, 0, 0);
			if (p_dma_info->size1)
				wrap_dhub_channel_write_cmd(&VPP_dhubHandle.dhub,
						       chanId,
						       p_dma_info->addr1,
						       p_dma_info->size1, 0, 0,
						       0, 1, 0, 0);
#endif
		} else {
			p_hdmi_fifo->fifo_underflow = 1;
#if ((BERLIN_CHIP_VERSION == BERLIN_BG4_CT) || (BERLIN_CHIP_VERSION == BERLIN_BG4_CD))
            chanId = avioDhubChMap_aio64b_HDMI_R;
			wrap_dhub_channel_write_cmd(&AG_dhubHandle.dhub, chanId,
					       p_hdmi_fifo->zero_buffer,
					       p_hdmi_fifo->zero_buffer_size, 0,
					       0, 0, 1, 0, 0);
#else
            chanId = avioDhubChMap_vpp_HDMI_R;
			wrap_dhub_channel_write_cmd(&VPP_dhubHandle.dhub, chanId,
					       p_hdmi_fifo->zero_buffer,
					       p_hdmi_fifo->zero_buffer_size, 0,
					       0, 0, 1, 0, 0);
#endif
		}
	}
#endif
}

AOUT_CTX * drv_aout_init(VOID)
{
    AOUT_CTX *hAoutCtx = &aout_ctx;
    UINT err;

    memset(hAoutCtx, 0, sizeof(AOUT_CTX));

    spin_lock_init(&hAoutCtx->aout_spinlock);
    spin_lock_init(&hAoutCtx->aout_msg_spinlock);

    sema_init(&hAoutCtx->aout_sem, 0);
    err = AMPMsgQ_Init(&hAoutCtx->hAoutMsgQ, AOUT_ISR_MSGQ_SIZE);
    if (unlikely(err != S_OK)) {
        amp_error("hAoutMsgQ init: failed, err:%8x\n", err);
        return NULL;
    }

    return hAoutCtx;
}

VOID drv_aout_exit(AOUT_CTX *hAoutCtx)
{
    UINT err;

    err = AMPMsgQ_Destroy(&hAoutCtx->hAoutMsgQ);
    if (unlikely(err != S_OK)) {
        amp_trace("aout MsgQ Destroy: failed, err:%8x\n", err);
    }
}
