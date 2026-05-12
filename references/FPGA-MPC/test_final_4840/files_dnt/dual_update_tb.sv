`timescale 1ns/1ps

module tb_dual_update;
  // Match the DUT parameters
  localparam int STATE_DIM   = 6;
  localparam int CONTROL_DIM = 12;
  localparam int W           = 16;

  // Clock, reset, handshake
  logic clk;
  logic reset;
  logic start;
  logic done;

  // DUT inputs: state-related vectors (u and z are state)
  logic signed [W-1:0] u_k   [STATE_DIM];
  logic signed [W-1:0] z_k   [STATE_DIM];
  logic signed [W-1:0] y_k   [STATE_DIM];

  // DUT inputs: control-related vectors (x and v are control)
  logic signed [W-1:0] x_k   [CONTROL_DIM];
  logic signed [W-1:0] v_k   [CONTROL_DIM];
  logic signed [W-1:0] g_k   [CONTROL_DIM];

  // DUT outputs
  logic signed [W-1:0] y_out [STATE_DIM];
  logic signed [W-1:0] g_out [CONTROL_DIM];

  // Instantiate the dual_update module
  dual_update #(
    .STATE_DIM  (STATE_DIM),
    .CONTROL_DIM(CONTROL_DIM),
    .W          (W)
  ) dut (
    .clk    (clk),
    .reset  (reset),
    .start  (start),
    .u_k    (u_k),
    .z_k    (z_k),
    .y_k    (y_k),
    .x_k    (x_k),
    .v_k    (v_k),
    .g_k    (g_k),
    .y_out  (y_out),
    .g_out  (g_out),
    .done   (done)
  );

  // Clock generation: 10 ns period
  initial clk = 0;
  always #5 clk = ~clk;

  initial begin
    integer i;

    // 1) Reset sequence
    reset = 1;
    start = 0;
    #20;
    reset = 0;

    // 2) Initialize state-related inputs (u, z, y)
    for (i = 0; i < STATE_DIM; i++) begin
      u_k[i] = i + 1;          // 1,2,3,4,5,6
      z_k[i] = STATE_DIM - i;  // 6,5,4,3,2,1
      y_k[i] = 0;              // initial duals = 0
    end

    // 3) Initialize control-related inputs (x, v, g)
    for (i = 0; i < CONTROL_DIM; i++) begin
      x_k[i] = i + 1;               // 1,2,...,12
      v_k[i] = CONTROL_DIM - i;     // 12,11,...,1
      g_k[i] = 0;                   // initial duals = 0
    end

    // 4) Pulse start for one clock cycle
    @(posedge clk);
      start = 1;
    @(posedge clk);
      start = 0;

    // 5) Wait for the DUT to assert done
    wait (done);

    // 6) Display the results
    $display("--- y_out (state dual) ---");
    for (i = 0; i < STATE_DIM; i++)
      $display("y_out[%0d] = %0d", i, y_out[i]);

    $display("--- g_out (control dual) ---");
    for (i = 0; i < CONTROL_DIM; i++)
      $display("g_out[%0d] = %0d", i, g_out[i]);

    // 7) End simulation
    #20 $finish;
  end

endmodule

