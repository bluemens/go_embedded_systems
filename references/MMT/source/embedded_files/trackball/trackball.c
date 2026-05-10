#include <stdio.h>
#include <linux/input.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>
#include <stdint.h>

#include "trackball.h"

int dx = 0;
int dy = 0;
//1 iff corresponding button was pressed since last checking
char left_button = 0;
char right_button = 0;

struct ball_input input_vals;

pthread_mutex_t mutex;
pthread_t reader_thread;


static void *reader_loop(void* arg)
{
    int fd = (intptr_t)arg;
    struct input_event ev;
    while (read(fd, &ev, sizeof(struct input_event)) > 0)
    {
        if (ev.type == EV_REL)
        {
            if (ev.code == REL_X)
            {
                pthread_mutex_lock(&mutex);
                input_vals.dx += ev.value;
                pthread_mutex_unlock(&mutex);
            }
            else if (ev.code == REL_Y)
            {
                pthread_mutex_lock(&mutex);
                input_vals.dy += ev.value;
                pthread_mutex_unlock(&mutex);
            }
        }
        if (ev.type == EV_KEY)
        {
            if (ev.value == 0)
            {
                continue;
            }
            else if (ev.code == BTN_RIGHT)
            {
                pthread_mutex_lock(&mutex);
                input_vals.right_button = 1;
                pthread_mutex_unlock(&mutex);
            }
            else if (ev.code == BTN_LEFT)
            {
                pthread_mutex_lock(&mutex);
                input_vals.left_button = 1;
                pthread_mutex_unlock(&mutex);
            } 
        }
    }
    return NULL;
}

int setupReader()
{
    int fd = open("/dev/input/event0",O_RDONLY);
    if (fd<0)
    {
        perror("open");
        return -1;
    }
    if (pthread_mutex_init(&mutex, NULL) != 0)
    {
        perror("mutex setup");
        return -1;
    }
    pthread_create(&reader_thread, NULL, reader_loop, (void*)fd);
    return 0;
}

void zero_out()
{  
    input_vals.dx = 0;
    input_vals.dy = 0;
    input_vals.left_button = 0;
    input_vals.right_button = 0;
}

void safe_zero()
{
    pthread_mutex_lock(&mutex);
    zero_out();
    pthread_mutex_unlock(&mutex);
}

struct ball_input getAccumulatedInput()
{
    pthread_mutex_lock(&mutex);
    struct ball_input net_rotation = input_vals;
    zero_out();

    pthread_mutex_unlock(&mutex);
    return net_rotation;
}



/*int main()
{
    setupReader();

    sleep(5);
    struct ball_input vals = getAccumulatedInput();
    printf("x: %d, y:%d\n", vals.dx, vals.dy);

}*/
