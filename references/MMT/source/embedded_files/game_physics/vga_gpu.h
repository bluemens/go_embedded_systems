#ifndef _VGA_MARBLE_H
#define _VGA_MARBLE_H

#include <linux/ioctl.h>
#include <linux/types.h>

typedef __u8 u8;
typedef __u16 u16;
typedef __u32 u32;

typedef struct {
  u16 ind;
  u8 palette_offs[8][8];
} GPUTextureArgs;

typedef struct {
  u8 layer, x, y;

  u16 texture_ind;
  u8 palette_ind;
  u8 priority, h_flip, v_flip;
} GPUTileArgs;

typedef struct {
  u8 ind;

  u16 x;
  u8 y;
  u16 texture_ind;
  u8 palette_ind;
  u8 h_flip, v_flip;
} GPUSpriteArgs;

typedef struct {
  u8 ind, off;
  u8 r, g, b, a;
} GPUPaletteArgs;

typedef struct {
  u32 force_blank : 1;
} GPUCtrlArgs;

typedef struct {
  u16 scroll_x;
  u16 scroll_y;
} GPUCScrollArgs;

typedef struct {
  u8 r, g, b, a;
} GPUBackgroundArgs;

typedef struct {
  u16 h_count;
  u16 v_count;
} GPUCountArgs;

typedef union {
  GPUTextureArgs texture;
  GPUTileArgs tile;
  GPUSpriteArgs sprite;
  GPUPaletteArgs palette;
  GPUCtrlArgs ctrl;
} GPUArgs;

#define VGA_MARBLE_MAGIC 'q'

#define VGA_GPU_WRITE_TEXTURE _IOW(VGA_MARBLE_MAGIC, 1, GPUTextureArgs)
#define VGA_GPU_WRITE_TILE _IOW(VGA_MARBLE_MAGIC, 2, GPUTileArgs)
#define VGA_GPU_WRITE_SPRITE _IOW(VGA_MARBLE_MAGIC, 3, GPUSpriteArgs)
#define VGA_GPU_WRITE_PALETTE _IOW(VGA_MARBLE_MAGIC, 4, GPUPaletteArgs)
#define VGA_GPU_WRITE_CTRL _IOW(VGA_MARBLE_MAGIC, 5, GPUCtrlArgs)
#define VGA_GPU_WRITE_SCROLL_X _IOW(VGA_MARBLE_MAGIC, 6, u16)
#define VGA_GPU_WRITE_SCROLL_Y _IOW(VGA_MARBLE_MAGIC, 7, u16)
#define VGA_GPU_READ_H_COUNT _IOW(VGA_MARBLE_MAGIC, 8, u16)
#define VGA_GPU_READ_V_COUNT _IOW(VGA_MARBLE_MAGIC, 9, u16)

#endif
