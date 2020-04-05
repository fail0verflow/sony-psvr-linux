/*
 *  emnotify.c  - notification from EM to user
 */

/* With non GPL files, use following license */
/*
 * Copyright 2004-2006,2008 Sony Corporation.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions, and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
/* Otherwise with GPL files, use following license */
/*
 *  Copyright 2004-2006,2008 Sony Corporation.
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  version 2 of the  License.
 *
 *  THIS  SOFTWARE  IS PROVIDED   ``AS  IS'' AND   ANY  EXPRESS OR IMPLIED
 *  WARRANTIES,   INCLUDING, BUT NOT  LIMITED  TO, THE IMPLIED WARRANTIES OF
 *  MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN
 *  NO  EVENT  SHALL   THE AUTHOR  BE    LIABLE FOR ANY   DIRECT, INDIRECT,
 *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 *  NOT LIMITED   TO, PROCUREMENT OF  SUBSTITUTE GOODS  OR SERVICES; LOSS OF
 *  USE, DATA,  OR PROFITS; OR  BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 *  ANY THEORY OF LIABILITY, WHETHER IN  CONTRACT, STRICT LIABILITY, OR TORT
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 *  THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *  You should have received a copy of the  GNU General Public License along
 *  with this program; if not, write  to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <linux/poll.h>
#include <linux/proc_fs.h>
#include <linux/sched.h>
#include <linux/pid_namespace.h>
#include <linux/tty.h>
#include <asm/uaccess.h>

#include "exception.h"

static struct proc_dir_entry *em_proc_entry = NULL;

static DECLARE_WAIT_QUEUE_HEAD(emn_wait);
static DECLARE_COMPLETION(emn_done);
static DEFINE_MUTEX(emn_mutex);
static int emn_alive = 0;
static int emn_readable = 0;
static int emn_pos = 0;
static int emn_len = 0;
static char emn_msg[LOG_BUF_SZ];

static ssize_t em_notify_read(struct file *file, char __user *buf,
			 size_t count, loff_t *ppos)
{
	ssize_t ret;
	size_t readable_size, write_size;

	/*
	 *  This simple implementation assumes single-threadness:
	 *   - One reader.  Otherwise, two readers may (or may not) be
	 *     simultaneously woken up and acquire the same string.
	 *   - After the reader is woken up, EM is expected to enter
	 *     sleeping and never call the next em_notify at least the
	 *     reader have read the entire string.  Otherwise, the
	 *     reader may lost (possibly part of) notifications.
	 *
	 *  These limitations are due to the fact that wait_event()
	 *  and mutex_lock(&emn_lock) is not atomic.  Fixing it is
	 *  possible but less-meaningfull because single-threadness is
	 *  anyway required by the overall design of em_notify
	 *  mechanism, e.g, there in no mechanism to maintain
	 *  correspondence between multiple notifycations and acks.
	 */
	ret = wait_event_interruptible(emn_wait, !emn_alive || emn_readable);
	if (ret)
		return ret;
	if (!emn_alive)
		/* rmmod'ed while waiting */
		return -ENODEV;
	/* readable */
	mutex_lock(&emn_mutex);
	readable_size = emn_len - emn_pos;
	write_size = min(count, readable_size);
	ret = -EFAULT;
	if (copy_to_user(buf, emn_msg + emn_pos, write_size))
		goto out;
	if (write_size == readable_size)
		emn_readable = 0;
	*ppos += write_size;
	emn_pos += write_size;
	ret = write_size;
out:
	mutex_unlock(&emn_mutex);
	return ret;
}

static void em_notify_send(const char *str)
{
	bool signal_send = 0;
	struct task_struct *g, *p;

	if (!not_interrupt) {
		em_dump_write("\nException Monitor invoked from kernel mode:\n"
			      "Can't notify to userspace.\n");
		return;
	}

	/* send SIGCONT signal to foreground process
	   to let notify command work for ARM platform */
	rcu_read_lock();
	do_each_thread(g,p) {
		pid_t pgid = task_pgrp_nr_ns(p, NULL);
		if (p->signal->tty) {
			struct pid *pgrp = tty_get_pgrp(p->signal->tty);
			int tty_pgrp = pid_vnr(pgrp);
			/* send signal only if current process is not foreground */
			if ((pgid == tty_pgrp) && (p->tgid != current->tgid)) {
				/* Get the PPID of the p process */
				pid_t ppid = task_tgid_nr_ns(p->real_parent, current->nsproxy->pid_ns);
				struct task_struct *p_task = pid_task(find_pid_ns(ppid, current->nsproxy->pid_ns), 0);

				/* get the tty_process group id for Parent process */
				if (p_task->signal->tty) {
					int p_tty_pgrp;
					pgrp = tty_get_pgrp(p_task->signal->tty);
					p_tty_pgrp = pid_vnr(pgrp);
					if (p_tty_pgrp == tty_pgrp)
						continue;
				}
				send_sig (SIGCONT, p, 1);
				signal_send = 1;
			}
		}
	}while_each_thread(g,p);
	rcu_read_unlock();

	if (!signal_send) {
		em_dump_write("\n Current process running in foreground: can't send notify command \n");
		return;
	}

	em_enable_irq(); /* allow serial interrupt */

	mutex_lock(&emn_mutex);
	init_completion(&emn_done);  /* dispose old notification */
	emn_len = scnprintf(emn_msg, LOG_BUF_SZ, "%s\n", str);
	emn_pos = 0;
	emn_readable = 1;
	wake_up_interruptible(&emn_wait);
	mutex_unlock(&emn_mutex);

	printk("\nEM: waiting for /proc/driver/em/notify\n");
	wait_for_completion(&emn_done);
	em_flush_serial();
	printk("EM: done waiting\n");
	em_disable_irq();
}

void em_notify_cmd(int argc, char **argv)
{
	const char *str;

	if (argc <= 1)
		str = "-";
	else
		str = argv[1];
	em_notify_send(str);
}

void em_notify_enter(void)
{
#ifdef CONFIG_SNSC_EM_NOTIFY_ENTER
	em_notify_send("ENTER");
#endif
}

static ssize_t em_notify_write(struct file *file, const char __user * buffer,
			      size_t count, loff_t *ppos)
{
	mutex_lock(&emn_mutex);
	complete(&emn_done);
	mutex_unlock(&emn_mutex);
	*ppos += count;
	return count;
}

static unsigned int em_notify_poll(struct file *file, poll_table *wait)
{
	unsigned int ret = 0;

	poll_wait(file, &emn_wait, wait);
	if (emn_readable)
		ret = POLLIN | POLLRDNORM;
	return ret;
}


static const struct file_operations proc_em_notify_operations = {
	.read		= em_notify_read,
	.write		= em_notify_write,
	.poll		= em_notify_poll,
};

int em_notify_register(struct proc_dir_entry *proc_dir)
{
	struct proc_dir_entry *entry;

	emn_alive = 1;
	em_proc_entry = proc_dir;
	entry = proc_create_data("notify", S_IWUSR|S_IRUSR, em_proc_entry, &proc_em_notify_operations, NULL);
	if (!entry) {
		printk(KERN_ERR
		       "Exception Montior: Unable to create proc entry\n");
		return -ENOMEM;
	}
	return 0;
}


void em_notify_unregister(void)
{
	mutex_lock(&emn_mutex);
	emn_alive = 0;
	wake_up_interruptible(&emn_wait);
	mutex_unlock(&emn_mutex);

	remove_proc_entry("notify", em_proc_entry);
}

