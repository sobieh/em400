//  Copyright (c) 2012-2021 Jakub Filipowicz <jakubf@gmail.com>
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


#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <stdbool.h>
#include <emawp.h>

#include "cpu/cpu.h"
#include "cpu/interrupts.h"
#include "mem/mem.h"
#include "cpu/iset.h"
#include "cpu/instructions.h"
#include "cpu/interrupts.h"
#include "cpu/clock.h"
#include "cpu/buzzer.h"
#include "io/defs.h"
#include "io/io.h"

#include "em400.h"
#include "utils/utils.h"
#include "log.h"
#include "log_crk.h"
#include "ectl/brk.h"

#include "ectl.h" // for global constants
#include "cfg.h"

static int cpu_state = ECTL_STATE_OFF;

uint16_t r[8];
uint16_t ic, kb, ir, ac, ar;
bool rALARM;
int mc;
unsigned rm, nb;
bool p, q, bs;

bool zc17;

bool cpu_mod_present;
bool cpu_mod_active;
bool cpu_user_io_illegal;
bool awp_enabled;
static bool nomem_stop;

unsigned long ips_counter;

static int speed_real;
static struct timespec cpu_timer;
static int cpu_time_cumulative;
static int throttle_granularity;
static float cpu_delay_factor;

static int sound_enabled;

// opcode table (instruction decoder decision table)
struct iset_opcode *cpu_op_tab[0x10000];

pthread_mutex_t cpu_wake_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cpu_wake_cond = PTHREAD_COND_INITIALIZER;

// -----------------------------------------------------------------------
static void cpu_do_wait()
{
	LOG(L_CPU, "idling in state WAIT");

	pthread_mutex_lock(&cpu_wake_mutex);
	while ((cpu_state == ECTL_STATE_WAIT) && !(atom_load_acquire(&rp) && !p && !mc)) {
			pthread_cond_wait(&cpu_wake_cond, &cpu_wake_mutex);
	}
	cpu_state &= ~ECTL_STATE_WAIT;
	pthread_mutex_unlock(&cpu_wake_mutex);
}

// -----------------------------------------------------------------------
static int cpu_do_stop()
{
	LOG(L_CPU, "idling in state STOP");

	pthread_mutex_lock(&cpu_wake_mutex);
	while ((cpu_state & (ECTL_STATE_STOP|ECTL_STATE_OFF|ECTL_STATE_CLO|ECTL_STATE_CLM|ECTL_STATE_CYCLE|ECTL_STATE_BIN)) == ECTL_STATE_STOP) {
		pthread_cond_wait(&cpu_wake_cond, &cpu_wake_mutex);
	}
	int res = cpu_state;
	pthread_mutex_unlock(&cpu_wake_mutex);

	return res;
}

// -----------------------------------------------------------------------
int cpu_state_change(int to, int from)
{
	int res = 1;

	pthread_mutex_lock(&cpu_wake_mutex);
	if ((from == ECTL_STATE_ANY) || (cpu_state == from)) {
		cpu_state = to;
		pthread_cond_broadcast(&cpu_wake_cond);
		res = 0;
	}
	pthread_mutex_unlock(&cpu_wake_mutex);

	return res;
}

// -----------------------------------------------------------------------
int cpu_state_get()
{
	return atom_load_acquire(&cpu_state);
}

// -----------------------------------------------------------------------
static void cpu_mem_fail(bool barnb)
{
	int_set(INT_NO_MEM);
	if (!barnb) {
		rALARM = true;
		if (nomem_stop) cpu_state_change(ECTL_STATE_STOP, ECTL_STATE_ANY);
	}
}

// -----------------------------------------------------------------------
bool cpu_mem_read_1(bool barnb, uint16_t addr, uint16_t *data)
{
	if (!mem_read_1(barnb * nb, addr, data)) {
		cpu_mem_fail(barnb);
		return false;
	}
	return true;
}

// -----------------------------------------------------------------------
bool cpu_mem_write_1(bool barnb, uint16_t addr, uint16_t data)
{
	if (!mem_write_1(barnb * nb, addr, data)) {
		cpu_mem_fail(barnb);
		return false;
	}
	return true;
}

// -----------------------------------------------------------------------
int cpu_init(em400_cfg *cfg)
{
	int res;

	awp_enabled = cfg_getbool(cfg, "cpu:awp", CFG_DEFAULT_CPU_AWP);

	kb = cfg_getint(cfg, "cpu:kb", CFG_DEFAULT_CPU_KB);

	cpu_mod_present = cfg_getbool(cfg, "cpu:modifications", CFG_DEFAULT_CPU_MODIFICATIONS);
	cpu_user_io_illegal = cfg_getbool(cfg, "cpu:user_io_illegal", CFG_DEFAULT_CPU_IO_USER_ILLEGAL);
	nomem_stop = cfg_getbool(cfg, "cpu:stop_on_nomem", CFG_DEFAULT_CPU_STOP_ON_NOMEM);
	speed_real = cfg_getbool(cfg, "cpu:speed_real", CFG_DEFAULT_CPU_SPEED_REAL);
	throttle_granularity = 1000 * cfg_getint(cfg, "cpu:throttle_granularity", CFG_DEFAULT_CPU_THROTTLE_GRANULARITY);
	double cpu_speed_factor = cfg_getdouble(cfg, "cpu:speed_factor", CFG_DEFAULT_CPU_SPEED_FACTOR);
	cpu_delay_factor = 1.0f/cpu_speed_factor;

	res = iset_build(cpu_op_tab, cpu_user_io_illegal);
	if (res != E_OK) {
		return LOGERR("Failed to build CPU instruction table.");
	}

	int_update_mask(0);

	// this is checked only at power-on
	if (mem_mega_boot()) {
		ic = 0xf000;
	} else {
		ic = 0;
	}

	cpu_mod_off();

	LOG(L_CPU, "CPU initialized. AWP: %s, KB=0x%04x, modifications: %s, user I/O: %s, stop on nomem: %s",
		awp_enabled ? "enabled" : "disabled",
		kb,
		cpu_mod_present ? "present" : "absent",
		cpu_user_io_illegal ? "illegal" : "legal",
		nomem_stop ? "true" : "false");
	LOG(L_CPU, "CPU speed: %s, throttle granularity: %i, speed factor: %.2f",
		speed_real ? "real" : "max",
		throttle_granularity/1000,
		cpu_speed_factor);

	sound_enabled = cfg_getbool(cfg, "sound:enabled", CFG_DEFAULT_SOUND_ENABLED);

	if (sound_enabled) {
		if (!speed_real || (cpu_speed_factor < 0.1f) || (cpu_speed_factor > 2.0f)) {
			LOGERR("EM400 needs to be configured with speed_real=true and 2.0 >= cpu_speed_factor >= 0.1 for the buzzer emulation to work.");
			LOGERR("Disabling sound.");
			sound_enabled = false;
		} else {
			if (buzzer_init(cfg) != E_OK) {
				return LOGERR("Failed to initialize buzzer.");
			}
		}
	}

	return E_OK;
}

// -----------------------------------------------------------------------
void cpu_shutdown()
{
	if (sound_enabled) {
		buzzer_shutdown();
	}
}

// -----------------------------------------------------------------------
int cpu_mod_on()
{
	cpu_mod_active = true;
	clock_set_int(INT_EXTRA);

	return E_OK;
}

// -----------------------------------------------------------------------
int cpu_mod_off()
{
	cpu_mod_active = false;
	clock_set_int(INT_CLOCK);

	return E_OK;
}

// -----------------------------------------------------------------------
static void cpu_do_clear(int scope)
{
	// I/O reset should return when we're sure that I/O won't change CPU state (backlogged interrupts, memory writes, ...)
	io_reset();
	mem_reset();
	cpu_mod_off();

	r[0] = 0;
	SR_WRITE(0);

	int_update_mask(rm);
	int_clear_all();

	if (scope == ECTL_STATE_CLO) {
		rALARM = false;
		mc = 0;
	}

	// call even if logging is disabled - user may enable it later
	// and we still want to know if we're running a known OS
	log_check_os();
	log_reset_process();
	log_intlevel_reset();
	log_syscall_reset();
}

// -----------------------------------------------------------------------
void cpu_ctx_switch(uint16_t arg, uint16_t new_ic, uint16_t int_mask)
{
	if (!cpu_mem_read_1(false, STACK_POINTER, &ar)) return;

	LOG(L_CPU, "Store current process ctx [IC: 0x%04x, R0: 0x%04x, SR: 0x%04x, 0x%04x] @ 0x%04x, set new IC: 0x%04x", ic, r[0], SR_READ(), arg, ar, new_ic);

	uint16_t vector[] = { ic, r[0], SR_READ(), arg };
	for (int i=0 ; i<4 ; i++, ar++) {
		if (!cpu_mem_write_1(false, ar, vector[i])) return;
	}
	if (!cpu_mem_write_1(false, STACK_POINTER, ar)) return;

	r[0] = 0;
	ic = new_ic;
	q = false;
	rm &= int_mask;
	int_update_mask(rm);
}

// -----------------------------------------------------------------------
void cpu_sp_rewind()
{
	if (!cpu_mem_read_1(false, STACK_POINTER, &ar)) return;
	ar -= 4;
	if (!cpu_mem_write_1(false, STACK_POINTER, ar)) return;
}

// -----------------------------------------------------------------------
void cpu_ctx_restore(bool barnb)
{
	uint16_t sr_tmp;
	uint16_t *vector[] = { &ic, r+0, &sr_tmp };
	for (int i=0 ; i<3 ; i++, ar++) {
		if (!cpu_mem_read_1(barnb, ar, vector[i])) return;
	}
	SR_WRITE(sr_tmp);
	int_update_mask(rm);
}

// -----------------------------------------------------------------------
static bool cpu_do_bin(bool start)
{
	static int words = 0;
	static uint16_t data;
	static uint8_t bdata[3];
	static int cnt = 0;

	if (start) {
		LOG(L_CPU, "Binary load initiated @ 0x%04x", ar);
		words = 0;
		cnt = 0;
		return false;
	}

	int res = io_dispatch(IO_IN, ic, &data);
	if (res == IO_OK) {
		bdata[cnt] = data & 0xff;
		if ((cnt == 0) && bin_is_end(bdata[cnt])) {
			LOG(L_CPU, "Binary load done, %i words loaded", words);
			return true;
		} else if (bin_is_valid(bdata[cnt])) {
			cnt++;
			if (cnt >= 3) {
				cnt = 0;
				if (cpu_mem_write_1(q, ar, bin2word(bdata)) == 1) {
					words++;
					ar++;
				}
			}
		}
	}

	return false;
}

// -----------------------------------------------------------------------
static int cpu_do_cycle()
{
	struct iset_opcode *op;
	int instruction_time = 0;

	if (LOG_WANTS(L_CPU)) log_store_cycle_state(SR_READ(), ic);

	ips_counter++;

	// fetch instruction
	if (!cpu_mem_read_1(q, ic++, &ir)) {
		LOGCPU(L_CPU, "        no mem, instruction fetch");
		goto ineffective_memfail;
	}

	op = cpu_op_tab[ir];
	unsigned flags = op->flags;

	// check instruction effectiveness
	if (p || ((r[0] & op->jmp_nef_mask) != op->jmp_nef_result)) {
		LOGDASM(0, 0, "skip: ");
		// if the instruction is ineffective, argument for 2-word instructions is skipped
		if ((flags & OP_FL_ARG_NORM) && !IR_C) ic++;
		goto ineffective;
	}

	// check instruction legalness
	// NOTE: for illegal and user-illegal 2-word instructions argument is _not_ skipped
	if (flags & OP_FL_ILLEGAL) {
		LOGCPU(L_CPU, "    illegal: 0x%04x", ir);
		int_set(INT_ILLEGAL_INSTRUCTION);
		goto ineffective;
	}
	if (q && (flags & OP_FL_USR_ILLEGAL)) {
		LOGDASM(0, 0, "user illegal: ");
		int_set(INT_ILLEGAL_INSTRUCTION);
		goto ineffective;
	}
	if ((op->fun == op_77_md) && (mc == 3)) {
		LOGDASM(0, 0, "illegal (4th md): ");
		int_set(INT_ILLEGAL_INSTRUCTION);
		goto ineffective;
	}

	// AC and AR in argument preparation is simplified compared to real h/w.
	// Only AC is updated, AR is synchronized at the end

	// get the argument
	if (flags & OP_FL_ARG_NORM) {
		if (IR_C) {
			ac = r[IR_C];
		} else {
			if (!cpu_mem_read_1(q, ic, &ac)) {
				LOGCPU(L_CPU, "    no mem, long arg fetch @ %i:0x%04x", q*nb, ic);
				goto ineffective_memfail;
			}
			ic++;
			instruction_time += TIME_MEM_ARG;
		}
	} else if (flags & OP_FL_ARG_SHORT) {
		ac = IR_T;
	} else if (flags & OP_FL_ARG_BYTE) {
		ac = IR_b;
	}

	// pre-mod
	if (mc) {
		zc17 = (ac + ar) > 0xffff;
		ac += ar;
		instruction_time += TIME_PREMOD;
	} else {
		zc17 = false;
	}

	// B-mod
	if ((flags & OP_FL_ARG_NORM) && IR_B) {
		zc17 = (ac + r[IR_B]) > 0xffff;
		ac += r[IR_B];
		instruction_time += TIME_BMOD;
	}

	ar = ac;

	// D-mod
	if ((flags & OP_FL_ARG_NORM) && IR_D) {
		if (!cpu_mem_read_1(q, ac, &ac)) {
			LOGCPU(L_CPU, "    no mem, indirect arg fetch @ %i:0x%04x", q*nb, ar);
			goto ineffective_memfail;
		}
		ar = ac;
		instruction_time += TIME_DMOD;
	}

	// execute instruction
	LOGDASM((op->flags & (OP_FL_ARG_NORM | OP_FL_ARG_SHORT)), ac, "");
	op->fun();
	instruction_time += op->time;

	// clear modification counter if instruction was not MD
	if (op->fun != op_77_md) mc = 0;

	if (op->fun == op_72_shc) {
		instruction_time += IR_t * TIME_SHIFT;
	} else if (op->fun == op_ou) {
		// Negative instruction time means "skip time keeping for this cycle".
		// Do this after each OU instruction.
		// This is required for minimalistic I/O routines using OU+HLT to work.
		// Without it, it may happen that interrupt HLT was supposed to wait for
		// is served just after OU, causing HLT to sleep indefinitely.
		instruction_time *= -1;
	}

	return instruction_time;

ineffective_memfail:
	instruction_time += TIME_NOANS_IF;
ineffective:
	instruction_time += TIME_P;
	p = false;
	mc = 0;
	return instruction_time;
}

// -----------------------------------------------------------------------
static void cpu_timekeeping(int cpu_time)
{
	bool skip_sleep = false;

	if (cpu_time < 0) {
		cpu_time *= -1;
		skip_sleep = true;
	}

	cpu_time *= cpu_delay_factor;
	cpu_time_cumulative += cpu_time;

	if (sound_enabled) {
		buzzer_update(ir, cpu_time);
	}

	if (!skip_sleep && (cpu_time_cumulative >= throttle_granularity)) {
		cpu_timer.tv_nsec += cpu_time_cumulative;
		while (cpu_timer.tv_nsec >= 1000000000) {
			cpu_timer.tv_nsec -= 1000000000;
			cpu_timer.tv_sec++;
		}
		cpu_time_cumulative = 0;
		while (clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &cpu_timer, NULL) == EINTR);
	}
}

// -----------------------------------------------------------------------
void cpu_loop()
{
	cpu_state_change(ECTL_STATE_STOP, ECTL_STATE_ANY);
	clock_gettime(CLOCK_MONOTONIC, &cpu_timer);

	while (1) {
		int cpu_time = 0;
		int state = atom_load_acquire(&cpu_state);

		switch (state) {
			case ECTL_STATE_CYCLE:
				cpu_state_change(ECTL_STATE_STOP, ECTL_STATE_CYCLE);
			case ECTL_STATE_RUN:
				if (atom_load_acquire(&rp) && !p && (mc == 0)) {
					int_serve();
					cpu_time = TIME_INT_SERVE;
				} else {
					cpu_time = cpu_do_cycle();
					if (ectl_brk_check()) {
						cpu_state_change(ECTL_STATE_STOP, ECTL_STATE_RUN);
					}
				}
				break;
			case ECTL_STATE_OFF:
				if (sound_enabled) buzzer_stop();
				return;
			case ECTL_STATE_CLM:
				cpu_do_clear(ECTL_STATE_CLM);
				cpu_state_change(ECTL_STATE_RUN, ECTL_STATE_CLM);
				break;
			case ECTL_STATE_CLO:
				if (sound_enabled) buzzer_stop();
				cpu_do_clear(ECTL_STATE_CLO);
				cpu_state_change(ECTL_STATE_STOP, ECTL_STATE_CLO);
				break;
			case ECTL_STATE_BIN:
				if (cpu_do_bin(false)) cpu_state_change(ECTL_STATE_STOP, ECTL_STATE_BIN);
				break;
			case ECTL_STATE_STOP:
				if (sound_enabled) buzzer_stop();
				int res = cpu_do_stop();
				if (speed_real && (res == ECTL_STATE_RUN)) {
					if (sound_enabled) buzzer_start();
					clock_gettime(CLOCK_MONOTONIC, &cpu_timer);
					cpu_time_cumulative = 0;
				} else if (res == ECTL_STATE_BIN) {
					cpu_do_bin(true); // initiate binary load
				}
				break;
			case ECTL_STATE_WAIT:
				if (speed_real) {
					if (atom_load_acquire(&rp) && !p && !mc) {
						cpu_state_change(ECTL_STATE_RUN, ECTL_STATE_WAIT);
					} else {
						cpu_time = throttle_granularity;
					}
				}
				else cpu_do_wait();
				break;
		}

		if (speed_real) cpu_timekeeping(cpu_time);
	}
}

// vim: tabstop=4 shiftwidth=4 autoindent
