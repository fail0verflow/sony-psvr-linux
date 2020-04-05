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
#ifndef _TEE_CLIENT_UTIL_H_
#define _TEE_CLIENT_UTIL_H_

#include "tee_client_api.h"
#include "tee_comm.h"

uint32_t TEEC_CallParamToComm(int tzc,
			union tee_param *commParam,
			TEEC_Parameter *param, uint32_t paramType);

uint32_t TEEC_CallCommToParam(TEEC_Parameter *param,
			union tee_param *commParam,
			uint32_t paramType);

uint32_t TEEC_CallbackCommToParam(TEEC_Parameter *param,
			union tee_param *commParam,
			uint32_t paramType);

uint32_t TEEC_CallbackParamToComm(union tee_param *commParam,
			TEEC_Parameter *param, uint32_t paramType);

/*
 * it will fill cmdData except cmdData->param_ext
 */
TEEC_Result TEEC_CallOperactionToCommand(int tzc,
				struct tee_comm_param *cmd,
				uint32_t cmdId, TEEC_Operation *operation);

/*
 * it will copy data from cmdData->params[4] to operation->params[4]
 */
TEEC_Result TEEC_CallCommandToOperaction(TEEC_Operation *operation,
				struct tee_comm_param *cmd);

/*
 * it will copy data from cmd->params[4] to operation->params[4]
 */
TEEC_Result TEEC_CallbackCommandToOperaction(TEEC_Operation *operation,
				struct tee_comm_param *cmd);

/*
 * it will fill cmd->param_types and cmd->params[4]
 */
TEEC_Result TEEC_CallbackOperactionToCommand(struct tee_comm_param *cmd,
				TEEC_Operation *operation);

#endif /* _TEE_CLIENT_UTIL_H_ */
