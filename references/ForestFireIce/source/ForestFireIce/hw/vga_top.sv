module vga_top(input logic        clk,
                   input logic 	   reset,
                   input logic [31:0]  writedata,
                   input logic 	   write,
                   input 		   chipselect,
                   input logic [5:0]  address, // 64

                   output logic [31:0] readdata,
                   output logic [7:0] VGA_R, VGA_G, VGA_B,
                   output logic 	   VGA_CLK, VGA_HS, VGA_VS,
                   VGA_BLANK_n,
                   output logic 	   VGA_SYNC_n,
                   
                   output logic [2:0] audio_ctrl);

    // current VGA pixel coord
    logic [10:0]	   hcount;
    logic [9:0]     vcount;

    logic [31:0] status_reg;
    logic [31:0] ctrl_reg;

    assign status_reg[19:0] = {hcount[10:1], vcount};
    // linebuffer
    // addr
    logic [5:0] addr_tile_disp;
    logic [9:0] addr_pixel_disp;
    logic [5:0] addr_tile_draw;
    logic [9:0] addr_pixel_draw;

    // indata
    logic [255:0] data_tile_disp;
    logic [15:0]  data_pixel_disp;
    logic [255:0] data_tile_draw;
    logic [15:0]  data_pixel_draw;

    // wren
    logic wren_tile_disp;
    logic wren_pixel_disp;
    logic wren_tile_draw;
    logic wren_pixel_draw;

    // outdata
    logic [255:0] q_tile_disp;
    logic [15:0]  q_pixel_disp;
    logic [255:0] q_tile_draw;
    logic [15:0]  q_pixel_draw;

    logic switch;

    linebuffer u_linebuffer (.*);

    // connection between tile_engine and linebuffer
    assign addr_tile_draw = tile_col;
    assign data_tile_draw = tile_data;

	// tile engine
	logic tile_start;
	// output declaration of module tile_engine
	logic[5:0] tile_col;
	logic [255:0] tile_data;
	logic tile_done;
	
	tile_engine u_tile_engine(
		.clk         	(clk          ),
		.reset       	(reset        ),
		.tile_start  	(tile_start   ),
		.tilemap_idx 	(ctrl_reg[1:0]  ),
		.vcount      	(vcount       ),
		.tile_col    	(tile_col     ),
		.tile_data   	(tile_data    ),
		.tile_done   	(tile_done    ),
        .wren_tile_draw (wren_tile_draw)
	);
	

	// sprite engine
    logic sprite_start;
    logic sprite_done;

    logic sprite_write_reg;
    logic [4:0] sprite_wr_idx;
    logic [31:0] sprite_writedata;

    always_ff @(posedge clk) begin
        if (reset) begin
            sprite_write_reg <= 0;
            sprite_wr_idx    <= 0;
            sprite_writedata <= 0;
        end else begin
        // latch data to keep stable
        if (chipselect && write && address[5]) begin
            sprite_write_reg <= 1;
            sprite_wr_idx    <= address[4:0];
            sprite_writedata <= writedata;
        end else begin
                sprite_write_reg <= 0;
            end
        end
    end
    sprite_engine u_sprite_engine(
        .clk         	(clk          ),
        .reset       	(reset        ),
        .sprite_start  	(sprite_start   ),
        .vcount      	(vcount       ),
        .spr_wr_en    	(sprite_write_reg     ),
        .spr_wr_idx   	(sprite_wr_idx    ),
        .spr_wr_data   	(sprite_writedata    ),
        .sprite_pixel_col (addr_pixel_draw),
        .sprite_pixel_data (data_pixel_draw),
        .wren_pixel_draw (wren_pixel_draw),
        .done (sprite_done)
    );



    vga_counters counters(.clk50(clk), .*);

    always_ff @(posedge clk) begin
        if (reset) begin
			// status_reg <= 0;
			ctrl_reg <= 0;
			tile_start <= 0;
			sprite_start <= 0;
            wren_tile_disp <= 0;
            wren_pixel_disp <= 0;

            switch <= 0;

            audio_ctrl <= 0;
        end
        else begin
            if (vcount < 479 || vcount == 524) begin
                if (hcount == 0) begin
                    tile_start <= 1;
                end else begin
                    tile_start <= 0;
                end     
                // sprite start
                // 60 clk enough to draw tile
                if (hcount == 60 && tile_done && sprite_done) begin
                    sprite_start <= 1;
                end 
                
                if (sprite_start) begin // 1 cycle pulse
                    sprite_start <= 0;
                end

                // 1 cycle flip "switch", 1 cycle read "switch" to "disp_sel",1 cycle read memory
                // more cycles to insure robust
                if (hcount == 1590 && tile_done && sprite_done)
                    switch <= ~switch;
            end

            if (chipselect) begin
                if (write) begin
                    case (address)
                        6'h0: begin
                            ctrl_reg <= writedata;
                            // audio part
                            audio_ctrl <= writedata[31:29];
                        end
                    endcase
                end
                else begin // read
                    case (address)
                        6'h1: readdata <= status_reg;
                    endcase
                end
            end

        end
    end
 
    // 1 cycle delay
    assign addr_pixel_disp = hcount[10:1] < 639 ? hcount[10:1] + 1 : 0;
    // MSB is transparency bit ( 1 = transparent, 0 = no)
    assign VGA_R = q_pixel_disp[14:10] << 3;
    assign VGA_G = q_pixel_disp[9:5] << 3;
    assign VGA_B = q_pixel_disp[4:0] << 3;
    

endmodule

module vga_counters(
        input logic 	     clk50, reset,
        output logic [10:0] hcount,  // hcount[10:1] is pixel column
        output logic [9:0]  vcount,  // vcount[9:0] is pixel row
        output logic 	     VGA_CLK, VGA_HS, VGA_VS, VGA_BLANK_n, VGA_SYNC_n);

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
    // Parameters for hcount
    parameter HACTIVE      = 11'd 1280,
              HFRONT_PORCH = 11'd 32,
              HSYNC        = 11'd 192,
              HBACK_PORCH  = 11'd 96,
              HTOTAL       = HACTIVE + HFRONT_PORCH + HSYNC +
              HBACK_PORCH; // 1600

    // Parameters for vcount
    parameter VACTIVE      = 10'd 480,
              VFRONT_PORCH = 10'd 10,
              VSYNC        = 10'd 2,
              VBACK_PORCH  = 10'd 33,
              VTOTAL       = VACTIVE + VFRONT_PORCH + VSYNC +
              VBACK_PORCH; // 525

    logic endOfLine;

    always_ff @(posedge clk50 or posedge reset)
        if (reset)
            hcount <= 0;
        else if (endOfLine)
            hcount <= 0;
        else
            hcount <= hcount + 11'd 1;

    assign endOfLine = hcount == HTOTAL - 1;

    logic endOfField;

    always_ff @(posedge clk50 or posedge reset)
        if (reset)
            vcount <= 0;
        else if (endOfLine)
            if (endOfField)
                vcount <= 0;
            else
                vcount <= vcount + 10'd 1;

    assign endOfField = vcount == VTOTAL - 1;

    // Horizontal sync: from 0x520 to 0x5DF (0x57F)
    // 101 0010 0000 to 101 1101 1111
    assign VGA_HS = !( (hcount[10:8] == 3'b101) &
                       !(hcount[7:5] == 3'b111));
    assign VGA_VS = !( vcount[9:1] == (VACTIVE + VFRONT_PORCH) / 2);

    assign VGA_SYNC_n = 1'b0; // For putting sync on the green signal; unused

    // Horizontal active: 0 to 1279     Vertical active: 0 to 479
    // 101 0000 0000  1280	       01 1110 0000  480
    // 110 0011 1111  1599	       10 0000 1100  524
    assign VGA_BLANK_n = !( hcount[10] & (hcount[9] | hcount[8]) ) &
           !( vcount[9] | (vcount[8:5] == 4'b1111) );

    /* VGA_CLK is 25 MHz
     *             __    __    __
     * clk50    __|  |__|  |__|
     *        
     *             _____       __
     * hcount[0]__|     |_____|
     */
    assign VGA_CLK = hcount[0]; // 25 MHz clock: rising edge sensitive

endmodule
