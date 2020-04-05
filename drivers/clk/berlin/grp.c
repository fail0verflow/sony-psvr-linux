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

#define CLKPLLSEL_MASK	7
#define CLKSEL_MASK	7

static u8 clk_div[] = {1, 2, 4, 6, 8, 12, 1, 1};

struct berlin_grpctl {
	struct clk **clks;
	void __iomem *switch_base;
	void __iomem *select_base;
	void __iomem *enable_base;
	spinlock_t *lock;
	struct clk_onecell_data	onecell_data;
};

struct berlin_grpclk {
	struct clk_hw hw;
	struct berlin_grpctl *ctl;
	int switch_shift;
	int select_shift;
	int enable_shift;
};

#define to_berlin_grpclk(hw)	container_of(hw, struct berlin_grpclk, hw)

static u8 berlin_grpclk_get_parent(struct clk_hw *hw)
{
	u32 val;
	struct berlin_grpclk *clk = to_berlin_grpclk(hw);
	struct berlin_grpctl *ctl = clk->ctl;
	void __iomem *base = ctl->switch_base;

	base += clk->switch_shift / 32 * 4;
	val = readl_relaxed(base);
	if (val & (1 << clk->switch_shift)) {
		base = ctl->select_base;
		base += clk->select_shift / 32 * 4;
		val = readl_relaxed(base);
		val >>= clk->select_shift;
		val &= CLKPLLSEL_MASK;
		return val;
	}

	return 0;
}

static unsigned long berlin_grpclk_recalc_rate(struct clk_hw *hw,
					unsigned long parent_rate)
{
	u32 val, divider;
	struct berlin_grpclk *clk = to_berlin_grpclk(hw);
	struct berlin_grpctl *ctl = clk->ctl;
	void __iomem *base = ctl->switch_base;
	int shift;

	shift = clk->switch_shift + 2;
	base += shift / 32 * 4;
	shift = shift - shift / 32 * 32;
	val = readl_relaxed(base);
	if (val & (1 << shift))
		divider = 3;
	else {
		if (unlikely(base != ctl->switch_base))
			val = readl_relaxed(ctl->switch_base);

		shift = clk->switch_shift + 1;
		shift = shift - shift / 32 * 32;
		if (val & (1 << shift)) {
			base = ctl->select_base;
			shift = clk->select_shift + 3;
			base += shift / 32 * 4;
			shift = shift - shift / 32 * 32;
			val = readl_relaxed(base);
			val >>= shift;
			val &= CLKSEL_MASK;
			divider = clk_div[val];
		} else
			divider = 1;
	}

	return parent_rate / divider;
}

static int berlin_grpclk_enable(struct clk_hw *hw)
{
	u32 val;
	unsigned long flags;
	struct berlin_grpclk *clk = to_berlin_grpclk(hw);
	struct berlin_grpctl *ctl = clk->ctl;

	spin_lock_irqsave(ctl->lock, flags);
	val = readl_relaxed(ctl->enable_base);
	val |= BIT(clk->enable_shift);
	writel_relaxed(val, ctl->enable_base);
	spin_unlock_irqrestore(ctl->lock, flags);

	return 0;
}

static void berlin_grpclk_disable(struct clk_hw *hw)
{
	u32 val;
	unsigned long flags;
	struct berlin_grpclk *clk = to_berlin_grpclk(hw);
	struct berlin_grpctl *ctl = clk->ctl;

	spin_lock_irqsave(ctl->lock, flags);
	val = readl_relaxed(ctl->enable_base);
	val &= ~BIT(clk->enable_shift);
	writel_relaxed(val, ctl->enable_base);
	spin_unlock_irqrestore(ctl->lock, flags);
}

static int berlin_grpclk_is_enabled(struct clk_hw *hw)
{
	u32 val;
	struct berlin_grpclk *clk = to_berlin_grpclk(hw);
	struct berlin_grpctl *ctl = clk->ctl;

	val = readl_relaxed(ctl->enable_base);
	val &= BIT(clk->enable_shift);
	return val ? 1 : 0;
}

static const struct clk_ops berlin_grpclk_ops = {
	.recalc_rate	= berlin_grpclk_recalc_rate,
	.get_parent	= berlin_grpclk_get_parent,
	.enable		= berlin_grpclk_enable,
	.disable	= berlin_grpclk_disable,
	.is_enabled	= berlin_grpclk_is_enabled,
};

static struct clk * __init berlin_grpclk_register(const char *name,
		const char **parent_names, int num_parents,
		int switch_shift, int select_shift,
		int enable_shift, unsigned long flags,
		struct berlin_grpctl *ctl)
{
	struct berlin_grpclk *grpclk;
	struct clk *clk;
	struct clk_init_data init;

	/* allocate the gate */
	grpclk = kzalloc(sizeof(*grpclk), GFP_KERNEL);
	if (!grpclk) {
		pr_err("%s: could not allocate berlin grpclk\n", __func__);
		return ERR_PTR(-ENOMEM);
	}

	init.flags = flags;
	init.name = name;
	init.ops = &berlin_grpclk_ops;
	init.parent_names = parent_names;
	init.num_parents = num_parents;

	grpclk->ctl = ctl;
	grpclk->switch_shift = switch_shift;
	grpclk->select_shift = select_shift;
	grpclk->enable_shift = enable_shift;
	grpclk->hw.init = &init;

	clk = clk_register(NULL, &grpclk->hw);

	if (IS_ERR(clk))
		kfree(grpclk);

	return clk;
}

void __init berlin_grpclk_setup(struct device_node *np,
				const struct grpclk_desc *desc,
				spinlock_t *lock)
{
	struct berlin_grpctl *ctl;
	const char *parent_names[5];
	int i, n, ret;

	ctl = kzalloc(sizeof(*ctl), GFP_KERNEL);
	if (WARN_ON(!ctl))
		return;

	ctl->switch_base = of_iomap(np, 0);
	if (WARN_ON(!ctl->switch_base))
		return;

	ctl->select_base = of_iomap(np, 1);
	if (WARN_ON(!ctl->select_base))
		return;

	ctl->enable_base = of_iomap(np, 2);
	if (WARN_ON(!ctl->enable_base))
		return;

	/* Count, allocate, and register clock gates */
	for (n = 0; desc[n].name;)
		n++;

	ctl->clks = kzalloc(n * sizeof(struct clk *), GFP_KERNEL);
	if (WARN_ON(!ctl->clks))
		return;

	for (i = 0; i < ARRAY_SIZE(parent_names); i++)
		parent_names[i] = of_clk_get_parent_name(np, i);

	for (i = 0; i < n; i++) {
		ctl->clks[i] = berlin_grpclk_register(desc[i].name,
				parent_names, ARRAY_SIZE(parent_names),
				desc[i].switch_shift, desc[i].select_shift,
				desc[i].enable_shift, desc[i].flags, ctl);
		WARN_ON(IS_ERR(ctl->clks[i]));
	}

	ctl->onecell_data.clks = ctl->clks;
	ctl->onecell_data.clk_num = i;
	ctl->lock = lock;

	ret = of_clk_add_provider(np, of_clk_src_onecell_get,
				  &ctl->onecell_data);
	if (WARN_ON(ret))
		return;
}
