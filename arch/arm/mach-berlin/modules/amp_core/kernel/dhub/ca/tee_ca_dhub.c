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
#ifdef VPP_DHUB_IN_TZ
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/mutex.h>
#include <linux/proc_fs.h>

#include "tee_client_api.h"
#include "tee_ca_dhub.h"
#include "dhub_cmd.h"
#include "galois_io.h"


#define bTST(x, b)      (((x) >> (b)) & 1)

static const TEEC_UUID TADhub_UUID = TA_DHUB_UUID;
static int initialized = 0;
static TEEC_Context context;
static TEEC_Session session;

int DhubInitialize(void)
{
    TEEC_Result result;

    if (initialized)
        return TEEC_SUCCESS;

    initialized = true;

    /* ========================================================================
       [1] Connect to TEE
    ======================================================================== */
    result = TEEC_InitializeContext(
             NULL,
             &context);
    if (result != TEEC_SUCCESS) {
        printk("ret=0x%08x\n", result);
        goto cleanup1;
    }

    /* ========================================================================
       [2] Allocate DHUB SHM
    ======================================================================== */
    /* ========================================================================
       [3] Open session with TEE application
    ======================================================================== */

    result = TEEC_OpenSession(
            &context,
            &session,
            &TADhub_UUID,
            TEEC_LOGIN_USER,
            NULL,    /* No connection data needed for TEEC_LOGIN_USER. */
            NULL,    /* No payload, and do not want cancellation. */
            NULL);
        if (result != TEEC_SUCCESS) {
            printk("ret=0x%08x\n", result);
            goto cleanup2;
        }

    return TEEC_SUCCESS;

    cleanup2:
    TEEC_FinalizeContext(&context);
    cleanup1:
    return result;
}


void DhubFinalize(void)
{
    if (!initialized)
        return;
    initialized = 0;

    TEEC_CloseSession(&session);
    TEEC_FinalizeContext(&context);
}

int DhubRegisterShm(unsigned int *VirtAddr, unsigned int Size)
{
    TEEC_SharedMemory TzShm;

    TzShm.phyAddr = VirtAddr;
    TzShm.size = Size;
    TzShm.flags = TEEC_MEM_INPUT | TEEC_MEM_OUTPUT;

    return TEEC_RegisterSharedMemory(&context, &TzShm);

}

void DhubUnregisterShm(unsigned int *VirtAddr, unsigned int Size)
{
    TEEC_SharedMemory TzShm;

    TzShm.buffer = VirtAddr;
    TzShm.size = Size;
    TzShm.flags = TEEC_MEM_INPUT | TEEC_MEM_OUTPUT;

    return TEEC_ReleaseSharedMemory(&TzShm);
}

void tz_DhubInitialization(SIGN32 cpuId, UNSG32 dHubBaseAddr, UNSG32 hboSramAddr, int *pdhubHandle, int *dhub_config,SIGN32 numOfChans)
{
    TEEC_Result result;
    TEEC_Operation operation;

    operation.paramTypes = TEEC_PARAM_TYPES(
            TEEC_VALUE_INPUT,
            TEEC_VALUE_INPUT,
            TEEC_VALUE_OUTPUT,
            TEEC_NONE);

    operation.params[0].value.a = cpuId;
    operation.params[0].value.b = dHubBaseAddr;
    operation.params[1].value.a = hboSramAddr;
    operation.params[1].value.b = numOfChans;

    /* clear result */
    operation.params[2].value.a = 0xdeadbeef;

    operation.started = 1;
    result = TEEC_InvokeCommand(
            &session,
            DHUB_INIT,
            &operation,
            NULL);
}


void tz_DhubChannelClear(void *hdl, SIGN32 id, T64b cfgQ[])
{
    TEEC_Result result;
    TEEC_Operation operation;

    operation.paramTypes = TEEC_PARAM_TYPES(
            TEEC_VALUE_INPUT,
            TEEC_VALUE_OUTPUT,
            TEEC_NONE,
            TEEC_NONE);

    operation.params[0].value.a = id;

    /* clear result */
    operation.params[1].value.a = 0xdeadbeef;

    operation.started = 1;
    result = TEEC_InvokeCommand(
            &session,
            DHUB_CHANNEL_CLEAR,
            &operation,
            NULL);
}


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
        )

{
    TEEC_Result result;
    TEEC_Operation operation;

	if( addr_check_normal_world_noncache(addr,size) != 0 ) {
		return 0;
	}

    operation.paramTypes = TEEC_PARAM_TYPES(
            TEEC_VALUE_INPUT,
            TEEC_VALUE_INPUT,
            TEEC_VALUE_INPUT,
            TEEC_VALUE_INOUT);

    operation.params[0].value.a = id;
    operation.params[0].value.b = addr;
    operation.params[1].value.a = size;
    operation.params[1].value.b = semOnMTU;

    operation.params[2].value.a = chkSemId;
    operation.params[2].value.b = updSemId;
    operation.params[3].value.a = interrupt;

    /* clear result */
    operation.params[3].value.b = 0xdeadbeef;

    operation.started = 1;
    result = TEEC_InvokeCommand(
            &session,
            DHUB_CHANNEL_WRITECMD,
            &operation,
            NULL);

    return operation.params[3].value.b;
}


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
        )
{
    TEEC_Result result;
    TEEC_Operation operation;

	if( addr_check_normal_world(addr,size) != 0 ) {
		return;
	}

    if(!pData)
        return;

    operation.paramTypes = TEEC_PARAM_TYPES(
            TEEC_VALUE_INOUT,
            TEEC_VALUE_INPUT,
            TEEC_VALUE_INPUT,
            TEEC_VALUE_INPUT);

    operation.params[0].value.a = id;
    operation.params[0].value.b = addr;
    operation.params[1].value.a = size;
    operation.params[1].value.b = semOnMTU;

    operation.params[2].value.a = chkSemId;
    operation.params[2].value.b = updSemId;
    operation.params[3].value.a = interrupt;

    operation.started = 1;
    result = TEEC_InvokeCommand(
            &session,
            DHUB_CHANNEL_GENERATECMD,
            &operation,
            NULL);
    pData[0] = operation.params[0].value.a;
    pData[1] = operation.params[0].value.b;
}

void* tz_dhub_semaphore(
        void    *hdl        /*  Handle to HDL_dhub */
        )
{
    return 0; //no use for tz code
}

void tz_semaphore_pop(
        void    *hdl,       /*  Handle to HDL_semaphore */
        SIGN32  id,     /*  Semaphore ID in $SemaHub */
        SIGN32  delta       /*  Delta to pop as a consumer */
        )
{
    TEEC_Result result;
    TEEC_Operation operation;

    operation.paramTypes = TEEC_PARAM_TYPES(
            TEEC_VALUE_INPUT,
            TEEC_VALUE_OUTPUT,
            TEEC_NONE,
            TEEC_NONE);

    operation.params[0].value.a = id;
    operation.params[0].value.b = delta;

    /* clear result */
    operation.params[1].value.a = 0xdeadbeef;

    operation.started = 1;
    result = TEEC_InvokeCommand(
            &session,
            DHUB_SEM_POP,
            &operation,
            NULL);

    //return operation.params[1].value.a;
}


void tz_semaphore_clr_full(
        void    *hdl,       /*  Handle to HDL_semaphore */
        SIGN32  id      /*  Semaphore ID in $SemaHub */
        )
{
    TEEC_Result result;
    TEEC_Operation operation;

    operation.paramTypes = TEEC_PARAM_TYPES(
            TEEC_VALUE_INPUT,
            TEEC_VALUE_OUTPUT,
            TEEC_NONE,
            TEEC_NONE);

    operation.params[0].value.a = id;

    /* clear result */
    operation.params[1].value.a = 0xdeadbeef;

    operation.started = 1;
    result = TEEC_InvokeCommand(
            &session,
            DHUB_SEM_CLR_FULL,
            &operation,
            NULL);

  //  return operation.params[1].value.a;
}


UNSG32 tz_semaphore_chk_full(
        void    *hdl,       /*Handle to HDL_semaphore */
        SIGN32  id      /*Semaphore ID in $SemaHub
                    -1 to return all 32b of the interrupt status
                    */
        )
{
    TEEC_Result result;
    TEEC_Operation operation;
    UNSG32 d;

    operation.paramTypes = TEEC_PARAM_TYPES(
            TEEC_VALUE_INPUT,
            TEEC_VALUE_OUTPUT,
            TEEC_NONE,
            TEEC_NONE);

    operation.params[0].value.a = id;

    /* clear result */
    operation.params[1].value.a = 0xdeadbeef;
    operation.params[1].value.b = 0xdeadbeef;

    operation.started = 1;
    result = TEEC_InvokeCommand(
            &session,
            DHUB_SEM_CHK_FULL,
            &operation,
            NULL);

    /*(d = value of(sem->ra + RA_SemaHub_full);*/
    d = operation.params[1].value.a;
    return (id < 0) ? d : bTST(d, id);

}

void    tz_semaphore_intr_enable(
                    void        *hdl,               /*! Handle to HDL_semaphore !*/
                    SIGN32      id,                 /*! Semaphore ID in $SemaHub !*/
                    SIGN32      empty,              /*! Interrupt enable for CPU at condition 'empty' !*/
                    SIGN32      full,               /*! Interrupt enable for CPU at condition 'full' !*/
                    SIGN32      almostEmpty,        /*! Interrupt enable for CPU at condition 'almostEmpty' !*/
                    SIGN32      almostFull,         /*! Interrupt enable for CPU at condition 'almostFull' !*/
                    SIGN32      cpu                 /*! CPU ID (0/1/2) !*/
                    )
{
    TEEC_Result result;
    TEEC_Operation operation;

    operation.paramTypes = TEEC_PARAM_TYPES(
            TEEC_VALUE_INPUT,
            TEEC_VALUE_INPUT,
            TEEC_VALUE_INPUT,
            TEEC_VALUE_OUTPUT);

    operation.params[0].value.a = id;
    operation.params[0].value.b = empty;
    operation.params[1].value.a = full;
    operation.params[1].value.b = almostEmpty;

    operation.params[2].value.a = almostFull;
    operation.params[2].value.b = cpu;

    /* clear result */
    operation.params[3].value.a = 0xdeadbeef;

    operation.started = 1;
    result = TEEC_InvokeCommand(
            &session,
            DHUB_SEM_INTR_ENABLE,
            &operation,
            NULL);

}
#endif

