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
#ifndef __TZ_LOG_H__
#define __TZ_LOG_H__

#include <linux/printk.h>
#include "config.h"

#ifdef CONFIG_DEBUG
#define tz_debug(fmt, args...)	\
	printk(KERN_DEBUG "TZ DEBUG %s(%i, %s): " fmt "\n", \
			__func__, current->pid, current->comm, ## args)
#else
#define tz_debug(...)
#endif

#define tz_info(fmt, args...)	\
	printk(KERN_INFO "TZ INFO %s(%i, %s): " fmt "\n", \
			__func__, current->pid, current->comm, ## args)

#define tz_error(fmt, args...)	\
	printk(KERN_ERR "TZ ERROR %s(%i, %s): " fmt "\n", \
			__func__, current->pid, current->comm, ## args)

#endif /* __TZ_LOG_H__ */
