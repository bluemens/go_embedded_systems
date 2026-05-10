#ifndef GAME_MAP_H
#define GAME_MAP_H

#include "types.h"
#include "level_reader.h"

#define LEVEL_COUNT 1
#define LEVEL_0_ROWS 37
#define LEVEL_0_COLS 26



// Attention for ramp naming convention
// UP_X_RAMP means that x and z are increasing
// DOWN_X_RAMP means that z is decreasing as x is increasing
enum TileType {
    NO_TILE = 0,
    START_TILE = 1,
    FLAT = 2,
    UP_Y_RAMP = 3,
    UP_X_RAMP = 4,
    DOWN_X_RAMP = 6,
    DOWN_Y_RAMP = 7,
    WIN_TILE = 8
};

Tile check_position(MarbleState3D *marble_state3D);
void free_levels();
void initialize_levels(MarbleState3D *marble_state3D);

extern Tile*** levels;
extern int current_level;
extern int current_level_cols;
extern int current_level_rows;


#endif
