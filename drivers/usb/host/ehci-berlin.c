/*
 *  EHCI-compliant USB host controller driver for Marvell Berlin SoCs
 *
 *  Author:	Jisheng Zhang <jszhang@marvell.com>
 *  Copyright (c) 2013 Marvell Technology Group Ltd.
 *		http://www.marvell.com
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
#include <linux/dma-mapping.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/io.h>
#include <linux/usb.h>
#include <linux/usb/hcd.h>
#include <linux/usb/phy.h>

#include "ehci.h"

#define MV_SBUSCFG			0x090
#define MV_USB_CHIP_CAP			0x100
#define MV_USBMODE			0x1a8

static struct hc_driver __read_mostly berlin_ehci_hc_driver;

struct berlin_ehci_hcd {
	struct ehci_hcd *ehci;
};

static int berlin_ehci_reset(struct usb_hcd *hcd)
{
	int retval;
	struct ehci_hcd *ehci = hcd_to_ehci(hcd);
	struct device *dev = hcd->self.controller;

	usb_phy_init(hcd->phy);

	hcd->has_tt = 1;

	retval = ehci_setup(hcd);
	if (retval)
		dev_err(dev, "ehci_setup failed %d\n", retval);

	ehci_writel(ehci, 0x07, hcd->regs + MV_SBUSCFG);
	ehci_writel(ehci, 0x5013, hcd->regs + MV_USBMODE);

	return retval;
}

static int berlin_ehci_probe(struct platform_device *pdev)
{
	int retval, irq;
	struct usb_hcd *hcd;
	struct ehci_hcd *ehci;
	struct resource *res;
	struct berlin_ehci_hcd *berlin;

	irq = platform_get_irq(pdev, 0);
	if (!irq) {
		dev_err(&pdev->dev, "Failed to get IRQ\n");
		return -ENODEV;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(&pdev->dev,
			"Found HC with no register addr. Check %s setup!\n",
			dev_name(&pdev->dev));
		return -ENODEV;
	}

	berlin = devm_kzalloc(&pdev->dev, sizeof(*berlin), GFP_KERNEL);
	if (!berlin)
		return -ENOMEM;

	if (!pdev->dev.dma_mask)
		pdev->dev.dma_mask = &pdev->dev.coherent_dma_mask;
	if (!pdev->dev.coherent_dma_mask)
		pdev->dev.coherent_dma_mask = DMA_BIT_MASK(32);

	hcd = usb_create_hcd(&berlin_ehci_hc_driver, &pdev->dev,
			     dev_name(&pdev->dev));
	if (!hcd) {
		dev_err(&pdev->dev, "Unable to create HCD\n");
		return -ENOMEM;
	}

	hcd->phy = devm_usb_get_phy_by_phandle(&pdev->dev, "usb-phy", 0);
	if (IS_ERR(hcd->phy)) {
		retval = PTR_ERR(hcd->phy);
		dev_err(&pdev->dev, "Unable to find transceiver\n");
		goto err;
	}

	hcd->rsrc_start = res->start;
	hcd->rsrc_len = resource_size(res);

	hcd->regs = devm_ioremap_resource(&pdev->dev, res);
	if (!hcd->regs) {
		dev_err(&pdev->dev, "ioremap failed \n");
		retval = -EFAULT;
		goto err;
	}

	berlin->ehci = ehci = hcd_to_ehci(hcd);
	ehci->caps = hcd->regs + MV_USB_CHIP_CAP;
	ehci->regs = hcd->regs + MV_USB_CHIP_CAP +
		HC_LENGTH(ehci, readl(&ehci->caps->hc_capbase));

	platform_set_drvdata(pdev, berlin);

	retval = usb_add_hcd(hcd, irq, IRQF_SHARED);

	if (!retval) {
		dev_info(&pdev->dev, "usb_add_hcd successful\n");
		return retval;
	}

err:
	usb_put_hcd(hcd);
	return retval;
}

static int berlin_ehci_remove(struct platform_device *pdev)
{
	struct berlin_ehci_hcd *berlin = platform_get_drvdata(pdev);
	struct usb_hcd *hcd = ehci_to_hcd(berlin->ehci);

	usb_remove_hcd(hcd);
	usb_phy_shutdown(hcd->phy);
	usb_put_hcd(hcd);

	return 0;
}

static void berlin_ehci_shutdown(struct platform_device *pdev)
{
	struct berlin_ehci_hcd *berlin = platform_get_drvdata(pdev);
	struct usb_hcd *hcd = ehci_to_hcd(berlin->ehci);

	if (hcd->driver->shutdown)
		hcd->driver->shutdown(hcd);
}

#ifdef CONFIG_PM
static int berlin_ehci_suspend(struct device *dev)
{
	int rc;
	struct berlin_ehci_hcd *berlin =
			platform_get_drvdata(to_platform_device(dev));
	struct ehci_hcd	*ehci = berlin->ehci;
	struct usb_hcd *hcd = ehci_to_hcd(ehci);
	bool do_wakeup = device_may_wakeup(dev);

	rc = ehci_suspend(hcd, do_wakeup);

	usb_phy_shutdown(hcd->phy);

	return rc;
}

static int berlin_ehci_resume(struct device *dev)
{
	struct berlin_ehci_hcd *berlin =
			platform_get_drvdata(to_platform_device(dev));
	struct ehci_hcd	*ehci = berlin->ehci;
	struct usb_hcd *hcd = ehci_to_hcd(ehci);

	usb_phy_init(hcd->phy);

	ehci_resume(hcd, true);

	ehci_writel(ehci, 0x07, hcd->regs + MV_SBUSCFG);
	ehci_writel(ehci, 0x5013, hcd->regs + MV_USBMODE);

	return 0;
}

static const struct dev_pm_ops berlin_ehci_pm_ops = {
	.suspend	= berlin_ehci_suspend,
	.resume		= berlin_ehci_resume,
};
#endif

static const struct of_device_id berlin_ehci_of_match[] = {
	{.compatible = "marvell,berlin-ehci",},
	{},
};
MODULE_DEVICE_TABLE(of, berlin_ehci_of_match);
MODULE_ALIAS("berlin-ehci");

static struct platform_driver berlin_ehci_driver = {
	.probe 		= berlin_ehci_probe,
	.remove 	= berlin_ehci_remove,
	.shutdown 	= berlin_ehci_shutdown,
	.driver = {
		.name = "berlin-ehci",
		.of_match_table = berlin_ehci_of_match,
#ifdef CONFIG_PM
		.pm	= &berlin_ehci_pm_ops,
#endif
	}
};

static const struct ehci_driver_overrides berlin_overrides __initdata = {
	.extra_priv_size = sizeof(struct berlin_ehci_hcd),
	.reset = berlin_ehci_reset,
};

static int __init berlin_ehci_init(void)
{
	if (usb_disabled())
		return -ENODEV;

	ehci_init_driver(&berlin_ehci_hc_driver, &berlin_overrides);
	return platform_driver_register(&berlin_ehci_driver);
}
module_init(berlin_ehci_init);

static void __exit berlin_ehci_exit(void)
{
	platform_driver_unregister(&berlin_ehci_driver);
}
module_exit(berlin_ehci_exit);

MODULE_DESCRIPTION("Marvell Berlin EHCI USB host driver");
MODULE_AUTHOR("Jisheng Zhang <jszhang@marvell.com>");
MODULE_LICENSE("GPL");
