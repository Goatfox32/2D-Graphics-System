#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>

#define LW_BRIDGE_BASE  0xFF200000
#define LW_BRIDGE_SIZE  0x1000

#define SDR_BASE        0xFFC25000
#define SDR_SIZE        0x1000

#define TEST_DATA_PHYS  0x18000000
#define TEST_DATA_SIZE  0x10000  // 64KB test region

#define CMD_ADDR    (0x00 / 4)
#define CMD_SIZE    (0x10 / 4)
#define GPU_CONTROL (0x20 / 4)
#define GPU_STATUS  (0x30 / 4)

#define STATICCFG   (0x5C / 4)
#define CPORTWIDTH  (0x64 / 4)
#define CPORTWMAP   (0x68 / 4)
#define CPORTRMAP   (0x6C / 4)
#define RFIFOCMAP   (0x70 / 4)
#define WFIFOCMAP   (0x74 / 4)
#define CPORTRDWR   (0x78 / 4)
#define FPGAPORTRST (0x80 / 4)

static volatile uint32_t *lw, *sdr, *data;

static volatile uint32_t *map_phys(int fd, off_t base, size_t size) {
    void *p = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, base);
    if (p == MAP_FAILED) { perror("mmap"); exit(1); }
    return (volatile uint32_t *)p;
}

// Single read test: returns 1 on success, 0 on failure
// expect_lsb: the expected value in result (lower 8 bits of first 64-bit word)
int do_read_test(uint32_t fpga_addr, uint8_t read_size, uint8_t expect_lsb) {
    // Clear start
    lw[GPU_CONTROL] = 0x00000000;
    usleep(100);

    // Set address and size
    lw[CMD_ADDR] = fpga_addr;
    lw[CMD_SIZE] = read_size;

    // Trigger FIRST
    lw[GPU_CONTROL] = 0x00000001;

    // Configure MPFE — transaction completes during brief window
    sdr[FPGAPORTRST] = 0x00003FFF;
    sdr[CPORTWIDTH] = 0x00044555;
    sdr[CPORTWMAP]  = 0x2C011000;
    sdr[CPORTRMAP]  = 0x00B00088;
    sdr[RFIFOCMAP]  = 0x00760210;
    sdr[WFIFOCMAP]  = 0x00980543;
    sdr[CPORTRDWR]  = 0x0005A56A;
    sdr[FPGAPORTRST] = 0x00000000;
    sdr[STATICCFG]  = 0x0000000A;

    // Clear start immediately
    lw[GPU_CONTROL] = 0x00000000;

    usleep(1000);

    uint32_t status = lw[GPU_STATUS];
    int done    = (status >> 7) & 1;
    uint8_t result = status & 0x7F;

    // result is only 7 bits (bit 7 is done), so mask expected too
    uint8_t expect_masked = expect_lsb & 0x7F;

    return (done == 1) && (result == expect_masked);
}

int main() {
    int fd = open("/dev/mem", O_RDWR | O_SYNC);
    if (fd < 0) { perror("open /dev/mem"); return 1; }

    lw   = map_phys(fd, LW_BRIDGE_BASE, LW_BRIDGE_SIZE);
    sdr  = map_phys(fd, SDR_BASE, SDR_SIZE);
    data = map_phys(fd, TEST_DATA_PHYS, TEST_DATA_SIZE);

    int pass = 0, fail = 0;

    printf("========================================\n");
    printf("  F2H SDRAM Read Test Suite\n");
    printf("========================================\n\n");

    // ----------------------------------------
    // TEST 1: Basic patterns at base address
    // ----------------------------------------
    printf("--- Test 1: Data patterns at base address ---\n");
    uint32_t patterns[] = {
        0xABCDBEEF, 0x00000000, 0xFFFFFFFF,
        0x12345678, 0xDEADCAFE, 0x01010101,
        0x000000FF, 0x0000FF00, 0x00FF0000
    };
    int num_patterns = sizeof(patterns) / sizeof(patterns[0]);

    for (int i = 0; i < num_patterns; i++) {
        data[0] = patterns[i];
        uint8_t expect = patterns[i] & 0xFF;

        int ok = do_read_test(0x03000000, 8, expect);
        printf("  Pattern 0x%08X → expect LSB 0x%02X: %s\n",
               patterns[i], expect, ok ? "PASS" : "FAIL");
        if (ok) pass++; else fail++;
    }

    // ----------------------------------------
    // TEST 2: Different byte offsets within the 64-bit word
    // The F2H port is 64-bit, so test what lands in bits [7:0]
    // Write known data and see what the FPGA reads back
    // ----------------------------------------
    printf("\n--- Test 2: Verify byte ordering ---\n");
    // Write two 32-bit words that form one 64-bit word
    // At phys 0x18000000: [63:32] = data[1], [31:0] = data[0]
    // FPGA reads bits [7:0] of the 64-bit readdata
    data[0] = 0x44332211;
    data[1] = 0x88776655;

    int ok = do_read_test(0x03000000, 8, 0x11);
    printf("  64-bit word [31:0]=0x44332211 [63:32]=0x88776655\n");
    printf("  readdata[7:0] → expect 0x11: %s\n", ok ? "PASS" : "FAIL");
    if (ok) pass++; else fail++;

    // ----------------------------------------
    // TEST 3: Different addresses
    // Each 64-bit word = 8 bytes, so FPGA addr increments by 1
    // per 8 bytes. HPS byte offset = FPGA_addr * 8 - 0x18000000
    // ----------------------------------------
    printf("\n--- Test 3: Different memory addresses ---\n");
    uint32_t offsets[] = { 0, 8, 16, 64, 256, 1024, 4096 };
    int num_offsets = sizeof(offsets) / sizeof(offsets[0]);

    for (int i = 0; i < num_offsets; i++) {
        uint32_t byte_off = offsets[i];
        uint32_t hps_word_idx = byte_off / 4;
        uint32_t fpga_addr = 0x03000000 + (byte_off / 8);
        uint32_t pattern = 0xA0 + i;  // unique per offset

        data[hps_word_idx] = pattern;

        ok = do_read_test(fpga_addr, 8, pattern & 0xFF);
        printf("  Byte offset %5u (FPGA addr 0x%08X) pattern 0x%02X: %s\n",
               byte_off, fpga_addr, pattern & 0xFF, ok ? "PASS" : "FAIL");
        if (ok) pass++; else fail++;
    }

    // ----------------------------------------
    // TEST 4: Burst sizes
    // read_size in bytes, burst = ceil(read_size/8)
    // Result always captures first beat's [7:0]
    // ----------------------------------------
    printf("\n--- Test 4: Different burst sizes ---\n");
    data[0] = 0xBBAABBCC;

    uint8_t sizes[] = { 8, 16, 24, 32, 64 };
    int num_sizes = sizeof(sizes) / sizeof(sizes[0]);

    for (int i = 0; i < num_sizes; i++) {
        ok = do_read_test(0x03000000, sizes[i], 0xCC);
        printf("  read_size=%3u bytes (burst=%u): %s\n",
               sizes[i], (sizes[i] + 7) / 8, ok ? "PASS" : "FAIL");
        if (ok) pass++; else fail++;
    }

    // ----------------------------------------
    // TEST 5: Rapid sequential reads
    // ----------------------------------------
    printf("\n--- Test 5: Rapid sequential reads (10x) ---\n");
    int seq_pass = 0;
    for (int i = 0; i < 10; i++) {
        uint32_t pattern = 0x10 + i;
        data[0] = pattern;

        ok = do_read_test(0x03000000, 8, pattern & 0xFF);
        if (ok) seq_pass++;
        else printf("  Iteration %d FAILED\n", i);
    }
    printf("  %d/10 sequential reads passed: %s\n",
           seq_pass, seq_pass == 10 ? "PASS" : "FAIL");
    if (seq_pass == 10) pass++; else fail++;

    // ----------------------------------------
    // TEST 6: Walking ones pattern
    // ----------------------------------------
    printf("\n--- Test 6: Walking ones in LSB ---\n");
    for (int bit = 0; bit < 8; bit++) {
        uint32_t pattern = 1 << bit;
        data[0] = pattern;

        ok = do_read_test(0x03000000, 8, pattern & 0xFF);
        printf("  Bit %d (0x%02X): %s\n", bit, pattern & 0xFF, ok ? "PASS" : "FAIL");
        if (ok) pass++; else fail++;
    }

    // ----------------------------------------
    // Summary
    // ----------------------------------------
    printf("\n========================================\n");
    printf("  Results: %d PASS, %d FAIL\n", pass, fail);
    printf("========================================\n");

    munmap((void *)lw,  LW_BRIDGE_SIZE);
    munmap((void *)sdr, SDR_SIZE);
    munmap((void *)data, TEST_DATA_SIZE);
    close(fd);

    return fail > 0 ? 1 : 0;
}