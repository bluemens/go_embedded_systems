`timescale 1ns/1ps

module tb_sprite_frontend;

    parameter NUM_SPRITE = 32;
    parameter MAX_SLOT   = 8;

    logic clk;
    logic reset;
    logic start_row;
    logic [9:0] next_vcount;

    // frontend <-> RAM
    logic [$clog2(NUM_SPRITE)-1:0] attr_ra;
    logic [31:0] attr_rd;

    // frontend <-> drawer
    logic draw_req;
    logic draw_done;
    logic [9:0] fe_col;
    logic fe_flip;
    logic [7:0] fe_frame;
    logic [3:0] fe_rowoff;
    logic fe_done;

    // drawer <-> ROM
    logic [15:0] rom_addr;
    logic [15:0] rom_q;
    logic [9:0]  pixel_col;
    logic [15:0] pixel_data;
    logic wren;

    logic [31:0] sprite_attr_ram [NUM_SPRITE];

    logic [15:0] sprite_rom [0:65535];

    // clock generation
    always #5 clk = ~clk;

    // DUT: frontend
    sprite_frontend #(
        .NUM_SPRITE(NUM_SPRITE),
        .MAX_SLOT(MAX_SLOT)
    ) u_fe (
        .clk(clk),
        .reset(reset),
        .start_row(start_row),
        .next_vcount(next_vcount),
        .ra(attr_ra),
        .rd_data(attr_rd),
        .draw_done(draw_done),
        .draw_req(draw_req),
        .col_base(fe_col),
        .flip(fe_flip),
        .frame_id(fe_frame),
        .row_off(fe_rowoff),
        .fe_done(fe_done)
    );

    // Read from sprite_attr_ram
    always_ff @(posedge clk) begin
        attr_rd <= sprite_attr_ram[attr_ra];
    end

    // DUT: drawer
    sprite_drawer u_drawer (
        .clk        (clk),
        .reset      (reset),
        .start      (draw_req),
        .col_base   (fe_col),
        .flip       (fe_flip),
        .frame_id   (fe_frame),
        .row_off    (fe_rowoff),
        .rom_addr   (rom_addr),
        .rom_q      (rom_q),
        .pixel_col  (pixel_col),
        .pixel_data (pixel_data),
        .wren       (wren),
        .done       (draw_done)
    );

    // ROM read
    always_ff @(posedge clk) begin
        rom_q <= sprite_rom[rom_addr];
    end
    integer i;
    initial begin
        clk = 0;
        reset = 1;
        start_row = 0;
        next_vcount = 10'd0;

        sprite_attr_ram[0]  = 32'h83200000;
        sprite_attr_ram[1]  = 32'h83201401;
        sprite_attr_ram[2]  = 32'h83202802;
        sprite_attr_ram[3]  = 32'h83203C03;
        sprite_attr_ram[4]  = 32'h83205004;
        sprite_attr_ram[5]  = 32'h83206405;
        sprite_attr_ram[6]  = 32'h83207806;
        sprite_attr_ram[7]  = 32'h83208C07;
        sprite_attr_ram[8]  = 32'h8320A008;
        sprite_attr_ram[9]  = 32'h8320B409;
        sprite_attr_ram[10] = 32'h8320C80A;
        sprite_attr_ram[11] = 32'h8320DC0B;
        sprite_attr_ram[12] = 32'h8320F00C;
        sprite_attr_ram[13] = 32'h8321040D;
        sprite_attr_ram[14] = 32'h8321180E;
        sprite_attr_ram[15] = 32'h83212C0F;
        sprite_attr_ram[16] = 32'h83214010;
        sprite_attr_ram[17] = 32'h83215411;
        sprite_attr_ram[18] = 32'h83216812;
        sprite_attr_ram[19] = 32'h83217C13;
        sprite_attr_ram[20] = 32'h83219014;
        sprite_attr_ram[21] = 32'h8321A415;
        sprite_attr_ram[22] = 32'h8321B816;
        sprite_attr_ram[23] = 32'h8321CC17;
        sprite_attr_ram[24] = 32'h8321E018;
        sprite_attr_ram[25] = 32'h8321F419;
        sprite_attr_ram[26] = 32'h8322081A;
        sprite_attr_ram[27] = 32'h83221C1B;
        sprite_attr_ram[28] = 32'h8322301C;
        sprite_attr_ram[29] = 32'h8322441D;
        sprite_attr_ram[30] = 32'h8322581E;
        sprite_attr_ram[31] = 32'h83226C1F;

        #20 reset = 0;

        #20;
        next_vcount = 10'd200;
        start_row = 1;
        #10 start_row = 0;

        wait(fe_done);
        #20;

        $display("Simulation finished.");
        $stop;
    end

endmodule