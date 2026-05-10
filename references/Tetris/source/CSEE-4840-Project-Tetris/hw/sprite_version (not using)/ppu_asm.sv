module PPU_asm( 
    input logic clk, //Clock
    input logic reset, //Active High Reset

    //VGA related IO
    input logic [10:0] hcount, //VGA hcount from VGA Controller
    input logic [9:0] vcount, //VGA vcount from VGA Controller
    input logic vblank, //VGA vblank from VGA Controller
    input logic hsync, //VGA hsync from VGA Controller
    output logic [23:0] pixel_color, //Pixel Color to Send to VGA Controller

    //RW Signals to Memories
    output logic rw_tile_buffer, 
    output logic rw_tile_graphics, 
    output logic rw_sprite_graphics, 
    output logic rw_color_palettes, 
    output logic rw_OAM,

    //Write Data to Memories
    output logic [31:0] write_data_tile_buffer, 
    output logic [31:0] write_data_tile_graphics, 
    output logic [31:0] write_data_sprite_graphics, 
    output logic [31:0] write_data_OAM,
    output logic [23:0] write_data_color_palettes,

    //Address Signals to Memories
    output logic [8:0] addr_tile_buffer,
    output logic [10:0] addr_tile_graphics, 
    output logic [10:0] addr_sprite_graphics,
    output logic [2:0] addr_color_palettes,
    output logic [7:0] addr_OAM,

    //Read Data from Memories
    input logic [31:0] read_data_tile_buffer, 
    input logic [31:0] read_data_tile_graphics, 
    input logic [31:0] read_data_sprite_graphics, 
    input logic [31:0] read_data_OAM,
    input logic [23:0] read_data_color_palettes,

    //Shift Register Signals
    output logic [31:0] shift_load_data [8:0],
    output logic [8:0] shift_enable,
    output logic shift_load_sprite, 
    output logic shift_load_background,

    //Priority Encoder Signals
    output logic  priority_palette_data_out[8:0],
    input logic [1:0] priority_pixel_data_in,
    input logic priority_palette_data_in
);

    //Buffers Updated During Vblank
    logic [23:0] color_palette_buffer [8:0];
    logic [15:0] sprite_x_buffer [127:0];
    logic [15:0] sprite_y_buffer [127:0];
    logic sprite_palette_buffer [127:0];
    logic [6:0] sprite_tile_id_buffer [127:0];
    logic [1:0] sprite_rotation_buffer [127:0];

    //Buffers Updated During Hsync
    logic [31:0] background_line_graphics_buffer [39:0];
    logic [39:0] background_line_palette_buffer;
    logic [31:0] sprite_graphics_buffer [7:0];
    logic [6:0] sprites_on_line [7:0];
    logic sprites_on_line_palettes[7:0];

    //Vblank memory access trackers
    logic [7:0] cords_sprite_load;
    logic [7:0] palette_sprite_load;
    logic [3:0] palette_ram_pointer;
    
    //Hsync memory access trackers
    logic [5:0] background_line_pointer;
    logic [2:0] sprite_graphics_pointer;
    logic [7:0] sprites_on_line_pointer;
    logic [3:0] shift_register_load_pointer;
    logic [3:0] sprites_found;

    always @(posedge clk) begin
        
        //reset 
        if (reset) begin
            rw_tile_buffer <= 0;
            rw_tile_graphics <= 0;
            rw_sprite_graphics <= 0;
            rw_color_palettes <= 0;
            rw_OAM <= 0;
            cords_sprite_load <= 0;
            palette_sprite_load <= 0;
            palette_ram_pointer <= 0;
            background_line_pointer <= 0;
            sprite_graphics_pointer <= 0;
            sprites_on_line_pointer <= 0;
            sprites_found <= 0;
            shift_register_load_pointer <= 0;
            shift_enable <= 0;
            shift_load_background <= 0;
            shift_load_sprite <= 0;
            background_line_palette_buffer <= 40'b0;
            for (int i = 0; i < 128; i = i + 1) begin
                sprite_x_buffer[i] <= 0;
                sprite_y_buffer[i] <= 0;
                sprite_tile_id_buffer[i] <= 0;
                sprite_rotation_buffer[i] <= 0;
		sprite_palette_buffer[i] <= 0;
            end
            for (int i = 0; i < 8; i += 1) begin
                sprite_graphics_buffer[i] <= 0;
                sprites_on_line[i] <= 0;
                shift_load_data[i] <= 0;
                sprites_on_line_palettes[i] <= 0;
            end
            for (int i = 0; i < 40; i += 1) begin
                background_line_graphics_buffer[i] <= 0;
            end
            for (int i = 0; i < 9; i += 1) begin
                priority_palette_data_out[i] <= 0;
            end

        end

        /* 
        Note that reading from any of the memories in memories.sv takes 2 cycles. On cycle 1, we give the memory the address
        we want to read from. On cycle 2, we get the data at that address on the memories respective read_data line. As such,
        you will notice that when filling the buffers from memory, there is a one cycle offset between sending the address and 
        filling the buffer, hence the at first confusing four tiered if statements. For the first one, we only send the address
        since we have no data to load. For the second, we load the data for the previous address and send the next address. For
        the third one we have no new addresses to send but we still need to received and load the final data return into the buffer.
        For the fourth one, we are done with both addresses and reading data.
        */

        //set buffers that fill once per frame
        else if (vblank) begin

            //fill color_palette_buffer
            if (palette_ram_pointer == 0) begin
                rw_color_palettes <= 0; //Set color palette memory to read
                addr_color_palettes <= palette_ram_pointer; 
                palette_ram_pointer <= palette_ram_pointer + 1;
            end

            else if (palette_ram_pointer < 8) begin
                rw_color_palettes <= 0; //Set color palette memory to read
                addr_color_palettes <= palette_ram_pointer;
                color_palette_buffer[palette_ram_pointer - 1] = read_data_color_palettes;
                palette_ram_pointer <= palette_ram_pointer + 1;
            end

            else if (palette_ram_pointer == 8) begin
                rw_color_palettes <= 0; //Set color palette memory to read
                addr_color_palettes <= 0;
                color_palette_buffer[palette_ram_pointer - 1] = read_data_color_palettes;
                palette_ram_pointer <= palette_ram_pointer + 1;
            end

            else begin
                rw_color_palettes <= 0; //Set color palette memory to read
                addr_color_palettes <= 0;
            end
            
            //fill sprite_x_buffer and sprite_y_buffer
            if (cords_sprite_load == 0) begin
                rw_OAM <= 0; //Set OAM memory to read
                addr_OAM <= cords_sprite_load * 2 + 1;
                cords_sprite_load <= cords_sprite_load + 1;
            end

            else if (cords_sprite_load < 128) begin
                rw_OAM <= 0; //Set OAM memory to read
                addr_OAM <= cords_sprite_load * 2 + 1;
                sprite_x_buffer[cords_sprite_load - 1] <= read_data_OAM[15:0];
                sprite_y_buffer[cords_sprite_load - 1] <= read_data_OAM[31:16];
                cords_sprite_load <= cords_sprite_load + 1;
            end

            else if (cords_sprite_load == 128) begin
                rw_OAM <= 0; //Set OAM memory to read
                addr_OAM <= palette_sprite_load;
                sprite_x_buffer[cords_sprite_load - 1] <= read_data_OAM[15:0];
                sprite_y_buffer[cords_sprite_load - 1] <= read_data_OAM[31:16];
                cords_sprite_load <= cords_sprite_load + 1;
                palette_sprite_load <= palette_sprite_load + 1;
            end

            else if (palette_sprite_load < 128) begin
                rw_OAM <= 0; //Set OAM memory to read
                addr_OAM <= palette_sprite_load * 2;
                sprite_palette_buffer[palette_sprite_load - 1] <= read_data_OAM[7];
                sprite_tile_id_buffer[palette_sprite_load - 1] <= read_data_OAM[6:0];
                sprite_rotation_buffer[palette_sprite_load - 1] <= read_data_OAM[31:30];
                palette_sprite_load <= palette_sprite_load + 1;
            end

            else if (palette_sprite_load == 128) begin
                rw_OAM <= 0; //Set OAM memory to read
                addr_OAM <= 0;
                sprite_palette_buffer[palette_sprite_load - 1] <= read_data_OAM[7];
                sprite_tile_id_buffer[palette_sprite_load - 1] <= read_data_OAM[6:0];
                sprite_rotation_buffer[palette_sprite_load - 1] <= read_data_OAM[31:30];
                palette_sprite_load <= palette_sprite_load + 1;
            end

            else begin
                rw_OAM <= 0; //Set OAM memory to read
                addr_OAM <= 0;
            end

        end

        //Set buffers that fill once per line
        else if (hsync) begin

            //Load background tiles into buffer

            if (background_line_pointer == 0) begin
                
                rw_tile_buffer <= 0; //Set tile buffer memory to read

                /*Calculate address into the tile buffer for tile at the start of the current line. 
                We do *10 and not *40 since each 32-bit entry of the tile-buffer holds 4 tile IDs */
                addr_tile_buffer <= vcount * 10; 

                background_line_pointer <= background_line_pointer + 1;

            end
        
            else if (background_line_pointer == 1) begin

                rw_tile_buffer <= 0; //Set tile buffer memory to read
                rw_tile_graphics <= 0;

                /*Calculate address into the tile buffer for current tile being processed.
                We do >> 2 since each 32-bit entry of the tile-buffer holds 4 tile IDs */
                addr_tile_buffer <= (vcount * 10) + (background_line_pointer >> 2);

        
                /* Calculate the address into the tile graphics memory for the current line
                of the current tile being processed */
                case (background_line_pointer[1:0]) 
                    
                    1: begin
                        addr_tile_graphics <= (read_data_tile_buffer[6:0] * 16) + (background_line_pointer - 1);
                        background_line_palette_buffer[background_line_pointer - 1] <= read_data_tile_buffer[7];
                    end

                    2: begin
                        addr_tile_graphics <= (read_data_tile_buffer[14:8] * 16) + (background_line_pointer - 1);
                        background_line_palette_buffer[background_line_pointer - 1] <= read_data_tile_buffer[15];

                    end

                    3: begin
                        addr_tile_graphics <= (read_data_tile_buffer[22:16] * 16) + (background_line_pointer - 1);
                        background_line_palette_buffer[background_line_pointer - 1] <= read_data_tile_buffer[23];

                    end

                    0: begin
                        addr_tile_graphics <= (read_data_tile_buffer[30:24] * 16) + (background_line_pointer - 1);
                        background_line_palette_buffer[background_line_pointer - 1] <= read_data_tile_buffer[31];
                    end
                    
                endcase

                

                background_line_pointer <= background_line_pointer + 1;
        
            end

            else if (background_line_pointer < 40) begin
                
                rw_tile_buffer <= 0; //Set tile buffer memory to read
                rw_tile_graphics <= 0;

                /*Calculate address into the tile buffer for current tile being processed.
                We do >> 2 since each 32-bit entry of the tile-buffer holds 4 tile IDs */
                addr_tile_buffer <= (vcount * 10) + (background_line_pointer >> 2); 

                
                /* Calculate the address into the tile graphics memory for the current line
                of the current tile being processed */
                case (background_line_pointer[1:0]) 
                    
                    1: begin
                        addr_tile_graphics <= (read_data_tile_buffer[6:0] * 16) + (background_line_pointer - 1);
                        background_line_palette_buffer[background_line_pointer - 1] <= read_data_tile_buffer[7];
                    end

                    2: begin
                        addr_tile_graphics <= (read_data_tile_buffer[14:8] * 16) + (background_line_pointer - 1);
                        background_line_palette_buffer[background_line_pointer - 1] <= read_data_tile_buffer[15];

                    end

                    3: begin
                        addr_tile_graphics <= (read_data_tile_buffer[22:16] * 16) + (background_line_pointer - 1);
                        background_line_palette_buffer[background_line_pointer - 1] <= read_data_tile_buffer[23];

                    end

                    0: begin
                        addr_tile_graphics <= (read_data_tile_buffer[30:24] * 16) + (background_line_pointer - 1);
                        background_line_palette_buffer[background_line_pointer - 1] <= read_data_tile_buffer[31];
                    end
                    
                endcase

                background_line_graphics_buffer[(background_line_pointer - 1)] <= read_data_tile_graphics;

                background_line_pointer <= background_line_pointer + 1;

            end

            else if (background_line_pointer == 40) begin

                rw_tile_buffer <= 0; //Set tile buffer memory to read
                rw_tile_graphics <= 0;

                addr_tile_buffer <= 0;

                /* Calculate the address into the tile graphics memory for the current line
                of the current tile being processed */
                case (background_line_pointer[1:0]) 
                    
                    1: begin
                        addr_tile_graphics <= (read_data_tile_buffer[6:0] * 16) + (background_line_pointer - 1);
                        background_line_palette_buffer[background_line_pointer - 1] <= read_data_tile_buffer[7];
                    end

                    2: begin
                        addr_tile_graphics <= (read_data_tile_buffer[14:8] * 16) + (background_line_pointer - 1);
                        background_line_palette_buffer[background_line_pointer - 1] <= read_data_tile_buffer[15];

                    end

                    3: begin
                        addr_tile_graphics <= (read_data_tile_buffer[22:16] * 16) + (background_line_pointer - 1);
                        background_line_palette_buffer[background_line_pointer - 1] <= read_data_tile_buffer[23];

                    end

                    0: begin
                        addr_tile_graphics <= (read_data_tile_buffer[30:24] * 16) + (background_line_pointer - 1);
                        background_line_palette_buffer[background_line_pointer - 1] <= read_data_tile_buffer[31];
                    end
                    
                endcase

                background_line_graphics_buffer[(background_line_pointer - 1)] <= read_data_tile_graphics;

                background_line_pointer <= background_line_pointer + 1;

            end

            else if (background_line_pointer == 41) begin

                rw_tile_buffer <= 0; //Set tile buffer memory to read
                rw_tile_graphics <= 0;

                addr_tile_buffer <= 0;
                addr_tile_graphics <= 0;

                background_line_graphics_buffer[background_line_pointer - 1] <= read_data_tile_graphics;

                background_line_pointer <= background_line_pointer + 1;

            end

            else begin
                rw_tile_buffer <= 0; //Set tile buffer memory to read
                rw_tile_graphics <= 0;

                addr_tile_buffer <= 0;
                addr_tile_graphics <= 0;
            end
            
            //Detect which sprites are on the line 
            if (sprites_on_line_pointer < 128) begin

                //If current sprite is on the line and we have not filled all the sprite slots
                if (sprites_found < 8 && (vcount >= sprite_y_buffer[sprites_on_line_pointer]) && (vcount < sprite_y_buffer[sprites_on_line_pointer] + 16)) begin
                    sprites_on_line[sprites_found] <= sprites_on_line_pointer;
                    sprites_on_line_palettes[sprites_found] <= sprite_palette_buffer[sprites_on_line_pointer];
                    sprites_found <= sprites_found + 1;
                end
                
                sprites_on_line_pointer <= sprites_on_line_pointer + 1;
            end

            //Calculate pointers to sprite graphics based on rotation flags, what line we are on, and the sprites' Y positions
            else if (shift_register_load_pointer == 0) begin
                rw_sprite_graphics <= 0;
                
                // If vertical flip bit is set
                if (sprite_rotation_buffer[sprites_on_line[shift_register_load_pointer]][1]) addr_sprite_graphics <= ((sprite_tile_id_buffer[sprites_on_line[shift_register_load_pointer]]) * 16) + (15 - (vcount - sprite_y_buffer[sprites_on_line[shift_register_load_pointer]]));
                
                // If vertical flip bit is not set
                else addr_sprite_graphics <= ((sprite_tile_id_buffer[sprites_on_line[shift_register_load_pointer]]) * 16) + (vcount - sprite_y_buffer[sprites_on_line[shift_register_load_pointer]]);
                
                shift_register_load_pointer <= shift_register_load_pointer + 1;

            end

            else if (shift_register_load_pointer < 8) begin

                rw_sprite_graphics <= 0;
                
                // If vertical flip bit is set
                if (sprite_rotation_buffer[sprites_on_line[shift_register_load_pointer]][1]) addr_sprite_graphics <= ((sprite_tile_id_buffer[sprites_on_line[shift_register_load_pointer]]) * 16) + (15 - (vcount - sprite_y_buffer[sprites_on_line[shift_register_load_pointer]]));

                // If vertical flip bit is not set
                else addr_sprite_graphics <= ((sprite_tile_id_buffer[sprites_on_line[shift_register_load_pointer]]) * 16) + (vcount - sprite_y_buffer[sprites_on_line[shift_register_load_pointer]]);
                
                //Check against sprites_found to make sure we don't load garbage data into the graphics buffers
                if (sprites_on_line_pointer <= sprites_found) begin
                    // If horizontal flip bit is set
                    //if (sprite_rotation_buffer[sprites_on_line[shift_register_load_pointer]][0]) sprite_graphics_buffer[shift_register_load_pointer - 1][31:0] <= read_data_sprite_graphics[0:31];
                    if (sprite_rotation_buffer[sprites_on_line[shift_register_load_pointer]][0]) begin
                      // horizontal‐flip: reverse all bits
                      sprite_graphics_buffer[shift_register_load_pointer-1] <= bit_reverse32(read_data_sprite_graphics);
                    end // If horizontal flip bit is not set
                    else sprite_graphics_buffer[shift_register_load_pointer - 1] <= read_data_sprite_graphics;
                end 
                //If sprite slot is empty, fill place in sprite graphics buffer with zeros
                else sprite_graphics_buffer[shift_register_load_pointer - 1] <= 0;
                
                shift_register_load_pointer <= shift_register_load_pointer + 1;

            end

            else if (shift_register_load_pointer == 8) begin

                rw_sprite_graphics <= 0;
                addr_sprite_graphics <= 0;

                //Check against sprites_found to make sure we don't load garbage data into the graphic buffers
                if (sprites_on_line_pointer <= sprites_found) begin
                    // If horizontal flip bit is set
                    //if (sprite_rotation_buffer[sprites_on_line[shift_register_load_pointer]][0]) sprite_graphics_buffer[shift_register_load_pointer - 1][31:0] <= read_data_sprite_graphics[0:31];
                    if (sprite_rotation_buffer[sprites_on_line[shift_register_load_pointer]][0]) begin
                      // horizontal‐flip: reverse all 32 bits
                          sprite_graphics_buffer[shift_register_load_pointer-1] 
                            <= bit_reverse32(read_data_sprite_graphics);                    // If horizontal flip bit is not set
                    end else sprite_graphics_buffer[shift_register_load_pointer - 1] <= read_data_sprite_graphics;
                end 
                //If sprite slot is empty, fill place in sprite graphics buffer with zeros
                else sprite_graphics_buffer[shift_register_load_pointer - 1] <= 0;

                shift_register_load_pointer <= shift_register_load_pointer + 1;

            end

            else if (shift_register_load_pointer == 9) begin
                rw_sprite_graphics <= 0;
                addr_sprite_graphics <= 0;

                shift_load_sprite <= 1;
                shift_load_data[7:0] <= sprite_graphics_buffer;

                shift_load_background <= 1;
                shift_load_data[8] <= background_line_graphics_buffer[0];
                
                shift_register_load_pointer <= shift_register_load_pointer + 1;

            end
                
            else begin 
                shift_load_sprite <= 0;
                rw_sprite_graphics <= 0;
                addr_sprite_graphics <= 0;
            end
        end

        else begin  
            //Reset vblank and hsync memory pointers
            cords_sprite_load <= 0;
            palette_sprite_load <= 0;
            palette_ram_pointer <= 0;
            background_line_pointer <= 0;
            sprite_graphics_pointer <= 0;
            sprites_on_line_pointer <= 0;
            sprites_found <= 0;
            shift_register_load_pointer <= 0;
            for (int i = 0; i < 8; i += 1) begin
                sprites_on_line_palettes[i] <= 0;
            end
            shift_load_sprite <= 0;
            priority_palette_data_out <= {background_line_palette_buffer[hcount[10:5]], sprites_on_line_palettes};

            //Logic to load new background tile and palette into shift registers
            if (hcount[4:0] == 5'b11111) begin
                
                shift_load_data[8] <= background_line_graphics_buffer[hcount[10:5]];

                shift_load_background <= 1;

            end
            else shift_load_background <= 0;
            
            //Logic to handle pixel doubling
            if (hcount[0]) begin

                shift_enable[8] <= 1;

                //Logic to enable and disable shift registers
                for (int i = 0; i < 8; i += 1) begin
                    if ((sprite_x_buffer[sprites_on_line[i]] >= hcount[10:1]) && ((sprite_x_buffer[sprites_on_line[i]] < hcount[10:1] + 16))) 
                        shift_enable[i] <= 1;
                    else 
                        shift_enable[i] <= 0;
                end
            end else shift_enable <= 0;
            

            //Convert pixel data to colors
            pixel_color <= color_palette_buffer[priority_pixel_data_in + (4 * priority_palette_data_in)];

        end
    end

function automatic logic [31:0] bit_reverse32(input logic [31:0] in);
  for (int i = 0; i < 32; i++)
    bit_reverse32[i] = in[31 - i];
endfunction

endmodule

