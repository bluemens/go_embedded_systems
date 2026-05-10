#ifndef _AUDIO_H
#define _AUDIO_H

#include <linux/ioctl.h>

typedef struct {
    int data; // 2048 elements each 32 bits
} audio_arg_t;

#define AUDIO_MAGIC 'q'

/* ioctls and their arguments */
#define AUDIO_READ_SAMPLES _IOR(AUDIO_MAGIC, 1, audio_arg_t *)

#endif
