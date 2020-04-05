/*
 * TrustZone Support Interface
 *
 * Author: Qingwei Huang <huangqw@mavell.com>
 *
 * Copyright (C) 2013 Marvell Ltd.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */
#include <linux/kernel.h>
#include <linux/io.h>
#include <linux/init.h>
#include <linux/of.h>
#include <asm/firmware.h>
#include "tz_nw_fastcall.h"

static const struct firmware_ops berlin_firmware_ops = {
	.l2x0_init = berlin_tz_outer_cache_enable,
	.do_idle = berlin_tz_cpu_suspend,
};

void __init berlin_firmware_init(void)
{
	struct device_node *nd;

	nd = of_find_compatible_node(NULL, NULL,
					"berlin,secure-firmware");
	if (!nd)
		return;

	pr_err("Running under secure firmware.\n");
	register_firmware_ops(&berlin_firmware_ops);

	of_node_put(nd);
}
