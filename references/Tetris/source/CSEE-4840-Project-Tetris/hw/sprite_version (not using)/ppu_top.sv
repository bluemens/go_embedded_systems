module ppu_top(

    input logic clk,
    input logic reset,
    input logic [31:0] write_data,
    input logic [12:0] address,
    input logic write,
    input chipselect,
    output logic irq,

    output logic [7:0] VGA_R, 
    output logic [7:0] VGA_G, 
    output logic [7:0] VGA_B,
    output logic VGA_CLK, VGA_HS, VGA_VS, VGA_BLANK_n, VGA_SYNC_n
);

    //PPU RW Signals to Memories 
    logic ppu_rw_tile_buffer;
    logic ppu_rw_tile_graphics;
    logic ppu_rw_sprite_graphics;
    logic ppu_rw_color_palettes;
    logic ppu_rw_OAM;

    //CPU RW Signals to Memories 
    logic cpu_rw_tile_buffer;
    logic cpu_rw_tile_graphics;
    logic cpu_rw_sprite_graphics;
    logic cpu_rw_color_palettes;
    logic cpu_rw_OAM;

    //PPU Write Data to Memories
    logic [31:0] ppu_write_data_tile_buffer;
    logic [31:0] ppu_write_data_tile_graphics;
    logic [31:0] ppu_write_data_sprite_graphics;
    logic [31:0] ppu_write_data_OAM;
    logic [23:0] ppu_write_data_color_palettes;

    //CPU Write Data to Memories
    logic [31:0] cpu_write_data_tile_buffer;
    logic [31:0] cpu_write_data_tile_graphics;
    logic [31:0] cpu_write_data_sprite_graphics;
    logic [31:0] cpu_write_data_OAM;
    logic [23:0] cpu_write_data_color_palettes;

    //PPU Address Signals to Memories
    logic [8:0] ppu_addr_tile_buffer;
    logic [10:0] ppu_addr_tile_graphics;
    logic [10:0] ppu_addr_sprite_graphics;
    logic [2:0] ppu_addr_color_palettes;
    logic [7:0] ppu_addr_OAM;

    //CPU Address Signals to Memories
    logic [8:0] cpu_addr_tile_buffer;
    logic [10:0] cpu_addr_tile_graphics;
    logic [10:0] cpu_addr_sprite_graphics;
    logic [2:0] cpu_addr_color_palettes;
    logic [7:0] cpu_addr_OAM;

    //PPU Read Data from Memories
    logic [31:0] ppu_read_data_tile_buffer;
    logic [31:0] ppu_read_data_tile_graphics;
    logic [31:0] ppu_read_data_sprite_graphics;
    logic [31:0] ppu_read_data_OAM;
    logic [23:0] ppu_read_data_color_palettes;

    //VGA Singals
    logic [10:0] hcount;
    logic [9:0] vcount;
    logic [23:0] pixel_color;
    logic vblank;

    //Shift Register Signals
    logic [31:0] shift_load_data [8:0];
    logic [8:0] shift_enable;
    logic shift_load_sprite; 
    logic shift_load_background;

    //Priority Encoder Signals
    logic priority_palette_data_asm_to_encoder [8:0];
    logic [1:0] priority_pixel_data_encoder_to_asm;
    logic priority_palette_data_encoder_to_asm;
    logic [1:0] priority_pixel_data_shifter_to_encoder [8:0];

      addr_decode decoder(
        .addr(address),
        .write_data(write_data),
        .chip_select(chipselect),
        .write(write),
        .rw_tile_buffer(cpu_rw_tile_buffer), 
        .rw_tile_graphics(cpu_rw_tile_graphics), 
        .rw_sprite_graphics(cpu_rw_sprite_graphics), 
        .rw_color_palettes(cpu_rw_color_palettes), 
        .rw_OAM(cpu_rw_OAM),
        .write_data_tile_buffer(cpu_write_data_tile_buffer), 
        .write_data_tile_graphics(cpu_write_data_tile_graphics), 
        .write_data_sprite_graphics(cpu_write_data_sprite_graphics), 
        .write_data_OAM(cpu_write_data_OAM),
        .write_data_color_palettes(cpu_write_data_color_palettes),
        .addr_tile_buffer(cpu_addr_tile_buffer),
        .addr_tile_graphics(cpu_addr_tile_graphics), 
        .addr_sprite_graphics(cpu_addr_sprite_graphics),
        .addr_color_palettes(cpu_addr_color_palettes),
        .addr_OAM(cpu_addr_OAM)
    );


    tile_buffer tile_buffer_mem (
        .clk(clk), 
        .rw_1(ppu_rw_tile_buffer),
        .rw_2(cpu_rw_tile_buffer),
        .write_data_1(ppu_write_data_tile_buffer), 
        .write_data_2(cpu_write_data_tile_buffer),
        .addr_1(ppu_addr_tile_buffer), 
        .addr_2(cpu_addr_tile_buffer),
        .read_data_1(ppu_read_data_tile_buffer), 
        .read_data_2()
    );

    tile_graphics tile_graphics_mem (
        .clk(clk), 
        .rw_1(ppu_rw_tile_graphics), 
        .rw_2(cpu_rw_tile_graphics),
        .write_data_1(ppu_write_data_tile_graphics), 
        .write_data_2(cpu_write_data_tile_graphics),
        .addr_1(ppu_addr_tile_graphics),
        .addr_2(cpu_addr_tile_graphics),
        .read_data_1(ppu_read_data_tile_graphics), 
        .read_data_2()
    );


    sprite_graphics sprite_graphics_mem (
        .clk(clk),
        .rw_1(ppu_rw_sprite_graphics), 
        .rw_2(cpu_rw_sprite_graphics),
        .write_data_1(ppu_write_data_sprite_graphics), 
        .write_data_2(cpu_write_data_sprite_graphics), 
        .addr_1(ppu_addr_sprite_graphics), 
        .addr_2(cpu_addr_sprite_graphics),
        .read_data_1(ppu_read_data_sprite_graphics), 
        .read_data_2()
    );


    color_palettes color_palettes_mem(
        .clk(clk), 
        .rw_1(ppu_rw_color_palettes), 
        .rw_2(cpu_rw_color_palettes),
        .write_data_1(ppu_write_data_color_palettes), 
        .write_data_2(cpu_write_data_color_palettes), 
        .addr_1(ppu_addr_color_palettes), 
        .addr_2(cpu_addr_color_palettes),
        .read_data_1(ppu_read_data_color_palettes), 
        .read_data_2()
    );

    OAM OAM_mem (
        .clk(clk),  
        .rw_1(ppu_rw_OAM), 
        .rw_2(cpu_rw_OAM),
        .write_data_1(ppu_write_data_OAM),
        .write_data_2(cpu_write_data_OAM), 
        .addr_1(ppu_addr_OAM),
        .addr_2(cpu_addr_OAM),
        .read_data_1(ppu_read_data_OAM), 
        .read_data_2()
    );

    PPU_asm asm( 
        .clk(clk),
        .reset(reset),
        .hcount(hcount), 
        .vcount(vcount), 
        .vblank(vblank), 
        .hsync(VGA_HS), 
        .pixel_color(pixel_color),
        .rw_tile_buffer(ppu_rw_tile_buffer), 
        .rw_tile_graphics(ppu_rw_tile_graphics), 
        .rw_sprite_graphics(ppu_rw_sprite_graphics), 
        .rw_color_palettes(ppu_rw_color_palettes), 
        .rw_OAM(ppu_rw_OAM),
        .write_data_tile_buffer(ppu_write_data_tile_buffer), 
        .write_data_tile_graphics(ppu_write_data_tile_graphics), 
        .write_data_sprite_graphics(ppu_write_data_sprite_graphics), 
        .write_data_OAM(ppu_write_data_OAM),
        .write_data_color_palettes(ppu_write_data_color_palettes),
        .addr_tile_buffer(ppu_addr_tile_buffer),
        .addr_tile_graphics(ppu_addr_tile_graphics), 
        .addr_sprite_graphics(ppu_addr_sprite_graphics),
        .addr_color_palettes(ppu_addr_color_palettes),
        .addr_OAM(ppu_addr_OAM),
        .read_data_tile_buffer(ppu_read_data_tile_buffer), 
        .read_data_tile_graphics(ppu_read_data_tile_graphics), 
        .read_data_sprite_graphics(ppu_read_data_sprite_graphics), 
        .read_data_OAM(ppu_read_data_OAM),
        .read_data_color_palettes(ppu_read_data_color_palettes),
        .shift_load_data(shift_load_data),
        .shift_enable(shift_enable),
        .shift_load_sprite(shift_load_sprite), 
        .shift_load_background(shift_load_background),
        .priority_palette_data_out(priority_palette_data_asm_to_encoder),
        .priority_pixel_data_in(priority_pixel_data_encoder_to_asm),
        .priority_palette_data_in(priority_palette_data_encoder_to_asm)
    );

    combined_priority_encoder priority_encoder (
        .pixel_data_in(priority_pixel_data_shifter_to_encoder),
        .palette_data_in(priority_palette_data_asm_to_encoder),
        .pixel_data_out(priority_pixel_data_encoder_to_asm),
        .palette_data_out(priority_palette_data_encoder_to_asm)
    );

    shift_register_block shift_registers (
        .load_data(shift_load_data),
        .clk(clk), 
        .reset(reset), 
        .load_sprite(shift_load_sprite), 
        .load_background(shift_load_background),
        .enable(shift_enable),
        .out_data(priority_pixel_data_shifter_to_encoder)
    );

    vga vga_inst(
        .clk(clk),
        .reset(reset),
        .pixel_color(pixel_color),
        .VGA_R(VGA_R), 
        .VGA_G(VGA_G), 
        .VGA_B(VGA_B),
        .VGA_CLK(VGA_CLK), 
        .VGA_HS(VGA_HS), 
        .VGA_VS(VGA_VS), 
        .VGA_BLANK_n(VGA_BLANK_n), 
        .VGA_SYNC_n(VGA_SYNC_n),
        .VGA_vBLANK(vblank),
        .hcount(hcount),
        .vcount(vcount)
    );



endmodule
