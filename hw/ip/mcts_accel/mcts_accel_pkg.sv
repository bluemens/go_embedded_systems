// mcts_accel_pkg — shared constants and shift helpers for the MCTS accelerator.
//
// Bitboard convention: cell_idx = row*9 + col, bit cell_idx of an 81-bit register.
// Row 0 col 0 = bit 0 (LSB), row 8 col 8 = bit 80 (MSB).
//
// Bit derivations for the column masks (verified against the row-major mapping
// row*9+col; off-by-one here will silently break capture detection on edge groups):
//   COL0 positions: 0, 9, 18, 27, 36, 45, 54, 63, 72
//   COL8 positions: 8, 17, 26, 35, 44, 53, 62, 71, 80

package mcts_accel_pkg;

  // Concatenated 21-digit hex literals (no underscores so they're trivially
  // diff'able against sw/golden/go_model.py output: hex(ON_BOARD_MASK) etc.).
  // Verified against go_model.py:
  //   ON_BOARD_MASK = 0x1ffffffffffffffffffff
  //   COL0_MASK     = 0x001008040201008040201
  //   COL8_MASK     = 0x100804020100804020100
  localparam logic [80:0] ON_BOARD_MASK = 81'h1ffffffffffffffffffff;
  localparam logic [80:0] COL0_MASK     = 81'h001008040201008040201;
  localparam logic [80:0] COL8_MASK     = 81'h100804020100804020100;

  // 5.5 komi → integer 5 with a "white wins integer-tied games" tiebreak.
  localparam int KOMI_INT = 5;

  function automatic logic [80:0] shift_n(input logic [80:0] x);
    return x >> 9;
  endfunction

  function automatic logic [80:0] shift_s(input logic [80:0] x);
    return (x << 9) & ON_BOARD_MASK;
  endfunction

  function automatic logic [80:0] shift_w(input logic [80:0] x);
    return (x >> 1) & ~COL8_MASK;
  endfunction

  function automatic logic [80:0] shift_e(input logic [80:0] x);
    return (x << 1) & ~COL0_MASK;
  endfunction

  // Popcount of 81 bits, combinational. Synthesized as an adder tree; if Fmax
  // suffers, register the output in the caller.
  function automatic logic [6:0] popcount81(input logic [80:0] x);
    int unsigned s;
    s = 0;
    for (int i = 0; i < 81; i++) s += x[i];
    return s[6:0];
  endfunction

endpackage
