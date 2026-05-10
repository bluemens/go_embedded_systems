/*
Modified version of tiles.sv. Original version provided by Professor Stephen Edwards
https://www.cs.columbia.edu/~sedwards/classes/2025/4840-spring/tiles.pdf
*/

module tiles
  (input logic         VGA_CLK, VGA_RESET,
   output logic [7:0]  VGA_R, VGA_G, VGA_B,
   output logic        VGA_HS, VGA_VS, VGA_BLANK_n,

   input logic 	       mem_clk,         // Clock for memory ports
   
   input logic [12:0]  tm_address,      // Tilemap memory port
   input logic 	       tm_we,
   input logic [7:0]   tm_din,
   output logic [7:0]  tm_dout,

   input logic [13:0]  ts_address,      // Tileset memory port
   input logic 	       ts_we,
   input logic [3:0]   ts_din,
   output logic [3:0]  ts_dout,

   input logic [3:0]   palette_address, // Palette memory port
   input logic 	       palette_we,
   input logic [23:0]  palette_din,
   output logic [23:0] palette_dout);
   
   logic [9:0] 	       hcount;          // From counters
   logic [8:0] 	       vcount;

   logic [2:0] 	       hcount1;         // Pipeline registers
   logic 	       VGA_HS0, VGA_HS1, VGA_HS2;
   logic 	       VGA_BLANK_n0, VGA_BLANK_n1, VGA_BLANK_n2;	       
   
   logic [7:0] 	       tilenumber;      // Memory outputs
   logic [3:0] 	       colorindex;

   /* verilator lint_off UNUSED */
   logic               unconnected; // Extra vcount bit from counters
   /* verilator lint_on UNUSED */

   // Frame-snapshot helper signals 
   logic [12:0] copy_addr;   //8 KiB tile map
   logic        copying;     //Actively copying
   logic        copy_we;     //write-enable for the display side cache
   logic [7:0]  tm_cpu_dout; //Data coming from the CPU side RAM to the display side cache

   logic        copying_d1;
   logic [12:0] copy_addr_d1;

   always_ff @(posedge VGA_CLK) begin
      copying_d1   <= copying;
      copy_addr_d1 <= copy_addr;
   end

   assign copy_we = copying_d1;
   
   vga_counters cntrs(.vcount( {unconnected, vcount} ), // VGA Counters
		      .VGA_BLANK_n( VGA_BLANK_n0 ),
		      .VGA_HS( VGA_HS0 ),
		      .*);

   //CPU side tile RAM 
   //port-A (clk = mem_clk) from Avalon bus
   //port-B (clk = VGA_CLK) to frame cacher 
   twoportbram #(.DATA_BITS(8), .ADDRESS_BITS(13)) tilemap_cpu (
      .clk1  ( mem_clk ),
      .addr1 ( tm_address ),
      .we1   ( tm_we ),
      .din1  ( tm_din ),
      .dout1 ( tm_dout ),

      .clk2  ( VGA_CLK ),
      .addr2 ( copy_addr ),
      .we2   ( 1'b0 ),
      .din2  ( 8'hxx ),
      .dout2 ( tm_cpu_dout )
   );

   //Display side tile cache
   //port-A (clk = VGA_CLK) from pixel pipeline
   //port-B (clk = VGA_CLK) to frame cacher (writes)
   twoportbram #(.DATA_BITS(8), .ADDRESS_BITS(13)) tilemap_disp (
      .clk1  ( VGA_CLK ),
      .addr1 ( { vcount[8:3], hcount[9:3] } ),
      .we1   ( 1'b0 ),
      .din1  ( 8'hxx ),
      .dout1 ( tilenumber ),                

      .clk2  ( VGA_CLK ),
      .addr2 ( copy_addr_d1 ),
      .we2   ( copy_we ),
      .din2  ( tm_cpu_dout ),
      .dout2 ( )
   );

   //Per-frame cacher (runs during vblank)
   //starts at the first pixel of vblank (vcount = 480, hcount = 0)
   always_ff @(posedge VGA_CLK or posedge VGA_RESET) begin
      if (VGA_RESET) begin
         copying   <= 1'b0;
         copy_addr <= 13'd0;
      end else begin
         if (!copying && hcount == 10'd0 && vcount == 10'd480) begin //start of vblank
            //start caching
            copying   <= 1'b1;
            copy_addr <= 13'd0;
         end else if (copying) begin
            copy_addr <= copy_addr + 13'd1;
            if (copy_addr == 13'd8191) //finished copying tile map into the cache
               copying <= 1'b0;
         end
      end
   end

   
   always_ff @(posedge VGA_CLK)                     // Pipeline registers
     { hcount1, VGA_BLANK_n1, VGA_HS1 } <=
       { hcount[2:0], VGA_BLANK_n0, VGA_HS0 };
      
   twoportbram #(.DATA_BITS(4), .ADDRESS_BITS(14))  // Tile Set
   tileset(.clk1  ( VGA_CLK ), .clk2 ( mem_clk ),
	   .addr1 ( { tilenumber, vcount[2:0], hcount1 } ),
	   .we1   ( 1'b0 ), .din1( 4'hx), .dout1( colorindex ),
	   .addr2 ( ts_address ),
	   .we2   ( ts_we ), .din2( ts_din ), .dout2( ts_dout ));   

   always_ff @(posedge VGA_CLK)                     // Pipeline registers
     { VGA_BLANK_n2, VGA_HS2 } <= { VGA_BLANK_n1, VGA_HS1 };

   twoportbram #(.DATA_BITS(24), .ADDRESS_BITS(4))  // Palette
   palette(.clk1  ( VGA_CLK ), .clk2 ( mem_clk ),
	   .addr1 ( colorindex ),
	   .we1   ( 1'b0 ), .din1( 24'hx), .dout1( { VGA_B, VGA_G, VGA_R } ),
	   .addr2 ( palette_address ),
	   .we2   ( palette_we ), .din2( palette_din ), .dout2( palette_dout ));

   always_ff @(posedge VGA_CLK)                     // Pipeline registers
     { VGA_BLANK_n, VGA_HS } <= { VGA_BLANK_n2, VGA_HS2 };
   
endmodule
	       
   
