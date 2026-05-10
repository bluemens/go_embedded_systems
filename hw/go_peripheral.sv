/*
 * go_peripheral — Avalon memory-mapped VGA peripheral for the 9x9 Go game
 *
 * Phase 1: static empty board (no registers used yet, no tilemap, no audio).
 *          The Avalon slave shape matches lab3 vga_ball.sv exactly so the
 *          Qsys descriptor and HPS-side wiring is a known-working path —
 *          we'll widen the slave when we add real registers in Phase 2.
 *
 * Pipeline:
 *   - vga_counters runs at 50 MHz, generates hcount (0..1599) / vcount (0..524)
 *   - VGA_CLK = hcount[0] gives 25 MHz to the ADV7123 (lab3 pattern)
 *   - Effective pixel column = hcount[10:1] (0..639), row = vcount (0..479)
 *   - Procedural rendering: board background + grid lines + 5 star points
 *
 * References:
 *   - hw/vga_ball.sv                                       (lab3 skeleton)
 *   - references/Chess/source/hw/vga_board.sv              (region-mask draw)
 *   - references/FlappyBird/source/.../vga_ball_hw.tcl     (Qsys descriptor)
 *   - design-document.md §4 (Display Subsystem)
 *   - DE1-SoC User Manual §3.6.6, Tables 3-14 (timing), 3-16 (VGA pins)
 */

module go_peripheral(
    input  logic        clk,                  // 50 MHz, CLOCK_50 (PIN_AF14)
    input  logic        reset,                // active-high, lab3 convention

    // Avalon slave — same shape as lab3 vga_ball: write-only, 3-bit byte
    // address, 8-bit data. Phase 1 uses no registers. Phase 2+ will widen.
    input  logic [2:0]  address,
    input  logic        chipselect,
    input  logic        write,
    input  logic [7:0]  writedata,

    // VGA conduit, exported through Qsys to top-level pins per UM Table 3-16
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

    // Phase 1: writes are accepted but ignored. Wires kept live so the Qsys
    // generator doesn't optimize them away or error on undriven sinks.
    /* verilator lint_off UNUSED */
    wire _unused_ok = &{1'b0, address, chipselect, write, writedata, 1'b0};
    /* verilator lint_on UNUSED */

    // ─── Board geometry (design-document.md §4.1) ───────────────────────────
    localparam int BOARD_LEFT = 131;
    localparam int BOARD_TOP  = 24;
    localparam int CELL_PITCH = 42;
    localparam int HALF_CELL  = 21;
    localparam int BOARD_W    = 9 * CELL_PITCH;   // 378
    localparam int BOARD_H    = 9 * CELL_PITCH;

    // Effective pixel column. hcount[10:1] increments once per VGA pixel
    // (since hcount counts every 50 MHz cycle and VGA_CLK is hcount[0]).
    logic [9:0] px;
    assign px = hcount[10:1];
    logic [9:0] py;
    assign py = vcount;

    // ─── Region mask: inside board area? ────────────────────────────────────
    logic in_board;
    assign in_board = (px >= BOARD_LEFT) && (px < BOARD_LEFT + BOARD_W)
                   && (py >= BOARD_TOP)  && (py < BOARD_TOP + BOARD_H);

    // Position relative to board origin
    logic [9:0] bx, by;
    assign bx = px - BOARD_LEFT;
    assign by = py - BOARD_TOP;

    // Cell coords and local-in-cell coords. Synthesis will infer dividers
    // by 42 — which is small and acceptable for Phase 1; we can optimize
    // later by precomputing range tables if timing slips.
    logic [3:0] col, row;
    logic [9:0] local_x, local_y;
    always_comb begin
        col     = bx / CELL_PITCH;
        row     = by / CELL_PITCH;
        local_x = bx - col * CELL_PITCH;
        local_y = by - row * CELL_PITCH;
    end

    // Star points (hoshi): (2,2)(2,6)(4,4)(6,2)(6,6) per Go convention
    logic is_star_cell;
    assign is_star_cell = (row == 4'd4 && col == 4'd4)
                       || (row == 4'd2 && (col == 4'd2 || col == 4'd6))
                       || (row == 4'd6 && (col == 4'd2 || col == 4'd6));

    // Distance² from cell center, signed
    logic signed [10:0] dx, dy;
    logic        [21:0] d2;
    assign dx = $signed({1'b0, local_x}) - 11'sd21;
    assign dy = $signed({1'b0, local_y}) - 11'sd21;
    assign d2 = dx*dx + dy*dy;

    // Final region masks
    logic on_grid;
    logic in_star_dot;
    assign on_grid     = (local_x == HALF_CELL) || (local_y == HALF_CELL);
    assign in_star_dot = is_star_cell && (d2 <= 22'd9);   // r=3 dot

    // ─── Color output ───────────────────────────────────────────────────────
    // Phase 1: hardcoded RGB888. Phase 4 introduces the palette ROM.
    localparam logic [23:0] COLOR_BG       = 24'h202020;  // dark gray surround
    localparam logic [23:0] COLOR_BOARD_BG = 24'hDEB887;  // burlywood
    localparam logic [23:0] COLOR_LINE     = 24'h000000;  // black grid + hoshi

    always_comb begin
        if (!VGA_BLANK_n) begin
            // ADV7123 expects 0 outside active video [UM §3.6.6]
            {VGA_R, VGA_G, VGA_B} = 24'h000000;
        end else if (in_board) begin
            if (on_grid || in_star_dot)
                {VGA_R, VGA_G, VGA_B} = COLOR_LINE;
            else
                {VGA_R, VGA_G, VGA_B} = COLOR_BOARD_BG;
        end else begin
            {VGA_R, VGA_G, VGA_B} = COLOR_BG;
        end
    end

endmodule


// ─────────────────────────────────────────────────────────────────────────────
// vga_counters — VERBATIM from lab3 vga_ball.sv (Stephen Edwards, Columbia)
// 640x480 @ 60 Hz from a 50 MHz clock.
//   - hcount counts 0..1599 (1280 active + 32 FP + 192 sync + 96 BP)
//   - vcount counts 0..524  (480 active + 10 FP + 2 sync + 33 BP)
//   - VGA_CLK = hcount[0] → 25 MHz output to ADV7123
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
