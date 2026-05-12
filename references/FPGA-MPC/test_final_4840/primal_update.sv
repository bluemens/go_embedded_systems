// Riccati backward pass and forward rollout for X-update step using RAM
// Based on TinyMPC algorithm

module primal_update #(
    parameter STATE_DIM = 12,
    parameter INPUT_DIM = 4,
    parameter HORIZON = 30,
    parameter DATA_WIDTH = 16,
    parameter FRAC_BITS = 8,
    parameter ADDR_WIDTH = 9
)(
    input logic clk,
    input logic rst,
    input logic start,

    // RAM ports for all matrices and vectors
    output logic [ADDR_WIDTH-1:0] A_rdaddress,
    input  logic [DATA_WIDTH-1:0] A_data_out,

    output logic [ADDR_WIDTH-1:0] B_rdaddress,
    input  logic [DATA_WIDTH-1:0] B_data_out,

    output logic [ADDR_WIDTH-1:0] K_rdaddress,
    input  logic [DATA_WIDTH-1:0] K_data_out,

    output logic [ADDR_WIDTH-1:0] C1_rdaddress,
    input  logic [DATA_WIDTH-1:0] C1_data_out,

    output logic [ADDR_WIDTH-1:0] C2_rdaddress,
    input  logic [DATA_WIDTH-1:0] C2_data_out,

    output logic [ADDR_WIDTH-1:0] q_rdaddress,
    input  logic [DATA_WIDTH-1:0] q_data_out,

    output logic [ADDR_WIDTH-1:0] r_rdaddress,
    input  logic [DATA_WIDTH-1:0] r_data_out,

    output logic [ADDR_WIDTH-1:0] p_rdaddress,
    input  logic [DATA_WIDTH-1:0] p_data_out,

    output logic [ADDR_WIDTH-1:0] d_rdaddress,
    input  logic [DATA_WIDTH-1:0] d_data_out,

    output logic [ADDR_WIDTH-1:0] x_rdaddress,
    input  logic [DATA_WIDTH-1:0] x_data_out,

    output logic [ADDR_WIDTH-1:0] u_rdaddress,
    input  logic [DATA_WIDTH-1:0] u_data_out,

    output logic [ADDR_WIDTH-1:0] x_wraddress,
    output logic [DATA_WIDTH-1:0] x_data_in,
    output logic x_wren,

    output logic [ADDR_WIDTH-1:0] u_wraddress,
    output logic [DATA_WIDTH-1:0] u_data_in,
    output logic u_wren,

    output logic [ADDR_WIDTH-1:0] d_wraddress,
    output logic [DATA_WIDTH-1:0] d_data_in,
    output logic d_wren,

    output logic [ADDR_WIDTH-1:0] p_wraddress,
    output logic [DATA_WIDTH-1:0] p_data_in,
    output logic p_wren,

    input logic [31:0] active_horizon,
    input logic [STATE_DIM*DATA_WIDTH-1:0] x_init, //modify this 2d
    output logic done
);

    // FSM state definitions
    localparam IDLE         = 3'd0;
    localparam INIT         = 3'd1;
    localparam BACKWARD     = 3'd2;
    localparam FORWARD      = 3'd3;
    localparam DONE_STATE   = 3'd4;

    localparam FP_COMPUTE_X = 2'd0;
    localparam FP_STORE     = 2'd1;

    logic [2:0] state;
    logic [1:0] substate;
    logic [2:0] fp_state;
    logic [31:0] i, j, k;
    logic [DATA_WIDTH-1:0] temp_sum;
    logic [31:0] cycle_counter;

    always_ff @(posedge clk or posedge rst) begin
        if (rst) begin
            state <= IDLE;
            substate <= 0;
            fp_state <= 0;
            x_wren <= 0;
            u_wren <= 0;
            d_wren <= 0;
            p_wren <= 0;
            done <= 0;
            i <= 0;
            j <= 0;
            k <= 0;
            temp_sum <= 0;
            cycle_counter <= 0;
        end else begin
            case (state)
                IDLE: begin
                    if (start) begin
                        i <= 0;
                        state <= INIT;
                    end
                end

                INIT: begin
                    if (i < STATE_DIM) begin
                        x_wraddress <= i;
                        x_data_in <= x_init[i*DATA_WIDTH +: DATA_WIDTH]; //change this 2d
                        x_wren <= 1;
                        i <= i + 1;
                    end else begin
                        x_wren <= 0;
                        i <= 0;
                        k <= 0;
                        state <= FORWARD;
                        substate <= FP_COMPUTE_X;
                    end
                end

                FORWARD: begin
                    case (substate)
                        FP_COMPUTE_X: begin
                            case (fp_state)
                                0: begin
                                    temp_sum <= 0;
                                    j <= 0;
                                    fp_state <= 1;
                                end
                                1: begin
                                    if (j < STATE_DIM) begin
                                        A_rdaddress <= i * STATE_DIM + j;
                                        x_rdaddress <= k * STATE_DIM + j;
                                        fp_state <= 2;
                                    end else begin
                                        j <= 0;
                                        fp_state <= 4;
                                    end
                                end
                                2: fp_state <= 3;
                                3: begin
                                    temp_sum <= temp_sum + A_data_out * x_data_out;
                                    j <= j + 1;
                                    fp_state <= 1;
                                end
                                4: begin
                                    if (j < INPUT_DIM) begin
                                        B_rdaddress <= i * INPUT_DIM + j;
                                        u_rdaddress <= k * INPUT_DIM + j;
                                        fp_state <= 5;
                                    end else begin
                                        fp_state <= 7;
                                    end
                                end
                                5: fp_state <= 6;
                                6: begin
                                    temp_sum <= temp_sum + B_data_out * u_data_out;
                                    j <= j + 1;
                                    fp_state <= 4;
                                end
                                7: begin
                                    x_wraddress <= (k+1) * STATE_DIM + i;
                                    x_data_in <= temp_sum;
                                    x_wren <= 1;
                                    fp_state <= 8;
                                end
                                8: begin
                                    x_wren <= 0;
                                    if (i < STATE_DIM - 1) begin
                                        i <= i + 1;
                                        fp_state <= 0;
                                    end else begin
                                        i <= 0;
                                        substate <= FP_STORE;
                                        fp_state <= 0;
                                    end
                                end
                            endcase
                        end

                        FP_STORE: begin
                            // Placeholder for storing u[k] — similar to x above
                            if (k < active_horizon - 1) begin
                                k <= k + 1;
                                substate <= FP_COMPUTE_X;
                            end else begin
                                state <= DONE_STATE;
                            end
                        end
                    endcase
                end

                DONE_STATE: begin
                    done <= 1;
                    if (!start) begin
                        state <= IDLE;
                        done <= 0;
                    end
                end
            endcase
        end
    end

endmodule

