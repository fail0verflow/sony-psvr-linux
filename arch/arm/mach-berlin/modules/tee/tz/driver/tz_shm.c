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
#include <linux/init.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/sched.h>
#include <linux/version.h>
#include <linux/moduleparam.h>
#include <linux/err.h>
#ifdef CONFIG_ION
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 10, 0)
#include <../drivers/staging/android/ion/ion.h>
#else /* < KERNEL_VERSION(3, 10, 0) */
#include <linux/ion.h>
#endif /* ION Version */
#endif /* CONFIG_ION */
#include "config.h"
#include "tz_log.h"
#include "tz_driver_private.h"

#define ROUND_UP(N, S) ((((N) + (S) - 1) / (S)) * (S))

struct shm_ops {
	int (*init)(void);
	int (*malloc)(struct tzd_shm *tzshm, gfp_t flags);
	int (*free)(struct tzd_shm *tzshm);
	int (*destroy)(void);
};

#ifdef CONFIG_ION
static struct ion_device *tz_ion_dev = NULL;
static struct ion_client *tz_ion_client = NULL;
static unsigned int heap_id_mask = 0;
#define NW_SHM_POOL_ATTR	(ION_A_FC | ION_A_NS)

static int ion_get_nw_all_heapidmask(unsigned int *id_mask)
{
	int heap_num = 0;
	int ret = 0;
	struct ion_heap_info *p = NULL;
	int i = 0;

	ret = ion_get_heap_num(&heap_num);
	if (ret != 0) {
		tz_error("ion_get_heap_num failed ret=%d \n", ret);
		return ret;
	}

	p = kmalloc(heap_num * sizeof(struct ion_heap_info), GFP_KERNEL);

	if (p == NULL) {
		tz_error("ion_get_nw_all_heapidmask kmalloc failed \n");
		return -ENOMEM;
	}

	ret = ion_get_heap_info(p);
	if (ret != 0) {
		tz_error("ion_get_heap_num failed ret=%d\n", ret);
		kfree(p);
		return ret;
	}

	for (i = 0; i < heap_num; i++) {
		if (((p + i)->attribute & NW_SHM_POOL_ATTR) == NW_SHM_POOL_ATTR) {
			*id_mask = 1<<i;
			break;
		}
	}

	kfree(p);
	if (*id_mask == 0) {
		tz_error("ion_get_nw_all_heapidmask failed\n");
		return -EINVAL;
	}
	return 0;
}

static int tzd_ion_shm_init(void)
{
	int ret;

	tz_ion_dev = ion_get_dev();

	tz_ion_client = ion_client_create(tz_ion_dev, "tz");
	if (tz_ion_client <= 0) {
		tz_error("ion_client_create failed\n");
		return -EINVAL;
	}

	ret = ion_get_nw_all_heapidmask(&heap_id_mask);
	if (ret < 0) {
		tz_error("ion_get_nw_all_heapidmask failed\n");
		ion_client_destroy(tz_ion_client);
		return ret;
	}

	return 0;
}

static int tzd_ion_shm_destroy(void)
{
	ion_client_destroy(tz_ion_client);
	return 0;
}

static int tzd_ion_shm_alloc(struct tzd_shm *tzshm, gfp_t flags)
{
	void *alloc_addr;
	struct ion_handle *handle;
	size_t ion_size;
	ion_phys_addr_t pa;

	handle = ion_alloc(tz_ion_client, tzshm->len, 0, heap_id_mask, ION_CACHED);
	if (IS_ERR(handle)) {
		tz_error("ion alloc failed, size=%d", tzshm->len);
		return -ENOMEM;
	}
	alloc_addr = ion_map_kernel(tz_ion_client, handle);
	if (IS_ERR(alloc_addr)) {
                tz_error("ion_map_kernel failed.");
		ion_free(tz_ion_client, handle);
		return -ENOMEM;
	}

	ion_phys(tz_ion_client, handle, &pa, &ion_size);
	tz_debug("client ion memory physaddr[%lx] size[%lu], handle=[%x]\n",
			pa, ion_size, handle);

	tzshm->k_addr = alloc_addr;
	tzshm->u_addr = 0;
	tzshm->userdata = (void *)handle;
	tzshm->p_addr = (void *)pa;

	return 0;
}

static int tzd_ion_shm_free(struct tzd_shm *tzshm)
{
	if (tzshm->userdata) {
		ion_unmap_kernel(tz_ion_client, (struct ion_handle*)tzshm->userdata);
		ion_free(tz_ion_client, (struct ion_handle*)tzshm->userdata);
	}
	return 0;
}

static const struct shm_ops ion_shm_ops = {
	.init = tzd_ion_shm_init,
	.malloc = tzd_ion_shm_alloc,
	.free = tzd_ion_shm_free,
	.destroy = tzd_ion_shm_destroy,
};
#endif /* CONFIG_ION */

static int tzd_kmem_shm_init(void)
{
	return 0;
}

static int tzd_kmem_shm_destroy(void)
{
	return 0;
}

static int tzd_kmem_shm_alloc(struct tzd_shm *tzshm, gfp_t flags)
{
	void *alloc_addr;

	alloc_addr = (void *)__get_free_pages(flags,
			get_order(ROUND_UP(tzshm->len, PAGE_SIZE)));
	if (unlikely(!alloc_addr)) {
		tz_error("get free pages failed");
		return -ENOMEM;
	}

	tzshm->k_addr = alloc_addr;
	tzshm->u_addr = 0;
	tzshm->p_addr = (void *)virt_to_phys(alloc_addr);

	return 0;
}

static int tzd_kmem_shm_free(struct tzd_shm *tzshm)
{
	if (likely(tzshm && tzshm->k_addr))
		free_pages((unsigned long)tzshm->k_addr,
			get_order(ROUND_UP(tzshm->len, PAGE_SIZE)));
	return 0;
}

static const struct shm_ops kmem_shm_ops = {
	.init = tzd_kmem_shm_init,
	.malloc = tzd_kmem_shm_alloc,
	.free = tzd_kmem_shm_free,
	.destroy = tzd_kmem_shm_destroy,
};

#ifdef CONFIG_ION
# define DEFAULT_TZ_SHM_OPS	(&ion_shm_ops)
#else
# define DEFAULT_TZ_SHM_OPS	(&kmem_shm_ops)
#endif
#define FAIL_SAFE_TZ_SHM_OPS	(&kmem_shm_ops)

static const struct shm_ops *tz_shm_ops = DEFAULT_TZ_SHM_OPS;

struct tzd_shm *tzd_shm_new(struct tzd_dev_file *dev, size_t size, gfp_t flags)
{
	struct tzd_shm *shm;

	if (unlikely(!dev || !size))
		return NULL;

	shm = kzalloc(sizeof(*shm), flags);
	if (unlikely(!shm))
		return NULL;
	shm->len = size;
	if (unlikely(tz_shm_ops->malloc(shm, flags) < 0)) {
		tz_error("shm_malloc failure on size (%d)", size);
		kfree(shm);
		return NULL;
	}

	mutex_lock(&dev->tz_mutex);
	list_add_tail(&shm->head, &dev->dev_shm_head.shm_list);
	dev->dev_shm_head.shm_cnt++;
	mutex_unlock(&dev->tz_mutex);

	return shm;
}

void *tzd_shm_alloc(struct tzd_dev_file *dev, size_t size, gfp_t flags)
{
	struct tzd_shm *shm = tzd_shm_new(dev, size, flags);
	if (!shm)
		return NULL;
	return shm->p_addr;
}

int tzd_shm_free(struct tzd_dev_file *dev, const void *x)
{
	struct tzd_shm *shm, *next;

	if (unlikely(!dev))
		return -ENODEV;

	if (unlikely(!x))
		return -EFAULT;

	mutex_lock(&dev->tz_mutex);
	list_for_each_entry_safe(shm, next,
			&dev->dev_shm_head.shm_list, head) {
		if (shm->p_addr == x) {
			list_del(&shm->head);
			mutex_unlock(&dev->tz_mutex);
			if (likely(shm->k_addr))
				tz_shm_ops->free(shm);
			kfree(shm);
			return 0;
		}
	}
	mutex_unlock(&dev->tz_mutex);

	return 0;
}

int tzd_shm_delete(struct tzd_dev_file *dev)
{
	struct tzd_shm *shm, *next;

	if (unlikely(!dev))
		return -ENODEV;

	mutex_lock(&dev->tz_mutex);
	list_for_each_entry_safe(shm, next,
			&dev->dev_shm_head.shm_list, head) {
		list_del(&shm->head);
		if (likely(shm->k_addr))
			tz_shm_ops->free(shm);
		kfree(shm);
	}
	mutex_unlock(&dev->tz_mutex);

	return 0;
}

int tzd_shm_mmap(struct tzd_dev_file *dev, struct vm_area_struct *vma)
{
	struct tzd_shm *shm, *next;
	unsigned long pgoff;

	if (unlikely(!dev))
		return -ENODEV;

	mutex_lock(&dev->tz_mutex);
	list_for_each_entry_safe(shm, next,
			&dev->dev_shm_head.shm_list, head) {
		pgoff = virt_to_phys(shm->k_addr) >> PAGE_SHIFT;
		if (pgoff == vma->vm_pgoff) {
			shm->u_addr = (void *)((unsigned long)vma->vm_start);
			break;
		}
	}
	mutex_unlock(&dev->tz_mutex);

	return 0;
}

void *tzd_shm_phys_to_virt_nolock(struct tzd_dev_file *dev, void *pa)
{
	struct tzd_shm *shm, *next;

	if (unlikely(!dev))
		return NULL;

	list_for_each_entry_safe(shm, next,
			&dev->dev_shm_head.shm_list, head) {
		if ((pa < shm->p_addr + shm->len) && (pa >= shm->p_addr))
			return shm->k_addr + (pa - shm->p_addr);
	}

	return NULL;
}

void *tzd_shm_phys_to_virt(struct tzd_dev_file *dev, void *pa)
{
	void *va;

	if (unlikely(!dev))
		return NULL;
	mutex_lock(&dev->tz_mutex);
	va = tzd_shm_phys_to_virt_nolock(dev, pa);
	mutex_unlock(&dev->tz_mutex);

	return va;
}

#ifdef CONFIG_ION
#define DEFAULT_SHM_TYPE	"ion"
#else
#define DEFAULT_SHM_TYPE	"kmem"
#endif

#define MAX_TYPE_LENGTH 8

static char shm_type[MAX_TYPE_LENGTH] = DEFAULT_SHM_TYPE;
#ifdef CONFIG_ION
module_param_string(shm_type, shm_type, sizeof (shm_type), 0);
MODULE_PARM_DESC(shm_type,
	"ion: use ion shared mem; kmem: use kmem shared mem");
#endif

int __init tzd_shm_init(void)
{
	int ret;
#ifdef CONFIG_ION
	if (!strncmp(shm_type, "ion", 3))
		tz_shm_ops = &ion_shm_ops;
	else
#endif
	if (!strncmp(shm_type, "kmem", 4))
		tz_shm_ops = &kmem_shm_ops;
	if (!tz_shm_ops)
		tz_shm_ops = FAIL_SAFE_TZ_SHM_OPS;
again:
	if (tz_shm_ops->init) {
		ret = tz_shm_ops->init();
		if (ret && tz_shm_ops != FAIL_SAFE_TZ_SHM_OPS) {
			tz_shm_ops = FAIL_SAFE_TZ_SHM_OPS;
			goto again;
		}
		return ret;
	}
	return 0;
}

int __exit tzd_shm_exit(void)
{
	return tz_shm_ops->destroy();
}
