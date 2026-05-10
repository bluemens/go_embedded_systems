#ifndef _VGA_TOP_H
#define _VGA_TOP_H

#include <linux/ioctl.h>
#include <linux/types.h>

/* ---------------- data types ---------------- */
typedef struct { __u32 value; } vga_top_ctrl_arg_t;
typedef struct { __u32 value; } vga_top_status_arg_t;
typedef struct {
	__u8  index;      /* 0-31                              */
	__u32 attr_word;  /* packed sprite attribute (see spec)*/
} vga_top_sprite_arg_t;

/* ---------------- ioctl magic ---------------- */
#define VGA_TOP_MAGIC 'q'

#define VGA_TOP_WRITE_CTRL     _IOW(VGA_TOP_MAGIC, 0x01, vga_top_ctrl_arg_t)
#define VGA_TOP_READ_STATUS    _IOR(VGA_TOP_MAGIC, 0x02, vga_top_status_arg_t)
#define VGA_TOP_WRITE_SPRITE   _IOW(VGA_TOP_MAGIC, 0x03, vga_top_sprite_arg_t)

#endif /* _VGA_TOP_H */
