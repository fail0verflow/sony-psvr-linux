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

#ifndef TEE_CA_DHUB_H
#define TEE_CA_DHUB_H
/** DHUB_API in trust zone
 */

#include    "ctypes.h"
#include    "dhub_cmd.h"
/*structure define*/

#ifdef  __cplusplus
    extern  "C"
    {
#endif

int DhubInitialize(void);
void DhubFinalize(void);
int DhubRegisterShm(unsigned int *VirtAddr, unsigned int Size);
int DhubRegisterShm(unsigned int *VirtAddr, unsigned int Size);

void tz_DhubInitialization(SIGN32 cpuId, UNSG32 dHubBaseAddr, UNSG32 hboSramAddr, int *pdhubHandle, int *dhub_config,SIGN32 numOfChans);
void tz_DhubChannelClear(void *hdl, SIGN32 id, T64b cfgQ[]);

UNSG32 tz_dhub_channel_write_cmd(
        void        *hdl,       /*! Handle to HDL_dhub !*/
        SIGN32      id,     /*! Channel ID in $dHubReg !*/
        UNSG32      addr,       /*! CMD: buffer address !*/
        SIGN32      size,       /*! CMD: number of bytes to transfer !*/
        SIGN32      semOnMTU,   /*! CMD: semaphore operation at CMD/MTU (0/1) !*/
        SIGN32      chkSemId,   /*! CMD: non-zero to check semaphore !*/
        SIGN32      updSemId,   /*! CMD: non-zero to update semaphore !*/
        SIGN32      interrupt,  /*! CMD: raise interrupt at CMD finish !*/
        T64b        cfgQ[],     /*! Pass NULL to directly update dHub, or
                            Pass non-zero to receive programming sequence
                            in (adr,data) pairs
                            !*/
        UNSG32      *ptr        /*! Pass in current cmdQ pointer (in 64b word),
                            & receive updated new pointer,
                            Pass NULL to read from HW
                            !*/
        );

void tz_dhub_channel_generate_cmd(
        void        *hdl,       /*! Handle to HDL_dhub !*/
        SIGN32      id,     /*! Channel ID in $dHubReg !*/
        UNSG32      addr,       /*! CMD: buffer address !*/
        SIGN32      size,       /*! CMD: number of bytes to transfer !*/
        SIGN32      semOnMTU,   /*! CMD: semaphore operation at CMD/MTU (0/1) !*/
        SIGN32      chkSemId,   /*! CMD: non-zero to check semaphore !*/
        SIGN32      updSemId,   /*! CMD: non-zero to update semaphore !*/
        SIGN32      interrupt,  /*! CMD: raise interrupt at CMD finish !*/
        SIGN32      *pData
        );
void tz_semaphore_pop(
        void    *hdl,       /*  Handle to HDL_semaphore */
        SIGN32  id,     /*  Semaphore ID in $SemaHub */
        SIGN32  delta       /*  Delta to pop as a consumer */
        );

void tz_semaphore_clr_full(
        void    *hdl,       /*  Handle to HDL_semaphore */
        SIGN32  id      /*  Semaphore ID in $SemaHub */
        );

UNSG32 tz_semaphore_chk_full(
        void    *hdl,       /*Handle to HDL_semaphore */
        SIGN32  id      /*Semaphore ID in $SemaHub
                    -1 to return all 32b of the interrupt status
                    */
        );
void    tz_semaphore_intr_enable(
        void        *hdl,               /*! Handle to HDL_semaphore !*/
        SIGN32      id,                 /*! Semaphore ID in $SemaHub !*/
        SIGN32      empty,              /*! Interrupt enable for CPU at condition 'empty' !*/
        SIGN32      full,               /*! Interrupt enable for CPU at condition 'full' !*/
        SIGN32      almostEmpty,        /*! Interrupt enable for CPU at condition 'almostEmpty' !*/
        SIGN32      almostFull,         /*! Interrupt enable for CPU at condition 'almostFull' !*/
        SIGN32      cpu                 /*! CPU ID (0/1/2) !*/
        );


#ifdef  __cplusplus
    }
#endif



/** DHUB_API in trust zone
 */
#endif

/** ENDOFFILE: tee_ca_dhub.h
 */

