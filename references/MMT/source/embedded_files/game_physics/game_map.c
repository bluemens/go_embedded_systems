#include "game_map.h"
#include <stdlib.h>
#include <stdio.h>

int current_level = 0;
int current_level_rows = LEVEL_0_ROWS;
int current_level_cols = LEVEL_0_COLS;
Tile*** levels = NULL;

void initialize_levels(MarbleState3D *marble_state3D) {

    levels = malloc(LEVEL_COUNT * sizeof(Tile**));

   Tile** level0 = read_level("levels/level4.csv",37,26, marble_state3D);
   levels[0] = level0;

   /* 
   for (int i = 0; i < 37; i++) {
    for (int j = 0; j < 26; j++) {
        printf("%d ", level0[i][j].z_idx);
    }
    printf("\n");
   }
    */
   

  printf("%d, %d, %d\n", level0[31][14].type, level0[31][14].x_idx, level0[31][14].y_idx);

}

void free_levels() {
    for (int L = 0; L < LEVEL_COUNT; ++L) {
        Tile **lvl = levels[L];
        for (int i = 0; i < LEVEL_0_ROWS; ++i) {
            free(lvl[i]);
        }
        free(lvl);
    }
    free(levels);
}


Tile check_position(MarbleState3D *marble_state3D) {
	int x_idx = (int)marble_state3D->pos3D.x;
	int y_idx = (int)marble_state3D->pos3D.y;

    if (x_idx <  0) {
        x_idx = 0;
        marble_state3D->pos3D.x = 0;
    }
    if (y_idx < 0) {
        y_idx = 0; 
        marble_state3D->pos3D.y = 0;
    }

   // printf("%d\n", levels[current_level][y_idx][x_idx].type);

	return levels[current_level][y_idx][x_idx];
}