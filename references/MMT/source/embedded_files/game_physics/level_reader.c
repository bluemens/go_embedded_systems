#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "level_reader.h"

/**
 * Reads a level from a CSV where each cell is formatted as "z,type"
 * and returns a height×width Tile** array.
 *
 * @param filename  path to the CSV file
 * @param height    number of rows in the level
 * @param width     number of columns in the level
 * @return          pointer to an array of Tile* (length = height), each pointing
 *                  to an array of Tiles (length = width). NULL on failure.
 */
Tile** read_level(const char* filename, int height, int width, MarbleState3D *marble_state3D) {
    FILE* fp = fopen(filename, "r");
    if (!fp) {
        perror("fopen");
        return NULL;
    }

    // Allocate the array of row pointers
    Tile** level = malloc(height * sizeof(Tile*));
    if (!level) {
        perror("malloc");
        fclose(fp);
        return NULL;
    }

    // Allocate each row
    for (int y = 0; y < height; y++) {
        level[y] = malloc(width * sizeof(Tile));
        if (!level[y]) {
            perror("malloc");
            // clean up previously allocated rows
            for (int j = 0; j < y; j++) free(level[j]);
            free(level);
            fclose(fp);
            return NULL;
        }
    }

    // Buffer to hold each line
    size_t bufsize = 4 * width * 8; // rough estimate
    char* line = malloc(bufsize);
    if (!line) {
        perror("malloc");
        // clean up
        for (int y = 0; y < height; ++y) free(level[y]);
        free(level);
        fclose(fp);
        return NULL;
    }

    // Read each row
    for (int y = 0; y < height; y++) {
        if (!fgets(line, bufsize, fp)) {
            fprintf(stderr, "Premature end of file at row %d\n", y);
            break;
        }
        char* p = line;
        for (int x = 0; x < width; ++x) {
            // Find opening quote
            p = strchr(p, '"');
            if (!p) {
                fprintf(stderr, "Parse error at row %d, col %d\n", y, x);
                goto fail;
            }
            p++;  // skip the '"'

            // Extract up to closing quote
            char cell[32];
            char* endq = strchr(p, '"');
            if (!endq) {
                fprintf(stderr, "Unterminated quote at row %d, col %d\n", y, x);
                goto fail;
            }
            int len = endq - p;
            if (len >= (int)sizeof(cell)) len = sizeof(cell) - 1;
            memcpy(cell, p, len);
            cell[len] = '\0';

            // Parse z,type
            int z, type;
            if (sscanf(cell, "%d,%d", &z, &type) != 2) {
                fprintf(stderr, "Invalid cell \"%s\" at row %d, col %d\n", cell, y, x);
                goto fail;
            }

            // Store into struct
            level[y][x].x_idx = x;
            level[y][x].y_idx = y;
            level[y][x].z_idx = z;
            level[y][x].type  = type;

            if (level[y][x].type == START_TILE) {
				marble_state3D->pos3D.x = x;
				marble_state3D->pos3D.y = y;
				marble_state3D->pos3D.z = z;
                printf("start: %f,%f\n",marble_state3D->pos3D.x,marble_state3D->pos3D.y);
			}

            // Move pointer past this cell's closing quote
            p = endq + 1;
        }
    }

    free(line);
    fclose(fp);
    return level;

fail:
    free(line);
    for (int j = 0; j < height; ++j) free(level[j]);
    free(level);
    fclose(fp);
    return NULL;
}