// rollout_engine — one playout-to-terminal MCTS rollout engine.
//
// Latches a root board on `start`, then runs SIMS_PER_ENGINE uniform-random
// playouts to terminal positions (two consecutive passes OR move_count cap),
// accumulating per-first-move (wins, visits) into the output arrays.
//
// One flood_fill instance is shared across three modes:
//   1. CAPTURE_FF  — propagate=enemy, liberty=empties, seed=enemy_neighbor
//   2. SUICIDE_FF  — propagate=own,   liberty=empties, seed=placed_cell
//   3. SCORE_FF    — propagate=empties, liberty=stones, seed=first_empty
// The mode is selected by which inputs the surrounding FSM drives.
//
// Move generation: legal_mask = ~(black|white) & ON_BOARD_MASK; LFSR mod
// popcount(legal_mask) selects an index; priority encoder finds the idx-th
// set bit. Up to 5 retries on suicide; PASS otherwise.

import mcts_accel_pkg::*;

module rollout_engine #(
    parameter int ENGINE_ID       = 0,
    parameter int SIMS_PER_ENGINE = 25,
    parameter int MOVE_CAP        = 162
)(
    input  logic         clk,
    input  logic         reset_n,
    input  logic         start,

    input  logic [80:0]  black_in,
    input  logic [80:0]  white_in,
    input  logic         turn_in,        // 0=BLACK to move, 1=WHITE to move
    input  logic [80:0]  ko_prev_b,
    input  logic [80:0]  ko_prev_w,
    input  logic [31:0]  seed,

    output logic         done,
    output logic [15:0]  wins_acc   [0:81],
    output logic [15:0]  visits_acc [0:81]
);

    // ===== State enum =====================================================
    typedef enum logic [4:0] {
        IDLE            = 5'd0,
        PER_SIM_RESET   = 5'd1,
        PICK_RANDOM     = 5'd2,
        ENUM_LEGAL      = 5'd3,
        SELECT_IDX      = 5'd4,
        TRY_PLACE       = 5'd5,
        CAPTURE_FF_INIT = 5'd6,
        CAPTURE_FF_WAIT = 5'd7,
        CAPTURE_APPLY   = 5'd8,
        SUICIDE_FF_INIT = 5'd9,
        SUICIDE_FF_WAIT = 5'd10,
        KO_CHECK        = 5'd11,
        APPLY           = 5'd12,
        TRY_PASS        = 5'd13,
        TERMINATE_CHECK = 5'd14,
        SCORE_INIT      = 5'd15,
        SCORE_FIND      = 5'd16,
        SCORE_FF_INIT   = 5'd17,
        SCORE_FF_WAIT   = 5'd18,
        SCORE_CLASSIFY  = 5'd19,
        ACCUMULATE      = 5'd20,
        DONE_ST         = 5'd21
    } state_t;

    state_t state;

    // ===== Per-rollout working state =====================================
    logic [80:0] black, white;
    logic        turn;                   // 0=BLACK, 1=WHITE
    logic [80:0] ko_b, ko_w;
    logic [15:0] move_count;
    logic [1:0]  consecutive_passes;
    logic [6:0]  sim_first_cell;         // 0..81 (81 = pass)
    logic [15:0] sims_remaining;
    logic [2:0]  suicide_retry;

    // Just-placed stone tracking
    logic [6:0]  placed_idx;
    logic [80:0] placed_bit;
    logic [1:0]  neighbor_idx;           // 0..3 across N/S/W/E for capture loop
    logic [80:0] enemy_before_capture;   // snapshot for ko-prev tracking
    logic [80:0] own_before_capture;

    // ===== flood_fill instance ===========================================
    logic        ff_start;
    logic [80:0] ff_seed;
    logic [80:0] ff_prop;
    logic [80:0] ff_lib;
    logic [80:0] ff_group;
    logic [6:0]  ff_size;
    logic        ff_has_lib;
    logic        ff_done;

    flood_fill u_ff (
        .clk(clk), .reset_n(reset_n),
        .start(ff_start),
        .seed_bit(ff_seed),
        .propagate_mask(ff_prop),
        .liberty_mask(ff_lib),
        .group_mask(ff_group),
        .group_size(ff_size),
        .has_liberty(ff_has_lib),
        .done(ff_done)
    );

    // ===== LFSR ==========================================================
    logic        lfsr_load;
    logic        lfsr_adv;
    logic [15:0] lfsr_q;

    lfsr16 u_lfsr (
        .clk(clk), .reset_n(reset_n),
        .seed_load(lfsr_load),
        .seed_in(seed[15:0] ^ {ENGINE_ID[3:0], 12'h5A5}),
        .advance(lfsr_adv),
        .out(lfsr_q)
    );

    // ===== Legality / move selection =====================================
    logic [80:0] legal_mask;
    logic [80:0] own_mask;
    logic [80:0] enemy_mask;
    logic [80:0] empties_mask;
    assign own_mask     = turn ? white : black;
    assign enemy_mask   = turn ? black : white;
    assign empties_mask = ~(black | white) & ON_BOARD_MASK;
    assign legal_mask   = empties_mask;

    logic [6:0]  legal_count;
    logic [6:0]  pick_idx_mod;
    logic [80:0] pick_bit;
    logic [6:0]  pick_idx;

    // Combinational priority-encoded "select N-th set bit". Synthesizes into
    // an adder tree + comparator chain; pipelining may be needed later if
    // Fmax suffers — see plan Step 11.
    function automatic logic [80:0] nth_set_bit(input logic [80:0] x, input int n);
        int unsigned seen;
        logic [80:0] out;
        seen = 0;
        out = '0;
        for (int i = 0; i < 81; i++) begin
            if (x[i]) begin
                if (seen == n) out[i] = 1'b1;
                seen++;
            end
        end
        return out;
    endfunction

    assign legal_count  = popcount81(legal_mask);
    assign pick_idx_mod = (legal_count == 0) ? 7'd0
                         : (lfsr_q[6:0] % legal_count);
    assign pick_bit     = (legal_count == 0) ? 81'd0
                         : nth_set_bit(legal_mask, int'(pick_idx_mod));

    // Convert one-hot pick_bit to cell index (combinational).
    function automatic logic [6:0] onehot_to_idx(input logic [80:0] x);
        logic [6:0] r;
        r = '0;
        for (int i = 0; i < 81; i++) if (x[i]) r = i[6:0];
        return r;
    endfunction

    // ===== Neighbor bit for capture loop =================================
    function automatic logic [80:0] neighbor_bit(input logic [80:0] center, input int dir);
        case (dir)
            0: return shift_n(center);
            1: return shift_s(center);
            2: return shift_w(center);
            3: return shift_e(center);
            default: return 81'd0;
        endcase
    endfunction

    // ===== Score tracking ================================================
    logic [80:0] score_visited_empty;
    logic        seen_black_border, seen_white_border;
    logic [6:0]  b_territory, w_territory;
    logic [80:0] first_empty_bit;
    logic        winner_black;   // 1 if rollout's winner was BLACK

    // ===== Accumulators (per first-move cell) ============================
    logic [15:0] wins   [0:81];
    logic [15:0] visits [0:81];
    integer      drain_i;

    // ===== Main FSM ======================================================
    always_ff @(posedge clk or negedge reset_n) begin
        if (!reset_n) begin
            state              <= IDLE;
            done               <= 1'b0;
            lfsr_load          <= 1'b0;
            lfsr_adv           <= 1'b0;
            ff_start           <= 1'b0;
            ff_prop            <= '0;
            ff_lib             <= '0;
            ff_seed            <= '0;
            sims_remaining     <= '0;
            for (int i = 0; i < 82; i++) begin
                wins[i]   <= '0;
                visits[i] <= '0;
            end
        end else begin
            lfsr_load <= 1'b0;
            lfsr_adv  <= 1'b0;
            ff_start  <= 1'b0;
            done      <= 1'b0;

            unique case (state)
                IDLE: begin
                    if (start) begin
                        // Latch root + clear accumulators
                        lfsr_load      <= 1'b1;
                        sims_remaining <= SIMS_PER_ENGINE[15:0];
                        for (int i = 0; i < 82; i++) begin
                            wins[i]   <= '0;
                            visits[i] <= '0;
                        end
                        state <= PER_SIM_RESET;
                    end
                end

                PER_SIM_RESET: begin
                    black              <= black_in;
                    white              <= white_in;
                    turn               <= turn_in;
                    ko_b               <= ko_prev_b;
                    ko_w               <= ko_prev_w;
                    move_count         <= '0;
                    consecutive_passes <= 2'd0;
                    sim_first_cell     <= 7'd81;   // default = pass if rollout
                                                    // makes 0 moves before terminal
                    suicide_retry      <= '0;
                    state              <= PICK_RANDOM;
                end

                PICK_RANDOM: begin
                    lfsr_adv <= 1'b1;
                    if (legal_count == 0) begin
                        state <= TRY_PASS;
                    end else begin
                        placed_bit <= pick_bit;
                        placed_idx <= onehot_to_idx(pick_bit);
                        state      <= TRY_PLACE;
                    end
                end

                TRY_PLACE: begin
                    // Speculative place into own_mask. Then we'll run capture
                    // flood-fill on each enemy neighbor.
                    if (turn) white <= white | placed_bit;
                    else      black <= black | placed_bit;
                    enemy_before_capture <= turn ? black : white;
                    own_before_capture   <= turn ? white : black;
                    neighbor_idx         <= 2'd0;
                    state                <= CAPTURE_FF_INIT;
                end

                CAPTURE_FF_INIT: begin
                    // Set up flood_fill in capture mode on neighbor[neighbor_idx]
                    // if that neighbor is enemy.
                    if (|(neighbor_bit(placed_bit, int'(neighbor_idx))
                          & enemy_mask)) begin
                        ff_seed <= neighbor_bit(placed_bit, int'(neighbor_idx))
                                   & enemy_mask;
                        ff_prop <= enemy_mask;
                        ff_lib  <= empties_mask & ~placed_bit;  // exclude our stone
                        ff_start <= 1'b1;
                        state   <= CAPTURE_FF_WAIT;
                    end else begin
                        // No enemy at this neighbor — skip
                        if (neighbor_idx == 2'd3) state <= SUICIDE_FF_INIT;
                        else begin
                            neighbor_idx <= neighbor_idx + 2'd1;
                            state        <= CAPTURE_FF_INIT;
                        end
                    end
                end

                CAPTURE_FF_WAIT: begin
                    if (ff_done) begin
                        if (!ff_has_lib) begin
                            // Remove the captured group from the enemy bitboard.
                            if (turn) black <= black & ~ff_group;
                            else      white <= white & ~ff_group;
                        end
                        if (neighbor_idx == 2'd3) state <= SUICIDE_FF_INIT;
                        else begin
                            neighbor_idx <= neighbor_idx + 2'd1;
                            state        <= CAPTURE_FF_INIT;
                        end
                    end
                end

                SUICIDE_FF_INIT: begin
                    ff_seed  <= placed_bit;
                    ff_prop  <= own_mask;          // updated own_mask (post-place)
                    ff_lib   <= empties_mask;
                    ff_start <= 1'b1;
                    state    <= SUICIDE_FF_WAIT;
                end

                SUICIDE_FF_WAIT: begin
                    if (ff_done) begin
                        if (!ff_has_lib) begin
                            // Suicide — revert and retry. Restore bitboards
                            // from snapshots taken in TRY_PLACE.
                            black <= turn ? enemy_before_capture : own_before_capture;
                            white <= turn ? own_before_capture   : enemy_before_capture;
                            if (suicide_retry == 3'd4) begin
                                state <= TRY_PASS;
                            end else begin
                                suicide_retry <= suicide_retry + 3'd1;
                                state         <= PICK_RANDOM;
                            end
                        end else begin
                            state <= KO_CHECK;
                        end
                    end
                end

                KO_CHECK: begin
                    if (black == ko_b && white == ko_w) begin
                        // Revert ko violation, treat like suicide retry.
                        black <= turn ? enemy_before_capture : own_before_capture;
                        white <= turn ? own_before_capture   : enemy_before_capture;
                        if (suicide_retry == 3'd4) state <= TRY_PASS;
                        else begin
                            suicide_retry <= suicide_retry + 3'd1;
                            state         <= PICK_RANDOM;
                        end
                    end else begin
                        state <= APPLY;
                    end
                end

                APPLY: begin
                    // Record first move of the rollout for accumulator credit.
                    if (move_count == 16'd0) sim_first_cell <= placed_idx;
                    ko_b               <= turn ? enemy_before_capture : own_before_capture;
                    ko_w               <= turn ? own_before_capture   : enemy_before_capture;
                    turn               <= ~turn;
                    consecutive_passes <= 2'd0;
                    move_count         <= move_count + 16'd1;
                    suicide_retry      <= 3'd0;
                    state              <= TERMINATE_CHECK;
                end

                TRY_PASS: begin
                    if (move_count == 16'd0) sim_first_cell <= 7'd81;
                    consecutive_passes <= consecutive_passes + 2'd1;
                    turn               <= ~turn;
                    move_count         <= move_count + 16'd1;
                    state              <= TERMINATE_CHECK;
                end

                TERMINATE_CHECK: begin
                    if (consecutive_passes >= 2'd2 || move_count >= MOVE_CAP[15:0])
                        state <= SCORE_INIT;
                    else
                        state <= PICK_RANDOM;
                end

                SCORE_INIT: begin
                    b_territory       <= '0;
                    w_territory       <= '0;
                    score_visited_empty <= '0;
                    state             <= SCORE_FIND;
                end

                SCORE_FIND: begin
                    // Pick the first unvisited empty bit. If none, classify
                    // final score and accumulate.
                    if ((empties_mask & ~score_visited_empty) == '0) begin
                        // (popcount(black) + b_territory) vs (popcount(white) + w_territory + KOMI_INT)
                        // White wins ties (5.5 komi).
                        if (({3'b0, popcount81(black)} + {3'b0, b_territory})
                          > ({3'b0, popcount81(white)} + {3'b0, w_territory}
                            + 10'(KOMI_INT))) begin
                            winner_black <= 1'b1;
                        end else begin
                            winner_black <= 1'b0;
                        end
                        state <= ACCUMULATE;
                    end else begin
                        first_empty_bit <= nth_set_bit(
                            empties_mask & ~score_visited_empty, 0);
                        state           <= SCORE_FF_INIT;
                    end
                end

                SCORE_FF_INIT: begin
                    ff_seed  <= first_empty_bit;
                    ff_prop  <= empties_mask;
                    ff_lib   <= black | white;
                    ff_start <= 1'b1;
                    state    <= SCORE_FF_WAIT;
                end

                SCORE_FF_WAIT: begin
                    if (ff_done) begin
                        // Distinguish black-bordering from white-bordering by
                        // re-running flood_fill with one stone-color liberty
                        // at a time. Simpler form here: probe the group's
                        // boundary directly via combinational shift logic.
                        seen_black_border <= |(
                            (shift_n(ff_group) | shift_s(ff_group)
                           | shift_w(ff_group) | shift_e(ff_group))
                            & ON_BOARD_MASK & black);
                        seen_white_border <= |(
                            (shift_n(ff_group) | shift_s(ff_group)
                           | shift_w(ff_group) | shift_e(ff_group))
                            & ON_BOARD_MASK & white);
                        score_visited_empty <= score_visited_empty | ff_group;
                        state               <= SCORE_CLASSIFY;
                    end
                end

                SCORE_CLASSIFY: begin
                    if      ( seen_black_border && !seen_white_border)
                        b_territory <= b_territory + ff_size;
                    else if (!seen_black_border &&  seen_white_border)
                        w_territory <= w_territory + ff_size;
                    // mixed or empty-only → neutral
                    state <= SCORE_FIND;
                end

                ACCUMULATE: begin
                    // Credit (sim_first_cell, wins/visits). wins counts a win
                    // for the root mover (turn_in side).
                    visits[sim_first_cell] <= visits[sim_first_cell] + 16'd1;
                    if ((winner_black && (turn_in == 1'b0))
                     || (!winner_black && (turn_in == 1'b1))) begin
                        wins[sim_first_cell] <= wins[sim_first_cell] + 16'd1;
                    end
                    if (sims_remaining == 16'd1) state <= DONE_ST;
                    else begin
                        sims_remaining <= sims_remaining - 16'd1;
                        state          <= PER_SIM_RESET;
                    end
                end

                DONE_ST: begin
                    done <= 1'b1;
                    if (!start) state <= IDLE;
                end

                default: state <= IDLE;
            endcase
        end
    end

    // Drive the output ports directly from internal accumulators.
    always_comb begin
        for (int i = 0; i < 82; i++) begin
            wins_acc[i]   = wins[i];
            visits_acc[i] = visits[i];
        end
    end

endmodule
