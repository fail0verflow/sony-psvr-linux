#include "sched.h"

/*
 * stop-task scheduling class.
 *
 * The stop task is the highest priority task in the system, it preempts
 * everything and will be preempted by nothing.
 *
 * See kernel/stop_machine.c
 */

#ifdef CONFIG_SMP
static int
select_task_rq_stop(struct task_struct *p, int sd_flag, int flags)
{
	return task_cpu(p); /* stop tasks as never migrate */
}
#endif /* CONFIG_SMP */

static void
check_preempt_curr_stop(struct rq *rq, struct task_struct *p, int flags)
{
	/* we're never preempted */
}

static struct task_struct *pick_next_task_stop(struct rq *rq)
{
	struct task_struct *stop = rq->stop;

	if (stop && stop->on_rq) {
		stop->se.exec_start = rq->clock_task;
		return stop;
	}

	return NULL;
}

static void
enqueue_task_stop(struct rq *rq, struct task_struct *p, int flags)
{
	inc_nr_running(rq);
}

static void
dequeue_task_stop(struct rq *rq, struct task_struct *p, int flags)
{
	dec_nr_running(rq);
}

static void yield_task_stop(struct rq *rq)
{
	BUG(); /* the stop task should never yield, its pointless. */
}

static void put_prev_task_stop(struct rq *rq, struct task_struct *prev)
{
	struct task_struct *curr = rq->curr;
	u64 delta_exec;

	delta_exec = rq->clock_task - curr->se.exec_start;
	if (unlikely((s64)delta_exec < 0))
		delta_exec = 0;

	schedstat_set(curr->se.statistics.exec_max,
			max(curr->se.statistics.exec_max, delta_exec));

	curr->se.sum_exec_runtime += delta_exec;
	account_group_exec_runtime(curr, delta_exec);

	curr->se.exec_start = rq->clock_task;
	cpuacct_charge(curr, delta_exec);
}

static void task_tick_stop(struct rq *rq, struct task_struct *curr, int queued)
{
}

static void set_curr_task_stop(struct rq *rq)
{
	struct task_struct *stop = rq->stop;

	stop->se.exec_start = rq->clock_task;
}

static void switched_to_stop(struct rq *rq, struct task_struct *p)
{
	BUG(); /* its impossible to change to this class */
}

static void
prio_changed_stop(struct rq *rq, struct task_struct *p, int oldprio)
{
	BUG(); /* how!?, what priority? */
}

static unsigned int
get_rr_interval_stop(struct rq *rq, struct task_struct *task)
{
	return 0;
}

/*
 * Simple, special scheduling class for the per-CPU stop tasks:
 */
const struct sched_class stop_sched_class = {
	.next			= &rt_sched_class,

	.enqueue_task		= enqueue_task_stop,
	.dequeue_task		= dequeue_task_stop,
	.yield_task		= yield_task_stop,

	.check_preempt_curr	= check_preempt_curr_stop,

	.pick_next_task		= pick_next_task_stop,
	.put_prev_task		= put_prev_task_stop,

#ifdef CONFIG_SMP
	.select_task_rq		= select_task_rq_stop,
#endif

	.set_curr_task          = set_curr_task_stop,
	.task_tick		= task_tick_stop,

	.get_rr_interval	= get_rr_interval_stop,

	.prio_changed		= prio_changed_stop,
	.switched_to		= switched_to_stop,
};

/* Stop all tasks running with the given mm, except for the calling task.  */
int stop_all_threads_timeout(struct mm_struct *mm, long timeout_msec,
			     int give_up_all_sibling)
{
	struct task_struct *g, *p;
	int all_stopped = 0;
	unsigned long end_time;
	int ret = 0;

	if (mm != NULL)
		set_bit(MMF_STOPPING_SIBLINGS_BIT, &mm->flags);
	read_lock(&tasklist_lock);
	do_each_thread(g,p) {
		if (p->mm != NULL && p != current && p->state != TASK_STOPPED) {
			send_sig (SIGSTOP, p, 1);
		}
	}while_each_thread(g,p);

	/* Now wait for every task to cease running.  */
	/* Beware: this loop might not terminate in the case of a malicious
	   program sending SIGCONT to threads.  But it is still killable, and
	   only moderately disruptive (because of the tasklist_lock).  */
	/* Ignore uninterruptible tasks because they may not respond
	   to the signal for unacceptably long period.	Instead, wait
	   for them with a timeout in the latter loop */
	for (;!give_up_all_sibling;) {
		all_stopped = 1;
		do_each_thread(g,p) {
			if (p->mm != NULL && p != current
			    && p->state != TASK_STOPPED
			    && p->state != TASK_UNINTERRUPTIBLE) {
				all_stopped = 0;
				break;
			}
		}while_each_thread(g,p);
		if (all_stopped)
			break;
		read_unlock(&tasklist_lock);
		set_current_state(TASK_UNINTERRUPTIBLE);
		schedule_timeout(1);
		read_lock(&tasklist_lock);
	}

	/* Wait (with timeout) for uninterruptible tasks to cease running.  */
	end_time = jiffies + msecs_to_jiffies(timeout_msec);
	for (;;) {
		all_stopped = 1;
		do_each_thread(g,p) {
			if (p->mm != NULL && p != current && p->pid != 1
			    && p->state != TASK_STOPPED) {
				all_stopped = 0;
				break;
			}
		}while_each_thread(g,p);
		if (all_stopped)
			break;
		if (timeout_msec >= 0 && time_before(end_time, jiffies)) {
			ret = -ETIMEDOUT;
			break;
		}
		read_unlock(&tasklist_lock);
		set_current_state(TASK_UNINTERRUPTIBLE);
		schedule_timeout(1);
		read_lock(&tasklist_lock);
	}

	read_unlock(&tasklist_lock);
#ifdef CONFIG_SMP
	/* wait task inactive could sleep for ever, so skip doing this... */
	if (!give_up_all_sibling)
		/* Make sure they've all gotten off their CPUs.  */
		do_each_thread(g,p) {
			if (p->mm != NULL && p != current
			    && p->state != TASK_UNINTERRUPTIBLE)
				wait_task_inactive(p, 0);
		}while_each_thread(g,p);
#endif
	if (mm != NULL)
		clear_bit(MMF_STOPPING_SIBLINGS_BIT, &mm->flags);
	return ret;
}

/* Restart all the tasks with the given mm.  Hope none of them were in state
   TASK_STOPPED for some other reason...  */
void start_all_threads(struct mm_struct *mm)
{
	struct task_struct *g, *p;

	read_lock(&tasklist_lock);
	do_each_thread(g,p) {
		if (p->mm != NULL && p != current) {
			send_sig (SIGCONT, p, 1);
		}
	}while_each_thread(g,p);
	read_unlock(&tasklist_lock);
}
