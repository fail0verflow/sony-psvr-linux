/*
 * Copyright (c) 2013-2014 Marvell International Ltd. and its affiliates.
 * All rights reserved.
 *
 * This software file (the "File") is owned and distributed by Marvell
 * International Ltd. and/or its affiliates ("Marvell") under the following
 * licensing terms.
 *
 * Marvell Commercial License Option
 * ------------------------------------
 * If you received this File from Marvell and you have entered into a
 * commercial license agreement (a "Commercial License") with Marvell, the
 * File is licensed to you under the terms of the applicable Commercial
 * License.
 *
 * Marvell GPL License Option
 * ------------------------------------
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
 */

#ifndef _TZ_NW_BOOT_H_
#define _TZ_NW_BOOT_H_

#include "tz_boot_cmd.h"

/*
 * Bellow are for Bootloader
 */
int tz_nw_verify_image(const void *src, unsigned long size, void *dst);
int tz_nw_enter_boot_stage(int stage, int mode);

/** Get the memory region count
 *
 * @param attr_mask	attribute mask to filter memory regions.
 * 			can be the flags in MR_M_*. 0 to get all.
 * 			only (region.attr & attr_mask == attr_val) is filtered.
 * @param attr_val	attribute value to filter memory regions.
 * 			use MR_ATTR() to generate it. ignored if attr_mask=0.
 *
 * @return		matched region number.
 */
unsigned long tz_nw_get_mem_region_count(
		unsigned long attr_mask, unsigned long attr_val);

/** Retrieve the memory region list.
 *
 * @param region	buffer to store the retrieved regions.
 * @param max_mum	max count can be retrieved (count region).
 * @param attr_mask	attribute mask to filter memory regions.
 * 			can be the flags in MR_M_*. 0 to get all.
 * 			only (region.attr & attr_mask == attr_val) is filtered.
 * @param attrVal	attribute value to filter memory regions.
 * 			use MR_ATTR() to generate it. ignored if attr_mask=0.
 *
 * @return		retrieved region number. if max_num < total
 *			matched region number, then only max_mum would be copied,
 *			and return total matched region_number.
 */
unsigned long tz_nw_get_mem_region_list(
		void *region, unsigned long max_num,
		unsigned long attr_mask, unsigned long attr_val);

/*
 * Bellow are for Linux
 */
int tz_nw_outer_cache_enable(void);
void tz_nw_outer_cache_disable(void);
void tz_nw_outer_cache_resume(void);

int tz_nw_cpu_boot(int cpu);
int tz_nw_cpu_idle(void);

#endif /* _TZ_NW_BOOT_H_ */
