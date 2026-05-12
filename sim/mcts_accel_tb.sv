// mcts_accel_tb — drives the Avalon-MM slave protocol against mcts_accel.sv.
//
// Streams a board (162 board-load writes) via REG_BOARD_LOAD, kicks START,
// polls STATUS until done, then reads all 82 result words.

`timescale 1ns/1ps

import mcts_accel_pkg::*;

module mcts_accel_tb;

    localparam int N_ENGINES = 1;   // single-engine sanity build first
    localparam int N_SIMS    = 25;

    logic clk, reset_n;
    logic [7:0]  avs_address;
    logic        avs_read, avs_write;
    logic [31:0] avs_writedata;
    logic        avs_chipselect;
    logic [31:0] avs_readdata;

    mcts_accel #(.NUM_ENGINES(N_ENGINES), .SIMS_PER_ENGINE(N_SIMS)) dut (
        .clk(clk), .reset_n(reset_n),
        .avs_address(avs_address),
        .avs_read(avs_read), .avs_write(avs_write),
        .avs_writedata(avs_writedata),
        .avs_chipselect(avs_chipselect),
        .avs_readdata(avs_readdata)
    );

    localparam logic [7:0] REG_BOARD_LOAD = 8'h00;
    localparam logic [7:0] REG_TURN       = 8'h08;
    localparam logic [7:0] REG_SEED       = 8'h0C;
    localparam logic [7:0] REG_START      = 8'h10;
    localparam logic [7:0] REG_STATUS     = 8'h14;
    localparam logic [7:0] REG_READ_ADDR  = 8'h18;
    localparam logic [7:0] REG_RESULT     = 8'h1C;

    initial clk = 0;
    always #5 clk = ~clk;

    task avs_wr(input logic [7:0] addr, input logic [31:0] data);
        @(posedge clk);
        avs_address    <= addr;
        avs_writedata  <= data;
        avs_chipselect <= 1;
        avs_write      <= 1;
        @(posedge clk);
        avs_chipselect <= 0;
        avs_write      <= 0;
    endtask

    task avs_rd(input logic [7:0] addr, output logic [31:0] data);
        @(posedge clk);
        avs_address    <= addr;
        avs_chipselect <= 1;
        avs_read       <= 1;
        @(posedge clk);
        avs_chipselect <= 0;
        avs_read       <= 0;
        @(posedge clk);
        data = avs_readdata;
    endtask

    integer fi, fo, code, sim;
    logic [80:0] black_in, white_in, ko_b, ko_w;
    logic        turn_in;
    logic [31:0] seed_in;
    logic [15:0] sims_in;
    logic [15:0] wins   [0:81];
    logic [15:0] visits [0:81];
    logic [31:0] tmp;

    initial begin
        reset_n = 0;
        avs_address = 0; avs_writedata = 0;
        avs_read = 0; avs_write = 0; avs_chipselect = 0;
        #50;
        reset_n = 1;
        #20;

        fi = $fopen("vectors/mc_vectors.txt", "r");
        fo = $fopen("mcts_accel_dut.txt", "w");
        if (fi == 0 || fo == 0) begin
            $display("ERROR opening mcts vector files");
            $finish;
        end

        while (!$feof(fi)) begin
            code = $fscanf(fi, " %h %h %h %h %h %h %h",
                           black_in, white_in, turn_in,
                           ko_b, ko_w, seed_in, sims_in);
            if (code != 7) begin
                integer ch; ch = $fgetc(fi);
                while (ch != -1 && ch != "\n") ch = $fgetc(fi);
                continue;
            end

            // For now we don't parse the expected wins/visits inline — the
            // DUT prints them, and compare.py diffs the input echoes too.
            // Skip the rest of the line.
            begin : skip_expected
                integer ch; ch = $fgetc(fi);
                while (ch != -1 && ch != "\n") ch = $fgetc(fi);
            end

            // Stream board in: 81 black-plane writes, 81 white-plane writes.
            for (int i = 0; i < 81; i++)
                avs_wr(REG_BOARD_LOAD, {15'b0, 1'b0, i[6:0], 1'b0, 7'b0, black_in[i]});
            for (int i = 0; i < 81; i++)
                avs_wr(REG_BOARD_LOAD, {15'b0, 1'b1, i[6:0], 1'b0, 7'b0, white_in[i]});

            avs_wr(REG_TURN, {31'b0, turn_in});
            avs_wr(REG_SEED, seed_in);
            avs_wr(REG_START, 32'h1);

            // Poll status until done bit (bit 0) sets
            tmp = 0;
            while ((tmp & 32'h1) == 0) begin
                avs_rd(REG_STATUS, tmp);
            end

            // Read 82 results
            for (int i = 0; i < 82; i++) begin
                avs_wr(REG_READ_ADDR, i);
                avs_rd(REG_RESULT, tmp);
                wins[i]   = tmp[15:0];
                visits[i] = tmp[31:16];
            end

            $fwrite(fo, "%021h %021h %1h %021h %021h %08h %04h ",
                    black_in, white_in, turn_in, ko_b, ko_w, seed_in, sims_in);
            for (int i = 0; i < 82; i++) $fwrite(fo, "%04h", wins[i]);
            $fwrite(fo, " ");
            for (int i = 0; i < 82; i++) $fwrite(fo, "%04h", visits[i]);
            $fwrite(fo, "\n");
        end

        $fclose(fi); $fclose(fo);
        $display("mcts_accel_tb done");
        $finish;
    end

endmodule
