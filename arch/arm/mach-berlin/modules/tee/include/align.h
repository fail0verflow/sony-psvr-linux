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
#ifndef _ALIGEN_H_
#define _ALIGEN_H_

#include "types.h"

#ifndef __KERNEL__

#define IS_POWER2(x)		(((x) & ((x) - 1)) == 0)
#define ALIGN_MASK(x, mask)	(((x) + (mask)) & ~(mask))

/* align x to a, a must be power of 2 */
#define ALIGN2(x, a)		ALIGN_MASK(x, (typeof(x))(a) - 1)
#define PTR_ALIGN2(p, a)	((typeof(p))ALIGN2((intptr_t)(p), a))
#define IS_ALIGNED2(x, a)	(((x) & ((typeof(x))(a) - 1)) == 0)

/* generic align x to a, we suppose align to power of 2 too */
#if 0
#define ALIGN(x, a)		(((x) + (a) - 1) / (a) * (a))
#define IS_ALIGNED(x, a)	(((x) % (a)) == 0)
#define PTR_ALIGN(p, a)		((typeof(p))ALIGN((unsigned long)(p), a))
#else
#define ALIGN(x, a)		ALIGN2(x, a)
#define PTR_ALIGN(p, a)		PTR_ALIGN2(p, a)
#define IS_ALIGNED(x, a)	IS_ALIGNED2(x, a)
#endif

#endif /* __KERNEL__ */

#endif /* _ALIGEN_H_ */
