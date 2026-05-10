module dino_cac_tog_rom (
    input  logic        clk,
    input  logic [11:0] address,          
    output logic [15:0] data             
);

    logic [15:0] memory [0:2047];         

    initial begin
        $readmemh("better_cactus_64x32.hex", memory);
    end

    always_ff @(posedge clk) begin
        data <= memory[address];
    end
endmodule
