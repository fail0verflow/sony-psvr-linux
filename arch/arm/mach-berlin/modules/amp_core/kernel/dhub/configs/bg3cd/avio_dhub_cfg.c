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

#if (BERLIN_CHIP_VERSION_EXT < SOC_MINOR_REV_A0)
DHUB_channel_config  VPP_config[VPP_NUM_OF_CHANNELS] = {
    // BANK0
    { avioDhubChMap_vpp_MV_R, VPP_DHUB_BANK0_START_ADDR, VPP_DHUB_BANK0_START_ADDR+32, 32, (2048*2-32), dHubChannel_CFG_MTU_128byte, 1, 0, 1},
    { avioDhubChMap_vpp_MV_FRC_R, VPP_DHUB_BANK0_START_ADDR + 2048*2, VPP_DHUB_BANK0_START_ADDR + 2048*2+32, 32, (2048-32), dHubChannel_CFG_MTU_128byte, 1, 0, 1},
    { avioDhubChMap_vpp_MV_FRC_W, VPP_DHUB_BANK0_START_ADDR + 2048*3, VPP_DHUB_BANK0_START_ADDR + 2048*3+32, 32, (2048-32), dHubChannel_CFG_MTU_128byte, 0, 0, 1},
    // BANK1
    { avioDhubChMap_vpp_DINT0_R,VPP_DHUB_BANK1_START_ADDR, VPP_DHUB_BANK1_START_ADDR+32, 32, (8192-32), dHubChannel_CFG_MTU_128byte, 1, 0, 1},
    // BANK2
    { avioDhubChMap_vpp_DINT1_R,VPP_DHUB_BANK2_START_ADDR, VPP_DHUB_BANK2_START_ADDR+32, 32, (8192-32), dHubChannel_CFG_MTU_128byte, 1, 0, 1},
    // BANK3
    { avioDhubChMap_vpp_DINT_W,VPP_DHUB_BANK3_START_ADDR, VPP_DHUB_BANK3_START_ADDR+32, 32, (8192-32), dHubChannel_CFG_MTU_128byte, 1, 0, 1},
    // Bank4
    { avioDhubChMap_vpp_BCM_R, VPP_DHUB_BANK4_START_ADDR, VPP_DHUB_BANK4_START_ADDR+128, 128, (1024-128), dHubChannel_CFG_MTU_128byte, 0, 0, 1},
    { avioDhubChMap_vpp_HDMI_R, VPP_DHUB_BANK4_START_ADDR+1024, VPP_DHUB_BANK4_START_ADDR+1024+32, 32, (1024-32), dHubChannel_CFG_MTU_128byte, 0, 0, 1},

#if 0 /* not in use */
    { avioDhubChMap_vpp_AUX_FRC_R_Z1,DHUB_BANK5_START_ADDR+1024*2, DHUB_BANK5_START_ADDR+1024*2+32, 32, (1024-32), dHubChannel_CFG_MTU_128byte, 1, 0, 1},
    { avioDhubChMap_vpp_AUX_FRC_W_Z1,DHUB_BANK5_START_ADDR+1024*3, DHUB_BANK5_START_ADDR+1024*3+32, 32, (1024-32), dHubChannel_CFG_MTU_128byte, 1, 0, 1},
    { avioDhubChMap_vpp_TT_R_Z1,DHUB_BANK5_START_ADDR+1024*4, DHUB_BANK5_START_ADDR+1024*4+32, 32, (1024-32), dHubChannel_CFG_MTU_128byte, 0, 0, 1}
#endif
};

DHUB_channel_config  AG_config[AG_NUM_OF_CHANNELS] = {
    // Bank0
    { avioDhubChMap_ag_APPCMD_R, AG_DHUB_BANK0_START_ADDR, AG_DHUB_BANK0_START_ADDR+32, 32, (512-32), dHubChannel_CFG_MTU_128byte, 0, 0, 1},
    { avioDhubChMap_ag_MA0_R, AG_DHUB_BANK0_START_ADDR+512, AG_DHUB_BANK0_START_ADDR+512+32, 32, (1024-32), dHubChannel_CFG_MTU_128byte, 0, 0, 1},
    { avioDhubChMap_ag_MA1_R, AG_DHUB_BANK0_START_ADDR+512+1024,AG_DHUB_BANK0_START_ADDR+512+1024+32, 32, (1024-32), dHubChannel_CFG_MTU_128byte, 0, 0, 1},
    { avioDhubChMap_ag_MA2_R, AG_DHUB_BANK0_START_ADDR+512+1024*2,AG_DHUB_BANK0_START_ADDR+512+1024*2+32, 32, (1024-32), dHubChannel_CFG_MTU_128byte, 0, 0, 1},
    { avioDhubChMap_ag_MA3_R, AG_DHUB_BANK0_START_ADDR+512+1024*3,AG_DHUB_BANK0_START_ADDR+512+1024*3+32, 32, (1024-32), dHubChannel_CFG_MTU_128byte, 0, 0, 1},
    { avioDhubChMap_ag_SA_R, AG_DHUB_BANK0_START_ADDR+512+1024*4,AG_DHUB_BANK0_START_ADDR+512+1024*4+32, 32, (1024-32), dHubChannel_CFG_MTU_128byte, 0, 0, 1},
    { avioDhubChMap_ag_SPDIF_R, AG_DHUB_BANK0_START_ADDR+512+1024*5, AG_DHUB_BANK0_START_ADDR+512+1024*5+32, 32, (512-32), dHubChannel_CFG_MTU_128byte, 0, 0, 1},
    { avioDhubChMap_ag_APPDAT_R, AG_DHUB_BANK0_START_ADDR+512+1024*6, AG_DHUB_BANK0_START_ADDR+512+1024*6+32, 32, (512-32), dHubChannel_CFG_MTU_128byte, 0, 0, 1},
    { avioDhubChMap_ag_APPDAT_W, AG_DHUB_BANK0_START_ADDR+1024*7, AG_DHUB_BANK0_START_ADDR+1024*7+32, 32, (512-32), dHubChannel_CFG_MTU_128byte, 0, 0, 1},
    // Bank1
    { avioDhubChMap_ag_CSR_R, AG_DHUB_BANK1_START_ADDR, AG_DHUB_BANK1_START_ADDR+32, 32, (8192-32), dHubChannel_CFG_MTU_128byte, 1, 0, 1},
    // Bank2
    { avioDhubChMap_ag_GFX_R, AG_DHUB_BANK2_START_ADDR, AG_DHUB_BANK2_START_ADDR+32, 32, (8192-32), dHubChannel_CFG_MTU_128byte, 1, 0, 1},
};

#else/* (BERLIN_CHIP_VERSION_EXT >= SOC_MINOR_REV_A0) */

DHUB_channel_config  VPP_config[VPP_NUM_OF_CHANNELS] = {
    // BANK0
    { avioDhubChMap_vpp_MV_R, VPP_DHUB_BANK0_START_ADDR, VPP_DHUB_BANK0_START_ADDR+32, 32, (2048*2-32), dHubChannel_CFG_MTU_128byte, 1, 0, 1},
    { avioDhubChMap_vpp_MV_FRC_R, VPP_DHUB_BANK0_START_ADDR + 2048*2, VPP_DHUB_BANK0_START_ADDR + 2048*2+32, 32, (2048-32), dHubChannel_CFG_MTU_128byte, 1, 0, 1},
    { avioDhubChMap_vpp_MV_FRC_W, VPP_DHUB_BANK0_START_ADDR + 2048*3, VPP_DHUB_BANK0_START_ADDR + 2048*3+32, 32, (2048-32), dHubChannel_CFG_MTU_128byte, 0, 0, 1},
    // Bank1
    { avioDhubChMap_vpp_BCM_R, VPP_DHUB_BANK1_START_ADDR, VPP_DHUB_BANK1_START_ADDR+128, 128, (2048-128), dHubChannel_CFG_MTU_128byte, 0, 0, 1},
    { avioDhubChMap_vpp_HDMI_R, VPP_DHUB_BANK1_START_ADDR+2048, VPP_DHUB_BANK1_START_ADDR+2048+32, 32, (2048-32), dHubChannel_CFG_MTU_128byte, 0, 0, 1},
};

DHUB_channel_config  AG_config[AG_NUM_OF_CHANNELS] = {
    // Bank0
    { avioDhubChMap_ag_MA0_R, AG_DHUB_BANK0_START_ADDR, AG_DHUB_BANK0_START_ADDR+32, 32, (512-32), dHubChannel_CFG_MTU_128byte, 0, 0, 1},
    { avioDhubChMap_ag_MA1_R, AG_DHUB_BANK0_START_ADDR+512,AG_DHUB_BANK0_START_ADDR+512+32, 32, (512-32), dHubChannel_CFG_MTU_128byte, 0, 0, 1},
    { avioDhubChMap_ag_MA2_R, AG_DHUB_BANK0_START_ADDR+512*2,AG_DHUB_BANK0_START_ADDR+512*2+32, 32, (512-32), dHubChannel_CFG_MTU_128byte, 0, 0, 1},
    { avioDhubChMap_ag_MA3_R, AG_DHUB_BANK0_START_ADDR+512*3,AG_DHUB_BANK0_START_ADDR+512*3+32, 32, (512-32), dHubChannel_CFG_MTU_128byte, 0, 0, 1},
    { avioDhubChMap_ag_SA0_R, AG_DHUB_BANK0_START_ADDR+512*4,AG_DHUB_BANK0_START_ADDR+512*4+32, 32, (1024-32), dHubChannel_CFG_MTU_128byte, 0, 0, 1},
    { avioDhubChMap_ag_MIC0_W, AG_DHUB_BANK0_START_ADDR+1024*3, AG_DHUB_BANK0_START_ADDR+1024*3+32, 32, (512-32), dHubChannel_CFG_MTU_128byte, 0, 0, 1},
    { avioDhubChMap_ag_MIC1_W, AG_DHUB_BANK0_START_ADDR+512+1024*3, AG_DHUB_BANK0_START_ADDR+512+1024*3+32, 32, (512-32), dHubChannel_CFG_MTU_128byte, 0, 0, 1},
    // Bank1
    { avioDhubChMap_ag_CSR_R, AG_DHUB_BANK1_START_ADDR, AG_DHUB_BANK1_START_ADDR+32, 32, (8192-32), dHubChannel_CFG_MTU_128byte, 1, 0, 1},
    // Bank2
    { avioDhubChMap_ag_GFX_R, AG_DHUB_BANK2_START_ADDR, AG_DHUB_BANK2_START_ADDR+32, 32, (8192-32), dHubChannel_CFG_MTU_128byte, 1, 0, 1},
};
#endif

