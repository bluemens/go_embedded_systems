#include "physics.h"
#include "vga_marble.h"
#include "game_map.h"
#include <stdio.h>
#include <stdlib.h>

MarbleState3D marble_state3D = {
	.pos3D = {0, 0, 0},
	.velocity = {0, 0, 0}
};

int on_win()
{
    return 1;
}

void apply_impulse(Vec3 impulse, double dt)
{
    // Update velocity based on impulse an time
    marble_state3D.velocity.x += impulse.x*TRACKBALL_SENSITIVITY;
    marble_state3D.velocity.y += impulse.y*TRACKBALL_SENSITIVITY;
    marble_state3D.velocity.z += impulse.z*TRACKBALL_SENSITIVITY;


    // Update position based on new velocity and time
    marble_state3D.pos3D.x += marble_state3D.velocity.x*TRACKBALL_SENSITIVITY;
    marble_state3D.pos3D.y += marble_state3D.velocity.y*TRACKBALL_SENSITIVITY;
    marble_state3D.pos3D.z += marble_state3D.velocity.z*TRACKBALL_SENSITIVITY;

    if(impulse.x != 0 || impulse.y != 0){
        //printf("Impulse: x: %f, y: %f, z: f\n", impulse.x, impulse.y, impulse.z);
        //printf("Updated Position: x %f y %f \n", marble_state3D.pos3D.x, marble_state3D.pos3D.y);
    }
}



void handle_ramp(int direction, double dt) {
    printf("THERE IS A RAMP!\n");
    if (direction == DOWN_X_RAMP) {
        printf("DOWN X");
        Vec3 impulse = {0.2*GRAVITY, 0, 0.5*GRAVITY};
        apply_impulse(impulse,dt);
    } else if (direction == UP_X_RAMP) {
        printf("UP X");
        Vec3 impulse = {-0.2*GRAVITY, 0, 0.5*GRAVITY};
        apply_impulse(impulse,dt);
    } else if (direction == DOWN_Y_RAMP) {
        printf("DOWN Y");
        Vec3 impulse = {0, 0.2*GRAVITY, 0.5*GRAVITY};
        apply_impulse(impulse,dt);
    } else if (direction == UP_Y_RAMP) {
        printf("UP Y");
        Vec3 impulse = {0, -0.2*GRAVITY, 0.5*GRAVITY};
        apply_impulse(impulse,dt);
    }
}


int check_wall(Tile cur_tile) {

    if ((cur_tile.x_idx + BALL_RADIUS > marble_state3D.pos3D.x) && cur_tile.x_idx != 0) {
        Tile next_tile = levels[current_level][cur_tile.y_idx][cur_tile.x_idx - 1];

        if (next_tile.type <= 7 && next_tile.type >= 3) {
            return 0;
        }
        
        if (next_tile.z_idx < cur_tile.z_idx && marble_state3D.velocity.x < 0) {
            printf("Left Wall\n");
            return LEFT;
        }
    }  else if ((cur_tile.x_idx + 1 - BALL_RADIUS < marble_state3D.pos3D.x) && cur_tile.x_idx != current_level_rows - 1) {
        Tile next_tile = levels[current_level][cur_tile.y_idx][cur_tile.x_idx + 1];

        if (next_tile.type <= 7 && next_tile.type >= 3) {
            return 0;
        }

        //printf("Cur Pos: %f, %f\n", marble_state3D.pos3D.x, marble_state3D.pos3D.y);
        //printf("Next Tile: %d, Current Tile: %d\n", next_tile.z_idx, cur_tile.z_idx);
        //printf("Next Tile: %d %d, Current Tile: %d %d\n", next_tile.x_idx, next_tile.y_idx, cur_tile.x_idx, cur_tile.y_idx);
        if (next_tile.z_idx < cur_tile.z_idx && marble_state3D.velocity.x > 0) {
            printf("Right Wall\n");
            return RIGHT;
        }
    } else if ((cur_tile.y_idx + BALL_RADIUS > marble_state3D.pos3D.y) && cur_tile.y_idx != 0) {
        Tile next_tile = levels[current_level][cur_tile.y_idx - 1][cur_tile.x_idx];

        if (next_tile.type <= 7 && next_tile.type >= 3) {
            return 0;
        }

        if (next_tile.z_idx < cur_tile.z_idx && marble_state3D.velocity.y > 0) {
            printf("Up Wall\n");
            return UP;
        }
        
    } else if ((cur_tile.y_idx + 1 - BALL_RADIUS < marble_state3D.pos3D.y) && cur_tile.y_idx != current_level_cols - 1) {
        Tile next_tile = levels[current_level][cur_tile.y_idx + 1][cur_tile.x_idx];

        if (next_tile.type <= 7 && next_tile.type >= 3) {
            return 0;
        }

        if (next_tile.z_idx < cur_tile.z_idx && marble_state3D.velocity.y < 0) {
            printf("Down Wall\n");
            return DOWN;
        }
    }

    return 0;
}

int check_fall(Tile cur_tile) {

    if ((cur_tile.x_idx + BALL_RADIUS > marble_state3D.pos3D.x) && cur_tile.x_idx != 0) {
        Tile next_tile = levels[current_level][cur_tile.y_idx][cur_tile.x_idx - 1];

        if (next_tile.z_idx > cur_tile.z_idx) {
            printf("Left Fall\n");
            return 1;
        } 
    } else if ((cur_tile.x_idx + 1 - BALL_RADIUS < marble_state3D.pos3D.x) && cur_tile.x_idx != current_level_rows - 1) {
        Tile next_tile = levels[current_level][cur_tile.y_idx][cur_tile.x_idx + 1];

        if (next_tile.z_idx > cur_tile.z_idx) {
             printf("Right Fall\n");
            return 1;
        } 
    } else if ((cur_tile.y_idx + BALL_RADIUS > marble_state3D.pos3D.y) && cur_tile.y_idx != 0) {
        Tile next_tile = levels[current_level][cur_tile.y_idx-1][cur_tile.x_idx];

        if (next_tile.z_idx > cur_tile.z_idx) {
             printf("Up Fall\n");
            return 1;
        } 
    } else if ((cur_tile.y_idx + 1 - BALL_RADIUS < marble_state3D.pos3D.y) && cur_tile.y_idx != current_level_cols - 1) {
        Tile next_tile = levels[current_level][cur_tile.y_idx + 1][cur_tile.x_idx];
        if (next_tile.z_idx > cur_tile.z_idx) {
             printf("Down Fall\n");
            return 1;
        } 
    }

    return 0;
}



int check_game_state(Tile cur_tile){
    if(cur_tile.type == NO_TILE) {
        printf("%f, %f, %f\n", marble_state3D.pos3D.x, marble_state3D.pos3D.y, marble_state3D.pos3D.z);
        printf("On empty tile\n");
        return LOST; 
    } else if (cur_tile.type == WIN_TILE) {
        return WON; 
    } 
    return CONTINUE; 
}


//1 if you have lost, 0 otherwise
// int check_game_state(Tile cur_tile) {
    
//     if (cur_tile.x_idx == 0 && (cur_tile.x_idx + BALL_RADIUS > marble_state3D.pos3D.x)) {
//         return LOST; 
//     } else if ((cur_tile.x_idx == current_level_cols - 1) && (cur_tile.x_idx + 1 - BALL_RADIUS < marble_state3D.pos3D.x)) {
//         return LOST; 
//     } else if (cur_tile.y_idx == 0 && (cur_tile.y_idx + BALL_RADIUS > marble_state3D.pos3D.y)) {
//         return LOST; 
//     } else if ((cur_tile.y_idx == current_level_rows - 1) && (cur_tile.y_idx + 1 - BALL_RADIUS < marble_state3D.pos3D.y)) {
//         return LOST; 
//     }

//     if (check_win(cur_tile))
//     {
//         return WON;
//     }

//     return CONTINUE;
// }

void handle_wall(int wall_direction) {
    printf("handling wall\n");
    if (wall_direction == LEFT || wall_direction == RIGHT)
    {
        marble_state3D.velocity.x *= -1;
    }
    if (wall_direction == UP || wall_direction == DOWN)
    {
        marble_state3D.velocity.y *= -1;
    }
}

int check_win(Tile cur_tile)
{
    return cur_tile.type == WIN_TILE;
}

//returns LOST if you lost, WON if you won, CONTINUE otherwise
int check_boundaries(double dt) {
    // fetch current tile
    Tile cur_tile = check_position(&marble_state3D);

    //printf("current tile type: %d\n", cur_tile.type); 

    int game_state = check_game_state(cur_tile);
    if(game_state == LOST) {
        //we lost
        return LOST;
    }
    else if (game_state == WON)
    {
        return WON;
    }

    int wall_direction = check_wall(cur_tile); 

    if (wall_direction) {
        handle_wall(wall_direction); 
    }

    int fall = check_fall(cur_tile); 

    if (fall) {

        int fall_result = handle_fall(dt); 

        if (fall_result == LOST)
        {
            printf("fall lost\n");
            return LOST;
        }
    }

    int ramp = check_ramp(cur_tile);

    if (ramp)
    {   
        handle_ramp(ramp,dt);
    }
    return CONTINUE;

}

int check_ramp(Tile cur_tile)
{
    if (cur_tile.type == UP_X_RAMP)
    {
        return cur_tile.type;
    }
    if (cur_tile.type == UP_Y_RAMP)
    {
        return cur_tile.type;
    }
    if (cur_tile.type == DOWN_X_RAMP)
    {
        return cur_tile.type;
    }
    if (cur_tile.type == DOWN_Y_RAMP)
    {
        return cur_tile.type;
    }
    return 0;
}

int handle_fall(double dt)
{
    printf("In handle fall\n");
    Tile cur_tile = check_position(&marble_state3D);

    if (cur_tile.type == NO_TILE)
    {
        printf("%f, %f, %f\n", marble_state3D.pos3D.x, marble_state3D.pos3D.y, marble_state3D.pos3D.z);
        printf("on empty tile\n");
        return LOST;
    }

    Vec3 impulse = {0, 0, GRAVITY};
    apply_impulse(impulse, dt);
    return CONTINUE;
}