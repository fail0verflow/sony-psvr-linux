/*
 * Synopsys DW APB ICTL irqchip driver.
 *
 * Sebastian Hesselbarth <sebastian.hesselbarth@gmail.com>
 *
 * based on GPL'ed 2.6 kernel sources
 *  (c) Marvell International Ltd.
 *
 * This file is licensed under the terms of the GNU General Public
 * License version 2.  This program is licensed "as is" without any
 * warranty of any kind, whether express or implied.
 */

#include <linux/io.h>
#include <linux/irq.h>
#include <linux/irqchip/chained_irq.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>

#include "irqchip.h"

#define APB_INT_ENABLE_L	0x00
#define APB_INT_ENABLE_H	0x04
#define APB_INT_MASK_L		0x08
#define APB_INT_MASK_H		0x0c
#define APB_INT_FINALSTATUS_L	0x30
#define APB_INT_FINALSTATUS_H	0x34

static inline struct irq_chip_regs *cur_regs(struct irq_data *d)
{
	return &container_of(d->chip, struct irq_chip_type, chip)->regs;
}

static void dw_apb_ictl_handler(unsigned int irq, struct irq_desc *desc)
{
	struct irq_chip *chip = irq_get_chip(irq);
	struct irq_chip_generic *gc = irq_get_handler_data(irq);
	struct irq_domain *d = gc->private;
	u32 stat;
	int n;

	chained_irq_enter(chip, desc);

	for (n = 0; n < gc->num_ct; n++) {
		stat = readl_relaxed(gc->reg_base +
				     APB_INT_FINALSTATUS_L + 4 * n);
		while (stat) {
			u32 hwirq = ffs(stat) - 1;
			generic_handle_irq(irq_find_mapping(d,
					   hwirq + 32 * n));
			stat &= ~(1 << hwirq);
		}
	}

	chained_irq_exit(chip, desc);
}

#ifdef CONFIG_PM
static void dw_apb_ictl_suspend(struct irq_data *d)
{
	struct irq_chip_generic *gc = irq_data_get_irq_chip_data(d);

	irq_gc_lock(gc);
	writel_relaxed(gc->wake_active, gc->reg_base + cur_regs(d)->mask);
	irq_gc_unlock(gc);
}

static void dw_apb_ictl_resume(struct irq_data *d)
{
	struct irq_chip_generic *gc = irq_data_get_irq_chip_data(d);

	irq_gc_lock(gc);
	writel_relaxed(~0, gc->reg_base + cur_regs(d)->enable);
	writel_relaxed(gc->mask_cache, gc->reg_base + cur_regs(d)->mask);
	irq_gc_unlock(gc);
}
#else
#define dw_apb_ictl_suspend	NULL
#define dw_apb_ictl_resume	NULL
#endif /* CONFIG_PM */

static int __init dw_apb_ictl_init(struct device_node *np,
				   struct device_node *parent)
{
	struct resource r;
	struct irq_domain *domain;
	struct irq_chip_generic *gc;
	void __iomem *iobase;
	int ret, nrirqs, irq, irqbase;
	u32 reg;

	/* Map the parent interrupt for the chained handler */
	irq = irq_of_parse_and_map(np, 0);
	if (irq <= 0) {
		pr_err("%s: unable to parse irq\n", np->full_name);
		return -EINVAL;
	}

	ret = of_address_to_resource(np, 0, &r);
	if (ret) {
		pr_err("%s: unable to get resource\n", np->full_name);
		return ret;
	}

	if (!request_mem_region(r.start, resource_size(&r), np->full_name)) {
		pr_err("%s: unable to request mem region\n", np->full_name);
		return -ENOMEM;
	}

	iobase = ioremap(r.start, resource_size(&r));
	if (!iobase) {
		pr_err("%s: unable to map resource\n", np->full_name);
		ret = -ENOMEM;
		goto err_release;
	}

	/*
	 * DW IP can be configured to allow 2-64 irqs. We can determine
	 * the number of irqs supported by writing into enable register
	 * and look for bits not set, as corresponding flip-flops will
	 * have been removed by sythesis tool.
	 */

	/* mask and enable all interrupts */
	writel(~0, iobase + APB_INT_MASK_L);
	writel(~0, iobase + APB_INT_MASK_H);
	writel(~0, iobase + APB_INT_ENABLE_L);
	writel(~0, iobase + APB_INT_ENABLE_H);

	reg = readl(iobase + APB_INT_ENABLE_H);
	if (reg)
		nrirqs = 32 + fls(reg);
	else
		nrirqs = fls(readl(iobase + APB_INT_ENABLE_L));

	irqbase = irq_alloc_descs(-1, 0, nrirqs, -1);
	domain = irq_domain_add_legacy(np, nrirqs, irqbase, 0,
				       &irq_domain_simple_ops, NULL);
	if (!domain) {
		pr_err("%s: unable to add irq domain\n", np->full_name);
		ret = -ENOMEM;
		goto err_unmap;
	}

	gc = irq_alloc_generic_chip(np->name, (nrirqs > 32) ? 2 : 1,
				    irqbase, iobase, handle_level_irq);
	if (!gc) {
		pr_err("%s: unable to alloc gc\n", np->full_name);
		ret = -ENOMEM;
		goto err_unmap;
	}

	gc->private = domain;

	gc->chip_types[0].regs.mask = APB_INT_MASK_L;
	gc->chip_types[0].regs.enable = APB_INT_ENABLE_L;
	gc->chip_types[0].chip.irq_mask = irq_gc_mask_set_bit;
	gc->chip_types[0].chip.irq_unmask = irq_gc_mask_clr_bit;
	gc->chip_types[0].chip.irq_suspend = dw_apb_ictl_suspend;
	gc->chip_types[0].chip.irq_resume = dw_apb_ictl_resume;

	if (nrirqs > 32) {
		gc->chip_types[1].regs.mask = APB_INT_MASK_H;
		gc->chip_types[1].regs.enable = APB_INT_ENABLE_H;
		gc->chip_types[1].chip.irq_mask = irq_gc_mask_set_bit;
		gc->chip_types[1].chip.irq_unmask = irq_gc_mask_clr_bit;
		gc->chip_types[1].chip.irq_suspend = dw_apb_ictl_suspend;
		gc->chip_types[1].chip.irq_resume = dw_apb_ictl_resume;
	}

	irq_setup_generic_chip(gc, IRQ_MSK(32), IRQ_GC_INIT_MASK_CACHE,
			       IRQ_NOREQUEST | IRQ_NOPROBE | IRQ_NOAUTOEN, 0);
	irq_set_handler_data(irq, gc);
	irq_set_chained_handler(irq, dw_apb_ictl_handler);

	return 0;

err_unmap:
	iounmap(iobase);
err_release:
	release_mem_region(r.start, resource_size(&r));
	return ret;
}
IRQCHIP_DECLARE(dw_apb_ictl,
		"snps,dw-apb-ictl", dw_apb_ictl_init);
