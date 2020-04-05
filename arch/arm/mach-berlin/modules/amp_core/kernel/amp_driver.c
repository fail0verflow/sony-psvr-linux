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

/*******************************************************************************
  System head files
  */
#include <linux/init.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/sched.h>
#include <net/sock.h>
#include <linux/proc_fs.h>
#include <linux/io.h>

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/of.h>
#include <linux/of_irq.h>

#include <linux/mm.h>
#include <linux/kdev_t.h>
#include <asm/page.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/version.h>
#include <linux/interrupt.h>
#include <linux/spinlock.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION (3,10,0)
#include <../drivers/staging/android/ion/ion.h>
#include <../drivers/staging/android/ion/ion_priv.h>
#else
#include <linux/ion.h>
#endif
#include <asm/uaccess.h>

/*******************************************************************************
  Local head files
  */

#include "galois_io.h"
#include "cinclude.h"

#include "api_avio_dhub.h"
#include "amp_driver.h"
#include "drv_debug.h"
#include "amp_memmap.h"
#include "tee_ca_dhub.h"
#include "hal_dhub_wrap.h"
#if defined(CONFIG_MV_AMP_COMPONENT_CPM_ENABLE)
#include "cpm_driver.h"
#endif
#include "amp_ioctl.h"
#include "drv_msg.h"

/* Sub Driver Modules */
#if defined(CONFIG_MV_AMP_COMPONENT_AVIN_ENABLE) || defined(CONFIG_MV_AMP_COMPONENT_AVIN_ENABLE)
#include "api_avif_dhub.h"
#include "avif_dhub_config.h"
#endif
#include "drv_avif.h"
#include "drv_aout.h"
#include "drv_vpu.h"
#include "drv_vpp.h"

#ifdef MORPHEUS_TV_VIEW_MODE
#include "drv_morpheus.h"
#endif

/*******************************************************************************
  Static Variables
  */
static AVIF_CTX *hAvifCtx = NULL;
static AOUT_CTX *hAoutCtx = NULL;
static VPP_CTX *hVppCtx = NULL;
static VPU_CTX *hVpuCtx = NULL;
#ifdef CONFIG_MV_AMP_COMPONENT_ZSP_ENABLE
#include "drv_zsp.h"
static ZSP_CTX *hZspCtx = NULL;
#endif
#ifdef CONFIG_MV_AMP_COMPONENT_HWAPP_ENABLE
#include "drv_app.h"
static APP_CTX *hAppCtx = NULL;
#endif

#ifdef BERLIN_BOOTLOGO
#define MEMMAP_AVIO_BCM_REG_BASE			0xF7B50000
#define RA_AVIO_BCM_AUTOPUSH				0x0198
#endif

#define SPDIFRX_AUDIO_SOURCE				(1)
#define CONFIG_MV88DE3010_REGAREA_BASE          0xf6000000
#define CONFIG_MV88DE3010_REGAREA_SIZE          0x03000000

/*******************************************************************************
  Static Functions
  */
static INT amp_driver_open(struct inode *inode, struct file *filp);
static INT amp_driver_release(struct inode *inode, struct file *filp);
static LONG amp_driver_ioctl_unlocked(struct file *filp, UINT cmd, ULONG arg);
static INT amp_driver_mmap(struct file *file, struct vm_area_struct *vma);
static INT amp_device_init(struct amp_device_t *amp_dev, UINT user);
static INT amp_device_exit(struct amp_device_t *amp_dev, UINT user);

static struct file_operations amp_ops = {
    .open = amp_driver_open,
    .release = amp_driver_release,
    .unlocked_ioctl = amp_driver_ioctl_unlocked,
    .compat_ioctl = amp_driver_ioctl_unlocked,
    .mmap = amp_driver_mmap,
    .owner = THIS_MODULE,
};

struct amp_device_t legacy_pe_dev = {
    .dev_name = AMP_DEVICE_NAME,
    .minor = AMP_MINOR,
    .dev_init = amp_device_init,
    .dev_exit = amp_device_exit,
    .fops = &amp_ops,
};


/*******************************************************************************
  IO remap function
*/
/*AMP memory map table
 */
static struct amp_reg_map_desc amp_reg_map = {
    .vir_addr = 0,
    .phy_addr = CONFIG_MV88DE3010_REGAREA_BASE,
    .length = CONFIG_MV88DE3010_REGAREA_SIZE,
};

static INT amp_create_devioremap(VOID)
{
    amp_reg_map.vir_addr = ioremap(amp_reg_map.phy_addr, amp_reg_map.length);
    if (!amp_reg_map.vir_addr) {
        amp_error("Fail to map address before it is used!\n");
        return -1;
    }
    amp_error("ioremap success: vir_addr: %p, size: 0x%x, phy_addr: %p!\n",
        amp_reg_map.vir_addr, amp_reg_map.length, (void*)amp_reg_map.phy_addr);
    return S_OK;
}

static INT amp_destroy_deviounmap(VOID)
{
    if (amp_reg_map.vir_addr) {
        iounmap(amp_reg_map.vir_addr);
        amp_reg_map.vir_addr = 0;
    }
    return S_OK;
}

VOID *amp_memmap_phy_to_vir(UINT32 phyaddr)
{
    if (amp_reg_map.vir_addr &&
        (phyaddr >= CONFIG_MV88DE3010_REGAREA_BASE) &&
        (phyaddr < CONFIG_MV88DE3010_REGAREA_BASE + CONFIG_MV88DE3010_REGAREA_SIZE))
    {
        return (phyaddr - CONFIG_MV88DE3010_REGAREA_BASE + amp_reg_map.vir_addr);
    }
    amp_error("Fail to map phyaddr: 0x%x, Base vir: %p!\n", phyaddr, amp_reg_map.vir_addr);

    return 0;
}

UINT32 amp_memmap_vir_to_phy(void *vir_addr)
{
    if (amp_reg_map.vir_addr &&
        (vir_addr >= amp_reg_map.vir_addr) &&
        (vir_addr < amp_reg_map.vir_addr + CONFIG_MV88DE3010_REGAREA_SIZE))
    {
        return (vir_addr - amp_reg_map.vir_addr + CONFIG_MV88DE3010_REGAREA_BASE);
    }

    return 0;
}

/*******************************************************************************
  Module Variable
  */
INT amp_irqs[IRQ_AMP_MAX];

static INT amp_major;
static struct cdev amp_dev;
static INT debug_ctl;

static INT isAmpReleased = 0;

#define HEAP_ID_NW				2
#define HEAP_ID_NW_NONCACHE		3
static unsigned long nw_start = 0;
static unsigned long nw_noncache_start = 0;
static long nw_size = 0;
static long nw_noncache_size = 0;

static INT amp_device_init(struct amp_device_t *amp_dev, UINT user)
{
    hVppCtx = drv_vpp_init();
	if (unlikely(hVppCtx == NULL)) {
		amp_trace("drv_vpp_init: falied, unknown error\n");
		return -1;
	}

#if defined(CONFIG_MV_AMP_COMPONENT_AVIN_ENABLE) || defined(CONFIG_MV_AMP_COMPONENT_AIP_ENABLE)
    hAvifCtx = drv_avif_init();
	if (unlikely(hAvifCtx == NULL)) {
		amp_error("drv_avif_init: falied, unknown error\n");
		return -1;
	}
#endif

#ifdef CONFIG_MV_AMP_COMPONENT_HWAPP_ENABLE
    hAppCtx = drv_app_init();
	if (unlikely(hAppCtx == NULL)) {
		amp_trace("drv_app_init init: failed\n");
		return -1;
	}
#endif

#if defined(CONFIG_MV_AMP_COMPONENT_VMETA_ENABLE) || defined (CONFIG_MV_AMP_COMPONENT_V2G_ENABLE)
    hVpuCtx = drv_vpu_init();
    if (unlikely(hVpuCtx == NULL)) {
        amp_error("drv_vpu_init: falied, unknown error\n");
        return -1;
    }
#endif

    hAoutCtx = drv_aout_init();
    if (unlikely(hAoutCtx == NULL)) {
	    amp_error("drv_aout_init: falied, unknown error\n");
	    return -1;
    }

#ifdef CONFIG_MV_AMP_COMPONENT_ZSP_ENABLE
    hZspCtx = drv_zsp_init();
	if (unlikely(hZspCtx == NULL)) {
		amp_error("drv_zsp_init init: failed, unknown err\n");
		return -1;
	}
#endif
    if(hAvifCtx){
        hAoutCtx->p_arc_fifo = &hAvifCtx->arc_fifo;
    }

	// get heap info
	nw_start          = berlin_get_heap_addr(HEAP_ID_NW);
	nw_noncache_start = berlin_get_heap_addr(HEAP_ID_NW_NONCACHE);
	nw_size           = berlin_get_heap_size(HEAP_ID_NW);
	nw_noncache_size  = berlin_get_heap_size(HEAP_ID_NW_NONCACHE);
	nw_size += nw_noncache_size;

	amp_trace("amp_device_init ok");

	return S_OK;
}

static INT amp_device_exit(struct amp_device_t *amp_dev, UINT user)
{
#if defined(CONFIG_MV_AMP_COMPONENT_AIP_ENABLE) || defined(CONFIG_MV_AMP_COMPONENT_AVIN_ENABLE)
    drv_avif_exit(hAvifCtx);
#endif

	drv_vpp_exit(hVppCtx);

#ifdef CONFIG_MV_AMP_COMPONENT_HWAPP_ENABLE
    drv_app_exit(hAppCtx);
#endif

#ifdef CONFIG_MV_AMP_COMPONENT_VMETA_ENABLE
	drv_vpu_exit(hVpuCtx);
#endif
    drv_aout_exit(hAoutCtx);

#ifdef CONFIG_MV_AMP_COMPONENT_ZSP_ENABLE
    drv_zsp_exit(hZspCtx);
#endif
	amp_trace("amp_device_exit ok");

	return S_OK;
}

/****************************************************
 *  address check
 ****************************************************/
int addr_check_normal_world(unsigned long addr, long size)
{
	if (size == 0 || size >= nw_size  ||
		(addr - nw_start) > nw_size  ||
		(addr + size - nw_start) > nw_size ) {
		return -1;
	}
	return 0;
}
int addr_check_normal_world_noncache(unsigned long addr, long size)
{
	if (size == 0 || size >= nw_noncache_size  ||
		(addr - nw_noncache_start) > nw_noncache_size  ||
		(addr + size - nw_noncache_start) > nw_noncache_size ) {
		return -1;
	}
	return 0;
}


/*******************************************************************************
  Module API
  */

static INT amp_driver_open(struct inode *inode, struct file *filp)
{
	UINT vec_num;
	INT err = 0;

#ifdef CONFIG_MV88DE3100_FASTLOGO
    fastlogo_stop();
#endif

#if (BERLIN_CHIP_VERSION < BERLIN_BG4_CD)
	GA_REG_WORD32_WRITE(MEMMAP_AVIO_BCM_REG_BASE + RA_AVIO_BCM_AUTOPUSH, 0x0);
#elif (BERLIN_CHIP_VERSION == BERLIN_BG4_CD || BERLIN_CHIP_VERSION == BERLIN_BG4_CT)
    GA_REG_WORD32_WRITE(MEMMAP_AVIO_REG_BASE + AVIO_MEMMAP_AVIO_BCM_REG_BASE + RA_AVIO_BCM_AUTOPUSH, 0x0);
#endif

    /* initialize dhub */
#ifdef VPP_DHUB_IN_TZ
    DhubInitialize();
#endif
    wrap_DhubInitialization(CPUINDEX, VPP_DHUB_BASE, VPP_HBO_SRAM_BASE,
           &VPP_dhubHandle, VPP_config, VPP_NUM_OF_CHANNELS);
    DhubInitialization(CPUINDEX, AG_DHUB_BASE, AG_HBO_SRAM_BASE,
           &AG_dhubHandle, AG_config, AG_NUM_OF_CHANNELS);

#ifdef CONFIG_MV_AMP_COMPONENT_AVIN_ENABLE
	DhubInitialization(CPUINDEX, AVIF_DHUB_BASE, AVIF_HBO_SRAM_BASE,
			&AVIF_dhubHandle, AVIF_config, AVIF_NUM_OF_CHANNELS);
#endif

#if ((BERLIN_CHIP_VERSION != BERLIN_BG4_CD) && (BERLIN_CHIP_VERSION != BERLIN_BG4_CT))
    //Fix me: since cec interrupt is not specific, hack it temporary
	/* register and enable cec interrupt */
	vec_num = amp_irqs[IRQ_SM_CEC];
	err =
	    request_irq(vec_num, amp_devices_vpp_cec_isr, IRQF_DISABLED,
			"amp_module_vpp_cec", (VOID *)hVppCtx);
	if (unlikely(err < 0)) {
		amp_trace("vec_num:%5d, err:%8x\n", vec_num, err);
		return err;
	}
#endif
	/* register and enable VPP ISR */
	vec_num = amp_irqs[IRQ_DHUBINTRAVIO0];
	err =
	    request_irq(vec_num, amp_devices_vpp_isr, IRQF_DISABLED,
			"amp_module_vpp", (VOID *)hVppCtx);
	if (unlikely(err < 0)) {
		amp_trace("vec_num:%5d, err:%8x\n", vec_num, err);
		return err;
	}

    //vec_num = 0x4a; //0x2a + 0x20
#if defined(CONFIG_MV_AMP_COMPONENT_AVIN_ENABLE) || defined(CONFIG_MV_AMP_COMPONENT_AIP_ENABLE)
	/* register and enable AVIF ISR */
	vec_num = amp_irqs[IRQ_DHUBINTRAVIIF0];
	err = request_irq(vec_num, amp_devices_avif_isr, IRQF_DISABLED,
			  "amp_module_avif", (VOID *)hAvifCtx);
	if (unlikely(err < 0)) {
		amp_trace("vec_num:%5d, err:%8x\n", vec_num, err);
		return err;
	}
#endif

#ifdef CONFIG_MV_AMP_COMPONENT_VMETA_ENABLE
	/* register and enable VDEC VMETA ISR */
	vec_num = amp_irqs[IRQ_DHUBINTRVPRO];
	err =
	    request_irq(vec_num, amp_devices_vdec_isr, IRQF_DISABLED,
			"amp_module_vdec", (VOID *)hVpuCtx);
	if (unlikely(err < 0)) {
		amp_trace("vec_num:%5d, err:%8x\n", vec_num, err);
		return err;
	}
    disable_irq(amp_irqs[IRQ_DHUBINTRVPRO]);
#endif

#ifdef CONFIG_MV_AMP_COMPONENT_V2G_ENABLE
    /* register and enable VDEC V2G ISR */
	vec_num = V2G_IRQ_NUM;
	err =
	    request_irq(vec_num, amp_devices_vdec_isr, IRQF_DISABLED,
			"amp_module_vdec", (VOID *)hVpuCtx);
	if (unlikely(err < 0)) {
		amp_trace("vec_num:%5d, err:%8x\n", vec_num, err);
		return err;
	}
    disable_irq(V2G_IRQ_NUM);
#endif

	/* register and enable audio out ISR */
	vec_num = amp_irqs[IRQ_DHUBINTRAVIO1];
	err =
	    request_irq(vec_num, amp_devices_aout_isr, IRQF_DISABLED,
			"amp_module_aout", (VOID *)hAoutCtx);
	if (unlikely(err < 0)) {
		amp_trace("vec_num:%5d, err:%8x\n", vec_num, err);
		return err;
	}

#ifdef CONFIG_MV_AMP_COMPONENT_ZSP_ENABLE
	/* register and enable ZSP ISR */
	vec_num = amp_irqs[IRQ_ZSPINT];
	err =
	    request_irq(vec_num, amp_devices_zsp_isr, IRQF_DISABLED,
			"amp_module_zsp", (VOID *)hZspCtx);
	if (unlikely(err < 0)) {
		amp_error("vec_num:%5d, err:%8x\n", vec_num, err);
		return err;
	}
#endif

    amp_trace("amp_driver_open ok\n");
    isAmpReleased = 0;

	return 0;
}

static INT amp_driver_release(struct inode *inode, struct file *filp)
{
    ULONG aoutirq;

    if(isAmpReleased)
	return 0;

    /* unregister cec interrupt */
    free_irq(amp_irqs[IRQ_SM_CEC], (VOID *)hVppCtx);
    /* unregister VPP interrupt */
    free_irq(amp_irqs[IRQ_DHUBINTRAVIO0], (VOID *)hVppCtx);

#ifdef CONFIG_MV_AMP_COMPONENT_VMETA_ENABLE
    /* unregister VDEC vmeta/v2g interrupt */
    free_irq(amp_irqs[IRQ_DHUBINTRVPRO], (VOID *)hVpuCtx);
#endif

#ifdef CONFIG_MV_AMP_COMPONENT_V2G_ENABLE
    free_irq(V2G_IRQ_NUM, (VOID *)hVpuCtx);
#endif

    /* unregister audio out interrupt */
    free_irq(amp_irqs[IRQ_DHUBINTRAVIO1], (VOID *)hAoutCtx);

#ifdef CONFIG_MV_AMP_COMPONENT_ZSP_ENABLE
    /* unregister ZSP interrupt */
    free_irq(amp_irqs[IRQ_ZSPINT], (VOID *)hZspCtx);
#endif

#if defined(CONFIG_MV_AMP_COMPONENT_AVIN_ENABLE) || defined(CONFIG_MV_AMP_COMPONENT_AIP_ENABLE)
    /* unregister AVIF interrupt */
    free_irq(amp_irqs[IRQ_DHUBINTRAVIIF0], (VOID *)hAvifCtx);
#endif

#ifdef VPP_DHUB_IN_TZ
	DhubFinalize();
#endif

	/* Stop all commands */
	spin_lock_irqsave(&hAoutCtx->aout_spinlock, aoutirq);
	aout_stop_cmd(hAoutCtx, MULTI_PATH);
#ifdef CONFIG_MV_AMP_AUDIO_PATH_20_ENABLE
	aout_stop_cmd(hAoutCtx, LoRo_PATH);
#endif
#ifdef CONFIG_MV_AMP_AUDIO_PATH_SPDIF_ENABLE
	aout_stop_cmd(hAoutCtx, SPDIF_PATH);
#endif
#ifdef CONFIG_MV_AMP_AUDIO_PATH_HDMI_ENABLE
	aout_stop_cmd(hAoutCtx, HDMI_PATH);
#endif
	spin_unlock_irqrestore(&hAoutCtx->aout_spinlock, aoutirq);

	isAmpReleased = 1;
	amp_trace("amp_driver_release ok\n");

    return 0;
}

static INT amp_driver_mmap(struct file *file, struct vm_area_struct *vma)
{
	size_t size = vma->vm_end - vma->vm_start;


	if (size > CONFIG_MV88DE3010_REGAREA_SIZE)
		return -EINVAL;

	vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);

	/* Remap-pfn-range will mark the range VM_IO and VM_RESERVED */
	if (remap_pfn_range(vma,
			    vma->vm_start,
			    (CONFIG_MV88DE3010_REGAREA_BASE >> PAGE_SHIFT) + vma->vm_pgoff,
			    size,
			    vma->vm_page_prot)) {
		return -EAGAIN;
	}
	amp_trace("amp_driver_mmap ok\n");
	return 0;
}

static LONG
amp_driver_ioctl_unlocked(struct file *filp, UINT cmd,
			  ULONG arg)
{
    INT bcmbuf_info[2];
    INT aout_info[2];
    ULONG irqstat=0;
    ULONG aoutirq=0;
#ifdef CONFIG_MV_AMP_COMPONENT_HWAPP_ENABLE
    ULONG appirq=0;
#endif
#ifdef CONFIG_MV_AMP_COMPONENT_AIP_ENABLE
    ULONG aipirq=0;
    INT aip_info[3];
#endif
	UINT bcm_sche_cmd_info[3], q_id;
	INT shm_mem = 0, gid = 0;
	PVOID param = NULL;
	struct ion_handle *ionh = NULL;
	struct ion_client *ionc = legacy_pe_dev.ionClient;
	switch (cmd) {
	case VPP_IOCTL_BCM_SCHE_CMD:
		if (copy_from_user
		    (bcm_sche_cmd_info, (VOID __user *)arg,
		     3 * sizeof(UINT)))
			return -EFAULT;
		q_id = bcm_sche_cmd_info[2];
		if (q_id > BCM_SCHED_Q5) {
			amp_trace("error BCM queue ID = %d\n", q_id);
			return -EFAULT;
		}
		hVppCtx->bcm_sche_cmd[q_id][0] = bcm_sche_cmd_info[0];
		hVppCtx->bcm_sche_cmd[q_id][1] = bcm_sche_cmd_info[1];
		amp_trace("vpp bcm shed qid:%d info:%x %x\n",
				q_id,
				bcm_sche_cmd_info[0],
				bcm_sche_cmd_info[0]);
		break;

	case VPP_IOCTL_GET_MSG:
		{
			unsigned long flag;
			MV_CC_MSG_t msg = { 0 };
			HRESULT rc = S_OK;

			rc = down_interruptible(&hVppCtx->vpp_sem);
			if (rc < 0)
				return rc;

#ifdef CONFIG_IRQ_LATENCY_PROFILE
			hVppCtx->amp_irq_profiler.vpp_task_sched_last =
			    cpu_clock(smp_processor_id());
#endif
			// check fullness, clear message queue once.
			// only send latest message to task.
			if (AMPMsgQ_Fullness(&hVppCtx->hVPPMsgQ) <= 0) {
				//amp_trace(" E/[vpp isr task]  message queue empty\n");
				return -EFAULT;
			}

			spin_lock_irqsave(&hVppCtx->vpp_msg_spinlock, flag);
			AMPMsgQ_DequeueRead(&hVppCtx->hVPPMsgQ, &msg);
			spin_unlock_irqrestore(&hVppCtx->vpp_msg_spinlock, flag);

			if (!atomic_read(&hVppCtx->vpp_isr_msg_err_cnt)) {
				atomic_set(&hVppCtx->vpp_isr_msg_err_cnt, 0);
			}
			if (copy_to_user
			    ((VOID __user *)arg, &msg, sizeof(MV_CC_MSG_t)))
				return -EFAULT;
			break;
		}
	case CEC_IOCTL_RX_MSG_BUF_MSG:	// copy cec rx message to user space buffer
		if (copy_to_user
		    ((VOID __user *)arg, &hVppCtx->rx_buf, sizeof(VPP_CEC_RX_MSG_BUF)))
			return -EFAULT;

		return S_OK;
		break;
	case VPP_IOCTL_START_BCM_TRANSACTION:
#if ((BERLIN_CHIP_VERSION != BERLIN_BG4_CD) && (BERLIN_CHIP_VERSION != BERLIN_BG4_CT))
		if (copy_from_user
		    (bcmbuf_info, (VOID __user *)arg, 2 * sizeof(INT)))
			return -EFAULT;
		spin_lock_irqsave(&hVppCtx->bcm_spinlock, irqstat);
		wrap_dhub_channel_write_cmd(&(VPP_dhubHandle.dhub),
				       avioDhubChMap_vpp_BCM_R, bcmbuf_info[0],
				       bcmbuf_info[1], 0, 0, 0, 1, 0, 0);
		spin_unlock_irqrestore(&hVppCtx->bcm_spinlock, irqstat);
#endif
		break;

	case VPP_IOCTL_INTR_MSG:
		//get VPP INTR MASK info
		{
			INTR_MSG vpp_intr_info = { 0, 0 };

			if (copy_from_user
			    (&vpp_intr_info, (VOID __user *)arg,
			     sizeof(INTR_MSG)))
				return -EFAULT;

			if (vpp_intr_info.DhubSemMap < MAX_INTR_NUM)
				hVppCtx->vpp_intr_status[vpp_intr_info.DhubSemMap] =
				    vpp_intr_info.Enable;
			else
				return -EFAULT;

			break;
		}

#ifdef CONFIG_MV_AMP_COMPONENT_AVIN_ENABLE
	/**************************************
	 * AVIF IOCTL
	 **************************************/
	case AVIF_IOCTL_GET_MSG:
	{
		MV_CC_MSG_t msg = { 0 };
		HRESULT rc = S_OK;
		rc = down_interruptible(&hAvifCtx->avif_sem);
		if (rc < 0)
			return rc;

		rc = AMPMsgQ_ReadTry(&hAvifCtx->hPEAVIFMsgQ, &msg);
		if (unlikely(rc != S_OK)) {
			amp_trace("AVIF read message queue failed\n");
			return -EFAULT;
		}
		AMPMsgQ_ReadFinish(&hAvifCtx->hPEAVIFMsgQ);
		if (!atomic_read(&hAvifCtx->avif_isr_msg_err_cnt)) {
			atomic_set(&hAvifCtx->avif_isr_msg_err_cnt, 0);
		}
		if (copy_to_user
		    ((VOID __user *)arg, &msg, sizeof(MV_CC_MSG_t)))
			return -EFAULT;
		break;
	}
	case AVIF_IOCTL_VDE_CFGQ:
		break;
	case AVIF_IOCTL_VBI_CFGQ:
		break;
	case AVIF_IOCTL_SD_WRE_CFGQ:
		break;
	case AVIF_IOCTL_SD_RDE_CFGQ:
		break;
	case AVIF_IOCTL_SEND_MSG:
	{
		//get msg from AVIF
		INT avif_msg = 0;
		if (copy_from_user(&avif_msg, (VOID __user *)arg, sizeof(INT)))
			return -EFAULT;

		if (avif_msg == AVIF_MSG_DESTROY_ISR_TASK) {
			//force one more INT to AVIF to destroy ISR task
			printk("Destroy ISR Task...\r\n");
			up(&hAvifCtx->avif_sem);
		}
		printk("Destroyed ISR Task...\r\n");
#ifdef MORPHEUS_TV_VIEW_MODE
		{
		    extern int vip_stable_isr;
			extern int vip_stable;
			extern int vpp_res_set;
			extern int vpp_cpcb0_res;
			extern int vpp_4k_res;
			extern TG_CHANGE_STATE tg_changed;
			extern long long vip_isr_count;
			extern long long cur_vip_isr_count;
			extern long long vpp_isr_count;
			extern long long cur_vpp_isr_count;
			amp_trace("*** timeline info ***\n");
			vip_stable_isr = 0;
			vip_stable = 0;
		    vpp_res_set = 0;
		    vpp_cpcb0_res = -1;
		    vpp_4k_res = -1;
			tg_changed = TG_CHANGE_STATE_CHECK;
			vip_isr_count = 0;
			cur_vip_isr_count = 0;
			vpp_isr_count = 0;
			cur_vpp_isr_count = 0;
			reset_adjust(1);
		}
#endif
		break;
	}
#ifdef MORPHEUS_TV_VIEW_MODE
	case AVIF_IOCTL_SEND_MSG_TV_VIEW:
	{
		extern int video_mode;
		extern int vip_stable_isr;
		extern int vip_stable;
		extern int vpp_res_set;
		extern int vpp_cpcb0_res;
		extern int vpp_4k_res;
		extern TG_CHANGE_STATE tg_changed;
		extern long long vip_isr_count;
		extern long long cur_vip_isr_count;
		extern long long vpp_isr_count;
		extern long long cur_vpp_isr_count;

		MSG_TV_VIEW avif_msg = {0, 0, 0};
		if (copy_from_user(&avif_msg, (void __user *)arg, sizeof(MSG_TV_VIEW)))
			return -EFAULT;

		if (avif_msg.msg_type == AVIF_MSG_NOTIFY_STABLE){
			amp_trace("AVIF stable\n");
			vip_stable = 1;
			amp_trace("end AVIF stable\n");
		} else if (avif_msg.msg_type == AVIF_MSG_NOTIFY_VPP_RES_SET) {
			amp_trace("VPP res set done. vpp_cpcb0_res %d, vpp_4k_res %d\n",
					avif_msg.para0, avif_msg.para1);
			vpp_res_set = 1;
			vpp_cpcb0_res = avif_msg.para0;
			vpp_4k_res = avif_msg.para1;
			amp_trace("end VPP res set done\n");
		} else if (avif_msg.msg_type == AVIF_MSG_NOTIFY_VPP_DISCONNECT) {
			amp_trace("VPP disconnect\n");
			vpp_res_set = 0;
			vpp_cpcb0_res = -1;
			vpp_4k_res = -1;
			reset_adjust(1);
			tg_changed = TG_CHANGE_STATE_CHECK;
			amp_trace("end VPP disconnect\n");
		} else if (avif_msg.msg_type == AVIF_MSG_SET_TV_VIEW_MODE) {
			if (video_mode != MODE_TV_VIEW) {
				amp_trace("Set natvie mode\n");
				video_mode = MODE_INVALID;
				reset_adjust(1);
				tg_changed = TG_CHANGE_STATE_CHECK;
				video_mode = MODE_TV_VIEW;
				amp_trace("end Set natvie mode\n");
			} else {
				amp_trace("Already in the native mode\n");
			}
		} else if (avif_msg.msg_type == AVIF_MSG_SET_INVALID_MODE) {
			if (video_mode != MODE_INVALID) {
				amp_trace("Set invalid mode\n");
				video_mode = MODE_INVALID;
				vip_stable_isr = 0;
				vip_stable = 0;
				vpp_res_set = 0;
				vpp_cpcb0_res = -1;
				vpp_4k_res = -1;

				vip_isr_count = 0;
				cur_vip_isr_count = 0;
				vpp_isr_count = 0;
				cur_vpp_isr_count = 0;
				reset_adjust(1);
				amp_trace("end Set invalid mode\n");
			} else {
				amp_trace("Already in the invalid mode\n");
			}
		}
		break;
	}
#endif
	case AVIF_IOCTL_INTR_MSG:
	{
		//get AVIF INTR MASK info
		INTR_MSG avif_intr_info = { 0, 0 };

		if (copy_from_user(&avif_intr_info, (VOID __user *)arg, sizeof(INTR_MSG)))
			return -EFAULT;

		if (avif_intr_info.DhubSemMap<MAX_INTR_NUM)
			hAvifCtx->avif_intr_status[avif_intr_info.DhubSemMap] = avif_intr_info.Enable;
		else
			return -EFAULT;
		break;
	}
#ifdef MORPHEUS_TV_VIEW_MODE
	case AVIF_IOCTL_GET_AVIF_VPP_DRIFT_INFO:
	{
		extern spinlock_t drift_countlock;
		extern DRIFT_INFO vip_vpp_drift_info;
		DRIFT_INFO internal_vip_vpp_drift_info = {0};

		spin_lock_irqsave(&drift_countlock, irqstat);

		internal_vip_vpp_drift_info.valid = vip_vpp_drift_info.valid;
		internal_vip_vpp_drift_info.start_latency = vip_vpp_drift_info.start_latency;
		internal_vip_vpp_drift_info.drift_count = vip_vpp_drift_info.drift_count;
		internal_vip_vpp_drift_info.total_drift_count = vip_vpp_drift_info.total_drift_count;
		internal_vip_vpp_drift_info.frame_count = vip_vpp_drift_info.frame_count;
		internal_vip_vpp_drift_info.latency_in_the_expected_range =
		    vip_vpp_drift_info.latency_in_the_expected_range;

		vip_vpp_drift_info.valid  = 0;
		vip_vpp_drift_info.start_latency = 0;
		vip_vpp_drift_info.drift_count = 0;
		vip_vpp_drift_info.total_drift_count = 0;
		vip_vpp_drift_info.frame_count = 0;
		vip_vpp_drift_info.latency_in_the_expected_range = 0;

		spin_unlock_irqrestore(&drift_countlock, irqstat);

		if (copy_to_user
				((void __user *)arg, &internal_vip_vpp_drift_info, sizeof(DRIFT_INFO))) {
			return -EFAULT;
		}
		break;
	}
#endif
	case AVIF_HRX_IOCTL_GET_MSG:
	{
		MV_CC_MSG_t msg = { 0 };
		HRESULT rc = S_OK;
		rc = down_interruptible(&hAvifCtx->avif_hrx_sem);
		if (rc < 0)
			return rc;

		rc = AMPMsgQ_ReadTry(&hAvifCtx->hPEAVIFHRXMsgQ, &msg);
		if (unlikely(rc != S_OK)) {
			amp_trace("HRX read message queue failed\n");
			return -EFAULT;
		}
		AMPMsgQ_ReadFinish(&hAvifCtx->hPEAVIFHRXMsgQ);
		if (copy_to_user
		    ((VOID __user *)arg, &msg, sizeof(MV_CC_MSG_t)))
			return -EFAULT;
		break;
	}
	case AVIF_HRX_IOCTL_SEND_MSG: /* get msg from User Space */
	{
		INT hrx_msg = 0;
		if (copy_from_user
		    (&hrx_msg, (VOID __user *)arg, sizeof(INT)))
			return -EFAULT;

		if (hrx_msg == AVIF_HRX_DESTROY_ISR_TASK) {
			/* force one more INT to HRX to destroy ISR task */
			up(&hAvifCtx->avif_hrx_sem);
		}

		break;
	}

	case AVIF_HDCP_IOCTL_GET_MSG:
	{
		MV_CC_MSG_t msg = { 0 };
		HRESULT rc = S_OK;
		rc = down_interruptible(&hAvifCtx->avif_hdcp_sem);
		if (rc < 0)
		return rc;

		rc = AMPMsgQ_ReadTry(&hAvifCtx->hPEAVIFHDCPMsgQ, &msg);
		if (unlikely(rc != S_OK)) {
			amp_trace("HRX read message queue failed\n");
			return -EFAULT;
		}
		AMPMsgQ_ReadFinish(&hAvifCtx->hPEAVIFHDCPMsgQ);
		if (copy_to_user
			((VOID __user *)arg, &msg, sizeof(MV_CC_MSG_t)))
			return -EFAULT;
		break;
	}

	case AVIF_HDCP_IOCTL_SEND_MSG: /* get msg from User Space */
	{
	    INT hdcp_msg = 0;
		if (copy_from_user
			(&hdcp_msg, (VOID __user *)arg, sizeof(INT)))
			return -EFAULT;

		if (hdcp_msg == AVIF_HDCP_DESTROY_ISR_TASK) {
		/* force one more INT to HDCP to destroy ISR task */
			up(&hAvifCtx->avif_hdcp_sem);
		}
	break;
	}

#ifdef CONFIG_MV_AMP_AUDIO_PATH_SPDIF_ENABLE
	case AVIF_HRX_IOCTL_ARC_SET:
	{
		INT hrx_msg = 0;
		static INT count = 0;
		MV_CC_MSG_t msg = {0, 0, 0};

		if (copy_from_user
		    (&hrx_msg, (VOID __user *)arg, sizeof(INT)))
			return -EFAULT;
		if(count != 0)
			break;
		count++;
		printk("set arc enable value to %d\n",hrx_msg);
		//issue a dhub interrupt for avif arc or aout
		arc_copy_spdiftx_data(hAvifCtx);

		msg.m_MsgID = 1 << avioDhubChMap_ag_SPDIF_R;
		AMPMsgQ_Add(&hAoutCtx->hAoutMsgQ, &msg);
		up(&hAoutCtx->aout_sem);

		break;
	}
#endif
    case AVIF_HRX_IOCTL_SET_FACTORY:
    {
        UINT32 *mode = (UINT32 *)arg;
        hAvifCtx->factory_mode = *mode;
        printk("Set factory mode: %d\r\n", hAvifCtx->factory_mode);
        break;
    }
	case AVIF_VDEC_IOCTL_GET_MSG:
	{
		MV_CC_MSG_t msg = { 0 };
		HRESULT rc = S_OK;
		rc = down_interruptible(&hAvifCtx->avif_vdec_sem);
		if (rc < 0)
			return rc;

		rc = AMPMsgQ_ReadTry(&hAvifCtx->hPEAVIFVDECMsgQ, &msg);
		if (unlikely(rc != S_OK)) {
			amp_trace("VDEC read message queue failed\n");
			return -EFAULT;
		}
		AMPMsgQ_ReadFinish(&hAvifCtx->hPEAVIFVDECMsgQ);
		if (copy_to_user
		    ((VOID __user *)arg, &msg, sizeof(MV_CC_MSG_t)))
			return -EFAULT;
		break;
	}

	case AVIF_VDEC_IOCTL_SEND_MSG: /* get msg from User Space */
	{
		INT hrx_msg = 0;
		if (copy_from_user
		    (&hrx_msg, (VOID __user *)arg, sizeof(INT)))
			return -EFAULT;

		if (hrx_msg == AVIF_VDEC_DESTROY_ISR_TASK) {
			/* force one more INT to HRX to destroy ISR task */
			up(&hAvifCtx->avif_vdec_sem);
		}

		break;
	}
#endif

	/**************************************
	 * VDEC IOCTL
	 **************************************/
    case VDEC_IOCTL_ENABLE_INT:
    {
        MV_CC_MSG_t msg = { 0 };
        HRESULT rc = S_OK;
        INT vpu_id = (INT)arg;

#if CONFIG_VDEC_IRQ_CHECKER
        if ((vpu_id == VPU_VMETA) ||
            (vpu_id == VPU_G1)) {
            hVpuCtx->vdec_enable_int_cnt++;
        }
#endif

        if ((vpu_id == VPU_VMETA) ||
            (vpu_id == VPU_G1)) {
#ifdef CONFIG_MV_AMP_COMPONENT_VMETA_ENABLE
            enable_irq(amp_irqs[IRQ_DHUBINTRVPRO]);
            rc = down_interruptible(&hVpuCtx->vdec_vmeta_sem);
            if (rc < 0)
                return rc;

            // check fullness, clear message queue once.
            // only send latest message to task.
            if (AMPMsgQ_Fullness(&hVpuCtx->hVPUMsgQ) <= 0) {
                //amp_trace(" E/[vdec isr task]  message queue empty\n");
                return -EFAULT;
            }

            AMPMsgQ_DequeueRead(&hVpuCtx->hVPUMsgQ, &msg);

            if (!atomic_read(&hVpuCtx->vdec_isr_msg_err_cnt)) {
                atomic_set(&hVpuCtx->vdec_isr_msg_err_cnt, 0);
            }
#endif
        } else if (vpu_id == VPU_V2G) {
#ifdef CONFIG_MV_AMP_COMPONENT_V2G_ENABLE
            enable_irq(V2G_IRQ_NUM);
            rc = down_interruptible(&hVpuCtx->vdec_v2g_sem);
            if (rc < 0)
                return rc;
#endif
        }
        break;
    }

    /**************************************
     * AOUT IOCTL
     **************************************/
    case AOUT_IOCTL_START_CMD:
        {
            UINT32 output_port;
            if (copy_from_user
                (aout_info, (VOID __user *)arg, 2 * sizeof(INT)))
                return -EFAULT;
            output_port = aout_info[0];
            if (output_port >= MAX_OUTPUT_AUDIO) {
                printk("output %d out of range\r\n", output_port);
                break;
            }
            gid = aout_info[1];
            if (ionc) {
                ionh = ion_gethandle(ionc, gid);
            }
            if (ionh) {
                hAoutCtx->gstAoutIon.ionh[output_port] = ionh;
                param = ion_map_kernel(ionc, ionh);
                if (debug_ctl > 0) {
                    ion_phys_addr_t addr = 0;
                    size_t len = 0;
                    ion_phys(ionc, ionh, &addr, &len);
                    amp_trace("aout start path:%x gid:%x ion:%x pfifo:%x phyadd:%x len:%x\n",
                         (UINT)output_port, (UINT)gid, (UINT)ionh,
                         (UINT)param, (UINT)addr, (UINT)len);
                }
            }
            if (!param) {
                amp_trace("ctl:%x bad shm mem offset:%x\n", AOUT_IOCTL_START_CMD, shm_mem);
                return -EFAULT;
            }
            //MV_SHM_Takeover(shm_mem);
            spin_lock_irqsave(&hAoutCtx->aout_spinlock, aoutirq);
            aout_start_cmd(hAoutCtx, aout_info, param);
            spin_unlock_irqrestore(&hAoutCtx->aout_spinlock, aoutirq);
            break;
        }
    case AOUT_IOCTL_STOP_CMD:
        {
            UINT32 output_port;
            if (copy_from_user(aout_info, (VOID __user *)arg, 1 * sizeof(INT)))
                return -EFAULT;

            output_port = aout_info[0];
            if (output_port >= MAX_OUTPUT_AUDIO) {
                printk("output %d out of range\r\n", output_port);
                break;
            }
            spin_lock_irqsave(&hAoutCtx->aout_spinlock, aoutirq);
            aout_stop_cmd(hAoutCtx, output_port);
            spin_unlock_irqrestore(&hAoutCtx->aout_spinlock, aoutirq);
            if (ionc && hAoutCtx->gstAoutIon.ionh[output_port]) {
                if (debug_ctl > 0) {
                    amp_trace("aout stop path:%x ion:%x\n",
                        (UINT)output_port, (UINT)hAoutCtx->gstAoutIon.ionh[output_port]);
                }
                ion_unmap_kernel(ionc, hAoutCtx->gstAoutIon.ionh[output_port]);
                ion_free(ionc, hAoutCtx->gstAoutIon.ionh[output_port]);
                hAoutCtx->gstAoutIon.ionh[output_port] = NULL;
            }
            break;
        }

#ifdef CONFIG_MV_AMP_COMPONENT_AIP_ENABLE
	/**************************************
	 * AIP IOCTL
	 **************************************/
	/**************************************
	 * AIP AVIF IOCTL
	 **************************************/
	case AIP_AVIF_IOCTL_START_CMD:
		if (copy_from_user(aip_info, (VOID __user *)arg, 3 * sizeof(INT))) {
			return -EFAULT;
		}

		if ((SPDIFRX_AUDIO_SOURCE == aip_info[2]) && hVppCtx) {
			hVppCtx->is_spdifrx_enabled = 1;
		}
		gid = aip_info[1];
		if (ionc) {
			ionh = ion_gethandle(ionc, gid);
		}
		if (ionh) {
			hAvifCtx->gstAipIon.ionh = ionh;
			param = ion_map_kernel(ionc, ionh);
		}
		if (!param) {
			amp_trace("ctl:%x bad shm mem offset:%x\n", AIP_AVIF_IOCTL_START_CMD, shm_mem);
			return -EFAULT;
		}
		spin_lock_irqsave(&hAvifCtx->aip_spinlock, aipirq);
		aip_avif_start_cmd(hAvifCtx, aip_info, param);
		spin_unlock_irqrestore(&hAvifCtx->aip_spinlock, aipirq);
		break;

	case AIP_AVIF_IOCTL_STOP_CMD:
        {
		aip_avif_stop_cmd(hAvifCtx);
		if (ionc && hAvifCtx->gstAipIon.ionh) {
			ion_unmap_kernel(ionc, hAvifCtx->gstAipIon.ionh);
			ion_free(ionc, hAvifCtx->gstAipIon.ionh);
			hAvifCtx->gstAipIon.ionh = NULL;
		}

		break;
        }

	case AIP_IOCTL_SemUp_CMD:
		{
			 up(&hAvifCtx->aip_sem);
			 break;
		}

	case AIP_IOCTL_GET_MSG_CMD:
		{
			MV_CC_MSG_t msg = {0};
			HRESULT rc = S_OK;

			rc = down_interruptible(&hAvifCtx->aip_sem);
			if (rc < 0) {
				return rc;
			}

			rc = AMPMsgQ_ReadTry(&hAvifCtx->hAIPMsgQ, &msg);
			if (unlikely(rc != S_OK)) {
				amp_trace("AIP read message queue failed\n");
				return -EFAULT;
			}
			AMPMsgQ_ReadFinish(&hAvifCtx->hAIPMsgQ);

			if (copy_to_user
				((VOID __user *)arg, &msg, sizeof(MV_CC_MSG_t))) {
				return -EFAULT;
			}
			break;
		}

#endif // CONFIG_MV_AMP_COMPONENT_AIP_ENABLE

#ifdef CONFIG_MV_AMP_COMPONENT_HWAPP_ENABLE
	/**************************************
	 * APP IOCTL
	 **************************************/
    case APP_IOCTL_INIT_CMD:
        if (copy_from_user(&gid, (VOID __user *)arg, sizeof(INT))) {
            return -EFAULT;
        }
        if (ionc) {
            ionh = ion_gethandle(ionc, gid);
        }
        if (ionh) {
            hAppCtx->gstAppIon.ionh = ionh;
            hAppCtx->p_app_fifo = (HWAPP_CMD_FIFO *)ion_map_kernel(ionc, ionh);
        }
        break;

    case APP_IOCTL_DEINIT_CMD:
        {
            if (copy_from_user(&gid, (VOID __user *)arg, sizeof(INT))) {
                return -EFAULT;
            }
            if (ionc && hAppCtx->gstAppIon.ionh) {
                ion_unmap_kernel(ionc, hAppCtx->gstAppIon.ionh);
                ion_free(ionc, hAppCtx->gstAppIon.ionh);
                hAppCtx->gstAppIon.ionh = NULL;
            }
            break;
        }

	case APP_IOCTL_START_CMD:
		spin_lock_irqsave(&hAppCtx->app_spinlock, appirq);
        if (hAppCtx->p_app_fifo) {
            app_start_cmd(hAppCtx, hAppCtx->p_app_fifo);
        }
		spin_unlock_irqrestore(&hAppCtx->app_spinlock, appirq);
		break;

	case APP_IOCTL_GET_MSG_CMD:
		{
			MV_CC_MSG_t msg = { 0 };
			HRESULT rc = S_OK;

			rc = down_interruptible(&hAppCtx->app_sem);
			if (rc < 0)
				return rc;

			rc = AMPMsgQ_ReadTry(&hAppCtx->hAPPMsgQ, &msg);
			if (unlikely(rc != S_OK)) {
				amp_trace("APP read message queue failed\n");
				return -EFAULT;
			}
			AMPMsgQ_ReadFinish(&hAppCtx->hAPPMsgQ);
			if (copy_to_user
			    ((VOID __user *)arg, &msg, sizeof(MV_CC_MSG_t)))
				return -EFAULT;
			break;
		}
#endif

    case AOUT_IOCTL_GET_MSG_CMD:
        {
			HRESULT rc = S_OK;
			MV_CC_MSG_t msg = {0};
			rc = down_interruptible(&hAoutCtx->aout_sem);
			if (rc < 0) {
			    return rc;
			}
			rc = AMPMsgQ_ReadTry(&hAoutCtx->hAoutMsgQ, &msg);
			if (unlikely(rc != S_OK)) {
			    amp_trace("AOUT read message queue failed\n");
			    return -EFAULT;
			}
			AMPMsgQ_ReadFinish(&hAoutCtx->hAoutMsgQ);
			if (copy_to_user((VOID __user *)arg, &msg, sizeof(MV_CC_MSG_t))) {
			    return -EFAULT;
			}
			break;
        }

#ifdef CONFIG_MV_AMP_COMPONENT_ZSP_ENABLE
	case ZSP_IOCTL_GET_MSG_CMD:
		{
		    MV_CC_MSG_t msg = {0};
		    HRESULT rc = S_OK;
		    rc = down_interruptible(&hZspCtx->zsp_sem);
		    if (rc < 0) {
		        return rc;
		    }
		    rc = AMPMsgQ_ReadTry(&hZspCtx->hZSPMsgQ, &msg);
		    if (unlikely(rc != S_OK)) {
		        amp_trace("ZSP read message queue failed\n");
		        return -EFAULT;
		    }
		    AMPMsgQ_ReadFinish(&hZspCtx->hZSPMsgQ);
		    if (copy_to_user
		        ((VOID __user *)arg, &msg, sizeof(MV_CC_MSG_t))) {
		        return -EFAULT;
		    }
		    break;
		}
#endif
	default:
		break;
	}

	return 0;
}

/*******************************************************************************
  Module Register API
  */
static INT
amp_driver_setup_cdev(struct cdev *dev, INT major, INT minor,
          struct file_operations *fops)
{
    cdev_init(dev, fops);
    dev->owner = THIS_MODULE;
    dev->ops = fops;
    return cdev_add(dev, MKDEV(major, minor), 1);
}

INT __init amp_drv_init(struct amp_device_t *amp_device)
{
    INT res;
#if LINUX_VERSION_CODE < KERNEL_VERSION (3,10,0)
    struct proc_dir_entry *e = NULL;
#endif

    /* Now setup cdevs. */
    res =
    amp_driver_setup_cdev(&amp_device->cdev, amp_device->major,
              amp_device->minor, amp_device->fops);
    if (res) {
    amp_error("amp_driver_setup_cdev failed.\n");
    res = -ENODEV;
    goto err_add_device;
    }
    amp_trace("setup cdevs device minor [%d]\n", amp_device->minor);

    /* add PE devices to sysfs */
    amp_device->dev_class = class_create(THIS_MODULE, amp_device->dev_name);
    if (IS_ERR(amp_device->dev_class)) {
    amp_error("class_create failed.\n");
    res = -ENODEV;
    goto err_add_device;
    }

    device_create(amp_device->dev_class, NULL,
          MKDEV(amp_device->major, amp_device->minor), NULL,
          amp_device->dev_name);
    amp_trace("create device sysfs [%s]\n", amp_device->dev_name);

    /* create hw device */
    if (amp_device->dev_init) {
    res = amp_device->dev_init(amp_device, 0);
    if (res != 0) {
        amp_error("amp_int_init failed !!! res = 0x%08X\n",
              res);
        res = -ENODEV;
        goto err_add_device;
    }
    }

    return 0;

 err_add_device:

    if (amp_device->dev_procdir) {
    remove_proc_entry(AMP_DEVICE_PROCFILE_DETAIL,
              amp_device->dev_procdir);
    remove_proc_entry(AMP_DEVICE_PROCFILE_STATUS,
              amp_device->dev_procdir);
    remove_proc_entry(amp_device->dev_name, NULL);
    }

    if (amp_device->dev_class) {
    device_destroy(amp_device->dev_class,
               MKDEV(amp_device->major, amp_device->minor));
    class_destroy(amp_device->dev_class);
    }

    cdev_del(&amp_device->cdev);

    return res;
}

INT __exit amp_drv_exit(struct amp_device_t *amp_device)
{
    INT res;

    amp_trace("amp_drv_exit [%s] enter\n", amp_device->dev_name);

    /* destroy kernel API */
    if (amp_device->dev_exit) {
    res = amp_device->dev_exit(amp_device, 0);
    if (res != 0)
        amp_error("dev_exit failed !!! res = 0x%08X\n", res);
    }

    if (amp_device->dev_procdir) {
    /* remove PE device proc file */
    remove_proc_entry(AMP_DEVICE_PROCFILE_DETAIL,
              amp_device->dev_procdir);
    remove_proc_entry(AMP_DEVICE_PROCFILE_STATUS,
              amp_device->dev_procdir);
    remove_proc_entry(amp_device->dev_name, NULL);
    }

    if (amp_device->dev_class) {
    /* del sysfs entries */
    device_destroy(amp_device->dev_class,
               MKDEV(amp_device->major, amp_device->minor));
    amp_trace("delete device sysfs [%s]\n", amp_device->dev_name);

    class_destroy(amp_device->dev_class);
    }
    /* del cdev */
    cdev_del(&amp_device->cdev);

    return 0;
}

static INT __init amp_module_init(VOID)
{
    INT i, res;
    dev_t pedev;
    struct device_node *np, *iter;
    struct resource r;

    np = of_find_compatible_node(NULL, NULL, "marvell,berlin-amp");
    if (!np)
    return -ENODEV;
    for (i = 0; i <= IRQ_DHUBINTRVPRO; i++) {
        of_irq_to_resource(np, i, &r);
        amp_irqs[i] = r.start;
    }
    //TODO:get the interrupt number dynamically
    amp_irqs[IRQ_DHUBINTRAVIIF0] = 0x4a;

    for_each_child_of_node(np, iter) {
    if (of_device_is_compatible(iter, "marvell,berlin-cec")) {
        of_irq_to_resource(iter, 0, &r);
        amp_irqs[IRQ_SM_CEC] = r.start;
    }
    }
    of_node_put(np);

    res = alloc_chrdev_region(&pedev, 0, AMP_MAX_DEVS, AMP_DEVICE_NAME);
    amp_major = MAJOR(pedev);
    if (res < 0) {
    amp_error("alloc_chrdev_region() failed for amp\n");
    goto err_reg_device;
    }
    amp_trace("register cdev device major [%d]\n", amp_major);

    legacy_pe_dev.major = amp_major;
    res = amp_drv_init(&legacy_pe_dev);
    legacy_pe_dev.ionClient = ion_client_create(ion_get_dev(), "amp_ion_client");
    amp_dev = legacy_pe_dev.cdev;

    res = amp_create_devioremap();
    if (res < 0) {
        amp_error("AMP ioremap fail!\n");
        goto err_reg_device;
    }

    HDMIRX_power_down();
#if defined(CONFIG_MV_AMP_COMPONENT_CPM_ENABLE)
    cpm_module_init();
#endif

    return 0;
 err_reg_device:
    amp_drv_exit(&legacy_pe_dev);

    unregister_chrdev_region(MKDEV(amp_major, 0), AMP_MAX_DEVS);

    amp_trace("amp_driver_init failed !!! (%d)\n", res);

    return res;
}

static VOID __exit amp_module_exit(VOID)
{
    amp_destroy_deviounmap();
    amp_trace("amp_driver_exit 1\n");
    ion_client_destroy(legacy_pe_dev.ionClient);
    legacy_pe_dev.ionClient = NULL;
    amp_drv_exit(&legacy_pe_dev);

    unregister_chrdev_region(MKDEV(amp_major, 0), AMP_MAX_DEVS);
    amp_trace("unregister cdev device major [%d]\n", amp_major);
    amp_major = 0;

#if defined(CONFIG_MV_AMP_COMPONENT_CPM_ENABLE)
    cpm_module_exit();
#endif

    amp_trace("amp_driver_exit OK\n");
}

module_init(amp_module_init);
module_exit(amp_module_exit);

/*******************************************************************************
    Module Descrption
*/
MODULE_AUTHOR("marvell");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("pe module template");
