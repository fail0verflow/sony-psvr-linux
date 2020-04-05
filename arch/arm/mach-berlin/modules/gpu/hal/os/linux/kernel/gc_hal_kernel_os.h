/****************************************************************************
*
*    Copyright (C) 2005 - 2014 by Vivante Corp.
*
*    This program is free software; you can redistribute it and/or modify
*    it under the terms of the GNU General Public License as published by
*    the Free Software Foundation; either version 2 of the license, or
*    (at your option) any later version.
*
*    This program is distributed in the hope that it will be useful,
*    but WITHOUT ANY WARRANTY; without even the implied warranty of
*    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
*    GNU General Public License for more details.
*
*    You should have received a copy of the GNU General Public License
*    along with this program; if not write to the Free Software
*    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*
*****************************************************************************/


#ifndef __gc_hal_kernel_os_h_
#define __gc_hal_kernel_os_h_

/*####modified for marvell-bg2*/
#if MRVL_VIDEO_MEMORY_USE_ION
#if LINUX_VERSION_CODE >= KERNEL_VERSION (3,10,0)
#include <../drivers/staging/android/ion/ion.h>
#else
#include <linux/ion.h>
#endif

#ifndef ION_A_NS
#define ION_AF_GFX_NONSECURE (A_NS | A_CG)
#define ION_AF_GFX_SECURE    (A_FS | A_CG)
#else
#define ION_AF_GFX_NONSECURE (ION_A_NS | ION_A_CG | ION_A_FC)
#define ION_AF_GFX_SECURE    (ION_A_FS | ION_A_CG | ION_A_FC)
#endif

#define ION_SIZE_THRESHOLD   (1 << 19) // 512KB
#endif
/*####end for marvell-bg2*/

typedef struct _LINUX_MDL_MAP
{
    gctINT                  pid;
    gctPOINTER              vmaAddr;
    gctUINT32               count;
    struct vm_area_struct * vma;
    struct _LINUX_MDL_MAP * next;
}
LINUX_MDL_MAP;

typedef struct _LINUX_MDL_MAP * PLINUX_MDL_MAP;

typedef struct _LINUX_MDL
{
    char *                  addr;

    union _pages
    {
        /* Pointer to a array of pages. */
        struct page *       contiguousPages;
        /* Pointer to a array of pointers to page. */
        struct page **      nonContiguousPages;
    }
    u;

#ifdef NO_DMA_COHERENT
    gctPOINTER              kaddr;
#endif /* NO_DMA_COHERENT */

    gctINT                  numPages;
    gctINT                  pagedMem;
    gctBOOL                 contiguous;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 27)
    gctBOOL                 exact;
#endif
    dma_addr_t              dmaHandle;
    PLINUX_MDL_MAP          maps;
    struct _LINUX_MDL *     prev;
    struct _LINUX_MDL *     next;

    /* Pointer to allocator which allocates memory for this mdl. */
    void *                  allocator;

    /* Private data used by allocator. */
    void *                  priv;

/*####modified for marvell-bg2*/
#if MRVL_VIDEO_MEMORY_USE_ION
    struct ion_handle       *ionHandle;
    gctUINT32               gid;
#endif
/*####end for marvell-bg2*/
}
LINUX_MDL, *PLINUX_MDL;

extern PLINUX_MDL_MAP
FindMdlMap(
    IN PLINUX_MDL Mdl,
    IN gctINT PID
    );

typedef struct _DRIVER_ARGS
{
    gctUINT64               InputBuffer;
    gctUINT64               InputBufferSize;
    gctUINT64               OutputBuffer;
    gctUINT64               OutputBufferSize;
}
DRIVER_ARGS;

#endif /* __gc_hal_kernel_os_h_ */
