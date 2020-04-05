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
#include "tee_ca_dhub.h"
#include "hal_dhub_wrap.h"

void wrap_DhubInitialization(SIGN32 cpuId, UNSG32 dHubBaseAddr, UNSG32 hboSramAddr, HDL_dhub2d *pdhubHandle, DHUB_channel_config *dhub_config,SIGN32 numOfChans)
{
#ifdef VPP_DHUB_IN_TZ
#ifndef __LINUX_KERNEL__
    DhubInitialize();
#endif
    tz_DhubInitialization(cpuId, dHubBaseAddr, hboSramAddr, 0, 0, numOfChans);
#else
    DhubInitialization(cpuId, dHubBaseAddr, hboSramAddr, pdhubHandle, dhub_config, numOfChans);
#endif
}

void wrap_DhubChannelClear(void *hdl, SIGN32 id, T64b cfgQ[])
{
#ifdef VPP_DHUB_IN_TZ
        tz_DhubChannelClear(hdl, id, cfgQ);
#else
        DhubChannelClear(hdl, id, cfgQ);
#endif
}

UNSG32 wrap_dhub_channel_write_cmd(
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
        )

{
#ifdef VPP_DHUB_IN_TZ
    return tz_dhub_channel_write_cmd(hdl, id, addr, size, semOnMTU, chkSemId, updSemId, interrupt, cfgQ, ptr);
#else
    return dhub_channel_write_cmd(hdl, id, addr, size, semOnMTU, chkSemId, updSemId, interrupt, cfgQ, ptr);
#endif
}


void wrap_dhub_channel_generate_cmd(
        void        *hdl,       /*! Handle to HDL_dhub !*/
        SIGN32      id,     /*! Channel ID in $dHubReg !*/
        UNSG32      addr,       /*! CMD: buffer address !*/
        SIGN32      size,       /*! CMD: number of bytes to transfer !*/
        SIGN32      semOnMTU,   /*! CMD: semaphore operation at CMD/MTU (0/1) !*/
        SIGN32      chkSemId,   /*! CMD: non-zero to check semaphore !*/
        SIGN32      updSemId,   /*! CMD: non-zero to update semaphore !*/
        SIGN32      interrupt,  /*! CMD: raise interrupt at CMD finish !*/
        SIGN32      *pData
        )
{
#ifdef VPP_DHUB_IN_TZ
    tz_dhub_channel_generate_cmd(hdl, id, addr, size, semOnMTU, chkSemId, updSemId, interrupt, pData);
#else
    dhub_channel_generate_cmd(hdl, id, addr, size, semOnMTU, chkSemId, updSemId, interrupt, pData);
#endif
}

void* wrap_dhub_semaphore(
        void    *hdl        /*  Handle to HDL_dhub */
        )
{

#ifdef VPP_DHUB_IN_TZ
    return 0;
#else
    return dhub_semaphore(hdl);
#endif
}

void wrap_semaphore_pop(
        void    *hdl,       /*  Handle to HDL_semaphore */
        SIGN32  id,     /*  Semaphore ID in $SemaHub */
        SIGN32  delta       /*  Delta to pop as a consumer */
        )
{

#ifdef VPP_DHUB_IN_TZ
    tz_semaphore_pop(hdl, id, delta);
#else
    semaphore_pop(hdl, id, delta);
#endif
}


void wrap_semaphore_clr_full(
        void    *hdl,       /*  Handle to HDL_semaphore */
        SIGN32  id      /*  Semaphore ID in $SemaHub */
        )
{

#ifdef VPP_DHUB_IN_TZ
    tz_semaphore_clr_full(hdl, id);
#else
    semaphore_clr_full(hdl, id);
#endif
}


UNSG32 wrap_semaphore_chk_full(
        void    *hdl,       /*Handle to HDL_semaphore */
        SIGN32  id      /*Semaphore ID in $SemaHub
                    -1 to return all 32b of the interrupt status
                    */
        )
{
#ifdef VPP_DHUB_IN_TZ
    return tz_semaphore_chk_full(hdl, id);
#else
    return semaphore_chk_full(hdl, id);
#endif
}

void wrap_semaphore_intr_enable(
        void        *hdl,               /*! Handle to HDL_semaphore !*/
        SIGN32      id,                 /*! Semaphore ID in $SemaHub !*/
        SIGN32      empty,              /*! Interrupt enable for CPU at condition 'empty' !*/
        SIGN32      full,               /*! Interrupt enable for CPU at condition 'full' !*/
        SIGN32      almostEmpty,        /*! Interrupt enable for CPU at condition 'almostEmpty' !*/
        SIGN32      almostFull,         /*! Interrupt enable for CPU at condition 'almostFull' !*/
        SIGN32      cpu                 /*! CPU ID (0/1/2) !*/
        )
{
#ifdef VPP_DHUB_IN_TZ
    tz_semaphore_intr_enable(hdl, id, empty, full, almostEmpty, almostFull, cpu);
#else
    semaphore_intr_enable(hdl, id, empty, full, almostEmpty, almostFull, cpu);
#endif

}

