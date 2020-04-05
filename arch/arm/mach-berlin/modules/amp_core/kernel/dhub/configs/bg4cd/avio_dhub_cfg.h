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
#include "avio_memmap.h"

#define VPP_DHUB_BASE           (MEMMAP_AVIO_REG_BASE + AVIO_MEMMAP_VPP128B_DHUB_REG_BASE + RA_vpp128bDhub_dHub0)
#define VPP_HBO_SRAM_BASE       (MEMMAP_AVIO_REG_BASE + AVIO_MEMMAP_VPP128B_DHUB_REG_BASE + RA_vpp128bDhub_tcm0)
#define VPP_NUM_OF_CHANNELS     (avioDhubChMap_vpp128b_DINT1_R+1)
#define AG_DHUB_BASE            (MEMMAP_AVIO_REG_BASE + AVIO_MEMMAP_AIO64B_DHUB_REG_BASE + RA_aio64bDhub_dHub0)
#define AG_HBO_SRAM_BASE        (MEMMAP_AVIO_REG_BASE + AVIO_MEMMAP_AIO64B_DHUB_REG_BASE + RA_aio64bDhub_tcm0)
#define AG_NUM_OF_CHANNELS      (avioDhubChMap_aio64b_TT_R+1)

#define VPP_DHUB_BANK0_START_ADDR       avioDhubTcmMap_vpp128bDhub_BANK0_START_ADDR
#define VPP_DHUB_BANK1_START_ADDR       avioDhubTcmMap_vpp128bDhub_BANK1_START_ADDR
#define VPP_DHUB_BANK2_START_ADDR       avioDhubTcmMap_vpp128bDhub_BANK2_START_ADDR
#define VPP_DHUB_BANK3_START_ADDR       avioDhubTcmMap_vpp128bDhub_BANK3_START_ADDR
#define AG_DHUB_BANK0_START_ADDR        avioDhubTcmMap_aio64bDhub_BANK0_START_ADDR

#endif //__AVIO_DHUB_CFG_H__

