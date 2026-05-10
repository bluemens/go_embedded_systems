module sprite_rom #(
    parameter ADDR_WIDTH = 12,  // enough for 4096 pixels
    parameter DATA_WIDTH = 12,  // 12-bit RGB
    parameter DEPTH = 1024,     // e.g., 32x32 sprite → 1024 pixels
    parameter INIT_FILE = ""    // MIF file name
)(
    input  logic [ADDR_WIDTH-1:0] addr,
    output logic [DATA_WIDTH-1:0] color
);
    logic [DATA_WIDTH-1:0] mem [0:DEPTH-1];

    initial begin
        $readmemh(INIT_FILE, mem);  // load from .mif file
    end

    always_comb begin
        if (addr < DEPTH)
            color = mem[addr];
        else
            color = 12'h000;  // black if out of range
    end
endmodule
