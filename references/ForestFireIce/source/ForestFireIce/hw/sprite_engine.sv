/* sprite_attr
[31] : Enable = 1, Disable = 0
[30] : Flip = 1, otherise = 0
[29: 27]: Reserved
[26:18] sprite_pos_row (0-479)
[17:8] : sprite_pos_col (0-639)
[7:0] : frame_id (0-255)
*/

// Test example: 1000 0100 0000 0001 0000 0000 0000 0001
//               8      4   0   1       0   0   0   1
module sprite_engine #(
    parameter NUM_SPRITE = 32,
    parameter MAX_SLOT   = 8
)(
    input  logic        clk,
    input  logic        reset,

    input  logic        sprite_start,

    input  logic [9:0]  vcount,

    input  logic                            spr_wr_en,
    input  logic [$clog2(NUM_SPRITE)-1:0]   spr_wr_idx,
    input  logic [31:0]                     spr_wr_data,

    output logic [9:0]  sprite_pixel_col,
    output logic [15:0] sprite_pixel_data,
    output logic        wren_pixel_draw,
    // debug
    // input logic [4:0] debug_addr,
    // output logic [31:0] debug_data,

    output logic        done
);
    logic [9:0] next_vcount;
    assign next_vcount = (vcount < 10'd479) ? vcount + 10'd1 :
                         (vcount == 10'd524) ? 10'd0      : vcount + 1;

    logic [31:0] attr_rd;
    logic [$clog2(NUM_SPRITE)-1:0] attr_ra;

    sprite_attr_ram u_ram(
        .clock (clk),
        .data (spr_wr_data),
        .rdaddress (attr_ra),
        .wraddress (spr_wr_idx),
        .wren (spr_wr_en),
        .q(attr_rd) );

    // FE
    logic fe_draw_req, fe_flip, fe_done;
    logic dw_done;
    logic [9:0] fe_col;
    logic [7:0] fe_frame;
    logic [3:0] fe_rowoff;

    sprite_frontend #(
        .NUM_SPRITE (NUM_SPRITE),
        .MAX_SLOT   (MAX_SLOT)
    ) u_fe (
        .clk        (clk),
        .reset      (reset),
        .start_row  (sprite_start),
        .next_vcount(next_vcount),
        .ra         (attr_ra),
        .rd_data    (attr_rd),
        .draw_done (dw_done),
        .draw_req   (fe_draw_req),
        .col_base   (fe_col),
        .flip       (fe_flip),
        .frame_id   (fe_frame),
        .row_off    (fe_rowoff),
        .fe_done    (fe_done)
    );

    // ------------------- ROM ------------------------------------------
    logic [15:0] rom_addr, rom_q;
    sprite_pattern_rom u_rom (.clock(clk), .address(rom_addr), .q(rom_q));

    // ------------------- Drawer ---------------------------------------
    sprite_drawer u_dw (
        .clk       (clk),
        .reset     (reset),
        .start     (fe_draw_req),
        .col_base  (fe_col),
        .flip      (fe_flip),
        .frame_id  (fe_frame),
        .row_off   (fe_rowoff),
        .rom_addr  (rom_addr),
        .rom_q     (rom_q),
        .pixel_col (sprite_pixel_col),
        .pixel_data(sprite_pixel_data),
        .wren      (wren_pixel_draw),
        .done      (dw_done)
    );

    assign done = (fe_done) || (vcount >= 479 && vcount < 524);

endmodule
