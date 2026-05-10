#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <math.h>

#define TILE_SIZE 8
#define MAX_PALETTE 256

typedef struct {
    uint8_t r, g, b, a;
} rgba_t;

rgba_t palette[MAX_PALETTE];
int palette_size = 0;

// Load palette from .hex file (RRGGBBAA per line)
int load_palette(const char *filename) {
    FILE *f = fopen(filename, "r");
    if (!f) {
        perror("Cannot open palette file");
        return 0;
    }

    char line[16];
    while (fgets(line, sizeof(line), f) && palette_size < MAX_PALETTE) {
        unsigned int r, g, b, a;
        if (sscanf(line, "%02x%02x%02x%02x", &r, &g, &b, &a) == 4) {
            palette[palette_size].r = r;
            palette[palette_size].g = g;
            palette[palette_size].b = b;
            palette[palette_size].a = a;
            palette_size++;
        }
    }

    fclose(f);
    return palette_size;
}

// Find closest palette color (Euclidean distance in RGBA)
uint8_t closest_palette_index(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    int best_idx = 0;
    int best_dist = 999999;

    for (int i = 0; i < palette_size; ++i) {
        int dr = r - palette[i].r;
        int dg = g - palette[i].g;
        int db = b - palette[i].b;
        int da = a - palette[i].a;
        int dist = dr * dr + dg * dg + db * db + da * da;

        if (dist < best_dist) {
            best_dist = dist;
            best_idx = i;
        }
    }

    return (uint8_t)best_idx;
}

int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <tileset.png> <palette.hex>\n", argv[0]);
        return 1;
    }

    const char *img_path = argv[1];
    const char *palette_path = argv[2];

    // Load palette
    if (!load_palette(palette_path)) {
        fprintf(stderr, "Failed to load palette.\n");
        return 1;
    }

    int width, height, channels;
    uint8_t *img = stbi_load(img_path, &width, &height, &channels, 4); // Force RGBA
    if (!img) {
        fprintf(stderr, "Failed to load image: %s\n", img_path);
        return 1;
    }

    if (width % TILE_SIZE != 0 || height % TILE_SIZE != 0) {
        fprintf(stderr, "Image dimensions must be divisible by %d.\n", TILE_SIZE);
        stbi_image_free(img);
        return 1;
    }

    FILE *out = fopen("tileset.hex", "w");
    if (!out) {
        perror("Failed to open output file");
        stbi_image_free(img);
        return 1;
    }

    int tiles_x = width / TILE_SIZE;
    int tiles_y = height / TILE_SIZE;

    for (int ty = 0; ty < tiles_y; ++ty) {
        for (int tx = 0; tx < tiles_x; ++tx) {
            for (int row = 0; row < TILE_SIZE; ++row) {
                for (int col = 0; col < TILE_SIZE; ++col) {
                    int x = tx * TILE_SIZE + col;
                    int y = ty * TILE_SIZE + row;
                    int idx = (y * width + x) * 4;

                    uint8_t r = img[idx];
                    uint8_t g = img[idx + 1];
                    uint8_t b = img[idx + 2];
                    uint8_t a = img[idx + 3];

                    uint8_t index = closest_palette_index(r, g, b, a);
                    fprintf(out, "%02X", index);
                    if (col != TILE_SIZE - 1) fprintf(out, " ");
                }
                fprintf(out, "\n");
            }
            // fprintf(out, "\n");
        }
    }

    fclose(out);
    stbi_image_free(img);

    printf("Wrote tileset.hex using RGBA palette %s\n", palette_path);
    return 0;
}
