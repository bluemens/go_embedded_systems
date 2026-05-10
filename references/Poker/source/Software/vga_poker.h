#ifndef _VGA_POKER_H
#define _VGA_POKER_H

#include <linux/ioctl.h>

typedef struct {
  short row, col;
  char tile_id;
} vga_poker_tile_coords_t;

typedef struct {
  char tile_id;
  char pixels[64];
} vga_poker_pixels_t;

typedef struct {
  char index, red, green, blue;
} vga_poker_palette_t;

typedef struct {
  vga_poker_tile_coords_t tile_coords;
  vga_poker_pixels_t pixel_coors;
  vga_poker_palette_t palette_t;
} vga_poker_arg_t;


#define VGA_POKER_MAGIC 'q'

/* ioctls and their arguments */
#define VGA_POKER_SET_TILE  _IOW(VGA_POKER_MAGIC, 1, vga_poker_arg_t)
#define VGA_POKER_SET_PALETTE  _IOW(VGA_POKER_MAGIC, 2, vga_poker_arg_t)
#define VGA_POKER_SET_PIXELS  _IOW(VGA_POKER_MAGIC, 3, vga_poker_arg_t)

#endif
