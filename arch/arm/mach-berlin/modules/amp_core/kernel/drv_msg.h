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
#ifndef _DRV_MSG_H_
#define _DRV_MSG_H_
#include <linux/version.h>

#if LINUX_VERSION_CODE >= KERNEL_VERSION (3,10,0)
#include <../drivers/staging/android/ion/ion.h>
#else
#include <linux/ion.h>
#endif

#include "amp_type.h"

typedef struct amp_message_queue {
	UINT q_length;
	UINT rd_number;
	UINT wr_number;
	MV_CC_MSG_t *pMsg;

	HRESULT(*Add) (struct amp_message_queue * pMsgQ, MV_CC_MSG_t * pMsg);
	HRESULT(*ReadTry) (struct amp_message_queue * pMsgQ, MV_CC_MSG_t * pMsg);
	HRESULT(*ReadFin) (struct amp_message_queue * pMsgQ);
	HRESULT(*Destroy) (struct amp_message_queue * pMsgQ);
} AMPMsgQ_t;

INT AMPMsgQ_DequeueRead(AMPMsgQ_t * pMsgQ, MV_CC_MSG_t * pMsg);
INT AMPMsgQ_Fullness(AMPMsgQ_t * pMsgQ);
HRESULT AMPMsgQ_Init(AMPMsgQ_t * pAMPMsgQ, UINT q_length);
HRESULT AMPMsgQ_Destroy(AMPMsgQ_t * pMsgQ);
HRESULT AMPMsgQ_Add(AMPMsgQ_t * pMsgQ, MV_CC_MSG_t * pMsg);
HRESULT AMPMsgQ_ReadTry(AMPMsgQ_t * pMsgQ, MV_CC_MSG_t * pMsg);
HRESULT AMPMsgQ_ReadFinish(AMPMsgQ_t * pMsgQ);

#endif    //_DRV_MSG_H_
