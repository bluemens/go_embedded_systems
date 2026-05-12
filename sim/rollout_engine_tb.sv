// rollout_engine_tb — exercises hw/ip/mcts_accel/rollout_engine.sv.
//
// Input vectors (sim/vectors/re_vectors.txt):
//   <black:21h> <white:21h> <turn:1h> <ko_b:21h> <ko_w:21h>
//   <seed:8h> <expected_first_cell:2h> <expected_winner:1h> [# name]
//
// One rollout per vector: SIMS_PER_ENGINE is overridden to 1 for the DUT.

`timescale 1ns/1ps

import mcts_accel_pkg::*;

module rollout_engine_tb;

    logic clk, reset_n, start;
    logic [80:0] black_in, white_in;
    logic        turn_in;
    logic [80:0] ko_prev_b, ko_prev_w;
    logic [31:0] seed;
    logic        done;
    logic [15:0] wins_acc   [0:81];
    logic [15:0] visits_acc [0:81];

    rollout_engine #(
        .ENGINE_ID(0),
        .SIMS_PER_ENGINE(1)
    ) dut (
        .clk(clk), .reset_n(reset_n),
        .start(start),
        .black_in(black_in), .white_in(white_in),
        .turn_in(turn_in),
        .ko_prev_b(ko_prev_b), .ko_prev_w(ko_prev_w),
        .seed(seed),
        .done(done),
        .wins_acc(wins_acc),
        .visits_acc(visits_acc)
    );

    initial clk = 0;
    always #5 clk = ~clk;

    integer fi, fo, code;
    logic [6:0] exp_first;
    logic       exp_winner;

    // Find the first-cell with visits == 1 in the engine output (only one
    // rollout means exactly one cell has a visit).
    function automatic logic [6:0] find_first_cell();
        for (int i = 0; i < 82; i++) if (visits_acc[i] != 0) return i[6:0];
        return 7'd81;
    endfunction

    initial begin
        reset_n = 0;
        start = 0;
        #50;
        reset_n = 1;
        #20;

        fi = $fopen("vectors/re_vectors.txt", "r");
        fo = $fopen("rollout_engine_dut.txt", "w");
        if (fi == 0 || fo == 0) begin
            $display("ERROR opening rollout vector files");
            $finish;
        end

        while (!$feof(fi)) begin
            code = $fscanf(fi, " %h %h %h %h %h %h %h %h\n",
                           black_in, white_in, turn_in,
                           ko_prev_b, ko_prev_w, seed,
                           exp_first, exp_winner);
            if (code != 8) begin
                integer ch;
                ch = $fgetc(fi);
                while (ch != -1 && ch != "\n") ch = $fgetc(fi);
                continue;
            end

            // Reset between vectors to clear accumulators
            reset_n = 0; @(posedge clk); reset_n = 1; @(posedge clk);

            @(posedge clk); start <= 1;
            @(posedge clk); start <= 0;
            while (!done) @(posedge clk);

            $fwrite(fo, "%021h %021h %1h %021h %021h %08h %02h %1h\n",
                    black_in, white_in, turn_in,
                    ko_prev_b, ko_prev_w, seed,
                    find_first_cell(),
                    (wins_acc[find_first_cell()] != 0) ?
                        (turn_in == 1'b0 ? 1'b0 : 1'b1) :
                        (turn_in == 1'b0 ? 1'b1 : 1'b0));
            @(posedge clk);
        end

        $fclose(fi); $fclose(fo);
        $display("rollout_engine_tb done");
        $finish;
    end

endmodule
