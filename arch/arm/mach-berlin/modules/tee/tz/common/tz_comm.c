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
#include "tz_comm.h"
#include "log.h"
#include "smc.h"

int __smc_trace(int task_id, int cmd_id, int param0, int param1, void *ret)
{
	int status;

	trace("start. (task_to=%d, cmd_id=%d, param0=0x%08x, param1=0x%08x)\n",
		task_id, cmd_id, param0, param1);

	status = __smc_tee(task_id, cmd_id, param0, param1, ret);

	trace("done. (task_to=%d, cmd_id=%d, result=0x%08x, origin=0x%08x, "
		"status=0x%08x)\n",
		task_id, cmd_id, param0, param1, status);

	return status;
}

tz_errcode_t tz_comm_register_cmd_handler(
			struct tz_cmd_handler_pair *handler_list,
			uint32_t max_count, uint32_t cmd_id,
			tz_cmd_handler handler, void *userdata)
{
	int i;
	for (i = 0; i < max_count; i++) {
		if (handler_list[i].cmd_id == 0) {
			handler_list[i].cmd_id = cmd_id;
			handler_list[i].handler = handler;
			handler_list[i].userdata = userdata;
			return TZ_SUCCESS;
		}
	}
	error("reach max (%d) command handlers\n", max_count);
	return TZ_ERROR_OUT_OF_MEMORY;
}

tz_errcode_t tz_comm_unregister_cmd_handler(
			struct tz_cmd_handler_pair *handler_list,
			uint32_t max_count, uint32_t cmd_id)
{
	struct tz_cmd_handler_pair *h = tz_comm_find_cmd_handler(handler_list,
							max_count, cmd_id);
	if (h) {
		h->cmd_id = 0;
		return TZ_SUCCESS;
	}
	error("can't find handler for the command id (0x%08x)\n", cmd_id);
	return TZ_ERROR_ITEM_NOT_FOUND;
}


struct tz_cmd_handler_pair *tz_comm_find_cmd_handler(
			struct tz_cmd_handler_pair *handler_list,
			uint32_t max_count, uint32_t cmd_id)
{
	int i;
	for (i = 0; i < max_count; i++) {
		if (handler_list[i].cmd_id == cmd_id)
			return &handler_list[i];
	}
	return NULL;
}
