/*
 * hw_timer — userspace wrapper around the Phase 9 FPGA cycle counter
 *
 * The peripheral has a 32-bit free-running counter at 50 MHz, with start
 * and stop/delta capture registers (see go_peripheral.sv §"HW cycle timer").
 *
 * Typical use:
 *     hw_timer_init(go_regs_base);
 *     hw_timer_start();
 *     ... work ...
 *     uint32_t cycles = hw_timer_stop_cycles();
 *     double   ms     = hw_timer_cycles_to_ms(cycles);
 *
 * 32 bits @ 50 MHz wraps at ~85.9 s — enough for an MCTS turn but not an
 * entire game. If the delta returned is suspiciously small, the counter
 * wrapped during the measurement; reset and retry.
 */

#ifndef _HW_TIMER_H
#define _HW_TIMER_H

#include <stdint.h>

/* Pass the same `go_regs` byte-pointer that go_main.c uses for wr8. The
 * timer registers live at fixed byte offsets 0x10..0x16 in that slave. */
void hw_timer_init(volatile uint8_t *go_regs_base);

/* Latch start. Side-effect: writes REG_TIMER_START. */
void hw_timer_start(void);

/* Latch stop, then read the 32-bit delta back (4 byte reads). Returns the
 * elapsed cycles between the last hw_timer_start() and this call. */
uint32_t hw_timer_stop_cycles(void);

/* Read the live counter's low byte (sanity check that the counter ticks). */
uint8_t  hw_timer_live_byte(void);

/* 50 MHz clock → 50 cycles per µs, 50,000 per ms. */
static inline double hw_timer_cycles_to_us(uint32_t c) { return (double)c / 50.0; }
static inline double hw_timer_cycles_to_ms(uint32_t c) { return (double)c / 50000.0; }

#endif /* _HW_TIMER_H */
