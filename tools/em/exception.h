/*
 *  exception.h - common header for Exception Monitor
 *
 */

/* With non GPL files, use following license */
/*
 * Copyright 2007,2008 Sony Corporation.
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
 *  Copyright 2007,2008 Sony Corporation.
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
#ifndef __EXCEPTION_H__
#define __EXCEPTION_H__

#include <linux/elf.h>

struct _elf_info {
	char *filename;
	struct file *file;
	mm_segment_t fs;
	Elf32_Shdr sh_dynsym;
	Elf32_Shdr sh_dynstr;
	Elf32_Shdr sh_strtab;
	Elf32_Shdr sh_symtab;
	unsigned long addr_offset;
	unsigned long addr_end;
	unsigned long vaddr;
	int strip;
};

#ifndef CONFIG_SNSC_EM_MAX_SHARED_LIBRARIES
#define LIB_MAX 20
#else
#define LIB_MAX CONFIG_SNSC_EM_MAX_SHARED_LIBRARIES
#endif

#ifndef CONFIG_SNSC_EM_CALLSTACK_DISABLE_EMLEGACY
#define EMLEGACY_CALLSTACK 1
#endif

#ifdef EMLEGACY_CALLSTACK
#define CALLSTACK_STR_SZ 256
struct callstack {
	unsigned long entry;
	unsigned long caller;
#define CALLER_END       (-1UL)
#define CALLER_SP        (-2UL)

	unsigned long size;
	struct _elf_info *elf_info;
	char entry_str[CALLSTACK_STR_SZ + 1];
};
#endif

#define WRITE_BUF_SZ  512

#define fs_unavailable() (in_interrupt() || in_atomic() || irqs_disabled() || (current->fs == NULL))
#define print_fs_status() printk("inr %lu atm %d irq %d fs %p\n",	\
		in_interrupt(), in_atomic(), irqs_disabled(), current->fs)

extern void em_dump_regs(int argc, char **argv);
extern void em_dump_stack(int argc, char **argv);
#ifdef CONFIG_SNSC_EM_VERSION_FILE
extern void em_dump_version(int argc, char **argv);
#endif
extern void em_dump_regs_detail(int argc, char **argv);
extern void em_get_callstack(void);
extern void em_show_syndrome(void);

extern int em_dump_write(const char *format, ...);
extern char *em_get_execname(void);
extern void em_dump_callstack(int argc, char **argv);
extern int monitor_mode;
extern int not_interrupt;
extern char em_callstack_param[];
extern void em_disable_irq(void);
extern void em_enable_irq(void);
extern void em_flush_serial(void);
extern char em_convert_char(unsigned long c);

#ifndef CONFIG_SNSC_EM_I686
extern void em_disasm_arm(int argc, char **argv, int *disasm_size, unsigned long **disasm_point);
#endif
#define LOG_BUF_SZ  128
extern void em_arch_reset_cache(void);

#ifdef CONFIG_SNSC_EM_NOTIFY
extern int em_notify_register(void);
extern void em_notify_unregister(void);
extern void em_notify_cmd(int argc, char **argv);
extern void em_notify_enter(void);
#else
static inline int em_notify_register(void) { return 0; }
static inline void em_notify_unregister(void) { }
static inline void em_notify_cmd(int argc, char **argv) { }
static inline void em_notify_enter(void) { }
#endif

#endif
