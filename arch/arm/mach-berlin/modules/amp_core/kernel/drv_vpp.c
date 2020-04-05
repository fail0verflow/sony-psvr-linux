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
#include "api_dhub.h"
#include "avioDhub.h"
#include "api_avio_dhub.h"
#include "hal_dhub_wrap.h"

#include "amp_type.h"
#include "amp_ioctl.h"
#include "drv_vpp.h"
#include "drv_msg.h"
#include "drv_debug.h"
#include "drv_avif.h"
#include "drv_aout.h"

static VPP_CTX vpp_ctx;
#if ((BERLIN_CHIP_VERSION == BERLIN_BG4_CD) || (BERLIN_CHIP_VERSION == BERLIN_BG4_CT))
/*FIXME: VPP_CT changes - need correct mapping*/
#define avioDhubSemMap_vpp_vppCPCB0_intr avioDhubSemMap_vpp128b_vpp_inr0
#define avioDhubSemMap_vpp_vppOUT4_intr avioDhubSemMap_vpp128b_vpp_inr6
//#define BERLIN_BG2_BCM_SCHE_IN_KERNEL
#endif

static VOID start_vbi_bcm_transaction(VPP_CTX *hVppCtx, INT cpcbID)
{
#if ((BERLIN_CHIP_VERSION != BERLIN_BG4_CD) && (BERLIN_CHIP_VERSION != BERLIN_BG4_CT))
    UINT bcm_sched_cmd[2];
    if (hVppCtx->vbi_bcm_info[cpcbID].DmaLen) {
    wrap_dhub_channel_generate_cmd(&(VPP_dhubHandle.dhub),
                  avioDhubChMap_vpp_BCM_R,
                  (INT) hVppCtx->vbi_bcm_info[cpcbID].DmaAddr,
                  (INT) hVppCtx->vbi_bcm_info[cpcbID].DmaLen * 8,
                  0, 0, 0, 1, bcm_sched_cmd);
    while (!BCM_SCHED_PushCmd(BCM_SCHED_Q12, bcm_sched_cmd, NULL)) {
        hVppCtx->vbi_bcm_cmd_fullcnt++;
    }
    }
    /* invalid vbi_bcm_info */
    hVppCtx->vbi_bcm_info[cpcbID].DmaLen = 0;
#endif
}

static VOID start_vde_bcm_transaction(VPP_CTX *hVppCtx, INT cpcbID)
{
#if ((BERLIN_CHIP_VERSION != BERLIN_BG4_CD) && (BERLIN_CHIP_VERSION != BERLIN_BG4_CT))
    UINT bcm_sched_cmd[2];

    if (hVppCtx->vde_bcm_info[cpcbID].DmaLen) {
    wrap_dhub_channel_generate_cmd(&(VPP_dhubHandle.dhub),
                  avioDhubChMap_vpp_BCM_R,
                  (INT) hVppCtx->vde_bcm_info[cpcbID].DmaAddr,
                  (INT) hVppCtx->vde_bcm_info[cpcbID].DmaLen * 8,
                  0, 0, 0, 1, bcm_sched_cmd);
    while (!BCM_SCHED_PushCmd(BCM_SCHED_Q12, bcm_sched_cmd, NULL)) {
        hVppCtx->vde_bcm_cmd_fullcnt++;
    }
    }
    /* invalid vde_bcm_info */
    hVppCtx->vde_bcm_info[cpcbID].DmaLen = 0;
#endif
}

static VOID start_vbi_dma_transaction(VPP_CTX *hVppCtx, INT cpcbID)
{
    UINT32 cnt;
    UINT32 *ptr;

    ptr = (UINT32 *) hVppCtx->dma_info[cpcbID].DmaAddr;
    for (cnt = 0; cnt < hVppCtx->dma_info[cpcbID].DmaLen; cnt++) {
    *((volatile INT *)*(ptr + 1)) = *ptr;
    ptr += 2;
    }
    /* invalid dma_info */
    hVppCtx->dma_info[cpcbID].DmaLen = 0;
}

static VOID send_bcm_sche_cmd(VPP_CTX *hVppCtx, INT q_id)
{
    if ((hVppCtx->bcm_sche_cmd[q_id][0] != 0) && (hVppCtx->bcm_sche_cmd[q_id][1] != 0))
    BCM_SCHED_PushCmd(q_id, hVppCtx->bcm_sche_cmd[q_id], NULL);
}

irqreturn_t amp_devices_vpp_cec_isr(INT irq, VOID *dev_id)
{
    UINT16 value = 0;
    UINT16 reg = 0;
    INT intr;
    INT i;
    INT dptr_len = 0;
    HRESULT ret = S_OK;
	VPP_CTX *hVppCtx = (VPP_CTX *)dev_id;

    GA_REG_WORD32_READ(0xf7e82C00 + 0x04 + 7 * 0x14, &hVppCtx->vpp_intr_timestamp);
    hVppCtx->vpp_intr_timestamp = DEBUG_TIMER_VALUE - hVppCtx->vpp_intr_timestamp;

    // Read CEC status register
    GA_REG_BYTE_READ(SOC_SM_CEC_BASE +
         (CEC_INTR_STATUS0_REG_ADDR << 2), &value);
    reg = (UINT16) value;
    GA_REG_BYTE_READ(SOC_SM_CEC_BASE +
         (CEC_INTR_STATUS1_REG_ADDR << 2), &value);
    reg |= ((UINT16) value << 8);
    // Clear CEC interrupt
    if (reg & BE_CEC_INTR_TX_FAIL) {
    intr = BE_CEC_INTR_TX_FAIL;
    GA_REG_BYTE_WRITE(SOC_SM_CEC_BASE + (CEC_RDY_ADDR << 2), 0x0);
    GA_REG_BYTE_READ(SOC_SM_CEC_BASE +
             (CEC_INTR_ENABLE0_REG_ADDR << 2), &value);
    value &= ~(intr & 0x00ff);
    GA_REG_BYTE_WRITE(SOC_SM_CEC_BASE +
              (CEC_INTR_ENABLE0_REG_ADDR << 2), value);
    }
    if (reg & BE_CEC_INTR_TX_COMPLETE) {
    intr = BE_CEC_INTR_TX_COMPLETE;
    GA_REG_BYTE_READ(SOC_SM_CEC_BASE +
             (CEC_INTR_ENABLE0_REG_ADDR << 2), &value);
    value &= ~(intr & 0x00ff);
    GA_REG_BYTE_WRITE(SOC_SM_CEC_BASE +
              (CEC_INTR_ENABLE0_REG_ADDR << 2), value);
    }
    if (reg & BE_CEC_INTR_RX_FAIL) {
    intr = BE_CEC_INTR_RX_FAIL;
    GA_REG_BYTE_READ(SOC_SM_CEC_BASE +
             (CEC_INTR_ENABLE0_REG_ADDR << 2), &value);
    value &= ~(intr & 0x00ff);
    GA_REG_BYTE_WRITE(SOC_SM_CEC_BASE +
              (CEC_INTR_ENABLE0_REG_ADDR << 2), value);
    }
    if (reg & BE_CEC_INTR_RX_COMPLETE) {
    intr = BE_CEC_INTR_RX_COMPLETE;
    GA_REG_BYTE_READ(SOC_SM_CEC_BASE +
             (CEC_INTR_ENABLE0_REG_ADDR << 2), &value);
    value &= ~(intr & 0x00ff);
    GA_REG_BYTE_WRITE(SOC_SM_CEC_BASE +
              (CEC_INTR_ENABLE0_REG_ADDR << 2), value);
    // read cec mesg from rx buffer
    GA_REG_BYTE_READ(SOC_SM_CEC_BASE + (CEC_RX_FIFO_DPTR << 2),
             &dptr_len);
    hVppCtx->rx_buf.len = dptr_len;
    for (i = 0; i < dptr_len; i++) {
        GA_REG_BYTE_READ(SOC_SM_CEC_BASE +
                 (CEC_RX_BUF_READ_REG_ADDR << 2),
                 &hVppCtx->rx_buf.buf[i]);
        GA_REG_BYTE_WRITE(SOC_SM_CEC_BASE +
                  (CEC_TOGGLE_FOR_READ_REG_ADDR << 2),
                  0x01);
    }
    GA_REG_BYTE_WRITE(SOC_SM_CEC_BASE + (CEC_RX_RDY_ADDR << 2),
              0x00);
    GA_REG_BYTE_WRITE(SOC_SM_CEC_BASE + (CEC_RX_RDY_ADDR << 2),
              0x01);
    GA_REG_BYTE_READ(SOC_SM_CEC_BASE +
             (CEC_INTR_ENABLE0_REG_ADDR << 2), &value);
    value |= (intr & 0x00ff);
    GA_REG_BYTE_WRITE(SOC_SM_CEC_BASE +
              (CEC_INTR_ENABLE0_REG_ADDR << 2), value);
    }

    // schedule tasklet with intr status as param
    {
		unsigned long flag;
        MV_CC_MSG_t msg =
            { VPP_CC_MSG_TYPE_CEC, reg, hVppCtx->vpp_intr_timestamp };
        spin_lock_irqsave(&hVppCtx->vpp_msg_spinlock, flag);
        ret = AMPMsgQ_Add(&hVppCtx->hVPPMsgQ, &msg);
        spin_unlock_irqrestore(&hVppCtx->vpp_msg_spinlock, flag);
        if (ret != S_OK) {
            return IRQ_HANDLED;
        }
    }
    up(&hVppCtx->vpp_sem);

    return IRQ_HANDLED;
}

irqreturn_t amp_devices_vpp_isr(INT irq, VOID *dev_id)
{
	INT instat;
	HDL_semaphore *pSemHandle;
	INT vpp_intr = 0;
	INT instat_used = 0;
	INT chanid;
	HRESULT ret = S_OK;
	VPP_CTX *hVppCtx = (VPP_CTX *)dev_id;

#ifdef MORPHEUS_TV_VIEW_MODE
	u64  clock_ns = local_clock();
#endif

	hVppCtx->vpp_cpcb0_vbi_int_cnt++;
#if ((BERLIN_CHIP_VERSION != BERLIN_BG4_CD) && (BERLIN_CHIP_VERSION != BERLIN_BG4_CT))
	/* disable interrupt */
	GA_REG_WORD32_READ(0xf7e82C00 + 0x04 + 7 * 0x14, &hVppCtx->vpp_intr_timestamp);
	hVppCtx->vpp_intr_timestamp = DEBUG_TIMER_VALUE - hVppCtx->vpp_intr_timestamp;
#endif

#ifdef CONFIG_IRQ_LATENCY_PROFILE
	hVppCtx->amp_irq_profiler.vpp_isr_start = cpu_clock(smp_processor_id());
#endif

	/* VPP interrupt handling  */
	pSemHandle = wrap_dhub_semaphore(&VPP_dhubHandle.dhub);
	instat = wrap_semaphore_chk_full(pSemHandle, -1);

	if (bTST(instat, avioDhubSemMap_vpp_vppCPCB0_intr)
	    && (hVppCtx->vpp_intr_status[avioDhubSemMap_vpp_vppCPCB0_intr])
	    ) {
#ifdef MORPHEUS_TV_VIEW_MODE
		extern int video_mode;
		extern long long vpp_isr_count;
		extern u64 cur_vpp_intr_timestamp;
		extern int vpp_res_set;
        extern int vip_stable_isr;
        extern int vpp_4k_res;
		if (video_mode == MODE_TV_VIEW) {
			//in/out timeline
			//amp_trace("vpp isr start vpp_isr_count %lld, vip_isr_count %lld\n", vpp_isr_count, vip_isr_count);
			//vpp_isr_count++;
			//amp_trace("vpp isr clock_ns %lld\n", clock_ns);
            //cur_vpp_intr_timestamp = clock_ns;
			//amp_trace("vpp isr end vpp_isr_count %lld, vip_isr_count %lld\n",
			//        vpp_isr_count, vip_isr_count);
			if(vpp_res_set == 1 && vip_stable_isr == 1 && vpp_4k_res == -1)
			{
				vpp_isr_count++;
				cur_vpp_intr_timestamp = clock_ns;

				//adjust VIP/VPP latency
				adjust_inout_latency();
			}
		}
#endif

		vpp_intr |= bSETMASK(avioDhubSemMap_vpp_vppCPCB0_intr);
		bSET(instat_used, avioDhubSemMap_vpp_vppCPCB0_intr);

#ifdef CONFIG_IRQ_LATENCY_PROFILE
		hVppCtx->amp_irq_profiler.vppCPCB0_intr_curr =
		    cpu_clock(smp_processor_id());
#endif
		/* CPCB-0 interrupt */
		/* clear interrupt */
		wrap_semaphore_pop(pSemHandle, avioDhubSemMap_vpp_vppCPCB0_intr, 1);
		wrap_semaphore_clr_full(pSemHandle,
				   avioDhubSemMap_vpp_vppCPCB0_intr);

		/* Clear the bits for CPCB0 VDE interrupt */
		if (bTST(instat, avioDhubSemMap_vpp_vppOUT4_intr)) {
			wrap_semaphore_pop(pSemHandle,
				      avioDhubSemMap_vpp_vppOUT4_intr, 1);
			wrap_semaphore_clr_full(pSemHandle,
					   avioDhubSemMap_vpp_vppOUT4_intr);
			bCLR(instat, avioDhubSemMap_vpp_vppOUT4_intr);
		}

		start_vbi_dma_transaction(hVppCtx, 0);
		start_vbi_bcm_transaction(hVppCtx, 0);
		send_bcm_sche_cmd(hVppCtx, BCM_SCHED_Q0);
	}

#if ((BERLIN_CHIP_VERSION != BERLIN_BG4_CD) && (BERLIN_CHIP_VERSION != BERLIN_BG4_CT))
#if (BERLIN_CHIP_VERSION < BERLIN_BG2_DTV_A0)
	if (bTST(instat, avioDhubSemMap_vpp_vppCPCB1_intr)
	    && (hVppCtx->vpp_intr_status[avioDhubSemMap_vpp_vppCPCB1_intr])
	    ) {
		vpp_intr |= bSETMASK(avioDhubSemMap_vpp_vppCPCB1_intr);
		bSET(instat_used, avioDhubSemMap_vpp_vppCPCB1_intr);

		/* CPCB-1 interrupt */
		/* clear interrupt */
		wrap_semaphore_pop(pSemHandle, avioDhubSemMap_vpp_vppCPCB1_intr, 1);
		wrap_semaphore_clr_full(pSemHandle,
				   avioDhubSemMap_vpp_vppCPCB1_intr);

		/* Clear the bits for CPCB1 VDE interrupt */
		if (bTST(instat, avioDhubSemMap_vpp_vppOUT5_intr)) {
			wrap_semaphore_pop(pSemHandle,
				      avioDhubSemMap_vpp_vppOUT5_intr, 1);
			wrap_semaphore_clr_full(pSemHandle,
					   avioDhubSemMap_vpp_vppOUT5_intr);
			bCLR(instat, avioDhubSemMap_vpp_vppOUT5_intr);
		}
		start_vbi_dma_transaction(hVppCtx, 1);
		start_vbi_bcm_transaction(hVppCtx, 1);
		send_bcm_sche_cmd(hVppCtx, BCM_SCHED_Q1);
	}
#endif
	if (bTST(instat, avioDhubSemMap_vpp_vppCPCB2_intr)
	    && (hVppCtx->vpp_intr_status[avioDhubSemMap_vpp_vppCPCB2_intr])
	    ) {
		vpp_intr |= bSETMASK(avioDhubSemMap_vpp_vppCPCB2_intr);
		bSET(instat_used, avioDhubSemMap_vpp_vppCPCB2_intr);

		/* CPCB-2 interrupt */
		/* clear interrupt */
		wrap_semaphore_pop(pSemHandle, avioDhubSemMap_vpp_vppCPCB2_intr, 1);
		wrap_semaphore_clr_full(pSemHandle,
				   avioDhubSemMap_vpp_vppCPCB2_intr);

		/* Clear the bits for CPCB2 VDE interrupt */
		if (bTST(instat, avioDhubSemMap_vpp_vppOUT6_intr)) {
			wrap_semaphore_pop(pSemHandle,
				      avioDhubSemMap_vpp_vppOUT6_intr, 1);
			wrap_semaphore_clr_full(pSemHandle,
					   avioDhubSemMap_vpp_vppOUT6_intr);
			bCLR(instat, avioDhubSemMap_vpp_vppOUT6_intr);
		}

		start_vbi_dma_transaction(hVppCtx, 2);
		start_vbi_bcm_transaction(hVppCtx, 2);
		send_bcm_sche_cmd(hVppCtx, BCM_SCHED_Q2);
	}
#if (BERLIN_CHIP_VERSION == BERLIN_BG2_DTV_A0)
        //Reuse CBCP1 for 4KTG CPCB interrupt
        if (bTST(instat, avioDhubSemMap_vpp_vpp4Ktg_intr)
            && (hVppCtx->vpp_intr_status[avioDhubSemMap_vpp_vpp4Ktg_intr])){
#ifdef MORPHEUS_TV_VIEW_MODE
			extern int video_mode;
			extern long long vpp_isr_count;
			extern u64 cur_vpp_intr_timestamp;
			extern int vpp_res_set;
			extern int vip_stable_isr;
			extern int vpp_4k_res;
			if (video_mode == MODE_TV_VIEW) {
				//in/out timeline
				//amp_trace("vpp isr start vpp_isr_count %lld, vip_isr_count %lld\n", vpp_isr_count, vip_isr_count);
				//vpp_isr_count++;
				//amp_trace("vpp isr clock_ns %lld\n", clock_ns);
				//cur_vpp_intr_timestamp = clock_ns;
				//amp_trace("vpp isr end vpp_isr_count %lld, vip_isr_count %lld\n",
				//        vpp_isr_count, vip_isr_count);

				if(vpp_res_set == 1 && vip_stable_isr == 1 && vpp_4k_res != -1)
				{
					vpp_isr_count++;
					cur_vpp_intr_timestamp = clock_ns;
					//adjust VIP/VPP latency
					adjust_inout_latency();
				}
			}
#endif

                vpp_intr |= bSETMASK(avioDhubSemMap_vpp_vpp4Ktg_intr);
                bSET(instat_used, avioDhubSemMap_vpp_vpp4Ktg_intr);
                wrap_semaphore_pop(pSemHandle, avioDhubSemMap_vpp_vpp4Ktg_intr, 1);
                wrap_semaphore_clr_full(pSemHandle,
                                   avioDhubSemMap_vpp_vpp4Ktg_intr);
		start_vbi_dma_transaction(hVppCtx, 1);
		start_vbi_bcm_transaction(hVppCtx, 1);
		send_bcm_sche_cmd(hVppCtx, BCM_SCHED_Q1);
        }
#endif
#endif
	if (bTST(instat, avioDhubSemMap_vpp_vppOUT4_intr)
	    && (hVppCtx->vpp_intr_status[avioDhubSemMap_vpp_vppOUT4_intr])
	    ) {
		vpp_intr |= bSETMASK(avioDhubSemMap_vpp_vppOUT4_intr);
		bSET(instat_used, avioDhubSemMap_vpp_vppOUT4_intr);

		/* CPCB-0 VDE interrupt */
		/* clear interrupt */
		wrap_semaphore_pop(pSemHandle, avioDhubSemMap_vpp_vppOUT4_intr, 1);
		wrap_semaphore_clr_full(pSemHandle, avioDhubSemMap_vpp_vppOUT4_intr);

		start_vde_bcm_transaction(hVppCtx, 0);
		send_bcm_sche_cmd(hVppCtx, BCM_SCHED_Q3);
	}

#if ((BERLIN_CHIP_VERSION != BERLIN_BG4_CD) && (BERLIN_CHIP_VERSION != BERLIN_BG4_CT))
	if (bTST(instat, avioDhubSemMap_vpp_vppOUT5_intr)
	    && (hVppCtx->vpp_intr_status[avioDhubSemMap_vpp_vppOUT5_intr])
	    ) {
		vpp_intr |= bSETMASK(avioDhubSemMap_vpp_vppOUT5_intr);
		bSET(instat_used, avioDhubSemMap_vpp_vppOUT5_intr);

		/* CPCB-1 VDE interrupt */
		/* clear interrupt */
		wrap_semaphore_pop(pSemHandle, avioDhubSemMap_vpp_vppOUT5_intr, 1);
		wrap_semaphore_clr_full(pSemHandle, avioDhubSemMap_vpp_vppOUT5_intr);

		start_vde_bcm_transaction(hVppCtx, 1);
		send_bcm_sche_cmd(hVppCtx, BCM_SCHED_Q4);
	}

	if (bTST(instat, avioDhubSemMap_vpp_vppOUT6_intr)
	    && (hVppCtx->vpp_intr_status[avioDhubSemMap_vpp_vppOUT6_intr])
	    ) {
		vpp_intr |= bSETMASK(avioDhubSemMap_vpp_vppOUT6_intr);
		bSET(instat_used, avioDhubSemMap_vpp_vppOUT6_intr);

		/* CPCB-2 VDE interrupt */
		/* clear interrupt */
		wrap_semaphore_pop(pSemHandle, avioDhubSemMap_vpp_vppOUT6_intr, 1);
		wrap_semaphore_clr_full(pSemHandle, avioDhubSemMap_vpp_vppOUT6_intr);

		start_vde_bcm_transaction(hVppCtx, 2);
		send_bcm_sche_cmd(hVppCtx, BCM_SCHED_Q5);
	}

	if (bTST(instat, avioDhubSemMap_vpp_vppOUT3_intr)
	    && (hVppCtx->vpp_intr_status[avioDhubSemMap_vpp_vppOUT3_intr])
	    ) {
		vpp_intr |= bSETMASK(avioDhubSemMap_vpp_vppOUT3_intr);
		bSET(instat_used, avioDhubSemMap_vpp_vppOUT3_intr);

		/* VOUT3 interrupt */
		/* clear interrupt */
		wrap_semaphore_pop(pSemHandle, avioDhubSemMap_vpp_vppOUT3_intr, 1);
		wrap_semaphore_clr_full(pSemHandle, avioDhubSemMap_vpp_vppOUT3_intr);
	}

#ifdef CONFIG_MV_AMP_AUDIO_PATH_HDMI_ENABLE
// @yeliu: TODO: use DT to get avio channel number

#if (BERLIN_CHIP_VERSION == BERLIN_BG3_CD)
  #if (BERLIN_CHIP_VERSION_EXT >= SOC_MINOR_REV_A0)
	chanid = avioDhubSemMap_vpp_CH4_intr;
  #else
        chanid = avioDhubSemMap_vpp_CH7_intr;
  #endif
#else
	chanid = avioDhubSemMap_vpp_CH10_intr;
#endif

	if (bTST(instat, chanid)
		&& (hVppCtx->vpp_intr_status[chanid])
	) {
	    MV_CC_MSG_t msg = {0, 0, 0};
		bSET(instat_used, chanid);
		/* HDMI audio interrupt */
		/* clear interrupt */
		wrap_semaphore_pop(pSemHandle, chanid, 1);
		wrap_semaphore_clr_full(pSemHandle, chanid);
		aout_resume(HDMI_PATH);

		msg.m_MsgID = 1 << chanid;
		send_msg2aout(&msg);
		bCLR(instat, chanid);
	}
#endif

	if (bTST(instat, avioDhubSemMap_vpp_vppOUT0_intr)
	    && (hVppCtx->vpp_intr_status[avioDhubSemMap_vpp_vppOUT0_intr])
	    ) {
		vpp_intr |= bSETMASK(avioDhubSemMap_vpp_vppOUT0_intr);
		bSET(instat_used, avioDhubSemMap_vpp_vppOUT0_intr);

		/* VOUT0 interrupt */
		/* clear interrupt */
		wrap_semaphore_pop(pSemHandle, avioDhubSemMap_vpp_vppOUT0_intr, 1);
		wrap_semaphore_clr_full(pSemHandle, avioDhubSemMap_vpp_vppOUT0_intr);
	}

	if (bTST(instat, avioDhubSemMap_vpp_vppOUT1_intr)
	    && (hVppCtx->vpp_intr_status[avioDhubSemMap_vpp_vppOUT1_intr])
	    ) {
		vpp_intr |= bSETMASK(avioDhubSemMap_vpp_vppOUT1_intr);
		bSET(instat_used, avioDhubSemMap_vpp_vppOUT1_intr);

		/* VOUT1 interrupt */
		/* clear interrupt */
		wrap_semaphore_pop(pSemHandle, avioDhubSemMap_vpp_vppOUT1_intr, 1);
		wrap_semaphore_clr_full(pSemHandle, avioDhubSemMap_vpp_vppOUT1_intr);
	}
#endif
	if (vpp_intr) {
		unsigned long flag;
		MV_CC_MSG_t msg = { VPP_CC_MSG_TYPE_VPP,
			vpp_intr,
			hVppCtx->vpp_intr_timestamp
		};
		spin_lock_irqsave(&hVppCtx->vpp_msg_spinlock, flag);
		ret = AMPMsgQ_Add(&hVppCtx->hVPPMsgQ, &msg);
		spin_unlock_irqrestore(&hVppCtx->vpp_msg_spinlock, flag);
		if (ret != S_OK) {
			if (!atomic_read(&hVppCtx->vpp_isr_msg_err_cnt)) {
				amp_error
				    ("[vpp isr] MsgQ full\n");
			}
			atomic_inc(&hVppCtx->vpp_isr_msg_err_cnt);
			return IRQ_HANDLED;
		}
		up(&hVppCtx->vpp_sem);

#ifdef CONFIG_IRQ_LATENCY_PROFILE
		hVppCtx->amp_irq_profiler.vpp_isr_end = cpu_clock(smp_processor_id());
		unsigned long isr_time =
		    hVppCtx->amp_irq_profiler.vpp_isr_end -
		    hVppCtx->amp_irq_profiler.vpp_isr_start;
		int32_t jitter = 0;
		isr_time /= 1000;

		if (bTST(instat, avioDhubSemMap_vpp_vppCPCB0_intr)) {
			if (hVppCtx->amp_irq_profiler.vppCPCB0_intr_last) {
				jitter =
				    (int64_t) hVppCtx->amp_irq_profiler.
				    vppCPCB0_intr_curr -
				    (int64_t) hVppCtx->amp_irq_profiler.
				    vppCPCB0_intr_last;

				//nanosec_rem = do_div(interval, 1000000000);
				// transform to us unit
				jitter /= 1000;
				jitter -= 16667;
			}
			hVppCtx->amp_irq_profiler.vppCPCB0_intr_last =
			    hVppCtx->amp_irq_profiler.vppCPCB0_intr_curr;
		}

		if ((jitter > 670) || (jitter < -670) || (isr_time > 1000)) {
			amp_trace
			    (" W/[vpp isr] jitter:%6d > +-670 us, instat:0x%x last_instat:"
			     "0x%0x max_instat:0x%0x, isr_time:%d us last:%d max:%d \n",
			     jitter, instat_used,
			     hVppCtx->amp_irq_profiler.vpp_isr_last_instat,
			     hVppCtx->amp_irq_profiler.vpp_isr_instat_max, isr_time,
			     hVppCtx->amp_irq_profiler.vpp_isr_time_last,
			     hVppCtx->amp_irq_profiler.vpp_isr_time_max);
		}

		hVppCtx->amp_irq_profiler.vpp_isr_last_instat = instat_used;
		hVppCtx->amp_irq_profiler.vpp_isr_time_last = isr_time;

		if (isr_time > hVppCtx->amp_irq_profiler.vpp_isr_time_max) {
			hVppCtx->amp_irq_profiler.vpp_isr_time_max = isr_time;
			hVppCtx->amp_irq_profiler.vpp_isr_instat_max = instat_used;
		}
#endif
	}
#ifdef CONFIG_IRQ_LATENCY_PROFILE
	else {
		hVppCtx->amp_irq_profiler.vpp_isr_end = cpu_clock(smp_processor_id());
		unsigned long isr_time =
		    hVppCtx->amp_irq_profiler.vpp_isr_end -
		    hVppCtx->amp_irq_profiler.vpp_isr_start;
		isr_time /= 1000;

		if (isr_time > 1000) {
			amp_trace("###isr_time:%d us instat:%x last:%x\n",
				  isr_time, vpp_intr, instat,
				  hVppCtx->amp_irq_profiler.vpp_isr_last_instat);
		}

		hVppCtx->amp_irq_profiler.vpp_isr_time_last = isr_time;
	}
#endif

#ifdef CONFIG_MV_AMP_COMPONENT_AIP_ENABLE
	if (hVppCtx->is_spdifrx_enabled) {
#if (BERLIN_CHIP_VERSION > BERLIN_BG2_A0)
        UINT chanId = avioDhubChMap_vpp_SPDIF_W;		
#else
        UINT chanId = avioDhubChMap_vpp_SPDIF_Rx;
#endif
		if (bTST(instat, chanId)) {
			wrap_semaphore_pop(pSemHandle, chanId, 1);
			wrap_semaphore_clr_full(pSemHandle, chanId);

			amp_error("FIXME: in file drv_vpp.c line:%d\n", __LINE__);
			aip_resume();
		}
	}
#endif
	return IRQ_HANDLED;
}
VPP_CTX * drv_vpp_init(VOID)
{
    VPP_CTX *hVppCtx = &vpp_ctx;
    UINT err;

    memset(hVppCtx, 0, sizeof(VPP_CTX));

    sema_init(&hVppCtx->vpp_sem, 0);
    spin_lock_init(&hVppCtx->vpp_msg_spinlock);
	spin_lock_init(&hVppCtx->bcm_spinlock);
    err = AMPMsgQ_Init(&hVppCtx->hVPPMsgQ, VPP_ISR_MSGQ_SIZE);
    if (unlikely(err != S_OK)) {
        amp_error("drv_vpp_init: hVPPMsgQ init failed, err:%8x\n", err);
        return NULL;
    }

    return hVppCtx;
}

VOID drv_vpp_exit(VPP_CTX *hVppCtx)
{
    UINT err;
    err = AMPMsgQ_Destroy(&hVppCtx->hVPPMsgQ);
    if (unlikely(err != S_OK)) {
        amp_error("vpp MsgQ Destroy: failed, err:%8x\n", err);
        return;
    }
}
