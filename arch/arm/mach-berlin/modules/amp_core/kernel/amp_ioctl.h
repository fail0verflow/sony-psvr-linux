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
#ifndef _AMP_IOCTL_H_
#define _AMP_IOCTL_H_
#include <linux/version.h>

#include "amp_type.h"

#define VPP_IOCTL_VBI_DMA_CFGQ              0xbeef0001
#define VPP_IOCTL_VBI_BCM_CFGQ              0xbeef0002
#define VPP_IOCTL_VDE_BCM_CFGQ              0xbeef0003
#define VPP_IOCTL_GET_MSG                   0xbeef0004
#define VPP_IOCTL_START_BCM_TRANSACTION	    0xbeef0005
#define VPP_IOCTL_BCM_SCHE_CMD              0xbeef0006
#define VPP_IOCTL_INTR_MSG				    0xbeef0007
#define CEC_IOCTL_RX_MSG_BUF_MSG            0xbeef0008

#define VDEC_IOCTL_ENABLE_INT               0xbeef1001

#define AOUT_IOCTL_START_CMD            0xbeef2001
#define AOUT_IOCTL_STOP_CMD             0xbeef2002
#define AIP_IOCTL_START_CMD             0xbeef2003
#define AIP_IOCTL_STOP_CMD              0xbeef2004
#define AIP_AVIF_IOCTL_START_CMD        0xbeef2005
#define AIP_AVIF_IOCTL_STOP_CMD	        0xbeef2006
#define AIP_AVIF_IOCTL_SET_MODE         0xbeef2007
#define AIP_IOCTL_GET_MSG_CMD           0xbeef2008
#define AIP_IOCTL_SemUp_CMD             0xbeef2009
#define APP_IOCTL_INIT_CMD              0xbeef3001
#define APP_IOCTL_START_CMD             0xbeef3002
#define APP_IOCTL_DEINIT_CMD            0xbeef3003
#define APP_IOCTL_GET_MSG_CMD           0xbeef3004
#define AOUT_IOCTL_GET_MSG_CMD          0xbeef3005
#define ZSP_IOCTL_GET_MSG_CMD           0xbeef3006

#define VIP_IOCTL_GET_MSG               0xbeef4001
#define VIP_IOCTL_VBI_BCM_CFGQ          0xbeef4002
#define VIP_IOCTL_SD_WRE_CFGQ           0xbeef4003
#define VIP_IOCTL_SD_RDE_CFGQ           0xbeef4004
#define VIP_IOCTL_SEND_MSG              0xbeef4005
#define VIP_IOCTL_VDE_BCM_CFGQ          0xbeef4006
#define VIP_IOCTL_INTR_MSG              0xbeef4007

#define AVIF_IOCTL_GET_MSG              0xbeef6001
#define AVIF_IOCTL_VBI_CFGQ             0xbeef6002
#define AVIF_IOCTL_SD_WRE_CFGQ          0xbeef6003
#define AVIF_IOCTL_SD_RDE_CFGQ          0xbeef6004
#define AVIF_IOCTL_SEND_MSG             0xbeef6005
#define AVIF_IOCTL_VDE_CFGQ             0xbeef6006
#define AVIF_IOCTL_INTR_MSG             0xbeef6007

#define AVIF_HRX_IOCTL_GET_MSG          0xbeef6050
#define AVIF_HRX_IOCTL_SEND_MSG         0xbeef6051
#define AVIF_VDEC_IOCTL_GET_MSG         0xbeef6052
#define AVIF_VDEC_IOCTL_SEND_MSG        0xbeef6053
#define AVIF_HRX_IOCTL_ARC_SET          0xbeef6054
#define AVIF_HDCP_IOCTL_GET_MSG         0xbeef6055
#define AVIF_HDCP_IOCTL_SEND_MSG        0xbeef6056
#define AVIF_HRX_IOCTL_SET_FACTORY        0xbeef6057
#define HDMIRX_IOCTL_GET_MSG            0xbeef5001
#define HDMIRX_IOCTL_SEND_MSG           0xbeef5002

#define MAX_INTR_NUM 0x20

typedef struct INTR_MSG_T {
    UINT32 DhubSemMap;
    UINT32 Enable;
} INTR_MSG;

typedef enum {
	MULTI_PATH = 0,
	LoRo_PATH = 1,
	SPDIF_PATH = 2,
	HDMI_PATH = 3,
	MAX_OUTPUT_AUDIO = 5,
} AUDIO_PATH;

typedef struct {
    struct ion_handle *ionh[MAX_OUTPUT_AUDIO];
} AOUT_ION_HANDLE;

typedef struct {
    struct ion_handle *ionh;
} AIP_ION_HANDLE;

typedef struct aout_dma_info_t {
	UINT32 addr0;
	UINT32 size0;
	UINT32 addr1;
	UINT32 size1;
} AOUT_DMA_INFO;

typedef struct aout_path_cmd_fifo_t {
	AOUT_DMA_INFO aout_dma_info[4][8];
	UINT32 update_pcm[8];
	UINT32 takeout_size[8];
	UINT32 size;
	UINT32 wr_offset;
	UINT32 rd_offset;
	UINT32 kernel_rd_offset;
	UINT32 zero_buffer;
	UINT32 zero_buffer_size;
	UINT32 fifo_underflow;
} AOUT_PATH_CMD_FIFO;

typedef struct aip_dma_cmd_t {
	UINT32 addr0;
	UINT32 size0;
	UINT32 addr1;
	UINT32 size1;
} AIP_DMA_CMD;

typedef struct aip_cmd_fifo_t {
	AIP_DMA_CMD aip_dma_cmd[4][8];
	UINT32 update_pcm[8];
	UINT32 takein_size[8];
	UINT32 size;
	UINT32 wr_offset;
	UINT32 rd_offset;
	UINT32 kernel_rd_offset;
	UINT32 prev_fifo_overflow_cnt;
	/* used by kernel */
	UINT32 kernel_pre_rd_offset;
	UINT32 overflow_buffer;
	UINT32 overflow_buffer_size;
	UINT32 fifo_overflow;
	UINT32 fifo_overflow_cnt;
} AIP_DMA_CMD_FIFO;

typedef struct {
	UINT uiShmOffset;	/**< Not used by kernel */
	UINT *unaCmdBuf;
	UINT *cmd_buffer_base;
	UINT max_cmd_size;
	UINT cmd_len;
	UINT cmd_buffer_hw_base;
	VOID *p_cmd_tag;
} APP_CMD_BUFFER;

typedef struct {
	UINT uiShmOffset;	/**< Not used by kernel */
	APP_CMD_BUFFER in_coef_cmd[8];
	APP_CMD_BUFFER out_coef_cmd[8];
	APP_CMD_BUFFER in_data_cmd[8];
	APP_CMD_BUFFER out_data_cmd[8];
	UINT size;
	UINT wr_offset;
	UINT rd_offset;
	UINT kernel_rd_offset;
/************** used by Kernel *****************/
	UINT kernel_idle;
} HWAPP_CMD_FIFO;

#endif    //_AMP_IOCTL_H_
