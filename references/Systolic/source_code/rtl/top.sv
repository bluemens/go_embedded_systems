`define N 16        // input img size
`define Col 8       // weight column
`define Row 8       // weight row

// Input need to re-order in the buffer, so there would be no need to 
// has the col and row.
// 16'h0000             :     img_size
// 16'h0001-16'h0046    :     weight_data
// 16'h0050-16'h2050    :     input_data


// Why we don't use the addr map here?
// we don't really need to store the data in the top, but guide the data
// to the correct buffer.
// 4'h0: img_size
// 4'h1: weight_data
// 4'h2: input_data
// 4'h3: output_ready
// 4'h4: output_data

module top(
    input logic clk,
    input logic reset,
    input logic [7:0] data_in,
    input logic write,
    input logic read,
    input logic [3:0] addr,
    input logic chipselect,
    output logic [7:0] data_out
);
    logic [7:0] img_size;
    logic [7:0] input_data;
    logic [7:0] weight_data;
    logic [7:0] output_data;
    logic input_ld_en;
    logic weight_ld_en;
    logic output_sw_en;  
    logic [71:0] input_data_buffer;
    logic [7:0]  weight_data_buffer;
    logic [15:0] output_data_pe;
    logic input_ready;
    logic weight_ready;
    logic output_ready;
    logic input_done;
    logic weight_done;
    logic output_done;
    logic input_start;
    logic weight_start;
    logic output_start;
    // iowrite
    always_ff @(posedge clk) begin
        if (reset) begin
            img_size <= 16; // set default to 16
            input_data <= 0;
            weight_data <= 0;
            input_ld_en <= 0;
            weight_ld_en <= 0;
        end
        else begin
            if (chipselect && write) begin
                case (addr)
                    4'h0: begin
                        img_size <= data_in; 
                        weight_ld_en <= 0;
                        input_ld_en <= 0;
                    end
                    4'h1: begin
                        weight_data <= data_in;
                        weight_ld_en <= 1;
                        input_ld_en <= 0;
                    end
                    4'h2: begin
                        input_data <= data_in;
                        input_ld_en <= 1;
                        weight_ld_en <= 0;
                    end
                    default: begin
                        weight_ld_en <= 0;
                        input_ld_en <= 0;
                    end
                endcase
            end
            else begin
                weight_ld_en <= 0;
                input_ld_en <= 0;
            end
        end
    end
    // ioread
    logic read_latched; 
    logic read_prev;    // read signal in the previous clock cycle

    always_ff @(posedge clk) begin
        if (reset) begin
            read_latched <= 0;
            read_prev <= 0;
        end
        else begin
            // keep read latched until read is toggle
            if (chipselect && read && !read_prev) begin
                read_latched <= 1; 
            end
            else begin
                read_latched <= 0; 
            end
            read_prev <= read; 
        end
    end
    always_ff @(posedge clk) begin
        if (reset) begin
            data_out <= 0;
            output_sw_en <= 0;
        end
        else begin
            if (chipselect && read && !read_prev) begin
                case (addr)
                    4'h3: begin
                        output_sw_en <= 0;
                        data_out <= output_ready;
                    end
                    4'h4: begin
                        output_sw_en <= 1;
                        data_out <= output_data;
                    end
                    default: begin
                        data_out <= 0;
                        output_sw_en <= 0;
                    end
                endcase
            end
            else begin
                output_sw_en <= 0;
            end
        end
    end
    
    // Instantiate the buffer
    input_buffer input_buffer_inst(
        .clk(clk),
        .rst_n(!reset),
        .data_in(input_data),
        .rd_en(input_start),
        .wr_en(input_ld_en),
        .data_out(input_data_buffer),
        .ready(input_ready),
        .done(input_done)
    );

    weight_buffer weight_buffer_inst(
        .clk(clk),
        .rst_n(!reset),
        .data_in(weight_data),
        .rd_en(weight_start),
        .wr_en(weight_ld_en),
        .data_out(weight_data_buffer),
        .ready(weight_ready),
        .done(weight_done)
    );
    output_buffer output_buffer_inst(
        .clk(clk),
        .rst_n(!reset),
        .data_in(output_data_pe[15:8]),
        .rd_en(output_sw_en),
        .wr_en(output_start),
        .data_out(output_data),
        .ready(output_ready),
        .done(output_done)
    );
    ctrl ctrl_inst(
        .clk(clk),
        .rst_n(!reset),
        .input_data_ready(input_ready),
        .input_data(input_data_buffer),
        .weight_data_ready(weight_ready),
        .weight_data(weight_data_buffer),
        .output_start(output_start),
        .input_start(input_start),
        .weight_start(weight_start),
        .output_data(output_data_pe)
    );

endmodule