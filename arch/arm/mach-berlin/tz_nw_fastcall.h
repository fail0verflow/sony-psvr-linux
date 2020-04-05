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

#ifndef _TZ_NW_FASTCALL_H_
#define _TZ_NW_FASTCALL_H_

/*
 * Outer Cache API
 */
int berlin_tz_outer_cache_enable(void);
void berlin_tz_outer_cache_disable(void);
void berlin_tz_outer_cache_resume(void);

/*
 * CPU API (Going to be Deprecated)
 */
int berlin_tz_cpu_suspend(void);

#endif /* _TZ_NW_FASTCALL_H_ */
