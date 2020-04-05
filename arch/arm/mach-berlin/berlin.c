/*
 *  MARVELL BERLIN Flattened Device Tree enabled machine
 *
 *  Author:	Jisheng Zhang <jszhang@marvell.com>
 *  Copyright (c) 2013 Marvell Technology Group Ltd.
 *		http://www.marvell.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/init.h>
#include <linux/clk-provider.h>
#include <linux/clocksource.h>
#include <linux/of_platform.h>

#include <asm/mach/map.h>
#include <asm/mach/arch.h>
#include <asm/hardware/cache-l2x0.h>
#include <asm/firmware.h>

#include "common.h"
#include "tz_nw_fastcall.h"

static struct map_desc berlin_io_desc[] __initdata = {
	{
		.virtual = 0xF7000000,
		.pfn = __phys_to_pfn(0xF7000000),
		.length = 0x00CC0000,
		.type = MT_DEVICE
	},
	{
		.virtual = 0xF7CD0000,
		.pfn = __phys_to_pfn(0xF7CD0000),
		.length = 0x00330000,
		.type = MT_DEVICE
	},
};

static void __init berlin_map_io(void)
{
	iotable_init(berlin_io_desc, ARRAY_SIZE(berlin_io_desc));
}

#ifdef CONFIG_CACHE_L2X0
static void __init berlin_l2x0_init(void)
{
	int ret;

	ret = call_firmware_op(l2x0_init);
	if (ret != -ECOMM)
		ret = l2x0_of_init(0x70c00000, 0xfeffffff);
	if (!ret) {
		outer_cache.disable = berlin_tz_outer_cache_disable;
		outer_cache.resume = berlin_tz_outer_cache_resume;
	}
}
#else
static void __init berlin_l2x0_init(void) {}
#endif

static void __init berlin_init_early(void)
{
	berlin_firmware_init();
	berlin_l2x0_init();
}

static void __init berlin_timer_init(void)
{
	of_clk_init(NULL);
	clocksource_of_init();
}

static void __init berlin_init_late(void)
{
	platform_device_register_simple("cpufreq-cpu0", -1, NULL, 0);
}

static char const *berlin_dt_compat[] __initdata = {
	"marvell,berlin",
	NULL
};

DT_MACHINE_START(BERLIN_DT, "Marvell Berlin")
	/* Maintainer: Jisheng Zhang<jszhang@marvell.com> */
	.smp		= smp_ops(berlin_smp_ops),
	.map_io		= berlin_map_io,
	.init_early	= berlin_init_early,
	.init_time	= berlin_timer_init,
	.dt_compat	= berlin_dt_compat,
	.init_late	= berlin_init_late,
MACHINE_END
