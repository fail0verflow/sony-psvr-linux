/*
 *  memdrip/memdump.c
 *
 *  Dump the segments of a user process
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
 *
 */

#include <linux/mm.h>
#include <linux/mount.h>
#include <linux/module.h>
#include <linux/mempolicy.h>
#include <linux/mman.h>
#include <linux/highmem.h>
#include <linux/hugetlb.h>
#include <linux/seq_file.h>
#include <linux/swap.h>
#include <linux/swapops.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/signal.h>
#include <linux/string.h>
#include <linux/pagemap.h>
#include <linux/ptrace.h>
#include <linux/proc_fs.h>
#include <linux/kernel.h>
#include <linux/ctype.h>
#include <linux/init.h>
#include <linux/time.h>
#include <linux/efi.h>
#include <linux/errno.h>
#include <linux/file.h>
#include <linux/uaccess.h>
#include <linux/mmu_notifier.h>
#include <linux/memdump.h>
#include <linux/memdump_trigger.h>
#include <linux/elf.h>
#include <linux/io.h>

#include <asm/cacheflush.h>
#include <asm/tlbflush.h>

#include "memdump_fwrite.h"

DEFINE_RWLOCK(memdrip_dump_count_lock);
unsigned long memdrip_dump_count;
unsigned long **mss_addr;
unsigned long referenced = 0;

/*
 * Structure holds page usage info
 */
struct mem_size_stats {
	struct vm_area_struct *vma;
	unsigned long resident;
	unsigned long shared_clean;
	unsigned long shared_dirty;
	unsigned long private_clean;
	unsigned long private_dirty;
	unsigned long referenced;
	unsigned long swap;
	u64 pss;
};

/*
 * function to get the content of the user memory
 */
static struct page *get_dump_page_task(unsigned long addr,
		struct task_struct *dump_task)
{
	struct vm_area_struct *vma;
	struct page *page;

	if (get_user_pages(dump_task, dump_task->mm, addr, 1,
				0, FOLL_FORCE, &page, &vma) < 1)
		return NULL;
	return page;
}

/*
 * Write the content of the user memory to /tmp/dump.pid. file
 */
static int dump_content_to_file(unsigned long start_addr,
		struct task_struct *dump_task)
{
	unsigned long value;
	struct page *page;
	void *kaddr;
	unsigned long end_addr = start_addr + PAGE_SIZE;
	int ret = 0;

	page = get_dump_page_task(start_addr, dump_task);
	if (page) {
		if ((write_to_file(&start_addr, ULONG_LEN)) != 0)
			goto err;
		if ((write_to_file(&end_addr, ULONG_LEN)) != 0)
			goto err;

		kaddr = kmap(page);

		/* During first dump skip all the page with zeros */
		if (dump_task->memdump_count == 0) {
			unsigned int cnt;
			for (cnt = 0; (cnt < PAGE_SIZE/4); cnt++) {
				value = *((unsigned long *)(kaddr) + cnt);
				if (value != 0) {
					ret = write_to_file(kaddr, PAGE_SIZE);
					if (ret != 0)
						goto err;
					break;
				}
			}
		} else {
			if (write_to_file(kaddr, PAGE_SIZE) != 0)
				goto err;
		}
		kunmap(page);
		page_cache_release(page);
	}
	return 0;

err:
	kunmap(page);
	page_cache_release(page);
	return -1;
}

/*
 * walk through entrite pte range of a vma
 */
static int chk_pte_memdump(pmd_t *pmd, unsigned long addr, unsigned long end,
		struct mm_walk *walk)
{
	pte_t *pte, ptent;
	pte_t entry;
	spinlock_t *ptl;
	struct page *page;
	struct mem_size_stats *mss = walk->private;
	struct vm_area_struct *vma = mss->vma;

	mss->referenced = referenced;
	pte = pte_offset_map_lock(vma->vm_mm, pmd, addr, &ptl);

	for (; addr != end; pte++, addr += PAGE_SIZE) {
		ptent = *pte;

		if (is_swap_pte(ptent)) {
			mss->swap += PAGE_SIZE;
			continue;
		}

		if (!pte_present(ptent))
			continue;

		page = vm_normal_page(vma, addr, ptent);
		if (!page)
			continue;
		/* Check dump bit is set in the page*/
		if (pte_memdump(ptent)) {
			/* add the dump bit set address for dumping */
			mss_addr[mss->referenced] = (unsigned long *) addr;
			mss->referenced += 1;
		}
		/* clear the dump bit for dump bit set page*/
		if (pte_memdump(ptent)) {
			entry = ptent;
			flush_cache_page(vma, addr, pte_pfn(ptent));
			entry = ptep_clear_flush_notify(vma, addr, pte);
			entry = pte_wrprotect(entry);
			set_pte_at(vma->vm_mm, addr, pte, entry);
			ptep_clear_dump(vma, addr, pte);
		}
	}

	referenced = mss->referenced;

	pte_unmap_unlock(pte - 1, ptl);

	return 0;
}

/*
 * Check all the pages marked for dumping
 */
int find_and_dump_ref_pages(struct vm_area_struct *v)
{

	struct vm_area_struct *vma = v;
	struct mem_size_stats mss;
	struct mm_walk smaps_walk = {
		.pmd_entry = chk_pte_memdump,
		.mm = vma->vm_mm,
		.private = &mss,
	};

	memset(&mss, 0, sizeof mss);
	mss.vma = vma;
	referenced = 0;

	/* mmap_sem is held in m_start */
	if (vma->vm_mm && !is_vm_hugetlb_page(vma))
		walk_page_range(vma->vm_start, vma->vm_end, &smaps_walk);

	return 0;
}

/*
 * Open the dump file in /tmp
 */
int open_dump_file(struct task_struct *task, char dump_seg,
		unsigned long dump_count)
{
	char file_name[MEMDRIP_FILE_SIZE];
	char seg_name;
	char file_location[] = MEMDRIP_DUMP_LOCATION;

	switch (dump_seg) {
	case DUMP_STACK:
		seg_name = 's';
		break;
	case DUMP_HEAP:
		seg_name = 'h';
		break;
	case DUMP_DATA:
		seg_name = 'd';
		break;
	case DUMP_COR:
		seg_name = 'c';
		break;
	default:
		return -1;
	}
	/* dump file name format /tmp/pid_ppid_addr_seg_name_count */
	sprintf(file_name, "%s%s.%d.%d.%c.%ld", file_location, "dump",
			task->pid, task->parent->pid, seg_name, dump_count);

	/* open file at the first page*/
	if ((open_file(file_name))) {
		MEMDRIP_DBG(KERN_ERR "memdripper:error in File opening %s\n",
				__func__);
		return -1;
	}

	return 0;
}

/*
 * close the handler of dump file
 */
int close_dump_file(void)
{
	if (close_file() != 0)
		MEMDRIP_DBG(KERN_ERR "memdripper:error in closing file %s\n",
				__func__);
	return 0;
}

/*
 * write all register state to file
 */
int write_regs_to_file(struct pt_regs *regs)
{
	unsigned char reg_cnt;
	for (reg_cnt = 0; reg_cnt < 18; reg_cnt++) {
		if (write_to_file((unsigned long *)&regs->uregs[reg_cnt],
					sizeof(unsigned long)) != 0) {
			return -1;
		}
	}
	return 0;
}

/*
 * write dump file header
 */
static int write_the_header(struct task_struct *task,
		unsigned short thread_flag, unsigned long seg_start,
		unsigned long seg_end)
{
	unsigned long pid, ppid;

	pid = task->pid;
	ppid = task->parent->pid;

	/* Write start of segment and end of segment */
	if (write_to_file(&pid, ULONG_LEN) != 0)
		goto err;

	if (thread_flag != THREAD_MEMDUMP)
		if (write_to_file(&ppid, ULONG_LEN) != 0)
			goto err;

	if (write_to_file(&seg_start, ULONG_LEN) != 0)
		goto err;

	if (write_to_file(&seg_end, ULONG_LEN) != 0)
		goto err;

	return 0;
err:
	return -1;
}

/*
 * write user process segments to a file in specific format
 */
static int dump_segment_in_frmt(unsigned long seg_start, unsigned long seg_end,
		struct task_struct *task,
		struct vm_area_struct *vma,
		bool thread_flag)
{

	unsigned long count;
	struct pt_regs *regs;
	unsigned long page_cnt = 0;

	page_cnt = ((seg_end - seg_start) / PAGE_SIZE);

	/* malloc for number of address */
	mss_addr = kmalloc((page_cnt*sizeof(unsigned long)), GFP_ATOMIC);
	if (!mss_addr) {
		MEMDRIP_DBG(KERN_ERR "memdripper: kmalloc failed (%s)\n", __func__);
		return -1;
	}
	/* Get The referenced pages */
	if (find_and_dump_ref_pages(vma) != 0)
		goto err;

	if (write_the_header(task, thread_flag, seg_start, seg_end) != 0)
		goto err;

	/* write page count after segment address */
	if (write_to_file(&referenced, ULONG_LEN) != 0)
		goto err;

	/* Get present register status*/
	if (thread_flag != THREAD_MEMDUMP) {
		regs = task_pt_regs(task);
		if (write_regs_to_file(regs) != 0)
			goto err;
	}

	/* Dump the content of ref pages to file */
	for (count = 0; count < referenced; count++) {
		if (dump_content_to_file((unsigned long)mss_addr[count],
					task) != 0)
			goto err;
	}

	kfree(mss_addr);
	return 0;

err:
	kfree(mss_addr);
	return -1;
}

/*
 * write user process segments to a file in specific format for corruption
 */
static int dump_segment_in_frmt_corr(unsigned long seg_start,
		unsigned long seg_end, struct task_struct *task,
		struct vm_area_struct *vma, unsigned short thread_flag)
{

	unsigned long count;
	unsigned long page_cnt = 0;
	int ret = 0;
	referenced = DUMP_SINGLE_PAGE;

	page_cnt = ((seg_end - seg_start) / PAGE_SIZE);

	ret = write_the_header(task, thread_flag, seg_start, seg_end);
	if (ret != 0)
		goto out;

	/* write page count after segment address */
	ret = write_to_file(&referenced, ULONG_LEN);
	if (ret != 0)
		goto out;

	/* Dump the content of ref pages to file */
	for (count = 0; count < referenced; count++) {
		ret = dump_content_to_file(seg_start, task);
		if (ret != 0)
			goto out;
	}

out:
	return ret;
}

static inline int vm_is_stack_for_task(struct task_struct *t,
		struct vm_area_struct *vma)
{
	return (vma->vm_start <= KSTK_ESP(t) && vma->vm_end >= KSTK_ESP(t));
}

/*
 * dump the stack segment of the thread
 */
int dump_thread_stack(struct task_struct *task)
{

	struct task_struct *thread_t = task;
	struct mm_struct *mm_t;
	struct vm_area_struct *vma_t;
	int ret = 0;

	/*Go through each thread */
	while_each_thread(task, thread_t) {
		mm_t = thread_t->mm;
		vma_t = mm_t->mmap;

		/* find out stack vma and dump that */
		for (vma_t = mm_t->mmap; vma_t; vma_t = vma_t->vm_next) {
			if (vm_is_stack_for_task(thread_t, vma_t)) {
				ret = dump_segment_in_frmt(vma_t->vm_start,
						vma_t->vm_end, thread_t, vma_t,
						THREAD_MEMDUMP);
				if (ret != 0)
					return ret;
			}
		}
	}

	return 0;
}

/*
 *Find each segment start and end address
 */
static int find_segment_vma(struct task_struct *task, char segment,
		unsigned long dump_count)
{
	struct mm_struct *mm;
	struct vm_area_struct *vma;
	unsigned long stack_end = 0, stack_start = 0;
	unsigned long heap_end  = 0, heap_start  = 0;
	unsigned long data_end  = 0, data_start  = 0;
	unsigned long thread_cnt = 0;
	unsigned char data_end_flag;
	int ret = 0;

	if (!task->memdump)
		return 0;

	mm = get_task_mm(task);
	if (mm) {
		down_read(&mm->mmap_sem);

		/* Data segment can extend in multiple pages so to get the end
		   of data segment vma ,using this flag */
		data_end_flag = 1;

		/* go through each vma and find the start and end */
		for (vma = mm->mmap; vma; vma = vma->vm_next) {
			if ((vm_is_stack_for_task(task, vma) &&
						(segment == DUMP_STACK))) {

				stack_end = vma->vm_end;
				stack_start = vma->vm_start;

				ret = open_dump_file(task, segment, dump_count);
				if (ret != 0)
					goto err;
				/* dump the segment in dump format */
				ret = dump_segment_in_frmt(stack_start,
						stack_end, task, vma,
						PROCESS_MEMDUMP);
				if (ret != 0) {
					close_dump_file();
					goto err;
				}
				/* if threads are present dump thread stack */
				thread_cnt = get_nr_threads(task);
				thread_cnt--;
				if ((write_to_file(&thread_cnt, ULONG_LEN)) != 0)
					goto err;
				if (thread_cnt > 0) {
					if (dump_thread_stack(task) != 0)
						goto err;
				}
				ret = close_dump_file();
				if (ret != 0)
					goto err;
				break;
			}

			if (vma->vm_start <= mm->start_brk &&
					vma->vm_end >= mm->brk
					&& (mm->brk - mm->start_brk)
					&& (segment == DUMP_HEAP)) {

				heap_start = vma->vm_start;
				heap_end   = vma->vm_end;

				ret = open_dump_file(task, segment, dump_count);
				if (ret != 0)
					goto err;

				ret = dump_segment_in_frmt(heap_start, heap_end,
						task, vma, PROCESS_MEMDUMP);

				if (ret != 0) {
					close_dump_file();
					goto err;
				}
				ret = close_dump_file();
				if (ret != 0)
					goto err;
				break;
			}
			if (vma->vm_start <= mm->start_data)
				data_start = vma->vm_start;
			if (vma->vm_end >= mm->end_data && data_end_flag
					&& (segment == DUMP_DATA)) {

				data_end   = vma->vm_end;
				data_end_flag = 0;
				ret = open_dump_file(task, segment, dump_count);
				if (ret != 0)
					goto err;

				ret = dump_segment_in_frmt(data_start, data_end,
						task, vma, PROCESS_MEMDUMP);

				if (ret != 0) {
					close_dump_file();
					goto err;
				}
				ret = close_dump_file();
				if (ret != 0)
					goto err;
				break;
			}
		}
		up_read(&mm->mmap_sem);
		mmput(mm);
	}

	/* in case segment not present write that information to file */
	if (((heap_start == 0  && heap_end  == 0)
					&& (segment == DUMP_HEAP))  ||
			((stack_start == 0 && stack_end == 0)
					&& (segment == DUMP_STACK)) ||
			((data_start == 0  && data_end  == 0)
					&& (segment == DUMP_DATA))) {

		ret = open_dump_file(task, segment, dump_count);
		if (ret != 0)
			goto out;

		ret = write_the_header(task, PROCESS_MEMDUMP,
				0, 0);
		if (ret != 0) {
			close_dump_file();
			goto out;
		}
		ret = close_dump_file();
		goto out;
	}
out:
	return ret;

err:
	up_read(&mm->mmap_sem);
	mmput(mm);
	return ret;
}

/*
 * Function to dump one segment for corruption analysis
 */
int memdump_corruption(struct task_struct *task, unsigned long seg_start,
		unsigned long seg_end, unsigned long addr)
{
	unsigned long dump_count;
	struct mm_struct *mm;
	int ret = 0;

	dump_count = modify_memdripper_count(MEMDRIPPER_ONE);

	mm = get_task_mm(task);
	if (mm) {
		down_read(&mm->mmap_sem);
		ret = open_dump_file(task, DUMP_COR, dump_count);
		if (ret != 0) {
			mmput(mm);
			goto out;
		}
		/* dump the segment in dump format */
		dump_segment_in_frmt_corr(seg_start, seg_end, task,
				NULL, CORRUPTION_DUMP);
		if (ret != 0) {
			close_dump_file();
			goto out;
		}
		ret = close_dump_file();
out:
		up_read(&mm->mmap_sem);
		mmput(mm);
	}
	return ret;
}
EXPORT_SYMBOL(memdump_corruption);

/*
 * Function to dump all the segments of the memory
 */
int memdump_all_seg(struct task_struct *task)
{
	unsigned long dump_count;
	int ret = 0;

	dump_count = modify_memdripper_count(MEMDRIPPER_ONE);

	/* Dump Stack segment */
	ret = find_segment_vma(task, DUMP_STACK, dump_count);
	if (ret != 0)
		return ret;

	/* Dump heap segment */
	ret = find_segment_vma(task, DUMP_HEAP, dump_count);
	if (ret != 0)
		return ret;

	/* Dump data segment */
	ret = find_segment_vma(task, DUMP_DATA, dump_count);
	if (ret != 0)
		return ret;

	task->memdump_count++;
	return ret;
}
EXPORT_SYMBOL(memdump_all_seg);

/*
 * Create an empty file in /tmp directory as notifcation of
 * process exit
 */
void memdump_exit_notifier(struct task_struct *task)
{
	unsigned long dump_count;
	struct file *exit_file;
	char file_name[MEMDRIP_FILE_SIZE];
	char file_location[] = MEMDRIP_DUMP_LOCATION;

	dump_count = modify_memdripper_count(MEMDRIPPER_ONE);

	/* stop the dump */
	task->memdump = 0;

	sprintf(file_name, "%s%s.%d.%d.%c.%ld", file_location, "dump",
			task->pid, task->parent->pid, 'e', dump_count);

	exit_file = filp_open(file_name, O_CREAT | O_TRUNC, S_IRUGO | S_IWUSR);

	if (IS_ERR(exit_file)) {
		MEMDRIP_DBG(KERN_ERR "memdripper:can't create '%s'\n", file_name);
		return ;
	}
	filp_close(exit_file, NULL);

	return ;
}
EXPORT_SYMBOL(memdump_exit_notifier);

/*
 * update memdripper dump count value as with new value
 */
void set_memdripper_count(unsigned long value)
{
	write_lock(&memdrip_dump_count_lock);
	memdrip_dump_count = value;
	write_unlock(&memdrip_dump_count_lock);
}
EXPORT_SYMBOL(set_memdripper_count);

/*
 * return current memdripper dump count
 */
int get_memdripper_count(void)
{
	unsigned long ret;

	write_lock(&memdrip_dump_count_lock);
	ret = memdrip_dump_count;
	write_unlock(&memdrip_dump_count_lock);
	return ret;
}
EXPORT_SYMBOL(get_memdripper_count);

/*
 * add memdripper dump count value with another value
 * and return the new value
 */
int modify_memdripper_count(unsigned long value)
{
	unsigned long ret;

	write_lock(&memdrip_dump_count_lock);
	memdrip_dump_count += value;
	ret = memdrip_dump_count;
	write_unlock(&memdrip_dump_count_lock);
	return ret;
}
EXPORT_SYMBOL(modify_memdripper_count);
