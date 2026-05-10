/*
 * go_test.c — Phase 2 hardware bring-up test
 *
 * Opens /dev/mem, mmaps the LW HPS-to-FPGA bridge at 0xFF200000, and pokes
 * the go_peripheral registers to draw a few stones on the VGA monitor.
 *
 * Pattern: userspace /dev/mem + mmap (Chess-style), NOT the lab3 kernel-module
 * pattern. See design-document.md §8.3 for rationale.
 *
 * Build (on the DE1-SoC running Linux):
 *   gcc -O2 -Wall -o go_test go_test.c
 *
 * Run as root:
 *   sudo ./go_test
 *
 * Expected behavior on the VGA monitor:
 *   - Initially empty board (or whatever was there from prior boot)
 *   - "Reset board" → all cells empty, hoshi dots reappear
 *   - 5 stones placed at the hoshi points: 3 black, 2 white
 *   - Stones removed one by one via CLEAR_CELL
 *
 * If the visual matches, the Avalon write path + tilemap + procedural stone
 * rendering all work. If stones appear in the wrong cells, suspect bit
 * ordering in the cell_idx encoding. If stones are wrong color, suspect the
 * REG_SET_BLACK / REG_SET_WHITE constants.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>

/* HPS-to-FPGA Lightweight bridge — base address per Cyclone V HPS TRM. */
#define LW_BRIDGE_BASE   0xFF200000UL
#define LW_BRIDGE_SPAN   0x00200000UL    /* 2 MB */

/* go_peripheral register offsets (byte-addressed) — match go_peripheral.sv */
#define REG_SET_BLACK    0x00
#define REG_SET_WHITE    0x01
#define REG_CLEAR_CELL   0x02
#define REG_RESET_BOARD  0x03

/* go_peripheral_0 base offset within the LW bridge.
 * Phase 1b in PHASE1.md auto-assigned this to 0x0000 (we accepted the default
 * in Platform Designer). If you assigned a different offset, change this.
 */
#define GO_PERIPHERAL_OFFSET  0x0000

static volatile uint8_t *go_regs;   /* byte pointer into mmaped LW bridge */

static int hw_init(void)
{
    int fd = open("/dev/mem", O_RDWR | O_SYNC);
    if (fd < 0) { perror("open /dev/mem (need root?)"); return -1; }

    void *base = mmap(NULL, LW_BRIDGE_SPAN, PROT_READ | PROT_WRITE,
                      MAP_SHARED, fd, LW_BRIDGE_BASE);
    if (base == MAP_FAILED) { perror("mmap LW bridge"); close(fd); return -1; }

    go_regs = (volatile uint8_t *)base + GO_PERIPHERAL_OFFSET;
    /* fd can be closed; mapping persists until munmap */
    close(fd);
    return 0;
}

/* Single byte write — matches the 8-bit Avalon slave shape. */
static inline void wr8(uint32_t off, uint8_t val)
{
    go_regs[off] = val;
    /* Memory-mapped I/O: this is a posted write. The compiler won't reorder
     * (volatile) but the bus might. For our test, ordering between writes
     * doesn't matter since each operation completes independently. */
}

static int cell_idx(int row, int col)
{
    return row * 9 + col;
}

static void place_black(int row, int col) { wr8(REG_SET_BLACK,   cell_idx(row, col)); }
static void place_white(int row, int col) { wr8(REG_SET_WHITE,   cell_idx(row, col)); }
static void clear_cell (int row, int col) { wr8(REG_CLEAR_CELL,  cell_idx(row, col)); }
static void reset_board(void)             { wr8(REG_RESET_BOARD, 0x01); }

int main(void)
{
    printf("go_test (Phase 2): opening /dev/mem...\n");
    if (hw_init() != 0) return 1;
    printf("go_test: LW bridge mapped, go_peripheral at offset 0x%04x\n",
           GO_PERIPHERAL_OFFSET);

    printf("\n[1/3] reset_board — board should be empty, hoshi dots visible\n");
    reset_board();
    sleep(2);

    printf("\n[2/3] placing stones at the 5 hoshi points\n");
    place_black(2, 2);  /* upper-left hoshi  */
    place_white(2, 6);  /* upper-right hoshi */
    place_black(4, 4);  /* tengen (center)   */
    place_white(6, 2);  /* lower-left hoshi  */
    place_black(6, 6);  /* lower-right hoshi */
    printf("        observe the VGA monitor; you should see B/W/B/W/B at hoshi\n");
    sleep(3);

    printf("\n[3/3] clearing stones one at a time, 1 sec apart\n");
    clear_cell(2, 2); sleep(1);
    clear_cell(2, 6); sleep(1);
    clear_cell(4, 4); sleep(1);
    clear_cell(6, 2); sleep(1);
    clear_cell(6, 6);

    printf("\ngo_test: done. board should be empty again.\n");
    return 0;
}
