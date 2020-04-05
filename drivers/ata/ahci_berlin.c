/*
 *  Marvell Berlin AHCI SATA platform driver
 *  Author:	Jisheng Zhang <jszhang@marvell.com>
 *  Copyright (c) 2013 Marvell Technology Group Ltd.
 *		http://www.marvell.com
 *
 * based on the AHCI SATA platform driver by Jeff Garzik and Anton Vorontsov
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/ahci_platform.h>
#include <linux/of_device.h>
#include "ahci.h"

#define PORT_VSR_ADDR		0x78
#define PORT_VSR_DATA		0x7C
#define HOST_VSA_ADDR		0xA0
#define HOST_VSA_DATA		0xA4
#define COMPHY_CTRL0	0x0000

struct berlin_ahci_priv {
	struct platform_device *ahci_pdev;
	void __iomem *phy_base;
};

static void port_init(struct device *dev, void __iomem *mmio, int n)
{
	u32 tmp;
	void __iomem *addr = mmio + 0x100 + (n * 0x80);
	struct berlin_ahci_priv *priv = dev_get_drvdata(dev->parent);

	/* To force interface select register 24 to 128B mbus rd/wr
	 * size, 3 to SRAM, 4 to PCIE
	 */
	__raw_writel(0x4, mmio + HOST_VSA_ADDR);
	__raw_writel(0x240034, mmio + HOST_VSA_DATA);

	/* Set PIN_PU_IVREF */
	tmp = __raw_readl(priv->phy_base + COMPHY_CTRL0);
	tmp |= 0x1;
	__raw_writel(tmp, priv->phy_base + COMPHY_CTRL0);

	/* Power down PLL until register programming is done */
	__raw_writel(0x0, mmio + HOST_VSA_ADDR);
	tmp = __raw_readl(mmio + HOST_VSA_DATA);
	tmp |= 0x40;
	__raw_writel(tmp, mmio + HOST_VSA_DATA);

	/* Set PHY_MODE to SATA, REF_FREF_SEL to 25MHz */
	__raw_writel(0x200 + 0x1, addr + PORT_VSR_ADDR);
	tmp = __raw_readl(addr + PORT_VSR_DATA);
	tmp &= 0xFF00;
	tmp |= 0x01;
	__raw_writel(tmp, addr + PORT_VSR_DATA);

	/* Set PHY_GEN_MAX to 6Gbps */
	__raw_writel(0x200 + 0x25, addr + PORT_VSR_ADDR);
	tmp = __raw_readl(addr + PORT_VSR_DATA);
	tmp &= 0xF3FF;
	tmp |= 0x800;
	__raw_writel(tmp, addr + PORT_VSR_DATA);

	/* Set SEL_BITS to 40-bit */
	__raw_writel(0x200 + 0x23, addr + PORT_VSR_ADDR);
	tmp = __raw_readl(addr + PORT_VSR_DATA);
	tmp &= 0xF3FF;
	tmp |= 0x800;
	__raw_writel(tmp, addr + PORT_VSR_DATA);

	/* USE_MAX_PLL_RATE=1 */
	__raw_writel(0x200 + 0x2, addr + PORT_VSR_ADDR);
	tmp = __raw_readl(addr + PORT_VSR_DATA);
	tmp |= 0x1000;
	__raw_writel(tmp, addr + PORT_VSR_DATA);

	/* Disable PLL Power done */
	__raw_writel(0x0, mmio + HOST_VSA_ADDR);
	tmp = __raw_readl(mmio + HOST_VSA_DATA);
	tmp &= 0xFFFFFFBF;
	__raw_writel(tmp, mmio + HOST_VSA_DATA);

	/* Set The Speed to upto 6Gbps in controller */
	__raw_writel(0x00000030, addr + PORT_SCR_CTL);
	__raw_writel(0x00000031, addr + PORT_SCR_CTL);
	__raw_writel(0x00000030, addr + PORT_SCR_CTL);
}

static int berlin_ahci_init(struct device *dev, void __iomem *mmio)
{
	int i;
	u32 port_map;
	struct ahci_platform_data *pdata = dev_get_platdata(dev);

	if (pdata->force_port_map)
		port_map = pdata->force_port_map;
	else
		port_map = readl(mmio + HOST_PORTS_IMPL);

	for (i = 0; port_map; i++) {
		port_map &= ~(1 << i);
		port_init(dev, mmio, i);
	}

	return 0;
}

static int berlin_ahci_resume(struct device *dev)
{
	struct ata_host *host = dev_get_drvdata(dev);
	struct ahci_host_priv *hpriv = host->private_data;

	return berlin_ahci_init(dev, hpriv->mmio);
}

static struct ahci_platform_data berlin_ahci_pdata = {
	.init		= berlin_ahci_init,
	.resume		= berlin_ahci_resume,
	.force_port_map	= 1,
};

static const struct of_device_id berlin_ahci_of_match[] = {
	{ .compatible = "marvell,berlin-ahci", .data = &berlin_ahci_pdata},
	{},
};
MODULE_DEVICE_TABLE(of, berlin_ahci_of_match);

static int berlin_ahci_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct resource *mem, *irq, *phy, res[2];
	const struct of_device_id *of_id;
	const struct ahci_platform_data *pdata = NULL;
	struct berlin_ahci_priv *priv;
	struct device *ahci_dev;
	struct platform_device *ahci_pdev;
	int ret;

	priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
	if (!priv) {
		dev_err(dev, "can't alloc ahci_host_priv\n");
		return -ENOMEM;
	}

	ahci_pdev = platform_device_alloc("ahci", -1);
	if (!ahci_pdev)
		return -ENODEV;

	ahci_dev = &ahci_pdev->dev;
	ahci_dev->parent = dev;

	priv->ahci_pdev = ahci_pdev;
	platform_set_drvdata(pdev, priv);

	of_id = of_match_device(berlin_ahci_of_match, dev);
	if (of_id) {
		pdata = of_id->data;
	} else {
		ret = -EINVAL;
		goto err_out;
	}

	mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	irq = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	phy = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	if (!mem || !irq || !phy) {
		dev_err(dev, "no mmio/irq resource\n");
		ret = -ENOMEM;
		goto err_out;
	}

	res[0] = *mem;
	res[1] = *irq;

	priv->phy_base = devm_ioremap(dev, phy->start, resource_size(phy));
	if (!priv->phy_base) {
		dev_err(dev, "can't map %pR\n", phy);
		ret = -ENOMEM;
		goto err_out;
	}

	ahci_dev->coherent_dma_mask = DMA_BIT_MASK(32);
	ahci_dev->dma_mask = &ahci_dev->coherent_dma_mask;

	ret = platform_device_add_resources(ahci_pdev, res, 2);
	if (ret)
		goto err_out;

	ret = platform_device_add_data(ahci_pdev, pdata, sizeof(*pdata));
	if (ret)
		goto err_out;

	ret = platform_device_add(ahci_pdev);
	if (ret) {
err_out:
		platform_device_put(ahci_pdev);
		return ret;
	}

	return 0;
}

static int berlin_ahci_remove(struct platform_device *pdev)
{
	struct berlin_ahci_priv *priv = platform_get_drvdata(pdev);
	struct platform_device *ahci_pdev = priv->ahci_pdev;

	platform_device_unregister(ahci_pdev);
	return 0;
}

static struct platform_driver berlin_ahci_driver = {
	.probe = berlin_ahci_probe,
	.remove = berlin_ahci_remove,
	.driver = {
		.name = "ahci-berlin",
		.owner = THIS_MODULE,
		.of_match_table = berlin_ahci_of_match,
	},
};
module_platform_driver(berlin_ahci_driver);

MODULE_DESCRIPTION("Marvell Berlin AHCI SATA platform driver");
MODULE_AUTHOR("Jisheng Zhang <jszhang@marvell.com>");
MODULE_LICENSE("GPL");
MODULE_ALIAS("ahci:berlin");
