#include "Vcommand_pipeline.h"
#include "verilated.h"
#include "verilated_vcd_c.h"

#include <cstdio>
#include <cstdint>
#include <vector>

// ── Globals ──────────────────────────────────────────────────────────────────

static vluint64_t sim_time = 0;
static Vcommand_pipeline *dut;
static VerilatedVcdC *tfp;

static int tests_passed = 0;
static int tests_failed = 0;

// ── Avalon-MM slave model ────────────────────────────────────────────────────
// Identical to the command_reader testbench slave.
// S_GAP ensures 1-cycle separation between waitrequest drop and first data.

struct Beat {
    uint64_t data;
    int      gap;
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
                slave_wait_ctr = slave_wait_cycles - 1;
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
    dut->clk = 0;
    dut->eval();
    tfp->dump(sim_time++);

    slave_tick();

    dut->clk = 1;
    dut->eval();
    tfp->dump(sim_time++);
}

static void reset() {
    dut->reset_n    = 0;
    dut->control    = 0;
    dut->read_addr  = 0;
    dut->read_size  = 0;
    dut->rast_ready = 1;
    slave_reset();
    for (int i = 0; i < 4; i++) tick();
    dut->reset_n = 1;
    tick();
}

static void start_pulse() {
    dut->control = 0x01;
    tick();
    dut->control = 0x00;
}

// Fire a start pulse and configure the SDRAM slave in one call.
static void issue_command(uint32_t addr, uint32_t size,
                          std::vector<Beat> beats, int wait = 1) {
    slave_beats       = beats;
    slave_wait_cycles = wait;
    dut->read_addr    = addr;
    dut->read_size    = size;
    start_pulse();
}

// ── Capture helpers ─────────────────────────────────────────────────────────

struct PipeCapture {
    int clear_pulses;
    int vertex_valid_pulses;
    struct VertexBundle {
        uint64_t v[3];
    };
    std::vector<VertexBundle> vertices;
};

// Wait for the full pipeline to settle:
//   reader not busy, both FIFOs empty, executer idle.
static PipeCapture run_pipeline(int max_cycles = 500) {
    PipeCapture cap = {};
    for (int i = 0; i < max_cycles; i++) {
        tick();

        if (dut->clear)
            cap.clear_pulses++;

        if (dut->vertex_valid) {
            cap.vertex_valid_pulses++;
            PipeCapture::VertexBundle vb;
            const auto *w = dut->vertex_data.data();
            vb.v[0] = (uint64_t)w[0] | ((uint64_t)w[1] << 32);
            vb.v[1] = (uint64_t)w[2] | ((uint64_t)w[3] << 32);
            vb.v[2] = (uint64_t)w[4] | ((uint64_t)w[5] << 32);
            cap.vertices.push_back(vb);
        }

        // Pipeline settled: reader idle, FIFOs empty, no output pulses
        bool reader_idle = !(dut->status & 0x01);
        bool fifos_empty = (dut->cmd_fifo_count == 0) && (dut->data_fifo_count == 0);
        bool exec_quiet  = !dut->vertex_valid && !dut->clear;

        if (reader_idle && fifos_empty && exec_quiet && i > 5) {
            // A few extra cycles to confirm it's truly settled
            for (int j = 0; j < 3; j++) {
                tick();
                if (dut->clear) cap.clear_pulses++;
                if (dut->vertex_valid) {
                    cap.vertex_valid_pulses++;
                    PipeCapture::VertexBundle vb;
                    const auto *w = dut->vertex_data.data();
                    vb.v[0] = (uint64_t)w[0] | ((uint64_t)w[1] << 32);
                    vb.v[1] = (uint64_t)w[2] | ((uint64_t)w[3] << 32);
                    vb.v[2] = (uint64_t)w[4] | ((uint64_t)w[5] << 32);
                    cap.vertices.push_back(vb);
                }
            }
            return cap;
        }
    }
    printf("  [WARN] pipeline timed out\n");
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

    check(dut->avm_read == 0,        "avm_read deasserted");
    check((dut->status & 0x01) == 0,  "reader not busy");
    check(dut->vertex_valid == 0,     "vertex_valid low");
    check(dut->clear == 0,           "clear low");
    check(dut->cmd_fifo_count == 0,   "cmd FIFO empty");
    check(dut->data_fifo_count == 0,  "data FIFO empty");
}

// ── NOP end-to-end ───────────────────────────────────────────────────────────

static void test_nop_e2e() {
    printf("\n--- test_nop (end-to-end) ---\n");
    reset();

    issue_command(0x1000, 8, { {0x00, 0} });
    PipeCapture cap = run_pipeline();

    check(cap.clear_pulses == 0,        "no clear");
    check(cap.vertex_valid_pulses == 0, "no vertex_valid");
}

// ── CLEAR end-to-end ─────────────────────────────────────────────────────────

static void test_clear_e2e() {
    printf("\n--- test_clear (end-to-end) ---\n");
    reset();

    issue_command(0x2000, 8, { {0x01, 0} });
    PipeCapture cap = run_pipeline();

    check(cap.clear_pulses == 1,        "1 clear pulse");
    check(cap.vertex_valid_pulses == 0, "no vertex_valid");
}

// ── DRAW_TRIANGLE end-to-end ─────────────────────────────────────────────────

static void test_draw_triangle_e2e() {
    printf("\n--- test_draw_triangle (end-to-end) ---\n");
    reset();

    uint64_t v0 = 0xAAAABBBBCCCC0001ULL;
    uint64_t v1 = 0xDDDDEEEEFFFF0002ULL;
    uint64_t v2 = 0x1111222233330003ULL;

    issue_command(0x3000, 32, {
        {0x03, 0},   // command byte
        {v0,   0},   // first data → slot [0]
        {v1,   0},   // second     → slot [1]
        {v2,   0},   // third      → slot [2]
    });

    PipeCapture cap = run_pipeline();

    check(cap.clear_pulses == 0,         "no clear");
    check(cap.vertex_valid_pulses == 1,  "1 vertex_valid pulse");

    if (!cap.vertices.empty()) {
        auto &vb = cap.vertices[0];
        check(vb.v[0] == v0,  "slot [0] correct (bits 63:0)");
        check(vb.v[1] == v1,  "slot [1] correct (bits 127:64)");
        check(vb.v[2] == v2,  "slot [2] correct (bits 191:128)");
    }
}

// ── DRAW_TRIANGLE with bus delays ────────────────────────────────────────────

static void test_draw_triangle_slow_bus() {
    printf("\n--- test_draw_triangle (slow bus) ---\n");
    reset();

    uint64_t v0 = 0x0A, v1 = 0x0B, v2 = 0x0C;

    issue_command(0x4000, 32, {
        {0x03, 0},
        {v0,   3},   // 3 idle cycles before vertex 0
        {v1,   2},
        {v2,   5},
    }, /*wait=*/3);

    PipeCapture cap = run_pipeline();

    check(cap.vertex_valid_pulses == 1,  "1 vertex_valid");
    if (!cap.vertices.empty()) {
        auto &vb = cap.vertices[0];
        check(vb.v[0] == v0 && vb.v[1] == v1 && vb.v[2] == v2,
              "vertices correct despite bus delays");
    }
}

// ── DRAW_TRIANGLE with rast_ready stall ──────────────────────────────────────

static void test_draw_triangle_rast_stall() {
    printf("\n--- test_draw_triangle (rast_ready stall) ---\n");
    reset();
    dut->rast_ready = 0;

    uint64_t v0 = 0x10, v1 = 0x20, v2 = 0x30;

    issue_command(0x5000, 32, {
        {0x03, 0}, {v0, 0}, {v1, 0}, {v2, 0},
    });

    // Let reader finish and executer consume data, but stall on rast_ready
    for (int i = 0; i < 30; i++) tick();
    check(dut->vertex_valid == 0,  "stalls without rast_ready");

    dut->rast_ready = 1;
    PipeCapture cap = run_pipeline();

    check(cap.vertex_valid_pulses == 1,  "vertex_valid fires after rast_ready");
    if (!cap.vertices.empty()) {
        check(cap.vertices[0].v[0] == v0,  "data survived stall");
    }
}

// ── CLEAR then DRAW_TRIANGLE sequentially ────────────────────────────────────
// Two separate SDRAM reads, one after the other.

static void test_clear_then_triangle() {
    printf("\n--- test_clear_then_triangle ---\n");
    reset();

    // First: CLEAR
    issue_command(0x6000, 8, { {0x01, 0} });
    PipeCapture cap1 = run_pipeline();
    check(cap1.clear_pulses == 1,  "clear fires");

    // Second: DRAW_TRIANGLE
    uint64_t v0 = 0xAA, v1 = 0xBB, v2 = 0xCC;
    issue_command(0x7000, 32, {
        {0x03, 0}, {v0, 0}, {v1, 0}, {v2, 0},
    });
    PipeCapture cap2 = run_pipeline();

    check(cap2.vertex_valid_pulses == 1,  "triangle fires after clear");
    if (!cap2.vertices.empty()) {
        auto &vb = cap2.vertices[0];
        check(vb.v[0] == v0 && vb.v[1] == v1 && vb.v[2] == v2,
              "triangle data correct");
    }
}

// ── Size error: CLEAR with wrong burst ───────────────────────────────────────

static void test_size_error_no_output() {
    printf("\n--- test_size_error_no_output ---\n");
    reset();

    // CLEAR command byte but burst=2 (wrong)
    issue_command(0x8000, 16, { {0x01, 0}, {0xDEAD, 0} });
    PipeCapture cap = run_pipeline();

    check(cap.clear_pulses == 0,        "no clear on size error");
    check(cap.vertex_valid_pulses == 0, "no vertex_valid on size error");
    check((dut->status & 0x02) != 0,    "size_error flag set");
}

// ── Unknown command passes through without effect ────────────────────────────

static void test_unknown_command_e2e() {
    printf("\n--- test_unknown_command (end-to-end) ---\n");
    reset();

    issue_command(0x9000, 8, { {0xFF, 0} });
    PipeCapture cap = run_pipeline();

    check(cap.clear_pulses == 0,        "no clear");
    check(cap.vertex_valid_pulses == 0, "no vertex_valid");
}

// ── Multiple triangles back-to-back ──────────────────────────────────────────

static void test_two_triangles() {
    printf("\n--- test_two_triangles ---\n");
    reset();

    uint64_t a0 = 0x100, a1 = 0x200, a2 = 0x300;
    uint64_t b0 = 0x400, b1 = 0x500, b2 = 0x600;

    // First triangle
    issue_command(0xA000, 32, {
        {0x03, 0}, {a0, 0}, {a1, 0}, {a2, 0},
    });
    PipeCapture cap1 = run_pipeline();
    check(cap1.vertex_valid_pulses == 1,  "first triangle valid");
    if (!cap1.vertices.empty()) {
        auto &vb = cap1.vertices[0];
        check(vb.v[0] == a0 && vb.v[1] == a1 && vb.v[2] == a2,
              "first triangle data correct");
    }

    // Second triangle
    issue_command(0xB000, 32, {
        {0x03, 0}, {b0, 0}, {b1, 0}, {b2, 0},
    });
    PipeCapture cap2 = run_pipeline();
    check(cap2.vertex_valid_pulses == 1,  "second triangle valid");
    if (!cap2.vertices.empty()) {
        auto &vb = cap2.vertices[0];
        check(vb.v[0] == b0 && vb.v[1] == b1 && vb.v[2] == b2,
              "second triangle data correct");
    }
}

// ══════════════════════════════════════════════════════════════════════════════

int main(int argc, char **argv) {
    Verilated::commandArgs(argc, argv);
    Verilated::traceEverOn(true);

    dut = new Vcommand_pipeline;
    tfp = new VerilatedVcdC;
    dut->trace(tfp, 99);
    tfp->open("command_pipeline.vcd");

    test_reset();
    test_nop_e2e();
    test_clear_e2e();
    test_draw_triangle_e2e();
    test_draw_triangle_slow_bus();
    test_draw_triangle_rast_stall();
    test_clear_then_triangle();
    test_size_error_no_output();
    test_unknown_command_e2e();
    test_two_triangles();

    printf("\n========================================\n");
    printf("  %d passed, %d failed\n", tests_passed, tests_failed);
    printf("========================================\n");

    tfp->close();
    delete tfp;
    delete dut;

    return tests_failed > 0 ? 1 : 0;
}