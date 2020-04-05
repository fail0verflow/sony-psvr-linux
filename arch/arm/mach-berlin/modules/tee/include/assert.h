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
#ifndef _ASSERT_H_
#define _ASSERT_H_

#ifdef __TRUSTZONE__
#include "cpu_context.h"
#define HALT()		cpu_halt()
#else /* !__TRUSTZONE__ */
#define HALT()		while(1)
#endif /* __TRUSTZONE__ */

#ifdef CONFIG_ASSERT
#define assert(_x_)							\
	do {								\
		if (!(_x_)) {						\
			printf("assert fail at "#_x_ " %s:%d/%s()\n",	\
					__FILE__, __LINE__, __func__);	\
			HALT();						\
		}							\
	}while(0)
#else
#define assert(_x_)
#endif /* CONFIG_ASSERT */

#endif /* _ASSERT_H_ */
