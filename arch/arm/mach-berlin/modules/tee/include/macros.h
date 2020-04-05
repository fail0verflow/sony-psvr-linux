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
#ifndef __MACROS_H__
#define __MACROS_H__

#include "align.h"

#ifndef MAX
# define MAX(a, b)	(((a) > (b)) ? (a) : (b))
#endif

#ifndef MIN
# define MIN(a, b)	(((a) < (b)) ? (a) : (b))
#endif

#ifndef UNUSED
# define UNUSED(x)	((void)(x))
#endif

#ifndef N_ELEMENTS
# define N_ELEMENTS(x)	(sizeof(x) / sizeof((x)[0]))
#endif

#ifndef ARRAY_SIZE
# define ARRAY_SIZE	N_ELEMENTS
#endif

#ifndef ALWAYS_INLINE
# define ALWAYS_INLINE	inline __attribute__((always_inline))
#endif

#ifndef STRINGIFY
# define __STRINGIFY(x)	#x
# define STRINGIFY(x)	__STRINGIFY(x)
#endif

#if defined(__GNUC__) && (__GNUC__ > 2)
# define LIKELY(expr)	(__builtin_expect(!!(expr), 1))
# define UNLIKELY(expr)	(__builtin_expect(!!(expr), 0))
#else
# define LIKELY(expr)	(expr)
# define UNLIKELY(expr)	(expr)
#endif

#ifndef offsetof
# if defined(__GNUC__)  && __GNUC__ >= 4
#  define offsetof(TYPE, MEMBER)		__builtin_offsetof(TYPE, MEMBER)
# else
#  define offsetof(TYPE, MEMBER)		((size_t)&(((TYPE *)0)->MEMBER))
# endif
#endif

#ifndef container_of
# define container_of(PTR, TYPE, MEMBER)	((TYPE *)((char *)(PTR) - offsetof(TYPE, MEMBER)))
#endif

#ifndef WEAK_ALIAS
# define WEAK_ALIAS(name, aliasname) \
	extern typeof(name) aliasname __attribute__ ((weak, alias (#name)));
#endif

#ifndef STRONG_ALIAS
# define STRONG_ALIAS(name, aliasname) \
	extern typeof(name) aliasname __attribute__ ((alias (#name)));
#endif

#ifndef BUG
# define BUG(str) do { \
		printk("BUG: " str " failure at %s:%d/%s()!\n", \
			__FILE__, __LINE__, __func__); \
		cpu_halt(); \
	} while (0)
#endif

#ifndef bug
# define bug()	BUG("")
#endif

#ifndef BUG_ON
# define BUG_ON(cond)	\
	do { if (UNLIKELY(cond)) BUG(STRINGIFY(cond)); } while (0)
#endif

#ifndef bug_on
# define bug_on(cond)	BUG_ON(cond)
#endif

#endif
