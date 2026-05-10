#ifndef TILEMAP_H
#define TILEMAP_H

#include <stdbool.h>
#include "sprite.h" 
#include "type.h"
typedef struct item_t item_t;

// === External map array (read-only) ===

// === Tile collision detection functions ===
// Check if the given area collides with a "wall" tile
bool is_tile_blocked(float x, float y, float width, float height);

void item_place_on_tile(item_t *item, int tile_x, int tile_y);

int get_tile_at_pixel(float x, float y);
bool is_death(float x, float y, float width, float height, player_type_t player);
bool check_both_players_goal();
#endif // TILEMAP_H
