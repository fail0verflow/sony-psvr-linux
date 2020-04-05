/*
*       Allion HSEHET test board driver
*
*       Copyright (C) 2012 Marvell International Ltd.
*
*       This program is free software; you can redistribute it and/or
*       modify it under the terms of the GNU General Public License
*       as published by the Free Software Foundation; either version 2
*       of the License, or (at your option) any later version.
*
*       This program is distributed in the hope that it will be useful,
*       but WITHOUT ANY WARRANTY; without even the implied warranty of
*       MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*       GNU General Public License for more details.
*
*       You should have received a copy of the GNU General Public License
*       along with this program; if not, write to the Free Software
*       Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*
*/
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/usb.h>
#include <linux/usb/hcd.h>

#define ALLION_HSEHET_VENDOR_ID 		0x1A0A
#define HSEHET_PRODUCT_ID_SEO_NAK		0x0101
#define HSEHET_PRODUCT_ID_J			0x0102
#define HSEHET_PRODUCT_ID_K			0x0103
#define HSEHET_PRODUCT_ID_PACKET		0x0104
#define HSEHET_PRODUCT_ID_REV			0x0105

static int __init allion_hsehet_init (void);
static void __exit allion_hsehet_exit (void);

static const struct usb_device_id id_table[] = {
	{ USB_DEVICE(ALLION_HSEHET_VENDOR_ID, HSEHET_PRODUCT_ID_SEO_NAK) },
	{ USB_DEVICE(ALLION_HSEHET_VENDOR_ID, HSEHET_PRODUCT_ID_J) },
	{ USB_DEVICE(ALLION_HSEHET_VENDOR_ID, HSEHET_PRODUCT_ID_K) },
	{ USB_DEVICE(ALLION_HSEHET_VENDOR_ID, HSEHET_PRODUCT_ID_PACKET) },
	{ USB_DEVICE(ALLION_HSEHET_VENDOR_ID, HSEHET_PRODUCT_ID_REV) },
	{ }                                             /* Terminating entry */
};

MODULE_DEVICE_TABLE (usb, id_table);

static int set_port_in_test_mode(struct usb_device *hdev, int port, int test_mode)
{
	int ret;

	switch (test_mode) {
		case 01: dev_dbg(&hdev->dev, "Setting HUB to send Test_J\n");
			break;
		case 02: dev_dbg(&hdev->dev, "Setting HUB to send Test_K\n");
			break;
		case 03: dev_dbg(&hdev->dev, "Setting HUB to send Test_SE0_NAK\n");
			break;
		case 04: dev_dbg(&hdev->dev, "Setting HUB to send Test_Packet\n");
			break;
		case 05: dev_dbg(&hdev->dev, "Setting HUB to send Test_Force_Enable\n");
			break;
	}
	ret = usb_control_msg(hdev, usb_sndctrlpipe(hdev, 0),
		USB_REQ_SET_FEATURE, USB_RT_PORT, USB_PORT_FEAT_TEST,
		(test_mode << 8) | port, NULL, 0, USB_CTRL_GET_TIMEOUT);
	if ( ret ) {
		dev_info(&hdev->dev, "Placing port %d in test mode[%d] failed with return %d\n",
				port, test_mode, ret);
	} else {
		dev_info(&hdev->dev, "Placing port %d in test mode[%d] passed\n",
				port, test_mode);
	}
	return ret;
}

static int allion_hsehet_probe(struct usb_interface *intf, const struct usb_device_id *id)
{
	struct usb_device *udev = interface_to_usbdev(intf);

	return set_port_in_test_mode(udev->parent, udev->portnum, id->idProduct-0x100);
}

static void allion_hsehet_disconnect(struct usb_interface *intf)
{
}

static struct usb_driver allion_hsehet_driver = {
	.name		= "allion_hsehet test v1.0",
	.probe		= allion_hsehet_probe,
	.disconnect	= allion_hsehet_disconnect,
	.id_table	= id_table,
};

static int __init allion_hsehet_init (void)
{
	return usb_register(&allion_hsehet_driver);
}

static void __exit allion_hsehet_exit (void)
{
	usb_deregister (&allion_hsehet_driver);
}

module_init(allion_hsehet_init);
module_exit(allion_hsehet_exit);
