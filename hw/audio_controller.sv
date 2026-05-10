/*
 * audio_controller — sample-ROM playback for game sound effects
 *
 * Reads 16-bit signed PCM samples at 8 kHz from one of four ROM banks and
 * outputs them on `dac_left` / `dac_right` whenever `advance` (from the
 * Altera-UP codec_interface, ~48 kHz) pulses. Each ROM sample is held for
 * 6 advance pulses (8 kHz × 6 = 48 kHz) — a zero-order-hold upsample with
 * no anti-aliasing. Audio quality is more than fine for short SFX (clicks,
 * captures, game-over chime).
 *
 * Sound banks (one-hot via the `sound_id` 3-bit code, 0=idle):
 *   1 = place        (0.20 s = 1600 samples = 25,600 bits ≈ 3 M10K)
 *   2 = capture      (0.30 s = 2400 samples ≈ 4 M10K)
 *   3 = illegal      (0.15 s = 1200 samples ≈ 2 M10K)
 *   4 = game_over    (1.50 s = 12000 samples ≈ 19 M10K)
 *
 * Sample data lives in `*.vh` files generated from .wav via the
 * Autotune-style python pipeline described in design-document.md §5.2.
 * For Phase 6 these files contain placeholder data (silence + 1-sample
 * click at start) — replace with real audio in Phase 6b.
 *
 * Avalon control:
 *   - Software writes AUDIO_CMD with values 1..4 to start playback. Writes
 *     while busy are ignored.
 *   - AUDIO_STATUS[0] is asserted while playing.
 *
 * Reference: design-document.md §5.3 (audio FSM); references/Pac-Man/
 * source/PacMan/hardware/audio.vh (similar $readmemh ROM pattern).
 */

module audio_controller (
    input  logic        clk,
    input  logic        reset,

    // Command interface (from Avalon register decoder)
    input  logic [2:0]  audio_cmd,        // 0=idle, 1..4 = sound id
    input  logic        audio_cmd_valid,  // 1-cycle pulse on new write
    output logic        busy,

    // codec_interface signals
    input  logic        advance,          // ~48 kHz strobe from codec_interface
    output logic [23:0] dac_left,
    output logic [23:0] dac_right
);

    // ─── ROMs (placeholder; populated by $readmemh) ─────────────────────────
    localparam int LEN_PLACE    = 1600;
    localparam int LEN_CAPTURE  = 2400;
    localparam int LEN_ILLEGAL  = 1200;
    localparam int LEN_GAMEOVER = 12000;

    logic signed [15:0] rom_place    [0:LEN_PLACE   -1];
    logic signed [15:0] rom_capture  [0:LEN_CAPTURE -1];
    logic signed [15:0] rom_illegal  [0:LEN_ILLEGAL -1];
    logic signed [15:0] rom_gameover [0:LEN_GAMEOVER-1];

    initial begin
        $readmemh("place.vh",    rom_place);
        $readmemh("capture.vh",  rom_capture);
        $readmemh("illegal.vh",  rom_illegal);
        $readmemh("gameover.vh", rom_gameover);
    end

    // ─── Playback state ─────────────────────────────────────────────────────
    typedef enum logic [0:0] { S_IDLE, S_PLAY } state_t;
    state_t      state;
    logic [2:0]  cur_sound;
    logic [13:0] sample_idx;       // up to 16383 — covers gameover (12000)
    logic [2:0]  sub_count;        // 0..5: hold each ROM sample for 6 advances

    logic [13:0] cur_length;
    always_comb begin
        unique case (cur_sound)
            3'd1:    cur_length = LEN_PLACE   [13:0];
            3'd2:    cur_length = LEN_CAPTURE [13:0];
            3'd3:    cur_length = LEN_ILLEGAL [13:0];
            3'd4:    cur_length = LEN_GAMEOVER[13:0];
            default: cur_length = 14'd0;
        endcase
    end

    logic signed [15:0] cur_sample;
    always_comb begin
        unique case (cur_sound)
            3'd1:    cur_sample = rom_place   [sample_idx[10:0]];   // <2048
            3'd2:    cur_sample = rom_capture [sample_idx[11:0]];   // <4096
            3'd3:    cur_sample = rom_illegal [sample_idx[10:0]];
            3'd4:    cur_sample = rom_gameover[sample_idx];
            default: cur_sample = 16'sd0;
        endcase
    end

    assign busy = (state != S_IDLE);

    // 16-bit signed → 24-bit signed (sign-extend)
    wire signed [23:0] cur24 = {{8{cur_sample[15]}}, cur_sample};
    assign dac_left  = (state == S_PLAY) ? cur24 : 24'sd0;
    assign dac_right = (state == S_PLAY) ? cur24 : 24'sd0;

    always_ff @(posedge clk) begin
        if (reset) begin
            state      <= S_IDLE;
            cur_sound  <= 3'd0;
            sample_idx <= 14'd0;
            sub_count  <= 3'd0;
        end else begin
            unique case (state)
                S_IDLE: begin
                    if (audio_cmd_valid && audio_cmd != 3'd0
                                        && audio_cmd <= 3'd4) begin
                        cur_sound  <= audio_cmd;
                        sample_idx <= 14'd0;
                        sub_count  <= 3'd0;
                        state      <= S_PLAY;
                    end
                end
                S_PLAY: begin
                    /* New audio_cmd while busy: ignored. */
                    if (advance) begin
                        if (sub_count == 3'd5) begin
                            sub_count <= 3'd0;
                            if (sample_idx + 14'd1 >= cur_length) begin
                                state <= S_IDLE;
                            end else begin
                                sample_idx <= sample_idx + 14'd1;
                            end
                        end else begin
                            sub_count <= sub_count + 3'd1;
                        end
                    end
                end
            endcase
        end
    end

endmodule
