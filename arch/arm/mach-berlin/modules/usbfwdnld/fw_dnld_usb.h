/** @file fw_dnld_usb.h
 *
 *  @brief This file defines all the data structures and
 *  all the APIs for firmware download.
 *
 *  Copyright (C) 2008-2014, Marvell International Ltd.
 *
 *  This software file (the "File") is distributed by Marvell International
 *  Ltd. under the terms of the GNU General Public License Version 2, June 1991
 *  (the "License").  You may use, redistribute and/or modify this File in
 *  accordance with the terms and conditions of the License, a copy of which
 *  is available by writing to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA or on the
 *  worldwide web at http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt.
 *
 *  THE FILE IS DISTRIBUTED AS-IS, WITHOUT WARRANTY OF ANY KIND, AND THE
 *  IMPLIED WARRANTIES OF MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE
 *  ARE EXPRESSLY DISCLAIMED.  The License provides additional details about
 *  this warranty disclaimer.
 */

/******************************************************
Change log:
    02/20/2012: initial version
******************************************************/

/* Linux header files */
#include        <linux/kernel.h>
#include        <linux/module.h>
#include        <linux/init.h>
#include        <linux/version.h>
#include        <linux/param.h>
#include        <linux/delay.h>
#include        <linux/slab.h>
#include        <linux/mm.h>
#include        <linux/types.h>
#include        <linux/sched.h>
#include        <linux/timer.h>
#include        <linux/ioport.h>
#include        <linux/pci.h>
#include        <linux/ctype.h>
#include        <linux/proc_fs.h>
#include        <linux/vmalloc.h>
#include        <linux/ptrace.h>
#include        <linux/string.h>

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,18)
#include       <linux/config.h>
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,22)
#include        <linux/freezer.h>
#endif
#include        <linux/usb.h>

/* ASM files */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,27)
#include        <linux/semaphore.h>
#else
#include        <asm/semaphore.h>
#endif
#include        <asm/byteorder.h>
#include        <asm/irq.h>
#include        <asm/uaccess.h>
#include        <asm/io.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,4,0)
#include        <asm/switch_to.h>
#else
#include        <asm/system.h>
#endif
/* Net header files */
#include        <linux/netdevice.h>
#include        <linux/net.h>
#include        <linux/inet.h>
#include        <linux/ip.h>
#include        <linux/skbuff.h>
#include        <linux/if_arp.h>
#include        <linux/if_ether.h>
#include        <linux/etherdevice.h>
#include        <net/sock.h>
#include        <net/arp.h>

#include        <linux/firmware.h>

/** Re-define generic data types */
/** Signed char (1-byte) */
typedef char t_s8;
/** Unsigned char (1-byte) */
typedef unsigned char t_u8;
/** Signed short (2-bytes) */
typedef short t_s16;
/** Unsigned short (2-bytes) */
typedef unsigned short t_u16;
/** Signed int (4-bytes) */
typedef int t_s32;
/** Unsigned int (4-bytes) */
typedef unsigned int t_u32;
/** Signed long long 8-bytes) */
typedef long long t_s64;
/** Unsigned long long 8-bytes) */
typedef unsigned long long t_u64;
/** Void pointer (4-bytes) */
typedef void t_void;
/** Size type */
typedef t_u32 t_size;
/** Boolean type */
typedef t_u8 t_bool;

#ifdef DRIVER_64BIT
/** Pointer type (64-bit) */
typedef t_u64 t_ptr;
/** Signed value (64-bit) */
typedef t_s64 t_sval;
#else
/** Pointer type (32-bit) */
typedef t_u32 t_ptr;
/** Signed value (32-bit) */
typedef t_s32 t_sval;
#endif

/** Driver TRUE */
#define MTRUE                    (1)
/** Driver FALSE */
#define MFALSE                   (0)
/** Macros for Data Alignment : address */
#define ALIGN_ADDR(p, a)    \
    ((((t_ptr)(p)) + (((t_ptr)(a)) - 1)) & ~(((t_ptr)(a)) - 1))
/** DMA alignment */
#define DMA_ALIGNMENT            64
/** IN parameter */
#define IN
/** OUT parameter */
#define OUT

/** BIT value */
#define MBIT(x)    (((t_u32)1) << (x))
/** Buffer flag for malloc driver_buffer */
#define DRIVER_BUF_FLAG_MALLOC_BUF        MBIT(2)

#ifdef DEBUG_LEVEL1
/** Debug level bit definition */
#define MMSG        MBIT(0)
#define MFATAL      MBIT(1)
#define MERROR      MBIT(2)

#define MIF_D       MBIT(20)

#define MENTRY      MBIT(28)
#define MINFO       MBIT(30)
#define MHEX_DUMP   MBIT(31)
#endif /* DEBUG_LEVEL1 */

/** Memory allocation type: DMA */
#define DRIVER_MEM_DMA    MBIT(0)

/** Default memory allocation flag */
#define DRIVER_MEM_DEF    0

/** driver_status */
typedef enum _driver_status {
	DRIVER_STATUS_FAILURE = 0xffffffff,
	DRIVER_STATUS_SUCCESS = 0,
	DRIVER_STATUS_PENDING,
} driver_status;

/** USB device type */
typedef enum _usb_device_type {
	DEV_TYPE_USB8766,
	DEV_TYPE_USB8797,
	DEV_TYPE_USB8897,
} usb_device_type;

/** driver_usb_ep */
typedef enum _driver_usb_ep {
	DRIVER_USB_EP_CTRL = 0,
	DRIVER_USB_EP_CMD_EVENT = 1,
	DRIVER_USB_EP_DATA = 2,
} driver_usb_ep;

/** Timeout in milliseconds for usb_bulk_msg function */
#define DRIVER_USB_BULK_MSG_TIMEOUT      100

/** Data Structures */
/** driver_image data structure */
typedef struct _driver_fw_image {
    /** Helper image buffer pointer */
	t_u8 *phelper_buf;
    /** Helper image length */
	t_u32 helper_len;
    /** Firmware image buffer pointer */
	t_u8 *pfw_buf;
    /** Firmware image length */
	t_u32 fw_len;
} driver_fw_image, *pdriver_fw_image;

/** driver_buffer data structure */
typedef struct _driver_buffer {
    /** Flags for this buffer */
	t_u32 flags;
    /** Buffer descriptor, e.g. skb in Linux */
	t_void *pdesc;
    /** Pointer to buffer */
	t_u8 *pbuf;
    /** Offset to data */
	t_u32 data_offset;
    /** Data length */
	t_u32 data_len;
} driver_buffer, *pdriver_buffer;

/** Define BOOLEAN */
typedef t_u8 BOOLEAN;

/** Handle data structure for driver  */
typedef struct _driver_handle driver_handle;

/** Hardware status codes */
typedef enum _DRIVER_HARDWARE_STATUS {
	HardwareStatusReady,
	HardwareStatusInitializing,
	HardwareStatusFwReady,
	HardwareStatusReset,
	HardwareStatusClosing,
	HardwareStatusNotReady
} DRIVER_HARDWARE_STATUS;

/**
 *  @brief Schedule timeout
 *
 *  @param millisec Timeout duration in milli second
 *
 *  @return     N/A
 */
static inline void
driver_sched_timeout(t_u32 millisec)
{
	set_current_state(TASK_INTERRUPTIBLE);

	schedule_timeout((millisec * HZ) / 1000);
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,37)
/** Initialize semaphore */
#define DRIVER_INIT_SEMAPHORE(x)          init_MUTEX(x)
/** Initialize semaphore */
#define DRIVER_INIT_SEMAPHORE_LOCKED(x)   init_MUTEX_LOCKED(x)
#else
/** Initialize semaphore */
#define DRIVER_INIT_SEMAPHORE(x)          sema_init(x,1)
/** Initialize semaphore */
#define DRIVER_INIT_SEMAPHORE_LOCKED(x)   sema_init(x,0)
#endif

/** Acquire semaphore and with blocking */
#define DRIVER_ACQ_SEMAPHORE_BLOCK(x)     down_interruptible(x)

/** Release semaphore */
#define DRIVER_REL_SEMAPHORE(x)           up(x)

/** Request FW timeout in second */
#define REQUEST_FW_TIMEOUT            30

/** 1 second */
#define DRIVER_TIMER_1S                 1000

/** Handle data structure for driver */
struct _driver_handle {
    /** Priv number */
	t_u8 priv_num;
    /** name of firmware image */
	char *fw_name;
    /** Firmware download skip flag */
	t_u8 skip_fw_dnld;
    /** Firmware */
	const struct firmware *firmware;
    /** Firmware request start time */
	struct timeval req_fw_time;
    /** Hotplug device */
	struct device *hotplug_device;
    /** STATUS variables */
	DRIVER_HARDWARE_STATUS hardware_status;
    /** POWER MANAGEMENT AND PnP SUPPORT */
	BOOLEAN surprise_removed;

    /** Card pointer */
	t_void *card;
    /** Malloc count */
	t_u32 malloc_count;
    /** Driver buffer alloc count */
	t_u32 mbufalloc_count;

    /** handle index - for multiple card supports */
	t_u8 handle_idx;
    /** Flag to indicate boot state */
	t_u8 boot_state;
    /** Flag of USB device type */
	usb_device_type usb_dev_type;
};

/** Max number of char in custom event - use multiple of them if needed */
#define IW_CUSTOM_MAX       256	/* In bytes */

/** Debug Macro definition*/
#ifdef  DEBUG_LEVEL1
#ifdef  DEBUG_LEVEL2
#define PRINTM_MINFO(msg...)  do {printk(KERN_DEBUG msg);} while(0)
#define PRINTM_MENTRY(msg...) do {printk(KERN_DEBUG msg);} while(0)
#else
#define PRINTM_MINFO(msg...)  do {} while (0)
#define PRINTM_MENTRY(msg...) do {} while (0)
#endif /* DEBUG_LEVEL2 */
#define PRINTM_MWARN(msg...) do {printk(KERN_DEBUG msg);} while(0)
#define PRINTM_MIF_D(msg...) do {printk(KERN_DEBUG msg);} while(0)
#define PRINTM_MERROR(msg...) do {printk(KERN_ERR msg);} while(0)
#define PRINTM_MFATAL(msg...) do {printk(KERN_ERR msg);} while(0)
#define PRINTM_MMSG(msg...)   do {printk(KERN_ALERT msg);} while(0)

#define PRINTM(level,msg...) PRINTM_##level(msg)

#else

#define PRINTM(level,msg...) do {} while (0)

#endif /* DEBUG_LEVEL1 */

/** Log entry point for debugging */
#define ENTER()         PRINTM(MENTRY, "Enter: %s\n", \
                                    __FUNCTION__)
/** Log exit point for debugging */
#define LEAVE()         PRINTM(MENTRY, "Leave: %s\n", \
                                    __FUNCTION__)

#ifdef DEBUG_LEVEL1
#define DBG_DUMP_BUF_LEN    64
#define MAX_DUMP_PER_LINE   16

static inline void
hexdump(char *prompt, t_u8 * buf, int len)
{
	int i;
	char dbgdumpbuf[DBG_DUMP_BUF_LEN];
	char *ptr = dbgdumpbuf;

	printk(KERN_DEBUG "%s:\n", prompt);
	for (i = 1; i <= len; i++) {
		ptr += snprintf(ptr, 4, "%02x ", *buf);
		buf++;
		if (i % MAX_DUMP_PER_LINE == 0) {
			*ptr = 0;
			printk(KERN_DEBUG "%s\n", dbgdumpbuf);
			ptr = dbgdumpbuf;
		}
	}
	if (len % MAX_DUMP_PER_LINE) {
		*ptr = 0;
		printk(KERN_DEBUG "%s\n", dbgdumpbuf);
	}
}

#define DBG_HEXDUMP_MERROR(x,y,z)    do {hexdump(x,y,z);} while(0)
#define DBG_HEXDUMP_MIF_D(x,y,z)     do {hexdump(x,y,z);} while(0)
#define DBG_HEXDUMP(level,x,y,z)    DBG_HEXDUMP_##level(x,y,z)

#else
/** Do nothing since debugging is not turned on */
#define DBG_HEXDUMP(level,x,y,z)    do {} while (0)
#endif

/** MARVELL USB VID  */
#define MARVELL_USB_VID  0x1286
/** USB8766 USB PID 1 */
#define MARVELL_USB8766_PID  0x2041
/** USB8797 USB PID 1 */
#define MARVELL_USB8797_PID  0x2043
/** USB8897 USB PID 1 */
#define MARVELL_USB8897_PID  0x2045
/** Boot state: FW download */
#define MARVELL_USB_FW_DNLD     1
/** Boot state: FW ready */
#define MARVELL_USB_FW_READY    2

/** High watermark for Tx data */
#define MVUSB_TX_HIGH_WMARK    6

/** Number of Rx data URB */
#define MVUSB_RX_DATA_URB   6

/* Transmit buffer size for chip revision check */
#define CHIP_REV_TX_BUF_SIZE    16
/* Receive buffer size for chip revision check */
#define CHIP_REV_RX_BUF_SIZE    2048

/* Extensions */
#define EXTEND_HDR       (0xAB95)
#define EXTEND_V1        (0x0001)

/** USB8797 chip revision ID */
#define USB8797_A0       0x00000000
#define USB8797_B0       0x03800010

/* USB8766 DEFAULT FW NAME */
#define USB8766_DEFAULT_FW_NAME    "mrvl/usb8766_uapsta.bin"
/* USB8797 A0 DEFAULT FW NAME */
#define USB8797_A0_DEFAULT_FW_NAME "mrvl/usb8797_uapsta_a0.bin"
/* USB8797 B0 DEFAULT FW NAME */
#define USB8797_B0_DEFAULT_FW_NAME "mrvl/usb8797_uapsta.bin"
/* USB8897 DEFAULT FW NAME */
#define USB8897_DEFAULT_FW_NAME    "mrvl/usb8897_uapsta.bin"

/** Card-type detection frame response */
typedef struct {
    /** 32-bit ACK+WINNER field */
	t_u32 ack_winner;
    /** 32-bit Sequence number */
	t_u32 seq;
    /** 32-bit extend */
	t_u32 extend;
    /** 32-bit chip-revision code */
	t_u32 chip_rev;
} usb_ack_pkt;

/** urb context */
typedef struct _urb_context {
    /** Pointer to driver_handle structure */
	driver_handle *handle;
    /** Pointer to driver buffer */
	driver_buffer *pmbuf;
    /** URB */
	struct urb *urb;
    /** EP */
	t_u8 ep;
} urb_context;

/** USB card description structure*/
struct usb_card_rec {
    /** USB device */
	struct usb_device *udev;
    /** Driver handle */
	void *phandle;
    /** USB interface */
	struct usb_interface *intf;
    /** Rx data endpoint address */
	t_u8 rx_cmd_ep;
    /** Rx cmd contxt */
	urb_context rx_cmd;
    /** Rx command URB pending count */
	atomic_t rx_cmd_urb_pending;
    /** Rx data context list */
	urb_context rx_data_list[MVUSB_RX_DATA_URB];
    /** Flag to indicate boot state */
	t_u8 boot_state;
    /** Rx data endpoint address */
	t_u8 rx_data_ep;
    /** Rx data URB pending count */
	atomic_t rx_data_urb_pending;
    /** Tx data endpoint address */
	t_u8 tx_data_ep;
    /** Tx command endpoint address */
	t_u8 tx_cmd_ep;
    /** Tx data URB pending count */
	atomic_t tx_data_urb_pending;
    /** Tx command URB pending count */
	atomic_t tx_cmd_urb_pending;
    /** Bulk out max packet size */
	int bulk_out_maxpktsize;
    /** Pre-allocated urb for command */
	urb_context tx_cmd;
    /** Index to point to next data urb to use */
	int tx_data_ix;
    /** Pre-allocated urb for data */
	urb_context tx_data_list[MVUSB_TX_HIGH_WMARK];
    /** Flag of USB device type */
	usb_device_type usb_dev_type;
};

/** Tx buffer size for firmware download*/
#define FW_DNLD_TX_BUF_SIZE       620
/** Rx buffer size for firmware download*/
#define FW_DNLD_RX_BUF_SIZE       2048
/** Max firmware retry */
#define MAX_FW_RETRY              3

/** Firmware has last block */
#define FW_HAS_LAST_BLOCK         0x00000004

/** Firmware data transmit size */
#define FW_DATA_XMIT_SIZE \
    sizeof(FWHeader) + DataLength + sizeof(t_u32)

/** FWHeader */
typedef struct _FWHeader {
    /** FW download command */
	t_u32 dnld_cmd;
    /** FW base address */
	t_u32 base_addr;
    /** FW data length */
	t_u32 data_length;
    /** FW CRC */
	t_u32 crc;
} FWHeader;

/** FWData */
typedef struct _FWData {
    /** FW data header */
	FWHeader fw_header;
    /** FW data sequence number */
	t_u32 seq_num;
    /** FW data buffer */
	t_u8 data[1];
} FWData;

/** FWSyncHeader */
typedef struct _FWSyncHeader {
    /** FW sync header command */
	t_u32 cmd;
    /** FW sync header sequence number */
	t_u32 seq_num;
} FWSyncHeader;

#ifdef BIG_ENDIAN_SUPPORT
/** Convert sequence number and command fields of fwheader to correct endian format */
#define endian_convert_syncfwheader(x)  { \
        (x)->cmd = driver_le32_to_cpu((x)->cmd); \
        (x)->seq_num = driver_le32_to_cpu((x)->seq_num); \
    }
#else
/** Convert sequence number and command fields of fwheader to correct endian format */
#define endian_convert_syncfwheader(x)
#endif /* BIG_ENDIAN_SUPPORT */

/** Driver MNULL pointer */
#define MNULL                           (0)

/** 16 bits byte swap */
#define swap_byte_16(x) \
((t_u16)((((t_u16)(x) & 0x00ffU) << 8) | \
         (((t_u16)(x) & 0xff00U) >> 8)))

/** 32 bits byte swap */
#define swap_byte_32(x) \
((t_u32)((((t_u32)(x) & 0x000000ffUL) << 24) | \
         (((t_u32)(x) & 0x0000ff00UL) <<  8) | \
         (((t_u32)(x) & 0x00ff0000UL) >>  8) | \
         (((t_u32)(x) & 0xff000000UL) >> 24)))

#ifdef BIG_ENDIAN_SUPPORT
/** Convert from 16 bit little endian format to CPU format */
#define driver_le16_to_cpu(x) swap_byte_16(x)
/** Convert from 32 bit little endian format to CPU format */
#define driver_le32_to_cpu(x) swap_byte_32(x)
/** Convert to 16 bit little endian format from CPU format */
#define driver_cpu_to_le16(x) swap_byte_16(x)
/** Convert to 32 bit little endian format from CPU format */
#define driver_cpu_to_le32(x) swap_byte_32(x)
#else
/** Do nothing */
#define driver_le16_to_cpu(x) x
/** Do nothing */
#define driver_le32_to_cpu(x) x
/** Do nothing */
#define driver_cpu_to_le16(x) x
/** Do nothing */
#define driver_cpu_to_le32(x) x
#endif /* BIG_ENDIAN_SUPPORT */

/* Functions in interface module */
/** Add card */
driver_handle *driver_add_card(struct usb_card_rec *cardp);
/** Remove card */
driver_status driver_remove_card(void *card);
/** Download firmware using helper */
driver_status driver_request_fw(driver_handle * handle);
/** Read one block of firmware data */
driver_status driver_get_fw_data(IN driver_handle * handle,
				 IN t_u32 offset, IN t_u32 len,
				 OUT t_u8 * pbuf);
/** Allocate a buffer */
driver_status driver_malloc(IN driver_handle * handle,
			    IN t_u32 size, IN t_u32 flag, OUT t_u8 ** ppbuf);
/** Free a buffer */
driver_status driver_mfree(IN driver_handle * handle, IN t_u8 * pbuf);
/** Download data blocks to device */
driver_status driver_write_data_sync(driver_handle * handle,
				     driver_buffer * pmbuf, t_u8 ep,
				     t_u32 timeout);
/** Read data blocks from device */
driver_status driver_read_data_sync(driver_handle * handle,
				    driver_buffer * pmbuf, t_u8 ep,
				    t_u32 timeout);
/** Free Tx/Rx urb, skb and Rx buffer */
void driver_usb_free(struct usb_card_rec *cardp);
/** Check chip revision */
driver_status driver_check_chip_revision(driver_handle * handle,
					 t_u32 * usb_chip_rev);
