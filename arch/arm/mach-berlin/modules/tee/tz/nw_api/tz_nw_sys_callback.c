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
#include "tz_nw_sys_callback.h"
#include "align.h"
#include "log.h"

/* currently, we only support TZ_CMD_TEE_SYS */
#ifndef CONFIG_TZNW_SYS_MAX_CALLBACK
#define CONFIG_TZNW_SYS_MAX_CALLBACK		(4)
#endif
static struct tz_cmd_handler_pair handler_list[CONFIG_TZNW_SYS_MAX_CALLBACK];

tz_errcode_t tz_nw_register_sys_callback(uint32_t cmd_id,
			tz_cmd_handler handler, void *userdata)
{
	return tz_comm_register_cmd_handler(handler_list,
			CONFIG_TZNW_SYS_MAX_CALLBACK,
			cmd_id, handler, userdata);
}

tz_errcode_t tz_nw_unregister_sys_callback(uint32_t cmd_id)
{
	return tz_comm_unregister_cmd_handler(handler_list,
			CONFIG_TZNW_SYS_MAX_CALLBACK, cmd_id);
}

uint32_t tz_nw_sys_callback(void *userdata, uint32_t cmd_id,
				uint32_t param, uint32_t *p_origin)
{
	uint32_t origin = TZ_ORIGIN_UNTRUSTED_APP;
	uint32_t result = TZ_ERROR_NOT_SUPPORTED;

	struct tz_cmd_handler_pair *h = tz_comm_find_cmd_handler(handler_list,
					CONFIG_TZNW_SYS_MAX_CALLBACK, cmd_id);
	if (h)
		result = h->handler(userdata, cmd_id, param, &origin);
	else
		error("cmd handler for id (0x%08x) is not registered\n",
				cmd_id);

	if (p_origin)
		*p_origin = origin;

	return result;
}
