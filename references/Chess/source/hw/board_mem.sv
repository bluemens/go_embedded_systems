/*
 * Board memory to store color and piece
 *
 * Chess Group: Hooman Khaloo, Hongchi Liu, and Pengfei Yan
 * Spring 2025
 * Columbia University
 */

module board_mem (
    input logic clk,
    input logic rst,

    // Write port 1
    input logic write_en,
    input logic [5:0] write_addr1,
    input logic [3:0] data_in1,

    // Write port 2
    input logic [5:0] write_addr2,
    input logic [3:0] data_in2,

    // Read port
    input logic [5:0] read_addr,
    output logic [3:0] data_out
);

    /* Memory cells */
    logic [3:0] board[0:63];

    /* Write and reset logic */
    always_ff @(posedge clk) begin
        if (rst) begin
            integer i;
            
            /* Reset everything to 0000 */
            for (i = 0; i < 64; i++) begin
                board[i] <= 4'b0000;
            end

        end else begin
            if (write_en && (write_addr1 != write_addr2)) begin
                /* write enable is 1 and different addr, write for both addr */
                board[write_addr1] <= data_in1;
                board[write_addr2] <= data_in2;
            end else if (write_en) begin
                /* write enable is 1 and same addr, only write to addr1 */
                board[write_addr1] <= data_in1;
            end
        end
    end

    /* Read logic */
    assign data_out = board[read_addr];

endmodule
