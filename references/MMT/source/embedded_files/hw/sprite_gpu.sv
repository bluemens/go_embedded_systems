// While !HACTIVE, we have ~340 cycles to load the data of all sprites. This
// can be done through pipelining: when loading sprite data for sprite 1,
// fetch texture data for sprite 0. Doing so, we only need 258 cycles to
// buffer all sprites for the next line!
//
// Cycle 1280: clear line buffer data; set it all to 0x0
//   It's a good thing we can use 0, otherwise I'm guessing this would suck
//
// Cycle 1280-1537: load all 256 (!) sprites into buffer
//
// 0x500  | 0x501 | 0x502 | ... | 0x5FF    | 0x600   | 0x601
// data 0 | data 1| data 2|     | data 255 |         |
//        | tex. 0| tex. 1|     | tex. 254 | tex. 255|
//        |       | draw 0|     | draw 253 | draw 254| draw 255

module sprite_gpu #(
    parameter int SPRITE_ADDR_WIDTH  = 8,
    parameter int TEXTURE_ADDR_WIDTH = 13,
    parameter int PALETTE_ADDR_WIDTH = 7,
    parameter int SPRITE_DATA_WIDTH  = 32,
    parameter int TEXTURE_DATA_WIDTH = 32,
    parameter int VGA_COUNTER_WIDTH  = 10,

    parameter logic [VGA_COUNTER_WIDTH-1:0] HACTIVE,
    parameter logic [VGA_COUNTER_WIDTH-1:0] VTOTAL
) (
    input clk,
    vga_clk,
    blank_n,
    reset,
    input [VGA_COUNTER_WIDTH-1:0] h_count,
    v_count,
    input [SPRITE_DATA_WIDTH-1:0] sprite_data,  // Sprite data from previous fetch
    input [TEXTURE_DATA_WIDTH-1:0] texture_data,  // Texture data from previous fetch

    output logic [ SPRITE_ADDR_WIDTH-1:0] sprite_addr,   // Which sprite address to fetch
    output logic [TEXTURE_ADDR_WIDTH-1:0] texture_addr,  // Which texture sliver to fetch
    output logic [PALETTE_ADDR_WIDTH-1:0] palette_addr   // Which palette address to use
);


  localparam logic [VGA_COUNTER_WIDTH:0] NUM_SPRITES = 1 << SPRITE_ADDR_WIDTH;

  // Timing intervals
  localparam logic [VGA_COUNTER_WIDTH:0] CLEAR = {HACTIVE, 1'b0};
  localparam logic [VGA_COUNTER_WIDTH:0] FETCH_ATTR_BEGIN = CLEAR;
  localparam logic [VGA_COUNTER_WIDTH:0] FETCH_ATTR_END = FETCH_ATTR_BEGIN + NUM_SPRITES;
  localparam logic [VGA_COUNTER_WIDTH:0] FETCH_TEXTURE_BEGIN = FETCH_ATTR_BEGIN + 1;
  localparam logic [VGA_COUNTER_WIDTH:0] FETCH_TEXTURE_END = FETCH_TEXTURE_BEGIN + NUM_SPRITES;
  localparam logic [VGA_COUNTER_WIDTH:0] DRAW_SLIVER_BEGIN = FETCH_TEXTURE_BEGIN + 1;
  localparam logic [VGA_COUNTER_WIDTH:0] DRAW_SLIVER_END = DRAW_SLIVER_BEGIN + NUM_SPRITES;

  // So we can take full advantage of the 50MHz clock
  logic [VGA_COUNTER_WIDTH:0] h_count_raw;
  assign h_count_raw = {h_count, vga_clk};

  logic [VGA_COUNTER_WIDTH-2:0] screen_x;
  logic [VGA_COUNTER_WIDTH-3:0] screen_y;
  assign screen_x = h_count[VGA_COUNTER_WIDTH-1:1];
  // Topmost bit is never used, divide v_count by 2
  assign screen_y = v_count == (VTOTAL - 1) ? 0 : v_count[VGA_COUNTER_WIDTH-2:1] + 1;

  // Reading is simple enough
  logic unused;
  logic [2:0] palette_ind;
  logic [3:0] palette_offset;

  // Data related to the sprites
  logic [8:0] next_sprite_x, sprite_x;
  logic [7:0] next_sprite_y, sprite_y;
  logic [9:0] sprite_texture_ind;
  logic [3:0] next_sprite_palette_ind, sprite_palette_ind;
  assign next_sprite_x = sprite_data[8:0];
  assign next_sprite_y = sprite_data[16:9];
  assign sprite_texture_ind = sprite_data[26:17];
  assign next_sprite_palette_ind = {1'b0, sprite_data[29:27]};

  // TODO: Add flipping
  /* verilator lint_off UNUSEDSIGNAL */
  logic next_sprite_flip_v, next_sprite_flip_h;
  assign next_sprite_flip_v = sprite_data[31];
  assign next_sprite_flip_h = sprite_data[30];

  // Difference in sprite's position and screen's position
  logic [7:0] sprite_y_offset;

  /* verilator lint_on UNUSEDSIGNAL */
  assign sprite_y_offset = screen_y - next_sprite_y;

  // When should we do each thing
  logic update_line_buffers, clear, fetch_attr, fetch_texture, draw_sliver;
  // Since each pixel is 2x2 pixels displayed, only perform actions on odd
  // scanlines
  assign update_line_buffers = ((!blank_n && v_count[0]) || v_count == VTOTAL - 1)
                  && h_count_raw >= CLEAR && h_count_raw < DRAW_SLIVER_END;

  assign clear = reset | (update_line_buffers && h_count_raw == CLEAR);
  assign fetch_attr = h_count_raw >= FETCH_ATTR_BEGIN && h_count_raw < FETCH_ATTR_END;
  assign fetch_texture = h_count_raw >= FETCH_TEXTURE_BEGIN && h_count_raw < FETCH_TEXTURE_END;
  // Not only do we need to be at the right time, the current sprite must also
  // be visible somewhere
  assign draw_sliver = update_line_buffers && h_count_raw >= DRAW_SLIVER_BEGIN
                       && h_count_raw < DRAW_SLIVER_END
                       && screen_y >= sprite_y
                       && screen_y < sprite_y + 8;

  // Store the indicies and offsets separately to make loading easier
  linebuffer #(
      .DATA_WIDTH_R(4)
  ) palette_inds_buffer (
      .clk(clk),
      .we(draw_sliver),
      .clear(clear),
      .addr_r(screen_x),
      .addr_w(sprite_x),
      .data_r({unused, palette_ind}),
      // TODO: Rewrite this later to be less stupid
      .data_w({8{sprite_palette_ind}})

  );

  linebuffer #(
      .DATA_WIDTH_R(4)
  ) palette_offs_buffer (
      .clk(clk),
      .we(draw_sliver),
      .clear(clear),
      .addr_r(screen_x),
      .addr_w(sprite_x),
      .data_r(palette_offset),
      .data_w(texture_data)
  );

  always_ff @(posedge clk) begin
    // Pipe these all to so that they're visible on the next
    sprite_x <= next_sprite_x;
    sprite_y <= next_sprite_y;
    sprite_palette_ind <= next_sprite_palette_ind;
  end

  always_comb begin
    sprite_addr  = 0;
    texture_addr = 0;
    palette_addr = 0;

    // Only draw the buffer when not blanking
    if (blank_n) palette_addr = {palette_ind[2:0], palette_offset};

    if (update_line_buffers) begin
      // if (clear) $write("[%02X][%03X]: Clearing\n", v_count, h_count_raw);

      // Technically this should be h_count_raw - HACTIVE, but HACTIVE is
      // a multiple of 256, so this also works!
      if (fetch_attr) sprite_addr = h_count_raw[7:0];
      // Use the texture index fetched last cycle, using the sprite's relative
      // y position to choose the sliver
      if (fetch_texture) texture_addr = {sprite_texture_ind, sprite_y_offset[2:0]};

      // if (fetch_attr)
      //   $write("[%0d][%0d]: Fetching sprite [%02X]\n", h_count_raw, v_count, sprite_addr);
      //
      // if (fetch_texture) begin
      //   $write("[%0d][%0d]: Fetched sprite [%0d] with values [%0t][%0t][%01X][%02X][%02X][%03X]\n",
      //          h_count_raw, v_count, h_count_raw[7:0] - 1, next_sprite_flip_v, next_sprite_flip_h,
      //          next_sprite_palette_ind, texture_addr[9:3], next_sprite_y, next_sprite_x);
      // end

      // if (draw_sliver)
      //   $write(
      //       "[%0d][%0d]: Drawing sprite [%02X] at [%0d][%0d]\n",
      //       h_count_raw,
      //       v_count,
      //       h_count_raw[7:0] - 2,
      //       sprite_x,
      //       sprite_y
      //   );
    end

  end

endmodule : sprite_gpu
