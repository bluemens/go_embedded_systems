#ifndef _VGA_MARBLE_H
#define _VGA_MARBLE_H

#include <linux/ioctl.h>
#include <linux/kernel.h>
#include <linux/types.h>

typedef __u8 u8;
typedef __u16 u16;
typedef __u32 u32;

typedef struct  {
    u8 index;
    u8 r, g, b, a;
} vga_palette_arg;

typedef struct {
	u32 offset;
	u8 texture_index;
	u8 palette_index;
	u8 tile_priority;
	u8 horizontal_flip;
	u8 vertical_flip;
} vga_tile_arg;

typedef struct {
	u32 offset;
	char* data[32];
} vga_tile_texture_arg;

/* Figure out later */
typedef struct {
	unsigned int x, y;
} vga_marble_pos_t;

typedef struct {
	vga_marble_pos_t pos;
} vga_marble_pos_arg_t;

#define VGA_MARBLE_MAGIC 'q'

#define VGA_MARBLE_WRITE_POSITION _IOW(VGA_MARBLE_MAGIC, 1, vga_marble_pos_t)

#define VGA_MARBLE_WRITE_PALETTE   		_IOW(VGA_MARBLE_MAGIC, 2, vga_palette_arg)
#define VGA_MARBLE_WRITE_TILE      		_IOW(VGA_MARBLE_MAGIC, 3, vga_tile_arg)
#define VGA_MARBLE_WRITE_TILE_TEXTURE 		_IOW(VGA_MARBLE_MAGIC, 4, vga_tile_texture_arg)


#endif
