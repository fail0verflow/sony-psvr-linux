/*
 *  Copyright 2013 Sony Corporation
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
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/ktime.h>
#include <linux/posix-timers.h>

__attribute__((weak)) unsigned long long notrace snsc_raw_clock(void)
{
	pr_err("Please implement snsc_raw_clock() "
	       "to return nanosecond monotonic time.\n");

	return -1;
}
EXPORT_SYMBOL(snsc_raw_clock);

__attribute__((weak)) unsigned long long notrace snsc_raw_clock_get_res(void)
{
	pr_err("Please implement snsc_raw_clock_get_res() "
	       "to return nanosecond resolution of snsc_raw_clock().\n");

	return -1;
}
EXPORT_SYMBOL(snsc_raw_clock_get_res);

#ifdef CONFIG_SNSC_POSIX_CLOCK_SNSC_RAW_CLOCK
/*
 * POSIX clock support for snsc_raw_clock
 */
static int posix_snsc_raw_clock_get_res(const clockid_t which_clock,
					struct timespec *tp)
{
	*tp = ns_to_timespec(snsc_raw_clock_get_res());

	return 0;
}

static int posix_snsc_raw_clock_get(const clockid_t which_clock,
				    struct timespec *tp)
{
	*tp = ns_to_timespec(snsc_raw_clock());

	return 0;
}

static int posix_snsc_raw_clock_set(const clockid_t which_clock,
				    const struct timespec *tp)
{
	return -EINVAL;
}

static __init int init_posix_snsc_raw_clock(void)
{
	struct k_clock posix_snsc_raw_clock = {
		.clock_getres	= posix_snsc_raw_clock_get_res,
		.clock_get	= posix_snsc_raw_clock_get,
		.clock_set	= posix_snsc_raw_clock_set,
	};
	posix_timers_register_clock(CLOCK_SNSC_RAW_CLOCK, &posix_snsc_raw_clock);

	return 0;
}
__initcall(init_posix_snsc_raw_clock);
#endif /* CONFIG_SNSC_POSIX_CLOCK_SNSC_RAW_CLOCK */
