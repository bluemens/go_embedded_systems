/*
 * go_peripheral — Avalon memory-mapped VGA peripheral for the 9x9 Go game
 *
 * Phase 5: adds the score-strip framebuffer (640×60 8bpp double-buffered).
 *   - New second Avalon slave (strip_*) for the bulk pixel write window.
 *   - STRIP_SWAP register on the first slave (offset 0x05) arms a swap
 *     that fires on the next VS rising edge.
 *   - Strip region (py ∈ [420, 479]) sources pixels from the active strip
 *     buffer via a tiny inline 8-color palette.
 *
 * (Earlier phases: Phase 1 static board, Phase 2 tilemap + stones, Phase 3
 *  cursor + USB keyboard.)
 *
 * Register map (avalon_slave_0, byte-addressed):
 *   0x00  SET_BLACK    W   data = cell_idx → cells[idx] = BLACK
 *   0x01  SET_WHITE    W   data = cell_idx → cells[idx] = WHITE
 *   0x02  CLEAR_CELL   W   data = cell_idx → cells[idx] = EMPTY
 *   0x03  RESET_BOARD  W   any             → all cells = EMPTY
 *   0x04  CURSOR       W   {visible[7], cell_idx[6:0]}
 *   0x05  STRIP_SWAP   W   any             → swap strip buffers at next VS
 *   0x06..0x07         (reserved for Phase 8 — RENDER_MODE)
 *
 * Strip framebuffer (avalon_slave_1, 32-bit word-addressed):
 *   word 0..9599: 38,400 pixels of the BACK buffer (4 packed 8-bit pixels each)
 *
 * Pixel encoding (bottom 3 bits index a small palette):
 *   0: dark gray bg    1: white text       2: light gray
 *   3: green accent    4: gold (winner)    5: red (illegal/error)
 *   6: blue accent     7: burlywood
 */

module go_peripheral(
    input  logic        clk,
    input  logic        reset,

    // ── avalon_slave_0: control registers ────────────────────────────────
    input  logic [2:0]  address,
    input  logic        chipselect,
    input  logic        write,
    input  logic [7:0]  writedata,
    output logic [7:0]  readdata,        // for AUDIO_STATUS reads

    // ── avalon_slave_1: strip framebuffer write window ───────────────────
    input  logic [13:0] strip_address,
    input  logic        strip_chipselect,
    input  logic        strip_write,
    input  logic [31:0] strip_writedata,

    // VGA conduit
    output logic [7:0]  VGA_R, VGA_G, VGA_B,
    output logic        VGA_CLK, VGA_HS, VGA_VS,
                        VGA_BLANK_n, VGA_SYNC_n,

    // ── Audio conduit (to/from codec_interface in toplevel) ──────────────
    output logic [23:0] dac_left,
    output logic [23:0] dac_right,
    input  logic        advance
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

    // VS rising-edge detector → vsync_pulse for strip swap timing.
    logic vs_d;
    logic vsync_pulse;
    always_ff @(posedge clk) vs_d <= VGA_VS;
    assign vsync_pulse = (VGA_VS == 1'b1) && (vs_d == 1'b0);

    // ─── Avalon register decoder (slave 0) ──────────────────────────────────
    localparam logic [2:0] REG_SET_BLACK    = 3'h0;
    localparam logic [2:0] REG_SET_WHITE    = 3'h1;
    localparam logic [2:0] REG_CLEAR_CELL   = 3'h2;
    localparam logic [2:0] REG_RESET_BOARD  = 3'h3;
    localparam logic [2:0] REG_CURSOR       = 3'h4;
    localparam logic [2:0] REG_STRIP_SWAP   = 3'h5;
    localparam logic [2:0] REG_AUDIO_CMD    = 3'h6;
    localparam logic [2:0] REG_AUDIO_STATUS = 3'h7;

    logic        bm_write_en, bm_reset_all;
    logic [6:0]  bm_write_addr;
    logic [1:0]  bm_write_data;
    logic        strip_swap_request;
    logic        audio_cmd_valid;
    logic [2:0]  audio_cmd_value;

    logic        cursor_visible;
    logic [6:0]  cursor_idx;

    always_comb begin
        bm_write_en        = 1'b0;
        bm_write_addr      = writedata[6:0];
        bm_write_data      = 2'b00;
        bm_reset_all       = 1'b0;
        strip_swap_request = 1'b0;
        audio_cmd_valid    = 1'b0;
        audio_cmd_value    = writedata[2:0];

        if (chipselect && write) begin
            unique case (address)
                REG_SET_BLACK:   begin bm_write_en = 1; bm_write_data = 2'b01; end
                REG_SET_WHITE:   begin bm_write_en = 1; bm_write_data = 2'b10; end
                REG_CLEAR_CELL:  begin bm_write_en = 1; bm_write_data = 2'b00; end
                REG_RESET_BOARD: bm_reset_all       = 1'b1;
                REG_CURSOR:      ;   // handled in always_ff below
                REG_STRIP_SWAP:  strip_swap_request = 1'b1;
                REG_AUDIO_CMD:   audio_cmd_valid    = 1'b1;
                default: ;
            endcase
        end
    end

    /* AUDIO_STATUS reads: bit 0 = busy. */
    logic audio_busy;
    always_comb begin
        if (chipselect && !write && address == REG_AUDIO_STATUS)
            readdata = {7'b0, audio_busy};
        else
            readdata = 8'h00;
    end

    always_ff @(posedge clk) begin
        if (reset) begin
            cursor_visible <= 1'b1;
            cursor_idx     <= 7'd40;
        end else if (chipselect && write && address == REG_CURSOR) begin
            cursor_visible <= writedata[7];
            cursor_idx     <= writedata[6:0];
        end
    end

    // ─── Geometry ───────────────────────────────────────────────────────────
    localparam int BOARD_LEFT    = 131;
    localparam int BOARD_TOP     = 24;
    localparam int CELL_PITCH    = 42;
    localparam int HALF_CELL     = 21;
    localparam int BOARD_W       = 9 * CELL_PITCH;
    localparam int BOARD_H       = 9 * CELL_PITCH;
    localparam int STONE_R2_MAX  = 324;
    localparam int STAR_R2_MAX   = 9;
    localparam int STRIP_TOP     = 420;
    localparam int STRIP_BOTTOM  = 479;
    localparam int STRIP_HEIGHT  = STRIP_BOTTOM - STRIP_TOP + 1;   // 60

    logic [9:0] px, py;
    assign px = hcount[10:1];
    assign py = vcount;

    // ─── Board area pipeline ────────────────────────────────────────────────
    logic in_board;
    assign in_board = (px >= BOARD_LEFT) && (px < BOARD_LEFT + BOARD_W)
                   && (py >= BOARD_TOP)  && (py < BOARD_TOP + BOARD_H);

    logic [9:0] bx, by;
    assign bx = px - BOARD_LEFT;
    assign by = py - BOARD_TOP;

    logic [3:0] col, row;
    logic [9:0] local_x, local_y;
    always_comb begin
        col     = bx / CELL_PITCH;
        row     = by / CELL_PITCH;
        local_x = bx - col * CELL_PITCH;
        local_y = by - row * CELL_PITCH;
    end

    logic [6:0] cell_idx;
    assign cell_idx = row * 4'd9 + col;

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

    logic is_star_cell;
    assign is_star_cell = (row == 4'd4 && col == 4'd4)
                       || (row == 4'd2 && (col == 4'd2 || col == 4'd6))
                       || (row == 4'd6 && (col == 4'd2 || col == 4'd6));

    logic signed [10:0] dx, dy;
    logic        [21:0] d2;
    assign dx = $signed({1'b0, local_x}) - 11'sd21;
    assign dy = $signed({1'b0, local_y}) - 11'sd21;
    assign d2 = dx*dx + dy*dy;

    logic in_stone, on_grid, in_star_dot, on_cursor;
    assign in_stone    = (cell_value != 2'b00) && (d2 <= STONE_R2_MAX);
    assign on_grid     = (local_x == HALF_CELL) || (local_y == HALF_CELL);
    assign in_star_dot = is_star_cell && (cell_value == 2'b00) && (d2 <= STAR_R2_MAX);
    assign on_cursor   = cursor_visible && (cursor_idx == cell_idx)
                      && (d2 >= 22'd264) && (d2 <= 22'd400);

    // ─── Strip area pipeline ────────────────────────────────────────────────
    logic in_strip;
    assign in_strip = (py >= STRIP_TOP) && (py <= STRIP_BOTTOM);

    // pixel_addr = (py - 420) * 640 + px ; range 0..38399 within strip.
    // Quartus will pick the implementation; on Cyclone V it'll likely use
    // a single 18×18 DSP block for the multiply, free for our purposes.
    logic [15:0] strip_pixel_full;
    logic [13:0] strip_pixel_addr;
    assign strip_pixel_full = (py - 16'd420) * 16'd640 + px;
    assign strip_pixel_addr = strip_pixel_full[13:0];

    logic [7:0] strip_pixel;
    strip_fb sfb (
        .clk          (clk),
        .reset        (reset),
        .write_en     (strip_chipselect && strip_write),
        .write_addr   (strip_address),
        .write_data   (strip_writedata),
        .swap_request (strip_swap_request),
        .vsync_pulse  (vsync_pulse),
        .pixel_addr   (strip_pixel_addr),
        .pixel_out    (strip_pixel)
    );

    // ─── Audio ──────────────────────────────────────────────────────────────
    audio_controller ac (
        .clk             (clk),
        .reset           (reset),
        .audio_cmd       (audio_cmd_value),
        .audio_cmd_valid (audio_cmd_valid),
        .busy            (audio_busy),
        .advance         (advance),
        .dac_left        (dac_left),
        .dac_right       (dac_right)
    );

    // Strip palette — bottom 3 bits of the pixel byte index a small RGB table.
    logic [23:0] strip_rgb;
    always_comb begin
        unique case (strip_pixel[2:0])
            3'd0: strip_rgb = 24'h202020;   // dark gray bg
            3'd1: strip_rgb = 24'hF0F0F0;   // white text
            3'd2: strip_rgb = 24'h808080;   // light gray
            3'd3: strip_rgb = 24'h00C040;   // green accent
            3'd4: strip_rgb = 24'hFFD700;   // gold (winner)
            3'd5: strip_rgb = 24'hC04040;   // red (error)
            3'd6: strip_rgb = 24'h4080C0;   // blue accent
            3'd7: strip_rgb = 24'hDEB887;   // burlywood (board match)
        endcase
    end

    // ─── Color output ───────────────────────────────────────────────────────
    localparam logic [23:0] COLOR_BG       = 24'h202020;
    localparam logic [23:0] COLOR_BOARD_BG = 24'hDEB887;
    localparam logic [23:0] COLOR_LINE     = 24'h000000;
    localparam logic [23:0] COLOR_BLACK    = 24'h101010;
    localparam logic [23:0] COLOR_WHITE    = 24'hF0F0F0;
    localparam logic [23:0] COLOR_OUTLINE  = 24'h000000;
    localparam logic [23:0] COLOR_CURSOR   = 24'h00C040;

    always_comb begin
        if (!VGA_BLANK_n) begin
            {VGA_R, VGA_G, VGA_B} = 24'h000000;
        end else if (in_strip) begin
            {VGA_R, VGA_G, VGA_B} = strip_rgb;
        end else if (in_board) begin
            if (on_cursor) begin
                {VGA_R, VGA_G, VGA_B} = COLOR_CURSOR;
            end else if (in_stone) begin
                if (cell_value == 2'b01)
                    {VGA_R, VGA_G, VGA_B} = COLOR_BLACK;
                else if (d2 >= 22'd289)
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
