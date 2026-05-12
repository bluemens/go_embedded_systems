`timescale 1ns/1ps

module cost_update #(
    parameter STATE_DIM   = 12,             // Dimension of state vector (nx)
    parameter INPUT_DIM   = 4,              // Dimension of input vector (nu)
    parameter HORIZON     = 30,             // Maximum MPC horizon length (N)
    parameter DATA_WIDTH  = 16,             // 16-bit fixed point
    parameter FRAC_BITS   = 8,              // Number of fractional bits for fixed point
    parameter ADDR_WIDTH  = 9               // Address width for memory access
)(
    input  logic                         clk,
    input  logic                         rst,
    input  logic                         start,

    // State and input trajectories
    output logic [ADDR_WIDTH-1:0]        x_rdaddress,
    input  logic [DATA_WIDTH-1:0]        x_data_out,
    output logic [ADDR_WIDTH-1:0]        u_rdaddress,
    input  logic [DATA_WIDTH-1:0]        u_data_out,

    // Reference trajectories
    input  wire [DATA_WIDTH-1:0]        x_ref [STATE_DIM][HORIZON],
    input  wire [DATA_WIDTH-1:0]        u_ref [INPUT_DIM][HORIZON-1],

    // Cost matrices
    output logic [ADDR_WIDTH-1:0]        R_rdaddress,
    input  logic [DATA_WIDTH-1:0]        R_data_out,
    output logic [ADDR_WIDTH-1:0]        Q_rdaddress,
    input  logic [DATA_WIDTH-1:0]        Q_data_out,
    output logic [ADDR_WIDTH-1:0]        P_rdaddress,
    input  logic [DATA_WIDTH-1:0]        P_data_out,

    // Regularization parameter
    input  logic [DATA_WIDTH-1:0]        rho,

    // Active horizon length
    input  logic [31:0]                  active_horizon,

    // Residual memory write interfaces
    output logic [ADDR_WIDTH-1:0]        r_wraddress,
    output logic [DATA_WIDTH-1:0]        r_data_in,
    output logic                         r_wren,
    output logic [ADDR_WIDTH-1:0]        q_wraddress,
    output logic [DATA_WIDTH-1:0]        q_data_in,
    output logic                         q_wren,
    output logic [ADDR_WIDTH-1:0]        p_wraddress,
    output logic [DATA_WIDTH-1:0]        p_data_in,
    output logic                         p_wren,

    // Residual calculation outputs
    output logic [DATA_WIDTH-1:0]        pri_res_u,
    output logic [DATA_WIDTH-1:0]        pri_res_x,

    // Done signal
    output logic                         done
);

    // State machine states
    localparam IDLE           = 3'd0;
    localparam UPDATE_R       = 3'd1;
    localparam UPDATE_Q       = 3'd2;
    localparam UPDATE_P       = 3'd3;
    localparam CALC_RESIDUALS = 3'd4;
    localparam DONE_STATE     = 3'd5;

    // State variables
    logic [2:0]   state;
    logic [31:0]  k, i, state_timer;
    logic [31:0]  read_stage, write_stage;

    // Temp variables for computation
    logic [DATA_WIDTH-1:0] temp_r, temp_q, temp_p;
    logic [DATA_WIDTH-1:0] temp_u, temp_x;
    logic [DATA_WIDTH-1:0] max_pri_res_u, max_pri_res_x;

    always_ff @(posedge clk or posedge rst) begin
        if (rst) begin
            // reset state
            state         <= IDLE;
            done          <= 1'b0;
            i             <= 0;
            k             <= 0;
            state_timer   <= 0;
            read_stage    <= 0;
            write_stage   <= 0;
            pri_res_u     <= 0;
            pri_res_x     <= 0;
            max_pri_res_u <= 0;
            max_pri_res_x <= 0;

            // initialize read addresses
            x_rdaddress <= 0;
            u_rdaddress <= 0;
            R_rdaddress <= 0;
            Q_rdaddress <= 0;
            P_rdaddress <= 0;

            // initialize residual write interfaces
            r_wraddress <= 0;
            q_wraddress <= 0;
            p_wraddress <= 0;
            r_data_in   <= 0;
            q_data_in   <= 0;
            p_data_in   <= 0;
            r_wren      <= 1'b0;
            q_wren      <= 1'b0;
            p_wren      <= 1'b0;
        end else begin
            case (state)
                IDLE: begin
                    if (start) begin
                        state         <= UPDATE_R;
                        done          <= 1'b0;
                        k             <= 0;
                        i             <= 0;
                        state_timer   <= 0;
                        read_stage    <= 0;
                        write_stage   <= 0;
                        max_pri_res_u <= 0;
                        max_pri_res_x <= 0;
                    end
                end

                UPDATE_R: begin
                    // r = ?R*u_ref ? rho*(z?y)
                    state_timer <= state_timer + 1;
                    if (state_timer == 1) begin
                        read_stage <= 1;
                        k <= 0; i <= 0;
                    end
                    if (read_stage == 1) begin
                        if (k < active_horizon && i < INPUT_DIM) begin
                            int index;
				index = k*INPUT_DIM + i;
                            case (state_timer % 6)
                                1: begin
                                    R_rdaddress <= i;
                                    u_rdaddress <= index;
                                end
                                3: temp_u <= u_data_out;
                                4: begin
                                    temp_r     = -R_data_out * temp_u;
                                    r_data_in  <= temp_r - rho * (temp_u - temp_u);
                                    r_wraddress<= index;
                                    r_wren     <= 1'b1;
                                end
                                default: begin
                                    r_wren <= 1'b0;
                                    i <= i + 1;
                                    if (i == INPUT_DIM-1) begin
                                        i <= 0;
                                        k <= k + 1;
                                    end
                                end
                            endcase
                        end else begin
                            read_stage <= 0;
                            r_wren     <= 1'b0;
                            state      <= UPDATE_Q;
                        end
                    end
                end

                UPDATE_Q: begin
                    // q = ?Q*x_ref ? rho*(v?g)
                    state_timer <= state_timer + 1;
                    if (state_timer == 1) begin
                        read_stage <= 1;
                        k <= 0; i <= 0;
                    end
                    if (read_stage == 1) begin
                        if (k < active_horizon && i < STATE_DIM) begin
                            int index;
				index = k*STATE_DIM + i;
                            case (state_timer % 6)
                                1: begin
                                    Q_rdaddress <= i;
                                    x_rdaddress <= index;
                                end
                                3: temp_x <= x_data_out;
                                4: begin
                                    temp_q     = -Q_data_out * temp_x;
                                    q_data_in  <= temp_q - rho * (temp_x - temp_x);
                                    q_wraddress<= index;
                                    q_wren     <= 1'b1;
                                end
                                default: begin
                                    q_wren <= 1'b0;
                                    i <= i + 1;
                                    if (i == STATE_DIM-1) begin
                                        i <= 0;
                                        k <= k + 1;
                                    end
                                end
                            endcase
                        end else begin
                            read_stage <= 0;
                            q_wren     <= 1'b0;
                            state      <= UPDATE_P;
                        end
                    end
                end

                UPDATE_P: begin
                    // p = ?P*x_ref (terminal cost)
                    state_timer <= state_timer + 1;
                    if (state_timer == 1) begin
                        read_stage <= 1;
                        k <= 0; i <= 0;
                    end
                    if (read_stage == 1) begin
                        if (k < active_horizon && i < STATE_DIM) begin
                            int index;
				index = k*STATE_DIM + i;
                            case (state_timer % 6)
                                1: begin
                                    P_rdaddress <= i;
                                    x_rdaddress <= index;
                                end
                                3: temp_x <= x_data_out;
                                4: begin
                                    temp_p     = -P_data_out * temp_x;
                                    p_data_in  <= temp_p;
                                    p_wraddress<= index;
                                    p_wren     <= 1'b1;
                                end
                                default: begin
                                    p_wren <= 1'b0;
                                    i <= i + 1;
                                    if (i == STATE_DIM-1) begin
                                        i <= 0;
                                        k <= k + 1;
                                    end
                                end
                            endcase
                        end else begin
                            read_stage <= 0;
                            p_wren     <= 1'b0;
                            state      <= CALC_RESIDUALS;
                        end
                    end
                end

                CALC_RESIDUALS: begin
                    pri_res_u <= max_pri_res_u;
                    pri_res_x <= max_pri_res_x;
                    state     <= DONE_STATE;
                end

                DONE_STATE: begin
                    done   <= 1'b1;
                    r_wren <= 1'b0;
                    q_wren <= 1'b0;
                    p_wren <= 1'b0;
                    if (!start) begin
                        state <= IDLE;
                        done  <= 1'b0;
                    end
                end

                default: state <= IDLE;
            endcase
        end
    end

endmodule
