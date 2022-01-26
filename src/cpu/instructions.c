//  Copyright (c) 2012-2018 Jakub Filipowicz <jakubf@gmail.com>
//
//  This program is free software; you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation; either version 2 of the License, or
//  (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program; if not, write to the Free Software
//  Foundation, Inc.,
//  51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA

#include <stdlib.h>

#include "em400.h"
#include "cpu/cpu.h"
#include "cpu/interrupts.h"
#include "cpu/alu.h"
#include "mem/mem.h"
#include "cpu/iset.h"
#include "cpu/instructions.h"
#include "io/defs.h"
#include "io/io.h"

#include "utils/utils.h"
#include "log.h"
#include "log_crk.h"

#include "ectl.h" // for global constants

// -----------------------------------------------------------------------
// ---- 20 - 36 ----------------------------------------------------------
// -----------------------------------------------------------------------

// -----------------------------------------------------------------------
void op_lw()
{
	REG_RESTRICT_WRITE(IR_A, ac);
}

// -----------------------------------------------------------------------
void op_tw()
{
	uint16_t data;
	if (cpu_mem_get(nb, ac, &data)) {
		REG_RESTRICT_WRITE(IR_A, data);
	}
}

// -----------------------------------------------------------------------
void op_ls()
{
	REG_RESTRICT_WRITE(IR_A, (r[IR_A] & ~r[7]) | (ac & r[7]));
}

// -----------------------------------------------------------------------
void op_ri()
{
	if (cpu_mem_put(QNB, r[IR_A], ac)) {
		REG_RESTRICT_WRITE(IR_A, r[IR_A]+1);
	}
}

// -----------------------------------------------------------------------
void op_rw()
{
	cpu_mem_put(QNB, ac, r[IR_A]);
}

// -----------------------------------------------------------------------
void op_pw()
{
	cpu_mem_put(nb, ac, r[IR_A]);
}

// -----------------------------------------------------------------------
void op_rj()
{
	REG_RESTRICT_WRITE(IR_A, ic);
	ic = ac;
}

// -----------------------------------------------------------------------
void op_is()
{
	uint16_t data;
	if (cpu_mem_get(nb, ac, &data)) {
		if ((data & r[IR_A]) == r[IR_A]) {
			p = true;
		} else {
			cpu_mem_put(nb, ac, data | r[IR_A]);
		}
	}
}

// -----------------------------------------------------------------------
void op_bb()
{
	if ((r[IR_A] & (uint16_t) ac) == (uint16_t) ac) {
		p = true;
	}
}

// -----------------------------------------------------------------------
void op_bm()
{
	uint16_t data;
	if (cpu_mem_get(nb, ac, &data)) {
		if ((data & r[IR_A]) == r[IR_A]) {
			p = true;
		}
	}
}

// -----------------------------------------------------------------------
void op_bs()
{
	if ((r[IR_A] & r[7]) == ((uint16_t) ac & r[7])) {
		p = true;
	}
}

// -----------------------------------------------------------------------
void op_bc()
{
	if ((r[IR_A] & (uint16_t) ac) != (uint16_t) ac) {
		p = true;
	}
}

// -----------------------------------------------------------------------
void op_bn()
{
	if ((r[IR_A] & (uint16_t) ac) == 0) {
		p = true;
	}
}

// -----------------------------------------------------------------------
void op_ou()
{
	uint16_t data;
	int io_result = io_dispatch(IO_OU, ac, r+IR_A);
	if (cpu_mem_get(QNB, ic + io_result, &data)) {
		ic = data;
	}
}

// -----------------------------------------------------------------------
void op_in()
{
	uint16_t data;
	int io_result = io_dispatch(IO_IN, ac, r+IR_A);
	if (cpu_mem_get(QNB, ic + io_result, &data)) {
		ic = data;
	}
}

// -----------------------------------------------------------------------
// ---- 37 ---------------------------------------------------------------
// -----------------------------------------------------------------------

// -----------------------------------------------------------------------
void op_37_ad()
{
	awp_dispatch(AWP_AD, ac);
}

// -----------------------------------------------------------------------
void op_37_sd()
{
	awp_dispatch(AWP_SD, ac);
}

// -----------------------------------------------------------------------
void op_37_mw()
{
	awp_dispatch(AWP_MW, ac);
}

// -----------------------------------------------------------------------
void op_37_dw()
{
	awp_dispatch(AWP_DW, ac);
}

// -----------------------------------------------------------------------
void op_37_af()
{
	awp_dispatch(AWP_AF, ac);
}

// -----------------------------------------------------------------------
void op_37_sf()
{
	awp_dispatch(AWP_SF, ac);
}

// -----------------------------------------------------------------------
void op_37_mf()
{
	awp_dispatch(AWP_MF, ac);
}

// -----------------------------------------------------------------------
void op_37_df()
{
	awp_dispatch(AWP_DF, ac);
}

// -----------------------------------------------------------------------
// ---- 40 - 57 ----------------------------------------------------------
// -----------------------------------------------------------------------

// -----------------------------------------------------------------------
void op_aw()
{
	alu_16_add(r[IR_A], ac, 0);
}

// -----------------------------------------------------------------------
void op_ac()
{
	alu_16_add(r[IR_A], ac, FGET(FL_C));
}

// -----------------------------------------------------------------------
void op_sw()
{
	alu_16_sub(r[IR_A], ac);
}

// -----------------------------------------------------------------------
void op_cw()
{
	alu_16_set_LEG((int16_t) r[IR_A], (int16_t) ac);
}

// -----------------------------------------------------------------------
void op_or()
{
	uint16_t result = r[IR_A] | ac;
	alu_16_set_Z_bool(result);
	REG_RESTRICT_WRITE(IR_A, result);
}

// -----------------------------------------------------------------------
void op_om()
{
	uint16_t data;
	if (cpu_mem_get(nb, ac, &data)) {
		data |= r[IR_A];
		if (cpu_mem_put(nb, ac, data)) {
			alu_16_set_Z_bool(data);
		}
	}
}

// -----------------------------------------------------------------------
void op_nr()
{
	uint16_t result = r[IR_A] & ac;
	alu_16_set_Z_bool(result);
	REG_RESTRICT_WRITE(IR_A, result);
}

// -----------------------------------------------------------------------
void op_nm()
{
	uint16_t data;
	if (cpu_mem_get(nb, ac, &data)) {
		data &= r[IR_A];
		if (cpu_mem_put(nb, ac, data)) {
			alu_16_set_Z_bool(data);
		}
	}
}

// -----------------------------------------------------------------------
void op_er()
{
	uint16_t result = r[IR_A] & ~ac;
	alu_16_set_Z_bool(result);
	REG_RESTRICT_WRITE(IR_A, result);
}

// -----------------------------------------------------------------------
void op_em()
{
	uint16_t data;
	if (cpu_mem_get(nb, ac, &data)) {
		data &= ~r[IR_A];
		if (cpu_mem_put(nb, ac, data)) {
			alu_16_set_Z_bool(data);
		}
	}
}

// -----------------------------------------------------------------------
void op_xr()
{
	uint16_t result = r[IR_A] ^ ac;
	alu_16_set_Z_bool(result);
	REG_RESTRICT_WRITE(IR_A, result);
}

// -----------------------------------------------------------------------
void op_xm()
{
	uint16_t data;
	if (cpu_mem_get(nb, ac, &data)) {
		data ^= r[IR_A];
		if (cpu_mem_put(nb, ac, data)) {
			alu_16_set_Z_bool(data);
		}
	}
}

// -----------------------------------------------------------------------
void op_cl()
{
	alu_16_set_LEG(r[IR_A], (uint16_t) ac);
}

// -----------------------------------------------------------------------
void op_lb()
{
	uint8_t data;
	if (cpu_mem_get_byte(nb, ac, &data)) {
		REG_RESTRICT_WRITE(IR_A, (r[IR_A] & 0xff00) | data);
	}
}

// -----------------------------------------------------------------------
void op_rb()
{
	cpu_mem_put_byte(nb, ac, r[IR_A]);
}

// -----------------------------------------------------------------------
void op_cb()
{
	uint8_t data;
	if (cpu_mem_get_byte(nb, ac, &data)) {
		alu_16_set_LEG((uint8_t) r[IR_A], data);
	}
}

// -----------------------------------------------------------------------
// ---- 60 - 67 ----------------------------------------------------------
// -----------------------------------------------------------------------

// -----------------------------------------------------------------------
void op_awt()
{
	alu_16_add(r[IR_A], ac, 0);
}

// -----------------------------------------------------------------------
void op_trb()
{
	REG_RESTRICT_WRITE(IR_A, r[IR_A] + ac);
	if (r[IR_A] == 0) {
		p = true;
	}
}

// -----------------------------------------------------------------------
void op_irb()
{
	REG_RESTRICT_WRITE(IR_A, r[IR_A]+1);
	if (r[IR_A]) ic += ac;
}

// -----------------------------------------------------------------------
void op_drb()
{
	REG_RESTRICT_WRITE(IR_A, r[IR_A]-1);
	if (r[IR_A] != 0) ic += ac;
}

// -----------------------------------------------------------------------
void op_cwt()
{
	alu_16_set_LEG((int16_t) r[IR_A], (int16_t) ac);
}

// -----------------------------------------------------------------------
void op_lwt()
{
	REG_RESTRICT_WRITE(IR_A, ac);
}

// -----------------------------------------------------------------------
void op_lws()
{
	uint16_t data;
	if (cpu_mem_get(QNB, ic + ac, &data)) {
		REG_RESTRICT_WRITE(IR_A, data);
	}
}

// -----------------------------------------------------------------------
void op_rws()
{
	cpu_mem_put(QNB, ic + ac, r[IR_A]);
}

// -----------------------------------------------------------------------
// ---- 70 ---------------------------------------------------------------
// -----------------------------------------------------------------------

// -----------------------------------------------------------------------
void op_70_jump()
{
	ic += ac;
}

// -----------------------------------------------------------------------
void op_70_jvs()
{
	ic += ac;
	FCLR(FL_V);
}

// -----------------------------------------------------------------------
// ---- 71 ---------------------------------------------------------------
// -----------------------------------------------------------------------

// -----------------------------------------------------------------------
void op_71_blc()
{
	if (((r[0] >> 8) & ac) != ac) {
		p = true;
	}
}

// -----------------------------------------------------------------------
void op_71_exl()
{
	uint16_t data;

	if (LOG_ENABLED) {
		if (LOG_WANTS(L_OP)) {
			log_log_cpu(L_OP, "EXL: %i (r4: 0x%04x)", ac, r[4]);
		}
		if (LOG_WANTS(L_CRK5)) {
			log_handle_syscall(L_CRK5, ac, QNB, ic, r[4]);
		}
	}

	if (cpu_mem_get(0, 96, &data)) {
		cpu_ctx_switch(ac, data, MASK_9);
	}
}

// -----------------------------------------------------------------------
void op_71_brc()
{
	if ((r[0] & ac) != ac) {
		p = true;
	}
}

// -----------------------------------------------------------------------
void op_71_nrf()
{
	int nrf_op = IR_A & 0b011;
	awp_dispatch(nrf_op, IR_b);
}

// -----------------------------------------------------------------------
// ---- 72 ---------------------------------------------------------------
// -----------------------------------------------------------------------

// -----------------------------------------------------------------------
void op_72_ric()
{
	REG_RESTRICT_WRITE(IR_A, ic);
}

// -----------------------------------------------------------------------
void op_72_zlb()
{
	REG_RESTRICT_WRITE(IR_A, r[IR_A] & 0x00ff);
}

// -----------------------------------------------------------------------
void op_72_sxu()
{
	if (r[IR_A] & 0x8000) {
		FSET(FL_X);
	} else {
		FCLR(FL_X);
	}
}

// -----------------------------------------------------------------------
void op_72_nga()
{
	alu_16_add(~r[IR_A], 0, 1);
}

// -----------------------------------------------------------------------
void shift_left(uint16_t shift_in, int check_v)
{
	uint16_t result = r[IR_A]<<1 | shift_in;
	if (check_v && ((r[IR_A] ^ result) & 0x8000)) {
		FSET(FL_V);
	}
	if (r[IR_A] & 0x8000) FSET(FL_Y);
	else FCLR(FL_Y);
	REG_RESTRICT_WRITE(IR_A, result);
}

// -----------------------------------------------------------------------
void op_72_slz()
{
	shift_left(0, 0);
}

// -----------------------------------------------------------------------
void op_72_sly()
{
	shift_left(FGET(FL_Y), 0);
}

// -----------------------------------------------------------------------
void op_72_slx()
{
	shift_left(FGET(FL_X), 0);
}

// -----------------------------------------------------------------------
void op_72_svz()
{
	shift_left(0, 1);
}

// -----------------------------------------------------------------------
void op_72_svy()
{
	shift_left(FGET(FL_Y), 1);
}

// -----------------------------------------------------------------------
void op_72_svx()
{
	shift_left(FGET(FL_X), 1);
}

// -----------------------------------------------------------------------
void shift_right(uint16_t shift_in)
{
	uint16_t result = (r[IR_A]>>1) | shift_in;
	if (r[IR_A] & 1) FSET(FL_Y);
	else FCLR(FL_Y);
	REG_RESTRICT_WRITE(IR_A, result);
}

// -----------------------------------------------------------------------
void op_72_sry()
{
	shift_right(FGET(FL_Y)<<15);
}

// -----------------------------------------------------------------------
void op_72_srx()
{
	shift_right(FGET(FL_X)<<15);
}

// -----------------------------------------------------------------------
void op_72_srz()
{
	shift_right(0);
}

// -----------------------------------------------------------------------
void op_72_ngl()
{
	uint16_t result = ~r[IR_A];
	alu_16_set_Z_bool(result);
	r[IR_A] = result;
}

// -----------------------------------------------------------------------
void op_72_rpc()
{
	REG_RESTRICT_WRITE(IR_A, r[0]);
}

// -----------------------------------------------------------------------
void op_72_shc()
{
	if (!IR_t) return;

	uint16_t falling = (r[IR_A] & ((1<<IR_t)-1)) << (16-IR_t);

	REG_RESTRICT_WRITE(IR_A, (r[IR_A] >> IR_t) | falling);
}

// -----------------------------------------------------------------------
void op_72_rky()
{
	REG_RESTRICT_WRITE(IR_A, kb);
}

// -----------------------------------------------------------------------
void op_72_zrb()
{
	REG_RESTRICT_WRITE(IR_A, r[IR_A] & 0xff00);
}

// -----------------------------------------------------------------------
void op_72_sxl()
{
	if (r[IR_A] & 1) {
		FSET(FL_X);
	} else {
		FCLR(FL_X);
	}
}

// -----------------------------------------------------------------------
void op_72_ngc()
{
	alu_16_add(~r[IR_A], 0, FGET(FL_C));
}

// -----------------------------------------------------------------------
void op_72_lpc()
{
	r[0] = r[IR_A];
}

// -----------------------------------------------------------------------
// ---- 73 ---------------------------------------------------------------
// -----------------------------------------------------------------------

// -----------------------------------------------------------------------
void op_73_hlt()
{
	LOGCPU(L_OP, "HALT 0%02o (alarm: %i)", ac, r[6]&255);
	cpu_state_change(ECTL_STATE_WAIT, ECTL_STATE_RUN);
}

// -----------------------------------------------------------------------
void op_73_mcl()
{
	cpu_state_change(ECTL_STATE_CLM, ECTL_STATE_RUN);
}

// -----------------------------------------------------------------------
void op_73_softint()
{
	// SIT, SIL, SIU, CIT
	if ((IR_C & 3) == 0) {
		int_clear(INT_SOFT_U);
		int_clear(INT_SOFT_L);
	} else {
		if ((IR_C & 1)) int_set(INT_SOFT_L);
		if ((IR_C & 2)) int_set(INT_SOFT_U);
	}

	// SINT, SIND
	if (cpu_mod_present && (IR_C & 4)) int_set(INT_CLOCK);
}

// -----------------------------------------------------------------------
void op_73_giu()
{
	// TODO: 2-cpu configuration
}

// -----------------------------------------------------------------------
void op_73_gil()
{
	// TODO: 2-cpu configuration
}

// -----------------------------------------------------------------------
void op_73_lip()
{
	cpu_ctx_restore();

	if (LOG_ENABLED) {
		log_update_process();
		if (LOG_WANTS(L_CRK5)) {
			log_handle_syscall_ret(L_CRK5, ic, SR_READ(), r[4]);
		}
		if (LOG_WANTS(L_CRK5)) {
			log_log_process(L_CRK5);
		}
		log_intlevel_dec();
	}
}

// -----------------------------------------------------------------------
void op_73_cron()
{
	if (cpu_mod_present) {
		cpu_mod_on();
	}
	// CRON is an illegal instruction anyway
	int_set(INT_ILLEGAL_INSTRUCTION);
}

// -----------------------------------------------------------------------
// ---- 74 ---------------------------------------------------------------
// -----------------------------------------------------------------------

// -----------------------------------------------------------------------
void op_74_jump()
{
	ic = ac;
}

// -----------------------------------------------------------------------
void op_74_lj()
{
	if (cpu_mem_put(QNB, ac, ic)) {
		ic = ac+1;
	}
}

// -----------------------------------------------------------------------
// ---- 75 ---------------------------------------------------------------
// -----------------------------------------------------------------------

// -----------------------------------------------------------------------
void op_75_ld()
{
	cpu_mem_mget(QNB, ac, r+1, 2);
}

// -----------------------------------------------------------------------
void op_75_lf()
{
	cpu_mem_mget(QNB, ac, r+1, 3);
}

// -----------------------------------------------------------------------
void op_75_la()
{
	cpu_mem_mget(QNB, ac, r+1, 7);
}

// -----------------------------------------------------------------------
void op_75_ll()
{
	cpu_mem_mget(QNB, ac, r+5, 3);
}

// -----------------------------------------------------------------------
void op_75_td()
{
	cpu_mem_mget(nb, ac, r+1, 2);
}

// -----------------------------------------------------------------------
void op_75_tf()
{
	cpu_mem_mget(nb, ac, r+1, 3);
}

// -----------------------------------------------------------------------
void op_75_ta()
{
	cpu_mem_mget(nb, ac, r+1, 7);
}

// -----------------------------------------------------------------------
void op_75_tl()
{
	cpu_mem_mget(nb, ac, r+5, 3);
}

// -----------------------------------------------------------------------
// ---- 76 ---------------------------------------------------------------
// -----------------------------------------------------------------------

// -----------------------------------------------------------------------
void op_76_rd()
{
	cpu_mem_mput(QNB, ac, r+1, 2);
}

// -----------------------------------------------------------------------
void op_76_rf()
{
	cpu_mem_mput(QNB, ac, r+1, 3);
}

// -----------------------------------------------------------------------
void op_76_ra()
{
	cpu_mem_mput(QNB, ac, r+1, 7);
}

// -----------------------------------------------------------------------
void op_76_rl()
{
	cpu_mem_mput(QNB, ac, r+5, 3);
}

// -----------------------------------------------------------------------
void op_76_pd()
{
	cpu_mem_mput(nb, ac, r+1, 2);
}

// -----------------------------------------------------------------------
void op_76_pf()
{
	cpu_mem_mput(nb, ac, r+1, 3);
}

// -----------------------------------------------------------------------
void op_76_pa()
{
	cpu_mem_mput(nb, ac, r+1, 7);
}

// -----------------------------------------------------------------------
void op_76_pl()
{
	cpu_mem_mput(nb, ac, r+5, 3);
}

// -----------------------------------------------------------------------
// ---- 77 ---------------------------------------------------------------
// -----------------------------------------------------------------------

// -----------------------------------------------------------------------
void op_77_mb()
{
	uint16_t data;
	if (cpu_mem_get(QNB, ac, &data)) {
		q =  data & 0b100000;
		bs = data & 0b010000;
		nb = data & 0b001111;

	}
}

// -----------------------------------------------------------------------
void op_77_im()
{
	uint16_t data;
	if (cpu_mem_get(QNB, ac, &data)) {
		rm = (data >> 6) & 0b1111111111;
		int_update_mask(rm);
	}
}

// -----------------------------------------------------------------------
void op_77_ki()
{
	uint16_t data = int_get_nchan();
	cpu_mem_put(QNB, ac, data);
}

// -----------------------------------------------------------------------
void op_77_fi()
{
	uint16_t data;
	if (cpu_mem_get(QNB, ac, &data)) {
		int_put_nchan(data);
	}
}

// -----------------------------------------------------------------------
void op_77_sp()
{
	uint16_t data[3];
	if (cpu_mem_mget(nb, ac, data, 3) != 3) return;

	ic = data[0];
	r[0] = data[1];
	SR_WRITE(data[2]);

	int_update_mask(rm);

	if (LOG_ENABLED) {
		log_update_process();
		log_intlevel_reset();
		if (LOG_WANTS(L_OP)) {
			log_log_cpu(L_OP, "SP: context @ 0x%04x", ac);
		}
		if (LOG_WANTS(L_CRK5)) {
			log_handle_syscall_ret(L_CRK5, ic, SR_READ(), r[4]);
		}
		if (LOG_WANTS(L_CRK5)) {
			log_log_process(L_CRK5);
		}
	}
}

// -----------------------------------------------------------------------
void op_77_md()
{
	if (mc >= 3) {
		LOGCPU(L_CPU, "    (ineffective: 4th MD)");
		int_set(INT_ILLEGAL_INSTRUCTION);
		mc = 0;
		return;
	}
	mc++;
}

// -----------------------------------------------------------------------
void op_77_rz()
{
	cpu_mem_put(QNB, ac, 0);
}

// -----------------------------------------------------------------------
void op_77_ib()
{
	uint16_t data;
	if (cpu_mem_get(QNB, ac, &data)) {
		if (cpu_mem_put(QNB, ac, ++data)) {
			if (data == 0) {
				p = true;
			}
		}
	}
}

// vim: tabstop=4 shiftwidth=4 autoindent
