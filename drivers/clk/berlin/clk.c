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
#include <linux/clk-provider.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/slab.h>

#include "clk.h"

#define CLKEN		(1 << 0)
#define CLKPLLSEL_MASK	7
#define CLKPLLSEL_SHIFT	1
#define CLKPLLSWITCH	(1 << 4)
#define CLKSWITCH	(1 << 5)
#define CLKD3SWITCH	(1 << 6)
#define CLKSEL_MASK	7
#define CLKSEL_SHIFT	7

struct berlin_clk {
	struct clk_hw hw;
	void __iomem *base;
};

#define to_berlin_clk(hw)	container_of(hw, struct berlin_clk, hw)

static u8 clk_div[] = {1, 2, 4, 6, 8, 12, 1, 1};

static unsigned long berlin_clk_recalc_rate(struct clk_hw *hw,
					unsigned long parent_rate)
{
	u32 val, divider;
	struct berlin_clk *clk = to_berlin_clk(hw);

	val = readl_relaxed(clk->base);
	if (val & CLKD3SWITCH)
		divider = 3;
	else {
		if (val & CLKSWITCH) {
			val >>= CLKSEL_SHIFT;
			val &= CLKSEL_MASK;
			divider = clk_div[val];
		} else
			divider = 1;
	}

	return parent_rate / divider;
}

static u8 berlin_clk_get_parent(struct clk_hw *hw)
{
	u32 val;
	struct berlin_clk *clk = to_berlin_clk(hw);

	val = readl_relaxed(clk->base);
	if (val & CLKPLLSWITCH) {
		val >>= CLKPLLSEL_SHIFT;
		val &= CLKPLLSEL_MASK;
		return val;
	}

	return 0;
}

static int berlin_clk_enable(struct clk_hw *hw)
{
	u32 val;
	struct berlin_clk *clk = to_berlin_clk(hw);

	val = readl_relaxed(clk->base);
	val |= CLKEN;
	writel_relaxed(val, clk->base);

	return 0;
}

static void berlin_clk_disable(struct clk_hw *hw)
{
	u32 val;
	struct berlin_clk *clk = to_berlin_clk(hw);

	val = readl_relaxed(clk->base);
	val &= ~CLKEN;
	writel_relaxed(val, clk->base);
}

static int berlin_clk_is_enabled(struct clk_hw *hw)
{
	u32 val;
	struct berlin_clk *clk = to_berlin_clk(hw);

	val = readl_relaxed(clk->base);
	val &= CLKEN;

	return val ? 1 : 0;
}

static const struct clk_ops berlin_clk_ops = {
	.recalc_rate	= berlin_clk_recalc_rate,
	.get_parent	= berlin_clk_get_parent,
	.enable		= berlin_clk_enable,
	.disable	= berlin_clk_disable,
	.is_enabled	= berlin_clk_is_enabled,
};

static void __init berlin_clk_setup(struct device_node *np)
{
	struct clk_init_data init;
	struct berlin_clk *bclk;
	struct clk *clk;
	const char *parent_names[5];
	int i, ret;

	bclk = kzalloc(sizeof(*bclk), GFP_KERNEL);
	if (WARN_ON(!bclk))
		return;

	bclk->base = of_iomap(np, 0);
	if (WARN_ON(!bclk->base))
		return;

	if (of_property_read_bool(np, "ignore-unused"))
		init.flags = CLK_IGNORE_UNUSED;
	else
		init.flags = 0;
	init.name = np->name;
	init.ops = &berlin_clk_ops;
	for (i = 0; i < ARRAY_SIZE(parent_names); i++) {
		parent_names[i] = of_clk_get_parent_name(np, i);
		if (!parent_names[i])
			break;
	}
	init.parent_names = parent_names;
	init.num_parents = i;

	bclk->hw.init = &init;

	clk = clk_register(NULL, &bclk->hw);
	if (WARN_ON(IS_ERR(clk)))
		return;

	ret = of_clk_add_provider(np, of_clk_src_simple_get, clk);
	if (WARN_ON(ret))
		return;
}
CLK_OF_DECLARE(berlin_clk, "marvell,berlin-clk", berlin_clk_setup);
