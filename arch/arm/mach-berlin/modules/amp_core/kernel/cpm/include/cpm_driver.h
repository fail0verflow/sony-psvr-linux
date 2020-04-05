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
#ifndef _CPM_DRIVER_H_
#define _CPM_DRIVER_H_

#define LEAKAGE_GROUP_NUM               10
#define VOLTAGE_TABLE_SIZE              (LEAKAGE_GROUP_NUM*4) //group*(leakage,low,middle,high)

#define GFX3D_CLK_GATTING_VAL           0xFFFFFFFF
#define GFX3D_TEMP_LIMIT                95

#define GFX_LOW_TIME_OUT                (HZ) //1s

#define CPM_RES_TIMIER                  1
#define CPM_RES_TIMIER_TASK             2

typedef  int (*GFX_CALLBACK)(void* gfx, int gfx_mod);

typedef struct {
    void *          gfx;
    GFX_CALLBACK    before_change;
    GFX_CALLBACK    after_change;
}CPM_GFX_CALLBACK;

typedef struct {
    struct cpm_core_mod_spec *gfx3d;
}CPM_GFX_LOADING;

enum {
    CPM_GFX2D_CALLBACK,
    CPM_GFX3D_CALLBACK,
};

enum {
    CPM_IOCTL_CMD_LOAD_CORE_VOLTAGE_TABLE = 100,
    CPM_IOCTL_CMD_INIT_CORE_STATE,
    CPM_IOCTL_CMD_SET_STARTUP_CFG,
    CPM_IOCTL_CMD_CP_CONTROL,
    CPM_IOCTL_CMD_GET_CP_MOD_NUM,
    CPM_IOCTL_CMD_GET_CP_MOD_NAME,
    CPM_IOCTL_CMD_GET_CP_MOD_STATUS,
    CPM_IOCTL_CMD_GET_CORE_MOD_NUM,
    CPM_IOCTL_CMD_GET_CORE_MOD_STATUS,
    CPM_IOCTL_CMD_GET_CORE_CLK_NUM,
    CPM_IOCTL_CMD_GET_CORE_CLK_FREQ,
    CPM_IOCTL_CMD_GET_CORE_STATUS,
    CPM_IOCTL_CMD_REQUEST_CORE_LOW,
    CPM_IOCTL_CMD_REQUEST_CORE_HIGH,
    CPM_IOCTL_CMD_REQUEST_IGNORE_OFF,
    CPM_IOCTL_CMD_REQUEST_IGNORE_ON,
    CPM_IOCTL_CMD_MAX,
};

enum {
    CPM_REQUEST_CORE_LOW,
    CPM_REQUEST_CORE_HIGH,
    CPM_REQUEST_MAX,
};

enum {
    CPM_REQUEST_IGNORE_OFF,
    CPM_REQUEST_IGNORE_ON,
    CPM_REQUEST_IGNORE_MAX,
};

enum {
    CPM_CORE_VOLTAGE_LOW,
    CPM_CORE_VOLTAGE_MIDDLE,
    CPM_CORE_VOLTAGE_HIGH,
    CPM_CORE_VOLTAGE_MAX,
};

enum {
    CPM_NOT_CHANGE_GFX_CLK_SRC,
    CPM_CHANGE_GFX2_CLK_SRC,
    CPM_CHANGE_GFX3_CLK_SRC,
};

int cpm_set_core_mode(char *name, unsigned int request);
void cpm_register_gfx_callback(CPM_GFX_CALLBACK* callback);
void cpm_unregister_gfx_callback(void);
int cpm_set_gfx3d_loading(unsigned int loading);
int cpm_module_init(void);
void cpm_module_exit(void);
#endif
