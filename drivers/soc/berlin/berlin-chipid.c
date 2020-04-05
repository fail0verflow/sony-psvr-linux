/*
 *  MARVELL BERLIN CHIP ID support
 *
 *  Author:	Jisheng Zhang <jszhang@marvell.com>
 *  Copyright (c) 2014 Marvell Technology Group Ltd.
 *		http://www.marvell.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/err.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_fdt.h>
#include <linux/slab.h>
#include <linux/sys_soc.h>

static const struct of_device_id chipid_of_match[] __initconst = {
	{ .compatible = "marvell,berlin-chipid", },
	{},
};

static const char * __init berlin_id_to_family(u32 id)
{
	const char *soc_family;

	switch (id) {
	case 0x3005:
		soc_family = "Marvell Berlin BG2CD";
		break;
	case 0x3006:
		soc_family = "Marvell Berlin BG2CDP";
		break;
	case 0x3012:
		soc_family = "Marvell Berlin BG4DTV";
		break;
	case 0x3100:
		soc_family = "Marvell Berlin BG2";
		break;
	case 0x3108:
		soc_family = "Marvell Berlin BG2CT";
		break;
	case 0x3114:
		soc_family = "Marvell Berlin BG2Q";
		break;
	case 0x3215:
		soc_family = "Marvell Berlin BG2Q4K";
		break;
	default:
		soc_family = "<unknown>";
		break;
	}
	return soc_family;
}

static int __init berlin_chipid_init(void)
{
	u64 chipid;
	u32 val;
	unsigned long dt_root;
	struct soc_device_attribute *soc_dev_attr;
	struct soc_device *soc_dev;
	struct device_node *np;
	int ret = 0;
	void __iomem *chipid_base = NULL;
	void __iomem *id_base = NULL;

	np = of_find_matching_node(NULL, chipid_of_match);
	if (!np)
		return -ENODEV;

	id_base = of_iomap(np, 0);
	if (!id_base) {
		ret = -ENOMEM;
		goto out_put_node;
	}

	chipid_base = of_iomap(np, 1);
	if (!chipid_base) {
		ret = -ENOMEM;
		goto out_unmap_id;
	}

	soc_dev_attr = kzalloc(sizeof(*soc_dev_attr), GFP_KERNEL);
	if (!soc_dev_attr) {
		ret = -ENOMEM;
		goto out_unmap;
	}

	dt_root = of_get_flat_dt_root();
	soc_dev_attr->machine = of_get_flat_dt_prop(dt_root, "model", NULL);
	if (!soc_dev_attr->machine)
		soc_dev_attr->machine = "<unknown>";

	val = readl_relaxed(id_base);
	val = (val >> 12) & 0xffff;
	soc_dev_attr->family = berlin_id_to_family(val);

	val = readl_relaxed(id_base + 4);
	soc_dev_attr->revision = kasprintf(GFP_KERNEL, "%X", val);

	chipid = readl_relaxed(chipid_base + 4);
	chipid <<= 32;
	val = readl_relaxed(chipid_base);
	chipid |= val;
	chipid = cpu_to_be64(chipid);
	soc_dev_attr->soc_id = kasprintf(GFP_KERNEL, "%llX", chipid);

	soc_dev = soc_device_register(soc_dev_attr);
	if (IS_ERR(soc_dev)) {
		kfree(soc_dev_attr);
		ret = PTR_ERR(soc_dev);
	}

out_unmap:
	iounmap(chipid_base);
out_unmap_id:
	iounmap(id_base);
out_put_node:
	of_node_put(np);
	return ret;
}
arch_initcall(berlin_chipid_init);
