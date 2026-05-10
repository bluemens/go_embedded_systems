`timescale 1ns / 1ps
`define CLK_PERIOD 100
`define INPUT_FILE_NAME_REAL "../../PYTHON/500Hz/fx_fft_real.dat"
`define INPUT_FILE_NAME_IMAG "../../PYTHON/500Hz/fx_fft_imag.dat"

module testbench();

    logic clk;
    logic reset;
    logic [27:0] source_real;
    logic [27:0] source_imag;
    logic [10:0] address;
    logic write_en;
    logic source_sop;
    logic source_eop;
    logic [27:0] sink_real;
    logic [27:0] sink_imag; 
    logic ready;

    mem_block mem_block_inst(
        .clk(clk),
        .reset(reset),
        .source_real(source_real),
        .source_imag(source_imag),
        .address(address),
        .write_en(write_en),
        .source_sop(source_sop),
        .source_eop(source_eop),
        .sink_real(sink_real),
        .sink_imag(sink_imag),
        .ready(ready)
    );

    // read input data
    integer input_real_file;
    integer input_imag_file;
    integer i;
    integer ret_read;

    // generate clock
    always #(`CLK_PERIOD / 2) clk = ~clk;


    // initialize signals
    initial begin
        clk = 0;
        reset = 0;
        write_en = 0;
        source_sop = 0;
        source_eop = 0;
        address = 0;
        source_real = 0;
        source_imag = 0;

        // open input files
        input_real_file = $fopen(`INPUT_FILE_NAME_REAL, "r");
        input_imag_file = $fopen(`INPUT_FILE_NAME_IMAG, "r");

        @ (posedge clk);
        @ (negedge clk);
        reset = 1;
        @ (posedge clk);
        write_en = 1;
        source_sop = 1;
        ret_read = $fscanf(input_real_file, "%b", source_real);
        ret_read = $fscanf(input_imag_file, "%b", source_imag);

        for (i = 1; i < 2048; i++) begin
            @ (posedge clk);
            source_sop = 0;
            ret_read = $fscanf(input_real_file, "%b", source_real);
            ret_read = $fscanf(input_imag_file, "%b", source_imag);
            $display("sink_real: %d, sink_real: %d", sink_real, sink_imag);
        end

        source_eop = 1;
        @ (posedge clk);
        write_en = 0;
        source_eop = 0;
        
        for (i = 0; i < 2048; i++) begin
            @ (posedge clk);
            address = i;
            #10;
            $display("address: %d, sink_real: %d, sink_imag: %d", address, sink_real, sink_imag);
        end

        @ (posedge clk);

        $display("Testbench finished");
        // close input files
        $fclose(input_real_file);
        $fclose(input_imag_file);
        $finish;
    end



endmodule   