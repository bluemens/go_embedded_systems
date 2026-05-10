// hw/address_decoder.sv
`timescale 1ns/1ps

module addr_decode(
    addr,
    write_data,
    chip_select,
    write,
    rw_tile_buffer,
    rw_tile_graphics,
    rw_sprite_graphics,
    rw_color_palettes,
    rw_OAM,
    write_data_tile_buffer,
    write_data_tile_graphics,
    write_data_sprite_graphics,
    write_data_OAM,
    write_data_color_palettes,
    addr_tile_buffer,
    addr_tile_graphics,
    addr_sprite_graphics,
    addr_color_palettes,
    addr_OAM
);
  // port declarations
  input  logic [12:0] addr;
  input  logic [31:0] write_data;
  input  logic        chip_select;
  input  logic        write;

  output logic        rw_tile_buffer;
  output logic        rw_tile_graphics;
  output logic        rw_sprite_graphics;
  output logic        rw_color_palettes;
  output logic        rw_OAM;

  output logic [31:0] write_data_tile_buffer;
  output logic [31:0] write_data_tile_graphics;
  output logic [31:0] write_data_sprite_graphics;
  output logic [31:0] write_data_OAM;
  output logic [23:0] write_data_color_palettes;

  output logic [ 8:0] addr_tile_buffer;
  output logic [10:0] addr_tile_graphics;
  output logic [10:0] addr_sprite_graphics;
  output logic [ 2:0] addr_color_palettes;
  output logic [ 7:0] addr_OAM;

  always @(*) begin
    // defaults
    rw_tile_buffer          = 1'b0;
    rw_tile_graphics        = 1'b0;
    rw_sprite_graphics      = 1'b0;
    rw_color_palettes       = 1'b0;
    rw_OAM                  = 1'b0;

    write_data_tile_buffer       = 32'h0;
    write_data_tile_graphics     = 32'h0;
    write_data_sprite_graphics   = 32'h0;
    write_data_OAM               = 32'h0;
    write_data_color_palettes    = 24'h0;

    addr_tile_buffer         = 9'h0;
    addr_tile_graphics       = 11'h0;
    addr_sprite_graphics     = 11'h0;
    addr_color_palettes      = 3'h0;
    addr_OAM                 = 8'h0;

    // decode top 3 bits (addr[12:10])
    casez (addr[12:10])
      3'b00z: begin  // Tile Buffer (00x)
        if (chip_select && write) begin
          rw_tile_buffer        = 1'b1;
          addr_tile_buffer      = addr[8:0];
          write_data_tile_buffer= write_data;
        end
      end

      3'b01z: begin  // Tile Graphics (01x)
        if (chip_select && write) begin
          rw_tile_graphics         = 1'b1;
          addr_tile_graphics       = addr[10:0];
          write_data_tile_graphics = write_data;
        end
      end

      3'b10z: begin  // Sprite Graphics (10x)
        if (chip_select && write) begin
          rw_sprite_graphics         = 1'b1;
          addr_sprite_graphics       = addr[10:0];
          write_data_sprite_graphics = write_data;
        end
      end

      3'b110: begin // Color Palettes
        if (chip_select && write) begin
          rw_color_palettes         = 1'b1;
          addr_color_palettes       = addr[2:0];
          write_data_color_palettes = write_data[23:0];
        end
      end

      3'b111: begin // OAM
        if (chip_select && write) begin
          rw_OAM        = 1'b1;
          addr_OAM      = addr[7:0];
          write_data_OAM= write_data;
        end
      end
    endcase
  end
endmodule
