// Dino Run code
module vga_ball(
    input  logic        clk,
    input  logic        reset,
input logic [31:0]  writedata,
    input logic         write,
    input               chipselect,
    input logic [8:0]   address,
    input  logic [7:0]  controller_report,

    output logic [7:0]  VGA_R, VGA_G, VGA_B,
    output logic        VGA_CLK, VGA_HS, VGA_VS, VGA_BLANK_n, VGA_SYNC_n,



    input  logic        L_READY,
input  logic        R_READY,
output logic [15:0] L_DATA,
output logic [15:0] R_DATA,
output logic        L_VALID,
output logic        R_VALID

);

    
    localparam HACTIVE = 11'd1280;
    localparam SCORE_X = 120;
    localparam SCORE_Y = 10;
     logic replay_button;


    initial begin
    $readmemh("background.hex", audio_data);
end

   

//audio sample variables

logic [15:0] audio_data[0:9659];  
logic [17:0] audio_index;
logic [15:0] sample_clock;
logic [15:0] audio_sample;



    // VGA TIMING
    logic [10:0] hcount;
    logic [9:0]  vcount;
      
      //Score as 5-digit BCD 
   localparam int N_DIGITS = 5;
   logic [3:0] bcd [N_DIGITS-1:0];  // [0]=units … [4]=ten-thousands

  logic [16:0]  score;
  logic [3:0]  digit_ten_thou, digit_thou, digit_h, digit_t, digit_u;
    logic [7:0]  font_rom [0:9][0:7];
     // Frame Animation
    logic [23:0] frame_counter;
    logic [1:0]  sprite_state;

    logic [10:0] cloud_offset;
    logic [23:0] cloud_counter;

    logic [7:0] sky_r, sky_g, sky_b;
    logic [23:0] sky_counter;
    logic [3:0]  sky_phase;
     logic [9:0] rx;
    logic [2:0] idx, ry;
    logic [3:0] cx;

 // Sun color
    logic [7:0] sun_r, sun_g, sun_b; //  sun color variables
  
  

    initial begin
        // simple 8×8 font for digits 0–9
        font_rom[0] = '{8'h3C,8'h66,8'h6E,8'h7E,8'h76,8'h66,8'h3C,8'h00};
        font_rom[1] = '{8'h18,8'h38,8'h18,8'h18,8'h18,8'h18,8'h7E,8'h00};
        font_rom[2] = '{8'h3C,8'h66,8'h06,8'h1C,8'h30,8'h66,8'h7E,8'h00};
        font_rom[3] = '{8'h3C,8'h66,8'h06,8'h1C,8'h06,8'h66,8'h3C,8'h00};
        font_rom[4] = '{8'h0C,8'h1C,8'h2C,8'h4C,8'h7E,8'h0C,8'h0C,8'h00};
        font_rom[5] = '{8'h7E,8'h60,8'h7C,8'h06,8'h06,8'h66,8'h3C,8'h00};
        font_rom[6] = '{8'h3C,8'h66,8'h60,8'h7C,8'h66,8'h66,8'h3C,8'h00};
        font_rom[7] = '{8'h7E,8'h06,8'h0C,8'h18,8'h30,8'h30,8'h30,8'h00};
        font_rom[8] = '{8'h3C,8'h66,8'h66,8'h3C,8'h66,8'h66,8'h3C,8'h00};
        font_rom[9] = '{8'h3C,8'h66,8'h66,8'h3E,8'h06,8'h66,8'h3C,8'h00};
    end
    localparam [7:0] FG_R = 8'h00, FG_G = 8'h00, FG_B = 8'h00;  // black digits


    logic [7:0]  a, b, c;
// Night Transition
    logic [39:0] night_timer; // Timer to trigger night time
    logic night_time; // Flag to indicate if it's nighttime
   
    logic [15:0] dino_sprite_output;
    logic [15:0] dino_new_output;
    logic [9:0]  dino_sprite_addr;
    logic [10:0] dino_x = 100, dino_y = 248;
    logic [10:0] godzilla_x, godzilla_y;


    logic ducking, jumping;
    logic [15:0] duck_sprite_output, jump_sprite_output;
    logic [15:0] dino_left_output, dino_right_output;


    // Obstacles
    logic [10:0] s_cac_x = 1200, s_cac_y = 248;
    logic [10:0] group_x = 1600, group_y = 248;
    logic [10:0] lava_x   = 1800, lava_y   = 248;
    logic [10:0] ptr_x    = 1400, ptr_y    = 200;
    logic [10:0] powerup_x, powerup_y;

    logic [10:0] cg_x, cg_y;


    logic [15:0] scac_sprite_output, group_output, lava_output;
    logic [15:0] ptr_up_output, ptr_down_output, ptr_sprite_output;
    logic [9:0]  scac_sprite_addr, lava_sprite_addr, ptr_sprite_addr;
    logic [20:0] group_addr;
    logic [15:0] powerup_sprite_output;
    logic [9:0] powerup_sprite_addr;
  
  logic [15:0] godzilla_sprite_output; 
  logic [9:0] godzilla_sprite_addr;
    // === Replay ===
    logic [15:0] replay_output;
    logic [9:0]  replay_addr;
    logic [10:0] replay_x = 560, replay_y = 200;

    // Motion 
    logic [23:0] motion_timer;
    logic [10:0] obstacle_speed = 1;
    logic [4:0]  passed_count;
    logic        game_over;
   // logic [1:0]  sprite_state;

  // Power-up (Godzilla mode)
logic godzilla_mode;

    logic [39:0] godzilla_timer;
  //lfsr logic for random offset (obstacle positions)
    logic [5:0] lfsr;
    always_ff @(posedge clk or posedge reset) begin
        if (reset) begin
            lfsr <= 6'b101011; // non‐zero seed
        end else if (!game_over && motion_timer >= 24'd2_000_000) begin
            // x^6 + x^5 + 1 polynomial
            lfsr <= { lfsr[4:0], lfsr[5] ^ lfsr[4] };
        end
    end

    function automatic logic is_visible(input logic [15:0] px);
        return (px != 16'hF81F && px != 16'hFFFF);
    endfunction

    function automatic logic collide(
        input logic [10:0] ax, ay, bx, by,
        input logic [5:0]  aw, ah, bw, bh
    );
        return ((ax < bx + bw) && (ax + aw > bx) &&
                (ay < by + bh) && (ay + ah > by));
    endfunction

    always_ff @(posedge clk or posedge reset) begin
        if (reset) begin
            s_cac_x        <= 1200;
            group_x        <= 1600;
            lava_x         <= 1800;
            ptr_x          <= 1400;
            obstacle_speed <= 1;
            passed_count   <= 0;
            game_over      <= 0;
            sprite_state   <= 0;
            motion_timer   <= 0;
            score          <= 0;
            frame_counter <= 0;
             
            cloud_counter <= 0;
            cloud_offset <= 0;
            sky_counter <= 0;
            sky_phase <= 0;
            sky_r <= 8'd135;
            sky_g <= 8'd206;
            sky_b <= 8'd235;
            sun_counter <= 0;
            sun_offset_x <= 0;
            sun_offset_y <= 0;
            night_timer <= 32'd0;
            night_time <= 0; // Start with day
            sky_r <= 8'd135;
            sky_g <= 8'd206;
            sky_b <= 8'd235; // Day sky color
            
            

            score <= 17'd0;
            // Power-up reset
            powerup_x      <= 800;
            powerup_y      <= 248;
            godzilla_mode  <= 0;
            godzilla_timer <= 0;

        end else if (chipselect && write) begin
            case (address)
                9'd0: dino_x <= writedata[9:0];
                9'd1: dino_y <= writedata[9:0];
           
                9'd13: ducking <= writedata[0];
                9'd14: jumping <= writedata[0];
               
                9'd17: lava_x <= writedata[9:0];   
                9'd18: lava_y <= writedata[9:0];
                9'd19: replay_button <= writedata[0]; // trigger replay

            endcase

        end else if (!game_over) begin
            if (motion_timer >= 24'd2_000_000) begin
                // wrap each obstacle with a different pseudo‐random offset
                s_cac_x <= (s_cac_x <= obstacle_speed)
                           ? (HACTIVE + {lfsr,       4'd0})
                           : s_cac_x - obstacle_speed;
                group_x <= (group_x <= obstacle_speed)
                           ? (HACTIVE + {lfsr ^ 6'h3F,4'd0})
                           : group_x - obstacle_speed;
                lava_x  <= (lava_x  <= obstacle_speed)
                           ? (HACTIVE + {{lfsr[3:0]},6'd0})
                           : lava_x  - obstacle_speed;
                ptr_x   <= (ptr_x   <= obstacle_speed)
                           ? (HACTIVE + {{lfsr[5:2]},6'd0})
                           : ptr_x   - obstacle_speed;
                // Power-up movement
powerup_x <= (powerup_x <= obstacle_speed)
             ? (HACTIVE + {{lfsr[4:0]}, 5'd0})
             : powerup_x - obstacle_speed;

                  bcd[0] <= bcd[0] + 1;
               for (int i = 0; i < N_DIGITS-1; i++) begin
                 if (bcd[i] == 4'd10) begin
                   bcd[i]   <= 4'd0;
                   bcd[i+1] <= bcd[i+1] + 1;
                 end
               end
               // wrap highest digit
               if (bcd[N_DIGITS-1] == 4'd10)
                 bcd[N_DIGITS-1] <= 4'd0;
                // tick the score (wrap from 999 back to 0)
score <= (score == 17'd99999) ? 17'd0 : score + 1;                // count passes and speed up
                if (s_cac_x <= obstacle_speed || group_x <= obstacle_speed ||
                    lava_x  <= obstacle_speed || ptr_x   <= obstacle_speed) begin
                    passed_count <= passed_count + 1;
                end
                if (passed_count >= 12) begin
                    obstacle_speed <= obstacle_speed + 1;
                    passed_count   <= 0;
                end

                motion_timer <= 0;
                sprite_state <= sprite_state + 1;
            end else begin
                motion_timer <= motion_timer + 1;
            end

            if (!godzilla_mode &&(collide(dino_x, dino_y, s_cac_x,  s_cac_y,  32,32,32,32) ||
                collide(dino_x, dino_y, group_x,  group_y, 64,32,32,32) ||
                collide(dino_x, dino_y, lava_x,   lava_y,   32,32,32,32) ||
                                  collide(dino_x, dino_y, ptr_x,    ptr_y,    32,32,32,32))) begin
                game_over <= 1;

                
            end

            if (frame_counter == 24'd5_000_000) begin
                sprite_state <= sprite_state + 1;
               
                frame_counter <= 0;
            end else begin
                frame_counter <= frame_counter + 1;
            end

            // Cloud drifting
            if (cloud_counter == 24'd8_000_000) begin
                cloud_counter <= 0;
                cloud_offset <= cloud_offset + 1;
                if (cloud_offset > 1280) cloud_offset <= 0;
            end else begin
                cloud_counter <= cloud_counter + 1;
            end


       
          
         // Night Timer Logic 
            if (night_timer < 40'd1_500_000_000) begin
            night_timer <= night_timer + 1;  // Increment the timer
         end else if (night_timer == 40'd1_500_000_000) begin
            night_time <= ~night_time;
             night_timer <= 32'd0;
         end
            if (night_time) begin
                sky_r <= 8'd10;  // Dark blue night sky
                sky_g <= 8'd10;
                sky_b <= 8'd40;
             
            sun_r <= 8'd255; // White moon
            sun_g <= 8'd255;
            sun_b <= 8'd255;
            end else begin
                sky_r <= 8'd135;
                sky_g <= 8'd206;
                sky_b <= 8'd235; // Day sky color
                sun_r <= 8'd255; // Yellow sun
                sun_g <= 8'd255;
                sun_b <= 8'd0;
            end
     
            if (collide(dino_x, dino_y, powerup_x, powerup_y, 32, 32, 32, 32)) begin
    godzilla_mode <= 1;
    godzilla_timer <= 0;
    powerup_x <= 2000; // move off screen
end
            //Godzilla destroys 
if (godzilla_mode) begin
    if (collide(dino_x, dino_y, s_cac_x, s_cac_y, 32, 32, 32, 32))
        s_cac_x <= 2000;
    if (collide(dino_x, dino_y, group_x, group_y, 64, 32, 32, 32))
        group_x <= 2000;
    if (collide(dino_x, dino_y, lava_x, lava_y, 32, 32, 32, 32))
        lava_x <= 2000;
    if (collide(dino_x, dino_y, ptr_x, ptr_y, 32, 32, 32, 32))
        ptr_x <= 2000;
end





//Godzilla timeout
if (godzilla_mode)
    godzilla_timer <= godzilla_timer + 1;

if (godzilla_timer >= 32'd100_000_000_000) begin
    godzilla_mode <= 0;
    godzilla_timer <= 0;
end
        end else begin
            // on replay, reset everything
             if (replay_button) begin
                s_cac_x        <= 1200;
                group_x        <= 1600;
                lava_x         <= 1800;
                ptr_x          <= 1400;
                obstacle_speed <= 1;
                passed_count   <= 0;
                game_over      <= 0;
                score          <= 0;
                motion_timer   <= 0;
                godzilla_mode  <= 0;
                godzilla_timer <= 0;
                powerup_x      <= 800;
                powerup_y      <= 248;
                for (int i = 0; i < N_DIGITS; i++) begin
                bcd[i] <= 4'd0;
                end
              

            end
        end
    end

    // VGA COUNTERS
    vga_counters counters(
        .clk50     (clk),
        .reset     (reset),
        .hcount    (hcount),
        .vcount    (vcount),
        .VGA_CLK   (VGA_CLK),
        .VGA_HS    (VGA_HS),
        .VGA_VS    (VGA_VS),
        .VGA_BLANK_n(VGA_BLANK_n),
        .VGA_SYNC_n(VGA_SYNC_n)
    );

    // SPRITE ROMS
    dino_s_cac_rom       s_cac_rom(.clk(clk), .address(scac_sprite_addr), .data(scac_sprite_output));
    dino_cac_tog_rom     cacti_group_rom(.clk(clk), .address(group_addr),      .data(group_output));
    dino_lava_rom        lava_rom(.clk(clk),   .address(lava_sprite_addr),  .data(lava_output));
    dino_pterodactyl_down_rom  ptero_up(.clk(clk), .address(ptr_sprite_addr), .data(ptr_up_output));
    dino_pterodactyl_up_rom    ptero_down(.clk(clk), .address(ptr_sprite_addr), .data(ptr_down_output));

    
   

   always_comb begin
    if (godzilla_mode)
        dino_sprite_output = godzilla_sprite_output;
    else if (ducking)
        dino_sprite_output = duck_sprite_output;
    else if (jumping)
        dino_sprite_output = jump_sprite_output;
    else begin
        case (sprite_state)
            2'd0: dino_sprite_output = dino_new_output;
            2'd1: dino_sprite_output = dino_left_output;
            2'd2: dino_sprite_output = dino_right_output;
            default: dino_sprite_output = dino_new_output;
        endcase
    end
end


    dino_sprite_rom dino_rom(.clk(clk), .address(dino_sprite_addr), .data(dino_new_output));

    dino_duck_rom duck_rom(.clk(clk), .address(dino_sprite_addr), .data(duck_sprite_output));
    dino_jump_rom jump_rom(.clk(clk), .address(dino_sprite_addr), .data(jump_sprite_output));
    dino_left_leg_up_rom dino_rom1(.clk(clk), .address(dino_sprite_addr), .data(dino_left_output));
    dino_right_leg_up_rom dino_rom2(.clk(clk), .address(dino_sprite_addr), .data(dino_right_output));


   
    dino_replay_rom replay_rom(.clk(clk), .address(replay_addr), .data(replay_output));

      dino_powerup_rom powerup_rom(.clk(clk), .address(powerup_sprite_addr), .data(powerup_sprite_output));

      dino_godzilla_rom godzilla_rom(.clk(clk), .address(godzilla_sprite_addr), .data(godzilla_sprite_output));



    // Pterodactyl animation
    always_comb begin
        case (sprite_state)
            2'd0: ptr_sprite_output = ptr_up_output;
            2'd1: ptr_sprite_output = ptr_down_output;
            default: ptr_sprite_output = ptr_up_output;
        endcase
    end



   
always_ff @(posedge clk) begin
    a <= 8'd135; b <= 8'd206; c <= 8'd235;

    if (sample_clock >= 285) begin  // 50MHz / 175550 ≈ 285
    sample_clock <= 0;
    audio_sample <= audio_data[audio_index];

    if (audio_index == 9659)
        audio_index <= 0;
    else
        audio_index <= audio_index + 1;
end else begin
    sample_clock <= sample_clock + 1;
end

if (L_READY) begin
    L_DATA  <= audio_sample;
    L_VALID <= (sample_clock == 0);
end else begin
    L_VALID <= 0;
end

if (R_READY) begin
    R_DATA  <= audio_sample;
    R_VALID <= (sample_clock == 0);
end else begin
    R_VALID <= 0;
end





    if (!game_over ) begin
//begin
     
   

    if (vcount < 280) begin
        a <= sky_r;
        b <= sky_g;
        c <= sky_b;
    end else if (vcount > 300) begin
        a <= 8'd100; 
        b <= 8'd40;
        c <= 8'd10;
    end else begin
        a <= 8'd139; 
        b <= 8'd69;
        c <= 8'd19;
        
    end

    //Ground Line 
    if (vcount == 280) begin
        a <= 8'd0;
        b <= 8'd0;
        c <= 8'd0;
    end

    
    if ((hcount-(1150-sun_offset_x))*(hcount-(1150-sun_offset_x)) +
        (vcount-(80+sun_offset_y))*(vcount-(80+sun_offset_y)) < 1200 &&
        (hcount-(1150-sun_offset_x))*(hcount-(1150-sun_offset_x)) +
        (vcount-(80+sun_offset_y))*(vcount-(80+sun_offset_y)) > 900) begin
        a <= sun_r;
        b <= sun_g;
        c <= sun_b;
    end
     if ((hcount-(1150-sun_offset_x))*(hcount-(1150-sun_offset_x)) +
        (vcount-(80+sun_offset_y))*(vcount-(80+sun_offset_y)) < 900) begin
        a <= sun_r;
        b <= sun_g;
        c <= sun_b;
    end

    //
    // --- Cloud 1 ---
    if (((hcount-(235+cloud_offset))*(hcount-(235+cloud_offset)) + (vcount-70)*(vcount-70) < 100) ||
        ((hcount-(245+cloud_offset))*(hcount-(245+cloud_offset)) + (vcount-65)*(vcount-65) < 100) ||
        ((hcount-(255+cloud_offset))*(hcount-(255+cloud_offset)) + (vcount-65)*(vcount-65) < 100) ||
        ((hcount-(245+cloud_offset))*(hcount-(245+cloud_offset)) + (vcount-75)*(vcount-75) < 100) ||
        ((hcount-(255+cloud_offset))*(hcount-(255+cloud_offset)) + (vcount-75)*(vcount-75) < 100) ||
        ((hcount-(265+cloud_offset))*(hcount-(265+cloud_offset)) + (vcount-70)*(vcount-70) < 100) ||

        ((hcount-(235+cloud_offset-1280))*(hcount-(235+cloud_offset-1280)) + (vcount-70)*(vcount-70) < 100) ||
        ((hcount-(245+cloud_offset-1280))*(hcount-(245+cloud_offset-1280)) + (vcount-65)*(vcount-65) < 100) ||
        ((hcount-(255+cloud_offset-1280))*(hcount-(255+cloud_offset-1280)) + (vcount-65)*(vcount-65) < 100) ||
        ((hcount-(245+cloud_offset-1280))*(hcount-(245+cloud_offset-1280)) + (vcount-75)*(vcount-75) < 100) ||
        ((hcount-(255+cloud_offset-1280))*(hcount-(255+cloud_offset-1280)) + (vcount-75)*(vcount-75) < 100) ||
        ((hcount-(265+cloud_offset-1280))*(hcount-(265+cloud_offset-1280)) + (vcount-70)*(vcount-70) < 100)) begin
        a <= 8'd255;
        b <= 8'd255;
        c <= 8'd255;
    end
        // --- Cloud 2 ---
    if (((hcount-(440+cloud_offset))*(hcount-(440+cloud_offset)) + (vcount-100)*(vcount-100) < 100) ||
        ((hcount-(450+cloud_offset))*(hcount-(450+cloud_offset)) + (vcount-95)*(vcount-95) < 100) ||
        ((hcount-(460+cloud_offset))*(hcount-(460+cloud_offset)) + (vcount-95)*(vcount-95) < 100) ||
        ((hcount-(440+cloud_offset))*(hcount-(440+cloud_offset)) + (vcount-105)*(vcount-105) < 100) ||
        ((hcount-(450+cloud_offset))*(hcount-(450+cloud_offset)) + (vcount-110)*(vcount-110) < 100) ||
        ((hcount-(460+cloud_offset))*(hcount-(460+cloud_offset)) + (vcount-105)*(vcount-105) < 100) ||

        ((hcount-(440+cloud_offset-1280))*(hcount-(440+cloud_offset-1280)) + (vcount-100)*(vcount-100) < 100) ||
        ((hcount-(450+cloud_offset-1280))*(hcount-(450+cloud_offset-1280)) + (vcount-95)*(vcount-95) < 100) ||
        ((hcount-(460+cloud_offset-1280))*(hcount-(460+cloud_offset-1280)) + (vcount-95)*(vcount-95) < 100) ||
        ((hcount-(440+cloud_offset-1280))*(hcount-(440+cloud_offset-1280)) + (vcount-105)*(vcount-105) < 100) ||
        ((hcount-(450+cloud_offset-1280))*(hcount-(450+cloud_offset-1280)) + (vcount-110)*(vcount-110) < 100) ||
        ((hcount-(460+cloud_offset-1280))*(hcount-(460+cloud_offset-1280)) + (vcount-105)*(vcount-105) < 100)) begin
        a <= 8'd250;
        b <= 8'd250;
        c <= 8'd250;
    end
        // --- Cloud 3 ---
    if (((hcount-(690+cloud_offset))*(hcount-(690+cloud_offset)) + (vcount-60)*(vcount-60) < 100) ||
        ((hcount-(700+cloud_offset))*(hcount-(700+cloud_offset)) + (vcount-55)*(vcount-55) < 100) ||
        ((hcount-(710+cloud_offset))*(hcount-(710+cloud_offset)) + (vcount-55)*(vcount-55) < 100) ||
        ((hcount-(690+cloud_offset))*(hcount-(690+cloud_offset)) + (vcount-65)*(vcount-65) < 100) ||
        ((hcount-(700+cloud_offset))*(hcount-(700+cloud_offset)) + (vcount-70)*(vcount-70) < 100) ||
        ((hcount-(710+cloud_offset))*(hcount-(710+cloud_offset)) + (vcount-65)*(vcount-65) < 100) ||

        ((hcount-(690+cloud_offset-1280))*(hcount-(690+cloud_offset-1280)) + (vcount-60)*(vcount-60) < 100) ||
        ((hcount-(700+cloud_offset-1280))*(hcount-(700+cloud_offset-1280)) + (vcount-55)*(vcount-55) < 100) ||
        ((hcount-(710+cloud_offset-1280))*(hcount-(710+cloud_offset-1280)) + (vcount-55)*(vcount-55) < 100) ||
        ((hcount-(690+cloud_offset-1280))*(hcount-(690+cloud_offset-1280)) + (vcount-65)*(vcount-65) < 100) ||
        ((hcount-(700+cloud_offset-1280))*(hcount-(700+cloud_offset-1280)) + (vcount-70)*(vcount-70) < 100) ||
        ((hcount-(710+cloud_offset-1280))*(hcount-(710+cloud_offset-1280)) + (vcount-65)*(vcount-65) < 100)) begin
        a <= 8'd245;
        b <= 8'd245;
        c <= 8'd245;
    end

    // Tiny Birds
    if (((hcount > 300 && hcount < 305) && (vcount == 50)) ||
        ((hcount > 305 && hcount < 310) && (vcount == 51)) ||
        ((hcount > 310 && hcount < 315) && (vcount == 50)) ||
        ((hcount > 600 && hcount < 605) && (vcount == 80)) ||
        ((hcount > 605 && hcount < 610) && (vcount == 81)) ||
        ((hcount > 610 && hcount < 615) && (vcount == 80))) begin
        a <= 8'd0;
        b <= 8'd0;
        c <= 8'd0;
    end
         //  Ground Rocks 
    if (vcount > 280 && vcount < 480) begin
        if ((hcount % 120 == 0 && vcount % 50 < 10) ||
            (hcount % 200 == 15 && vcount % 60 < 8)) begin
            a <= 8'd110;
            b <= 8'd50;
            c <= 8'd10;
        end
    end

        
        // Power-up sprite drawing
if (hcount >= powerup_x && hcount < powerup_x + 32 &&
    vcount >= powerup_y && vcount < powerup_y + 32) begin
    powerup_sprite_addr <= (hcount - powerup_x) + ((vcount - powerup_y) * 32);
    if (is_visible(powerup_sprite_output)) begin
        a <= {powerup_sprite_output[15:11], 3'b000};
        b <= {powerup_sprite_output[10:5],  2'b00};
        c <= {powerup_sprite_output[4:0],   3'b000};
    end
end
    
        if (hcount >= dino_x && hcount < dino_x + 32 && vcount >= dino_y && vcount < dino_y + 32) begin
    if (godzilla_mode)
        godzilla_sprite_addr <= (hcount - dino_x) + ((vcount - dino_y) * 32);
    else
        dino_sprite_addr <= (hcount - dino_x) + ((vcount - dino_y) * 32);

    if (is_visible(dino_sprite_output)) begin
        a <= {dino_sprite_output[15:11], 3'b000};
        b <= {dino_sprite_output[10:5],  2'b00};
        c <= {dino_sprite_output[4:0],   3'b000};
    end
end

        if (hcount >= s_cac_x && hcount < s_cac_x + 32 && vcount >= s_cac_y && vcount < s_cac_y + 32) begin
            scac_sprite_addr <= (hcount - s_cac_x) + ((vcount - s_cac_y) * 32);
            if (is_visible(scac_sprite_output)) begin
                a <= {scac_sprite_output[15:11], 3'b000};
                b <= {scac_sprite_output[10:5],  2'b00};
                c <= {scac_sprite_output[4:0],   3'b000};
            end
        end
        if (hcount >= group_x && hcount < group_x + 64 && vcount >= group_y && vcount < group_y + 32) begin
            group_addr <= (hcount - group_x) + ((vcount - group_y) * 64);
            if (is_visible(group_output)) begin
                a <= {group_output[15:11], 3'b000};
                b <= {group_output[10:5],  2'b00};
                c <= {group_output[4:0],   3'b000};
            end
        end
        if (hcount >= lava_x && hcount < lava_x + 32 && vcount >= lava_y && vcount < lava_y + 32) begin
            lava_sprite_addr <= (hcount - lava_x) + ((vcount - lava_y) * 32);
            if (is_visible(lava_output)) begin
                a <= {lava_output[15:11], 3'b000};
                b <= {lava_output[10:5],  2'b00};
                c <= {lava_output[4:0],   3'b000};
            end
        end
        if (hcount >= ptr_x && hcount < ptr_x + 32 && vcount >= ptr_y && vcount < ptr_y + 32) begin
            ptr_sprite_addr <= (31 - (hcount - ptr_x)) + ((vcount - ptr_y) * 32);
            if (is_visible(ptr_sprite_output)) begin
                a <= {ptr_sprite_output[15:11], 3'b000};
                b <= {ptr_sprite_output[10:5],  2'b00};
                c <= {ptr_sprite_output[4:0],   3'b000};
            end
        end
    
      if (vcount >= SCORE_Y && vcount < SCORE_Y + 8) begin
    if (hcount >= SCORE_X && hcount < (SCORE_X + N_DIGITS * 8)) begin
        rx  = hcount - SCORE_X;
        idx = rx / 8;          // Each digit is 8 pixels wide
        cx  = rx % 8;
        ry  = vcount - SCORE_Y;

        if (idx < N_DIGITS && cx < 8) begin
            if (font_rom[bcd[N_DIGITS - 1 - idx]][ry][7 - cx]) begin
                a <= FG_R;
                b <= FG_G;
                c <= FG_B;
            end
        end
    end
end


    end else begin
        if (hcount >= replay_x && hcount < replay_x + 160 && vcount >= replay_y && vcount < replay_y + 32) begin
            replay_addr <= (hcount - replay_x) + ((vcount - replay_y) * 160);
            if (is_visible(replay_output)) begin
                a <= {replay_output[15:11], 3'b000};
                b <= {replay_output[10:5],  2'b00};
                c <= {replay_output[4:0],   3'b000};
            end
        end
         
    end
    
end

assign {VGA_R, VGA_G, VGA_B} = {a, b, c};

endmodule

 module vga_counters(
    input  logic        clk50, reset,
    output logic [10:0] hcount,
    output logic [9:0]  vcount,
    output logic        VGA_CLK, VGA_HS, VGA_VS, VGA_BLANK_n, VGA_SYNC_n
);

   parameter HACTIVE = 11'd1280,
             HFRONT = 11'd32,
             HSYNC  = 11'd192,
             HBACK  = 11'd96,
             HTOTAL = HACTIVE + HFRONT + HSYNC + HBACK;

   parameter VACTIVE = 10'd480,
             VFRONT = 10'd10,
             VSYNC  = 10'd2,
             VBACK  = 10'd33,
             VTOTAL = VACTIVE + VFRONT + VSYNC + VBACK;

   logic endOfLine;
   always_ff @(posedge clk50 or posedge reset)
      if (reset)
         hcount <= 0;
      else if (endOfLine)
         hcount <= 0;
      else
         hcount <= hcount + 1;

   assign endOfLine = (hcount == HTOTAL - 1);

   logic endOfField;
   always_ff @(posedge clk50 or posedge reset)
      if (reset)
         vcount <= 0;
      else if (endOfLine)
         if (endOfField)
            vcount <= 0;
         else
            vcount <= vcount + 1;

   assign endOfField = (vcount == VTOTAL - 1);

   assign VGA_HS = !((hcount >= (HACTIVE + HFRONT)) && (hcount < (HACTIVE + HFRONT + HSYNC)));
   assign VGA_VS = !((vcount >= (VACTIVE + VFRONT)) && (vcount < (VACTIVE + VFRONT + VSYNC)));
   assign VGA_SYNC_n = 1'b0;
   assign VGA_BLANK_n = (hcount < HACTIVE) && (vcount < VACTIVE);
   assign VGA_CLK = hcount[0];

   endmodule
