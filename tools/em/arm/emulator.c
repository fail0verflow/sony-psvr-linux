/* With non GPL files, use following license */
/*
 * Copyright 2008 Sony Corporation.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions, and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
/* Otherwise with GPL files, use following license */
/*
 *  Copyright 2008 Sony Corporation.
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  version 2 of the  License.
 *
 *  THIS  SOFTWARE  IS PROVIDED   ``AS  IS'' AND   ANY  EXPRESS OR IMPLIED
 *  WARRANTIES,   INCLUDING, BUT NOT  LIMITED  TO, THE IMPLIED WARRANTIES OF
 *  MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN
 *  NO  EVENT  SHALL   THE AUTHOR  BE    LIABLE FOR ANY   DIRECT, INDIRECT,
 *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 *  NOT LIMITED   TO, PROCUREMENT OF  SUBSTITUTE GOODS  OR SERVICES; LOSS OF
 *  USE, DATA,  OR PROFITS; OR  BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 *  ANY THEORY OF LIABILITY, WHETHER IN  CONTRACT, STRICT LIABILITY, OR TORT
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 *  THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *  You should have received a copy of the  GNU General Public License along
 *  with this program; if not, write  to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

static int R_M = 1;

#define ERROP (-1UL)
#define ERR_OP(op) ((op) == ERROP)

static inline ulong get_op(ulong *p)
{
	ulong val;

	if (__get_user(val, p))
		val = ERROP;

	return val;
}

enum {
	R0_no = 0,
	R1_no,
	R2_no,
	R3_no,
	R4_no,
	R5_no,
	R6_no,
	R7_no,
	R8_no,
	R9_no,
	R10_no,
	FP_no,
	R12_no,
	SP_no,
	LR_no,
	PC_no,
	CPSR_no
};

#ifdef DEBUG
static const char *REGs[] = {
	"r0", "r1", "r2", "r3", "r4", "r5", "r6",
	"r7", "r8", "r9", "r10", "fp", "r12", "sp",
	"lr", "pc"};
#endif

static inline ulong get_bits(ulong op, ulong start, ulong len)
{
	op >>= start;
	op &= ((1 << len) - 1);
	return op;
}

static inline void set_bits(ulong op, ulong start, ulong len)
{
	ulong v = ((1 << len) - 1) << start;
	op |= v;
}

static inline void clear_bits(ulong op, ulong start, ulong len)
{
	ulong v = ((1 << len) - 1) << start;
	op &= ~v;
}

enum {
	COND_EQ = 0,
	COND_NE,
	COND_CSHS,
	COND_CCLO,
	CASE_MI,
	CASE_PL,
	CASE_VS,
	CASE_VC,
	CASE_HI,
	CASE_LS,
	CASE_GE,
	CASE_LT,
	CASE_GT,
	CASE_LE,
	COND_AL,
	COND_NV
};

#define op_COND(op) get_bits(op, 28, 4)
#define op_OPCODE(op) get_bits(op, 21, 4)

#define N(r) (get_bits(r, 31, 1))
#define Z(r) (get_bits(r, 30, 1))
#define C(r) (get_bits(r, 29, 1))
#define V(r) (get_bits(r, 28, 1))

static int cond_true(ulong op, struct pt_regs *regs)
{
#if 0
	ulong r = regs->uregs[CPSR_no];

	switch (op_COND(op)) {
	default:
	case COND_AL:
		return 1;
	case COND_EQ:
		return Z(r);
	case COND_NE:
		return !Z(r);
	case COND_CSHS:
		return C(r);
	case COND_CCLO:
		return !C(r);
	case CASE_MI:
		return N(r);
	case CASE_PL:
		return !N(r);
	case CASE_VS:
		return V(r);
	case CASE_VC:
		return !V(r);
	case CASE_HI:
		return (C(r) && !Z(r));
	case CASE_LS:
		return (!C(r) && Z(r));
	case CASE_GE:
		return (N(r) == V(r));
	case CASE_LT:
		return (N(r) != V(r));
	case CASE_GT:
		return (!Z(r) && (N(r) == V(r)));
	case CASE_LE:
		return (Z(r) || (N(r) != V(r)));
	}
#else
	switch (op_COND(op)) {
	default:
		return 0;
	case COND_AL:
		return 1;
	}
#endif
}

#define RN(op) get_bits(op, 16, 4)
#define RD(op) get_bits(op, 12, 4)
#define RS(op) get_bits(op, 8,  4)
#define RM(op) get_bits(op, 0,  4)
#define P(op)  get_bits(op, 24, 1)
#define U(op)  get_bits(op, 23, 1)
#define EM_W(op)  get_bits(op, 21, 1)
#define L(op)  get_bits(op, 20, 1)

static ulong rotate_right(ulong v, ulong s)
{
	ulong h = v & ~((1 << s) -1);
	ulong l = v & ((1 << s) -1);

	return (h >> s) | (l << (32 - s));
}

static ulong imm8_shifter(ulong op)
{
	ulong shifter = get_bits(op, 8, 4);
	ulong imm8 = get_bits(op, 0, 8);

	return rotate_right(imm8, shifter * 2);
}

enum {
	RET_OK = 0,
	RET_BRANCHED
};

/*
 * instruction emulator
 */

/********Data Processing*****************/
#ifdef DEBUG
static const char *OPCODEs[] = {
	"&", "^", "-", "^-", "+",
	"+.", "-.", "^-.",
	"tst", "teq", "cmp", "cmn",
	"|", "mov", "& ~", "mvn"
};
#endif

#define COND_RET(c, t, f) ((c)?(t):(f))
#define R_R(t, f) COND_RET((R_M), (t), (f))

static ulong op_math_and(ulong rn, ulong shifter, ulong rd)
{
	return R_R(rn & shifter, rd);
}

static ulong op_math_eor(ulong rn, ulong shifter, ulong rd)
{
	return R_R(rn ^ shifter, rd);
}

static ulong op_math_sub(ulong rn, ulong shifter, ulong rd)
{
	return R_R(rn - shifter, rn + shifter);
}

static ulong op_math_rsb(ulong rn, ulong shifter, ulong rd)
{
	return R_R(shifter - rn, rd);
}

static ulong op_math_add(ulong rn, ulong shifter, ulong rd)
{
	return R_R(rn + shifter, rn - shifter);
}

static ulong op_math_adc(ulong rn, ulong shifter, ulong rd)
{
	return R_R(rn + shifter, rn - shifter);
}

static ulong op_math_sbc(ulong rn, ulong shifter, ulong rd)
{
	return R_R(rn - shifter, rn + shifter);
}

static ulong op_math_rsc(ulong rn, ulong shifter, ulong rd)
{
	return R_R(shifter - rn, shifter - rd);
}

static ulong op_math_tst(ulong rn, ulong shifter, ulong rd)
{
	return rd;
}

static ulong op_math_teq(ulong rn, ulong shifter, ulong rd)
{
	return rd;
}

static ulong op_math_cmp(ulong rn, ulong shifter, ulong rd)
{
	return rd;
}

static ulong op_math_cmn(ulong rn, ulong shifter, ulong rd)
{
	return rd;
}

static ulong op_math_orr(ulong rn, ulong shifter, ulong rd)
{
	return R_R(rn | shifter, rd);
}

static ulong op_math_mov(ulong rn, ulong shifter, ulong rd)
{
	return R_R(rn, rd);
}

static ulong op_math_bic(ulong rn, ulong shifter, ulong rd)
{
	return R_R(rn & ~shifter, rd);
}

static ulong op_math_mvn(ulong rn, ulong shifter, ulong rd)
{
	return R_R(rn, rd);
}

typedef ulong (*op_math)(ulong rn, ulong shifter, ulong rd);
static op_math op_maths[] = {
	op_math_and,
	op_math_eor,
	op_math_sub,
	op_math_rsb,
	op_math_add,
	op_math_adc,
	op_math_sbc,
	op_math_rsc,
	op_math_tst,
	op_math_teq,
	op_math_cmp,
	op_math_cmn,
	op_math_orr,
	op_math_mov,
	op_math_bic,
	op_math_mvn,
};

#define SHIFT_IMM(op) (get_bits(op, 7, 5))
#define SHIFT(op) (get_bits(op, 5, 2))

enum {
	LSL,
	LSR,
	ASR,
	ROR
};

static ulong shifter_operand(ulong op, ulong shift_imm, ulong shift, ulong rm)
{
	ulong v = 0;
	switch(shift) {
	case LSL:
		v = rm << shift_imm;
		break;
	case LSR:
		v = rm >> shift_imm;
		break;
	case ASR:
		v = rm >> shift_imm;
		break;
	case ROR:
		v = rotate_right(rm, shift_imm);
		break;
	}

	return v;
}

static int dp_immediate_shift(ulong op, struct pt_regs *regs)
{
	ulong rn = RN(op);
	ulong rd = RD(op);
	ulong shift_imm = SHIFT_IMM(op);
	ulong shift = SHIFT(op);
	ulong rm = RM(op);
	ulong shifter;

	dbg("%08lx -> Data Processing Imm shift\n", op);
	if (!cond_true(op, regs)) {
		return 0;
	}

	if (shift_imm)
		shifter = shifter_operand(op, shift_imm, shift, regs->uregs[rm]);
	else
		shifter = regs->uregs[rm];

	dbg("%s: %8lx %s %8lx-> ", REGs[rd], regs->uregs[rn], OPCODEs[op_OPCODE(op)], shifter);
	if (R_M || rd != LR_no)
		regs->uregs[rd] = op_maths[op_OPCODE(op)](regs->uregs[rn], shifter, regs->uregs[rd]);
	dbg("%8lx\n ", regs->uregs[rd]);

	return 0;
}

static int dp_register_shift(ulong op, struct pt_regs *regs)
{
	ulong rn = RN(op);
	ulong rd = RD(op);
	ulong rs = RS(op);
	ulong rs_type = rs & 0xFF;
	ulong shift = get_bits(op, 5, 2);
	ulong rm = RM(op);
	ulong shifter;

	dbg("%08lx -> Data Processing Register shift\n", op);
	if (!cond_true(op, regs)) {
		return 0;
	}

	if (rs_type == 0)
		shifter = rm;
	else if(rs_type < 32)
		shifter = shifter_operand(op, rs_type, shift, regs->uregs[rm]);
	else if(rs_type == 32)
		shifter = 0;
	else
		shifter = 0;

	dbg("%s: %8lx %s %8lx-> ", REGs[rd], regs->uregs[rn], OPCODEs[op_OPCODE(op)], shifter);
	if (R_M || rd != LR_no)
		regs->uregs[rd] = op_maths[op_OPCODE(op)](regs->uregs[rn], shifter, regs->uregs[rd]);
	dbg("%8lx\n ", regs->uregs[rd]);

	return 0;
}

static int dp_immediate(ulong op, struct pt_regs *regs)
{
	ulong rn = RN(op);
	ulong rd = RD(op);
	ulong shifter;

	dbg("%08lx -> Data Processing Imm\n", op);
	if (!cond_true(op, regs)) {
		return 0;
	}

	shifter = imm8_shifter(op);

	dbg("%s: %8lx %s %8lx-> ", REGs[rd], regs->uregs[rd], OPCODEs[op_OPCODE(op)], shifter);
	if (R_M || (rd != LR_no && rn == rd))
		regs->uregs[rd] = op_maths[op_OPCODE(op)](regs->uregs[rn], shifter, regs->uregs[rd]);
	dbg("%8lx\n ", regs->uregs[rd]);

	return 0;
}

/********************Load/Store******************/
static int ldrstr_immediate_offset(ulong op, struct pt_regs *regs)
{
	ulong p = P(op);
	ulong u = U(op);
	ulong w = EM_W(op);
	ulong l = L(op);
	ulong rn = RN(op);
	ulong rd = RD(op);
	ulong val = imm8_shifter(op);

	dbg("%08lx -> Load/Store immediate offset\n", op);

	if (p == 0) { /*post-index*/
		if (!w) {
			dbg("ldr %s,[sp],#%c0x%lx\n", REGs[rd], u?'+':'-', val);
			if (R_M) {
				if (l)
					regs->uregs[rd] = em_get_user((ulong *)regs->uregs[rn]);

				if (u)
					regs->uregs[rn] += val;
				else
					regs->uregs[rn] -= val;
			}
			else {
				if (u)
					regs->uregs[rn] -= val;
				else
					regs->uregs[rn] += val;

				if (!l)
					regs->uregs[rd] = em_get_user((ulong *)regs->uregs[rn]);
			}
		}
	}
	else { /*pre-index*/
		if (w) {
			dbg("ldr %s,[sp,#%c0x%lx]!\n", REGs[rd], u?'+':'-', val);
			if (R_M) {
				if (u)
					regs->uregs[rn] += val;
				else
					regs->uregs[rn] -= val;

				if (l)
					regs->uregs[rd] = em_get_user((ulong *)regs->uregs[rn]);
			}
			else {
				if (!l)
					regs->uregs[rd] = em_get_user((ulong *)regs->uregs[rn]);

				if (u)
					regs->uregs[rn] -= val;
				else
					regs->uregs[rn] += val;
			}
		}
	}

	return 0;
}

static int ldrstr_register_offset(ulong op, struct pt_regs *regs)
{
	dbg("%08lx -> Load/Store register offset\n", op);
	return 0;
}

/********************Load/Store Multiple******************/
static int reg_num(ulong reglist)
{
	int i=0;
	while(reglist) {
		if (reglist & 0x1)
			i ++;
		reglist >>= 1;
	}

	return i;
}

static int ldmstm(ulong op, struct pt_regs *regs)
{
	ulong p = P(op);
	ulong u = U(op);
	ulong w = EM_W(op);
	ulong l = L(op);
	ulong sp = 0;
	ulong reglist = op & 0xFFFF;
	int no = 0;

	dbg("%08lx -> Load/store multiple\n", op);

	if (R_M) {
		switch(u) {
		case 1:
			sp = regs->uregs[SP_no] + p * 4;
			if (w)
				regs->uregs[SP_no] = sp + reg_num(reglist) * 4;

			break;
		case 0:
			sp = regs->uregs[SP_no] - reg_num(reglist)* 4 + (1 - p) * 4;
			if (w)
				regs->uregs[SP_no] = sp;

			break;
		}
	}
	else {
		switch(u) {
		case 1:
			sp = regs->uregs[SP_no] - reg_num(reglist)* 4 + p * 4;
			if (w)
				regs->uregs[SP_no] = sp;

			break;
		case 0:
			sp = regs->uregs[SP_no] + (1 - p) * 4;
			if (w)
				regs->uregs[SP_no] = sp + reg_num(reglist) * 4;

			break;
		}
	}

	while(reglist) {
		if (reglist & 0x1) {
			if (l == R_M) {
				dbg("%s: %08lx -> %08lx @ %08lx\n",
				    REGs[no], regs->uregs[no], em_get_user((ulong *)sp), sp);

				regs->uregs[no] = em_get_user((ulong *)sp);
			}

			sp += 4;
		}

		no ++;
		reglist >>= 1;
	}

	return 0;
}

/********************Load/Store Multiple******************/
static int branch_link(ulong op, struct pt_regs *regs)
{
	ulong imm24_u = op & 0xFFFFFF;
	long imm24 = (long)imm24_u;
	ulong l = get_bits(op, 24, 1);

	dbg("%08lx -> Branch and branch with link\n", op);

	if (!R_M)
		goto out;

	if (l) {
		dbg("BL -> ignored!\n");
		goto out;
	}

	if (!cond_true(op, regs)) {
		return 0;
	}

	if (imm24_u & 0x800000) {
		imm24 = -(((~imm24_u) + 1) & 0xFFFFFF);
	}

	imm24 *= 4;
	dbg("%s: imm24 -> %ld\n", __func__, imm24);

	imm24 += 8;
	dbg("pc: %08lx -> %08lx\n", regs->uregs[PC_no], regs->uregs[PC_no] + imm24);

	regs->uregs[PC_no] += imm24;

	return RET_BRANCHED;
out:
	return 0;
}

/*************************Misc************************/
static int misc_000(ulong op, struct pt_regs *regs)
{
	dbg("%08lx -> Categoray: Misc\n", op);

	if (get_bits(op, 21, 2) == 0x1 && get_bits(op, 4, 4) == 0x1) {
		ulong rm = RM(op);

		dbg("PC: %08lx -> %08lx\n", regs->uregs[PC_no] ,regs->uregs[rm]);
		regs->uregs[PC_no] = regs->uregs[rm];

		return RET_BRANCHED;
	}

	return 0;
}

/*
 * level 1 dispatcher
 */
struct op_emu {
	int (*emu)(ulong op, struct op_emu *emu, struct pt_regs *regs);
};

static int  l1_op_000(ulong op, struct op_emu *emu, struct pt_regs *regs)
{
	if (get_bits(op, 23, 5) == 0x2 && get_bits(op, 20, 1) == 0) {
		return misc_000(op, regs);
	}
	else {
		switch(get_bits(op, 4, 1)) {
		default:
		case 0:
			return dp_immediate_shift(op, regs);
		case 1:
			return dp_register_shift(op, regs);
		}
	}

	return 0;
}

static int  l1_op_001(ulong op, struct op_emu *emu, struct pt_regs *regs)
{
	if (get_bits(op, 23, 5) == 0x6 && get_bits(op, 20, 1) == 0) {
		switch(get_bits(op, 21, 1)) {
		default:
		case 0:
			dbg("%08lx -> Undefined Instruction\n", op);
			break;
		case 1:
			dbg("%08lx -> Movie immediate to status register\n", op);
			break;
		}
	}
	else {
		return dp_immediate(op, regs);
	}

	return 0;
}

static int  l1_op_010(ulong op, struct op_emu *emu, struct pt_regs *regs)
{
	return ldrstr_immediate_offset(op, regs);
}

static int  l1_op_011(ulong op, struct op_emu *emu, struct pt_regs *regs)
{
	if (get_bits(op, 4, 1) == 0) {
		return ldrstr_register_offset(op, regs);
	}
	else
		dbg("%08lx -> Undefined Instruction\n", op);

	return 0;
}

static int  l1_op_100(ulong op, struct op_emu *emu, struct pt_regs *regs)
{
	if (op_COND(op) == COND_NV)
		dbg("%08lx -> Undefined Instruction\n", op);
	else
		return ldmstm(op, regs);

	return 0;
}

static int  l1_op_101(ulong op, struct op_emu *emu, struct pt_regs *regs)
{
	if (op_COND(op) == COND_NV)
		dbg("%08lx -> Branch and branch with link and change to Thumb\n", op);
	else
		return branch_link(op, regs);

	return 0;
}

static int  l1_op_110(ulong op, struct op_emu *emu, struct pt_regs *regs)
{
	dbg("%08lx -> Coprocessor load/store and double register transfer\n", op);

	return 0;
}

static int  l1_op_111(ulong op, struct op_emu *emu, struct pt_regs *regs)
{
	if (get_bits(op, 24, 1) == 0) {
		switch(get_bits(op, 4, 1)) {
		default:
		case 0:
			dbg("%08lx -> Coprocessor data processing\n", op);
			break;
		case 1:
			dbg("%08lx -> Coprocessor register transfers\n", op);
			break;
		}
	}
	else {
		if (op_COND(op) == COND_NV)
			dbg("%08lx -> Undefined Instruction\n", op);
		else
			dbg("%08lx -> Software interrupt\n", op);
	}

	return 0;
}

static struct op_emu op_000 = {
	.emu = l1_op_000
};

static struct op_emu op_001 = {
	.emu = l1_op_001
};

static struct op_emu op_010 = {
	.emu = l1_op_010
};

static struct op_emu op_011 = {
	.emu = l1_op_011
};

static struct op_emu op_100 = {
	.emu = l1_op_100
};

static struct op_emu op_101 = {
	.emu = l1_op_101
};

static struct op_emu op_110 = {
	.emu = l1_op_110
};

static struct op_emu op_111 = {
	.emu = l1_op_111
};

struct op_emu *ops_l1[] = {
	&op_000,
	&op_001,
	&op_010,
	&op_011,
	&op_100,
	&op_101,
	&op_110,
	&op_111
};

static int emu_one_ins(ulong op, struct pt_regs *regs)
{
	struct op_emu *op_emu = ops_l1[get_bits(op, 25, 3)];
	return op_emu->emu(op, op_emu, regs);
}

