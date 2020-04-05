/*
 * Copyright (c) 2013 Marvell Technology Group Ltd.
 *
 * Author: Jisheng Zhang <jszhang@marvell.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include <linux/clk-provider.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/of.h>

#include "clk.h"

static DEFINE_SPINLOCK(lock);

static const struct grpclk_desc berlin2q_grpclk_desc[] __initconst = {
	{ "sys", 3, 0, 0, CLK_IGNORE_UNUSED},
	{ "drmfigo", 6, 6, 17, CLK_IGNORE_UNUSED},
	{ "cfg", 9, 12, 1 },
	{ "gfx2d", 12, 18, 4, CLK_IGNORE_UNUSED},
	{ "zsp", 15, 24, 6, CLK_IGNORE_UNUSED},
	{ "perif", 18, 32, 7,},
	{ "pcube", 21, 38, 2, CLK_IGNORE_UNUSED},
	{ "vscope", 24, 44, 3, CLK_IGNORE_UNUSED},
	{ "nfcecc", 27, 50, 19 },
	{ "app", 33, 64, 20, CLK_IGNORE_UNUSED},
	{},
};

static void __init berlin2q_grpclk_setup(struct device_node *np)
{
	berlin_grpclk_setup(np, berlin2q_grpclk_desc, &lock);
}
CLK_OF_DECLARE(berlin2q_grpclk, "marvell,berlin2q-grpclk",
	       berlin2q_grpclk_setup);

static const struct gateclk_desc berlin2q_gateclk_desc[] __initconst = {
	{ "gfx2daxi",	"perif",	5, CLK_IGNORE_UNUSED },
	{ "eth",	"perif",	8 },
	{ "sata",	"perif",	9 },
	{ "ahbapb",	"perif",	10, CLK_IGNORE_UNUSED },
	{ "usb0",	"perif",	11, CLK_IGNORE_UNUSED },
	{ "usb1",	"perif",	12, CLK_IGNORE_UNUSED },
	{ "usb2",	"perif",	13, CLK_IGNORE_UNUSED },
	{ "usb3",	"perif",	14, CLK_IGNORE_UNUSED },
	{ "pbridge",	"perif",	15, CLK_IGNORE_UNUSED },
	{ "sdio",	"perif",	16, CLK_IGNORE_UNUSED },
	{ "nfc",	"perif",	18, CLK_IGNORE_UNUSED },
	{ "pcie",	"perif",	22, CLK_IGNORE_UNUSED },
	{ "geth",	"perif",	23, CLK_IGNORE_UNUSED },
	{},
};

static void __init berlin2q_gateclk_setup(struct device_node *np)
{
	berlin_gateclk_setup(np, berlin2q_gateclk_desc, &lock);
}
CLK_OF_DECLARE(berlin2q_gateclk, "marvell,berlin2q-gateclk",
	       berlin2q_gateclk_setup);
