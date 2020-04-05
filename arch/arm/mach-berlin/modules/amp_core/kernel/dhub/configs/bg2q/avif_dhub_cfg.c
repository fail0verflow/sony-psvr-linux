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
#include "api_avif_dhub.h"

HDL_dhub2d AVIF_dhubHandle;

DHUB_channel_config AVIF_config[AVIF_NUM_OF_CHANNELS] = {
//BANK 0
    { avif_dhub_config_ChMap_avif_SD_WR, AVIF_DHUB_BANK0_START_ADDR, AVIF_DHUB_BANK0_START_ADDR+32, 32, (4096-32), dHubChannel_CFG_MTU_128byte, 0, 0, 1},
    { avif_dhub_config_ChMap_avif_SD_RE, AVIF_DHUB_BANK0_START_ADDR+4096, AVIF_DHUB_BANK0_START_ADDR+4096+32, 32, (4096-32), dHubChannel_CFG_MTU_128byte, 0, 0, 1},
//BANK 1
    {avif_dhub_config_ChMap_avif_MAIN_WR,AVIF_DHUB_BANK1_START_ADDR,AVIF_DHUB_BANK1_START_ADDR+32, 32, (8192+4096-32), dHubChannel_CFG_MTU_128byte, 0, 0, 1},
//BANK 2
    {avif_dhub_config_ChMap_avif_PIP_WR,AVIF_DHUB_BANK2_START_ADDR,AVIF_DHUB_BANK2_START_ADDR+32, 32, (8192+4096-32), dHubChannel_CFG_MTU_128byte, 0, 0, 1},
//BANK 3
    {avif_dhub_config_ChMap_avif_VBI_WR,AVIF_DHUB_BANK3_START_ADDR,AVIF_DHUB_BANK3_START_ADDR+32, 32, (1024-32), dHubChannel_CFG_MTU_8byte, 0, 0, 1},
    {avif_dhub_config_ChMap_avif_AUD_WR0_MAIN,AVIF_DHUB_BANK3_START_ADDR+1024,AVIF_DHUB_BANK3_START_ADDR+1024+32, 32, (512-32), dHubChannel_CFG_MTU_128byte, 0, 0, 1},
    {avif_dhub_config_ChMap_avif_AUD_WR1_MAIN,AVIF_DHUB_BANK3_START_ADDR+1024+512,AVIF_DHUB_BANK3_START_ADDR+1024+512+32, 32, (512-32), dHubChannel_CFG_MTU_128byte, 0, 0, 1},   
    {avif_dhub_config_ChMap_avif_AUD_WR2_MAIN,AVIF_DHUB_BANK3_START_ADDR+2048,AVIF_DHUB_BANK3_START_ADDR+2048+32, 32, (512-32), dHubChannel_CFG_MTU_128byte, 0, 0, 1},
    {avif_dhub_config_ChMap_avif_AUD_WR3_MAIN,AVIF_DHUB_BANK3_START_ADDR+2048+512,AVIF_DHUB_BANK3_START_ADDR+2048+512+32, 32, (512-32), dHubChannel_CFG_MTU_128byte, 0, 0, 1},
    {avif_dhub_config_ChMap_avif_AUD_WR0_PIP,AVIF_DHUB_BANK3_START_ADDR+3072,AVIF_DHUB_BANK3_START_ADDR+3072+32, 32, (512-32), dHubChannel_CFG_MTU_128byte, 0, 0, 1},
    {avif_dhub_config_ChMap_avif_AUD_WR1_PIP,AVIF_DHUB_BANK3_START_ADDR+3072+512,AVIF_DHUB_BANK3_START_ADDR+3072+512+32, 32, (512-32), dHubChannel_CFG_MTU_128byte, 0, 0, 1},
    {avif_dhub_config_ChMap_avif_AUD_WR2_PIP,AVIF_DHUB_BANK3_START_ADDR+4096,AVIF_DHUB_BANK3_START_ADDR+4096+32, 32, (512-32), dHubChannel_CFG_MTU_128byte, 0, 0, 1},
    {avif_dhub_config_ChMap_avif_AUD_WR3_PIP,AVIF_DHUB_BANK3_START_ADDR+4096+512,AVIF_DHUB_BANK3_START_ADDR+4096+512+32, 32, (512-32), dHubChannel_CFG_MTU_128byte, 0, 0, 1},
    {avif_dhub_config_ChMap_avif_AUD_RD0,AVIF_DHUB_BANK3_START_ADDR+5120,AVIF_DHUB_BANK3_START_ADDR+5120+32, 32, (3072-32), dHubChannel_CFG_MTU_128byte, 0, 0, 1},
};

