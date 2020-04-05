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
//! \file dhub_cmd.h
//!
//! \brief Header file of commands for TrustZone.
//!
//! Purpose: This is used for controlling access to dhub API in TrustZone.
//!
//!
//! Note:
////////////////////////////////////////////////////////////////////////////////

#ifndef __DHUB_CMD_H__
#define __DHUB_CMD_H__

#define TA_DHUB_UUID {0x13a7d413, 0x1b94, 0x4780, {0x84, 0xac, 0x49, 0xad, 0x0b, 0x03, 0x17, 0x67}}

/* enum for DHUB commands */
typedef enum {
    DHUB_INIT,
    DHUB_CHANNEL_CLEAR,

    DHUB_CHANNEL_WRITECMD,
    DHUB_CHANNEL_GENERATECMD,

    DHUB_SEM_POP,
    DHUB_SEM_CLR_FULL,
    DHUB_SEM_CHK_FULL,
    DHUB_SEM_INTR_ENABLE,

    DHUB_PASSSHM,

}DHUB_CMD_ID;

#endif /* __DHUB_CMD_H__ */

