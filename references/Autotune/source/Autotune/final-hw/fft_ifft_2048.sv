module fft_ifft_2048(
    input logic clk, // clock
    input logic reset_n, // reset
    input logic sink_valid, // valid
    output logic sink_ready, // ready
    // input logic [1:0] sink_error, // just set to 0
    input logic sink_sop, // start of packet
    input logic sink_eop, // end of packet
    input logic [15:0] sink_real, // real part of input
    // input logic [15:0] sink_imag, // imaginary part of input
    output logic source_valid, // valid
    input logic source_ready, // wire this to speaker interface
    // output logic [1:0] source_error, // error
    // output logic source_sop, // start of packet
    // output logic source_eop, // end of packet
    output logic [27:0] source_real
    // output logic [27:0] source_imag,
    // output logic [11:0] fftpts_out
);

    // define internal signals
    logic fft_source_valid;
    logic fft_source_ready;
    logic [1:0]fft_source_error;
    logic fft_source_sop;
    logic fft_source_eop;
    logic [15:0] sink_imag = 0;
    logic [27:0] fft_source_real;
    logic [27:0] fft_source_imag;
    logic [11:0] fft_fftpts_out;
    logic [15:0] ifft_sink_real;
    logic [15:0] ifft_sink_imag;
    logic ifft_sink_ready; 
    logic [15:0] source_imag;
    logic [11:0] fftpts_out;
    logic [1:0] source_error;
    logic [15:0] source_sop;
    logic [15:0] source_eop;

    convert_28_to_16 convert_28_to_16_inst(
        .source_real(fft_source_real),
        .source_imag(fft_source_imag),
        .sink_real(ifft_sink_real),
        .sink_imag(ifft_sink_imag)
    );

    // Instantiate the FFT module
    my_fft fft_inst(
        .clk(clk),
        .reset_n(reset_n),
        .sink_valid(sink_valid),
        .sink_ready(sink_ready), // 
        .sink_error(2'b0), // maybe just set it to 0
        .sink_sop(sink_sop), 
        .sink_eop(sink_eop),
        .sink_real(sink_real), 
        .sink_imag(sink_imag),
        .fftpts_in(12'd2048),
        .inverse(1'b0),
        .source_valid(fft_source_valid), // wire into ifft's sink_valid
        .source_ready(ifft_sink_ready), // wire from ifft's sink_ready
        .source_error(fft_source_error), // wire to ifft's sink_error
        .source_sop(fft_source_sop), // wire to ifft's sink_sop
        .source_eop(fft_source_eop), // wire to ifft's sink_eop
        .source_real(fft_source_real), // wire to 28_to_16's source_real
        .source_imag(fft_source_imag), // wire to 28_to_16's source_imag
        .fftpts_out(fft_fftpts_out) // wire to ifft's fftpts_in
    );

    // Instantiate the IFFT module
    my_fft ifft_inst(
        .clk(clk),
        .reset_n(reset_n),
        .sink_valid(fft_source_valid), // wire from fft's source_valid
        .sink_ready(ifft_sink_ready), // wire to fft's source_ready
        .sink_error(fft_source_error), // wire from fft's source_error
        .sink_sop(fft_source_sop), // wire from fft's source_sop
        .sink_eop(fft_source_eop), // wire from fft's source_eop
        .sink_real(ifft_sink_real), // wire from 28_to_16's sink_real
        .sink_imag(ifft_sink_imag), // wire from 28_to_16's sink_imag
        .fftpts_in(12'd2048), // wire from fft's fftpts_out
        .inverse(1'b1),
        .source_valid(source_valid), 
        .source_ready(source_ready), // can be assumed to be 1 testbench
        .source_error(source_error), 
        .source_sop(source_sop), 
        .source_eop(source_eop), 
        .source_real(source_real), 
        .source_imag(source_imag),
        .fftpts_out(fftpts_out)
    );
    
endmodule

module convert_28_to_16(
    input logic [27:0] source_real,
    input logic [27:0] source_imag,
    output logic [15:0] sink_real,
    output logic [15:0] sink_imag
);

    assign sink_real = source_real[27:12];
    assign sink_imag = source_imag[27:12];

endmodule

    
