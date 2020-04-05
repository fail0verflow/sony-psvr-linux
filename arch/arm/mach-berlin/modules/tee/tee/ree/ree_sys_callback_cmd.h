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
#ifndef _REE_SYS_CALLBACK_CMD_H_
#define _REE_SYS_CALLBACK_CMD_H_

#include "types.h"
#include "tee_comm.h"
#include "tee_internal_api.h"

enum REE_SysCallbackCmd {
	REE_CMD_TIME_GET,
	REE_CMD_TIME_WAIT,
	REE_CMD_THREAD_CREATE,
	REE_CMD_THREAD_DESTROY,
	REE_CMD_LOG_WRITE,
	REE_CMD_MUTEX_CREATE,
	REE_CMD_MUTEX_DESTROY,
	REE_CMD_MUTEX_LOCK,
	REE_CMD_MUTEX_TRYLOCK,
	REE_CMD_MUTEX_UNLOCK,
	REE_CMD_SEMAPHORE_CREATE,
	REE_CMD_SEMAPHORE_DESTROY,
	REE_CMD_SEMAPHORE_WAIT,
	REE_CMD_SEMAPHORE_TIMEDWAIT,
	REE_CMD_SEMAPHORE_TRYWAIT,
	REE_CMD_SEMAPHORE_POST,
	REE_CMD_FILE_OPEN,
	REE_CMD_FILE_CLOSE,
	REE_CMD_FILE_READ,
	REE_CMD_FILE_WRITE,
	REE_CMD_FILE_SEEK,
	REE_CMD_FOLDER_MAKE,
	REE_CMD_MAX
};

#ifndef REE_MUTEX_NAME_MAX_LEN
#define REE_MUTEX_NAME_MAX_LEN		32
#endif

#ifndef REE_SEMAPHORE_NAME_MAX_LEN
#define REE_SEMAPHORE_NAME_MAX_LEN	32
#endif

#ifndef REE_FILE_PATH_MAX_LEN
#define REE_FILE_PATH_MAX_LEN		(TEE_COMM_PARAM_EXT_SIZE - 1)
#endif

#ifndef REE_FOLDER_PATH_MAX_LEN
#define REE_FOLDER_PATH_MAX_LEN		(TEE_COMM_PARAM_EXT_SIZE - 1)
#endif

typedef struct REE_MutexCreateParam
{
	void		*lock;
	char		name[REE_MUTEX_NAME_MAX_LEN + 1];
} REE_MutexCreateParam;

typedef struct REE_SemaphoreCreateParam
{
	void		*sem;
	int		value;
	char		name[REE_SEMAPHORE_NAME_MAX_LEN + 1];
} REE_SemaphoreCreateParam;

typedef struct REE_SemaphoreTimedWaitParam
{
	void		*sem;
	uint32_t	timeout;
} REE_SemaphoreTimedWaitParam;

typedef struct REE_FileOpenParam
{
	char		fileName[REE_FILE_PATH_MAX_LEN + 1];
} REE_FileOpenParam;

typedef struct REE_FileReadWriteParam
{
	void		*filp;
	int32_t		offset;
	int32_t		ret;
	size_t		size;
	char		buff[0];
} REE_FileReadWriteParam;

typedef struct REE_FileSeekParam
{
	void		*filp;
	int32_t		offset;
	TEE_Whence	whence;
	uint32_t	pos;
} REE_FileSeekParam;

typedef struct REE_FolderMakeParam
{
	char		folderPath[REE_FOLDER_PATH_MAX_LEN + 1];
} REE_FolderMakeParam;


#endif /* _REE_SYS_CALLBACK_CMD_H_ */
