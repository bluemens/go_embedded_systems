//------------------------------------------------------------------------------
// driver_interface.sv (IRQ removed – tied low)
//------------------------------------------------------------------------------
`timescale 1ns/1ps

module driver_interface #(
    parameter int DATA_SIZE = 28
)(
    // ─── Bus Output ───────────────────────────────────────────────────────
    input  logic                   clk,       // 50 MHz
    input  logic                   reset,     // synchronous active‑high
    input  logic                   chipselect,
    input  logic                   address,   // only bit‑0 used
    input  logic                   read,
    output logic [31:0]            read_data,

    // ─── FFT input ────────────────────────────────────────────────────────
    input  logic                   source_valid,
    input  logic [DATA_SIZE-1:0]   source_data,
    output logic                   source_ready
);

    // Single data register
    logic [DATA_SIZE-1:0] data_reg;
    logic data_full;

    // ─── Data handling ───────────────────────────────────────────────────────
    always_ff @(posedge clk) begin
        if (reset) begin
            data_reg <= '0;
            data_full <= 1'b0;
            read_data <= '0;
        end else begin
            if (source_valid && !data_full) begin
                data_reg <= source_data;
                data_full <= 1'b1;
            end else if (chipselect && read && data_full && (address == 1'b0)) begin
                read_data <= {{32-DATA_SIZE{1'b0}}, data_reg};
                data_full <= 1'b0;
            end
        end
    end

    // ─── Ready signal to producer ────────────────────────────────────────────
    assign source_ready = !data_full;  // ready when no data in our register

endmodule
