/*
 * hw_timer.c — userspace bindings for the Phase 9 cycle counter
 *
 * See hw_timer.h. Register offsets match go_peripheral.sv:
 *   0x10 TIMER_START (W), 0x11 TIMER_STOP (W),
 *   0x12..0x15 TIMER_D0..D3 (R), 0x16 TIMER_LIVE0 (R).
 */

#include "hw_timer.h"

#define REG_TIMER_START   0x10
#define REG_TIMER_STOP    0x11
#define REG_TIMER_D0      0x12
#define REG_TIMER_D1      0x13
#define REG_TIMER_D2      0x14
#define REG_TIMER_D3      0x15
#define REG_TIMER_LIVE0   0x16

static volatile uint8_t *regs;

void hw_timer_init(volatile uint8_t *go_regs_base)
{
    regs = go_regs_base;
}

void hw_timer_start(void)
{
    /* Any write to REG_TIMER_START latches cycles_count → t_start. */
    regs[REG_TIMER_START] = 1;
}

uint32_t hw_timer_stop_cycles(void)
{
    /* Any write to REG_TIMER_STOP latches (cycles_count − t_start). */
    regs[REG_TIMER_STOP] = 1;
    uint32_t b0 = regs[REG_TIMER_D0];
    uint32_t b1 = regs[REG_TIMER_D1];
    uint32_t b2 = regs[REG_TIMER_D2];
    uint32_t b3 = regs[REG_TIMER_D3];
    return b0 | (b1 << 8) | (b2 << 16) | (b3 << 24);
}

uint8_t hw_timer_live_byte(void)
{
    return regs[REG_TIMER_LIVE0];
}
