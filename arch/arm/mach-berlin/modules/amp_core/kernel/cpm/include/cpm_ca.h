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
//! \file cpm_ca.h
//!
//! \brief Common definition for CPM (Clock/Power Management) module.
//!
//! Purpose:
//!
//!     Version    Date                     Author
//!     V 1.00,    Sep 2 2014,              Joe Zhu
//!
//!
////////////////////////////////////////////////////////////////////////////////

#ifndef _CPM_CA_H_
#define _CPM_CA_H_

#define MAX_CHIP_NAME_LEN       16
#define MAX_MOD_NAME_LEN        5

#define CPM_MAX_DEVS                    8
#define CPM_MINOR                       0
#define CPM_DEVICE_NAME                 "cpm"
#define CPM_DEVICE_TAG                  "[cpm_driver] "


#define CPM_RES_CTX                     1
#define CPM_RES_SESSION                 2
#define CPM_RES_SHM_IN                  4
#define CPM_RES_SHM_OUT                 8

#define CPM_IN_SHM_SIZE                 64
#define CPM_OUT_SHM_SIZE                32


#define TA_CPM_UUID {0xea2c26f2, 0x9942, 0x11e3, {0xab, 0xa5, 0x00, 0x0c, 0x29, 0x1c, 0x86, 0x93}}


enum {
    CPM_INVALID,
    CPM_READ_LEAKAGE_ID,
    CPM_SET_STARTUP_CFG,
    CPM_CP_CONTROL,
    CPM_CORE_CONTROL,
    CPM_GET_NUM_MOD,
    CPM_GET_MOD_NAME,
    CPM_GET_MOD_STAT,
    CPM_GET_CORE_CLK_NUM,
    CPM_GET_CORE_CLK_FREQ,
    CPM_GET_TEMP,
    CPM_CMD_MAX
};


struct cpm_core_voltage_tbl {
    unsigned int    leakage;
    unsigned int    low_voltage;
    unsigned int    middle_voltage;
    unsigned int    high_voltage;
};

struct cpm_core_mod_spec {
    char            name[MAX_MOD_NAME_LEN];
    unsigned int    request;
    unsigned int    ignore;
    unsigned int    chg_gfx; //change gfx clock
};

struct cpm_core_ctrl_tbl {
    unsigned int num_voltage_tbl;
    struct cpm_core_voltage_tbl *voltage_tbl;
    unsigned int num_modules;
    struct cpm_core_mod_spec *mod_spec;
};


#endif /* _CPM_CA_H_ */
