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
#ifndef _TEE_SYS_CMD_H_
#define _TEE_SYS_CMD_H_

#include "tee_internal_core_common.h"

/** TA System Command Protocol.
 *
 * All TA must support these System Commands.
 *
 * currently, we only support 2 system command in TA:
 * - OpenSession
 * - CloseSession
 */

enum TASysCmd {
	TASYS_CMD_OPEN_SESSION,
	TASYS_CMD_CLOSE_SESSION,	/* return instanceDead by param[0].value.a */
	TASYS_CMD_MAX
};

typedef struct {
	TEE_UUID client;	/* input: client TA uuid */
	uint32_t login;		/* input: login method */
	uint32_t group;		/* input: group to login for TEE_LOGIN_GROUP &
				 * TEE_LOGIN_APPLICATION_GROUP */
	uint32_t sessionId;	/* output: taskId (8bits) | index (24bits) */
} TASysCmdOpenSessionParamExt;

#endif /* _TEE_SYS_CMD_H_ */
