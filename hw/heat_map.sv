/*
 * heat_map — 81-cell × 4-bit per-cell heat overlay for the MCTS demo.
 *
 * Storage shape mirrors board_mem.sv exactly: 81 4-bit cells, synchronous
 * write, combinational read, synchronous clear-all. 81 × 4 = 324 bits is
 * well below the M10K threshold, so Quartus will infer LUT-RAM / flops.
 *
 * Semantics (interpreted by the renderer in go_peripheral.sv):
 *   read_data == 4'b0000     no data yet — render normal COLOR_BOARD_BG
 *   read_data == 4'b0001..15 mapped through the 16-entry heat palette
 *                            (cold→warm, red→green) by go_peripheral.sv
 *
 * Software writes one cell at a time via the (REG_HEAT_IDX, REG_HEAT_VAL)
 * register pair; clear_all is pulsed via REG_HEAT_CLEAR.
 *
 * Reference: design-document.md §7.5 (MCTS rollout accelerator — heat overlay
 *            is the demo-side visualization of the per-cell win/visit table).
 */

module heat_map (
    input  logic        clk,
    input  logic        reset,          // active-high

    // Write port (driven by Avalon register decoder)
    input  logic        write_en,
    input  logic [6:0]  write_addr,     // 0..80
    input  logic [3:0]  write_data,     // heat value 0..15
    input  logic        clear_all,      // synchronous clear-all pulse

    // Read port (combinational, VGA pipe)
    input  logic [6:0]  read_addr,
    output logic [3:0]  read_data
);

    logic [3:0] heat [0:80];

    always_ff @(posedge clk) begin
        if (reset || clear_all) begin
            for (int i = 0; i < 81; i++) heat[i] <= 4'b0;
        end else if (write_en && write_addr < 7'd81) begin
            heat[write_addr] <= write_data;
        end
    end

    assign read_data = (read_addr < 7'd81) ? heat[read_addr] : 4'b0;

endmodule
