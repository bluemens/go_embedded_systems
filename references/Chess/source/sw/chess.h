#ifndef CHESS_H
#define CHESS_H

#include <stdint.h>     
#include <stdbool.h>
#include <stdio.h>

#define BOARD_SIZE 8

typedef enum {
    /*Type     # dec # bin   */
    EMPTY,  //    0    000
    PAWN,   //    1    001
    ROOK,   //    2    010
    KNIGHT, //    3    011
    BISHOP, //    4    100
    QUEEN,  //    5    101
    KING    //    6    110
} PieceType;

typedef enum {
    /*Type     # dec # bin   */
    NONE,   //    0    00
    WHITE,  //    1    01
    BLACK   //    2    10
} Color;

typedef struct {
    PieceType type;
    Color color;
} Piece;

int init_vga_board();
void write_move_to_fpga(int start_x, int start_y, int end_x, int end_y, Piece p);

/* Check castling */
extern bool whiteKingMoved;
extern bool blackKingMoved;
extern bool whiteLeftRookMoved;
extern bool whiteRightRookMoved;
extern bool blackLeftRookMoved;
extern bool blackRightRookMoved;

/* check en passant */
extern int lastDoublePushRow;
extern int lastDoublePushCol;
extern char lastDoublePushColor;

/* Use 8*8 arrays of Pieces to present board */
extern Piece board[BOARD_SIZE][BOARD_SIZE];

/* Core Fuctions */
const char* pieceToString(Piece p);
int encode_piece(Piece p);
void init_board();
void print_board();
bool check_move(char playerColor, int startRow, int startCol, int endRow, int endCol);

/* Coordinate helpers */
int decode_x(char file);                   // e.g. 'A' → 1
int decode_y(char rank);                   // e.g. '1' → 1 (optional if needed)
const char* encode_xy(int x, int y);       // e.g. (1, 2) → "A2"

/* Game mode */
void pvp_mode();
void pve_mode();

/* pve helpers */
void apply_move(const char *uci, char color);

#endif
