`timescale 1ns/1ps

module testbench();
    // Clock and reset signals
    logic reset;
    
    // Microphone interface signals
    logic i2s_sd1;
    logic SCK;
    logic i2s_ws;
    logic i2s_btn_record;
    
    // Output signals
    logic [15:0] sample_data;
    logic sample_valid;
    logic out_startofpacket;
    logic out_endofpacket;

    // Constants
    localparam SCK_PERIOD = 488;  // 2.048MHz = 488.28125ns period
    localparam HOLD_TIME = 20;    // 20ns hold time after WS

    // Counters for tracking packets
    integer sample_count = 0;
    integer packet_count = 0;
    integer frame_count = 0;

    // Instantiate the module under test
    mic dut (
        .reset(reset),
        .i2s_sd1(i2s_sd1),
        .i2s_btn_record(i2s_btn_record),
        .SCK(SCK),
        .i2s_ws(i2s_ws),
        .sample_data(sample_data),
        .sample_valid(sample_valid),
        .out_startofpacket(out_startofpacket),
        .out_endofpacket(out_endofpacket)
    );

    // Generate SCK
    initial begin
        SCK = 0;
        forever #(SCK_PERIOD/2) SCK = ~SCK;
    end

    // Monitor signals for debugging (reduced frequency to avoid too much output)
    integer monitor_count = 0;
    always @(posedge SCK) begin
        if (sample_valid) begin
            monitor_count++;
            if (monitor_count % 100 == 0 || out_startofpacket || out_endofpacket) begin
                $display("Time=%t Sample=%d sample_valid=%b sample_data=%h SOP=%b EOP=%b", 
                         $time, monitor_count, sample_valid, sample_data, out_startofpacket, out_endofpacket);
            end
        end
    end

    // Track packet boundaries
    always @(posedge SCK) begin
        if (sample_valid) begin
            sample_count++;
            if (out_startofpacket) begin
                packet_count++;
                $display("*** PACKET %d START at time %t (sample %d) ***", packet_count, $time, sample_count);
            end
            if (out_endofpacket) begin
                $display("*** PACKET %d END at time %t (sample %d) ***", packet_count, $time, sample_count);
                $display("    Packet contained %d samples", sample_count - (packet_count - 1) * 2048);
            end
        end
    end

    // Test stimulus
    initial begin
        $display("Simulation started at time %t", $time);
        $display("Target: 2090 samples (one complete packet + 42 samples from next packet)");
        
        // Initialize signals
        reset = 1;
        i2s_sd1 = 0;
        i2s_btn_record = 0;
        SCK = 0;
        sample_count = 0;
        packet_count = 0;
        frame_count = 0;
        monitor_count = 0;
        
        // Reset for 100ns to ensure proper initialization
        #100;
        reset = 0;
        $display("Reset released at time %t", $time);
        
        // Wait for 5 SCK cycles after reset
        repeat(5) @(negedge SCK);
        
        // Start recording
        i2s_btn_record = 1;
        $display("Recording started at time %t", $time);
        
        // Wait for 10 SCK cycles to let WS stabilize
        repeat(10) @(negedge SCK);
        
        // Check WS state
        $display("WS state after 10 SCK cycles: %b", i2s_ws);
        
        // Since we capture 1 sample every 4 frames, we need 2090 * 4 = 8360 frames
        // to get 2090 samples
        repeat(8360) begin
            // Wait for WS to go low
            @(negedge i2s_ws);
            frame_count++;
            
            // Progress reporting every 1000 frames
            if (frame_count % 1000 == 0) begin
                $display("Progress: Frame %d, Samples captured: %d", frame_count, sample_count);
            end
            
            // Wait for one complete SCK cycle after WS goes low
            @(posedge SCK);
            @(negedge SCK);
            
            // Sample 24 bits of SD data at every SCK cycle while WS is low
            repeat(24) begin
                // Change SD at falling edge of SCK
                @(negedge SCK);
                i2s_sd1 = $random;    // Random data for testing
            end
            
            // Set SD low after sampling 24 bits
            @(negedge SCK);
            i2s_sd1 = 0;
            
            // Wait for remaining SCK cycles in the low period
            repeat(7) @(negedge SCK);  // 32 - 24 - 1 = 7 remaining cycles
            
            // Wait for WS to go high
            @(posedge i2s_ws);
            
            // Stop when we reach 2090 samples
            if (sample_count >= 2090) begin
                $display("Reached target of 2090 samples, stopping...");
                break;
            end
        end
        
        // Stop recording
        i2s_btn_record = 0;
        $display("Recording stopped at time %t", $time);
        $display("Total samples captured: %d", sample_count);
        $display("Total packets: %d", packet_count);
        $display("Total frames processed: %d", frame_count);
        
        // Verify packet boundaries
        if (sample_count >= 2048) begin
            $display("SUCCESS: Captured enough samples to see at least one complete packet");
            if (sample_count >= 2090) begin
                $display("SUCCESS: Captured 2090+ samples showing packet boundary behavior");
            end
        end
        
        // Wait for processing
        #5000;
        
        // End simulation
        $display("Simulation ending at time %t", $time);
        $finish;
    end

endmodule
