`define BGM_BEGIN 16'h0
`define BGM_END 16'hAF8A
/*
  bgm.mp3: 0x0 to 0xAF8A
  death.mp3: 0xAF8B to 0xC4F5
  jumpfb.mp3: 0xC4F6 to 0xCA7E
  jumpwg.mp3: 0xCA7F to 0xD5BE
 
audio_ctrl[2:0]
[2] is bgm-start, [1:0] is sound selection
 
Reference: spring 2024 Bubble Bobble
*/
module audio_play(input logic        clk,
                      input logic 	   reset,

                      input left_chan_ready,
                      input right_chan_ready,

                      input logic [2:0] audio_ctrl,

                      output logic [15:0] sample_data_l,
                      output logic sample_valid_l,
                      output logic [15:0] sample_data_r,
                      output logic sample_valid_r);

    logic [15:0] sound_begin_addresses [3:0] = '{16'h0 ,16'hAF8B, 16'hC4F6, 16'hCA7F};
    logic [15:0] sound_end_addresses [3:0]   = '{16'h0 ,16'hC4F5, 16'hCA7E, 16'hD5BE};

    logic [15:0] sound_address;
    logic [15:0] sound_end_address;
    logic [15:0] bgm_address; // loop
    logic [7:0] sound_data;
    logic [7:0] bgm_data;

    logic left_busy;
    logic right_busy;

    logic bgm_playing;
    logic sfx_playing;

    logic [2:0] audio_ctrl_prev;

    audio_rom u_audio_rom(
                  .address_a 	(bgm_address),
                  .address_b  (sound_address),
                  .clock   	(clk    ),
                  .q_a       	(bgm_data),
                  .q_b        (sound_data)
              );



    assign sample_data_l = bgm_data << 8;
    assign sample_data_r = sound_data << 8;

    always_ff @(posedge clk) begin
        if (reset) begin
            sample_valid_l <= 0;
            sample_valid_r <= 0;
            left_busy <= 0;
            right_busy <= 0;
            sound_address <= `BGM_BEGIN;
            bgm_address <= `BGM_BEGIN;
            sound_end_address <= `BGM_END;
            bgm_playing <= 0;
            sfx_playing <= 0;
            audio_ctrl_prev <= 0;
        end
        else begin
            if (audio_ctrl[2] != audio_ctrl_prev[2]) begin
                audio_ctrl_prev[2] <= audio_ctrl[2];
                if (audio_ctrl[2]) begin
                    bgm_playing <= 1;
                    bgm_address <= `BGM_BEGIN;
                    if(!sfx_playing) begin
                        sound_address <= `BGM_BEGIN;
                        sound_end_address <= `BGM_END;
                    end
                end
                else begin
                    bgm_playing <= 0;
                end
            end
            if (audio_ctrl[1:0] != audio_ctrl_prev[1:0]) begin
                audio_ctrl_prev[1:0] <= audio_ctrl[1:0];
                if (audio_ctrl[1:0] !=0) begin
                    sound_address <= sound_begin_addresses[audio_ctrl[1:0]];
                    sound_end_address <= sound_end_addresses[audio_ctrl[1:0]];
                    sfx_playing <= 1;
                end
                else begin
                    sfx_playing <= 0;
                end
            end
            if (bgm_playing || sfx_playing) begin
                if (left_chan_ready == 1 && right_chan_ready == 1) begin
                    if(left_busy == 0 && right_busy == 0) begin
                        if(sound_address >= sound_end_address) begin
                            if(sfx_playing) begin
                                sound_address <= bgm_address;
                                sound_end_address <= `BGM_END;
                                sfx_playing <= 0;
                            end
                            else begin
                                sound_address <= bgm_address;
                                sound_end_address <= `BGM_END;
                            end
                        end
                        else begin
                            sound_address <= sound_address + 1;
                        end

                        if (bgm_address >= `BGM_END) begin
                            bgm_address <= `BGM_BEGIN;
                        end
                        else begin
                            bgm_address <= bgm_address + 1;
                        end
                    end
                    left_busy <= 1;
                    right_busy <= 1;
                    sample_valid_l <= 1;
                    sample_valid_r <= 1;
                end
                else if (left_chan_ready == 0 && right_chan_ready == 0) begin
                    left_busy <= 0;
                    right_busy <= 0;
                    sample_valid_l <= 0;
                    sample_valid_r <= 0;
                end
            end
            else begin
                sample_valid_l <= 0;
                sample_valid_r <= 0;
            end

        end
    end

endmodule


