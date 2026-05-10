struct ball_input {
    short dx;
    short dy;
    char left_button;
    char right_button;
};

//net change in the trackball since it was last called
struct ball_input getAccumulatedInput();

//sets up the thread that reads and processes mouse input, returns -1 on faulure
int setupReader();
