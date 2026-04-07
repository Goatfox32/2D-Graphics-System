
#include <stdint.h>
#include <stdio.h>
#include "comm.h"

int main() {
    init_comm();

    // Example usage: send a command to the GPU
    draw_triangle(10, 20, 31, 0, 0,
                  20, 40, 0, 63, 0,
                  30, 60, 0, 0, 31);

    // Read status back from the GPU
    uint8_t status = read_status();
    printf("GPU Status: 0x%02X\n", status);

    return 0;
}
