// tb_rasterizer.cpp
#include "Vrasterizer.h"
#include "verilated.h"
#include "verilated_vcd_c.h"
#include <cstdio>
#include <cstdint>
#include <vector>

static vluint64_t sim_time = 0;
static VerilatedVcdC* tfp = nullptr;
static Vrasterizer* dut = nullptr;

static void tick() {
    dut->clk = 0;
    dut->eval();
    if (tfp) tfp->dump(sim_time++);
    dut->clk = 1;
    dut->eval();
    if (tfp) tfp->dump(sim_time++);
}

// Pack one vertex into 64 bits: x[8:0], y[16:9], color[32:17], rest zero
static uint64_t pack_vertex(uint32_t x, uint32_t y, uint32_t rgb565) {
    return ((uint64_t)(x & 0x1FF))
         | ((uint64_t)(y & 0xFF) << 9)
         | ((uint64_t)(rgb565 & 0xFFFF) << 17);
}

int main(int argc, char** argv) {
    Verilated::commandArgs(argc, argv);
    dut = new Vrasterizer;

    Verilated::traceEverOn(true);
    tfp = new VerilatedVcdC;
    dut->trace(tfp, 99);
    tfp->open("rasterizer.vcd");

    // Reset (s1 is active-low reset per the module)
    dut->clk = 0;
    dut->s1 = 0;
    dut->vertex_valid = 0;
    for (int i = 0; i < 6; i++) dut->vertex_data[i] = 0;
    for (int i = 0; i < 4; i++) tick();

    dut->s1 = 1;  // release reset
    tick();
    tick();

    // Triangle: (50,40) red, (150,40) green, (100,120) blue
    // RGB565: R[4:0]=[15:11], G[5:0]=[10:5], B[4:0]=[4:0]
    // But per the module: r = color[4:0], g = color[10:5], b = color[15:11]
    // So "red" in this module's convention = color[4:0] = 0x1F
    uint64_t v1 = pack_vertex(50,  40,  0x001F);  // r=31
    uint64_t v2 = pack_vertex(150, 40,  0x07E0);  // g=63
    uint64_t v3 = pack_vertex(100, 120, 0xF800);  // b=31

    // Module expects vertex_data[63:0]=v1, [127:64]=v2, [191:128]=v3
    // Verilator represents 192-bit signals as an array of uint32_t words (6 words).
    // Access via dut->vertex_data[0..5], little-endian word order.
    dut->vertex_data[0] = (uint32_t)(v1 & 0xFFFFFFFF);
    dut->vertex_data[1] = (uint32_t)(v1 >> 32);
    dut->vertex_data[2] = (uint32_t)(v2 & 0xFFFFFFFF);
    dut->vertex_data[3] = (uint32_t)(v2 >> 32);
    dut->vertex_data[4] = (uint32_t)(v3 & 0xFFFFFFFF);
    dut->vertex_data[5] = (uint32_t)(v3 >> 32);

    // Wait for rast_ready, then pulse vertex_valid for 1 cycle
    int timeout = 100;
    while (!dut->rast_ready && timeout--) tick();
    if (!dut->rast_ready) {
        printf("FAIL: rast_ready never asserted after reset\n");
        return 1;
    }

    dut->vertex_valid = 1;
    tick();
    dut->vertex_valid = 0;

    // Scan should take roughly bbox_w * bbox_h cycles = 100 * 80 = 8000
    // Plus a couple handshake cycles. Cap at 20000 for safety.
    int pixel_count = 0;
    int max_cycles = 20000;
    bool saw_ready_drop = false;
    std::vector<std::tuple<int,int,int>> pixels;

    for (int i = 0; i < max_cycles; i++) {
        if (!dut->rast_ready) saw_ready_drop = true;

        if (dut->write_en) {
            pixel_count++;
            pixels.emplace_back(dut->write_x, dut->write_y, dut->write_color);
            if (pixel_count <= 10) {
                printf("pixel %d: (%d, %d) color=0x%02x\n",
                       pixel_count, dut->write_x, dut->write_y, dut->write_color);
            }
        }

        // Done when ready comes back high after having dropped
        if (saw_ready_drop && dut->rast_ready) break;
        tick();
    }

    printf("\n=== Summary ===\n");
    printf("saw rast_ready drop: %s\n", saw_ready_drop ? "yes" : "NO (handshake broken)");
    printf("total pixels written: %d\n", pixel_count);
    printf("rast_ready at end: %d\n", dut->rast_ready);

    // Sanity checks
    bool pass = true;
    if (!saw_ready_drop) { printf("FAIL: rast_ready never dropped\n"); pass = false; }
    if (pixel_count == 0) { printf("FAIL: no pixels written\n"); pass = false; }
    // Expected pixel count for this triangle: area = |100*(40-120) + 150*(120-40) + 100*(40-40)| / 2
    //                                             = |-8000 + 12000 + 0| / 2 = 2000
    if (pixel_count > 0) {
        printf("expected ~2000 pixels (triangle area), got %d\n", pixel_count);
        if (pixel_count < 1500 || pixel_count > 2500) {
            printf("WARN: pixel count far from expected\n");
        }
    }

    tfp->close();
    delete dut;
    printf("%s\n", pass ? "PASS" : "FAIL");
    return pass ? 0 : 1;
}