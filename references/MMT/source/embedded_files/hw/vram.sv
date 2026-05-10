/*
* Memory map (16-bit address space)
* MSB is used to determine which area to read from
* 0x0000 - 0x7FFF Texture data (32-bit)
*   0xDDDDDDDD Texture row data/palette offset (4bpp)
* 0x8000 - 0xBFFF Tile data (16-bit)
*   0bVHOPPPTT_TTTTTTTT
*     |||||||| ||||||||
*     ||||||++-++++++++- Texture index
*     |||+++------------ Palette index
*     ||+--------------- Tile priority
*     |+---------------- Horizontal flip
*     +----------------- Vertical flip
* 0xF000 - 0xF3FF Sprite data (32-bit)
*   0bVHPPPTTT_TTTTTTTY_YYYYYYYX_XXXXXXXX
*     |||||||| |||||||| |||||||| ||||||||
*     |||||||| |||||||| |||||||+-++++++++- X position
*     |||||||| |||||||+-+++++++----------- Y position
*     |||||+++-+++++++-------------------- Texture index
*     ||+++------------------------------- Palette index
*     |+---------------------------------- Horizontal flip
*     +----------------------------------- Vertical flip
*
* 0xF400 - 0F5FF Palette data
*   0bRRRRRRRR_GGGGGGGG_BBBBBBBB_AAAAAAAA Color data (A is unused for now)
*
* ====Registers====
* 0xF000 Control (W32)
*   0b0000_0000_0000_000F
*                       |
*                       +- Force blanking
* 0xF004 X scroll (W16)
*   0b0000_000X_XXXX_XXXX
*             | |||| ||||
*             +-++++-++++- X value
* 0xF006 Y scroll (W16)
*   0b0000_000Y_YYYY_YYYY
*             | |||| ||||
*             +-++++-++++- Y value
* 0xF008 Background color (W32)
*   0bRRRRRRRR_GGGGGGGG_BBBBBBBB_AAAAAAAA
* 0xF00c Horizontal count (R16)
*   0b0000_00HH_HHHH_HHHH
*            || |||| ||||
*            ++-++++-++++- Horizontal count
* 0xF010 Vertical count (R16)
*   0b0000_00VV_VVVV_VVVV
*            || |||| ||||
*            ++-++++-++++- Vertical count
*/
module vram #(
    // Bytes per word
    parameter int BYTES_PER_WORD = 4,
    parameter int ADDR_WIDTH = 14,
    // # of bits read at once
    parameter int DATA_WIDTH = 32,

    parameter int TEXTURE_ADDR_WIDTH = 13,
    parameter int TILE_ADDR_WIDTH = 13,
    parameter int SPRITE_ADDR_WIDTH = 8,
    parameter int PALETTE_ADDR_WIDTH = 7,

    parameter int TEXTURE_DATA_WIDTH = 32,
    parameter int TILE_DATA_WIDTH = 16,
    parameter int SPRITE_DATA_WIDTH = 32,
    parameter int PALETTE_DATA_WIDTH = 32
) (
    input clk,
    // Wire these to access from the outside
    input [ADDR_WIDTH-1:0] addr,  // Always 14-bit (2**16 bytes == 2**14 words)
    input [DATA_WIDTH-1:0] din,
    input [BYTES_PER_WORD-1:0] be,
    input we,
    output logic [DATA_WIDTH-1:0] dout,

    // These can all be accessed simultaneously
    input logic [TEXTURE_ADDR_WIDTH-1:0] texture_addr,  // 32 KiB == 13 bits
    output logic [TEXTURE_DATA_WIDTH-1:0] texture_data,
    input logic [TILE_ADDR_WIDTH-1:0] tile_addr,  // 16 KiB == 12 bits

    output logic [TILE_DATA_WIDTH-1:0] tile_data,
    input logic [SPRITE_ADDR_WIDTH-1:0] sprite_addr,  // 1 KiB == 8 bits
    output logic [SPRITE_DATA_WIDTH-1:0] sprite_data,
    input logic [PALETTE_ADDR_WIDTH-1:0] palette_addr,  // 512 B == 7 bits
    output logic [PALETTE_DATA_WIDTH-1:0] palette_data
);

  logic texture_select, tile_select, sprite_select, palette_select;
  logic [DATA_WIDTH-1:0] texture_dout, tile_dout, sprite_dout, palette_dout;
  logic prev_tile_data_select;

  logic [DATA_WIDTH-1:0] tile_data_raw;

  assign texture_select = ~addr[13];  // 0x0000 - 0x7FFF
  assign tile_select = addr[13] & ~addr[12];  // 0x8000 - 0xBFFF
  assign sprite_select = &addr[13:10] & ~|addr[9:8];  // 0xF000 - 0xF3FF
  assign palette_select = {addr, 2'b0} >= 16'hF400 && {addr, 2'b0} < 16'hF5FF;
  // assign palette_select = &addr[13:10] & addr[9] & ~addr[8];  // 0xF400 - 0xF5FF

  dual_port_be_ram #(
      .ADDR_WIDTH(TEXTURE_ADDR_WIDTH)  // 32 KiB
  ) textures (
      .clk(clk),

      .addr_a(addr[TEXTURE_ADDR_WIDTH-1:0]),
      .be_a  (be),
      .din_a (din),
      .we_a  (we & texture_select),
      .dout_a(texture_dout),

      .addr_b(texture_addr),
      .be_b  (0),
      .din_b (0),
      .we_b  (0),
      .dout_b(texture_data)
  );

  dual_port_be_ram #(
      .ADDR_WIDTH(TILE_ADDR_WIDTH - 1)  // 16 KiB
  ) bg_tiles (
      .clk(clk),

      .addr_a(addr[TILE_ADDR_WIDTH-2:0]),
      .be_a  (be),
      .din_a (din),
      .we_a  (we & tile_select),
      .dout_a(tile_dout),

      .addr_b(tile_addr[TILE_ADDR_WIDTH-1:1]),
      .be_b  (0),
      .din_b (0),
      .we_b  (0),
      .dout_b(tile_data_raw)
  );
  // assign tile_data = tile_data_raw[15:0];
  assign tile_data = prev_tile_data_select ? tile_data_raw[31:16] : tile_data_raw[15:0];
  always_ff @(posedge clk) prev_tile_data_select <= tile_addr[0];


  dual_port_be_ram #(
      .ADDR_WIDTH(SPRITE_ADDR_WIDTH)  // 1 KiB
  ) sprites (
      .clk(clk),

      .addr_a(addr[SPRITE_ADDR_WIDTH-1:0]),
      .be_a  (be),
      .din_a (din),
      .we_a  (we & sprite_select),
      .dout_a(sprite_dout),

      .addr_b(sprite_addr),
      .be_b  (0),
      .din_b (0),
      .we_b  (0),
      .dout_b(sprite_data)
  );

  dual_port_be_ram #(
      .ADDR_WIDTH(PALETTE_ADDR_WIDTH)  // 512 B
  ) palette (
      .clk(clk),

      .addr_a(addr[PALETTE_ADDR_WIDTH-1:0]),
      .be_a  (be),
      .din_a (din),
      .we_a  (we & palette_select),
      .dout_a(palette_dout),

      .addr_b(palette_addr),
      .be_b  (0),
      .din_b (0),
      .we_b  (0),
      .dout_b(palette_data)
  );

  always_comb begin
    dout = 0;

    if (texture_select) dout = texture_dout;
    else if (tile_select) dout = tile_dout;
    else if (sprite_select) dout = sprite_dout;
    else if (palette_select) dout = palette_dout;

    // if (clk & tile_select)
    //   $write("Addr %04X (byte enable %04b) given value %04X\n", {addr, 2'b0}, be, din);
  end

endmodule : vram
