/*
 * arch/arm/kernel/unwind.c
 *
 * Copyright (C) 2008 ARM Limited
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
 * Stack unwinding support for ARM
 *
 * An ARM EABI version of gcc is required to generate the unwind
 * tables. For information about the structure of the unwind tables,
 * see "Exception Handling ABI for the ARM Architecture" at:
 *
 * http://infocenter.arm.com/help/topic/com.arm.doc.subset.swdev.abi/index.html
 */

#ifndef __CHECKER__
#if !defined (__ARM_EABI__)
#warning Your compiler does not have EABI support.
#warning    ARM unwind is known to compile only with EABI compilers.
#warning    Change compiler or disable ARM_UNWIND option.
#elif (__GNUC__ == 4 && __GNUC_MINOR__ <= 2)
#warning Your compiler is too buggy; it is known to not compile ARM unwind support.
#warning    Change compiler or disable ARM_UNWIND option.
#endif
#endif /* __CHECKER__ */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/export.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/list.h>

#include <asm/stacktrace.h>
#include <asm/traps.h>
#include <asm/unwind.h>
#include <asm/unwind_internal.h>

/* Dummy functions to avoid linker complaints */
void __aeabi_unwind_cpp_pr0(void)
{
};
EXPORT_SYMBOL(__aeabi_unwind_cpp_pr0);

void __aeabi_unwind_cpp_pr1(void)
{
};
EXPORT_SYMBOL(__aeabi_unwind_cpp_pr1);

void __aeabi_unwind_cpp_pr2(void)
{
};
EXPORT_SYMBOL(__aeabi_unwind_cpp_pr2);

enum regs {
#ifdef CONFIG_THUMB2_KERNEL
	FP = 7,
#else
	FP = 11,
#endif
	SP = 13,
	LR = 14,
	PC = 15
};

extern const struct unwind_idx __start_unwind_idx[];
extern const struct unwind_idx __stop_unwind_idx[];

static DEFINE_SPINLOCK(unwind_lock);
static LIST_HEAD(unwind_tables);

/* Convert a prel31 symbol to an absolute address */
#ifdef CONFIG_ARM_UNWIND_USER
#define prel31_to_addr(ptr)										\
({																\
	/* sign-extend to 32 bits */								\
	long offset = (((long)bt_get_user32(ptr)) << 1) >> 1;		\
	(unsigned long)(ptr) + offset;								\
})
#else
#define prel31_to_addr(ptr)				\
({							\
	/* sign-extend to 32 bits */			\
	long offset = (((long)*(ptr)) << 1) >> 1;	\
	(unsigned long)(ptr) + offset;			\
 })
#endif

/*
 * Binary search in the unwind index. The entries entries are
 * guaranteed to be sorted in ascending order by the linker.
 */
static const struct unwind_idx *search_index(unsigned long addr,
				       const struct unwind_idx *first,
				       const struct unwind_idx *last)
{
	pr_debug("%s(%08lx, %p, %p)\n", __func__, addr, first, last);

	if (addr < prel31_to_addr(&first->addr_offset)) {
		pr_warning("unwind: Unknown symbol address %08lx\n", addr);
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

static const struct unwind_idx *unwind_find_idx(unsigned long addr)
{
	const struct unwind_idx *idx = NULL;
	unsigned long flags;

	pr_debug("%s(%08lx)\n", __func__, addr);

	if (core_kernel_text(addr))
		/* main unwind table */
		idx = search_index(addr, __start_unwind_idx,
				   __stop_unwind_idx - 1);
	else {
		/* module unwind tables */
		struct unwind_table *table;

		spin_lock_irqsave(&unwind_lock, flags);
		list_for_each_entry(table, &unwind_tables, list) {
			if (addr >= table->begin_addr &&
			    addr < table->end_addr) {
				idx = search_index(addr, table->start,
						   table->stop - 1);
				break;
			}
		}
		spin_unlock_irqrestore(&unwind_lock, flags);
	}

	pr_debug("%s: idx = %p\n", __func__, idx);
	return idx;
}

static unsigned long unwind_get_byte(struct unwind_ctrl_block *ctrl)
{
	unsigned long ret;

	if (ctrl->entries <= 0) {
		pr_warning("unwind: Corrupt unwind table\n");
		return 0;
	}

	ret = (*ctrl->insn >> (ctrl->byte * 8)) & 0xff;

	if (ctrl->byte == 0) {
		ctrl->insn++;
		ctrl->entries--;
		ctrl->byte = 3;
	} else
		ctrl->byte--;

	return ret;
}

typedef unsigned long (*unwind_get_byte_func_t )(struct unwind_ctrl_block *);
static unsigned int unwind_conv_uleb128(unwind_get_byte_func_t get_b, struct unwind_ctrl_block *ctrl)
{
    unsigned int result = 0;
    unsigned int shift = 0;
    int count=5; /* theoretical max */
    while(count--) {
        unsigned char byte = get_b(ctrl);
        result |= ((byte & 0x7f) << shift);
        if ((byte & 0x80) == 0)
            break;
        shift += 7;
    }
    return result;
}

/*
 * Execute the current unwind instruction.
 */
static int unwind_exec_insn(struct unwind_ctrl_block *ctrl)
{
	unsigned long insn = unwind_get_byte(ctrl);

	pr_debug("%s: insn = %08lx\n", __func__, insn);

	if ((insn & 0xc0) == 0x00)
		ctrl->vrs[SP] += ((insn & 0x3f) << 2) + 4;
	else if ((insn & 0xc0) == 0x40)
		ctrl->vrs[SP] -= ((insn & 0x3f) << 2) + 4;
	else if ((insn & 0xf0) == 0x80) {
		unsigned long mask;
		unsigned long *vsp = (unsigned long *)ctrl->vrs[SP];
		int load_sp, reg = 4;

		insn = (insn << 8) | unwind_get_byte(ctrl);
		mask = insn & 0x0fff;
		if (mask == 0) {
			pr_warning("unwind: 'Refuse to unwind' instruction %04lx\n",
				   insn);
			return -URC_FAILURE;
		}

		/* pop R4-R15 according to mask */
		load_sp = mask & (1 << (13 - 4));
		while (mask) {
			if (mask & 1)
				ctrl->vrs[reg] = *vsp++;
			mask >>= 1;
			reg++;
		}
		if (!load_sp)
			ctrl->vrs[SP] = (unsigned long)vsp;
	} else if ((insn & 0xf0) == 0x90 &&
		   (insn & 0x0d) != 0x0d)
		ctrl->vrs[SP] = ctrl->vrs[insn & 0x0f];
	else if ((insn & 0xf0) == 0xa0) {
		unsigned long *vsp = (unsigned long *)ctrl->vrs[SP];
		int reg;

		/* pop R4-R[4+bbb] */
		for (reg = 4; reg <= 4 + (insn & 7); reg++)
			ctrl->vrs[reg] = *vsp++;
		if (insn & 0x8) /* pop R4-R[4+bbb] + R14 if b000 */
			ctrl->vrs[14] = *vsp++;
		ctrl->vrs[SP] = (unsigned long)vsp;
	} else if (insn == 0xb0) {
		if (ctrl->vrs[PC] == 0)
			ctrl->vrs[PC] = ctrl->vrs[LR];
		/* no further processing */
		ctrl->entries = 0;
	} else if (insn == 0xb1) {
		unsigned long mask = unwind_get_byte(ctrl);
		unsigned long *vsp = (unsigned long *)ctrl->vrs[SP];
		int reg = 0;

		if (mask == 0 || mask & 0xf0) {
			pr_warning("unwind: Spare encoding %04lx\n",
			       (insn << 8) | mask);
			return -URC_FAILURE;
		}

		/* pop R0-R3 according to mask */
		while (mask) {
			if (mask & 1)
				ctrl->vrs[reg] = *vsp++;
			mask >>= 1;
			reg++;
		}
		ctrl->vrs[SP] = (unsigned long)vsp;
	} else if (insn == 0xb2) {
		unsigned long uleb128 = unwind_conv_uleb128(unwind_get_byte, ctrl);

		ctrl->vrs[SP] += 0x204 + (uleb128 << 2);
	} else {
		pr_warning("unwind: Unhandled instruction %02lx\n", insn);
		return -URC_FAILURE;
	}

	pr_debug("%s: fp = %08lx sp = %08lx lr = %08lx pc = %08lx\n", __func__,
		 ctrl->vrs[FP], ctrl->vrs[SP], ctrl->vrs[LR], ctrl->vrs[PC]);

	return URC_OK;
}

#ifdef CONFIG_ARM_UNWIND_MANUAL
extern const unsigned long kallsyms_addresses[] __attribute__((weak));
extern const unsigned long kallsyms_num_syms __attribute__((weak, section(".rodata")));

#define INSTR_OFFSET 0x0e000000
#define INSTR_SHIFT 25

#define OPCODE_OFFSET 0x01e00000
#define OPCODE_SHIFT 21

#define SUB_CMD 0x0010
#define ADD_CMD 0x0100

/* Data processing instruction offsets and constants */
#define DP_DEST_REG_OFFSET 0x0000f000
#define DP_DEST_REG_SP_CONST 0x0000d000
#define DP_SRC_REG_OFFSET 0x000f0000
#define DP_SRC_REG_SP_CONST 0x000d0000
#define DP_SRC_REG_FP_CONST 0x000b0000

/*Data processing shifter operand*/
#define DP_SHIFT_OPERAND 0x00000fff

/* Load Store instruction offset and constants */
#define LDST_CONST 0x00100000
#define LDST_DEST_REG_OFFSET 0x000f0000
#define LDST_DEST_SP_CONST 0x000d0000
#define LDST_PBIT_CONST 0x01000000
#define LDST_WBIT_CONST 0x00200000
#define LDST_UBIT_CONST 0x00800000


/* Load/Store immediate offset instr, destination register offset*/
#define LDST_IM_DEST_OFFSET 0x0000f000

/* Load/Store immediate offset instr, destination register is LR*/
#define LDST_IM_DEST_LR_CONST 0x0000e000
#define LDST_IM_SHIFT_OPERAND 0x00000fff

/* Load/Store Multiple shifter operand */
#define LDST_ML_SHIFT_OPERAND 0x0000ffff

/*
* Decode the data processing instructions with immediate shift.
* If data processing operation involves stack pointer.
*/

int decode_1(unsigned long instruction, struct unwind_ctrl_block *ctrl)
{
	unsigned long  offset, opcode, source_reg;

	opcode = (instruction & OPCODE_OFFSET) >> OPCODE_SHIFT;
	switch (opcode) {
		case SUB_CMD: /* SUB command */
		case ADD_CMD: /* ADD command */
			if ((instruction & DP_DEST_REG_OFFSET) == DP_DEST_REG_SP_CONST) { /* stack is updated */
				if ((instruction & DP_SRC_REG_OFFSET) == DP_SRC_REG_SP_CONST) /* source register is sp */
					source_reg = ctrl->vrs[SP];
				else if ((instruction & DP_SRC_REG_OFFSET) == DP_SRC_REG_FP_CONST) /* source register is fp */
					source_reg = ctrl->vrs[FP];
				else
					return -URC_FAILURE;

				offset = instruction & DP_SHIFT_OPERAND;
				if (opcode == SUB_CMD)
					ctrl->vrs[SP] = source_reg + offset;
				else
					ctrl->vrs[SP] = source_reg - offset;
			}
		break;
	}
	return URC_OK;
}

/*
* Check if stack value matches with LR register.
*/
int check_lr(unsigned long addr, struct unwind_ctrl_block *ctrl)
{
	unsigned int ret = URC_OK;
	unsigned long insn;
	if(access_ok(VERIFY_READ, addr, sizeof(insn)) && (! __get_user(insn, (unsigned long *)addr)))
		if (ctrl->vrs[LR] == insn)
			ret = 1;
	return ret;
}

/*
* Decode the Load/Store operation.
* If operation involves stack pointer.
*/
int decode_2(unsigned long instruction, struct unwind_ctrl_block *ctrl)
{
	unsigned long offset;
	unsigned long addr;
	unsigned int ret = URC_OK;

	/* LR stored in STR operation, store LR for next unwind */
	if ((instruction & LDST_IM_DEST_OFFSET) == LDST_IM_DEST_LR_CONST)
		ctrl->vrs[PC] = ctrl->vrs[LR];

	offset = instruction & LDST_IM_SHIFT_OPERAND;
	/* check if U bit to decide stack increment/decrement */
	if (instruction & LDST_UBIT_CONST) {
		addr = ctrl->vrs[SP] -= offset;
		addr += 4;
	}
	else {
		addr = ctrl->vrs[SP] += offset;
		addr -= 4;
	}
	ret = check_lr(addr, ctrl);
	return ret;
}

/*
* Decode the Load/Store Multiple operation.
* If operation involves stack pointer.
*/
int decode_4(unsigned long instruction, struct unwind_ctrl_block *ctrl)
{
	unsigned int i, ret = URC_OK;
	unsigned long offset, shift;
	unsigned long addr = ctrl->vrs[SP];

	offset = instruction & LDST_ML_SHIFT_OPERAND;
	for (i=0; i<16; i++) {
		shift = 1 << i;
		if (offset & shift) {
			if (i == 14)
				ctrl->vrs[PC] = ctrl->vrs[LR];

			/* check if U bit to decide stack increment/decrement */
			if (instruction & LDST_UBIT_CONST)
				addr = ctrl->vrs[SP] -= 4;
			else
				addr = ctrl->vrs[SP] += 4;
		}
	}
	if (instruction & LDST_UBIT_CONST)
		addr += 4;
	else
		addr -= 4;

	ret = check_lr(addr, ctrl);
	return ret;
}

int decode_2_4(unsigned int cmd, unsigned long instruction, struct unwind_ctrl_block *ctrl)
{
	unsigned int ret = URC_OK;
	if(!(instruction & LDST_CONST)) /* find if load=1/store=0 instuction */
		if((instruction & LDST_DEST_REG_OFFSET) == LDST_DEST_SP_CONST)  /* PUSH operation */
			if((instruction & LDST_PBIT_CONST) && (instruction & LDST_WBIT_CONST)) { /* P and W bit are set, update sp */
				if (cmd == 2)
					ret = decode_2(instruction, ctrl);
				else
					ret = decode_4(instruction, ctrl);
			}
	return ret;
}

int unwind_instr(struct unwind_ctrl_block *ctrl, unsigned long addr)
{
	unsigned int cmd;
	int ret = URC_OK;
	unsigned long instruction;

	if(!access_ok(VERIFY_READ, addr, sizeof(instruction)) || __get_user(instruction, (unsigned long *)addr))
		return ret;

	cmd = (instruction & INSTR_OFFSET) >> INSTR_SHIFT;
	pr_debug("addr:%lx  instr:%lx  cmd=%x sp:%lx, lr:%lx, pc:%lx \n", addr, instruction, cmd, ctrl->vrs[SP], ctrl->vrs[LR], ctrl->vrs[PC]);

	switch (cmd) {
		case 1: /* data processing immediate shift[2] */
			ret = decode_1(instruction, ctrl);
		break;

		case 2: /* Load/Store immediate offset */
		case 4: /* Load/Store multiple */
			ret = decode_2_4(cmd, instruction, ctrl);
		break;
	}
	return ret;
}

/*
* Manually unwind the stack frames.
* For assembly function which do not follow/add unwind assembly directives.
*/
int manual_unwind_frame(struct stackframe *frame, struct unwind_ctrl_block *ctrl)
{
	unsigned long symbol_start = 0;
	unsigned long low, high, mid;
	unsigned long addr = frame->pc;
	int ret ;

	ctrl->vrs[PC] = frame->pc;
	low = 0;
	high = kallsyms_num_syms;

	/* get the function top address for pc */
	while (high - low > 1) {
		mid = low + (high - low) / 2;
		if (kallsyms_addresses[mid] <= addr)
			low = mid;
		else
			high = mid;
	}

	while (low && kallsyms_addresses[low-1] == kallsyms_addresses[low])
		--low;

	symbol_start = kallsyms_addresses[low];

	/* read the instructions using pc and function top address */
	while (addr>=symbol_start) {
		ret = unwind_instr(ctrl, symbol_start);
		if (ret < 0)
			return -URC_FAILURE;
		if (ret > URC_OK)
			break;
		symbol_start+=4;

	}

	if (!(ctrl->vrs[PC] == frame->pc)) {
		frame->fp = ctrl->vrs[FP];
		frame->sp = ctrl->vrs[SP];
		frame->lr = ctrl->vrs[LR];

		/* Condition when no stack is updated */
		if(ctrl->vrs[PC] == 0)
			ctrl->vrs[PC] = ctrl->vrs[LR];

		/* Condition for inifinite loop */
		if(ctrl->vrs[PC] == frame->pc)
			return -URC_FAILURE;
		frame->pc = ctrl->vrs[PC];
		return URC_OK;
	}
	return -URC_FAILURE;
}
#else
int manual_unwind_frame(struct stackframe *frame, struct unwind_ctrl_block *ctrl)
{
	return -URC_FAILURE;
}
#endif

/*
 * Unwind a single frame starting with *sp for the symbol at *pc. It
 * updates the *pc and *sp with the new values.
 */
int unwind_frame(struct stackframe *frame)
{
	unsigned long high, low;
	const struct unwind_idx *idx;
	struct unwind_ctrl_block ctrl;
	static int first_time = 1;

	/* only go to a higher address on the stack */
	low = frame->sp;
	high = ALIGN(low, THREAD_SIZE);

	pr_debug("%s(pc = %08lx lr = %08lx sp = %08lx)\n", __func__,
		 frame->pc, frame->lr, frame->sp);

	if (!kernel_text_address(frame->pc))
		return -URC_FAILURE;

	idx = unwind_find_idx(frame->pc);
	if (!idx) {
		pr_warning("unwind: Index not found %08lx\n", frame->pc);
		return -URC_FAILURE;
	}

	ctrl.vrs[FP] = frame->fp;
	ctrl.vrs[SP] = frame->sp;
	ctrl.vrs[LR] = frame->lr;
	ctrl.vrs[PC] = 0;

	if (idx->insn == 1) {
		/* limiting manual unwinding for topmost stack-frame only */
		if (first_time){
			first_time = 0;
			return manual_unwind_frame(frame, &ctrl);
		}
		else
			return -URC_FAILURE;
	}
	else if ((idx->insn & 0x80000000) == 0)
		/* prel31 to the unwind table */
		ctrl.insn = (unsigned long *)prel31_to_addr(&idx->insn);
	else if ((idx->insn & 0xff000000) == 0x80000000)
		/* only personality routine 0 supported in the index */
		ctrl.insn = &idx->insn;
	else {
		pr_warning("unwind: Unsupported personality routine %08lx in the index at %p\n",
			   idx->insn, idx);
		return -URC_FAILURE;
	}

	first_time = 0;

	/* check the personality routine */
	if ((*ctrl.insn & 0xff000000) == 0x80000000) {
		ctrl.byte = 2;
		ctrl.entries = 1;
	} else if ((*ctrl.insn & 0xff000000) == 0x81000000) {
		ctrl.byte = 1;
		ctrl.entries = 1 + ((*ctrl.insn & 0x00ff0000) >> 16);
	} else {
		pr_warning("unwind: Unsupported personality routine %08lx at %p\n",
			   *ctrl.insn, ctrl.insn);
		return -URC_FAILURE;
	}

	while (ctrl.entries > 0) {
		int urc = unwind_exec_insn(&ctrl);
		if (urc < 0)
			return urc;
		if (ctrl.vrs[SP] < low || ctrl.vrs[SP] >= high)
			return -URC_FAILURE;
	}

	if (ctrl.vrs[PC] == 0)
		ctrl.vrs[PC] = ctrl.vrs[LR];

	/* check for infinite loop */
	if (frame->pc == ctrl.vrs[PC])
		return -URC_FAILURE;

	frame->fp = ctrl.vrs[FP];
	frame->sp = ctrl.vrs[SP];
	frame->lr = ctrl.vrs[LR];
	frame->pc = ctrl.vrs[PC];

	return URC_OK;
}

void unwind_backtrace(struct pt_regs *regs, struct task_struct *tsk, bt_print_func_t prfunc)
{
	struct stackframe frame;
	register unsigned long current_sp asm ("sp");

	pr_debug("%s(regs = %p tsk = %p)\n", __func__, regs, tsk);

	if (!tsk)
		tsk = current;

	if (regs) {
		frame.fp = regs->ARM_fp;
		frame.sp = regs->ARM_sp;
		frame.lr = regs->ARM_lr;
		/* PC might be corrupted, use LR in that case. */
		frame.pc = kernel_text_address(regs->ARM_pc)
			 ? regs->ARM_pc : regs->ARM_lr;
	} else if (tsk == current) {
		frame.fp = (unsigned long)__builtin_frame_address(0);
		frame.sp = current_sp;
		frame.lr = (unsigned long)__builtin_return_address(0);
		frame.pc = (unsigned long)unwind_backtrace;
	} else {
		/* task blocked in __switch_to */
		frame.fp = thread_saved_fp(tsk);
		frame.sp = thread_saved_sp(tsk);
		/*
		 * The function calling __switch_to cannot be a leaf function
		 * so LR is recovered from the stack.
		 */
		frame.lr = 0;
		frame.pc = thread_saved_pc(tsk);
	}

	while (1) {
		int urc;
		unsigned long where = frame.pc;

		urc = unwind_frame(&frame);
		if (urc < 0)
			break;
		dump_backtrace_entry(where, frame.pc, frame.sp - 4, prfunc);
	}
}

struct unwind_table *unwind_table_add(unsigned long start, unsigned long size,
				      unsigned long text_addr,
				      unsigned long text_size)
{
	unsigned long flags;
	struct unwind_table *tab = kmalloc(sizeof(*tab), GFP_KERNEL);

	pr_debug("%s(%08lx, %08lx, %08lx, %08lx)\n", __func__, start, size,
		 text_addr, text_size);

	if (!tab)
		return tab;

	tab->start = (struct unwind_idx *)start;
	tab->stop = (struct unwind_idx *)(start + size);
	tab->begin_addr = text_addr;
	tab->end_addr = text_addr + text_size;

	spin_lock_irqsave(&unwind_lock, flags);
	list_add_tail(&tab->list, &unwind_tables);
	spin_unlock_irqrestore(&unwind_lock, flags);

	return tab;
}

void unwind_table_del(struct unwind_table *tab)
{
	unsigned long flags;

	if (!tab)
		return;

	spin_lock_irqsave(&unwind_lock, flags);
	list_del(&tab->list);
	spin_unlock_irqrestore(&unwind_lock, flags);

	kfree(tab);
}

#ifdef CONFIG_ARM_UNWIND_USER
#include "unwind_user.c"
#endif
