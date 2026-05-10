module sprite_drawer (
    input  logic        clk,
    input  logic        reset,

    input  logic        start,
    input  logic [9:0]  col_base,
    input  logic        flip,
    input  logic [7:0]  frame_id,
    input  logic [3:0]  row_off,

    // ROM
    output logic [15:0] rom_addr,
    input  logic [15:0] rom_q,

    output logic [9:0]  pixel_col,
    output logic [15:0] pixel_data,
    output logic        wren,
    output logic        done
);

    logic [3:0] idx;          // 0‑15
    logic [3:0] idx_d;
    logic       valid_d;

    always_ff @(posedge clk) begin
        if (reset) begin
            done  <= 1;
            idx   <= 0;
            idx_d <= 0;
            valid_d <= 0;
            rom_addr <= 0;
            wren <= 0;
        end else begin
            if (start) begin
                done  <= 0;
                valid_d <= 0;
                idx <= 0;
                idx_d <= 0; // FxxKKK, I forgot to reset it, it let me struggle for a long time.
                rom_addr <= {frame_id, row_off, 4'b0};
                wren <= 0;
            end
            else begin
                // Output
                pixel_col  <= flip ? (col_base + (10'd15 - {6'b0, idx_d})) : (col_base + {6'b0, idx_d});
                pixel_data <= rom_q;
                wren <= (!done) && (valid_d) && (rom_q[15] == 1'b0) && (pixel_col <= 639);
                if (!done) begin
                    idx_d   <= idx;
                    if (idx < 15) begin
                        rom_addr <= rom_addr + 16'd1;
                        idx      <= idx + 1;
                        valid_d  <= 1;
                    end

                    if (idx_d == 15) begin
                        done <= 1'b1;
                        valid_d <= 1'b0;
                    end
                end
            end 
        end
    end
endmodule
