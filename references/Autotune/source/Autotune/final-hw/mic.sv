module mic (
    input  logic        reset,        // Reset signal
    input  logic        i2s_sd1,           // Microphone data output
    input  logic        i2s_btn_record,   // Active-high button to control recording
    input  logic        SCK,          // Serial clock for mic (2048kHz)
    output logic     [6:0]   i2s_hex,
    output logic        i2s_ws,           // Word select for mic (512kHz)
    output logic [15:0] sample_data,           // Output to FIFO
    output logic        sample_valid,  // Sample valid signal
    output logic        out_startofpacket, // Start of packet signal
    output logic        out_endofpacket,    // End of packet signal
    output logic        sck_new
);

    // Internal signals
    logic [5:0]   clk_cnt;            // 64 counter to generate i2s_ws signal
    logic [23:0]  shift_reg;          // Shift register for incoming data
    logic [15:0]  sample;             // Truncated sample
    logic         wr_en;              // Write enable (sample ready)
    logic         recording;          // Are we currently recording?
    logic         sck_rst;            // SCK reset signal
    logic         start_capture;      // Signal to start capturing data
    logic [1:0]   frame_cnt;          // Counter for i2s_ws frames
    logic [10:0]  packet_cnt;         // Counter for packet boundaries (0-2047)

    assign sck_new = SCK;
   assign i2s_hex = sample_data [6:0];

    // Control recording state
    always_ff @(posedge SCK or posedge reset) begin
        if (reset) begin
            recording <= 0;
            sck_rst <= 1;
        end else begin
            recording <= i2s_btn_record;  // Direct button control
            
            // Reset SCK counter when not recording
            if (!recording)
                sck_rst <= 1;
            else
                sck_rst <= 0;
        end
    end

    // i2s_ws clock generator (64 division) and start_capture control
    always_ff @(posedge SCK) begin
        if (sck_rst) begin
            clk_cnt <= 6'd0;
            i2s_ws <= 0;
            start_capture <= 0;
            frame_cnt <= 0;
        end else begin
            clk_cnt <= clk_cnt + 6'd1;

            if (clk_cnt == 31) begin
                i2s_ws <= 1;
            end else if (clk_cnt == 63) begin
                i2s_ws <= 0;
            end

            // At the beginning of each frame (clk_cnt == 0), increment frame counter
            if (clk_cnt == 0) begin
                frame_cnt <= frame_cnt + 2'd1;

                // Only capture once every 4 i2s_ws frames
                if (frame_cnt == 2'd3) begin
                    start_capture <= 1;
                end else begin
                    start_capture <= 0;
                end
            end else if (clk_cnt == 24) begin
                start_capture <= 0;
            end
        end
    end

    // I2S data capture (shift in on rising edge of SCK)
    always_ff @(posedge SCK) begin
        if (sck_rst) begin
            shift_reg <= 0;
            wr_en <= 0;
        end else begin
            // Capture data when start_capture is high and i2s_ws is low
            if (start_capture && !i2s_ws) begin
                shift_reg <= {shift_reg[22:0], i2s_sd1};
                if (clk_cnt == 23) begin  // After 24 bits are captured
                    // Only set wr_en if we have valid data (check if shift_reg is not all zeros)
                    if (shift_reg != 0) begin
                        sample <= shift_reg[23:8];
                        wr_en <= 1;
                    end else begin
                        wr_en <= 0;
                    end
                end else begin
                    wr_en <= 0;
                end
            end else begin
                wr_en <= 0;
            end
        end
    end

    // Output sample data to FIFO
    always_ff @(posedge SCK or posedge reset) begin
        if (reset) begin
            sample_data <= 0;
            sample_valid <= 0;
            out_startofpacket <= 0;
            out_endofpacket <= 0;
            packet_cnt <= 0;
        end else if (recording) begin
            if (wr_en) begin
                sample_data <= sample;
                sample_valid <= 1;
                
                // Packet boundary logic
                if (packet_cnt == 0) begin
                    out_startofpacket <= 1;
                    out_endofpacket <= 0;
                    packet_cnt <= packet_cnt + 1;  // Increment counter here
                end else if (packet_cnt == 2047) begin
                    out_startofpacket <= 0;
                    out_endofpacket <= 1;
                    packet_cnt <= 0;  // Reset counter for next period
                end else begin
                    out_startofpacket <= 0;
                    out_endofpacket <= 0;
                    packet_cnt <= packet_cnt + 1;
                end
            end else begin
                sample_valid <= 0;
                out_startofpacket <= 0;
                out_endofpacket <= 0;
            end
        end else begin
            sample_valid <= 0;
            out_startofpacket <= 0;
            out_endofpacket <= 0;
            packet_cnt <= 0;  // Reset counter when not recording
        end
    end

endmodule
