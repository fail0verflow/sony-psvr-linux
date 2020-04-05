/*
 * Copyright (c) 2014 Marvell Technology Group Ltd.
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

struct berlin_gatectl {
	struct clk **clks;
	void __iomem *base;
	struct clk_onecell_data	onecell_data;
};

void __init berlin_gateclk_setup(struct device_node *np,
				const struct gateclk_desc *desc,
				spinlock_t *lock)
{
	struct berlin_gatectl *ctl;
	int i, n, ret;

	ctl = kzalloc(sizeof(*ctl), GFP_KERNEL);
	if (WARN_ON(!ctl))
		return;

	ctl->base = of_iomap(np, 0);
	if (WARN_ON(!ctl->base))
		return;

	/* Count, allocate, and register clock gates */
	for (n = 0; desc[n].name;)
		n++;

	ctl->clks = kzalloc(n * sizeof(struct clk *), GFP_KERNEL);
	if (WARN_ON(!ctl->clks))
		return;

	for (i = 0; i < n; i++) {
		ctl->clks[i] = clk_register_gate(NULL, desc[i].name,
				desc[i].parent_name, desc[i].flags, ctl->base,
				desc[i].bit_idx, 0, lock);
		WARN_ON(IS_ERR(ctl->clks[i]));
	}

	ctl->onecell_data.clks = ctl->clks;
	ctl->onecell_data.clk_num = i;

	ret = of_clk_add_provider(np, of_clk_src_onecell_get,
				  &ctl->onecell_data);
	if (WARN_ON(ret))
		return;
}
