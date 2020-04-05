/*
 *  exception-arm.c  - arm specific part of Exception Monitor
 *
 */

/* With non GPL files, use following license */
/*
 * Copyright 2005,2008,2009 Sony Corporation.
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
 *  Copyright 2005,2008,2009 Sony Corporation.
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
#include <linux/vmalloc.h>
#include <linux/version.h>
#include <asm/uaccess.h>
#include <linux/pci.h>
#include <linux/console.h>
#include <linux/fs.h>
#include <linux/sched.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include "exception.h"
#ifdef CONFIG_SNSC_EM_DEMANGLE
#include "demangle.h"
#endif
#ifdef CONFIG_SNSC_ALT_BACKTRACE
#include <linux/snsc_backtrace.h>
#include <linux/snsc_backtrace_ioctl.h>
#endif

//#define DEBUG
#ifdef DEBUG
#define dbg(fmt, argv...) em_dump_write(fmt, ##argv)
#else
#define dbg(fmt, argv...) do{}while(0)
#endif

#define ARRAY_NUM(x) (sizeof(x)/sizeof((x)[0]))
#define ALIGN4(x) (((x) + 0x3) & 0xfffffffc)
#define FILE int
#define LOWER_ADDR_DUMP_SIZE 0x200

#ifdef CONFIG_SNSC_EM_DISASSEMBLE
#ifndef CONFIG_SNSC_EM_I686
extern int print_insn(FILE *, unsigned long, unsigned);
#endif
#endif

#ifdef CONFIG_BERLIN_SM
extern int sm_asserted_watchdog(void);
extern void sm_reset_watchdog(int value); /* 15: 80sec, 14: 40sec, 13: 20sec, 12: 10sec */
extern void sm_request_reboot(void);
#endif

extern struct pt_regs *em_regs;

#ifdef CONFIG_SNSC_EM_VERSION_FILE
void em_dump_file(struct file* f)
{
	int buf_size = 4096;
	char * buf = (char *)kmalloc(buf_size, GFP_ATOMIC);
	int ret=1;
	if (buf == NULL){
		return;
	}
	while((ret=f->f_op->read(f,
						    buf,
						    buf_size,
						    &f->f_pos))){
        char prchunk[WRITE_BUF_SZ];
        char *pp = buf;
        int remain;
        for (; pp < (buf+ret)-WRITE_BUF_SZ; pp += (WRITE_BUF_SZ-1)) {
            snprintf(prchunk, WRITE_BUF_SZ, "%s", pp);
            em_dump_write("%s", prchunk);
        }
        remain=(buf+ret)-pp+1;
        if(remain>0) {
            snprintf(prchunk, remain, "%s", pp);
            prchunk[remain+1]='\0';
            em_dump_write("%s", prchunk);
        }
    }
    kfree(buf);
}

void em_dump_version(int argc, char **argv)
{
    struct file * vfile;
    if (fs_unavailable())
        return;

    vfile = filp_open(CONFIG_SNSC_EM_VERSION_FILENAME, O_RDONLY, 0444);
    if (IS_ERR(vfile))
        return;
    em_dump_write("[software version]\n");
    em_dump_file(vfile);
    filp_close(vfile, NULL);
}

#endif

/*
 * for callstack
 */
#define LIB_NAME_SIZE 64
static char libname[LIB_MAX][LIB_NAME_SIZE];

#define ELF_INFO_MAX (1 + LIB_MAX)
static struct _elf_info elf_info[ELF_INFO_MAX];

/*
 * for demangle
 */
#ifdef CONFIG_SNSC_EM_DEMANGLE
/* static int demangle_flag = DMGL_PARAMS | DMGL_ANSI | DMGL_VERBOSE | DMGL_TYPES; */
static int demangle_flag = DMGL_PARAMS | DMGL_ANSI | DMGL_VERBOSE;
#endif

static void em_close_elffile(unsigned int index)
{
	if (elf_info[index].file) {
		filp_close(elf_info[index].file, NULL);
		elf_info[index].file = NULL;
	}
}

static void em_close_elffiles(int elf_cnt)
{
	int i;

	for (i = 0; i < elf_cnt; i++) {
		em_close_elffile(i);
	}
}

static int em_open_elffile(unsigned int index)
{
	int i;
	int strip = 2;
	Elf32_Ehdr ehdr;
	Elf32_Shdr shdr;
	Elf32_Shdr sh_shstrtab;
	Elf32_Phdr phdr;
	char *shstrtab;
	struct file *fp;

	/*
	 * open elf file
	 */
	elf_info[index].file =
	    filp_open(elf_info[index].filename, O_RDONLY, 0444);

	if (IS_ERR(elf_info[index].file)) {
		elf_info[index].file = NULL;
		goto fail;
	}
	fp = elf_info[index].file;

	if (!fp->f_op || !fp->f_op->read)
		goto close_fail;

	/*
	 * read elf header
	 */
	fp->f_op->read(fp, (char *)&ehdr, sizeof(Elf32_Ehdr), &fp->f_pos);
	if (memcmp(ehdr.e_ident, ELFMAG, SELFMAG) != 0)
		goto close_fail;
	if (!elf_check_arch(&ehdr))
		goto close_fail;

	/*
	 * read program header
	 */
	vfs_llseek(fp, ehdr.e_phoff, 0);
	elf_info[index].vaddr = 0;
	for (i = 0; i < ehdr.e_phnum; i++) {
		fp->f_op->read(fp, (char *)&phdr, sizeof(Elf32_Phdr),
			       &fp->f_pos);
		if (phdr.p_type == PT_LOAD) { /* first PT_LOAD segment */
			elf_info[index].vaddr = phdr.p_vaddr;
			break;
		}
	}

	/*
	 * read section header table
	 */
	vfs_llseek(fp,
		   ehdr.e_shoff + sizeof(Elf32_Shdr) * ehdr.e_shstrndx,
		   0);
	fp->f_op->read(fp, (char *)&sh_shstrtab, sizeof(Elf32_Shdr),
		       &fp->f_pos);
	shstrtab = (char *)kmalloc(sh_shstrtab.sh_size, GFP_ATOMIC);
	if(shstrtab == NULL){
		goto close_fail;
	}
	vfs_llseek(fp, sh_shstrtab.sh_offset, 0);
	fp->f_op->read(fp, shstrtab, sh_shstrtab.sh_size, &fp->f_pos);

	/*
	 * read shsymtab
	 */
	vfs_llseek(fp, ehdr.e_shoff, 0);
	for (i = 0; i < ehdr.e_shnum; i++) {
		fp->f_op->read(fp, (char *)&shdr, sizeof(Elf32_Shdr),
			       &fp->f_pos);
		if (strcmp(&shstrtab[shdr.sh_name], ".dynsym") == 0)
			elf_info[index].sh_dynsym = shdr;
		else if (strcmp(&shstrtab[shdr.sh_name], ".dynstr") == 0)
			elf_info[index].sh_dynstr = shdr;
		else if (strcmp(&shstrtab[shdr.sh_name], ".symtab") == 0) {
			elf_info[index].sh_symtab = shdr;
			strip--;
		} else if (strcmp(&shstrtab[shdr.sh_name], ".strtab") == 0) {
			elf_info[index].sh_strtab = shdr;
			strip--;
		}
	}

	if (!strip)
		elf_info[index].strip = strip;

	kfree(shstrtab);
	return 0;

      close_fail:
	em_close_elffile(index);
      fail:
	return -1;
}

static void init_struct_elfinfo(void)
{
	int i;

	for (i = 0; i < ELF_INFO_MAX; i++) {
		elf_info[i].filename = 0;
		elf_info[i].sh_dynsym.sh_size = 0;
		elf_info[i].sh_dynstr.sh_size = 0;
		elf_info[i].sh_symtab.sh_size = 0;
		elf_info[i].sh_strtab.sh_size = 0;
		elf_info[i].addr_offset = 0;
		elf_info[i].addr_end = 0;
		elf_info[i].strip = 1;
	}

}

static int em_open_elffiles(void)
{
	char *path;
	char buf[LIB_NAME_SIZE];
	struct vm_area_struct *vm = NULL;
	char *short_name;
	int elf_cnt = 0;
	int i;

	/*
	 * initialize
	 */
	init_struct_elfinfo();

	if (!not_interrupt)
		goto out;

	/*
	 * set elf_info
	 */
	elf_info[0].filename = em_get_execname();
	short_name = elf_info[0].filename;
	for (i = 0; elf_info[0].filename[i]; i++)
		if (elf_info[0].filename[i] == '/')
			short_name = &elf_info[0].filename[i + 1];
	if (current->mm != NULL)
		vm = current->mm->mmap;
	for (; vm != NULL; vm = vm->vm_next) {
		if (vm->vm_flags & VM_WRITE)
			continue;
		if (vm->vm_file == NULL)
			continue;
		if (vm->vm_file->f_dentry) {
			if (strcmp
			    (vm->vm_file->f_dentry->d_name.name,
			     short_name) == 0) {
				elf_info[0].addr_offset = vm->vm_start;
				elf_info[0].addr_end = vm->vm_end;
			}
		}
	}

	elf_cnt = 1;

	if (current->mm != NULL)
		vm = current->mm->mmap;
	for (i = 0; i < ARRAY_NUM(libname) && vm != NULL; vm = vm->vm_next) {
		if ((vm->vm_flags & (VM_READ | VM_EXEC)) != (VM_READ | VM_EXEC))
			continue;
		if (vm->vm_flags & VM_WRITE)	/* assume text is r-x and text
						   seg addr is base addr */
			continue;
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,27)
		if (vm->vm_flags & VM_EXECUTABLE)
			continue;
#endif
		if (vm->vm_file == NULL)
			continue;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,26)
		path = d_path(&vm->vm_file->f_path, buf, sizeof(buf));
#else
		path = d_path(vm->vm_file->f_dentry, vm->vm_file->f_vfsmnt,
			      buf, sizeof(buf));
#endif
		buf[sizeof(buf) - 1] = '\0';

		if (path == NULL || access_ok(VERIFY_READ, (unsigned long)path, sizeof(buf)-1)) {
			continue;
		}
		if (strcmp(path, "/lib/ld-linux.so.2") == 0)
			continue;
		if (strcmp(path, "/devel/lib/ld-linux.so.2") == 0)
			continue;
		if (strcmp(path, "/lib/sonyld.so") == 0)
			continue;

		strncpy(libname[i], path, LIB_NAME_SIZE);
		libname[i][LIB_NAME_SIZE - 1] = 0;

		elf_info[elf_cnt].filename = libname[i];
		elf_info[elf_cnt].addr_offset = vm->vm_start;
		elf_info[elf_cnt].addr_end = vm->vm_end;
		elf_cnt++;
		i++;
	}

	for (i = 1; i < elf_cnt; i++) {
		/* skips exec file (means i=0) because recent kernel never includes path info in task->comm */
		if (em_open_elffile(i) == -1) 
			em_dump_write("\n\tWARNING: file not found: %s\n",
				      elf_info[i].filename);

		dbg("file : %s (%08lx %08lx)\n",
			 elf_info[i].filename, elf_info[i].addr_offset, elf_info[i].addr_end);
	}

out:
	return elf_cnt;
}

static long arm_pc_adjustment(void){
	static long ad;
	long tmp1,tmp2;

	if (ad)
		return ad;

	__asm__ __volatile__ (
		"1: stmfd sp!, {pc}\n"
		"   adr %1, 1b\n"
		"   ldmfd sp!, {%2}\n"
		"   sub %0, %2, %1\n"
		: "=r"(ad),"=r"(tmp1),"=r"(tmp2)
		);
	return ad;
}

static inline ulong em_get_user(ulong *p)
{
	ulong v;

	if (__get_user(v, p))
		v = 0;

	return v;
}

static inline ulong em_put_user(ulong v, ulong *p)
{
	return __put_user(v, p);
}

static inline ulong arch_stack_pointer(ulong *frame)
{
	return em_get_user(frame - 2);
}

static inline ulong arch_entry_address(ulong *frame)
{
	ulong val = em_get_user(frame);

	if (val)
		val -= (arm_pc_adjustment() + 4);
	return val;
}

static inline ulong arch_caller_address(ulong *frame)
{
	return em_get_user(frame - 1);
}

static inline ulong *arch_prev_frame(ulong *frame)
{
	return (ulong *)em_get_user(frame - 3);
}

void em_get_callstack(void)
{
	int elf_cnt;

	elf_cnt = em_open_elffiles();

	em_close_elffiles(elf_cnt);

}

void em_dump_regs(int argc, char **argv)
{
	char *mode;
	char mode_list[][4] = {"USR","FIQ","IRQ","SVC","ABT"
			       ,"UND","SYS","???"};

	em_dump_write("\n[register dump]\n");

	em_dump_write(
          "a1: r0: 0x%08lx  a2: r1: 0x%08lx  a3: r2: 0x%08lx  a4: r3: 0x%08lx\n",
		      em_regs->ARM_r0, em_regs->ARM_r1,
		      em_regs->ARM_r2, em_regs->ARM_r3);
	em_dump_write(
	  "v1: r4: 0x%08lx  v2: r5: 0x%08lx  v3: r6: 0x%08lx  v4: r7: 0x%08lx\n",
		      em_regs->ARM_r4, em_regs->ARM_r5,
		      em_regs->ARM_r6, em_regs->ARM_r7);
	em_dump_write(
	  "v5: r8: 0x%08lx  v6: r9: 0x%08lx  v7:r10: 0x%08lx  fp:r11: 0x%08lx\n",
		      em_regs->ARM_r8, em_regs->ARM_r9,
		      em_regs->ARM_r10, em_regs->ARM_fp);
	em_dump_write(
	  "ip:r12: 0x%08lx  sp:r13: 0x%08lx  lr:r14: 0x%08lx  pc:r15: 0x%08lx\n",
		      em_regs->ARM_ip, em_regs->ARM_sp,
		      em_regs->ARM_lr, em_regs->ARM_pc);

#define PSR_MODE_MASK 0x0000001f
	switch (em_regs->ARM_cpsr & PSR_MODE_MASK) {
	case USR_MODE: mode = mode_list[0]; break;
	case FIQ_MODE: mode = mode_list[1]; break;
	case IRQ_MODE: mode = mode_list[2]; break;
	case SVC_MODE: mode = mode_list[3]; break;
	case ABT_MODE: mode = mode_list[4]; break;
	case UND_MODE: mode = mode_list[5]; break;
	case SYSTEM_MODE: mode = mode_list[6]; break;
	default: mode = mode_list[7];break;
	}
	em_dump_write("cpsr: 0x%08lx: Flags: %c%c%c%c, "
		      "IRQ: o%s, FIQ: o%s, Thumb: o%s, Mode: %s\n",
		      em_regs->ARM_cpsr,
		      (em_regs->ARM_cpsr & PSR_N_BIT) ? 'N' : 'n',
		      (em_regs->ARM_cpsr & PSR_Z_BIT) ? 'Z' : 'z',
		      (em_regs->ARM_cpsr & PSR_C_BIT) ? 'C' : 'c',
		      (em_regs->ARM_cpsr & PSR_V_BIT) ? 'V' : 'v',
		      (em_regs->ARM_cpsr & PSR_I_BIT) ? "ff" : "n",
		      (em_regs->ARM_cpsr & PSR_F_BIT) ? "ff" : "n",
		      (em_regs->ARM_cpsr & PSR_T_BIT) ? "n" : "ff",
		      mode);
}

void em_dump_regs_detail(int argc, char **argv)
{
	em_dump_regs(1, NULL);
	em_dump_write("\n[cp15 register dump]\n\n");

	/* FIXME: This is ARM926EJ-S specific... */
	{
		unsigned long id, cache, tcm, control, trans,
			dac, d_fsr, i_fsr, far, d_lock, i_lock,
			d_tcm, i_tcm, tlb_lock, fcse, context;
		char size_list[][8] = {
			"  0", "  4", "  8", " 16", " 32", " 64",
			"128", "256", "512", "1024", "???"};
		char *dsiz, *isiz;
		char fault_stat[][32] = {
			"Alignment", "External abort on translation",
			"Translation", "Domain", "Permission",
			"External abort", "???" };
		char *stat;
		char alloc[][8] = {"locked", "opened"};

		asm volatile ("mrc p15, 0, %0, c0, c0, 0" : "=r" (id));
		asm volatile ("mrc p15, 0, %0, c0, c0, 1" : "=r" (cache));
		asm volatile ("mrc p15, 0, %0, c0, c0, 2" : "=r" (tcm));
		asm volatile ("mrc p15, 0, %0, c1, c0, 0" : "=r" (control));
		asm volatile ("mrc p15, 0, %0, c2, c0, 0" : "=r" (trans));
		asm volatile ("mrc p15, 0, %0, c3, c0, 0" : "=r" (dac));
		asm volatile ("mrc p15, 0, %0, c5, c0, 0" : "=r" (d_fsr));
		asm volatile ("mrc p15, 0, %0, c5, c0, 1" : "=r" (i_fsr));
		asm volatile ("mrc p15, 0, %0, c6, c0, 0" : "=r" (far));
		asm volatile ("mrc p15, 0, %0, c9, c0, 0" : "=r" (d_lock));
		asm volatile ("mrc p15, 0, %0, c9, c0, 1" : "=r" (i_lock));
		asm volatile ("mrc p15, 0, %0, c9, c1, 0" : "=r" (d_tcm));
		asm volatile ("mrc p15, 0, %0, c9, c1, 1" : "=r" (i_tcm));
		asm volatile ("mrc p15, 0, %0, c10, c0, 0" : "=r" (tlb_lock));
		asm volatile ("mrc p15, 0, %0, c13, c0, 0" : "=r" (fcse));
		asm volatile ("mrc p15, 0, %0, c13, c0, 1" : "=r" (context));

#define MASK_ASCII  0xff000000
#define MASK_SPEC   0x00f00000
#define MASK_ARCH   0x000f0000
#define MASK_PART   0x0000fff0
#define MASK_LAYOUT 0x0000000f
		em_dump_write("* ID code: %08lx:  "
			      "tm: %c, spec: %1lx, arch: %1lx, "
			      "part: %3lx, layout: %1lx\n",
			      id,
			      (unsigned char)((id & MASK_ASCII)  >> 24),
			      ((id & MASK_SPEC)   >> 20),
			      ((id & MASK_ARCH)   >> 16),
			      ((id & MASK_PART)   >>  4),
			      ((id & MASK_LAYOUT)));

#define MASK_CTYPE 0x1e000000
#define MASK_S_BIT 0x01000000
#define MASK_DSIZE 0x00fff000
#define MASK_ISIZE 0x00000fff
#define MASK_SIZE  0x000003c0
#define MASK_ASSOC 0x00000038
#define MASK_LEN   0x00000003

		switch ((((cache & MASK_DSIZE) >> 12) & MASK_SIZE) >> 6) {
		case 0x3: dsiz = size_list[1]; break;
		case 0x4: dsiz = size_list[2]; break;
		case 0x5: dsiz = size_list[3]; break;
		case 0x6: dsiz = size_list[4]; break;
		case 0x7: dsiz = size_list[5]; break;
		case 0x8: dsiz = size_list[6]; break;
		default:  dsiz = size_list[10]; break;
		}
		switch (((cache & MASK_ISIZE) & MASK_SIZE) >> 6) {
		case 0x3: isiz = size_list[1]; break;
		case 0x4: isiz = size_list[2]; break;
		case 0x5: isiz = size_list[3]; break;
		case 0x6: isiz = size_list[4]; break;
		case 0x7: isiz = size_list[5]; break;
		case 0x8: isiz = size_list[6]; break;
		default:  isiz = size_list[10]; break;
		}
		em_dump_write("* Cache Type: %08lx:  %s, %s,\n"
			      "\tDCache: %sKB, %s-way, line: %s word\n"
			      "\tICache: %sKB, %s-way, line: %s word\n",
			      cache,
			      (((cache & MASK_CTYPE) >> 25) == 0xe) ?
			      "write-back" : "write-???",
			      (cache & MASK_S_BIT) ?
			      "harvard" : "unified",
			      dsiz,
			      ((((cache & MASK_DSIZE) >> 12) & MASK_ASSOC >> 3)
			       == 0x2) ? "4" : "?",
			      ((((cache & MASK_DSIZE) >>12 ) & MASK_LEN)
			       == 0x2) ? "8" : "?",
			      isiz,
			      (((cache & MASK_ISIZE) & MASK_ASSOC >> 3)
			       == 0x2) ? "4" : "?",
			      (((cache & MASK_ISIZE) & MASK_LEN)
			       == 0x2) ? "8" : "?");

#define MASK_DTCM 0x00010000
#define MASK_ITCM 0x00000001
		em_dump_write("* TCM Status: %08lx: "
			      "DTCM %spresent, ITCM %spresent\n",
			      tcm,
			      (tcm & MASK_DTCM) ? "" : "not ",
			      (tcm & MASK_ITCM) ? "" : "not ");

#define MASK_L4     0x00008000
#define MASK_RR     0x00004000
#define MASK_VEC    0x00002000
#define MASK_ICACHE 0x00001000
#define MASK_ROM    0x00000200
#define MASK_SYS    0x00000100
#define MASK_END    0x00000080
#define MASK_DCACHE 0x00000004
#define MASK_ALIGN  0x00000002
#define MASK_MMU    0x00000001
		em_dump_write("* Control: %08lx: L4: %s, Cache: %s replace\n"
			      "\texception vector at %s endian: %s\n"
			      "\tICache %sabled, DCache %sabled, "
			      "Align %sabled, MMU %sabled\n"
			      "\tROM protection: %s, system protection: %s\n",
			      control,
			      (control & MASK_L4) ? "1" : "0",
			      (control & MASK_RR) ? "round robin" : "random",
			      (control & MASK_VEC) ?
			      "ffff00{00-1c}" : "000000{00-1c}",
			      (control & MASK_END) ? "big" : "little",
			      (control & MASK_ICACHE) ? "en" : "dis",
			      (control & MASK_DCACHE) ? "en" : "dis",
			      (control & MASK_ALIGN) ? "en" : "dis",
			      (control & MASK_MMU) ? "en" : "dis",
			      (control & MASK_ROM) ? "1" : "0",
			      (control & MASK_SYS) ? "1" : "0");

		em_dump_write("* Translation Table Base: %08lx\n", trans);
		em_dump_write("* Domain Access Control: %08lx\n", dac);

#define MASK_DOMAIN 0x000000f0
#define MASK_STATUS 0x0000000f
		switch (d_fsr & MASK_STATUS) {
		case 0x1: case 0x3: stat = fault_stat[0]; break;
		case 0xc: case 0xe: stat = fault_stat[1]; break;
		case 0x5: case 0x7: stat = fault_stat[2]; break;
		case 0x9: case 0xb: stat = fault_stat[3]; break;
		case 0xd: case 0xf: stat = fault_stat[4]; break;
		case 0x8: case 0xa: stat = fault_stat[5]; break;
		default:            stat = fault_stat[6]; break;
		}
		em_dump_write("* Fault Status: data: %08lx, inst: %08lx\n"
			      "\tat domain: %lx, status: %s\n",
			      d_fsr, i_fsr,
			      ((d_fsr & MASK_DOMAIN) >> 4), stat);

		em_dump_write("* Fault Address: %08lx\n", far);

#define MASK_WAY3 0x00000008
#define MASK_WAY2 0x00000004
#define MASK_WAY1 0x00000002
#define MASK_WAY0 0x00000001
		em_dump_write("* Cache Lockdown: DCache: %08lx, ICache: %08lx\n"
			      "\tDCache: way 3: %s, 2: %s, 1: %s, 0: %s\n"
			      "\tICache: way 3: %s, 2: %s, 1: %s, 0: %s\n",
			      d_lock, i_lock,
			      (d_lock & MASK_WAY3) ? alloc[0] : alloc[1],
			      (d_lock & MASK_WAY2) ? alloc[0] : alloc[1],
			      (d_lock & MASK_WAY1) ? alloc[0] : alloc[1],
			      (d_lock & MASK_WAY0) ? alloc[0] : alloc[1],
			      (i_lock & MASK_WAY3) ? alloc[0] : alloc[1],
			      (i_lock & MASK_WAY2) ? alloc[0] : alloc[1],
			      (i_lock & MASK_WAY1) ? alloc[0] : alloc[1],
			      (i_lock & MASK_WAY0) ? alloc[0] : alloc[1]);

#define MASK_BASE   0xfffff000
#undef  MASK_SIZE
#define MASK_SIZE   0x0000003c
#define MASK_ENABLE 0x00000001
		switch ((d_tcm & MASK_SIZE) >> 2) {
		case 0x0: dsiz = size_list[0]; break;
		case 0x3: dsiz = size_list[1]; break;
		case 0x4: dsiz = size_list[2]; break;
		case 0x5: dsiz = size_list[3]; break;
		case 0x6: dsiz = size_list[4]; break;
		case 0x7: dsiz = size_list[5]; break;
		case 0x8: dsiz = size_list[6]; break;
		case 0x9: dsiz = size_list[7]; break;
		case 0xa: dsiz = size_list[8]; break;
		case 0xb: dsiz = size_list[9]; break;
		default:  dsiz = size_list[10]; break;
		}
		switch ((i_tcm & MASK_SIZE) >> 2) {
		case 0x0: isiz = size_list[0]; break;
		case 0x3: isiz = size_list[1]; break;
		case 0x4: isiz = size_list[2]; break;
		case 0x5: isiz = size_list[3]; break;
		case 0x6: isiz = size_list[4]; break;
		case 0x7: isiz = size_list[5]; break;
		case 0x8: isiz = size_list[6]; break;
		case 0x9: isiz = size_list[7]; break;
		case 0xa: isiz = size_list[8]; break;
		case 0xb: isiz = size_list[9]; break;
		default:  isiz = size_list[10]; break;
		}
		em_dump_write("* TCM Region: data: %08lx, inst: %08lx\n"
			      "\tDTCM: Base addr: %08lx, size: %sKB, %sabled\n"
			      "\tITCM: Base addr: %08lx, size: %sKB, %sabled\n",
			      d_tcm, i_tcm,
			      ((d_tcm & MASK_BASE) >> 12), dsiz,
			      (d_tcm & MASK_ENABLE) ? "en" : "dis",
			      ((i_tcm & MASK_BASE) >> 12), isiz,
			      (i_tcm & MASK_ENABLE) ? "en" : "dis");

#define MASK_VICT 0x1c000000
#define MASK_PBIT 0x00000001
		em_dump_write("* TLB Lockdown: %08lx: "
			      "victim: %lx, preserve: %s\n",
			      tlb_lock,
			      ((tlb_lock & MASK_VICT) >> 26),
			      (tlb_lock & MASK_PBIT) ? alloc[0] : alloc[1]);

#define MASK_FCSE 0xfe000000
		em_dump_write("* FCSE PID: %08lx: pid: %lx\n",
			      fcse, ((fcse & MASK_FCSE) >> 25));

		em_dump_write("* Context ID: %08lx\n", context);
	}
	em_dump_write("\n");
}

static void em_dump_till_end_of_page(unsigned long *sp, unsigned long *start)
{
	unsigned long *tail = sp;
	unsigned long *actual_sp = sp;
	unsigned long stackdata;
	int i = 0;
	char buf[17];

	tail = (unsigned long *)(((unsigned long)sp + PAGE_SIZE - 1) & PAGE_MASK);

#define MIN_STACK_LEN 2048
	if (((unsigned long)tail - (unsigned long)sp) < MIN_STACK_LEN)
		tail = (unsigned long *)((unsigned long) sp + MIN_STACK_LEN);

	buf[16] = 0;
	sp = start;
	while (sp < tail) {

		if ((i % 4) == 0) {
			em_dump_write("%p : ", sp);
		}
		if (__get_user(stackdata, sp++)) {
			em_dump_write(" (Bad stack address)\n");
			if (sp <= actual_sp) {
				/* retry to dump from the actual sp */
				sp = actual_sp;
				continue;
			}
			break;
		}
		em_dump_write(" 0x%08lx", stackdata);
		buf[(i % 4) * 4]     = em_convert_char(stackdata);
		buf[(i % 4) * 4 + 1] = em_convert_char(stackdata >> 8);
		buf[(i % 4) * 4 + 2] = em_convert_char(stackdata >> 16);
		buf[(i % 4) * 4 + 3] = em_convert_char(stackdata >> 24);

		if ((i % 4) == 3) {
			em_dump_write(" : %s\n", buf);
		}

		++i;
	}
}

void em_dump_stack(int argc, char **argv)
{
	unsigned long *sp = (unsigned long *)(em_regs->ARM_sp & ~0x03);
	unsigned long *fp = (unsigned long *)(em_regs->ARM_fp & ~0x03);
	unsigned long *actual_sp = sp;
	unsigned long *tail;
	unsigned long backchain;
	unsigned long stackdata;
	int frame = 1;

	tail = sp + PAGE_SIZE / sizeof(unsigned long*);

	em_dump_write("\n[stack dump]\n");

	backchain = arch_stack_pointer(fp);
	/* to dump lower address than sp */
	if (sp > (unsigned long *)LOWER_ADDR_DUMP_SIZE)
		sp -= LOWER_ADDR_DUMP_SIZE / sizeof(unsigned long*);
	while (sp < tail) {
		if (backchain == (unsigned long)sp) {
			em_dump_write("|");
			fp = arch_prev_frame(fp);
			if (!fp)
				break;

			backchain = arch_stack_pointer(fp);
			if (!backchain)
				break;
		} else {
			em_dump_write(" ");
		}

		if (backchain < (unsigned long)sp) {
			break;
		}

		if (__get_user(stackdata, sp)) {
			em_dump_write("\n (bad stack address)\n");
			if (sp < actual_sp) {
				/* retry to dump from the actual sp */
				sp = actual_sp;
				continue;
			}
			break;
		}

		if (((unsigned long)tail-(unsigned long)sp) % 0x10 == 0) {
			if (frame) {
				em_dump_write("\n0x%p:|", sp);
				frame = 0;
			} else {
				em_dump_write("\n0x%p: ", sp);
			}
		}

		em_dump_write("0x%08lx", stackdata);

		sp++;
	}

	em_dump_write("\n");

	em_dump_write("\n #################em_dump_till_end_of_page###########\n");
	em_dump_till_end_of_page(actual_sp, sp);
	em_dump_write("\n");
}

static struct fsr_info {
	const char *name;
} fsr_info[] = {
	/*
	 * The following are the standard ARMv3 and ARMv4 aborts.  ARMv5
	 * defines these to be "precise" aborts.
	 */
	{ "vector exception"		   },
	{ "alignment exception"		   },
	{ "terminal exception"		   },
	{ "alignment exception"		   },
	{ "external abort on linefetch"	   },
	{ "section translation fault"	   },
	{ "external abort on linefetch"	   },
	{ "page translation fault"	   },
	{ "external abort on non-linefetch" },
	{ "section domain fault"		   },
	{ "external abort on non-linefetch" },
	{ "page domain fault"		   },
	{ "external abort on translation"   },
	{ "section permission fault"	   },
	{ "external abort on translation"   },
	{ "page permission fault"	   },
	/*
	 * The following are "imprecise" aborts, which are signalled by bit
	 * 10 of the FSR, and may not be recoverable.  These are only
	 * supported if the CPU abort handler supports bit 10.
	 */
	{ "unknown 16"			   },
	{ "unknown 17"			   },
	{ "unknown 18"			   },
	{ "unknown 19"			   },
	{ "lock abort"			   }, /* xscale */
	{ "unknown 21"			   },
	{ "imprecise external abort"	   }, /* xscale */
	{ "unknown 23"			   },
	{ "dcache parity error"		   }, /* xscale */
	{ "unknown 25"			   },
	{ "unknown 26"			   },
	{ "unknown 27"			   },
	{ "unknown 28"			   },
	{ "unknown 29"			   },
	{ "unknown 30"			   },
	{ "unknown 31"			   }
};

void em_show_syndrome(void)
{
	unsigned long fsr,far;
	const struct fsr_info *inf;
	struct task_struct *tsk = current;

	em_dump_write("\n\n[Exception Syndrome]\n");

	switch(tsk->thread.trap_no){
	case 6:
		em_dump_write("Illeagle Instruction at 0x%08lx\n",
			      em_regs->ARM_pc);

		break;
	case 14:
	default:
		fsr = tsk->thread.error_code;
		far = tsk->thread.address;
		inf = fsr_info + (fsr & 15) + ((fsr & (1 << 10)) >> 6);

		em_dump_write("%s (0x%03lx) at 0x%08lx\n",
			      inf->name, fsr, far);

		break;
	}
}

#ifdef CONFIG_SNSC_ALT_BACKTRACE
static void em_print_symbol(const char *str)
{
#ifdef CONFIG_SNSC_EM_DEMANGLE
	char *demangle = cplus_demangle_v3(str, demangle_flag);
	if (demangle) {
		em_dump_write("%s", demangle);
		kfree(demangle);
		return;
	}
#endif
	em_dump_write("%s", str);
}

/* variables needed during disasseble of instruction  */
static unsigned long func_start, exception_addr;
static bool copy_addr;

void em_arch_reset_cache(void)
{
	func_start = exception_addr = 0;
	copy_addr = 0;
}

#ifdef CONFIG_SNSC_EM_DISASSEMBLE
extern int disasm_command;
/* function to print only insns and not the disasm information */
static void dump_only_insns(int disasm_size, unsigned long **disasm_point)
{
	unsigned long insn;
	int i = 0;
	unsigned long *point = (unsigned long *)*disasm_point;
	point -= 8;

	for (i=0; i<disasm_size; i++) {
		if (__get_user(insn, point)) {
			point++;
			continue;
		}

		/* ARM instructions */
		if (!(em_regs->ARM_cpsr & PSR_T_BIT)) {
			em_dump_write("0x%p:\t%08lx \n", point, insn);
		}
		else { /* Thumb2 instructions */
			unsigned long tmp_insn = insn & 0xffff;
			em_dump_write("0x%p:\t%04lx ", point, tmp_insn);
			tmp_insn = ((insn >> 16)& 0xffff);
			em_dump_write("%04lx\n", tmp_insn);
		}
		point++;
	}

	disasm_command = 1;
	*disasm_point = point;
}

void em_disasm_arm(int argc, char **argv, int *disasm_size, unsigned long **disasm_point)
{
	unsigned long insn;
	int size = *disasm_size;
	unsigned long *point = (unsigned long *)*disasm_point;
	int i;
	unsigned long *end_addr;
	int extra_print_size = 0;

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

	/* called from EM normal flow and not from exception prompt */
	if(disasm_command == 0) {
		if((unsigned long)point <= USER_DS) {
			/* codition where retreiving addsymlist info failed */
			if ((func_start == 0) || (exception_addr == 0)) {
				dump_only_insns(*disasm_size, disasm_point);
				return;
			}
			point = (unsigned long *)func_start;
			size = ((exception_addr - func_start)+(sizeof(unsigned long))-1) / (sizeof(unsigned long));
		}
		else {
			/* called for kernel exception, start addr should be 8 instr before pc
			   we just moved the logic from em_dump_exception to here */
			point -= 8;
		}
	}
	else {
		/* No need to get the alligned adderss for ARM case, as it is always 4 byte alligned */
		if(em_regs->ARM_cpsr & PSR_T_BIT)
			while (1) {
				if (__get_user(insn, point--)) {
					em_dump_write("(bad data address)\n");
					point++;
					break;
				}
				else {
					/* to get the proper starting address for thumb2 instruction */
					if((insn & 0xF800) == 0xF800 ||
					  (insn & 0xF800) == 0xF000 ||
					  (insn & 0xF800) == 0xE800) {
						insn = ((insn >> 16)& 0xffff);
						if((insn & 0xF800) == 0xF800 ||
						  (insn & 0xF800) == 0xF000 ||
						  (insn & 0xF800) == 0xE800){
							continue;
						}
						else {
							point = (unsigned long *)((unsigned long)point+2);
							break;
						}
					}
					else {
						point++;
						break;
					}
				}
			}
	}

	end_addr = point + size;
	/* disassemble size should not be more than 16, */
	if(disasm_command == 0) {
		if (size < *disasm_size) {
			extra_print_size = (*disasm_size - size)/2;
			point = point - extra_print_size;
			for (i=0; i<extra_print_size; i++) {
				if (__get_user(insn, point)) {
					point++;
					continue;
				}

				/* ARM instructions */
				if (!(em_regs->ARM_cpsr & PSR_T_BIT)) {
					em_dump_write("0x%p:\t%08lx \n", point, insn);
				}
				else { /* Thumb2 instructions */
					unsigned long tmp_insn = insn & 0xffff;
					em_dump_write("0x%p:\t%04lx ", point, tmp_insn);
					tmp_insn = ((insn >> 16)& 0xffff);
					em_dump_write("%04lx\n", tmp_insn);
				}
				point++;
			}
		}
		size = *disasm_size;

		while ((end_addr - point) > *disasm_size) {
			if (__get_user(insn, point++))
				continue;
			if (!(em_regs->ARM_cpsr & PSR_T_BIT)) {
				point = end_addr - *disasm_size;
				break;
			}
			else {
				unsigned long tmp_insn;
				tmp_insn = ((insn >> 16)& 0xffff);

				if ((tmp_insn & 0xF800) == 0xF800 ||
				    (tmp_insn & 0xF800) == 0xF000 ||
				    (tmp_insn & 0xF800) == 0xE800)
					point = (unsigned long *)((unsigned long)point-2);
			}
		}
	}

	while (point <= end_addr) {
		em_dump_write("0x%p:\t", point);
		if (__get_user(insn, point)) {
			em_dump_write("(bad data address)\n");
			point++;
			continue;
		}
		if (print_insn(NULL, insn, (unsigned)point++) == 0xFF)
			point = (unsigned long *)((unsigned long)point-2);
	}
	*disasm_point = point;
	*disasm_size = size;

	if (disasm_command == 1)
		em_dump_write("disassembly may display invalid information sometimes !!! \n");
	else{
		disasm_command = 1;
		if (extra_print_size) {
			for (i=0; i<extra_print_size; i++) {
				if (__get_user(insn, point)) {
					point++;
					continue;
				}

				/* ARM instructions */
				if (!(em_regs->ARM_cpsr & PSR_T_BIT)) {
					em_dump_write("0x%p:\t%08lx \n", point, insn);
				}
				else { /* Thumb2 instructions */
					unsigned long tmp_insn = insn & 0xffff;
					em_dump_write("0x%p:\t%04lx ", point, tmp_insn);
					tmp_insn = ((insn >> 16)& 0xffff);
					em_dump_write("%04lx\n", tmp_insn);
				}
				point++;
			}
		}
	}
}
#endif

int em_bt_ustack_callback(struct bt_arch_callback_arg *cbarg, void *user)
{
	em_dump_write("[0x%08lx] ", cbarg->ba_addr);
	if (bt_status_is_error(cbarg->ba_status)) {
		em_dump_write("stop backtracing: %s\n", cbarg->ba_str);

		/* if PC is invalid/zero, unwind/disassemble may tried with the valid LR.
		   CPSR bit need to be adjusted as per the LR register
		   if return addr is thumb2, last bit of LR is always 1
		   So set the T bit in cpsr regsiter. */
		if (copy_addr == 0){
			if(em_regs->ARM_lr & 1)
				em_regs->ARM_cpsr |= PSR_T_BIT;
			else
				em_regs->ARM_cpsr &= ~PSR_T_BIT;
		}
		return 0;
	}

	/* copying the func start addr and pc, needed during disassebly
	   disassembly need func start address as a starting point to avoid arm/thumb2 confusion */
	if (copy_addr == 0) {
		unsigned int insn_size = 4;
		if(em_regs->ARM_cpsr & PSR_T_BIT)
			insn_size = 2;
		exception_addr = (cbarg->ba_addr/insn_size) * insn_size;
		func_start = (cbarg->ba_sym_start/insn_size) * insn_size;
		copy_addr = 1;
	}

	if (cbarg->ba_str[0]) {
		em_print_symbol(cbarg->ba_str);
	}
	/* condition for missing addsymlist information */
	else if (cbarg->ba_sym_start == 0) {
		em_dump_write("stripped (%s +%#lx) (%s hash:00000000)\n",
			      bt_file_name(cbarg->ba_file),
			      cbarg->ba_addr - cbarg->ba_adjust,
			      bt_file_name(cbarg->ba_file));
		return 0;
	}
	else {
		em_dump_write("0x%08lx", cbarg->ba_sym_start);
	}
	if (bt_hash_valid(cbarg)) {
		/* by symlist section */
		const unsigned char *hash = (unsigned char *)cbarg->ba_hash;
		em_dump_write("+%#lx (%s hash:%02x%02x%02x%02x adj:%ld)\n",
			      cbarg->ba_addr - cbarg->ba_sym_start,
			      bt_file_name(cbarg->ba_file),
			      hash[0], hash[1], hash[2], hash[3],
			      cbarg->ba_adjust);
	}
	else {
		/* by symtab section */
		em_dump_write("+%#lx/%#lx (%s +%#lx)\n",
			      cbarg->ba_addr - cbarg->ba_sym_start,
			      cbarg->ba_sym_size,
			      bt_file_name(cbarg->ba_file),
                  cbarg->ba_addr - cbarg->ba_adjust);
	}
	return 0;
}

static void em_dump_callstack_ustack(const char *mode)
{
	/* cant pass em_regs here, it contains kernel register values during kernel exception */
	struct pt_regs *usr_regs = task_pt_regs(current);

	em_dump_write(
		"\na1: r0: 0x%08lx  a2: r1: 0x%08lx  a3: r2: 0x%08lx  a4: r3: 0x%08lx\n",
		usr_regs->ARM_r0, usr_regs->ARM_r1,
		usr_regs->ARM_r2, usr_regs->ARM_r3);
	em_dump_write(
		"v1: r4: 0x%08lx  v2: r5: 0x%08lx  v3: r6: 0x%08lx  v4: r7: 0x%08lx\n",
		usr_regs->ARM_r4, usr_regs->ARM_r5,
		usr_regs->ARM_r6, usr_regs->ARM_r7);
	em_dump_write(
		"v5: r8: 0x%08lx  v6: r9: 0x%08lx  v7:r10: 0x%08lx  fp:r11: 0x%08lx\n",
		usr_regs->ARM_r8, usr_regs->ARM_r9,
		usr_regs->ARM_r10, usr_regs->ARM_fp);
	em_dump_write(
		"ip:r12: 0x%08lx  sp:r13: 0x%08lx  lr:r14: 0x%08lx  pc:r15: 0x%08lx\n",
		usr_regs->ARM_ip, usr_regs->ARM_sp,
		usr_regs->ARM_lr, usr_regs->ARM_pc);
	em_dump_write("cpsr:r16: 0x%08lx\n", usr_regs->ARM_cpsr);

	em_dump_write("\n[call stack (ustack)]\n");
	bt_ustack(mode, !not_interrupt, usr_regs, em_bt_ustack_callback, NULL);
	em_dump_write("\n");
}

static void em_dump_callstack_kstack(const char *mode)
{
	em_dump_write("\n[call stack (kstack)]\n");
	bt_kstack_current(mode, em_dump_write, NULL);
	em_dump_write("\n");
}

static void em_dump_callstack_kstack_regs(const char *mode)
{
	em_dump_write("\n[call stack (kstack_regs)]\n");
	bt_kstack_regs(current, em_regs, em_dump_write, NULL, 1);
	em_dump_write("\n");
}
#endif

static int em_is_param_char(char c)
{
	return isalnum(c) || c == '_';
}

static const char *em_param_match(const char *param, const char *name)
{
	const char *from = strstr(param, name);
	const char *to;

	if (from == NULL)
		return NULL;
	if (from > param && em_is_param_char(from[-1])) /* suffix match */
		return NULL;
	to = from + strlen(name);
	if (em_is_param_char(*to))
		return NULL; /* prefix match */
	return to;
}

static int em_callstack_mode(const char *mode)
{
	int count = 0;
#ifdef CONFIG_SNSC_ALT_BACKTRACE
	if (em_param_match(mode, "kstack")) {
		em_dump_callstack_kstack(mode);
		count++;
	}
	if (!not_interrupt && em_param_match(mode, "kstack_regs")) {
		em_dump_callstack_kstack_regs(mode);
		count++;
	}
	if (current->mm && em_param_match(mode, "ustack")) {
		em_dump_callstack_ustack(mode);
		count++;
	}
#endif
	return count;
}

void em_dump_callstack(int argc, char **argv)
{
	int count;
	if (!argv || argc <= 1)
		count = em_callstack_mode(em_callstack_param);
	else
		count = em_callstack_mode(argv[1]);
	if (count == 0)
		em_dump_write("\n[call stack]\nno callstack selected\n\n");
}

#define BT_PARAM_LEN 128
char bt_param[BT_PARAM_LEN] = "kstack";

#define PROC_PARAM_NAME            "backtrace"
#define PROC_UIF_NAME              "bt"

int __init bt_setup(char *str)
{
	/* Rely on implicit final '\0' for static data */
	strncpy(bt_param, str, BT_PARAM_LEN - 1);
	return 0;
}
__setup("backtrace=", bt_setup);

static int btparam_show(struct seq_file *m, void *v)
{
	return seq_printf(m, "%s\n", bt_param);
}

static ssize_t btparam_write(struct file *file, const char __user *buffer,
							 size_t count, loff_t *ppos)
{
	if (count > BT_PARAM_LEN - 1)
		return -EINVAL;
	if (copy_from_user(bt_param, buffer, count))
		return -EFAULT;
	bt_param[count] = '\0';
	return count;
}

static int btparam_file_open(struct inode *inode, struct file *file)
{
	return single_open(file, btparam_show, NULL);
}

static void *btuif_seq_start(struct seq_file *m, loff_t *pos)
{
	return 0;
}

static void btuif_seq_stop(struct seq_file *m, void *p)
{
}

static void *btuif_seq_next(struct seq_file *m, void *p, loff_t *pos)
{
	return 0;
}

static int btuif_seq_show(struct seq_file *m, void *v)
{
	return 0;
}

static struct seq_operations btuif_seq_op = {
	.start	= btuif_seq_start,
	.next	= btuif_seq_next,
	.stop	= btuif_seq_stop,
	.show	= btuif_seq_show
};

static int btuif_file_open(struct inode *inode, struct file *file)
{
	struct bt_session *priv;
	struct seq_file *sfile;
	int ret;
	priv = kmalloc(sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;
	ret = seq_open(file, &btuif_seq_op);
	if (ret) {
		kfree(priv);
		return ret;
	}
	bt_init_session(priv, bt_param, GFP_KERNEL);
	sfile = file->private_data;
	sfile->private = priv;
	return ret;
}

static int btuif_file_release(struct inode *inode, struct file *file)
{
	struct seq_file *seq = file->private_data;
	struct bt_session *priv = seq->private;
	bt_release_session(priv);
	return seq_release_private(inode, file);
}

static int btuif_ioctl_addrs(struct bt_session *session,
			     struct bt_ioctl_addrs __user *u_arg)
{
	struct bt_ioctl_addrs arg;

	if (copy_from_user(&arg, u_arg, sizeof(arg))) {
		return -EFAULT;
	}
	return bt_ustack_user(session, arg.ba_size, arg.ba_buf, arg.ba_skip_addr);
}

/* return 0 for success, 1 for failure. */
/* number pointed to by cur is incremented by the length written, */
/* excluding terminating '\0', dest always null terminated. */
static int bt_copy_str_to_user(char __user *u_dest, const char *src,
			       size_t max, int *cur)
{
	size_t len = strnlen(src, max - 1);
	if (copy_to_user(u_dest, src, len))
		return 1;
	if (put_user(0, u_dest + len))
		return 1;
	*cur += len;
	return 0;
}

static int btuif_ioctl_symbol(struct bt_session *session,
			      struct bt_ioctl_symbol __user *u_arg)
{
	struct bt_ioctl_symbol arg;
	int ret, cur;
	char buf[256];
	int buflen = sizeof(buf);
	struct bt_arch_callback_arg cbarg;
	const char *libname;
	struct bt_elf_cache *ep = NULL;

	if (copy_from_user(&arg, u_arg, sizeof(arg))) {
		return -EFAULT;
	}
	if (arg.bs_size == 0)
		return 0;
	ret = bt_find_symbol((unsigned long)arg.bs_addr, session,
			     buf, buflen, &cbarg, &ep);
	if (ret < 0) {
		/* no symbol found, return "" */
		put_user(0, arg.bs_buf);
		return 1;
	}
	cur = 0;
	if (cbarg.ba_file) {
		libname = bt_file_name(cbarg.ba_file);
		if (bt_copy_str_to_user(arg.bs_buf, libname, arg.bs_size, &cur))
			return -EFAULT;
	}
	if (bt_copy_str_to_user(arg.bs_buf + cur, "(", arg.bs_size - cur, &cur))
		return -EFAULT;
	if (bt_copy_str_to_user(arg.bs_buf + cur, buf, arg.bs_size - cur, &cur))
		return -EFAULT;
	snprintf(buf, buflen, "+0x%lx", (unsigned long)arg.bs_addr
		                      - cbarg.ba_sym_start);
	if (bt_copy_str_to_user(arg.bs_buf + cur, buf, arg.bs_size - cur, &cur))
		return -EFAULT;
	if (bt_copy_str_to_user(arg.bs_buf + cur, ")", arg.bs_size - cur, &cur))
		return -EFAULT;
	return cur + 1;
}


static long btuif_ioctl(struct file *filp,
			unsigned int num, unsigned long arg)
{
	struct seq_file *seq = filp->private_data;
	struct bt_session *priv = seq->private;

	switch (num) {
	case BT_IOCTL_ADDRS:
		return btuif_ioctl_addrs(priv, (struct bt_ioctl_addrs
					       __user *)arg);
	case BT_IOCTL_SYMBOL:
		return btuif_ioctl_symbol(priv, (struct bt_ioctl_symbol
						 __user *)arg);
	default:
		return -EINVAL;
	}
}

static const struct file_operations btparam_file_operations = {
	.owner	 = THIS_MODULE,
	.open	 = btparam_file_open,
	.read	 = seq_read,
	.write	 = btparam_write,
	.llseek	 = seq_lseek,
	.release = seq_release
};

static const struct file_operations btuif_file_operations = {
	.open            = btuif_file_open,
	.read            = seq_read,
	.unlocked_ioctl  = btuif_ioctl,
	.release         = btuif_file_release,
};

int __init em_arch_init(void)
{
	struct proc_dir_entry *btuif_entry;
	struct proc_dir_entry *proc_bt = NULL;

	proc_bt = proc_create(PROC_PARAM_NAME, 0600, NULL, &btparam_file_operations);
	if (proc_bt == NULL) {
		printk("cannot create proc entry for %s\n", PROC_PARAM_NAME);
		return 1;
	}

	btuif_entry = proc_create(PROC_UIF_NAME, 0600, NULL, &btuif_file_operations);
	return 0;
}

int em_arch_get_wdt_status(void)
{
#ifdef CONFIG_BERLIN_SM
	return sm_asserted_watchdog();
#else
#warning always returns 0 as wdt status.
	return 0;
#endif
}

void em_arch_reset_wdt(void)
{
#ifdef CONFIG_SNSC_EM_RESET_WATCHDOG
#ifdef CONFIG_BERLIN_SM
	sm_reset_watchdog(CONFIG_SNSC_EM_RESET_WATCHDOG_DEADLINE);
#else
#warning no reset function is implemented.
#endif
#endif
}

void em_arch_reboot(void)
{
	printk("requesting reboot...\n");
#ifdef CONFIG_BERLIN_SM
	sm_request_reboot();
#else
#warning no reboot function is implemented.
#endif
}


const char *bt_param_match(const char *param, const char *name)
{
	const char *from = strstr(param, name);
	const char *to;

	if (from == NULL)
		return NULL;
	if (from > param && isalnum(from[-1])) /* suffix match */
		return NULL;
	to = from + strlen(name);
	if (isalnum(*to))
		return NULL; /* prefix match */
	return to;
}

int bt_ustack_enabled(void)
{
	return (int)bt_param_match(bt_param, "ustack");
}


void em_arch_exit(void)
{
    remove_proc_entry(PROC_UIF_NAME, NULL);
    remove_proc_entry(PROC_PARAM_NAME, NULL);
}
