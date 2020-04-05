/*
 *  exception.c  - Exception Monitor: entered when exception occured.
 *
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

#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/ctype.h>
#include <linux/elf.h>
#include <linux/interrupt.h>
#include <linux/smp.h>
#include <asm/uaccess.h>
#include <linux/pci.h>
#include <linux/console.h>
#include <linux/fs.h>
#include <linux/sched.h>
#include <linux/version.h>
#include "exception.h"
#include <linux/em_export.h>
#include <linux/log_driver_types.h>
#include <linux/snsc_raw_clock.h>
#ifdef CONFIG_SNSC_EM_USER_HOOK
#include "em_user_hook.h"
#endif

MODULE_LICENSE("GPL v2");
MODULE_VERSION("1.0");

#ifndef CONFIG_SNSC_EM_INTERACT
#define CONFIG_SNSC_EM_INTERACT "on"
#endif
#ifndef CONFIG_SNSC_EM_INITDUMP
#define CONFIG_SNSC_EM_INITDUMP "both"
#endif
#ifndef CONFIG_SNSC_EM_CALLSTACK
#define CONFIG_SNSC_EM_CALLSTACK "emlegacy"
#endif

static char* interact=CONFIG_SNSC_EM_INTERACT;
module_param(interact, charp, S_IRUGO);

static char* initdump=CONFIG_SNSC_EM_INITDUMP;
module_param(initdump, charp, S_IRUGO);

static char* reboot = CONFIG_SNSC_EM_REBOOT;
module_param(reboot, charp, S_IRUGO);

#define CALLSTACK_PARAM_SIZE 128
char em_callstack_param[CALLSTACK_PARAM_SIZE] = CONFIG_SNSC_EM_CALLSTACK;
module_param_string(callstack, em_callstack_param, CALLSTACK_PARAM_SIZE,
		   S_IRUGO|S_IWUSR);

#define INTERACT_BIT  0x00000001
#define INITDUMP_CONS 0x00000002
#define INITDUMP_FILE 0x00000004
#define INITDUMP_LOG  0x00000008
#define REBOOT_BIT    0x00000010

static int em_param_flags = 0x0;
static int is_initdumping = 0;

#define ALIGN4(x) (((x) + 0x3) & 0xfffffffc)

/*
 * MONITOR_MODE:
 * 1: don't show function name at callstack dump
 * 2: show function name at callstack dump
 * 3: show function name at callstack dump
 */
int monitor_mode = 2;
struct pt_regs *em_regs;
int not_interrupt = 1;
static int wdt_fired = 0;

static char *log = CONFIG_SNSC_EM_LOGFILENAME;
module_param(log, charp, S_IRUGO);

static int oom_exit;
module_param_named(oom_exit, oom_exit, int, 0644);

static struct file *log_file = NULL;

#ifdef CONFIG_SNSC_EM_MIPS
extern void em_dump_tlb(int argc, char **argv);
int __init em_arch_init(void) {
    return 0;
}
void em_arch_exit(void) {}
#endif
#ifdef CONFIG_SNSC_EM_ARM
extern int __init em_arch_init(void);
extern void em_arch_exit(void);
extern int  em_arch_get_wdt_status(void);
extern int  em_arch_reset_wdt(void);
extern void em_arch_reboot(void);
#endif

#ifdef CONFIG_SNSC_EM_LOG_DRIVER
/*
 * for log driver
 */
struct log_line {
	struct log_header drv;
	unsigned char data1;
	unsigned char data2;
	unsigned char level;
	unsigned short data3;
	unsigned int dumpAddr;
};

#define DATA1_BUF_LEN 6		/* %5.5s          */
#define DATA2_BUF_LEN 14	/* Category[%03x] */
#define DATA3_BUF_LEN 4		/* %03x           */

static const char *const string_color[] = {
	"\x1B[0m",
	"\x1B[1m",
	"\x1B[4m",
	"\x1B[30m",
	"\x1B[31m",
	"\x1B[32m",
	"\x1B[33m",
	"\x1B[34m",
	"\x1B[35m",
	"\x1B[36m",
	"\x1B[37m"
};

static struct log_info *loginfo = NULL;
static int logable = 0;
#endif

/*
 * for dump byte/word/long
 */
#ifdef CONFIG_SNSC_EM_PPC
static unsigned char *dump_point = (unsigned char *)0x10000000;
#endif
#ifdef CONFIG_SNSC_EM_ARM
static unsigned char *dump_point = (unsigned char *)0x00008000;
#endif
#ifdef CONFIG_SNSC_EM_MIPS
static unsigned char *dump_point = (unsigned char *)0x00400000;
#endif
#ifdef CONFIG_SNSC_EM_I686
static unsigned char *dump_point = (unsigned char *)0x08048000;
#endif
static int dump_size = 0x100;

/*
 * for disassemble
 */
#ifdef CONFIG_SNSC_EM_DISASSEMBLE
#define DISASM_BUF_SZ 128
#define FILE int
#ifdef CONFIG_SNSC_EM_PPC
static const unsigned long * const init_disasm_point = (const unsigned long * const)0x10000000;
#endif
#ifdef CONFIG_SNSC_EM_ARM
static const unsigned long * const init_disasm_point = (const unsigned long * const)0x00008000;
#endif
#ifdef CONFIG_SNSC_EM_MIPS
static const unsigned long * const init_disasm_point = (const unsigned long * const)0x00400000;
#endif
#ifdef CONFIG_SNSC_EM_I686
static const unsigned long * const init_disasm_point = (const unsigned long * const)0x08048000;
enum I686_SYMTAX {
  ATT_SYNTAX,
  INTEL_SYNTAX
};
static int x86_disas_syntax = ATT_SYNTAX;
/* same size as in i386-dis.c */
#define MAX_BUF 20
static unsigned char x86_disas_buf[MAX_BUF];
#endif
static unsigned long *disasm_point;
static int disasm_size = 16;
int disasm_command = 0;
#ifdef CONFIG_SNSC_EM_I686
extern int print_insn_i386_att(unsigned char *);
extern int print_insn_i386_intel(unsigned char *);
#endif
#endif

#ifdef CONFIG_MODULES
extern struct list_head *em_modules;
#endif
extern void (*exception_check) (int mode, struct pt_regs * em_regs);
extern int console_read(unsigned char *buf, int count);
extern int console_write(const unsigned char *buf, int count);
extern int flush_serial_tty(void);

#define LOG_BUF_SZ  128
static char log_buf[LOG_BUF_SZ];

struct em_callback_node {
	struct list_head list;
	em_callback_t fun;
	void *arg;
};

#ifdef CONFIG_SNSC_EM_USERMODE_CALLBACK
static DECLARE_RWSEM(em_usermode_callback_sem);
static LIST_HEAD(em_usermode_callback_head);

void *em_register_usermode_callback(em_callback_t fun, void *arg)
{
	struct em_callback_node *node;

	node = (struct em_callback_node *)kmalloc(sizeof(*node),
						  GFP_KERNEL);
	if (!node)
		return NULL;
	node->fun = fun;
	node->arg = arg;
	down_write(&em_usermode_callback_sem);
	list_add(&node->list, &em_usermode_callback_head);
	up_write(&em_usermode_callback_sem);
	return node;
}
EXPORT_SYMBOL(em_register_usermode_callback);

void em_unregister_usermode_callback(void *handle)
{
	if (!handle)
		return;
	down_write(&em_usermode_callback_sem);
	list_del(handle);
	up_write(&em_usermode_callback_sem);
	kfree(handle);
}
EXPORT_SYMBOL(em_unregister_usermode_callback);

static void em_call_usermode_callback(struct pt_regs *regs)
{
	struct em_callback_node *node;

	down_read(&em_usermode_callback_sem);
	list_for_each_entry(node, &em_usermode_callback_head, list) {
		node->fun(regs, node->arg);
	}
	up_read(&em_usermode_callback_sem);
}
#else /* CONFIG_SNSC_EM_USERMODE_CALLBACK */
inline static void em_call_usermode_callback(struct pt_regs *regs) {}
#endif /* CONFIG_SNSC_EM_USERMODE_CALLBACK */

#ifdef CONFIG_SNSC_EM_ATOMIC_CALLBACK
static DEFINE_SPINLOCK(em_atomic_callback_lock);
static LIST_HEAD(em_atomic_callback_head);

void *em_register_atomic_callback(em_callback_t fun, void *arg)
{
	struct em_callback_node *node;

	node = (struct em_callback_node *)kmalloc(sizeof(*node),
						  GFP_KERNEL);
	if (!node)
		return NULL;
	node->fun = fun;
	node->arg = arg;
	spin_lock(&em_atomic_callback_lock);
	list_add_rcu(&node->list, &em_atomic_callback_head);
	spin_unlock(&em_atomic_callback_lock);
	return node;
}
EXPORT_SYMBOL(em_register_atomic_callback);

void em_unregister_atomic_callback(void *handle)
{
	if (!handle)
		return;
	spin_lock(&em_atomic_callback_lock);
	list_del_rcu(handle);
	spin_unlock(&em_atomic_callback_lock);
	synchronize_rcu();
	kfree(handle);
}
EXPORT_SYMBOL(em_unregister_atomic_callback);

static void em_call_atomic_callback(struct pt_regs *regs)
{
	struct em_callback_node *node;

	rcu_read_lock();
	list_for_each_entry_rcu(node, &em_atomic_callback_head, list) {
		node->fun(regs, node->arg);
	}
	rcu_read_unlock();
}
#else  /* CONFIG_SNSC_EM_ATOMIC_CALLBACK */
inline static void em_call_atomic_callback(struct pt_regs *regs) {}
#endif /* CONFIG_SNSC_EM_ATOMIC_CALLBACK */

static void em_dump_string_to_file(struct file* f, int filter_flag, char * all_buf, int all_buf_len)
{
	if (f &&
			(em_param_flags & filter_flag)) {
		f->f_op->write(f, all_buf, all_buf_len,
				&f->f_pos);
	}
}

int em_dump_write(const char *format, ...)
{
	char buf[WRITE_BUF_SZ];
	va_list args;
	int ret = 0;

	va_start(args, format);
	vsnprintf(buf, WRITE_BUF_SZ, format, args);
	va_end(args);
	buf[WRITE_BUF_SZ - 1] = '\0';

	if (!is_initdumping ||
	    (em_param_flags & INITDUMP_CONS))
#ifdef CONFIG_SNSC_EM_USE_CONSOLE_WRITE
		console_write(buf, strlen(buf));
#else
		ret = printk("%s", buf);
#endif

	em_dump_string_to_file(log_file, INITDUMP_FILE, buf, strlen(buf));
	return ret;
}

char *em_get_execname(void)
{
	current->comm[TASK_COMM_LEN-1] = '\0';
	return current->comm;
}

char em_convert_char(unsigned long c)
{
	if (((c & 0xff) < 0x20) || ((c & 0xff) > 0x7e))
		return '.';
	else
		return c & 0xff;
}

static void em_dump_current_task(int argc, char **argv)
{
	em_dump_write("\n[current task]\n");

	if (!not_interrupt) {
		em_dump_write("Exception Monitor invoked from kernel mode:\n"
			      "Showing information from task_struct current.\n"
			      );
	}

	em_dump_write("program: %s (pid: %d, task_struct: 0x%08x)\n",
		      em_get_execname(), current->pid, (unsigned int)current);
#if defined(CONFIG_SNSC_EM_PPC) && LINUX_VERSION_CODE < KERNEL_VERSION(2,6,21)
	em_dump_write("last syscall: %ld\n", current->thread.last_syscall);
#endif
#ifdef CONFIG_SNSC_EM_ARM
	em_dump_write("address: %08x, trap_no: %08x, error_code: %08x at epc: %08x \n",
		      current->thread.address,
		      current->thread.trap_no,
		      current->thread.error_code,
		      em_regs->ARM_pc);
#endif
}

#ifdef CONFIG_MODULES
void em_dump_modules(int argc, char **argv)
{
	struct module *mod;

	em_dump_write("\n[modules]\n");

	em_dump_write("%10s %8s   %s\n", "Address", "Size", "Module");

	mutex_lock(&module_mutex);
	list_for_each_entry(mod, em_modules, list) {
		em_dump_write("0x%8p %8lu   %s [%8p]\n",
			      mod->module_core,
			      mod->init_size + mod->core_size,
			      mod->name,
			      mod);
	}
	mutex_unlock(&module_mutex);
}
#endif

static void em_dump_system_maps(int argc, char **argv)
{
	unsigned long page_size = 0x00001000;
	struct vm_area_struct *vm;

	em_dump_write("\n[system maps]\n");

	if (!not_interrupt) {
		em_dump_write("Exception Monitor invoked from kernel mode:\n"
			      "Showing information from task_struct current\n"
			      );
	}

	if (!current || !current->mm || !current->mm->mmap) {
		em_dump_write("current->mm->mmap is NULL");
		return;
	}

	em_dump_write("start    end      flg offset     name\n");

	for (vm = current->mm->mmap; vm; vm = vm->vm_next) {
		em_dump_write("%08x-%08x ", vm->vm_start, vm->vm_end);
		em_dump_write("%c", (vm->vm_flags & VM_READ) ? 'r' : '-');
		em_dump_write("%c", (vm->vm_flags & VM_WRITE) ? 'w' : '-');
		em_dump_write("%c ", (vm->vm_flags & VM_EXEC) ? 'x' : '-');

		em_dump_write("0x%08x ", vm->vm_pgoff * page_size);

		if (vm->vm_file && vm->vm_file->f_dentry &&
		    vm->vm_file->f_dentry->d_name.name)
			em_dump_write("%s\n",
				      vm->vm_file->f_dentry->d_name.name);
		else
			em_dump_write("\n");
	}
}

#ifdef CONFIG_SNSC_EM_DISASSEMBLE
#ifdef CONFIG_SNSC_EM_I686
static void em_disasm_i686(int argc, char **argv)
{
	int n;
	int size = disasm_size;
	unsigned long *point = (unsigned long *)disasm_point;
	int i;
	unsigned char *pointc;

	for (i = 0; i < argc && argv != NULL; i++) {
		if (strncmp(argv[i], "-M", 2) == 0 ||
		    strncmp(argv[i], "-m", 2) == 0) {
			if ((i + 1) == argc) {
				em_dump_write("-M option needs syntax (intel or att) 1\n");
				return;
			}
			if (strncmp(argv[i+1], "intel", 5) == 0) {
				x86_disas_syntax = INTEL_SYNTAX;
			} else if (strncmp(argv[i+1], "att", 3) == 0) {
				x86_disas_syntax = ATT_SYNTAX;
			} else {
				em_dump_write("-M option syntax must be intel or att.\n");
				return;
			}
			/* wipe off x86 syntax args */
			i += 2;
			for (; i < argc; i++) {
				argv[i-2] = argv[i];
			}
			argc -= 2;
			break;
		}
	}

	switch (argc) {
	case 3:
		if ((argv[2][0] == '0') && (toupper(argv[2][1]) == 'X')) {
			argv[2] = &argv[2][2];
		}
		size = simple_strtoul(argv[2], NULL, 16);
	case 2:
		if ((argv[1][0] == '0') && (toupper(argv[1][1]) == 'X')) {
			argv[1] = &argv[1][2];
		}
		point =
		    (unsigned long *)ALIGN4(simple_strtoul(argv[1], NULL, 16));
		break;
	case 1:
		break;
	default:
		return;
	}

	pointc = (unsigned char *)point;
	n = size;
	while (n--) {
		em_dump_write("0x%08x:\t", pointc);
		for (i = 0; i < MAX_BUF; i++) {
			if (__get_user(x86_disas_buf[i],
				       pointc+i)) {
				em_dump_write("(bad data address)\n");
				pointc++;
				continue;
			}
		}
		if (x86_disas_syntax == ATT_SYNTAX)
			pointc += print_insn_i386_att(x86_disas_buf);
		else if (x86_disas_syntax == INTEL_SYNTAX)
			pointc += print_insn_i386_intel(x86_disas_buf);
		else
			em_dump_write("invalid syntax");
		em_dump_write("\n");
	}
	point = (unsigned long *)pointc;
	disasm_point = point;
	disasm_size = size;
}
#endif

static void em_disasm(int argc, char **argv)
{
#ifndef CONFIG_SNSC_EM_I686
	em_disasm_arm(argc, argv, &disasm_size, &disasm_point);
#else
	em_disasm_i686(argc, argv);
#endif
}
#endif

#ifdef CONFIG_SNSC_EM_LOG_DRIVER
static void em_out_string_dump_file(struct log_line *read_line, struct file *log_file);
void em_out_string_dump(struct log_line *read_line)
{
	em_out_string_dump_file(read_line, 0);
}
static void em_out_string_dump_file(struct log_line *read_line, struct file *log_file)
{
	char binbuf[80];
	char charbuf[80];

	unsigned int addr = read_line->dumpAddr;
	unsigned char *p = (unsigned char *)read_line +
	    read_line->drv.string_offset;
	unsigned char *end_p = p + read_line->drv.size;
	unsigned char *buf_end_p = (unsigned char *)loginfo +
	    loginfo->buf_offset + loginfo->buf_size;
	int i;
#define ALL_BUFSIZ (80 + 80 + 30)
	char all_buf[ALL_BUFSIZ] = "";
	int all_buf_len = 0;


	if (end_p > buf_end_p)
		end_p = buf_end_p;

	/* "%08x : %02x %02x %02x %02x %02x %02x %02x %02x  %02x %02x %02x %02x %02x %02x %02x %02x : %c%c%c%c%c%c%c%c %c%c%c%c%c%c%c%c\r\n" */
	while (p < end_p) {
		unsigned int start_addr = addr;
		binbuf[0] = '\0';
		charbuf[0] = '\0';
		for (i = 0; i < 16; ++i, ++addr, ++p) {
#define LOCAL_BUFSIZ 5
			if (p < end_p) {
				char buf[LOCAL_BUFSIZ];
				snprintf(buf, LOCAL_BUFSIZ, "%02x ", *p);
				/*
				 * The following line does not cause
				 * binbuf overflow because
				 * 3 * 16 + 1 + 1 <= 80
				 *   3: appended size
				 *   * 16: num of repetition
				 *   + 1: for i==7 case below
				 *   + 1: for terminating null
				 *   80: binbuf size
				 */
				strncat(binbuf, buf, 3);
				snprintf(buf, LOCAL_BUFSIZ, "%c",
					((0x20 <= *p)
					 && (*p <= 0x7f)) ? *p : '.');
				/*
				 * The following line does not cause
				 * charbuf overflow because
				 * 1 * 16 + 1 <= 80
				 *   1: appended size (atmost)
				 *   * 16: num of repetition
				 *   + 1: for terminating null
				 *   80: charbuf size
				 */
				strncat(charbuf, buf, 1);
			} else {
				/* See comment above. */
				strncat(binbuf, "   ", 3);
				strncat(charbuf, " ", 1);
			}
			if (i == 7) {
				/* See comment above. */
				strncat(binbuf, " ", 1);
			}
#undef LOCAL_BUFSIZ
		}
		all_buf_len = snprintf(all_buf, ALL_BUFSIZ, "0x%08x : %s: %s\r\n", start_addr, binbuf, charbuf);
		all_buf[all_buf_len] = '\0';
		printk("%s", all_buf);
		em_dump_string_to_file(log_file, INITDUMP_LOG, all_buf, all_buf_len);
#undef ALL_BUFSIZ
	}
}

static void em_out_string_file(struct log_header *read_ptr, struct file *log_file);
static void em_out_string(struct log_header *read_ptr)
{
	em_out_string_file(read_ptr, NULL);
}

static void em_out_string_file(struct log_header *read_ptr, struct file *log_file)
{
	static int new_line = 1;
	struct log_line *read_line = (struct log_line *)read_ptr;
	char (*mtrx)[DATA1_BUF_LEN];
#define DATA_BUFSIZ (DATA1_BUF_LEN + DATA2_BUF_LEN + DATA3_BUF_LEN + 1)
	char data_buf[DATA_BUFSIZ] = "";
	const char *start_color_buf = "";
	const char *end_color_buf = "";
	int string_width = 128;
#define TIME_BUFSIZ 20
	char time_buf[TIME_BUFSIZ];
	int i;
#define ALL_BUFSIZ (TIME_BUFSIZ + 79 + DATA_BUFSIZ + 100)
	char all_buf[ALL_BUFSIZ] = "";
	int all_buf_len = 0;

 	if (read_ptr->time.tv_sec >= 10000) {

		string_width -= snprintf(time_buf, TIME_BUFSIZ, "%8ds ",
					(int)read_ptr->time.tv_sec);
	} else {
		long msec = (read_ptr->time.tv_usec + 500) / 1000;
		string_width -= snprintf(time_buf, TIME_BUFSIZ, "%04d.%03ds ",
					(int)read_ptr->time.tv_sec, (int)msec);
	}
	if (read_ptr->string_offset == sizeof(struct log_header)) {
		/* COLOR */
		start_color_buf = string_color[2];
		end_color_buf = string_color[0];
	} else {
		mtrx = (char (*)[DATA1_BUF_LEN])((unsigned char *)loginfo +
						 loginfo->info_offset);
		string_width -= snprintf(data_buf, DATA_BUFSIZ,
					"%5.5s Category[%03x] %03x ",
					mtrx[read_line->data1],
					read_line->data2, read_line->data3);
		/* COLOR */
		switch (read_line->level) {
		case 0:
			start_color_buf = string_color[1];
			end_color_buf = string_color[0];
			break;
		case 1:
			start_color_buf = string_color[4];
			end_color_buf = string_color[0];
			break;
		case 2:
			start_color_buf = string_color[6];
			end_color_buf = string_color[0];
			break;
		case 3:
		case 4:
		case 5:
			break;
		}
	}
	/* PRINT OUT */
	if ((read_ptr->string_offset == sizeof(struct log_header)) ||
	    (read_line->dumpAddr == 0)) {
		char *string = (char *)read_ptr + read_ptr->string_offset;
		if (read_ptr->size < 0 || CONFIG_SNSC_EM_LOGFILE_ACCEPTABLE_LINE_BYTES <= read_ptr->size) {
			new_line = 1;
			return;
		}
		for (i = 0;
		     i < read_ptr->size / string_width + 1;
		     ++i, string += string_width) {
			int write_width = strlen(string);
			write_width = (write_width > string_width) ?
			    string_width : write_width;
			all_buf_len = 0;
			if (i != 0) {
				all_buf_len += snprintf(all_buf + all_buf_len, ALL_BUFSIZ - all_buf_len, "\r\n");
			}
			all_buf_len += snprintf(all_buf + all_buf_len, ALL_BUFSIZ - all_buf_len, "%.12s", start_color_buf);
			if (new_line) {
				all_buf_len += snprintf(all_buf + all_buf_len, ALL_BUFSIZ - all_buf_len, "%s", time_buf);
			}
			all_buf_len += snprintf(all_buf + all_buf_len, ALL_BUFSIZ - all_buf_len, "%.12s", start_color_buf);
			all_buf_len += snprintf(all_buf + all_buf_len, ALL_BUFSIZ - all_buf_len, "%s%-*.*s%.12s",
									data_buf, write_width, write_width, string, end_color_buf);
			all_buf[all_buf_len] = '\0';
			if (strchr(all_buf, '\n') == NULL) {
				new_line = 0;
			} else {
				new_line = 1;
			}
			if (log_file == NULL) {
				printk("%s", all_buf);
			} else {
				em_dump_string_to_file(log_file, INITDUMP_LOG, all_buf, all_buf_len);
			}
		}
	} else {
		printk("%.12s", start_color_buf);
		em_out_string_dump_file(read_line, log_file);
		printk("%.12s", end_color_buf);
	}
#undef ALL_BUFSIZ
#undef TIME_BUFSIZ
#undef DATA_BUFSIZ
}

static unsigned int em_get_avail_bufsz(unsigned int r_offset)
{
	unsigned int w_offset = loginfo->write_offset;
	if (r_offset >= loginfo->buf_size) {
		return 0xffffffff;
	}
	if (r_offset < w_offset) {
		return loginfo->write_offset - (w_offset - r_offset);
	} else if (r_offset > w_offset) {
		return r_offset - w_offset;
	} else {
		return loginfo->write_offset;
	}
	/* never come */
	return 0;
}

static void em_dump_log(long step, struct file *log_file)
{
	unsigned int start;
	unsigned int end;
	struct log_header *header_p;
	long i;
	unsigned int prev_start;
	unsigned int avail_bufsz, prev_avail_bufsz;

	if (!logable) {
		return;
	}

	end = loginfo->write_offset;
	avail_bufsz = loginfo->buf_size;
	start = loginfo->write_offset;
	for (i = 0; i < step; i++) {
		prev_start = start;
		start =
			((struct log_header *)((unsigned char *)loginfo +
								   loginfo->buf_offset +
								   prev_start))->prev_offset;
		if (start == 0xffffffff) {
			start = prev_start;
			break;
		}
		prev_avail_bufsz = avail_bufsz;
		avail_bufsz = em_get_avail_bufsz(start);
		if (prev_avail_bufsz <= avail_bufsz
			|| avail_bufsz == 0xffffffff) {
			start = prev_start;
			break;
		}
	}

	while (start != end) {
		header_p = (struct log_header *)((unsigned char *)loginfo +
						 loginfo->buf_offset + start);
		if (header_p->next_offset == 0) {
			start = 0;
			header_p =
			    (struct log_header *)((unsigned char *)loginfo +
						  loginfo->buf_offset);
		}
		em_out_string_file(header_p, log_file);
		start = header_p->next_offset;
	}
}

static void em_dump_log_command(int argc, char **argv)
{
	long step = LONG_MAX;
	if (argc >= 2) {
		step = simple_strtol(argv[1], NULL, 10);
	}
	em_dump_log(step, NULL);
}

static void em_dump_log_to_file(struct file *log_file, long step)
{
	if (fs_unavailable()) {
		return;
	}
	em_dump_log(step, log_file);
}

static void em_flush_log(void)
{
	volatile unsigned int *read_p = (unsigned int *)&(loginfo->read_offset);
	volatile unsigned int *write_p =
	    (unsigned int *)&(loginfo->write_offset);
	struct log_header *header_p;

	while (*read_p != *write_p) {
		header_p = (struct log_header *)((unsigned char *)loginfo +
				loginfo->buf_offset + *read_p);

		if (header_p->next_offset == 0) {
			*read_p = 0;
			header_p =
			    (struct log_header *)((unsigned char *)loginfo +
						  loginfo->buf_offset);
		}

		em_out_string(header_p);
		*read_p = header_p->next_offset;
		if(*read_p > loginfo->buf_size) {
			break;
		}
	}
}
#endif /* CONFIG_SNSC_EM_LOG_DRIVER */

static void em_dump_byte(int argc, char **argv)
{
	int i;
	char buf[17];
	int n = 0;
	unsigned long insn;
	unsigned char c = 0;
	unsigned char *point = (unsigned char *)dump_point;
	int size = dump_size;

	switch (argc) {
	case 3:
		if ((argv[2][0] == '0') && (toupper(argv[2][1]) == 'X')) {
			argv[2] = &argv[2][2];
		}
		size = simple_strtoul(argv[2], NULL, 16);
	case 2:
		if ((argv[1][0] == '0') && (toupper(argv[1][1]) == 'X')) {
			argv[1] = &argv[1][2];
		}
		point = (unsigned char *)simple_strtoul(argv[1], NULL, 16);
		break;
	case 1:
		break;
	default:
		return;
	}

	buf[16] = 0;
	while (n < size) {
		em_dump_write("%08x :", point);
		for (i = 0; i < 16; i++) {
			if (n < size) {
				if (__get_user(insn, point)) {
					em_dump_write(" (Bad data address)\n");
					return;
				}
				c = *point++;
				buf[i] = em_convert_char(c);
				em_dump_write(" %02x", c);
				n++;
			} else {
				buf[i] = ' ';
				em_dump_write("   ");
			}
		}
		em_dump_write(" : %s\n", buf);
	}
	dump_point = point;
	dump_size = size;
}

static void em_dump_word(int argc, char **argv)
{
	int i;
	char buf[17];
	int n = 0;
	unsigned long insn;
	unsigned short c = 0;
	unsigned short *point = (unsigned short *)dump_point;
	int size = dump_size;

	switch (argc) {
	case 3:
		if ((argv[2][0] == '0') && (toupper(argv[2][1]) == 'X')) {
			argv[2] = &argv[2][2];
		}
		size = simple_strtoul(argv[2], NULL, 16);
	case 2:
		if ((argv[1][0] == '0') && (toupper(argv[1][1]) == 'X')) {
			argv[1] = &argv[1][2];
		}
		point = (unsigned short *)simple_strtoul(argv[1], NULL, 16);
		break;
	case 1:
		break;
	default:
		return;
	}

	buf[16] = 0;
	while (n < (size / 2)) {
		em_dump_write("%08x :", point);
		for (i = 0; i < 8; i++) {
			if (n < size) {
				if (__get_user(insn, point)) {
					em_dump_write(" (Bad data address)\n");
					return;
				}
				c = *point++;
				buf[i * 2] = em_convert_char(c >> 8);
				buf[i * 2 + 1] = em_convert_char(c);
				em_dump_write(" %04x", c);
				n++;
			} else {
				buf[i] = ' ';
				em_dump_write("   ");
			}
		}
		em_dump_write(" : %s\n", buf);
	}
	dump_point = (unsigned char *)point;
	dump_size = size;
}

static void em_dump_long(int argc, char **argv)
{
	int i;
	char buf[17];
	int n = 0;
	unsigned long insn;
	unsigned long c = 0;
	unsigned long *point = (unsigned long *)dump_point;
	int size = dump_size;

	switch (argc) {
	case 3:
		if ((argv[2][0] == '0') && (toupper(argv[2][1]) == 'X')) {
			argv[2] = &argv[2][2];
		}
		size = simple_strtoul(argv[2], NULL, 16);
	case 2:
		if ((argv[1][0] == '0') && (toupper(argv[1][1]) == 'X')) {
			argv[1] = &argv[1][2];
		}
		point = (unsigned long *)simple_strtoul(argv[1], NULL, 16);
		break;
	case 1:
		break;
	default:
		return;
	}

	buf[16] = 0;
	while (n < (size / 4)) {
		em_dump_write("%08x :", point);
		for (i = 0; i < 4; i++) {
			if (n < size) {
				if (__get_user(insn, point)) {
					em_dump_write(" (Bad data address)\n");
					return;
				}
				c = *point++;
				buf[i * 4] = em_convert_char(c >> 24);
				buf[i * 4 + 1] = em_convert_char(c >> 16);
				buf[i * 4 + 2] = em_convert_char(c >> 8);
				buf[i * 4 + 3] = em_convert_char(c);
				em_dump_write(" %08x", c);
				n++;
			} else {
				buf[i] = ' ';
				em_dump_write("   ");
			}
		}
		em_dump_write(" : %s\n", buf);
	}
	dump_point = (unsigned char *)point;
	dump_size = size;
}

static void em_write_byte(int argc, char **argv)
{
	char buf[17];
	unsigned char datum;
	unsigned char *point;
	int i;
	unsigned long insn;
	unsigned char c = 0;

	switch (argc) {
	case 3:
		if ((argv[2][0] == '0') && (toupper(argv[2][1]) == 'X')) {
			argv[2] = &argv[2][2];
		}
		datum = (unsigned char)simple_strtoul(argv[2], NULL, 16);
		point = (unsigned char *)simple_strtoul(argv[1], NULL, 16);
		break;
	case 2:
	case 1:
	default:
		return;
	}

	em_dump_write("%08x: ", point);

	if (__put_user(datum, point)) {
		em_dump_write(" (Bad data address)\n");
		return;
	}

	for (i = 0; i < 16; i++) {
		if (__get_user(insn, point)) {
			em_dump_write(" (Bad data address)\n");
			return;
		}
		c = *point++;
		buf[i] = em_convert_char(c);
		em_dump_write(" %02x", c);
	}
	buf[16] = 0;
	em_dump_write(" : %s\n", buf);
}

static void em_write_word(int argc, char **argv)
{
	char buf[17];
	unsigned short datum;
	unsigned short *point;
	int i;
	unsigned long insn;
	unsigned short c = 0;

	switch (argc) {
	case 3:
		if ((argv[2][0] == '0') && (toupper(argv[2][1]) == 'X')) {
			argv[2] = &argv[2][2];
		}
		datum = (unsigned short)simple_strtoul(argv[2], NULL, 16);
		point = (unsigned short *)simple_strtoul(argv[1], NULL, 16);
		break;
	case 2:
	case 1:
	default:
		return;
	}

	em_dump_write("%08x: ", point);

	if (__put_user(datum, point)) {
		em_dump_write(" (Bad data address)\n");
		return;
	}

	for (i = 0; i < 8; i++) {
		if (__get_user(insn, point)) {
			em_dump_write(" (Bad data address)\n");
			return;
		}
		c = *point++;
		buf[i * 2] = em_convert_char(c >> 8);
		buf[i * 2 + 1] = em_convert_char(c);
		em_dump_write(" %04x", c);
	}
	buf[16] = 0;
	em_dump_write(" : %s\n", buf);
}

static void em_write_long(int argc, char **argv)
{
	char buf[17];
	unsigned long datum;
	unsigned long *point;
	int i;
	unsigned long insn;
	unsigned long c = 0;

	switch (argc) {
	case 3:
		if ((argv[2][0] == '0') && (toupper(argv[2][1]) == 'X')) {
			argv[2] = &argv[2][2];
		}
		datum = (unsigned long)simple_strtoul(argv[2], NULL, 16);
		point = (unsigned long *)simple_strtoul(argv[1], NULL, 16);
		break;
	case 2:
	case 1:
	default:
		return;
	}

	em_dump_write("%08x: ", point);

	if (__put_user(datum, point)) {
		em_dump_write(" (Bad data address)\n");
		return;
	}

	for (i = 0; i < 4; i++) {
		if (__get_user(insn, point)) {
			em_dump_write(" (Bad data address)\n");
			return;
		}
		c = *point++;
		buf[i * 4] = em_convert_char(c >> 24);
		buf[i * 4 + 1] = em_convert_char(c >> 16);
		buf[i * 4 + 2] = em_convert_char(c >> 8);
		buf[i * 4 + 3] = em_convert_char(c);
		em_dump_write(" %08x", c);
	}
	buf[16] = 0;
	em_dump_write(" : %s\n", buf);
}

static void em_kernel_dump_stack(int argc, char **argv)
{
	dump_stack();
}

static void em_dump_exception(int argc, char **argv)
{
#ifdef CONFIG_SNSC_EM_DISASSEMBLE
	unsigned long *point = (unsigned long *)instruction_pointer(em_regs);
#endif
	em_dump_write
	    ("============================================================================");
#ifdef CONFIG_SMP
	/* use raw_smp_processor_id instead of smp_processor_id to
	 * prevent "smp_processor_id() in preemptible" error message.
	 */
	em_dump_write("\n[cpu id = %d]\n", raw_smp_processor_id());
#endif
	em_dump_regs(1, NULL);
	em_dump_stack(1, NULL);
#ifdef CONFIG_SNSC_EM_VERSION_FILE
	em_dump_version(1, NULL);
#endif
	em_dump_callstack(1, NULL);
#ifdef CONFIG_SNSC_EM_ARM
	em_dump_modules(1, NULL);
#endif
	em_dump_system_maps(1, NULL);
#ifdef CONFIG_SNSC_EM_DISASSEMBLE
	disasm_point = point;
	disasm_size = 16;
	/* em_disasm is not called from exception prompt
	   do the function start address for disassemble */
	disasm_command = 0;
	em_dump_write("\n[disassemble]\n");
	em_disasm(1, NULL);
#endif /* CONFIG_SNSC_EM_DISASSEMBLE */
	em_dump_current_task(1, NULL);
	em_show_syndrome();
	em_dump_write
	    ("============================================================================\n");
}

#define MAX_COMMAND_NAME_LEN 32
struct command {
	char name[MAX_COMMAND_NAME_LEN];
	void (*func) (int, char **);
};

static void em_help(int argc, char **argv)
{
	printk("\n[Exception monitor commands]\n");
	printk(" show                       : show exception message\n");
	printk(" reg                        : show registers\n");
	printk(" stack                      : stack dump\n");
	printk(" call                       : call stack dump\n");
	printk(" map                        : show memory map\n");
	printk(" task                       : show current task info\n");
	printk(" d[b] [<addr>] [<size>]     : dump byte-access\n");
	printk(" dw [<addr>] [<size>]       : dump word-access\n");
	printk(" dl [<addr>] [<size>]       : dump long-access\n");
#ifdef CONFIG_SNSC_EM_DISASSEMBLE
	printk(" l [<addr>] [<size>]        : disassemble\n");
#endif
	printk(" w[b] <addr> <value>        : write byte-access\n");
	printk(" ww <addr> <value>          : write word-access\n");
	printk(" wl <addr> <value>          : write long-access\n");
#ifdef CONFIG_SNSC_EM_ARM
	printk(" module                     : show modules\n");
#endif
#ifdef CONFIG_SNSC_EM_LOG_DRIVER
	printk(" log                        : dump log\n");
#endif
#ifdef CONFIG_SNSC_EM_NOTIFY
	printk(" notify <msg>               : notify msg to userspace\n");
#endif
	printk(" help                       : show this message\n");
	printk(" exit                       : exit exception monitor\n\n");
}

static const struct command command[] = {
	{"show", &em_dump_exception},
	{"reg", &em_dump_regs},
	{"regd", &em_dump_regs_detail},
	{"stack", &em_dump_stack},
#ifdef CONFIG_SNSC_EM_VERSION_FILE
	{"version", &em_dump_version},
#endif
	{"map", &em_dump_system_maps},
	{"task", &em_dump_current_task},
	{"d", &em_dump_byte},
	{"db", &em_dump_byte},
	{"dw", &em_dump_word},
	{"dl", &em_dump_long},
	{"w", &em_write_byte},
	{"wb", &em_write_byte},
	{"ww", &em_write_word},
	{"wl", &em_write_long},
	{"kd", &em_kernel_dump_stack},
#ifdef CONFIG_SNSC_EM_MIPS
	{"tlb", &em_dump_tlb},
#endif
#ifdef CONFIG_SNSC_EM_ARM
	{"module", &em_dump_modules},
#endif
#ifdef CONFIG_SNSC_EM_DISASSEMBLE
	{"l", &em_disasm},
#endif
#ifdef CONFIG_SNSC_EM_LOG_DRIVER
	{"log", &em_dump_log_command},
#endif
	{"help", &em_help}
};

/*
 *  For these commands, argc is 1 or 2.  If 2, argv[1] contains whole
 *  arguments including spaces
 */
static const struct command rawarg_command[] = {
	{"call", &em_dump_callstack},
#ifdef CONFIG_SNSC_EM_NOTIFY
	{"notify", &em_notify_cmd},
#endif
};

/* Split STR into DEST_SIZE words.  STR is modified ('\0's are written) */
/* If num of words >= DEST_SIZE, the last word contains all the rest. */
static size_t em_split(char **dest_argv, size_t dest_size, char *str)
{
	size_t i = 0;
	int word = 0;
	if (dest_size == 0)
		return 0;
	for (; *str; ++str) {
		if (isspace(*str)) {
			*str = '\0';
			word = 0;
			continue;
		}
		if (!word) {
			dest_argv[i++] = str;
			if (i == dest_size)
				return i;
			word = 1;
		}
	}
	return i;
}

#define EM_MAX_WORDS 8
static int em_execute_command(char *buf)
{
	int i;
	char *argv[EM_MAX_WORDS];
	int argc;

	argc = em_split(argv, 2, buf);
	if (argc == 0)
		return 0;
	/* CMD ENTIRE_ARGS style commands */
	for (i = 0; i < sizeof(rawarg_command) / sizeof(*rawarg_command); i++) {
		if (strncmp(argv[0], rawarg_command[i].name, MAX_COMMAND_NAME_LEN) == 0) {
			(*rawarg_command[i].func) (argc, argv);
			return 0;
		}
	}
	/* CMD ARG ARG ... style commands */
	if (argc > 1)
		argc = em_split(argv + 1, EM_MAX_WORDS - 1, argv[1]) + 1;
	for (i = 0; i < sizeof(command) / sizeof(*command); i++) {
		if (strncmp(argv[0], command[i].name, MAX_COMMAND_NAME_LEN) == 0) {
			(*command[i].func) (argc, argv);
			return 0;
		}
	}

	return -1;
}

static int em_open_logfile(struct file** f, char *name, int flags)
{
	struct inode *inode;
	struct file *pf = *f;

	if (fs_unavailable()) {
		goto fail;
	}

	*f = filp_open(name, flags, 0666);
	pf = *f;
	if (IS_ERR(*f))
		goto fail;
	inode = pf->f_dentry->d_inode;
	if (inode->i_nlink > 1)
		goto close_fail;	/* multiple links - don't dump */
	if (d_unhashed(pf->f_dentry))
		goto close_fail;
	if (!S_ISREG(inode->i_mode))
		goto close_fail;
	if (!pf->f_op)
		goto close_fail;
	if (!pf->f_op->write)
		goto close_fail;

	return 0;

      close_fail:
	filp_close(*f, NULL);

      fail:
	*f = NULL;
	return -1;
}

static void em_dump_to_file(void)
{
	if (fs_unavailable()) {
		printk("file system unavailable:");
		print_fs_status();
		log_file = NULL;
	}

	is_initdumping = 1;
	em_dump_exception(0, NULL);
	is_initdumping = 0;

	if (log_file) {
		if (log_file->f_op->fsync != NULL) {
			unsigned long long time;
			unsigned long nanosec_rem;
			time = snsc_raw_clock();
			nanosec_rem = do_div(time, 1000000000);
			em_dump_write(" [%6lu.%06lu] log_file Sync starts. \n", (unsigned long)time, nanosec_rem / 1000);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,10,0)
			vfs_fsync_range(log_file, 0, LLONG_MAX, 1);
#else
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,35)
			log_file->f_op->fsync(log_file, 0);
#else
			log_file->f_op->fsync(log_file, log_file->f_dentry, 0);
#endif
#endif
		}
		filp_close(log_file, NULL);
		log_file = NULL;
	}
}

#ifdef CONFIG_SNSC_EM_PRINT_INFO_EXTERNAL

static em_printk_t em_ext_printk;
static em_printk_open_t em_ext_printk_open;
static em_printk_close_t em_ext_printk_close;

static void em_print_info_external(void)
{
	int ret;
#ifndef CONFIG_SNSC_EM_PRINT_INFO_EXTERNAL_ALWAYS
	if (!fs_unavailable()) {
		return;
	}
#endif
	if (em_ext_printk_open) {
		ret = em_ext_printk_open();
		if (ret != 0) {
			return;
		}
	}
	if (em_ext_printk) {
		em_ext_printk("a kernel exception (wdt=%s)\n", wdt_fired ? "fired" : "not fired");
	}
	if (em_ext_printk_close) {
		em_ext_printk_close();
	}
}

void em_register_printk_external(em_printk_open_t open, em_printk_t print, em_printk_close_t close)
{
	if (em_ext_printk_open == NULL) {
		em_ext_printk_open = open;
	}
	if (em_ext_printk == NULL) {
		em_ext_printk = print;
	}
	if (em_ext_printk_close == NULL) {
		em_ext_printk_close = close;
	}
}
EXPORT_SYMBOL(em_register_printk_external);

void em_unregister_printk_external(void)
{
	em_ext_printk_open = NULL;
	em_ext_printk = NULL;
	em_ext_printk_close = NULL;
}
EXPORT_SYMBOL(em_unregister_printk_external);

#endif

extern void disable_irq(unsigned int irq);
extern void enable_irq(unsigned int irq);
void em_disable_irq(void)
{
	/* encode interrupt requests to disable here */
#ifdef CONFIG_SNSC_EM_CONSOLE_IRQ
	disable_irq(CONFIG_SNSC_EM_CONSOLE_IRQ);
#endif
}

void em_enable_irq(void)
{
	/* encode interrupt requests to enable here */
#ifdef CONFIG_SNSC_EM_CONSOLE_IRQ
	enable_irq(CONFIG_SNSC_EM_CONSOLE_IRQ);
#endif
}

static void em_open_trunc_file(struct file **f, char *name, int lf_flags, int maxsize)
{
	em_open_logfile(f, name, lf_flags);
	if (*f && (*f)->f_dentry->d_inode->i_size > maxsize) {
		filp_close(*f, NULL);
		em_open_logfile(f, name, lf_flags | O_TRUNC);
	}
}

void em_flush_serial(void)
{
	if (not_interrupt) {
		console_lock();
		console_unlock();
	}
	flush_serial_tty();
}

static void em_reset_cache(void)
{
	disasm_point = (unsigned long *)init_disasm_point;
	em_arch_reset_cache();
}

static int em_get_wdt_status(void)
{
#ifdef CONFIG_SNSC_EM_ARM
	return em_arch_get_wdt_status();
#endif
}

static void em_reset_wdt(void)
{
#ifdef CONFIG_SNSC_EM_RESET_WATCHDOG
	em_arch_reset_wdt();
#endif
}

static void em_reboot(void)
{
#ifdef CONFIG_SNSC_EM_ARM
	em_arch_reboot();
#endif
}

static DEFINE_SPINLOCK(em_lock);
void em_exception_monitor(int mode, struct pt_regs *registers)
{
	char *buf;
	unsigned long flags;
	mm_segment_t fs = get_fs();
	unsigned long long time;
	unsigned long nanosec_rem;

	/* disconnect exception monitor from hook. */
	spin_lock_irqsave(&em_lock, flags);
	if (exception_check == NULL) {
		spin_unlock_irqrestore(&em_lock, flags);
		return;
	}
	em_reset_cache();
	exception_check = NULL;
	spin_unlock_irqrestore(&em_lock, flags);

	set_fs(KERNEL_DS);
#ifdef CONFIG_LOCK_KERNEL
	if (preempt_count() || irqs_disabled() || kernel_locked())
#else
	if (preempt_count() || irqs_disabled())
#endif
	{
		not_interrupt = 0;
	} else {
		not_interrupt = 1;
	}

	if (!wdt_fired) {
		wdt_fired = em_get_wdt_status();
	}
	if ((mode > 0) && (mode < 4)) {
		monitor_mode = mode;
	} else {
		monitor_mode = 3;
	}

	time = snsc_raw_clock();
#ifdef CONFIG_SNSC_EM_LOG_DRIVER
	if (write_log_driver != NULL) {
		logable = 1;
		write_log_driver = NULL;
		log_disable_write();
	}
#endif

	if (oom_exit && test_tsk_thread_flag(current, TIF_MEMDIE)) {
		em_dump_write("oom_exit enabled - quit exception monitor\n");
		goto end;
	}

	/*
	 * group_stop_count is non_zero if do_coredump() is called as
	 * a result of sending signal (not as a result of a CPU
	 * exception). In which case we should explicitly clear
	 * group_stop_count and TIF_SIGPENDING in order to access
	 * files, as do_coredump() does before coredumping.
	 */
	if (user_mode(registers)) {
		spin_lock_irqsave(&current->sighand->siglock, flags);
		current->signal->group_stop_count = 0;
		clear_thread_flag(TIF_SIGPENDING);
		spin_unlock_irqrestore(&current->sighand->siglock, flags);
	}

	/*
	 * Flush serial buffer first
	 */
	em_flush_serial();

	if (user_mode(registers))
		em_call_usermode_callback(registers);
	em_call_atomic_callback(registers);

	/*
	 * Disable interrupt requests
	 */
	em_disable_irq();
	em_reset_wdt();

#ifdef CONFIG_SNSC_EM_PRINT_INFO_EXTERNAL
	em_print_info_external();
#endif

#ifdef CONFIG_SNSC_EM_LOG_DRIVER
	if (logable) {
		int lf_flags = O_CREAT | O_NOFOLLOW | O_APPEND | O_RDWR;
		loginfo = (struct log_info *)log_buffer;
		em_open_trunc_file(&log_file, log, lf_flags, CONFIG_SNSC_EM_LOGFILE_MAX_SIZE);
		em_flush_log();
		em_dump_log_to_file(log_file, CONFIG_SNSC_EM_LOGFILE_LOG_MAX_LINES);
	}
#endif

	nanosec_rem = do_div(time, 1000000000);
	em_dump_write(" [%6lu.%06lu] Exception happened\n", (unsigned long)time, nanosec_rem / 1000);

	/*
	 * Do some initialization stuff
	 */
	if (registers != NULL) {
		em_regs = registers;
	} else {
		em_dump_write("pt_regs is NULL\nreturn\n");
		goto end;
	}
#ifdef CONFIG_SNSC_EM_DISASSEMBLE
	disasm_point = (unsigned long *)instruction_pointer(em_regs);
	disasm_size = 16;
#endif
#ifdef EMLEGACY_CALLSTACK
	if (not_interrupt)
		em_get_callstack();
#endif
#ifdef CONFIG_SNSC_EM_USER_HOOK
	if (not_interrupt)
		em_user_hook();
#endif
	em_dump_to_file();
	em_notify_enter();

	if (!(em_param_flags & INTERACT_BIT))
		goto end;

	em_dump_write("\n\nEntering exception monitor.\n");
	if (em_param_flags & REBOOT_BIT)
		em_reboot();
	em_dump_write("Type `help' to show commands.\n");
	em_dump_write("Type `exit' to exit.\n\n");

#ifdef CONFIG_SNSC_EM_PREEMPT_DISABLE
	preempt_disable();
#endif
	while (1) {
		em_dump_write("Exception> ");
		console_read((unsigned char *)log_buf, LOG_BUF_SZ);
		buf = log_buf;

		if (buf == NULL)
			continue;

		if (strcmp(buf, "exit") == 0)
			break;

		if ((buf[0] != '\0') && (em_execute_command(buf) == -1)) {
			em_dump_write("%s: Command not found.\n", buf);
		}
	}
	em_dump_write("\nGood Bye.\n");

#ifdef CONFIG_SNSC_EM_PREEMPT_DISABLE
	preempt_enable();
#endif

 end:
	/*
	 * Enable interrupt requests
	 */
	em_enable_irq();
#ifdef CONFIG_SNSC_EM_LOG_DRIVER
	log_enable_write();
#endif

	/* re-connect exception monitor to hook. */
	exception_check = em_exception_monitor;
	set_fs(fs);
}


#ifdef UNIFIED_DRIVER
int init_module_exception(void)
#else
static int __init em_module_init(void)
#endif
{
	int ret = 0;

	em_param_flags = 0;
	if (strncmp(interact, "on", 3) == 0)
		em_param_flags |= INTERACT_BIT;
	else if (strncmp(interact, "off", 4) == 0)
		;
	else {
		printk("ERROR: parameter `interact' does not support: %s.\n",
		       interact);
		return -EINVAL;
	}

	if (strncmp(initdump, "console", 8) == 0)
		em_param_flags |= INITDUMP_CONS;
	else if (strncmp(initdump, "file", 5) == 0)
		em_param_flags |= INITDUMP_FILE;
	else if (strncmp(initdump, "nolog", 6) == 0) {
		em_param_flags |= INITDUMP_CONS;
		em_param_flags |= INITDUMP_FILE;
	} else if (strncmp(initdump, "both", 5) == 0) {
		em_param_flags |= INITDUMP_CONS;
		em_param_flags |= INITDUMP_FILE;
		em_param_flags |= INITDUMP_LOG;
	} else if (strncmp(initdump, "none", 5) == 0)
		;
	else {
		printk("ERROR: parameter `initdump' does not support: %s.\n",
		       initdump);
		return -EINVAL;
	}

	if (strncmp(reboot, "on", 3) == 0)
		em_param_flags |= REBOOT_BIT;
	else if (strncmp(reboot, "off", 4) == 0)
		em_param_flags &= ~REBOOT_BIT;
	else {
		printk("ERROR: parameter `reboot' does not support: %s.\n", reboot);
		return -EINVAL;
	}

	ret = em_notify_register();
	if (ret < 0)
		return ret;

	exception_check = em_exception_monitor;

	em_arch_init();

	return 0;
}

#ifdef UNIFIED_DRIVER
void cleanup_module_exception(void)
#else
static void __exit em_module_exit(void)
#endif
{
	em_notify_unregister();
	em_arch_exit();
	exception_check = NULL;
}

#ifdef UNIFIED_DRIVER
#else
module_init(em_module_init);
module_exit(em_module_exit);
#endif
