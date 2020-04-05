/*
 *  memdrip/memdump_proc.c
 *
 *  create a proc entries under /proc/memdump and use it for memdump
 *		configuration
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
#include <linux/elf.h>
#include <linux/uaccess.h>
#include <linux/io.h>

#include <asm/tlbflush.h>
#include <asm/cacheflush.h>

#include "memdump_fwrite.h"

#define procfs_name		"memdump"
#define memdump_auto_start	"memdump_auto_start"
#define PROCFS_MAX_SIZE		1024
#define CODE_REVISION		"1.0.2"

extern bool memdump_auto_dump;
extern bool memdrip_enable_inherit;
/*
 *  the /proc file structure
 */
struct proc_dir_entry *memdump_dir;
struct proc_dir_entry *memdump_auto_proc_file;
struct proc_dir_entry *memdump_enable_inherit_file;
/*
 * Function invoked when writing into this proc file
 */
int memdump_enable_inherit_write(struct file *file, const char *buffer,
		unsigned long count, void *data)
{
	int proc;
	proc = simple_strtol(buffer, NULL, 10);

	if (proc < 0 || proc > 1)
		return -EINVAL;

	memdrip_enable_inherit = proc;

	return count;
}

/*
 * Function invoked during read call to this proc file
 */
int memdump_enable_inherit_read(char *buffer,
		char **buffer_location, off_t offset,
		int buffer_length, int *eof, void *data)
{
	int ret;

	if (offset > 0) {
		/* we have finished to read, return 0 */
		ret  = 0;
	} else {
		/* fill the buffer, return the buffer size */
		ret = sprintf(buffer, "%d\n", memdrip_enable_inherit);
	}

	return ret;
}

/*
 * Function invoked when writing into this proc file
 */
int memdump_auto_procfile_write(struct file *file, const char *buffer,
		unsigned long count, void *data)
{
	int proc;
	proc = simple_strtol(buffer, NULL, 10);

	if (proc < 0 || proc > 1)
		return -EINVAL;

	memdump_auto_dump = proc;

	return count;
}

/*
 * Function invoked during read call to this proc file
 */
int memdump_auto_procfile_read(char *buffer,
		char **buffer_location,
		off_t offset, int buffer_length,
		int *eof, void *data)
{
	int ret;

	MEMDRIP_DBG(KERN_INFO "procfile_read (/proc/%s) called\n", procfs_name);

	if (offset > 0) {
		/* we have finished to read, return 0 */
		ret  = 0;
	} else {
		/* fill the buffer, return the buffer size */
		ret = sprintf(buffer, "%d\n", memdump_auto_dump);
	}

	return ret;
}

/*
 * Module init routine
 */
static int __init memdump_init(void)
{

	memdump_dir = proc_mkdir(procfs_name, NULL);

	if (memdump_dir == NULL) {
		printk(KERN_ERR "error in creating memdump dir entry\n");
		return -ENOMEM;
	}

	/* create a proc entry under /proc/ for user to input pid,segmnet name*/
	memdump_auto_proc_file = create_proc_entry(memdump_auto_start,
			0644, memdump_dir);

	if (memdump_auto_proc_file == NULL) {
		remove_proc_entry(memdump_auto_start, NULL);
		MEMDRIP_DBG(KERN_ALERT "Error: Could not initialize /proc/%s\n",
				procfs_name);
		return -ENOMEM;
	}

	memdump_auto_proc_file->read_proc	= memdump_auto_procfile_read;
	memdump_auto_proc_file->write_proc	= memdump_auto_procfile_write;
	memdump_auto_proc_file->mode		= S_IFREG | S_IRUGO | S_IWUGO;
	memdump_auto_proc_file->uid		= 0;
	memdump_auto_proc_file->gid		= 0;

	/* create a file for enabling and disbling inherit option */
	memdump_enable_inherit_file = create_proc_entry("memdrip_enable_inherit",
			0666, memdump_dir);
	if (memdump_enable_inherit_file == NULL) {
		printk(KERN_ERR "error in creating memdrip inherit entry\n");
		return -ENOMEM;
	}

	memdump_enable_inherit_file->read_proc	= memdump_enable_inherit_read;
	memdump_enable_inherit_file->write_proc	= memdump_enable_inherit_write;
	memdump_enable_inherit_file->mode	= S_IFREG | S_IRUGO | S_IWUGO;
	memdump_auto_proc_file->uid		= 0;
	memdump_auto_proc_file->gid		= 0;

	MEMDRIP_DBG(KERN_INFO "memdripper:/proc/%s created\n", procfs_name);

	memdump_trigger_thread_create();

	return 0;
}

/*
 * Module clean up routine
 */
static void __exit cleanup_mod(void)
{
	remove_proc_entry(procfs_name, NULL);
	MEMDRIP_DBG(KERN_INFO "Removing \'memdripper module.\n");
	memdump_trigger_thread_delete();
}
module_init(memdump_init);
module_exit(cleanup_mod);
