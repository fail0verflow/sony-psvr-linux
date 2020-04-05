/******************************************************************************
 * Copyright (c) 2013-2014 Marvell International Ltd. and its affiliates.
 * All rights reserved.
 *
 * This software file (the "File") is owned and distributed by Marvell
 * International Ltd. and/or its affiliates ("Marvell") under the following
 * licensing terms.
 ******************************************************************************
 * Marvell Commercial License Option
 *
 * If you received this File from Marvell and you have entered into a
 * commercial license agreement (a "Commercial License") with Marvell, the
 * File is licensed to you under the terms of the applicable Commercial
 * License.
 ******************************************************************************
 * Marvell GPL License Option
 *
 * If you received this File from Marvell, you may opt to use, redistribute
 * and/or modify this File in accordance with the terms and conditions of the
 * General Public License Version 2, June 1991 (the "GPL License"), a copy of
 * which is available along with the File in the license.txt file or by writing
 * to the Free Software Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 or on the worldwide web at http://www.gnu.org/licenses/gpl.txt.
 * THE FILE IS DISTRIBUTED AS-IS, WITHOUT WARRANTY OF ANY KIND, AND THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE ARE
 * EXPRESSLY DISCLAIMED. The GPL License provides additional details about this
 * warranty disclaimer.
 *******************************************************************************/
#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/uio.h>
#include <asm/uaccess.h>

#include "ree_sys_callback_logger.h"

static int ree_logger_notify(struct notifier_block *self,
		unsigned long action, void *data)
{
	struct ree_logger_param *param = (struct ree_logger_param *) (data);
	pr_info("I %s(%d): %s", param->tag, param->prio, param->text);
	return NOTIFY_DONE;
}

static struct notifier_block nb = {
	.notifier_call	= ree_logger_notify,
};

static int __init tz_logger_init(void)
{
    register_ree_logger_notifier(&nb);
    return 0;
}

static void __exit tz_logger_deinit(void)
{
    unregister_ree_logger_notifier(&nb);
}

#if 0
static int tz_logger_open(struct inode *inode, struct file *filp)
{
	filp->private_data = (void *)log_filps[LOG_ID_SYSTEM];
	return 0;
}

static ssize_t tz_logger_write(struct file *filp, const char __user *buf,
		size_t count, loff_t *ppos)
{
	struct file *log_filp = (struct file *)(filp->private_data);
	char text[128];
	if (unlikely(log_filp == NULL))
		return -ENODEV;
	if (count >= sizeof(text))
		count = sizeof(text)-1;
	if (copy_from_user(text, buf, count))
		return -EFAULT;
	text[count] = 0;
	return (ssize_t)__android_log_buf_write(log_filp, ANDROID_LOG_ERROR, "TZLOGGER", text);
}

static struct file_operations tz_logger_fops =  {
	.owner		= THIS_MODULE,
	.open		= tz_logger_open,
	.write		= tz_logger_write,
};

static struct miscdevice tz_logger_dev = {
	.minor		= MISC_DYNAMIC_MINOR,
	.name		= "tzlog",
	.fops		= &tz_logger_fops,
};

static int __init tz_logger_dev_init(void)
{
	int res;

	res = tz_logger_init();
	if (res)
		return res;
	res = misc_register(&tz_logger_dev);
	if (res)
		tz_logger_deinit();

	return res;
}

static void __exit tz_logger_dev_exit(void)
{
	misc_deregister(&tz_logger_dev);
	tz_logger_deinit();
}

module_init(tz_logger_dev_init);
module_exit(tz_logger_dev_exit);
#else
module_init(tz_logger_init);
module_exit(tz_logger_deinit);
#endif

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Marvell");
MODULE_DESCRIPTION("Marvell TZ logger driver");
MODULE_VERSION("1.00");
