`timescale 1ns/100ps
`define N 16

module testbench;
    // Test parameters
    localparam CLK_PERIOD = 10; // 10ns for 100MHz clock
    
    // System signals
    logic clk;
    logic rst_n;
    logic en;
    logic w_en;
    
    // Input buffer signals
    logic [7:0] data_in;
    logic rd_en;
    logic wr_en;
    logic [7:0] data_out [0:8];
    logic buffer_done;
    
    // PE array signals
    wire [9*8-1:0] active_left;
    wire [1*8-1:0] in_weight_above;
    wire [1*8-1:0] out_weight_final;
    wire [1*16-1:0] out_sum_final;
    
    // Test control signals
    int errors = 0;
    int total_tests = 0;
    
    // Internal storage to track input/output values
    logic [7:0] input_values [`N*`N];
    logic [1*16-1:0] output_values [(`N-2)*(`N-2)];
    int output_index = 0;
    
    // Connect input buffer outputs to PE array inputs
    genvar i;
    generate
        for (i = 0; i < 9; i++) begin
            assign active_left[(i+1)*8-1:i*8] = data_out[i];
        end
    endgenerate
    
    // Clock generation
    initial begin
        clk = 0;
        forever #(CLK_PERIOD/2) clk = ~clk;
    end
    
    // Use a constant weight for predictable results
    assign in_weight_above = 8'b01000000; //
    
    // Instantiate input buffer
    input_buffer input_buffer_inst (
        .clk(clk),
        .rst_n(rst_n),
        .data_in(data_in),
        .rd_en(rd_en),
        .wr_en(wr_en),
        .data_out(data_out),
        .done(buffer_done)
    );
    
    // Instantiate PE array
    PE_array #(
        .num1(9),
        .num2(1)
    ) pe_array_inst (
        .CLK(clk),
        .RESET(rst_n),
        .EN(en),
        .W_EN(w_en),
        .active_left(active_left),
        .in_weight_above(in_weight_above),
        .out_weight_final(out_weight_final),
        .out_sum_final(out_sum_final)
    );
    
    // Task to initialize the testbench
    task initialize();
        // Initialize signals
        rst_n = 1;
        en = 0;
        w_en = 0;
        rd_en = 0;
        wr_en = 0;
        data_in = 0;
        
        // Apply reset
        #(CLK_PERIOD*2);
        rst_n = 0;
        #(CLK_PERIOD*20);
        rst_n = 1;
        
        // Enable modules
        #(CLK_PERIOD);
        en = 1;
    endtask
    
    // Task to generate test data
    task generate_test_data();
        // Initialize with some pattern for predictable testing
        for (int i = 0; i < `N*`N; i++) begin
            input_values[i] = (i % 256) + 1; // Values 1-16 repeating
        end
    endtask
    
    // Task to write data to input buffer
    task write_to_buffer();
        @(posedge clk);
        wr_en = 1;
        
        $display("Starting to write data to input buffer");
        for (int i = 0; i < `N*`N; i++) begin
            data_in = input_values[i];
            $display("Writing data[%0d] = %0d", i, data_in);
            @(posedge clk);
        end
        
        wr_en = 0;
        $display("Finished writing data to input buffer");
    endtask

    task load_weights();
    $display("Loading weights into PE array");
    @(posedge clk);
    w_en = 1;  
    
    repeat (20) @(posedge clk);
    
    w_en = 0;  
    $display("Weights loaded and fixed");
    endtask
    
    // Task to read from input buffer and process through PE array
    task process_data();
        // Wait a few cycles for data propagation
        #(CLK_PERIOD*5);
        
        $display("Starting to read and process data");
        @(posedge clk);
        rd_en = 1;
        w_en = 0; 
        // Capture output values while reading
        output_index = 0;
        
        // Wait for processing to complete
        while (!buffer_done) begin
            @(posedge clk);
            if (rd_en) begin
                // Store output for verification
                output_values[output_index] = out_sum_final;
                $display("Cycle %0d: Reading out_sum_final = %0d", output_index, out_sum_final);
                output_index = output_index + 1;
            end
        end
        
        // Continue a few more cycles to get final outputs
        for (int i = 0; i < 10; i++) begin
            @(posedge clk);
            if (output_index < (`N-2)*(`N-2)) begin
                output_values[output_index] = out_sum_final;
                $display("Final cycle %0d: Reading out_sum_final = %0d", output_index, out_sum_final);
                output_index = output_index + 1;
            end
        end
        
        rd_en = 0;
        $display("Finished reading and processing data");
    endtask
    
    // Task to verify results
    task verify_results();
        logic [15:0] expected;
        logic [15:0] actual;
        int valid_outputs;
        
        $display("\n===== VERIFICATION RESULTS =====");
        valid_outputs = (`N-2)*(`N-2);
        // Check only valid outputs (depends on your specific architecture)
        for (int i = 0; i < valid_outputs; i++) begin
            // Calculate expected output
            // For this simple example, we're multiplying each input by the weight (5)
            // In your real design, you'll need to implement the proper convolution calculation
            expected = input_values[i] * 5; // Simplified for testing
            actual = output_values[i];
            
            total_tests++;
            
            if (actual != expected) begin
                $display("ERROR: Output[%0d] = %0d, Expected = %0d", i, actual, expected);
                errors++;
            end else begin
                $display("PASS: Output[%0d] = %0d, Expected = %0d", i, actual, expected);
            end
        end
        
        // Print summary
        $display("\n===== TEST SUMMARY =====");
        $display("Total tests: %0d", total_tests);
        $display("Passed: %0d", total_tests - errors);
        $display("Failed: %0d", errors);
        
        if (errors == 0) begin
            $display("TEST PASSED: All outputs match expected values");
        end else begin
            $display("TEST FAILED: Some outputs did not match expected values");
        end
    endtask
    
    // Main test sequence
    initial begin
        // Setup waveform dumping for ModelSim
        $dumpfile("waveform.vcd");
        $dumpvars(0, testbench);
        
        // Initialize testbench
        initialize();
        
        // Generate test data
        generate_test_data();

        load_weights();
        
        // Write data to input buffer
        write_to_buffer();
        
        // Process data through PE array
        process_data();
        
        // Verify results
        verify_results();
        
        // End simulation
        #(CLK_PERIOD*10);
        $finish;
    end
    
    // Monitor signals for debugging
    initial begin
        $monitor("Time: %t, Buffer Done: %b, OutSum: %h, OutWeight: %h", 
                 $time, buffer_done, out_sum_final, out_weight_final);
    end
    
endmodule