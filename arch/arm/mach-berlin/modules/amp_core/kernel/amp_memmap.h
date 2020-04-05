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
#ifndef _PE_MEMMAP_H_
#define _PE_MEMMAP_H_

#define        MEMMAP_VIP_DHUB_REG_BASE                    0xF7AE0000
#define        MEMMAP_VIP_DHUB_REG_SIZE                    0x20000
#define        MEMMAP_VIP_DHUB_REG_DEC_BIT                 0x11

#define        MEMMAP_AVIF_REG_BASE                        0xF7980000
#define        MEMMAP_AVIF_REG_SIZE                        0x80000
#define        MEMMAP_AVIF_REG_DEC_BIT                     0x13

#define        MEMMAP_ZSP_REG_BASE                         0xF7C00000
#define        MEMMAP_ZSP_REG_SIZE                         0x80000
#define        MEMMAP_ZSP_REG_DEC_BIT                      0x13

#define        MEMMAP_AG_DHUB_REG_BASE                     0xF7D00000
#define        MEMMAP_AG_DHUB_REG_SIZE                     0x20000
#define        MEMMAP_AG_DHUB_REG_DEC_BIT                  0x11

#define        MEMMAP_VPP_DHUB_REG_BASE                    0xF7F40000
#define        MEMMAP_VPP_DHUB_REG_SIZE                    0x20000
#define        MEMMAP_VPP_DHUB_REG_DEC_BIT                 0x11

#endif				//_PE_MEMMAP_H_
