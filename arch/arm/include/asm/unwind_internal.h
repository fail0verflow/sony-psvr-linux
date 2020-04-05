/*
 * arch/arm/include/asm/unwind_internal.h
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
 */

#ifndef __ASM_UNWIND_INTERNAL_H
#define __ASM_UNWIND_INTERNAL_H

#include <asm/uaccess.h>

struct unwind_ctrl_block {
	unsigned long vrs[16];		/* virtual register set */
	unsigned long *insn;		/* pointer to the current instructions word */
	int entries;			/* number of entries left to interpret */
	int byte;			/* current byte number in the instructions word */
};

extern int unwind_frame(struct stackframe *frame);

static inline unsigned long bt_get_user32(const unsigned long *addr)
{
	unsigned long __user data;
	mm_segment_t oldfs = get_fs();
	register unsigned int current_pc asm ("pc");

	set_fs(KERNEL_DS);
	if (copy_from_user(&data, addr, sizeof(unsigned long))) {
		/* data is set to zero */
		printk("bt_get_user32: illegal address %p pc:%x ra:%p\n", addr, current_pc, __builtin_return_address(0));
	}
	set_fs(oldfs);
	return data;
}

static inline void bt_put_user32(unsigned long *to, unsigned long *from)
{
	mm_segment_t oldfs = get_fs();

	set_fs(KERNEL_DS);
	if (copy_to_user(to, from, sizeof(unsigned long))) {
		printk("bt_put_user32: illegal address %p %p\n",to, from);
	}
	set_fs(oldfs);
}

#endif  /* __ASM_UNWIND_INTERNAL_H */
