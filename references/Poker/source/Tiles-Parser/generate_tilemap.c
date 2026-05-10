#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stddef.h>

#define TILE_SIZE 8
#define MAX_TILES 4096
#define MAX_PALETTE 256

typedef struct {
    uint8_t r, g, b, a;
} rgba_t;

typedef struct {
    uint64_t hash;
    int tile_index;
} tile_hash_entry;

tile_hash_entry tile_hashes[MAX_TILES];
uint8_t tileset[MAX_TILES][64];
int tile_count = 0;

rgba_t palette[MAX_PALETTE];
int palette_size = 0;

uint64_t fnv1a_hash(const uint8_t *data, size_t len) {
    uint64_t hash = 14695981039346656037ULL;
    for (size_t i = 0; i < len; i++) {
        hash ^= data[i];
        hash *= 1099511628211ULL;
    }
    return hash;
}

int load_palette(const char *filename) {
    FILE *f = fopen(filename, "r");
    if (!f) return 0;
    char line[16];
    while (fgets(line, sizeof(line), f) && palette_size < MAX_PALETTE) {
        unsigned int r, g, b, a;
        if (sscanf(line, "%02x%02x%02x%02x", &r, &g, &b, &a) == 4) {
            palette[palette_size++] = (rgba_t){r, g, b, a};
        }
    }
    fclose(f);
    return palette_size;
}

uint8_t match_palette(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    for (int i = 0; i < palette_size; i++) {
        if (palette[i].r == r && palette[i].g == g && palette[i].b == b && palette[i].a == a)
            return i;
    }
    return 0xFF;
}

int load_tileset(const char *filename) {
    FILE *f = fopen(filename, "r");
    if (!f) return -1;
    char buffer[256];
    int line = 0;
    while (fgets(buffer, sizeof(buffer), f)) {
        if (line % 8 == 0 && tile_count >= MAX_TILES) break;
        char *token = strtok(buffer, " \t\r\n");
        for (int i = 0; i < 8 && token; i++) {
            uint8_t val = (uint8_t)strtol(token, NULL, 16);
            tileset[tile_count][(line % 8) * 8 + i] = val;
            token = strtok(NULL, " \t\r\n");
        }
        line++;
        if (line % 8 == 0) tile_count++;
    }
    fclose(f);
    return tile_count;
}

void build_tile_hashes(void) {
    for (int i = 0; i < tile_count; i++) {
        tile_hashes[i].hash = fnv1a_hash(tileset[i], 64);
        tile_hashes[i].tile_index = i;
    }
}

int tiles_equal(uint8_t *a, uint8_t *b) {
    for (int i = 0; i < 64; i++) {
        if (a[i] != b[i]) return 0;
    }
    return 1;
}

int find_tile_index(uint8_t *tile) {
    uint64_t h = fnv1a_hash(tile, 64);
    for (int i = 0; i < tile_count; i++) {
        if (tile_hashes[i].hash == h && tiles_equal(tile, tileset[i])) {
            return tile_hashes[i].tile_index;
        }
    }
    return -1;
}

int main(int argc, char **argv) {
    if (argc < 4) {
        fprintf(stderr, "Usage: %s <tilemap.png> <tileset.hex> <palette.hex>\n", argv[0]);
        return 1;
    }

    const char *tilemap_img = argv[1];
    const char *tileset_hex = argv[2];
    const char *palette_hex = argv[3];

    if (!load_palette(palette_hex)) {
        fprintf(stderr, "Failed to load palette.\n");
        return 1;
    }

    int width, height, channels;
    uint8_t *img = stbi_load(tilemap_img, &width, &height, &channels, 4);
    if (!img) {
        fprintf(stderr, "Failed to load tilemap image\n");
        return 1;
    }

    if (width % TILE_SIZE != 0 || height % TILE_SIZE != 0) {
        fprintf(stderr, "Tilemap image dimensions must be divisible by %d.\n", TILE_SIZE);
        stbi_image_free(img);
        return 1;
    }

    if (load_tileset(tileset_hex) < 0) {
        fprintf(stderr, "Failed to load tileset.hex\n");
        stbi_image_free(img);
        return 1;
    }

    build_tile_hashes();

    FILE *out = fopen("test.hex", "w");
    if (!out) {
        perror("Failed to open output file");
        stbi_image_free(img);
        return 1;
    }

    int tiles_x = width / TILE_SIZE;
    int tiles_y = height / TILE_SIZE;

    uint8_t current_tile[64];

    for (int ty = 0; ty < tiles_y; ty++) {
        for (int tx = 0; tx < tiles_x; tx++) {
            int p = 0;
            for (int row = 0; row < TILE_SIZE; row++) {
                for (int col = 0; col < TILE_SIZE; col++) {
                    int x = tx * TILE_SIZE + col;
                    int y = ty * TILE_SIZE + row;
                    int idx = (y * width + x) * 4;
                    uint8_t r = img[idx];
                    uint8_t g = img[idx + 1];
                    uint8_t b = img[idx + 2];
                    uint8_t a = img[idx + 3];
                    current_tile[p++] = match_palette(r, g, b, a);
                }
            }
            int index = find_tile_index(current_tile);
            if (index >= 0)
                fprintf(out, "%02X ", index);
            else {
                fprintf(stderr, "Tile at (%d,%d) not found\n", tx, ty);
                fprintf(out, "FF ");
            }
        }
        fprintf(out, "\n");
    }

    fclose(out);
    stbi_image_free(img);
    printf("Wrote tilemap.hex\n");
    return 0;
}
