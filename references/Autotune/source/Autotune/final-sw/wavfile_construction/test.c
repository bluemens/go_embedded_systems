#include <math.h>
#include "make_wav.h"
 
#define S_RATE  (44100)
#define BUF_SIZE (S_RATE*2) /* 2 second buffer */
 
int buffer[BUF_SIZE];
 
int main(int argc, char ** argv)
{
    int i;
    float t;
    float amplitude = 32000;
    float freq_Hz = 440;
    float phase=0;
 
    float freq_radians_per_sample = freq_Hz*2*M_PI/S_RATE;
 
    /* fill buffer with a sine wave */
    for (i=0; i<BUF_SIZE; i++)
    {
        phase += freq_radians_per_sample;
        buffer[i] = (int)(amplitude * sin(phase));
    }
 
    write_wav("test.wav", BUF_SIZE, buffer, S_RATE);
 
    return 0;
}