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
#ifndef _REE_SYS_CALLBACK_H_
#define _REE_SYS_CALLBACK_H_

#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/semaphore.h>
#include <linux/spinlock.h>

void REE_SysCallbackInit(void);

#ifndef REE_MUTEX_NAME_MAX_LEN
#define REE_MUTEX_NAME_MAX_LEN	32
#endif

#ifndef REE_SEMAPHORE_NAME_MAX_LEN
#define REE_SEMAPHORE_NAME_MAX_LEN	32
#endif

typedef struct REE_MutexList
{
	spinlock_t		lock;
	int			count;
	struct list_head	head;
} REE_MutexList;

typedef struct REE_Mutex
{
	struct list_head	node;
	struct mutex		mutex;
	int			refCount;
	char			name[REE_MUTEX_NAME_MAX_LEN + 1];
} REE_Mutex;

typedef struct REE_SemaphoreList
{
	spinlock_t		lock;
	int			count;
	struct list_head	head;
} REE_SemaphoreList;

typedef struct REE_Semaphore
{
	struct list_head	node;
	struct semaphore	sem;
	int			refCount;
	char			name[REE_SEMAPHORE_NAME_MAX_LEN + 1];
} REE_Semaphore;

void REE_MutexInit(void);
void REE_SemaphoreInit(void);

#endif /* _REE_SYS_CALLBACK_H_ */
