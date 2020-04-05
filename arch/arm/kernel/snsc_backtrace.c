/*
 * arch/arm/kernel/snsc_backtrace.c
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

/* #define DEBUG */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/list.h>
#include <linux/mm.h>
#include <linux/mman.h>
#include <linux/fs.h>
#include <linux/file.h>
#include <linux/snsc_backtrace.h>
#include <linux/kallsyms.h>

#include <asm/stacktrace.h>
#include <asm/traps.h>
#include <asm/unwind.h>
#include <asm/unwind_internal.h>
#include <asm/unaligned.h>
#include <asm/uaccess.h>
#include <asm/mmu_context.h>


extern int unwind_frame_user(struct stackframe *frame, struct bt_elf_cache *ep, struct unwind_ctrl_block *pctrl);

/* find a backing file for specified addr */
/* caller need to do fput() for returned file */

static int search_current_map_exec_addr(const char *needle, struct vm_area_struct **rvm);

struct file *bt_get_mapped_file(unsigned long addr, unsigned long *base)
{
	struct vm_area_struct *vma = NULL;
	char fname[256];

	down_read(&current->mm->mmap_sem);
	vma = find_vma(current->mm, addr);
	if (!vma
	    || addr < vma->vm_start
	    || (vma->vm_flags & (VM_READ | VM_EXEC)) != (VM_READ | VM_EXEC)
	    || vma->vm_file == NULL) {
		up_read(&current->mm->mmap_sem);
		return NULL;
	}
	*base = 0;
	strncpy(fname, bt_file_name(vma->vm_file), 256);
	fname[255] = 0;
	vma = NULL;
	while(search_current_map_exec_addr(fname, &vma)) {
		if(*base == 0 || *base > vma->vm_start)
			*base = vma->vm_start;
	}
	get_file(vma->vm_file);
	up_read(&current->mm->mmap_sem);
	return vma->vm_file;
}

const char *bt_file_name(struct file *filp)
{
	if (filp && filp->f_path.dentry)
		return filp->f_path.dentry->d_name.name;
	else
		return "<noname>";
}
EXPORT_SYMBOL(bt_file_name);

/*
 * mmap with non-page-aligned offset.  Return similarly unaligned
 * address.
 */
void *bt_mmap(struct file *filp, unsigned long offset, unsigned long len)
{
	unsigned long offset_in_page = offset & ~PAGE_MASK;
	unsigned long paged_offset = offset & PAGE_MASK;
	unsigned long ret;

	/* len alignup will be done by do_mmap() */
	ret = vm_mmap(filp, 0, len + offset_in_page,
		      PROT_READ, MAP_PRIVATE, paged_offset);
	if (IS_ERR_VALUE(ret))
		return (void *)ret;
	return (void *)(ret + offset_in_page);
}

int bt_munmap(const void *addr, unsigned long len)
{
	unsigned long offset_in_page = (unsigned long)addr & ~PAGE_MASK;
	unsigned long paged_addr = (unsigned long)addr & PAGE_MASK;

	return vm_munmap(paged_addr, len + offset_in_page);
}

/*
 * bt_calc_adjust():
 *
 * Code sections may be moved by some post-processing (e.g. prelink's
 * REL->RELA conversion) after addsymlist, obsoleting offsets recorded
 * in addrlist.  This routine computs the amount of the move and store
 * it to ep->be_adjust.
 *
 * This routine assumes that first entry of symbol offsets in
 * .snsc_addrlist section points to the beginning of the first
 * PROGBITS section other than .interp at addsymlist time.  The
 * routine recalculates the value and compare it with the recorded
 * value.
 */
static void bt_calc_adjust(struct bt_elf_cache *ep)
{
	unsigned long first_entry, orig_progbits_vaddr;
	struct addrlist_header *p_header =
		(struct addrlist_header *)ep->be_addrlist;
	unsigned long *addrlist = (unsigned long *)(p_header + 1);
	long len  = (ep->be_addrlist_size
		     - sizeof(struct addrlist_header))
		     / sizeof(unsigned long);
	if (len <= 0)
		return;
	first_entry = bt_get_user32(addrlist);
	orig_progbits_vaddr = ep->be_vaddr + first_entry;
	ep->be_adjust = ep->be_progbits_vaddr - orig_progbits_vaddr;
}

void bt_release_elf_cache(struct bt_elf_cache *ep)
{
	if (ep->be_shdr)
		bt_munmap(ep->be_shdr, sizeof(Elf_Shdr) * ep->be_ehdr.e_shnum);
	if (ep->be_shstrtab)
		bt_munmap(ep->be_shstrtab, ep->be_shstrtab_size);
	if (ep->be_strtab)
		bt_munmap(ep->be_strtab, ep->be_strtab_size);
	if (ep->be_symtab)
		bt_munmap(ep->be_symtab, ep->be_symtab_size);
	if (ep->be_addrlist)
		bt_munmap(ep->be_addrlist, ep->be_addrlist_size);
	if (ep->be_symlist)
		bt_munmap(ep->be_symlist, ep->be_symlist_size);
	fput(ep->be_filp);
}

int bt_map_section(struct bt_elf_cache *ep, int idx, void **paddr,
		   unsigned long *psize)
{
	unsigned long size;
	void *addr;
	Elf_Shdr *shdr;
	unsigned long sh_offset;

	shdr = ep->be_shdr + idx;
	size = bt_get_user32((unsigned long *)&shdr->sh_size);
	sh_offset = bt_get_user32((unsigned long *)&shdr->sh_offset);
	addr = bt_mmap(ep->be_filp, sh_offset, size);
	if (IS_ERR(addr)) {
		pr_debug("backtrace: mmap fail(%ld): %s\n",
		       PTR_ERR(addr), bt_file_name(ep->be_filp));
		return PTR_ERR(addr);
	}
	*paddr = addr;
	*psize = size;

	return 0;
}

static int search_current_map_exec_addr(const char *needle, struct vm_area_struct **rvm)
{
	struct vm_area_struct* vm = NULL;
	if (*rvm == NULL)
		vm = current->mm->mmap;
	else
		vm = (*rvm)->vm_next;

	for (; vm; vm = vm->vm_next) {
		if (vm->vm_file && vm->vm_file->f_dentry &&
		    vm->vm_file->f_dentry->d_name.name) {
			if(strncmp(needle, vm->vm_file->f_dentry->d_name.name, strlen(needle))==0) {
				if(vm->vm_flags & VM_EXEC) {
					*rvm = vm;
					return 1;
				}
			}
		}
	}
	return 0;
}

#ifndef PT_ARM_EXIDX
#define PT_ARM_EXIDX 0x70000001
#endif

int bt_init_elf_cache(struct bt_session *session,
		      struct bt_elf_cache *ep, struct file *filp,
		      unsigned long base)
{
	int ret;
	struct elfhdr *ehdr = &ep->be_ehdr;
	struct elf_phdr phdr;
	int i, idx_strtab, idx_symtab, idx_symlist, idx_addrlist, idx_exidx;
	unsigned long offset;
	int progbits_found = 0;

	memset(ep, 0, sizeof(*ep));
	/* checking elf header */
	ep->be_filp = filp;
	ret = kernel_read(filp, 0, (char *)ehdr, sizeof(*ehdr));
	if (ret != sizeof(*ehdr)) {
		pr_debug("backtrace: too small elf: %s\n",
		       bt_file_name(filp));
		return -EINVAL;
	}
	if (memcmp(ehdr->e_ident, ELFMAG, SELFMAG) != 0) {
		pr_debug("backtrace: not elf: %s\n",
		       bt_file_name(filp));
		return -EINVAL;
	}
	if (ehdr->e_shoff == 0 || ehdr->e_shstrndx == 0) {
		pr_debug("backtrace: no section info in elf: %s\n",
		       bt_file_name(filp));
		return -EINVAL;
	}
	offset = ehdr->e_phoff;
	for (i = 0; i < ehdr->e_phnum; ++i, offset += sizeof(phdr)) {
		ret = kernel_read(filp, offset, (char *)&phdr, sizeof(phdr));
		if (ret != sizeof(phdr)) {
			pr_debug("backtrace: too small elf: %s\n",
				 bt_file_name(filp));
			return -EINVAL;
		}
		if (phdr.p_type == PT_LOAD) { /* first PT_LOAD segment */
			ep->be_vaddr = phdr.p_vaddr;
			break;
		}
		if (phdr.p_type == PT_ARM_EXIDX) { /* PT_ARM_EXIDX comes first */
			ep->be_exidx_base = phdr.p_vaddr;
		}
	}
	ep->be_base = base;

	/* reading section header */
	ep->be_shdr = (Elf_Shdr *)bt_mmap(filp, ehdr->e_shoff,
					  sizeof(Elf_Shdr) * ehdr->e_shnum);
	if (IS_ERR(ep->be_shdr)) {
		pr_debug("backtrace: mmap fail: %p\n", ep->be_shdr);
		return PTR_ERR(ep->be_shdr);
	}
	ret = bt_map_section(ep, ehdr->e_shstrndx, (void **)&ep->be_shstrtab,
			     &ep->be_shstrtab_size);
	if (ret < 0)
		goto err_out;

	/* checking section names */
	idx_strtab = SHN_UNDEF;
	idx_symtab = SHN_UNDEF;
	idx_symlist = SHN_UNDEF;
	idx_addrlist = SHN_UNDEF;
	idx_exidx = SHN_UNDEF;
	for (i = 1; i < ehdr->e_shnum; ++i) { /* skip 0 (SHN_UNDEF) */
		Elf_Shdr *shdr = ep->be_shdr + i;
		char name[32];
		int ret;
		int sh_name = bt_get_user32((unsigned long *)&shdr->sh_name);
		pr_debug("i:%d name:%d (%p) @ %p\n", i, sh_name, &(shdr->sh_name), ep->be_shstrtab + sh_name);
		if (sh_name == 0)
			continue;
		ret = copy_from_user(name, ep->be_shstrtab + sh_name, 32);
		name[31] = '\0';
		pr_debug("i:%d name:%s (%p)\n", i, name, &(shdr->sh_name));
		if (strcmp(name, ".symtab") == 0)
			idx_symtab = i;
		else if (strcmp(name, ".strtab") == 0)
			idx_strtab = i;
/*
 *  [13] .ARM.extab        PROGBITS        00032e0c 032e0c 001888 00   A  0   0  4
 *  [14] .ARM.exidx        ARM_EXIDX       00034694 034694 001168 00  AL 10   0  4
 */
		else if (strcmp(name, ".ARM.exidx") == 0) {
			idx_exidx = i;
		}
		else if (strcmp(name, ".debug.snsc_sym") == 0)
			idx_symlist = i;
		else if (strcmp(name, ".snsc_addrlist") == 0)
			idx_addrlist = i;
		if (bt_get_user32((unsigned long *)&shdr->sh_type) == SHT_PROGBITS
		    && !progbits_found
		    && strcmp(name, ".interp")) {
			/* first non-.interp progbits section */
			progbits_found = 1;
			ep->be_progbits_vaddr = bt_get_user32((unsigned long *)&shdr->sh_addr);
		}
	}

	/* read symbols */
	if (idx_exidx != SHN_UNDEF) {
		struct vm_area_struct *vm = NULL;
		Elf_Shdr *shdr;
		/* using .ARM.exidx section */
		shdr = ep->be_shdr + idx_exidx;
		ep->be_exidx_size = bt_get_user32((unsigned long *)&shdr->sh_size);
		ep->be_adjust_exidx = (unsigned long)(ep->be_base - ep->be_vaddr);
		pr_debug("adjust exidx: be_base(%x) - be_vaddr(%x) = %x\n",
			(unsigned int)ep->be_base, (unsigned int)ep->be_vaddr, (unsigned int)ep->be_adjust_exidx);
		ep->be_exidx = (unsigned long *)(ep->be_exidx_base + ep->be_adjust_exidx);
		while(search_current_map_exec_addr(bt_file_name(filp), &vm)) {
			ep->be_exidx_tab = unwind_table_add_user((unsigned long)ep->be_exidx,
					ep->be_exidx_size,
					vm->vm_start,
					vm->vm_end - vm->vm_start);
			pr_debug("add exidx tab:%p start:%p end:%p\n", ep->be_exidx_tab, (void *)vm->vm_start, (void *)vm->vm_end);
		}
	}

	if (idx_addrlist != SHN_UNDEF) {
		/* using .snsc_addrlist section */
		ret = bt_map_section(ep, idx_addrlist, (void **)&ep->be_addrlist,
				     &ep->be_addrlist_size);
		if (ret < 0)
			goto err_out;
		bt_calc_adjust(ep);
	}

	if (idx_symlist != SHN_UNDEF) {
		ret = bt_map_section(ep, idx_symlist, (void **)&ep->be_symlist,
			     &ep->be_symlist_size);
		if (ret < 0)
			goto err_out;
	}

	if (idx_symtab != SHN_UNDEF) {
		ret = bt_map_section(ep, idx_symtab, (void **)&ep->be_symtab,
				     &ep->be_symtab_size);
		if (ret < 0)
			goto err_out;
	}

	if (idx_strtab != SHN_UNDEF) {
		ret = bt_map_section(ep, idx_strtab, (void **)&ep->be_strtab,
				     &ep->be_strtab_size);
		if (ret < 0)
			goto err_out;
	}

	return 0;
err_out:
	bt_release_elf_cache(ep);
	return ret;
}

/* search session's bs_elfs using filp as a key */
static struct bt_elf_cache *bt_find_elf_cache(struct bt_session *session,
					      struct file *filp)
{
	struct bt_elf_cache *ep;
	list_for_each_entry(ep, &session->bs_elfs, be_list) {
		if (ep->be_filp == filp)
			return ep;
	}
	return 0;
}

/*
 * Return previously created elf_cache.  If none, create new one.
 *
 * Upon succeeded, filp's refcount (which is assumed to be
 * inceremented by the caller) is automatically decremented when
 * session is destoroyed, Upon failure, caller need to decrement the
 * refcount.
 */
static struct bt_elf_cache *
bt_find_or_new_elf_cache(struct bt_session *session, struct file *filp,
			 unsigned long base)
{
	int ret;
	struct bt_elf_cache *ep;

	ep = bt_find_elf_cache(session, filp);
	if (ep) {
		/* filp's refcount is doubly incremented, one when ep
		   is registered to the session and one by me.  undo
		   mine. */
		fput(filp);
		return ep;
	}
	ep = kmalloc(sizeof(struct bt_elf_cache), session->bs_memflag);
	if (!ep)
		return ERR_PTR(-ENOMEM);
	ret = bt_init_elf_cache(session, ep, filp, base);
	if (ret < 0) {
		kfree(ep);
		return ERR_PTR(ret);
	}
	/* ret == 0 */
	list_add(&ep->be_list, &session->bs_elfs);
	return ep;
}

void bt_init_session(struct bt_session *session, const char *mode,
			    int memflag)
{
	INIT_LIST_HEAD(&session->bs_elfs);
	session->bs_memflag = memflag;
}
EXPORT_SYMBOL(bt_init_session);

void bt_release_session(struct bt_session *session)
{
	struct bt_elf_cache *ep, *n;
	list_for_each_entry_safe(ep, n, &session->bs_elfs, be_list) {
		bt_release_elf_cache(ep);
		kfree(ep);
	}
}
EXPORT_SYMBOL(bt_release_session);

/*
 * 0: symbol found
 * other: symbol not found
 */
int bt_addrlist_search(struct bt_elf_cache *ep, unsigned long addr,
		       char *buf, unsigned long buflen,
		       struct bt_arch_callback_arg *cbarg)
{
	struct addrlist_header *p_header =
		(struct addrlist_header *)ep->be_addrlist;
	unsigned long str, symaddr, ofs, mid;
	unsigned long strtab_len;
	unsigned long *addrlist = (unsigned long *)(p_header + 1);
	unsigned long low = 0;
	unsigned long high = (ep->be_addrlist_size -
			      sizeof(struct addrlist_header)) /
		             sizeof(unsigned long);

	strtab_len = bt_get_user32(&p_header->strtab_len);

	ofs = addr - ep->be_base - ep->be_adjust;
	while (low < high) {
		mid = (low + high) / 2;
		if (ofs < bt_get_user32(&addrlist[mid]))
			high = mid;
		else
			low = mid + 1;
	}
	if (low == 0)
		return BT_SYMBOL_NOT_FOUND;
	low--;
	cbarg->ba_sym_size = bt_get_user32(&addrlist[low]);
	symaddr = cbarg->ba_sym_size + ep->be_base + ep->be_adjust;

	if (buf) {
		if (!ep->be_symlist) {
			snprintf(buf, buflen, "stripped (%s +%#lx)", bt_file_name(ep->be_filp), (addr - ep->be_base));
		}
		else if (strtab_len != ep->be_strtab_size) {
			snprintf(buf, buflen, ".strtab changed (%ld->%ld)",
				strtab_len, ep->be_strtab_size);
		}
		else {
			int ret; /* warning workaround */
			str = bt_get_user32(&ep->be_symlist[low]);
			ret = copy_from_user(buf, ep->be_strtab + str, buflen - 1);
			buf[buflen - 1] = '\0';
		}
	}

	cbarg->ba_sym_start = symaddr;
	cbarg->ba_sym_size = (low < (ep->be_addrlist_size-1) ? bt_get_user32(&addrlist[low+1]) - cbarg->ba_sym_size : 0);
	cbarg->ba_file = ep->be_filp;
	memcpy(&cbarg->ba_hash, &p_header->hash, sizeof(p_header->hash));
	cbarg->ba_adjust = ep->be_adjust;

	pr_debug("%s base:%lx sym:%s\n", __FUNCTION__, ep->be_base, buf);

	return 0;
}

/*
 * 0: symbol found
 * < 0: symbol not found
 */
int bt_symtab_search(struct bt_elf_cache *ep, unsigned long addr,
		     char *buf, unsigned long buflen,
		     struct bt_arch_callback_arg *cbarg)
{
	Elf_Sym *symtab = ep->be_symtab;
	Elf_Sym *max_ent = NULL;
	unsigned long sym_num = ep->be_symtab_size / sizeof(Elf_Sym);
	unsigned long max = 0;
	unsigned long i, sym_addr;

	if (!ep->be_symtab)
		return BT_SYMBOL_NOT_FOUND;
	for (i = 0; i < sym_num; ++i) {
		Elf_Sym *ent = symtab + i;
		if (ELF_ST_TYPE(bt_get_user32((unsigned long *)&ent->st_info)) != STT_FUNC)
			continue;
		sym_addr = bt_get_user32((unsigned long *)&ent->st_value) - ep->be_vaddr + ep->be_base;
		if (sym_addr <= addr && sym_addr > max) {
			max = sym_addr;
			max_ent = ent;
		}
	}
	if (!max)
		return -1;
	cbarg->ba_sym_start = max;
	cbarg->ba_sym_size = bt_get_user32((unsigned long *)&max_ent->st_size);
	cbarg->ba_file = ep->be_filp;
	memset(&cbarg->ba_hash, 0, sizeof(cbarg->ba_hash));
	if (ep->be_strtab && buf) {
		int ret; /* warning workaround */
		ret = copy_from_user(buf, ep->be_strtab + max_ent->st_name, buflen - 1);
		buf[buflen - 1] = '\0';
	}
	/* intentional for getting offset from lib/exe top */
	cbarg->ba_adjust = ep->be_base;
	return 0;
}

/*
 *   0: success, symbol name (or '\0' if unavailable) set to buf)
 * < 0: error, description (or '\0' if unavailable) set to buf)
 * It is OK to pass NULL as buf.
 */
int bt_find_symbol(unsigned long addr, struct bt_session *session,
		   char *buf, unsigned long buflen,
		   struct bt_arch_callback_arg *cbarg,
		   struct bt_elf_cache **rep)
{
	int ret;
	struct file *filp;
	struct bt_elf_cache *ep;
	unsigned long base;

	if (buf)
		buf[0] = '\0';
	filp = bt_get_mapped_file(addr, &base);
	if (!filp) {
		if (buf)
			snprintf(buf, buflen, "no file associated to addr");
		return -1;
	}
	pr_debug("%s addr:%lx\n", bt_file_name(filp), addr);
	ep = bt_find_or_new_elf_cache(session, filp, base);

	if (IS_ERR(ep)) {
		if (buf)
			snprintf(buf, buflen, "reading elf failed (%s)",
				 bt_file_name(filp));
		fput(filp);
		return PTR_ERR(ep);
	}
	*rep = ep;

	pr_debug("exidx:%p\n", ep->be_exidx);

	if (ep->be_addrlist) {
		/* using .snsc_addrlist section */
		ret = bt_addrlist_search(ep, addr, buf, buflen, cbarg);
	} else {
		/* using .symtab section */
		ret = bt_symtab_search(ep, addr, buf, buflen, cbarg);
	}
	return ret;
}

EXPORT_SYMBOL(bt_find_symbol);

unsigned long sym_start;
unsigned long bt_ustack_unwind(struct bt_session *session,
			struct stackframe *frame,
			void *pcb,
			bt_callback_t *cb, void *uarg)
{
	int  ret, cbret;
	char buf[256];
	int buflen = 256;
	struct bt_arch_callback_arg cbarg;
	int urc;
	struct bt_elf_cache *ep = NULL;
	struct unwind_ctrl_block *pctrl = pcb;

	buf[0] = '\0';
	cbarg.ba_extra = 0;
	cbarg.ba_str = buf;
	cbarg.ba_addr = frame->pc;
	ret = bt_find_symbol(frame->pc, session, buf, buflen, &cbarg, &ep);
	/* condition for missing addsymlist info */
	if (ret == BT_SYMBOL_NOT_FOUND) {
		cbarg.ba_status = BT_STATUS_SUCCESS;
		cbarg.ba_sym_start = 0x0;
		cbarg.ba_file = ep->be_filp;
		cbarg.ba_adjust = ep->be_base;
		sym_start = 0x0;
		cb(&cbarg, uarg);
	}
	else if (ret < 0) {
		cbarg.ba_status = BT_STATUS_ERROR;
		cb(&cbarg, uarg);
		return 0;
	}
	else {
		cbarg.ba_status = BT_STATUS_SUCCESS;
		sym_start = cbarg.ba_sym_start & ~1;
		cbret = cb(&cbarg, uarg);
	}

	pr_debug("%s f.pc:%lx f.sp:%lx f.lr:%lx\n",__FUNCTION__, frame->pc, frame->sp, frame->lr);
	urc = unwind_frame_user(frame, ep, pctrl);
	if (urc < 0) {
		cbarg.ba_status = BT_STATUS_ERROR;
		return 0;
	}
	return frame->pc;
}

int bt_null_callback(struct bt_arch_callback_arg *cbarg, void *uarg)
{
	return 0;
}

int bt_ustack_user(struct bt_session *session,
	int ba_size, void **ba_buf, void *ba_skip_addr)
{
	int pc;
	struct pt_regs *_regs = task_pt_regs(current);
	struct stackframe frame;
	struct unwind_ctrl_block ctrl;
	int count = 0;
	int skipping = 1;
	int i;
	int first_time = 1;

	frame.sp = _regs->ARM_sp;
	frame.lr = _regs->ARM_lr;
	frame.pc = _regs->ARM_pc;
	for (i=0;i<0xf;i++) {
		ctrl.vrs[i] = _regs->uregs[i];
	}

	if (ba_skip_addr == NULL) {
		skipping = 0;
		put_user((void *)frame.pc, ba_buf + count);
		count++;
	}

	while (count < ba_size) {
		pc = bt_ustack_unwind(session, &frame,
				      &ctrl, bt_null_callback, NULL);
		if ((pc == 0) && (frame.pc != frame.lr) && first_time) {
			frame.pc = frame.lr;
			pc = frame.pc;
		}
		first_time = 0;
		pr_debug("next:%x f.pc:%lx f.sp:%lx f.lr:%lx\n",pc, frame.pc, frame.sp, frame.lr);
		if (!pc)
			break;
		if (skipping)
			skipping = ((void *)pc != ba_skip_addr);
		if (!skipping) {
			put_user((void *)pc, ba_buf + count);
			count++;
		}
	}
	return count;
}
EXPORT_SYMBOL(bt_ustack_user);

void bt_ustack(const char *mode, int is_atomic,
	       struct pt_regs *_regs, bt_callback_t *cb, void *uarg)
{
	struct stackframe frame;
	struct bt_session session;
	int first_time;
	unsigned long pc;
	struct unwind_ctrl_block ctrl;

	if (is_atomic) {
		struct bt_arch_callback_arg cbarg;
		cbarg.ba_status = BT_STATUS_ERROR;
		cbarg.ba_extra = 0;
		cbarg.ba_addr = _regs->ARM_pc;
		cbarg.ba_str = "ustack disabled in atomic";
		cb(&cbarg, uarg);
		return;
	}

	if (_regs) {
		int i;
		frame.fp = _regs->ARM_fp;
		frame.sp = _regs->ARM_sp;
		frame.lr = _regs->ARM_lr;
		frame.pc = _regs->ARM_pc;
		for (i=0;i<0xf;i++) {
			ctrl.vrs[i] = _regs->uregs[i];
		}
	}

	/* PC should be replaced with LR only for topmost/fisrt PC on the stack */
	first_time = 1;
	bt_init_session(&session, mode, GFP_KERNEL);
	do{
		pc = bt_ustack_unwind(&session, &frame,
				      &ctrl, cb, uarg);
		if ((pc == 0) && (frame.pc != frame.lr) && first_time) {
			frame.pc = frame.lr;
			pc = frame.pc;
		}
		first_time = 0;
		pr_debug("next:%lx f.pc:%lx f.sp:%lx f.lr:%lx\n",pc, frame.pc, frame.sp, frame.lr);
	}while(pc);
	bt_release_session(&session);
}
EXPORT_SYMBOL(bt_ustack);


void bt_kstack_regs(struct task_struct *task, struct pt_regs *_regs,
		    bt_print_func_t prfunc, void *uarg, int funtop_possible)
{
	struct stackframe frame;
	unsigned long pc;
	frame.sp = _regs->ARM_sp;
	frame.lr = _regs->ARM_lr;
	frame.pc = _regs->ARM_pc;
	pc = frame.pc;

	unwind_backtrace(_regs, task, prfunc);
}

EXPORT_SYMBOL(bt_kstack_regs);

void bt_kstack_current(const char *mode,
		       bt_print_func_t prfunc, void *uarg)
{
	struct pt_regs _regs;
	struct task_struct *task = current;
	register unsigned long current_pc asm ("pc");
	register unsigned long current_sp asm ("sp");
	register unsigned long current_lr asm ("lr");
	_regs.ARM_pc = current_pc;
	_regs.ARM_sp= current_sp;
	_regs.ARM_lr = current_lr;
	_regs.ARM_fp = (unsigned long)__builtin_frame_address(0);
	bt_kstack_regs(task, &_regs, prfunc, uarg, 0);
}
EXPORT_SYMBOL(bt_kstack_current);

/* call bt_ustack with regs of specified task */
void bt_ustack_task(struct task_struct *task,
		    const char *mode, int is_atomic,
		    bt_callback_t *cb, void *uarg)
{
	struct pt_regs *regs = task_pt_regs(task);
	struct task_struct *cur = current;
	struct mm_struct *saved_mm, *saved_active_mm, *new_mm;

	if (cur == task) {
		bt_ustack(mode, is_atomic, regs, cb, uarg);
		return;
	}

	new_mm = get_task_mm(task); /* need mmput() below */
	task_lock(cur);
	saved_mm = cur->mm;
	saved_active_mm = cur->active_mm;
	cur->mm = new_mm;
	cur->active_mm = new_mm;
	switch_mm(saved_mm, new_mm, cur);
	task_unlock(cur);

	bt_ustack(mode, is_atomic, regs, cb, uarg);

	task_lock(cur);
	cur->mm = saved_mm;
	cur->active_mm = saved_active_mm;
	switch_mm(new_mm, saved_mm, cur);
	task_unlock(cur);
	mmput(new_mm);
}
EXPORT_SYMBOL(bt_ustack_task);


