/*
 * Avalon memory-mapped peripheral that generates VGA
 *
 * Chess Group: Hooman Khaloo, Hongchi Liu, and Pengfei Yan
 * Spring 2025
 * Columbia University
 */

module vga_board(
    input  logic        clk,
    input  logic        reset,
    input  logic [7:0]  writedata,
    input  logic        write,
    input               chipselect,
    input  logic [3:0]  address,

    output logic [7:0]  VGA_R, VGA_G, VGA_B,
    output logic        VGA_CLK, VGA_HS, VGA_VS, VGA_BLANK_n,
    output logic        VGA_SYNC_n
);

    localparam BOX_SIZE = 64;   // Each box: 64x64 pixels
    integer i, j;               // Loop index

    /* Set VGA Counters (embedded module instance) */
    logic [10:0] hcount;
    logic [9:0]  vcount;
    
    vga_counters counters (
        .clk(clk),
        .reset(reset),
        .hcount(hcount),
        .vcount(vcount),
        .VGA_HS(VGA_HS),
        .VGA_VS(VGA_VS),
        .VGA_BLANK_n(VGA_BLANK_n),
        .VGA_SYNC_n(VGA_SYNC_n),
        .VGA_CLK(VGA_CLK));
    
    /* Colors and themes */ 
    logic [7:0] background_r, background_g, background_b;
    logic [7:0] white_box_r, white_box_g, white_box_b;
    logic [7:0] black_box_r, black_box_g, black_box_b;
    logic [7:0] white_piece_r, white_piece_g, white_piece_b;
    logic [7:0] black_piece_r, black_piece_g, black_piece_b;

    logic [3:0]    color_id;
    logic [119:0]  current_color;

    theme_mem theme_mem_inst (.color_id(color_id), .color_out(current_color));

    assign {black_piece_r, black_piece_g, black_piece_b,
            white_piece_r, white_piece_g, white_piece_b,
            black_box_r, black_box_g, black_box_b,
            white_box_r, white_box_g, white_box_b,
            background_r, background_g, background_b } = current_color;

    /* Variable for reg to store info from software */
    logic [2:0] start_x, start_y;
    logic [2:0] end_x, end_y;
    logic [3:0] c_and_t;
    logic       done_move;

    // Checkerboard logic
    logic [2:0] x_box_no, y_box_no;
    logic       x_parity, y_parity;
    logic [3:0] local_x, local_y;

    // 0-7 number in both direction
    assign x_box_no = (hcount - 256) / BOX_SIZE;
    assign y_box_no = (vcount - 128) / BOX_SIZE;

    // parity used to define black/white
    assign x_parity = x_box_no % 2;         // 0 for black, 1 for white
    assign y_parity = y_box_no % 2;

    // local xy value in a 64*64 box
    assign local_x = (hcount - 256) % BOX_SIZE / 4;
    assign local_y = (vcount - 128) % BOX_SIZE / 4;

    // put address to read port of mem
    assign board_rd_addr = {y_box_no, x_box_no};

    /* Board memory interface: each cell is 4 bits = {color, type} */
    logic        board_write_en;
    logic [5:0]  board_wr_addr1, board_wr_addr2;
    logic [3:0]  board_wr_data1, board_wr_data2;
    logic [5:0]  board_rd_addr;
    logic [3:0]  board_rd_data;

    board_mem board_mem_inst (
        .clk(clk),
        .rst(reset),
        .write_en(board_write_en),
        .write_addr1(board_wr_addr1),
        .data_in1(board_wr_data1),
        .write_addr2(board_wr_addr2),
        .data_in2(board_wr_data2),
        .read_addr(board_rd_addr),
        .data_out(board_rd_data));

    /* Shape arrays */
    logic [15:0] pawn16 [0:15];
    logic [15:0] rook16 [0:15];
    logic [15:0] knight16 [0:15];
    logic [15:0] bishop16 [0:15];
    logic [15:0] queen16 [0:15];
    logic [15:0] king16 [0:15];

    /* Register interface */
    always_ff @(posedge clk) begin
        if (reset) begin
            /* Reset id here, the board is reset in board_mem.sv */
            color_id <= 0;
            done_move <= 0;
            board_write_en <= 0;
        end else if (done_move == 1) begin
            /* uplade end location */
            board_wr_addr1 <= {end_y, end_x};
            board_wr_data1 <= c_and_t;

            /* update start location to empty */
            board_wr_addr2 <= {start_y, start_x};
            board_wr_data2 <= 4'b0000;

            /* Write to board mem */
            board_write_en <= 1; // Will be cleared on next clock edge    
            done_move <= 0;
        end else if (board_write_en) begin
            board_write_en <= 0;
        end else if (chipselect && write) begin
        
            case (address)
                // Theme selection
                4'h0: begin
                    color_id <= writedata[3:0];
                end

                // Movement - lower part
                4'h1: begin
                    start_x       <= writedata[7:5];
                    end_x         <= writedata[4:2];
                    start_y[2:1]  <= writedata[1:0];
                end 
                
                // Movement - upper part and apply
                4'h2: begin
                    start_y[0]    <= writedata[7];
                    end_y         <= writedata[6:4];
                    c_and_t       <= writedata[3:0];
                end
                
                4'h3: begin
                    done_move <= 1;
                end
            endcase
        end 
    end

    /* Draw the board and pieces here */
    always_comb begin
        if (VGA_BLANK_n) begin
            if (hcount >= 256 && hcount < 768 && vcount >= 128 && vcount < 640) begin
                // Inside board range
                logic [2:0] piece_type;
                logic       piece_color;
                piece_type  = board_rd_data[2:0];
                piece_color = board_rd_data[3];

                // Check if pixel is part of pawn
                if ((piece_type == 3'b001 &&  pawn16[local_y][local_x])  ||
                    (piece_type == 3'b010 &&  rook16[local_y][local_x])  ||
                    (piece_type == 3'b011 && knight16[local_y][local_x]) ||
                    (piece_type == 3'b100 && bishop16[local_y][local_x]) ||
                    (piece_type == 3'b101 && queen16[local_y][local_x])  ||
                    (piece_type == 3'b110 && king16[local_y][local_x])) begin
                    if (piece_color == 1'b1) begin
                        // White piece
                        {VGA_R, VGA_G, VGA_B} = {white_piece_r, white_piece_g, white_piece_b};
                    end else begin
                        // Black piece
                        {VGA_R, VGA_G, VGA_B} = {black_piece_r, black_piece_g, black_piece_b};
                    end
                end else begin
                    // Piecs is empty, just draw the box
                    if ((x_parity ^ y_parity) == 0) begin
                        {VGA_R, VGA_G, VGA_B} = {white_box_r, white_box_g, white_box_b};
                    end else begin
                        {VGA_R, VGA_G, VGA_B} = {black_box_r, black_box_g, black_box_b};
                    end
                end
            end else begin
                // Outside board: background
                {VGA_R, VGA_G, VGA_B} = {background_r, background_g, background_b};
            end
        end else begin
            // VGA blanking
            {VGA_R, VGA_G, VGA_B} = 24'h000000;
        end
    end

    /* Shape Definitions (16x16) */
    initial begin
        pawn16[ 0] = 16'b0000000000000000;
        pawn16[ 1] = 16'b0000000000000000;
        pawn16[ 2] = 16'b0000000110000000;
        pawn16[ 3] = 16'b0000001111000000;
        pawn16[ 4] = 16'b0000011111100000;
        pawn16[ 5] = 16'b0000111111110000;
        pawn16[ 6] = 16'b0000111111110000;
        pawn16[ 7] = 16'b0000011111100000;
        pawn16[ 8] = 16'b0000001111000000;
        pawn16[ 9] = 16'b0000001111000000;
        pawn16[10] = 16'b0000011111100000;
        pawn16[11] = 16'b0000111111110000;
        pawn16[12] = 16'b0001111111111000;
        pawn16[13] = 16'b0000000000000000;
        pawn16[14] = 16'b0000000000000000;
        pawn16[15] = 16'b0000000000000000;

        rook16[ 0] = 16'b0000000000000000;
        rook16[ 1] = 16'b0000000000000000;
        rook16[ 2] = 16'b0000110110110000;
        rook16[ 3] = 16'b0000110110110000;   
        rook16[ 4] = 16'b0000110110110000;
        rook16[ 5] = 16'b0000111111110000;
        rook16[ 6] = 16'b0000011111100000;  
        rook16[ 7] = 16'b0000011111100000;
        rook16[ 8] = 16'b0000011111100000;
        rook16[ 9] = 16'b0000111111110000; 
        rook16[10] = 16'b0000111111110000;
        rook16[11] = 16'b0011111111111100;
        rook16[12] = 16'b0011111111111100;
        rook16[13] = 16'b0011111111111100;
        rook16[14] = 16'b0000000000000000;
        rook16[15] = 16'b0000000000000000; 

        knight16[ 0] = 16'b0000000000000000;
        knight16[ 1] = 16'b0000000000000000;
        knight16[ 2] = 16'b0001010000000000; 
        knight16[ 3] = 16'b0011111000000000;
        knight16[ 4] = 16'b0010101100000000;
        knight16[ 5] = 16'b0011111111000000;
        knight16[ 6] = 16'b0011111111100000; 
        knight16[ 7] = 16'b0011111111110000;
        knight16[ 8] = 16'b0011111111111000;
        knight16[ 9] = 16'b0001110111111100;
        knight16[10] = 16'b0000000111111100;
        knight16[11] = 16'b0000001111111100;
        knight16[12] = 16'b0000001111111100;
        knight16[13] = 16'b0000001111111100;
        knight16[14] = 16'b0000000000000000;
        knight16[15] = 16'b0000000000000000;

        bishop16[ 0] = 16'b0000000000000000;
        bishop16[ 1] = 16'b0000000000000000;
        bishop16[ 2] = 16'b0000001110000000;  
        bishop16[ 3] = 16'b0000111111100000;
        bishop16[ 4] = 16'b0001111011110000;
        bishop16[ 5] = 16'b0001110001110000;  
        bishop16[ 6] = 16'b0001100100110000;
        bishop16[ 7] = 16'b0001101110110000;  
        bishop16[ 8] = 16'b0001100100110000;
        bishop16[ 9] = 16'b0001110001110000;
        bishop16[10] = 16'b0001111111110000;
        bishop16[11] = 16'b0000111111100000;
        bishop16[12] = 16'b0011111111111000;
        bishop16[13] = 16'b0011111111111000;
        bishop16[14] = 16'b0000000000000000;
        bishop16[15] = 16'b0000000000000000;

        queen16[ 0] = 16'b0000000000000000;
        queen16[ 1] = 16'b0000000000000000;
        queen16[ 2] = 16'b0001000110001000;
        queen16[ 3] = 16'b0011101111011100;
        queen16[ 4] = 16'b0001000110001000;
        queen16[ 5] = 16'b0011100110011100;
        queen16[ 6] = 16'b0011100110011100;
        queen16[ 7] = 16'b0011111111111100;
        queen16[ 8] = 16'b0001111111111000;
        queen16[ 9] = 16'b0001111111111000;
        queen16[10] = 16'b0000111111110000;
        queen16[11] = 16'b0000111111110000;
        queen16[12] = 16'b0001111111111000;
        queen16[13] = 16'b0011111111111100;
        queen16[14] = 16'b0000000000000000;
        queen16[15] = 16'b0000000000000000;

        king16[ 0] = 16'b0000000000000000;
        king16[ 1] = 16'b0000000000000000;
        king16[ 2] = 16'b0000000110000000;  
        king16[ 3] = 16'b0000001111000000;
        king16[ 4] = 16'b0001101111011000;
        king16[ 5] = 16'b0011110110111100;
        king16[ 6] = 16'b0010111111110100;
        king16[ 7] = 16'b0010111111110100;  
        king16[ 8] = 16'b0011011001101100;
        king16[ 9] = 16'b0011011001101100;
        king16[10] = 16'b0001111001111000;
        king16[11] = 16'b0000111001110000;
        king16[12] = 16'b0001111111111000;
        king16[13] = 16'b0011111111111100;
        king16[14] = 16'b0000000000000000;
        king16[15] = 16'b0000000000000000;
    end

endmodule
