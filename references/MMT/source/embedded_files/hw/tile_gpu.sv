module tile_gpu #(
    parameter int NUM_BG_LAYERS = 2,
    parameter int TILE_ADDR_WIDTH = 12 + $clog2(NUM_BG_LAYERS),
    parameter int TEXTURE_ADDR_WIDTH = 13,
    parameter int PALETTE_ADDR_WIDTH = 7,
    parameter int TILE_DATA_WIDTH = 16,
    parameter int TEXTURE_DATA_WIDTH = 32,
    parameter int BPP = 4,  // Bits per pixel

    parameter int VGA_COUNTER_WIDTH = 10,
    parameter logic [VGA_COUNTER_WIDTH:0] CYCLES_PER_SLIVER = 32,
    parameter logic [VGA_COUNTER_WIDTH-1:0] HACTIVE,
    parameter logic [VGA_COUNTER_WIDTH-1:0] VACTIVE,
    parameter logic [VGA_COUNTER_WIDTH-1:0] HTOTAL,
    parameter logic [VGA_COUNTER_WIDTH-1:0] VTOTAL
) (
    input clk,
    vga_clk,
    blank_n,
    reset,
    input [VGA_COUNTER_WIDTH-1:0] h_count,
    v_count,
    input [VGA_COUNTER_WIDTH-2:0] next_scroll_x,
    next_scroll_y,
    input [TILE_DATA_WIDTH-1:0] tile_data,  // Tile data from previous fetch
    input [TEXTURE_DATA_WIDTH-1:0] texture_data,  // Texture data from previous fetch

    output logic [TILE_ADDR_WIDTH-1:0] tile_addr,  // Which tile address to fetch
    output logic [TEXTURE_ADDR_WIDTH-1:0] texture_addr,  // Which texture sliver to fetch
    // Palette address to draw in front of sprites
    output logic [PALETTE_ADDR_WIDTH-1:0] fg_palette_addr,
    // Palette address to draw behind sprites
    output logic [PALETTE_ADDR_WIDTH-1:0] bg_palette_addr
);
  localparam logic [VGA_COUNTER_WIDTH:0] HACTIVE_RAW = {HACTIVE, 1'b0};
  localparam logic [VGA_COUNTER_WIDTH:0] HTOTAL_RAW = {HTOTAL, 1'b0};
  localparam int TILE_CYCLE_WIDTH = $clog2(CYCLES_PER_SLIVER);

  localparam logic [TILE_CYCLE_WIDTH-1:0] FETCH_ATTR_START = 0;
  localparam logic [TILE_CYCLE_WIDTH-1:0] FETCH_ATTR_END = NUM_BG_LAYERS[TILE_CYCLE_WIDTH-1:0];
  localparam logic [TILE_CYCLE_WIDTH-1:0] FETCH_TEXTURE_START = FETCH_ATTR_START + 1;
  localparam logic [TILE_CYCLE_WIDTH-1:0] FETCH_TEXTURE_END = FETCH_ATTR_END + 1;
  localparam logic [TILE_CYCLE_WIDTH-1:0] DRAW_SLIVER_START = FETCH_TEXTURE_START + 1;
  localparam logic [TILE_CYCLE_WIDTH-1:0] DRAW_SLIVER_END = FETCH_TEXTURE_END + 1;

  // We want to use the 50MHz clock in its entirety
  logic [VGA_COUNTER_WIDTH:0] h_count_raw;
  logic [VGA_COUNTER_WIDTH-2:0] screen_x, screen_y;
  logic [TILE_CYCLE_WIDTH-1:0] tile_render_cycle;
  assign h_count_raw = {h_count, vga_clk};
  assign screen_x = h_count[VGA_COUNTER_WIDTH-1:1];
  assign screen_y = v_count[VGA_COUNTER_WIDTH-1:1];
  // assign screen_x_abs = screen_x + scroll_x;
  // assign screen_y_abs = screen_y + scroll_y;
  assign tile_render_cycle = h_count_raw[TILE_CYCLE_WIDTH-1:0];

  logic fetch_attr, fetch_texture, draw_sliver;
  assign fetch_attr = tile_render_cycle < FETCH_ATTR_END;
  assign fetch_texture = tile_render_cycle >= FETCH_TEXTURE_START
                        && tile_render_cycle < FETCH_TEXTURE_END;
  assign draw_sliver = tile_render_cycle >= DRAW_SLIVER_START
                        && tile_render_cycle < DRAW_SLIVER_END;

  logic on_pre_render_line, before_next_line;
  logic draw_first_two, draw_rest, draw_any;
  // Are we on the line right before we turn on blanking?
  assign on_pre_render_line = v_count == VTOTAL - 1;
  // Are we two tiles before we start drawing again?
  assign before_next_line = h_count_raw >= HTOTAL_RAW - (CYCLES_PER_SLIVER << 1);

  // Draw the first two tiles of the next scanline
  assign draw_first_two = (on_pre_render_line || (v_count < VACTIVE - 1)) && before_next_line;
  // Draw two tiles ahead of the tile currently being drawn; draw the rest of
  // the tiles on this scanline
  assign draw_rest = blank_n && h_count_raw < HACTIVE_RAW - CYCLES_PER_SLIVER;
  assign draw_any = draw_first_two | draw_rest;

  // Scrolling is only updated once per frame, to avoid jittering
  logic [VGA_COUNTER_WIDTH-2:0] scroll_x, scroll_y;
  always_ff @(posedge clk or posedge reset) begin
    if (reset) begin
      scroll_x <= next_scroll_x;
      scroll_y <= next_scroll_y;
    end else if (on_pre_render_line && ~|h_count_raw) begin
      scroll_x <= next_scroll_x;
      scroll_y <= next_scroll_y;
    end
  end

  /* verilator lint_off UNUSEDSIGNAL */
  logic [VGA_COUNTER_WIDTH-2:0] tile_pos_x;
  /* verilator lint_on UNUSEDSIGNAL */
  logic [VGA_COUNTER_WIDTH-2:0] tile_pos_y;

  logic [5:0] tile_ind_x, tile_ind_y;

  assign tile_ind_x = tile_pos_x[VGA_COUNTER_WIDTH-2:3];
  assign tile_ind_y = tile_pos_y[VGA_COUNTER_WIDTH-2:3];

  always_comb begin
    tile_pos_x = 0;
    tile_pos_y = 0;

    if (draw_first_two) begin
      // Equivalent to scroll_x + screen_x - (HTOTAL - 2 * CYCLES_PER_TILE)
      tile_pos_x = scroll_x + {4'b0, screen_x[TILE_CYCLE_WIDTH-1:0]};

      if (on_pre_render_line) tile_pos_y = scroll_y;
      else tile_pos_y = screen_y + scroll_y + {8'b0, v_count[0]};

    end else if (draw_rest) begin
      tile_pos_y = scroll_y + screen_y;
      tile_pos_x = scroll_x + screen_x + 16;
    end

    // if (vga_clk && draw_any)
    //   $write("[%0d][%0d] Drawing [%0d][%0d]\n", h_count, v_count, tile_draw_x, tile_draw_y);
  end

  // We also need to save a couple of values
  logic [2:0] prev_tile_off_y;
  always_ff @(posedge clk) begin
    prev_tile_off_y <= tile_pos_y[2:0];
  end

  /* verilator lint_off UNUSEDSIGNAL */
  logic [TEXTURE_ADDR_WIDTH - 4:0] tile_texture_ind;
  logic [PALETTE_ADDR_WIDTH - 5:0] next_tile_palette_ind, tile_palette_ind;

  // TODO: Add tile priority and flipping
  logic tile_flip_v, tile_flip_h;
  logic next_tile_priority, tile_priority;
  /* verilator lint_on UNUSEDSIGNAL */

  assign tile_flip_v = tile_data[15];
  assign tile_flip_h = tile_data[14];
  assign next_tile_priority = tile_data[13];
  assign next_tile_palette_ind = tile_data[12:10];
  assign tile_texture_ind = tile_data[9:0];

  always_ff @(posedge clk) begin
    tile_palette_ind <= next_tile_palette_ind;
    tile_priority <= next_tile_priority;
  end

  logic [TEXTURE_DATA_WIDTH-1:0] next_fg_palette_offs;
  logic [TEXTURE_DATA_WIDTH-1:0] next_bg_palette_offs;
  logic [TEXTURE_DATA_WIDTH-1:0] next_fg_palette_inds;
  logic [TEXTURE_DATA_WIDTH-1:0] next_bg_palette_inds;

  logic [TEXTURE_DATA_WIDTH * 2 - 1:0] fg_palette_offs;
  logic [TEXTURE_DATA_WIDTH * 2 - 1:0] bg_palette_offs;
  logic [TEXTURE_DATA_WIDTH * 2 - 1:0] fg_palette_inds;
  logic [TEXTURE_DATA_WIDTH * 2 - 1:0] bg_palette_inds;

  always_ff @(posedge clk or posedge reset)
    if (reset) begin
      next_fg_palette_offs <= 0;
      next_bg_palette_offs <= 0;
      next_fg_palette_inds <= 0;
      next_bg_palette_inds <= 0;
      fg_palette_offs <= 0;
      fg_palette_inds <= 0;
      bg_palette_offs <= 0;
      bg_palette_inds <= 0;
    end else begin
      // if (blank_n && h_count == 0)
      //   $write(
      //       "[%0d][%0d]\t[%08X][%08X][%08X][%08X]\t[%0X][%0X]\n",
      //       h_count_raw,
      //       v_count,
      //       fg_palette_inds[31:0],
      //       fg_palette_offs[31:0],
      //       bg_palette_inds[31:0],
      //       bg_palette_offs[31:0],
      //       fg_palette_addr,
      //       bg_palette_addr
      //   );
      // if (draw_any && v_count < 8) begin
      //   if (fetch_attr)
      //     $write(
      //         "[%0d][%0d]: Fetching tile Layer:[%0t] X:[%0d] Y:[%0d]\tAddress:[%0X]\n",
      //         h_count_raw,
      //         v_count,
      //         tile_addr[12],
      //         tile_ind_x,
      //         tile_ind_y,
      //         tile_addr
      //     );
      //   if (fetch_texture)
      //     $write(
      //         "[%0d][%0d]: Tile has data [%04X] palette [%0d] texture [%0d] (offset %0d)\t Address [%04X]\n",
      //         h_count_raw - 1,
      //         v_count,
      //         tile_data,
      //         next_tile_palette_ind,
      //         tile_texture_ind,
      //         prev_tile_off_y,
      //         {
      //           texture_addr, 2'b0
      //         }
      //     );
      //   if (draw_sliver)
      //     $write("[%0d][%0d]: Tile has sliver [%08X]\n", h_count_raw - 2, v_count, texture_data);
      // end

      // Draw slivers in increasing importance, making sure not to draw
      // tranparent
      // if (draw_sliver && draw_any) begin
      //   next_bg_palette_offs <= texture_data;
      //   next_bg_palette_inds <= {8{1'b0, tile_palette_ind}};
      // end

      if (draw_sliver && draw_any)
        for (int i = 0; i < TEXTURE_DATA_WIDTH; i += BPP)
        if (|texture_data[i+:BPP])
          if (tile_priority) begin
            next_fg_palette_offs[i+:BPP]   <= texture_data[i+:BPP];
            next_fg_palette_inds[i+:BPP-1] <= tile_palette_ind;
          end else begin
            next_bg_palette_offs[i+:BPP]   <= texture_data[i+:BPP];
            next_bg_palette_inds[i+:BPP-1] <= tile_palette_ind;
          end

      // At the end of each sliver, load in the next one
      if (&tile_render_cycle) begin
        // $write("[%0d][%0d] Next bg offs: [%08X]\n",
        //        h_count_raw[VGA_COUNTER_WIDTH:TILE_CYCLE_WIDTH], v_count, next_bg_palette_offs);

        if (draw_any) begin
          // Write the next buffers
          fg_palette_offs <= {
            next_fg_palette_offs, fg_palette_offs[(TEXTURE_DATA_WIDTH-1)+BPP : BPP]
          };
          fg_palette_inds <= {
            next_fg_palette_inds, fg_palette_inds[(TEXTURE_DATA_WIDTH-1)+BPP : BPP]
          };
          bg_palette_offs <= {
            next_bg_palette_offs, bg_palette_offs[(TEXTURE_DATA_WIDTH-1)+BPP : BPP]
          };
          bg_palette_inds <= {
            next_bg_palette_inds, bg_palette_inds[(TEXTURE_DATA_WIDTH-1)+BPP : BPP]
          };
        end

        // Clear the next buffers
        next_fg_palette_offs <= 0;
        next_fg_palette_inds <= 0;
        next_bg_palette_offs <= 0;
        next_bg_palette_inds <= 0;
      end else if (&tile_render_cycle[1:0]) begin
        // Still ensure that things are shifted
        fg_palette_offs <= fg_palette_offs >> BPP;
        bg_palette_offs <= bg_palette_offs >> BPP;
        fg_palette_inds <= fg_palette_inds >> BPP;
        bg_palette_inds <= bg_palette_inds >> BPP;
      end

    end

  assign fg_palette_addr = {
    fg_palette_inds[{1'b0, scroll_x[2:0], 2'b0}+:(BPP-1)],
    fg_palette_offs[{1'b0, scroll_x[2:0], 2'b0}+:BPP]
  };
  assign bg_palette_addr = {
    bg_palette_inds[{1'b0, scroll_x[2:0], 2'b0}+:(BPP-1)],
    bg_palette_offs[{1'b0, scroll_x[2:0], 2'b0}+:BPP]
  };

  always_comb begin
    tile_addr = 0;
    texture_addr = 0;

    if (draw_any) begin
      // TODO: Add more background layers
      if (fetch_attr) tile_addr = {tile_render_cycle[0], tile_ind_y, tile_ind_x};
      if (fetch_texture) texture_addr = {tile_texture_ind, prev_tile_off_y};
    end

  end


endmodule : tile_gpu
