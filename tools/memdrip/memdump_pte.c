/*
 *  memdrip/memdump_pte.c
 *
 *  Functions which set/clear the page table bits for memdump module
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
#include <linux/mm.h>
#include <linux/hugetlb.h>
#include <linux/mount.h>
#include <linux/seq_file.h>
#include <linux/highmem.h>
#include <linux/ptrace.h>
#include <linux/slab.h>
#include <linux/pagemap.h>
#include <linux/mempolicy.h>
#include <linux/swap.h>
#include <linux/swapops.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/sched.h>
#include <linux/ctype.h>
#include <linux/init.h>
#include <linux/time.h>
#include <linux/efi.h>
#include <linux/memdump_trigger.h>
#include <linux/memdump.h>
#include <linux/mmu_notifier.h>
#include <linux/uaccess.h>
#include <linux/elf.h>
#include <linux/io.h>

#include <asm/cacheflush.h>
#include <asm/tlbflush.h>

#include "memdump_fwrite.h"

/* set_memdripper_pte_wrprotect( void )
 * This function loops through the page table entries and
 * set the write protect bit bit of the page
 * task.
*/
void set_memdripper_pte_wrprotect(struct task_struct *task, unsigned long addr,
				struct vm_area_struct *vma)
{
	pgd_t *pgd;
	pud_t *pud;
	pmd_t *pmd;
	pte_t *pte;
	pte_t entry;

	pgd = pgd_offset(task->mm, addr);
	pud = pud_offset(pgd, addr);
	pmd = pmd_offset(pud, addr);
	if (!pmd_none(*pmd)) {
		pte = pte_offset_map(pmd, addr);
		if (!pte_none(*pte)) {
			entry = *pte;
			flush_cache_page(vma, addr, pte_pfn(*pte));
			entry = ptep_clear_flush_notify(vma, addr, pte);
			entry = pte_wrprotect(entry);
			set_pte_at(task->mm, addr, pte, entry);
		}

		pte_unmap(pte);
	}
}
EXPORT_SYMBOL(set_memdripper_pte_wrprotect);

void set_memdripper_wrprotect_all_watch_page(struct task_struct *task)
{
	unsigned long start_addr = 0;
	struct vm_area_struct *vma  = NULL;
	struct mm_struct *mm = task->mm;
	int i;

	if (!mm)
		return ;

	for (i = 0; i < task->memwatch; i++) {
		vma = find_vma(mm, task->memwatch_addr[i]);
		if (vma) {
			start_addr = task->memwatch_addr[i] & ~(PAGE_SIZE-1);
			set_memdripper_pte_wrprotect(task, start_addr, vma);
			task->watch_mark[i] = 0;
		} else {
			task->memwatch_addr[i] = 0;
			task->watch_mark[i] = 0;
		}

	}
}
EXPORT_SYMBOL(set_memdripper_wrprotect_all_watch_page);

int set_memdripper_wrprotect_single_page(struct task_struct *task,
		unsigned long addr)
{
	unsigned long start_addr;
	struct vm_area_struct *vma = NULL;
	struct mm_struct *mm = task->mm;

	if (!mm)
		return -1;

	vma = find_vma(mm, addr);
	if (vma) {
		start_addr = addr & ~(PAGE_SIZE-1) ;
		set_memdripper_pte_wrprotect(task, start_addr, vma);
	} else {
		return -1;
	}

	return 0;
}
EXPORT_SYMBOL(set_memdripper_wrprotect_single_page);
