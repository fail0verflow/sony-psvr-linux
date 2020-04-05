#include <linux/linkage.h>
#include <linux/threads.h>
#include <asm/asm-offsets.h>
#include <asm/assembler.h>

	.text

#define	SM_Send_MsgQ		(0x04004000-512*4 - 0x04000000 + 0xF7FA0000)
#define SM_MSG_SIZE		32
#define SM_MSGQ_TOTAL_SIZE	512
#define SM_MSGQ_HEADER_SIZE	SM_MSG_SIZE
#define SM_MSGQ_SIZE		(SM_MSGQ_TOTAL_SIZE - SM_MSGQ_HEADER_SIZE )
#define MCBASE			0xF7CB0000
ENTRY(berlin_enter_ddrsr)
#if defined(CONFIG_BERLIN2) || defined(CONFIG_BERLIN2CT)
	/*
	 * Block all data requests.:
	 * SDRAM_Control_Register_14.Block_all_data_req: 1
	 */
	mov	r6, #1
	mov	r2, #(MCBASE & 0xFF000000)
	orr	r2, r2, #(MCBASE & 0x00FF0000)
	orr	r1, r2, #0x68
	str	r6, [r1]

	/*
	 * flush write buffer:
	 * User_Initiated_Command_Register_0.user_wcb_drain_req: write 1
	 */
	mov	r6, #2
	orr	r1, r2, #0x160
	str	r6, [r1]

	/* Check Memory_Controller_Status_Register.wcb_empty */
	orr	r1, r2, #0x4
loop1:
	ldr	r6, [r1]
	and	r6, r6, #0x40
	cmp	r6, #0x40
	bne	loop1

	/*
	 * Stop scheduler scheduling regular requests from BQ:
	 * SDRAM_Control_Register_14.halt_scheduler: 1
	 */
	mov	r6, #3
	orr	r1, r2, #0x68
	str	r6, [r1]

	/*
	 * turn off ODT:
	 * SDRAM_Configuration_Register_Type_1.RTT#: 1->0
	 */
	orr	r1, r2, #0x20
	ldr	r6, [r1]
	bic	r6, r6, #0xE0000
	str	r6, [r1]

	mov	r6, #0x200
	orr	r6, r6, #0x0F000000
	orr	r1, r2, #0x160
	str	r6, [r1]

	/*
	 * Enter Self Refresh:
	 * User_Initiated_Command_Register_0.user_sr_req: write b01
	 */
	mov	r6, #0x100000
1:	subs	r6, r6, #1
	bne	1b

	mov	r6, #0x40
	orr	r6, r6, #0x0F000000
	orr	r1, r2, #0x160
	str	r6, [r1]

	mov	r6, #0x100000
2:	subs	r6, r6, #1
	bne	2b

	/* wait DDR enter self-refresh done */
	orr	r1, r2, #0x8
loop2:
	ldr	r6, [r1]
	and	r6, r6, #0x11
	cmp	r6, #0x11
	bne	loop2
#else
	/*
	 * Block all data requests.:
	 * SDRAM_Control_Register_14.Block_all_data_req: 1
	 */
	mov	r2, #(MCBASE & 0xFF000000)
	orr	r2, r2, #(MCBASE & 0x00FF0000)
	orr	r1, r2, #0x44
	ldr	r6, [r1]
	orr	r6, r6, #0x10000
	str	r6, [r1]

	/*
	 * flush write buffer:
	 * User_Initiated_Command_Register_0.user_wcb_drain_req: write 1
	 */
	mov	r6, #0x30000002
	orr	r1, r2, #0x20
	str	r6, [r1]

	/* Check Memory_Controller_Status_Register.wcb_empty */
	orr	r1, r2, #0x4
loop1:
	ldr	r6, [r1]
	and	r6, r6, #0x8
	cmp	r6, #0x8
	bne	loop1

	/*
	 * Stop scheduler scheduling regular requests from BQ:
	 * SDRAM_Control_Register_14.halt_scheduler: 1
	 */
	orr	r1, r2, #0x44
	ldr	r6, [r1]
	orr	r6, r6, #0x10000
	orr	r6, r6, #0x2
	str	r6, [r1]

	/*
	 * turn off ODT:
	 * SDRAM_Configuration_Register_Type_1.RTT#: 1->0
	 */
	mov	r6, #0x0
	orr	r1, r2, #0x310
	str	r6, [r1]
	orr	r1, r2, #0xB10
	str	r6, [r1]

	mov	r6, #0x3F000000
	orr	r6, r6, #0x200
	orr	r1, r2, #0x20
	str	r6, [r1]

	/*
	 * Enter Self Refresh:
	 */
	mov	r6, #0x10000
1:	subs	r6, r6, #1
	bne	1b

	mov	r6, #0x3F000000
	orr	r6, r6, #0x40
	orr	r1, r2, #0x20
	str	r6, [r1]

	mov	r6, #0x10000
2:	subs	r6, r6, #1
	bne	2b

	/* wait DDR enter self-refresh done */
	orr	r1, r2, #0x8
	mov	r2, #0x50000
	orr	r2, r2, #0x5
loop2:
	ldr	r6, [r1]
	and	r6, r6, r2
	cmp	r6, r2
	bne	loop2
#endif

	/* MsgQ = SM_Send_MsgQ */
	mov	r6, #(SM_Send_MsgQ & 0xff000000)
	orr	r6, r6, #(SM_Send_MsgQ & 0x00ff0000)
	orr	r6, r6, #(SM_Send_MsgQ & 0x0000ff00)
	orr	r6, r6, #(SM_Send_MsgQ & 0x000000ff)
	ldr	r1, [r6]
	/* r2 = m_Queue */
	add	r2, r6, #SM_MSGQ_HEADER_SIZE
	/* r2 = m_Quene[MsgQ->m_iWrite] */
	add	r2, r2, r1
	/* back up */
	mov	r5, r1
	mov	r3, #4
	lsl	r3, #16
	/* iModuleID = MV_SM_ID_POWER */
	add	r3, r3, #5
	/* save iModuleID & len */
	str	r3, [r2], #4

	/* save message content */
	str	r0, [r2], #4

	/*
	 * MsgQ->m_iWrite += SM_MSG_SIZE;
	 * if( MsgQ->m_iWrite >= SM_MSGQ_SIZE )
	 * {
	 * 	MsgQ->m_iWrite -= SM_MSGQ_SIZE;
	 * }
	 */

	add	r5, r5, #SM_MSG_SIZE
	cmp	r5, #SM_MSGQ_SIZE
	subcs	r5, r5, #SM_MSGQ_SIZE
	str	r5, [r6]

	/*
	 * MsgQ->m_iWriteTotal += SM_MSG_SIZE;
	 */
	ldr	r5, [r6, #8]
	add	r5, r5, #SM_MSG_SIZE
	str	r5, [r6, #8]
dead_loop:
	b	dead_loop
ENDPROC(berlin_enter_ddrsr)
ENTRY(berlin_enter_ddrsr_sz)
	.word	. - berlin_enter_ddrsr
