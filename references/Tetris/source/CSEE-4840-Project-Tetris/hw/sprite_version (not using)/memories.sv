module tile_buffer(
    input logic clk, rw_1, rw_2,
    input logic [31:0] write_data_1, write_data_2,
    input logic [8:0] addr_1, addr_2,
    output logic [31:0] read_data_1, read_data_2
);

    logic [31:0] tile_buffer_array [299:0];

    always @(posedge clk) begin
        if (rw_1) tile_buffer_array[addr_1] <= write_data_1;
        else read_data_1 <= tile_buffer_array[addr_1];
        if(rw_2) tile_buffer_array[addr_2] <= write_data_2;
        else read_data_2 <= tile_buffer_array[addr_2];
    end

endmodule 

module tile_graphics(
    input logic clk, rw_1, rw_2,
    input logic [31:0] write_data_1, write_data_2,
    input [10:0] addr_1, addr_2,
    output logic [31:0] read_data_1, read_data_2
);

    logic [31:0] tile_graphics_array [2047:0];

    always @(posedge clk) begin
        if (rw_1) tile_graphics_array[addr_1] <= write_data_1;
        else read_data_1 <= tile_graphics_array[addr_1];
        if(rw_2) tile_graphics_array[addr_2] <= write_data_2;
        else read_data_2 <= tile_graphics_array[addr_2];
    end

endmodule 

module sprite_graphics(
    input logic clk, rw_1, rw_2,
    input logic [31:0] write_data_1, write_data_2, 
    input [10:0] addr_1, addr_2,
    output logic [31:0] read_data_1, read_data_2
);

    logic [31:0] sprite_graphics_array [2047:0];

    always @(posedge clk) begin
        if (rw_1) sprite_graphics_array[addr_1] <= write_data_1;
        else read_data_1 <= sprite_graphics_array[addr_1];
        if(rw_2) sprite_graphics_array[addr_2] <= write_data_2;
        else read_data_2 <= sprite_graphics_array[addr_2];
    end

endmodule 

module color_palettes(
    input logic clk, rw_1, rw_2,
    input logic [23:0] write_data_1, write_data_2, 
    input [2:0] addr_1, addr_2,
    output logic [23:0] read_data_1, read_data_2
);

    logic [23:0] color_palette_array [7:0];

    always @(posedge clk) begin
        if (rw_1) color_palette_array[addr_1] <= write_data_1;
        else read_data_1 <= color_palette_array[addr_1];
        if(rw_2) color_palette_array[addr_2] <= write_data_2;
        else read_data_2 <= color_palette_array[addr_2];
    end

endmodule

module OAM (
    input logic clk, rw_1, rw_2,
    input logic [31:0] write_data_1, write_data_2, 
    input [7:0] addr_1, addr_2,
    output logic [31:0] read_data_1, read_data_2
);

    logic [31:0] OAM_array [255:0];

    always @(posedge clk) begin
        if (rw_1) OAM_array[addr_1] <= write_data_1;
        else read_data_1 <= OAM_array[addr_1];
        if(rw_2) OAM_array[addr_2] <= write_data_2;
        else read_data_2 <= OAM_array[addr_2];
    end

endmodule
