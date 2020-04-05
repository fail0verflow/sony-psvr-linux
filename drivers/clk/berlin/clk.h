/*
 * Copyright (c) 2013 Marvell Technology Group Ltd.
 *
 * Author: Jisheng Zhang <jszhang@marvell.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#ifndef __BERLIN_CLK_H
#define __BERLIN_CLK_H

#include <linux/clk-provider.h>

struct berlin_pll {
	struct clk_hw	hw;
	void __iomem	*ctrl;
	void __iomem	*bypass;
	u8		bypass_shift;
};

#define to_berlin_pll(hw)	container_of(hw, struct berlin_pll, hw)

struct berlin_avpllch {
	struct clk_hw	hw;
	void __iomem	*base;
	u8		which;
};

#define to_berlin_avpllch(hw)	container_of(hw, struct berlin_avpllch, hw);

struct gateclk_desc {
	const char	*name;
	const char	*parent_name;
	u8		bit_idx;
	unsigned long	flags;
};

struct grpclk_desc {
	const char	*name;
	int		switch_shift;
	int		select_shift;
	int		enable_shift;
	unsigned long	flags;
};

struct berlin_avplldata {
	u32			offset;
	u32			fbdiv;
	u32			offset_mask;
	u32			fbdiv_mask;
	u8			offset_shift;
	u8			fbdiv_shift;
	const struct clk_ops	*ops;
};

extern void __init berlin_gateclk_setup(struct device_node *np,
				const struct gateclk_desc *desc,
				spinlock_t *lock);

extern void __init berlin_grpclk_setup(struct device_node *np,
				const struct grpclk_desc *desc,
				spinlock_t *lock);

extern void __init berlin_pll_setup(struct device_node *np,
				const struct clk_ops *ops);

extern void __init berlin_avpll_setup(struct device_node *np,
				struct berlin_avplldata *data);
#endif /* BERLIN_CLK_H */
