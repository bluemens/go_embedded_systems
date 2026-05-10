// Quartus Prime SystemVerilog Template
//
// True Dual-Port RAM with different read/write addresses and single read/write clock
// and with a control for writing single bytes into the memory word; byte enable

// Read during write produces old data on ports A and B and old data on mixed ports
// For device families that do not support this mode (e.g. Stratix V) the ram is not inferred
module dual_port_be_ram #(
    parameter int BYTES_PER_WORD = 4,
    parameter int DATA_WIDTH = 32,
    parameter int ADDR_WIDTH
) (
    input [ADDR_WIDTH-1:0] addr_a,
    input [ADDR_WIDTH-1:0] addr_b,
    input [BYTES_PER_WORD-1:0] be_a,
    input [BYTES_PER_WORD-1:0] be_b,
    input [DATA_WIDTH-1:0] din_a,
    input [DATA_WIDTH-1:0] din_b,
    input we_a,
    we_b,
    clk,
    output [DATA_WIDTH-1:0] dout_a,
    output [DATA_WIDTH-1:0] dout_b
);
  localparam int BITS_PER_BYTE = 8;
  localparam int NUM_WORDS = 1 << ADDR_WIDTH;

  // model the RAM with two dimensional packed array
  logic [BYTES_PER_WORD-1:0][BITS_PER_BYTE-1:0] ram[NUM_WORDS];

  reg [DATA_WIDTH-1:0] reg_a, reg_b;

  // port A
  always @(posedge clk) begin
    if (we_a) begin
      if (be_a[0]) ram[addr_a][0] <= din_a[7:0];
      if (be_a[1]) ram[addr_a][1] <= din_a[15:8];
      if (be_a[2]) ram[addr_a][2] <= din_a[23:16];
      if (be_a[3]) ram[addr_a][3] <= din_a[31:24];

      // $write("VRAM: Writing [%08X][%04b] to rel. addr. [%04X]/byte addr. [%04X]\n", din_a, be_a,
      //        addr_a, {addr_a, 2'b0});
      // $write("VRAM: ram now holds [%08X]\n", ram[addr_a]);

      // for (int i = 0; i < BYTES_PER_WORD; i++)
      // if (be_a[i]) ram[addr_a][i] <= din_a[(i*BITS_PER_BYTE)+:BITS_PER_BYTE];
      // for (int i = 0; i < BYTES_PER_WORD; i++) begin
      //   if (be_a[i]) ram[addr_a][i] <= din_a[(i*BYTES_PER_WORD)+:BITS_PER_BYTE];
      // end
    end
    reg_a <= ram[addr_a];
  end

  assign dout_a = reg_a;

  // port B
  always @(posedge clk) begin
    if (we_b) begin
      if (be_b[0]) ram[addr_b][0] <= din_b[7:0];
      if (be_b[1]) ram[addr_b][1] <= din_b[15:8];
      if (be_b[2]) ram[addr_b][2] <= din_b[23:16];
      if (be_b[3]) ram[addr_b][3] <= din_b[31:24];

      // for (int i = 0; i < BYTES_PER_WORD; i++)
      // if (be_b[i]) ram[addr_b][i] <= din_b[(i*BITS_PER_BYTE)+:BITS_PER_BYTE];
    end
    reg_b <= ram[addr_b];
  end

  assign dout_b = reg_b;

endmodule : dual_port_be_ram


