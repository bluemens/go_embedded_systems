`timescale 1ns/1ps
`define HALF_CLOCK_PERIOD #10
`define CLOCK_PERIOD #20
`define PYTHON_INPUT_REAL_FN "../../../PYTHON/500Hz/fx_real.dat"
`define PYTHON_OUTPUT_REAL_FN "output.dat"

module testbench();
    
    // Clock and reset
    logic clk = 0;
    logic reset_n = 0;
    
    // Avalon-ST sink interface
    logic sink_valid = 0;
    logic sink_ready;
    logic sink_sop = 0;
    logic sink_eop = 0;
    logic [15:0] sink_real = 0;
    
    // Avalon-ST source interface
    logic source_valid;
    logic source_ready = 1;
    logic source_sop;
    logic source_eop;
    logic [27:0] source_real;

    // // peak_detector
    // logic peak_valid;
    // logic [9:0] peak_index;

    // // shift
    // logic [5:0] shift_factor;

    // // helper signals
    // logic [15:0] ifft_sink_real;
    // logic [15:0] ifft_sink_imag;

    // logic [10:0] mem_address;
    // logic [10:0] shift_address;

    // logic ifft_sink_valid;
    // logic ifft_sink_sop;
    // logic ifft_sink_eop;

    // Test variables
    integer i = 0;
    integer ret_read;

    integer real_input;
    integer real_output;
    integer count = 0;

    // Clock generation
    always begin
        `HALF_CLOCK_PERIOD;
        clk = ~clk;
    end
    
    // Instantiate the FFT module
    fft_ifft_peak fft_ifft_peak_inst (
        .clk(clk),
        .reset_n(reset_n),
        .sink_valid(sink_valid),
        .sink_ready(sink_ready),
        .sink_sop(sink_sop),
        .sink_eop(sink_eop),
        .sink_real(sink_real),
        .source_valid(source_valid),
        .source_ready(source_ready),
        // .source_sop(source_sop),
        // .source_eop(source_eop),
        .source_real(source_real)
        // .peak_valid(peak_valid),
        // .peak_index(peak_index),
        // .shift_factor(shift_factor),
        // .ifft_sink_real(ifft_sink_real),
        // .ifft_sink_imag(ifft_sink_imag)
        // .mem_address(mem_address),
        // .shift_address(shift_address)
        // .ifft_sink_valid(ifft_sink_valid),
        // .ifft_sink_sop(ifft_sink_sop),
        // .ifft_sink_eop(ifft_sink_eop)
    );
    
    //  Start Simulation
    initial begin
        // open input files
        real_input = $fopen(`PYTHON_INPUT_REAL_FN, "r");
        if (real_input == 0) begin  
            $display("Failed to open real input file");
            $finish;
        end

        real_output = $fopen(`PYTHON_OUTPUT_REAL_FN, "w");
        if (real_output == 0) begin 
            $display("Failed to open real output file");
            $finish;
        end
        
        // Reset
        @(posedge clk);
        @(negedge clk);
        reset_n = 1;
        sink_sop = 1;
        sink_eop = 0;
        @(posedge clk);


        while (!sink_ready) begin
            @(posedge clk);
        end

        i = 0;
        sink_valid = 0;

        while (i < 2048) begin
            @(posedge clk);
            sink_valid = 1;
            ret_read = $fscanf(real_input, "%b\n", sink_real);
            if (i == 1) begin
                sink_sop = 0;
            end
            if (i == 2047) begin
                sink_eop = 1;
            end
            i = i + 1;
        end 

        @(posedge clk);
        sink_valid = 0;
        sink_eop = 0;

        @(posedge clk); 
        i = 0;
        sink_sop = 1;

        while (!source_valid) begin
            @(posedge clk);
        end

        while (i < 2048) begin
            if (source_valid) begin
                $fwrite(real_output, "%b\n", source_real);
                count = count + 1;
            end
            @(posedge clk);
            sink_valid = 1;
            ret_read = $fscanf(real_input, "%b\n", sink_real);
            if (i == 1) begin
                sink_sop = 0;
            end
            if (i == 2047) begin
                sink_eop = 1;
            end
            i = i + 1;
        end 
        
        if (source_valid) begin
            $fwrite(real_output, "%b\n", source_real);
            count = count + 1;
        end

        @(posedge clk);
        sink_valid = 0;
        sink_eop = 0;
        if (source_valid) begin
            $fwrite(real_output, "%b\n", source_real);
            count = count + 1;
        end

        @(posedge clk);
        i = 0;  
        sink_sop = 1;

        while (!source_valid) begin
            @(posedge clk);
        end


        while (i < 2048) begin
            if (source_valid) begin
                $fwrite(real_output, "%b\n", source_real);
                count = count + 1;
            end
            @(posedge clk);
            sink_valid = 1;
            ret_read = $fscanf(real_input, "%b\n", sink_real);
            if (i == 1) begin
                sink_sop = 0;
            end
            if (i == 2047) begin
                sink_eop = 1;
            end
            i = i + 1;
        end

        if (source_valid) begin
            $fwrite(real_output, "%b\n", source_real);
            count = count + 1;
        end

        @(posedge clk);
        sink_valid = 0;
        sink_eop = 0;
        if (source_valid) begin
            $fwrite(real_output, "%b\n", source_real);
            count = count + 1;
        end

        @(posedge clk);
        i = 0;  
        sink_sop = 1;

        while (!source_valid) begin
            @(posedge clk);
        end

        while (i < 2048) begin
            if (source_valid) begin
                $fwrite(real_output, "%b\n", source_real);
                count = count + 1;
            end
            @(posedge clk);
            sink_valid = 1;
            ret_read = $fscanf(real_input, "%b\n", sink_real);
            if (i == 1) begin
                sink_sop = 0;
            end
            if (i == 2047) begin
                sink_eop = 1;
            end
            i = i + 1;
        end

        if (source_valid) begin
            $fwrite(real_output, "%b\n", source_real);
            count = count + 1;
        end

        @(posedge clk);
        sink_valid = 0;
        sink_eop = 0;
    
        while (count < 8192) begin
            if (source_valid) begin
                $fwrite(real_output, "%b\n", source_real);
                count = count + 1;
            end
            @(posedge clk);
        end

        for (i = 0; i < 10; i = i + 1) begin
            @(posedge clk);
        end
         
        // Display results
        $display("FFT Test completed");
        
        $fclose(real_input);
        $fclose(real_output);
        
        // End simulation
        $finish;
    end

endmodule 

