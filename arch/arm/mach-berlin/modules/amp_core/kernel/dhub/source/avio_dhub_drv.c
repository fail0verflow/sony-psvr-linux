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
#include "api_avio_dhub.h"

HDL_dhub2d AG_dhubHandle;
HDL_dhub2d VPP_dhubHandle;
#ifdef CONFIG_MV_AMP_COMPONENT_VIP_ENABLE
#if (BERLIN_CHIP_VERSION >= BERLIN_BG2)
HDL_dhub2d VIP_dhubHandle;
#endif
#endif // CONFIG_MV_AMP_COMPONENT_VIP_ENABLE

#if (BERLIN_CHIP_VERSION == BERLIN_BG2_DTV_A0)
HDL_dhub2d VPP_Dh3_dhubHandle;
#endif

void DhubInitialization_128BytesAXIChannel(HDL_dhub2d *pdhubHandle, DHUB_channel_config *dhub_config, UNSG8 uchNumChannels)
{
#if (BERLIN_CHIP_VERSION == BERLIN_BG2_DTV_A0)
    HDL_semaphore *pSemHandle;
    SIGN32 chanId;
    UNSG8 uchCount = 0;

    pSemHandle = dhub_semaphore(&pdhubHandle->dhub);

    for(uchCount = 0; uchCount < uchNumChannels; uchCount++)
    {
        chanId = dhub_config[uchCount].chanId;

        dhub_channel_cfg(
                &pdhubHandle->dhub,                 /*! Handle to HDL_dhub !*/
                chanId,                     /*! Channel ID in $dHubReg !*/
                dhub_config[uchCount].chanCmdBase,      //UNSG32 baseCmd,   /*! Channel FIFO base address (byte address) for cmdQ !*/
                dhub_config[uchCount].chanDataBase,     //UNSG32 baseData,  /*! Channel FIFO base address (byte address) for dataQ !*/
                dhub_config[uchCount].chanCmdSize/16,   //SIGN32        depthCmd,           /*! Channel FIFO depth for cmdQ, in 64b word !*/
                dhub_config[uchCount].chanDataSize/16,  //SIGN32        depthData,          /*! Channel FIFO depth for dataQ, in 64b word !*/
                dhub_config[uchCount].chanMtuSize,                      /*! See 'dHubChannel.CFG.MTU', 0/1/2 for 8/32/128 bytes !*/
                dhub_config[uchCount].chanQos,                              /*! See 'dHubChannel.CFG.QoS' !*/
                dhub_config[uchCount].chanSelfLoop,                             /*! See 'dHubChannel.CFG.selfLoop' !*/
                dhub_config[uchCount].chanEnable,                               /*! 0 to disable, 1 to enable !*/
                0                               /*! Pass NULL to directly init dHub, or
                                                  Pass non-zero to receive programming sequence
                                                            in (adr,data) pairs
                                                            !*/
                );
        semaphore_cfg(pSemHandle, chanId, 1, 0);
    }
#endif
}

void DhubInitialization_128BytesAXI(SIGN32 cpuId, UNSG32 dHubBaseAddr, UNSG32 hboSramAddr, HDL_dhub2d *pdhubHandle, DHUB_channel_config *dhub_config, UNSG8 numOfChans)
{
#if (BERLIN_CHIP_VERSION == BERLIN_BG2_DTV_A0)
    HDL_semaphore *pSemHandle;
    SIGN32 i;
    SIGN32 chanId;

    //Initialize HDL_dhub with a $dHub BIU instance.
    dhub2d_hdl( hboSramAddr,            /*! Base address of dHub.HBO SRAM !*/
                dHubBaseAddr,           /*! Base address of a BIU instance of $dHub !*/
                pdhubHandle             /*! Handle to HDL_dhub2d !*/
            );
    //set up semaphore to trigger cmd done interrupt
    //note that this set of semaphores are different from the HBO semaphores
    //the ID must match the dhub ID because they are hardwired.
    pSemHandle = dhub_semaphore(&pdhubHandle->dhub);

    for (i = 0; i< numOfChans; i++) {
        //Configurate a dHub channel
        //note that in this function, it also configured right HBO channels(cmdQ and dataQ) and semaphores
        chanId = dhub_config[i].chanId;
        {
            dhub_channel_cfg(
                        &pdhubHandle->dhub,                 /*! Handle to HDL_dhub !*/
                        chanId,                     /*! Channel ID in $dHubReg !*/
                        dhub_config[chanId].chanCmdBase,        //UNSG32 baseCmd,   /*! Channel FIFO base address (byte address) for cmdQ !*/
                        dhub_config[chanId].chanDataBase,       //UNSG32 baseData,  /*! Channel FIFO base address (byte address) for dataQ !*/
                        dhub_config[chanId].chanCmdSize/16, //SIGN32        depthCmd,           /*! Channel FIFO depth for cmdQ, in 64b word !*/
                        dhub_config[chanId].chanDataSize/16,    //SIGN32        depthData,          /*! Channel FIFO depth for dataQ, in 64b word !*/
                        dhub_config[chanId].chanMtuSize,                        /*! See 'dHubChannel.CFG.MTU', 0/1/2 for 8/32/128 bytes !*/
                        dhub_config[chanId].chanQos,                                /*! See 'dHubChannel.CFG.QoS' !*/
                        dhub_config[chanId].chanSelfLoop,                               /*! See 'dHubChannel.CFG.selfLoop' !*/
                        dhub_config[chanId].chanEnable,                             /*! 0 to disable, 1 to enable !*/
                        0                               /*! Pass NULL to directly init dHub, or
                                                            Pass non-zero to receive programming sequence
                                                            in (adr,data) pairs
                                                            !*/
                        );
            // setup interrupt for channel chanId
            //configure the semaphore depth to be 1
            semaphore_cfg(pSemHandle, chanId, 1, 0);
        }
    }
#endif
}

static int getDhubCMDDiv(HDL_dhub2d *pdhubHandle)
{
    if (pdhubHandle == &VPP_dhubHandle) {
#if (BERLIN_CHIP_VERSION >= BERLIN_BG4_CD)
        return 16;
#else
        return 8;
#endif
    }
    if (pdhubHandle == &AG_dhubHandle) {
        return 8;
    }

    return 8;
}

/******************************************************************************************************************
 *	Function: DhubInitialization
 *	Description: Initialize DHUB .
 *	Parameter : cpuId ------------- cpu ID
 *             dHubBaseAddr -------------  dHub Base address.
 *             hboSramAddr ----- Sram Address for HBO.
 *             pdhubHandle ----- pointer to 2D dhubHandle
 *             dhub_config ----- configuration of AG
 *             numOfChans	 ----- number of channels
 *	Return:		void
******************************************************************************************************************/
void DhubInitialization(SIGN32 cpuId, UNSG32 dHubBaseAddr, UNSG32 hboSramAddr,
            HDL_dhub2d *pdhubHandle, DHUB_channel_config *dhub_config,SIGN32 numOfChans)
{
	HDL_semaphore *pSemHandle;
	SIGN32 i;
	SIGN32 chanId;
    SIGN32 cmdDiv = 8;

    cmdDiv = getDhubCMDDiv(pdhubHandle);
    if (!cmdDiv) {
        return;
    }

	//Initialize HDL_dhub with a $dHub BIU instance.
	dhub2d_hdl(	hboSramAddr,			/*!	Base address of dHub.HBO SRAM !*/
			 	dHubBaseAddr,			/*!	Base address of a BIU instance of $dHub !*/
			 	pdhubHandle				/*!	Handle to HDL_dhub2d !*/
			);
	//set up semaphore to trigger cmd done interrupt
	//note that this set of semaphores are different from the HBO semaphores
	//the ID must match the dhub ID because they are hardwired.
	pSemHandle = dhub_semaphore(&pdhubHandle->dhub);

	for (i = 0; i< numOfChans; i++) {
		//Configurate a dHub channel
		//note that in this function, it also configured right HBO channels(cmdQ and dataQ) and semaphores
		chanId = dhub_config[i].chanId;
		{
			dhub_channel_cfg(
						&pdhubHandle->dhub,					/*!	Handle to HDL_dhub !*/
						chanId,						/*!	Channel ID in $dHubReg !*/
						dhub_config[chanId].chanCmdBase,		//UNSG32 baseCmd,	/*!	Channel FIFO base address (byte address) for cmdQ !*/
						dhub_config[chanId].chanDataBase,		//UNSG32 baseData,	/*!	Channel FIFO base address (byte address) for dataQ !*/
						dhub_config[chanId].chanCmdSize/cmdDiv,	//SIGN32		depthCmd,			/*!	Channel FIFO depth for cmdQ, in 64b word !*/
						dhub_config[chanId].chanDataSize/cmdDiv,	//SIGN32		depthData,			/*!	Channel FIFO depth for dataQ, in 64b word !*/
						dhub_config[chanId].chanMtuSize,						/*!	See 'dHubChannel.CFG.MTU', 0/1/2 for 8/32/128 bytes !*/
						dhub_config[chanId].chanQos,								/*!	See 'dHubChannel.CFG.QoS' !*/
						dhub_config[chanId].chanSelfLoop,								/*!	See 'dHubChannel.CFG.selfLoop' !*/
						dhub_config[chanId].chanEnable,								/*!	0 to disable, 1 to enable !*/
						0								/*!	Pass NULL to directly init dHub, or
															Pass non-zero to receive programming sequence
															in (adr,data) pairs
															!*/
						);
			// setup interrupt for channel chanId
			//configure the semaphore depth to be 1
			semaphore_cfg(pSemHandle, chanId, 1, 0);
#if 0
			// enable interrupt from this semaphore
		  	semaphore_intr_enable (
			pSemHandle, // semaphore handler
		    	chanId,
			0,       // empty
		    	1,       // full
			0,       // almost_empty
		    	0,       // almost_full
			cpuId        // 0~2, depending on which CPU the interrupt is enabled for.
		  	);
#endif
		}
	}
}

/******************************************************************************************************************
 *	Function: DhubChannelClear
 *	Description: Clear corresponding DHUB channel.
 *	Parameter:	hdl  ---------- handle to HDL_dhub
 *			id   ---------- channel ID in dHubReg
 *			cfgQ ---------- pass null to directly init dhub, or pass non-zero to receive programming
 *					sequence in (adr, data) pairs
 *	Return:		void
******************************************************************************************************************/
void DhubChannelClear(void *hdl, SIGN32 id, T64b cfgQ[])
{
	UNSG32	cmdID = dhub_id2hbo_cmdQ(id);
	UNSG32	dataID = dhub_id2hbo_data(id);
	HDL_dhub *dhub = (HDL_dhub *)hdl;
	HDL_hbo *hbo = &(dhub->hbo);

	/* 1.Software stops the command queue in HBO (please refer to HBO.sxw for details) */
	hbo_queue_enable(hbo, cmdID, 0, cfgQ);
	/* 2.Software stops the channel in dHub by writing zero to  dHubChannel.START.EN */
	dhub_channel_enable(dhub, id, 0, cfgQ);
	/* 3.Software clears the channel in dHub by writing one to  dHubChannel.CLEAR.EN */
	dhub_channel_clear(dhub, id);
	/* 4.Software waits for the register bits dHubChannel.PENDING.ST and dHubChannel.BUSY.ST to be 0 */
	dhub_channel_clear_done(dhub, id);
	/* 5.Software stops and clears the command queue */
	hbo_queue_enable(hbo, cmdID, 0, cfgQ);
	hbo_queue_clear(hbo, cmdID);
	/* 6.Software wait for the corresponding busy bit to be 0 */
	hbo_queue_clear_done(hbo, cmdID);
	/* 7.Software stops and clears the data queue */
	hbo_queue_enable(hbo, dataID, 0, cfgQ);
	hbo_queue_clear(hbo, dataID);
	/* 8.Software wait for the corresponding data Q busy bit to be 0 */
	hbo_queue_clear_done(hbo, dataID);
	/* 9.Software enable dHub and HBO */
	dhub_channel_enable(dhub, id, 1, cfgQ);
	hbo_queue_enable(hbo, cmdID, 1, cfgQ);
	hbo_queue_enable(hbo, dataID, 1, cfgQ);
}
