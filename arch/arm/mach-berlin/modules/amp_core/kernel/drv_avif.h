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
#ifndef _DRV_AVIF_H_
#define _DRV_AVIF_H_
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
#include "drv_msg.h"
#include "amp_ioctl.h"

// TODO:: please add descriptions

#define AIP_SOUECE_SPDIF 1

#define AVIF_ISR_MSGQ_SIZE		64
#define AVIF_HRX_ISR_MSGQ_SIZE	64
#define AVIF_HDCP_ISR_MSGQ_SIZE	64
#define AVIF_VDEC_ISR_MSGQ_SIZE	64
#define AIP_ISR_MSGQ_SIZE			16
#define AVIF_MSG_DESTROY_ISR_TASK   1

#define HDMIRX_FACTORY_IGNORE_GMD_FIX    1
typedef struct _AIP_CONTEXT_ {
    INT aip_source;
	INT aip_i2s_pair;
	INT IsPIPAudio;

	AIP_DMA_CMD_FIFO *p_aip_cmdfifo;
	AOUT_PATH_CMD_FIFO arc_fifo;
    AIP_ION_HANDLE  gstAipIon;

    AMPMsgQ_t hPEAVIFMsgQ;
    AMPMsgQ_t hPEAVIFHRXMsgQ;
    AMPMsgQ_t hPEAVIFHDCPMsgQ;
    AMPMsgQ_t hPEAVIFVDECMsgQ;
	AMPMsgQ_t hAIPMsgQ;

    atomic_t avif_isr_msg_err_cnt;
	spinlock_t aip_spinlock;
    struct semaphore aip_sem;
    struct semaphore avif_sem;
    struct semaphore avif_hrx_sem;
    struct semaphore avif_hdcp_sem;
    struct semaphore avif_vdec_sem;

    UINT32 factory_mode;
    UINT32 avif_intr_status[MAX_INTR_NUM];
} AVIF_CTX;

AVIF_CTX * drv_avif_init(VOID);
VOID drv_avif_exit(AVIF_CTX *hAvifCtx);
VOID send_msg2avif(MV_CC_MSG_t *pMsg);
irqreturn_t amp_devices_avif_isr(INT irq, VOID *dev_id);

VOID aip_start_cmd(AVIF_CTX *hAipCtx, INT *aip_info, VOID *param);
VOID aip_stop_cmd(AVIF_CTX *hAipCtx);
VOID aip_resume_cmd(AVIF_CTX *hAipCtx);
VOID aip_resume(VOID);

VOID aip_avif_start_cmd(AVIF_CTX *hAipCtx, INT *aip_info, VOID *param);
VOID aip_avif_stop_cmd(AVIF_CTX *hAipCtx);
VOID aip_avif_resume_cmd(AVIF_CTX *hAipCtx);
VOID arc_copy_spdiftx_data(AVIF_CTX *hAvifCtx);
VOID HDMIRX_power_down(VOID);

/* Registers */
#define HDMI_RX_BASE				0x3900
#define HDMIRX_INTR_EN_0			(HDMI_RX_BASE + 0x0160)
#define HDMIRX_INTR_EN_1			(HDMI_RX_BASE + 0x0161)
#define HDMIRX_INTR_EN_2			(HDMI_RX_BASE + 0x0162)
#define HDMIRX_INTR_EN_3			(HDMI_RX_BASE + 0x0163)
#define HDMIRX_INTR_STATUS_0			(HDMI_RX_BASE + 0x0164)
#define HDMIRX_INTR_STATUS_1			(HDMI_RX_BASE + 0x0165)
#define HDMIRX_INTR_STATUS_2			(HDMI_RX_BASE + 0x0166)
#define HDMIRX_INTR_STATUS_3			(HDMI_RX_BASE + 0x0167)
#define HDMIRX_INTR_CLR_0			(HDMI_RX_BASE + 0x0168)
#define HDMIRX_INTR_CLR_1			(HDMI_RX_BASE + 0x0169)
#define HDMIRX_INTR_CLR_2			(HDMI_RX_BASE + 0x016A)
#define HDMIRX_INTR_CLR_3			(HDMI_RX_BASE + 0x016B)
#define HDMIRX_PHY_CAL_TRIG			(HDMI_RX_BASE + 0x0461)	//new register
#define HDMIRX_PHY_PM_TRIG			(HDMI_RX_BASE + 0x0462) //new register

#define HDMI_RX_INT_ACR_N				0x01
#define HDMI_RX_INT_ACR_CTS			0x02
#define HDMI_RX_INT_GCP_AVMUTE		0x04
#define HDMI_RX_INT_GCP_COLOR_DEPTH	0x08
#define HDMI_RX_INT_GCP_PHASE			0x10
#define HDMI_RX_INT_ACP_PKT			0x20
#define HDMI_RX_INT_ISRC1_PKT			0x40
#define HDMI_RX_INT_ISRC2_PKT			0x80
#define HDMI_RX_INT_ALL_INTR0			0xFF

#define HDMI_RX_INT_GAMUT_PKT			0x01
#define HDMI_RX_INT_VENDOR_PKT		0x02
#define HDMI_RX_INT_AVI_INFO			0x04
#define HDMI_RX_INT_SPD_INFO			0x08
#define HDMI_RX_INT_AUD_INFO			0x10
#define HDMI_RX_INT_MPEG_INFO			0x20
#define HDMI_RX_INT_CHNL_STATUS		0x40
#define HDMI_RX_INT_TMDS_MODE			0x80
#define HDMI_RX_INT_ALL_INTR1			0xFF

#define HDMI_RX_INT_AUTH_STARTED		0x01
#define HDMI_RX_INT_AUTH_COMPLETE	0x02
#define HDMI_RX_INT_VSYNC				0x04	// new
#define HDMI_RX_INT_VSI_STOP			0x10	// new
#define HDMI_RX_INT_SYNC_DET			0x20
#define HDMI_RX_INT_VRES_CHG			0x40
#define HDMI_RX_INT_HRES_CHG			0x80
#define HDMI_RX_INT_ALL_INTR2			0xFF

#define HDMI_RX_INT_5V_PWR              0x02
#define HDMI_RX_INT_CLOCK_CHANGE        0x04
#define HDMI_RX_INT_EDID                0x08
#define HDMI_RX_INT_WR_MSSG_STARTED     0x80
#define HDMI_RX_INT_ALL_INTR3           0xFF
#define HDMI_RX_INT_PRT                 0x04

#define HDMIRX_INTR_SYNC				0x01
#define HDMIRX_INTR_HDCP				0x02
#define HDMIRX_INTR_EDID				0x03
#define HDMIRX_INTR_PKT				0x04
#define HDMIRX_INTR_AVMUTE			0x05
#define HDMIRX_INTR_TMDS				0x06
#define HDMIRX_INTR_CHNL_STS			0x07
#define HDMIRX_INTR_CLR_DEPTH			0x08
#define HDMIRX_INTR_VSI_STOP			0x09
#define HDMIRX_INTR_GMD_PKT			0x0A
#define HDMIRX_INTR_HDCP_2X			0x0B
#define HDMIRX_INTR_PRT                         0x0C

#define BG2Q4K_CHIP_REV_ADDRESS 0xF7EA0004

#define AVIF_INTR_VDEC_SYNC         0x01
#define AVIF_INTR_VGA_SCYN          0x02

#define CPU_INTR_MASK0_EXTREG_ADDR		SOC_AVIF_BASE+((CPU_SS_MBASE_ADDR + 0x003D)<<2)
#define CPU_INTR_MASK1_EXTREG_ADDR		SOC_AVIF_BASE+((CPU_SS_MBASE_ADDR + 0x0020)<<2)
#define CPU_INTR_MASK2_EXTREG_ADDR		SOC_AVIF_BASE+((CPU_SS_MBASE_ADDR + 0x0021)<<2)
#define CPU_INTR_MASK3_EXTREG_ADDR		SOC_AVIF_BASE+((CPU_SS_MBASE_ADDR + 0x0022)<<2)
#define CPU_INTR_MASK4_EXTREG_ADDR		SOC_AVIF_BASE+((CPU_SS_MBASE_ADDR + 0x0023)<<2)

#define CPU_INTR_MASK0_STATUS_EXTREG_ADDR SOC_AVIF_BASE+((CPU_SS_MBASE_ADDR + 0x003E)<<2)
#define CPU_INTR_MASK1_STATUS_EXTREG_ADDR SOC_AVIF_BASE+((CPU_SS_MBASE_ADDR + 0x0028)<<2)
#define CPU_INTR_MASK2_STATUS_EXTREG_ADDR SOC_AVIF_BASE+((CPU_SS_MBASE_ADDR + 0x0029)<<2)
#define CPU_INTR_MASK3_STATUS_EXTREG_ADDR SOC_AVIF_BASE+((CPU_SS_MBASE_ADDR + 0x002A)<<2)
#define CPU_INTR_MASK4_STATUS_EXTREG_ADDR SOC_AVIF_BASE+((CPU_SS_MBASE_ADDR + 0x002B)<<2)

#define CPU_INTR_CLR0_EXTREG_ADDR		SOC_AVIF_BASE+((CPU_SS_MBASE_ADDR + 0x004D)<<2)
#define CPU_INTR_CLR1_EXTREG_ADDR		SOC_AVIF_BASE+((CPU_SS_MBASE_ADDR + 0x002C)<<2)
#define CPU_INTR_CLR2_EXTREG_ADDR		SOC_AVIF_BASE+((CPU_SS_MBASE_ADDR + 0x002D)<<2)
#define CPU_INTR_CLR3_EXTREG_ADDR		SOC_AVIF_BASE+((CPU_SS_MBASE_ADDR + 0x002E)<<2)
#define CPU_INTR_CLR4_EXTREG_ADDR		SOC_AVIF_BASE+((CPU_SS_MBASE_ADDR + 0x002F)<<2)

#define CPU_INTR_RAW_STATUS0_EXTREG_ADDR	SOC_AVIF_BASE+((CPU_SS_MBASE_ADDR + 0x003C)<<2)
#define CPU_INTR_RAW_STATUS1_EXTREG_ADDR	SOC_AVIF_BASE+((CPU_SS_MBASE_ADDR + 0x0024)<<2)
#define CPU_INTR_RAW_STATUS2_EXTREG_ADDR	SOC_AVIF_BASE+((CPU_SS_MBASE_ADDR + 0x0025)<<2)
#define CPU_INTR_RAW_STATUS3_EXTREG_ADDR	SOC_AVIF_BASE+((CPU_SS_MBASE_ADDR + 0x0026)<<2)
#define CPU_INTR_RAW_STATUS4_EXTREG_ADDR	SOC_AVIF_BASE+((CPU_SS_MBASE_ADDR + 0x0027)<<2)
#define CPU_INTCPU_2_EXTCPU_MASK0INTR_ADDR SOC_AVIF_BASE+(CPU_SS_MBASE_ADDR + 0x0048)*4
#define CPU_INTCPU_2_EXTCPU_INTR0_ADDR SOC_AVIF_BASE+(CPU_SS_MBASE_ADDR + 0x003F)*4
#define CPU_INTCPU_2_EXTCPU_INTR1_ADDR SOC_AVIF_BASE+(CPU_SS_MBASE_ADDR + 0x0040)*4
#define CPU_INTCPU_2_EXTCPU_INTR2_ADDR SOC_AVIF_BASE+(CPU_SS_MBASE_ADDR + 0x0041)*4
#define CPU_INTCPU_2_EXTCPU_INTR3_ADDR SOC_AVIF_BASE+(CPU_SS_MBASE_ADDR + 0x0042)*4
#define CPU_INTCPU_2_EXTCPU_INTR4_ADDR SOC_AVIF_BASE+(CPU_SS_MBASE_ADDR + 0x0043)*4
#define CPU_INTCPU_2_EXTCPU_INTR5_ADDR SOC_AVIF_BASE+(CPU_SS_MBASE_ADDR + 0x0044)*4
#define CPU_INTCPU_2_EXTCPU_INTR6_ADDR SOC_AVIF_BASE+(CPU_SS_MBASE_ADDR + 0x0045)*4
#define CPU_INTCPU_2_EXTCPU_INTR7_ADDR SOC_AVIF_BASE+(CPU_SS_MBASE_ADDR + 0x0046)*4

//intr0 reg
#define AVIF_INTR_TIMER1				 0x01
#define AVIF_INTR_TIMER0				0x02
#define AVIF_INTR_PIP_VDE				0x04
#define AVIF_INTR_MAIN_VDE			0x08
#define AVIF_INTR_VGA_SYNC_MON		0x10
#define AVIF_INTR_VGA_DI				0x20
#define AVIF_INTR_DELAYED_MAIN_VDE	0x40

#define SOC_AVIF_BASE						0xF7980000
#define AVIF_HRX_BASE_OFFSET				0x800
#define CPU_SS_MBASE_ADDR					0x960

#define AVIF_INTR_DELAYED_PIP_VDE	0x20
#define AVIF_HRX_BASE					0x0C00

#define SOC_HDMIRX_BASE             0xF79E0000
#define RA_HDRX_INTR_EN             (SOC_HDMIRX_BASE + 0x0168)
#define RA_HDRX_INTR_EN1            (SOC_HDMIRX_BASE + 0x016C)
#define RA_HDRX_INTR_STATUS         (SOC_HDMIRX_BASE + 0x0170)
#define RA_HDRX_INTR_STATUS1        (SOC_HDMIRX_BASE + 0x0174)
#define RA_HDRX_INTR_CLR            (SOC_HDMIRX_BASE + 0x0178)
#define RA_HDRX_INTR_CLR1           (SOC_HDMIRX_BASE + 0x017C)
#define RA_HDRX_PHY_PM_OVRWEN       (SOC_HDMIRX_BASE + 0x04AC)
#define RA_HDRX_PHY_PM_OVRW         (SOC_HDMIRX_BASE + 0x04B0)
#define RA_HDRX_SOFT_RST_CTRL       (SOC_HDMIRX_BASE + 0x002C)
#define RA_HDRX_HDCP_WR_FIFO        (SOC_HDMIRX_BASE + 0x1038)
#define RA_HDRX_HDCP_BCAPS          (SOC_HDMIRX_BASE + 0x0200)
#define RA_HDRX_HDCP_BSTATUS        (SOC_HDMIRX_BASE + 0x0234)
#endif    //_DRV_AVIF_H_
