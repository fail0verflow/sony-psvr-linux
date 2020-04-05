/*
 *  PCIe driver for Marvell Berlin SoCs
 *
 *  Author:	Jisheng Zhang <jszhang@marvell.com>
 *  Copyright (c) 2014 Marvell Technology Group Ltd.
 *		http://www.marvell.com
 *
 * This file is licensed under the terms of the GNU General Public
 * License version 2.  This program is licensed "as is" without any
 * warranty of any kind, whether express or implied.
 */

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/debugfs.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mbus.h>
#include <linux/msi.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/of_gpio.h>
#include <linux/of_pci.h>
#include <linux/of_platform.h>
#include <linux/pci.h>
#include <linux/platform_device.h>
#include <linux/reset.h>
#include <linux/slab.h>

#define INT_PCI_MSI_NR 32

#define PF0_CMD_STS		0x0004
#define PF0_BAR0		0x0010
#define PIO_CTRL		0x4000
#define PIO_STAT		0x4004
#define PIO_ADDR_LS		0x4008
#define PIO_ADDR_MS		0x400C
#define PIO_WR_DATA		0x4010
#define PIO_WR_DATA_STRB	0x4014
#define PIO_RD_DATA		0x4018
#define PIO_START		0x401C
#define PCIE_CTRL0		0x4800
#define PCIE_FLUSH		0x480C
#define PCIE_ISR0		0x4840
#define PCIE_ISRM0		0x4844
#define LMI			0x6000
#define MSI_INTR_RX		0xA000
#define MSI_INTR_STATUS		0xA004
#define MSI_INTR_MASK		0xA008
#define PCIE_MAC_CTRL		0xA00C

#define PCIE_CONF_BUS(b)	((b) << 20)
#define PCIE_CONF_DEV(d)	((d) << 15)
#define PCIE_CONF_FUNC(f)	((f) << 12)
#define PCIE_CONF_REG(r)	((r) & 0xffc)

struct berlin_msi {
	struct msi_chip chip;
	DECLARE_BITMAP(used, INT_PCI_MSI_NR);
	struct irq_domain *domain;
	struct mutex lock;
	int irq;
};

struct berlin_pcie {
	void __iomem *base;
	struct device *dev;
	struct resource io;
	struct resource mem;
	struct resource busn;
	phys_addr_t mem_bus_addr;
	struct clk *clk;
	int root_bus_nr;
	int reset_gpio;
	int power_gpio;
	int irq;
	struct berlin_msi msi;
	struct reset_control *rc_rst;
	struct dentry *debugfs_root;
	struct debugfs_blob_wrapper reg_blob;
};

static inline struct berlin_msi *to_berlin_msi(struct msi_chip *chip)
{
	return container_of(chip, struct berlin_msi, chip);
}

static inline void berlin_writel(struct berlin_pcie *pcie, u32 val, u32 reg)
{
	writel(val, pcie->base + reg);
}

static inline u32 berlin_readl(struct berlin_pcie *pcie, u32 reg)
{
	return readl(pcie->base + reg);
}

static inline void pcie_phy_writel(u32 val, u32 reg)
{
	writel(reg + 0x200, (volatile void __iomem *)0xF7E90178);
	writel(val, (volatile void __iomem *)0xF7E9017C);
}

static inline u32 pcie_phy_readl(u32 reg)
{
	writel(reg + 0x200, (volatile void __iomem *)0xF7E90178);
	return readl((volatile void __iomem *)0xF7E9017C);
}

static int berlin_pcie_link_up(struct berlin_pcie *pcie)
{
	int count = 1000;

	while (1) {
		u32 rc;

		rc = readl(pcie->base + LMI);
		if (rc & 0x1)
			return 1;
		if (!count--)
			break;
		udelay(200);
	}

	return 0;
}

static inline struct berlin_pcie *sys_to_pcie(struct pci_sys_data *sys)
{
	return sys->private_data;
}

static int pcie_valid_config(struct berlin_pcie *pcie, int bus, int dev)
{
	/*
	 * Don't go out when trying to access nonexisting devices
	 * on the local bus.
	 */
	if (bus == pcie->root_bus_nr && dev >= 1)
		return 0;

	return 1;
}

static int berlin_pcie_hw_wr_conf(struct berlin_pcie *pcie, int bus,
				  u32 devfn, int where, int size, u32 val)
{
	u32 reg, strobe = 0xf;
	int r, count = 100000;

	reg = berlin_readl(pcie, PCIE_MAC_CTRL);
	if (reg & (1 << 2))
		berlin_writel(pcie, 0xB | (size << 11), PIO_CTRL);
	else
		berlin_writel(pcie, 0xA | (size << 11), PIO_CTRL);

	reg = (PCIE_CONF_BUS(bus) | PCIE_CONF_DEV(PCI_SLOT(devfn)) |
		PCIE_CONF_FUNC(PCI_FUNC(devfn)) | PCIE_CONF_REG(where));
	berlin_writel(pcie, reg, PIO_ADDR_LS);

	switch (size) {
	case SZ_1:
		r = where % 4;
		strobe = 1 << r;
		val <<= (r * 8);
		break;
	case SZ_2:
		r = where % 4;
		strobe = 3 << r;
		val <<= (r * 8);
		break;
	case SZ_4:
		strobe = 0xf;
		break;
	}

	berlin_writel(pcie, val, PIO_WR_DATA);
	berlin_writel(pcie, strobe, PIO_WR_DATA_STRB);
	berlin_writel(pcie, 1, PIO_START);

	for (;count > 0; --count) {
		reg = berlin_readl(pcie, PIO_START);
		if (!reg)
			break;
	}
	if (!count) {
		berlin_writel(pcie, 0, PIO_START);
		return PCIBIOS_SET_FAILED;
	}

	reg = berlin_readl(pcie, PIO_STAT);
	reg = (reg >> 7) & 7;
	if (reg)
		return PCIBIOS_DEVICE_NOT_FOUND;

	return PCIBIOS_SUCCESSFUL;
}

static int berlin_pcie_hw_rd_conf(struct berlin_pcie *pcie, int bus,
				  u32 devfn, int where, int size, u32 *val)
{
	u32 reg;
	int count = 100000;

	reg = berlin_readl(pcie, PCIE_MAC_CTRL);
	if (reg & (1 << 2))
		berlin_writel(pcie, 0x9 | (4 << 11), PIO_CTRL);
	else
		berlin_writel(pcie, 0x8 | (4 << 11), PIO_CTRL);

	reg = (PCIE_CONF_BUS(bus) | PCIE_CONF_DEV(PCI_SLOT(devfn)) |
		PCIE_CONF_FUNC(PCI_FUNC(devfn)) | PCIE_CONF_REG(where));
	berlin_writel(pcie, reg, PIO_ADDR_LS);

	berlin_writel(pcie, 0xf, PIO_WR_DATA_STRB);
	berlin_writel(pcie, 1, PIO_START);

	for (;count > 0; --count) {
		reg = berlin_readl(pcie, PIO_START);
		if (!reg)
			break;
	}
	if (!count) {
		berlin_writel(pcie, 0, PIO_START);
		return PCIBIOS_SET_FAILED;
	}

	reg = berlin_readl(pcie, PIO_STAT);
	reg = (reg >> 7) & 7;
	if (!reg)
		*val = berlin_readl(pcie, PIO_RD_DATA);
	else if (reg == 2)
		*val = 0xffff0001;
	else
		return PCIBIOS_DEVICE_NOT_FOUND;

	if (size == 1)
		*val = (*val >> (8 * (where & 3))) & 0xff;
	else if (size == 2)
		*val = (*val >> (8 * (where & 3))) & 0xffff;

	return PCIBIOS_SUCCESSFUL;
}

static int berlin_pcie_wr_conf(struct pci_bus *bus, u32 devfn, int where,
			       int size, u32 val)
{
	struct berlin_pcie *pcie = sys_to_pcie(bus->sysdata);

	if (pcie_valid_config(pcie, bus->number, PCI_SLOT(devfn)) == 0)
		return PCIBIOS_DEVICE_NOT_FOUND;

	return berlin_pcie_hw_wr_conf(pcie, bus->number, devfn,
					where, size, val);
}

static int berlin_pcie_rd_conf(struct pci_bus *bus, u32 devfn, int where,
			       int size, u32 *val)
{
	struct berlin_pcie *pcie = sys_to_pcie(bus->sysdata);

	if (pcie_valid_config(pcie, bus->number, PCI_SLOT(devfn)) == 0) {
		*val = 0xffffffff;
		return PCIBIOS_DEVICE_NOT_FOUND;
	}

	return berlin_pcie_hw_rd_conf(pcie, bus->number, devfn,
					where, size, val);
}

static struct pci_ops berlin_pcie_ops = {
	.read = berlin_pcie_rd_conf,
	.write = berlin_pcie_wr_conf,
};

static int berlin_pcie_setup(int nr, struct pci_sys_data *sys)
{
	struct berlin_pcie *pcie = sys_to_pcie(sys);

	pcie->root_bus_nr = sys->busnr;
	sys->mem_offset = pcie->mem.start - pcie->mem_bus_addr;
	pci_add_resource_offset(&sys->resources, &pcie->mem, sys->mem_offset);
	pci_add_resource(&sys->resources, &pcie->busn);

	return 1;
}

static struct pci_bus *berlin_pcie_scan_bus(int nr, struct pci_sys_data *sys)
{
	struct berlin_pcie *pcie = sys_to_pcie(sys);
	struct pci_bus *bus;

	bus = pci_create_root_bus(pcie->dev, sys->busnr,
				  &berlin_pcie_ops, sys, &sys->resources);
	if (!bus)
		return NULL;

	pci_scan_child_bus(bus);

	return bus;
}

static void berlin_pcie_add_bus(struct pci_bus *bus)
{
	if (IS_ENABLED(CONFIG_PCI_MSI)) {
		struct berlin_pcie *pcie = sys_to_pcie(bus->sysdata);
		bus->msi = &pcie->msi.chip;
	}
}

static irqreturn_t berlin_pcie_irq(int irq, void *data)
{
	u32 val;
	struct berlin_pcie *pcie = data;

	val = berlin_readl(pcie, PCIE_ISR0);
	berlin_writel(pcie, val, PCIE_ISR0);
	return IRQ_HANDLED;
}

static int berlin_msi_alloc(struct berlin_msi *chip)
{
	int msi;

	mutex_lock(&chip->lock);

	msi = find_first_zero_bit(chip->used, INT_PCI_MSI_NR);
	if (msi < INT_PCI_MSI_NR)
		set_bit(msi, chip->used);
	else
		msi = -ENOSPC;

	mutex_unlock(&chip->lock);

	return msi;
}

static void berlin_msi_free(struct berlin_msi *chip, unsigned long irq)
{
	struct device *dev = chip->chip.dev;

	mutex_lock(&chip->lock);

	if (!test_bit(irq, chip->used))
		dev_err(dev, "trying to free unused MSI#%lu\n", irq);
	else
		clear_bit(irq, chip->used);

	mutex_unlock(&chip->lock);
}

static irqreturn_t berlin_pcie_msi_irq(int irq, void *data)
{
	struct berlin_pcie *pcie = data;
	struct berlin_msi *msi = &pcie->msi;
	unsigned int processed = 0;
	unsigned long reg = berlin_readl(pcie, MSI_INTR_STATUS);

	while (reg) {
		unsigned int index = find_first_bit(&reg, 32);
		unsigned int irq;

		/* clear the interrupt */
		berlin_writel(pcie, 1 << index , MSI_INTR_STATUS);

		irq = irq_find_mapping(msi->domain, index);
		if (irq) {
			if (test_bit(index, msi->used))
				generic_handle_irq(irq);
			else
				dev_info(pcie->dev, "unhandled MSI\n");
		} else {
			/*
			 * that's weird who triggered this?
			 * just clear it
			 */
			dev_info(pcie->dev, "unexpected MSI\n");
		}

		/* see if there's any more pending register */
		reg = berlin_readl(pcie, MSI_INTR_STATUS);

		processed++;
	}

	return processed > 0 ? IRQ_HANDLED : IRQ_NONE;
}

static int berlin_msi_setup_irq(struct msi_chip *chip, struct pci_dev *pdev,
			       struct msi_desc *desc)
{
	struct berlin_msi *msi = to_berlin_msi(chip);
	struct berlin_pcie *pcie = container_of(msi, struct berlin_pcie, msi);
	struct msi_msg msg;
	unsigned int irq;
	int hwirq;

	hwirq = berlin_msi_alloc(msi);
	if (hwirq < 0)
		return hwirq;

	irq = irq_create_mapping(msi->domain, hwirq);
	if (!irq) {
		berlin_msi_free(msi, hwirq);
		return -EINVAL;
	}

	irq_set_msi_desc(irq, desc);

	msg.address_lo = (u32)(pcie->base + MSI_INTR_RX);
	/* 32 bit address only */
	msg.address_hi = 0;
	msg.data = PCI_FUNC(pdev->devfn) << 13;

	write_msi_msg(irq, &msg);

	return 0;
}

static void berlin_msi_teardown_irq(struct msi_chip *chip, unsigned int irq)
{
	struct berlin_msi *msi = to_berlin_msi(chip);
	struct irq_data *d = irq_get_irq_data(irq);
	unsigned long hwirq = d->hwirq;

	irq_dispose_mapping(irq);
	berlin_msi_free(msi, hwirq);
}

static int berlin_msi_check_device(struct msi_chip *chip, struct pci_dev *dev,
				   int nvec, int type)
{
	if (type == PCI_CAP_ID_MSI)
		return 0;
	return -EINVAL;
}

static struct irq_chip berlin_msi_irq_chip = {
	.name = "Berlin PCIe MSI",
	.irq_enable = unmask_msi_irq,
	.irq_disable = mask_msi_irq,
	.irq_mask = mask_msi_irq,
	.irq_unmask = unmask_msi_irq,
};

static int berlin_msi_map(struct irq_domain *domain, unsigned int irq,
			 irq_hw_number_t hwirq)
{
	irq_set_chip_and_handler(irq, &berlin_msi_irq_chip, handle_simple_irq);
	irq_set_chip_data(irq, domain->host_data);
	set_irq_flags(irq, IRQF_VALID);

	return 0;
}

static const struct irq_domain_ops msi_domain_ops = {
	.map = berlin_msi_map,
};

static int berlin_pcie_enable_msi(struct berlin_pcie *pcie)
{
	struct platform_device *pdev = to_platform_device(pcie->dev);
	struct berlin_msi *msi = &pcie->msi;
	int err;

	mutex_init(&msi->lock);

	msi->chip.dev = pcie->dev;
	msi->chip.setup_irq = berlin_msi_setup_irq;
	msi->chip.teardown_irq = berlin_msi_teardown_irq;
	msi->chip.check_device = berlin_msi_check_device;

	msi->domain = irq_domain_add_linear(pcie->dev->of_node, INT_PCI_MSI_NR,
					    &msi_domain_ops, &msi->chip);
	if (!msi->domain) {
		dev_err(&pdev->dev, "failed to create IRQ domain\n");
		return -ENOMEM;
	}

	err = platform_get_irq(pdev, 1);
	if (err < 0) {
		dev_err(&pdev->dev, "failed to get IRQ: %d\n", err);
		goto err;
	}

	msi->irq = err;

	err = devm_request_irq(&pdev->dev, msi->irq, berlin_pcie_msi_irq,
				IRQF_SHARED, berlin_msi_irq_chip.name, pcie);
	if (err < 0) {
		dev_err(&pdev->dev, "failed to request IRQ: %d\n", err);
		goto err;
	}

	/* unmask all the MSI interrupt */
	berlin_writel(pcie, 0x0, MSI_INTR_MASK);

	return 0;

err:
	irq_domain_remove(msi->domain);
	return err;
}

static int berlin_pcie_disable_msi(struct berlin_pcie *pcie)
{
	struct berlin_msi *msi = &pcie->msi;
	unsigned int i, irq;

	/* mask the MSI interrupt */
	berlin_writel(pcie, ~0x0, MSI_INTR_MASK);

	for (i = 0; i < INT_PCI_MSI_NR; i++) {
		irq = irq_find_mapping(msi->domain, i);
		if (irq > 0)
			irq_dispose_mapping(irq);
	}

	irq_domain_remove(msi->domain);

	return 0;
}

static int berlin_pcie_map_irq(const struct pci_dev *dev, u8 slot, u8 pin)
{
	struct of_irq oirq;
	int ret;

	ret = of_irq_map_pci(dev, &oirq);
	if (ret)
		return ret;

	return irq_create_of_mapping(oirq.controller, oirq.specifier,
				     oirq.size);
}

static inline int berlin_pcie_phy_ready(struct berlin_pcie *pcie)
{
	int count = 1000;

	for (; count > 0; --count) {
		u32 rc1, rc2;

		rc1 = pcie_phy_readl(1);
		rc2 = pcie_phy_readl(2);
		if((rc1 & (1 << 8)) && (rc2 & (1 << 14)))
			return 1;
		udelay(500);
	}

	return 0;
}

static int berlin_pcie_enable_controller(struct berlin_pcie *pcie)
{
	u32 val;

	reset_control_assert(pcie->rc_rst);
	reset_control_deassert(pcie->rc_rst);

	if (gpio_is_valid(pcie->power_gpio)) {
		/* power up EP */
		gpio_direction_output(pcie->power_gpio, 0);
		udelay(100);
		gpio_direction_output(pcie->power_gpio, 1);
	}

	/* Reset PCIE RefClkBuffer */
	writel(0x8AC, (volatile void __iomem *)0xF7EA048C);
	writel(0x8AD, (volatile void __iomem *)0xF7EA048C);

	/* select PCIE function */
	val = readl((volatile void __iomem *)0xF7EA049C);
	val |= (1 << 0);
	writel(val, (volatile void __iomem *)0xF7EA049C);

	val = readl((volatile void __iomem *)0xF7EA0484);
	val &= ~(1 << 1);
	val &= ~(1 << 2);
	writel(val, (volatile void __iomem *)0xF7EA0484);
	val |= (1 << 1);
	val |= (1 << 2);
	val |= (1 << 0);
	val &= ~(1 << 3);
	writel(val, (volatile void __iomem *)0xF7EA0484);

	pcie_phy_writel(0x21, 0xC1);
	pcie_phy_writel(0xFC62, 0x1);
	pcie_phy_writel(0x60CA, 0x4F);
	pcie_phy_writel(0x8081, 0x54);
	pcie_phy_writel(0xE408, 0x52);
	pcie_phy_writel(0xE008, 0x52);
	pcie_phy_writel(0x20, 0xC1);
	pcie_phy_writel(0x8000, 0x70);

	if (!berlin_pcie_phy_ready(pcie))
		return -ENODEV;

	if (gpio_is_valid(pcie->reset_gpio)) {
		/* reset EP */
		gpio_direction_output(pcie->reset_gpio, 0);
//		udelay(100);
		udelay(200); // WA
		gpio_direction_output(pcie->reset_gpio, 1);
	}

	/* disable link and clear rc, lane, speed fieds */
	val = berlin_readl(pcie, PCIE_CTRL0);
	val &= ~0x5F;
	berlin_writel(pcie, val, PCIE_CTRL0);
	udelay(200);
	/* set rc, lane, speed, enable link training */
	val |= ((1 << 2) | 0x40);
	berlin_writel(pcie, val, PCIE_CTRL0);

	/* disable the config space in memory map */
	val = berlin_readl(pcie, PCIE_MAC_CTRL);
	val &= ~(1 << 3);
	berlin_writel(pcie, val, PCIE_MAC_CTRL);

	/* unmask legacy interrupts (assert messages only) */
	val = berlin_readl(pcie, PCIE_ISRM0);
	val &= ~(0xf << 16);
	berlin_writel(pcie, val, PCIE_ISRM0);

	return 0;
}

static int berlin_pcie_enable(struct berlin_pcie *pcie)
{
	u32 val;
	struct hw_pci hw;

	if (!berlin_pcie_link_up(pcie)) {
		dev_info(pcie->dev, "link down, ignoring\n");
		return -ENODEV;
	}

	dev_info(pcie->dev, "link up\n");
	udelay(100); //WA

	/* enable rc bus master, mem and io */
	berlin_writel(pcie, 0x7, PF0_CMD_STS);

	/* enable mem master, mem space, io space */
	berlin_pcie_hw_rd_conf(pcie, 0, 0, PCI_COMMAND, 4, &val);
	val = PCI_COMMAND_IO | PCI_COMMAND_MEMORY | PCI_COMMAND_MASTER;
	berlin_pcie_hw_wr_conf(pcie, 0, 0, PCI_COMMAND, 4, val);

	/* set root complex BAR0 to 0 */
	berlin_writel(pcie, 0, PF0_BAR0);

	/* disable flush timer timout */
	val = berlin_readl(pcie, PCIE_FLUSH);
	val &= ~0xffff;
	berlin_writel(pcie, val, PCIE_FLUSH);

	/* disable ordering checks; Spark bugzilla #778 */
	val = berlin_readl(pcie, LMI + 0x208);
	val |= 1 << 30;
	berlin_writel(pcie, val, LMI + 0x208);

	pcie->root_bus_nr = -1;
	memset(&hw, 0, sizeof(hw));

	hw.nr_controllers = 1;
	hw.private_data   = (void **)&pcie;
	hw.setup          = berlin_pcie_setup;
	hw.scan           = berlin_pcie_scan_bus;
	hw.map_irq        = berlin_pcie_map_irq;
	hw.ops            = &berlin_pcie_ops;
	hw.add_bus        = berlin_pcie_add_bus;

	pci_common_init(&hw);

	return 0;
}

static struct berlin_pcie *g_pcie;

static int berlin_pcie_probe(struct platform_device *pdev)
{
	struct berlin_pcie *pcie;
	struct device_node *np = pdev->dev.of_node;
	struct of_pci_range_parser parser;
	struct of_pci_range range;
	struct resource *res;
	int ret;

	pcie = devm_kzalloc(&pdev->dev, sizeof(struct berlin_pcie),
			    GFP_KERNEL);
	if (!pcie)
		return -ENOMEM;

	g_pcie = pcie;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	pcie->base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(pcie->base))
		return PTR_ERR(pcie->base);

	ret = platform_get_irq(pdev, 0);
	if (ret < 0) {
		dev_err(&pdev->dev, "failed to get IRQ: %d\n", ret);
		return ret;
	}
	pcie->irq = ret;
	ret = devm_request_irq(&pdev->dev, pcie->irq, berlin_pcie_irq,
				IRQF_SHARED, "berlin-pcie", pcie);
	if (ret < 0) {
		dev_err(&pdev->dev, "failed to request IRQ: %d\n", ret);
		return ret;
	}

	pcie->dev = &pdev->dev;
	platform_set_drvdata(pdev, pcie);

	pcie->rc_rst = devm_reset_control_get(pcie->dev, NULL);
	if (IS_ERR(pcie->rc_rst))
		return PTR_ERR(pcie->rc_rst);

	if (of_pci_range_parser_init(&parser, np)) {
		dev_err(&pdev->dev, "missing ranges property\n");
		return -EINVAL;
	}

	/* Get the I/O and memory ranges from DT */
	for_each_of_pci_range(&parser, &range) {
		unsigned long restype = range.flags & IORESOURCE_TYPE_BITS;
		if (restype == IORESOURCE_IO) {
			of_pci_range_to_resource(&range, np, &pcie->io);
			pcie->io.name = "I/O";
			pcie->io.start = max_t(resource_size_t,
						PCIBIOS_MIN_IO,
						range.pci_addr);
			pcie->io.end = min_t(resource_size_t,
					     IO_SPACE_LIMIT,
					     range.pci_addr + range.size);
		}
		if (restype == IORESOURCE_MEM) {
			of_pci_range_to_resource(&range, np, &pcie->mem);
			pcie->mem.name = "MEM";
			pcie->mem_bus_addr = range.pci_addr;
		}
	}

	ret = of_pci_parse_bus_range(np, &pcie->busn);
	if (ret < 0) {
		dev_err(pcie->dev, "failed to parse ranges property: %d\n",
			ret);
		pcie->busn.name = np->name;
		pcie->busn.start = 0;
		pcie->busn.end = 0xff;
		pcie->busn.flags = IORESOURCE_BUS;
	}

	pcie->reset_gpio = of_get_named_gpio(np, "reset-gpio", 0);
	if (gpio_is_valid(pcie->reset_gpio)) {
		ret = devm_gpio_request_one(&pdev->dev, pcie->reset_gpio,
					    GPIOF_DIR_OUT, "PCIe reset");
		if (ret) {
			dev_err(&pdev->dev, "unable to get reset gpio\n");
			return ret;
		}
	}

	pcie->power_gpio = of_get_named_gpio(np, "power-gpio", 0);
	if (gpio_is_valid(pcie->power_gpio)) {
		ret = devm_gpio_request_one(&pdev->dev, pcie->power_gpio,
					    GPIOF_DIR_OUT, "PCIe EP Power");
		if (ret) {
			dev_err(&pdev->dev, "unable to get power gpio\n");
			return ret;
		}
	}

	ret = berlin_pcie_enable_controller(pcie);
	if (ret < 0)
		return ret;

	if (IS_ENABLED(CONFIG_PCI_MSI)) {
		ret = berlin_pcie_enable_msi(pcie);
		if (ret < 0)
			dev_err(&pdev->dev, "failed to enable MSI support\n");
	}

	ret = berlin_pcie_enable(pcie);
	if (ret < 0 && IS_ENABLED(CONFIG_PCI_MSI))
		berlin_pcie_disable_msi(pcie);

	pcie->debugfs_root = debugfs_create_dir("berlin_pci", NULL);
	pcie->reg_blob.data = (void *)pcie->base;
	pcie->reg_blob.size = resource_size(res);
	debugfs_create_blob("reg", S_IFREG | S_IRUSR,
			    pcie->debugfs_root, &pcie->reg_blob);
	return ret;
}

static const struct of_device_id berlin_pcie_of_match_table[] = {
	{ .compatible = "marvell,berlin-pcie", },
	{},
};
MODULE_DEVICE_TABLE(of, berlin_pcie_of_match_table);

static struct platform_driver berlin_pcie_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name = "berlin-pcie",
		.of_match_table = berlin_pcie_of_match_table,
		.suppress_bind_attrs = true,
	},
	.probe = berlin_pcie_probe,
};
module_platform_driver(berlin_pcie_driver);

MODULE_AUTHOR("Jisheng Zhang<jszhang@marvell.com>");
MODULE_DESCRIPTION("Marvell Berlin PCIe driver");
MODULE_LICENSE("GPL v2");
