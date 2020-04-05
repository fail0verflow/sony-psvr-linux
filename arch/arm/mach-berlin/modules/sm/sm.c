#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/wait.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/interrupt.h>
#include <linux/miscdevice.h>
#include <linux/io.h>
#include <linux/spinlock.h>
#include <linux/pm.h>
#include <linux/platform_device.h>

#include <asm/uaccess.h>
#include <asm/cacheflush.h>
#include <asm/system_misc.h>

#include "sm.h"

#define SRAM_PA		0xF7FD0000

static void (*figo_sram)(int msg);
extern unsigned int berlin_enter_ddrsr_sz;
extern void berlin_enter_ddrsr(void);

#define w32smSysCtl_SM_CTRL { \
	unsigned int uSM_CTRL_ISO_EN		:  1; \
	unsigned int uSM_CTRL_SM2SOC_SW_INTR	:  1; \
	unsigned int uSM_CTRL_SOC2SM_SW_INTR	:  1; \
	unsigned int uSM_CTRL_REV_0		:  2; \
	unsigned int uSM_CTRL_ADC_SEL		:  4; \
	unsigned int uSM_CTRL_ADC_PU		:  1; \
	unsigned int uSM_CTRL_ADC_CKSEL		:  2; \
	unsigned int uSM_CTRL_ADC_START		:  1; \
	unsigned int uSM_CTRL_ADC_RESET		:  1; \
	unsigned int uSM_CTRL_ADC_BG_RDY	:  1; \
	unsigned int uSM_CTRL_ADC_CONT		:  1; \
	unsigned int uSM_CTRL_ADC_BUF_EN	:  1; \
	unsigned int uSM_CTRL_ADC_VREF_SEL	:  1; \
	unsigned int uSM_CTRL_ADC_TST_SAR	:  1; \
	unsigned int uSM_CTRL_ADC_ROTATE_SEL	:  1; \
	unsigned int uSM_CTRL_TSEN_EN		:  1; \
	unsigned int uSM_CTRL_TSEN_CLK_SEL	:  1; \
	unsigned int uSM_CTRL_TSEN_MODE_SEL	:  1; \
	unsigned int uSM_CTRL_TSEN_ADC_CAL	:  2; \
	unsigned int uSM_CTRL_TSEN_TST_SEL	:  2; \
	unsigned int RSVDx14_b27		:  5; \
}

#define RA_smSysCtl_SM_CTRL	0x0014

static int sm_irq;
static unsigned long sm_base;
static unsigned long sm_asserted_watchdog_flag = 0;
#define SM_ASSERTED_WATCHDOG_BIT (0)

typedef void (*wdt_cb_t)(void);
static wdt_cb_t wdt_cb = NULL;

typedef union {
	unsigned int u32;
	struct w32smSysCtl_SM_CTRL;
} T32smSysCtl_SM_CTRL;

#define SM_Q_PUSH( pSM_Q ) {				\
	pSM_Q->m_iWrite += SM_MSG_SIZE;			\
	if( pSM_Q->m_iWrite >= SM_MSGQ_SIZE )		\
		pSM_Q->m_iWrite -= SM_MSGQ_SIZE;	\
	pSM_Q->m_iWriteTotal += SM_MSG_SIZE; }

#define SM_Q_POP( pSM_Q ) {				\
	pSM_Q->m_iRead += SM_MSG_SIZE;			\
	if( pSM_Q->m_iRead >= SM_MSGQ_SIZE )		\
		pSM_Q->m_iRead -= SM_MSGQ_SIZE;		\
	pSM_Q->m_iReadTotal += SM_MSG_SIZE; }

static int bsm_link_msg(MV_SM_MsgQ *q, MV_SM_Message *m, spinlock_t *lock)
{
	unsigned long flags;
	MV_SM_Message *p;
	int ret = 0;

	if (lock)
		spin_lock_irqsave(lock, flags);

	if (q->m_iWrite < 0 || q->m_iWrite >= SM_MSGQ_SIZE) {
		/* buggy ? */
		ret = -EIO;
		goto out;
	}

	/* message queue full, ignore the newest message */
	if (q->m_iRead == q->m_iWrite && q->m_iReadTotal != q->m_iWriteTotal) {
		ret = -EBUSY;
		goto out;
	}

	p = (MV_SM_Message*)(&(q->m_Queue[q->m_iWrite]));
	memcpy(p, m, sizeof(*p));
	SM_Q_PUSH(q);
out:
	if (lock)
		spin_unlock_irqrestore(lock, flags);

	return ret;
}

static int bsm_unlink_msg(MV_SM_MsgQ *q, MV_SM_Message *m, spinlock_t *lock)
{
	unsigned long flags;
	MV_SM_Message *p;
	int ret = -EAGAIN; /* means no data */

	if (lock)
		spin_lock_irqsave(lock, flags);

	if (q->m_iRead < 0 || q->m_iRead >= SM_MSGQ_SIZE ||
			q->m_iReadTotal > q->m_iWriteTotal) {
		/* buggy ? */
		ret = -EIO;
		goto out;
	}

	/* if buffer was overflow written, only the last messages are
	 * saved in queue. move read pointer into the same position of
	 * write pointer and keep buffer full.
	 */
	if (q->m_iWriteTotal - q->m_iReadTotal > SM_MSGQ_SIZE) {
		int iTotalDataSize = q->m_iWriteTotal - q->m_iReadTotal;

		q->m_iReadTotal += iTotalDataSize - SM_MSGQ_SIZE;
		q->m_iRead += iTotalDataSize % SM_MSGQ_SIZE;
		if (q->m_iRead >= SM_MSGQ_SIZE)
			q->m_iRead -= SM_MSGQ_SIZE;
	}

	if (q->m_iReadTotal < q->m_iWriteTotal) {
		/* alright get one message */
		p = (MV_SM_Message*)(&(q->m_Queue[q->m_iRead]));
		memcpy(m, p, sizeof(*m));
		SM_Q_POP(q);
		ret = 0;
	}
out:
	if (lock)
		spin_unlock_irqrestore(lock, flags);

	return ret;
}

static spinlock_t sm_lock;

static inline int bsm_link_msg_to_sm(MV_SM_Message *m)
{
	MV_SM_MsgQ *q = (MV_SM_MsgQ *)(SOC_DTCM(SM_CPU1_INPUT_QUEUE_ADDR));

	return bsm_link_msg(q, m, &sm_lock);
}

static inline int bsm_unlink_msg_from_sm(MV_SM_Message *m)
{
	MV_SM_MsgQ *q = (MV_SM_MsgQ *)(SOC_DTCM(SM_CPU0_OUTPUT_QUEUE_ADDR));

	return bsm_unlink_msg(q, m, &sm_lock);
}

#define DEFINE_SM_MODULES(id)				\
	{						\
		.m_iModuleID  = id,			\
		.m_bWaitQueue = false,			\
	}

typedef struct {
	int m_iModuleID;
	bool m_bWaitQueue;
	wait_queue_head_t m_Queue;
	spinlock_t m_Lock;
	MV_SM_MsgQ m_MsgQ;
	struct mutex m_Mutex;
} MV_SM_Module;

static MV_SM_Module SMModules[MAX_MSG_TYPE] = {
	DEFINE_SM_MODULES(MV_SM_ID_SYS),
	DEFINE_SM_MODULES(MV_SM_ID_COMM),
	DEFINE_SM_MODULES(MV_SM_ID_IR),
	DEFINE_SM_MODULES(MV_SM_ID_KEY),
	DEFINE_SM_MODULES(MV_SM_ID_POWER),
	DEFINE_SM_MODULES(MV_SM_ID_WDT),
	DEFINE_SM_MODULES(MV_SM_ID_TEMP),
	DEFINE_SM_MODULES(MV_SM_ID_VFD),
	DEFINE_SM_MODULES(MV_SM_ID_SPI),
	DEFINE_SM_MODULES(MV_SM_ID_I2C),
	DEFINE_SM_MODULES(MV_SM_ID_UART),
	DEFINE_SM_MODULES(MV_SM_ID_CEC),
	DEFINE_SM_MODULES(MV_SM_ID_WOL),
	DEFINE_SM_MODULES(MV_SM_ID_LED),
	DEFINE_SM_MODULES(MV_SM_ID_ETH),
	DEFINE_SM_MODULES(MV_SM_ID_DDR),
	DEFINE_SM_MODULES(MV_SM_ID_WIFIBT),
	DEFINE_SM_MODULES(MV_SM_ID_RTC),
	DEFINE_SM_MODULES(MV_SM_ID_LOGOBAR),
	DEFINE_SM_MODULES(MV_SM_ID_VGADPMS),
	DEFINE_SM_MODULES(MV_SM_ID_LOG),
	DEFINE_SM_MODULES(MV_SM_ID_DEBUG),
};

static inline MV_SM_Module *bsm_search_module(int id)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(SMModules); i++)
		if (SMModules[i].m_iModuleID == id)
			return &(SMModules[i]);

	return NULL;
}

static int bsm_link_msg_to_module(MV_SM_Message *m)
{
	MV_SM_Module *module;
	int ret;

	module = bsm_search_module(m->m_iModuleID);
	if (!module)
		return -EINVAL;

	ret = bsm_link_msg(&(module->m_MsgQ), m, &(module->m_Lock));
	if (ret == 0) {
		/* wake up any process pending on wait-queue */
		wake_up_interruptible(&(module->m_Queue));
	}

	return ret;
}

static int bsm_unlink_msg_from_module(MV_SM_Message *m)
{
	MV_SM_Module *module;
	DEFINE_WAIT(__wait);
	unsigned long flags;
	int ret;

	module = bsm_search_module(m->m_iModuleID);
	if (!module)
		return -EINVAL;

repeat:
	spin_lock_irqsave(&(module->m_Lock), flags);

	if (!in_interrupt())
		prepare_to_wait(&(module->m_Queue), &__wait,
						TASK_INTERRUPTIBLE);

	ret = bsm_unlink_msg(&(module->m_MsgQ), m, NULL);
	if (ret == -EAGAIN && module->m_bWaitQueue && !in_interrupt()) {
		spin_unlock_irqrestore(&(module->m_Lock), flags);

		if (!signal_pending(current))
			schedule();
		else {
			finish_wait(&(module->m_Queue), &__wait);
			return -ERESTARTSYS;
		}

		goto repeat;
	}

	if (!in_interrupt())
		finish_wait(&(module->m_Queue), &__wait);

	spin_unlock_irqrestore(&(module->m_Lock), flags);

	return ret;
}

static void null_int(void)
{
}

void (*ir_sm_int)(void) = null_int;

int bsm_msg_send(int id, void *msg, int len)
{
	MV_SM_Message m;
	int ret;

	if (unlikely(len < 4) || unlikely(len > SM_MSG_BODY_SIZE))
		return -EINVAL;

	m.m_iModuleID = id;
	m.m_iMsgLen   = len;
	memcpy(m.m_pucMsgBody, msg, len);
	for (;;) {
		ret = bsm_link_msg_to_sm(&m);
		if (ret != -EBUSY)
			break;
		mdelay(10);
	}

	return ret;
}
EXPORT_SYMBOL(bsm_msg_send);

int bsm_msg_recv(int id, void *msg, int *len)
{
	MV_SM_Message m;
	int ret;

	m.m_iModuleID = id;
	ret = bsm_unlink_msg_from_module(&m);
	if (ret)
		return ret;

	if (msg)
		memcpy(msg, m.m_pucMsgBody, m.m_iMsgLen);

	if (len)
		*len = m.m_iMsgLen;

	return 0;
}
EXPORT_SYMBOL(bsm_msg_recv);

static void bsm_msg_dispatch(void)
{
	MV_SM_Message m;
	int ret;

	/* read all messages from SM buffers and dispatch them */
	for (;;) {
		ret = bsm_unlink_msg_from_sm(&m);
		if (ret)
			break;

		/* try best to dispatch received message */
		ret = bsm_link_msg_to_module(&m);
		if (ret != 0) {
			printk(KERN_ERR "Drop SM message\n");
			continue;
		}

		/* special case for IR events */
		if (m.m_iModuleID == MV_SM_ID_IR)
			ir_sm_int();
	}
}

static irqreturn_t bsm_intr(int irq, void *dev_id)
{
	void __iomem *addr;
	T32smSysCtl_SM_CTRL reg;

	addr = IOMEM(SOC_APBC(SM_SM_SYS_CTRL_REG_BASE + RA_smSysCtl_SM_CTRL));
	reg.u32 = readl_relaxed(addr);
	reg.uSM_CTRL_SM2SOC_SW_INTR = 0;
	writel_relaxed(reg.u32, addr);

	if(0xAEEDACD0 == readl(SOC_DTCM(SM_DTCM_ASSERTED_WATCHDOG))) {
		writel(0, SOC_DTCM(SM_DTCM_ASSERTED_WATCHDOG));
		if(!test_and_set_bit(SM_ASSERTED_WATCHDOG_BIT, &sm_asserted_watchdog_flag)) {
			if( wdt_cb ) {
				wdt_cb();
			} else {
				panic("sm asserted watchdog!");
			}
		}
	}

	bsm_msg_dispatch();

	return IRQ_HANDLED;
}

static ssize_t berlin_sm_read(struct file *file, char __user *buf,
				size_t count, loff_t *ppos)
{
	MV_SM_Message m;
	int id = (int)(*ppos);
	int ret;

	if (count < SM_MSG_SIZE)
		return -EINVAL;

	m.m_iModuleID = id;
	ret = bsm_unlink_msg_from_module(&m);
	if (!ret) {
		if (copy_to_user(buf, (void *)&m, SM_MSG_SIZE))
			return -EFAULT;
		return SM_MSG_SIZE;
	}

	return 0;
}

static ssize_t berlin_sm_write(struct file *file, const char __user *buf,
				size_t count, loff_t *ppos)
{
	MV_SM_Message SM_Msg;
	int ret;
	int id = (int)(*ppos);

	if (count < 4 || count > SM_MSG_BODY_SIZE)
		return -EINVAL;

	if (copy_from_user(SM_Msg.m_pucMsgBody, buf, count))
		return -EFAULT;

	ret = bsm_msg_send(id, SM_Msg.m_pucMsgBody, count);
	if (ret < 0)
		return -EFAULT;
	else
		return count;
}

static long berlin_sm_unlocked_ioctl(struct file *file,
			unsigned int cmd, unsigned long arg)
{
	MV_SM_Module *module;
	MV_SM_Message m;
	int ret = 0, id;

	if (cmd == SM_SHARED_READ || cmd == SM_SHARED_WRITE) {
		SM_SHARED_RDWR_DATA rw_data;
		if (copy_from_user(&rw_data, (void __user *)arg, sizeof(SM_SHARED_RDWR_DATA))) {
			return -EFAULT;
		}
		if (rw_data.offset >= SM_DTCM_SHARED_DATA_SIZE || rw_data.offset % 4) {
			return -EINVAL;
		}
		if (cmd == SM_SHARED_READ) {
			rw_data.data = *(unsigned int*)SOC_DTCM(SM_DTCM_SHARED_DATA_ADDR + rw_data.offset);
			if (copy_to_user((void __user *)arg, &rw_data, sizeof(SM_SHARED_RDWR_DATA))) {
				return -EFAULT;
			}
		} else {
			*(unsigned int*)SOC_DTCM(SM_DTCM_SHARED_DATA_ADDR + rw_data.offset) = rw_data.data;
		}
		return ret;
	} else if (cmd == SM_Enable_WaitQueue || cmd == SM_Disable_WaitQueue || cmd == SM_REFRESH_WDT) {
		id = (int)arg;
	} else {
		if (copy_from_user(&m, (void __user *)arg, SM_MSG_SIZE))
			return -EFAULT;
		id = m.m_iModuleID;
	}

	module = bsm_search_module(id);
	if (!module)
		return -EINVAL;

	mutex_lock(&(module->m_Mutex));

	switch (cmd) {
	case SM_Enable_WaitQueue:
		module->m_bWaitQueue = true;
		break;
	case SM_Disable_WaitQueue:
		module->m_bWaitQueue = false;
		wake_up_interruptible(&(module->m_Queue));
		break;
	case SM_READ:
		ret = bsm_unlink_msg_from_module(&m);
		if (!ret) {
			if (copy_to_user((void __user *)arg, &m, SM_MSG_SIZE))
				ret = -EFAULT;
		}
		break;
	case SM_WRITE:
		ret = bsm_msg_send(m.m_iModuleID, m.m_pucMsgBody, m.m_iMsgLen);
		break;
	case SM_RDWR:
		ret = bsm_msg_send(m.m_iModuleID, m.m_pucMsgBody, m.m_iMsgLen);
		if (ret)
			break;
		ret = bsm_unlink_msg_from_module(&m);
		if (!ret) {
			if (copy_to_user((void __user *)arg, &m, SM_MSG_SIZE))
				ret = -EFAULT;
		}
		break;
	case SM_REFRESH_WDT:
		// write magic number 0x76 to CRR
		writel(0x76, (SOC_SM_APB_WDT1_BASE + WDT_CRR));
		break;
	default:
		ret = -EINVAL;
		break;
	}

	mutex_unlock(&(module->m_Mutex));

	return ret;
}

static struct file_operations berlin_sm_fops = {
	.owner		= THIS_MODULE,
	.write		= berlin_sm_write,
	.read		= berlin_sm_read,
	.unlocked_ioctl	= berlin_sm_unlocked_ioctl,
	.llseek		= default_llseek,
};

static struct miscdevice sm_dev = {
	.minor	= MISC_DYNAMIC_MINOR,
	.name	= "bsm",
	.fops	= &berlin_sm_fops,
};

static void enter_ddrsr(int msg)
{
	/* copy enter DDR self-refresh function into SRAM */
	memcpy(figo_sram, berlin_enter_ddrsr, berlin_enter_ddrsr_sz);
	flush_cache_all();
	flush_icache_range((long)figo_sram, ((long)figo_sram + berlin_enter_ddrsr_sz));

	/* go */
	figo_sram(msg);
}

static void berlin_restart(char mode, const char *cmd)
{
	int len = 0;
	int msg = MV_SM_POWER_SYS_RESET;
	void *p = IOMEM(SM_MSG_EXTRA_BUF_ADDR);

	if (cmd && (len = strlen(cmd))) {
		len += 1;
		if (len > SM_MSG_EXTRA_BUF_SIZE - sizeof(len)) {
			len = SM_MSG_EXTRA_BUF_SIZE - sizeof(len);
			printk("cut off too long reboot args to %d bytes\n", len);
		}
		printk("reboot cmd is %s@%d\n", cmd, len);
		memcpy(p, &len, sizeof(len));
		memcpy(p + sizeof(len), cmd, len);
	} else
		memset(p, 0, sizeof(int));

	flush_cache_all();

	bsm_msg_send(MV_SM_ID_POWER, &msg, sizeof(int));
	for (;;);
}

static void berlin_power_off(void)
{
	local_irq_disable();
	local_fiq_disable();
	outer_disable();
	enter_ddrsr(MV_SM_POWER_WARMDOWN_REQUEST);
}

static int berlin_sm_probe(struct platform_device *pdev)
{
	int i;
	struct resource *r;

	r = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!r)
		return -EINVAL;
	sm_base = r->start;
	sm_irq = platform_get_irq(pdev, 0);
	if (sm_irq < 0)
		return -ENXIO;

	for (i = 0; i < ARRAY_SIZE(SMModules); i++) {
		init_waitqueue_head(&(SMModules[i].m_Queue));
		spin_lock_init(&(SMModules[i].m_Lock));
		mutex_init(&(SMModules[i].m_Mutex));
		memset(&(SMModules[i].m_MsgQ), 0, sizeof(MV_SM_MsgQ));
	}

	if (misc_register(&sm_dev))
		return -ENODEV;

	pm_power_off = berlin_power_off;
	arm_pm_restart = berlin_restart;
	spin_lock_init(&sm_lock);

	figo_sram = __arm_ioremap_exec(SRAM_PA, PAGE_SIZE, 0);
	if (!figo_sram) {
		pr_err("SRAM: Could not map\n");
		return -ENOMEM;
	}

	return request_irq(sm_irq, bsm_intr, 0, "bsm", &sm_dev);
}

static int berlin_sm_remove(struct platform_device *pdev)
{
	misc_deregister(&sm_dev);
	free_irq(sm_irq, &sm_dev);

	return 0;
}

static int berlin_sm_suspend(struct device *dev)
{
	int msg[2];

	msg[0] = 2;
	msg[1] = virt_to_phys(cpu_resume);
	bsm_msg_send(5, msg, 2 * sizeof(int));

	return 0;
}

static const struct dev_pm_ops berlin_sm_pmops = {
	.suspend	= berlin_sm_suspend,
};

static struct of_device_id berlin_sm_match[] = {
	{ .compatible = "marvell,berlin-sm", },
	{},
};
MODULE_DEVICE_TABLE(of, berlin_sm_match);

static struct platform_driver berlin_sm_driver = {
	.probe		= berlin_sm_probe,
	.remove		= berlin_sm_remove,
	.driver = {
		.name	= "marvell,berlin-sm",
		.owner	= THIS_MODULE,
		.of_match_table = berlin_sm_match,
		.pm	= &berlin_sm_pmops,
	},
};

module_platform_driver(berlin_sm_driver);

/*
 * write/read to ex memory on itcm
 */
int sm_write_to_ex(const void* src, int len, int offset)
{
	if((offset < 0) || (SM_ITCM_SOC_USE_SIZE <= offset))
		return -EINVAL;
	if(SM_ITCM_SOC_USE_SIZE < offset+len)
		len -= offset+len-SM_ITCM_SOC_USE_SIZE;

	memcpy((void*)SOC_ITCM(SM_ITCM_SOC_USE_ADDR+offset), src, len);

	return len;
}
EXPORT_SYMBOL(sm_write_to_ex);

int sm_read_from_ex(void* dst, int len, int offset)
{
	if((offset < 0) || (SM_ITCM_SOC_USE_SIZE <= offset))
		return -EINVAL;
	if(SM_ITCM_SOC_USE_SIZE < offset+len)
		len -= offset+len-SM_ITCM_SOC_USE_SIZE;

	memcpy(dst, (const void*)SOC_ITCM(SM_ITCM_SOC_USE_ADDR+offset), len);

	return len;
}
EXPORT_SYMBOL(sm_read_from_ex);

void sm_request_reboot(void)
{
	writel(0xEB00, SOC_DTCM(SM_DTCM_REQUEST_REBOOT));
}
EXPORT_SYMBOL(sm_request_reboot);

int sm_asserted_watchdog(void)
{
	return test_bit(SM_ASSERTED_WATCHDOG_BIT, &sm_asserted_watchdog_flag);
}
EXPORT_SYMBOL(sm_asserted_watchdog);

void sm_reset_watchdog(int value)
{
	writel(0xEEACD000 + (value & 0xFF), SOC_DTCM(SM_DTCM_RESET_WATCHDOG));
}
EXPORT_SYMBOL(sm_reset_watchdog);

void* sm_get_ex_mem_addr(void)
{
	return (void*)SOC_ITCM(SM_ITCM_SOC_USE_ADDR);
}
EXPORT_SYMBOL(sm_get_ex_mem_addr);

int sm_get_ex_mem_size(void)
{
	return SM_ITCM_SOC_USE_SIZE;
}
EXPORT_SYMBOL(sm_get_ex_mem_size);

int sm_register_wdt_callback( void (*cb_func)(void) )
{
	if(cb_func) {
		wdt_cb = cb_func;
	}
	return 0;
}
EXPORT_SYMBOL(sm_register_wdt_callback);

int sm_unregister_wdt_callback(void)
{
	wdt_cb = NULL;
	return 0;
}
EXPORT_SYMBOL(sm_unregister_wdt_callback);

MODULE_AUTHOR("Marvell-Galois");
MODULE_DESCRIPTION("System Manager Driver");
MODULE_LICENSE("GPL");
