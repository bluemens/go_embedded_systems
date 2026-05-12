/*
 * Userspace program that communicates with the top device driver
 * through ioctls
 *
 * Hang Ye
 * Columbia University
 */

 #include <stdio.h>
 #include "top.h"
 #include <sys/ioctl.h>
 #include <sys/types.h>
 #include <sys/stat.h>
 #include <fcntl.h>
 #include <string.h>
 #include <unistd.h>
 #include <stdlib.h>
 
 int top_fd;
 
 /* Write img_size */
 void write_img_size(unsigned char img_size) {
     top_arg_t vla;
     vla.img_size.img_size = img_size;
     if (ioctl(top_fd, TOP_WRITE_IMG_SIZE, &vla)) {
         perror("ioctl(TOP_WRITE_IMG_SIZE) failed");
         return;
     }
 }
 
 /* Write input_data */
 void write_input_data(unsigned char input_data) {
     top_arg_t vla;
     vla.input_data.input_data = input_data;
     if (ioctl(top_fd, TOP_WRITE_INPUT_DATA, &vla)) {
         perror("ioctl(TOP_WRITE_INPUT_DATA) failed");
         return;
     }
 }
 
 /* Write weight_data */
 void write_weight_data(unsigned char weight_data) {
     top_arg_t vla;
     vla.weight_data.weight_data = weight_data;
     if (ioctl(top_fd, TOP_WRITE_WEIGHT_DATA, &vla)) {
         perror("ioctl(TOP_WRITE_WEIGHT_DATA) failed");
         return;
     }
 }
 
 /* Read done signal */
 unsigned char read_done() {
     top_arg_t vla;
     if (ioctl(top_fd, TOP_READ_DONE, &vla)) {
         perror("ioctl(TOP_READ_DONE) failed");
         return 0;
     }
     return vla.done.done;
 }
 
 /* Read output_data */
 unsigned char read_output_data() {
     top_arg_t vla;
     if (ioctl(top_fd, TOP_READ_OUTPUT_DATA, &vla)) {
         perror("ioctl(TOP_READ_OUTPUT_DATA) failed");
         return 0;
     }
     return vla.output_data.output_data;
 }
 
 int main() {
     static const char filename[] = "/dev/top";
     unsigned char img_size = 16; // Example img_size
     unsigned char input_data[16*16];
     signed char weight_data[3*3] = {-1, 0, 1, -2, 0, 2, -1, 0, 1};
//     signed char weight_data[3*3];
     unsigned char output_data[14*14];
     int golden_data[14*14];
     int i;
     int j;
 
     printf("Top Userspace program started\n");
 
     if ((top_fd = open(filename, O_RDWR)) == -1) {
         fprintf(stderr, "could not open %s\n", filename);
         return -1;
     }
 
     // Initialize input_data and weight_data
     for (i = 0; i < 16 ; i++) {
        for (j = 0; j < 16; j++) {
            if (j == 4 || j ==5 || j == 6 || j == 10 || j == 11 || j == 12 || j == 15){
                input_data[i*16+j] = 127;
            }
            else {
                input_data[i*16+j] = 0;
            }
        }
    }
//      // Initialize input_data and weight_data
//      for (i = 0; i < 16*16; i++) {
//         input_data[i] = rand() % 256; // Random value between 0 and 255
//     }
//     for (i = 0; i < 3*3; i++) {
//         weight_data[i] = rand() % 32;     // Example weight data: 32
//     }
     // Write img_size
     printf("Writing img_size: %d\n", img_size);
    //  write_img_size(img_size);
 
     // Write input_data
     printf("Writing input_data:\n");
     for (i = 0; i < 16*16; i++) {
         printf("  input_data[%d] = %d\n", i, input_data[i]);
         write_input_data(input_data[i]);
     }
    for (i = 0; i < 14; i++) 
        for (j = 0; j < 14; j++) {
            golden_data[i*14+j] =   input_data[i*16+j]          * weight_data[0]+
                                    input_data[i*16+j+1]        * weight_data[1]+
                                    input_data[i*16+j+2]        * weight_data[2]+
                                    input_data[(i+1)*16+j]      * weight_data[3]+
                                    input_data[(i+1)*16+j+1]    * weight_data[4]+
                                    input_data[(i+1)*16+j+2]    * weight_data[5]+
                                    input_data[(i+2)*16+j]      * weight_data[6]+
                                    input_data[(i+2)*16+j+1]    * weight_data[7]+
                                    input_data[(i+2)*16+j+2]    * weight_data[8];
        }


     printf("Writing weight_data:\n");
     for (i = 0; i < 9; i++) {
         printf("  weight_data[%d] = %d\n", i, weight_data[i]);
         write_weight_data(weight_data[i]);
     }
     // Wait for done signal
     printf("Waiting for done signal...\n");
     i = 0;
     while (read_done() == 0){
         printf("  done = 0\n");
         i = i + 1;
         usleep(100000); // Sleep for 100ms
     }
     printf("  done = 1\n");
     printf("Done signal received!\n");
 
     // Read output_data
     printf("Reading output_data:\n");
     for (i = 0; i < 14*14; i++) {
        output_data[i] = read_output_data();
        //  output_data[i] = (int)(golden_data[i]/4);
         printf("  output_data[%d] = %d\n", i, output_data[i]);
 
         // Verify output_data
         if (golden_data[i] !=0){
         if ((abs(output_data[i] * 4 - golden_data[i]) / golden_data[i]) > 0.5) {
             fprintf(stderr, "Error: output_data[%d] = %d (expected %d)\n",
                     i, output_data[i], golden_data[i]);
         }}
     }
     for (i = 0; i < 14; i++) {
         for (j = 0; j < 14; j++) {
             printf("  %d, ", output_data[i*14+j]);
             }
            printf("\n");
            }
     printf("Top Userspace program terminating\n");
     return 0;
 }
//  /*
//  * Userspace program that communicates with the top device driver
//  * through ioctls
//  *
//  * Hang Ye
//  * Columbia University
//  */

