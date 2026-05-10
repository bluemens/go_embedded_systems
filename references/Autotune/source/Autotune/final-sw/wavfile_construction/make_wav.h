/* Creates a WAV file from an array of ints.
 * Output is stereo, signed 32-bit samples
 * 
 * Based on code from Kevin Karplus, licensed under Creative Commons
 * https://karplus4arduino.wordpress.com/2011/10/08/making-wav-files-from-c-programs/
 */

#ifndef MAKE_WAV_H
#define MAKE_WAV_H
 
void write_wav(char * filename, unsigned long num_samples, long unsigned int * data, int s_rate);
 
#endif
