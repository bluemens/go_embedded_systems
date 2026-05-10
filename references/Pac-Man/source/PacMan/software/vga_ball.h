#ifndef _VGA_BALL_H_
#define _VGA_BALL_H_

#include <linux/ioctl.h>
// #include <stdint.h>
#include <linux/types.h>
#define VGA_BALL_MAGIC 'q'

// Sprite descriptor structure (matches your memory map)
struct sprite_desc {
    __u8 x;
    __u8 y;
    __u8 frame;
    __u8 visible;
    __u8 direction;
    __u8 type_id;
    __u8 reserved1;
    __u8 reserved2;
};

// IOCTL interface
#define VGA_BALL_WRITE_ALL _IOW(VGA_BALL_MAGIC, 1, struct vga_all_state)
#define VGA_BALL_READ_ALL  _IOR(VGA_BALL_MAGIC, 2, struct vga_all_state)

// Aggregate structure
struct vga_all_state {
    struct sprite_desc sprites[5]; // Pac-Man + 4 ghosts
    __u16 score;
    __u8 control;
    __u16 pellet_to_eat;
};

typedef struct sprite_desc sprite_desc_t;
typedef struct vga_all_state vga_all_state_t;

#endif
