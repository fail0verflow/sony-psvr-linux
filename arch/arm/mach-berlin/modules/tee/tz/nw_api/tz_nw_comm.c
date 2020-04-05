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
#include "tz_nw_comm.h"
#include "log.h"
#include "smc.h"


#ifndef __KERNEL__
# define tz_nw_comm_invoke_command_nolock	tz_nw_comm_invoke_command
#endif

static int s_mgr_task_occupied = 0;

uint32_t tz_nw_comm_invoke_command_nolock(struct tz_nw_task_client *tc,
		struct tz_nw_comm *cc, uint32_t call_id, void *call_info)
{
	uint32_t result = TZ_SUCCESS;
	uint32_t status;
	union tz_smc_return ret;

	uint32_t cmd;
	uint32_t param0, param1;
    uint32_t cb_result;

	if (!tc) {
		return TZ_ERROR_ITEM_NOT_FOUND;
	}

	/*
	 * FIXME: got bug in lock handling.
	 * 1. can't handle the lock correctly, for another session will always
	 *    enter the callback flow.
	 * 2. if use spinlock, it can't callback to userspace, linux kernel
	 *    complains it as a bug.
	 *
	 * How to fix:
	 * 1. in userspace, must add a lock for each session to avoid the
	 *    session be used in multiple thread.
	 * 2. in this communication protocol, must check whether previous call
	 *    is in progress (by call_id, call_id can be session_id).
	 *    - if yes, and the pending call_id is same as current call_id, then
	 *        enter callback return flow.
	 *    - else if irq is disabled, then report bug: the task/instance must
	 *           not be shared with other sessions.
	 *      else lock the task by mutex.
	 * 3. the channel will be unlock when the whole command is done.
	 */
	if (tc->state == TZ_NW_TASK_STATE_CALLBACK) {
		trace("user callback return, %d, cmd=%d, param=0x%08x, result=0x%08x\n",
			cc->call.task_id, cc->callback.cmd_id,
			cc->callback.param, cc->callback.result);
		cmd = TZ_CMD_DONE;
		param0 = cc->callback.result;
		param1 = TZ_ORIGIN_UNTRUSTED_APP;
	} else {
		cmd = cc->call.cmd_id;
		param0 = cc->call.param;
		param1 = call_id;
#ifndef __KERNEL__
		spin_lock(tc->lock);
#endif
	}
	trace("enter. to %d (state=%d), cmd=%d, param0=0x%08x, param1=0x%08x\n",
		cc->call.task_id, tc->state, cmd, param0, param1);


	tc->state = TZ_NW_TASK_STATE_INVOKING;


	/* fiq trap or callback may break this loop, so, when __smc_tee()
	 * return, the request may not be done.
	 * so we need looply switch to sw until the request is done.
	 */
	while (1) {
		status = __smc_tee(cc->call.task_id, cmd,
				param0, param1, (unsigned long *)&ret);

		if (status & TZ_COMM_RSP_DONE) {
			cc->call.result = ret.res.result;
			cc->call.origin = ret.res.origin;
			/* cc->status = status; */
			tc->state = TZ_NW_TASK_STATE_IDLE;
			result = TZ_SUCCESS;
#ifndef __KERNEL__
			spin_unlock(tc->lock);
#endif
			break;
		}

		if (status & TZ_COMM_RSP_CALLBACK) {
			if (tc->atomic)
				BUG();

			cb_result = TZ_ERROR_NOT_SUPPORTED;
			/* for NW callback:
			 * - SYS CMD is handled in kernel space.
			 * - USER CMD is handled user space.
			 */
			if (ret.cb.cmd_id & TZ_CMD_SYS) {
				uint32_t cb_origin = TZ_ORIGIN_UNTRUSTED_APP;

				trace("kernel callback cmd=%d, param=0x%08x\n",
					ret.cb.cmd_id, ret.cb.param);
				if (tc->callback) {
					cb_result = tc->callback(tc->userdata,
						ret.cb.cmd_id, ret.cb.param,
						&cb_origin);
					cmd = TZ_CMD_DONE;
					param0 = cb_result;			/* result */
					param1 = TZ_ORIGIN_UNTRUSTED_APP;	/* origin */
				}
			}

			/* if kernel space callback doesn't support it, then
			 * callback to user space.
			 */
			if (cb_result == TZ_ERROR_NOT_SUPPORTED) {

				cc->callback.cmd_id = ret.cb.cmd_id;
				cc->callback.param = ret.cb.param;
				tc->state = TZ_NW_TASK_STATE_CALLBACK;
				result = TZ_PENDING;
				trace("callback to user space\n");
				break;
			}
		} else {
			cmd = cc->call.cmd_id;
			/* we don't need to set param or call_id, for
			 * they are not used.
			 */
			param0 = cc->call.param;
			param1 = call_id;
		}
	}

	trace("exit. to %d (state=%d), cmd=%d, param1=0x%08x, param1=0x%08x, result=0x%08x\n",
		cc->call.task_id, tc->state, cmd, param0, param1, result);

	return result;
}

#ifdef __KERNEL__
uint32_t tz_nw_comm_invoke_command(struct tz_nw_task_client *tc,
		struct tz_nw_comm *cc, uint32_t call_id, void *call_info)
{
	uint32_t res = TZ_SUCCESS;

	if (!tc) {
		return TZ_ERROR_ITEM_NOT_FOUND;
	}

	/*
	 * For preempt case, in_atomic() can detect all atomic case,
	 * including the interrupt context.
	 */
	if (in_atomic()) {
		/* By design, for IRQ the task is singleton and should
		 * prevent concurrent access, otherwise result is undefined
		 */

		spin_lock(tc->lock);
		tc->call_info = call_info;
		tc->atomic = true;
		res = tz_nw_comm_invoke_command_nolock(tc, cc, call_id, NULL);
		spin_unlock(tc->lock);
	} else {
		DECLARE_WAITQUEUE(wait, current);

		spin_lock(tc->lock);
		tc->atomic = false;
		add_wait_queue(tc->waitq, &wait);
		for (;;) {
			set_current_state(TASK_INTERRUPTIBLE);

			/* 
			 * TZMGR's call should be issued only when other TZMGR's call is not proceeding.
			 * If task state is TZ_NW_TASK_STATE_CALLBACK, we deal with call is
			 * not completed, and put off next TZMGR's call until previous call
			 * is completed.
			 */
			if (tc->state == TZ_NW_TASK_STATE_IDLE &&
				(tc->task_id != TZ_TASK_ID_MGR || !s_mgr_task_occupied)) {
				/* now own it */
				if (tc->task_id == TZ_TASK_ID_MGR) {
					s_mgr_task_occupied = 1;
				} 
				tc->state = TZ_NW_TASK_STATE_INVOKING;
				tc->call_id = call_id; /* save task identity */
				tc->call_info = call_info;
				break;
			} else if (tc->state == TZ_NW_TASK_STATE_CALLBACK) {
				/* check if we are the owner */
				if (tc->call_id == call_id) {
					break;
				}
			}
			/* no schedule should happen if irq is disabled */
			if (irqs_disabled()) {
				dump_stack();
				res = TZ_ERROR_BAD_STATE;
				break;
			}
			spin_unlock(tc->lock);
			schedule();
			spin_lock(tc->lock);
		}
		set_current_state(TASK_RUNNING);
		remove_wait_queue(tc->waitq, &wait);
		spin_unlock(tc->lock);

		if (res != TZ_SUCCESS)
			return res;

		res = tz_nw_comm_invoke_command_nolock(tc, cc, call_id, NULL);
		if (res == TZ_PENDING) {
			BUG_ON(tc->state != TZ_NW_TASK_STATE_CALLBACK);
			BUG_ON(tc->call_id != call_id);
		} else {
			BUG_ON(res != TZ_SUCCESS);
			wake_up_interruptible(tc->waitq);
		}
	}

	if (tc->task_id == TZ_TASK_ID_MGR && res == TZ_SUCCESS) {
		if (s_mgr_task_occupied) {
			s_mgr_task_occupied = 0;
		}
	}

	return res;
}
#endif
