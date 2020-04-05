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
#include <linux/mm.h>
#include <linux/uaccess.h>

/*
 * This function walks through the page tables to convert a userland
 * virtual address to a page table entry (PTE)
 */
unsigned long tz_user_virt_to_pte(struct mm_struct *mm, unsigned long address)
{
	uint32_t tmp;
	pgd_t *pgd;
	pud_t *pud;
	pmd_t *pmd;
	pte_t *ptep, pte;
#ifdef pte_offset_map_lock
	spinlock_t *lock;
#endif

	/* va in user space might not be mapped yet, so do a dummy read here
	 * to trigger a page fault and tell kernel to create the revalant
	 * pte entry in the page table for it. It is possible to fail if
	 * the given va is not a valid one
	 */
	if (get_user(tmp, (uint32_t __user *)address))
		return 0;

	pgd = pgd_offset(mm, address);
	if (!pgd_present(*pgd))
		return 0;

	pud = pud_offset(pgd, address);
	if (!pud_present(*pud))
		return 0;

	pmd = pmd_offset(pud, address);
	if (!pmd_present(*pmd))
		return 0;

#ifdef pte_offset_map_lock
	ptep = pte_offset_map_lock(mm, pmd, address, &lock);
#else
	ptep = pte_offset_map(pmd, address);
#endif
	if (!ptep)
		return 0;
	pte = *ptep;
#ifdef pte_offset_map_lock
	pte_unmap_unlock(pte, lock);
#endif

	if (pte_present(pte))
		return pte_val(pte) & PHYS_MASK;

	return 0;
}

/*
 * This function walks through the page tables to convert a userland
 * virtual address to physical address
 */
unsigned long tz_user_virt_to_phys(struct mm_struct *mm, unsigned long address)
{
	unsigned long pte = tz_user_virt_to_pte(mm, address);
	return pte & PAGE_MASK;
}
