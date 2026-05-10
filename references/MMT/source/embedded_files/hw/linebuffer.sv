module linebuffer #(
    parameter int ADDR_WIDTH   = 9,  // 512 == 2 ** 9
    parameter int DATA_WIDTH_R = 4,  // Bits per pixel
    parameter int DATA_WIDTH_W = 32  // Bits per sliver
) (
    input [ADDR_WIDTH-1:0] addr_r,
    input [ADDR_WIDTH-1:0] addr_w,
    input [DATA_WIDTH_W-1:0] data_w,
    input we,
    clk,
    clear,
    output reg [DATA_WIDTH_R-1:0] data_r
);
  localparam int RATIO = DATA_WIDTH_W / DATA_WIDTH_R;
  localparam int DEPTH = 1 << ADDR_WIDTH;
  // Use a multi-dimensional packed array to model the different read/ram width
  reg [DATA_WIDTH_R-1:0] ram[DEPTH];
  reg [DATA_WIDTH_R-1:0] data_reg_r;

  always @(posedge clk) begin
    if (clear) for (int i = 0; i < DEPTH; i++) ram[i] <= 0;
    else if (we)
      for (int i = 0; i < RATIO; i++)
      if (|data_w[i*DATA_WIDTH_R+:DATA_WIDTH_R]) begin
        // $write("Drawing [%02X] to [%03X]\n", data_w[(i*DATA_WIDTH_R)+:DATA_WIDTH_R],
        //        addr_w + i[ADDR_WIDTH-1:0]);
        ram[addr_w+i[ADDR_WIDTH-1:0]] <= data_w[i*DATA_WIDTH_R+:DATA_WIDTH_R];
      end
    data_reg_r <= ram[addr_r];
  end

  assign data_r = data_reg_r;

endmodule : linebuffer
