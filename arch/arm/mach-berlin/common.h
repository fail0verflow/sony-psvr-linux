/*
 *  Author:	Jisheng Zhang <jszhang@marvell.com>
 *  Copyright (c) 2013 Marvell Technology Group Ltd.
 *		http://www.marvell.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#ifndef __BERLIN_COMMON_H
#define __BERLIN_COMMON_H

extern struct smp_operations berlin_smp_ops;

extern void berlin_cpu_die(unsigned int cpu);
extern void __init berlin_firmware_init(void);

#endif
