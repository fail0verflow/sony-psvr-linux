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
#include <linux/mm.h>
#include <linux/fs.h>
#include <linux/compat.h>
#include <linux/uaccess.h>

#include "config.h"
#include "tz_log.h"
#include "tz_utils.h"
#include "tz_nw_ioctl.h"
#include "tz_boot_cmd.h"
#include "tz_driver_private.h"

struct tz_compat_mem_info {
	compat_uptr_t va;
	compat_uptr_t pa;
	compat_uint_t attr;
};

struct tz_compat_memmove_param {
	compat_uptr_t dst;
	compat_uptr_t src;
	compat_size_t size;
};

struct tz_compat_cache_param {
	compat_uptr_t start;
	compat_size_t size;
};

#define SMC_RET(result, func_id) ({ \
	int __rc = 0; \
	if (result != 0) { \
		if (result == SMC_FUNC_ID_UNKNOWN) { \
			tz_error("not suppported func (%u)", func_id); \
			__rc = -EINVAL; \
		} else { \
			tz_error("func(%u) access denied", func_id); \
			__rc = -EACCES; \
		} \
	} \
	__rc; })

static int tzd_compat_fastcall_memmove(
		struct tzd_dev_file *dev, unsigned long arg)
{
	struct tz_compat_memmove_param param;
	uint32_t result, func_id = SMC_FUNC_TOS_MEM_MOVE;
	void __user *argp = (void __user *)arg;
	if (copy_from_user((void *)&param, argp, sizeof(param)))
		return -EFAULT;
	/* ulong __smc(ulong, ulong, ulong, ulong) */
	result = __smc(func_id, param.dst, param.src, param.size);
	return SMC_RET(result, func_id);
}

static int tzd_compat_fastcall_secure_cache(
		struct tzd_dev_file *dev, unsigned long arg, uint32_t func_id)
{
	struct tz_compat_cache_param param;
	uint32_t result;
	void __user *argp = (void __user *)arg;
	if (copy_from_user((void *)&param, argp, sizeof(param)))
		return -EFAULT;
	/* ulong __smc(ulong, ulong, ulong, ulong) */
	result = __smc(func_id, param.start, param.size);
	return SMC_RET(result, func_id);
}

static inline int tzd_compat_fastcall_secure_cache_clean(
		struct tzd_dev_file *dev, unsigned long arg)
{
	uint32_t func_id = SMC_FUNC_TOS_SECURE_CACHE_CLEAN;
	return tzd_compat_fastcall_secure_cache(dev, arg, func_id);
}

static inline int tzd_compat_fastcall_secure_cache_invalidate(
		struct tzd_dev_file *dev, unsigned long arg)
{
	uint32_t func_id = SMC_FUNC_TOS_SECURE_CACHE_INVALIDATE;
	return tzd_compat_fastcall_secure_cache(dev, arg, func_id);
}

static inline int tzd_compat_fastcall_secure_cache_flush(
		struct tzd_dev_file *dev, unsigned long arg)
{
	uint32_t func_id = SMC_FUNC_TOS_SECURE_CACHE_CLEAN_INVALIDATE;
	return tzd_compat_fastcall_secure_cache(dev, arg, func_id);
}

static int tzd_compat_alloc_mem(struct tzd_dev_file *dev, unsigned long arg)
{
	compat_ulong_t __user *argp = (compat_ulong_t __user *)arg;
	compat_ulong_t size;
	void *p;
	if (get_user(size, argp))
		return -EFAULT;
	if (unlikely(!size))
		return -EINVAL;
	p = tzd_shm_alloc(dev, size, GFP_KERNEL);
	if (unlikely(!p))
		return -ENOMEM;
	if (put_user(ptr_to_compat(p), argp)) {
		tzd_shm_free(dev, p);
		return -EFAULT;
	}
	return 0;
}

static int tzd_compat_get_meminfo(struct tzd_dev_file *dev, unsigned long arg)
{
	struct tz_compat_mem_info param;
	void __user *argp = (void __user *)arg;
	unsigned long pa, pte;

	if (copy_from_user((void *)&param, argp, sizeof(param)))
		return -EFAULT;
	if (unlikely(!param.va))
		return -EINVAL;
	pte = tz_user_virt_to_pte(current->mm, param.va);
	if (pte) {
		pa = (pte & PAGE_MASK) +
			((unsigned long)param.va & ~PAGE_MASK);
		param.pa   = ptr_to_compat((void *)pa);
		param.attr = pte & ~PAGE_MASK;
	} else {
		param.pa = param.attr = 0;
	}
	if (copy_to_user(argp, &param, sizeof(param)))
		return -EFAULT;
	return 0;
}

long tzd_compat_ioctl(struct file *file, unsigned cmd, unsigned long arg)
{
	struct tzd_dev_file *dev = (struct tzd_dev_file *) file->private_data;
	if (unlikely(!dev))
		return -ENXIO;
	switch (cmd) {
	case TZ_CLIENT_IOCTL_FASTCALL_MEMMOVE:
		return tzd_compat_fastcall_memmove(dev, arg);
	case TZ_CLIENT_IOCTL_FASTCALL_CACHE_CLEAN:
		return tzd_compat_fastcall_secure_cache_clean(dev, arg);
	case TZ_CLIENT_IOCTL_FASTCALL_CACHE_INVALIDATE:
		return tzd_compat_fastcall_secure_cache_invalidate(dev, arg);
	case TZ_CLIENT_IOCTL_FASTCALL_CACHE_FLUSH:
		return tzd_compat_fastcall_secure_cache_flush(dev, arg);
	case TZ_CLIENT_IOCTL_ALLOC_MEM:
		return tzd_compat_alloc_mem(dev, arg);
	case TZ_CLIENT_IOCTL_GET_MEMINFO:
		return tzd_compat_get_meminfo(dev, arg);
	default:
		return tzd_ioctl(file, cmd, arg);
	}
	return 0;
}
