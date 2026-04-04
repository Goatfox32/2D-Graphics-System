#include "Vcommand_reader.h"
#include "verilated.h"
#include "verilated_vcd_c.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <string>

// ── Globals ──────────────────────────────────────────────────────────────────

static vluint64_t sim_time = 0;
static Vcommand_reader *dut;
static VerilatedVcdC *tfp;

static int tests_passed = 0;
static int tests_failed = 0;

// ── Avalon-MM slave model ────────────────────────────────────────────────────
//
// Phases:
//   S_IDLE  – waiting for avm_read
//   S_WAIT  – holding waitrequest high (configurable duration)
//   S_GAP   – one cycle with waitrequest=0, no data yet.
//             This is critical: the DUT needs one posedge to see
//             !waitrequest and transition from WAIT_REQ to READ_COMMAND
//             *before* we start asserting readdatavalid.
//   S_DATA  – delivering beats via readdatavalid

struct Beat {
    uint64_t data;
    int      gap;        // idle cycles *before* this beat's valid
};

enum SlaveState { S_IDLE, S_WAIT, S_GAP, S_DATA };

static std::vector<Beat> slave_beats;
static int        slave_wait_cycles = 1;
static SlaveState slave_state       = S_IDLE;
static int        slave_wait_ctr    = 0;
static int        slave_gap_ctr     = 0;
static int        slave_beat_idx    = 0;

static void slave_reset() {
    slave_beats.clear();
    slave_state     = S_IDLE;
    slave_wait_ctr  = 0;
    slave_gap_ctr   = 0;
    slave_beat_idx  = 0;
    dut->avm_waitrequest   = 0;
    dut->avm_readdatavalid = 0;
    dut->avm_readdata      = 0;
}

// Called once per tick, between negedge eval and posedge eval.
static void slave_tick() {
    dut->avm_readdatavalid = 0;
    dut->avm_readdata      = 0;
    dut->avm_waitrequest   = 0;

    switch (slave_state) {

    case S_IDLE:
        if (dut->avm_read) {
            slave_beat_idx = 0;
            if (slave_wait_cycles > 0) {
                dut->avm_waitrequest = 1;
                slave_wait_ctr = slave_wait_cycles - 1;  // this call counts as one
                slave_state = (slave_wait_ctr == 0) ? S_GAP : S_WAIT;
            } else {
                slave_state = S_GAP;
            }
        }
        break;

    case S_WAIT:
        dut->avm_waitrequest = 1;
        slave_wait_ctr--;
        if (slave_wait_ctr == 0)
            slave_state = S_GAP;
        break;

    case S_GAP:
        // waitrequest=0 (default), no data.
        // DUT sees !waitrequest this posedge → transitions to READ_COMMAND.
        // Next cycle we start delivering data.
        slave_gap_ctr = (!slave_beats.empty()) ? slave_beats[0].gap : 0;
        slave_state = S_DATA;
        break;

    case S_DATA:
        if (slave_beat_idx >= (int)slave_beats.size()) {
            slave_state = S_IDLE;
            break;
        }
        if (slave_gap_ctr > 0) {
            slave_gap_ctr--;
            break;
        }
        dut->avm_readdatavalid = 1;
        dut->avm_readdata      = slave_beats[slave_beat_idx].data;
        slave_beat_idx++;
        if (slave_beat_idx < (int)slave_beats.size())
            slave_gap_ctr = slave_beats[slave_beat_idx].gap;
        if (slave_beat_idx >= (int)slave_beats.size())
            slave_state = S_IDLE;
        break;
    }
}

// ── Clock / utility ─────────────────────────────────────────────────────────

static void tick() {
    // Negedge
    dut->clk = 0;
    dut->eval();
    tfp->dump(sim_time++);

    // Drive slave model between edges
    slave_tick();

    // Posedge
    dut->clk = 1;
    dut->eval();
    tfp->dump(sim_time++);
}

static void reset() {
    dut->reset_n = 0;
    dut->control = 0;
    dut->read_addr = 0;
    dut->read_size = 0;
    dut->command_buffer_full = 0;
    dut->data_buffer_full = 0;
    dut->data_buffer_count = 0;
    slave_reset();
    for (int i = 0; i < 4; i++) tick();
    dut->reset_n = 1;
    tick();
}

// Raise control[0] for one cycle then lower it.
static void start_pulse() {
    dut->control = 0x01;
    tick();
    dut->control = 0x00;
}

// ── Capture helpers ─────────────────────────────────────────────────────────

struct FifoCapture {
    std::vector<uint64_t> cmd;
    std::vector<uint64_t> data;
};

// Run cycles, capturing FIFO writes, until DUT is no longer busy or timeout.
static FifoCapture run_and_capture(int max_cycles = 200) {
    FifoCapture cap;
    for (int i = 0; i < max_cycles; i++) {
        tick();

        if (dut->command_buffer_en)
            cap.cmd.push_back(dut->command_buffer_data);
        if (dut->data_buffer_en)
            cap.data.push_back(dut->data_buffer_data);

        if (!(dut->status & 0x01))
            return cap;
    }
    printf("  [WARN] timed out waiting for idle\n");
    return cap;
}

// ── Check helper ────────────────────────────────────────────────────────────

static void check(bool cond, const char *name) {
    if (cond) {
        printf("  [PASS] %s\n", name);
        tests_passed++;
    } else {
        printf("  [FAIL] %s\n", name);
        tests_failed++;
    }
}

// ══════════════════════════════════════════════════════════════════════════════
//  Tests
// ══════════════════════════════════════════════════════════════════════════════

static void test_reset() {
    printf("\n--- test_reset ---\n");
    reset();

    check(dut->avm_read == 0,               "avm_read deasserted");
    check((dut->status & 0x01) == 0,         "not busy after reset");
    check((dut->status & 0x02) == 0,         "no size error");
    check(dut->command_buffer_en == 0, "cmd write_en low");
    check(dut->data_buffer_en == 0,    "data write_en low");
}

// ── NOP (command 0x00, burst = 1) ────────────────────────────────────────────

static void test_nop() {
    printf("\n--- test_nop ---\n");
    reset();

    slave_beats = { {0x00, 0} };
    slave_wait_cycles = 1;

    dut->read_addr = 0x1000;
    dut->read_size = 8;
    dut->data_buffer_count = 0;

    start_pulse();
    FifoCapture cap = run_and_capture();

    check(cap.cmd.empty(),              "no command pushed for NOP");
    check(cap.data.empty(),             "no data pushed for NOP");
    check((dut->status & 0x01) == 0,    "returns to idle");
    check((dut->status & 0x02) == 0,    "no size error");
}

// ── CLEAR (command 0x01, burst = 1) ──────────────────────────────────────────

static void test_clear() {
    printf("\n--- test_clear ---\n");
    reset();

    slave_beats = { {0x01, 0} };
    slave_wait_cycles = 2;

    dut->read_addr = 0x2000;
    dut->read_size = 8;
    dut->data_buffer_count = 0;

    start_pulse();
    FifoCapture cap = run_and_capture();

    check(cap.cmd.size() == 1,                      "1 command pushed");
    check(!cap.cmd.empty() && cap.cmd[0] == 0x01,   "command is CLEAR (0x01)");
    check(cap.data.empty(),                          "no data for CLEAR");
    check((dut->status & 0x01) == 0,                 "returns to idle");
}

// ── CLEAR with wrong burst → size error ──────────────────────────────────────

static void test_clear_size_error() {
    printf("\n--- test_clear_size_error ---\n");
    reset();

    slave_beats = { {0x01, 0}, {0xDEAD, 0} };
    slave_wait_cycles = 0;

    dut->read_addr = 0x3000;
    dut->read_size = 16;
    dut->data_buffer_count = 0;

    start_pulse();
    FifoCapture cap = run_and_capture();

    check(cap.cmd.empty(),              "no command pushed on size error");
    check((dut->status & 0x02) != 0,    "size_error flag set");
}

// ── DRAW_TRIANGLE (command 0x03, burst = 4) ──────────────────────────────────

static void test_draw_triangle() {
    printf("\n--- test_draw_triangle ---\n");
    reset();

    uint64_t v0 = 0xAAAABBBBCCCC0001ULL;
    uint64_t v1 = 0xDDDDEEEEFFFF0002ULL;
    uint64_t v2 = 0x1111222233330003ULL;

    slave_beats = {
        {0x03, 0},
        {v0,   0},
        {v1,   0},
        {v2,   0},
    };
    slave_wait_cycles = 1;

    dut->read_addr = 0x4000;
    dut->read_size = 32;
    dut->data_buffer_count = 0;

    start_pulse();
    FifoCapture cap = run_and_capture();

    check(cap.cmd.size() == 1,                      "1 command pushed");
    check(!cap.cmd.empty() && cap.cmd[0] == 0x03,   "command is DRAW_TRIANGLE");
    check(cap.data.size() == 3,                      "3 data beats pushed");

    if (cap.data.size() == 3) {
        check(cap.data[0] == v0,  "vertex 0 correct");
        check(cap.data[1] == v1,  "vertex 1 correct");
        check(cap.data[2] == v2,  "vertex 2 correct");
    }

    check((dut->status & 0x01) == 0,    "returns to idle");
    check((dut->status & 0x02) == 0,    "no size error");
}

// ── DRAW_TRIANGLE with inter-beat gaps ───────────────────────────────────────

static void test_draw_triangle_gaps() {
    printf("\n--- test_draw_triangle (gaps) ---\n");
    reset();

    uint64_t v0 = 0x0A;
    uint64_t v1 = 0x0B;
    uint64_t v2 = 0x0C;

    slave_beats = {
        {0x03, 0},
        {v0,   3},
        {v1,   1},
        {v2,   5},
    };
    slave_wait_cycles = 0;

    dut->read_addr = 0x5000;
    dut->read_size = 32;
    dut->data_buffer_count = 0;

    start_pulse();
    FifoCapture cap = run_and_capture(300);

    check(cap.cmd.size() == 1 && cap.cmd[0] == 0x03,  "command correct");
    check(cap.data.size() == 3,                         "3 data beats despite gaps");
    if (cap.data.size() == 3) {
        check(cap.data[0] == v0 && cap.data[1] == v1 && cap.data[2] == v2,
              "data values correct through gaps");
    }
}

// ── DRAW_TRIANGLE with wrong burst → size error ─────────────────────────────

static void test_draw_triangle_size_error() {
    printf("\n--- test_draw_triangle_size_error ---\n");
    reset();

    slave_beats = { {0x03, 0}, {0xBEEF, 0} };
    slave_wait_cycles = 0;

    dut->read_addr = 0x6000;
    dut->read_size = 16;
    dut->data_buffer_count = 0;

    start_pulse();
    FifoCapture cap = run_and_capture();

    check(cap.cmd.empty(),              "no command on size error");
    check((dut->status & 0x02) != 0,    "size_error flag set");
}

// ── Busy when data buffer nearly full ────────────────────────────────────────

static void test_busy_backpressure() {
    printf("\n--- test_busy_backpressure ---\n");
    reset();

    dut->data_buffer_count = 14;
    tick(); tick();
    check((dut->status & 0x01) != 0,    "busy when buffer nearly full");

    slave_beats = { {0x01, 0} };
    dut->read_addr = 0x7000;
    dut->read_size = 8;

    start_pulse();
    tick(); tick(); tick();

    check(dut->avm_read == 0,           "no read issued when busy");
}

// ── Unrecognized command → drains and returns to idle ────────────────────────

static void test_unrecognized_command() {
    printf("\n--- test_unrecognized_command ---\n");
    reset();

    slave_beats = { {0xFF, 0} };
    slave_wait_cycles = 0;

    dut->read_addr = 0x8000;
    dut->read_size = 8;
    dut->data_buffer_count = 0;

    start_pulse();
    FifoCapture cap = run_and_capture();

    check(cap.cmd.empty(),              "no command pushed for unknown cmd");
    check(cap.data.empty(),             "no data pushed");
    check((dut->status & 0x01) == 0,    "returns to idle");
}

// ── Back-to-back commands ────────────────────────────────────────────────────

static void test_back_to_back() {
    printf("\n--- test_back_to_back ---\n");
    reset();

    // First: CLEAR
    slave_beats = { {0x01, 0} };
    slave_wait_cycles = 0;
    dut->read_addr = 0xA000;
    dut->read_size = 8;
    dut->data_buffer_count = 0;

    start_pulse();
    FifoCapture cap1 = run_and_capture();
    check(cap1.cmd.size() == 1 && cap1.cmd[0] == 0x01,  "first command: CLEAR");

    // Second: DRAW_TRIANGLE
    uint64_t v0 = 0xAA, v1 = 0xBB, v2 = 0xCC;
    slave_beats = { {0x03, 0}, {v0, 0}, {v1, 0}, {v2, 0} };
    slave_wait_cycles = 1;
    dut->read_addr = 0xB000;
    dut->read_size = 32;

    start_pulse();
    FifoCapture cap2 = run_and_capture();
    check(cap2.cmd.size() == 1 && cap2.cmd[0] == 0x03,  "second command: DRAW_TRIANGLE");
    check(cap2.data.size() == 3,                          "second command: 3 data beats");
}

// ══════════════════════════════════════════════════════════════════════════════

int main(int argc, char **argv) {
    Verilated::commandArgs(argc, argv);
    Verilated::traceEverOn(true);

    dut = new Vcommand_reader;
    tfp = new VerilatedVcdC;
    dut->trace(tfp, 99);
    tfp->open("command_reader.vcd");

    test_reset();
    test_nop();
    test_clear();
    test_clear_size_error();
    test_draw_triangle();
    test_draw_triangle_gaps();
    test_draw_triangle_size_error();
    test_busy_backpressure();
    test_unrecognized_command();
    test_back_to_back();

    printf("\n========================================\n");
    printf("  %d passed, %d failed\n", tests_passed, tests_failed);
    printf("========================================\n");

    tfp->close();
    delete tfp;
    delete dut;

    return tests_failed > 0 ? 1 : 0;
}