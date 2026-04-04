#!/bin/bash
set -e

echo "=== Verilating ==="
verilator --cc --trace --exe \
    -Wno-WIDTHEXPAND -Wno-WIDTHTRUNC -Wno-UNUSEDSIGNAL \
    command_reader.sv \
    tb.cpp

echo "=== Building ==="
make -C obj_dir -f Vcommand_reader.mk Vcommand_reader -j$(nproc)

echo "=== Running ==="
./obj_dir/Vcommand_reader

echo ""
echo "=== Waveform saved to command_reader.vcd ==="
echo "Open with: surfer command_reader.vcd"