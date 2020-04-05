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
#include "drv_morpheus.h"

#define VPP_VTOTAL_L    0xf7f65010
#define VPP_VTOTAL_H    0xf7f65014
#define VPP_FRST_L        0xf7f65038
#define VPP_FRST_H        0xf7f6503C
#define VPP_VS_S_L        0xf7f65050
#define VPP_VS_S_H        0xf7f65054
#define VPP_VS_E_L        0xf7f65058
#define VPP_VS_E_H        0xf7f6505c

//#define ADJUST_CPCB1
#define CPCB1_VPP_VTOTAL_L    0xf7f65810
#define CPCB1_VPP_VTOTAL_H    0xf7f65814
#define CPCB1_VPP_FRST_L        0xf7f65838
#define CPCB1_VPP_FRST_H        0xf7f6583C
#define CPCB1_VPP_VS_S_L        0xf7f65850
#define CPCB1_VPP_VS_S_H        0xf7f65854
#define CPCB1_VPP_VS_E_L        0xf7f65858
#define CPCB1_VPP_VS_E_H        0xf7f6585c

#define BASE_ADDR_4K 0xF7C80000
#define RA_Vpp_4K_TG4K 0x0494
#define RA_TG4K_TG_PRG 0x0058
#define RA_TG_PRG_VSYNC 0x0010
#define RA_TG_PRG_Total 0x0004
#define RA_TG_PRG_CTRL 0x0000
#define TOTAL_4K (BASE_ADDR_4K + RA_Vpp_4K_TG4K + RA_TG4K_TG_PRG + RA_TG_PRG_Total)
#define CTRL_4K (BASE_ADDR_4K + RA_Vpp_4K_TG4K + RA_TG4K_TG_PRG + RA_TG_PRG_CTRL)
#define VS_4K (BASE_ADDR_4K + RA_Vpp_4K_TG4K + RA_TG4K_TG_PRG + RA_TG_PRG_VSYNC)

#define LATENCY_EXPECTED 750000 // 750us
#define LATENCY_THRESHOLD 250000 // 250us
#define MAX_ADJ_LATENCY_COMMON_1 118742 // cacaulte from 1080p5994 8 line
#define ADJUST_PRINT_INTERVAL 10

// Copy from vpp_cfg.h and vpp_cfg.c
/* definition of video resolution type */
enum {
    TYPE_SD = 0,
    TYPE_HD = 1,
#if (BERLIN_CHIP_VERSION >= BERLIN_BG2_DTV)
    TYPE_UHD = 2,
#endif

};

/* definition of video scan mode */
enum {
    SCAN_PROGRESSIVE = 0,
    SCAN_INTERLACED  = 1,
};

/* definition of video frame-rate */
enum {
    FRAME_RATE_23P98 = 0,
    FRAME_RATE_24    = 1,
    FRAME_RATE_25    = 2,
    FRAME_RATE_29P97 = 3,
    FRAME_RATE_30    = 4,
    FRAME_RATE_47P96 = 5,
    FRAME_RATE_48    = 6,
    FRAME_RATE_50    = 7,
    FRAME_RATE_59P94 = 8,
    FRAME_RATE_60    = 9,
    FRAME_RATE_100   = 10,
    FRAME_RATE_119P88 = 11,
    FRAME_RATE_120   = 12,
    FRAME_RATE_89P91 = 13,
    FRAME_RATE_90    = 14,
    MAX_NUM_FRAME_RATE
};

typedef struct RESOLUTION_INFO_T {
    int active_width;
    int active_height;   /* active height of channel in pixel */
    int width;  /* width of channel in pixel */
    int height; /* height of channel in pixel */
    int hfrontporch; /* front porch of hsync */
    int hsyncwidth; /* hsync width */
    int hbackporch; /* back porch of hsync */
    int vfrontporch; /* front porch of vsync */
    int vsyncwidth; /* vsync width */
    int vbackporch; /* back porch of vsync */
    int type;   /* resolution type: HD or SD */
    int scan;   /* scan mode: progressive or interlaced */
    int frame_rate;   /* frame rate */
    int flag_3d;   /* 1 for 3D, 0 for 2D */
    int freq;   /* pixel frequency */
    int pts_per_cnt_4;   /* time interval in term of PTS for every 4 frames */
}RESOLUTION_INFO;

typedef struct {
    unsigned int line_count;
    unsigned int max_adj_latency;
} LINE_COUNT_MAX_ADJ_LATENCY;

const LINE_COUNT_MAX_ADJ_LATENCY m_line_count_max_adj_latency[MAX_NUM_RESS] = {
                           /* {    line_count, max_adj_latency             } */
/* RES_NTSC_M    */           {    31838,      MAX_ADJ_LATENCY_COMMON_1    },
/* RES_NTSC_J    */           {    31838,      MAX_ADJ_LATENCY_COMMON_1    },
/* RES_PAL_M    */            {    31838,      MAX_ADJ_LATENCY_COMMON_1    },
/* RES_PAL_BGH     */         {    32051,      MAX_ADJ_LATENCY_COMMON_1    },
/* RES_525I60    */           {    31806,      MAX_ADJ_LATENCY_COMMON_1    },
/* RES_525I5994    */         {    31838,      MAX_ADJ_LATENCY_COMMON_1    },
/* RES_625I50     */          {    32051,      MAX_ADJ_LATENCY_COMMON_1    },
/* RES_525P60  */             {    31806,      MAX_ADJ_LATENCY_COMMON_1    },
/* RES_525P5994  */           {    31838,      MAX_ADJ_LATENCY_COMMON_1    },
/* RES_625P50  */             {    32051,      MAX_ADJ_LATENCY_COMMON_1    },
/* RES_720P30  */             {    44503,      MAX_ADJ_LATENCY_COMMON_1    },
/* RES_720P2997  */           {    44548,      MAX_ADJ_LATENCY_COMMON_1    },
/* RES_720P25  */             {    53404,      MAX_ADJ_LATENCY_COMMON_1    },
/* RES_720P60  */             {    22251,      MAX_ADJ_LATENCY_COMMON_1    },
/* RES_720P5994  */           {    22274,      MAX_ADJ_LATENCY_COMMON_1    },
/* RES_720P50  */             {    26702,      MAX_ADJ_LATENCY_COMMON_1    },
/* RES_1080I60 */             {    14827,      MAX_ADJ_LATENCY_COMMON_1    },
/* RES_1080I5994 */           {    14842,      MAX_ADJ_LATENCY_COMMON_1    },
/* RES_1080I50 */             {    17793,      MAX_ADJ_LATENCY_COMMON_1    },
/* RES_1080P30 */             {    29655,      MAX_ADJ_LATENCY_COMMON_1    },
/* RES_1080P2997 */           {    29685,      MAX_ADJ_LATENCY_COMMON_1    },
/* RES_1080P25 */             {    35587,      MAX_ADJ_LATENCY_COMMON_1    },
/* RES_1080P24 */             {    37069,      MAX_ADJ_LATENCY_COMMON_1    },
/* RES_1080P2398 */           {    37107,      MAX_ADJ_LATENCY_COMMON_1    },
/* RES_1080P60 */             {    14827,      MAX_ADJ_LATENCY_COMMON_1    },
/* RES_1080P5994 */           {    14842,      MAX_ADJ_LATENCY_COMMON_1    },
/* RES_1080P50 */             {    17793,      MAX_ADJ_LATENCY_COMMON_1    },
/* RES_LVDS_1080P48 */        {    14827,      MAX_ADJ_LATENCY_COMMON_1    },
/* RES_LVDS_1080P50 */        {    14825,      MAX_ADJ_LATENCY_COMMON_1    },
/* RES_LVDS_1080P60 */        {    14827,      MAX_ADJ_LATENCY_COMMON_1    },
/* RES_LVDS_2160P12*/         {    14827,      MAX_ADJ_LATENCY_COMMON_1    },
/* RES_VGA_480P60  */         {    31806,      MAX_ADJ_LATENCY_COMMON_1    },
/* RES_VGA_480P5994  */       {    31838,      MAX_ADJ_LATENCY_COMMON_1    },
/* RES_720P50_3D  */          {    13351,      MAX_ADJ_LATENCY_COMMON_1    },
/* RES_720P60_3D  */          {    11125,      MAX_ADJ_LATENCY_COMMON_1    },
/* RES_720P5994_3D  */        {    11137,      MAX_ADJ_LATENCY_COMMON_1    },
/* RES_1080P24_3D */          {    18534,      MAX_ADJ_LATENCY_COMMON_1    },
/* RES_1080P2398_3D */        {    18553,      MAX_ADJ_LATENCY_COMMON_1    },
/* RES_1080P30_3D */          {    14827,      MAX_ADJ_LATENCY_COMMON_1    },
/* RES_1080P2997_3D */        {    14842,      MAX_ADJ_LATENCY_COMMON_1    },
/* RES_1080P25_3D */          {    17793,      MAX_ADJ_LATENCY_COMMON_1    },
/* RES_1080I60_FP */          {    14821,      MAX_ADJ_LATENCY_COMMON_1    },
/* RES_1080I5994_FP */        {    14821,      MAX_ADJ_LATENCY_COMMON_1    },
/* RES_1080I50_FP */          {    17785,      MAX_ADJ_LATENCY_COMMON_1    },
/* RES_LVDS_1920X540P60_3D */ {    13354,      MAX_ADJ_LATENCY_COMMON_1    },
/* RES_LVDS_1920X540P30_3D */ {    26709,      MAX_ADJ_LATENCY_COMMON_1    },
/* RES_LVDS_1920X540P24_3D */ {    33386,      MAX_ADJ_LATENCY_COMMON_1    },
/* RES_LVDS_720P100_3D  */    {    9578,       MAX_ADJ_LATENCY_COMMON_1    },
/* RES_LVDS_720P120_3D  */    {    9578,       MAX_ADJ_LATENCY_COMMON_1    },
/* RES_LVDS_1080P48_3D */     {    14827,      MAX_ADJ_LATENCY_COMMON_1    },
/* RES_LVDS_1080P50_3D */     {    14825,      MAX_ADJ_LATENCY_COMMON_1    },
/* RES_LVDS_1080P60_3D */     {    14827,      MAX_ADJ_LATENCY_COMMON_1    },
/* RES_LVDS_1920x540P100_3D */{    14836,      MAX_ADJ_LATENCY_COMMON_1    },
/* RES_LVDS_1920X540P120_3D */{    14854,      MAX_ADJ_LATENCY_COMMON_1    },
/* RES_LVDS_960x1080P100_3D */{    7412,       MAX_ADJ_LATENCY_COMMON_1    },
/* RES_LVDS_960x1080P120_3D */{    7413,       MAX_ADJ_LATENCY_COMMON_1    },
/* RES_4Kx2K2398    */        {    18545,      MAX_ADJ_LATENCY_COMMON_1    },
/* RES_4Kx2K24    */          {    18526,      MAX_ADJ_LATENCY_COMMON_1    },
/* RES_4Kx2K24_SMPTE */       {    18526,      MAX_ADJ_LATENCY_COMMON_1    },
/* RES_4Kx2K25    */          {    17785,      MAX_ADJ_LATENCY_COMMON_1    },
/* RES_4Kx2K2997    */        {    14836,      MAX_ADJ_LATENCY_COMMON_1    },
/* RES_4Kx2K30    */          {    14821,      MAX_ADJ_LATENCY_COMMON_1    },
/* RES_4Kx2K50    */          {    8892,       MAX_ADJ_LATENCY_COMMON_1    },
/* RES_4Kx2K5994    */        {    7418,       MAX_ADJ_LATENCY_COMMON_1    },
/* RES_4Kx2K60    */          {    7410,       MAX_ADJ_LATENCY_COMMON_1    },
/* RES_4Kx2K30_HDMI*/         {    29655,      MAX_ADJ_LATENCY_COMMON_1    },
/* RES_4Kx1K120    */         {    7413,       MAX_ADJ_LATENCY_COMMON_1    },
/* RES_720P_4Kx1K120_3D */    {    11125,      MAX_ADJ_LATENCY_COMMON_1    },
/* RES_720P100  */            {    13351,      MAX_ADJ_LATENCY_COMMON_1    },
/* RES_720P11988  */          {    11122,      MAX_ADJ_LATENCY_COMMON_1    },
/* RES_720P120  */            {    11125,      MAX_ADJ_LATENCY_COMMON_1    },
/* RES_720P8991 */            {    14829,      MAX_ADJ_LATENCY_COMMON_1    },
/* RES_720P90 */              {    14815,      MAX_ADJ_LATENCY_COMMON_1    },
/* RES_1080P100 */            {    8896,       MAX_ADJ_LATENCY_COMMON_1    },
/* RES_1080P11988 */          {    7414,       MAX_ADJ_LATENCY_COMMON_1    },
/* RES_1080P120 */            {    7413,       MAX_ADJ_LATENCY_COMMON_1    },
/* RES_1080P8991 */           {    9886,       MAX_ADJ_LATENCY_COMMON_1    },
/* RES_1080P90 */             {    9876,       MAX_ADJ_LATENCY_COMMON_1    },
/* RES_4Kx2K2398_SMPTE*/      {    18545,      MAX_ADJ_LATENCY_COMMON_1    },
/* RES_4Kx2K25_SMPTE*/        {    17785,      MAX_ADJ_LATENCY_COMMON_1    },
/* RES_4Kx2K2997_SMPTE*/      {    14836,      MAX_ADJ_LATENCY_COMMON_1    },
/* RES_4Kx2K30_SMPTE*/        {    14821,      MAX_ADJ_LATENCY_COMMON_1    },
/* RES_4Kx2K50_SMPTE*/        {    8892,       MAX_ADJ_LATENCY_COMMON_1    },
/* RES_4Kx2K5994_SMPTE*/      {    7418,       MAX_ADJ_LATENCY_COMMON_1    },
/* RES_4Kx2K60_SMPTE*/        {    7410,       MAX_ADJ_LATENCY_COMMON_1    },
/* RES_4Kx2K50_420*/          {    17793,      MAX_ADJ_LATENCY_COMMON_1    },
/* RES_4Kx2K5994_420*/        {    14842,      MAX_ADJ_LATENCY_COMMON_1    },
/* RES_4Kx2K60_420*/          {    14827,      MAX_ADJ_LATENCY_COMMON_1    },
/* RES_4Kx2K2398_3D*/         {    18553,      MAX_ADJ_LATENCY_COMMON_1    },
/* RES_4Kx2K24_3D*/           {    18534,      MAX_ADJ_LATENCY_COMMON_1    },
/* RES_4Kx2K25_3D*/           {    17793,      MAX_ADJ_LATENCY_COMMON_1    },
/* RES_4Kx2K2997_3D*/         {    14842,      MAX_ADJ_LATENCY_COMMON_1    },
/* RES_4Kx2K30_3D */          {    14827,      MAX_ADJ_LATENCY_COMMON_1    },
/* RES_LVDS_1088P60 */        {    14827,      MAX_ADJ_LATENCY_COMMON_1    },
/* RES_RESET   */             {    0,          0                           },
};

const RESOLUTION_INFO m_resinfo_table[MAX_NUM_RESS] = {
                                   /* { active_width, active_height, width, height, hfrontporch, hsyncwidth, hbackporch, vfrontporch, vsyncwidth, vbackporch, type, scan, frame_rate, flag_3d, freq, pts_per_cnt_4 } */
    /*  0: RES_NTSC_M              */ {     720,          480,         858,    525,    19,          62,         57,         4,           3,          15,   TYPE_SD,   SCAN_INTERLACED, FRAME_RATE_59P94,    0,  27000,  6006 },
    /*  1: RES_NTSC_J              */ {     720,          480,         858,    525,    19,          62,         57,         4,           3,          15,   TYPE_SD,   SCAN_INTERLACED, FRAME_RATE_59P94,    0,  27000,  6006 },
    /*  2: RES_PAL_M               */ {     720,          480,         858,    525,    19,          62,         57,         4,           3,          15,   TYPE_SD,   SCAN_INTERLACED, FRAME_RATE_59P94,    0,  27000,  6006 },
    /*  3: RES_PAL_BGH             */ {     720,          576,         864,    625,    12,          63,         69,         2,           3,          19,   TYPE_SD,   SCAN_INTERLACED, FRAME_RATE_50,       0,  27000,  7200 },
    /*  4: RES_525I60              */ {     720,          480,         858,    525,    19,          62,         57,         4,           3,          15,   TYPE_SD,   SCAN_INTERLACED, FRAME_RATE_60,       0,  27027,  6000 },
    /*  5: RES_525I5994            */ {     720,          480,         858,    525,    19,          62,         57,         4,           3,          15,   TYPE_SD,   SCAN_INTERLACED, FRAME_RATE_59P94,    0,  27000,  6006 },
    /*  6: RES_625I50              */ {     720,          576,         864,    625,    12,          63,         69,         2,           3,          19,   TYPE_SD,   SCAN_INTERLACED, FRAME_RATE_50,       0,  27000,  7200 },
    /*  7: RES_525P60              */ {     720,          480,         858,    525,    16,          62,         60,         9,           6,          30,   TYPE_SD,   SCAN_PROGRESSIVE, FRAME_RATE_60,      0,  27027,  6000 },
    /*  8: RES_525P5994            */ {     720,          480,         858,    525,    16,          62,         60,         9,           6,          30,   TYPE_SD,   SCAN_PROGRESSIVE, FRAME_RATE_59P94,   0,  27000,  6006 },
    /*  9: RES_625P50              */ {     720,          576,         864,    625,    12,          64,         68,         5,           5,          39,   TYPE_SD,   SCAN_PROGRESSIVE, FRAME_RATE_50,      0,  27000,  7200 },
    /* 10: RES_720P30              */ {    1280,          720,         3300,   750,    1760,        40,         220,        5,           5,          20,   TYPE_HD,   SCAN_PROGRESSIVE, FRAME_RATE_30,      0,  74250, 12000 },
    /* 11: RES_720P2997            */ {    1280,          720,         3300,   750,    1760,        40,         220,        5,           5,          20,   TYPE_HD,   SCAN_PROGRESSIVE, FRAME_RATE_29P97,   0,  74176, 12012 },
    /* 12: RES_720P25              */ {    1280,          720,         3960,   750,    2420,        40,         220,        5,           5,          20,   TYPE_HD,   SCAN_PROGRESSIVE, FRAME_RATE_25,      0,  74250, 14400 },
    /* 13: RES_720P60              */ {    1280,          720,         1650,   750,    110,         40,         220,        5,           5,          20,   TYPE_HD,   SCAN_PROGRESSIVE, FRAME_RATE_60,      0,  74250,  6000 },
    /* 14: RES_720P5994            */ {    1280,          720,         1650,   750,    110,         40,         220,        5,           5,          20,   TYPE_HD,   SCAN_PROGRESSIVE, FRAME_RATE_59P94,   0,  74176,  6006 },
    /* 15: RES_720P50              */ {    1280,          720,         1980,   750,    440,         40,         220,        5,           5,          20,   TYPE_HD,   SCAN_PROGRESSIVE, FRAME_RATE_50,      0,  74250,  7200 },
    /* 16: RES_1080I60             */ {    1920,         1080,         2200,   1125,   88,          44,         148,        2,           5,          15,   TYPE_HD,   SCAN_INTERLACED,  FRAME_RATE_60,      0, 148500,  6000 },
    /* 17: RES_1080I5994           */ {    1920,         1080,         2200,   1125,   88,          44,         148,        2,           5,          15,   TYPE_HD,   SCAN_INTERLACED,  FRAME_RATE_59P94,   0, 148352,  6006 },
    /* 18: RES_1080I50             */ {    1920,         1080,         2640,   1125,   528,         44,         148,        2,           5,          15,   TYPE_HD,   SCAN_INTERLACED,  FRAME_RATE_50,      0, 148500,  7200 },
    /* 19: RES_1080P30             */ {    1920,         1080,         2200,   1125,   88,          44,         148,        4,           5,          36,   TYPE_HD,   SCAN_PROGRESSIVE, FRAME_RATE_30,      0, 74250,  12000 },
    /* 20: RES_1080P2997           */ {    1920,         1080,         2200,   1125,   88,          44,         148,        4,           5,          36,   TYPE_HD,   SCAN_PROGRESSIVE, FRAME_RATE_29P97,   0, 74176,  12012 },
    /* 21: RES_1080P25             */ {    1920,         1080,         2640,   1125,   528,         44,         148,        4,           5,          36,   TYPE_HD,   SCAN_PROGRESSIVE, FRAME_RATE_25,      0, 74250,  14400 },
    /* 22: RES_1080P24             */ {    1920,         1080,         2750,   1125,   638,         44,         148,        4,           5,          36,   TYPE_HD,   SCAN_PROGRESSIVE, FRAME_RATE_24,      0, 74250,  15000 },
    /* 23: RES_1080P2398           */ {    1920,         1080,         2750,   1125,   638,         44,         148,        4,           5,          36,   TYPE_HD,   SCAN_PROGRESSIVE, FRAME_RATE_23P98,   0, 74176,  15015 },
    /* 24: RES_1080P60             */ {    1920,         1080,         2200,   1125,   88,          44,         148,        4,           5,          36,   TYPE_HD,   SCAN_PROGRESSIVE, FRAME_RATE_60,      0, 148500,  6000 },
    /* 25: RES_1080P5994           */ {    1920,         1080,         2200,   1125,   88,          44,         148,        4,           5,          36,   TYPE_HD,   SCAN_PROGRESSIVE, FRAME_RATE_59P94,   0, 148352,  6006 },
    /* 26: RES_1080P50             */ {    1920,         1080,         2640,   1125,   528,         44,         148,        4,           5,          36,   TYPE_HD,   SCAN_PROGRESSIVE, FRAME_RATE_50,      0, 148500,  7200 },
    /* 27: RES_LVDS_1080P48        */ {    1920,         1080,         2750,   1125,   638,         44,         148,        4,           5,          36,   TYPE_HD,   SCAN_PROGRESSIVE, FRAME_RATE_48,      0, 148500,  7500 },
    /* 28: RES_LVDS_1080P50        */ {    1920,         1080,         2200,   1350,   32,          44,         204,        229,         5,          36,   TYPE_HD,   SCAN_PROGRESSIVE, FRAME_RATE_50,      0, 148500,  7200 },
    /* 29: RES_LVDS_1080P60        */ {    1920,         1080,         2200,   1125,   32,          44,         204,        4,           5,          36,   TYPE_HD,   SCAN_PROGRESSIVE, FRAME_RATE_60,      0, 148500,  6000 },
    /* 30: RES_LVDS_2160P12        */ {    1920,         1080,         2200,   1406,   32,          44,         204,        235,         5,          86,   TYPE_HD,   SCAN_PROGRESSIVE, FRAME_RATE_48,      0, 148470,  7500 },
    /* 31: RES_VGA_480P60          */ {    640,           480,         800,    525,    16,          96,         48,         10,          2,          33,   TYPE_SD,   SCAN_PROGRESSIVE, FRAME_RATE_60,      0, 25200,   6000 },
    /* 32: RES_VGA_480P5994        */ {    640,           480,         800,    525,    16,          96,         48,         10,          2,          33,   TYPE_SD,   SCAN_PROGRESSIVE, FRAME_RATE_59P94,   0, 25175,   6006 },
    /* 33: RES_720P50_3D           */ {    1280,          720,         1980,   750,    440,         40,         220,        5,           5,          20,   TYPE_HD,   SCAN_PROGRESSIVE, FRAME_RATE_100,     1, 148500,  7200 },
    /* 34: RES_720P60_3D           */ {    1280,          720,         1650,   750,    110,         40,         220,        5,           5,          20,   TYPE_HD,   SCAN_PROGRESSIVE, FRAME_RATE_120,     1, 148500,  6000 },
    /* 35: RES_720P5994_3D         */ {    1280,          720,         1650,   750,    110,         40,         220,        5,           5,          20,   TYPE_HD,   SCAN_PROGRESSIVE, FRAME_RATE_119P88,  1, 148352,  6006 },
    /* 36: RES_1080P24_3D          */ {    1920,         1080,         2750,   1125,   638,         44,         148,        4,           5,          36,   TYPE_HD,   SCAN_PROGRESSIVE, FRAME_RATE_48,      1, 148500, 15000 },
    /* 37: RES_1080P2398_3D        */ {    1920,         1080,         2750,   1125,   638,         44,         148,        4,           5,          36,   TYPE_HD,   SCAN_PROGRESSIVE, FRAME_RATE_47P96,   1, 148352, 15015 },
    /* 38: RES_1080P30_3D          */ {    1920,         1080,         2200,   1125,   88,          44,         148,        4,           5,          36,   TYPE_HD,   SCAN_PROGRESSIVE, FRAME_RATE_60,      1, 148500, 12000 },
    /* 39: RES_1080P2997_3D        */ {    1920,         1080,         2200,   1125,   88,          44,         148,        4,           5,          36,   TYPE_HD,   SCAN_PROGRESSIVE, FRAME_RATE_59P94,   1, 148352, 12012 },
    /* 40: RES_1080P25_3D          */ {    1920,         1080,         2640,   1125,   528,         44,         148,        4,           5,          36,   TYPE_HD,   SCAN_PROGRESSIVE, FRAME_RATE_50,      1, 148500, 14400 },
    /* 41: RES_1080I60_FP          */ {    1920,         2228,         2200,   2250,    88,         44,         148,        2,           5,          15,   TYPE_HD,   SCAN_PROGRESSIVE, FRAME_RATE_30,      0, 148500,  12000},
    /* 42: RES_1080I5994_FP        */ {    1920,         2228,         2200,   2250,    88,         44,         148,        2,           5,          15,   TYPE_HD,   SCAN_PROGRESSIVE, FRAME_RATE_30,      0, 148352,  12000},
    /* 43: RES_1080I50_FP          */ {    1920,         2228,         2640,   2250,   528,         44,         148,        2,           5,          15,   TYPE_HD,   SCAN_PROGRESSIVE, FRAME_RATE_25,      0, 148500,  14400},
    /* 44: RES_LVDS_1920X540P60_3D */ {    1920,          540,         1980,    625,     8,         44,          8,        16,           5,          64,   TYPE_HD,   SCAN_PROGRESSIVE, FRAME_RATE_120,     1, 148500,  6000 },
    /* 45: RES_LVDS_1920X540P30_3D */ {    1920,          540,         1980,    625,     8,         44,          8,        16,           5,          64,   TYPE_HD,   SCAN_PROGRESSIVE, FRAME_RATE_60,      1, 74250,  12000 },
    /* 46: RES_LVDS_1920X540P24_3D */ {    1920,          540,         2475,    625,   407,         44,        104,        16,           5,          64,   TYPE_HD,   SCAN_PROGRESSIVE, FRAME_RATE_48,      1, 74250,  15000 },
    /* 47: RES_LVDS_720P100_3D  */    {    1280,          720,         1980,    750,   440,         40,         220,         5,          5,          20,   TYPE_HD,   SCAN_PROGRESSIVE, FRAME_RATE_100,     1, 148500,  7200 },
    /* 48: RES_LVDS_720P120_3D  */    {    1280,          720,         1650,    750,   110,         40,         220,         5,          5,          20,   TYPE_HD,   SCAN_PROGRESSIVE, FRAME_RATE_120,     1, 148500,  6000 },
    /* 49: RES_LVDS_1080P48_3D */     {    1920,         1080,         2750,   1125,   638,         44,         148,        4,           5,          36,   TYPE_HD,   SCAN_PROGRESSIVE, FRAME_RATE_48,      1, 148500, 15000 },
    /* 50: RES_LVDS_1080P50_3D */     {    1920,         1080,         2200,    1350,   32,         44,         204,        229,         5,          36,   TYPE_HD,   SCAN_PROGRESSIVE, FRAME_RATE_50,      1, 148500, 14400 },
    /* 51: RES_LVDS_1080P60_3D */     {    1920,         1080,         2200,   1125,   32,          44,         204,          4,         5,          36,   TYPE_HD,   SCAN_PROGRESSIVE, FRAME_RATE_60,      1, 148500, 12000 },
    /* 52: RES_LVDS_1920x540P100_3D */{    1920,          540,          2200,   675,    32,         44,         204,         94,         5,          36,   TYPE_HD,   SCAN_PROGRESSIVE, FRAME_RATE_100,     1, 148500,  7200 },
    /* 53: RES_LVDS_1920X540P120_3D */{    1920,          540,          2202,   562,    32,         44,         206,          7,         5,          8,    TYPE_HD,   SCAN_PROGRESSIVE, FRAME_RATE_120,     1, 148500,  6000 },
    /* 54: RES_LVDS_960x1080P100_3D */{     960,         1080,         1100,   1350,   42,          44,         54,         229,         5,          36,   TYPE_HD,   SCAN_PROGRESSIVE, FRAME_RATE_100,     1, 148500,  7200 },
    /* 55: RES_LVDS_960x1080P120_3D */{     960,         1080,         1100,   1125,   42,          44,         54,           4,         5,          36,   TYPE_HD,   SCAN_PROGRESSIVE, FRAME_RATE_120,     1, 148500,  6000 },
#if (BERLIN_CHIP_VERSION >= BERLIN_BG2_DTV)
    /* 56: RES_4Kx2K2398    */        {    3840,         2160,         5500,   2250,   1276,        88,         296,          8,         10,         72,   TYPE_UHD,  SCAN_PROGRESSIVE, FRAME_RATE_23P98,   0, 296703, 15015 },
    /* 57: RES_4Kx2K24    */          {    3840,         2160,         5500,   2250,   1276,        88,         296,          8,         10,         72,   TYPE_UHD,  SCAN_PROGRESSIVE, FRAME_RATE_24,      0, 297000, 15000 },
    /* 58: RES_4Kx2K24_SMPTE */       {    4096,         2160,         5500,   2250,   1020,        88,         296,          8,         10,         72,   TYPE_UHD,  SCAN_PROGRESSIVE, FRAME_RATE_24,      0, 297000, 15000 },
    /* 59: RES_4Kx2K25    */          {    3840,         2160,         5280,   2250,   1056,        88,         296,          8,         10,         72,   TYPE_UHD,  SCAN_PROGRESSIVE, FRAME_RATE_25,      0, 297000, 14400 },
    /* 60: RES_4Kx2K2997    */        {    3840,         2160,         4400,   2250,   176,         88,         296,          8,         10,         72,   TYPE_UHD,  SCAN_PROGRESSIVE, FRAME_RATE_29P97,   0, 296703, 12012 },
    /* 61: RES_4Kx2K30    */          {    3840,         2160,         4400,   2250,   176,         88,         296,          8,         10,         72,   TYPE_UHD,  SCAN_PROGRESSIVE, FRAME_RATE_30,      0, 297000, 12000 },
    /* 62: RES_4Kx2K50    */          {    3840,         2160,         5280,   2250,   1056,        88,         296,          8,         10,         72,   TYPE_UHD,  SCAN_PROGRESSIVE, FRAME_RATE_50,      0, 594000,  7200 },
    /* 63: RES_4Kx2K5994    */        {    3840,         2160,         4400,   2250,   176,         88,         296,          8,         10,         72,   TYPE_UHD,  SCAN_PROGRESSIVE, FRAME_RATE_59P94,   0, 593406,  6006 },
    /* 64: RES_4Kx2K60    */          {    3840,         2160,         4400,   2250,   176,         88,         296,          8,         10,         72,   TYPE_UHD,  SCAN_PROGRESSIVE, FRAME_RATE_60,      0, 594000,  6000 },
    /* 65: RES_4Kx2K30_HDMI*/         {    3840,         2160,         4400,   2250,   176,         88,         296,          8,         10,         72,   TYPE_UHD,  SCAN_PROGRESSIVE, FRAME_RATE_30,      0, 297000, 12000 },
    /* 66: RES_4Kx1K120    */         {    3840,         1080,         4400,   1125,   176,         88,         296,          4,          5,         36,   TYPE_UHD,  SCAN_PROGRESSIVE, FRAME_RATE_120,     0, 594000,  3000 },
    /* 67: RES_720P_4Kx1K120_3D */    {    1280,          720,         1650,   750,    110,         40,         220,          5,          5,         20,   TYPE_HD,   SCAN_PROGRESSIVE, FRAME_RATE_120,     1, 148500,  6000 },
    /* 68: RES_720P100  */            {    1280,          720,         1980,   750,    440,         40,         220,          5,          5,         20,   TYPE_HD,   SCAN_PROGRESSIVE, FRAME_RATE_100,     0, 148500,  3600 },
    /* 69: RES_720P11988  */          {    1280,          720,         1650,   750,    110,         40,         220,          5,          5,         20,   TYPE_HD,   SCAN_PROGRESSIVE, FRAME_RATE_119P88,  0, 148500,  3000 },
    /* 70: RES_720P120  */            {    1280,          720,         1650,   750,    110,         40,         220,          5,          5,         20,   TYPE_HD,   SCAN_PROGRESSIVE, FRAME_RATE_120,     0, 148500,  3000 },
    /* 71: RES_720P8991 */            {    1280,          720,        1650,   750,    110,          40,         220,          5,          5,         20,   TYPE_HD,   SCAN_PROGRESSIVE, FRAME_RATE_89P91,   0, 111375,  4004 },
    /* 72: RES_720P90 */              {    1280,          720,        1650,   750,    110,          40,         220,          5,          5,         20,   TYPE_HD,   SCAN_PROGRESSIVE, FRAME_RATE_90,      0, 111375,  4000 },
    /* 73: RES_1080P100 */            {    1920,         1080,         2640,  1125,    528,         44,         148,          4,          5,         36,   TYPE_HD,   SCAN_PROGRESSIVE, FRAME_RATE_100,     0, 297000,  3600 },
    /* 74: RES_1080P11988 */          {    1920,         1080,         2200,  1125,    88,          44,         148,          4,          5,         36,   TYPE_HD,   SCAN_PROGRESSIVE, FRAME_RATE_119P88,  0, 297000,  3003 },
    /* 75: RES_1080P120 */            {    1920,         1080,         2200,  1125,    88,          44,         148,          4,          5,         36,   TYPE_HD,   SCAN_PROGRESSIVE, FRAME_RATE_120,     0, 297000,  3000 },
    /* 76: RES_1080P8991 */            {    1920,         1080,         2200,  1125,    88,          44,         148,          4,          5,         36,   TYPE_HD,   SCAN_PROGRESSIVE, FRAME_RATE_89P91,     0, 222750,  4004 },
    /* 77: RES_1080P90 */            {     1920,          1080,         2200,  1125,    88,          44,         148,          4,          5,         36,   TYPE_HD,   SCAN_PROGRESSIVE, FRAME_RATE_90,     0, 222750,  4000 },
#if (BERLIN_CHIP_VERSION == BERLIN_BG2_DTV_A0)
    /* 78: RES_4Kx2K2398_SMPTE*/      {    4096,         2160,         5500,   2250,   1020,        88,         296,          8,         10,         72,   TYPE_UHD,  SCAN_PROGRESSIVE, FRAME_RATE_23P98,   0, 296703, 15015 },
    /* 79: RES_4Kx2K25_SMPTE*/        {    4096,         2160,         5280,   2250,    968,        88,         296,          8,         10,         72,   TYPE_UHD,  SCAN_PROGRESSIVE, FRAME_RATE_25,      0, 297000, 14400 },
    /* 80: RES_4Kx2K2997_SMPTE*/      {    4096,         2160,         4400,   2250,     88,        88,         296,          8,         10,         72,   TYPE_UHD,  SCAN_PROGRESSIVE, FRAME_RATE_29P97,   0, 296703, 12012 },
    /* 81: RES_4Kx2K30_SMPTE*/        {    4096,         2160,         4400,   2250,     88,        88,         296,          8,         10,         72,   TYPE_UHD,  SCAN_PROGRESSIVE, FRAME_RATE_30,      0, 297000, 12000 },
    /* 82: RES_4Kx2K50_SMPTE*/        {    4096,         2160,         5280,   2250,    968,        88,         296,          8,         10,         72,   TYPE_UHD,  SCAN_PROGRESSIVE, FRAME_RATE_50,      0, 594000,  7200 },
    /* 83: RES_4Kx2K5994_SMPTE*/      {    4096,         2160,         4400,   2250,     88,        88,         296,          8,         10,         72,   TYPE_UHD,  SCAN_PROGRESSIVE, FRAME_RATE_59P94,   0, 593406,  6006 },
    /* 84: RES_4Kx2K60_SMPTE*/        {    4096,         2160,         4400,   2250,    176,        88,         296,          8,         10,         72,   TYPE_UHD,  SCAN_PROGRESSIVE, FRAME_RATE_60,      0, 594000,  6000 },

    //Timings for HDMITX
    /* 85: RES_4Kx2K50_420*/          {    3840,         2160,         5280,   2250,   1056,        88,         296,          8,         10,         72,   TYPE_UHD,  SCAN_PROGRESSIVE, FRAME_RATE_50,      0, 297000,  7200 },
    /* 86: RES_4Kx2K5994_420*/        {    3840,         2160,         4400,   2250,   176,         88,         296,          8,         10,         72,   TYPE_UHD,  SCAN_PROGRESSIVE, FRAME_RATE_59P94,   0, 296703,  6006 },
    /* 87: RES_4Kx2K60_420*/          {    3840,         2160,         4400,   2250,   176,         88,         296,          8,         10,         72,   TYPE_UHD,  SCAN_PROGRESSIVE, FRAME_RATE_60,      0, 297000,  6000 },
    /* 88: RES_4Kx2K2398_3D*/         {    3840,         2160,         5500,   2250,   1276,        88,         296,          8,         10,         72,   TYPE_UHD,  SCAN_PROGRESSIVE, FRAME_RATE_47P96,   1, 593406, 15015 },
    /* 89: RES_4Kx2K24_3D */          {    3840,         2160,         5500,   2250,   1276,        88,         296,          8,         10,         72,   TYPE_UHD,  SCAN_PROGRESSIVE, FRAME_RATE_48,      1, 594000, 15000 },
    /* 90: RES_4Kx2K25_3D */          {    3840,         2160,         5280,   2250,   1056,        88,         296,          8,         10,         72,   TYPE_UHD,  SCAN_PROGRESSIVE, FRAME_RATE_50,      1, 594000, 14400 },
    /* 91: RES_4Kx2K2997_3D */        {    3840,         2160,         4400,   2250,   176,         88,         296,          8,         10,         72,   TYPE_UHD,  SCAN_PROGRESSIVE, FRAME_RATE_59P94,   1, 593406, 12012 },
    /* 92: RES_4Kx2K30_3D */          {    3840,         2160,         4400,   2250,   176,         88,         296,          8,         10,         72,   TYPE_UHD,  SCAN_PROGRESSIVE, FRAME_RATE_60,      1, 594000, 12000 },
    /* 93: RES_LVDS_1088P60 */        {    1932,         1088,         2200,   1125,     60,        44,         110,          4,         5,          36,   TYPE_HD,   SCAN_PROGRESSIVE, FRAME_RATE_60,      0, 148500,  6000 },
#endif //(BERLIN_CHIP_VERSION == BERLIN_BG2_DTV_A0)
#endif

    /* RES_RESET   */ {       0,            0,            1,      1,     0,          0,           0,        0,           0,           0,         0,                0, 0,    0 }
};

u64 cur_vpp_intr_timestamp = (u64)-1;
u64 cur_vip_intr_timestamp = (u64)-1;
long long vip_isr_count = 0;
long long cur_vip_isr_count = 0;
long long vpp_isr_count = 0;
long long cur_vpp_isr_count = 0;
u64 start_latency = 0;
int vip_stable=0;
int vip_stable_isr=0;
int vpp_res_set=0;
int vpp_cpcb0_res = -1;
int vpp_4k_res = -1;
TG_CHANGE_STATE tg_changed=TG_CHANGE_STATE_CHECK;
int vpp_set_normal=0;
int video_mode=MODE_INVALID;

DRIFT_INFO vip_vpp_drift_info;
DEFINE_SPINLOCK(drift_countlock);

void reset_adjust(int saveIrq)
{
    ULONG irqstat=0;
    if (0 == saveIrq) {
        spin_lock(&drift_countlock);
    } else {
        spin_lock_irqsave(&drift_countlock, irqstat);
    }
    amp_trace("reset_adjust, saveIrq %d\n", saveIrq);
    vip_vpp_drift_info.valid = 0;
    vip_vpp_drift_info.start_latency = 0;
    vip_vpp_drift_info.drift_count = 0;
    vip_vpp_drift_info.total_drift_count = 0;
    vip_vpp_drift_info.frame_count = 0;
    vip_vpp_drift_info.latency_in_the_expected_range = 0;
    start_latency = 0;
    amp_trace("reset_adjust done\n");

    if (0 == saveIrq) {
        spin_unlock(&drift_countlock);
    } else {
        spin_unlock_irqrestore(&drift_countlock, irqstat);
    }
}

static unsigned int print_counter = 0;

static void write_reg_word32_check(unsigned int reg, unsigned int value)
{
        unsigned int oldValue = 0;
        GA_REG_WORD32_READ(reg, &oldValue);
        if (value != oldValue) {
            GA_REG_WORD32_WRITE(reg, value);
        }

        return;
}

static void adjust_cpcb0(INT in_out_latency)
{
    static int printCount = 0;

    int res_vtotal = 0;
    int height = 0;
    int adjustment = 0;
    unsigned int one_line_count = m_line_count_max_adj_latency[vpp_cpcb0_res].line_count;
    unsigned int max_adj_latency = 0;
    max_adj_latency = m_resinfo_table[vpp_cpcb0_res].height - m_resinfo_table[vpp_cpcb0_res].active_height - 12 - 3;
    max_adj_latency *= one_line_count;
    if (LATENCY_EXPECTED < in_out_latency) {
        if (max_adj_latency < in_out_latency - LATENCY_EXPECTED) {
            // Adjust the -max_adj_latency
            // increase the vtotal to let vpp come back later to sync next frame
            adjustment = (in_out_latency - LATENCY_EXPECTED) / one_line_count;
            if (adjustment < m_resinfo_table[vpp_cpcb0_res].height) {
                adjustment = m_resinfo_table[vpp_cpcb0_res].height - adjustment;
            } else {
                adjustment = m_resinfo_table[vpp_cpcb0_res].height;
            }
            printCount++;
        } else {
            adjustment = -1 * ((in_out_latency - LATENCY_EXPECTED) / one_line_count);

            // Note: last one need to print
            printCount = 0;
        }

        if (0 == printCount % ADJUST_PRINT_INTERVAL || 1 == printCount) {
            amp_trace("CPCB0 Try to reduce latency: %d, adjustment: %d, one_line_count %d\n", in_out_latency, adjustment, one_line_count);
        }
    } else if (in_out_latency < LATENCY_EXPECTED) {
        if (max_adj_latency < LATENCY_EXPECTED - in_out_latency) {
            // Adjust the max_adj_latency
            adjustment = (LATENCY_EXPECTED - in_out_latency) / one_line_count;

            printCount++;
        } else {
            adjustment = (LATENCY_EXPECTED - in_out_latency) / one_line_count;

            // Note: last one need to print
            printCount = 0;
        }

        if (0 == printCount % ADJUST_PRINT_INTERVAL || 1 == printCount) {
            amp_trace("CPCB0 Try to increase latency: %d, adjustment: %d, one_line_count %d\n", in_out_latency, adjustment, one_line_count);
        }
    } else {
        adjustment = 0;
    }


    height = m_resinfo_table[vpp_cpcb0_res].height + adjustment;

    if (4096 < height) {
        height = 4096;
    }
    res_vtotal = height - 1;
    //amp_trace(">>CPCB0 %d, %d\n", m_resinfo_table[vpp_cpcb0_res].height + adjustment, height);

    write_reg_word32_check(VPP_VTOTAL_L, res_vtotal&0xff);
    write_reg_word32_check(VPP_VTOTAL_H, (res_vtotal>>8)&0xff);

    return;
}

static void adjust_4k(INT in_out_latency)
{
    static int printCount = 0;

    int res_vtotal = 0;
    int height = 0;
    int adjustment = 0;
    unsigned int one_line_count = m_line_count_max_adj_latency[vpp_4k_res].line_count;
    unsigned int max_adj_latency = 0;
    unsigned int old4kTotal = 0;
    unsigned int new4kTotal = 0;
    max_adj_latency = m_resinfo_table[vpp_4k_res].height - m_resinfo_table[vpp_4k_res].active_height - 12 - 3;
    max_adj_latency *= one_line_count;
    if (LATENCY_EXPECTED < in_out_latency) {
        if (max_adj_latency < in_out_latency - LATENCY_EXPECTED) {
            // increase the vtotal to let vpp come back later to sync next frame
            adjustment = (in_out_latency - LATENCY_EXPECTED) / one_line_count;
            if (adjustment < m_resinfo_table[vpp_4k_res].height) {
                adjustment = m_resinfo_table[vpp_4k_res].height - adjustment;
            } else {
                adjustment = m_resinfo_table[vpp_4k_res].height;
            }

            printCount++;
        } else {
            adjustment = -1 * ((in_out_latency - LATENCY_EXPECTED) / one_line_count);

            // Note: last one need to print
            printCount = 0;
        }

        if (0 == printCount % ADJUST_PRINT_INTERVAL || 1 == printCount) {
            amp_trace("4K Try to reduce latency: %d, adjustment: %d, one_line_count %d\n", in_out_latency, adjustment, one_line_count);
        }
    } else if(in_out_latency < LATENCY_EXPECTED){
        if (max_adj_latency < LATENCY_EXPECTED - in_out_latency) {
            // Adjust the max_adj_latency
            adjustment = (LATENCY_EXPECTED - in_out_latency) / one_line_count;

            printCount++;
        } else {
            adjustment = (LATENCY_EXPECTED - in_out_latency) / one_line_count;

            // Note: last one need to print
            printCount = 0;
        }

        if (0 == printCount % ADJUST_PRINT_INTERVAL || 1 == printCount) {
            amp_trace("4K Try to increase latency: %d, adjustment: %d, one_line_count %d\n", in_out_latency, adjustment, one_line_count);
        }
    } else {
        adjustment = 0;
    }

    height = m_resinfo_table[vpp_4k_res].height + adjustment;
    if (4096 < height) {
        height = 4096;
    }
    res_vtotal = height - 1;
/*
    #define     w32TG_PRG_Total                                {\
            UNSG32 uTotal_vertical                             : 12;\
            UNSG32 uTotal_horizontal                           : 13;\
            UNSG32 RSVDx4_b25                                  :  7;\
          }

    #define     w32TG_PRG_VSYNC                                {\
            UNSG32 uVSYNC_v_start                              : 12;\
            UNSG32 uVSYNC_v_end                                : 12;\
            UNSG32 RSVDx10_b24                                 :  8;\
          }

    #define     w32TG_PRG_CTRL                                 {\
            UNSG32 uCTRL_mode                                  :  2;\
            UNSG32 uCTRL_lwin                                  :  8;\
            UNSG32 uCTRL_frst                                  : 12;\
            UNSG32 uCTRL_freeze                                :  4;\
            UNSG32 uCTRL_sync_ctrl                             :  2;\
            UNSG32 RSVDx0_b28                                  :  4;\
          }
*/
        GA_REG_WORD32_READ(TOTAL_4K, &old4kTotal);
        new4kTotal = (old4kTotal & 0xFFFFF000) + (res_vtotal & 0xFFF);
        if (old4kTotal != new4kTotal) {
            GA_REG_WORD32_WRITE(TOTAL_4K, new4kTotal);
        }

        return;
}

void adjust_inout_latency(void)
{
    static INT wait_frame_count = 0;
    INT in_out_latency = 0;
    INT flag_3d = 0;
    long long save_vip_isr_count = vip_isr_count;
    u64 one_frame_time = 0;

    //amp_trace("adjust_inout_latency cur_vpp_intr_timestamp %lld, cur_vip_intr_timestamp %lld\n",
    //        cur_vpp_intr_timestamp, cur_vip_intr_timestamp);
    in_out_latency = cur_vpp_intr_timestamp - cur_vip_intr_timestamp;

    //amp_trace(">>>>> vip (%lld, %lld), vpp (%lld, %lld), %d\n",
    //        cur_vip_isr_count, save_vip_isr_count, cur_vpp_isr_count, vpp_isr_count, in_out_latency);


    if (-1 == vpp_4k_res) {
        flag_3d = m_resinfo_table[vpp_cpcb0_res].flag_3d;

        one_frame_time = m_resinfo_table[vpp_cpcb0_res].height *
                m_line_count_max_adj_latency[vpp_cpcb0_res].line_count;
    } else {
        flag_3d = m_resinfo_table[vpp_4k_res].flag_3d;
        one_frame_time = m_resinfo_table[vpp_4k_res].height *
                m_line_count_max_adj_latency[vpp_4k_res].line_count;
    }

    //unsigned int vpp_FRST_VTOTAL_delta = 0;
    //amp_trace("adjust_inout_latency\n");
    if (1 == flag_3d) {
        if (TG_CHANGE_STATE_DONE == tg_changed) {
            //amp_trace(">>>>> vip (%lld, %lld), vpp (%lld, %lld), %d\n",
            //        cur_vip_isr_count, save_vip_isr_count, cur_vpp_isr_count, vpp_isr_count, in_out_latency);
            cur_vip_isr_count = save_vip_isr_count;
            cur_vpp_isr_count = vpp_isr_count;
            if (in_out_latency < one_frame_time) {
                //amp_trace("Check latency. %d, %lld\n",
                //        in_out_latency, one_frame_time);
            } else {
                //amp_trace("Do nothing here. Wait next frame. %d, %lld\n",
                //        in_out_latency, one_frame_time);
                return;
            }
        } else {
            if (cur_vip_isr_count == save_vip_isr_count) {
                if (one_frame_time <= in_out_latency) {
                    in_out_latency -= one_frame_time;
                } else {
                    amp_trace("ERROR!!! 3D adjust mismatch %lld, %lld, %d, %lld\n",
                            cur_vip_isr_count, save_vip_isr_count, in_out_latency,
                            one_frame_time);
                    return;
                }
            }

            cur_vip_isr_count = save_vip_isr_count;
            cur_vpp_isr_count = vpp_isr_count;
        }
    } else {
        // Go to promise vpp and vip isr count adding one at the same count
        if ((0 == cur_vip_isr_count) && (0 == cur_vpp_isr_count)) {
            cur_vip_isr_count = save_vip_isr_count;
            cur_vpp_isr_count = vpp_isr_count;
        } else if ((cur_vip_isr_count + 1 == save_vip_isr_count) &&
                (cur_vpp_isr_count + 1 == vpp_isr_count)) {
            cur_vip_isr_count = save_vip_isr_count;
            cur_vpp_isr_count = vpp_isr_count;
        } else {
            if ((print_counter % 30) == 0){
                amp_trace("!!! isr mismatch vip(%lld, %lld), vpp(%lld, %lld)\n",
                        cur_vip_isr_count, save_vip_isr_count,
                        cur_vpp_isr_count, vpp_isr_count);
            }
            cur_vip_isr_count = save_vip_isr_count;
            cur_vpp_isr_count = vpp_isr_count;
            print_counter++;
        }
    }

#if 0
    if (-1 == vpp_4k_res) {
        vpp_FRST_VTOTAL_delta = m_resinfo_table[vpp_cpcb0_res].height - m_resinfo_table[vpp_cpcb0_res].active_height - 12;
        vpp_FRST_VTOTAL_delta *= m_line_count_max_adj_latency[vpp_cpcb0_res].line_count;
    } else {
        vpp_FRST_VTOTAL_delta = m_resinfo_table[vpp_4k_res].height - m_resinfo_table[vpp_4k_res].active_height - 12;
        vpp_FRST_VTOTAL_delta *= m_line_count_max_adj_latency[vpp_4k_res].line_count;
    }
    in_out_latency += vpp_FRST_VTOTAL_delta;
#endif

#if 0
    if (TG_CHANGE_STATE_DONE != tg_changed) {
        amp_trace("%d %d\n", tg_changed, in_out_latency);
    }
#endif
    switch (tg_changed) {
        case TG_CHANGE_STATE_CHECK: {
            if (-1 == vpp_4k_res) {
                adjust_cpcb0(in_out_latency);
            } else {
                adjust_4k(in_out_latency);
            }

            tg_changed = TG_CHANGE_STATE_SET_BACK_TO_NORMAL;
            break;
        }
        case TG_CHANGE_STATE_SET_BACK_TO_NORMAL: {
            if (-1 == vpp_4k_res) {
                adjust_cpcb0(LATENCY_EXPECTED);
            } else {
                adjust_4k(LATENCY_EXPECTED);
            }

            tg_changed = TG_CHANGE_STATE_WAIT_EFFECT;
            break;
        }
        case TG_CHANGE_STATE_WAIT_EFFECT: {
            // Wait two frame here. Check at the second frame
            wait_frame_count++;

            if (2 == wait_frame_count) {
                // reset the wait_frame_count
                wait_frame_count = 0;

                if (((LATENCY_EXPECTED - LATENCY_THRESHOLD) <= in_out_latency) &&
                    (in_out_latency <= (LATENCY_EXPECTED + LATENCY_THRESHOLD))) {

                    reset_adjust(0);
                    start_latency = in_out_latency;
                    tg_changed = TG_CHANGE_STATE_DONE;
                    amp_trace("start_latency %lld\n", start_latency);
                } else {
                   tg_changed = TG_CHANGE_STATE_CHECK;
                }
            }
            break;
        }
        case TG_CHANGE_STATE_DONE: {
            //record input/output clock drifting to adjust AVPLL
            spin_lock(&drift_countlock);

            vip_vpp_drift_info.valid = 1;
            vip_vpp_drift_info.start_latency = start_latency;
            vip_vpp_drift_info.drift_count = (in_out_latency - start_latency);    //current drifting
            vip_vpp_drift_info.total_drift_count += vip_vpp_drift_info.drift_count;
            vip_vpp_drift_info.frame_count += 1;
            if (((LATENCY_EXPECTED - LATENCY_THRESHOLD) <= in_out_latency) &&
                (in_out_latency <= (LATENCY_EXPECTED + LATENCY_THRESHOLD))) {
              vip_vpp_drift_info.latency_in_the_expected_range = 1;
            } else {
              vip_vpp_drift_info.latency_in_the_expected_range = 0;
            }
            spin_unlock(&drift_countlock);
            break;
        }
        default: {
            amp_trace("ERROR!!! unsupport state %d\n", tg_changed);
            break;
        }
    }
}
