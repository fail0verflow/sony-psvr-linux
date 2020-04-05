/*
 *  Copyright 2009 Sony Corporation.
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
 *  675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef _SNSC_BACKTRACE_H_
#define _SNSC_BACKTRACE_H_

#include <linux/compiler.h>
#include <linux/ioctl.h>
#ifdef __KERNEL__
#include <linux/module.h>
#endif

#ifdef __KERNEL__

#define BT_STATUS_SUCCESS 0
#define BT_STATUS_ERROR -1
#define BT_SYMBOL_NOT_FOUND -2
static inline int bt_status_is_error(long status) { return status < 0; }

struct bt_elf_cache {
	struct file *be_filp;
	struct elfhdr be_ehdr;
	unsigned long be_base;
	unsigned long be_vaddr;
	Elf_Shdr *be_shdr;
	const char *be_shstrtab;
	const char *be_strtab;
	Elf_Sym *be_symtab;
	unsigned long *be_addrlist;
	unsigned long *be_exidx;
	struct unwind_table *be_exidx_tab;
	unsigned long *be_symlist;
	unsigned long be_shdr_size;
	unsigned long be_shstrtab_size;
	unsigned long be_strtab_size;
	unsigned long be_symtab_size;
	unsigned long be_addrlist_size;
	unsigned long be_exidx_size;
	unsigned long be_symlist_size;
	struct list_head be_list;
	unsigned long be_progbits_vaddr;
	long be_adjust;
	unsigned long be_exidx_base;
	unsigned long be_adjust_exidx;
};

struct addrlist_header {
	unsigned char magic[4];
	unsigned char hash[4];
	unsigned long strtab_len;
};

struct bt_arch_callback_arg {
	unsigned long ba_addr;
	unsigned long ba_sym_start;
	unsigned long ba_sym_size;
	struct file *ba_file;
	char ba_hash[4];
	const char *ba_str;
	long ba_status;
	const char *ba_modname;
	unsigned long ba_extra;
	long ba_adjust;
};

struct bt_session {
	/* virtually an assoc array with be_filp as a key */
	struct list_head bs_elfs;
	int bs_memflag;
};

typedef int (*bt_print_func_t)(const char *fmt, ...);
typedef int bt_callback_t(struct bt_arch_callback_arg *cbarg, void *user);
extern void bt_ustack(const char *mode, int is_atomic,
		      struct pt_regs *regs, bt_callback_t *cb, void *uarg);
extern void bt_kstack_regs(struct task_struct *task, struct pt_regs *regs,
			   bt_print_func_t prfunc, void *uarg,
			   int funtop_possible);
extern void bt_kstack_current(const char *mode,
			      bt_print_func_t prfunc, void *uarg);
extern const char *bt_file_name(struct file *filp);
extern void bt_init_session(struct bt_session *session, const char *mode,
			    int memflag);
extern int bt_ustack_user(struct bt_session *session,
	int ba_size, void **ba_buf, void *ba_skip_addr);
extern void bt_release_session(struct bt_session *session);
static inline int bt_hash_valid(const struct bt_arch_callback_arg *cbarg)
{
	return    cbarg->ba_hash[0] | cbarg->ba_hash[1]
		| cbarg->ba_hash[2] | cbarg->ba_hash[3];
}


extern const char *bt_param_match(const char *param, const char *name);
extern char bt_param[];
extern int bt_find_symbol(unsigned long addr, struct bt_session *session,
		   char *buf, unsigned long buflen,
		   struct bt_arch_callback_arg *cbarg,
		   struct bt_elf_cache **rep);
extern void bt_ustack_task(struct task_struct *task,
		    const char *mode, int is_atomic,
		    bt_callback_t *cb, void *uarg);
extern struct file *bt_get_mapped_file(unsigned long addr, unsigned long *base);

#endif /* __KERNEL__ */

#endif /* _SNSC_BACKTRACE_H_ */

