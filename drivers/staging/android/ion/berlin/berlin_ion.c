/*
 * Copyright 2012 Marvell International Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/file.h>
#include <linux/err.h>
#include <linux/fs.h>
#include <linux/platform_device.h>
#include <linux/list.h>
#include <linux/kthread.h>
#include <linux/mm.h>
#include <linux/mm_types.h>
#include <linux/slab.h>
#include <linux/rbtree.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>
#include <asm/cacheflush.h>
#include <linux/debugfs.h>
#include <linux/list.h>
#include <linux/dma-buf.h>
#include <linux/string.h>
#include <linux/miscdevice.h>
#include <linux/idr.h>
#include <linux/netlink.h>
#include <net/sock.h>

#include "../ion.h"
#include "../ion_priv.h"


struct ion_handle {
	struct kref ref;
	struct ion_client *client;
	struct ion_buffer *buffer;
	struct rb_node node;
	unsigned int kmap_cnt;
	int id;
};

struct ion_device {
	struct miscdevice dev;
	struct rb_root buffers;
	struct mutex buffer_lock;
	struct rw_semaphore lock;
	struct plist_head heaps;
	long (*custom_ioctl) (struct ion_client *client, unsigned int cmd,
			      unsigned long arg);
	struct rb_root clients;
	struct dentry *debug_root;
};

struct ion_client {
	struct rb_node node;
	struct ion_device *dev;
	struct rb_root handles;
	struct idr idr;
	struct mutex lock;
	const char *name;
	struct task_struct *task;
	pid_t pid;
	struct dentry *debug_root;
};

struct berlin_ion_info {
	int heap_mun;
	struct ion_platform_heap *heaps_data;
	struct ion_heap **heaps;
	struct dentry *debug_root;
};

struct ion_vma_list {
	struct list_head list;
	struct vm_area_struct *vma;
};

struct berlin_gid_root {
	struct rw_semaphore rwsem;
	struct rb_root root;
	struct dentry *debug_root;
};

struct berlin_gid_node {
	struct rb_node gid_node;
	unsigned int gid;
	pid_t threadid;
	char threadname[TASK_COMM_LEN];
	pid_t pid;
	char task_comm[TASK_COMM_LEN];
	void *data;
	int ref;
	struct list_head user_list;
};

struct berlin_gid_user_node
{
	pid_t threadid;
	char threadname[TASK_COMM_LEN];
	pid_t pid;
	char taskname[TASK_COMM_LEN];
	struct list_head list;
};

struct secure_heap {
	int heap_num;
	int heap_id[20];
};

#define BERLIN_CC_DYNAMIC_ID	(0x80000000)
#define BERLIN_CC_START_ID	(0x80000001)
#define BERLIN_CC_DEBUG_LEVEL	(20)

#define NETLINK_BERLIN_CC			(29)
#define NETLINK_BERLIN_CC_GROUP		(0)
#define BERLIN_CC_ICCFIFO_FRAME_SIZE		(128)


extern struct dma_buf_ops dma_buf_ops;
extern void ion_buffer_get(struct ion_buffer *buffer);
extern int ion_buffer_put(struct ion_buffer *buffer);
extern struct ion_handle *ion_handle_lookup(struct ion_client *client,
				struct ion_buffer *buffer);
extern void ion_handle_get(struct ion_handle *handle);
extern int ion_handle_put(struct ion_handle *handle);
extern int ion_handle_add(struct ion_client *client, struct ion_handle *handle);
extern struct ion_handle *ion_handle_create(struct ion_client *client,
				     struct ion_buffer *buffer);


static struct ion_device *berlin_idev = NULL;
static struct berlin_ion_info *berlin_info = NULL;
static struct berlin_gid_root berlin_gid;
static struct berlin_gid_root berlin_cc;
static struct secure_heap berlin_secure;
struct sock *berlin_socket;


int ion_kill_size = 2048; // KB
module_param_named(debug_kill_size, ion_kill_size, int, S_IRUGO | S_IWUSR);

int ion_lowmem_start_oom_adj = 0;
module_param_named(start_oom_adj, ion_lowmem_start_oom_adj, int, S_IRUGO | S_IWUSR);

int ion_alloc_retry_count = 10;
module_param_named(alloc_retry_count, ion_alloc_retry_count, int, S_IRUGO | S_IWUSR);

static int ion_debug_level = 3;
module_param_named(debug_print_level, ion_debug_level, int, S_IRUGO | S_IWUSR);

static int ion_debug_flag = 0;
module_param_named(debug_print_flag, ion_debug_flag, int, S_IRUGO | S_IWUSR);

static int ion_debug_heap_details = 0;
module_param_named(debug_heap_details, ion_debug_heap_details, int, S_IRUGO | S_IWUSR);



#define berlin_debug(level, x...)   \
	do {                                \
		if (((ion_debug_level > (level)) && (ion_debug_flag == 0)) || \
			((ion_debug_level == (level)) && (ion_debug_flag == 1))) \
			printk(x);                  \
	}while(0)


static void *berlin_ion_malloc_info(struct device *dev, int heap_num)
{
	int size = 0;
	unsigned char* p = NULL;
	struct berlin_ion_info *info = NULL;
	int info_len = sizeof(struct berlin_ion_info);
	int heaps_data_len = sizeof(struct ion_platform_heap);
	int heaps_len = sizeof(struct ion_heap *);

	size =  info_len + heaps_data_len * heap_num
		+ heaps_len * heap_num;
	p = devm_kzalloc(dev, size, GFP_KERNEL);
	if (p == NULL) {
		printk("berlin_ion_malloc_info fail\n");
		return p;
	}

	memset(p, 0, size);
	info = (struct berlin_ion_info *)(p);
	info->heap_mun = heap_num;
	info->heaps_data = (struct ion_platform_heap *)(p+info_len);
	info->heaps = (struct ion_heap **)(p + info_len +
			heaps_data_len * heap_num);

	return (void *)(p);
}


static int berlin_ion_get_info(struct device *dev, struct berlin_ion_info **info)
{
	int i, res = -ENODEV;
	int heap_num = 0;
	int attri_num = 0;
	int size = 0;
	int uint_len = sizeof(unsigned int);
	unsigned int *attributes;
	struct device_node *np;
	struct resource r;
	struct berlin_ion_info *tmp_info;

	np = of_find_compatible_node(NULL, NULL, "marvell,berlin-ion");
	if (!np)
		goto err_node;
	res = of_property_read_u32(np, "marvell,ion-pool-num", &heap_num);
	if (res)
		goto err_reg_device;

	res = of_property_read_u32(np, "marvell,ion-attributes-num-per-pool",
						&attri_num);
	if (res)
		goto err_reg_device;

	tmp_info = (struct berlin_ion_info *)berlin_ion_malloc_info(dev, heap_num);
	if (tmp_info == NULL) {
		res = -ENOMEM;
		printk("berlin_ion_malloc_info fail\n");
		goto err_reg_device;
	}

	size = heap_num * attri_num * uint_len;
	attributes = (unsigned int *)devm_kzalloc(dev, size, GFP_KERNEL);
	if (attributes == NULL) {
		res = -ENOMEM;
		goto err_reg_device;
	}

	res = of_property_read_u32_array(np, "marvell,ion-pool-attributes",
				attributes, heap_num*attri_num);
	if (res) {
		printk("get mrvl,ion-pool-attributes fail\n");
		goto err_reg_device;
	}

	for (i = 0; i < heap_num; i++) {
		res = of_address_to_resource(np, i, &r);
		if (res)
			goto err_reg_device;
		(tmp_info->heaps_data + i)->id = i;
		(tmp_info->heaps_data + i)->base = r.start;
		(tmp_info->heaps_data + i)->size = resource_size(&r);
		(tmp_info->heaps_data + i)->name = r.name;
		(tmp_info->heaps_data + i)->type = *(attributes + i*attri_num);
		(tmp_info->heaps_data + i)->attribute = *(attributes + i*attri_num + 1) ;
	}

	devm_kfree(dev, attributes);
	of_node_put(np);
	*info = tmp_info;
	return 0;

err_reg_device:
	of_node_put(np);
err_node:
	printk("berlin_ion_get_info failed !!! (%d)\n", res);

	return res;
}


static int berlin_ion_print_info(struct berlin_ion_info *info)
{
	int i = 0;

	printk("Marvell Berlin ION information\n");

	for (i = 0; i < info->heap_mun; i++) {
		printk("heap_id[%d] memory start[%08lx] len[%08x]"
			" name[%s] type[%d] attri[%08x]\n",
			(info->heaps_data + i)->id,
			(info->heaps_data + i)->base,
			(info->heaps_data + i)->size,
			(info->heaps_data + i)->name,
			(info->heaps_data + i)->type,
			(info->heaps_data + i)->attribute);
	}
	return 0;
}


static int berlin_get_heap_num(int *heap_num)
{
	*heap_num = berlin_info->heap_mun;
	return 0;
}


static int berlin_get_heap_detail_info(struct ion_heap_info *info)
{
	int i = 0;

	for (i=0; i< berlin_info->heap_mun; i++) {
		(info+i)->id = (berlin_info->heaps_data + i)->id;
		(info+i)->base = (berlin_info->heaps_data + i)->base;
		(info+i)->size = (berlin_info->heaps_data + i)->size;
		(info+i)->type = (berlin_info->heaps_data + i)->type;
		(info+i)->attribute = (berlin_info->heaps_data + i)->attribute;
		strncpy((info+i)->name, (berlin_info->heaps_data + i)->name, 16);
	}
	return 0;
}


static void berlin_get_secure_heap_info(void)
{
	int i = 0;

	berlin_secure.heap_num = 0;
	for (i = 0; i < berlin_info->heap_mun; i++) {
		if (((berlin_info->heaps_data + i)->attribute & ION_A_FS)
							== ION_A_FS) {
			berlin_secure.heap_id[berlin_secure.heap_num] = i;
			berlin_secure.heap_num++;
		}
	}
	berlin_debug(1, "berlin found %d secure heap\n", berlin_secure.heap_num);
	for (i = 0; i < berlin_secure.heap_num; i++) {
		berlin_debug(1, "berlin found secure heap[%d] head_id[%d]\n", i,
			berlin_secure.heap_id[i]);
	}
}



int berlin_check_secure_heap(struct ion_heap *heap)
{
	int i = 0;
	for (i = 0; i < berlin_secure.heap_num; i++) {
		if (berlin_secure.heap_id[i] == heap->id)
			return 1;
	}
	return 0;
}


size_t berlin_get_heap_size(int heap_id)
{
	return (berlin_info->heaps_data + heap_id)->size;
}


size_t berlin_get_heap_addr(int heap_id)
{
	return (berlin_info->heaps_data + heap_id)->base;
}


int berlin_get_heap_print_details(void)
{
	return ion_debug_heap_details;
}


struct vm_area_struct *berlin_ion_find_vma(struct ion_buffer *buffer,
					size_t virtaddr,  size_t len)
{
	struct ion_vma_list *vma_list;
	struct vm_area_struct *vma;
	berlin_debug(15, "berlin_ion_find_vma virtaddr[%x] len[%x]\n",virtaddr, len);
	list_for_each_entry(vma_list, &(buffer->vmas), list) {
		vma = vma_list->vma;
		berlin_debug(15, "vma->vm_mm[%p] current->mm[%p] vma->vm_start[%lx]"
			" vma->vm_end[%lx] virtaddr[%x] len[%x]\n",
			vma->vm_mm, current->mm,
			vma->vm_start, vma->vm_end, virtaddr, len);
		if ((vma->vm_mm == current->mm) &&
			(vma->vm_start <= virtaddr) &&
			(virtaddr + len <= vma->vm_end)){
			return vma;
		}
	}
	return NULL;
}


int berlin_get_heap_status(struct ion_heap *heap, struct heap_status *status)
{
	struct rb_node *n;
	struct ion_device *dev = heap->dev;
	struct ion_buffer *buffer;
	struct task_struct *ion_task;
	struct ion_process_status *ps;
	int i = 0;
	int flag = 0;

	status->process_num = 0;

	mutex_lock(&dev->buffer_lock);
	for (n = rb_first(&dev->buffers); n; n = rb_next(n)) {
		buffer = rb_entry(n, struct ion_buffer, node);
		flag = 0;
		ion_task = NULL;

		if (status->process_num == BERLIN_MAX_CLIENT) {
			printk("%s error ion client is too much and "
				"more than %d\n", __func__,
				BERLIN_MAX_CLIENT);
			break;
		}

		for (i = 0; i < status->process_num; i++) {
			if (status->process[i].pid == buffer->pid) {
				flag = 1;
				break;
			}
		}

		if ((flag == 1) || (buffer->heap->id != heap->id))
			continue;

		ps = &status->process[status->process_num];
		ps->pid = buffer->pid;
		ps->size = 0;
		ion_task = find_task_by_vpid(buffer->pid);
		if (ion_task) {
			ps->task = ion_task;
			ps->oom_score_adj
				= ion_task->signal->oom_score_adj;
			get_task_comm(ps->name, ion_task);
		} else {
			ps->task = NULL;
			ps->oom_score_adj = -5000;
			strcpy(ps->name, buffer->task_comm);
		}
		status->process_num++;
	}

	for (n = rb_first(&dev->buffers); n; n = rb_next(n)) {
		buffer = rb_entry(n, struct ion_buffer, node);
		for (i = 0; i < status->process_num; i++) {
			if ((status->process[i].pid == buffer->pid)
				&& (buffer->heap->id == heap->id)) {
				status->process[i].size += buffer->size;
				break;
			}
		}

		if ((i == status->process_num) &&
			(buffer->heap->id == heap->id)) {
			printk("%s error, don't forget buffer pid[%d] name[%s]"
				" size[%x] heapid[%d]\n", __func__, buffer->pid,
				buffer->task_comm, buffer->size, heap->id);
		}
	}
	mutex_unlock(&dev->buffer_lock);

	return 0;
}


int berlin_print_heap_status(struct ion_heap *heap, struct heap_status *status)
{
	int i = 0;
	size_t total_size = 0;

	berlin_debug(2, "%s heap physical start[0x%08lx] size[%d]\n", heap->name,
		 (berlin_info->heaps_data + heap->id)->base,
		 (berlin_info->heaps_data + heap->id)->size);
	berlin_debug(2, "---------------------------------------------------\n");
	berlin_debug(2, "|No |  pid |  process_name  |   size   | oom_adj|\n");

	for (i = 0; i< status->process_num; i++) {
		total_size += status->process[i].size;
		berlin_debug(2, "|%3d|%6d|%16s|%10d|%8d|\n",
			i, status->process[i].pid, status->process[i].name,
			status->process[i].size,
			status->process[i].oom_score_adj);
	}
	berlin_debug(2, "---------------------------------------------------\n");
	berlin_debug(2, "all used memory total size[%d] and free size[%d]\n",
		total_size, (berlin_info->heaps_data + heap->id)->size - total_size);

	return 0;
}


static struct ion_buffer *berlin_get_buffer(int share_fd)
{
	struct dma_buf *dmabuf;
	struct ion_buffer *buffer;

	dmabuf = dma_buf_get(share_fd);
	if (IS_ERR(dmabuf)) {
		printk("%s: dma_buf_get fail\n", __func__);
		return NULL;
	}

	if (dmabuf->ops != &dma_buf_ops) {
		printk("%s: got dmabuf->ops fail\n", __func__);
		dma_buf_put(dmabuf);
		return NULL;
	}

	buffer = (struct ion_buffer *)dmabuf->priv;
	dma_buf_put(dmabuf);

	return buffer;
}


static void berlin_gid_insert(struct rb_root *gid_root,
			struct berlin_gid_node *gid_node)
{
	struct rb_node **p = &gid_root->rb_node;
	struct rb_node *parent = NULL;
	struct berlin_gid_node *tmp_node;

	while (*p) {
		parent = *p;
		tmp_node = rb_entry(parent, struct berlin_gid_node, gid_node);

		if (gid_node->gid < tmp_node->gid)
			p = &parent->rb_left;
		else
			p = &parent->rb_right;
	}

	rb_link_node(&gid_node->gid_node, parent, p);
	rb_insert_color(&gid_node->gid_node, gid_root);
}


static struct berlin_gid_node *berlin_gid_lookup(
			struct rb_root *gid_root, unsigned int gid)
{
	struct rb_node *n = gid_root->rb_node;
	struct berlin_gid_node *tmp_node;

	while (n) {
		tmp_node = rb_entry(n, struct berlin_gid_node, gid_node);
		if (gid < tmp_node->gid) {
			n = n->rb_left;
		} else if (gid > tmp_node->gid) {
			n = n->rb_right;
		} else {
			return tmp_node;
		}
	}
	return NULL;
}


static void berlin_gid_delete(struct rb_root *gid_root,
			struct berlin_gid_node *gid_node)
{
	rb_erase(&gid_node->gid_node, gid_root);
}


static int berlin_alloc_gid(struct rb_root *gid_root,
		unsigned int min_gid, unsigned int *gid)
{
	struct rb_node *node;
	struct berlin_gid_node *tmp;
	unsigned int tmp_gid = min_gid;
	int flag = 0;

	node = rb_first(gid_root);
	if (node == NULL) {
		*gid = min_gid;
		return 0;
	}

	while (node) {
		tmp = rb_entry(node, struct berlin_gid_node, gid_node);
		node = rb_next(node);
		if (tmp->gid > min_gid) {
			if (flag == 0) {
				*gid = min_gid;
				return 0;
			}

			if (tmp_gid + 1 < tmp->gid) {
				*gid = tmp_gid + 1;
				return 0;
			} else {
				tmp_gid = tmp->gid;
			}
		} else if (tmp->gid == min_gid) {
			flag = 1;
		}
	}
	*gid = tmp_gid + 1;
	return 0;
}


int berlin_ion_gid_create(struct ion_buffer *buffer)
{
	unsigned int new_gid = 0;
	int ret = 0;
	struct berlin_gid_node *node;

	if (buffer == NULL)
		return -EBADF;

	node = kzalloc(sizeof(struct berlin_gid_node), GFP_KERNEL);
	if (!node) {
		printk("%s: kzalloc fail\n", __func__);
		return -ENOMEM;
	}

	node->data = (void *)(buffer);
	node->pid = task_tgid_vnr(current);
	get_task_comm(node->task_comm, current->group_leader);
	node->threadid = task_pid_vnr(current);
	strncpy(node->threadname, current->comm, TASK_COMM_LEN);
	INIT_LIST_HEAD(&node->user_list);

	down_write(&berlin_gid.rwsem);
	ret = berlin_alloc_gid(&berlin_gid.root, 1, &new_gid);
	if (ret != 0) {
		kfree(node);
		printk("%s: get new gid fail\n", __func__);
		up_write(&berlin_gid.rwsem);
		return -1;
	}

	buffer->gid = new_gid;
	node->gid = new_gid;
	node->ref = 1;
	berlin_gid_insert(&berlin_gid.root, node);
	up_write(&berlin_gid.rwsem);
	berlin_debug(9, "%s pid[%d] create gid[%u]\n",
		__func__, node->pid, node->gid);

	return 0;
}


int berlin_ion_gid_destroy(struct ion_buffer *buffer)
{
	struct berlin_gid_node *node;
	pid_t pid = task_tgid_vnr(current);

	if (buffer == NULL)
		return -EBADF;

	down_write(&berlin_gid.rwsem);
	node = berlin_gid_lookup(&berlin_gid.root, buffer->gid);
	if (!node) {
		up_write(&berlin_gid.rwsem);
		printk("%s: pid[%d] can't found gid[%u]\n", __func__,
					pid, buffer->gid);
		return -1;
	}

	berlin_gid_delete(&berlin_gid.root, node);
	up_write(&berlin_gid.rwsem);

	berlin_debug(9, "%s pid[%d] destroy gid[%u]\n",
		__func__, pid, node->gid);
	kfree(node);

	return 0;
}


static int berlin_ion_get_gid(struct ion_berlin_data *region)
{
	struct ion_buffer *buffer;

	buffer = berlin_get_buffer(region->share_fd);
	if (buffer == NULL)
		return -EBADF;
	region->gid = buffer->gid;
	berlin_debug(9, "%s pid[%d] get gid[%u]\n", __func__,
			task_tgid_vnr(current), buffer->gid);
	return 0;
}


static int berlin_ion_get_sfd(struct ion_berlin_data *region)
{
	int fd;
	struct ion_buffer *buffer;
	struct dma_buf *dmabuf;
	struct berlin_gid_node *node;

	down_read(&berlin_gid.rwsem);
	node = berlin_gid_lookup(&berlin_gid.root, region->gid);
	up_read(&berlin_gid.rwsem);
	if (!node) {
		printk("%s: can't found gid[%u]\n", __func__, region->gid);
		return -1;
	}

	buffer = (struct ion_buffer *)(node->data);
	if (buffer == NULL) {
		printk("%s: get buffer fail[%p]\n", __func__, buffer);
		return -EBADF;
	}

	ion_buffer_get(buffer);
	dmabuf = dma_buf_export(buffer, &dma_buf_ops, buffer->size, O_RDWR);
	if (IS_ERR(dmabuf)) {
		ion_buffer_put(buffer);
		printk("%s: dma export buffer fail[%p]\n", __func__, buffer);
		return PTR_ERR(dmabuf);
	}

	fd = dma_buf_fd(dmabuf, O_CLOEXEC);
	if (fd < 0) {
		printk("%s: dma get fd fail\n",__func__ );
		dma_buf_put(dmabuf);
	}

	region->share_fd = fd;
	return 0;
}


static int berlin_ion_phys(struct ion_berlin_data *region)
{
	struct ion_buffer *buffer;
	struct berlin_gid_node *node;

	down_read(&berlin_gid.rwsem);
	node = berlin_gid_lookup(&berlin_gid.root, region->gid);
	up_read(&berlin_gid.rwsem);
	if (!node) {
		printk("%s: can't found gid[%u]\n", __func__, region->gid);
		return -1;
	}

	buffer = (struct ion_buffer *)(node->data);
	if (buffer == NULL) {
		printk("%s: get buffer fail[%p]\n", __func__, buffer);
		return -EBADF;
	}

	if (!buffer->heap->ops->phys) {
		printk("%s: is not implemented by this heap.\n",
			__func__);
		return -ENODEV;
	}
	buffer->heap->ops->phys(buffer->heap, buffer,
			(ion_phys_addr_t *)&region->addr,
					&region->len);
	return 0;
}


static int berlin_ion_sync_exec(struct ion_berlin_data *region,
				struct ion_buffer *buffer)
{
	struct vm_area_struct *vma;
	phys_addr_t phys_start, phys_end, tmpaddr, tmplen;
	unsigned long virt_start, virt_end, offset;

	if (buffer == NULL)
		return -EBADF;

	if (!buffer->heap->ops->phys) {
		printk("%s: SYNC is not implemented by this heap.\n",
			__func__);
		return -ENODEV;
	}
	buffer->heap->ops->phys(buffer->heap, buffer,
		(ion_phys_addr_t *)&tmpaddr, &tmplen);

	mutex_lock(&buffer->lock);
	vma = berlin_ion_find_vma(buffer, region->addr, region->len);
	mutex_unlock(&buffer->lock);
	if (!vma) {
		printk("%s: Buffer doesn't map to virtual address\n",
			__func__);
		return -EINVAL;
	}

	offset = region->addr -vma->vm_start;

	virt_start = region->addr;
	virt_end = virt_start + region->len;
	phys_start = tmpaddr + offset;
	phys_end = phys_start + region->len;

	switch (region->cmd) {
	case ION_INVALIDATE_CACHE:
	{
		dmac_map_area((const void *)virt_start,
			region->len, DMA_FROM_DEVICE);
		outer_inv_range(phys_start, phys_end);
		break;
	}

	case ION_CLEAN_CACHE:
	{
		dmac_map_area((const void *)virt_start,
			region->len, DMA_TO_DEVICE);
		outer_clean_range(phys_start, phys_end);
		break;
	}

	case ION_FLUSH_CACHE:
	{
		dmac_flush_range((const void *)virt_start,
				(const void *)virt_end);
		outer_flush_range(phys_start, phys_end);
		break;
	}
	default:
		printk("%s: Unknown cache command %d\n",
				__func__, region->cmd);
		return -EINVAL;
	}

	return 0;
}


static int berlin_ion_sync(struct ion_berlin_data *region)
{
	struct ion_buffer *buffer;
	struct berlin_gid_node *node;

	if (region->len == 0) {
		printk("%s invalid parameter gid[%d] virtaddr[%x] len[%d] "
		"threadname[%s] threadid[%d]\n", __func__, region->gid,
		region->addr, region->len, current->comm, task_pid_vnr(current));
		dump_stack();
		return -EINVAL;
	}

	down_read(&berlin_gid.rwsem);
	node = berlin_gid_lookup(&berlin_gid.root, region->gid);
	up_read(&berlin_gid.rwsem);
	if (!node) {
		printk("%s: can't found gid[%u]\n", __func__, region->gid);
		return -1;
	}

	buffer = (struct ion_buffer *)(node->data);
	if (buffer == NULL) {
		printk("%s: get buffer fail[%p]\n", __func__, buffer);
		return -EBADF;
	}

	return berlin_ion_sync_exec(region, buffer);
}


static int berlin_cc_reg(struct berlin_cc_info *info)
{
	int ret = 0;
	struct berlin_cc_info *tmp;
	struct berlin_gid_node *node;
	unsigned int new_gid = 0;

	berlin_debug(BERLIN_CC_DEBUG_LEVEL,
		"%s start sid[%u] [%08x]\n", __func__,
		info->m_ServiceID, info->m_ServiceID);
	if (info == NULL)
		return -EBADF;

	tmp = kzalloc(sizeof(struct berlin_cc_info), GFP_KERNEL);
	if (!tmp)
		return -ENOMEM;
	node = kzalloc(sizeof(struct berlin_gid_node), GFP_KERNEL);
	if (!node) {
		kfree(tmp);
		return -ENOMEM;
	}

	node->data = (void *)(tmp);
	node->pid = task_tgid_vnr(current);
	get_task_comm(node->task_comm, current->group_leader);
	node->threadid = task_pid_vnr(current);
	strncpy(node->threadname, current->comm, TASK_COMM_LEN);

	down_write(&berlin_cc.rwsem);
	if (info->m_ServiceID == BERLIN_CC_DYNAMIC_ID) {
		ret = berlin_alloc_gid(&berlin_cc.root,
			BERLIN_CC_START_ID, &new_gid);
		if (ret !=0) {
			up_write(&berlin_cc.rwsem);
			printk("%s alloc gid fail\n", __func__);
			kfree(tmp);
			kfree(node);
			return -1;
		}

		info->m_ServiceID = new_gid;
	}

	memcpy(tmp, info, sizeof(struct berlin_cc_info));
	node->gid = info->m_ServiceID;

	berlin_gid_insert(&berlin_cc.root, node);
	up_write(&berlin_cc.rwsem);

	berlin_debug(BERLIN_CC_DEBUG_LEVEL,
		"%s end sid[%u] [%08x]\n", __func__,
		info->m_ServiceID,  info->m_ServiceID);

	return 0;
}


static int berlin_cc_free(struct berlin_cc_info *info)
{
	struct berlin_gid_node *node;

	berlin_debug(BERLIN_CC_DEBUG_LEVEL,
		"%s sid[%u] [%08x]\n", __func__
		, info->m_ServiceID, info->m_ServiceID);

	down_write(&berlin_cc.rwsem);
	node = berlin_gid_lookup(&berlin_cc.root, info->m_ServiceID);
	if (!node) {
		up_write(&berlin_cc.rwsem);
		printk("%s: can't found gid[%u]\n", __func__, info->m_ServiceID);
		return -1;
	}

	berlin_gid_delete(&berlin_cc.root, node);
	up_write(&berlin_cc.rwsem);

	kfree(node->data);
	kfree(node);
	return 0;
}

int berlin_cc_inquiry(struct berlin_cc_info *info)
{
	struct berlin_gid_node *node;

	berlin_debug(BERLIN_CC_DEBUG_LEVEL,
		"%s sid[%u] [%08x]\n", __func__,
		info->m_ServiceID, info->m_ServiceID);

	down_read(&berlin_cc.rwsem);
	node = berlin_gid_lookup(&berlin_cc.root, info->m_ServiceID);
	up_read(&berlin_cc.rwsem);
	if (!node) {
		if (info->m_ServiceID > BERLIN_CC_DYNAMIC_ID)
		printk("%s: can't found sid[%u] [%08x]\n", __func__
			, info->m_ServiceID, info->m_ServiceID);
		return -1;
	}

	memcpy(info, node->data, sizeof(struct berlin_cc_info));

	return 0;
}


static int berlin_cc_update(struct berlin_cc_info *info)
{
	struct berlin_gid_node *node;

	berlin_debug(BERLIN_CC_DEBUG_LEVEL,
		"%s sid[%u] [%08x]\n", __func__
		, info->m_ServiceID, info->m_ServiceID);

	if (info->m_ServiceID == BERLIN_CC_DYNAMIC_ID) {
		printk("%s can't update for sid[%08x]\n",
			__func__, info->m_ServiceID);
	}

	down_write(&berlin_cc.rwsem);
	node = berlin_gid_lookup(&berlin_cc.root, info->m_ServiceID);
	if (!node) {
		up_write(&berlin_cc.rwsem);
		printk("%s: can't found gid[%u] [%08x]\n", __func__
			, info->m_ServiceID, info->m_ServiceID);
		return -1;
	}
	memcpy(node->data, info, sizeof(struct berlin_cc_info));
	up_write(&berlin_cc.rwsem);

	return 0;
}


static int berlin_cc_cmd(struct berlin_cc_data *data)
{
	int ret = -1;

	switch (data->cmd) {
	case ION_CC_REG:
	{
		ret = berlin_cc_reg(&data->cc);
		break;
	}

	case ION_CC_FREE:
	{
		ret = berlin_cc_free(&data->cc);
		break;
	}

	case ION_CC_INQUIRY:
	{
		ret = berlin_cc_inquiry(&data->cc);
		break;
	}

	case ION_CC_UPDATE:
	{
		ret = berlin_cc_update(&data->cc);
		break;
	}

	default:
		printk("%s: Unknown cache command %d\n",
				__func__, data->cmd);
		return -EINVAL;
	}

	return ret;
}


int berlin_cc_release_by_taskid(void)
{
	struct rb_node *node;
	struct berlin_gid_node *tmp;
	pid_t pid = task_tgid_vnr(current);

	berlin_debug(BERLIN_CC_DEBUG_LEVEL,
		"%s pid[%d]\n", __func__, pid);
	down_write(&berlin_cc.rwsem);
	for(node = rb_first(&berlin_cc.root); node;) {
		tmp = rb_entry(node, struct berlin_gid_node, gid_node);
		node = rb_next(node);
		if (tmp->pid == pid) {
			berlin_debug(BERLIN_CC_DEBUG_LEVEL,
				"%s sid[%u] [%08x]\n",
				__func__, tmp->gid, tmp->gid);
			berlin_gid_delete(&berlin_cc.root, tmp);
			kfree(tmp->data);
			kfree(tmp);
		}
	}
	up_write(&berlin_cc.rwsem);

	return 0;
}


static void berlin_udp_recvmsg_handle(struct sk_buff* skb)
{
	//wake_up_interruptible(sk->sk_sleep);
	return;
}


static int berlin_udp_init(void)
{
	struct netlink_kernel_cfg cfg = {
		.groups = 0,
		.flags = NL_CFG_F_NONROOT_RECV | NL_CFG_F_NONROOT_SEND,
		.input = berlin_udp_recvmsg_handle,
		.cb_mutex = NULL,
	};

	berlin_socket = netlink_kernel_create(&init_net,
			NETLINK_BERLIN_CC, &cfg);
	if (berlin_socket == NULL) {
		printk("%s netlink_kernel_create fail\n", __func__);
		return -1;
	}

	return 0;
}


static int berlin_udp_exit(void)
{
	sock_release(berlin_socket->sk_socket);
	return 0;
}


static int berlin_udp_sendmsg(struct sock *port,
			unsigned int dstserviceID,
			unsigned	char *msgbuff,
			unsigned int msglen )
{
	struct sk_buff * skb;
	struct nlmsghdr *nlhdr;

	if ((msgbuff == NULL) || (msglen > BERLIN_CC_ICCFIFO_FRAME_SIZE)) {
		printk("%s  parameter error msgbuff[%p] msglen[%d] > 128\n",
			__func__, msgbuff, msglen);
		return -1;
	}

	if (port == NULL)
		port = berlin_socket;

	skb = alloc_skb(msglen, GFP_ATOMIC);
	if (skb == NULL) {
		printk("%s alloc_skb fail\n", __func__);
		return -2;
	}

	NETLINK_CB(skb).portid = 0;
	NETLINK_CB(skb).dst_group = NETLINK_BERLIN_CC_GROUP;

	nlhdr = nlmsg_put(skb, 0, 0, 0, msglen, 0);
	memcpy(NLMSG_DATA(nlhdr), msgbuff, msglen);

	netlink_unicast(port, skb, dstserviceID, MSG_DONTWAIT);

	berlin_debug(BERLIN_CC_DEBUG_LEVEL,
		"%s (SID = 0x%08X, size = %d )\n",
		__func__, dstserviceID, msglen);
	return 0;
}


static long berlin_ion_ioctl(struct ion_client *client, unsigned int cmd,
						unsigned long arg)
{
	struct ion_berlin_data region ;
	int ret = 0;
	int heap_num = 0;
	struct ion_heap_info *info;
	struct berlin_cc_data data;

	switch (cmd) {
	case ION_BERLIN_PHYS:
	{
		if (copy_from_user(&region, (void __user *)arg,
				sizeof(struct ion_berlin_data)))
			return -EFAULT;

		ret = berlin_ion_phys(&region);
		if (ret)
			return ret;
		if (copy_to_user((void __user *)arg, &region,
				sizeof(struct ion_berlin_data)))
			return -EFAULT;
		break;
	}

	case ION_BERLIN_SYNC:
	{
		if (copy_from_user(&region, (void __user *)arg,
				   sizeof(struct ion_berlin_data)))
			return -EFAULT;
		ret = berlin_ion_sync(&region);
		if (ret)
			return ret;
		break;
	}

	case ION_BERLIN_GETGID:
	{
		if (copy_from_user(&region, (void __user *)arg,
				sizeof(struct ion_berlin_data)))
			return -EFAULT;

		ret = berlin_ion_get_gid(&region);
		if (ret)
			return ret;
		if (copy_to_user((void __user *)arg, &region,
				sizeof(struct ion_berlin_data)))
			return -EFAULT;
		break;
	}

	case ION_BERLIN_GETSFD:
	{
		if (copy_from_user(&region, (void __user *)arg,
				sizeof(struct ion_berlin_data)))
			return -EFAULT;

		ret = berlin_ion_get_sfd(&region);
		if (ret)
			return ret;
		if (copy_to_user((void __user *)arg, &region,
				sizeof(struct ion_berlin_data)))
			return -EFAULT;
		break;
	}

	case ION_BERLIN_GETHM:
	{
		ret = berlin_get_heap_num(&heap_num);
		if (ret)
			return ret;
		if (copy_to_user((void __user *)arg, &heap_num,
				sizeof(int)))
			return -EFAULT;
		break;
	}

	case ION_BERLIN_GETHI:
	{
		info = kzalloc(sizeof(struct ion_heap_info) *
			berlin_info->heap_mun, GFP_KERNEL);
		if (info == NULL)  {
			printk("%s: kzalloc fail\n", __func__);
			return -ENOMEM;
		}

		ret = berlin_get_heap_detail_info(info);
		if (ret) {
			kfree(info);
			return ret;
		}
		if (copy_to_user((void __user *)arg, info,
			sizeof(struct ion_heap_info) * berlin_info->heap_mun)) {
			kfree(info);
			return -EFAULT;
		}
		kfree(info);
		break;
	}

	case ION_BERLIN_CC:
	{
		if (copy_from_user(&data, (void __user *)arg,
				sizeof(struct berlin_cc_data)))
			return -EFAULT;

		ret = berlin_cc_cmd(&data);
		if (ret)
			return ret;
		if (data.cmd == ION_CC_FREE || data.cmd == ION_CC_UPDATE)
			return ret;
		if (copy_to_user((void __user *)arg, &data,
				sizeof(struct ion_berlin_data)))
			return -EFAULT;
		break;
	}

	default:
	       printk("%s: Unknown ioctl %d\n", __func__, cmd);
	       return -EINVAL;
	}
	return 0;
}


int MV_CC_MsgQ_PostMsgByID(unsigned int ServiceID,
					void *msgbuf)
{
	int ret;

	if (msgbuf == NULL) {
		printk("%s parameter error msgbuf[%p]\n",
			__func__, msgbuf);
		return -1;
	}

	ret = berlin_udp_sendmsg(NULL, ServiceID, msgbuf,
				sizeof(MV_CC_MSG_t));
	if (ret != 0) {
		printk("%s berlin_udp_sendmsg error ret[%d]\n",
					__func__, ret);
	}

	return ret;
}
EXPORT_SYMBOL(MV_CC_MsgQ_PostMsgByID);


int ion_get_heap_num(int *heap_num)
{
	return berlin_get_heap_num(heap_num);
}
EXPORT_SYMBOL(ion_get_heap_num);


int ion_get_heap_info(struct ion_heap_info *info)
{
	return berlin_get_heap_detail_info(info);
}
EXPORT_SYMBOL(ion_get_heap_info);


struct ion_device *ion_get_dev(void)
{
	if (!berlin_idev) {
		printk("%s fail\n", __func__);
	}
	return berlin_idev;
}
EXPORT_SYMBOL(ion_get_dev);


int ion_cleancache(struct ion_handle *handle, void *virtaddr, size_t size)
{
	int ret = 0;
	struct ion_berlin_data region;
	region.cmd = ION_CLEAN_CACHE;
	region.addr = (size_t)virtaddr;
	region.len = size;

	if (handle == NULL)
		return -EBADF;

	ret = berlin_ion_sync_exec(&region, handle->buffer);
	if (ret) {
		printk("kernel %s fail\n", __func__);
	}
	return ret;
}
EXPORT_SYMBOL(ion_cleancache);


int ion_flushcache(struct ion_handle *handle, void *virtaddr, size_t size)
{
	int ret = 0;
	struct ion_berlin_data region;
	region.cmd = ION_FLUSH_CACHE;
	region.addr = (size_t)virtaddr;
	region.len = size;

	if (handle == NULL)
		return -EBADF;

	ret = berlin_ion_sync_exec(&region, handle->buffer);
	if (ret) {
		printk("kernel %s fail\n", __func__);
	}
	return ret;
}
EXPORT_SYMBOL(ion_flushcache);


int ion_invalidatecache(struct ion_handle *handle, void *virtaddr, size_t size)
{
	int ret = 0;
	struct ion_berlin_data region;
	region.cmd = ION_INVALIDATE_CACHE;
	region.addr = (size_t)virtaddr;
	region.len = size;

	if (handle == NULL)
		return -EBADF;

	ret = berlin_ion_sync_exec(&region, handle->buffer);
	if (ret) {
		printk("kernel %s fail\n", __func__);
	}
	return ret;
}
EXPORT_SYMBOL(ion_invalidatecache);


int ion_getgid(struct ion_handle *handle, unsigned int *gid)
{
	struct ion_buffer *buffer;

	buffer = handle->buffer;
	if (buffer == NULL)
		return -EBADF;

	*gid = buffer->gid;
	berlin_debug(9, "kernel %s pid[%d] get gid[%u]\n", __func__,
			task_tgid_vnr(current), buffer->gid);
	return 0;
}
EXPORT_SYMBOL(ion_getgid);


int ion_getsfd(unsigned int gid, int *share_fd)
{
	int ret = 0;
	struct ion_berlin_data region;
	region.gid = gid;
	ret = berlin_ion_get_sfd(&region);
	if (ret) {
		printk("kernel %s fail\n", __func__);
		return ret;
	}
	*share_fd = region.share_fd;
	return 0;
}
EXPORT_SYMBOL(ion_getsfd);


struct ion_handle *ion_gethandle(struct ion_client *client, unsigned int gid)
{
	struct ion_buffer *buffer;
	struct berlin_gid_node *node;
	struct ion_handle *handle;
	int ret;

	down_read(&berlin_gid.rwsem);
	node = berlin_gid_lookup(&berlin_gid.root, gid);
	up_read(&berlin_gid.rwsem);
	if (!node) {
		printk("%s: can't found gid[%u]\n", __func__, gid);
		return NULL;
	}

	buffer = (struct ion_buffer *)(node->data);
	if (buffer == NULL) {
		printk("%s: get buffer fail[%p]\n", __func__, buffer);
		return NULL;
	}

	ion_buffer_get(buffer);
	mutex_lock(&client->lock);
	/* if a handle exists for this buffer just take a reference to it */
	handle = ion_handle_lookup(client, buffer);
	if (!IS_ERR(handle)) {
		ion_handle_get(handle);
		goto end;
	}
	handle = ion_handle_create(client, buffer);
	if (IS_ERR(handle)) {
		handle = NULL;
		goto end;
	}
	ret = ion_handle_add(client, handle);
	if (ret) {
		ion_handle_put(handle);
		handle = NULL;
	}
end:
	mutex_unlock(&client->lock);
	ion_buffer_put(buffer);
	return handle;
}
EXPORT_SYMBOL(ion_gethandle);


static int ion_debug_cc_show(struct seq_file *s, void *unused)
{
	struct rb_node *node;
	struct berlin_gid_node *tmp;
	struct berlin_gid_root *debug_gid = s->private;
	int i = 0;

	seq_printf(s, "|No  |  sid   |Ref |  pid   |  process_name  |   tid  "
		"|   thread_name  |\n");
	seq_printf(s, "----------------------------------------------"
		"--------------------------\n");

	down_write(&debug_gid->rwsem);
	for(node = rb_first(&debug_gid->root); node;) {
		tmp = rb_entry(node, struct berlin_gid_node, gid_node);
		node = rb_next(node);
		seq_printf(s, "|%4d|%08x|%4d|%8d|%16s|%8d|%16s|\n", i,
				tmp->gid, tmp->ref, tmp->pid, tmp->task_comm,
				tmp->threadid, tmp->threadname);
		i++;
	}
	up_write(&debug_gid->rwsem);
	seq_printf(s, "----------------------------------------------"
		"--------------------------\n");

	return 0;
}


static int ion_debug_cc_open(struct inode *inode, struct file *file)
{
	return single_open(file, ion_debug_cc_show, inode->i_private);
}


static const struct file_operations debug_cc_fops = {
	.open = ion_debug_cc_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};


static int berlin_cc_init(void)
{
	init_rwsem(&berlin_cc.rwsem);
	berlin_cc.root = RB_ROOT;
	berlin_cc.debug_root = debugfs_create_file("cc", 0664,
				berlin_idev->debug_root, (void *)&berlin_cc,
				&debug_cc_fops);
	return 0;
}


static int berlin_cc_exit(void)
{
	debugfs_remove_recursive(berlin_cc.debug_root);
	return 0;
}


static int ion_debug_gid_show(struct seq_file *s, void *unused)
{
	struct rb_node *node;
	struct berlin_gid_node *tmp;
	struct berlin_gid_root *debug_gid = s->private;
	int i = 0;

	seq_printf(s, "|No  |Gid  |Ref |  pid   |  process_name  |   tid  "
		"|   thread_name  |physaddr|  size  |alloclen|heap_id|\n");
	seq_printf(s, "--------------------------------------------------"
		"------------------------------------------------------\n");

	down_write(&debug_gid->rwsem);
	for(node = rb_first(&debug_gid->root); node;) {
		tmp = rb_entry(node, struct berlin_gid_node, gid_node);
		node = rb_next(node);
		seq_printf(s, "|%4d|%5u|%4d|%8d|%16s|%8d|%16s|%8lx|%8x|%8x|%7d|\n",
				i,tmp->gid, tmp->ref, tmp->pid, tmp->task_comm,
				tmp->threadid, tmp->threadname,
				((struct ion_buffer *)(tmp->data))->priv_phys,
				((struct ion_buffer *)(tmp->data))->size,
				((struct ion_buffer *)(tmp->data))->alloc_size,
				((struct ion_buffer *)(tmp->data))->heap->id);
		i++;
	}
	up_write(&debug_gid->rwsem);
	seq_printf(s, "--------------------------------------------------"
		"------------------------------------------------------\n");

	return 0;
}


static int ion_debug_gid_open(struct inode *inode, struct file *file)
{
	return single_open(file, ion_debug_gid_show, inode->i_private);
}


static const struct file_operations debug_gid_fops = {
	.open = ion_debug_gid_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};


static int berlin_gid_init(void)
{
	init_rwsem(&berlin_gid.rwsem);
	berlin_gid.root = RB_ROOT;
	berlin_gid.debug_root = debugfs_create_file("gid", 0664,
				berlin_idev->debug_root, (void *)&berlin_gid,
				&debug_gid_fops);
	return 0;
}


static int berlin_gid_exit(void)
{
	debugfs_remove_recursive(berlin_gid.debug_root);
	return 0;
}


static int ion_debug_meminfo_show(struct seq_file *s, void *unused)
{
	struct berlin_ion_info *info = s->private;
	int i = 0;
	struct rb_node *n;
	struct ion_buffer *buffer;
	size_t free_size;

	for (i = 0; i < info->heap_mun; i++) {
		(info->heaps_data + i)->used_size = 0;
	}

	mutex_lock(&berlin_idev->buffer_lock);
	for (n = rb_first(&berlin_idev->buffers); n; n = rb_next(n)) {
		buffer = rb_entry(n, struct ion_buffer, node);
		for (i = 0; i < info->heap_mun; i++) {
			if (buffer->heap->id == i)
				(info->heaps_data + i)->used_size +=
					buffer->size;
		}
	}
	mutex_unlock(&berlin_idev->buffer_lock);

	seq_printf(s, "Marvell Berlin ION information\n");

	for (i = 0; i < info->heap_mun; i++) {
		free_size = (info->heaps_data + i)->size -
			(info->heaps_data + i)->used_size;
		seq_printf(s, "----------------------"
			"--------------\n");
		seq_printf(s, "heap_id [%2d]  heap_name [%10s]\n"
			"attri [0x%08x] type [%10d]\n"
			"start [0x%08lx] size [%10d]\n"
			"used size [0x%08x]  [%10d]\n"
			"free size [0x%08x]  [%10d]\n\n",
			(info->heaps_data + i)->id,
			(info->heaps_data + i)->name,
			(info->heaps_data + i)->attribute,
			(info->heaps_data + i)->type,
			(info->heaps_data + i)->base,
			(info->heaps_data + i)->size,
			(info->heaps_data + i)->used_size,
			(info->heaps_data + i)->used_size,
			free_size, free_size);
	}
	return 0;
}


static int ion_debug_meminfo_open(struct inode *inode, struct file *file)
{
	return single_open(file, ion_debug_meminfo_show, inode->i_private);
}


static const struct file_operations debug_meminfo_fops = {
	.open = ion_debug_meminfo_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};


static int berlin_meminfo_init(void)
{
	berlin_info->debug_root = debugfs_create_file("meminfo", 0664,
			berlin_idev->debug_root, (void *)berlin_info,
				&debug_meminfo_fops);
	return 0;
}


static int berlin_meminfo_exit(void)
{
	debugfs_remove_recursive(berlin_info->debug_root);
	return 0;
}


static int berlin_ion_probe(struct platform_device *pdev)
{
	int res =0;
	int i = 0;
	struct berlin_ion_info * info;

	res = berlin_ion_get_info(&pdev->dev, &info);
	if (res != 0) {
		return res;
	}

	berlin_ion_print_info(info);

	berlin_info = info;
	berlin_get_secure_heap_info();

	berlin_idev = ion_device_create(berlin_ion_ioctl);
	if (IS_ERR_OR_NULL(berlin_idev)) {
		return PTR_ERR(berlin_idev);
	}

	for (i = 0; i < info->heap_mun; i++) {
		struct ion_platform_heap *heap_data = (info->heaps_data+i);

		info->heaps[i] = ion_heap_create(heap_data);
		if (IS_ERR_OR_NULL(info->heaps[i])) {
			res = PTR_ERR(info->heaps[i]);
			goto err_create_heap;
		}
		ion_device_add_heap(berlin_idev, info->heaps[i]);
	}

	platform_set_drvdata(pdev, info);

	berlin_gid_init();
	berlin_meminfo_init();
	berlin_cc_init();
	berlin_udp_init();
	return 0;

err_create_heap:
	for (i = 0; i < info->heap_mun; i++) {
		if (info->heaps[i])
			ion_heap_destroy(info->heaps[i]);
	}
	ion_device_destroy(berlin_idev);
	return res;

}

static int berlin_ion_remove(struct platform_device *pdev)
{
	struct berlin_ion_info *info = platform_get_drvdata(pdev);
	int i;

	if (info) {
		for (i = 0; i < info->heap_mun; i++) {
			if (info->heaps[i])
			ion_heap_destroy(info->heaps[i]);
		}
		ion_device_destroy(berlin_idev);
	}
	berlin_gid_exit();
	berlin_meminfo_exit();
	berlin_cc_exit();
	berlin_udp_exit();
	return 0;
}


static const struct of_device_id berlin_ion_of_match[] = {
	{.compatible = "marvell,berlin-ion",},
	{},
};

static struct platform_driver berlin_ion_driver = {
	.probe = berlin_ion_probe,
	.remove = berlin_ion_remove,
	.driver = {
		.name	= "berlin-ion",
		.of_match_table = berlin_ion_of_match,
	},
};

module_platform_driver(berlin_ion_driver);

MODULE_LICENSE("GPL v2");

