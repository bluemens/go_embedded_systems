#ifndef LEVEL_READER_H
#define LEVEL_READER_H

#include "types.h" 
#include "game_map.h"

Tile** read_level(const char* filename, int height, int width, MarbleState3D *marble_state3D);

typedef struct {
    unsigned char red, green, blue, alpha;
} ColorRGBA;

typedef struct {
    char palette_indicies[8][8];
} Texture;

typedef struct {
    char *tiles;
    char height;
    char width;
} Tilemap;

#endif
