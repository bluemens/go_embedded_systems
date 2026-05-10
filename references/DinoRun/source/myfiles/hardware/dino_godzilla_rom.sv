module dino_godzilla_rom (
    input  logic        clk,
    input  logic [9:0]  address,
    output logic [15:0] data
);

    logic [15:0] memory [0:1023];

    initial begin
        $readmemh("godzilla_sprite.hex", memory);
    end

    always_ff @(posedge clk) begin
        data <= memory[address];
    end
endmodule
