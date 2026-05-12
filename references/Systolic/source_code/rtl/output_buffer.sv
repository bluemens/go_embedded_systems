`define OUTPUT_SIZE 14
module output_buffer(
    input logic clk,
    input logic rst_n,
    input logic [7:0] data_in,
    input logic wr_en,
    input logic rd_en,
    output logic [7:0] data_out,
    output logic ready,
    output logic done
);

    // Memory banks
    logic [7:0] output_bank0 [`OUTPUT_SIZE * `OUTPUT_SIZE-1:0];

    // Write and read address counters
    logic [$clog2(`OUTPUT_SIZE * `OUTPUT_SIZE)-1:0] wr_addr;
    logic [$clog2(`OUTPUT_SIZE * `OUTPUT_SIZE)-1:0] rd_addr;



    // Write logic
    always_ff @(posedge clk) begin
        if (!rst_n) begin
            wr_addr <= 0;
            ready <= 0;
        end else if (wr_en) begin
            output_bank0[wr_addr] <= data_in;
            wr_addr <= wr_addr + 1;
            if (wr_addr == `OUTPUT_SIZE * `OUTPUT_SIZE-1) begin
                ready <= 1;
            end
            else begin
                ready <= 0; // Reset ready when not writing
            end
        end
        else if (done) begin
            wr_addr <= 0;
            ready <= 0; // Reset ready when done
        end
    end

    // Read logic
    always_ff @(posedge clk ) begin
        if (!rst_n) begin
            rd_addr <= 0;
            done <= 0;
        end else if (rd_en) begin
            rd_addr <= rd_addr + 1;
            if (rd_addr == `OUTPUT_SIZE * `OUTPUT_SIZE-1) begin
                done <= 1; // Indicate that reading is done
            end else begin
                done <= 0; // Reset done when not reading
            end
        end if (!ready) begin
            rd_addr <= 0;
            done <= 0; // Reset done when not ready
        end
    end

    // Output logic
    always_comb begin
        if (rd_en && (rd_addr < `OUTPUT_SIZE*`OUTPUT_SIZE)) begin
            data_out = output_bank0[rd_addr];
        end else begin
            data_out = 8'b0; // Default value when not reading
        end
    end
endmodule