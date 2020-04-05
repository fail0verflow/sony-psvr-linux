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
#ifndef __AVIO_DHUB_CFG_H__
#define __AVIO_DHUB_CFG_H__
#include "dHub.h"
#include "avioDhub.h"
#include "api_dhub.h"

#define VPP_DHUB_BASE			(MEMMAP_VPP_DHUB_REG_BASE + RA_vppDhub_dHub0)
#define VPP_HBO_SRAM_BASE		(MEMMAP_VPP_DHUB_REG_BASE + RA_vppDhub_tcm0)
#define VPP_NUM_OF_CHANNELS		(avioDhubChMap_vpp_SPDIF_W+1)

#define AG_DHUB_BASE			(MEMMAP_AG_DHUB_REG_BASE + RA_agDhub_dHub0)
#define AG_HBO_SRAM_BASE		(MEMMAP_AG_DHUB_REG_BASE + RA_agDhub_tcm0)
#define AG_NUM_OF_CHANNELS		(avioDhubChMap_ag_PG_ENG_W+1)

#define AVIO_DHUB3_DHUB_BASE         (MEMMAP_AVIO_DH3_DHUB_REG_BASE + RA_dh3Dhub_dHub0)
#define AVIO_DHUB3_SRAM_BASE         (MEMMAP_AVIO_DH3_DHUB_REG_BASE + RA_dh3Dhub_tcm0)
#define AVIO_DHUB3_NUM_OF_CHANNELS    (avioDhubChMap_Dh3_MOSD_R1 + 1)


#define AVIO_DHUB3_BANK0_START_ADDR     (0)
#define AVIO_DHUB3_BANK1_START_ADDR     (8192 * 2)

#define VPP_DHUB_BANK0_START_ADDR       (8192*0)
#define VPP_DHUB_BANK1_START_ADDR       (8192*1)
#define VPP_DHUB_BANK2_START_ADDR       (8192*2)
#define VPP_DHUB_BANK3_START_ADDR       (8192*3)
#define VPP_DHUB_BANK4_START_ADDR       (8192*4)

#define AG_DHUB_BANK0_START_ADDR        (8192*0)
#define AG_DHUB_BANK1_START_ADDR        (8192*1)
#define AG_DHUB_BANK2_START_ADDR        (8192*2)

#define DHUB_BANK0_START_ADDR		(8192*0)
#define DHUB_BANK1_START_ADDR		(8192*1)
#define DHUB_BANK2_START_ADDR		(8192*2)
#define DHUB_BANK3_START_ADDR		(8192*3)
#define DHUB_BANK4_START_ADDR		(8192*4)
#define DHUB_BANK5_START_ADDR		(8192*5)
#endif //__AVIO_DHUB_CFG_H__

