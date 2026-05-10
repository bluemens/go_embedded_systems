#ifndef ASSETS_H
#define ASSETS_H
#include <cstdint>

//Hardware layout constants 
constexpr int TILE_COLS = 80; //Tile map width
constexpr int TILE_ROWS = 60; //Tile map height
constexpr int TM_STRIDE = 128; //Bytes per tile map row

//Playfield: 15×20 interior, with framed 1‑tile border = 17×22 
constexpr int PF_WIDTH  = 17;
constexpr int PF_HEIGHT = 22;

//Coordinates to center the playfield
constexpr int PF_LEFT  = (TILE_COLS - PF_WIDTH ) / 2;  
constexpr int PF_TOP   = (TILE_ROWS - PF_HEIGHT) / 4;

//Coordinates for score / lines
constexpr int HUD_COL = 10;
constexpr int HUD_SCORE_ROW = 42;
constexpr int HUD_LINES_ROW = HUD_SCORE_ROW + 9;
constexpr int LEVEL_ROW = HUD_SCORE_ROW - 9;

//Coordinates for next box
constexpr int NEXT_COL = PF_LEFT + PF_WIDTH + 3;
constexpr int NEXT_ROW = PF_TOP + 1;



//Palette Definition (Bytes are in reverse order (BGR))
static constexpr uint32_t PALETTE24[16] = {
    0x000000, //Color 0
    0xFF0000, //Color 1
    0x00FF00, //Color 2
    0x0000FF, //Color 3
    0xFFFF00, //Color 4
    0x00FFFF, //Color 5
    0xFF00FF, //Color 6
    0x808080, //Color 7
    0xfc036f, //Color 8
    0x606060, //Color 9
    0xA0A0A0, //Color 10
    0xC0C0C0, //Color 11
    0xE0E0E0, //Color 12
    0xF0F0F0, //Color 13
    0xFFFFFF, //Color 14
    0xFFFFFF //Color 15
};

//Tile indexes
enum : uint8_t {
    TILE_EMPTY = 0,
    TILE_WALL = 1,
    TILE_RED = 2,
    TILE_GREEN = 3,
    TILE_BLUE = 4,
    TILE_YELLOW = 5,
    TILE_CYAN = 6,
    TILE_MAG = 7,
    TILE_PURPLE = 8,
    TILE_WHITE = 14
};

//Palette indexes
static constexpr uint8_t TILE2PAL(uint8_t tile) {
    switch(tile){
        case TILE_WALL: return 7;
        case TILE_RED: return 1;
        case TILE_GREEN: return 2;
        case TILE_BLUE: return 3;
        case TILE_YELLOW: return 4;
        case TILE_CYAN: return 5;
        case TILE_MAG: return 6;
        case TILE_PURPLE: return 8;
        case TILE_WHITE: return 14;
        default: return 0;
    }
}

//Tile set
static uint8_t TILESET[16384];
inline void build_tileset() {
    for (int tile = 0; tile < 256; ++tile) {
        uint8_t col = TILE2PAL(tile);
        for (int pixel = 0; pixel < 64; ++pixel)
            TILESET[tile * 64 + pixel] = col;
    }
}

#endif