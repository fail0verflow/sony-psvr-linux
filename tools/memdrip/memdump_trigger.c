/*
 *  memdrip/memdump_trigger.c
 *
 *  A kernel thread which will wait for user trigger and once get the user
 *  trigger dump the content (stack,heap,data) segment of that user process
 *  in the interval specified by user process
 *
 *  Copyright 2012,2013 Sony Corporation
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
 *  51 Franklin Street, Suite 500, Boston, MA 02110-1335, USA.
 *
 */

#include <linux/memdump_trigger.h>
#include <linux/memdump.h>
#include <linux/reboot.h>

struct task_struct *memwatch_task = NULL;
unsigned short dump_running  = 0;
unsigned short dump_interval = LEAK_DUMP_INTRVL_MS;
static int dump_corruption_page(struct task_struct *c);
extern bool memdump_auto_dump;
/*
 * Global Memdump thread structure
 */
memdump_trigger *memdump_thread;

struct timer_list mem_corruption_trigger;
EXPORT_SYMBOL(mem_corruption_trigger);

static int memdump_reboot_tsk(struct notifier_block *notifier,
		unsigned long what, void *data)
{
	memdump_auto_dump = 0;
	return 0;
}
static struct notifier_block reboot_notifier = {
	.notifier_call	= memdump_reboot_tsk,
	.priority	= 0,
};

/* disable_memdump(void)
 * This function will stop the memdripper dumping
 * intermeditelty in case of some error,
 */
static void disable_memdump(void)
{
	struct task_struct *c;

	//read_lock(&tasklist_lock);
	for_each_process(c) {
		get_task_struct(c);
		if (c->memdump)
			c->memdump  = DISABLE_MEMDUMP;
		if (c->memwatch)
			c->memwatch = DISABLE_MEMWATCH;
		put_task_struct(c);
	}
	//read_unlock(&tasklist_lock);

}
/* memdump parse_tasks( void )
 * This function loops through the circular linked list of
 * currently running tasks and checks for the memdrip_enable flag
 * If the flags is SET for a task, it calls the dump function for that
 * task.
*/
static int memdump_parse_tasks(void)
{
	struct task_struct *c;

	//read_lock(&tasklist_lock);
	for_each_process(c) {
		if (c->memdump) {
			get_task_struct(c);
			if ((memdump_all_seg(c)) != 0)
				goto err;
			put_task_struct(c);
			dump_interval = MEMDUMP_MS(c->memdump_interval);
		}

	}
	//read_unlock(&tasklist_lock);
	return 0;
err:
	put_task_struct(c);
	//read_unlock(&tasklist_lock);
	disable_memdump();
	return -1;
}

static int mark_same_pages(struct task_struct *c, int no_dumped_page,
						unsigned long seg_start)
{
	int i;

	for (i = no_dumped_page; i < c->memwatch; i++) {
		if ((c->watch_mark[i]) &&
			(seg_start == (c->memwatch_addr[i] & ~(PAGE_SIZE-1)))) {
				c->watch_mark[i] = 0;
		}
	}
	return 0;
}

static int dump_corruption_page(struct task_struct *c)
{
	unsigned long seg_start;
	unsigned long seg_end;
	int  i = 0, ret = 0;

	for (i = 0; i < c->memwatch; i++) {
		if (c->watch_mark[i]) {
			seg_start =  c->memwatch_addr[i] & ~(PAGE_SIZE-1);
			seg_end = seg_start + PAGE_SIZE ;
			c->watch_mark[i] = 0;
			mark_same_pages(c, i, seg_start);
			ret = set_memdripper_wrprotect_single_page(c,
							c->memwatch_addr[i]);
			if (ret != 0)
				return ret;
			ret = memdump_corruption(c, seg_start, seg_end,
							c->memwatch_addr[i]);
			if (ret != 0)
				return ret;
		}
	}
	return 0;
}

static int mem_corr_dump(void)
{
	struct task_struct *c;

	//read_lock(&tasklist_lock);
	for_each_process(c) {
		if (c->memwatch) {
			get_task_struct(c);
			if (dump_corruption_page(c) != 0)
				goto err;
			put_task_struct(c);
		}
	}
	//read_unlock(&tasklist_lock);
	return	0;
err:
	put_task_struct(c);
	//read_unlock(&tasklist_lock);
	disable_memdump();
	return	-1;
}

void memdump_corruption_alter_timer(unsigned long data)
{
	if (!timer_pending(&mem_corruption_trigger)) {
		mem_corruption_trigger.expires = MEMDUMP_TRIGGER(data);
		mod_timer(&(mem_corruption_trigger),
					mem_corruption_trigger.expires);
	}
}
EXPORT_SYMBOL(memdump_corruption_alter_timer);

void memdump_trigger_alter_timer(unsigned long data)
{
	if (!timer_pending(&memdump_thread->trigger)) {
		memdump_thread->trigger.expires = MEMDUMP_TRIGGER(dump_interval);
		mod_timer(&(memdump_thread->trigger),
					memdump_thread->trigger.expires);
	}
}
EXPORT_SYMBOL(memdump_trigger_alter_timer);

/*
 * Fn memdump_thread_handler( void *unused )
 * Thread handler function, which will execute continously,
 * triggering periodic events
 */
static int memdump_thread_handler(void *unused)
{
	int ret;
	DECLARE_WAITQUEUE(wait, current);

#ifdef CONFIG_SNSC_MEMDRIPPER_RT_PRIO
	struct sched_param param;
	param.sched_priority = MEMDRIPPER_RT_PRIO;
	if (sched_setscheduler(current, SCHED_FIFO, &param))
		MEMDRIP_DBG(KERN_ERR "memdripper:error in setting rt priority\n");
#endif

	add_wait_queue(&memdump_thread->event_wait, &wait);

	/* While loop for the kernel thread */
	while (!kthread_should_stop()) {
		if (dump_running == CORRUPTION_DUMP) {
			mem_corr_dump();
			dump_running = DISABLE_MEMDUMP;
		} else if (dump_running == LEAK_DUMP) {
			/* Checking for tasks SET for dump enable here */
			ret = memdump_parse_tasks();
			dump_running = DISABLE_MEMDUMP;
		}

		/* Trigger the thread Event here and put the task to sleep */
		set_current_state(TASK_INTERRUPTIBLE);
		schedule();
	}

	set_current_state(TASK_RUNNING);
	remove_wait_queue(&memdump_thread->event_wait, &wait);

	return 0;
}

/*
 * mem_corr_trigger_handler( unsigned long data )
 * Timer handler function, which will  be triggered every,
 * memdump_thread->trigger.expires interval of time
 */
static void mem_corr_trigger_handler(unsigned long data)
{

	if (!dump_running) {
		dump_running = CORRUPTION_DUMP;
		wake_up_interruptible(&memdump_thread->event_wait);
		memdump_corruption_alter_timer(CORR_DUMP_INTRVL);
	} else {
		memdump_corruption_alter_timer(CORR_DUMP_INTRVL);
	}
}
EXPORT_SYMBOL(mem_corr_trigger_handler);

/*
 * memdump_trigger_handler( unsigned long data )
 * Timer handler function, which will  be triggered every,
 * memdump_thread->trigger.expires interval of time
 */
static void memdump_trigger_handler(unsigned long data)
{
	/* if previous dump is not finished then dont wake up thread*/
	if (!dump_running) {
		dump_running = LEAK_DUMP;
		wake_up_interruptible(&memdump_thread->event_wait);
	}
	/*restart timer */
	memdump_trigger_alter_timer(dump_interval);
}

/*
 * Fn memdump_trigger_thread_create( void )
 * This function will create a thread of mca_thread->name,
 * and prepare to trigger it every mca_thread->trigger
 * interval
 */
int memdump_trigger_thread_create(void)
{
	int rval = 0;

	memdump_thread = kmalloc(sizeof(memdump_trigger), GFP_KERNEL);

	if (memdump_thread == NULL) {
		MEMDRIP_DBG(KERN_ERR "memdripper:memory allocation error\n");
		return -ENOMEM;
	}

	/* Initialize the task wait queue */
	init_waitqueue_head(&memdump_thread->event_wait);

	/* Initialize the MCA thread */
	strncpy(memdump_thread->name, MEMDUMP_THREAD, MEMDUMP_NAME_SIZE);

	/* Initialize the timer and its interval */
	init_timer(&(memdump_thread->trigger));
	memdump_thread->trigger.function = memdump_trigger_handler;
	memdump_thread->trigger.data = 0;
	memdump_thread->trigger.expires = MEMDUMP_TRIGGER(dump_interval);

	add_timer(&(memdump_thread->trigger));

	/* Spawn and Run the thread */
	memdump_thread->task = kthread_run(memdump_thread_handler, NULL,
			"%s", memdump_thread->name);
	if (IS_ERR(memdump_thread->task)) {
		MEMDRIP_DBG(KERN_ERR "memdripper:error creating %s thread\n",
				memdump_thread->name);
		rval = -EINVAL;
	}

	/* register a reboot notifier*/
	register_reboot_notifier(&reboot_notifier);

	/* add a timer for corruption */
	init_timer(&(mem_corruption_trigger));
	mem_corruption_trigger.function = mem_corr_trigger_handler;
	mem_corruption_trigger.data = 0;
	mem_corruption_trigger.expires = MEMDUMP_TRIGGER(CORR_DUMP_INTRVL);
	add_timer(&(mem_corruption_trigger));

	MEMDRIP_DBG(KERN_INFO "memdripper:kernel thread \'%s\' with pid %d\n",
			MEMDUMP_THREAD, memdump_thread->task->pid);

	return 0;
}
EXPORT_SYMBOL(memdump_trigger_thread_create);

/* Fn memdump_trigger_thread_delete( void )
 * This function will stop the kernel thread,
 * delete the timer and free the thread memory.
 */
int memdump_trigger_thread_delete(void)
{
	int rval = 0;

	if (memdump_thread != NULL) {

		MEMDRIP_DBG(KERN_INFO "memdripper:deleting memdrip thread\n");
		rval = kthread_stop(memdump_thread->task);

		del_timer(&(memdump_thread->trigger));

		del_timer(&(mem_corruption_trigger));

		kfree(memdump_thread);
		memdump_thread = NULL;
	}

	return rval;
}
EXPORT_SYMBOL(memdump_trigger_thread_delete);
