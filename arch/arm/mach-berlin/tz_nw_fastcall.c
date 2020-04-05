/*
 *  TrustZone fastcall interfaces for outer cache and minimal PSCI support.
 *
 *  Author:	Yongsen Chen <chenys@marvell.com>
 *  Copyright:	Marvell International Ltd.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 */

#include <linux/types.h>

#include <asm/compiler.h>
#include <asm/errno.h>
#include <asm/opcodes-sec.h>
#include <asm/bug.h>

/* unknown function ID */
#define SMC_FUNC_ID_UNKNOWN		(0xffffffff)

static noinline int __invoke_smc(u32 function_id, u32 arg0, u32 arg1,
					 u32 arg2)
{
	register u32 r0 asm("r0") = function_id;
	register u32 r1 asm("r1") = arg0;
	register u32 r2 asm("r2") = arg1;
	register u32 r3 asm("r3") = arg2;

	asm volatile(
			__asmeq("%0", "r0")
			__asmeq("%1", "r1")
			__asmeq("%2", "r2")
			__asmeq("%3", "r3")
			"dsb	@ avoid data abort to bother tz\n"
			__SMC(0)
		: "+r" (r0)
		: "r" (r1), "r" (r2), "r" (r3));

	return r0;
}

/*
 * Outer Cache control
 */

/* 0xbf000030 - 0xbf000040: outer cache */
/* param: none; return: error code */
#define SMC_FUNC_TOS_OUTER_CACHE_ENABLE		(0xbf000030)
/* param: none; return: error code */
#define SMC_FUNC_TOS_OUTER_CACHE_DISABLE	(0xbf000031)
/* param: none; return: error code */
#define SMC_FUNC_TOS_OUTER_CACHE_RESUME		(0xbf000032)

int berlin_tz_outer_cache_enable(void)
{
	int err = __invoke_smc(SMC_FUNC_TOS_OUTER_CACHE_ENABLE, 0, 0, 0);

	if (err == SMC_FUNC_ID_UNKNOWN)
		return -EOPNOTSUPP;

	BUG_ON(err != 0);
	return 0;
}

void berlin_tz_outer_cache_disable(void)
{
	int err = __invoke_smc(SMC_FUNC_TOS_OUTER_CACHE_DISABLE, 0, 0, 0);
	BUG_ON(err != 0);
}

void berlin_tz_outer_cache_resume(void)
{
	int err = __invoke_smc(SMC_FUNC_TOS_OUTER_CACHE_RESUME, 0, 0, 0);
	BUG_ON(err != 0);
}

/*
 * PSCI: only minimal set is supported for deprecated firmware approach.
 *
 * this piece of code will be removed after we support PSCI in linux.
 */

/* 0x84000000-0x8400001f: PSCI */
#define SMC_FUNC_PSCI_CPU_SUSPEND	(0x84000001)
#define SMC_FUNC_PSCI_CPU_OFF		(0x84000002)
#define SMC_FUNC_PSCI_CPU_ON		(0x84000003)

#define PSCI_RET_SUCCESS		0
#define PSCI_RET_EOPNOTSUPP		-1
#define PSCI_RET_EINVAL			-2
#define PSCI_RET_EPERM			-3

static int psci_to_linux_errno(int errno)
{
	switch (errno) {
	case PSCI_RET_SUCCESS:
		return 0;
	case PSCI_RET_EOPNOTSUPP:
		return -EOPNOTSUPP;
	case PSCI_RET_EINVAL:
		return -EINVAL;
	case PSCI_RET_EPERM:
		return -EPERM;
	};

	return -EINVAL;
}

int berlin_tz_cpu_suspend(void)
{
	int err = __invoke_smc(SMC_FUNC_PSCI_CPU_SUSPEND, 0, 0, 0);
	return psci_to_linux_errno(err);
}
