/*
 *  memdrip/memdump_fwrite.c
 *
 *  Writing data to a file from kernel module
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
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/mman.h>
#include <linux/errno.h>
#include <linux/signal.h>
#include <linux/binfmts.h>
#include <linux/string.h>
#include <linux/file.h>
#include <linux/uaccess.h>
#include <linux/memdump.h>


struct file *dump_file;

static ssize_t __write_to_file(struct file *file, void *buf, size_t len)
{
	ssize_t ret;
	ret = file->f_op->write(file, buf, len, &file->f_pos);
	if (file->f_op->fsync)
		file->f_op->fsync(file, 0);
	return ret;
}

/*
 * write to file
 */
int write_to_file(ulong *buf, unsigned long size)
{
	mm_segment_t fs;
	fs = get_fs();
	set_fs(KERNEL_DS);
	if ((__write_to_file(dump_file, buf, size)) < size) {
		MEMDRIP_DBG(KERN_ERR "memdripper: error in file write\n");
		goto err;
	}
	set_fs(fs);
	return 0;
err:
	set_fs(fs);
	return -1;
}
EXPORT_SYMBOL(write_to_file);

static void __file_lseek(struct file *file, loff_t offset, int set)
{
	file->f_op->llseek(file, offset, set);
}

int file_lseek(loff_t offset, int set)
{
	mm_segment_t fs;
	fs = get_fs();
	set_fs(KERNEL_DS);
	__file_lseek(dump_file, offset, set);
	set_fs(fs);
	return 0;
}
EXPORT_SYMBOL(file_lseek);

/*
 * open file handler
 */
int open_file(char *fname)
{
	dump_file = filp_open(fname, O_CREAT | O_TRUNC, S_IRUGO | S_IWUSR);

	if (IS_ERR(dump_file)) {
		MEMDRIP_DBG(KERN_ERR "memdripper:Can't create '%s'\n", fname);
		return -1;
	}
	if (!dump_file->f_op->write) {
		filp_close(dump_file, NULL);
		MEMDRIP_DBG(KERN_ERR "memdripper:No write op\n");
		return -1;
	}
	return 0;

}
EXPORT_SYMBOL(open_file);

/*
 * close file handler
 */
int close_file(void)
{
	if (IS_ERR(dump_file)) {
		MEMDRIP_DBG(KERN_ERR "memdripper:invalid file handler %s\n",
				__func__);
		return -1;
	}

	if (filp_close(dump_file, NULL)) {
		MEMDRIP_DBG(KERN_ERR "memdripper:Error in file close %s\n",
				__func__);
		return -1;
	}
	return 0;
}
EXPORT_SYMBOL(close_file);

