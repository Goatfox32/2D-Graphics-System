#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <stdint.h>

#define LW_BRIDGE_BASE  0xFF200000
#define LW_BRIDGE_SIZE  0x1000

#define SDR_BASE        0xFFC25000
#define SDR_SIZE        0x1000

#define TEST_DATA_PHYS  0x18000000
#define TEST_DATA_SIZE  0x1000

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

static volatile uint32_t *map_phys(int fd, off_t base, size_t size) {
    void *p = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, base);
    if (p == MAP_FAILED) { perror("mmap"); exit(1); }
    return (volatile uint32_t *)p;
}

int main(int argc, char *argv[]) {
    uint32_t test_pattern = 0xABCDBEEF;
    uint32_t read_addr    = 0x03000000;

    if (argc > 1) test_pattern = strtoul(argv[1], NULL, 0);
    if (argc > 2) read_addr    = strtoul(argv[2], NULL, 0);

    int fd = open("/dev/mem", O_RDWR | O_SYNC);
    if (fd < 0) { perror("open /dev/mem"); return 1; }

    volatile uint32_t *lw   = map_phys(fd, LW_BRIDGE_BASE, LW_BRIDGE_SIZE);
    volatile uint32_t *sdr  = map_phys(fd, SDR_BASE, SDR_SIZE);
    volatile uint32_t *data = map_phys(fd, TEST_DATA_PHYS, TEST_DATA_SIZE);

    // Write test data
    data[0] = test_pattern;
    printf("Wrote 0x%08X to phys 0x%08X\n", test_pattern, TEST_DATA_PHYS);

    // Clear start
    lw[GPU_CONTROL] = 0x00000000;

    // Set address and size
    lw[CMD_ADDR] = read_addr;
    lw[CMD_SIZE] = 0x00000008;

    // STEP 1: Trigger FIRST — FSM enters WAIT_REQ
    lw[GPU_CONTROL] = 0x00000001;

    // STEP 2: Now configure MPFE — port briefly enables,
    // pending transaction completes during the window
    sdr[FPGAPORTRST] = 0x00003FFF;
    sdr[CPORTWIDTH] = 0x00044555;
    sdr[CPORTWMAP]  = 0x2C011000;
    sdr[CPORTRMAP]  = 0x00B00088;
    sdr[RFIFOCMAP]  = 0x00760210;
    sdr[WFIFOCMAP]  = 0x00980543;
    sdr[CPORTRDWR]  = 0x0005A56A;
    sdr[FPGAPORTRST] = 0x00000000;
    sdr[STATICCFG]  = 0x0000000A;

    // STEP 3: Clear start IMMEDIATELY to prevent re-trigger
    lw[GPU_CONTROL] = 0x00000000;

    // STEP 4: Small delay then read
    usleep(1000);

    uint32_t status = lw[GPU_STATUS];
    printf("Status: 0x%08X\n", status);
    printf("  done     = %d\n", (status >> 7) & 1);
    printf("  result   = 0x%02X\n", status & 0x7F);

    munmap((void *)lw,  LW_BRIDGE_SIZE);
    munmap((void *)sdr, SDR_SIZE);
    munmap((void *)data, TEST_DATA_SIZE);
    close(fd);
    return 0;
}