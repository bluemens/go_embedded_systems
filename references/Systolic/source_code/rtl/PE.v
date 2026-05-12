module PE(
    // interface to system
    input wire CLK,                         // CLK = 200MHz
    input wire RESET,                       // RESET, Negedge is active
    input wire EN,                          // enable signal for the accelerator, high for active
    input wire W_EN,                        // enable weight to flow
    // interface to PE row .....
    input wire signed [7:0] active_left,
    output reg signed [7:0] active_right,
    input wire signed [15:0] in_sum,       
    output wire signed [15:0] out_sum,     
    input wire signed [7:0] in_weight_above,
    output wire signed [7:0] out_weight_below
);

    // 内部信号
    wire signed [7:0] weight_next, weight_q;
    wire signed [7:0] input_next, input_q;

    wire signed [15:0] sum_next, sum_q;         

    wire signed [15:0] out_next, out_q;         


    // 输入寄存器逻辑
    assign input_next = EN ? active_left : input_q;

    // 求和输入寄存器逻辑
    assign sum_next = EN ? in_sum : sum_q;

    // weight 寄存器逻辑
    assign weight_next = (EN && W_EN) ? in_weight_above : weight_q;

    // 计算乘法部分
    wire signed [19:0] mul_result;
    assign mul_result = weight_q * input_q;

    // 计算加法部分
    wire signed [19:0] add_result;
    assign add_result = mul_result + in_sum;

    // 输出寄存器逻辑
    assign out_next = EN ? add_result[19:4] : out_q; 

    // active_right寄存器 (直接使用always块实现)
    always @(posedge CLK) begin
        active_right <= input_q;
    end

    assign out_sum = out_q;                      
    assign out_weight_below = weight_q;

    // 实例化寄存器，带有异步复位
    dffr #(.WIDTH(8)) input_reg (
        .clk(CLK),
        .rst_n(RESET),
        .d(input_next),
        .q(input_q)
    );

    dffr #(.WIDTH(8)) weight_reg (
        .clk(CLK),
        .rst_n(RESET),
        .d(weight_next),
        .q(weight_q)
    );

    dffr #(.WIDTH(16)) sum_reg (               
        .clk(CLK),
        .rst_n(RESET),
        .d(sum_next),
        .q(sum_q)
    );

    dffr #(.WIDTH(16)) out_reg (               
        .clk(CLK),
        .rst_n(RESET),
        .d(out_next),
        .q(out_q)
    );


endmodule
