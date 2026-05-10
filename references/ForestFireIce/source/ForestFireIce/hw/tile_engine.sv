module tile_engine(
    input logic clk,
    input logic reset,
    input logic tile_start,
    input logic [1:0] tilemap_idx,
    input logic [9:0] vcount,
    output logic [5:0] tile_col,
    output logic [255:0] tile_data,
    output logic tile_done,
    output logic wren_tile_draw
);

// internal
logic [11:0] tilemap_addr;
logic [11:0] tile_pattern_addr;


logic [7:0] tile_id;
logic [9:0] next_vcount;
logic [5:0] col[2:0];
assign tile_col = col[2];

assign wren_tile_draw = (!tile_done) && (col[0] > 0);

assign tilemap_addr = tilemap_idx * 1200 +  (next_vcount >> 4) * 40 + col[0];
tilemap u_tilemap(
    .address 	(tilemap_addr  ),
    .clock   	(clk),
    .q       	(tile_id)
);

assign tile_pattern_addr = (tile_id << 4) + next_vcount[3:0];
tile_pattern u_tile_pattern(
    .address 	(tile_pattern_addr  ),
    .clock   	(clk),
    .q       	(tile_data)
);

always_ff @(posedge clk) begin
    if (reset) begin
        col[0] <= 0;
        col[1] <= 0;
        col[2] <= 0;
        tile_done <= 1;
    end else begin
        if (tile_start) begin
            col[0] <= 0;
            col[1] <= 0;
            col[2] <= 0;
            if (vcount < 479) begin
                next_vcount <= vcount + 1;
                tile_done <= 0;
            end else if (vcount >= 479 && vcount < 524) begin
                tile_done <= 1;
            end else if (vcount == 524) begin
                next_vcount <= 0;
                tile_done <= 0;
            end
        end else if (!tile_done) begin
            col[1] <= col[0];
            col[2] <= col[1];
            if (col[0] < 39) begin
                col[0] <= col[0] + 1;
            end
            if (col[2] == 39) begin
                tile_done <= 1;
            end
        end
    end
end
    
endmodule