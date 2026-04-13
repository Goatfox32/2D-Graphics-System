
#include <stdint.h>
#include <stdio.h>
#include "comm.h"
#include "sprites.h"

/*
int main() {

    init_comm();


    clear();
    draw_triangle(160,  10, 0,  0,  24,
                  20,  200, 0,  63,  0,
                  300, 200, 0,  0,  31);

    // usleep(3300000);

    // uint64_t smile = SMILEY_FACE;
    // draw_sprite(40, 10, 0, 0, 31, &smile);

    return 0;
} */

int main(void) {
    init_comm();

    for (int x = 0; x < 10; x++) {
        clear();
        clear();
        // uint64_t smile = SMILEY_FACE;
        // draw_sprite(x * 20, 10, 0, 0, 31, &smile);

        usleep(2000000);
    }
    
    return 0;
}