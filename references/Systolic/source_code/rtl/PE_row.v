module PE_row #(
    parameter num = 9
)(
    // 系统接口
    input wire CLK,                         
    input wire RESET,                       
    input wire EN,                          
    input wire W_EN,                        

    // PE阵列接口
    input wire signed [7:0] active_left,    
    input wire [num*8-1:0] in_weight_above,  
    output wire [num*8-1:0] out_weight_below,
    input wire [num*16-1:0] in_sum,          
    output wire [num*16-1:0] out_sum         

`ifdef DEBUG
    , output wire [num*8-1:0] out_active_right
`endif
);

    // 激活值级联线网
    wire [num*8-1:0] active_right;

`ifdef DEBUG
    assign out_active_right = active_right;
`endif

    genvar gi;
    generate
        for (gi = 0; gi < num; gi = gi + 1) begin: label
            if (gi == 0) begin
                PE PE_unit (
                    .CLK(CLK),
                    .RESET(RESET),
                    .EN(EN),
                    .W_EN(W_EN),
                    .active_left(active_left),
                    .active_right(active_right[8*(gi+1)-1:8*gi]),
                    .in_sum(in_sum[16*(gi+1)-1:16*gi]),
                    .out_sum(out_sum[16*(gi+1)-1:16*gi]),
                    .in_weight_above(in_weight_above[8*(gi+1)-1:8*gi]),
                    .out_weight_below(out_weight_below[8*(gi+1)-1:8*gi])
                );
            end else begin
                PE PE_unit (
                    .CLK(CLK),
                    .RESET(RESET),
                    .EN(EN),
                    .W_EN(W_EN),
                    .active_left(active_right[8*gi-1:8*(gi-1)]),
                    .active_right(active_right[8*(gi+1)-1:8*gi]),
                    .in_sum(in_sum[16*(gi+1)-1:16*gi]),
                    .out_sum(out_sum[16*(gi+1)-1:16*gi]),
                    .in_weight_above(in_weight_above[8*(gi+1)-1:8*gi]),
                    .out_weight_below(out_weight_below[8*(gi+1)-1:8*gi])
                );
            end
        end
    endgenerate

endmodule

