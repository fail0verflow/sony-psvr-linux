/*
 * arch/arm/kernel/unwind_user.c
 *
 * Copyright (C) 2011 Sony corp.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 *
 * Stack unwinding support for ARM user space
 * ... included in unwind.c
 *
 * An ARM EABI version of gcc is required to generate the unwind
 * tables. For information about the structure of the unwind tables,
 * see "Exception Handling ABI for the ARM Architecture" at:
 *
 * http://infocenter.arm.com/help/topic/com.arm.doc.subset.swdev.abi/index.html
 */

#include <asm/unaligned.h>
#include <asm/uaccess.h>
#include <linux/mm.h>
#include <linux/mman.h>
#include <linux/fs.h>
#include <linux/file.h>
#include <linux/snsc_backtrace.h>
#include <linux/kallsyms.h>

static LIST_HEAD(unwind_tables_user);

static const struct unwind_idx *search_index_safe(unsigned long addr,
				       const struct unwind_idx *first,
				       const struct unwind_idx *last)
{
	pr_debug("%s(%08lx, %p, %p, fa:%lx, la:%lx)\n", __func__, addr, first, last, prel31_to_addr(&first->addr_offset), prel31_to_addr(&last->addr_offset));

	if (addr < prel31_to_addr(&first->addr_offset)) {
		pr_debug("unwind: Unknown symbol address %08lx\n", addr);
		return NULL;
	} else if (addr >= prel31_to_addr(&last->addr_offset))
		return last;

	while (first < last - 1) {
		const struct unwind_idx *mid = first + ((last - first + 1) >> 1);

		if (addr < prel31_to_addr(&mid->addr_offset))
			last = mid;
		else
			first = mid;
	}

	return first;
}

static const struct unwind_idx *unwind_find_idx_user(unsigned long addr, struct bt_elf_cache *ep)
{
	const struct unwind_idx *idx = NULL;

	pr_debug("%s(%08lx)\n", __func__, addr);

	if (core_kernel_text(addr))
		/* main unwind table */
		idx = search_index(addr, __start_unwind_idx,
				   __stop_unwind_idx - 1);
	else {
		/* module unwind tables */
		struct unwind_table *table;

		/* spin_lock_irqsave(&unwind_lock, flags); */
		/* MUST NOT lock irq here despite unwind.c, because it is needed to access file mapped address */		
		list_for_each_entry(table, &unwind_tables_user, list) {
			if (addr >= table->begin_addr &&
			    addr < table->end_addr &&
				current->pid == table->pid) {
				unsigned long ofs = addr - ep->be_base + ep->be_vaddr + ep->be_adjust_exidx;
				idx = search_index_safe(ofs, table->start,
						   table->stop - 1);
				pr_debug("check uw tab:%p idx:%p\n", table, idx);
				if (idx) {
					if ((((bt_get_user32(&idx->insn) & 0x80000000) == 0)
						|| ((bt_get_user32(&idx->insn) & 0xff000000) == 0x80000000))
						&& (bt_get_user32(&idx->insn) != 1)) {
						break;
					}else{
						/* maybe unneeded to check multiple mapped area */
						pr_debug("idx %p will fail to unwind\n", table);
					}
				}else{
					pr_debug("cant find idx in table:%p\n", table);
				}
			}
		}
	}

	pr_debug("%s: idx = %p\n", __func__, idx);
	return idx;
}

static unsigned long unwind_get_byte_user(struct unwind_ctrl_block *ctrl)
{
	unsigned long ret;

	if (ctrl->entries <= 0) {
		pr_debug("unwind: Corrupt unwind table\n");
		return 0;
	}

	ret = (bt_get_user32(ctrl->insn) >> (ctrl->byte * 8)) & 0xff;

	if (ctrl->byte == 0) {
		ctrl->insn++;
		ctrl->entries--;
		ctrl->byte = 3;
	} else
		ctrl->byte--;

	return ret;
}


/*
 * Execute the current unwind instruction.
 */
static int unwind_exec_insn_user(struct unwind_ctrl_block *ctrl)
{
	unsigned long insn = unwind_get_byte_user(ctrl);

	pr_debug("%s: insn = %08lx\n", __func__, insn);

	if ((insn & 0xc0) == 0x00)
	{
		pr_debug("insn&c0==0: SP:%lx -> SP+%lx\n", ctrl->vrs[SP], ((insn & 0x3f) << 2)+4);
		ctrl->vrs[SP] += ((insn & 0x3f) << 2) + 4;
	}
	else if ((insn & 0xc0) == 0x40)
	{
		pr_debug("insn&c0==0x40: SP:%lx -> SP-%lx\n", ctrl->vrs[SP], ((insn & 0x3f) << 2)+4);
		ctrl->vrs[SP] -= ((insn & 0x3f) << 2) + 4;
	}
	else if ((insn & 0xf0) == 0x80) {
		unsigned long mask;
		unsigned long *vsp = (unsigned long *)ctrl->vrs[SP];
		int load_sp, reg = 4;

		insn = (insn << 8) | unwind_get_byte_user(ctrl);
		mask = insn & 0x0fff;
		pr_debug("insn&f0==0x80: masked insn:%lx\n", mask);
		if (mask == 0) {
			pr_debug("unwind: 'Refuse to unwind' instruction %04lx\n",
				   insn);
			return -URC_FAILURE;
		}

		/* pop R4-R15 according to mask */
		load_sp = mask & (1 << (13 - 4));
		while (mask) {
			if (mask & 1) {
				pr_debug("vrs[%d]:%p(%lx) <= vsp:%p(%lx)\n", reg, &(ctrl->vrs[reg]), ctrl->vrs[reg], vsp, bt_get_user32(vsp));
				ctrl->vrs[reg] = bt_get_user32(vsp);
				vsp++;
			}
			mask >>= 1;
			reg++;
		}
		if (!load_sp)
		{
			pr_debug("load_sp == 0 vrs[SP](%lx) <= %p\n", ctrl->vrs[SP], vsp);
			ctrl->vrs[SP] = (unsigned long)vsp;
		}
		if (ctrl->vrs[PC] == 0)
			ctrl->vrs[PC] = ctrl->vrs[LR];
	} else if ((insn & 0xf0) == 0x90 &&
		   (insn & 0x0d) != 0x0d)
	{
		ctrl->vrs[SP] = ctrl->vrs[insn & 0x0f];
		pr_debug("insn&0xf0 == 0x90 && ((insn & 0x0d) != 0x0d) vrs[SP](%lx) <= vrs[%lx]:%p\n", ctrl->vrs[SP], insn & 0x0f, &(ctrl->vrs[insn & 0x0f]));
	}
	else if ((insn & 0xf0) == 0xa0) {
		unsigned long *vsp = (unsigned long *)ctrl->vrs[SP];
		int reg;

		/* pop R4-R[4+bbb] */
		for (reg = 4; reg <= 4 + (insn & 7); reg++)
		{
			pr_debug("vrs[%d]:%p(%lx) <= vsp:%p(%lx)\n", reg, &(ctrl->vrs[reg]), ctrl->vrs[reg], vsp, bt_get_user32(vsp));
			ctrl->vrs[reg] = bt_get_user32(vsp);
			vsp++;
		}
		if (insn & 0x8) /* pop R4-R[4+bbb] + R14 if b000 */
		{
			pr_debug("vrs[%d]:%p(%lx) <= vsp:%p(%lx)\n", 14, &(ctrl->vrs[14]), ctrl->vrs[reg], vsp, bt_get_user32(vsp));
			ctrl->vrs[LR] = bt_get_user32(vsp);
			ctrl->vrs[PC] = ctrl->vrs[LR];
			vsp++;
		}

		ctrl->vrs[SP] = (unsigned long)vsp;
	} else if (insn == 0xb0) {
		pr_debug("pop 0xb0 PC:%lx LR:%lx\n", ctrl->vrs[PC], ctrl->vrs[LR]);
		if (ctrl->vrs[PC] == 0)
			ctrl->vrs[PC] = ctrl->vrs[LR];
		/* no further processing */
		ctrl->entries = 0;
	} else if (insn == 0xb1) {
		unsigned long mask = unwind_get_byte_user(ctrl);
		unsigned long *vsp = (unsigned long *)ctrl->vrs[SP];
		int reg = 0;

		if (mask == 0 || mask & 0xf0) {
			pr_debug("unwind: Spare encoding %04lx\n",
			       (insn << 8) | mask);
			return -URC_FAILURE;
		}

		/* pop R0-R3 according to mask */
		while (mask) {
			if (mask & 1)
			{
				pr_debug("vrs[%d]:%p(%lx) <= vsp:%p(%lx)\n", reg, &(ctrl->vrs[14]), ctrl->vrs[reg], vsp, bt_get_user32(vsp));
				ctrl->vrs[reg] = bt_get_user32(vsp);
				vsp++;
			}
			mask >>= 1;
			reg++;
		}
		ctrl->vrs[SP] = (unsigned long)vsp;
	} else if (insn == 0xb2) {
		unsigned long uleb128 = unwind_conv_uleb128(unwind_get_byte_user, ctrl);

		ctrl->vrs[SP] += 0x204 + (uleb128 << 2);
	} else {
		pr_debug("unwind: Unhandled instruction %02lx\n", insn);
		return -URC_FAILURE;
	}

	pr_debug("%s: fp = %08lx sp = %08lx lr = %08lx pc = %08lx\n", __func__,
		 ctrl->vrs[FP], ctrl->vrs[SP], ctrl->vrs[LR], ctrl->vrs[PC]);

	return URC_OK;
}

/*
    d864:       df00            svc     0
    d866:       bd80            pop     {r7, pc}
    d868:       f3af 8000       nop.w
    d86c:       f3af 8000       nop.w
 */

static int arm_ip_svc_thumb_call_at(unsigned short *ip) {
	union {
		unsigned short s[4];
		unsigned long  l[2];
	} t;
	int i;
	for (i = 0; i < 2; i++) {
		t.l[i] = bt_get_user32((unsigned long *)ip + i);
	}
	return ((t.s[0] == 0xdf00) && (t.s[1] == 0xbd80) && (t.s[2] == 0xf3af) && (t.s[3] == 0x8000));
}

static int arm_ip_svc_call_at(unsigned long *ip) {
	return (bt_get_user32(ip) == 0xef000000);
}

static int arm_ip_svc_bxcc_lr_case(unsigned long *ip){
	unsigned long *cip;
	for(cip = ip + 1; cip < ip + 6; cip++) {
		/*
		 *    bb44:       e8bd4080        pop     {r7, lr}
		 *    bb48:       e3700a01        cmn     r0, #4096       ; 0x1000
		 *    bb4c:       312fff1e        bxcc    lr
		 *
		 */
		if(bt_get_user32(cip)  == 0x312fff1e) { /* bxcc lr */
			/*     bb44:       e8bd4080        pop     {r7, lr} */
			return 1;
		}else if((bt_get_user32(cip) & 0xff0f4000) == 0xe80d4000) { /* ldmxx sp {xx, lr} */
			return 0;
		}
	}
	return 0;
}


extern unsigned long sym_start;
#ifdef CONFIG_ARM_UNWIND_MANUAL
extern int unwind_instr(struct unwind_ctrl_block *ctrl, unsigned long addr);

int user_manual_unwind(struct stackframe *frame, struct unwind_ctrl_block *pctrl)
{
	int ret;
	unsigned long addr = frame->pc;
	unsigned usr_sym_start = sym_start;

	while (addr >= usr_sym_start) {
		ret = unwind_instr(pctrl, usr_sym_start);
		if (ret < 0)
			return -URC_FAILURE;
		if (ret > URC_OK)
			break;

		usr_sym_start += 4;
	}

	if (!(pctrl->vrs[PC] == frame->pc)) {
		frame->fp = pctrl->vrs[FP];
		frame->sp = pctrl->vrs[SP];
		frame->lr = pctrl->vrs[LR];

		/* Condition when no stack is updated */
		if(pctrl->vrs[PC] == 0)
			pctrl->vrs[PC] = pctrl->vrs[LR];

		/* Condition for inifinite loop */
		if(pctrl->vrs[PC] == frame->pc)
			return -URC_FAILURE;

		frame->pc = pctrl->vrs[PC];
		return URC_OK;
	}
	return -URC_FAILURE;
}
#else
int user_manual_unwind(struct stackframe *frame, struct unwind_ctrl_block *pctrl)
{
	return -URC_FAILURE;
}
#endif

/*
 * Unwind a single frame starting with *sp for the symbol at *pc. It
 * updates the *pc and *sp with the new values.
 */
int unwind_frame_user(struct stackframe *frame, struct bt_elf_cache *ep, struct unwind_ctrl_block *pctrl)
{
	unsigned long high, low;
	const struct unwind_idx *idx = NULL;
	struct unwind_ctrl_block saved_ctrl;
	int i;
	unsigned long idx_start_addr;
	static int first_time = 1;

	/* only go to a higher address on the stack */
	low = frame->sp;
	high = ALIGN(low, THREAD_SIZE) + THREAD_SIZE;

	pr_debug("%s(pc = %lx lr = %08lx sp = %08lx) iaddr:%p \n", __func__,
		 frame->pc, frame->lr, frame->sp, pctrl->insn);

	if(0) {
	if(arm_ip_svc_call_at((unsigned long *)(frame->pc - 4))) { /* svc call */
		if(arm_ip_svc_bxcc_lr_case((unsigned long *)frame->pc)) {
			/* if scan pc + 1, 2, 3 ,4 contains bxcc lr, then this is normal case */
			pr_debug("unwind: found syscall restore r7 from ip(r12):%lx pc <= lr (%lx)\n", pctrl->vrs[12], pctrl->vrs[LR]);
			pctrl->vrs[7] = pctrl->vrs[12];
			pctrl->vrs[PC] = pctrl->vrs[LR];
			frame->pc = frame->lr;
			return URC_OK;
		}else{ /* if didnt, then you can just pop the stack as in exidx */
			pr_debug("unwind: found syscall with error check pattern\n");
		}
	}
	if(arm_ip_svc_thumb_call_at((unsigned short*)(frame->pc - 2))) {
		unsigned long *vsp = (unsigned long *)pctrl->vrs[SP];
		pr_debug("svc thumb: vsp[0]:%lx vsp[1]:%lx\n", bt_get_user32(&vsp[0]), bt_get_user32(&vsp[1]));
		pctrl->vrs[7] = bt_get_user32(&vsp[0]);
		pctrl->vrs[LR] = bt_get_user32(&vsp[1]);
		frame->pc = frame->lr = pctrl->vrs[PC] = pctrl->vrs[LR];
		return URC_OK;
	}
	}
	idx = unwind_find_idx_user(frame->pc, ep);
	if (!idx) {
		pr_debug("unwind: Index not found %08lx\n", frame->pc);
		return -URC_FAILURE;
	}

	pr_debug("unwind: found idx user:%p idx->insn:%lx\n",idx, idx->insn);
	pctrl->vrs[FP] = frame->fp;
	pctrl->vrs[SP] = frame->sp;
	pctrl->vrs[LR] = frame->lr;
	pctrl->vrs[PC] = 0;

	/* limit the manual unwinding for first PC only.
	   LR register value is used as reference point
	   It will be valid only for first stack-frame only */
	if (first_time && (sym_start != 0)) {
		first_time = 0;
		idx_start_addr = prel31_to_addr(&idx->addr_offset);

		if(idx_start_addr != sym_start) {
			return user_manual_unwind(frame, pctrl);
		}
	}
	first_time = 0;

	if (bt_get_user32(&idx->insn) == 1)
	{
		/* can't unwind */
		pr_debug("unwind: %s:%d cant unwind this function\n", __FUNCTION__, __LINE__);
		return -URC_FAILURE;
	}
	else if ((bt_get_user32(&idx->insn) & 0x80000000) == 0)
	{
		/* prel31 to the unwind table */
		unsigned long prelfix = (unsigned long)prel31_to_addr(&idx->insn);
		/* this is extab case extab is mapped with be_base */
		pctrl->insn = (unsigned long *)(prelfix - ep->be_adjust_exidx + ep->be_base - ep->be_vaddr);
		pr_debug("pctrl->insn idx->insn:%p prel31:%p prel31:%lx - adjust:%lx + base:%lx\n", pctrl->insn, &idx->insn, prelfix, ep->be_adjust_exidx, ep->be_base - ep->be_vaddr);
	}
	else if ((bt_get_user32(&idx->insn) & 0xff000000) == 0x80000000)
		/* only personality routine 0 supported in the index */
		pctrl->insn = &idx->insn;
	else {
		pr_debug("unwind: Unsupported personality routine %08lx in the index at %p\n",
			   idx->insn, idx);
		return -URC_FAILURE;
	}

	/* check the personality routine */
	do {
		pr_debug("personality: pctrl->insn:%p *pctrl->insn:%lx\n", pctrl->insn, *pctrl->insn);
		if ((bt_get_user32(pctrl->insn) & 0xff000000) == 0x80000000) {
			pctrl->byte = 2;
			pctrl->entries = 1;
			break;
		} else if ((bt_get_user32(pctrl->insn) & 0xff000000) == 0x81000000) {
			pctrl->byte = 1;
			pctrl->entries = 1 + ((bt_get_user32(pctrl->insn) & 0x00ff0000) >> 16);
			break;
		}else if ((bt_get_user32(&idx->insn) & 0x80000000) == 0) {
			/* prel31 to the unwind table */
			unsigned long prelfix = (unsigned long)prel31_to_addr(pctrl->insn);
			prelfix -= (ep->be_base - ep->be_vaddr);
			pr_debug("personality routine offset:%lx insn:%p *insn:%lx\n", prelfix, pctrl->insn, *(pctrl->insn));
			pctrl->insn++;
			pctrl->byte = 2;
			pctrl->entries = 1 + ((bt_get_user32(pctrl->insn) & 0xff000000) >> 24);
			break;
		} else {
			pr_debug("unwind: Unsupported personality routine %08lx at %p\n",
					bt_get_user32(pctrl->insn), pctrl->insn);
			return -URC_FAILURE;
		}
	}while(1);

	for (i = 0; i < 0xf; i++) {
		saved_ctrl.vrs[i] = pctrl->vrs[i];
	}
	while (pctrl->entries > 0) {
		int urc = unwind_exec_insn_user(pctrl);
		if (urc < 0) {
			pr_debug("unwind: %s:%d exec_insn failed\n", __FUNCTION__, __LINE__);
			return urc;
		}
		if (pctrl->vrs[SP] < low || pctrl->vrs[SP] >= high) {
			pr_debug("unwind: %s:%d sp overflow sp:%lx low:%lx high:%lx\n", __FUNCTION__, __LINE__, pctrl->vrs[SP], low, high);
			pctrl->vrs[PC] = 0;
			break;
		}
	}

	if (pctrl->vrs[PC] == 0) {
		pr_debug("unwind: %s:%d unwind failed... restore and use lr\n", __FUNCTION__, __LINE__);
		for (i = 0; i < 0xf; i++) {
			pctrl->vrs[i] = saved_ctrl.vrs[i];
		}
		pctrl->vrs[PC] = pctrl->vrs[LR];
	}

	/* check for infinite loop */
	if (frame->pc == pctrl->vrs[PC])
		return -URC_FAILURE;

	frame->fp = pctrl->vrs[FP];
	frame->sp = pctrl->vrs[SP];
	frame->lr = pctrl->vrs[LR];
	frame->pc = pctrl->vrs[PC];

	return URC_OK;
}


struct unwind_table *unwind_table_add_user(unsigned long start, unsigned long size,
				      unsigned long text_addr,
				      unsigned long text_size)
{
	unsigned long flags;
	struct unwind_table *tab = NULL;
	struct unwind_table *table;
	int newtab = 0;
	struct unwind_table *pnewtab = kmalloc(sizeof(*tab), GFP_KERNEL);
	pr_debug("%s(%08lx, %08lx, %08lx, %08lx)\n", __func__, start, size,
		 text_addr, text_size);

	spin_lock_irqsave(&unwind_lock, flags);
	list_for_each_entry(table, &unwind_tables_user, list) {
		if(table->begin_addr == text_addr
			&& table->end_addr == (text_addr + text_size)
			&& table->pid == current->pid) {
			tab = table;
			break;
		}
	}
	if (!tab) {
		tab = pnewtab;
		newtab = 1;
	}

	tab->start = (struct unwind_idx *)start;
	tab->stop = (struct unwind_idx *)(start + size);
	tab->begin_addr = text_addr;
	tab->end_addr = text_addr + text_size;
	tab->pid = current->pid;

	if (newtab) {
		list_add_tail(&tab->list, &unwind_tables_user);
	}
	spin_unlock_irqrestore(&unwind_lock, flags);

	if (!newtab) {
		kfree(pnewtab);
	}

	return tab;
}

