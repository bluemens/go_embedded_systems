module peak_detector(
    input logic clk,
    input logic reset_n,
    input logic [15:0] source_real,
    input logic [15:0] source_imag,
    input logic sink_sop,
    input logic sink_eop,
    input logic sink_valid,
    output logic source_valid,
    output logic [9:0] peak_index
);

    logic [10:0] counter;
    logic [31:0] current_mag;
    logic [31:0] max_mag;

    data2mag data2mag_inst(
        .source_real(source_real),
        .source_imag(source_imag),
        .magnitude(current_mag)
    );

    always_ff @(posedge clk) begin
        if (!reset_n) begin
            peak_index <= 0;
            source_valid <= 0;
            counter <= 0;
        end
        else if (sink_valid) begin
            counter <= counter + 1;
            if (sink_sop) begin
                peak_index <= 0;
                max_mag <= current_mag;
                source_valid <= 0;
            end
            if (counter < 1024) begin
                if (current_mag > max_mag) begin
                    max_mag <= current_mag;
                    peak_index <= counter;
                end
            end
            if (sink_eop) begin
                counter <= 0;
                source_valid <= 1;
            end
        end
    end
endmodule

module data2mag(
    input logic [15:0] source_real,
    input logic [15:0] source_imag,
    output logic [31:0] magnitude
);

    // we are doing 2's complement multiplication
    logic [31:0] real_mult, imag_mult;
    logic real_sign, imag_sign;
    logic [31:0] real_abs, imag_abs;
    logic [31:0] real_mult_abs, imag_mult_abs;

    assign real_sign = source_real[15];
    assign imag_sign = source_imag[15];

    assign real_abs = real_sign ? ~source_real + 1'b1 : source_real;
    assign imag_abs = imag_sign ? ~source_imag + 1'b1 : source_imag;

    assign real_mult_abs = real_abs * real_abs;
    assign imag_mult_abs = imag_abs * imag_abs;


    always_comb begin
        real_mult = real_sign ? ~real_mult_abs + 1'b1 : real_mult_abs;
        imag_mult = imag_sign ? ~imag_mult_abs + 1'b1 : imag_mult_abs;
        magnitude = real_mult + imag_mult;
    end

endmodule
