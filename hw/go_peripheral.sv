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
 * Phase 6: audio (sound effects), AUDIO_CMD / AUDIO_STATUS at 0x06/0x07.
 *
 * Phase 9: live demo features.
 *   - Heat-map overlay tilemap (81 × 4-bit), rendered into empty cells when
 *     overlay_en is set. Software publishes per-cell win-rate during MCTS.
 *   - 32-bit free-running HW cycle counter at 50 MHz with start-capture +
 *     stop/delta-capture. Used to time AI moves on both SW and HW backends.
 *   - avalon_slave_0 address widened 3 → 5 bits (8 → 32 registers).
 *
 * Register map (avalon_slave_0, byte-addressed):
 *   0x00  SET_BLACK    W   data = cell_idx → cells[idx] = BLACK
 *   0x01  SET_WHITE    W   data = cell_idx → cells[idx] = WHITE
 *   0x02  CLEAR_CELL   W   data = cell_idx → cells[idx] = EMPTY
 *   0x03  RESET_BOARD  W   any             → all cells = EMPTY
 *   0x04  CURSOR       W   {visible[7], cell_idx[6:0]}
 *   0x05  STRIP_SWAP   W   any             → swap strip buffers at next VS
 *   0x06  AUDIO_CMD    W   data[2:0]: 0=stop,1=place,2=cap,3=ill,4=over
 *   0x07  AUDIO_STATUS R   bit 0 = audio busy
 *   0x08  HEAT_IDX     W   data[6:0] = cell_idx for next HEAT_VAL write
 *   0x09  HEAT_VAL     W   data[3:0] = heat value; commits at last HEAT_IDX
 *   0x0A  OVERLAY_EN   W   data[0]   = 1 enable heat overlay, 0 disable
 *   0x0B  HEAT_CLEAR   W   any       = clear all 81 heat cells to 0
 *   0x10  TIMER_START  W   any       = latch cycles_count → t_start
 *   0x11  TIMER_STOP   W   any       = latch (cycles_count − t_start) → delta
 *   0x12  TIMER_D0     R   delta[7:0]
 *   0x13  TIMER_D1     R   delta[15:8]
 *   0x14  TIMER_D2     R   delta[23:16]
 *   0x15  TIMER_D3     R   delta[31:24]
 *   0x16  TIMER_LIVE0  R   cycles_count[7:0] (sanity-check that counter ticks)
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
    // Phase 9: address widened from 3 to 5 bits (8 → 32 register slots).
    input  logic [4:0]  address,
    input  logic        chipselect,
    input  logic        write,
    input  logic [7:0]  writedata,
    output logic [7:0]  readdata,        // for AUDIO_STATUS + TIMER reads

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
    localparam logic [4:0] REG_SET_BLACK    = 5'h00;
    localparam logic [4:0] REG_SET_WHITE    = 5'h01;
    localparam logic [4:0] REG_CLEAR_CELL   = 5'h02;
    localparam logic [4:0] REG_RESET_BOARD  = 5'h03;
    localparam logic [4:0] REG_CURSOR       = 5'h04;
    localparam logic [4:0] REG_STRIP_SWAP   = 5'h05;
    localparam logic [4:0] REG_AUDIO_CMD    = 5'h06;
    localparam logic [4:0] REG_AUDIO_STATUS = 5'h07;
    // Phase 9: heat-map overlay
    localparam logic [4:0] REG_HEAT_IDX     = 5'h08;
    localparam logic [4:0] REG_HEAT_VAL     = 5'h09;
    localparam logic [4:0] REG_OVERLAY_EN   = 5'h0A;
    localparam logic [4:0] REG_HEAT_CLEAR   = 5'h0B;
    // Phase 9: HW cycle timer
    localparam logic [4:0] REG_TIMER_START  = 5'h10;
    localparam logic [4:0] REG_TIMER_STOP   = 5'h11;
    localparam logic [4:0] REG_TIMER_D0     = 5'h12;
    localparam logic [4:0] REG_TIMER_D1     = 5'h13;
    localparam logic [4:0] REG_TIMER_D2     = 5'h14;
    localparam logic [4:0] REG_TIMER_D3     = 5'h15;
    localparam logic [4:0] REG_TIMER_LIVE0  = 5'h16;

    logic        bm_write_en, bm_reset_all;
    logic [6:0]  bm_write_addr;
    logic [1:0]  bm_write_data;
    logic        strip_swap_request;
    logic        audio_cmd_valid;
    logic [2:0]  audio_cmd_value;

    logic        cursor_visible;
    logic [6:0]  cursor_idx;

    // Phase 9: heat-map overlay state
    logic [6:0]  heat_idx_latch;
    logic        hm_write_en;
    logic        hm_clear_all;
    logic        overlay_en;

    // Phase 9: free-running HW timer
    logic [31:0] cycles_count;
    logic [31:0] t_start;
    logic [31:0] t_delta_latched;

    always_comb begin
        bm_write_en        = 1'b0;
        bm_write_addr      = writedata[6:0];
        bm_write_data      = 2'b00;
        bm_reset_all       = 1'b0;
        strip_swap_request = 1'b0;
        audio_cmd_valid    = 1'b0;
        audio_cmd_value    = writedata[2:0];
        hm_write_en        = 1'b0;
        hm_clear_all       = 1'b0;

        if (chipselect && write) begin
            unique case (address)
                REG_SET_BLACK:   begin bm_write_en = 1; bm_write_data = 2'b01; end
                REG_SET_WHITE:   begin bm_write_en = 1; bm_write_data = 2'b10; end
                REG_CLEAR_CELL:  begin bm_write_en = 1; bm_write_data = 2'b00; end
                REG_RESET_BOARD: bm_reset_all       = 1'b1;
                REG_CURSOR:      ;   // handled in always_ff below
                REG_STRIP_SWAP:  strip_swap_request = 1'b1;
                REG_AUDIO_CMD:   audio_cmd_valid    = 1'b1;
                REG_HEAT_IDX:    ;   // handled in always_ff below
                REG_HEAT_VAL:    hm_write_en        = 1'b1;
                REG_OVERLAY_EN:  ;   // handled in always_ff below
                REG_HEAT_CLEAR:  hm_clear_all       = 1'b1;
                REG_TIMER_START: ;   // handled in timer always_ff
                REG_TIMER_STOP:  ;   // handled in timer always_ff
                default: ;
            endcase
        end
    end

    /* Readdata mux: AUDIO_STATUS, TIMER delta bytes, TIMER_LIVE0. */
    logic audio_busy;
    always_comb begin
        readdata = 8'h00;
        if (chipselect && !write) begin
            unique case (address)
                REG_AUDIO_STATUS: readdata = {7'b0, audio_busy};
                REG_TIMER_D0:     readdata = t_delta_latched[7:0];
                REG_TIMER_D1:     readdata = t_delta_latched[15:8];
                REG_TIMER_D2:     readdata = t_delta_latched[23:16];
                REG_TIMER_D3:     readdata = t_delta_latched[31:24];
                REG_TIMER_LIVE0:  readdata = cycles_count[7:0];
                default:          readdata = 8'h00;
            endcase
        end
    end

    /* State registers: cursor, heat-overlay index/enable. */
    always_ff @(posedge clk) begin
        if (reset) begin
            cursor_visible <= 1'b1;
            cursor_idx     <= 7'd40;
            heat_idx_latch <= 7'd0;
            overlay_en     <= 1'b0;
        end else if (chipselect && write) begin
            unique case (address)
                REG_CURSOR: begin
                    cursor_visible <= writedata[7];
                    cursor_idx     <= writedata[6:0];
                end
                REG_HEAT_IDX:    heat_idx_latch <= writedata[6:0];
                REG_OVERLAY_EN:  overlay_en     <= writedata[0];
                default: ;
            endcase
        end
    end

    /* Free-running cycle counter + start/delta latches.
     * cycles_count ticks every clk_50 edge, wrapping at ~85.9 s.
     * Software writes TIMER_START to capture t_start, then later writes
     * TIMER_STOP to capture (cycles_count − t_start) into t_delta_latched.
     * t_delta_latched is held until the next TIMER_STOP write. */
    always_ff @(posedge clk) begin
        if (reset) begin
            cycles_count    <= 32'b0;
            t_start         <= 32'b0;
            t_delta_latched <= 32'b0;
        end else begin
            cycles_count <= cycles_count + 32'd1;
            if (chipselect && write && address == REG_TIMER_START)
                t_start <= cycles_count;
            if (chipselect && write && address == REG_TIMER_STOP)
                t_delta_latched <= cycles_count - t_start;
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

    /* Phase 9: heat-map tilemap. Reads combinationally per pixel from the
     * VGA scanout cell_idx; writes commit when software pulses HEAT_VAL
     * after setting HEAT_IDX. */
    logic [3:0] heat_value;
    heat_map hm (
        .clk        (clk),
        .reset      (reset),
        .write_en   (hm_write_en),
        .write_addr (heat_idx_latch),
        .write_data (writedata[3:0]),
        .clear_all  (hm_clear_all),
        .read_addr  (cell_idx),
        .read_data  (heat_value)
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
    logic [15:0] strip_pixel_addr;
    assign strip_pixel_full = (py - 16'd420) * 16'd640 + px;
    assign strip_pixel_addr = strip_pixel_full;        // full 16 bits → strip_fb

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

    /* Heat palette — 16 entries on a red→yellow→green spectrum.
     * heat_value 0 falls through to plain COLOR_BOARD_BG; 1..15 tint the
     * empty cell's background. Grid lines, star points, cursor, and stones
     * all draw on top of any heat tint by virtue of branch ordering. */
    logic [23:0] heat_rgb;
    always_comb begin
        unique case (heat_value)
            4'd1:    heat_rgb = 24'hB44040;
            4'd2:    heat_rgb = 24'hC84838;
            4'd3:    heat_rgb = 24'hD85830;
            4'd4:    heat_rgb = 24'hE87030;
            4'd5:    heat_rgb = 24'hF09038;
            4'd6:    heat_rgb = 24'hF0B048;
            4'd7:    heat_rgb = 24'hE8C860;
            4'd8:    heat_rgb = 24'hD8D870;
            4'd9:    heat_rgb = 24'hB8D868;
            4'd10:   heat_rgb = 24'h90D058;
            4'd11:   heat_rgb = 24'h70C850;
            4'd12:   heat_rgb = 24'h50C048;
            4'd13:   heat_rgb = 24'h38B040;
            4'd14:   heat_rgb = 24'h28A038;
            4'd15:   heat_rgb = 24'h209030;
            default: heat_rgb = COLOR_BOARD_BG;     // heat_value == 0
        endcase
    end

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
            end else if (cell_value == 2'b00 && overlay_en && heat_value != 4'd0) begin
                {VGA_R, VGA_G, VGA_B} = heat_rgb;
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
