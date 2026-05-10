/*
 * Avalon memory-mapped peripheral that generates VGA
 */

module gpu #(
    parameter int DATA_WIDTH = 32,
    parameter int ADDR_WIDTH = 14
) (
    input logic clk,
    input logic reset,
    input chipselect,
    input logic [DATA_WIDTH-1:0] writedata,  // Write at most 32 bits at once
    input logic read,
    input logic write,
    input logic [ADDR_WIDTH-1:0] address,
    input logic [3:0] byteenable,

    output logic [DATA_WIDTH-1:0] readdata,

    output logic [7:0] VGA_R,
    VGA_G,
    VGA_B,
    output logic VGA_CLK,
    VGA_HS,
    VGA_VS,
    VGA_BLANK_N,
    VGA_SYNC_N
);

  /* VGA Controller setup */
  // Input from the VGA counter
  logic [9:0] h_count, v_count;
  // The actual location on the screen
  // logic [8:0] screen_x, screen_y;

  // next_next during attribute fetch, next during texture fetch
  // logic next_next_hs, next_hs;
  // logic next_next_vs, next_vs;
  // logic next_next_blank_n, next_blank_n;

  // For both, divide by 2 (VGA is 640x480, our resolution is 320x240)
  // assign screen_x = h_count[9:1];
  // assign screen_y = v_count[9:1];

  localparam logic [9:0] HACTIVE      = 10'd640,
                         HFRONT_PORCH = 10'd16,
                         HSYNC        = 10'd96,
                         HBACK_PORCH  = 10'd48,
                         VACTIVE      = 10'd480,
                         VFRONT_PORCH = 10'd10,
                         VSYNC        = 10'd2,
                         VBACK_PORCH  = 10'd33;

  // localparam logic [9:0] HACTIVE      = 10'd1,
  //                        HFRONT_PORCH = 10'd1,
  //                        HSYNC        = 10'd1,
  //                        HBACK_PORCH  = 10'd1,
  //                        VACTIVE      = 10'd1,
  //                        VFRONT_PORCH = 10'd1,
  //                        VSYNC        = 10'd1,
  //                        VBACK_PORCH  = 10'd1;
  // localparam int SCREEN_WIDTH = HACTIVE >> 1, SCREEN_HEIGHT = VACTIVE >> 1;

  localparam logic [9:0] HTOTAL = HACTIVE + HFRONT_PORCH + HSYNC + HBACK_PORCH,
                         VTOTAL = VACTIVE + VFRONT_PORCH + VSYNC + VBACK_PORCH;

  // force_blanking should be in registers, but it makes more sense to keep it
  // here...
  logic blank_n, force_blanking;

  vga_counter #(
      .HACTIVE(HACTIVE),
      .HFRONT_PORCH(HFRONT_PORCH),
      .HSYNC(HSYNC),
      .HBACK_PORCH(HBACK_PORCH),
      .VACTIVE(VACTIVE),
      .VFRONT_PORCH(VFRONT_PORCH),
      .VSYNC(VSYNC),
      .VBACK_PORCH(VBACK_PORCH)
  ) counter (
      .clk50  (clk),
      .reset  (reset),
      .h_count(h_count),
      .v_count(v_count),
      .vga_clk(VGA_CLK),
      .h_sync (VGA_HS),
      .v_sync (VGA_VS),
      .blank_n(blank_n)
  );

  // TODO: Should we use this in the future?
  assign VGA_SYNC_N  = 1'b0;
  assign VGA_BLANK_N = blank_n & ~force_blanking;

  /* VRAM */

  logic [31:0] vram_dout;

  // FIXME uncomment
  logic [12:0] tile_texture_addr, sprite_texture_addr, texture_addr;  // 32 KiB == 13+2 bit address
  logic [31:0] texture_data;

  logic [12:0] tile_addr;  // 16 KiB == 12+2 bit address
  logic [15:0] tile_data;

  logic [ 7:0] sprite_addr;  // 1 KiB == 8+2 bit address
  logic [31:0] sprite_data;

  logic [6:0]
      fg_palette_addr,
      bg_palette_addr,
      sprite_palette_addr,
      palette_addr;  // 512 B == 7+2 bit address
  logic [ 3:0] palette_off;
  logic [31:0] palette_data;

  logic [7:0] pixel_r, pixel_g, pixel_b;

  /* verilator lint_off UNUSEDSIGNAL */
  logic [7:0] pixel_a;
  /* verilator lint_on UNUSEDSIGNAL */

  // always_comb begin
  //   if (|sprite_palette_addr)
  //     $write("[%0d][%0d] sprite_palette_addr = [%04X]\n", h_count, v_count, tile_texture_addr);
  //   if (|tile_palette_addr)
  //     $write("[%0d][%0d] tile_palette_addr = [%04X]\n", h_count, v_count, sprite_texture_addr);
  // end

  // If everything works as intended, they will never both be active at the
  // same time!
  assign texture_addr = tile_texture_addr | sprite_texture_addr;
  // assign texture_addr = (h_count < H_ACTIVE || h_count >= H_TOTAL - 32) ? tile_texture_addr : sprite_texture_addr;
  // assign texture_addr = tile_texture_addr;

  assign palette_off = palette_addr[3:0];
  assign pixel_r = palette_data[31:24];
  assign pixel_g = palette_data[23:16];
  assign pixel_b = palette_data[15:8];
  assign pixel_a = palette_data[7:0];

  vram #(
      .BYTES_PER_WORD(4),
      .DATA_WIDTH(DATA_WIDTH)
  ) mem (
      .clk (clk),
      .addr(address),
      .din (writedata),
      .be  (byteenable),
      .we  (chipselect & write),
      .dout(vram_dout),

      .texture_addr(texture_addr),  // 32 KiB == 13 bits
      .texture_data(texture_data),
      .tile_addr(tile_addr),  // 8 KiB == 11 bits
      .tile_data(tile_data),
      .sprite_addr(sprite_addr),  // 1 KiB == 8 bits
      .sprite_data(sprite_data),
      .palette_addr(palette_addr),  // 512 B == 7 bits
      .palette_data(palette_data)
  );

  /* Registers */

  logic [31:0] reg_dout;

  // Forced blanking (basically, turn the screen off)
  // logic force_blank;

  // Scroll
  logic [8:0] scroll_x, scroll_y;

  // Background color (For now, it's just black)
  logic [23:0] bg_color;
  logic [7:0] bg_r, bg_g, bg_b;

  assign bg_r = bg_color[23:16];
  assign bg_g = bg_color[15:8];
  assign bg_b = bg_color[7:0];

  always_ff @(posedge clk or posedge reset)
    if (reset) begin
      bg_color <= 24'h008000;
      scroll_x <= 0;
      scroll_y <= 0;
      force_blanking <= 1;
      reg_dout <= 32'h0;
    end else begin
      if (chipselect)
        if (write)
          unique case ({
            address, 2'b0
          })
            16'hFF00: begin
              if (byteenable[0]) force_blanking <= writedata[0];
            end
            16'hFF04: begin
              if (byteenable[0]) scroll_x[7:0] <= writedata[7:0];
              if (byteenable[1]) scroll_x[8] <= writedata[8];
              if (byteenable[2]) scroll_y[7:0] <= writedata[23:16];
              if (byteenable[3]) scroll_y[8] <= writedata[24];
            end
            16'hFF08: begin
              if (byteenable[1]) bg_color[23:16] <= writedata[15:8];
              if (byteenable[2]) bg_color[15:8] <= writedata[23:16];
              if (byteenable[3]) bg_color[7:0] <= writedata[31:24];
            end
            default: ;
          endcase
        else if (read)
          unique case ({
            address, 2'b0
          })
            16'hFF0C: begin
              reg_dout[15:0]  <= {6'b0, h_count};
              reg_dout[31:16] <= {6'b0, v_count};
              // $write("[%04X] -> [%08X]\n", {address, 2'b0}, readdata);
            end
            default: ;
          endcase
    end

  /* General logic */

  // These should never interfere
  assign readdata = read ? vram_dout | reg_dout : 0;

  // Used to index into the tile and sprite palette offsets; determines the
  // offset into the shift registers
  // assign fine_scroll_x = scroll_x[2:0];

  /* Tile logic */

  tile_gpu #(
      .HACTIVE(HACTIVE),
      .VACTIVE(VACTIVE),
      .HTOTAL (HTOTAL),
      .VTOTAL (VTOTAL)
  ) tile (
      .clk(clk),
      .vga_clk(VGA_CLK),
      .blank_n(VGA_BLANK_N),
      .reset(reset),
      .h_count(h_count),
      .v_count(v_count),
      .next_scroll_x(scroll_x),  // TODO: Set these for scrolling!
      .next_scroll_y(scroll_y),
      .tile_data(tile_data),
      .texture_data(texture_data),  // Texture data from previous fetch

      .tile_addr(tile_addr),  // Which tile address to fetch
      .texture_addr(tile_texture_addr),  // Which texture sliver to fetch
      .fg_palette_addr(fg_palette_addr),  // Which palette address to use
      .bg_palette_addr(bg_palette_addr)  // Which palette address to use
  );

  /* Sprite logic */

  sprite_gpu #(
      .HACTIVE(HACTIVE),
      .VTOTAL (VTOTAL)
  ) sprite (
      .clk(clk),
      .vga_clk(VGA_CLK),
      .blank_n(VGA_BLANK_N),
      .reset(reset),
      .h_count(h_count),
      .v_count(v_count),
      .sprite_data(sprite_data),
      .texture_data(texture_data),

      .sprite_addr (sprite_addr),
      .texture_addr(sprite_texture_addr),
      .palette_addr(sprite_palette_addr)
  );

  // Compositor
  always_comb begin
    // When blanking, only draw black!
    {VGA_R, VGA_G, VGA_B} = {8'd0, 8'd0, 8'd0};
    palette_addr = 0;

    if (VGA_BLANK_N) begin
      if (|fg_palette_addr) palette_addr = fg_palette_addr;
      else if (|sprite_palette_addr) palette_addr = sprite_palette_addr;
      else palette_addr = bg_palette_addr;

      // If the final palette offset is nonzero, draw the pixel
      // Otherwise, draw the background color
      if (|palette_off) {VGA_R, VGA_G, VGA_B} = {pixel_r, pixel_g, pixel_b};
      else {VGA_R, VGA_G, VGA_B} = {bg_r, bg_g, bg_b};
    end

    // if (VGA_BLANK_N && VGA_CLK && h_count == 0)
    //   $write("[%0d][%0d] drawing [%02X%02X%02X]\n", h_count, v_count, VGA_R, VGA_G, VGA_B);
  end
endmodule
