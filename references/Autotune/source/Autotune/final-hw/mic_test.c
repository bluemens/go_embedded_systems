#include <stdio.h>
#include <stdlib.h>
#include "write_wav.h"

int main() {
    FILE *fp;
    short int samples[1024];
    int i = 0;
    
    // Open the samples file
    fp = fopen("samples.txt", "r");
    if (fp == NULL) {
        printf("Error opening samples file\n");
        return 1;
    }
    
    // Read samples
    while (fscanf(fp, "%hd", &samples[i]) == 1 && i < 1024) {
        i++;
    }
    fclose(fp);
    
    // Write WAV file
    write_wav("output.wav", i, samples, 8000);
    
    printf("Created WAV file with %d samples\n", i);
    return 0;
} 