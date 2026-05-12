// mcts_accel — top-level MCTS rollout dispatcher.
//
// Avalon-MM LW slave at base 0x020 (relative to LW bridge). Register map per
// design-doc §8.1 / §7.5.8.
//
// Outer FSM: IDLE → BROADCAST → SEED → RUN → DRAIN → REPORT → DONE_ST → IDLE.
// N parallel rollout_engine instances are fanned out via SystemVerilog
// generate, mirroring references/Systolic/source_code/rtl/PE_array.v.

import mcts_accel_pkg::*;

module mcts_accel #(
    parameter int NUM_ENGINES     = 8,
    parameter int SIMS_PER_ENGINE = 25
)(
    input  logic        clk,
    input  logic        reset_n,

    // Avalon-MM lightweight slave (byte-addressed, 32-bit data)
    input  logic [7:0]  avs_address,
    input  logic        avs_read,
    input  logic        avs_write,
    input  logic [31:0] avs_writedata,
    input  logic        avs_chipselect,
    output logic [31:0] avs_readdata
);

    // ===== Register offsets (byte addresses, see design-doc §8.1) =========
    localparam logic [7:0] REG_BOARD_LOAD = 8'h00;  // mapped at base+0x20 in parent
    localparam logic [7:0] REG_KO_LOAD    = 8'h04;
    localparam logic [7:0] REG_TURN       = 8'h08;
    localparam logic [7:0] REG_SEED       = 8'h0C;
    localparam logic [7:0] REG_START      = 8'h10;
    localparam logic [7:0] REG_STATUS     = 8'h14;
    localparam logic [7:0] REG_READ_ADDR  = 8'h18;
    localparam logic [7:0] REG_RESULT     = 8'h1C;
    localparam logic [7:0] REG_RESET      = 8'h20;

    // ===== Storage =======================================================
    logic [80:0] root_black, root_white;
    logic        root_turn;
    logic [80:0] ko_b, ko_w;
    logic [31:0] seed_base;
    logic [6:0]  read_addr;
    logic [31:0] cycles_count;
    logic [15:0] result_wins   [0:81];
    logic [15:0] result_visits [0:81];

    // ===== Outer FSM =====================================================
    typedef enum logic [2:0] {
        IDLE      = 3'd0,
        BROADCAST = 3'd1,
        SEED_ST   = 3'd2,
        RUN       = 3'd3,
        DRAIN     = 3'd4,
        REPORT    = 3'd5,
        DONE_ST   = 3'd6
    } mcts_state_t;

    mcts_state_t state;
    logic              start_req;        // set by REG_START write, cleared in BROADCAST
    logic              reset_req;        // set by REG_RESET
    logic              broadcast_start;
    logic [NUM_ENGINES-1:0] eng_done;
    logic [15:0]       eng_wins   [NUM_ENGINES][0:81];
    logic [15:0]       eng_visits [NUM_ENGINES][0:81];
    logic [6:0]        drain_i;

    // ===== Engine instantiation (genvar fan-out) =========================
    generate
        for (genvar gi = 0; gi < NUM_ENGINES; gi++) begin: eng
            rollout_engine #(
                .ENGINE_ID(gi),
                .SIMS_PER_ENGINE(SIMS_PER_ENGINE)
            ) ru (
                .clk(clk),
                .reset_n(reset_n & ~reset_req),
                .start(broadcast_start),
                .black_in(root_black),
                .white_in(root_white),
                .turn_in(root_turn),
                .ko_prev_b(ko_b),
                .ko_prev_w(ko_w),
                .seed(seed_base ^ {gi[3:0], 12'h5A5, 16'h0}),
                .done(eng_done[gi]),
                .wins_acc(eng_wins[gi]),
                .visits_acc(eng_visits[gi])
            );
        end
    endgenerate

    wire all_done = &eng_done;

    // ===== Avalon write side =============================================
    always_ff @(posedge clk or negedge reset_n) begin
        if (!reset_n) begin
            root_black <= '0;
            root_white <= '0;
            root_turn  <= 1'b0;
            ko_b       <= '0;
            ko_w       <= '0;
            seed_base  <= '0;
            read_addr  <= '0;
            start_req  <= 1'b0;
            reset_req  <= 1'b0;
        end else begin
            reset_req <= 1'b0;
            if (avs_chipselect && avs_write) begin
                case (avs_address)
                    REG_BOARD_LOAD: begin
                        // [16] plane (0=black, 1=white), [15:8] cell_idx, [0] bit
                        if (avs_writedata[16]) begin
                            if (avs_writedata[0]) root_white <= root_white | (81'd1 << avs_writedata[14:8]);
                            else                  root_white <= root_white & ~(81'd1 << avs_writedata[14:8]);
                        end else begin
                            if (avs_writedata[0]) root_black <= root_black | (81'd1 << avs_writedata[14:8]);
                            else                  root_black <= root_black & ~(81'd1 << avs_writedata[14:8]);
                        end
                    end
                    REG_KO_LOAD: begin
                        if (avs_writedata[16]) begin
                            if (avs_writedata[0]) ko_w <= ko_w | (81'd1 << avs_writedata[14:8]);
                            else                  ko_w <= ko_w & ~(81'd1 << avs_writedata[14:8]);
                        end else begin
                            if (avs_writedata[0]) ko_b <= ko_b | (81'd1 << avs_writedata[14:8]);
                            else                  ko_b <= ko_b & ~(81'd1 << avs_writedata[14:8]);
                        end
                    end
                    REG_TURN:      root_turn  <= avs_writedata[0];
                    REG_SEED:      seed_base  <= avs_writedata;
                    REG_START:     start_req  <= avs_writedata[0];
                    REG_READ_ADDR: read_addr  <= avs_writedata[6:0];
                    REG_RESET:     reset_req  <= avs_writedata[0];
                    default: ;
                endcase
            end
            // clear start_req once outer FSM picks it up
            if (state == BROADCAST) start_req <= 1'b0;
        end
    end

    // ===== Avalon read side ==============================================
    always_ff @(posedge clk) begin
        if (avs_chipselect && avs_read) begin
            case (avs_address)
                REG_STATUS: avs_readdata <= {cycles_count[31:16],   // [31:16]
                                             4'b0,                  // [15:12]
                                             {4-NUM_ENGINES{1'b0}}, // [11:8] partial
                                             eng_done[NUM_ENGINES-1:0], // crude — see TODO
                                             4'b0,
                                             1'b0,                  // running placeholder
                                             (state == DONE_ST)};
                REG_RESULT: avs_readdata <= {result_visits[read_addr],
                                              result_wins[read_addr]};
                default:    avs_readdata <= 32'h0;
            endcase
        end
    end

    // ===== Outer FSM =====================================================
    logic [6:0] sum_addr;
    logic [31:0] sum_wins_word;
    logic [31:0] sum_visits_word;

    always_ff @(posedge clk or negedge reset_n) begin
        if (!reset_n) begin
            state           <= IDLE;
            broadcast_start <= 1'b0;
            drain_i         <= '0;
            cycles_count    <= '0;
            for (int i = 0; i < 82; i++) begin
                result_wins[i]   <= '0;
                result_visits[i] <= '0;
            end
        end else begin
            broadcast_start <= 1'b0;
            unique case (state)
                IDLE: begin
                    if (start_req) begin
                        cycles_count <= '0;
                        for (int i = 0; i < 82; i++) begin
                            result_wins[i]   <= '0;
                            result_visits[i] <= '0;
                        end
                        state <= BROADCAST;
                    end
                end

                BROADCAST: begin
                    broadcast_start <= 1'b1;
                    state           <= SEED_ST;
                end

                SEED_ST: begin
                    state <= RUN;
                end

                RUN: begin
                    cycles_count <= cycles_count + 32'd1;
                    if (all_done) begin
                        drain_i <= '0;
                        state   <= DRAIN;
                    end
                end

                DRAIN: begin
                    // Sum wins/visits across all engines for cell drain_i.
                    sum_wins_word   = '0;
                    sum_visits_word = '0;
                    for (int g = 0; g < NUM_ENGINES; g++) begin
                        sum_wins_word   = sum_wins_word   + {16'b0, eng_wins[g][drain_i]};
                        sum_visits_word = sum_visits_word + {16'b0, eng_visits[g][drain_i]};
                    end
                    result_wins[drain_i]   <= sum_wins_word[15:0];
                    result_visits[drain_i] <= sum_visits_word[15:0];
                    if (drain_i == 7'd81) state <= REPORT;
                    else                  drain_i <= drain_i + 7'd1;
                end

                REPORT: begin
                    state <= DONE_ST;
                end

                DONE_ST: begin
                    if (start_req) begin
                        // New run requested — back to IDLE so reset path takes effect.
                        state <= IDLE;
                    end
                end

                default: state <= IDLE;
            endcase
        end
    end

endmodule
