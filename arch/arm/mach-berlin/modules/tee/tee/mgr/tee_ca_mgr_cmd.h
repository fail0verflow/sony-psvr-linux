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
#ifndef _TEE_CA_MGR_CMD_H_
#define _TEE_CA_MGR_CMD_H_

#include "tee_client_api.h"

TEEC_Result TAMgr_Register(int tzc, struct tee_comm *comm,
	TEEC_Parameter *taCode, uint32_t paramType);

TEE_Result TAMgr_CreateInstance(
	int			tzc,
	struct tee_comm*	comm,
	const TEE_UUID*		destination,
	uint32_t*		returnOrigin,
	uint32_t*		taskId);

TEE_Result TAMgr_DestroyInstance(
	int			tzc,
	struct tee_comm*	comm,
	uint32_t		taskId,
	uint32_t*		returnOrigin);

#endif /* _TEE_CA_MGR_CMD_H_ */
