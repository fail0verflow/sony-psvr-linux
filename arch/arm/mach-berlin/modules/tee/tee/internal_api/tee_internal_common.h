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
#ifndef _TEE_INTERNAL_COMMON_H_
#define _TEE_INTERNAL_COMMON_H_

#include "tee_common.h"

enum {
	TEE_SUCCESS			= 0x00000000,	/* TEEC_SUCCESS			*/
	TEE_ERROR_GENERIC		= 0xFFFF0000,	/* TEEC_ERROR_GENERIC		*/
	TEE_ERROR_ACCESS_DENIED		= 0xFFFF0001,	/* TEEC_ERROR_ACCESS_DENIED	*/
	TEE_ERROR_CANCEL		= 0xFFFF0002,	/* TEEC_ERROR_CANCEL		*/
	TEE_ERROR_ACCESS_CONFLICT	= 0xFFFF0003,	/* TEEC_ERROR_ACCESS_CONFLICT	*/
	TEE_ERROR_EXCESS_DATA		= 0xFFFF0004,	/* TEEC_ERROR_EXCESS_DATA	*/
	TEE_ERROR_BAD_FORMAT		= 0xFFFF0005,	/* TEEC_ERROR_BAD_FORMAT	*/
	TEE_ERROR_BAD_PARAMETERS	= 0xFFFF0006,	/* TEEC_ERROR_BAD_PARAMETERS	*/
	TEE_ERROR_BAD_STATE		= 0xFFFF0007,	/* TEEC_ERROR_BAD_STATE		*/
	TEE_ERROR_ITEM_NOT_FOUND	= 0xFFFF0008,	/* TEEC_ERROR_ITEM_NOT_FOUND	*/
	TEE_ERROR_NOT_IMPLEMENTED	= 0xFFFF0009,	/* TEEC_ERROR_NOT_IMPLEMENTED	*/
	TEE_ERROR_NOT_SUPPORTED		= 0xFFFF000A,	/* TEEC_ERROR_NOT_SUPPORTED	*/
	TEE_ERROR_NO_DATA		= 0xFFFF000B,	/* TEEC_ERROR_NO_DATA		*/
	TEE_ERROR_OUT_OF_MEMORY		= 0xFFFF000C,	/* TEEC_ERROR_OUT_OF_MEMORY	*/
	TEE_ERROR_BUSY			= 0xFFFF000D,	/* TEEC_ERROR_BUSY		*/
	TEE_ERROR_COMMUNICATION		= 0xFFFF000E,	/* TEEC_ERROR_COMMUNICATION	*/
	TEE_ERROR_SECURITY		= 0xFFFF000F,	/* TEEC_ERROR_SECURITY		*/
	TEE_ERROR_SHORT_BUFFER		= 0xFFFF0010,	/* TEEC_ERROR_SHORT_BUFFER	*/
	TEE_PENDING			= 0xFFFF2000,
	TEE_ERROR_TIMEOUT		= 0xFFFF3001,
	TEE_ERROR_OVERFLOW		= 0xFFFF300F,
	TEE_ERROR_TARGET_DEAD		= 0xFFFF3024,	/* TEEC_ERROR_TARGET_DEAD	*/
	TEE_ERROR_STORAGE_NO_SPACE	= 0xFFFF3041,
	TEE_ERROR_MAC_INVALID		= 0xFFFF3071,
	TEE_ERROR_SIGNATURE_INVALID	= 0xFFFF3072,
	TEE_ERROR_TIME_NOT_SET		= 0xFFFF5000,
	TEE_ERROR_TIME_NEEDS_RESET	= 0xFFFF5001,
};

#define TEE_HANDLE_NULL			(0)

#define TEE_SUCCEEDED(result)		((TEE_Result)(result) <  0x80000000)
#define TEE_FAILED(result)		((TEE_Result)(result) >= 0x80000000)

#endif /* _TEE_INTERNAL_COMMON_H_ */
