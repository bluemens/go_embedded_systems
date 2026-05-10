
module gpu #(
    parameter int DATA_WIDTH = 32,
    parameter int ADDR_WIDTH = 18   // Complete overkill; most of this address space is not used
) (
    input logic clk,
    vga_clk,

    output logic [7:0] VGA_R,
    VGA_G,
    VGA_B,
    output logic VGA_CLK,
    VGA_HS,
    VGA_VS,
    VGA_BLANK_N,
    VGA_SYNC_N
);

  assign VGA_CLK = vga_clk;


endmodule
