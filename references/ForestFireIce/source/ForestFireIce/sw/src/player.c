#include "player.h"
#include "joypad_input.h"
#include "tilemap.h"
#include "hw_interact.h"
#include <math.h> // For floor()
#include "type.h"
#include <stdio.h> // Adding this at the top
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#define GRAVITY 0.2f
#define JUMP_VELOCITY -4.5f
#define MOVE_SPEED 2.5f

extern box_t boxes[NUM_BOXES];
void debug_print_player_state(player_t *p, const char *tag)
{
    float center_x = p->x + SPRITE_W_PIXELS / 2.0f;
    float foot_y = p->y + PLAYER_HEIGHT_PIXELS + 1;
    int tile = get_tile_at_pixel(center_x, foot_y);
    int tx = (int)(center_x / TILE_SIZE);
    int ty = (int)(foot_y / TILE_SIZE);

    printf("[%s] x=%.1f y=%.1f vx=%.2f vy=%.2f on_ground=%d foot_tile=%d (%d,%d)\n",
           tag, p->x, p->y, p->vx, p->vy, p->on_ground, tile, tx, ty);
}
void player_init(player_t *p, int x, int y,
                 uint8_t upper_index, uint8_t lower_index,
                 player_type_t type)
{
    p->x = x;
    p->y = y;
    p->vx = p->vy = 0;
    p->on_ground = false;
    p->state = STATE_IDLE;
    p->type = type;
    p->frame_timer = 0;
    p->frame_index = 0;
    p->was_on_slope_last_frame = false;

    if (type == PLAYER_FIREBOY)
    {
        sprite_set(&p->upper_sprite, upper_index, 0);
        sprite_set(&p->lower_sprite, lower_index, 0);
    }
    else
    {
        sprite_set(&p->upper_sprite, upper_index, 0);
        sprite_set(&p->lower_sprite, lower_index, 0);
    }
}

void player_handle_input(player_t *p, int player_index)
{
    game_action_t action = get_player_action(player_index);

    // Handle jumping, must be placed first
    if (action == ACTION_JUMP && p->on_ground)
    {
        set_map_and_audio(1, 1, 0);
        set_map_and_audio(1, 1, 1);
        p->vy = JUMP_VELOCITY;
        p->on_ground = false;
        p->state = STATE_JUMPING;
    }

    // Handle horizontal movement
    if (action == ACTION_MOVE_LEFT)
    {
        p->vx = -MOVE_SPEED;
        p->lower_sprite.flip = 1;
        p->upper_sprite.flip = 1;
    }
    else if (action == ACTION_MOVE_RIGHT)
    {
        p->vx = MOVE_SPEED;
        p->lower_sprite.flip = 0;
        p->upper_sprite.flip = 0;
    }
    else if (p->on_ground) // Don't immediately cancel horizontal velocity in air
    {
        p->vx = 0;
    }
}

int player_update_physics(player_t *p)
{
    p->vy += GRAVITY;
    // Vertical movement
    float tempVy = 0.0f;
    float new_y = p->y + p->vy;
    if (is_death(p->x, new_y + 1, SPRITE_W_PIXELS, PLAYER_HEIGHT_PIXELS, p->type))
    {
        return 1;
    }
    if (check_both_players_goal())
    {
        return 2;
    }
    if (!is_tile_blocked(p->x, new_y + 1, SPRITE_W_PIXELS, PLAYER_HEIGHT_PIXELS) &&
        !is_box_blocked(p->x + SPRITE_W_PIXELS / 2.0f, new_y + PLAYER_HITBOX_OFFSET_Y, 1.0f, PLAYER_HITBOX_HEIGHT) &&
        !is_elevator_blocked(p->x + SPRITE_W_PIXELS / 2.0f - 2, new_y + PLAYER_HITBOX_OFFSET_Y, 4.0f, PLAYER_HITBOX_HEIGHT, &tempVy))
    {
        p->y = new_y;
        p->on_ground = false;
    }
    else if (is_elevator_blocked(p->x + SPRITE_W_PIXELS / 2.0f - 2, new_y + PLAYER_HITBOX_OFFSET_Y, 4.0f, PLAYER_HITBOX_HEIGHT, &tempVy))
    {
        // Call your attachment function
        adjust_to_platform_y(p);

        if (p->vy > 0)
            p->on_ground = true;

        p->vy = tempVy;
    }
    else
    {
        // Call your attachment function
        adjust_to_platform_y(p);

        if (p->vy > 0)
            p->on_ground = true;

        p->vy = 0;
    }
    // Horizontal movement
    float new_x = p->x + p->vx;

    // Calculate current position and target position's foot center point
    float cur_foot_x = p->x + SPRITE_W_PIXELS / 2.0f;
    float new_foot_x = new_x + SPRITE_W_PIXELS / 2.0f;
    float foot_y = p->y + PLAYER_HEIGHT_PIXELS;

    // Determine whether current position's foot adjacent to left and right are in slope range
    bool on_slope = false;
    for (int dx = -1; dx <= 1; ++dx)
    {
        int tile = get_tile_at_pixel(new_foot_x + dx * 8, foot_y);
        if (tile == TILE_SLOPE_L_UP || tile == TILE_SLOPE_R_UP)
        {
            on_slope = true;
            break;
        }
    }
    if (on_slope && p->vy >= 0)
    {
        p->x = new_x;
        adjust_to_slope_y(p);
    }
    else if (!is_tile_blocked(new_x, p->y, SPRITE_W_PIXELS, PLAYER_HEIGHT_PIXELS) &&
             !is_box_blocked(new_x + SPRITE_W_PIXELS / 2.0f, p->y + PLAYER_HITBOX_OFFSET_Y, 1.0f, PLAYER_HITBOX_HEIGHT) &&
             !is_elevator_blocked(new_x + SPRITE_W_PIXELS / 2.0f - 2, p->y + PLAYER_HITBOX_OFFSET_Y, 4.0f, PLAYER_HITBOX_HEIGHT - 4, &tempVy))
    {
        p->x = new_x;
    }
    else
    {
        if (is_tile_blocked(new_x, p->y, SPRITE_W_PIXELS, PLAYER_HEIGHT_PIXELS))
        {
            p->vx = 0;
        }
        else if (is_box_blocked(new_x + SPRITE_W_PIXELS / 2.0f, p->y + PLAYER_HITBOX_OFFSET_Y, 1.0f, PLAYER_HITBOX_HEIGHT))
        {
            if (boxes[0].vx != 0)
                p->vx = boxes[0].vx;
            else
            {
                int player_index = (p->type == PLAYER_FIREBOY) ? 0 : 1;
                game_action_t action = get_player_action(player_index);
                if (action == ACTION_MOVE_RIGHT)
                    p->vx = 0.5f;
                else if (action == ACTION_MOVE_LEFT)
                    p->vx = -0.5f;
                else
                    p->vx = 0;
            }
        }
        else
        {
            p->vx = 0;
        }
    }

    // State switching
    if (!p->on_ground)
    {
        if (p->vy < -0.1f)
            p->state = STATE_JUMPING;
        else if (p->vy > 0.1f)
            p->state = STATE_FALLING;
        else
            p->state = STATE_IDLE; // Rarely seen motionless in air
    }
    else if (p->vx != 0)
    {
        p->state = STATE_RUNNING;
    }
    else
    {
        p->state = STATE_IDLE;
    }

    return 0;
}
void adjust_to_slope_y(player_t *p)
{
    // Calculate the horizontal position of the character's foot center
    float center_x = p->x + SPRITE_W_PIXELS / 2.0f;

    // Calculate the current y-coordinate of the character's foot (28 height)
    float base_foot_y = p->y + PLAYER_HEIGHT_PIXELS;

    // Begin a small vertical search around the foot area to find if feet are exactly on a slope
    // Search dy from -4 to +2, can capture small vertical errors (like floating or pressed in)
    for (int dy = -4; dy <= 2; ++dy) // Empirical offset
    {
        float foot_y = base_foot_y + dy; // Current search point's position in y direction

        // Get the tile type at this position
        int tile = get_tile_at_pixel(center_x, foot_y);

        // Only perform alignment processing if a slope tile is detected
        if (tile == TILE_SLOPE_L_UP || tile == TILE_SLOPE_R_UP)
        {
            // Calculate the x offset of the current point within the tile (i.e., top left is (0,0), how many pixels is current x within the tile)
            float x_in_tile = fmod(center_x, TILE_SIZE);
            int x_local = (int)x_in_tile;

            // Calculate the y height that this x offset should correspond to in the current slope tile
            // Left slope rises from bottom-left to top-right: the more right, the higher → y = x
            // Right slope rises from bottom-right to top-left: the more left, the higher → y = TILE_SIZE - 1 - x
            int min_y = (tile == TILE_SLOPE_L_UP)
                            ? x_local
                            : TILE_SIZE - 1 - x_local;

            // Calculate the top y-coordinate of the tile (tile is 16×16, find the starting y of the tile row)
            float tile_top_y = ((int)(foot_y / TILE_SIZE)) * TILE_SIZE;

            // // Set the character's y coordinate:
            // // - tile_top_y + min_y: gets the y height of the slope surface
            // // - minus PLAYER_HEIGHT_PIXELS: let the character stand on the slope surface
            // // - minus 3: an empirical offset for fine adjustment (can be debugged)
            // p->y = tile_top_y + min_y - PLAYER_HEIGHT_PIXELS - 3;

            // // Set character grounded state
            // p->on_ground = true;
            // // Stop vertical velocity (won't continue falling or rising)
            // p->vy = 0;

            // // Exit search after successful processing
            // break;
            // Record old y
            float old_y = p->y;

            // Calculate attachment target y
            float new_y = tile_top_y + min_y - PLAYER_HEIGHT_PIXELS - 3; // Empirical offset

            // If the y difference before and after is very small, keep the original vy to avoid animation judgment being disrupted
            if (fabsf(new_y - old_y) < 0.2f)
            {
                p->y = new_y;
                // Preserve vy, don't force set to 0
            }
            else
            {
                p->y = new_y;
                p->vy = 0; // Only reset to zero when there's significant attachment
            }

            p->on_ground = true;
            break;
        }
    }
}

void adjust_to_platform_y(player_t *p)
{
    // Calculate character's foot center horizontal position
    float foot_center_x = p->x + SPRITE_W_PIXELS / 2.0f; 
    
    // Calculate character's current foot y coordinate (28 height)
    float base_foot_y = p->y + PLAYER_HITBOX_OFFSET_Y + PLAYER_HITBOX_HEIGHT; 
    
    // Begin a small vertical search around the foot area to find if feet are exactly on a slope
    // Search dy from -4 to +2, can capture small vertical errors (like floating or pressed in)
    for (int dy = -4; dy <= 2; ++dy) // Empirical offset values
    {
        float foot_y = base_foot_y + dy; // Current search point's position in y direction
        
        // Get the tile type at this position
        int tile = get_tile_at_pixel(foot_center_x, foot_y);
        
        if (tile == 1) // Platform tile
        {
            float tile_top_y = ((int)(foot_y / TILE_SIZE)) * TILE_SIZE;
            float new_y = tile_top_y - PLAYER_HEIGHT_PIXELS - 1; // Empirical offset

            float old_y = p->y;

            if (fabsf(new_y - old_y) < 0.2f)
            {
                p->y = new_y;
                // Preserve vy
            }
            else
            {
                p->y = new_y;
                p->vy = 0;
            }

            p->on_ground = true;
            break;
        }
    }
}
// Fireboy
#define FB_HEAD_IDLE ((uint8_t)0)      // 0x0000 >> 8 = 0
#define FB_HEAD_WALK ((uint8_t)2)      // 0x0200 >> 8 = 2
#define FB_HEAD_UPDOWN ((uint8_t)7)    // 0x0700 >> 8 = 7
#define FB_HEAD_DOWNWALK ((uint8_t)12) // 0x0C00 >> 8 = 12

#define FB_LEG_IDLE ((uint8_t)17)         // 0x1100 >> 8 = 17
#define FB_LEG_WALK ((uint8_t)18)         // 0x1200 >> 8 = 18
#define FB_LEG_UPorDOWNWALK ((uint8_t)21) // 0x1500 >> 8 = 21

// Watergirl
#define WG_HEAD_IDLE ((uint8_t)22)     // 0x1600 >> 8 = 22
#define WG_HEAD_WALK ((uint8_t)24)     // 0x1800 >> 8 = 24
#define WG_HEAD_UPWALK ((uint8_t)29)   // 0x2100 >> 8 = 29
#define WG_HEAD_DOWNWALK ((uint8_t)34) // 0x2200 >> 8 = 34

#define WG_LEG_IDLE ((uint8_t)39)         // 0x2700 >> 8 = 39
#define WG_LEG_WALK ((uint8_t)40)         // 0x2800 >> 8 = 40
#define WG_LEG_UPorDOWNWALK ((uint8_t)43) // 0x2B00 >> 8 = 43

// Called each frame: automatically cycles between walking frames
static int get_frame_id(player_t *p, bool is_upper)
{
    int base = 0;

    if (p->type == PLAYER_FIREBOY)
    {
        if (p->state == STATE_IDLE)
            base = is_upper ? FB_HEAD_IDLE : FB_LEG_IDLE;
        else if (p->state == STATE_RUNNING)
            base = is_upper ? FB_HEAD_WALK : FB_LEG_WALK;
        else if (p->state == STATE_JUMPING)
            base = is_upper ? FB_HEAD_UPDOWN : FB_LEG_UPorDOWNWALK;
        else if (p->state == STATE_FALLING)
            base = is_upper ? FB_HEAD_DOWNWALK : FB_LEG_UPorDOWNWALK;
    }
    else // WATERGIRL
    {
        if (p->state == STATE_IDLE)
            base = is_upper ? WG_HEAD_IDLE : WG_LEG_IDLE;
        else if (p->state == STATE_RUNNING)
            base = is_upper ? WG_HEAD_WALK : WG_LEG_WALK;
        else if (p->state == STATE_JUMPING)
            base = is_upper ? WG_HEAD_UPWALK : WG_LEG_UPorDOWNWALK;
        else if (p->state == STATE_FALLING)
            base = is_upper ? WG_HEAD_DOWNWALK : WG_LEG_UPorDOWNWALK;
    }

    int frame_count = get_frame_count(p, is_upper);
    return base + (p->frame_index % frame_count);
}

int get_frame_count(player_t *p, bool is_upper)
{
    if (p->state == STATE_RUNNING)
        return is_upper ? 5 : 3;

    if (p->type == PLAYER_FIREBOY)
    {
        if (p->state == STATE_IDLE)
            return is_upper ? 2 : 1;
        else if (p->state == STATE_JUMPING || p->state == STATE_FALLING)
            return is_upper ? 5 : 3;
    }
    else
    {
        if (p->state == STATE_IDLE)
            return is_upper ? 2 : 1;
        else if (p->state == STATE_JUMPING || p->state == STATE_FALLING)
            return is_upper ? 5 : 3;
    }

    return 1;
}

void player_update_sprite(player_t *p)
{
    // Determine if animation is needed
    bool animate = false;

    switch (p->state)
    {
    case STATE_RUNNING:
    case STATE_IDLE:
        animate = true; // Both walking and standing can cycle through animations (like blinking)
        break;
    case STATE_JUMPING:
    case STATE_FALLING:
    default:
        animate = false; // No animation in air or unknown states
        break;
    }

    if (animate)
    {
        p->frame_timer++;
        if (p->frame_timer >= MAX_FRAME_TIMER)
        {
            p->frame_timer = 0;
            int frame_count = get_frame_count(p, true); // Upper body determines frame length
            p->frame_index = (p->frame_index + 1) % frame_count;
        }
    }
    else
    {
        p->frame_index = 0;
    }

    if (p->type == PLAYER_FIREBOY)
    {
        // Set frame ID
        p->lower_sprite.frame_id = get_frame_id(p, false);
        p->upper_sprite.frame_id = get_frame_id(p, true);

        // Set position and enable
        // Body
        p->lower_sprite.x = p->x;
        p->lower_sprite.y = p->y + SPRITE_H_PIXELS - 1;
        p->lower_sprite.enable = true;
        // Head
        p->upper_sprite.x = p->x;
        p->upper_sprite.y = p->y + 5;
        p->upper_sprite.enable = true;
    }
    if (p->type == PLAYER_WATERGIRL)
    {
        p->lower_sprite.frame_id = get_frame_id(p, false);
        p->upper_sprite.frame_id = get_frame_id(p, true);

        p->lower_sprite.x = p->x;
        p->lower_sprite.y = p->y + SPRITE_H_PIXELS - 2;
        p->lower_sprite.enable = true;

        p->upper_sprite.x = p->x;
        p->upper_sprite.y = p->y + 4;
        p->upper_sprite.enable = true;
    }

    sprite_update(&p->lower_sprite);
    sprite_update(&p->upper_sprite);
}
