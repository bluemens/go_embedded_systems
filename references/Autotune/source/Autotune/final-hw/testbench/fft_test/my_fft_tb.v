`timescale 1ns/1ps
`define HALF_CLOCK_PERIOD #10
`define CLOCK_PERIOD #20
`define PYTHON_INPUT_REAL_FN "../../../PYTHON/500Hz/fx_real.dat"
`define PYTHON_INPUT_IMAG_FN "../../../PYTHON/500Hz/fx_imag.dat"
`define PYTHON_OUTPUT_REAL_FN "real_output.dat"
`define PYTHON_OUTPUT_IMAG_FN "imag_output.dat"
`define PYTHON_OUTPUT_MAG_FN "magnitude_output.dat"

module testbench();
    
    // Clock and reset
    reg clk = 0;
    reg reset_n = 0;
    
    // Input signals
    reg sink_valid = 0;
    reg source_ready = 1;
    reg [1:0] sink_error = 0;
    reg sink_sop = 0;
    reg sink_eop = 0;
    reg [15:0] sink_real = 0;
    reg [15:0] sink_imag = 0;
    reg inverse = 0;
    reg [11:0] fftpts_in = 12'd2048;
    
    // Output signals
    wire sink_ready;
    wire source_valid;
    wire [1:0] source_error;
    wire source_sop;
    wire source_eop;
    wire [27:0] source_real;
    wire [27:0] source_imag;
    wire [11:0] fftpts_out;
    
    // Test variables
    integer i = 0;
    integer ret_read;

    integer real_input;
    integer imag_input;
    integer real_output;
    integer imag_output;
    integer mag_output;
    wire [55:0] source_mag; 

    data2mag data2mag_inst (
        .source_real(source_real),
        .source_imag(source_imag),
        .magnitude(source_mag)
    );

    // Clock generation
    always begin
        `HALF_CLOCK_PERIOD;
        clk = ~clk;
    end
    
    // Instantiate the FFT module
    my_fft dut (
        .clk(clk),
        .reset_n(reset_n),
        .sink_valid(sink_valid),
        .sink_ready(sink_ready),
        .sink_error(sink_error),
        .sink_sop(sink_sop),
        .sink_eop(sink_eop),
        .sink_real(sink_real),
        .sink_imag(sink_imag),
        // manually set fftpts_in to 2048
        .fftpts_in(12'd2048),
        .inverse(inverse),
        .source_valid(source_valid),
        .source_ready(source_ready),
        .source_error(source_error),
        .source_sop(source_sop),
        .source_eop(source_eop),
        .source_real(source_real),
        .source_imag(source_imag),
        .fftpts_out(fftpts_out)
    );
    
    //  Start Simulation
    initial begin
        // open input files
        real_input = $fopen(`PYTHON_INPUT_REAL_FN, "r");
        if (real_input == 0) begin  
            $display("Failed to open real input file");
            $finish;
        end
        imag_input = $fopen(`PYTHON_INPUT_IMAG_FN, "r");
        if (imag_input == 0) begin
            $display("Failed to open imag input file");
            $finish;
        end
        
        // Reset
        @(posedge clk);
        @(negedge clk);
        reset_n = 1;
        sink_sop = 1;
        sink_eop = 0;
        @(posedge clk);

        @(posedge clk);
        sink_sop = 1;

        while (!sink_ready) begin
            @(posedge clk);
        end

        i = 0;

        while (i < 2048) begin
            @(posedge clk);
            sink_valid = 1;
            ret_read = $fscanf(real_input, "%b\n", sink_real);
            ret_read = $fscanf(imag_input, "%b\n", sink_imag);
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
        @(posedge clk);
        @(posedge clk);
        @(posedge clk);
        @(posedge clk);
        @(posedge clk);

        @(posedge clk);
        sink_sop = 1;

        while (!sink_ready) begin
            @(posedge clk);
        end

        while (source_sop == 1'b1) begin
            @(posedge clk);
        end
        i = 0;

        while (i < 2048) begin
            @(posedge clk);
            sink_valid = 1;
            ret_read = $fscanf(real_input, "%b\n", sink_real);
            ret_read = $fscanf(imag_input, "%b\n", sink_imag);
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
        

        // read output
        real_output = $fopen(`PYTHON_OUTPUT_REAL_FN, "w");
        if (real_output == 0) begin 
            $display("Failed to open real output file");
            $finish;
        end
        imag_output = $fopen(`PYTHON_OUTPUT_IMAG_FN, "w");
        if (imag_output == 0) begin
            $display("Failed to open imag output file");
            $finish;
        end
        mag_output = $fopen(`PYTHON_OUTPUT_MAG_FN, "w");
        if (mag_output == 0) begin
            $display("Failed to open mag output file");
            $finish;
        end

        i = 0;
        while (i < 4096) begin
            @(posedge clk);
            if (source_valid == 1'b1) begin
                $fwrite(real_output, "%d\n", source_real);
                $fwrite(imag_output, "%d\n", source_imag);
                $fwrite(mag_output, "%d\n", source_mag);
                i = i + 1;
            end
        end


        for (i = 0; i < 10; i = i + 1) begin
            @(posedge clk);
        end
         
        // Display results
        $display("FFT Test completed");
        
        $fclose(real_input);
        $fclose(imag_input);
        $fclose(real_output);
        $fclose(imag_output);
        $fclose(mag_output);
        
        // End simulation
        $finish;
    end

    
endmodule 
