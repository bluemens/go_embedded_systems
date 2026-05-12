/*
 * hw_iface.h — /dev/mem + mmap accessor for the LW HPS-to-FPGA bridge.
 *
 * Pattern adapted from references/Chess/source/sw/chess.c. All HW-resident
 * peripherals (go_peripheral, mcts_accel) share one 2 MB span at 0xFF200000;
 * each peripheral owns a distinct offset range within it.
 */
#ifndef _HW_IFACE_H
#define _HW_IFACE_H

#include <stdint.h>

#define LW_BRIDGE_BASE  0xFF200000UL
#define LW_BRIDGE_SPAN  0x00200000UL

int      hw_init(void);
void     hw_close(void);
uint32_t hw_read(uint32_t off);
void     hw_write(uint32_t off, uint32_t val);

#endif
