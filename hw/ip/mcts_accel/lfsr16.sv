// lfsr16 — 16-bit Fibonacci LFSR, taps {16, 15, 13, 4}. Period 65535.
//
// `seed_load` overrides the state on a rising clock with `seed_in` (skip on 0
// to avoid the lock-up state). `advance` shifts the LFSR one position per
// cycle. `out` is the current state — combinational, no extra latency.

module lfsr16 (
    input  logic        clk,
    input  logic        reset_n,
    input  logic        seed_load,
    input  logic [15:0] seed_in,
    input  logic        advance,
    output logic [15:0] out
);

    logic [15:0] state;
    logic        fb;

    assign fb  = state[15] ^ state[14] ^ state[12] ^ state[3];
    assign out = state;

    always_ff @(posedge clk or negedge reset_n) begin
        if (!reset_n) begin
            state <= 16'hACE1;
        end else if (seed_load) begin
            state <= (seed_in == 16'h0) ? 16'hACE1 : seed_in;
        end else if (advance) begin
            state <= {state[14:0], fb};
        end
    end

endmodule
