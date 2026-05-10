module vga_ball(
    input logic        clk,
    input logic        reset,
    input logic [7:0]  writedata,
    input logic        write,
    input              chipselect,
    input logic [2:0]  address,

    output logic [7:0] VGA_R, VGA_G, VGA_B,
    output logic       VGA_CLK, VGA_HS, VGA_VS,
                       VGA_BLANK_n,
    output logic       VGA_SYNC_n
);

    // Game state definition
    typedef enum logic [1:0] {
        WAITING,   // Before first game starts
        PLAYING,   // Game is active
        GAME_OVER  // Bird has hit something
    } game_state_t;

    logic [10:0] hcount;
    logic [9:0]  vcount;

    logic [9:0] bird_y;
    logic [9:0] new_y;
    logic [1:0] bird_frame;
    logic [23:0] animation_counter;

    logic [18:0] bg_addr;
    logic [7:0]  bg_color;

    logic [11:0] bird_addr;
    logic [7:0]  bird_color;

    logic [9:0] scroll_offset;
    logic [23:0] scroll_counter;

    logic [7:0] bird_color_reg;
    logic collision;
	 logic game_over;
    
    logic [15:0] score;
    game_state_t game_state;
	 
    // gameover parameters
    parameter GAMEOVER_WIDTH = 192;
    parameter GAMEOVER_HEIGHT = 42;
    parameter GAMEOVER_X = 320 - GAMEOVER_WIDTH/2;
    parameter GAMEOVER_Y = 240 - GAMEOVER_HEIGHT/2;
    
    logic [15:0] gameover_addr;
    logic [7:0]  gameover_color;

    // === Score Display Start ===
    localparam DIGIT_WIDTH  = 16;
    localparam DIGIT_HEIGHT = 32;
    localparam SEG_THICK    = 4;
    localparam SCORE_X0     = 10;
    localparam SCORE_Y0     = 10;
    localparam SCORE_X1     = SCORE_X0 + DIGIT_WIDTH + 4;
    localparam SCORE_Y1     = SCORE_Y0;
	 
    // BCD digits
    logic [3:0] digit0, digit1;
    // seven-segment decode signals {A,B,C,D,E,F,G}
    logic [6:0] seg0, seg1;
    // 用于七段显示的像素输出
    logic        score_pixel;

    parameter BIRD_X = 100;
    parameter BIRD_WIDTH = 34;
    parameter BIRD_HEIGHT = 24;
    
    parameter GRAVITY = 1;
    parameter FLAP_STRENGTH = -7;
    parameter TEST_INTERVAL = 50_000_000;
    
    logic signed [9:0] bird_velocity;
    logic        vsync_reg, flap_latched;
    logic [31:0] test_counter;

    vga_counters counters (
        .clk50(clk),
        .reset(reset),
        .hcount(hcount),
        .vcount(vcount),
        .VGA_CLK(VGA_CLK),
        .VGA_HS(VGA_HS),
        .VGA_VS(VGA_VS),
        .VGA_BLANK_n(VGA_BLANK_n),
        .VGA_SYNC_n(VGA_SYNC_n)
    );
	 
    bg_rom    bg_rom_inst (.address(bg_addr), .clock(clk), .q(bg_color));
    bird_rom0 bird0       (.address(bird_addr), .clock(clk), .q(bird_color0));
    bird_rom1 bird1       (.address(bird_addr), .clock(clk), .q(bird_color1));
    bird_rom2 bird2       (.address(bird_addr), .clock(clk), .q(bird_color2));
	 
	 gameover_rom gameover_inst (.address(gameover_addr), .clock(clk), .q(gameover_color));
	 

    logic [7:0] bird_color0, bird_color1, bird_color2;
    
    function automatic bit in_rect(
        input int x0, input int y0,
        input int W,  input int H,
        input logic [10:0] hc,
        input logic [ 9:0] vc
    );
        in_rect = (hc >= x0 && hc < x0 + W &&
                   vc >= y0 && vc < y0 + H);
    endfunction

    always_ff @(posedge clk) begin
        case (bird_frame)
            2'd0: bird_color_reg <= bird_color0;
            2'd1: bird_color_reg <= bird_color1;
            2'd2: bird_color_reg <= bird_color2;
            default: bird_color_reg <= bird_color0;
        endcase
    end

    always_comb begin
        logic [9:0] bg_col;
		  bg_col = (hcount[10:1] + scroll_offset) % 640;
		  bg_addr = vcount * 640 + bg_col;
		  /*
        bg_col = (hcount[10:1] + scroll_offset);
        if(bg_col < 640)
            bg_addr = vcount * 640 + bg_col;
        else
            bg_addr = vcount * 640 + (bg_col - 640);
			*/

        if (hcount[10:1] >= BIRD_X && hcount[10:1] < BIRD_X + BIRD_WIDTH &&
            vcount >= bird_y && vcount < bird_y + BIRD_HEIGHT)
            bird_addr = (vcount - bird_y) * BIRD_WIDTH + (hcount[10:1] - BIRD_X);
        else
            bird_addr = 0;
		
		  if (in_rect(GAMEOVER_X, GAMEOVER_Y, GAMEOVER_WIDTH, GAMEOVER_HEIGHT, hcount[10:1], vcount))
            gameover_addr = (vcount - GAMEOVER_Y) * GAMEOVER_WIDTH + (hcount[10:1] - GAMEOVER_X);
        else
            gameover_addr = 0;
    end

    //管道结构参数（周日晚调试，左侧柱子左边触边缘 就立刻消失）
    parameter PIPE_WIDTH = 52;
    parameter GAP_HEIGHT = 100;      
    parameter PIPE_COUNT = 3;
    parameter PIPE_SPACING = 213; //前一条柱子最左边到前一条柱子最右边，实际间隔127-52 = 75

    typedef struct packed {
        logic [9:0] x; 
        logic [8:0] gap_y;
    } pipe_t;

    pipe_t pipes[PIPE_COUNT];

    integer i;
    logic [9:0] max_pipe_x;

    // === LFSR 随机数生成 ===
    logic [7:0] lfsr;
    logic       lfsr_enable;
    

    always_ff @(posedge clk or posedge reset) begin
        if (reset)
            lfsr <= 8'h5A;
        else if (lfsr_enable)
            lfsr <= {lfsr[6:0], lfsr[7] ^ lfsr[5]};
    end

    always_ff @(posedge clk or posedge reset) begin
        if (reset) begin
            bird_y <= 240;
            bird_velocity <= 0;
            vsync_reg <= 1'b0;
            flap_latched <= 1'b0;
            test_counter <= 32'd0;
            
            bird_frame <= 0;
            animation_counter <= 0;
            scroll_offset <= 0;
            scroll_counter <= 0;
            score <= 16'd0;
            game_state <= WAITING;
				
            for (i = 0; i < PIPE_COUNT; i = i + 1) begin
                pipes[i].x <= 640 + i * PIPE_SPACING;
                pipes[i].gap_y <= 150 + i * 40;
            end
        end else begin
            // Vertical sync edge detection
            vsync_reg <= VGA_VS;
            
            // Handle keyboard input (flapping)
            if (chipselect && write && address == 3'h7) begin
                // Register flap command from processor
                flap_latched <= writedata[0];
            end else if (VGA_VS && !vsync_reg && flap_latched) begin
                // Clear flap signal after frame update
                flap_latched <= 0;
            end
            
            // Animation counter for bird wings always updates
				if (game_state != GAME_OVER) begin
					animation_counter <= animation_counter + 1;
					if (animation_counter == 24'd5_000_000) begin
						 animation_counter <= 0;
						 bird_frame <= (bird_frame == 2) ? 0 : bird_frame + 1;
					end
				end
            
            // Background always scrolls
				if (game_state != GAME_OVER) begin
					scroll_counter <= scroll_counter + 1;
					if (scroll_counter == 24'd500_000) begin
						scroll_offset <= (scroll_offset + 1) % 640;
						/*
						 scroll_offset <= scroll_offset + 1;
						 if (scroll_offset >= 640) begin
							  scroll_offset <= 0;
						 end
						 */
						 scroll_counter <= 0;
					end
				end
            
            // Game state specific logic
            case (game_state)
                WAITING: begin
                    // Bird stays in the middle
                    bird_y <= 240;
                    bird_velocity <= 0;
                    
                    // Reset score
                    score <= 0;
                    
                    // Reset pipes (initialize offscreen)
                    for (i = 0; i < PIPE_COUNT; i = i + 1) begin
                        pipes[i].x <= 440 + i * PIPE_SPACING;
                        pipes[i].gap_y <= 150 + i * 20;
                    end
                    
                    // Start game on flap
                    if (flap_latched) begin
                        game_state <= PLAYING;
                        bird_velocity <= FLAP_STRENGTH; // Initial upward velocity
                    end
                end
                
                PLAYING: begin
                    // Bird physics - update on vsync
                    if (VGA_VS && !vsync_reg) begin
								
                        // Flap or apply gravity
                        if (flap_latched) begin 
									bird_velocity <= FLAP_STRENGTH; // Upward velocity
                        end else begin
                            bird_velocity <= bird_velocity + GRAVITY;
                        end
                        
                        // Update position
                        new_y = bird_y + bird_velocity;
                        
                        // Boundary checks
                        if (new_y < 0) begin
                            bird_y <= 0;
                        end else if (new_y >= 440 - BIRD_HEIGHT) begin
                            bird_y <= 440 - BIRD_HEIGHT;
                            bird_velocity <= 0;
                            game_state <= GAME_OVER; // Hit ground
                        end else begin
                            bird_y <= new_y;
                        end
                    
                        // Find rightmost pipe
                        max_pipe_x = 0;
                        for (i = 0; i < PIPE_COUNT; i = i + 1)
                            if (pipes[i].x > max_pipe_x)
                                max_pipe_x = pipes[i].x;
                        
                        // Update score when passing pipes
                        for (i = 0; i < PIPE_COUNT; i = i + 1) begin
                            //if (pipes[i].x + PIPE_WIDTH == BIRD_X) begin
                                //score <= score + 1;
                            //end
									 if (pipes[i].x + PIPE_WIDTH < BIRD_X && pipes[i].x + PIPE_WIDTH >= BIRD_X - 2) begin
										  score <= score + 1;
									 end
                        end
                        
                        // Move pipes and recycle them
                        for (i = 0; i < PIPE_COUNT; i = i + 1) begin
                            pipes[i].x <= pipes[i].x - 2; // Faster pipe movement
                            
                            if (pipes[i].x <= 1) begin
                                pipes[i].x <= max_pipe_x + PIPE_SPACING;
                                // Update random number generator for gap position
                                lfsr_enable <= 1;
                                pipes[i].gap_y <= 80 + (lfsr % 160); // Random gap position
                            end
                        end
                    end else begin
                        lfsr_enable <= 0;
                    end
                    
                    // Check for collisions
                    if (collision) begin
                        game_state <= GAME_OVER;
                    end
                end
                
                GAME_OVER: begin
                    // Game over - bird stops, pipes stop
                    bird_velocity <= 0;
						  
						  bird_y <= bird_y;
                    
                    // Wait for flap to restart
                    if (flap_latched) begin
                        game_state <= WAITING;
								flap_latched <= 0;
                    end
                end
            endcase
        end
    end

    logic [15:0] ground_addr;
    logic [7:0]  ground_color;

    base_rom ground_inst (.address(ground_addr), .clock(clk), .q(ground_color));

    always_comb begin
        if (vcount >= 440 && vcount < 480)
            ground_addr = (vcount - 440) * 640 + ((hcount[10:1] + scroll_offset) % 640);
        else
            ground_addr = 0;
    end

    logic pipe_pixel;
    
    // === BCD Conversion ===
    always_comb begin
        digit0 = (score / 10) % 10;         
        digit1 = score % 10;  
    end 

    // === Seven-Segment Decode ===
    always_comb begin
        case (digit0)
            4'd0: seg0 = 7'b1111110;
            4'd1: seg0 = 7'b0110000;
            4'd2: seg0 = 7'b1101101;
            4'd3: seg0 = 7'b1111001;
            4'd4: seg0 = 7'b0110011;
            4'd5: seg0 = 7'b1011011;
            4'd6: seg0 = 7'b1011111;
            4'd7: seg0 = 7'b1110000;
            4'd8: seg0 = 7'b1111111;
            4'd9: seg0 = 7'b1111011;
            default: seg0 = 7'b0000000;
        endcase
        case (digit1)
            4'd0: seg1 = 7'b1111110;
            4'd1: seg1 = 7'b0110000;
            4'd2: seg1 = 7'b1101101;
            4'd3: seg1 = 7'b1111001;
            4'd4: seg1 = 7'b0110011;
            4'd5: seg1 = 7'b1011011;
            4'd6: seg1 = 7'b1011111;
            4'd7: seg1 = 7'b1110000;
            4'd8: seg1 = 7'b1111111;
            4'd9: seg1 = 7'b1111011;
            default: seg1 = 7'b0000000;
        endcase
    end
    
    // === Score Pixel Generation ===
    always_comb begin
        score_pixel = 1'b0;
        // Digit0 segments
        if (seg0[6] && in_rect(SCORE_X0 + SEG_THICK, SCORE_Y0,
                             DIGIT_WIDTH - 2*SEG_THICK, SEG_THICK,
                             hcount[10:1], vcount))
            score_pixel = 1;
        // Other segment checks...
        // (keeping the existing segment rendering code)
        if (seg0[5] && in_rect(SCORE_X0 + DIGIT_WIDTH - SEG_THICK, SCORE_Y0 + SEG_THICK,
                                SEG_THICK, DIGIT_HEIGHT/2 - SEG_THICK,
                                hcount[10:1], vcount))
            score_pixel = 1;
        if (seg0[4] && in_rect(SCORE_X0 + DIGIT_WIDTH - SEG_THICK, SCORE_Y0 + DIGIT_HEIGHT/2,
                                SEG_THICK, DIGIT_HEIGHT/2 - SEG_THICK,
                                hcount[10:1], vcount))
            score_pixel = 1;
        if (seg0[3] && in_rect(SCORE_X0 + SEG_THICK, SCORE_Y0 + DIGIT_HEIGHT - SEG_THICK,
                                DIGIT_WIDTH - 2*SEG_THICK, SEG_THICK,
                                hcount[10:1], vcount))
            score_pixel = 1;
        if (seg0[2] && in_rect(SCORE_X0, SCORE_Y0 + DIGIT_HEIGHT/2,
                                SEG_THICK, DIGIT_HEIGHT/2 - SEG_THICK,
                                hcount[10:1], vcount))
            score_pixel = 1;
        if (seg0[1] && in_rect(SCORE_X0, SCORE_Y0 + SEG_THICK,
                                SEG_THICK, DIGIT_HEIGHT/2 - SEG_THICK,
                                hcount[10:1], vcount))
            score_pixel = 1;
        if (seg0[0] && in_rect(SCORE_X0 + SEG_THICK, SCORE_Y0 + DIGIT_HEIGHT/2 - SEG_THICK/2,
                                DIGIT_WIDTH - 2*SEG_THICK, SEG_THICK,
                                hcount[10:1], vcount))
            score_pixel = 1;
        
        // Digit1 segments
        // (keeping the existing digit1 segment rendering code)
        if (seg1[6] && in_rect(SCORE_X1 + SEG_THICK, SCORE_Y1,
                                DIGIT_WIDTH - 2*SEG_THICK, SEG_THICK,
                                hcount[10:1], vcount))
            score_pixel = 1;
        if (seg1[5] && in_rect(SCORE_X1 + DIGIT_WIDTH - SEG_THICK, SCORE_Y1 + SEG_THICK,
                                SEG_THICK, DIGIT_HEIGHT/2 - SEG_THICK,
                                hcount[10:1], vcount))
            score_pixel = 1;
        if (seg1[4] && in_rect(SCORE_X1 + DIGIT_WIDTH - SEG_THICK, SCORE_Y1 + DIGIT_HEIGHT/2,
                                SEG_THICK, DIGIT_HEIGHT/2 - SEG_THICK,
                                hcount[10:1], vcount))
            score_pixel = 1;
        if (seg1[3] && in_rect(SCORE_X1 + SEG_THICK, SCORE_Y1 + DIGIT_HEIGHT - SEG_THICK,
                                DIGIT_WIDTH - 2*SEG_THICK, SEG_THICK,
                                hcount[10:1], vcount))
            score_pixel = 1;
        if (seg1[2] && in_rect(SCORE_X1, SCORE_Y1 + DIGIT_HEIGHT/2,
                                SEG_THICK, DIGIT_HEIGHT/2 - SEG_THICK,
                                hcount[10:1], vcount))
            score_pixel = 1;
        if (seg1[1] && in_rect(SCORE_X1, SCORE_Y1 + SEG_THICK,
                                SEG_THICK, DIGIT_HEIGHT/2 - SEG_THICK,
                                hcount[10:1], vcount))
            score_pixel = 1;
        if (seg1[0] && in_rect(SCORE_X1 + SEG_THICK, SCORE_Y1 + DIGIT_HEIGHT/2 - SEG_THICK/2,
                                DIGIT_WIDTH - 2*SEG_THICK, SEG_THICK,
                                hcount[10:1], vcount))
            score_pixel = 1;
    end
    
    // Pipe pixel detection
    always_comb begin
        pipe_pixel = 0;
        // Only show pipes when not in WAITING state
        if (game_state != WAITING) begin
            for (int j = 0; j < PIPE_COUNT; j = j + 1) begin
                if (hcount[10:1] >= pipes[j].x && hcount[10:1] < pipes[j].x + PIPE_WIDTH) begin
                    if ((vcount < pipes[j].gap_y || vcount > pipes[j].gap_y + GAP_HEIGHT) &&
                        vcount < 440)
                        pipe_pixel = 1;
                end
            end
        end
    end

    // Collision detection logic
    always_comb begin
        collision = 0; 
        
        // Only check collisions in PLAYING state
        if (game_state == PLAYING) begin
            // Upper boundary collision
            //if (bird_y < 0)
            //    collision = 1;
                
            // Ground collision
            if (bird_y + BIRD_HEIGHT > 440)
                collision = 1;
                
            // Pipe collisions
            for (int j = 0; j < PIPE_COUNT; j = j + 1) begin
                // Check X overlap
                if (BIRD_X + BIRD_WIDTH > pipes[j].x &&
                    BIRD_X < pipes[j].x + PIPE_WIDTH) begin
                    
                    // Check Y overlap (not in gap)
                    if ((bird_y < pipes[j].gap_y && bird_y + BIRD_HEIGHT > 0) ||
                        bird_y + BIRD_HEIGHT > pipes[j].gap_y + GAP_HEIGHT)
                        collision = 1;
                end
            end
        end
    end
  
    // Rendering logic
    always_comb begin
        {VGA_R, VGA_G, VGA_B} = 24'h000000;

        if (VGA_BLANK_n) begin
            // Render score (highest priority)
				if (game_state == GAME_OVER && in_rect(GAMEOVER_X, GAMEOVER_Y, GAMEOVER_WIDTH, GAMEOVER_HEIGHT, hcount[10:1], vcount) && 
                gameover_color != 8'h00) begin
                VGA_R = {gameover_color[7:5], 5'b00000};
                VGA_G = {gameover_color[4:2], 5'b00000};
                VGA_B = {gameover_color[1:0], 6'b000000};
				//rendering score
            end else if (score_pixel) begin
                VGA_R = 8'hFF;
                VGA_G = 8'hFF;
                VGA_B = 8'hFF;
            end
            // Render bird
            else if (hcount[10:1] >= BIRD_X && hcount[10:1] < BIRD_X + BIRD_WIDTH &&
                     vcount >= bird_y && vcount < bird_y + BIRD_HEIGHT &&
                     bird_color_reg != 8'h00) begin
                VGA_R = {bird_color_reg[7:5], 5'b00000};
                VGA_G = {bird_color_reg[4:2], 5'b00000};
                VGA_B = {bird_color_reg[1:0], 6'b000000};
            end 
            // Render pipes (only if not in WAITING state)
            else if (pipe_pixel) begin
                VGA_R = 8'h00;
                VGA_G = 8'hFF;
                VGA_B = 8'h00;
            end 
            // Render ground
            else if (vcount >= 440 && vcount < 480) begin
                VGA_R = {ground_color[7:5], 5'b00000};
					 VGA_G = {ground_color[4:2], 5'b00000};
					 VGA_B = {ground_color[1:0], 6'b000000};
            end 
            // Render background
            else begin
                VGA_B = {bg_color[7:5], 5'b00000};
                VGA_G = {bg_color[4:2], 5'b00000};
                VGA_R = {bg_color[1:0], 6'b000000};
            end
            
				/*
            // Add a red tint in GAME_OVER state
            if (game_state == GAME_OVER) begin
                VGA_R = VGA_R | 8'h40; // Add some red tint
            end
				*/
        end
    end
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
     if (reset)          hcount <= 0;
     else if (endOfLine) hcount <= 0;
     else  	         hcount <= hcount + 11'd 1;

   assign endOfLine = hcount == HTOTAL - 1;
       
   logic endOfField;
   
   always_ff @(posedge clk50 or posedge reset)
     if (reset)          vcount <= 0;
     else if (endOfLine)
       if (endOfField)   vcount <= 0;
       else              vcount <= vcount + 10'd 1;

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
