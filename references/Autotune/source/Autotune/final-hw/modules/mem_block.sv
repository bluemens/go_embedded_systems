module mem_block(
    input logic clk,
    input logic reset_n,
    input logic [15:0] source_real,
    input logic [15:0] source_imag,
    input logic [10:0] address,
    input logic source_valid, // wire to FFT's source_valid
    input logic source_sop, // wire to FFT's source_sop
    input logic source_eop, // wire to FFT's source_eop
    output logic [15:0] sink_real,
    output logic [15:0] sink_imag 
);

    logic [15:0] mem_real [0:2047];
    logic [15:0] mem_imag [0:2047];
    logic write_en;

    logic [10:0] write_address;

    assign sink_real = mem_real[address];
    assign sink_imag = mem_imag[address];
    // assign ready = ~write_en;

    always_ff @(posedge clk) begin
        if (!reset_n) begin
            for (int i = 0; i < 2048; i++) begin
                mem_real[i] <= 0;
                mem_imag[i] <= 0;
            end
            write_address <= 0;
        end
        else if (source_valid) begin
            write_en <= 1;
            if (source_sop) begin
                mem_real[0] <= source_real;
                mem_imag[0] <= source_imag;
                write_address <= 1;
            end
            else if (source_eop) begin
                mem_real[write_address] <= source_real;
                mem_imag[write_address] <= source_imag;
                write_address <= 0;
                write_en <= 0;
            end
            else begin
                mem_real[write_address] <= source_real; 
                mem_imag[write_address] <= source_imag;
                write_address <= write_address + 1;
            end
        end
    end

endmodule