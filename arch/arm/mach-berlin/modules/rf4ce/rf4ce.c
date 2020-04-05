/*
 * Support Marvell RF4CE SPI driver
 *
 * Copyright (c) 2013 MARVELL
 *	Yuzhong Sun, <yzsun@marvell.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#define DEBUG
#include <linux/version.h>
#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/kthread.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/spi/spi.h>
#include <linux/usb/ch9.h>
#include <linux/hid.h>
#include <linux/delay.h>
#include <linux/string.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/notifier.h>
#include <linux/reboot.h>
#include <linux/gpio.h>
#include <linux/timer.h>
#include <asm/io.h>
#ifdef USE_GPIO_OLD_API
#include <mach/gpio.h>
#endif
#include "rf4ce_spi.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Yuzhong Sun <yzsun@marvell.com>");
MODULE_DESCRIPTION("RF4CE SPI driver");
MODULE_ALIAS("spi:rf4ce");
static char *rf4ce_version ="0.0.0.11";
module_param(rf4ce_version,charp,S_IRUSR);
static int rf4ce_debug = 0;
module_param(rf4ce_debug,int, S_IRUGO|S_IWUSR);

static struct rf4ce_common_ rf4ce_common;
static const uint8_t transfer_indicate = 0xaa;
/*For data resend when got rx receive error*/
static void *spi_backup_data_buf = NULL;
static size_t spi_backup_data_len = 0;
static spi_header_t spi_backup_packet_header;
/*For SPI device Config*/
#define SPI_BUS_NUM 1

#if defined (BG2Q) || defined (BG3CD)
static struct spi_board_info rf4ce_spi_board_info  = {
                 .modalias       = "rf4ce_spi",
                 .bus_num        = SPI_BUS_NUM,
                 .chip_select    = 2,
                 .max_speed_hz   = 10000000,
                 .platform_data  = &rf4ce_common,
				 .mode           = SPI_MODE_0,
};
#else
static struct spi_board_info rf4ce_spi_board_info  = {
                 .modalias       = "rf4ce_spi",
                 .bus_num        = SPI_BUS_NUM,
                 .chip_select    = 3,
                 .max_speed_hz   = 20000000,
                 .platform_data  = &rf4ce_common,
				 .mode           = SPI_MODE_0,
};
#endif
extern void rf4ce_handle_spi_data(struct rf4ce_common_ *common,spi_header_t spi_packet_header,uint8_t *spi_payload,uint8_t crc);

/*This function has not been implemented which will be called from input sys input_handle_event()*/
static int rf4ce_hidinput_event(struct input_dev *dev, unsigned int type, unsigned int code, int value)
{
//	struct hid_device *hid = input_get_drvdata(dev);
//	struct rf4ce_common_ *common = hid->driver_data;
	rf4ce_printk("hidp_hidinput_event is called but not implemented,type:%d,code:%d,value:%d\n",type,code,value);
	return 0;
}

static int rf4ce_hid_ll_open(struct hid_device *hid)
{
	return 0;
}

static void rf4ce_hid_ll_close(struct hid_device *hid)
{
}

/*this function will be called when create hid device*/
static int rf4ce_hid_ll_parse(struct hid_device *hid)
{
	struct rf4ce_common_ *common = hid->driver_data;

	return hid_parse_report(common->hid, common->report_descriptor, common->report_descriptor_length);
}

static int rf4ce_hid_ll_start(struct hid_device *hid)
{
	return 0;
}

static void rf4ce_hid_ll_stop(struct hid_device *hid)
{
	hid->claimed = 0;
}

static struct hid_ll_driver rf4ce_hid_ll_driver = {
	.parse = rf4ce_hid_ll_parse,
	.start = rf4ce_hid_ll_start,
	.stop = rf4ce_hid_ll_stop,
	.open  = rf4ce_hid_ll_open,
	.close = rf4ce_hid_ll_close,
	.hidinput_input_event = rf4ce_hidinput_event,
};

static int rf4ce_spi_read(struct spi_device *spi, void *buf, size_t len)
{
	int ret=0;
	ret = spi_read(spi,buf,len);
	return ret;
}

void check_rx_data_buf(struct spi_device *spi, uint8_t *buf, size_t len)
{
	size_t i=0;
	spi_header_t spi_packet_header;
	uint8_t *read_buf;
	uint16_t read_buf_length;
	uint16_t left_buf_length;
	uint16_t left_read_count;
	uint8_t crc;
	unsigned long flags;
	int error;
	for(i=0;i<len;i++){
		if(buf[i]){
			error = wait_event_interruptible(rf4ce_common.wait,kthread_should_stop() || rf4ce_common.spi_is_read);
			if(error){
				rf4ce_err_printk("failed to wait in check_rx_data_buf :%d\n",error);
				return;
			}
			if(rf4ce_common.spi_is_read){
				local_irq_save(flags);
				rf4ce_common.spi_is_read--;
				local_irq_restore(flags);
				rf4ce_err_printk("spi_is_read:%d\n",rf4ce_common.spi_is_read);
			}else{
				rf4ce_err_printk("thread should stop in check_rx_data_buf\n");
				return;
			}
			left_buf_length = len-i;
			rf4ce_printk("found not zero in rx buf when sending data,len:%d,i:%d\n",len,i);
			if(left_buf_length >= sizeof(spi_packet_header)){
				memcpy((void*)&spi_packet_header,(void*)&buf[i],sizeof(spi_packet_header));
				read_buf_length = sizeof(spi_packet_header)+spi_packet_header.length+sizeof(crc);
				read_buf = kmalloc(read_buf_length,GFP_KERNEL);
				if( read_buf_length > left_buf_length){
					left_read_count = read_buf_length-left_buf_length;
					memcpy(read_buf,(void*)&buf[i],left_buf_length);
					rf4ce_spi_read(spi,&read_buf[left_buf_length],left_read_count);
				}else{
					memcpy(read_buf,(void*)&buf[i],read_buf_length);
				}
			}else{
				memcpy((void*)&spi_packet_header,(void*)&buf[i],len-i);
				rf4ce_spi_read(spi,((uint8_t*)&spi_packet_header)+len-i,sizeof(spi_packet_header)-(len-i));
				read_buf_length = sizeof(spi_packet_header)+spi_packet_header.length+sizeof(crc);
				read_buf = kmalloc(read_buf_length,GFP_KERNEL);
				memcpy(read_buf,(void*)&spi_packet_header,sizeof(spi_packet_header));
				rf4ce_spi_read(spi,&read_buf[sizeof(spi_packet_header)],read_buf_length-sizeof(spi_packet_header));
			}
			/*read_buf: spi header+spi payload+crc*/
			rf4ce_err_printk("%d received during write, real length:%d, opcode:%d\n",left_buf_length,read_buf_length,spi_packet_header.opcode);
			rf4ce_dump(read_buf,read_buf_length);
			rf4ce_handle_spi_data(&rf4ce_common,spi_packet_header,&read_buf[sizeof(spi_packet_header)],read_buf[read_buf_length-sizeof(crc)]);
			kfree(read_buf);
			if(read_buf_length < left_buf_length){
				check_rx_data_buf(spi,&buf[i+read_buf_length],left_buf_length-read_buf_length);
			}
			break;
		}
	}
}
static int rf4ce_spi_write(struct spi_device *spi, const void *buf, size_t len)
{
	if(len>0){
		uint8_t rx_data[len];
		struct spi_transfer	t = {
				.rx_buf		= rx_data,
				.len		= len,
				.tx_buf     = buf,
			};
		struct spi_message	m;
		spi_message_init(&m);
		spi_message_add_tail(&t, &m);
		spi_sync(spi, &m);
		check_rx_data_buf(spi,rx_data,len);
		return len;
	}else{
		return 0;
	}
}


void rf4ce_write_data(struct rf4ce_common_ *common,spi_message_opcode_t opcode,const uint8_t *buf, size_t len)
{
	uint32_t i = 0, count = 0;
	uint8_t crc;
	spi_header_t spi_packet_header;
	uint8_t *write_buf;
	spi_packet_header.opcode = opcode;
	spi_packet_header.length = len;

	//Compute the CRC part
	while( i<len ){
		count += buf[i++];
	}
	crc = count%256;

	//Free pre-allocated backup data buf
	if(spi_backup_data_buf)
		kfree(spi_backup_data_buf);

	spi_backup_data_buf = kzalloc(len+1,GFP_KERNEL);
	if(spi_backup_data_buf){
		memcpy(spi_backup_data_buf,buf,len);
		memcpy(spi_backup_data_buf+len,&crc,sizeof(crc));
		spi_backup_data_len = len+1;
		memcpy(&spi_backup_packet_header,&spi_packet_header,sizeof(spi_packet_header));

		write_buf = kmalloc(sizeof(transfer_indicate)+sizeof(spi_backup_packet_header)+spi_backup_data_len,GFP_KERNEL);
		memcpy(write_buf,(void*)&transfer_indicate,sizeof(transfer_indicate));
		memcpy(write_buf+sizeof(transfer_indicate),(void*)&spi_backup_packet_header,sizeof(spi_backup_packet_header));
		memcpy(write_buf+sizeof(transfer_indicate)+sizeof(spi_backup_packet_header),spi_backup_data_buf,spi_backup_data_len);
		rf4ce_printk("send data,crc:%d\n",crc);
		rf4ce_dump(write_buf,sizeof(transfer_indicate)+sizeof(spi_backup_packet_header)+spi_backup_data_len);
		mutex_lock(&common->spi_mutex);
		rf4ce_spi_write(common->spidev,write_buf,sizeof(transfer_indicate)+sizeof(spi_backup_packet_header)+spi_backup_data_len);
		mutex_unlock(&common->spi_mutex);
		kfree(write_buf);
	}else{
		rf4ce_err_printk("failed to alloc for spi_backup_data_buf\n");
		write_buf = kmalloc(sizeof(transfer_indicate)+sizeof(spi_packet_header)+len+sizeof(crc),GFP_KERNEL);
		memcpy(write_buf,(void*)&transfer_indicate,sizeof(transfer_indicate));
		memcpy(write_buf+sizeof(transfer_indicate),(void*)&spi_packet_header,sizeof(spi_packet_header));
		memcpy(write_buf+sizeof(transfer_indicate)+sizeof(spi_packet_header),buf,len);
		memcpy(write_buf+sizeof(transfer_indicate)+sizeof(spi_packet_header)+len,&crc,sizeof(crc));
		rf4ce_printk("send data\n");
		rf4ce_dump(write_buf,sizeof(transfer_indicate)+sizeof(spi_packet_header)+len+sizeof(crc));
		mutex_lock(&common->spi_mutex);
		rf4ce_spi_write(common->spidev,write_buf,sizeof(transfer_indicate)+sizeof(spi_packet_header)+len+sizeof(crc));
		mutex_unlock(&common->spi_mutex);
		kfree(write_buf);
	}
}

static void rf4ce_write(struct rf4ce_common_ *common,spi_message_opcode_t opcode,const uint8_t *buf, size_t len)
{
	spi_write_data_item *spi_write_data;
	spi_write_data = kmalloc(sizeof(spi_write_data_item),GFP_KERNEL);
	spi_write_data->buf = kmalloc(len,GFP_KERNEL);
	spi_write_data->opcode = opcode;
	memcpy(spi_write_data->buf,buf,len);
	spi_write_data->buf_length = len;
	mutex_lock(&common->spi_write_list_mutex);
	common->spi_is_write++;
	list_add_tail(&spi_write_data->spi_write_workitem,&common->spi_write_list);
	mutex_unlock(&common->spi_write_list_mutex);
	wake_up_interruptible(&(common->wait));
}


static int rf4ce_hid_output_raw_report(struct hid_device *hid, unsigned char *data, size_t count,
		unsigned char report_type)
{
	struct rf4ce_common_ *common = hid->driver_data;
	unsigned char * buf;
	rf4ce_printk("rf4ce_hid_output_raw_report is called,count:%d,report_type:%d\n",count,report_type);
	rf4ce_dump(data,count);
	switch (report_type) {
	case HID_FEATURE_REPORT:
	case HID_OUTPUT_REPORT:
		rf4ce_printk("Send box_send_report_request SPI Message\n");
		buf = kmalloc(count+1,GFP_KERNEL);
		if(buf){
			//payload format: Report Type + Report Id + Report Data
			buf[0] = report_type;
			memcpy(buf+1,data,count);
			rf4ce_write(common,box_send_report_request,buf,count+1);
			kfree(buf);
		}else{
			rf4ce_err_printk("failed to alloc %d for rf4ce_hid_output_raw_report\n",count+1);
		}
		break;
	default:
		rf4ce_err_printk("unsupported report type\n");
		return -EINVAL;
	}
	return count;
}


static int rf4ce_create_hid(struct rf4ce_common_ *common)
{

	struct hid_device *hid;
	int err;


	hid = hid_allocate_device();
	if (IS_ERR(hid)) {
		err = PTR_ERR(hid);
		goto fault;
	}

	common->hid = hid;

	hid->driver_data = common;

	hid->bus     = BUS_BLUETOOTH;
	hid->vendor  = common->vendor;
	hid->product = common->product;
	hid->version = common->version;
	hid->country = common->country;

	strncpy(hid->name, "rf4ce-hid", 128);

//	strncpy(hid->phys, batostr(&bt_sk(session->ctrl_sock->sk)->src), 64);
//	strncpy(hid->uniq, batostr(&bt_sk(session->ctrl_sock->sk)->dst), 64);

	hid->dev.parent = &common->spidev->dev;
	hid->ll_driver = &rf4ce_hid_ll_driver;

	hid->hid_output_raw_report = rf4ce_hid_output_raw_report;

	err = hid_add_device(hid);
	if (err < 0)
		goto failed;

	return 0;

failed:
	hid_destroy_device(hid);
	common->hid = NULL;
fault:
	return err;
}

/*This function is used to check whether the CRC part of SPI message is correct*/
static int rf4ce_spi_data_crc_check(uint8_t *buf, uint32_t length, uint8_t crc)
{
	uint32_t i = 0, count = 0;
	if(!length)
		return 0;
	do{
		count += buf[i++];
	}while(i < length);

	count %= 256;
	rf4ce_printk("Expected CRC: %d, Real Got CRC: %d\n",count,crc);
	if(count == (uint32_t)crc)
		return 0;
	else{
		rf4ce_err_printk("Failed to check SPI CRC\n");
		return -1;
	}
}

void rf4ce_handle_spi_data(struct rf4ce_common_ *common,spi_header_t spi_packet_header,uint8_t *spi_payload,uint8_t crc)
{
	device_error_t device_error;
	device_spi_read_ack_t device_read_ack;
	if(!rf4ce_spi_data_crc_check(spi_payload,spi_packet_header.length,crc)){
		switch(spi_packet_header.opcode){
			case device_version_response:
				strncpy(common->dongle_version,spi_payload,DONGLE_VERSION_LENGTH);
				strncpy(common->remote_version,spi_payload+DONGLE_VERSION_LENGTH,REMOTE_VERSION_LENGTH);
				rf4ce_printk("Got device_version_response, dongle_version:%s,remote_version:%s\n",common->dongle_version,common->remote_version);
				break;
			case device_descriptor_response:
				common->vendor = ((struct usb_device_descriptor*)spi_payload)->idVendor;
				common->product = ((struct usb_device_descriptor*)spi_payload)->idProduct;
				rf4ce_printk("Got device_descriptor_response, idVendor:0x%x,idProduct:0x%x\n",common->vendor,common->product);
				rf4ce_printk("Send hid_descriptor_request SPI Message\n");
				rf4ce_write(common,hid_descriptor_request,NULL,0);
				common->rf4ce_state = HID_DESCRIPTOR_CHECK;
				break;
			case hid_descriptor_response:
				common->version = ((struct hid_descriptor*)spi_payload)->bcdHID;
				common->country = ((struct hid_descriptor*)spi_payload)->bCountryCode;
				rf4ce_printk("Got hid_descriptor_response, HID version:%x,country code:%x\n",common->version,common->country);
				rf4ce_printk("Send report_descriptor_request SPI Message\n");
				rf4ce_write(common,report_descriptor_request,NULL,0);
				common->rf4ce_state = REPORT_DESCRIPTOR_CHECK;
				break;
			case report_descriptor_response:
				common->report_descriptor = kmemdup(spi_payload,spi_packet_header.length,GFP_KERNEL);
				common->report_descriptor_length = spi_packet_header.length;
				rf4ce_printk("Got report_descriptor_response\n");
				//create hid device
				if(rf4ce_create_hid(common)){
					rf4ce_err_printk("Failed to create hid for RF4CE module\n");
					common->rf4ce_state = UNINITIALIZATION;
				}
				else{
					rf4ce_printk("Success to create hid for RF4CE module\n");
					common->rf4ce_state = HID_CREATE_DONE;
					rf4ce_printk("Send box_hid_create_indicate SPI Message\n");
					rf4ce_write(common,box_hid_create_indicate,NULL,0);
				}
				break;
			case device_status_change_event:
				rf4ce_printk("Got device_status_change_event,old status:%d, new status:%d\n",common->device_status,*spi_payload);

				common->device_status = *spi_payload;
				if( common->device_status == CONFIGURED ){
					if(common->rf4ce_state != HID_CREATE_DONE){
						rf4ce_printk("Send device_descriptor_request SPI Message\n");
						rf4ce_write(common,device_descriptor_request,NULL,0);
						common->rf4ce_state = DEVICE_DESCRIPTOR_CHECK;
					}
					else{
						rf4ce_printk("Send box_hid_create_indicate SPI Message\n");
						rf4ce_write(common,box_hid_create_indicate,NULL,0);
					}
				}else if( common->device_status == IDLE ){
					if(common->rf4ce_state == HID_CREATE_DONE){
						rf4ce_printk("Remove HID device for target idle status\n");
						if(rf4ce_common.hid){
							hid_destroy_device(rf4ce_common.hid);
							rf4ce_common.hid = NULL;
						}
					}
					common->rf4ce_state = UNINITIALIZATION;
				}
				else{
					rf4ce_printk("ignore such device status change,common->rf4ce_state:%d\n",common->rf4ce_state);
				}

				break;
			case device_report_indicate_event:
				rf4ce_printk("Got device_report_indicate_event\n");
				if(common->hid)
					hid_input_report(common->hid, HID_INPUT_REPORT, spi_payload, spi_packet_header.length, 1);
				else
					rf4ce_err_printk("common->hid is null when Got device_report_indicate_event");
				break;
			case device_error_indicate_event:
				rf4ce_err_printk("Got device_error_indicate_event ,error:%d\n",*spi_payload);
				device_error = *spi_payload;
				break;
			case device_spi_read_ack_indicate_event:
				device_read_ack = *spi_payload;
				if(device_read_ack == SPI_READ_FAILED){
					if(spi_backup_data_buf){
						uint8_t *write_buf;
						rf4ce_err_printk("device read failed\n");
						/*restart timer*/
						mod_timer(&common->timer, jiffies + SPI_READ_ACK_INTERVAL);
						write_buf = kmalloc(sizeof(transfer_indicate)+sizeof(spi_backup_packet_header)+spi_backup_data_len,GFP_KERNEL);
						memcpy(write_buf,(void*)&transfer_indicate,sizeof(transfer_indicate));
						memcpy(write_buf+sizeof(transfer_indicate),(void*)&spi_backup_packet_header,sizeof(spi_backup_packet_header));
						memcpy(write_buf+sizeof(transfer_indicate)+sizeof(spi_backup_packet_header),(void*)spi_backup_data_buf,spi_backup_data_len);
						mutex_lock(&common->spi_mutex);
						rf4ce_spi_write(common->spidev,write_buf,sizeof(transfer_indicate)+sizeof(spi_backup_packet_header)+spi_backup_data_len);
						mutex_unlock(&common->spi_mutex);
						kfree(write_buf);
					}else{
						rf4ce_err_printk("No data to send when got error read ack\n");
					}
				}else{
					rf4ce_printk("read ack\n");
					common->wait_for_device_read_ack = 0;
					/* Stop the timer */
					del_timer(&common->timer);
				}
				break;
			case device_ready_for_box_power_off_event:
				common->shutdown_complete = 1;
				wake_up_interruptible(&(common->shutdown_wait));
				break;
			default:
				rf4ce_err_printk("Got Unsupported SPI Message Opcode\n");
				break;
		}
	}
}

void rf4ce_read_spi_data(struct rf4ce_common_ *common)
{
//Firstly read a SPI header and check current statue
	spi_header_t spi_packet_header;
	uint8_t crc;
	uint8_t *spi_payload=NULL;
	rf4ce_printk("Read SPI Message Header:\n");
	mutex_lock(&common->spi_mutex);
	rf4ce_spi_read(common->spidev,(void*)&spi_packet_header,sizeof(spi_packet_header));
	rf4ce_dump(((char*)&spi_packet_header),sizeof(spi_packet_header));

	if(spi_packet_header.length){
		rf4ce_printk("Read SPI Message Payload:\n");
		spi_payload = kzalloc(spi_packet_header.length,GFP_KERNEL);
		if(spi_payload == NULL){
			rf4ce_err_printk("Can not allocate %d Byte for SPI read\n",spi_packet_header.length);
			mutex_unlock(&common->spi_mutex);
			return;
		}
		rf4ce_spi_read(common->spidev,spi_payload,spi_packet_header.length);
		rf4ce_dump(spi_payload,spi_packet_header.length);
	}

	rf4ce_printk("Read SPI Message CRC:\n");
	rf4ce_spi_read(common->spidev,&crc,sizeof(crc));
	rf4ce_dump(&crc,sizeof(crc));
	mutex_unlock(&common->spi_mutex);
	if(!common->shutdown_complete){
		rf4ce_handle_spi_data(common,spi_packet_header,spi_payload,crc);
	}else{
		rf4ce_printk("Ignore spi read data since shut down complete\n");
	}

	kfree(spi_payload);
	spi_payload = NULL;
	return ;
}

/*timer handler called in softirq env and can not be blocked*/
static void check_device_spi_read_ack(unsigned long data)
{
	struct rf4ce_common_ *common = (struct rf4ce_common_ *)data;
	if(common->wait_for_device_read_ack){
		common->spi_is_rewrite = 1;
		wake_up_interruptible(&(common->wait));
	}
}

static int rf4ce_spi_main_thread(void* common_)
{
	struct rf4ce_common_ *common = common_;
	int error;
	unsigned long flags;
	rf4ce_printk("rf4ce_spi_main_thread start\n");
	/* Allow the thread to be killed by a signal, but set the signal mask
	* to block everything but INT, TERM, KILL.*/
	allow_signal(SIGINT);
	allow_signal(SIGTERM);
	allow_signal(SIGKILL);

	while(!kthread_should_stop())
	{
		error = wait_event_interruptible(common->wait,kthread_should_stop() || common->spi_is_read || common->spi_is_write || common->spi_is_rewrite);
		if (error){
			rf4ce_err_printk("wait_event_interruptible is interrupt by signal and rf4ce_spi_main_thread exit\n");
			return -ERESTARTSYS;
		}

		if (common->spi_is_rewrite){
			if(spi_backup_data_buf){
				uint8_t *write_buf;
				rf4ce_err_printk("no device read ack and resend\n");
				write_buf = kmalloc(sizeof(transfer_indicate)+sizeof(spi_backup_packet_header)+spi_backup_data_len,GFP_KERNEL);
				memcpy(write_buf,(void*)&transfer_indicate,sizeof(transfer_indicate));
				memcpy(write_buf+sizeof(transfer_indicate),(void*)&spi_backup_packet_header,sizeof(spi_backup_packet_header));
				memcpy(write_buf+sizeof(transfer_indicate)+sizeof(spi_backup_packet_header),(void*)spi_backup_data_buf,spi_backup_data_len);
				mutex_lock(&common->spi_mutex);
				rf4ce_spi_write(common->spidev,write_buf,sizeof(transfer_indicate)+sizeof(spi_backup_packet_header)+spi_backup_data_len);
				mutex_unlock(&common->spi_mutex);
				kfree(write_buf);
				common->spi_is_rewrite = 0;
				/*restart timer*/
				mod_timer(&common->timer, jiffies + SPI_READ_ACK_INTERVAL);
			}else{
				rf4ce_err_printk("no data to be resent when no device read ack\n");
			}
		}else if (common->spi_is_read){
			local_irq_save(flags);
			common->spi_is_read--;
			local_irq_restore(flags);
			rf4ce_read_spi_data(common);
		}else if(common->spi_is_write && !common->wait_for_device_read_ack){
			mutex_lock(&common->spi_write_list_mutex);
			common->spi_is_write--;
			if(!list_empty(&common->spi_write_list)){
				spi_write_data_item* spi_write_data = list_entry(common->spi_write_list.next,spi_write_data_item,spi_write_workitem);
				list_del(common->spi_write_list.next);
				mutex_unlock(&common->spi_write_list_mutex);
				if(spi_write_data){
					common->wait_for_device_read_ack = 1;
					/* Init the timer */
					setup_timer(&common->timer, check_device_spi_read_ack, (unsigned long)common);
					/* Start the timer */
					mod_timer(&common->timer, jiffies + SPI_READ_ACK_INTERVAL);
					rf4ce_write_data(common,spi_write_data->opcode,spi_write_data->buf,spi_write_data->buf_length);					
					kfree(spi_write_data->buf);
					kfree(spi_write_data);
				}
			}else{
				rf4ce_err_printk("spi write list is empty\n");
				mutex_unlock(&common->spi_write_list_mutex);
			}
		}
	}

	rf4ce_printk("rf4ce_spi_main_thread exit\n");
	return 0;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,8,0)
static int rf4ce_spi_probe(struct spi_device *spidev)
#else
static int __devinit rf4ce_spi_probe(struct spi_device *spidev)
#endif
{
	rf4ce_printk("rf4ce_spi_probe is called\n");

	rf4ce_common.thread_task = kthread_create(rf4ce_spi_main_thread, &rf4ce_common, "rf4ce_spi");
	wake_up_process(rf4ce_common.thread_task);

	return 0;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,8,0)
static int rf4ce_spi_remove(struct spi_device *spidev)
#else
static int __devexit rf4ce_spi_remove(struct spi_device *spidev)
#endif
{
	rf4ce_printk("rf4ce_spi_remove is called\n");
	//wait for thread exit
	if(rf4ce_common.thread_task)
	kthread_stop(rf4ce_common.thread_task);
	return 0;
}

static int rf4ce_spi_suspend(struct spi_device *spi, pm_message_t mesg)
{
	rf4ce_printk("rf4ce_spi_suspend is called\n");
	return -1;
}

static int rf4ce_spi_resume(struct spi_device *spi)
{
	rf4ce_printk("rf4ce_spi_resume is called\n");
	return 0;
}

static struct spi_driver rf4ce_spi_driver = {
	.driver = {
		.name	= "rf4ce_spi",
		.owner	= THIS_MODULE,
	},
	.probe		= rf4ce_spi_probe,
	.suspend	= rf4ce_spi_suspend,
	.resume		= rf4ce_spi_resume,
	.remove		= rf4ce_spi_remove,
};

static const struct hid_device_id rf4ce_hid_table[] = {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,5,0)
	{ HID_DEVICE(BUS_SPI, HID_GROUP_ANY, HID_ANY_ID, HID_ANY_ID) },
#else
	{ HID_DEVICE(BUS_SPI, HID_ANY_ID, HID_ANY_ID) },
#endif
	{ }
};

static struct hid_driver rf4ce_hid_driver = {
	.name = "generic-spi",
	.id_table = rf4ce_hid_table,
};

static irqreturn_t rf4ce_spi_wakeup_interrupt(int irq, void *handle)
{
#ifdef USE_GPIO_OLD_API
	if(GPIO_PortHasInterrupt(GPIO_SPI_NOTIFY)){
		GPIO_PortClearInterrupt(GPIO_SPI_NOTIFY);
		rf4ce_common.spi_is_read++;
		wake_up_interruptible(&(rf4ce_common.wait));
//		rf4ce_printk("RF4CE interrupt,count:%d\n",rf4ce_common.spi_is_read);
		return IRQ_HANDLED;
	}else{
		return IRQ_NONE;
	}
#else
		rf4ce_common.spi_is_read++;
		wake_up_interruptible(&(rf4ce_common.wait));
		return IRQ_HANDLED;
#endif
}

static int rf4ce_gpio_config(void)
{
	int gpio_irq;

#ifdef USE_GPIO_OLD_API
	struct device_node *np;
	struct resource r;
	//Get GPIO Interrupt Num
	//defined in file /marvell-sdk/linux/arch/arm/boot/dts/berlin2ct.dtsi
	np = of_find_compatible_node(NULL, NULL, "berlin,apb-gpio");
	if (!np){
		rf4ce_err_printk("failed to of_find_compatible_node\n");
		return -1;
	}

	//0:GPIO 0-7, 1:GPIO 8-15, 2: 16-23, 3: 24-31
	of_irq_to_resource(np, GPIO_SPI_NOTIFY/8, &r);
	gpio_irq = r.start;
	of_node_put(np);

	/* Reigster GPIO inst0 interrupt */
	if (request_irq(gpio_irq, rf4ce_spi_wakeup_interrupt, IRQF_SHARED, "rf4ce_gpio", &rf4ce_common)) {
		printk("failed to request_irq %d.\n", gpio_irq);
		return -1;
	}

	rf4ce_common.rf4ce_gpio_irq = gpio_irq;

	//Set BG sends reset to ZB
	SM_GPIO_PortSetInOut(GPIO_ZB_RESETB, 0);
	//ZB wakes up BG, Enable IRQ WITH RISING EDGE
	GPIO_PortSetInOut(GPIO_SPI_NOTIFY, 1);
	GPIO_PortInitIRQ(GPIO_SPI_NOTIFY, 1, 1);

	GPIO_PortEnableIRQ(GPIO_SPI_NOTIFY);

#else

	if(gpio_request(GPIO_SPI_NOTIFY,"rf4ce_gpio")){
		rf4ce_err_printk("failed to gpio_request for rf4ce\n");
		return -1;
	}
	gpio_direction_input(GPIO_SPI_NOTIFY);
	gpio_irq = gpio_to_irq(GPIO_SPI_NOTIFY);
	if( gpio_irq<=0 ){
		rf4ce_err_printk("failed to gpio_to_irq for rf4ce\n");
		gpio_free(GPIO_SPI_NOTIFY);
		return -1;
	}

	if (request_irq(gpio_irq, rf4ce_spi_wakeup_interrupt, IRQF_SHARED||IRQF_TRIGGER_RISING, "rf4ce_gpio", &rf4ce_common)) {
		printk("failed to request_irq %d.\n", gpio_irq);
		gpio_free(GPIO_SPI_NOTIFY);
		return -1;
	}else{
		rf4ce_common.rf4ce_gpio_irq	= gpio_irq;
	}

	enable_irq_wake(gpio_irq);

#endif
	return 0;
}

static void rf4ce_gpio_deconfig(void)
{
#ifdef USE_GPIO_OLD_API
	GPIO_PortDisableIRQ(GPIO_SPI_NOTIFY);
#else
	gpio_free(GPIO_SPI_NOTIFY);
#endif
	free_irq(rf4ce_common.rf4ce_gpio_irq,&rf4ce_common);
}

static int rf4ce_reboot_callback(struct notifier_block *nb, unsigned long code, void *unused)
{
	int error;
	struct rf4ce_common_ *common = &rf4ce_common;
	if (common->rf4ce_state == HID_CREATE_DONE){
		rf4ce_write(common,box_power_off_indicate,NULL,0);
		error = wait_event_interruptible(common->shutdown_wait,common->shutdown_complete);
		if (error){
			rf4ce_err_printk("wait_event_interruptible shutdown_request is interrupt by signal\n");
		}else{
			rf4ce_printk("rf4ce shutdown complete\n");
		}
	}
	return NOTIFY_DONE;
}

static struct notifier_block rf4ce_reboot_notifier = {
		.notifier_call = rf4ce_reboot_callback
};


static __init int rf4ce_init(void)
{
	int ret =0;
	struct spi_master* rf4ce_master;
	rf4ce_master = spi_busnum_to_master(SPI_BUS_NUM);
	if(rf4ce_master){
		spi_register_driver(&rf4ce_spi_driver);
		init_waitqueue_head(&rf4ce_common.wait);
		init_waitqueue_head(&rf4ce_common.shutdown_wait);
		rf4ce_common.rf4ce_state = UNINITIALIZATION;
		mutex_init(&rf4ce_common.spi_mutex);
		mutex_init(&rf4ce_common.spi_write_list_mutex);
		INIT_LIST_HEAD(&rf4ce_common.spi_write_list);
		rf4ce_common.wait_for_device_read_ack = 0;
		if(!(rf4ce_common.spidev=spi_new_device(rf4ce_master,&rf4ce_spi_board_info))){
			rf4ce_err_printk("spi_new_device failed\n");
			spi_unregister_driver(&rf4ce_spi_driver);
			ret = -1;
		}else if(hid_register_driver(&rf4ce_hid_driver)){
			rf4ce_err_printk("hid_register_driver failed\n");
			spi_unregister_device(rf4ce_common.spidev);
			rf4ce_common.spidev = NULL;
			spi_unregister_driver(&rf4ce_spi_driver);
			ret = -1;
		}
		//Config GPIO
		if( !ret ){
			ret = rf4ce_gpio_config();
			if(!ret){
				register_reboot_notifier(&rf4ce_reboot_notifier);
			}else{
				spi_unregister_device(rf4ce_common.spidev);
				rf4ce_common.spidev = NULL;
				spi_unregister_driver(&rf4ce_spi_driver);
			}
		}
		put_device(&rf4ce_master->dev);
	}
	else{
		rf4ce_err_printk("can not find spi master\n");
		ret = -1;
	}

	return ret;
}

static __exit void rf4ce_exit(void)
{
	if(rf4ce_common.hid){
		hid_destroy_device(rf4ce_common.hid);
		rf4ce_common.hid = NULL;
	}

	if(rf4ce_common.spidev){
		spi_unregister_device(rf4ce_common.spidev);
		rf4ce_common.spidev = NULL;
	}

	unregister_reboot_notifier(&rf4ce_reboot_notifier);
	spi_unregister_driver(&rf4ce_spi_driver);
	hid_unregister_driver(&rf4ce_hid_driver);
	rf4ce_gpio_deconfig();
}

module_init(rf4ce_init);
module_exit(rf4ce_exit);
