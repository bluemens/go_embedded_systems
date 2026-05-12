

module PE_array #(

    parameter num1 = 9, 

    parameter num2 = 1

)(

    // interface to system
    input wire CLK,                         // CLK = 200MHz
    input wire RESET,                       // RESET, Negedge is active
    input wire EN,                          // enable signal for the accelerator, high for active
    input wire W_EN,                        // enable weight to flow



    input wire [num1*8-1:0] active_left,
    input wire [num2*8-1:0] in_weight_above,
    output wire [num2*8-1:0] out_weight_final,
    output wire [num2*16-1:0] out_sum_final



`ifdef DEBUG

    , output wire [num1*8-1:0] active_right

`endif

);



    // 定义传递权重和和的线网

    wire [num2*8-1:0] weight_connections[num1:0];

    wire [num2*16-1:0] sum_connections[num1:0];



    // 初始输入

    assign sum_connections[0] = {(num2*16){1'b0}};

    assign weight_connections[0] = in_weight_above;



    // 输出

    assign out_sum_final = sum_connections[num1];

    assign out_weight_final = weight_connections[num1];



    // 生成 PE_row 实例

    genvar gi;

    generate

        for (gi = 0; gi < num1; gi = gi + 1) begin: label

            PE_row #(

                .num(num2)

            ) PE_row_unit (

                .CLK(CLK),

                .RESET(RESET),

                .EN(EN),

                .W_EN(W_EN),

                .active_left(active_left[(gi+1)*8-1:gi*8]),

                .in_sum(sum_connections[gi]),

                .out_sum(sum_connections[gi+1]),

                .in_weight_above(weight_connections[gi]),

                .out_weight_below(weight_connections[gi+1])

            `ifdef DEBUG

                , .out_active_right(active_right[(gi+1)*8-1:gi*8])

            `endif

            );

        end

    endgenerate



endmodule

