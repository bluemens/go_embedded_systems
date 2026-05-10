module vga_counter #(

    parameter logic [9:0] HACTIVE      = 10'd640,
    parameter logic [9:0] HFRONT_PORCH = 10'd16,
    parameter logic [9:0] HSYNC        = 10'd96,
    parameter logic [9:0] HBACK_PORCH  = 10'd48,
    parameter logic [9:0] VACTIVE      = 10'd480,
    parameter logic [9:0] VFRONT_PORCH = 10'd10,
    parameter logic [9:0] VSYNC        = 10'd2,
    parameter logic [9:0] VBACK_PORCH  = 10'd33
) (
    input clk50,
    reset,
    output wire [9:0] h_count,  // Pixel column
    output logic [9:0] v_count,  // Pixel row
    output logic vga_clk,
    h_sync,  // 0 on sync, 1 otherwise
    v_sync,
    blank_n  // High when not blanking
);

  localparam logic [9:0] HTOTAL = HACTIVE + HFRONT_PORCH + HSYNC + HBACK_PORCH;  // 800
  localparam logic [9:0] VTOTAL = VACTIVE + VFRONT_PORCH + VSYNC + VBACK_PORCH;  // 525

  // Parameters for hcount
  // parameter logic [9:0] HACTIVE      = 10'd 640,
  //                       HFRONT_PORCH = 10'd 16,
  //                       HSYNC        = 10'd 96,
  //                       HBACK_PORCH  = 10'd 48,
  //                       HTOTAL       = HACTIVE + HFRONT_PORCH + HSYNC + HBACK_PORCH; // 800
  // parameter logic [10:0] HACTIVE      = 11'd1280,
  //                      HFRONT_PORCH = 11'd32,
  //                      HSYNC        = 11'd192,
  //                      HBACK_PORCH  = 11'd96,
  //                      HTOTAL       = HACTIVE + HFRONT_PORCH + HSYNC + HBACK_PORCH,  // 1600

  logic [10:0] h_count_raw;

  /*
  * 640 X 480 VGA timing for a 50 MHz clock: one pixel every other cycle
  *
  * HCOUNT 1599 0             1279       1599 0
  *             _______________              ________
  * ___________|    Video      |____________|  Video
  *
  *
  * |SYNC| BP |<-- HACTIVE -->|FP|SYNC| BP |<-- HACTIVE
  *       _______________________      _____________
  * |____|       VGA_HS          |____|
  */

  /* vga_clk is 25 mhz
    *             __    __    __
    * clk50    __|  |__|  |__|
    *
    *             _____       __
    * hcount[0]__|     |_____|
    */
  assign vga_clk = h_count_raw[0];
  assign h_count = h_count_raw[10:1];

  always_ff @(posedge clk50 or posedge reset) begin
    // if (vga_clk) begin
    // end
    if (reset) begin
      h_count_raw <= 11'd0;
      v_count <= 10'd0;
    end else if (vga_clk && h_count == HTOTAL - 1) begin
      h_count_raw <= 11'd0;
      v_count <= (v_count == VTOTAL - 1) ? 10'd0 : v_count + 10'd1;
    end else h_count_raw <= h_count_raw + 11'd1;
  end

  // Hopefully this is abstracted away...
  assign h_sync = (h_count < HACTIVE + HFRONT_PORCH) || (h_count >= HACTIVE + HFRONT_PORCH + HSYNC);
  assign v_sync = (v_count < VACTIVE + VFRONT_PORCH) || (v_count >= VACTIVE + VFRONT_PORCH + VSYNC);
  assign blank_n = (h_count < HACTIVE) && (v_count < VACTIVE);

  // logic endOfLine;
  //
  // always_ff @(posedge clk50 or posedge reset)
  //   if (reset) hcount <= 0;
  //   else if (endOfLine) hcount <= 0;
  //   else hcount <= hcount + 11'd1;
  //
  // assign endOfLine = hcount == HTOTAL - 1;
  //
  // logic endOfField;
  //
  // always_ff @(posedge clk50 or posedge reset)
  //   if (reset) vcount <= 0;
  //   else if (endOfLine)
  //     if (endOfField) vcount <= 0;
  //     else vcount <= vcount + 10'd1;
  //
  // assign endOfField = vcount == VTOTAL - 1;

  // Horizontal sync: from 0x520 to 0x5DF (0x57F)
  // 101 0010 0000 to 101 1101 1111
  // assign VGA_HS = !((hcount[10:8] == 3'b101) & !(hcount[7:5] == 3'b111));
  // assign VGA_HS = (hcount[10:8] != 3'b101) | (hcount[7:5] == 3'b111);
  // assign VGA_HS =
  // assign VGA_VS = !(vcount[9:1] == (VACTIVE + VFRONT_PORCH) / 2);
  // assign VGA_VS = vcount[9:1] != ((VACTIVE + VFRONT_PORCH) >> 1);

  // assign VGA_SYNC_n  = 1'b0;  // For putting sync on the green signal; unused
  // assign vsync = 1'b0;
  // assign hsync = 1'b0;

  // Horizontal active: 0 to 1279     Vertical active: 0 to 479
  // 101 0000 0000  1280              01 1110 0000  480
  // 110 0011 1111  1599              10 0000 1100  524
  // assign VGA_BLANK_n = !( hcount[10] & (hcount[9] | hcount[8]) ) &
  //   !( vcount[9] | (vcount[8:5] == 4'b1111) );
  // assign VGA_BLANK_n = !((hcount[10] & |hcount[9:8])) |
  //                       (vcount[9] | (vcount[8:5] == 4'b1111)));
  // assign h_blank =

  /* vga_clk is 25 mhz
    *             __    __    __
    * clk50    __|  |__|  |__|
    *
    *             _____       __
    * hcount[0]__|     |_____|
    */
  // assign VGA_CLK = h_count[0];  // 25 MHz clock: rising edge sensitive

endmodule
