`timescale 1ns/1ps

module tb_sprite_engine;

    parameter NUM_SPRITE = 32;
    parameter MAX_SLOT   = 8;

    logic clk;
    logic reset;
    logic sprite_start;
    logic [9:0] vcount;

    logic chipselect;
    logic write;
    logic [5:0] address;
    logic [31:0] writedata;

    logic spr_wr_en;
    logic [4:0] spr_wr_idx;
    logic [31:0] spr_wr_data;

    logic sprite_write_reg;
    logic [4:0] sprite_wr_idx;
    logic [31:0] sprite_writedata;

    always_ff @(posedge clk) begin
        if (reset) begin
            sprite_write_reg <= 0;
            sprite_wr_idx    <= 0;
            sprite_writedata <= 0;
        end else begin
            if (chipselect && write && address[5]) begin
                sprite_write_reg <= 1;
                sprite_wr_idx    <= address[4:0];
                sprite_writedata <= writedata;
            end else begin
                sprite_write_reg <= 0;
            end
        end
    end

    assign spr_wr_en   = sprite_write_reg;
    assign spr_wr_idx  = sprite_wr_idx;
    assign spr_wr_data = sprite_writedata;

    logic [9:0]  sprite_pixel_col;
    logic [15:0] sprite_pixel_data;
    logic        wren_pixel_draw;
    logic        done;

    always #5 clk = ~clk;

    // DUT
    sprite_engine #(
        .NUM_SPRITE(NUM_SPRITE),
        .MAX_SLOT(MAX_SLOT)
    ) u_eng (
        .clk(clk),
        .reset(reset),
        .sprite_start(sprite_start),
        .vcount(vcount),
        .spr_wr_en(spr_wr_en),
        .spr_wr_idx(spr_wr_idx),
        .spr_wr_data(spr_wr_data),
        .sprite_pixel_col(sprite_pixel_col),
        .sprite_pixel_data(sprite_pixel_data),
        .wren_pixel_draw(wren_pixel_draw),
        .done(done)
    );

    initial begin
        integer i;

        clk = 0;
        reset = 1;
        sprite_start = 0;
        vcount = 0;

        chipselect = 0;
        write = 0;
        address = 0;
        writedata = 0;

        #20 reset = 0;

        $display("Writing sprites...");
        write_sprite(0, 32'h83200000);
        write_sprite(1, 32'h83201401);
        write_sprite(2, 32'h83202802);
        write_sprite(3, 32'h83203C03);
        write_sprite(4, 32'h83205004);
        write_sprite(5, 32'h83206405);
        write_sprite(6, 32'h83207806);
        write_sprite(7, 32'h83208C07);
        write_sprite(8, 32'h8320A008);
        write_sprite(9, 32'h8320B409);
        write_sprite(10, 32'h8320C80A);
        write_sprite(11, 32'h8320DC0B);
        write_sprite(12, 32'h8320F00C);
        write_sprite(13, 32'h8321040D);
        write_sprite(14, 32'h8321180E);
        write_sprite(15, 32'h83212C0F);
        write_sprite(16, 32'h83214010);
        write_sprite(17, 32'h83215411);
        write_sprite(18, 32'h83216812);
        write_sprite(19, 32'h83217C13);
        write_sprite(20, 32'h83219014);
        write_sprite(21, 32'h8321A415);
        write_sprite(22, 32'h8321B816);
        write_sprite(23, 32'h8321CC17);
        write_sprite(24, 32'h8321E018);
        write_sprite(25, 32'h8321F419);
        write_sprite(26, 32'h8322081A);
        write_sprite(27, 32'h83221C1B);
        write_sprite(28, 32'h8322301C);
        write_sprite(29, 32'h8322441D);
        write_sprite(30, 32'h8322581E);
        write_sprite(31, 32'h83226C1F);
        $display("Write complete.");

        vcount = 200;
        sprite_start = 1;
        @(posedge clk);
        sprite_start = 0;
        @(posedge clk);
        @(posedge clk);
        @(posedge clk);
        @(posedge clk);
        @(posedge clk);

        wait(done);

        @(posedge clk);
        @(posedge clk);
        @(posedge clk);
        @(posedge clk);
        @(posedge clk);

        $display("Simulation complete: sprite_engine finished.");
        $stop;
    end

    task write_sprite(input [4:0] idx, input [31:0] data);
        begin
            @(posedge clk);
            chipselect = 1;
            write = 1;
            address = {1'b1, idx};
            writedata = data;

            @(posedge clk);
            chipselect = 0;
            write = 0;
            address = 0;
            writedata = 0;
        end
    endtask

endmodule