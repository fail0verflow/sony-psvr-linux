/*
 * Berlin Power Management Routines
 *
 *  Author:	Jisheng Zhang <jszhang@marvell.com>
 *  Copyright:	Marvell International Ltd.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License.
 */
#include <linux/init.h>
#include <linux/suspend.h>

#include <asm/outercache.h>
#include <asm/suspend.h>
#include <asm/system_misc.h>

#include "tz_nw_fastcall.h"

static int berlin_finish_suspend(unsigned long val)
{
	return berlin_tz_cpu_suspend();
}

/*
 * task freezed, deviced stopped
 */
extern void berlin_smp_init(void);
static int berlin_pm_enter(suspend_state_t state)
{
	if (state != PM_SUSPEND_MEM)
		return -EINVAL;

	outer_disable();
	cpu_suspend(0, berlin_finish_suspend);
	/* come back */
	outer_resume();
#ifdef CONFIG_SMP
	berlin_smp_init();
#endif
	printk("Welcome back\n");
	return 0;
}

static struct platform_suspend_ops berlin_pm_ops = {
	.valid = suspend_valid_only_mem,
	.enter = berlin_pm_enter,
};

static int __init berlin_pm_init(void)
{
	suspend_set_ops(&berlin_pm_ops);
	return 0;
}
late_initcall(berlin_pm_init);
