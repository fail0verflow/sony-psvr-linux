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
#ifndef _TZ_CLIENT_API_H_
#define _TZ_CLIENT_API_H_

#include "tz_errcode.h"
#include "tz_comm.h"
#include "tz_nw_comm_protocol.h"

/** open tz client device
 *
 * @param dev		device name. NULL for default device.
 *
 * @retval -1		fail to open the device.
 * @retval !=-1		device handle.
 */
int tzc_open(const char *dev);

/** close tz device
 *
 * @param tzc		tz client handle.
 */
void tzc_close(int tzc);

/** allocate shared memory.
 * the shared memory is used for communication between nw<->sw.
 * its physical address is continous.
 *
 * @param fd		file return by open().
 * @param size		size to allocate.
 * @param pa		buffer to save the physical address.
 *			it's used for nw<->sw communication.
 *
 * @retval NULL		fail to allocate shared memory
 * @retval !NULL	virtual address of the allocated shared memory.
 *
 * @sa tzc_free_shm()
 */
void *tzc_alloc_shm(int tzc, size_t size, void **pa);

/** free shared memory allocated by tzc_alloc_shm().
 *
 * @param va		virtual address of the allocated shm.
 * @param pa		physical address of the allocated shm.
 * @param size		size to free.
 *
 * @sa tzc_alloc_shm()
 */
void tzc_free_shm(int tzc, void *va, void *pa, size_t size);

int tzc_invoke_command(int tzc, int task_id, uint32_t cmd_id,
			uint32_t param, uint32_t *origin,
			tz_cmd_handler callback, void *userdata);

void *tzc_get_mem_info(int tzc, void *va, uint32_t *attr);

/*
 * mutex utils.
 *
 * one session may be used by multiple threads, but the session
 * context can't be accessed concurrently.
 */

/** create mutex.
 *
 * @retval NULL		fail to create mutex.
 * @retval !NULL	mutex handle.
 */
void *tzc_mutex_create(void);

/** destroy mutex.
 *
 * @param lock		mutex handle.
 *
 * @retval TZ_SUCCESS		success to destroy the lock.
 * @retval TZ_ERROR_GENERIC	system error.
 */
int tzc_mutex_destroy(void *lock);

/** lock mutex.
 *
 * @param lock		mutex handle.
 *
 * @retval TZ_SUCCESS		success to destroy the lock.
 * @retval TZ_ERROR_GENERIC	system error.
 */
int tzc_mutex_lock(void *lock);

/** try to lock mutex.
 *
 * @param lock		mutex handle.
 *
 * @retval TZ_SUCCESS		success to destroy the lock.
 * @retval TZ_ERROR_BAD_STATE	lock is owned by others.
 * @retval TZ_ERROR_GENERIC	system error.
 */
int tzc_mutex_trylock(void *lock);

/** unlock mutex.
 *
 * @param lock		mutex handle.
 *
 * @retval TZ_SUCCESS		success to destroy the lock.
 * @retval TZ_ERROR_GENERIC	system error.
 */
int tzc_mutex_unlock(void *lock);


#ifdef CONFIG_TEE
#include "tee_comm.h"

struct tee_comm *tzc_acquire_tee_comm_channel(int tzc);

void tzc_release_tee_comm_channel(int tzc, struct tee_comm *tc);

/** invoke command on a specified task.
 *
 * below fields must be filled in cmd.
 * - task_id: task to issue command.
 * - cmd_id: command to issue.
 * - param_types: param type of params, see TEE_PARAM_TYPE_*
 * - params[4]: 4 parameters. for memref.buffer, the address must be
 *		physical address.
 * - param_ext_size: size of extend parameters, if 4 params are not enough.
 * - shm: extend parameter buffer. (param_ext)
 *		format is user defined. must sync with TZ service.
 *
 * when return, below items would be updated.
 * - params[4]: value.a, value.b and memref.size may be updated.
 * - shm: for extend parameters
 * - rsp.result: result code
 * - rsp.origin: the origin to return rsp.return.
 *
 * @retval >=0	success to invoke command, check rsp.result and rsp.origin
 *		for returns.
 * @retval <0	fail to invoke command.
 */
/* for all tee communication channel, we don't need to care the page fault
 * issue, for this page won't swap out from DDR. But we need to care whether
 * user space pass a bad pointer.
 */
#define TZ_PA_RANGE_MIN 0x008c0000
#define TZ_PA_RANGE_MAX 0x1C400000

static inline int tzc_invoke_tee_user_command(int tzc, int task_id,
			struct tee_comm *tc, uint32_t *origin,
			tz_cmd_handler callback, void *userdata)
{
	uint32_t param = (uint32_t)((unsigned long)tc->pa) +
				offsetof(struct tee_comm, call_param);
#if 0
	if (!(TZ_PA_RANGE_MIN <= param && param <= TZ_PA_RANGE_MAX)) {
		 return TZ_ERROR_SECURITY;
	}
#endif
	return tzc_invoke_command(tzc, task_id, TZ_CMD_TEE_USER,
				param, origin, callback, userdata);
}

static inline int tzc_invoke_tee_sys_command(int tzc, int task_id,
			struct tee_comm *tc, uint32_t *origin,
			tz_cmd_handler callback, void *userdata)
{
	uint32_t param = (uint32_t)((unsigned long)tc->pa) +
				offsetof(struct tee_comm, call_param);
#if 0
	if (!(TZ_PA_RANGE_MIN <= param && param <= TZ_PA_RANGE_MAX)) {
		return TZ_ERROR_SECURITY;
	}
#endif
	return tzc_invoke_command(tzc, task_id, TZ_CMD_TEE_SYS,
				param, origin, callback, userdata);
}

#endif /* CONFIG_TEE */

/*
 * Fast Calls
 */

int tzc_fast_memmove(int tzc, void *dst_phy_addr, void *src_phy_addr, size_t size);

int tzc_fast_secure_cache_clean(int tzc, void *phy_addr, size_t size);

int tzc_fast_secure_cache_invalidate(int tzc, void *phy_addr, size_t size);

int tzc_fast_secure_cache_flush(int tzc, void *phy_addr, size_t size);

int tzc_open_session(int tzc, unsigned long param, uint32_t taskId,
                        uint32_t *origin);

int tzc_close_session(int tzc, unsigned long param, uint32_t taskId,
                        uint32_t *origin);

int tzc_create_instance(int tzc, unsigned long param, uint32_t *origin);

int tzc_destroy_instance(int tzc, unsigned long param, uint32_t *origin);
#endif /* _TZ_CLIENT_API_H_ */
