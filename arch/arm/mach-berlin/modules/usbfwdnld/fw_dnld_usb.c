/** @file fw_dnld_usb.c
  *
  * @brief This file contains the major functions for firmware
  * download.
  *
  * Copyright (C) 2008-2014, Marvell International Ltd.
  *
  * This software file (the "File") is distributed by Marvell International
  * Ltd. under the terms of the GNU General Public License Version 2, June 1991
  * (the "License").  You may use, redistribute and/or modify this File in
  * accordance with the terms and conditions of the License, a copy of which
  * is available by writing to the Free Software Foundation, Inc.,
  * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA or on the
  * worldwide web at http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt.
  *
  * THE FILE IS DISTRIBUTED AS-IS, WITHOUT WARRANTY OF ANY KIND, AND THE
  * IMPLIED WARRANTIES OF MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE
  * ARE EXPRESSLY DISCLAIMED.  The License provides additional details about
  * this warranty disclaimer.
  *
  */

/********************************************************
Change log:
    02/20/2012: initial version
********************************************************/

#include        "fw_dnld_usb.h"

/********************************************************
        Variables
********************************************************/

/** Firmware name */
char *fw_name = NULL;
/** Request firmware no wait flag */
int req_fw_nowait = 0;

/** Application version */
#define VERSION "M002"

/** Marvell USB device */
#define MARVELL_USB_DEVICE(vid, pid, name) \
           USB_DEVICE(vid, pid),  \
           .driver_info = (t_ptr)name

/** Name of the USB driver */
const char usbdriver_name[] = "usbfwdnld";

/** This structure contains the device signature */
struct usb_device_id driver_usb_table[] = {
	/* Enter the device signature inside */
	{MARVELL_USB_DEVICE
	 (MARVELL_USB_VID, MARVELL_USB8766_PID, "Marvell WLAN USB Adapter")},
	{MARVELL_USB_DEVICE
	 (MARVELL_USB_VID, MARVELL_USB8797_PID, "Marvell WLAN USB Adapter")},
	{MARVELL_USB_DEVICE
	 (MARVELL_USB_VID, MARVELL_USB8897_PID, "Marvell WLAN USB Adapter")},
	/* Terminating entry */
	{},
};

static int driver_usb_probe(struct usb_interface *intf,
			    const struct usb_device_id *id);
static void driver_usb_disconnect(struct usb_interface *intf);

/** driver_usb_driver */
static struct usb_driver driver_usb_driver = {
    /** Driver name */
	.name = usbdriver_name,

    /** Probe function name */
	.probe = driver_usb_probe,

    /** Disconnect function name */
	.disconnect = driver_usb_disconnect,

    /** Device signature table */
	.id_table = driver_usb_table,
};

MODULE_DEVICE_TABLE(usb, driver_usb_table);

/** Semaphore for add/remove card */
struct semaphore AddRemoveCardSem;
/** The maximum number of adapter supported */
#define MAX_DRIVER_ADAPTER    2
/** The global variable of a pointer to driver_handle structure variable */
driver_handle *m_handle[MAX_DRIVER_ADAPTER];

/********************************************************
        Functions
********************************************************/

/**
 *  @brief Alloc a buffer
 *
 *  @param pdriver_handle Pointer to the driver context
 *  @param size     The size of the buffer to be allocated
 *  @param flag     The type of the buffer to be allocated
 *  @param ppbuf    Pointer to a buffer location to store buffer pointer allocated
 *
 *  @return         DRIVER_STATUS_SUCCESS or DRIVER_STATUS_FAILURE
 */
driver_status
driver_malloc(IN driver_handle * handle,
	      IN t_u32 size, IN t_u32 flag, OUT t_u8 ** ppbuf)
{
	t_u32 mem_flag = GFP_ATOMIC;	/* Default type: GFP_ATOMIC */

	if (flag & DRIVER_MEM_DMA)
		mem_flag |= GFP_DMA;

	if (!(*ppbuf = kmalloc(size, mem_flag))) {
		PRINTM(MERROR, "%s: allocate  buffer %d failed!\n",
		       __FUNCTION__, (int)size);
		return DRIVER_STATUS_FAILURE;
	}
	handle->malloc_count++;

	return DRIVER_STATUS_SUCCESS;
}

/**
 *  @brief Free a buffer
 *
 *  @param driver_handle    Pointer to the driver context
 *  @param pbuf             Pointer to the buffer to be freed
 *
 *  @return                 DRIVER_STATUS_SUCCESS or DRIVER_STATUS_FAILURE
 */
driver_status
driver_mfree(IN driver_handle * handle, IN t_u8 * pbuf)
{
	if (!pbuf)
		return DRIVER_STATUS_FAILURE;
	kfree(pbuf);
	handle->malloc_count--;
	return DRIVER_STATUS_SUCCESS;
}

/**
 *  @brief This function reads one block of firmware data
 *
 *  @param pdriver_handle Pointer to the driver context
 *  @param offset       Offset from where the data will be copied
 *  @param len          Length to be copied
 *  @param pbuf         Buffer where the data will be copied
 *
 *  @return             DRIVER_STATUS_SUCCESS or DRIVER_STATUS_FAILURE
 */
driver_status
driver_get_fw_data(IN driver_handle * handle,
		   IN t_u32 offset, IN t_u32 len, OUT t_u8 * pbuf)
{
	if (!pbuf || !len)
		return DRIVER_STATUS_FAILURE;

	if (offset + len > handle->firmware->size)
		return DRIVER_STATUS_FAILURE;

	memcpy(pbuf, handle->firmware->data + offset, len);

	return DRIVER_STATUS_SUCCESS;
}

/**
 *  @brief  Free Tx/Rx urb, skb and Rx buffer
 *
 *  @param cardp    Pointer usb_card_rec
 *
 *  @return         N/A
 */
void
driver_usb_free(struct usb_card_rec *cardp)
{
	int i;

	ENTER();

	/* Unlink Rx cmd URB */
	if (atomic_read(&cardp->rx_cmd_urb_pending) && cardp->rx_cmd.urb) {
		usb_kill_urb(cardp->rx_cmd.urb);
	}
	/* Unlink Rx data URBs */
	if (atomic_read(&cardp->rx_data_urb_pending)) {
		for (i = 0; i < MVUSB_RX_DATA_URB; i++) {
			if (cardp->rx_data_list[i].urb)
				usb_kill_urb(cardp->rx_data_list[i].urb);
		}
	}

	/* Free Rx data URBs */
	for (i = 0; i < MVUSB_RX_DATA_URB; i++) {
		if (cardp->rx_data_list[i].urb) {
			usb_free_urb(cardp->rx_data_list[i].urb);
			cardp->rx_data_list[i].urb = NULL;
		}
	}
	/* Free Rx cmd URB */
	if (cardp->rx_cmd.urb) {
		usb_free_urb(cardp->rx_cmd.urb);
		cardp->rx_cmd.urb = NULL;
	}

	/* Free Tx data URBs */
	for (i = 0; i < MVUSB_TX_HIGH_WMARK; i++) {
		if (cardp->tx_data_list[i].urb) {
			usb_free_urb(cardp->tx_data_list[i].urb);
			cardp->tx_data_list[i].urb = NULL;
		}
	}
	/* Free Tx cmd URB */
	if (cardp->tx_cmd.urb) {
		usb_free_urb(cardp->tx_cmd.urb);
		cardp->tx_cmd.urb = NULL;
	}

	LEAVE();
	return;
}

/**
 *  @brief  This function downloads FW blocks to device
 *
 *  @param handle       A pointer to driver_handle
 *  @param pmfw         A pointer to firmware image
 *
 *  @return             DRIVER_STATUS_SUCCESS or DRIVER_STATUS_FAILURE
 */
static driver_status
driver_prog_fw_w_helper(IN driver_handle * handle, IN pdriver_fw_image pmfw)
{
	driver_status ret = DRIVER_STATUS_SUCCESS;
	t_u8 *firmware = pmfw->pfw_buf, *RecvBuff;
	t_u32 retries = MAX_FW_RETRY, DataLength;
	t_u32 FWSeqNum = 0, TotalBytes = 0, DnldCmd = 0;
	FWData *fwdata = MNULL;
	FWSyncHeader SyncFWHeader;
	t_u8 check_winner = 1;

	ENTER();

	if (!firmware) {
		PRINTM(MMSG, "No firmware image found! Terminating download\n");
		ret = DRIVER_STATUS_FAILURE;
		goto fw_exit;
	}

	/* Allocate memory for transmit */
	ret = driver_malloc(handle, FW_DNLD_TX_BUF_SIZE,
			    DRIVER_MEM_DEF | DRIVER_MEM_DMA,
			    (t_u8 **) & fwdata);
	if ((ret != DRIVER_STATUS_SUCCESS) || !fwdata) {
		PRINTM(MERROR, "Could not allocate buffer for FW download\n");
		goto fw_exit;
	}

	/* Allocate memory for receive */
	ret = driver_malloc(handle, FW_DNLD_RX_BUF_SIZE,
			    DRIVER_MEM_DEF | DRIVER_MEM_DMA, &RecvBuff);
	if ((ret != DRIVER_STATUS_SUCCESS) || !RecvBuff) {
		PRINTM(MERROR,
		       "Could not allocate buffer for FW download response\n");
		goto cleanup;
	}

	do {
		/* Send pseudo data to check winner status first */
		if (check_winner) {
			memset(&fwdata->fw_header, 0, sizeof(FWHeader));
			DataLength = 0;
		} else {
			/* Copy the header of the firmware data to get the
			   length */
			if (firmware)
				memcpy(&fwdata->fw_header,
				       &firmware[TotalBytes], sizeof(FWHeader));
			else
				driver_get_fw_data(handle, TotalBytes,
						   sizeof(FWHeader),
						   (t_u8 *) & fwdata->
						   fw_header);

			DataLength =
				driver_le32_to_cpu(fwdata->fw_header.
						   data_length);
			DnldCmd =
				driver_le32_to_cpu(fwdata->fw_header.dnld_cmd);
			TotalBytes += sizeof(FWHeader);

			/* Copy the firmware data */
			if (firmware)
				memcpy(fwdata->data, &firmware[TotalBytes],
				       DataLength);
			else
				driver_get_fw_data(handle, TotalBytes,
						   DataLength,
						   (t_u8 *) fwdata->data);

			fwdata->seq_num = driver_cpu_to_le32(FWSeqNum);
			TotalBytes += DataLength;
		}

		/* If the send/receive fails or CRC occurs then retry */
		while (retries) {
			driver_buffer mbuf;
			int length = FW_DATA_XMIT_SIZE;
			retries--;

			memset(&mbuf, 0, sizeof(driver_buffer));
			mbuf.pbuf = (t_u8 *) fwdata;
			mbuf.data_len = length;

			/* Send the firmware block */
			if ((ret =
			     driver_write_data_sync(handle, &mbuf,
						    DRIVER_USB_EP_CMD_EVENT,
						    DRIVER_USB_BULK_MSG_TIMEOUT))
			    != DRIVER_STATUS_SUCCESS) {
				PRINTM(MERROR,
				       "fw_dnld: write_data failed, ret %d\n",
				       ret);
				continue;
			}

			memset(&mbuf, 0, sizeof(driver_buffer));
			mbuf.pbuf = RecvBuff;
			mbuf.data_len = FW_DNLD_RX_BUF_SIZE;

			/* Receive the firmware block response */
			if ((ret =
			     driver_read_data_sync(handle, &mbuf,
						   DRIVER_USB_EP_CMD_EVENT,
						   DRIVER_USB_BULK_MSG_TIMEOUT))
			    != DRIVER_STATUS_SUCCESS) {
				PRINTM(MERROR,
				       "fw_dnld: read_data failed, ret %d\n",
				       ret);
				continue;
			}

			memcpy(&SyncFWHeader, RecvBuff, sizeof(FWSyncHeader));
			endian_convert_syncfwheader(&SyncFWHeader);

			/* Check the first firmware block response for highest
			   bit set */
			if (check_winner) {
				if (SyncFWHeader.cmd & 0x80000000) {
					PRINTM(MMSG,
					       "USB is not the winner 0x%x, returning success\n",
					       SyncFWHeader.cmd);
					ret = DRIVER_STATUS_SUCCESS;
					goto cleanup;
				}
				PRINTM(MINFO,
				       "USB is the winner, start to download FW\n");
				check_winner = 0;
				break;
			}

			/* Check the firmware block response for CRC errors */
			if (SyncFWHeader.cmd) {
				PRINTM(MERROR,
				       "FW received Blk with CRC error 0x%x\n",
				       SyncFWHeader.cmd);
				ret = DRIVER_STATUS_FAILURE;
				continue;
			}

			retries = MAX_FW_RETRY;
			break;
		}

		FWSeqNum++;
		PRINTM(MINFO, ".\n");

	} while ((DnldCmd != FW_HAS_LAST_BLOCK) && retries);

cleanup:
	PRINTM(MINFO, "fw_dnld: %d bytes downloaded\n", TotalBytes);

	if (RecvBuff)
		driver_mfree(handle, RecvBuff);
	if (fwdata)
		driver_mfree(handle, (t_u8 *) fwdata);
	if (retries) {
		ret = DRIVER_STATUS_SUCCESS;
	}

fw_exit:
	LEAVE();
	return ret;
}

/********************************************************
        Global Functions
********************************************************/
/**
 *  @brief Sets the configuration values
 *
 *  @param intf     Pointer to usb_interface
 *  @param id       Pointer to usb_device_id
 *
 *  @return         Address of variable usb_cardp, error code otherwise
 */
static int
driver_usb_probe(struct usb_interface *intf, const struct usb_device_id *id)
{
	struct usb_device *udev;
	struct usb_host_interface *iface_desc;
	struct usb_endpoint_descriptor *endpoint;
	int i;
	struct usb_card_rec *usb_cardp = NULL;

	ENTER();

	udev = interface_to_usbdev(intf);
	usb_cardp = kmalloc(sizeof(struct usb_card_rec), GFP_KERNEL);
	memset((t_u8 *) usb_cardp, 0, sizeof(struct usb_card_rec));
	if (!usb_cardp) {
		LEAVE();
		return (-ENOMEM);
	}

	udev->descriptor.idVendor =
		driver_cpu_to_le16(udev->descriptor.idVendor);
	udev->descriptor.idProduct =
		driver_cpu_to_le16(udev->descriptor.idProduct);
	udev->descriptor.bcdDevice =
		driver_cpu_to_le16(udev->descriptor.bcdDevice);
	udev->descriptor.bcdUSB = driver_cpu_to_le16(udev->descriptor.bcdUSB);

	if (udev->descriptor.idVendor == MARVELL_USB_VID &&
	    (udev->descriptor.idProduct == MARVELL_USB8766_PID ||
	     udev->descriptor.idProduct == MARVELL_USB8797_PID ||
	     udev->descriptor.idProduct == MARVELL_USB8897_PID)) {
		PRINTM(MMSG, "VID/PID = %X/%X, Boot2 version = %X\n",
		       udev->descriptor.idVendor, udev->descriptor.idProduct,
		       udev->descriptor.bcdDevice);
		usb_cardp->boot_state = MARVELL_USB_FW_DNLD;
		if (udev->descriptor.idProduct == MARVELL_USB8766_PID)
			usb_cardp->usb_dev_type = DEV_TYPE_USB8766;
		else if (udev->descriptor.idProduct == MARVELL_USB8797_PID)
			usb_cardp->usb_dev_type = DEV_TYPE_USB8797;
		else
			usb_cardp->usb_dev_type = DEV_TYPE_USB8897;
	} else {
		PRINTM(MERROR, "The Driver did not support this Device,"
		       "you can check whether your device is USB8766 or USB8797 or USB8897.\n");
		goto error;
	}

	if (udev->descriptor.idVendor) {
		usb_cardp->udev = udev;
		iface_desc = intf->cur_altsetting;
		usb_cardp->intf = intf;

		PRINTM(MINFO, "bcdUSB = 0x%X bDeviceClass = 0x%X"
		       " bDeviceSubClass = 0x%X, bDeviceProtocol = 0x%X\n",
		       udev->descriptor.bcdUSB,
		       udev->descriptor.bDeviceClass,
		       udev->descriptor.bDeviceSubClass,
		       udev->descriptor.bDeviceProtocol);

		for (i = 0; i < iface_desc->desc.bNumEndpoints; ++i) {
			endpoint = &iface_desc->endpoint[i].desc;

			if ((endpoint->bEndpointAddress & USB_ENDPOINT_DIR_MASK)
			    &&
			    ((endpoint->
			      bEndpointAddress & USB_ENDPOINT_NUMBER_MASK) ==
			     DRIVER_USB_EP_CMD_EVENT) &&
			    ((endpoint->
			      bmAttributes & USB_ENDPOINT_XFERTYPE_MASK) ==
			     USB_ENDPOINT_XFER_BULK)) {
				/* We found a bulk in command/event endpoint */
				PRINTM(MINFO,
				       "Bulk IN: max packet size = %d, address = %d\n",
				       driver_le16_to_cpu(endpoint->
							  wMaxPacketSize),
				       endpoint->bEndpointAddress);
				usb_cardp->rx_cmd_ep =
					(endpoint->
					 bEndpointAddress &
					 USB_ENDPOINT_NUMBER_MASK);
				atomic_set(&usb_cardp->rx_cmd_urb_pending, 0);
			}
			if (((endpoint->
			      bEndpointAddress & USB_ENDPOINT_DIR_MASK) ==
			     USB_DIR_OUT) &&
			    ((endpoint->
			      bEndpointAddress & USB_ENDPOINT_NUMBER_MASK) ==
			     DRIVER_USB_EP_CMD_EVENT) &&
			    ((endpoint->
			      bmAttributes & USB_ENDPOINT_XFERTYPE_MASK) ==
			     USB_ENDPOINT_XFER_BULK)) {
				/* We found a bulk out command/event endpoint */
				PRINTM(MINFO,
				       "Bulk OUT: max packet size = %d, address = %d\n",
				       driver_le16_to_cpu(endpoint->
							  wMaxPacketSize),
				       endpoint->bEndpointAddress);
				usb_cardp->tx_cmd_ep =
					endpoint->bEndpointAddress;
				atomic_set(&usb_cardp->tx_cmd_urb_pending, 0);
				usb_cardp->bulk_out_maxpktsize =
					driver_le16_to_cpu(endpoint->
							   wMaxPacketSize);
			}

		}

		usb_set_intfdata(intf, usb_cardp);
		/* At this point driver_add_card() will be called */
		if (!(driver_add_card(usb_cardp))) {
			PRINTM(MERROR, "%s: driver_add_callback failed\n",
			       __FUNCTION__);
			goto error;
		}
		usb_get_dev(udev);
		LEAVE();
		return 0;
	} else {
		PRINTM(MINFO, "Discard the Probe request\n");
		PRINTM(MINFO, "VID = 0x%X PID = 0x%X\n",
		       udev->descriptor.idVendor, udev->descriptor.idProduct);
	}

error:
	kfree(usb_cardp);
	usb_cardp = NULL;
	LEAVE();
	return (-ENOMEM);
}

/**
 *  @brief Free resource and cleanup
 *
 *  @param intf     Pointer to usb_interface
 *
 *  @return         N/A
 */
static void
driver_usb_disconnect(struct usb_interface *intf)
{
	struct usb_card_rec *cardp = usb_get_intfdata(intf);
	driver_handle *phandle = NULL;
	ENTER();
	if (!cardp || !cardp->phandle) {
		PRINTM(MERROR, "Card or phandle is not valid\n");
		LEAVE();
		return;
	}
	phandle = (driver_handle *) cardp->phandle;

	/*
	 * Update Surprise removed to TRUE
	 *  Free all the URB's allocated
	 */
	phandle->surprise_removed = MTRUE;

	/* Unlink and free urb */
	driver_usb_free(cardp);

	/* Card is removed and we can call driver_remove_card */
	PRINTM(MINFO, "Call remove card\n");
	driver_remove_card(cardp);

	usb_set_intfdata(intf, NULL);
	usb_put_dev(interface_to_usbdev(intf));
	kfree(cardp);
	LEAVE();
	return;
}

/**
 *  @brief  This function downloads data blocks to device
 *
 *  @param handle   Pointer to driver_handle structure
 *  @param pmbuf    Pointer to driver_buffer structure
 *  @param ep       Endpoint to send
 *  @param timeout  Timeout value in milliseconds (if 0 the wait is forever)
 *
 *  @return         DRIVER_STATUS_SUCCESS or DRIVER_STATUS_FAILURE
 */
driver_status
driver_write_data_sync(driver_handle * handle, driver_buffer * pmbuf, t_u8 ep,
		       t_u32 timeout)
{
	struct usb_card_rec *cardp = handle->card;
	t_u8 *data = (t_u8 *) (pmbuf->pbuf + pmbuf->data_offset);
	t_u32 length = pmbuf->data_len;
	int actual_length;
	driver_status ret = DRIVER_STATUS_SUCCESS;

	if (length % cardp->bulk_out_maxpktsize == 0)
		length++;
	/* Send the data block */
	if ((ret = usb_bulk_msg(cardp->udev, usb_sndbulkpipe(cardp->udev,
							     ep), (t_u8 *) data,
				length, &actual_length, timeout))) {
		PRINTM(MERROR, "usb_blk_msg for send failed, ret %d\n", ret);
		ret = DRIVER_STATUS_FAILURE;
	}
	pmbuf->data_len = actual_length;

	return ret;
}

/**
 *  @brief  This function read data blocks from device
 *
 *  @param handle   Pointer to driver_handle structure
 *  @param pmbuf    Pointer to driver_buffer structure
 *  @param ep       Endpoint to receive
 *  @param timeout  Timeout value in milliseconds (if 0 the wait is forever)
 *
 *  @return         DRIVER_STATUS_SUCCESS or DRIVER_STATUS_FAILURE
 */
driver_status
driver_read_data_sync(driver_handle * handle, driver_buffer * pmbuf, t_u8 ep,
		      t_u32 timeout)
{
	struct usb_card_rec *cardp = handle->card;
	t_u8 *data = (t_u8 *) (pmbuf->pbuf + pmbuf->data_offset);
	t_u32 buf_len = pmbuf->data_len;
	int actual_length;
	driver_status ret = DRIVER_STATUS_SUCCESS;
	ENTER();
	/* Receive the data response */
	if ((ret = usb_bulk_msg(cardp->udev, usb_rcvbulkpipe(cardp->udev,
							     ep), data,
				buf_len, &actual_length, timeout))) {
		PRINTM(MERROR, "usb_bulk_msg failed: %d\n", ret);
		ret = DRIVER_STATUS_FAILURE;
	}
	pmbuf->data_len = actual_length;
	LEAVE();
	return ret;
}

/**
 *  @brief  Check chip revision
 *
 *  @param handle        A pointer to driver_handle structure
 *  @param usb_chip_rev  A pointer to usb_chip_rev variable
 *
 *  @return              DRIVER_STATUS_SUCCESS or DRIVER_STATUS_FAILURE
 */
driver_status
driver_check_chip_revision(driver_handle * handle, t_u32 * usb_chip_rev)
{
	driver_status ret = DRIVER_STATUS_SUCCESS;
	driver_buffer mbuf;
	t_u8 *tx_buff = 0;
	t_u8 *recv_buff = 0;
	usb_ack_pkt ack_pkt;
	t_u32 extend = (EXTEND_HDR << 16) | EXTEND_V1;

	ENTER();

	/* Allocate memory for transmit */
	if ((tx_buff =
	     kzalloc(CHIP_REV_TX_BUF_SIZE, GFP_ATOMIC | GFP_DMA)) == NULL) {
		PRINTM(MERROR,
		       "Could not allocate buffer for chip revision check frame transmission\n");
		ret = DRIVER_STATUS_FAILURE;
		goto cleanup;
	}

	/* Allocate memory for receive */
	if ((recv_buff =
	     kzalloc(CHIP_REV_RX_BUF_SIZE, GFP_ATOMIC | GFP_DMA)) == NULL) {
		PRINTM(MERROR,
		       "Could not allocate buffer for chip revision check frame response\n");
		ret = DRIVER_STATUS_FAILURE;
		goto cleanup;
	}

	/* The struct is initialised to all zero */
	memset(&ack_pkt, 0, sizeof(usb_ack_pkt));

	/* Send pseudo data to check winner status first */
	memset(&mbuf, 0, sizeof(driver_buffer));
	mbuf.pbuf = (t_u8 *) tx_buff;
	mbuf.data_len = CHIP_REV_TX_BUF_SIZE;

	/* Send the chip revision check frame */
	if ((ret = driver_write_data_sync(handle,
					  &mbuf, DRIVER_USB_EP_CMD_EVENT,
					  DRIVER_USB_BULK_MSG_TIMEOUT)) !=
	    DRIVER_STATUS_SUCCESS) {
		PRINTM(MERROR,
		       "Chip revision check frame dnld: write_data failed, ret %d\n",
		       ret);
		ret = DRIVER_STATUS_FAILURE;
		goto cleanup;
	}

	memset(&mbuf, 0, sizeof(driver_buffer));
	mbuf.pbuf = (t_u8 *) recv_buff;
	mbuf.data_len = CHIP_REV_RX_BUF_SIZE;

	/* Receive the chip revision check frame response */
	if ((ret = driver_read_data_sync(handle,
					 &mbuf, DRIVER_USB_EP_CMD_EVENT,
					 DRIVER_USB_BULK_MSG_TIMEOUT)) !=
	    DRIVER_STATUS_SUCCESS) {
		PRINTM(MERROR,
		       "Chip revision check frame response: read_data failed, ret %d\n",
		       ret);
		ret = DRIVER_STATUS_FAILURE;
		goto cleanup;
	}

	memcpy(&ack_pkt, recv_buff, sizeof(usb_ack_pkt));
	ack_pkt.ack_winner = driver_le32_to_cpu(ack_pkt.ack_winner);
	ack_pkt.seq = driver_le32_to_cpu(ack_pkt.seq);
	ack_pkt.extend = driver_le32_to_cpu(ack_pkt.extend);
	ack_pkt.chip_rev = driver_le32_to_cpu(ack_pkt.chip_rev);

	if (ack_pkt.extend == extend)
		*usb_chip_rev = ack_pkt.chip_rev;
	else
		*usb_chip_rev = USB8797_A0;

cleanup:
	if (recv_buff)
		kfree(recv_buff);
	if (tx_buff)
		kfree(tx_buff);

	LEAVE();
	return ret;
}

/**
 *  @brief This function dynamically populates the driver FW name
 *
 *  @param handle           A pointer to driver_handle structure
 *
 *  @return        DRIVER_STATUS_SUCCESS or DRIVER_STATUS_FAILURE
 */
static driver_status
driver_update_fw_name(driver_handle * handle)
{
	driver_status ret = DRIVER_STATUS_SUCCESS;
	t_u32 revision_id = 0;

	ENTER();

	/* Update FW name */
	if (fw_name) {
		handle->fw_name = fw_name;
	} else {
		if (handle->usb_dev_type == DEV_TYPE_USB8766) {
			handle->fw_name = USB8766_DEFAULT_FW_NAME;
		} else if (handle->usb_dev_type == DEV_TYPE_USB8797) {
			if ((ret =
			     driver_check_chip_revision(handle,
							&revision_id)) !=
			    DRIVER_STATUS_SUCCESS) {
				PRINTM(MFATAL,
				       "USB8797 Chip revision check failure!\n");
				ret = DRIVER_STATUS_FAILURE;
			} else {
				/* Check revision ID */
				switch (revision_id) {
				case USB8797_A0:
					handle->fw_name =
						USB8797_A0_DEFAULT_FW_NAME;
					break;
				case USB8797_B0:
					handle->fw_name =
						USB8797_B0_DEFAULT_FW_NAME;
					break;
				default:
					break;
				}
			}
		} else if (handle->usb_dev_type == DEV_TYPE_USB8897) {
			handle->fw_name = USB8897_DEFAULT_FW_NAME;
		} else {
			PRINTM(MERROR, "Can not find the default fw name,"
			       "you can check whether your device is USB8766 or USB8797 or USB8897.\n");
		}
	}

	LEAVE();
	return ret;
}

/**
 *  @brief This function frees the structure of driver_handle
 *
 *  @param handle   A pointer to driver_handle structure
 *
 *  @return         N/A
 */
static void
driver_free_driver_handle(driver_handle * handle)
{
	ENTER();
	if (!handle) {
		PRINTM(MERROR, "The handle is NULL\n");
		LEAVE();
		return;
	}

	if (handle->malloc_count || handle->mbufalloc_count) {
		PRINTM(MERROR,
		       "driver has memory leak: malloc_count=%u  mbufalloc_count=%u\n",
		       handle->malloc_count, handle->mbufalloc_count);
	}
	/* Free the driver handle itself */
	kfree(handle);
	LEAVE();
}

/**
 * @brief Download and Initialize firmware DPC
 *
 * @param handle    A pointer to driver_handle structure
 *
 * @return        DRIVER_STATUS_SUCCESS or DRIVER_STATUS_FAILURE
 */
static driver_status
driver_init_fw_dpc(driver_handle * handle)
{
	driver_status ret = DRIVER_STATUS_SUCCESS;
	driver_fw_image fw;

	ENTER();

	if (handle->firmware) {
		memset(&fw, 0, sizeof(driver_fw_image));
		fw.pfw_buf = (t_u8 *) handle->firmware->data;
		fw.fw_len = handle->firmware->size;
		ret = driver_prog_fw_w_helper(handle, &fw);
		if (ret != DRIVER_STATUS_SUCCESS) {
			PRINTM(MERROR,
			       "WLAN: Download FW with nowait: %d failed. ret = 0x%x\n",
			       req_fw_nowait, ret);
			goto done;
		}

		if (handle->boot_state == MARVELL_USB_FW_DNLD) {
			struct usb_device *udev =
				((struct usb_card_rec *)(handle->card))->udev;
			int usbRstDev_ret = 0;

			/* Return now */
			PRINTM(MMSG, "FW is downloaded\n");

			/* Reset USB device to get re-enumeration */
			if (udev) {
#define USB_WAIT_FW_READY  (500)
				mdelay(USB_WAIT_FW_READY);
				usbRstDev_ret = usb_reset_device(udev);
				if ((usbRstDev_ret == 0) || (usbRstDev_ret == -ENODEV) ||	// expected
												// since
												// chip
												// re-enumerates
				    (usbRstDev_ret == -EINVAL)) {	// expected
									// if
									// USB
									// FW
									// detaches
									// first
					PRINTM(MMSG,
					       "usb_reset_device() successful.\n");
				} else {
					PRINTM(MERROR,
					       "usb_reset_device() failed with error code =%d\n",
					       usbRstDev_ret);
					ret = DRIVER_STATUS_FAILURE;
				}
			} else {
				PRINTM(MERROR,
				       "ERR: No handle to call usb_reset_device()!\n");
				ret = DRIVER_STATUS_FAILURE;
			}
			goto done;
		} else {
			PRINTM(MMSG, "FW is active\n");
		}
	}

	ret = DRIVER_STATUS_SUCCESS;
done:
	LEAVE();
	return ret;
}

/**
 * @brief Request firmware DPC
 *
 * @param handle    A pointer to driver_handle structure
 * @param firmware  A pointer to firmware image
 *
 * @return        DRIVER_STATUS_SUCCESS or DRIVER_STATUS_FAILURE
 */
static driver_status
driver_request_fw_dpc(driver_handle * handle, const struct firmware *firmware)
{
	driver_status ret = DRIVER_STATUS_SUCCESS;
	struct timeval tstamp;

	ENTER();

	if (!firmware) {
		do_gettimeofday(&tstamp);
		if (tstamp.tv_sec >
		    (handle->req_fw_time.tv_sec + REQUEST_FW_TIMEOUT)) {
			PRINTM(MERROR,
			       "No firmware image found. Skipping download\n");
			ret = DRIVER_STATUS_FAILURE;
			goto done;
		}
		PRINTM(MERROR,
		       "request_firmware_nowait failed for %s. Retrying..\n",
		       handle->fw_name);
		driver_sched_timeout(DRIVER_TIMER_1S);
		driver_request_fw(handle);
		LEAVE();
		return ret;
	}
	handle->firmware = firmware;

	ret = driver_init_fw_dpc(handle);

done:
	/* We should hold the semaphore until callback finishes execution */
	DRIVER_REL_SEMAPHORE(&AddRemoveCardSem);
	LEAVE();
	return ret;
}

/**
 * @brief Request firmware callback
 *        This function is invoked by request_firmware_nowait system call
 *
 * @param firmware  A pointer to firmware image
 * @param context   A pointer to driver_handle structure
 *
 * @return        N/A
 */
static void
driver_request_fw_callback(const struct firmware *firmware, void *context)
{
	ENTER();
	driver_request_fw_dpc((driver_handle *) context, firmware);
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,32)
	if (firmware)
		release_firmware(firmware);
#endif
	LEAVE();
	return;
}

/**
 * @brief   Download firmware using helper
 *
 * @param handle  A pointer to driver_handle structure
 *
 * @return        DRIVER_STATUS_SUCCESS or DRIVER_STATUS_FAILURE
 */
driver_status
driver_request_fw(driver_handle * handle)
{
	driver_status ret = DRIVER_STATUS_SUCCESS;
	int err;

	ENTER();

	if (req_fw_nowait) {
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,32)
		if ((err =
		     request_firmware_nowait(THIS_MODULE, FW_ACTION_HOTPLUG,
					     handle->fw_name,
					     handle->hotplug_device, GFP_KERNEL,
					     handle,
					     driver_request_fw_callback)) < 0) {
#else
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,13)
		if ((err =
		     request_firmware_nowait(THIS_MODULE, FW_ACTION_HOTPLUG,
					     handle->fw_name,
					     handle->hotplug_device, handle,
					     driver_request_fw_callback)) < 0) {
#else
		if ((err = request_firmware_nowait(THIS_MODULE,
						   handle->fw_name,
						   handle->hotplug_device,
						   handle,
						   driver_request_fw_callback))
		    < 0) {
#endif
#endif
			PRINTM(MFATAL,
			       "WLAN: request_firmware_nowait() failed, error code = %d\n",
			       err);
			ret = DRIVER_STATUS_FAILURE;
		}
	} else {
		if ((err =
		     request_firmware(&handle->firmware, handle->fw_name,
				      handle->hotplug_device)) < 0) {
			PRINTM(MFATAL,
			       "WLAN: request_firmware() failed, error code = %d\n",
			       err);
			ret = DRIVER_STATUS_FAILURE;
		} else {
			ret = driver_request_fw_dpc(handle, handle->firmware);
			release_firmware(handle->firmware);
		}
	}

	LEAVE();
	return ret;
}

/**
 *  @brief This function initializes firmware
 *
 *  @param handle  A pointer to driver_handle structure
 *
 *  @return        DRIVER_STATUS_SUCCESS or DRIVER_STATUS_FAILURE
 */
static driver_status
driver_init_fw(driver_handle * handle)
{
	driver_status ret = DRIVER_STATUS_SUCCESS;

	ENTER();

	if (MTRUE && (handle->skip_fw_dnld != MTRUE)
	    && (handle->boot_state != MARVELL_USB_FW_READY)) {
		do_gettimeofday(&handle->req_fw_time);
		if ((ret = driver_request_fw(handle)) < 0) {
			PRINTM(MFATAL, "driver_request_fw failed\n");
			ret = DRIVER_STATUS_FAILURE;
			goto done;
		}
	} else {
		PRINTM(MMSG, "FW is active\n");
		/* Set it to NULL to skip downloading firmware to card */
		handle->firmware = NULL;
		if ((ret = driver_init_fw_dpc(handle)))
			goto done;
		/* Release semaphore if download is not required */
		DRIVER_REL_SEMAPHORE(&AddRemoveCardSem);
	}
done:
	LEAVE();
	return ret;
}

/**
 * @brief This function adds the card. it will probe the
 * card, allocate the driver_private and initialize the device.
 *
 *  @param card    A pointer to card
 *
 *  @return        A pointer to driver_handle structure
 */
driver_handle *
driver_add_card(struct usb_card_rec * card)
{
	driver_handle *handle = NULL;
	int index = 0;

	ENTER();

	if (DRIVER_ACQ_SEMAPHORE_BLOCK(&AddRemoveCardSem))
		goto exit_sem_err;

	/* Allocate buffer for driver_handle */
	if (!(handle = kmalloc(sizeof(driver_handle), GFP_KERNEL))) {
		PRINTM(MERROR, "Allocate buffer for driver_handle failed!\n");
		goto err_handle;
	}

	/* Init driver_handle */
	memset(handle, 0, sizeof(driver_handle));
	handle->card = card;
	/* Save the handle */
	for (index = 0; index < MAX_DRIVER_ADAPTER; index++) {
		if (m_handle[index] == NULL)
			break;
	}
	if (index < MAX_DRIVER_ADAPTER) {
		m_handle[index] = handle;
		handle->handle_idx = index;
	} else {
		PRINTM(MERROR, "Exceeded maximum cards supported!\n");
		goto err_kmalloc;
	}

	handle->usb_dev_type = card->usb_dev_type;

	/* Init SW */
	handle->hardware_status = HardwareStatusInitializing;

	if (driver_update_fw_name(handle) != DRIVER_STATUS_SUCCESS) {
		PRINTM(MERROR, "Could not update driver FW name\n");
		PRINTM(MFATAL, "Software Init Failed\n");
		goto err_kmalloc;
	}

	handle->boot_state = card->boot_state;

	/* Register the device. Fill up the private data structure with
	   relevant information from the card and request for the required IRQ.
	 */
	card->phandle = handle;
	handle->hotplug_device = &(card->udev->dev);

	/* Init FW and HW */
	if (DRIVER_STATUS_SUCCESS != driver_init_fw(handle)) {
		PRINTM(MFATAL, "Firmware Init Failed\n");
		goto err_init_fw;
	}

	LEAVE();
	return handle;

err_init_fw:
	handle->surprise_removed = MTRUE;
err_kmalloc:
	if ((handle->hardware_status == HardwareStatusFwReady) ||
	    (handle->hardware_status == HardwareStatusReady)) {
		handle->hardware_status = HardwareStatusNotReady;
	}
	driver_usb_free(card);
	driver_free_driver_handle(handle);
	if (index < MAX_DRIVER_ADAPTER) {
		m_handle[index] = NULL;
	}
err_handle:
	DRIVER_REL_SEMAPHORE(&AddRemoveCardSem);
exit_sem_err:
	LEAVE();
	return NULL;
}

/**
 *  @brief This function removes the card.
 *
 *  @param card    A pointer to card
 *
 *  @return        DRIVER_STATUS_SUCCESS
 */
driver_status
driver_remove_card(void *card)
{
	driver_handle *handle = NULL;
	int index = 0;

	ENTER();

	if (DRIVER_ACQ_SEMAPHORE_BLOCK(&AddRemoveCardSem))
		goto exit_sem_err;
	/* Find the correct handle */
	for (index = 0; index < MAX_DRIVER_ADAPTER; index++) {
		if (m_handle[index] && (m_handle[index]->card == card)) {
			handle = m_handle[index];
			break;
		}
	}
	if (!handle)
		goto exit_remove;
	handle->surprise_removed = MTRUE;

	if ((handle->hardware_status == HardwareStatusFwReady) ||
	    (handle->hardware_status == HardwareStatusReady)) {
		handle->hardware_status = HardwareStatusNotReady;
	}

	/* Free handle structure */
	PRINTM(MINFO, "Free handle\n");
	driver_free_driver_handle(handle);

	for (index = 0; index < MAX_DRIVER_ADAPTER; index++) {
		if (m_handle[index] == handle) {
			m_handle[index] = NULL;
			break;
		}
	}
exit_remove:
	DRIVER_REL_SEMAPHORE(&AddRemoveCardSem);
exit_sem_err:
	LEAVE();
	return DRIVER_STATUS_SUCCESS;
}

/**
 *  @brief This function initializes module.
 *
 *  @return        DRIVER_STATUS_SUCCESS or DRIVER_STATUS_FAILURE
 */
static int
driver_init_module(void)
{
	int ret = (int)DRIVER_STATUS_SUCCESS;
	int index = 0;

	ENTER();

	/* Init the driver_private pointer array first */
	for (index = 0; index < MAX_DRIVER_ADAPTER; index++) {
		m_handle[index] = NULL;
	}
	/* Init mutex */
	DRIVER_INIT_SEMAPHORE(&AddRemoveCardSem);

	/* Register with USB bus */
	if (usb_register(&driver_usb_driver)) {
		PRINTM(MFATAL, "USB Driver Registration Failed \n");
		ret = DRIVER_STATUS_FAILURE;
	}

	LEAVE();
	return ret;
}

/**
 *  @brief This function cleans module
 *
 *  @return        N/A
 */
static void
driver_cleanup_module(void)
{
	driver_handle *handle = NULL;
	int index = 0;

	ENTER();

	if (DRIVER_ACQ_SEMAPHORE_BLOCK(&AddRemoveCardSem))
		goto exit_sem_err;
	for (index = 0; index < MAX_DRIVER_ADAPTER; index++) {
		handle = m_handle[index];
		if (!handle)
			continue;
		if (!handle->priv_num)
			goto exit;

	}

exit:
	DRIVER_REL_SEMAPHORE(&AddRemoveCardSem);
exit_sem_err:
	/* Unregister from bus */
	usb_deregister(&driver_usb_driver);
	LEAVE();
}

module_init(driver_init_module);
module_exit(driver_cleanup_module);

module_param(fw_name, charp, 0);
MODULE_PARM_DESC(fw_name, "Firmware name");
module_param(req_fw_nowait, int, 0);
MODULE_PARM_DESC(req_fw_nowait,
		 "0: Use request_firmware API; 1: Use request_firmware_nowait API");
MODULE_DESCRIPTION("Bluetooth HCI USB Fw Download driver ver " VERSION);
MODULE_AUTHOR("Marvell International Ltd.");
MODULE_LICENSE("GPL");
