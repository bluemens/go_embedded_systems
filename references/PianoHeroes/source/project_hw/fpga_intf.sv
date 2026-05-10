/*
 * Avalon memory-mapped peripheral that generates VGA and l/R Audio
 *
 * Team Piano Heros
 * Columbia University
 *
 * Register map:
 *   0x00 – VGA background red (8-bit)
 *   0x04 – VGA background green (8-bit)
 *   0x08 – VGA background blue (8-bit)
 *   0x0C – Audio note command (32-bit)
 *   0x10 – Audio sample1 value (32-bit)
 *   0x14 – Audio sample2 value (32-bit)
 *   0x18 - Midi input packets (64-bit)
 */
module fpga_intf (
    input  logic         clk,
    input  logic         reset,
    input  logic [31:0]  writedata,
    input  logic         write,
    input  logic         chipselect,
    input  logic [2:0]   address,
    input  logic         advance,

    // Audio Outputs
    output logic [23:0]  leftSample,
    output logic [23:0]  rightSample,

    // VGA Outputs
    output logic [7:0]   VGA_R,
    output logic [7:0]   VGA_G,
    output logic [7:0]   VGA_B,
    output logic         VGA_CLK,
    output logic         VGA_HS,
    output logic         VGA_VS,
    output logic         VGA_BLANK_n,
    output logic         VGA_SYNC_n
    );
    
    // VGA Counters
    logic [10:0] hcount;
    logic [9:0]  vcount;
    
    // VGA Color Registers
    logic [7:0] background_r, background_g, background_b;

    // Audio Driver Control
    logic [1:0]  audio_addr;
    logic        audio_we;
    logic        audio_cs;
    logic [31:0] audio_data;

     // --- MIDI Input Tracking ---
    logic [127:0] active_notes;
    logic [63:0]  midi_input_packet;
    logic [2:0]   midi_byte_count;
    logic [7:0]   status, note, velocity;
    
    // --- Falling Tile Simulation ---
    logic [9:0] tile_y;
    logic [3:0] tile_column;
    logic [23:0] frame_counter;

    
    // VGA Timing Module
    vga_counters counters (
        .clk50      (clk),
        .reset      (reset),
        .hcount     (hcount),
        .vcount     (vcount),
        .VGA_CLK    (VGA_CLK),
        .VGA_HS     (VGA_HS),
        .VGA_VS     (VGA_VS),
        .VGA_BLANK_n(VGA_BLANK_n),
        .VGA_SYNC_n (VGA_SYNC_n)
    );
    
    // Polyphonic Audio Driver Instantiation
    polyphonicDriver #(
        .SAMPLE_LEN(48000),
        .NUM_VOICES(8)
    ) poly_driver (
        .clk        (clk),
        .reset      (reset),
        .advance    (advance),
        .address    (audio_addr),
        .write      (audio_we),
        .chipselect (audio_cs),
        .writedata  (audio_data),
        .leftSample (leftSample),
        .rightSample(rightSample)
    );
   
    // === COMBINATIONAL VGA DRAWING ===
    always_comb begin
        {VGA_R, VGA_G, VGA_B} = {background_r, background_g, background_b};

        if (VGA_BLANK_n) begin
            int key_top    = 360;
            int key_bottom = 480;
            int border     = 2;
            int white_key_width = 42;
            int black_key_width = 38;
            int black_key_height = 60;
            int base_note = 60; // MIDI note C4
            logic [7:0] local_note = 0;
            int x1 = 0;
            int x0 = 0;
            int col = 0;
            int tile_width;
            int tile_x_start;
            int tile_x_end;

            tile_width = 42;
            tile_x_start = 5 + tile_column * tile_width;
            tile_x_end = tile_x_start + tile_width;
 
            // === WHITE KEYS ===
            if (vcount >= key_top && vcount < key_bottom) begin
                for (int i = 0; i < 15; i++) begin
                    x0 = 5 + i * white_key_width;
                    x1 = x0 + white_key_width;
                    local_note = base_note + i;

                    if (hcount >= x0 && hcount < x1) begin
                        if ((hcount - x0 < border) || (x1 - hcount <= border) ||
                            (vcount - key_top < border) || (key_bottom - vcount <= border)) begin
                            {VGA_R, VGA_G, VGA_B} = {8'h00, 8'h00, 8'h00}; // border
                        end else if (active_notes[local_note]) begin
                            {VGA_R, VGA_G, VGA_B} = {8'h00, 8'hFF, 8'h00}; // green = active
                        end else begin
                            {VGA_R, VGA_G, VGA_B} = {8'hFF, 8'hFF, 8'hFF}; // white
                        end
                    end
                end
            end

            // === BLACK KEYS ===
            if (vcount >= key_top && vcount < key_top + black_key_height) begin
                int black_key_cols[10]  = '{0, 1, 3, 4, 5, 7, 8, 10, 11, 12};
                int black_key_notes[10] = '{61, 63, 66, 68, 70, 73, 75, 78, 80, 82};

                for (int j = 0; j < 10; j++) begin
                    col = black_key_cols[j];
                    local_note = black_key_notes[j];
                    x0 = 5 + col * white_key_width + 21;
                    x1 = x0 + black_key_width;

                    if (hcount >= x0 && hcount < x1) begin
                        if (active_notes[local_note]) begin
                            {VGA_R, VGA_G, VGA_B} = {8'h00, 8'hFF, 8'h00}; // green = active
                        end else begin
                            {VGA_R, VGA_G, VGA_B} = {8'h00, 8'h00, 8'h00}; // black default
                        end
                    end
                end
            end

            // === FALLING TILE DEMO ===
            if (vcount >= tile_y && vcount < tile_y + 120) begin
                if (hcount >= tile_x_start && hcount < tile_x_end) begin
                    {VGA_R, VGA_G, VGA_B} = {8'h00, 8'hFF, 8'hFF}; // cyan
                end
            end
        end
    end

    // Write decode logic (VGAs and audio all together)
    always_ff @(posedge clk) begin
        if (reset) begin
            background_r <= 8'h00;
            background_g <= 8'h00;
            background_b <= 8'h80;
            tile_y <= 0;
            tile_column <= 4'd4;
            frame_counter <= 0;
            midi_byte_count <= 0;
            midi_input_packet <= 0;
            active_notes <= 128'd0;
        end else if (chipselect && write) begin
            case (address)
                3'd0: begin
                    background_r <= writedata[7:0];
                    audio_we     <= 1'b0;
                    audio_cs     <= 1'b0;
                end
                3'd1: begin
                    background_g <= writedata[7:0];
                    audio_we     <= 1'b0;
                    audio_cs     <= 1'b0;
                end
                3'd2: begin
                    background_b <= writedata[7:0];
                    audio_we     <= 1'b0;
                    audio_cs     <= 1'b0;
                end
                3'd3: begin // Audio note command
                    audio_we     <= 1'b1;
                    audio_cs     <= 1'b1;
                    audio_addr   <= 2'd0;
                    audio_data   <= writedata;
                end
                3'd4: begin // Audio sample 1
                    audio_we     <= 1'b1;
                    audio_cs     <= 1'b1;
                    audio_addr   <= 2'd1;
                    audio_data   <= writedata;
                end
                3'd5: begin // Audio sample 2
                    audio_we     <= 1'b1;
                    audio_cs     <= 1'b1;
                    audio_addr   <= 2'd2;
                    audio_data   <= writedata;
                end
                3'd6: begin
                        midi_input_packet <= {midi_input_packet[55:0], writedata};
                        midi_byte_count <= midi_byte_count + 1;

                        if (midi_byte_count == 7) begin
                            status   <= midi_input_packet[39:32];
                            note     <= midi_input_packet[31:24];
                            velocity <= midi_input_packet[23:16];

                            if ((midi_input_packet[39:32] & 8'hF0) == 8'h90 && midi_input_packet[23:16] > 0)
                                active_notes[midi_input_packet[31:24]] <= 1'b1;
                            else if ((midi_input_packet[39:32] & 8'hF0) == 8'h80 ||
                                     ((midi_input_packet[39:32] & 8'hF0) == 8'h90 && midi_input_packet[23:16] == 0))
                                active_notes[midi_input_packet[31:24]] <= 1'b0;

                            midi_byte_count <= 0;
                        end
                    end
                default: begin
                    audio_we     <= 1'b0;
                    audio_cs     <= 1'b0;
                end
            endcase
        end else begin
            audio_we <= 1'b0;
            audio_cs <= 1'b0;
        end
            tile_y <= 0;
            tile_column <= 4'd4;
            frame_counter <= 0;
            midi_byte_count <= 0;
            midi_input_packet <= 0;
            active_notes <= 128'd0;
    end


endmodule



//=================================================================================
// Polyphonic Module
//=================================================================================


module polyphonicDriver #(
    parameter SAMPLE_LEN = 48000,
    parameter NUM_VOICES = 8
)(
    input  logic        clk,
    input  logic        reset,

    // Avalon-MM interface
    input  logic [1:0]  address,
    input  logic        write,
    input  logic        chipselect,
    input  logic [31:0] writedata,

    // Audio interface
    input  logic        advance,
    output logic [23:0] leftSample,
    output logic [23:0] rightSample
);

    logic [7:0] note;
    logic cmd;

    //=====================================================
    // Sample Memories (sample1 = C3, sample2 = C4)
    //=====================================================
    logic [15:0] sample1_mem [0:SAMPLE_LEN-1];
    logic [15:0] sample2_mem [0:SAMPLE_LEN-1];
    logic [15:0] sample1_write_ptr, sample2_write_ptr;

    //=====================================================
    // Voice State
    //=====================================================
    typedef struct packed {
        logic        active;
        logic [7:0]  note;
        logic        sample_sel;
        logic [31:0] index;
        logic [31:0] step;
    } voice_t;

    voice_t voices[NUM_VOICES];
    logic signed [23:0] voice_out[NUM_VOICES];

    //=====================================================
    // Pitch Step Lookup
    //=====================================================
    function automatic logic [31:0] pitch_step(input int semitone);
        case (semitone)
            0:   return 32'h00010000;
            1:   return 32'h00010F3B;
            2:   return 32'h00011F5C;
            3:   return 32'h0001306F;
            4:   return 32'h00014289;
            5:   return 32'h000155B5;
            6:   return 32'h00016A09;
            7:   return 32'h00017F91;
            8:   return 32'h00019660;
            9:   return 32'h0001AE8A;
            10:  return 32'h0001C824;
            11:  return 32'h0001E3C3;
            default: return 32'h00010000;
        endcase
    endfunction

    logic signed [23:0] mix;
    int idx0;
    int idx1;
    int idx2;
    int idx3;
    int idx4;
    int idx5;
    int idx6;
    int idx7;

    logic [15:0] s0;
    logic [15:0] s1;
    logic [15:0] s2;
    logic [15:0] s3;
    logic [15:0] s4;
    logic [15:0] s5;
    logic [15:0] s6;
    logic [15:0] s7;

    //=====================================================
    // Command Handling
    //=====================================================
    always_ff @(posedge clk) begin
    if (reset) begin
        leftSample <= 24'd0;
        rightSample <= 24'd0;
        sample1_write_ptr <= 0;
        sample2_write_ptr <= 0;
        voices[0].active <= 0;
        voices[1].active <= 0;
        voices[2].active <= 0;
        voices[3].active <= 0;
        voices[4].active <= 0;
        voices[5].active <= 0;
        voices[6].active <= 0;
        voices[7].active <= 0;
    end else if (write && chipselect) begin
        case (address)
            2'd0: begin // Note Command
                note <= writedata[7:0];
                cmd  <= writedata[8];
                if (cmd) begin // Note On
                    if (!voices[0].active) begin
                        voices[0].active     <= 1;
                        voices[0].note       <= note;
                        voices[0].sample_sel <= (note < 60) ? 0 : 1;
                        voices[0].index      <= 0;
                        voices[0].step       <= pitch_step((note < 60) ? (note - 48) : (note - 60));
                    end else if (!voices[1].active) begin
                        voices[1].active     <= 1;
                        voices[1].note       <= note;
                        voices[1].sample_sel <= (note < 60) ? 0 : 1;
                        voices[1].index      <= 0;
                        voices[1].step       <= pitch_step((note < 60) ? (note - 48) : (note - 60));
                    end else if (!voices[2].active) begin
                        voices[2].active     <= 1;
                        voices[2].note       <= note;
                        voices[2].sample_sel <= (note < 60) ? 0 : 1;
                        voices[2].index      <= 0;
                        voices[2].step       <= pitch_step((note < 60) ? (note - 48) : (note - 60));
                    end else if (!voices[3].active) begin
                        voices[3].active     <= 1;
                        voices[3].note       <= note;
                        voices[3].sample_sel <= (note < 60) ? 0 : 1;
                        voices[3].index      <= 0;
                        voices[3].step       <= pitch_step((note < 60) ? (note - 48) : (note - 60));
                    end else if (!voices[4].active) begin
                        voices[4].active     <= 1;
                        voices[4].note       <= note;
                        voices[4].sample_sel <= (note < 60) ? 0 : 1;
                        voices[4].index      <= 0;
                        voices[4].step       <= pitch_step((note < 60) ? (note - 48) : (note - 60));
                    end else if (!voices[5].active) begin
                        voices[5].active     <= 1;
                        voices[5].note       <= note;
                        voices[5].sample_sel <= (note < 60) ? 0 : 1;
                        voices[5].index      <= 0;
                        voices[5].step       <= pitch_step((note < 60) ? (note - 48) : (note - 60));
                    end else if (!voices[6].active) begin
                        voices[6].active     <= 1;
                        voices[6].note       <= note;
                        voices[6].sample_sel <= (note < 60) ? 0 : 1;
                        voices[6].index      <= 0;
                        voices[6].step       <= pitch_step((note < 60) ? (note - 48) : (note - 60));
                    end else if (!voices[7].active) begin
                        voices[7].active     <= 1;
                        voices[7].note       <= note;
                        voices[7].sample_sel <= (note < 60) ? 0 : 1;
                        voices[7].index      <= 0;
                        voices[7].step       <= pitch_step((note < 60) ? (note - 48) : (note - 60));
                    end
                end else begin // Note Off
                    if (voices[0].active && voices[0].note == note) voices[0].active <= 0;
                    if (voices[1].active && voices[1].note == note) voices[1].active <= 0;
                    if (voices[2].active && voices[2].note == note) voices[2].active <= 0;
                    if (voices[3].active && voices[3].note == note) voices[3].active <= 0;
                    if (voices[4].active && voices[4].note == note) voices[4].active <= 0;
                    if (voices[5].active && voices[5].note == note) voices[5].active <= 0;
                    if (voices[6].active && voices[6].note == note) voices[6].active <= 0;
                    if (voices[7].active && voices[7].note == note) voices[7].active <= 0;
                end
            end
            2'd1: begin // Load Sample1
                sample1_mem[sample1_write_ptr] <= writedata[15:0];
                sample1_write_ptr <= sample1_write_ptr + 1;
            end
            2'd2: begin // Load Sample2
                sample2_mem[sample2_write_ptr] <= writedata[15:0];
                sample2_write_ptr <= sample2_write_ptr + 1;
            end
        endcase
    end
    //=====================================================
    // Voice Playback and Mixing
    //=====================================================
    else if (advance) begin
           
            mix = 24'sd0;
       // ---------------- Voice 0 ----------------
        if (voices[0].active) begin
            idx0 <= voices[0].index[31:16];
            if (idx0 >= SAMPLE_LEN) begin
                voices[0].active <= 1'b0;
                voice_out[0]     <= 24'sd0;
            end
            else begin
                s0              <= voices[0].sample_sel ? sample2_mem[idx0] : sample1_mem[idx0];
                voice_out[0]    <= {{8{s0[15]}}, s0};
                voices[0].index <= voices[0].index + voices[0].step;
            end
        end
        else begin
            voice_out[0] <= 24'sd0;
        end
        mix += voice_out[0];

        // ---------------- Voice 1 ----------------
        if (voices[1].active) begin
            idx1 <= voices[1].index[31:16];
            if (idx1 >= SAMPLE_LEN) begin
                voices[1].active <= 1'b0;
                voice_out[1]     <= 24'sd0;
            end
            else begin
                s1               <= voices[1].sample_sel ? sample2_mem[idx1] : sample1_mem[idx1];
                voice_out[1]    <= {{8{s1[15]}}, s1};
                voices[1].index <= voices[1].index + voices[1].step;
            end
        end
        else begin
            voice_out[1] <= 24'sd0;
        end
        mix += voice_out[1];

        // ---------------- Voice 2 ----------------
        if (voices[2].active) begin
            idx2 <= voices[2].index[31:16];
            if (idx2 >= SAMPLE_LEN) begin
                voices[2].active <= 1'b0;
                voice_out[2]     <= 24'sd0;
            end
            else begin
                s2               <= voices[2].sample_sel ? sample2_mem[idx2] : sample1_mem[idx2];
                voice_out[2]    <= {{8{s2[15]}}, s2};
                voices[2].index <= voices[2].index + voices[2].step;
            end
        end
        else begin
            voice_out[2] <= 24'sd0;
        end
        mix += voice_out[2];

        // ---------------- Voice 3 ----------------
        if (voices[3].active) begin
            idx3 <= voices[3].index[31:16];
            if (idx3 >= SAMPLE_LEN) begin
                voices[3].active <= 1'b0;
                voice_out[3]     <= 24'sd0;
            end
            else begin
                s3               <= voices[3].sample_sel ? sample2_mem[idx3] : sample1_mem[idx3];
                voice_out[3]    <= {{8{s3[15]}}, s3};
                voices[3].index <= voices[3].index + voices[3].step;
            end
        end
        else begin
            voice_out[3] <= 24'sd0;
        end
        mix += voice_out[3];

        // ---------------- Voice 4 ----------------
        if (voices[4].active) begin
            idx4 <= voices[4].index[31:16];
            if (idx4 >= SAMPLE_LEN) begin
                voices[4].active <= 1'b0;
                voice_out[4]     <= 24'sd0;
            end
            else begin
                s4               <= voices[4].sample_sel ? sample2_mem[idx4] : sample1_mem[idx4];
                voice_out[4]    <= {{8{s4[15]}}, s4};
                voices[4].index <= voices[4].index + voices[4].step;
            end
        end
        else begin
            voice_out[4] <= 24'sd0;
        end
        mix += voice_out[4];

        // ---------------- Voice 5 ----------------
        if (voices[5].active) begin
            idx5 <= voices[5].index[31:16];
            if (idx5 >= SAMPLE_LEN) begin
                voices[5].active <= 1'b0;
                voice_out[5]     <= 24'sd0;
            end
            else begin
                s5               <= voices[5].sample_sel ? sample2_mem[idx5] : sample1_mem[idx5];
                voice_out[5]    <= {{8{s5[15]}}, s5};
                voices[5].index <= voices[5].index + voices[5].step;
            end
        end
        else begin
            voice_out[5] <= 24'sd0;
        end
        mix += voice_out[5];

        // ---------------- Voice 6 ----------------
        if (voices[6].active) begin
            idx6 <= voices[6].index[31:16];
            if (idx6 >= SAMPLE_LEN) begin
                voices[6].active <= 1'b0;
                voice_out[6]     <= 24'sd0;
            end
            else begin
                s6               <= voices[6].sample_sel ? sample2_mem[idx6] : sample1_mem[idx6];
                voice_out[6]    <= {{8{s6[15]}}, s6};
                voices[6].index <= voices[6].index + voices[6].step;
            end
        end
        else begin
            voice_out[6] <= 24'sd0;
        end
        mix += voice_out[6];

        // ---------------- Voice 7 ----------------
        if (voices[7].active) begin
            idx7 <= voices[7].index[31:16];
            if (idx7 >= SAMPLE_LEN) begin
                voices[7].active <= 1'b0;
                voice_out[7]     <= 24'sd0;
            end
            else begin
                s7               <= voices[7].sample_sel ? sample2_mem[idx7] : sample1_mem[idx7];
                voice_out[7]    <= {{8{s7[15]}}, s7};
                voices[7].index <= voices[7].index + voices[7].step;
            end
        end
        else begin
            voice_out[7] <= 24'sd0;
        end
        mix += voice_out[7];

        // -----------------------------------------
        // Output mixed sample to left & right
        // -----------------------------------------
        leftSample  <= mix;
        rightSample <= mix;
    end
end
endmodule


//=================================================================================
//VGA MODULE
//=================================================================================

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
