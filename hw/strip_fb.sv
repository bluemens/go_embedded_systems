/*
 * strip_fb — 640×60 8bpp double-buffered framebuffer
 *
 * Holds the score panel / menu / game-over text region at the bottom of
 * the screen (y ∈ [420, 479]).
 *
 * Two banks, each 9,600 × 32-bit words = 307,200 bits ≈ 30 M10K blocks.
 * Each 32-bit word holds 4 packed 8-bit pixels (little-endian: bits [7:0]
 * are the leftmost pixel of the four).
 *
 * Write side (Avalon): the HPS writes to the BACK buffer. Software flushes
 * the entire 38,400-byte buffer in a tight loop (~770 µs at 50 MHz LW),
 * then writes STRIP_SWAP to arm a swap. The swap takes effect on the next
 * rising edge of vsync_pulse, so the visible buffer never tears.
 *
 * Read side (VGA): for every pixel inside the strip region, the active
 * (front) buffer is indexed and the corresponding 8-bit pixel returned.
 * BRAM read has 1-cycle latency, which the consumer must account for.
 *
 * Pixel address layout: pixel_addr = (py - 420) * 640 + px, range 0..38399.
 *   word_idx = pixel_addr[13:2]
 *   byte_idx = pixel_addr[1:0]
 *
 * Reference: design-document.md §4.3 (strip framebuffer).
 */

module strip_fb (
    input  logic        clk,
    input  logic        reset,

    // Avalon-write side (back buffer)
    input  logic        write_en,
    input  logic [13:0] write_addr,        // 0..9599 word index
    input  logic [31:0] write_data,
    input  logic        swap_request,      // 1-cycle pulse from STRIP_SWAP
    input  logic        vsync_pulse,       // 1-cycle pulse at start of vblank

    // VGA-read side (front buffer)
    input  logic [15:0] pixel_addr,        // 0..38399 pixel index (needs 16 bits)
    output logic [7:0]  pixel_out
);

    /* Two banks. We keep them as separate arrays so Quartus infers two
     * dual-port BRAMs (write port + read port each) — much easier to time
     * than a single shared BRAM with mux'd ports. */
    logic [31:0] buf_a [0:9599];
    logic [31:0] buf_b [0:9599];

    logic active;        // 0 = buf_a is FRONT (read), 1 = buf_b is FRONT
    logic swap_armed;

    /* Write port: writes go to whichever bank is currently BACK. */
    wire write_to_a = write_en && (active == 1'b1);
    wire write_to_b = write_en && (active == 1'b0);

    always_ff @(posedge clk) begin
        if (write_to_a) buf_a[write_addr] <= write_data;
    end
    always_ff @(posedge clk) begin
        if (write_to_b) buf_b[write_addr] <= write_data;
    end

    /* Read port: synchronous reads from each bank. */
    logic [13:0] read_word_idx;
    logic [1:0]  read_byte_idx;
    assign read_word_idx = pixel_addr[15:2];   // top 14 bits → word index (max 9599)
    assign read_byte_idx = pixel_addr[1:0];

    logic [31:0] read_word_a, read_word_b;
    logic [1:0]  byte_idx_d;
    logic        active_d;

    always_ff @(posedge clk) begin
        read_word_a <= buf_a[read_word_idx];
        read_word_b <= buf_b[read_word_idx];
        byte_idx_d  <= read_byte_idx;
        active_d    <= active;
    end

    logic [31:0] front_word;
    assign front_word = (active_d == 1'b0) ? read_word_a : read_word_b;

    /* Pixel select within word — combinational from the registered word. */
    always_comb begin
        unique case (byte_idx_d)
            2'd0: pixel_out = front_word[7:0];
            2'd1: pixel_out = front_word[15:8];
            2'd2: pixel_out = front_word[23:16];
            2'd3: pixel_out = front_word[31:24];
        endcase
    end

    /* Swap state machine: arm on swap_request, fire on vsync_pulse. */
    always_ff @(posedge clk) begin
        if (reset) begin
            active     <= 1'b0;
            swap_armed <= 1'b0;
        end else begin
            if (swap_request)
                swap_armed <= 1'b1;
            if (swap_armed && vsync_pulse) begin
                active     <= ~active;
                swap_armed <= 1'b0;
            end
        end
    end

endmodule
