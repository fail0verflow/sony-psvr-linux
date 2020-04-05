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
#include "tz_client_api.h"
#include "tz_nw_task_client.h"
#include "tz_nw_sys_callback.h"
#include "log.h"

#ifdef __KERNEL__
#include <linux/string.h>
#else
#include "string.h"
#endif

#include <linux/mutex.h>

#define ROUND_UP(x,y) (((x) + (y) - 1) & ~((y) - 1u))

static void *s_tz_nw_task_mutex = NULL;

static uint32_t s_task_client_used_map[ROUND_UP(CONFIG_TASK_NUM, sizeof(uint32_t))/sizeof(uint32_t)] = {0x0};

static struct tz_nw_task_client tz_nw_task_client[CONFIG_TASK_NUM];

#define CONFIG_MGR_TASK_NUM 32
static uint32_t s_mgr_task_client_used_map = 0x0;
static struct tz_nw_task_client tz_nw_mgr_task_client[CONFIG_MGR_TASK_NUM];


static struct tz_nw_task_client *tz_nw_mgr_task_client_get(void)
{
	struct tz_nw_task_client *ret = NULL;
	unsigned int i;
	uint32_t mask;

	for (i = 0; i < CONFIG_MGR_TASK_NUM; i++) {
		mask = 0x1 << i;
		if (!(mask & s_mgr_task_client_used_map)) {
			s_mgr_task_client_used_map |= mask;
			ret = &tz_nw_mgr_task_client[i];
			//error("[%s] allocate tz_nw_mgr_task_client[%d] = 0x%08x\n", 
			//	  __FUNCTION__, i, ret);
			break;
		}
	}
	if (!ret) {
		//error("[%s] not allocate tz_nw_mgr_task_client\n", __FUNCTION__);
	}

	return ret;
}

static void tz_nw_mgr_task_client_release(struct tz_nw_task_client *tc)
{
	unsigned int i;
	uint32_t mask;

	if (!tc) {
		return;
	}

	for (i = 0; i < CONFIG_MGR_TASK_NUM; i++) {
		mask = 0x1 << i;
		if ((tc == &tz_nw_mgr_task_client[i]) && (s_mgr_task_client_used_map & mask)) {
			s_mgr_task_client_used_map &= ~mask;
			break;
		}
	}
}

void tz_nw_task_client_release(int task_id, struct tz_nw_task_client *tc)
{
	uint32_t mask;
	uint32_t off, loc;

	if (!tc || !(0 <= task_id && task_id < CONFIG_TASK_NUM) || !s_tz_nw_task_mutex) {
		return;
	}

	if (tzc_mutex_lock(s_tz_nw_task_mutex) != TZ_SUCCESS) {
		return;
	}
	if (task_id == TZ_TASK_ID_MGR) {
		tz_nw_mgr_task_client_release(tc);
	} else {
		off = task_id / 32;
		loc = task_id % 32;
		mask = 1 << loc;
		if ((s_task_client_used_map[off] & mask)) {
			s_task_client_used_map[off] &= ~mask;
		}
	}
	(void)tzc_mutex_unlock(s_tz_nw_task_mutex);
}

struct tz_nw_task_client *tz_nw_task_client_get(int task_id)
{
	struct tz_nw_task_client *ret = NULL;
	uint32_t mask;
	uint32_t off, loc;

	assert(0 <= task_id && task_id < CONFIG_TASK_NUM);

	if (!(0 <= task_id && task_id < CONFIG_TASK_NUM)) {
		return NULL;
	}

	if (!s_tz_nw_task_mutex) {
		return NULL;
	}

	if (tzc_mutex_lock(s_tz_nw_task_mutex) != TZ_SUCCESS) {
		return NULL;
	}
	if (task_id == TZ_TASK_ID_MGR) {
		ret = tz_nw_mgr_task_client_get();
	} else {
		off = task_id / 32;
		loc = task_id % 32;
		mask = 1 << loc;
		if (!(s_task_client_used_map[off] & mask)) {
			ret = &tz_nw_task_client[task_id];
			//error("[%s] allocate tz_nw_task_client[%d] = 0x%08x\n", 
			//				  __FUNCTION__, task_id, ret);

		} else {
			ret = NULL;
		}
	}
	(void)tzc_mutex_unlock(s_tz_nw_task_mutex);

	return ret;
}

static struct tz_nw_task_client *tz_nw_task_client_init(int task_id)
{
	struct tz_nw_task_client *tc = &tz_nw_task_client[task_id];

	if (!tc) {
		return NULL;
	}

	memset(tc, 0, sizeof(*tc));

	tc->task_id = task_id;
	/* by default, we set default callback to tz_nw_sys_callback */
	tc->callback = tz_nw_sys_callback;
	tc->userdata = tc;

	spin_lock_init(&tc->lock);

#ifdef __KERNEL__
	init_waitqueue_head(&tc->waitq);
#endif

	return tc;
}

static void tz_nw_mgr_task_client_init(int id)
{
	struct tz_nw_task_client *tc = &tz_nw_mgr_task_client[id];

	if (!tc) {
		return;
	}

	memset(tc, 0, sizeof(*tc));

	tc->task_id = TZ_TASK_ID_MGR;
	/* by default, we set default callback to tz_nw_sys_callback */
	tc->callback = tz_nw_sys_callback;
	tc->userdata = tc;

	spin_lock_init(&tc->lock);

#ifdef __KERNEL__
	init_waitqueue_head(&tc->waitq);
#endif

	return;
}

int tz_nw_task_client_init_all(void)
{
	int i;
	s_tz_nw_task_mutex = tzc_mutex_create();
	for (i = 0; i < CONFIG_TASK_NUM; i++) {
		tz_nw_task_client_init(i);
	}
	for (i = 0; i < CONFIG_MGR_TASK_NUM; i++) {
		tz_nw_mgr_task_client_init(i);
	}

	return TZ_SUCCESS;
}

/*
 * register
 */
int tz_nw_task_client_register(int task_id,
		tz_cmd_handler callback, void *userdata)
{
	struct tz_nw_task_client *tc = tz_nw_task_client_get(task_id);

	if (!tc || !callback) {
		error("can't find task %d or callback (0x%08x) is invalid\n",
				task_id, (uint32_t)callback);
		return TZ_ERROR_BAD_PARAMETERS;
	}

	if (tc->state != TZ_NW_TASK_STATE_IDLE) {
		error("task %d, bad state (%d)\n", task_id, tc->state);
		return TZ_ERROR_BAD_STATE;
	}

	tc->callback = callback;
	tc->userdata = userdata;

	return TZ_SUCCESS;
}

int tz_nw_task_client_unregister(int task_id)
{
	struct tz_nw_task_client *tc;

	trace("task_id=%d\n", task_id);

	tc = tz_nw_task_client_get(task_id);

	if (!tc)
	    return TZ_ERROR_BAD_STATE;

	if (tc->state != TZ_NW_TASK_STATE_IDLE) {
		error("task %d, bad state (%d)\n", task_id, tc->state);
		return TZ_ERROR_BAD_STATE;
	}

	memset(&tc->callback, 0, sizeof(tc->callback));

	return TZ_SUCCESS;
}
