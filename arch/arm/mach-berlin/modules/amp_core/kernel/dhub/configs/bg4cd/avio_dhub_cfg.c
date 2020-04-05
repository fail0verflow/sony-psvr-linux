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
#include "api_avio_dhub.h"
DHUB_channel_config  VPP_config[VPP_NUM_OF_CHANNELS] = {
    // BANK0
    { avioDhubChMap_vpp128b_MV_R0, VPP_DHUB_BANK0_START_ADDR, VPP_DHUB_BANK0_START_ADDR+64, 64, (1024*9-64), dHubChannel_CFG_MTU_128byte, 1, 0, 1},
    { avioDhubChMap_vpp128b_VMM_VX_R, VPP_DHUB_BANK0_START_ADDR+1024*9, VPP_DHUB_BANK0_START_ADDR+1024*9+64, 64, (1024-64), dHubChannel_CFG_MTU_128byte, 1, 0, 1},
    // BANK1
    { avioDhubChMap_vpp128b_MV_R1, VPP_DHUB_BANK1_START_ADDR, VPP_DHUB_BANK1_START_ADDR+64, 64, (1024*2-64), dHubChannel_CFG_MTU_128byte, 1, 0, 1},
    { avioDhubChMap_vpp128b_DINT_W, VPP_DHUB_BANK1_START_ADDR+1024*2, VPP_DHUB_BANK1_START_ADDR+1024*2+64, 64, (1024*4-64), dHubChannel_CFG_MTU_128byte, 1, 0, 1},
    { avioDhubChMap_vpp128b_DINT0_R, VPP_DHUB_BANK1_START_ADDR+1024*6, VPP_DHUB_BANK1_START_ADDR+1024*6+64, 64, (1024*4-64), dHubChannel_CFG_MTU_128byte, 1, 0, 1},
    //BANK2
    { avioDhubChMap_vpp128b_GFX_PIP_R0, VPP_DHUB_BANK2_START_ADDR, VPP_DHUB_BANK2_START_ADDR+64, 64, (1024*8-64), dHubChannel_CFG_MTU_128byte, 1, 0, 1},
    { avioDhubChMap_vpp128b_PIP_R1, VPP_DHUB_BANK2_START_ADDR+1024*8, VPP_DHUB_BANK2_START_ADDR+1024*8+64, 64, (1024*2-64), dHubChannel_CFG_MTU_128byte, 1, 0, 1},
    //BANK3
    { avioDhubChMap_vpp128b_GFX_R1, VPP_DHUB_BANK3_START_ADDR, VPP_DHUB_BANK3_START_ADDR+64, 64, (1024*8-64), dHubChannel_CFG_MTU_128byte, 1, 0, 1},
    { avioDhubChMap_vpp128b_DINT1_R, VPP_DHUB_BANK3_START_ADDR+1024*8, VPP_DHUB_BANK3_START_ADDR+1024*8+64, 64, (1024*2-64), dHubChannel_CFG_MTU_128byte, 1, 0, 1},
};

DHUB_channel_config  AG_config[AG_NUM_OF_CHANNELS] = {
    // Bank0
	{ avioDhubChMap_aio64b_MA0_R, AG_DHUB_BANK0_START_ADDR, AG_DHUB_BANK0_START_ADDR+32, 32, (256-32), dHubChannel_CFG_MTU_128byte, 0, 0, 1},
    { avioDhubChMap_aio64b_MA1_R, AG_DHUB_BANK0_START_ADDR+256, AG_DHUB_BANK0_START_ADDR+256+32, 32, (256-32), dHubChannel_CFG_MTU_128byte, 0, 0, 1},
    { avioDhubChMap_aio64b_MA2_R, AG_DHUB_BANK0_START_ADDR+256*2,AG_DHUB_BANK0_START_ADDR+256*2+32, 32, (256-32), dHubChannel_CFG_MTU_128byte, 0, 0, 1},
    { avioDhubChMap_aio64b_MA3_R, AG_DHUB_BANK0_START_ADDR+256*3,AG_DHUB_BANK0_START_ADDR+256*3+32, 32, (256-32), dHubChannel_CFG_MTU_128byte, 0, 0, 1},
    { avioDhubChMap_aio64b_SA_R, AG_DHUB_BANK0_START_ADDR+256*4,AG_DHUB_BANK0_START_ADDR+256*4+32, 32, (256-32), dHubChannel_CFG_MTU_128byte, 0, 0, 1},
    { avioDhubChMap_aio64b_PDM_MIC_CH1_W, AG_DHUB_BANK0_START_ADDR+256*5, AG_DHUB_BANK0_START_ADDR+256*5+32, 32, (256-32), dHubChannel_CFG_MTU_128byte, 0, 0, 1},
    { avioDhubChMap_aio64b_PDM_MIC_CH2_W, AG_DHUB_BANK0_START_ADDR+256*6, AG_DHUB_BANK0_START_ADDR+256*6+32, 32, (256-32), dHubChannel_CFG_MTU_128byte, 0, 0, 1},
    { avioDhubChMap_aio64b_HDMI_R, AG_DHUB_BANK0_START_ADDR+256*7, AG_DHUB_BANK0_START_ADDR+256*7+32, 32, (256-32), dHubChannel_CFG_MTU_128byte, 0, 0, 1},
    { avioDhubChMap_aio64b_BCM_R, AG_DHUB_BANK0_START_ADDR+256*8, AG_DHUB_BANK0_START_ADDR+256*8+512, 512, (1024-512), dHubChannel_CFG_MTU_128byte, 0, 0, 1},
    { avioDhubChMap_aio64b_TT_R, AG_DHUB_BANK0_START_ADDR+256*8+1024, AG_DHUB_BANK0_START_ADDR+256*8+1024+32, 32, (256-32), dHubChannel_CFG_MTU_128byte, 0, 0, 1},
};

