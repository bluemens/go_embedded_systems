`timescale 1ns/1ps
`include "slack_update.sv"

module slack_update_tb;
  parameter STATE_DIM   = 6;
  parameter CONTROL_DIM = 12;
  parameter W           = 16;

  // Clock, reset, and start
  logic clk, reset, start;

  // Fixed-point data type
  typedef logic signed [W-1:0] fixed_t;

  // Inputs to DUT: state and dual, control and dual
  fixed_t x_k   [STATE_DIM];
  fixed_t y_k   [STATE_DIM];
  fixed_t u_k   [CONTROL_DIM];
  fixed_t g_k   [CONTROL_DIM];

  // Bounds
  fixed_t x_min, x_max;
  fixed_t u_min, u_max;

  // Outputs from DUT
  fixed_t v_k   [STATE_DIM];
  fixed_t z_k   [CONTROL_DIM];
  logic    done;

  // Instantiate DUT
  slack_update #(
    .STATE_DIM(STATE_DIM),
    .CONTROL_DIM(CONTROL_DIM),
    .W(W)
  ) dut (
    .clk    (clk),
    .reset  (reset),
    .start  (start),
    .x_k    (x_k),
    .y_k    (y_k),
    .u_k    (u_k),
    .g_k    (g_k),
    .x_min  (x_min),
    .x_max  (x_max),
    .u_min  (u_min),
    .u_max  (u_max),
    .v_k    (v_k),
    .z_k    (z_k),
    .done   (done)
  );

  // Clock generation: 10ns period
  initial clk = 0;
  always #5 clk = ~clk;

  initial begin
    integer i;
    // Initialize
    reset = 1;
    start = 0;
    x_min = 5; x_max =  6;
    u_min = 10; u_max =  12;
    #20;
    reset = 0;

    // Hardcoded input vectors (small natural numbers)
    x_k = '{16'sd1, 16'sd2, 16'sd3, 16'sd4, 16'sd5, 16'sd6};
    y_k = '{16'sd6, 16'sd5, 16'sd4, 16'sd3, 16'sd2, 16'sd1};

    u_k = '{16'sd1, 16'sd2, 16'sd3, 16'sd4,
            16'sd5, 16'sd6, 16'sd7, 16'sd8,
            16'sd9, 16'sd10,16'sd11,16'sd12};
    g_k = '{16'sd12,16'sd11,16'sd10,16'sd9,
            16'sd8, 16'sd7, 16'sd6, 16'sd5,
            16'sd4, 16'sd3, 16'sd2, 16'sd1};

    // Pulse start
    #10 start = 1;
    #10 start = 0;

    // Wait for done
    wait(done);

    // Display outputs
    $display("--- v_k (state slack) ---");
    for (i = 0; i < STATE_DIM; i++)
      $display("v_k[%0d] = %0d", i, v_k[i]);

    $display("--- z_k (control slack) ---");
    for (i = 0; i < CONTROL_DIM; i++)
      $display("z_k[%0d] = %0d", i, z_k[i]);

    #20 $finish;
  end
endmodule
