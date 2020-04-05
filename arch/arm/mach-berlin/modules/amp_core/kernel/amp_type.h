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
#ifndef _AMP_TYPE_H_
#define _AMP_TYPE_H_
typedef unsigned char   UCHAR;
typedef char            CHAR;
typedef unsigned int    UINT;
typedef int             INT;
typedef UCHAR           UINT8;
typedef CHAR            INT8;
typedef unsigned short  UINT16;
typedef short           INT16;
typedef int             INT32;
typedef unsigned int    UINT32;
typedef void            VOID;
typedef void*           PVOID;
typedef signed int      HRESULT;
typedef long            LONG;
typedef unsigned long   ULONG;
typedef long long       INT64;
typedef unsigned long long UINT64;

#define E_SUC               ( 0x00000000 )
#define E_ERR               ( 0x80000000 )

// generic error code macro
#define E_GEN_SUC( code ) ( E_SUC | E_GENERIC_BASE | ( code & 0x0000FFFF ) )
#define E_GEN_ERR( code )       ( E_ERR | E_GENERIC_BASE | ( code & 0x0000FFFF ) )

#define S_OK                E_GEN_SUC( 0x0000 ) // Success
#define S_FALSE             E_GEN_SUC( 0x0001 ) // Success but return false status
#define E_FAIL              E_GEN_ERR( 0x4005 ) // Unspecified failure
#define E_OUTOFMEMORY       E_GEN_ERR( 0x700E ) // Failed to allocate necessary memory

// error code base list
#define ERRORCODE_BASE          (0)
#define E_GENERIC_BASE          ( 0x0000 << 16 )
#define E_SYSTEM_BASE           ( 0x0001 << 16 )
#define E_DEBUG_BASE            ( 0x0002 << 16 )

#endif    //_AMP_TYPE_H_
