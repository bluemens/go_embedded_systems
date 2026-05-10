module tuning(
    input logic[10:0] shift_address,
    input logic [5:0] shift_factor,
    output logic [10:0] mem_address
);

    assign mem_address[10] = shift_address[10];

    always_comb begin
        if (shift_factor == 0) begin
            mem_address[9:0] = shift_address[9:0];
        end 
        else begin
            if (shift_address < 1024) begin
                mem_address[9:0] = shift_address[9:0] - shift_factor;
            end
            else begin
                mem_address[9:0] = shift_address[9:0] + shift_factor;
            end
        end
    end
    
endmodule
