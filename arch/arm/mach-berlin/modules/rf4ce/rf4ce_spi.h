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
#ifndef _RF4CE_SPI_H_
#define _RF4CE_SPI_H_

#define rf4ce_printk(format, ...)						\
		do{												\
			if(rf4ce_debug){							\
				printk(format, ##__VA_ARGS__);			\
			}											\
		}while(0)

#define CHARACTOR_PER_LINE 8
#define rf4ce_dump(buf_,count) 							    \
        do{												    \
			if(rf4ce_debug){							    \
				int i = 0;									\
				unsigned char *buf = (unsigned char*) buf_;	\
				while( i< count ){							\
				printk("0x%02x\t",buf[i++]);				\
				if(!(i%CHARACTOR_PER_LINE))					\
					printk("\n");							\
				}											\
				if(i%CHARACTOR_PER_LINE)					\
					printk("\n");							\
			}											    \
		}while(0)

#define rf4ce_err_printk(format, ...)						\
		do{													\
			printk(KERN_ERR format,##__VA_ARGS__);			\
		}while(0)

#define rf4ce_force_dump(buf_,count) 					    \
        do{												    \
			if(1){							    \
				int i = 0;									\
				unsigned char *buf = (unsigned char*) buf_;	\
				while( i< count ){							\
				printk("0x%02x\t",buf[i++]);				\
				if(!(i%CHARACTOR_PER_LINE))					\
					printk("\n");							\
				}											\
				if(i%CHARACTOR_PER_LINE)					\
					printk("\n");							\
			}											    \
		}while(0)
typedef enum {
		UNINITIALIZATION,
		VERSION_CHECK,
		DEVICE_DESCRIPTOR_CHECK,
		HID_DESCRIPTOR_CHECK,
		REPORT_DESCRIPTOR_CHECK,
		HID_CREATE_DONE,
} rf4ce_state_t;

typedef enum {
		POWERUP = 0x0,
		PARING,
		CONFIGURED,
		PROXY,
		ACTIVE,
		IDLE,
} device_status_t;

typedef enum {
                RX_ERROR = 0x0,

} device_error_t;

typedef enum  {
		SPI_READ_SUCCESS,
		SPI_READ_FAILED,
} device_spi_read_ack_t;

#define DONGLE_VERSION_LENGTH	8
#define REMOTE_VERSION_LENGTH	8

typedef enum  {
		device_status_request = 0x01,
		device_status_response,
		device_version_request,
		device_version_response,
		device_upgrade_dongle_request,
		device_upgrade_dongle_response,
		device_upgrade_remote_request,
		device_upgrade_remote_response,
		device_descriptor_request,
		device_descriptor_response,
		hid_descriptor_request,
		hid_descriptor_response,
		report_descriptor_request,
		report_descriptor_response,
		box_hid_create_indicate,
		box_send_report_request,
		box_power_off_indicate,

		device_status_change_event = 0x101,
		device_report_indicate_event,
		device_error_indicate_event,
		device_ready_for_box_power_off_event,
		device_spi_read_ack_indicate_event,
} spi_message_opcode_t;

typedef struct {
	struct list_head spi_write_workitem;
	spi_message_opcode_t opcode;
	void * buf;
	uint32_t buf_length;
} spi_write_data_item;

struct rf4ce_common_ {
		struct spi_device		*spidev;
		struct task_struct		*thread_task;
		wait_queue_head_t 		wait;
		wait_queue_head_t 		shutdown_wait;
		struct mutex 			spi_mutex;
		uint32_t				spi_is_read;
		uint32_t				spi_is_write;
		uint32_t				spi_is_rewrite;
		uint32_t				shutdown_complete;
		struct mutex 			spi_write_list_mutex;
		struct list_head 		spi_write_list;
		rf4ce_state_t			rf4ce_state;
		device_status_t			device_status;
		int						rf4ce_gpio_irq;
		char					*report_descriptor;
		uint32_t				report_descriptor_length;
		struct hid_device 		*hid;
		uint16_t 				vendor;		//Device Descriptor : Vendor ID
		uint16_t 				product;	//Device Descriptor : Product ID
		uint16_t 				version;	//HID Descriptor:     HID version
		uint8_t  				country;	//HID Descriptor:     HID country
		char					dongle_version[DONGLE_VERSION_LENGTH];
		char					remote_version[REMOTE_VERSION_LENGTH];
		char					device_err_code;
		uint8_t					wait_for_device_read_ack;
		struct timer_list		timer;
};



typedef struct {
		uint16_t	opcode;
		uint16_t	length;
} spi_header_t;

#define SPI_READ_ACK_INTERVAL msecs_to_jiffies(1000)

//For GPIO_ZB_RESETB and GPIO_BG_WKUP, it is SM_GPIO
#define	GPIO_ZB_RESETB	1
#define GPIO_BG_WKUP	9

#define SM_UART1_RX		10
#define SM_UART1_TX		11

//For GPIO_ZB_WKUP, it is normal GPIO
#ifdef USE_GPIO_OLD_API

#ifdef BG2Q
#define GPIO_ZB_WKUP 	50
#else
#define GPIO_ZB_WKUP 	6
#endif

#else

//check codes /vendor/marvell-sdk/linux/arch/arm/boot/dts/*.dts file to determine tha name of the gpio chip
//find the gpio chips in /sys/class/gpio/ by the label to determine whitch gpio should be used
#if defined BG2Q
#define GPIO_ZB_WKUP 	210
#elif defined BG3CD
#define GPIO_ZB_WKUP 	202     //GPIO42
#else
#define GPIO_ZB_WKUP 	254
#endif

#endif

#define GPIO_SPI_NOTIFY	GPIO_ZB_WKUP
#endif
