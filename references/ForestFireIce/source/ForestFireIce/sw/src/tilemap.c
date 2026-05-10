// tilemap.c
// Terrain map data and tile collision detection implementation
#include "tilemap.h"
#include <math.h> // For floor()
#include "player.h"
#include "sprite.h" // If missing, include for item_t definition
#include "type.h"
#include <stdio.h>
// === Example map data ===
// 0: Empty  1: Wall  2: Fire pit  3: Water pool  4: Goal
const int tilemap[30][40] = {
    //               x              1              5              2              5              3              5              4
    {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
    {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
    {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
    {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
    {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1}, //
    {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 6, 1, 1, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 10, 0, 9, 0, 0, 1},
    {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 7, 1, 1, 1, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 10, 0, 9, 0, 0, 1},
    {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
    {1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
    {1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1}, // 1
    {1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
    {1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
    {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 1},
    {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 7, 1, 1, 1, 1, 1, 0, 0, 0, 0, 1},
    {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1}, //
    {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
    {1, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
    {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 7, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
    {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 7, 1, 1, 1, 1, 1, 1},
    {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 7, 1, 1, 1, 1}, // 2
    {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 7, 1, 1, 1},
    {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
    {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 7, 1, 1, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
    {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 7, 1, 1, 1, 1, 1, 1, 1, 4, 4, 4, 4, 4, 1, 1, 1, 1, 1, 5, 0, 0, 0, 0, 1},
    {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1}, //
    {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
    {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 6, 1, 1, 1},
    {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1},
    {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1},
    {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 1, 1, 1, 3, 3, 3, 3, 3, 1, 1, 1, 1, 1, 1, 1, 1}};

#define COLLISION_MARGIN 1.0f

bool is_tile_blocked(float x, float y, float width, float height)
{
    float center_x = x + width / 2.0f;

    for (int i = PLAYER_HITBOX_OFFSET_Y; i < (int)height; ++i)
    {
        float sx = center_x;
        float sy = y + i + COLLISION_MARGIN;

        int tx = (int)(sx / TILE_SIZE);
        int ty = (int)(sy / TILE_SIZE);

        if (tx < 0 || tx >= MAP_WIDTH || ty < 0 || ty >= MAP_HEIGHT)
            return true;

        int tile = tilemap[ty][tx];

        // Normal wall
        if (tile == TILE_WALL)
            return true;

        if (tile == TILE_WATER || tile == TILE_POISON || tile == TILE_FIRE)
        {
            float y_in_tile = fmod(sy, TILE_SIZE);
            if (y_in_tile >= 8.0f)
                return true;
        }
        // Sloped ceiling handling (character head collision)
        if (tile == TILE_CEIL_L || tile == TILE_CEIL_R)
        {
            float x_in_tile = fmod(sx, TILE_SIZE);
            float y_in_tile = fmod(sy, TILE_SIZE);

            int x_local = (int)x_in_tile;
            int y_local = (int)y_in_tile;

            int max_y = (tile == TILE_CEIL_L)
                            ? TILE_SIZE - 1 - x_local // Left low, right high
                            : x_local;                // Right low, left high

            if (y_local <= max_y)
                return true;
        }

        // Sloped floor handling (character foot collision)
        if (tile == TILE_SLOPE_L_UP || tile == TILE_SLOPE_R_UP)
        {
            float x_in_tile = fmod(sx, TILE_SIZE);
            float y_in_tile = fmod(sy, TILE_SIZE);

            int x_local = (int)x_in_tile;
            int y_local = (int)y_in_tile;

            int min_y = (tile == TILE_SLOPE_L_UP)
                            ? x_local                  // \ ← Left high, right low
                            : TILE_SIZE - 1 - x_local; // / ← Right high, left low

            if (y_local >= min_y)
                return true;
        }
    }

    return false;
}
int get_tile_at_pixel(float x, float y)
{
    int tx = (int)(x / TILE_SIZE);
    int ty = (int)(y / TILE_SIZE);

    if (tx < 0 || tx >= MAP_WIDTH || ty < 0 || ty >= MAP_HEIGHT)
        return TILE_WALL; // Treat out of bounds as wall

    return tilemap[ty][tx];
}

void item_place_on_tile(item_t *item, int tile_x, int tile_y)
{
    item->x = tile_x * TILE_SIZE + (TILE_SIZE - item->width) / 2.0f;
    item->y = tile_y * TILE_SIZE + (TILE_SIZE - item->height) / 2.0f;

    item->sprite.x = (uint16_t)item->x;
    item->sprite.y = (uint16_t)item->y;
}

bool is_death(float x, float y, float width, float height, player_type_t p)
{

    float center_x = x + width / 2.0f;
    for (int i = PLAYER_HITBOX_OFFSET_Y; i < (int)height; ++i)
    {
        float sx = center_x;
        float sy = y + i + COLLISION_MARGIN;

        int tx = (int)(sx / TILE_SIZE);
        int ty = (int)(sy / TILE_SIZE);

        if (tx < 0 || tx >= MAP_WIDTH || ty < 0 || ty >= MAP_HEIGHT)
            return false;

        int tile = tilemap[ty][tx];

        // Dangerous terrain detection (death)
        if (p == PLAYER_FIREBOY && (tile == TILE_WATER || tile == TILE_POISON))
            return true;
        if (p == PLAYER_WATERGIRL && (tile == TILE_FIRE || tile == TILE_POISON))
            return true;
    }
    return false;
}

bool check_both_players_goal()
{
    bool fireboy_goal = false;
    bool watergirl_goal = false;

    for (int i = 0; i < NUM_PLAYERS; ++i)
    {
        float center_x = players[i].x + SPRITE_W_PIXELS / 2.0f;
        float center_y = players[i].y + SPRITE_H_PIXELS / 2.0f;
        int tile = get_tile_at_pixel(center_x, center_y);

        if (players[i].type == PLAYER_FIREBOY && tile == TILE_GOAL2)
            fireboy_goal = true;
        else if (players[i].type == PLAYER_WATERGIRL && tile == TILE_GOAL1)
            watergirl_goal = true;
    }

    return fireboy_goal && watergirl_goal;
}
