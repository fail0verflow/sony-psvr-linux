/******************************************************************************
 * Copyright (c) 2013-2014 Marvell International Ltd. and its affiliates.
 * All rights reserved.
 *
 * This software file (the "File") is owned and distributed by Marvell
 * International Ltd. and/or its affiliates ("Marvell") under the following
 * licensing terms.
 ******************************************************************************
 * Marvell Commercial License Option
 *
 * If you received this File from Marvell and you have entered into a
 * commercial license agreement (a "Commercial License") with Marvell, the
 * File is licensed to you under the terms of the applicable Commercial
 * License.
 ******************************************************************************
 * Marvell GPL License Option
 *
 * If you received this File from Marvell, you may opt to use, redistribute
 * and/or modify this File in accordance with the terms and conditions of the
 * General Public License Version 2, June 1991 (the "GPL License"), a copy of
 * which is available along with the File in the license.txt file or by writing
 * to the Free Software Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 or on the worldwide web at http://www.gnu.org/licenses/gpl.txt.
 * THE FILE IS DISTRIBUTED AS-IS, WITHOUT WARRANTY OF ANY KIND, AND THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE ARE
 * EXPRESSLY DISCLAIMED. The GPL License provides additional details about this
 * warranty disclaimer.
 *******************************************************************************/

////////////////////////////////////////////////////////////////////////////////
//! \file cpm_bg2dtv.c
//!
//! \brief Register tables of BG2DTV for clock/power management.
//!
//! Purpose:
//!
//!     Version    Date                     Author
//!     V 1.00,    June 26 2014,            Joe Zhu
//!
//! Note:
////////////////////////////////////////////////////////////////////////////////

#include "cpm_ca.h"
#include "cpm_driver.h"


#define CORE_VOLTAGE_NUM(a)     (sizeof(a) / sizeof(struct cpm_core_voltage_tbl))

static struct cpm_core_voltage_tbl bg2dtv_core_voltage[] = {
    {0,            1250,       1250,       1250},
    {400,          1290,       1320,       1320},
    {600,          1260,       1290,       1290},
    {9999,         1230,       1260,       1260},
};

#define CORE_SPEC_NUM(a)     (sizeof(a) / sizeof(struct cpm_core_mod_spec))

struct cpm_core_mod_spec bg2dtv_core_modules[] = {
    {"V2G",  CPM_REQUEST_CORE_LOW, CPM_REQUEST_IGNORE_OFF, CPM_NOT_CHANGE_GFX_CLK_SRC},
    {"VMET", CPM_REQUEST_CORE_LOW, CPM_REQUEST_IGNORE_OFF, CPM_NOT_CHANGE_GFX_CLK_SRC},
    {"GFX3", CPM_REQUEST_CORE_LOW, CPM_REQUEST_IGNORE_OFF, CPM_NOT_CHANGE_GFX_CLK_SRC},
    {"GFX2", CPM_REQUEST_CORE_HIGH, CPM_REQUEST_IGNORE_OFF, CPM_CHANGE_GFX2_CLK_SRC},
};

const struct cpm_core_ctrl_tbl core_ctrl_tbl_bg2dtv = {CORE_VOLTAGE_NUM(bg2dtv_core_voltage),
                bg2dtv_core_voltage, CORE_SPEC_NUM(bg2dtv_core_modules),bg2dtv_core_modules};

const struct cpm_core_ctrl_tbl * cpm_get_core_ctrl_tbl(void)
{
    return &core_ctrl_tbl_bg2dtv;
}

