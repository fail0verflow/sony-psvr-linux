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
#ifndef _AMP_DRIVER_H_
#define _AMP_DRIVER_H_
#include <linux/version.h>
#include <linux/cdev.h>
#include <linux/device.h>

#if LINUX_VERSION_CODE >= KERNEL_VERSION (3,10,0)
#include <../drivers/staging/android/ion/ion.h>
#else
#include <linux/ion.h>
#endif

#include "amp_type.h"
#include "amp_ioctl.h"

// server id for ICC message queue
#define MV_GS_DMX_Serv						(0x00060008)
#define MV_GS_VPP_Serv						(0x00060009)
#define MV_GS_AUD_Serv						(0x0006000a)
#define MV_GS_ZSP_Serv						(0x0006000b)
#define MV_GS_VDEC_Serv						(0x0006000c)
#define MV_GS_RLE_Serv						(0x0006000d)
#define MV_GS_VIP_Serv						(0x0006000e)
#define MV_GS_AIP_Serv						(0x0006000f)
#define MV_GS_VDM_Serv						(0x00060010)
#define MV_GS_AGENT_Serv					(0x00060011)
#define MV_GS_HWAPP_Serv					(0x00060012)
#define MV_GS_AVIF_Serv						(0x00060013)

#define MV_BERLIN_CPU0					0
#define AMP_MODULE_MSG_ID_VPP			MV_GS_VPP_Serv
#define AMP_MODULE_MSG_ID_VDEC			MV_GS_VDEC_Serv
#define AMP_MODULE_MSG_ID_AUD			MV_GS_AUD_Serv
#define AMP_MODULE_MSG_ID_ZSP			MV_GS_ZSP_Serv
#define AMP_MODULE_MSG_ID_RLE			MV_GS_RLE_Serv
#define AMP_MODULE_MSG_ID_VIP			MV_GS_VIP_Serv
#define AMP_MODULE_MSG_ID_AIP			MV_GS_AIP_Serv
#define AMP_MODULE_MSG_ID_HWAPP			MV_GS_HWAPP_Serv
#define AMP_MODULE_MSG_ID_AVIF			MV_GS_AVIF_Serv

#if 0//(BERLIN_CHIP_VERSION >= BERLIN_BG2_Q)
enum {
	IRQ_ZSPINT,
	IRQ_DHUBINTRAVIO0,
	IRQ_DHUBINTRAVIIF0, //interrupt number 42 + 32
	IRQ_DHUBINTRAVIO2,
	IRQ_DHUBINTRVPRO,
	IRQ_SM_CEC,
	IRQ_AMP_MAX,
};
#else
enum {
	IRQ_ZSPINT,
	IRQ_DHUBINTRAVIO0,
	IRQ_DHUBINTRAVIO1,
#if (BERLIN_CHIP_VERSION == BERLIN_BG2_CDP)
	IRQ_DHUBINTRV2G,
#else
	IRQ_DHUBINTRAVIO2,
#endif
	IRQ_DHUBINTRVPRO,
	IRQ_SM_CEC,
	IRQ_DHUBINTRAVIIF0,
	IRQ_AMP_MAX,
};
#endif
extern int amp_irqs[IRQ_AMP_MAX];
#define AMP_MAX_DEVS            8
#define AMP_MINOR               0
#define AMP_VPU_MINOR           1
#define AMP_SND_MINOR           2

#define MAX_CHANNELS 8

typedef enum {
	MAIN_AUDIO = 0,
	SECOND_AUDIO = 1,
	EFFECT_AUDIO = 2,
	EXT_IN_AUDIO = 10,
	BOX_EFFECT_AUDIO = 11,
	MAX_INTERACTIVE_AUDIOS = 8,
	MAX_INPUT_AUDIO = 12,
	AUDIO_INPUT_IRRELEVANT,
	NOT_DEFINED_AUDIO,
} AUDIO_CHANNEL;

typedef struct amp_reg_map_desc {
    void* vir_addr;
    phys_addr_t phy_addr;
    size_t length;
} amp_reg_map_desc_;

typedef struct {
	UINT m_block_samps;
	UINT m_fs;
	UINT m_special_flags;
} HWAPP_DATA_DS;

typedef struct {
	HWAPP_DATA_DS m_stream_in_data[MAX_INPUT_AUDIO];
	HWAPP_DATA_DS m_path_out_data[MAX_OUTPUT_AUDIO];
} HWAPP_EVENT_CTX;

struct amp_device_t {
	unsigned char *dev_name;
	struct cdev cdev;
	struct class *dev_class;
	struct file_operations *fops;
	struct mutex mutex;
	int major;
	int minor;
	struct proc_dir_entry *dev_procdir;
	void *private_data;

	int (*dev_init) (struct amp_device_t *, unsigned int);
	int (*dev_exit) (struct amp_device_t *, unsigned int);
    struct ion_client *ionClient;
};

/*******************************************************************************
  Module API defined
  */
// set 1 to enable message by ioctl
// set 0 to enable ICC message queue
#define CONFIG_VPP_IOCTL_MSG			1
// when CONFIG_VPP_IOCTL_MSG is 1
// set 1 to use internal message queue between isr and ioctl function
// set 0 to use no queue
#define CONFIG_VPP_ISR_MSGQ				1

// set 1 to use internal message queue between isr and ioctl function
// set 0 to use no queue
#define CONFIG_VIP_ISR_MSGQ  			1

#define CONFIG_VDEC_IRQ_CHECKER	        1

#define AMP_DEVICE_NAME					"ampcore"
#define AMP_DEVICE_PATH					("/dev/" AMP_DEVICE_NAME)

#define AMP_DEVICE_PROCFILE_STATUS		"status"
#define AMP_DEVICE_PROCFILE_DETAIL		"detail"

#define AVIF_HRX_DESTROY_ISR_TASK		1
#define AVIF_VDEC_DESTROY_ISR_TASK		1
#define AVIF_HDCP_DESTROY_ISR_TASK		1
#define HRX_MSG_DESTROY_ISR_TASK			1

VOID *amp_memmap_phy_to_vir(UINT32 phyaddr);
UINT32 amp_memmap_vir_to_phy(void *vir_addr);

#endif				//_PE_DRIVER_H_
