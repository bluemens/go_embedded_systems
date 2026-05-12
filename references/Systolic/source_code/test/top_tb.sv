`timescale 1ns/1ps
`define N 16

module top_tb;

    // Top module signals
    logic clk;
    logic reset;
    logic [7:0] data_in;
    logic write;
    logic read;
    logic [3:0] addr;
    logic chipselect;
    logic [7:0] data_out;
    
    // Clock period parameter
    parameter CLK_PERIOD = 10; // 10ns = 100MHz
    
    // Test data parameters
    parameter IMG_SIZE = `N;       // 16x16 image
    parameter INPUT_SIZE = IMG_SIZE * IMG_SIZE;  // input size
    parameter WEIGHT_SIZE = 3 * 3; // 3x3 kernel
    parameter OUTPUT_SIZE = (IMG_SIZE - 3 + 1) * (IMG_SIZE - 3 + 1); // output size = 14x14 = 196
    
    // Test sequence parameter
    parameter TEST_SEQUENCE = 1;  // 0: weights first then data, 1: data first then weights
    parameter MAX_WAIT_CYCLES = 5000; // maximum wait cycles
    
    // Test data
    logic [7:0] test_input [0:INPUT_SIZE-1];
    logic [7:0] test_weight [0:WEIGHT_SIZE-1];
    logic [7:0] test_output [0:OUTPUT_SIZE-1];
    
    // Convolution results for software simulation (optional)
    logic [19:0] expected_output [0:OUTPUT_SIZE-1];
    logic [19:0] sum;
    
    // Test statistics
    int output_counter = 0;
    int cycle_counter = 0;
    bit test_passed = 1;
    
    // Instantiate top-level module
    top DUT (
        .clk(clk),
        .reset(reset),
        .data_in(data_in),
        .write(write),
        .read(read),
        .addr(addr),
        .chipselect(chipselect),
        .data_out(data_out)
    );
    
    // Clock generation
    initial begin
        clk = 0;
        forever #(CLK_PERIOD/2) clk = ~clk;
    end
    
    // Counter
    always @(posedge clk) begin
        if (reset)
            cycle_counter <= 0;
        else
            cycle_counter <= cycle_counter + 1;
    end
    
    // Initialize test data
    initial begin
        // Initialize input image - using (i*2 + j + 1) % 256 pattern
        for (int i = 0; i < IMG_SIZE; i++) begin
            for (int j = 0; j < IMG_SIZE; j++) begin
                test_input[i*IMG_SIZE + j] = (i*2 + j + 1) % 256;
            end
        end
        
        // Initialize weights - using 10*(i+1) pattern (weights: 10,20,30,...,90)
        for (int i = 0; i < WEIGHT_SIZE; i++) begin
            test_weight[i] = 10*(i+1);
        end
        
        // Initialize output array
        for (int i = 0; i < OUTPUT_SIZE; i++) begin
            test_output[i] = 0;
            expected_output[i] = 0;
        end
        
        // Pre-calculate expected output (software simulated convolution)
        for (int y = 0; y < IMG_SIZE-2; y++) begin
            for (int x = 0; x < IMG_SIZE-2; x++) begin
                sum = 0;
                for (int ky = 0; ky < 3; ky++) begin
                    for (int kx = 0; kx < 3; kx++) begin
                        sum += test_input[(y+ky)*IMG_SIZE + (x+kx)] * test_weight[ky*3 + kx];
                    end
                end
                expected_output[y*(IMG_SIZE-2) + x] = sum;
            end
        end
    end
    

    //********************************task****************************//
    // Task: write data
    task write_data(input [3:0] address, input [7:0] data);
        @(posedge clk);
        addr = address;
        data_in = data;
        write = 1;
        chipselect = 1;
        @(posedge clk);
        write = 0;
        chipselect = 0;
        @(posedge clk); // extra clock cycle
    endtask
    
    // Task: read data
    task read_data(input [3:0] address, output [7:0] data);
        @(posedge clk);
        addr = address;
        read = 1;
        chipselect = 1;
        @(posedge clk);
        @(posedge clk); // wait for data to stabilize
        data = data_out;
        read = 0;
        chipselect = 0;
        @(posedge clk); // extra clock cycle
    endtask
    
    // Task: wait for completion
    task wait_for_completion(input int max_cycles);
        logic [7:0] ready_status;
        int wait_count;
        
        do begin
            wait_count = 0;
            read_data(4'h3, ready_status);
            wait_count++;
            
            if (wait_count % 50 == 0) begin
                $display("Waiting for output ready... (%0d checks)", wait_count);
            end
            
            if (wait_count >= max_cycles) begin
                $display("Error: wait timed out! Checked %0d times", wait_count);
                test_passed = 0;
                break;
            end
        end while (ready_status != 8'd1);
        
        $display("Output ready! After %0d checks, %0d clock cycles", wait_count, cycle_counter);
    endtask
    
    // Task: verify output
    task verify_output();
        int mismatch_count;
        
        begin
            mismatch_count = 0;
            for (int i = 0; i < OUTPUT_SIZE; i++) begin
                // Simple check: ensure output is not zero repeatedly
                if (i > 0 && test_output[i] == 0 && test_output[i-1] == 0) begin
                    mismatch_count++;
                    if (mismatch_count <= 5) begin
                        $display("Warning: output[%0d] and output[%0d] are both zero", i-1, i);
                    end
                end
            end
        end
        
        if (mismatch_count > OUTPUT_SIZE / 2) begin
            $display("Error: more than half of outputs are zero or abnormal pattern!");
            test_passed = 0;
        end else if (mismatch_count > 0) begin
            $display("Warning: found %0d suspicious output patterns", mismatch_count);
        end else begin
            $display("Output format verification passed!");
        end
        
        // Print some sample data for manual check
        $display("Sample output data:");
        for (int i = 0; i < 5; i++) begin
            for (int j = 0; j < 5; j++) begin
                $write("%4d ", test_output[i*(IMG_SIZE-2) + j]);
            end
            $write("\n");
        end
    endtask
    

    //********************************main test****************************//
    // Main testbench procedure
    initial begin
        // Initialize signals
        reset = 1;
        write = 0;
        read = 0;
        addr = 0;
        data_in = 0;
        chipselect = 0;
        
        // Reset
        #(CLK_PERIOD*50);
        reset = 0;
        #(CLK_PERIOD*2);
        
        // Select load sequence based on TEST_SEQUENCE
        if (TEST_SEQUENCE == 0) begin
            // Sequence 1: weights first, then input data
            $display("\nTest Sequence 1: Load weights first, then input data");
            
            // Step 1: set image size
            $display("Step 1: Set image size to %0d x %0d", IMG_SIZE, IMG_SIZE);
            write_data(4'h0, IMG_SIZE);
            
            // Step 2: load weight data
            $display("Step 2: Load weight data (3x3 = 9 weights)");
            for (int i = 0; i < WEIGHT_SIZE; i++) begin
                write_data(4'h1, test_weight[i]);
                $display("  Loaded weight[%0d] = %0d", i, test_weight[i]);
            end
            
            // Step 3: load input image data
            $display("Step 3: Load input image data (%0d pixels)", INPUT_SIZE);
            for (int i = 0; i < IMG_SIZE; i++) begin
                for (int j = 0; j < IMG_SIZE; j++) begin
                    write_data(4'h2, test_input[i*IMG_SIZE + j]);
                    if ((i*IMG_SIZE + j) % IMG_SIZE == 0)
                        $display("  Loading image row %0d/%0d", i+1, IMG_SIZE);
                end
            end
        end else begin
            // Sequence 2: input data first, then weights
            $display("\nTest Sequence 2: Load input data first, then weights");
            
            // Step 1: set image size
            $display("Step 1: Set image size to %0d x %0d", IMG_SIZE, IMG_SIZE);
            write_data(4'h0, IMG_SIZE);
            
            // Step 2: load input image data
            $display("Step 2: Load input image data (%0d pixels)", INPUT_SIZE);
            for (int i = 0; i < IMG_SIZE; i++) begin
                for (int j = 0; j < IMG_SIZE; j++) begin
                    write_data(4'h2, test_input[i*IMG_SIZE + j]);
                    if ((i*IMG_SIZE + j) % IMG_SIZE == 0)
                        $display("  Loading image row %0d/%0d", i+1, IMG_SIZE);
                end
            end
            
            // Step 3: load weight data
            $display("Step 3: Load weight data (3x3 = 9 weights)");
            for (int i = 0; i < WEIGHT_SIZE; i++) begin
                write_data(4'h1, test_weight[i]);
                $display("  Loaded weight[%0d] = %0d", i, test_weight[i]);
            end
        end
        
        // Step 4: waiting for convolution processing to complete
        $display("Step 4: Waiting for convolution processing to complete");
        wait_for_completion(MAX_WAIT_CYCLES);
        
        // Step 5: read output data
        $display("Step 5: Read output data (%0d values)", OUTPUT_SIZE);
        for (int i = 0; i < OUTPUT_SIZE; i++) begin
            read_data(4'h4, test_output[i]);
            output_counter++;
            
            if (i % (IMG_SIZE-2) == 0 && i > 0)
                $display("  Reading output row %0d/%0d", i/(IMG_SIZE-2), (IMG_SIZE-2));
        end
        
        // Step 6: verify output results
        $display("Step 6: Verify output results");
        verify_output();
        
        // Test summary
        $display("\nTest Summary:");
        $display("  Number of input pixels processed: %0d", INPUT_SIZE);
        $display("  Number of weights used: %0d", WEIGHT_SIZE);
        $display("  Number of outputs generated: %0d", OUTPUT_SIZE);
        $display("  Number of outputs read: %0d", output_counter);
        $display("  Total number of cycles executed: %0d", cycle_counter);
        
        if (test_passed)
            $display("\nTest completed: PASSED!");
        else
            $display("\nTest completed: FAILED! Please check the above error messages.");
        
        #(CLK_PERIOD*10);
        $finish;
    end
    
    // Monitor important signals (optional for debugging)
    initial begin
        $timeformat(-9, 2, " ns", 20);
    end

endmodule