/********************************************************************************
 * Marvell GPL License Option
 *
 * If you received this File from Marvell, you may opt to use, redistribute and/or
 * modify this File in accordance with the terms and conditions of the General
 * Public License Version 2, June 1991 (the "GPL License"), a copy of which is
 * available along with the File in the license.txt file or by writing to the Free
 * Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 or
 * on the worldwide web at http://www.gnu.org/licenses/gpl.txt.
 *
 * THE FILE IS DISTRIBUTED AS-IS, WITHOUT WARRANTY OF ANY KIND, AND THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE ARE EXPRESSLY
 * DISCLAIMED.  The GPL License provides additional details about this warranty
 * disclaimer.
 ********************************************************************************/
#ifndef _SM_H_
#define _SM_H_

#define SM_ITCM_BASE		0x00000000
#define SM_ITCM_SIZE		0x00010000	/* 64k */
#define SM_ITCM_END		(SM_ITCM_BASE + SM_ITCM_SIZE)
#define SM_ITCM_ALIAS_SIZE	0x00020000	/* 128k */

#define SM_DTCM_BASE		0x04000000
#define SM_DTCM_SIZE		0x00004000	/* 16k */
#define SM_DTCM_END		(SM_DTCM_BASE + SM_DTCM_SIZE)
#define SM_DTCM_ALIAS_SIZE	0x00020000	/* 128k */

#define SM_APBC_BASE		0x10000000
#define SM_APBC_SIZE		0x00010000	/* 64k */
#define SM_APBC_END		(SM_APBC_BASE + SM_APBC_SIZE)
#define SM_APBC_ALIAS_SIZE	0x00010000	/* 64k */

#define SOC_ITCM_BASE		(sm_base)
#define SOC_DTCM_BASE		(sm_base + 0x20000)
#define SOC_APBC_BASE		(sm_base + 0x40000)

#define SM_SM_SYS_CTRL_REG_BASE	(SM_APBC_BASE + 0xD000)

#define SOC_ITCM(addr)		((int)addr - SM_ITCM_BASE + SOC_ITCM_BASE)
#define SOC_DTCM(addr)		((int)addr - SM_DTCM_BASE + SOC_DTCM_BASE)
#define SOC_APBC(addr)		((int)addr - SM_APBC_BASE + SOC_APBC_BASE)

#define SM_ITCM_SOC_USE_ADDR	(SM_ITCM_BASE + 0x8000)
#define SM_ITCM_SOC_USE_SIZE	(SM_ITCM_END - SM_ITCM_SOC_USE_ADDR)

#define SM_MSG_SIZE		32
#define SM_MSG_BODY_SIZE	(SM_MSG_SIZE - sizeof(short) * 2)
#define SM_MSGQ_TOTAL_SIZE	512
#define SM_MSGQ_HEADER_SIZE	SM_MSG_SIZE
#define SM_MSGQ_SIZE		(SM_MSGQ_TOTAL_SIZE - SM_MSGQ_HEADER_SIZE)
#define SM_MSGQ_MSG_COUNT	(SM_MSGQ_SIZE/SM_MSG_SIZE)

typedef struct
{
	short	m_iModuleID;
	short	m_iMsgLen;
	char	m_pucMsgBody[SM_MSG_BODY_SIZE];
} MV_SM_Message;

typedef struct
{
	int	m_iWrite;
	int	m_iRead;
	int	m_iWriteTotal;
	int	m_iReadTotal;
	char	m_Padding[SM_MSGQ_HEADER_SIZE - sizeof(int) * 4];
	char	m_Queue[SM_MSGQ_SIZE];
} MV_SM_MsgQ;

typedef struct
{
	unsigned int offset;
	unsigned int data;
} SM_SHARED_RDWR_DATA;

#define SM_MSG_EXTRA_BUF_SIZE		494
#define SM_MSG_EXTRA_BUF_ADDR_OFF	512
#define SM_MSG_EXTRA_BUF_ADDR		(sm_base+SM_ITCM_ALIAS_SIZE+SM_DTCM_SIZE-512)

#define SM_DTCM_SHARED_DATA_SIZE        512
#define SM_DTCM_SHARED_DATA_ADDR		(SM_DTCM_END - SM_MSGQ_TOTAL_SIZE*4 - SM_DTCM_SHARED_DATA_SIZE)
#define SM_PINMUX_TABLE_OFFSET          SM_DTCM_SHARED_DATA_ADDR
#define SM_PINMUX_TABLE_SIZE            128

#define SM_DTCM_REQUEST_REBOOT          (SM_DTCM_SHARED_DATA_ADDR+256+(8*4))
#define SM_DTCM_ASSERTED_WATCHDOG       (SM_DTCM_SHARED_DATA_ADDR+256+(9*4))
#define SM_DTCM_RESET_WATCHDOG          (SM_DTCM_SHARED_DATA_ADDR+256+(10*4))

#define SM_CPU0_INPUT_QUEUE_ADDR	(SM_DTCM_END - SM_MSGQ_TOTAL_SIZE*4)
#define SM_CPU1_INPUT_QUEUE_ADDR	(SM_DTCM_END - SM_MSGQ_TOTAL_SIZE*3)
#define SM_CPU0_OUTPUT_QUEUE_ADDR	(SM_DTCM_END - SM_MSGQ_TOTAL_SIZE*2)
#define SM_CPU1_OUTPUT_QUEUE_ADDR	(SM_DTCM_END - SM_MSGQ_TOTAL_SIZE)

#define MV_SM_POWER_WARMDOWN_REQUEST	1
#define MV_SM_POWER_WARMDOWN_REQUEST_2	2
#define MV_SM_POWER_SYS_RESET		0xFF
#define MV_SM_GPIO_SET			0x6
#define MV_SM_GPIO_GET			0x7
#define MV_SM_IR_Linuxready		30

// Watchdog timer registers
#define SOC_SM_APB_WDT1_BASE		(SOC_APBC_BASE + 0x2000)
#define WDT_CRR				(0x0C)

typedef enum
{
	MV_SM_ID_SYS = 1,
	MV_SM_ID_COMM,
	MV_SM_ID_IR,
	MV_SM_ID_KEY,
	MV_SM_ID_POWER,
	MV_SM_ID_WDT,
	MV_SM_ID_TEMP,
	MV_SM_ID_VFD,
	MV_SM_ID_SPI,
	MV_SM_ID_I2C,
	MV_SM_ID_UART,
	MV_SM_ID_CEC,
	MV_SM_ID_WOL,
	MV_SM_ID_LED,
	MV_SM_ID_ETH,
	MV_SM_ID_DDR,
	MV_SM_ID_WIFIBT,
	MV_SM_ID_RTC,
	MV_SM_ID_LOGOBAR,
	MV_SM_ID_VGADPMS,
	MV_SM_ID_LOG,
	MV_SM_ID_BUZZER,
	MV_SM_ID_CBTN,
	MV_SM_ID_GPIO,
	/* add new module id here */
	MV_SM_ID_DEBUG,
	MV_SM_ID_MAX,
} MV_SM_MODULE_ID;

#define MAX_MSG_TYPE	(MV_SM_ID_MAX - 1)

/* SM driver ioctl cmd */
#define SM_READ			0x1
#define SM_WRITE		0x2
#define SM_RDWR			0x3
#define SM_Enable_WaitQueue	0x1234
#define SM_Disable_WaitQueue	0x3456
#define SM_SHARED_READ	0x11
#define SM_SHARED_WRITE	0x12
#define SM_REFRESH_WDT 0x21

extern int bsm_msg_recv(int id, void *msg, int *len);
extern int bsm_msg_send(int id, void *msg, int len);

extern void (*ir_sm_int)(void);
#endif /* _SM_H_ */
