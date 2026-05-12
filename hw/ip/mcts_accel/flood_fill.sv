// flood_fill — wavefront BFS over an 81-bit bitboard, one cycle per step.
//
// Three calling modes share one datapath inside rollout_engine:
//   Capture detection:  propagate_mask = enemy_mask, liberty_mask = empties,
//                       seed = just-played stone's enemy neighbor
//   Suicide check:      propagate_mask = own_mask,   liberty_mask = empties,
//                       seed = just-played stone
//   Tromp-Taylor scoring: propagate_mask = empties,  liberty_mask = own_or_enemy,
//                       seed = first unvisited empty
//
// On rising start: latch inputs, init {visited, frontier} = {seed_bit, seed_bit},
// then each cycle expand frontier to neighbors, mask, OR into visited. Raise
// done when frontier becomes empty.
//
// Worst case 81 cycles (a snake-path group). Typical < 10 on 9x9.

import mcts_accel_pkg::*;

module flood_fill (
    input  logic        clk,
    input  logic        reset_n,
    input  logic        start,
    input  logic [80:0] seed_bit,
    input  logic [80:0] propagate_mask,
    input  logic [80:0] liberty_mask,
    output logic [80:0] group_mask,
    output logic [6:0]  group_size,
    output logic        has_liberty,
    output logic        done
);

    typedef enum logic [1:0] {
        IDLE = 2'd0,
        STEP = 2'd1,
        DONE_ST = 2'd2
    } state_t;

    state_t            state;
    logic [80:0]       visited;
    logic [80:0]       frontier;
    logic              lib_seen;

    logic [80:0] neighbors;
    logic [80:0] new_front;
    logic [80:0] lib_hits;

    always_comb begin
        neighbors = (shift_n(frontier) | shift_s(frontier)
                   | shift_w(frontier) | shift_e(frontier)) & ON_BOARD_MASK;
        lib_hits  = neighbors & liberty_mask & ~propagate_mask;
        new_front = neighbors & propagate_mask & ~visited;
    end

    always_ff @(posedge clk or negedge reset_n) begin
        if (!reset_n) begin
            state    <= IDLE;
            visited  <= '0;
            frontier <= '0;
            lib_seen <= 1'b0;
            done     <= 1'b0;
        end else begin
            done <= 1'b0;
            unique case (state)
                IDLE: begin
                    if (start) begin
                        visited  <= seed_bit;
                        frontier <= seed_bit;
                        lib_seen <= 1'b0;
                        state    <= STEP;
                    end
                end
                STEP: begin
                    if (|lib_hits) lib_seen <= 1'b1;
                    if (new_front == '0) begin
                        state <= DONE_ST;
                        done  <= 1'b1;
                    end else begin
                        visited  <= visited | new_front;
                        frontier <= new_front;
                    end
                end
                DONE_ST: begin
                    if (start) begin
                        visited  <= seed_bit;
                        frontier <= seed_bit;
                        lib_seen <= 1'b0;
                        state    <= STEP;
                    end else begin
                        state <= IDLE;
                    end
                end
                default: state <= IDLE;
            endcase
        end
    end

    assign group_mask  = visited;
    assign group_size  = popcount81(visited);
    assign has_liberty = lib_seen;

endmodule
