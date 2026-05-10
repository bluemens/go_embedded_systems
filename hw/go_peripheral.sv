/*
 * go_peripheral — Avalon memory-mapped VGA peripheral for the 9x9 Go game
 *
 * Phase 2: tilemap-driven board with HPS-controllable stones.
 *   - Adds register decoder for SET_BLACK / SET_WHITE / CLEAR_CELL / RESET_BOARD.
 *   - Adds board_mem submodule (81 cells × 2 bits).
 *   - Adds procedural stone rendering: filled circle of stone color when the
 *     pixel falls inside a non-EMPTY cell's stone radius.
 *
 * The Avalon slave shape is unchanged from Phase 1 (3-bit byte address,
 * 8-bit data, write-only) so the Qsys IP stays imported as-is.
 *
 * Register map (byte-addressed, accessible from HPS via /dev/mem mmap):
 *   0x00  SET_BLACK    W   writedata = cell_idx (0..80) → cells[idx] = BLACK
 *   0x01  SET_WHITE    W   writedata = cell_idx          → cells[idx] = WHITE
 *   0x02  CLEAR_CELL   W   writedata = cell_idx          → cells[idx] = EMPTY
 *   0x03  RESET_BOARD  W   writedata = anything          → all cells = EMPTY
 *   0x04..0x07         (reserved for Phase 3 — cursor / render mode)
 *
 * Pipeline (combinational from VGA counters to RGB):
 *   hcount, vcount  →  px (=hcount[10:1]), py (=vcount)
 *                   →  bx, by  (in-board coords)
 *                   →  col, row, local_x, local_y
 *                   →  cell_idx, d² (squared distance from cell center)
 *                   →  cell_value = board_mem[cell_idx]
 *                   →  region masks (in_stone, on_grid, in_star_dot)
 *                   →  RGB
 *
 * If timing fails at 50 MHz, we'll add a pipeline register stage between the
 * cell_idx computation and the color decision. For Phase 2 we keep it
 * combinational and let Quartus optimize.
 *
 * References:
 *   - hw/board_mem.sv                         (tilemap module)
 *   - references/Chess/source/hw/vga_board.sv (procedural piece rendering pattern)
 *   - design-document.md §4 (Display Subsystem), §8 (register map)
 *   - DE1-SoC User Manual §3.6.6, Tables 3-14, 3-16
 */

module go_peripheral(
    input  logic        clk,
    input  logic        reset,

    // Avalon slave (unchanged from Phase 1)
    input  logic [2:0]  address,
    input  logic        chipselect,
    input  logic        write,
    input  logic [7:0]  writedata,

    // VGA conduit
    output logic [7:0]  VGA_R, VGA_G, VGA_B,
    output logic        VGA_CLK, VGA_HS, VGA_VS,
                        VGA_BLANK_n, VGA_SYNC_n
);

    // ─── VGA timing ─────────────────────────────────────────────────────────
    logic [10:0] hcount;
    logic [9:0]  vcount;

    vga_counters counters (
        .clk50       (clk),
        .reset       (reset),
        .hcount      (hcount),
        .vcount      (vcount),
        .VGA_HS      (VGA_HS),
        .VGA_VS      (VGA_VS),
        .VGA_BLANK_n (VGA_BLANK_n),
        .VGA_SYNC_n  (VGA_SYNC_n),
        .VGA_CLK     (VGA_CLK)
    );

    // ─── Avalon register decoder ────────────────────────────────────────────
    // Combinational: chipselect+write fires once per Avalon transaction
    // (writeWaitTime=0 in the Qsys descriptor).
    localparam logic [2:0] REG_SET_BLACK   = 3'h0;
    localparam logic [2:0] REG_SET_WHITE   = 3'h1;
    localparam logic [2:0] REG_CLEAR_CELL  = 3'h2;
    localparam logic [2:0] REG_RESET_BOARD = 3'h3;

    logic        bm_write_en;
    logic [6:0]  bm_write_addr;
    logic [1:0]  bm_write_data;
    logic        bm_reset_all;

    always_comb begin
        bm_write_en   = 1'b0;
        bm_write_addr = writedata[6:0];   // cell_idx in [0,80] (validated in board_mem)
        bm_write_data = 2'b00;
        bm_reset_all  = 1'b0;

        if (chipselect && write) begin
            unique case (address)
                REG_SET_BLACK:   begin bm_write_en = 1; bm_write_data = 2'b01; end
                REG_SET_WHITE:   begin bm_write_en = 1; bm_write_data = 2'b10; end
                REG_CLEAR_CELL:  begin bm_write_en = 1; bm_write_data = 2'b00; end
                REG_RESET_BOARD: begin bm_reset_all = 1;                       end
                default: ;  // reserved offsets ignored
            endcase
        end
    end

    // ─── Board geometry (design-document.md §4.1) ───────────────────────────
    localparam int BOARD_LEFT    = 131;
    localparam int BOARD_TOP     = 24;
    localparam int CELL_PITCH    = 42;
    localparam int HALF_CELL     = 21;
    localparam int BOARD_W       = 9 * CELL_PITCH;   // 378
    localparam int BOARD_H       = 9 * CELL_PITCH;
    localparam int STONE_R2_MAX  = 324;              // r=18 → r²=324
    localparam int STAR_R2_MAX   = 9;                // r=3 hoshi dot

    logic [9:0] px, py;
    assign px = hcount[10:1];
    assign py = vcount;

    logic in_board;
    assign in_board = (px >= BOARD_LEFT) && (px < BOARD_LEFT + BOARD_W)
                   && (py >= BOARD_TOP)  && (py < BOARD_TOP + BOARD_H);

    logic [9:0] bx, by;
    assign bx = px - BOARD_LEFT;
    assign by = py - BOARD_TOP;

    // Cell coords + local-in-cell coords. bx/by are bounded to [0, 377] so the
    // dividers/multipliers are small (4-bit result).
    logic [3:0] col, row;
    logic [9:0] local_x, local_y;
    always_comb begin
        col     = bx / CELL_PITCH;
        row     = by / CELL_PITCH;
        local_x = bx - col * CELL_PITCH;
        local_y = by - row * CELL_PITCH;
    end

    // cell_idx = row*9 + col. row ≤ 8 so row*9 ≤ 72; cell_idx fits in 7 bits.
    logic [6:0] cell_idx;
    assign cell_idx = row * 4'd9 + col;

    // ─── Tilemap read ───────────────────────────────────────────────────────
    logic [1:0] cell_value;
    board_mem bm (
        .clk        (clk),
        .reset      (reset),
        .write_en   (bm_write_en),
        .write_addr (bm_write_addr),
        .write_data (bm_write_data),
        .reset_all  (bm_reset_all),
        .read_addr  (cell_idx),
        .read_data  (cell_value)
    );

    // ─── Stone / grid / star-point geometry ─────────────────────────────────
    logic is_star_cell;
    assign is_star_cell = (row == 4'd4 && col == 4'd4)
                       || (row == 4'd2 && (col == 4'd2 || col == 4'd6))
                       || (row == 4'd6 && (col == 4'd2 || col == 4'd6));

    // signed dx, dy from cell center; squared distance
    logic signed [10:0] dx, dy;
    logic        [21:0] d2;
    assign dx = $signed({1'b0, local_x}) - 11'sd21;
    assign dy = $signed({1'b0, local_y}) - 11'sd21;
    assign d2 = dx*dx + dy*dy;

    logic in_stone;
    logic on_grid;
    logic in_star_dot;
    assign in_stone    = (cell_value != 2'b00) && (d2 <= STONE_R2_MAX);
    assign on_grid     = (local_x == HALF_CELL) || (local_y == HALF_CELL);
    assign in_star_dot = is_star_cell && (cell_value == 2'b00) && (d2 <= STAR_R2_MAX);

    // ─── Color output ───────────────────────────────────────────────────────
    // Phase 2: still hardcoded RGB888. Phase 4 introduces the palette ROM.
    localparam logic [23:0] COLOR_BG       = 24'h202020;
    localparam logic [23:0] COLOR_BOARD_BG = 24'hDEB887;
    localparam logic [23:0] COLOR_LINE     = 24'h000000;
    localparam logic [23:0] COLOR_BLACK    = 24'h101010;
    localparam logic [23:0] COLOR_WHITE    = 24'hF0F0F0;
    localparam logic [23:0] COLOR_OUTLINE  = 24'h000000;

    always_comb begin
        if (!VGA_BLANK_n) begin
            {VGA_R, VGA_G, VGA_B} = 24'h000000;
        end else if (in_board) begin
            if (in_stone) begin
                // Black stone: solid black. White stone: white with a 1-pixel
                // black outline at the edge of the radius (d² in top band) so
                // it remains visible against the burlywood board.
                if (cell_value == 2'b01)
                    {VGA_R, VGA_G, VGA_B} = COLOR_BLACK;
                else if (d2 >= 22'd289)               // r ≥ 17 → outline ring
                    {VGA_R, VGA_G, VGA_B} = COLOR_OUTLINE;
                else
                    {VGA_R, VGA_G, VGA_B} = COLOR_WHITE;
            end else if (on_grid || in_star_dot) begin
                {VGA_R, VGA_G, VGA_B} = COLOR_LINE;
            end else begin
                {VGA_R, VGA_G, VGA_B} = COLOR_BOARD_BG;
            end
        end else begin
            {VGA_R, VGA_G, VGA_B} = COLOR_BG;
        end
    end

endmodule


// ─────────────────────────────────────────────────────────────────────────────
// vga_counters — VERBATIM from lab3 vga_ball.sv (Stephen Edwards, Columbia)
// ─────────────────────────────────────────────────────────────────────────────
module vga_counters(
    input  logic        clk50, reset,
    output logic [10:0] hcount,
    output logic [9:0]  vcount,
    output logic        VGA_CLK, VGA_HS, VGA_VS, VGA_BLANK_n, VGA_SYNC_n
);

    parameter HACTIVE      = 11'd 1280;
    parameter HFRONT_PORCH = 11'd 32;
    parameter HSYNC        = 11'd 192;
    parameter HBACK_PORCH  = 11'd 96;
    parameter HTOTAL       = HACTIVE + HFRONT_PORCH + HSYNC + HBACK_PORCH;

    parameter VACTIVE      = 10'd 480;
    parameter VFRONT_PORCH = 10'd 10;
    parameter VSYNC        = 10'd 2;
    parameter VBACK_PORCH  = 10'd 33;
    parameter VTOTAL       = VACTIVE + VFRONT_PORCH + VSYNC + VBACK_PORCH;

    logic endOfLine;
    always_ff @(posedge clk50 or posedge reset)
        if (reset)          hcount <= 0;
        else if (endOfLine) hcount <= 0;
        else                hcount <= hcount + 11'd 1;
    assign endOfLine = hcount == HTOTAL - 1;

    logic endOfField;
    always_ff @(posedge clk50 or posedge reset)
        if (reset)          vcount <= 0;
        else if (endOfLine)
            if (endOfField) vcount <= 0;
            else            vcount <= vcount + 10'd 1;
    assign endOfField = vcount == VTOTAL - 1;

    assign VGA_HS      = !( (hcount[10:8] == 3'b101) & !(hcount[7:5] == 3'b111) );
    assign VGA_VS      = !( vcount[9:1] == (VACTIVE + VFRONT_PORCH) / 2 );
    assign VGA_SYNC_n  = 1'b0;
    assign VGA_BLANK_n = !( hcount[10] & (hcount[9] | hcount[8]) ) &
                         !( vcount[9] | (vcount[8:5] == 4'b1111) );
    assign VGA_CLK     = hcount[0];

endmodule
