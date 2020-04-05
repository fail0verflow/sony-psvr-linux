/*******************************************************************************************
*	Copyright (C) 2007-2011
*	Copyright © 2007 Marvell International Ltd.
*
*	This program is free software; you can redistribute it and/or
*	modify it under the terms of the GNU General Public License
*	as published by the Free Software Foundation; either version 2
*	of the License, or (at your option) any later version.
*
*	This program is distributed in the hope that it will be useful,
*	but WITHOUT ANY WARRANTY; without even the implied warranty of:
*	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*	GNU General Public License for more details.
*
*	You should have received a copy of the GNU General Public License
*	along with this program; if not, write to the Free Software
*	Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*
*
*	$Log: vsys#comm#cmodel#hal#drv#hal_dhub.c,v $
*	Revision 1.7  2007-11-26 09:12:33-08  kbhatt
*	merge changes with version of ctsai
*
*	Revision 1.6  2007-11-18 00:11:43-08  ctsai
*	disabled redundant io32wr & io32rd
*
*	Revision 1.5  2007-11-12 14:15:57-08  bofeng
*	removed #define    __CUSTOMIZED_IO__ as default
*	restore the modification for io32rd and io32wr
*
*	Revision 1.4  2007-10-30 11:08:35-07  ctsai
*	changed printf to xdbg
*
*	Revision 1.3  2007-10-24 21:41:16-07  kbhatt
*	add structures to api_dhub.h
*
*	Revision 1.2  2007-10-12 21:36:55-07  oussama
*	adapted the env to use Alpha's Cmodel driver
*
*	Revision 1.1  2007-08-29 20:27:00-07  lsha
*	Initial revision.
*
*
*	DESCRIPTION:
*	OS independent layer for dHub/HBO/SemaHub APIs.
*
********************************************************************************************/
#include	"api_dhub.h"
#include	"dHub.h"
#include	"avio.h"

#ifdef	__INLINE_ASM_ARM__
#define	IO32RD(d, a)	__asm__ __volatile__ ("ldr %0, [%1]" : "=r" (d) : "r" (a))
#define	IO32WR(d, a)	__asm__ __volatile__ ("str %0, [%1]" : : "r" (d), "r" (a) : "memory")
			"ldrd r4, [%2] \n\t" "mov %0, r4    \n\t" "mov %1, r5    \n\t"	\
					: "=r" (d0), "=r" (d1) : "r" (a) : "r4", "r5")
#else
#define	IO32RD(d, a)		do{	(d) = *(UNSG32*)(a); }while(0)
#define	IO32WR(d, a)		do{	*(UNSG32*)(a) = (d); }while(0)
#endif

#define IO32CFG(cfgQ, i, a, d)  do{ if(cfgQ) { (cfgQ)[i][0] = (d); (cfgQ)[i][1] = (a); } \
                                    else IO32WR(d, a); \
                                    (i) ++; \
                                    }while(0)

#define	bTST(x, b)		(((x) >> (b)) & 1)
#define	ModInc(x, i, mod)	do { (x) += (i); while((x) >= (mod)) (x) -= (mod); } while(0)

#ifdef DHUB_DBG
#define xdbg(fmt, args...)		printk(fmt, args...)
#else
#define xdbg(fmt, args...)		do { } while (0)
#endif

/**	SECTION - API definitions for $SemaHub
 */
/******************************************************************************
*	Function: semaphore_hdl
*	Description: Initialize HDL_semaphore with a $SemaHub BIU instance.
*******************************************************************************/
void	semaphore_hdl(
		UNSG32	ra,	/*	Base address of a BIU instance of $SemaHub */
		void	*hdl	/*	Handle to HDL_semaphore */
		)
{
	HDL_semaphore		*sem = (HDL_semaphore*)hdl;

	sem->ra = ra;
	/**	ENDOFFUNCTION: semaphore_hdl **/
}

/******************************************************************************
*	Function: semaphore_cfg
*	Description: Configurate a semaphore's depth & reset pointers.
*	Return:	UNSG32		Number of (adr,pair) added to cfgQ
*******************************************************************************/
UNSG32	semaphore_cfg(
		void	*hdl,		/*Handle to HDL_semaphore */
		SIGN32	id,		/*Semaphore ID in $SemaHub */
		SIGN32	depth,		/*Semaphore (virtual FIFO) depth */
		T64b	cfgQ[]		/*Pass NULL to directly init SemaHub, or
					Pass non-zero to receive programming sequence
					in (adr,data) pairs
					*/
		)
{
	HDL_semaphore		*sem = (HDL_semaphore*)hdl;

	T32Semaphore_CFG	cfg;
	UNSG32 i = 0, a;
	a = sem->ra + RA_SemaHub_ARR + id*sizeof(SIE_Semaphore);

	cfg.u32 = 0; cfg.uCFG_DEPTH = sem->depth[id] = depth;
	IO32CFG(cfgQ, i, a + RA_Semaphore_CFG, cfg.u32);

	return i;
	/**	ENDOFFUNCTION: semaphore_cfg **/
}

/******************************************************************************
*	Function: semaphore_intr_enable
*	Description: Configurate interrupt enable bits of a semaphore.
*******************************************************************************/
void	semaphore_intr_enable(
		void		*hdl,		/*Handle to HDL_semaphore */
		SIGN32		id,		/*Semaphore ID in $SemaHub */
		SIGN32		empty,		/*Interrupt enable for CPU at condition 'empty' */
		SIGN32		full,		/*Interrupt enable for CPU at condition 'full' */
		SIGN32		almostEmpty,	/*Interrupt enable for CPU at condition 'almostEmpty' */
		SIGN32		almostFull,	/*Interrupt enable for CPU at condition 'almostFull' */
		SIGN32		cpu		/*CPU ID (0/1/2) */
		)
{
	HDL_semaphore		*sem = (HDL_semaphore*)hdl;
	T32SemaINTR_mask	mask;
	UNSG32 a;
	a = sem->ra + RA_SemaHub_ARR + id*sizeof(SIE_Semaphore);

	mask.u32 = 0;
	mask.umask_empty = empty;
	mask.umask_full = full;
	mask.umask_almostEmpty = almostEmpty;
	mask.umask_almostFull = almostFull;

	IO32WR(mask.u32, a + RA_Semaphore_INTR + cpu*sizeof(SIE_SemaINTR));
	/**	ENDOFFUNCTION: semaphore_intr_enable **/
}

/******************************************************************************
*	Function: semaphore_query
*	Description: Query current status (counter & pointer) of a semaphore.
*	Return:	UNSG32		Current available unit level
*******************************************************************************/
UNSG32	semaphore_query(
		void	*hdl,	/*	Handle to HDL_semaphore */
		SIGN32	id,	/*	Semaphore ID in $SemaHub */
		SIGN32	master,	/*	0/1 as procuder/consumer query */
		UNSG32	*ptr	/*	Non-zero to receive semaphore r/w pointer */
		)
{
	HDL_semaphore		*sem = (HDL_semaphore*)hdl;
	T32SemaQuery_RESP	resp;
	T32SemaQueryMap_ADDR map;

	map.u32 = 0; map.uADDR_ID = id; map.uADDR_master = master;
	IO32RD(resp.u32, sem->ra + RA_SemaHub_Query + map.u32);

	if (ptr) *ptr = resp.uRESP_PTR;
	return resp.uRESP_CNT;
	/**	ENDOFFUNCTION: semaphore_query **/
}

/******************************************************************************
*	Function: semaphore_push
*	Description: Producer semaphore push.
*******************************************************************************/
void	semaphore_push(
			void	*hdl,	/*	Handle to HDL_semaphore */
			SIGN32	id,	/*	Semaphore ID in $SemaHub */
			SIGN32	delta	/*	Delta to push as a producer */
			)
{
	HDL_semaphore		*sem = (HDL_semaphore*)hdl;
	T32SemaHub_PUSH		push;

	push.u32 = 0; push.uPUSH_ID = id; push.uPUSH_delta = delta;
	IO32WR(push.u32, sem->ra + RA_SemaHub_PUSH);
	/**	ENDOFFUNCTION: semaphore_push **/
}

/**	SECTION - API definitions for $HBO
*/
/******************************************************************************
*	Function: hbo_hdl
*	Description: Initialize HDL_hbo with a $HBO BIU instance.
*******************************************************************************/
void	hbo_hdl(
		UNSG32	mem,		/*Base address of HBO SRAM */
		UNSG32	ra,		/*Base address of a BIU instance of $HBO */
		void	*hdl		/*Handle to HDL_hbo */
		)
{
	HDL_hbo				*hbo = (HDL_hbo*)hdl;
	HDL_semaphore		*fifoCtl = &(hbo->fifoCtl);

	hbo->mem = mem; hbo->ra = ra;
	semaphore_hdl(ra + RA_HBO_FiFoCtl, fifoCtl);
	/**	ENDOFFUNCTION: hbo_hdl **/
}

/******************************************************************************
*	Function: hbo_fifoCtl
*	Description: Get HDL_semaphore pointer from a HBO instance.
*	Return:	void*		Handle for HBO.FiFoCtl
*******************************************************************************/
void*	hbo_fifoCtl(
		void	*hdl		/*Handle to HDL_hbo */
		)
{
	HDL_hbo				*hbo = (HDL_hbo*)hdl;
	HDL_semaphore		*fifoCtl = &(hbo->fifoCtl);

	return fifoCtl;
	/**	ENDOFFUNCTION: hbo_fifoCtl **/
}

/******************************************************************************
*	Function: hbo_queue_cfg
*	Description: Configurate a FIFO's base, depth & reset pointers.
*	Return:	UNSG32		Number of (adr,pair) added to cfgQ
*******************************************************************************/
UNSG32	hbo_queue_cfg(
		void	*hdl,		/*Handle to HDL_hbo */
		SIGN32	id,		/*Queue ID in $HBO */
		UNSG32	base,		/*Channel FIFO base address (byte address) */
		SIGN32	depth,		/*Channel FIFO depth, in 64b word */
		SIGN32	enable,		/*0 to disable, 1 to enable */
		T64b	cfgQ[]		/*Pass NULL to directly init HBO, or
					Pass non-zero to receive programming sequence
					in (adr,data) pairs
					*/
		)
{
	HDL_hbo				*hbo = (HDL_hbo*)hdl;
	HDL_semaphore		*fifoCtl = &(hbo->fifoCtl);
	T32FiFo_CFG			cfg;
	UNSG32 i = 0, a;
	a = hbo->ra + RA_HBO_ARR + id*sizeof(SIE_FiFo);
	IO32CFG(cfgQ, i, a + RA_FiFo_START, 0);

	cfg.u32 = 0; cfg.uCFG_BASE = hbo->base[id] = base;
	IO32CFG(cfgQ, i, a + RA_FiFo_CFG, cfg.u32);

	i += semaphore_cfg(fifoCtl, id, depth, cfgQ ? (cfgQ + i) : NULL);
	IO32CFG(cfgQ, i, a + RA_FiFo_START, enable);

	return i;
	/**	ENDOFFUNCTION: hbo_queue_cfg **/
}

/******************************************************************************
*	Function: hbo_queue_enable
*	Description: HBO FIFO enable/disable.
*	Return:	UNSG32	Number of (adr,pair) added to cfgQ
*******************************************************************************/
UNSG32	hbo_queue_enable(
		void	*hdl,		/*Handle to HDL_hbo */
		SIGN32	id,		/*Queue ID in $HBO */
		SIGN32	enable,		/*0 to disable, 1 to enable */
		T64b	cfgQ[]		/*Pass NULL to directly init HBO, or
					Pass non-zero to receive programming sequence
					in (adr,data) pairs
					*/
		)
{
	HDL_hbo			*hbo = (HDL_hbo*)hdl;
	UNSG32 i = 0, a;
	a = hbo->ra + RA_HBO_ARR + id*sizeof(SIE_FiFo);

	IO32CFG(cfgQ, i, a + RA_FiFo_START, enable);
	return i;
	/**	ENDOFFUNCTION: hbo_queue_enable **/
}

/******************************************************************************
*	Function: hbo_queue_clear
*	Description: Issue HBO FIFO clear (will NOT wait for finish).
*******************************************************************************/
void	hbo_queue_clear(
		void	*hdl,		/*	Handle to HDL_hbo */
		SIGN32	id		/*	Queue ID in $HBO */
		)
{
	HDL_hbo				*hbo = (HDL_hbo*)hdl;
	UNSG32 a;
	a = hbo->ra + RA_HBO_ARR + id*sizeof(SIE_FiFo);

	IO32WR(1, a + RA_FiFo_CLEAR);
	/**	ENDOFFUNCTION: hbo_queue_enable **/
}

/******************************************************************************
*	Function: hbo_queue_busy
*	Description: Read HBO 'BUSY' status for all channel FIFOs.
*	Return:	UNSG32		'BUSY' status bits of all channels
*******************************************************************************/
UNSG32	hbo_queue_busy(
		void	*hdl		/*	Handle to HDL_hbo */
		)
{
	HDL_hbo				*hbo = (HDL_hbo*)hdl;
	UNSG32 d;

	IO32RD(d, hbo->ra + RA_HBO_BUSY);
	return d;
	/**	ENDOFFUNCTION: hbo_queue_busy **/
}

/******************************************************************************
*	Function: hbo_queue_write
*	Description: Write a number of 64b data & push FIFO to HBO SRAM.
*	Return:	UNSG32		Number of (adr,pair) added to cfgQ, or (when cfgQ==NULL)
*				0 if there're not sufficient space in FIFO
*******************************************************************************/
UNSG32	hbo_queue_write(
		void	*hdl,		/*	Handle to HDL_hbo */
		SIGN32	id,		/*	Queue ID in $HBO */
		SIGN32	n,		/*	Number 64b entries to write */
		T64b	data[],		/*	Write data */
		T64b	cfgQ[],		/*	Pass NULL to directly update HBO, or
						Pass non-zero to receive programming sequence
						in (adr,data) pairs
						*/
		UNSG32	*ptr		/*	Pass in current FIFO pointer (in 64b word),
						& receive updated new pointer,
						Pass NULL to read from HW
						*/
		)
{
	HDL_hbo				*hbo = (HDL_hbo*)hdl;
    SIGN32 i, p, depth;
	UNSG32 base, j = 0;

	base = hbo->mem + hbo->base[id]; depth = hbo->fifoCtl.depth[id];
	if (!ptr) {
		i = hbo_queue_query(hdl, id, SemaQueryMap_ADDR_master_producer, &p);
		if(i > depth - n)
			return 0;
	} else  {
		p = *ptr;
	}

	for (i = 0; i < n; i ++) {
		IO32CFG(cfgQ, j, base + p*8, data[i][0]);
		IO32CFG(cfgQ, j, base + p*8 + 4, data[i][1]);
		ModInc(p, 1, depth);
				}
	if (!cfgQ)
		hbo_queue_push(hdl, id, n);
	else {
		T32SemaHub_PUSH	push;
		push.u32 = 0; push.uPUSH_ID = id; push.uPUSH_delta = n;
		IO32CFG(cfgQ, j, hbo->fifoCtl.ra + RA_SemaHub_PUSH, push.u32);
				}
	if (ptr) *ptr = p;
	return j;
	/**	ENDOFFUNCTION: hbo_queue_write **/
}

/**	SECTION - API definitions for $dHubReg
*/
/******************************************************************************
*	Function: dhub_hdl
*	Description: Initialize HDL_dhub with a $dHub BIU instance.
*******************************************************************************/
void	dhub_hdl(
		UNSG32	mem,		/*	Base address of dHub.HBO SRAM */
		UNSG32	ra,		/*	Base address of a BIU instance of $dHub */
		void	*hdl		/*	Handle to HDL_dhub */
		)
{
	HDL_dhub		*dhub = (HDL_dhub*)hdl;
	HDL_hbo			*hbo = &(dhub->hbo);
	HDL_semaphore		*semaHub = &(dhub->semaHub);

	dhub->ra = ra;
	semaphore_hdl(ra + RA_dHubReg_SemaHub, semaHub);
	hbo_hdl(mem, ra + RA_dHubReg_HBO, hbo );
	/**	ENDOFFUNCTION: dhub_hdl **/
}

/******************************************************************************
*	Function: dhub_semaphore
*	Description: Get HDL_semaphore pointer from a dHub instance.
*	Return:	void*	Handle for dHub.SemaHub
*******************************************************************************/
void*	dhub_semaphore(
		void	*hdl		/*	Handle to HDL_dhub */
		)
{
	HDL_dhub			*dhub = (HDL_dhub*)hdl;
	HDL_semaphore		*semaHub = &(dhub->semaHub);

	return semaHub;
	/**	ENDOFFUNCTION: dhub_semaphore **/
}

/******************************************************************************
*	Function: dhub_channel_cfg
*	Description: Configurate a dHub channel.
*	Return:	UNSG32		Number of (adr,pair) added to cfgQ, or (when cfgQ==NULL)
*				0 if either cmdQ or dataQ in HBO is still busy
*******************************************************************************/
UNSG32	dhub_channel_cfg(
		void	*hdl,		/*Handle to HDL_dhub */
		SIGN32	id,		/*Channel ID in $dHubReg */
		UNSG32	baseCmd,	/*Channel FIFO base address (byte address) for cmdQ */
		UNSG32	baseData,	/*Channel FIFO base address (byte address) for dataQ */
		SIGN32	depthCmd,	/*Channel FIFO depth for cmdQ, in 64b word */
		SIGN32	depthData,	/*Channel FIFO depth for dataQ, in 64b word */
		SIGN32	MTU,		/*See 'dHubChannel.CFG.MTU' */
		SIGN32	QoS,		/*See 'dHubChannel.CFG.QoS' */
		SIGN32	selfLoop,	/*See 'dHubChannel.CFG.selfLoop' */
		SIGN32	enable,		/*0 to disable, 1 to enable */
		T64b	cfgQ[]		/*Pass NULL to directly init dHub, or
					Pass non-zero to receive programming sequence
					in (adr,data) pairs
					*/
		)
{
	HDL_dhub			*dhub = (HDL_dhub*)hdl;
	HDL_hbo				*hbo = &(dhub->hbo);
	T32dHubChannel_CFG	cfg;
	UNSG32 i = 0, a, busyStatus, cmdID = dhub_id2hbo_cmdQ(id), dataID = dhub_id2hbo_data(id);

	xdbg ("hal_dhub::  value of id is %0d \n" , id ) ;
	xdbg ("hal_dhub::  value of baseCmd   is %0d \n" , baseCmd ) ;
	xdbg ("hal_dhub::  value of baseData  is %0d \n" , baseData ) ;
	xdbg ("hal_dhub::  value of depthCmd  is %0d \n" , depthCmd ) ;
	xdbg ("hal_dhub::  value of depthData is %0d \n" , depthData ) ;
	xdbg ("hal_dhub::  value of MTU       is %0d \n" , MTU ) ;
	xdbg ("hal_dhub::  value of QOS       is %0d \n" , QoS ) ;
	xdbg ("hal_dhub::  value of SelfLoop  is %0d \n" , selfLoop ) ;
	xdbg ("hal_dhub::  value of Enable    is %0d \n" , enable ) ;

	if (!cfgQ) {
		hbo_queue_enable(hbo,  cmdID, 0, NULL);
		hbo_queue_clear(hbo,  cmdID);
		hbo_queue_enable(hbo, dataID, 0, NULL);
		hbo_queue_clear(hbo, dataID);
		busyStatus = hbo_queue_busy(hbo);
		//if(bTST(busyStatus, cmdID) || bTST(busyStatus, dataID))
		//	return 0;
						}
	a = dhub->ra + RA_dHubReg_ARR + id*sizeof(SIE_dHubChannel);
	xdbg ("hal_dhub::  value of Channel Addr    is %0x \n" , a ) ;
	IO32CFG(cfgQ, i, a + RA_dHubChannel_START, 0);

	cfg.u32 = 0; cfg.uCFG_MTU = MTU; cfg.uCFG_QoS = QoS; cfg.uCFG_selfLoop = selfLoop;
	switch (MTU) {
		case dHubChannel_CFG_MTU_8byte	: dhub->MTUb[id] = 3; break;
		case dHubChannel_CFG_MTU_32byte	: dhub->MTUb[id] = 5; break;
		case dHubChannel_CFG_MTU_128byte: dhub->MTUb[id] = 7; break;
		case dHubChannel_CFG_MTU_1024byte : dhub->MTUb[id] = 10; break;
						}
	xdbg ("hal_dhub::  addr of ChannelCFG is %0x data is %0x \n",
				a + RA_dHubChannel_CFG , cfg.u32  ) ;
	IO32CFG(cfgQ, i, a + RA_dHubChannel_CFG, cfg.u32);

	i += hbo_queue_cfg(hbo,  cmdID, baseCmd , depthCmd,
				enable, cfgQ ? (cfgQ + i) : NULL);
	i += hbo_queue_cfg(hbo, dataID, baseData, depthData,
				enable, cfgQ ? (cfgQ + i) : NULL);
	xdbg ("hal_dhub::  addr of ChannelEN is %0x data is %0x \n" ,
				a + RA_dHubChannel_START , enable  ) ;
	IO32CFG(cfgQ, i, a + RA_dHubChannel_START, enable);

	return i;
	/**	ENDOFFUNCTION: dhub_channel_cfg **/
}

/******************************************************************************
*	Function: dhub_channel_write_cmd
*	Description: Write a 64b command for a dHub channel.
*	Return:	UNSG32		Number of (adr,pair) added to cfgQ if success, or
*				0 if there're not sufficient space in FIFO
*******************************************************************************/
UNSG32	dhub_channel_write_cmd(
		void	*hdl,		/*Handle to HDL_dhub */
		SIGN32	id,		/*Channel ID in $dHubReg */
		UNSG32	addr,		/*CMD: buffer address */
		SIGN32	size,		/*CMD: number of bytes to transfer */
		SIGN32	semOnMTU,	/*CMD: semaphore operation at CMD/MTU (0/1) */
		SIGN32	chkSemId,	/*CMD: non-zero to check semaphore */
		SIGN32	updSemId,	/*CMD: non-zero to update semaphore */
		SIGN32	interrupt,	/*CMD: raise interrupt at CMD finish */
		T64b	cfgQ[],		/*Pass NULL to directly update dHub, or
					Pass non-zero to receive programming sequence
					in (adr,data) pairs
					*/
		UNSG32	*ptr		/*Pass in current cmdQ pointer (in 64b word),
					& receive updated new pointer,
					Pass NULL to read from HW
					*/
		)
{
	HDL_dhub			*dhub = (HDL_dhub*)hdl;
	HDL_hbo				*hbo = &(dhub->hbo);
	SIE_dHubCmd			cmd;
	SIGN32 i;

	cmd.ie_HDR.u32dHubCmdHDR_DESC = 0;
	i = size >> dhub->MTUb[id];
	if((i << dhub->MTUb[id]) < size)
		cmd.ie_HDR.uDESC_size = size;
	else {
		cmd.ie_HDR.uDESC_sizeMTU = 1;
		cmd.ie_HDR.uDESC_size = i;
						}
	cmd.ie_HDR.uDESC_chkSemId = chkSemId; cmd.ie_HDR.uDESC_updSemId = updSemId;
	cmd.ie_HDR.uDESC_semOpMTU = semOnMTU; cmd.ie_HDR.uDESC_interrupt = interrupt;
	cmd.uMEM_addr = addr;

	return hbo_queue_write(hbo, dhub_id2hbo_cmdQ(id), 1, (T64b*)&cmd, cfgQ, ptr);
	/**	ENDOFFUNCTION: dhub_channel_write_cmd **/
}
