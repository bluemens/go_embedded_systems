#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <fcntl.h>   
#include <sys/mman.h> 
#include <sys/types.h>  
#include <unistd.h>   
#include <stdbool.h>  

#include "chess.h"
#include "uci.h"

/* Use 8*8 arrays of Pieces to present board */
Piece board[BOARD_SIZE][BOARD_SIZE]; 

/* Check castling */
bool whiteKingMoved = false;
bool blackKingMoved = false;
bool whiteLeftRookMoved = false;
bool whiteRightRookMoved = false;
bool blackLeftRookMoved = false;
bool blackRightRookMoved = false;

/* check en passant */
int lastDoublePushRow = -1;
int lastDoublePushCol = -1;
char lastDoublePushColor = ' ';

/* pve game over indicator */
bool gameOver = false;

/* write to hw */
#define HW_BASE_ADDR  0xFF200000 
#define HW_SPAN       0x100

volatile uint8_t *vga_board_regs = NULL;

int init_vga_board() {
    int fd = open("/dev/mem", O_RDWR | O_SYNC);
    if (fd < 0) {
        perror("open /dev/mem failed");
        return -1;
    }

    void *map = mmap(NULL, HW_SPAN, PROT_READ | PROT_WRITE, MAP_SHARED, fd, HW_BASE_ADDR);
    if (map == MAP_FAILED) {
        perror("mmap failed");
        close(fd);
        return -1;
    }

    vga_board_regs = (volatile uint8_t *) map;
    close(fd);
    return 0;
}

void write_move_to_fpga(int start_x, int start_y, int end_x, int end_y, Piece p) {
    if (!vga_board_regs) { return;}
    int e_p = encode_piece(p);

    uint8_t reg1 = ((start_x & 0x7) << 5) | ((end_x & 0x7) << 2) | ((start_y >> 1) & 0x3);
    uint8_t reg2 = ((start_y & 0x1) << 7) | ((end_y & 0x7) << 4) | (e_p & 0xF);

    //printf("r1 is %d r2 is %d\n", reg1, reg2);

    vga_board_regs[1] = reg1;
    usleep(2000);  // Wait for next clock
    vga_board_regs[2] = reg2;
    usleep(2000);  // Wait for next clock
    vga_board_regs[3] = 1;  // commit the move immediately
    usleep(2000);
}

int main() {
    /* Check hw */
    if (init_vga_board() != 0) {
        fprintf(stderr, "Failed to init FPGA interface\n");
        exit(1);
    }

    int choice;
    int color_val;

    while (1) {
        printf("\n=== Chess Main Menu ===\n");
        printf("1. Player vs Player (PvP)\n");
        printf("2. Player vs Engine (PvE)\n");
        printf("3. Set Color Theme\n");
        printf("4. Exit\n");
        printf("Enter your choice: ");
        /* Get choice from the user */
        if (scanf("%d", &choice) != 1) {
            fprintf(stderr, "Invalid input.\n");
            while (getchar() != '\n');  // clear input buffer
            continue;
        }

        getchar();  // consume newline

        switch (choice) {
            case 1:
                pvp_mode();
                break;
            case 2:
                pve_mode();
                break;
            case 3:
                printf("1. Blue  tone with Black, White, Blue, Green (0-3)\n");
                printf("2. Red   tone with Black, White, Blue, Green (4-7)\n");
                printf("3. Green tone with Black, White, Blue, Green (8-11)\n");
                printf("4. Grey  tone with Black, White, Blue, Green (12-15)\n");
                printf("Enter your choice (0-15): ");
                if (scanf("%d", &color_val) != 1 || color_val < 0 || color_val > 15) {
                    fprintf(stderr, "Invalid color. Must be 0-15.\n");
                    while (getchar() != '\n');
                    break;
                }
                getchar();  // consume newline
                if (vga_board_regs) {
                    vga_board_regs[0] = (uint8_t)color_val;
                    printf("Color %d is set.\n", color_val);
                } else {
                    printf("VGA board not initialized.\n");
                }
                break;
            case 4:
                printf("Exiting. Goodbye!\n");
                return 0;
            default:
                printf("Invalid choice. Try again.\n");
        }
    }

    return 0;
}

/* Decode the coordinates eg: A1 -> (1, 1) */
int decode_x(char file) {
    file = tolower(file);
    if (file >= 'a' && file <= 'h') {
        return file - 'a';  // 'a' → 1, ..., 'h' → 8
    }
    return -1; // Invalid input
}

/* Encode the xy coordinates eg:(2, 2) -> B2 */
const char* encode_xy(int x, int y) {
    static char coord[3];  // e.g., "A2"

    if (x < 0 || x > 7 || y < 0 || y > 7) {
        return "?";
    }

    coord[0] = 'A' + x;         // 0 → A, 7 → H
    coord[1] = '1' + (7 - y);   // 0 → 8, 1 → 7, ..., 7 → 1
    coord[2] = '\0';

    return coord;
}

/* Print a piece with Strings */
const char* pieceToString(Piece p) {
    static char repr[3];

    if (p.type == EMPTY) {
        repr[0] = ' ';
        repr[1] = ' ';
        repr[2] = '\0';
        return repr;
    }

    repr[0] = (p.color == WHITE) ? 'w' : 'b';

    switch (p.type) {
        case PAWN: repr[1] = 'P'; break;
        case ROOK: repr[1] = 'R'; break;
        case KNIGHT: repr[1] = 'N'; break;
        case BISHOP: repr[1] = 'B'; break;
        case QUEEN: repr[1] = 'Q'; break;
        case KING: repr[1] = 'K'; break;
        default: repr[1] = ' '; break;
    }

    repr[2] = '\0';
    return repr;
}

/* Convert a piece to 4'b format (color + type) */
int encode_piece(Piece p) {
    if (p.type == EMPTY || p.color == NONE) { return 0; }
    int c = (p.color == BLACK) ? 0 : 1;
    return (c << 3) | (p.type & 0x7);
}

/* Initialize the board when game start */
void init_board() {
    int i, j;

    /* Set everything to empty */
    for (i = 0; i < BOARD_SIZE; i++) {
        for (j = 0; j < BOARD_SIZE; j++) {
            board[i][j].type = EMPTY;
            board[i][j].color = NONE;
            write_move_to_fpga(i, j, i, j, board[i][j]);
        }
    }

    board[0][0] = (Piece){ROOK, BLACK};
    board[0][1] = (Piece){KNIGHT, BLACK};
    board[0][2] = (Piece){BISHOP, BLACK};
    board[0][3] = (Piece){QUEEN, BLACK};
    board[0][4] = (Piece){KING, BLACK};
    board[0][5] = (Piece){BISHOP, BLACK};
    board[0][6] = (Piece){KNIGHT, BLACK};
    board[0][7] = (Piece){ROOK, BLACK};

    /* Set hardware */
    write_move_to_fpga(0, 0, 0, 0, board[0][0]);
    write_move_to_fpga(1, 0, 1, 0, board[0][1]);
    write_move_to_fpga(2, 0, 2, 0, board[0][2]);
    write_move_to_fpga(3, 0, 3, 0, board[0][3]);
    write_move_to_fpga(4, 0, 4, 0, board[0][4]);
    write_move_to_fpga(5, 0, 5, 0, board[0][5]);
    write_move_to_fpga(6, 0, 6, 0, board[0][6]);
    write_move_to_fpga(7, 0, 7, 0, board[0][7]);

    for (j = 0; j < BOARD_SIZE; j++) {
        board[1][j] = (Piece){PAWN, BLACK};
        write_move_to_fpga(j, 1, j, 1, board[1][j]);

        board[6][j] = (Piece){PAWN, WHITE};
        write_move_to_fpga(j, 6, j, 6, board[6][j]);
    }

    board[7][0] = (Piece){ROOK, WHITE};
    board[7][1] = (Piece){KNIGHT, WHITE};
    board[7][2] = (Piece){BISHOP, WHITE};
    board[7][3] = (Piece){QUEEN, WHITE};
    board[7][4] = (Piece){KING, WHITE};
    board[7][5] = (Piece){BISHOP, WHITE};
    board[7][6] = (Piece){KNIGHT, WHITE};
    board[7][7] = (Piece){ROOK, WHITE};

    write_move_to_fpga(0, 7, 0, 7, board[7][0]);
    write_move_to_fpga(1, 7, 1, 7, board[7][1]);
    write_move_to_fpga(2, 7, 2, 7, board[7][2]);
    write_move_to_fpga(3, 7, 3, 7, board[7][3]);
    write_move_to_fpga(4, 7, 4, 7, board[7][4]);
    write_move_to_fpga(5, 7, 5, 7, board[7][5]);
    write_move_to_fpga(6, 7, 6, 7, board[7][6]);
    write_move_to_fpga(7, 7, 7, 7, board[7][7]);
}

/* Print all peices on board */
void print_board() {
    int row, col;

    for (row = 0; row < BOARD_SIZE; row++) {
        printf("   +---+---+---+---+---+---+---+---+\n");
        printf(" %d |", BOARD_SIZE - row);
        for (col = 0; col < BOARD_SIZE; col++) {
            printf("%s |", pieceToString(board[row][col]));
        }
        printf("\n");
    }

    printf("   +---+---+---+---+---+---+---+---+\n");
    printf("     A   B   C   D   E   F   G   H  \n");
}

/* Check whether a movement is valid */
bool check_move(char playerColor, int startRow, int startCol, int endRow, int endCol) {
    /* Go out of board */
    if (startRow < 0 || startRow >= BOARD_SIZE ||
        startCol < 0 || startCol >= BOARD_SIZE ||
        endRow < 0 || endRow >= BOARD_SIZE ||
        endCol < 0 || endCol >= BOARD_SIZE) {
        return false;
    }

    Piece startPiece = board[startRow][startCol];
    Piece endPiece = board[endRow][endCol];

    /* Reject empty start square */
    if (startPiece.type == EMPTY) {
        printf("No piece at start location.\n");
        return false;
    }

    /* Check player color matches piece color */
    if ((playerColor == 'w' && startPiece.color != WHITE) || (playerColor == 'b' && startPiece.color != BLACK)) {
        printf("You can only move your own pieces.\n");
        return false;
    }

    /* Prevent capturing your own piece */
    if (endPiece.type != EMPTY && startPiece.color == endPiece.color) {
        printf("Cannot capture your own piece.\n");
        return false;
    }

    // Move legality frame by piece type
    switch (startPiece.type) {
        case PAWN: {
            int dir = (startPiece.color == WHITE) ? -1 : 1;  // direction: up for white, down for black
            int startRowExpected = (startPiece.color == WHITE) ? 6 : 1;
        
            // Move forward by 1
            if (endCol == startCol && endRow == startRow + dir) {
                if (board[endRow][endCol].type == EMPTY)
                    return true;
                else {
                    printf("Pawn cannot move forward into occupied square.\n");
                    return false;
                }
            }
        
            // Move forward by 2 from start position
            if (endCol == startCol && endRow == startRow + 2 * dir) {
                if (startRow == startRowExpected &&
                    board[startRow + dir][startCol].type == EMPTY &&
                    board[endRow][endCol].type == EMPTY) {
                    return true;
                } else {
                    printf("Pawn cannot move two steps forward.\n");
                    return false;
                }
            }

            // En passant capture (executed directly here)
            if (abs(endCol - startCol) == 1 && endRow == startRow + dir &&
            board[endRow][endCol].type == EMPTY &&
            startRow == ((startPiece.color == WHITE) ? 3 : 4) &&
            board[startRow][endCol].type == PAWN &&
            board[startRow][endCol].color != startPiece.color &&
            lastDoublePushRow == startRow &&
            lastDoublePushCol == endCol) {
                board[startRow][endCol].type = EMPTY;
                board[startRow][endCol].color = NONE;
                printf("En passant capture at %s\n", encode_xy(endCol + 1, 8 - startRow));
                write_move_to_fpga(endCol, startRow, endCol, startRow, (Piece){EMPTY, NONE});
                return true;
            }
        
            // Diagonal capture
            if (abs(endCol - startCol) == 1 && endRow == startRow + dir) {
                if (board[endRow][endCol].type != EMPTY &&
                    board[endRow][endCol].color != startPiece.color) {
                    return true;
                } else {
                    printf("Pawn capture not valid — no opponent piece to capture.\n");
                    return false;
                }
            }
        
            printf("Illegal pawn move.\n");
            return false;
        } case ROOK:{
            if (startRow != endRow && startCol != endCol) {
                printf("Rook must move in a straight line.\n");
                return false;
            }
        
            if (startRow == endRow) {
                // Horizontal move
                int step = (endCol > startCol) ? 1 : -1;
                for (int col = startCol + step; col != endCol; col += step) {
                    if (board[startRow][col].type != EMPTY) {
                        printf("Rook path blocked.\n");
                        return false;
                    }
                }
            } else {
                // Vertical move
                int step = (endRow > startRow) ? 1 : -1;
                for (int row = startRow + step; row != endRow; row += step) {
                    if (board[row][startCol].type != EMPTY) {
                        printf("Rook path blocked.\n");
                        return false;
                    }
                }
            }
        
            return true;

        } case KNIGHT: {
            int dx = abs(endCol - startCol);
            int dy = abs(endRow - startRow);
        
            if ((dx == 2 && dy == 1) || (dx == 1 && dy == 2)) {
                return true;
            } else {
                printf("Invalid knight move.\n");
                return false;
            }
            
        } case BISHOP: {
            if (abs(endRow - startRow) != abs(endCol - startCol)) {
                printf("Bishop must move diagonally.\n");
                return false;
            }
        
            int rowStep = (endRow > startRow) ? 1 : -1;
            int colStep = (endCol > startCol) ? 1 : -1;
        
            int row = startRow + rowStep;
            int col = startCol + colStep;
        
            while (row != endRow && col != endCol) {
                if (board[row][col].type != EMPTY) {
                    printf("Bishop path blocked.\n");
                    return false;
                }
                row += rowStep;
                col += colStep;
            }
        
            return true;

        } case QUEEN: {
            int dx = abs(endCol - startCol);
            int dy = abs(endRow - startRow);
        
            // Try bishop-like move (diagonal)
            if (dx == dy) {
                int rowStep = (endRow > startRow) ? 1 : -1;
                int colStep = (endCol > startCol) ? 1 : -1;
        
                int row = startRow + rowStep;
                int col = startCol + colStep;
        
                while (row != endRow && col != endCol) {
                    if (board[row][col].type != EMPTY) {
                        printf("Queen's diagonal path blocked.\n");
                        return false;
                    }
                    row += rowStep;
                    col += colStep;
                }
                return true;
            }
        
            // Try rook-like move (horizontal or vertical)
            if (startRow == endRow || startCol == endCol) {
                if (startRow == endRow) {
                    int step = (endCol > startCol) ? 1 : -1;
                    for (int col = startCol + step; col != endCol; col += step) {
                        if (board[startRow][col].type != EMPTY) {
                            printf("Queen's horizontal path blocked.\n");
                            return false;
                        }
                    }
                } else {
                    int step = (endRow > startRow) ? 1 : -1;
                    for (int row = startRow + step; row != endRow; row += step) {
                        if (board[row][startCol].type != EMPTY) {
                            printf("Queen's vertical path blocked.\n");
                            return false;
                        }
                    }
                }
                return true;
            }
        
            // If neither valid bishop nor rook move
            printf("Queen must move like a rook or bishop.\n");
            return false;

        } case KING: {
            int dx = abs(endCol - startCol);
            int dy = abs(endRow - startRow);
        
            if (dx <= 1 && dy <= 1 && (dx != 0 || dy != 0)) {
                return true;
            } 

            // Castling attempt (king moves 2 squares horizontally)
            if (startRow == endRow && abs(endCol - startCol) == 2) {
                // King-side castling (right rook)
                if (endCol == 6) {
                    if (startPiece.color == WHITE &&
                        !whiteKingMoved && !whiteRightRookMoved &&
                        board[7][5].type == EMPTY && board[7][6].type == EMPTY &&
                        board[7][7].type == ROOK && board[7][7].color == WHITE) {
                        // Optionally: check if path is under attack
                        return true;
                    }
                    if (startPiece.color == BLACK &&
                        !blackKingMoved && !blackRightRookMoved &&
                        board[0][5].type == EMPTY && board[0][6].type == EMPTY &&
                        board[0][7].type == ROOK && board[0][7].color == BLACK) {
                        return true;
                    }
                }

                // Queen-side castling (left rook)
                if (endCol == 2) {
                    if (startPiece.color == WHITE &&
                        !whiteKingMoved && !whiteLeftRookMoved &&
                        board[7][1].type == EMPTY && board[7][2].type == EMPTY && board[7][3].type == EMPTY &&
                        board[7][0].type == ROOK && board[7][0].color == WHITE) {
                        return true;
                    }
                    if (startPiece.color == BLACK &&
                        !blackKingMoved && !blackLeftRookMoved &&
                        board[0][1].type == EMPTY && board[0][2].type == EMPTY && board[0][3].type == EMPTY &&
                        board[0][0].type == ROOK && board[0][0].color == BLACK) {
                        return true;
                    }
                }
                printf("Invalid king move.\n");
                return false;
            }
        } default: { 
            return false;
        }
    }

    // For now, allow all non-rejected moves
    return true;
}

/* Start a game in person vs person mode */
void pvp_mode() {
    char move[5];  // e.g., A2A4
    int sr, sc, er, ec;
    int turn = 1;

    init_board();
    print_board();

    while (1) {
        printf("\n=== Turn %d ===\n", turn);
        if (turn % 2 == 1)
            printf("White player's move.\n");
        else
            printf("Black player's move.\n");
        printf("Enter move (e.g., A2A4) or type EXIT to return to menu: ");
        
        /* Get inpout from scanf */
        if (scanf("%4s", move) != 1) {
            fprintf(stderr, "Failed to read move.\n");
            continue;
        }

        if (strcasecmp(move, "EXIT") == 0)
            break;
        if (strlen(move) != 4) {
            printf("Invalid move format. Try again.\n");
            continue;
        }

        /* decode coordinates */
        sc = decode_x(move[0]);
        sr = 8 - (move[1] - '0');
        ec = decode_x(move[2]);
        er = 8 - (move[3] - '0');
        char currentColor = (turn % 2 == 1) ? 'w' : 'b';
        Piece startPiece = board[sr][sc];

        /* Check movement */
        if (sc == -1 || ec == -1 || sr < 0 || sr > 7 || er < 0 || er > 7) {
            printf("Invalid coordinates. Try again.\n");
            continue;
        }

        if (check_move(currentColor, sr, sc, er, ec)) {
            // Send move to hw
            write_move_to_fpga(sc, sr, ec, er, startPiece);
            //printf("s_rol is %d, end_row is %d, s_col is %d, end_col is %d\n", sr, er, sc, ec);
            printf("Move %s executed.\n", move);
            
            // Check if a king was captured
            if (board[er][ec].type == KING && board[er][ec].color != startPiece.color) {
                printf("\n=== GAME OVER ===\n");
                if (board[er][ec].color == WHITE)
                    printf("Black wins!\n");
                else
                    printf("White wins!\n");
                break;
            }
            
            // Make the move
            board[er][ec] = board[sr][sc];
            board[sr][sc].type = EMPTY;
            board[sr][sc].color = NONE;

            // Check Castling
            if (startPiece.type == KING && abs(ec - sc) == 2) {
                if (startPiece.color == WHITE) {
                    if (ec == 6) {  // White king-side
                        board[7][5] = board[7][7];
                        board[7][7].type = EMPTY;
                        board[7][7].color = NONE;
                        write_move_to_fpga(7, 7, 5, 7, board[7][5]);
                    } else if (ec == 2) {  // White queen-side
                        board[7][3] = board[7][0];
                        board[7][0].type = EMPTY;
                        board[7][0].color = NONE;
                        write_move_to_fpga(0, 7, 3, 7, board[7][3]);
                    }
                } else {
                    if (ec == 6) {  // Black king-side
                        board[0][5] = board[0][7];
                        board[0][7].type = EMPTY;
                        board[0][7].color = NONE;
                        write_move_to_fpga(7, 0, 5, 0, board[0][5]);
                    } else if (ec == 2) {  // Black queen-side
                        board[0][3] = board[0][0];
                        board[0][0].type = EMPTY;
                        board[0][0].color = NONE;
                        write_move_to_fpga(0, 0, 3, 0, board[0][3]);
                    }
                }
            }

            // Check for pawn promotion
            if (board[er][ec].type == PAWN) {
                if ((board[er][ec].color == WHITE && er == 0) ||
                    (board[er][ec].color == BLACK && er == 7)) {
                    printf("Pawn promoted to Queen at %s!\n", encode_xy(ec + 1, 8 - er));
                    board[er][ec].type = QUEEN;
                    write_move_to_fpga(ec, er, ec, er, board[er][ec]);
                }
            }

            // Update movement flags for castling
            if (startPiece.type == KING) {
                if (startPiece.color == WHITE) whiteKingMoved = true;
                else blackKingMoved = true;
            }

            if (startPiece.type == ROOK) {
                if (startPiece.color == WHITE) {
                    if (sr == 7 && sc == 0) whiteLeftRookMoved = true;
                    if (sr == 7 && sc == 7) whiteRightRookMoved = true;
                } else {
                    if (sr == 0 && sc == 0) blackLeftRookMoved = true;
                    if (sr == 0 && sc == 7) blackRightRookMoved = true;
                }
            }

            // Update movement flags for en passant
            if (board[er][ec].type == PAWN && abs(er - sr) == 2) {
                lastDoublePushRow = er;
                lastDoublePushCol = ec;
                lastDoublePushColor = (turn % 2 == 1) ? 'w' : 'b';
            } else {
                lastDoublePushRow = -1;
                lastDoublePushCol = -1;
                lastDoublePushColor = ' ';
            }

            print_board();
            turn++;
        } else {
            printf("Illegal move.\n");
        }
    }
}

/* Start a game in person vs AI mode */
void pve_mode() {
    char moves[2048] = "";       // expanded move history buffer
    char move[5];                // e.g., e2e4
    char engine_reply[6];        // e.g., e7e5
    int sr, sc, er, ec;
    int move_count = 0;

    init_board();
    print_board();

    uci_init();

    int isPlayerWhite;
    printf("Play as white (1) or black (0)? ");
    scanf("%d", &isPlayerWhite);
    getchar();

    if (!isPlayerWhite) {
        int status = uci_get_bestmove("", engine_reply);
        usleep(500000);
        if (status == 1 || strlen(engine_reply) < 4) {
            printf("Engine failed to move.\n");
            uci_close();
            return;
        }
        printf("Engine plays: %s\n", engine_reply);
        snprintf(moves + strlen(moves), sizeof(moves) - strlen(moves), "%s ", engine_reply);
        apply_move(engine_reply, 'w');
        move_count++;
        print_board();
    }

    while (1) {
        if (gameOver) break;

        printf("\nYour move (e.g., e2e4): ");
        if (scanf("%4s", move) != 1 || strlen(move) != 4) {
            printf("Invalid input.\n");
            continue;
        }

        sc = decode_x(move[0]);
        sr = 8 - (move[1] - '0');
        ec = decode_x(move[2]);
        er = 8 - (move[3] - '0');

        if (!check_move(isPlayerWhite ? 'w' : 'b', sr, sc, er, ec)) {
            printf("Illegal move.\n");
            continue;
        }

        Piece piece = board[sr][sc];
        Piece captured = board[er][ec];

        write_move_to_fpga(sc, sr, ec, er, piece);

        if (captured.type == KING && captured.color != piece.color) {
            printf("\n=== GAME OVER ===\nYou captured the engine's king. You win!\n");
            break;
        }

        board[er][ec] = piece;
        board[sr][sc].type = EMPTY;
        board[sr][sc].color = NONE;

        // Castling (player side)
        if (piece.type == KING && abs(ec - sc) == 2) {
            if (piece.color == WHITE) {
                if (ec == 6) {
                    board[7][5] = board[7][7];
                    board[7][7].type = EMPTY;
                    board[7][7].color = NONE;
                    write_move_to_fpga(7, 7, 5, 7, board[7][5]);
                } else if (ec == 2) {
                    board[7][3] = board[7][0];
                    board[7][0].type = EMPTY;
                    board[7][0].color = NONE;
                    write_move_to_fpga(0, 7, 3, 7, board[7][3]);
                }
            } else {
                if (ec == 6) {
                    board[0][5] = board[0][7];
                    board[0][7].type = EMPTY;
                    board[0][7].color = NONE;
                    write_move_to_fpga(7, 0, 5, 0, board[0][5]);
                } else if (ec == 2) {
                    board[0][3] = board[0][0];
                    board[0][0].type = EMPTY;
                    board[0][0].color = NONE;
                    write_move_to_fpga(0, 0, 3, 0, board[0][3]);
                }
            }
        }

        // Promotion
        if (piece.type == PAWN && (er == 0 || er == 7)) {
            piece.type = QUEEN;
            board[er][ec] = piece;
            write_move_to_fpga(ec, er, ec, er, piece);
            printf("Promotion! You promoted to Queen at %s.\n", encode_xy(ec, er));
        }

        // En passant
        if (piece.type == PAWN && captured.type == EMPTY && sc != ec) {
            int ep_row = (piece.color == WHITE) ? er + 1 : er - 1;
            board[ep_row][ec].type = EMPTY;
            board[ep_row][ec].color = NONE;
            write_move_to_fpga(ec, ep_row, ec, ep_row, (Piece){EMPTY, NONE});
        }

        // Track movement
        if (piece.type == PAWN && abs(er - sr) == 2) {
            lastDoublePushRow = er;
            lastDoublePushCol = ec;
            lastDoublePushColor = (isPlayerWhite ? 'w' : 'b');
        } else {
            lastDoublePushRow = -1;
            lastDoublePushCol = -1;
            lastDoublePushColor = ' ';
        }

        if (piece.type == KING) {
            if (piece.color == WHITE) whiteKingMoved = true;
            else blackKingMoved = true;
        }

        if (piece.type == ROOK) {
            if (piece.color == WHITE) {
                if (sr == 7 && sc == 0) whiteLeftRookMoved = true;
                if (sr == 7 && sc == 7) whiteRightRookMoved = true;
            } else {
                if (sr == 0 && sc == 0) blackLeftRookMoved = true;
                if (sr == 0 && sc == 7) blackRightRookMoved = true;
            }
        }

        // Append move
        snprintf(moves + strlen(moves), sizeof(moves) - strlen(moves), "%s ", move);
        move_count++;

        print_board();

        // AI turn — only if it's their move
        int engineTurn = (isPlayerWhite && move_count % 2 == 1) || (!isPlayerWhite && move_count % 2 == 0);
        if (!engineTurn) continue;

        usleep(500000);
        int status = uci_get_bestmove(moves, engine_reply);

        if (status == 1 || strlen(engine_reply) < 4) {
            printf("Engine has no legal moves. You win!\n");
            break;
        }

        printf("Engine plays: %s\n", engine_reply);
        snprintf(moves + strlen(moves), sizeof(moves) - strlen(moves), "%s ", engine_reply);
        apply_move(engine_reply, isPlayerWhite ? 'b' : 'w');
        move_count++;
        print_board();
    }

    uci_close();
}

/* Apply the move from uci */
void apply_move(const char *uci, char color) {
    int sc = decode_x(uci[0]);
    int sr = 8 - (uci[1] - '0');
    int ec = decode_x(uci[2]);
    int er = 8 - (uci[3] - '0');

    Piece piece = board[sr][sc];
    Piece captured = board[er][ec];

    // Safety check: prevent wrong side movement
    if ((color == 'w' && piece.color != WHITE) ||
        (color == 'b' && piece.color != BLACK)) {
        printf("Warning: invalid move — trying to move opponent's piece at %s\n", encode_xy(sc, sr));
        return;
    }

    // Send move to hardware
    write_move_to_fpga(sc, sr, ec, er, piece);

    // Game over condition
    if (captured.type == KING && captured.color != piece.color) {
        printf("\n=== GAME OVER ===\nYour king was captured. %s wins!\n", (piece.color == WHITE ? "White" : "Black"));
        gameOver = true;
        return;
    }

    // Basic move
    board[er][ec] = piece;
    board[sr][sc].type = EMPTY;
    board[sr][sc].color = NONE;

    // Castling
    if (piece.type == KING && abs(ec - sc) == 2) {
        if (piece.color == WHITE) {
            if (ec == 6) {
                board[7][5] = board[7][7];
                board[7][7].type = EMPTY;
                board[7][7].color = NONE;
                write_move_to_fpga(7, 7, 5, 7, board[7][5]);
            } else if (ec == 2) {
                board[7][3] = board[7][0];
                board[7][0].type = EMPTY;
                board[7][0].color = NONE;
                write_move_to_fpga(0, 7, 3, 7, board[7][3]);
            }
        } else {
            if (ec == 6) {
                board[0][5] = board[0][7];
                board[0][7].type = EMPTY;
                board[0][7].color = NONE;
                write_move_to_fpga(7, 0, 5, 0, board[0][5]);
            } else if (ec == 2) {
                board[0][3] = board[0][0];
                board[0][0].type = EMPTY;
                board[0][0].color = NONE;
                write_move_to_fpga(0, 0, 3, 0, board[0][3]);
            }
        }
    }

    // Promotion
    if (piece.type == PAWN && (er == 0 || er == 7)) {
        piece.type = QUEEN;
        board[er][ec] = piece;
        write_move_to_fpga(ec, er, ec, er, piece);
        printf("Promotion! Pawn promoted to Queen at %s.\n", encode_xy(ec, er));
    }

    // En passant
    if (piece.type == PAWN && captured.type == EMPTY && sc != ec) {
        int ep_row = (piece.color == WHITE) ? er + 1 : er - 1;
        board[ep_row][ec].type = EMPTY;
        board[ep_row][ec].color = NONE;
        write_move_to_fpga(ec, ep_row, ec, ep_row, (Piece){EMPTY, NONE});
    }

    // Track en passant
    if (piece.type == PAWN && abs(er - sr) == 2) {
        lastDoublePushRow = er;
        lastDoublePushCol = ec;
        lastDoublePushColor = (color == 'w') ? 'w' : 'b';
    } else {
        lastDoublePushRow = -1;
        lastDoublePushCol = -1;
        lastDoublePushColor = ' ';
    }

    // Track king/rook movement
    if (piece.type == KING) {
        if (piece.color == WHITE) whiteKingMoved = true;
        else blackKingMoved = true;
    }

    if (piece.type == ROOK) {
        if (piece.color == WHITE) {
            if (sr == 7 && sc == 0) whiteLeftRookMoved = true;
            if (sr == 7 && sc == 7) whiteRightRookMoved = true;
        } else {
            if (sr == 0 && sc == 0) blackLeftRookMoved = true;
            if (sr == 0 && sc == 7) blackRightRookMoved = true;
        }
    }
}
