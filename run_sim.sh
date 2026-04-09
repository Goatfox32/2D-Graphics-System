#!/bin/bash
set -e

echo "=== Verilating ==="
verilator --cc --trace --exe \
    -Wno-WIDTHEXPAND -Wno-WIDTHTRUNC -Wno-UNUSEDSIGNAL \
    rasterizer.sv \
    tb.cpp

echo "=== Building ==="
make -C obj_dir -f Vrasterizer.mk Vrasterizer -j$(nproc)

echo "=== Running ==="
./obj_dir/Vrasterizer

echo ""
echo "=== Waveform saved to rasterizer.vcd ==="
echo "Open with: surfer rasterizer.vcd"