#ifndef _VGA_BALL_H
#define _VGA_BALL_H

#include <linux/ioctl.h>

// typedef struct {
//   unsigned char red, green, blue;
// } vga_ball_color_t;
  

// typedef struct {
//   vga_ball_color_t background;
// } vga_ball_arg_t;
typedef struct {
  unsigned char weight_data;
}top_weight_data_t;
typedef struct {
  unsigned char input_data;
}top_input_data_t;
typedef struct {
  unsigned char img_size;
}top_img_size_t;
typedef struct {
  unsigned char done;
}top_done_t;
typedef struct {
  unsigned char output_data;
}top_output_data_t;
typedef struct {
  top_input_data_t input_data;
  top_weight_data_t weight_data;
  top_img_size_t img_size;
  top_output_data_t output_data;
  top_done_t done;
} top_arg_t;
#define TOP_MAGIC 'q'

/* ioctls and their arguments */
#define TOP_WRITE_IMG_SIZE      _IOW(TOP_MAGIC, 1, top_arg_t)
#define TOP_WRITE_WEIGHT_DATA   _IOW(TOP_MAGIC, 2, top_arg_t)
#define TOP_WRITE_INPUT_DATA    _IOW(TOP_MAGIC, 3, top_arg_t)
#define TOP_READ_OUTPUT_DATA    _IOR(TOP_MAGIC, 4, top_arg_t)
#define TOP_READ_DONE           _IOR(TOP_MAGIC, 5, top_arg_t)

#endif
