/*
 *  USB phy driver for Marvell Berlin SoCs
 *  Author:	Jisheng Zhang <jszhang@marvell.com>
 *  Copyright (c) 2013 Marvell Technology Group Ltd.
 *		http://www.marvell.com
 *
 * The code contained herein is licensed under the GNU General Public
 * License. You may obtain a copy of the GNU General Public License
 * Version 2 or later at the following locations:
 *
 * http://www.opensource.org/licenses/gpl-license.html
 * http://www.gnu.org/copyleft/gpl.html
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/usb/phy.h>

#define DRIVER_NAME "berlin_phy"

#define USB_PHY_PLL		0x04
#define USB_PHY_PLL_CONTROL	0x08
#define USB_PHY_TX_CTRL0	0x10
#define USB_PHY_TX_CTRL1	0x14
#define USB_PHY_TX_CTRL2	0x18
#define USB_PHY_RX_CTRL		0x20
#define USB_PHY_ANALOG		0x34

#define USB2_OTG_REG0		0x34
#define USB2_CHARGER_REG0	0x38
#define USB2_PLL_REG0		0x0
#define USB2_PLL_REG1		0x4
#define USB2_DIG_REG0		0x1c
#define USB2_TX_CH_CTRL0	0x0c
#define BERLIN2_CDP_MODE	2
#define BERLIN2_DEF_MODE	0

struct berlin_phy {
	struct usb_phy phy;
	void __iomem *reset_base;
	u32 reset_mask;
	int pwr_gpio;
	u32 phy_mode;
};

#define to_berlin_phy(p) container_of((p), struct berlin_phy, phy)

static int berlin_cdp_reinit_phy(void __iomem *base)
{
	u32 data, timeout;

	/* powering up OTG */
	data = readl( base + USB2_OTG_REG0);
	data |= 1<<4;
	writel(data, (base + USB2_OTG_REG0));

	/* powering Charger detector circuit */
	data = readl( base + USB2_CHARGER_REG0);
	data |= 1<<5;
	writel(data, (base + USB2_CHARGER_REG0));

	/* Power up analog port */
	data = readl( base + USB2_TX_CH_CTRL0);
	data |= 1<<24;
	writel(data, (base + USB2_TX_CH_CTRL0));

	/* Configuring FBDIV for SOC Clk 25 Mhz */
	data = readl( base + USB2_PLL_REG0);
	data &= 0xce00ff80;
	data |= 5 | (0x60<<16) | (0<<28);
	writel(data, (base + USB2_PLL_REG0));

	/* Power up PLL, Reset, Suspen_en disable */
	writel(0x407, (base + USB2_PLL_REG1));
	udelay(100);

	/* Deassert Reset */
	writel(0x403, (base + USB2_PLL_REG1));

	/* Wait for PLL Lock */
	timeout = 0x1000000;
	do {
		data = readl( base + USB2_PLL_REG0);
		if (!--timeout)
			break;
	} while ( !(data&0x80000000));

	if (!timeout)
		printk(KERN_ERR "ERROR: USB PHY PLL NOT LOCKED!\n");

	return 0;
}

static int berlin_phy_init(struct usb_phy *phy)
{
	int err;
	struct berlin_phy *berlin_phy = to_berlin_phy(phy);
	void __iomem *base = berlin_phy->phy.io_priv;

	if (gpio_is_valid(berlin_phy->pwr_gpio)) {
		err = gpio_direction_output(berlin_phy->pwr_gpio, 0);
		if (err) {
			dev_err(phy->dev, "can't set pwr gpio\n");
			return err;
		}
	}

	if (berlin_phy->reset_base) {
		writel(berlin_phy->reset_mask, berlin_phy->reset_base);
		udelay(10);
	}

	if (berlin_phy->phy_mode == BERLIN2_CDP_MODE) {
		berlin_cdp_reinit_phy(base);
	} else {
		writel(0x54C0, base + USB_PHY_PLL);
		writel(0x2235, base + USB_PHY_PLL_CONTROL);
		writel(0x5680, base + USB_PHY_ANALOG);
		writel(0xAA79, base + USB_PHY_RX_CTRL);

		writel(0x6c0, base + USB_PHY_TX_CTRL1);
		writel(0x588, base + USB_PHY_TX_CTRL0);
		udelay(1);
		writel(0x2588, base + USB_PHY_TX_CTRL0);
		udelay(1);
		writel(0x588, base + USB_PHY_TX_CTRL0);
		writel(0x6FF, base + USB_PHY_TX_CTRL2);
	}

	if (gpio_is_valid(berlin_phy->pwr_gpio))
		gpio_set_value(berlin_phy->pwr_gpio, 1);

	return 0;
}

static void berlin_phy_shutdown(struct usb_phy *phy)
{
	struct berlin_phy *berlin_phy = to_berlin_phy(phy);

	if (gpio_is_valid(berlin_phy->pwr_gpio))
		gpio_set_value(berlin_phy->pwr_gpio, 0);
}

static int berlin_phy_probe(struct platform_device *pdev)
{
	struct resource *res0, *res1;
	void __iomem *base, *reset_base;
	struct berlin_phy *berlin_phy;
	int ret, gpio;
	u32 val;

	res0 = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	base = devm_ioremap_resource(&pdev->dev, res0);
	if (IS_ERR(base))
		return PTR_ERR(base);

	res1 = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	if (res1) {
		reset_base = devm_ioremap(&pdev->dev,
				res1->start, resource_size(res1));
		if (!reset_base)
			return -ENOMEM;

		if (of_property_read_u32(pdev->dev.of_node, "reset-bit", &val)) {
			dev_err(&pdev->dev, "no reset-bit set\n");
			return -EINVAL;
		}
	} else {
		reset_base = NULL;
		val = 0;
	}

	berlin_phy = devm_kzalloc(&pdev->dev, sizeof(*berlin_phy), GFP_KERNEL);
	if (!berlin_phy) {
		dev_err(&pdev->dev, "Failed to allocate USB PHY structure!\n");
		return -ENOMEM;
	}

	gpio = of_get_named_gpio(pdev->dev.of_node, "pwr-gpio", 0);
	if (!gpio_is_valid(gpio)) {
		dev_err(&pdev->dev, "no pwr-gpio set\n");
		berlin_phy->pwr_gpio = -1;
	} else {
		ret = gpio_request(gpio, "pwr-gpio");
		if (ret) {
			dev_err(&pdev->dev, "can't request pwr gpio %d", gpio);
			return ret;
		}
		berlin_phy->pwr_gpio = gpio;
	}

	if (of_property_read_u32(pdev->dev.of_node,
		"phy-mode", &berlin_phy->phy_mode)) {
		berlin_phy->phy_mode = BERLIN2_DEF_MODE;
	}

	berlin_phy->reset_base		= reset_base;
	berlin_phy->reset_mask		= (1 << val);
	berlin_phy->phy.io_priv		= base;
	berlin_phy->phy.dev		= &pdev->dev;
	berlin_phy->phy.label		= DRIVER_NAME;
	berlin_phy->phy.init		= berlin_phy_init;
	berlin_phy->phy.shutdown	= berlin_phy_shutdown;
	berlin_phy->phy.type		= USB_PHY_TYPE_USB2;

	platform_set_drvdata(pdev, berlin_phy);

	return usb_add_phy_dev(&berlin_phy->phy);
}

static int berlin_phy_remove(struct platform_device *pdev)
{
	struct berlin_phy *berlin_phy = platform_get_drvdata(pdev);

	if (gpio_is_valid(berlin_phy->pwr_gpio))
		gpio_free(berlin_phy->pwr_gpio);

	usb_remove_phy(&berlin_phy->phy);

	return 0;
}

static const struct of_device_id berlin_phy_dt_ids[] = {
	{ .compatible = "marvell,berlin-usbphy", },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, berlin_phy_dt_ids);

static struct platform_driver berlin_phy_driver = {
	.probe = berlin_phy_probe,
	.remove = berlin_phy_remove,
	.driver = {
		.name = DRIVER_NAME,
		.owner	= THIS_MODULE,
		.of_match_table = berlin_phy_dt_ids,
	 },
};

static int __init berlin_phy_module_init(void)
{
	return platform_driver_register(&berlin_phy_driver);
}
subsys_initcall(berlin_phy_module_init);

static void __exit berlin_phy_module_exit(void)
{
	platform_driver_unregister(&berlin_phy_driver);
}
module_exit(berlin_phy_module_exit);

MODULE_AUTHOR("Jisheng Zhang <jszhang@marvell.com>");
MODULE_DESCRIPTION("Marvell Berlin USB PHY driver");
MODULE_LICENSE("GPL");
