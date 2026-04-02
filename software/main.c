

#include <stdint.h>
#include <stdio.h>
#include "comm.h"

int main() {
    init_comm();

    // Example usage: send a command to the GPU
    draw_triangle(30, 20, 255, 0, 0,
                  20, 40, 0, 255, 0,
                  10, 60, 0, 0, 255);

    // Read status back from the GPU
    uint8_t status = read_status();
    printf("GPU Status: 0x%02X\n", status);

    return 0;
}
