#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "wavfile_construction/make_wav.h"
#include "audio.h"
 
#define S_RATE  (8000)
#define BUF_SIZE (S_RATE*15) /* 15 second buffer */
 
long unsigned int buffer[BUF_SIZE];
int idx;
int audio_fd;

/* Read and save samples from the device to our buffer */
void read_samples() {
    audio_arg_t vla;
  
    if (ioctl(audio_fd, AUDIO_READ_SAMPLES, &vla)) {
        perror("ioctl(AUDIO_READ_SAMPLES) failed");
        printf("vla.data = %d\n", vla.data);
        return;
    }
    buffer[idx++] = vla.data;
}
 
int main(int argc, char ** argv)
{
    idx = 0;

    static const char filename[] = "/dev/unknown";

    printf("Audio Userspace program started\n");

    if ( (audio_fd = open(filename, O_RDWR)) == -1) {
        fprintf(stderr, "could not open %s\n", filename);
        return -1;
    }

    printf("buf size: %d\n", BUF_SIZE);
    while(idx < BUF_SIZE){
        // printf("idx: %d\n", idx);
        read_samples();
    }

    printf("sample read done");

    write_wav("./wavfiles/anonymous_audio.wav", BUF_SIZE, (long unsigned int *)buffer, S_RATE);

    printf("Audio Userspace program terminating\n");
    return 0;
}