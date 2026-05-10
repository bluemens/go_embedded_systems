`timescale 1ns / 1ps
`define CLK_PERIOD 100
`define INPUT_FILE_NAME_REAL "../../PYTHON/500Hz/fx_fft_real.dat"
`define INPUT_FILE_NAME_IMAG "../../PYTHON/500Hz/fx_fft_imag.dat"
`define OUTPUT_FILE_PEAK "fx_peak_index.dat"
`define OUTPUT_FILE_MEM_ADDRESS "mem_address.dat"

module testbench();

    logic clk;
    logic reset_n;
    logic [27:0] source_real;
    logic [27:0] source_imag;
    logic sink_sop;
    logic sink_valid;
    logic [9:0] peak_index;
    logic source_valid;
    logic [55:0] peak_mag;
    logic [2:0] shift_index;
    logic shift_direction;

    logic [10:0] mem_address;
    logic [10:0] shift_address;

    peak_detector peak_detector_inst(
        .clk(clk),
        .reset_n(reset_n),
        .source_real(source_real),  
        .source_imag(source_imag),
        .sink_sop(sink_sop),
        .sink_valid(sink_valid),
        .peak_index(peak_index),
        .source_valid(source_valid),
        .peak_mag(peak_mag)
    );  
    
    shift shift_inst(
        .peak_index(peak_index),
        .shift_index(shift_index),
        .shift_direction(shift_direction)
    );

    tuning tuning_inst(
        .shift_address(shift_address),
        .shift_index(shift_index),
        .shift_direction(shift_direction),
        .mem_address(mem_address)
    );

    integer ret_read;
    integer i;
    integer input_real_file;
    integer input_imag_file;
    integer output_file;    
    integer output_file_mem_address;

    always #(`CLK_PERIOD / 2) clk = ~clk;

    initial begin
        clk = 0;
        reset_n = 0;
        sink_sop = 0;
        sink_valid = 0;
        source_real = 0;
        source_imag = 0;
        peak_mag = 0;
        shift_address = 0;

        input_real_file = $fopen(`INPUT_FILE_NAME_REAL, "r");
        input_imag_file = $fopen(`INPUT_FILE_NAME_IMAG, "r");
        output_file = $fopen(`OUTPUT_FILE_PEAK, "w");
        output_file_mem_address = $fopen(`OUTPUT_FILE_MEM_ADDRESS, "w");

        @ (posedge clk);
        @ (negedge clk);
        reset_n = 1;
        @ (posedge clk);
        sink_sop = 1;
        sink_valid = 1;
        ret_read = $fscanf(input_real_file, "%b", source_real);
        ret_read = $fscanf(input_imag_file, "%b", source_imag);

        for (i = 1; i < 1024; i++) begin
            @ (posedge clk);
            sink_sop = 0;
            ret_read = $fscanf(input_real_file, "%b", source_real);
            ret_read = $fscanf(input_imag_file, "%b", source_imag);
        end

        
        @ (posedge clk);
        $fwrite(output_file, "%b\n", peak_index);
        $display("peak_index: %d", peak_index);
        sink_valid = 0;


        @(posedge clk);

        for (i = 0; i < 2048; i++) begin
            @ (posedge clk);
            shift_address = i;
            #10;
            $display("shift_address: %d, memory address: %d", shift_address, mem_address);
            $fwrite(output_file_mem_address, "shift_address: %d, memory address: %d\n", shift_address, mem_address);
        end

        @ (posedge clk);
        $fclose(input_real_file);
        $fclose(input_imag_file); 
        $fclose(output_file);
        $finish;

    end

endmodule