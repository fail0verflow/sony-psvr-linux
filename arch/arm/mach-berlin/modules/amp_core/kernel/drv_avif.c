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
#include "drv_avif.h"
#include "drv_msg.h"
#include "drv_debug.h"
#include "api_dhub.h"
#include "api_avio_dhub.h"
#include "hal_dhub_wrap.h"
#include "api_avif_dhub.h"
#include "avif_dhub_config.h"

#include "drv_aout.h"

static AVIF_CTX avif_ctx;

#if defined(CONFIG_MV_AMP_COMPONENT_AIP_ENABLE) || defined(CONFIG_MV_AMP_COMPONENT_AVIN_ENABLE)
static void *AIPFifoGetKernelPreRdDMAInfo(AIP_DMA_CMD_FIFO * p_aip_cmd_fifo,
					  INT pair)
{
	void *pHandle;
	INT rd_offset = p_aip_cmd_fifo->kernel_pre_rd_offset;
	if (rd_offset > p_aip_cmd_fifo->size || rd_offset < 0) {
		INT i = 0, fifo_cmd_size = sizeof(AIP_DMA_CMD_FIFO) >> 2;
		INT *temp = (INT *)p_aip_cmd_fifo;
		amp_trace("memory %p is corrupted! corrupted data :\n", p_aip_cmd_fifo);
		for (i = 0; i < fifo_cmd_size; i++) {
			amp_trace("0x%x\n", *temp++);
		}
		rd_offset = 0;
	}
	pHandle = &(p_aip_cmd_fifo->aip_dma_cmd[pair][rd_offset]);
	return pHandle;
}

static void AIPFifoKernelPreRdUpdate(AIP_DMA_CMD_FIFO * p_aip_cmd_fifo, INT adv)
{
	p_aip_cmd_fifo->kernel_pre_rd_offset += adv;
	p_aip_cmd_fifo->kernel_pre_rd_offset %= p_aip_cmd_fifo->size;
}

static void AIPFifoKernelRdUpdate(AIP_DMA_CMD_FIFO * p_aip_cmd_fifo, INT adv)
{
    p_aip_cmd_fifo->kernel_rd_offset += adv;
	p_aip_cmd_fifo->kernel_rd_offset %= p_aip_cmd_fifo->size;
}

static INT AIPFifoCheckKernelFullness(AIP_DMA_CMD_FIFO * p_aip_cmd_fifo)
{
	INT full;
	full = p_aip_cmd_fifo->wr_offset - p_aip_cmd_fifo->kernel_pre_rd_offset;
	if (full < 0)
		full += p_aip_cmd_fifo->size;
	return full;
}

int intr_counter = 0;
static void HdmirxHandle(AVIF_CTX *hAvifCtx, UINT32 hdmi_port)
{
    HRESULT rc = S_OK;
    UINT32 port_offset = 0;

    UINT8 en0, en1, en2, en3;
    INT hrx_intr = 0xff;
    UINT32 hrxsts0, hrxsts1, hrxen0, hrxen1;
    UINT32 chip_rev;

    if ((hdmi_port >= 0) && (hdmi_port <= 3)) {
        port_offset = (hdmi_port * AVIF_HRX_BASE_OFFSET) << 2;
        //printk("HDMIRX interrupt from port %d\r\n", hrx_port);
    } else {
        printk(KERN_ERR "Invalid HDMI Rx Port %d\n", hdmi_port);
        return;
    }

    GA_REG_WORD32_READ(port_offset+RA_HDRX_INTR_EN, &hrxen0);
    GA_REG_WORD32_READ(port_offset+RA_HDRX_INTR_EN1, &hrxen1);
    GA_REG_WORD32_READ(port_offset+RA_HDRX_INTR_STATUS, &hrxsts0);
    GA_REG_WORD32_READ(port_offset+RA_HDRX_INTR_STATUS1, &hrxsts1);

    ++intr_counter;

    if((intr_counter % 20000 == 0) || (intr_counter % 20000 == 1)){
        printk(KERN_ERR "HDMIINTR: en0 0x%x en1 0x%x sts0 0x%x sts1 0x%x\r\n", hrxen0, hrxen1, hrxsts0, hrxsts1);
    }
    hrxen0 = hrxsts0 & hrxen0;
    hrxen1 = hrxsts1 & hrxen1;

    en0 = (hrxen0 & 0xFF);
    en1 = ((hrxen0 & 0xFF00)>>8);
    en2 = ((hrxen0 & 0xFF0000)>>16);
    en3 = ((hrxen0 & 0xFF000000)>>24);

    GA_REG_WORD32_READ(BG2Q4K_CHIP_REV_ADDRESS, &chip_rev);
    //printk(KERN_ERR "HDMI interrupt status-1:%d interrupt mask-1:%d\n",stat1, en1);

    //printk(KERN_ERR"@port:%d intr regs[0-4] = 0x%x, interrupt_mask_regs[0-4]=%x",
     //   hrx_port, stat0|stat1<<8|stat2<<16|stat3<<24, en0 | en1<<8|en2<<16|en3<<24);
    hrx_intr = 0xff;
    if ((en2 & (HDMI_RX_INT_VRES_CHG | HDMI_RX_INT_HRES_CHG)) ||
           (en3 & (HDMI_RX_INT_5V_PWR | HDMI_RX_INT_CLOCK_CHANGE))) {
        hrx_intr = HDMIRX_INTR_SYNC;

        GA_REG_WORD32_READ(port_offset+RA_HDRX_INTR_EN, &hrxen0);
        hrxen0 = hrxen0 & ~((HDMI_RX_INT_SYNC_DET | HDMI_RX_INT_VRES_CHG | HDMI_RX_INT_HRES_CHG) << 16);
        hrxen0 = hrxen0 & ~((HDMI_RX_INT_5V_PWR | HDMI_RX_INT_CLOCK_CHANGE) << 24); //?? not needed
        hrxen0 = ((hrxen0 & 0xFF0000) | (HDMI_RX_INT_AUTH_STARTED << 16)| (HDMI_RX_INT_WR_MSSG_STARTED << 24));
        GA_REG_WORD32_WRITE((port_offset+RA_HDRX_INTR_EN),hrxen0);
        GA_REG_WORD32_WRITE((port_offset+RA_HDRX_INTR_EN1), 0x07);
    }
#if (BERLIN_CHIP_VERSION >= BERLIN_BG2_DTV_A0)
    else if ((chip_rev >= 0xB0) && (hrxen1 & HDMI_RX_INT_PRT)) {
        GA_REG_WORD32_WRITE((port_offset+RA_HDRX_INTR_EN1), HDMI_RX_INT_PRT);
        hrx_intr = HDMIRX_INTR_PRT;
    }
#endif
    else if (en0 & HDMI_RX_INT_GCP_AVMUTE) {
        hrx_intr = HDMIRX_INTR_AVMUTE;
        GA_REG_WORD32_READ(port_offset+RA_HDRX_INTR_EN, &hrxen0);
        hrxen0 = hrxen0 & ~(HDMI_RX_INT_GCP_AVMUTE);
        GA_REG_WORD32_WRITE((port_offset+RA_HDRX_INTR_EN),hrxen0);
    } else if((en3 & HDMI_RX_INT_WR_MSSG_STARTED)) {
        hrx_intr = HDMIRX_INTR_HDCP_2X;
        GA_REG_WORD32_READ(port_offset+RA_HDRX_INTR_EN, &hrxen0);
        hrxen0 =  hrxen0 & ~(HDMI_RX_INT_WR_MSSG_STARTED << 24);
        GA_REG_WORD32_WRITE((port_offset+RA_HDRX_INTR_EN),hrxen0);
    } else if (en2 & (HDMI_RX_INT_AUTH_STARTED | HDMI_RX_INT_AUTH_COMPLETE)) {
        hrx_intr = HDMIRX_INTR_HDCP;
        GA_REG_WORD32_READ(port_offset+RA_HDRX_INTR_EN, &hrxen0);
        hrxen0 =  hrxen0 & ~(HDMI_RX_INT_AUTH_STARTED << 16);
        GA_REG_WORD32_WRITE((port_offset+RA_HDRX_INTR_EN),hrxen0);
    } else if (en2 & HDMI_RX_INT_VSI_STOP) {
        hrx_intr = HDMIRX_INTR_VSI_STOP;
        GA_REG_WORD32_READ(port_offset+RA_HDRX_INTR_EN, &hrxen0);
        hrxen0 = hrxen0 & ~(HDMI_RX_INT_VSI_STOP << 16);
        GA_REG_WORD32_WRITE((port_offset+RA_HDRX_INTR_EN),hrxen0);
    } else if (en1 &(HDMI_RX_INT_AVI_INFO | HDMI_RX_INT_VENDOR_PKT)) {
        hrx_intr = HDMIRX_INTR_PKT;
        GA_REG_WORD32_READ(port_offset+RA_HDRX_INTR_EN, &hrxen0);
        hrxen0 = hrxen0 & ~((HDMI_RX_INT_AVI_INFO | HDMI_RX_INT_VENDOR_PKT)<<8);
        GA_REG_WORD32_WRITE((port_offset+RA_HDRX_INTR_EN),hrxen0);
    } else if (en1 & HDMI_RX_INT_GAMUT_PKT) {
        if(hAvifCtx->factory_mode & HDMIRX_FACTORY_IGNORE_GMD_FIX){
            GA_REG_WORD32_READ(port_offset+RA_HDRX_INTR_EN, &hrxen0);
            hrxen0 = hrxen0 & ~((HDMI_RX_INT_GAMUT_PKT)<<8);
            GA_REG_WORD32_WRITE((port_offset+RA_HDRX_INTR_EN),hrxen0);
        }
        hrx_intr = HDMIRX_INTR_GMD_PKT;
    } else if (en1 &(HDMI_RX_INT_CHNL_STATUS | HDMI_RX_INT_AUD_INFO)) {
        hrx_intr = HDMIRX_INTR_CHNL_STS;
        GA_REG_WORD32_READ(port_offset+RA_HDRX_INTR_EN, &hrxen0);
        hrxen0 = hrxen0 & ~((HDMI_RX_INT_CHNL_STATUS |HDMI_RX_INT_AUD_INFO)<<8);
        GA_REG_WORD32_WRITE((port_offset+RA_HDRX_INTR_EN),hrxen0);
    } else if (en0 & HDMI_RX_INT_GCP_COLOR_DEPTH) {
        hrx_intr = HDMIRX_INTR_CLR_DEPTH;
        GA_REG_WORD32_READ(port_offset+RA_HDRX_INTR_EN, &hrxen0);
        hrxen0 = hrxen0 & ~(HDMI_RX_INT_GCP_COLOR_DEPTH);
        GA_REG_WORD32_WRITE((port_offset+RA_HDRX_INTR_EN),hrxen0);
    }

    /* HDMI Rx interrupt */
    if (hrx_intr != 0xff) {
        //printk("hrx_intr = 0x%x\r\n", hrx_intr);
        /* process rx intr */
        MV_CC_MSG_t msg = {
            0,
            hrx_intr, //send port info also
            hdmi_port
        };

        if((hrx_intr == HDMIRX_INTR_SYNC) || (hrx_intr == HDMIRX_INTR_AVMUTE)){
            GA_REG_WORD32_READ(0xF7C80134, &hrxen0);
            hrxen0 = (hrxen0 | 0x1);
            GA_REG_WORD32_WRITE(0xF7C80134,hrxen0);
        }

        if(hrx_intr == HDMIRX_INTR_HDCP_2X) {
            rc = AMPMsgQ_Add(&hAvifCtx->hPEAVIFHDCPMsgQ, &msg);
            if (rc != S_OK) {
                amp_error("[AVIF HDCP isr] HDCP MsgQ full\n");
                return;
            }
            up(&hAvifCtx->avif_hdcp_sem);
        } else {
            rc = AMPMsgQ_Add(&hAvifCtx->hPEAVIFHRXMsgQ, &msg);
            if (rc != S_OK) {
                amp_error("[AVIF HRX isr] MsgQ full\n");
                return;
            }
            up(&hAvifCtx->avif_hrx_sem);
        }
    }

    return;
}

AVIF_CTX* drv_avif_init(void)
{
    INT err;
    AVIF_CTX *hAvifCtx = &avif_ctx;

    memset(&avif_ctx, 0, sizeof(AVIF_CTX));

    spin_lock_init(&hAvifCtx->aip_spinlock);
    spin_lock_init(&hAvifCtx->aip_spinlock);

    sema_init(&hAvifCtx->avif_sem, 0);
    err = AMPMsgQ_Init(&hAvifCtx->hPEAVIFMsgQ, AVIF_ISR_MSGQ_SIZE);
    if (unlikely(err != S_OK)) {
        amp_error("PEAVIFMsgQ_Init: falied, err:%8x\n", err);
        return NULL;
    }

    sema_init(&hAvifCtx->avif_hrx_sem, 0);
    err = AMPMsgQ_Init(&hAvifCtx->hPEAVIFHRXMsgQ, AVIF_HRX_ISR_MSGQ_SIZE);
    if (unlikely(err != S_OK)) {
        amp_error("PEAVIFHRXMsgQ_Init: falied, err:%8x\n", err);
        return NULL;
    }

    sema_init(&hAvifCtx->avif_hdcp_sem, 0);
    err = AMPMsgQ_Init(&hAvifCtx->hPEAVIFHDCPMsgQ, AVIF_HDCP_ISR_MSGQ_SIZE);
    if (unlikely(err != S_OK)) {
        amp_error("PEAVIFHDCPMsgQ_Init: falied, err:%8x\n", err);
        return NULL;
    }
    sema_init(&hAvifCtx->avif_vdec_sem, 0);
    err = AMPMsgQ_Init(&hAvifCtx->hPEAVIFVDECMsgQ, AVIF_VDEC_ISR_MSGQ_SIZE);
    if (unlikely(err != S_OK)) {
        amp_error("hPEAVIFVDECMsgQ: falied, err:%8x\n", err);
        return NULL;
    }

    sema_init(&hAvifCtx->aip_sem, 0);
    err = AMPMsgQ_Init(&hAvifCtx->hAIPMsgQ, AIP_ISR_MSGQ_SIZE);
    if (unlikely(err != S_OK)) {
        amp_error("hAIPMsgQ init: failed, err:%8x\n", err);
        return NULL;
    }
    hAvifCtx->factory_mode = 0;
    return hAvifCtx;
}

void HDMIRX_power_down(void)
{
    int i, port_offset;
#if (BERLIN_CHIP_VERSION >= BERLIN_BG2_DTV_A0)
    for (i = 0; i < 4; i++) {
        port_offset = (i * AVIF_HRX_BASE_OFFSET) << 2;
        GA_REG_WORD32_WRITE(port_offset+RA_HDRX_PHY_PM_OVRWEN, 0x1F);

        //Set PD_HDMI_COMM, PD_FRT_C, PD_PLL, PD_FRT_D, PD_CDR_D, RESET_PLL
        GA_REG_WORD32_WRITE(port_offset+RA_HDRX_PHY_PM_OVRW, 0x4);
	}
    msleep(100);

    for (i = 0; i < 4; i++) {
        port_offset = (i * AVIF_HRX_BASE_OFFSET) << 2;
        GA_REG_WORD32_WRITE(port_offset+RA_HDRX_PHY_PM_OVRW, 0x17FC);
        GA_REG_WORD32_WRITE(port_offset+RA_HDRX_PHY_PM_OVRWEN, 0x0);

        //Disable HDMI Core interrupts
        GA_REG_WORD32_WRITE(port_offset+RA_HDRX_INTR_EN, 0x0);
        GA_REG_WORD32_WRITE(port_offset+RA_HDRX_INTR_EN1, 0x0);

        //Soft reset the HDMI digital core
        GA_REG_WORD32_WRITE(port_offset+RA_HDRX_SOFT_RST_CTRL, 0x1);
        amp_trace("HDMIRX_power_down port %d\n", i);
    }
#endif
}
void drv_avif_exit(AVIF_CTX *hAvifCtx)
{
    UINT err;

    err = AMPMsgQ_Destroy(&hAvifCtx->hPEAVIFMsgQ);
    if (unlikely(err != S_OK)) {
        amp_trace("avif MsgQ Destroy: falied, err:%8x\n", err);
    }
    err = AMPMsgQ_Destroy(&hAvifCtx->hPEAVIFHRXMsgQ);
    if (unlikely(err != S_OK)) {
        amp_trace("AVIF hrx MsgQ Destroy: falied, err:%8x\n", err);
    }
    err = AMPMsgQ_Destroy(&hAvifCtx->hPEAVIFHDCPMsgQ);
    if (unlikely(err != S_OK)) {
        amp_trace("AVIF hdcp MsgQ Destroy: falied, err:%8x\n", err);
    }
    err = AMPMsgQ_Destroy(&hAvifCtx->hPEAVIFVDECMsgQ);
    if (unlikely(err != S_OK)) {
        amp_trace("AVIF VDEC MsgQ Destroy: falied, err:%8x\n", err);
    }

    err = AMPMsgQ_Destroy(&hAvifCtx->hAIPMsgQ);
    if (unlikely(err != S_OK)) {
        amp_trace("AIP MsgQ Destroy: failed, err:%8x\n", err);
    }
}

irqreturn_t amp_devices_avif_isr(int irq, void *dev_id)
{
    HDL_semaphore *pSemHandle;
    HRESULT rc = S_OK;
    INT channel;
    INT instat;
    UINT8 enreg0, enreg1, enreg2, enreg3, enreg4;
    UINT8 masksts0, masksts1, masksts2, masksts3, masksts4;
    UINT8 regval;
    //UINT32 hdmi_port = 0;
    UINT32 instat0 = 0, instat1 = 0;
    AVIF_CTX *hAvifCtx;

#ifdef MORPHEUS_TV_VIEW_MODE
    u64  clock_ns = local_clock();
	extern long long vip_isr_count;
	extern int video_mode;
	extern u64 cur_vip_intr_timestamp;
	extern int vip_stable;
    extern int vip_stable_isr;
#endif

    hAvifCtx = (AVIF_CTX *)dev_id;

    GA_REG_BYTE_READ(CPU_INTR_MASK0_EXTREG_ADDR,&enreg0);
    GA_REG_BYTE_READ(CPU_INTR_MASK0_STATUS_EXTREG_ADDR, &masksts0);

    GA_REG_BYTE_READ(CPU_INTR_MASK1_EXTREG_ADDR,&enreg1);
    GA_REG_BYTE_READ(CPU_INTR_MASK1_STATUS_EXTREG_ADDR,&masksts1);

    GA_REG_BYTE_READ(CPU_INTR_MASK2_EXTREG_ADDR, &enreg2);
    GA_REG_BYTE_READ(CPU_INTR_MASK2_STATUS_EXTREG_ADDR,&masksts2);

    GA_REG_BYTE_READ(CPU_INTR_MASK3_EXTREG_ADDR, &enreg3);
    GA_REG_BYTE_READ(CPU_INTR_MASK3_STATUS_EXTREG_ADDR,&masksts3);

    GA_REG_BYTE_READ(CPU_INTR_MASK4_EXTREG_ADDR, &enreg4);
    GA_REG_BYTE_READ(CPU_INTR_MASK4_STATUS_EXTREG_ADDR,&masksts4);

    instat0 = (masksts0 << 0) | (masksts1<<8) | (masksts2<<16) | (masksts3 <<24);

#ifdef MORPHEUS_TV_VIEW_MODE
    // check the AVIF MAIN PATH VDE
    if(((instat0>>0)&0xFF) & AVIF_INTR_MAIN_VDE){
		//in/out timeline
		if (video_mode == MODE_TV_VIEW) {
			vip_isr_count++;
			cur_vip_intr_timestamp = clock_ns;
			//amp_trace("avif isr AVIF_INTR_MAIN_VDE vip_isr_count %d\n", vip_isr_count);
			if(vip_stable == 1 && vip_stable_isr == 0)
			{
				vip_stable_isr=1;
			}
		}

    }

    // check the AVIP PIP PATH VDE
    if(((instat0>>8)&0xFF) & AVIF_INTR_DELAYED_PIP_VDE){
		//in/out timeline
		if (video_mode == MODE_TV_VIEW) {
			vip_isr_count++;
			cur_vip_intr_timestamp = clock_ns;
			//amp_trace("avif isr AVIF_INTR_DELAYED_PIP_VDE vip_isr_count %d\n", vip_isr_count);
			if(vip_stable == 1 && vip_stable_isr == 0)
			{
				vip_stable_isr=1;
			}
		}

    }
#endif


    GA_REG_BYTE_READ(CPU_INTCPU_2_EXTCPU_MASK0INTR_ADDR, &regval);
    instat1 |= regval;
    if(regval&0x1)
        GA_REG_BYTE_WRITE(CPU_INTCPU_2_EXTCPU_INTR0_ADDR, 0x00);
    if(regval&0x2)
        GA_REG_BYTE_WRITE(CPU_INTCPU_2_EXTCPU_INTR1_ADDR, 0x00);
    if(regval&0x4)
        GA_REG_BYTE_WRITE(CPU_INTCPU_2_EXTCPU_INTR2_ADDR, 0x00);
    if(regval&0x8)
        GA_REG_BYTE_WRITE(CPU_INTCPU_2_EXTCPU_INTR3_ADDR, 0x00);
    if(regval&0x10)
        GA_REG_BYTE_WRITE(CPU_INTCPU_2_EXTCPU_INTR4_ADDR, 0x00);
    if(regval&0x20)
        GA_REG_BYTE_WRITE(CPU_INTCPU_2_EXTCPU_INTR5_ADDR, 0x00);
    if(regval&0x40)
        GA_REG_BYTE_WRITE(CPU_INTCPU_2_EXTCPU_INTR6_ADDR, 0x00);
    if(regval&0x80)
        GA_REG_BYTE_WRITE(CPU_INTCPU_2_EXTCPU_INTR7_ADDR, 0x00);

    //Clear interrupts
    GA_REG_BYTE_WRITE(CPU_INTR_CLR0_EXTREG_ADDR, masksts0);
    GA_REG_BYTE_WRITE(CPU_INTR_CLR1_EXTREG_ADDR, masksts1);
    GA_REG_BYTE_WRITE(CPU_INTR_CLR2_EXTREG_ADDR, masksts2);
    GA_REG_BYTE_WRITE(CPU_INTR_CLR3_EXTREG_ADDR, masksts3);
    GA_REG_BYTE_WRITE(CPU_INTR_CLR4_EXTREG_ADDR, masksts4);

    if (((masksts1 & 0x0F)&(~enreg1)) || ((masksts2 & 0x0F) & (~enreg2))) {
        //Interrupt for HDMI port 0
        if (((masksts1 | masksts2) & 0x01) == 0x01) {
            HdmirxHandle(hAvifCtx, 0);
        }

        //Interrupt for HDMI port 1
        if (((masksts1 | masksts2) & 0x02) == 0x02) {
            HdmirxHandle(hAvifCtx, 1);
        }

        //Interrupt for HDMI port 2
        if (((masksts1 | masksts2) & 0x04) == 0x04) {
            HdmirxHandle(hAvifCtx, 2);
        }

        //Interrupt for HDMI port 3
        if (((masksts1 | masksts2) & 0x08) == 0x08) {
            HdmirxHandle(hAvifCtx, 3);
        }
    }

    if(hAvifCtx->IsPIPAudio)
        channel = avif_dhub_config_ChMap_avif_AUD_WR0_PIP;
    else
        channel = avif_dhub_config_ChMap_avif_AUD_WR0_MAIN;

    /* DHUB Interrupt status */
    pSemHandle = dhub_semaphore(&AVIF_dhubHandle.dhub);
    instat = semaphore_chk_full(pSemHandle, -1);

#ifdef CONFIG_MV_AMP_COMPONENT_AIP_ENABLE
    //Audio DHUB INterrupt handling
    {
        INT chanId;
        for (chanId = channel;chanId <= (channel + 3); chanId++) {
            if (bTST(instat, chanId)) {
                semaphore_pop(pSemHandle, chanId, 1);
                semaphore_clr_full(pSemHandle, chanId);
                if (chanId == channel) {
				    MV_CC_MSG_t msg = {0 , 0, 0};
                    aip_avif_resume_cmd(hAvifCtx);

				    msg.m_MsgID = 1 << chanId;
				    AMPMsgQ_Add(&hAvifCtx->hAIPMsgQ, &msg);
				    up(&hAvifCtx->aip_sem);
                }
             }
        }
    }
#endif

#ifdef CONFIG_MV_AMP_AUDIO_PATH_SPDIF_ENABLE
    if (bTST(instat, avif_dhub_config_ChMap_avif_AUD_RD0))
    {
		MV_CC_MSG_t msg = {0, 0, 0};
        semaphore_pop(pSemHandle, avif_dhub_config_ChMap_avif_AUD_RD0 , 1);
        semaphore_clr_full(pSemHandle, avif_dhub_config_ChMap_avif_AUD_RD0 );
        arc_copy_spdiftx_data(hAvifCtx);

		msg.m_MsgID = 1 << avioDhubChMap_ag_SPDIF_R;
        send_msg2aout(&msg);
    }

    //Send VDE interrupt info to AVIF driver
    if(instat0 || instat1) {
        MV_CC_MSG_t msg ={
            0,
            instat0,
            instat1
        };
        rc = AMPMsgQ_Add(&hAvifCtx->hPEAVIFMsgQ, &msg);
        if (rc != S_OK) {
            if (!atomic_read(&hAvifCtx->avif_isr_msg_err_cnt)) {
                amp_error("[AVIF isr] MsgQ full\n");
            }
            atomic_inc(&hAvifCtx->avif_isr_msg_err_cnt);
            return IRQ_HANDLED;
		}
        up(&hAvifCtx->avif_sem);
    }
#endif
    return IRQ_HANDLED;
}

#endif

#if defined(CONFIG_MV_AMP_COMPONENT_AIP_ENABLE)
void aip_start_cmd(AVIF_CTX *hAvifCtx, INT *aip_info, void *param)
{
	INT *p = aip_info;
	INT chanId;
	HDL_dhub *dhub = NULL;
	AIP_DMA_CMD *p_dma_cmd;
	AIP_DMA_CMD_FIFO *pCmdFifo = NULL;

	if ( !hAvifCtx ) {
        amp_error("aip_resume_cmd null handler!\n");
        return;
    }

	hAvifCtx->aip_source = aip_info[2];
	hAvifCtx->p_aip_cmdfifo = pCmdFifo = (AIP_DMA_CMD_FIFO *) param;

    pCmdFifo = hAvifCtx->p_aip_cmdfifo;
	if ( !pCmdFifo ) {
		amp_trace("aip_resume_cmd:p_aip_fifo is NULL\n");
		return;
	}

	if (*p == 1) {
		hAvifCtx->aip_i2s_pair = 1;
		p_dma_cmd =
		    (AIP_DMA_CMD *) AIPFifoGetKernelPreRdDMAInfo(pCmdFifo, 0);

		if (AIP_SOUECE_SPDIF == hAvifCtx->aip_source) {
			chanId = avioDhubChMap_vpp_SPDIF_W;
			dhub = &VPP_dhubHandle.dhub;
			wrap_dhub_channel_write_cmd(dhub, chanId,
				       p_dma_cmd->addr0, p_dma_cmd->size0, 0, 0,
				       0, 1, 0, 0);
		} else {
#if (BERLIN_CHIP_VERSION >= BERLIN_BG2_DTV_A0)
			chanId = avioDhubChMap_ag_PDM_MIC_ch1;
			dhub = &AG_dhubHandle.dhub;
			dhub_channel_write_cmd(dhub, chanId,
						p_dma_cmd->addr0, p_dma_cmd->size0, 0, 0,
						0, 1, 0, 0);
#endif
		}
		AIPFifoKernelPreRdUpdate(hAvifCtx->p_aip_cmdfifo, 1);

		// push 2nd dHub command
		p_dma_cmd =
		    (AIP_DMA_CMD *) AIPFifoGetKernelPreRdDMAInfo(hAvifCtx->p_aip_cmdfifo, 0);
		if(chanId == avioDhubChMap_vpp_SPDIF_W)
			wrap_dhub_channel_write_cmd(dhub, chanId,
					p_dma_cmd->addr0, p_dma_cmd->size0, 0, 0,
					0, 1, 0, 0);
		else
			dhub_channel_write_cmd(dhub, chanId,
					p_dma_cmd->addr0, p_dma_cmd->size0, 0, 0,
					0, 1, 0, 0);
		AIPFifoKernelPreRdUpdate(hAvifCtx->p_aip_cmdfifo, 1);
	} else if (*p == 4) {
#if (BERLIN_CHIP_VERSION >= BERLIN_BG2_DTV_A0)
        UINT pair;
        hAvifCtx->aip_i2s_pair = 4;
		for (pair = 0; pair < 4; pair++) {
			p_dma_cmd =
				(AIP_DMA_CMD *)
				AIPFifoGetKernelPreRdDMAInfo(pCmdFifo, pair);
			chanId = avioDhubChMap_ag_PDM_MIC_ch1 + pair;
			dhub_channel_write_cmd(&AG_dhubHandle.dhub, chanId,
							p_dma_cmd->addr0,
							p_dma_cmd->size0, 0, 0, 0, 1, 0,
							0);
		}

		AIPFifoKernelPreRdUpdate(pCmdFifo, 1);

		for (pair = 0; pair < 4; pair++) {
			p_dma_cmd = (AIP_DMA_CMD *)
				AIPFifoGetKernelPreRdDMAInfo(pCmdFifo, pair);
			chanId = avioDhubChMap_ag_PDM_MIC_ch1 + pair;
			dhub_channel_write_cmd(&AG_dhubHandle.dhub, chanId,
							p_dma_cmd->addr0,
							p_dma_cmd->size0, 0, 0, 0, 1, 0,
							0);
		}
		AIPFifoKernelPreRdUpdate(pCmdFifo, 1);
#endif
	}
}

void aip_stop_cmd(AVIF_CTX *hAvifCtx)
{
    if ( !hAvifCtx ) {
        amp_error("aip_stop_cmd null handler!\n");
        return;
    }
	hAvifCtx->p_aip_cmdfifo = NULL;
}

void aip_resume_cmd(AVIF_CTX *hAvifCtx)
{
	AIP_DMA_CMD *p_dma_cmd;
	HDL_dhub *dhub = NULL;
	UINT chanId;
	INT pair;
    AIP_DMA_CMD_FIFO *pCmdFifo;

	if ( !hAvifCtx ) {
        amp_error("aip_resume_cmd null handler!\n");
        return;
    }

    pCmdFifo = hAvifCtx->p_aip_cmdfifo;
	if ( !pCmdFifo ) {
		amp_trace("aip_resume_cmd:p_aip_fifo is NULL\n");
		return;
	}

	spin_lock(&hAvifCtx->aip_spinlock);

	if ( !pCmdFifo->fifo_overflow ) {
		AIPFifoKernelRdUpdate(pCmdFifo, 1);
    }

	if (AIPFifoCheckKernelFullness(pCmdFifo)) {
		pCmdFifo->fifo_overflow = 0;
		for (pair = 0; pair < hAvifCtx->aip_i2s_pair; pair++) {
			p_dma_cmd =
			    (AIP_DMA_CMD *)
			    AIPFifoGetKernelPreRdDMAInfo(pCmdFifo, pair);
			if (AIP_SOUECE_SPDIF == hAvifCtx->aip_source) {
				chanId = avioDhubChMap_vpp_SPDIF_W;
				dhub = &VPP_dhubHandle.dhub;
				wrap_dhub_channel_write_cmd(dhub, chanId,
								p_dma_cmd->addr0,
								p_dma_cmd->size0, 0, 0, 0,
								p_dma_cmd->addr1 ? 0 : 1, 0, 0);
				if (p_dma_cmd->addr1) {
					wrap_dhub_channel_write_cmd(dhub,
								chanId, p_dma_cmd->addr1,
								p_dma_cmd->size1, 0, 0,
								0, 1, 0, 0);
				}
			}
			else {
#if (BERLIN_CHIP_VERSION >= BERLIN_BG2_DTV_A0)
                chanId = avioDhubChMap_ag_PDM_MIC_ch1 + pair;
				dhub = &AG_dhubHandle.dhub;
				dhub_channel_write_cmd(dhub, chanId,
							p_dma_cmd->addr0,
							p_dma_cmd->size0, 0, 0, 0,
							p_dma_cmd->addr1 ? 0 : 1, 0, 0);
				if (p_dma_cmd->addr1) {
					dhub_channel_write_cmd(dhub,
							chanId, p_dma_cmd->addr1,
							p_dma_cmd->size1, 0, 0,
							0, 1, 0, 0);
				}
#endif
			}
		}
		AIPFifoKernelPreRdUpdate(pCmdFifo, 1);
	} else {
		pCmdFifo->fifo_overflow = 1;
		pCmdFifo->fifo_overflow_cnt++;
		for (pair = 0; pair < hAvifCtx->aip_i2s_pair; pair++) {
			/* FIXME:
			 *chanid should be changed if 4 pair is supported
			 */
			if (AIP_SOUECE_SPDIF == hAvifCtx->aip_source) {
				chanId = avioDhubChMap_vpp_SPDIF_W;
				dhub = &VPP_dhubHandle.dhub;
				wrap_dhub_channel_write_cmd(dhub, chanId,
							pCmdFifo->overflow_buffer,
							pCmdFifo->overflow_buffer_size,
							0, 0, 0, 1, 0, 0);
			}
			else {
#if (BERLIN_CHIP_VERSION >= BERLIN_BG2_DTV_A0)
                chanId = avioDhubChMap_ag_PDM_MIC_ch1 + pair;
                dhub = &AG_dhubHandle.dhub;
                dhub_channel_write_cmd(dhub, chanId,
                           pCmdFifo->overflow_buffer,
                           pCmdFifo->overflow_buffer_size,
                           0, 0, 0, 1, 0, 0);
#endif
			}
		}
	}

	spin_unlock(&hAvifCtx->aip_spinlock);
}

VOID aip_resume(VOID)
{
    aip_resume_cmd(&avif_ctx);
}

VOID send_msg2avif(MV_CC_MSG_t *pMsg)
{
	AVIF_CTX *hAvifCtx = &avif_ctx;

    if(hAvifCtx->IsPIPAudio) {
        pMsg->m_MsgID = (1 << avif_dhub_config_ChMap_avif_AUD_WR0_PIP);
    } else {
        pMsg->m_MsgID = (1 << avif_dhub_config_ChMap_avif_AUD_WR0_MAIN);
    }
    AMPMsgQ_Add(&hAvifCtx->hAIPMsgQ, pMsg);
    up(&hAvifCtx->aip_sem);
}
#endif

#ifdef CONFIG_MV_AMP_COMPONENT_AVIN_ENABLE
VOID arc_copy_spdiftx_data(AVIF_CTX *hAvifCtx)
{
    AOUT_DMA_INFO *p_dma_info;
    UINT chanId;
    AOUT_PATH_CMD_FIFO *p_arc_fifo = &hAvifCtx->arc_fifo;

	if (!p_arc_fifo->fifo_underflow)
		AoutFifoKernelRdUpdate(p_arc_fifo, 1);

	if (AoutFifoCheckKernelFullness(p_arc_fifo)) {
		p_arc_fifo->fifo_underflow = 0;
		p_dma_info =
			(AOUT_DMA_INFO *)
			AoutFifoGetKernelRdDMAInfo(p_arc_fifo, 0);
		chanId = avioDhubChMap_ag_SPDIF_R;
		dhub_channel_write_cmd(&AVIF_dhubHandle.dhub,
				       avif_dhub_config_ChMap_avif_AUD_RD0,
				       p_dma_info->addr0,
				       p_dma_info->size0, 0, 0, 0,
				       p_dma_info->size1 ? 0 : 1, 0, 0);
		if (p_dma_info->size1){
			dhub_channel_write_cmd(&AVIF_dhubHandle.dhub,
					       avif_dhub_config_ChMap_avif_AUD_RD0,
					       p_dma_info->addr1,
					       p_dma_info->size1, 0, 0,
					       0, 1, 0, 0);
		}
	} else {
		p_arc_fifo->fifo_underflow = 1;
		dhub_channel_write_cmd(&AVIF_dhubHandle.dhub, avif_dhub_config_ChMap_avif_AUD_RD0,
				       p_arc_fifo->zero_buffer,
				       p_arc_fifo->zero_buffer_size,
				       0, 0, 0, 1, 0, 0);
	}
	return;
}

void aip_avif_start_cmd(AVIF_CTX *hAvifCtx, INT *aip_info, void *param)
{
	INT *p = aip_info;
	INT chanId, pair;
	INT channel;
	AIP_DMA_CMD *p_dma_cmd;
    AIP_DMA_CMD_FIFO *pCmdFifo;

	if ( !hAvifCtx ) {
        amp_error("aip_avif_start_cmd null handler!\n");
        return;
    }

	hAvifCtx->p_aip_cmdfifo = pCmdFifo = (AIP_DMA_CMD_FIFO *) param;
	if ( !pCmdFifo ) {
		amp_trace("aip_avif_start_cmd: p_aip_fifo is NULL\n");
		return;
	}

	if(hAvifCtx->IsPIPAudio)
		chanId = avif_dhub_config_ChMap_avif_AUD_WR0_PIP;
	else
		chanId = avif_dhub_config_ChMap_avif_AUD_WR0_MAIN;

	channel = chanId;
	if (*p == 1)
	{
		hAvifCtx->aip_i2s_pair = 1;
		pCmdFifo = (AIP_DMA_CMD_FIFO *) param;
		p_dma_cmd =
			(AIP_DMA_CMD *) AIPFifoGetKernelPreRdDMAInfo(pCmdFifo, 0);

		dhub_channel_write_cmd(&AVIF_dhubHandle.dhub, chanId,
				       p_dma_cmd->addr0, p_dma_cmd->size0, 0, 0,
				       0, 1, 0, 0);
		AIPFifoKernelPreRdUpdate(pCmdFifo, 1);

		// push 2nd dHub command
		p_dma_cmd =
			(AIP_DMA_CMD *) AIPFifoGetKernelPreRdDMAInfo(pCmdFifo, 0);
		dhub_channel_write_cmd(&AVIF_dhubHandle.dhub, chanId,
				       p_dma_cmd->addr0, p_dma_cmd->size0, 0, 0,
				       0, 1, 0, 0);
		AIPFifoKernelPreRdUpdate(pCmdFifo, 1);
	} else if (*p == 4) {
		/* 4 I2S will be INTroduced since BG2 A0 */
		hAvifCtx->aip_i2s_pair = 4;

		for (pair = 0; pair < 4; pair++) {
			p_dma_cmd =
				(AIP_DMA_CMD *)
				AIPFifoGetKernelPreRdDMAInfo(pCmdFifo, pair);
			chanId = channel + pair;
			dhub_channel_write_cmd(&AVIF_dhubHandle.dhub, chanId,
					       p_dma_cmd->addr0,
					       p_dma_cmd->size0, 0, 0, 0, 1, 0,
					       0);
		}
		AIPFifoKernelPreRdUpdate(pCmdFifo, 1);

		// push 2nd dHub command
		for (pair = 0; pair < 4; pair++) {
			p_dma_cmd = (AIP_DMA_CMD *)
				AIPFifoGetKernelPreRdDMAInfo(pCmdFifo, pair);
			chanId = channel + pair;
			dhub_channel_write_cmd(&AVIF_dhubHandle.dhub, chanId,
					       p_dma_cmd->addr0,
					       p_dma_cmd->size0, 0, 0, 0, 1, 0,
					       0);
		}
		AIPFifoKernelPreRdUpdate(pCmdFifo, 1);
        }
}

void aip_avif_stop_cmd(AVIF_CTX *hAvifCtx)
{
    if ( !hAvifCtx ) {
        amp_error("aip_stop_cmd null handler!\n");
        return;
    }
	hAvifCtx->p_aip_cmdfifo = NULL;
}

void aip_avif_resume_cmd(AVIF_CTX *hAvifCtx)
{
	AIP_DMA_CMD *p_dma_cmd;
	UINT chanId, channel;
	INT pair;
    AIP_DMA_CMD_FIFO *pCmdFifo;

	if ( !hAvifCtx ) {
        amp_error("aip_avif_resume_cmd null handler!\n");
        return;
    }

    pCmdFifo = hAvifCtx->p_aip_cmdfifo;
	if ( !pCmdFifo ) {
		amp_trace("aip_avif_resume_cmd:p_aip_fifo is NULL\n");
		return;
	}

	if (!pCmdFifo->fifo_overflow)
		AIPFifoKernelRdUpdate(pCmdFifo, 1);

	if(hAvifCtx->IsPIPAudio)
		channel = avif_dhub_config_ChMap_avif_AUD_WR0_PIP;
	else
		channel = avif_dhub_config_ChMap_avif_AUD_WR0_MAIN;

	if (AIPFifoCheckKernelFullness(pCmdFifo))
	{
		pCmdFifo->fifo_overflow = 0;
		for (pair = 0; pair < hAvifCtx->aip_i2s_pair; pair++)
		{
			p_dma_cmd =
				(AIP_DMA_CMD *)
				AIPFifoGetKernelPreRdDMAInfo(pCmdFifo, pair);
			chanId = channel + pair;
			dhub_channel_write_cmd(&AVIF_dhubHandle.dhub, chanId,
					       p_dma_cmd->addr0,
					       p_dma_cmd->size0, 0, 0, 0,
					       p_dma_cmd->addr1 ? 0 : 1, 0, 0);
			if (p_dma_cmd->addr1) {
				dhub_channel_write_cmd(&AVIF_dhubHandle.dhub,
						       chanId, p_dma_cmd->addr1,
						       p_dma_cmd->size1, 0, 0,
						       0, 1, 0, 0);
			}
		}
		AIPFifoKernelPreRdUpdate(pCmdFifo, 1);
	}else {
		pCmdFifo->fifo_overflow = 1;
		pCmdFifo->fifo_overflow_cnt++;
		for (pair = 0; pair < hAvifCtx->aip_i2s_pair; pair++)
		{
			/* FIXME:
			 * chanid should be changed if 4 pair is supported
			 */
			chanId = channel + pair;
			dhub_channel_write_cmd(&AVIF_dhubHandle.dhub, chanId,
					       pCmdFifo->overflow_buffer,
					       pCmdFifo->overflow_buffer_size,
					       0, 0, 0, 1, 0, 0);
		}
	}
}

#endif // CONFIG_MV_AMP_COMPONENT_AVIN_ENABLE
