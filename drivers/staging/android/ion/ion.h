/*
 * drivers/staging/android/ion/ion.h
 *
 * Copyright (C) 2011 Google, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef _LINUX_ION_H
#define _LINUX_ION_H

#include <linux/types.h>

#include "../uapi/ion.h"

struct ion_handle;
struct ion_device;
struct ion_heap;
struct ion_mapper;
struct ion_client;
struct ion_buffer;

/* This should be removed some day when phys_addr_t's are fully
   plumbed in the kernel, and all instances of ion_phys_addr_t should
   be converted to phys_addr_t.  For the time being many kernel interfaces
   do not accept phys_addr_t's that would have to */
#define ion_phys_addr_t unsigned long

/**
 * struct ion_platform_heap - defines a heap in the given platform
 * @type:	type of the heap from ion_heap_type enum
 * @id:		unique identifier for heap.  When allocating higher numbers
 *		will be allocated from first.  At allocation these are passed
 *		as a bit mask and therefore can not exceed ION_NUM_HEAP_IDS.
 * @name:	used for debug purposes
 * @base:	base address of heap in physical memory if applicable
 * @size:	size of the heap in bytes if applicable
 * @align:	required alignment in physical memory if applicable
 * @priv:	private info passed from the board file
 *
 * Provided by the board file.
 */
struct ion_platform_heap {
	enum ion_heap_type type;
	unsigned int id;
	const char *name;
	ion_phys_addr_t base;
	size_t size;
	ion_phys_addr_t align;
	void *priv;
	unsigned int attribute;
	size_t used_size;
};

/**
 * struct ion_platform_data - array of platform heaps passed from board file
 * @nr:		number of structures in the array
 * @heaps:	array of platform_heap structions
 *
 * Provided by the board file in the form of platform data to a platform device.
 */
struct ion_platform_data {
	int nr;
	struct ion_platform_heap *heaps;
};

/**
 * ion_reserve() - reserve memory for ion heaps if applicable
 * @data:	platform data specifying starting physical address and
 *		size
 *
 * Calls memblock reserve to set aside memory for heaps that are
 * located at specific memory addresses or of specfic sizes not
 * managed by the kernel
 */
void ion_reserve(struct ion_platform_data *data);

/**
 * ion_client_create() -  allocate a client and returns it
 * @dev:		the global ion device
 * @heap_type_mask:	mask of heaps this client can allocate from
 * @name:		used for debugging
 */
struct ion_client *ion_client_create(struct ion_device *dev,
				     const char *name);

/**
 * ion_client_destroy() -  free's a client and all it's handles
 * @client:	the client
 *
 * Free the provided client and all it's resources including
 * any handles it is holding.
 */
void ion_client_destroy(struct ion_client *client);

/**
 * ion_alloc - allocate ion memory
 * @client:		the client
 * @len:		size of the allocation
 * @align:		requested allocation alignment, lots of hardware blocks
 *			have alignment requirements of some kind
 * @heap_id_mask:	mask of heaps to allocate from, if multiple bits are set
 *			heaps will be tried in order from highest to lowest
 *			id
 * @flags:		heap flags, the low 16 bits are consumed by ion, the
 *			high 16 bits are passed on to the respective heap and
 *			can be heap custom
 *
 * Allocate memory in one of the heaps provided in heap mask and return
 * an opaque handle to it.
 */
struct ion_handle *ion_alloc(struct ion_client *client, size_t len,
			     size_t align, unsigned int heap_id_mask,
			     unsigned int flags);

/**
 * ion_free - free a handle
 * @client:	the client
 * @handle:	the handle to free
 *
 * Free the provided handle.
 */
void ion_free(struct ion_client *client, struct ion_handle *handle);

/**
 * ion_phys - returns the physical address and len of a handle
 * @client:	the client
 * @handle:	the handle
 * @addr:	a pointer to put the address in
 * @len:	a pointer to put the length in
 *
 * This function queries the heap for a particular handle to get the
 * handle's physical address.  It't output is only correct if
 * a heap returns physically contiguous memory -- in other cases
 * this api should not be implemented -- ion_sg_table should be used
 * instead.  Returns -EINVAL if the handle is invalid.  This has
 * no implications on the reference counting of the handle --
 * the returned value may not be valid if the caller is not
 * holding a reference.
 */
int ion_phys(struct ion_client *client, struct ion_handle *handle,
	     ion_phys_addr_t *addr, size_t *len);

/**
 * ion_map_dma - return an sg_table describing a handle
 * @client:	the client
 * @handle:	the handle
 *
 * This function returns the sg_table describing
 * a particular ion handle.
 */
struct sg_table *ion_sg_table(struct ion_client *client,
			      struct ion_handle *handle);

/**
 * ion_map_kernel - create mapping for the given handle
 * @client:	the client
 * @handle:	handle to map
 *
 * Map the given handle into the kernel and return a kernel address that
 * can be used to access this address.
 */
void *ion_map_kernel(struct ion_client *client, struct ion_handle *handle);

/**
 * ion_unmap_kernel() - destroy a kernel mapping for a handle
 * @client:	the client
 * @handle:	handle to unmap
 */
void ion_unmap_kernel(struct ion_client *client, struct ion_handle *handle);

/**
 * ion_share_dma_buf() - share buffer as dma-buf
 * @client:	the client
 * @handle:	the handle
 */
struct dma_buf *ion_share_dma_buf(struct ion_client *client,
						struct ion_handle *handle);

/**
 * ion_share_dma_buf_fd() - given an ion client, create a dma-buf fd
 * @client:	the client
 * @handle:	the handle
 */
int ion_share_dma_buf_fd(struct ion_client *client, struct ion_handle *handle);

/**
 * ion_import_dma_buf() - given an dma-buf fd from the ion exporter get handle
 * @client:	the client
 * @fd:		the dma-buf fd
 *
 * Given an dma-buf fd that was allocated through ion via ion_share_dma_buf,
 * import that fd and return a handle representing it.  If a dma-buf from
 * another exporter is passed in this function will return ERR_PTR(-EINVAL)
 */
struct ion_handle *ion_import_dma_buf(struct ion_client *client, int fd);

/**  ion get ion device handle
 *
 * This function will get ion device handle for use
 * ion_client_create function.
 *
 * @retval  NULL            get ion device handle fail
 * @retval  !NULL           get ion device handle success
 *
 */
struct ion_device *ion_get_dev(void);


/**  ion get physical information of a ion buffer
 *
 * This function will get a ion buffer's physical address and size
 *
 * @param share_fd      a dma-buf share file descriptor.
 *
 * @param addr          return the ion buffer's physical address
 *
 * @param len           return the ion buffer's memory size.
 *
 * @retval -1           get physical information fail
 * @retval  0           get physical information success
 *
 */
int ion_getphys(int share_fd, size_t *addr, size_t *len);


/**  ion clean cache operate
 *
 * This function will clean cache
 *
 * @param handle        a memory buffer's handle
 *
 * @param virtaddr      need clean cache memory's virtual address
 *
 * @param size          need clean cache memory's size.
 *
 * @retval -1           clean cache operate fail
 * @retval  0           clean cache operate success
 *
 */
int ion_cleancache(struct ion_handle *handle, void *virtaddr, size_t size);


/**  ion clean and invalidatecache operate
 *
 * This function will clean cache
 *
 * @param handle        a memory buffer's handle
 *
 * @param virtaddr      need clean and invalidatecache cache memory's virtual address
 *
 * @param size          need clean and invalidatecache cache memory's size.
 *
 * @retval -1           clean and invalidatecache cache operate fail
 * @retval  0           clean and invalidatecache cache operate success
 *
 */
int ion_flushcache(struct ion_handle *handle, void *virtaddr, size_t size);


/**  ion invalidate cache operate
 *
 * This function will invalidate cache
 *
 * @param handle        a memory buffer's handle
 *
 * @param virtaddr      need invalidate cache memory's virtual address
 *
 * @param size          need invalidate cache memory's size.
 *
 * @retval -1           invalidate cache operate fail
 * @retval  0           invalidate cache operate success
 *
 */
int ion_invalidatecache(struct ion_handle *handle, void *virtaddr, size_t size);


/**  ion get gid
 *
 * This function will get a global id in the linux kernel,this id will be transport
 * to another process and use it to get a current process's share fd for mmap or do
 * all other things.
 *
 * @param handle        a memory buffer's handle.
 *
 * @param *gid          save the return a global id.
 *
 * @retval -1           get a global id fail
 * @retval  0           get a global id success
 *
 */
int ion_getgid(struct ion_handle *handle, unsigned int *gid);


/**  ion getsfd
 *
 * This function will get a current process's share fd by the global id
 *
 * @param share_fd      save the return a dma-buf share file descriptor.
 *
 * @param gid           a global id.
 *
 * @retval -1           get a current process's share fd fail
 * @retval  0           get a current process's share fd success
 *
 */
int ion_getsfd(unsigned int gid, int *share_fd);


/**  ion gethandle
 *
 * This function will get a buffer's handle by the global id
 *
 * @param client       a client by ion_client_create function got.
 *
 * @param gid          a global id.
 *
 * @retval NULL        get a buffer's handle fail
 * @retval !NULL       get a buffer's handle success
 *
 */

struct ion_handle *ion_gethandle(struct ion_client *client, unsigned int gid);


/**  ion get heap number
 *
 * This function will get heap number
 *
 * @param *heap_num    save heap number
 *
 * @retval 0           get fail
 * @retval !0          get success
 *
 */
int ion_get_heap_num(int *heap_num);


/**  ion get heaps information
 *
 * This function will get all heaps information
 *
 * @param *info     got heaps information
 *
 * @retval 0        get info fail
 * @retval !0       get info success
 *
 */
int ion_get_heap_info(struct ion_heap_info *info);


/**  mv cc msgq post msg
 *
 * This function will post msg by netlink
 *
 * @param ServiceID    target address
 *
 * @param msgbuf	 msg buffer
 *
 * @retval !0	post msg fail
 * @retval 0 	post msg success
 *
 */
int MV_CC_MsgQ_PostMsgByID(unsigned int ServiceID, void *msgbuf);


#endif /* _LINUX_ION_H */
