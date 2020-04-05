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
#ifndef _TEE_INTERNAL_CORE_CLIENT_API_H_
#define _TEE_INTERNAL_CORE_CLIENT_API_H_

#include "tee_internal_core_common.h"

TEE_Result TEE_OpenTASession(
	const TEE_UUID*		destination,
	uint32_t		cancellationRequestTimeout,
	uint32_t		paramTypes,
	TEE_Param		params[4],
	TEE_TASessionHandle*	session,
	uint32_t*		returnOrigin);

void TEE_CloseTASession(
	TEE_TASessionHandle	session);

TEE_Result TEE_InvokeTACommand(
	TEE_TASessionHandle	session,
	uint32_t		cancellationRequestTimeout,
	uint32_t		commandID,
	uint32_t		paramTypes,
	TEE_Param		params[4],
	uint32_t*		returnOrigin);

#endif /* _TEE_INTERNAL_CORE_CLIENT_API_H_ */
