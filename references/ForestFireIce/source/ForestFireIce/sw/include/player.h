#ifndef PLAYER_H
#define PLAYER_H

#include <stdint.h>
#include <stdbool.h>
#include "sprite.h"
#include "type.h"

void player_init(player_t *p, int x, int y,
                 uint8_t upper_index, uint8_t lower_index,
                 player_type_t type);

int get_frame_count(player_t *p, bool is_upper);

void player_handle_input(player_t *p, int player_index);
int player_update_physics(player_t *p);
void player_check_collision(player_t *p);
void player_update_sprite(player_t *p);
void adjust_to_slope_y(player_t *p);
void debug_print_player_state(player_t *p, const char *tag);
void adjust_to_platform_y(player_t *p);
#endif // PLAYER_H
