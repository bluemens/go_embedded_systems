#ifndef PHYSICS_H_
#define PHYSICS_H_

#include "types.h"


#define GRAVITY 0.1
#define BALL_RADIUS 0.5
#define UP 1
#define DOWN 2
#define LEFT 3
#define RIGHT 4

#define TRACKBALL_SENSITIVITY 0.0005

extern MarbleState3D marble_state3D;

void apply_impulse(Vec3 impulse, double dt);

void handle_ramp(int direction, double dt);

int check_boundaries();

int check_game_state(Tile cur_tile);

void end_game();

void handle_wall(int wall_direction);

int check_wall(Tile cur_tile);
int check_fall(Tile cur_tile);

int handle_fall(double dt);

int check_win(Tile cur_tile);

int check_ramp(Tile cur_tile);


#endif