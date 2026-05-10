/*
 * board_mem — 9x9 Go board tilemap (81 cells × 2 bits = 162 bits)
 *
 * Each cell holds a Stone value:
 *   2'b00 = EMPTY
 *   2'b01 = BLACK
 *   2'b10 = WHITE
 *   2'b11 = (reserved — currently unused)
 *
 * Synchronous write port (driven by the Avalon register decoder),
 * combinational read port (driven by the VGA scanout pipeline).
 *
 * On reset OR reset_all (a synchronous "clear board" pulse from the
 * RESET_BOARD register), all 81 cells return to EMPTY in one cycle.
 *
 * 162 bits is well below the M10K threshold; Quartus will infer LUT-RAM
 * or simple registers, which is what we want — combinational read with
 * zero latency keeps the VGA pipeline simple.
 *
 * Reference: design-document.md §10.3 (board_mem interface),
 *            references/Chess/source/hw/board_mem.sv (analogous tilemap).
 */

module board_mem (
    input  logic        clk,
    input  logic        reset,        // active high

    // Write port (from Avalon register decoder)
    input  logic        write_en,
    input  logic [6:0]  write_addr,   // cell index 0..80 (row*9 + col)
    input  logic [1:0]  write_data,   // Stone value
    input  logic        reset_all,    // synchronous clear-all pulse

    // Read port (combinational, to VGA scanout)
    input  logic [6:0]  read_addr,
    output logic [1:0]  read_data
);

    logic [1:0] cells [0:80];

    always_ff @(posedge clk) begin
        if (reset || reset_all) begin
            for (int i = 0; i < 81; i++) cells[i] <= 2'b00;
        end else if (write_en && write_addr < 7'd81) begin
            cells[write_addr] <= write_data;
        end
    end

    assign read_data = (read_addr < 7'd81) ? cells[read_addr] : 2'b00;

endmodule
