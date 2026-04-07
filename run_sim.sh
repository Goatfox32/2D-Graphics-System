#!/bin/bash
set -e

echo "=== Verilating ==="
verilator --cc --trace --exe \
    -Wno-WIDTHEXPAND -Wno-WIDTHTRUNC -Wno-UNUSEDSIGNAL \
    command_pipeline.sv command_reader.sv command_executer.sv fifo.sv \
    tb.cpp

echo "=== Building ==="
make -C obj_dir -f Vcommand_pipeline.mk Vcommand_pipeline -j$(nproc)

echo "=== Running ==="
./obj_dir/Vcommand_pipeline

echo ""
echo "=== Waveform saved to command_pipeline.vcd ==="
echo "Open with: surfer command_executer.vcd"