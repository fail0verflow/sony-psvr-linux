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
#include "config.h"
#include "tee_client_util.h"
#include "tz_client_api.h"
#include "log.h"
#ifndef __KERNEL__
#include "string.h"
#endif /* __KERNEL__ */

uint32_t TEEC_CallParamToComm(int tzc,
			union tee_param *commParam,
			TEEC_Parameter *param, uint32_t paramType)
{
	uint32_t t = paramType & 0x7;	/* covented paramType for communication */
	uint32_t attr;

	switch (paramType) {
	case TEEC_VALUE_INPUT:
	case TEEC_VALUE_OUTPUT:
	case TEEC_VALUE_INOUT:
		commParam->value.a = param->value.a;
		commParam->value.b = param->value.b;
		break;
	case TEEC_MEMREF_TEMP_INPUT:
	case TEEC_MEMREF_TEMP_OUTPUT:
	case TEEC_MEMREF_TEMP_INOUT:
		commParam->memref.buffer = (uint32_t)tzc_get_mem_info(tzc, param->tmpref.buffer, &attr);
		commParam->memref.size   = param->tmpref.size;
		break;
	case TEEC_MEMREF_WHOLE:
		t = TEE_PARAM_TYPE_MEMREF_INOUT;
		commParam->memref.buffer = (uint32_t)param->memref.parent->phyAddr;
		commParam->memref.size   = param->memref.parent->size;
		break;
	case TEEC_MEMREF_PARTIAL_INPUT:
	case TEEC_MEMREF_PARTIAL_OUTPUT:
	case TEEC_MEMREF_PARTIAL_INOUT:
		commParam->memref.buffer = (uint32_t)param->memref.parent->phyAddr +
					param->memref.offset;
		commParam->memref.size   = param->memref.size;
		break;
	}

	return t;
}

uint32_t TEEC_CallCommToParam(TEEC_Parameter *param,
			union tee_param *commParam,
			uint32_t paramType)
{
	uint32_t t = paramType & 0x7;	/* covented paramType for communication */
	switch (paramType) {
	case TEEC_VALUE_INPUT:
	case TEEC_VALUE_OUTPUT:
	case TEEC_VALUE_INOUT:
		param->value.a = commParam->value.a;
		param->value.b = commParam->value.b;
		break;
	case TEEC_MEMREF_TEMP_OUTPUT:
	case TEEC_MEMREF_TEMP_INOUT:
		param->tmpref.size = commParam->memref.size;
		break;
	case TEEC_MEMREF_WHOLE:
	case TEEC_MEMREF_PARTIAL_OUTPUT:
	case TEEC_MEMREF_PARTIAL_INOUT:
		param->memref.size = commParam->memref.size;
		break;
	}

	return t;
}

uint32_t TEEC_CallbackCommToParam(TEEC_Parameter *param,
			union tee_param *commParam,
			uint32_t paramType)
{
	uint32_t t = paramType & 0x7;	/* covented paramType for communication */

	switch (paramType) {
	case TEE_PARAM_TYPE_VALUE_INPUT:
	case TEE_PARAM_TYPE_VALUE_OUTPUT:
	case TEE_PARAM_TYPE_VALUE_INOUT:
		param->value.a = commParam->value.a;
		param->value.b = commParam->value.b;
		break;
	case TEE_PARAM_TYPE_MEMREF_INPUT:
	case TEE_PARAM_TYPE_MEMREF_OUTPUT:
	case TEE_PARAM_TYPE_MEMREF_INOUT:
		param->tmpref.buffer	= (void*)commParam->memref.buffer;
		param->tmpref.size	= commParam->memref.size;
		break;
	}

	return t;
}

uint32_t TEEC_CallbackParamToComm(union tee_param *commParam,
			TEEC_Parameter *param, uint32_t paramType)
{
	uint32_t t = paramType & 0x7;	/* covented paramType for communication */

	switch (paramType) {
	case TEE_PARAM_TYPE_VALUE_INPUT:
	case TEE_PARAM_TYPE_VALUE_OUTPUT:
	case TEE_PARAM_TYPE_VALUE_INOUT:
		commParam->value.a = param->value.a;
		commParam->value.b = param->value.b;
		break;
	case TEE_PARAM_TYPE_MEMREF_INPUT:
	case TEE_PARAM_TYPE_MEMREF_OUTPUT:
	case TEE_PARAM_TYPE_MEMREF_INOUT:
		commParam->memref.buffer	= (uint32_t)param->tmpref.buffer;
		commParam->memref.size		= param->tmpref.size;
		break;
	}

	return t;
}

/*
 * it will fill cmd->param_types and cmd->params[4]
 */
TEEC_Result TEEC_CallOperactionToCommand(int tzc,
				struct tee_comm_param *cmd,
				uint32_t cmdId, TEEC_Operation *operation)
{
	int i, t[4];

	if (!cmd) {
		error("illigle arguments\n");
		return TEEC_ERROR_BAD_PARAMETERS;
	}

	memset(cmd, 0, TEE_COMM_PARAM_BASIC_SIZE);

	cmd->cmd_id = cmdId;

	if (!operation) {
		cmd->param_types = TEE_PARAM_TYPES(TEE_PARAM_TYPE_NONE,
				TEE_PARAM_TYPE_NONE,
				TEE_PARAM_TYPE_NONE,
				TEE_PARAM_TYPE_NONE);
		goto out;
	}

	/* travel all the 4 parameters */
	for (i = 0; i < 4; i++) {
		int paramType = TEEC_PARAM_TYPE_GET(operation->paramTypes, i);
		t[i] = TEEC_CallParamToComm(tzc,
					&cmd->params[i],
					&operation->params[i], paramType);
	}

	cmd->param_types = TEE_PARAM_TYPES(t[0], t[1], t[2], t[3]);

out:
	trace("invoke command 0x%08x\n", cmd->cmd_id);
	return TEEC_SUCCESS;
}

/*
 * it will copy data from cmd->params[4] to operation->params[4]
 */
TEEC_Result TEEC_CallCommandToOperaction(TEEC_Operation *operation,
				struct tee_comm_param *cmd)
{
	int i;

	if (!operation)
		return TEEC_SUCCESS;

	if (!cmd) {
		error("illigle arguments\n");
		return TEEC_ERROR_BAD_PARAMETERS;
	}

	/* travel all the 4 param */
	for (i = 0; i < 4; i++) {
		int paramType = TEEC_PARAM_TYPE_GET(operation->paramTypes, i);
		TEEC_CallCommToParam(&operation->params[i],
					&cmd->params[i],
					paramType);
	}

	return TEEC_SUCCESS;
}

/*
 * it will copy data from cmd->params[4] to operation->params[4]
 */
TEEC_Result TEEC_CallbackCommandToOperaction(TEEC_Operation *operation,
				struct tee_comm_param *cmd)
{
	int i, t[4];

	if (!operation || !cmd) {
		error("illigle arguments\n");
		return TEEC_ERROR_BAD_PARAMETERS;
	}

	memset(operation, 0, sizeof(*operation));

	/* travel all the 4 param */
	for (i = 0; i < 4; i++) {
		int paramType = TEE_PARAM_TYPE_GET(cmd->param_types, i);
		t[i] = TEEC_CallbackCommToParam(&operation->params[i],
					&cmd->params[i],
					paramType);
	}

	operation->paramTypes = TEEC_PARAM_TYPES(t[0], t[1], t[2], t[3]);

	return TEEC_SUCCESS;
}


/*
 * it will fill cmd->param_types and cmd->params[4]
 */
TEEC_Result TEEC_CallbackOperactionToCommand(struct tee_comm_param *cmd,
				TEEC_Operation *operation)
{
	int i;

	if (!cmd) {
		error("illigle arguments\n");
		return TEEC_ERROR_BAD_PARAMETERS;
	}

	/* travel all the 4 parameters */
	for (i = 0; i < 4; i++) {
		int paramType = TEE_PARAM_TYPE_GET(cmd->param_types, i);
		TEEC_CallbackParamToComm(&cmd->params[i],
					&operation->params[i], paramType);
	}

	trace("invoke command 0x%08x\n", cmd->cmd_id);
	return TEEC_SUCCESS;
}
