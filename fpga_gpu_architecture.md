# FPGA GPU Command Protocol and System Architecture

## Overview

This system implements a simple **triangle rasterization GPU** in the
FPGA fabric of a Cyclone V SoC board.\
The **HPS (ARM CPU running Linux)** controls the GPU by writing commands
and vertex data into DDR memory and signaling the GPU using control
registers accessed through the lightweight HPS--FPGA bridge.

The FPGA GPU reads commands from DDR, processes them, rasterizes
triangles into a framebuffer stored in FPGA block RAM, and a VGA
controller outputs the framebuffer to a display.

The design intentionally keeps the GPU **read‑only with respect to DDR
memory**, simplifying the architecture.

------------------------------------------------------------------------

# System Architecture

                    +-----------------------+
                    |        HPS CPU        |
                    |      Linux OS         |
                    +-----------+-----------+
                                |
                Lightweight HPS → FPGA Bridge
                                |
                                v
                       GPU Control Registers
                                |
                                v
                            GPU Core
                                |
                                v
                     FPGA Avalon Memory Master
                                |
                  Full HPS → FPGA Memory Bridge
                                |
                                v
                              DDR3 RAM
                    (Command Buffer + Vertex Data)


    GPU Core Output Path

    GPU Core
       |
       v
    Framebuffer (FPGA BRAM)
       |
       v
    VGA Controller
       |
       v
    Monitor

------------------------------------------------------------------------

# Communication Paths

## 1. Lightweight Bridge (Register Control)

Used for **small control registers**.

Direction:

    HPS → FPGA

Registers allow the CPU to start rendering and check GPU status.

### Register Map

  Offset   Register   Direction   Description
  -------- ---------- ----------- ----------------------------------
  0x00     CMD_ADDR   CPU → GPU   Address of command buffer in DDR
  0x04     CMD_SIZE   CPU → GPU   Size of command buffer (bytes)
  0x08     START      CPU → GPU   Write 1 to begin execution
  0x0C     STATUS     GPU → CPU   GPU state (busy/done)

### Typical CPU Usage

1.  Write command buffer to DDR
2.  Write registers
3.  Trigger GPU

Example:

    CMD_ADDR = 0x20000000
    CMD_SIZE = 512
    START    = 1

CPU then polls:

    STATUS

------------------------------------------------------------------------

# 2. Full HPS--FPGA Bridge (Memory Reads)

Used for **high‑bandwidth data access**.

Direction:

    FPGA → DDR (read only)

The GPU reads:

-   command buffer
-   vertex data

The GPU **does not write to DDR**.

Framebuffer is stored locally in FPGA memory.

------------------------------------------------------------------------

# Command Buffer Format

Commands are stored sequentially in DDR memory.

Example layout:

    DDR Memory

    0x20000000
    --------------------------------
    CMD_DRAW_TRIANGLES
    vertex_count = 6

    v0
    v1
    v2
    v3
    v4
    v5

The GPU reads commands sequentially until `CMD_SIZE` bytes are
processed.

------------------------------------------------------------------------

# Vertex Format

Vertices are stored as **8 bytes each** for alignment with the 64‑bit
memory bus.

    struct Vertex
    {
        uint16 x;
        uint16 y;
        uint16 color;
        uint16 extra;
    };

Total size:

    8 bytes per vertex

This allows:

    1 DDR read = 1 vertex

when using a 64‑bit memory interface.

------------------------------------------------------------------------

# Example Command

### DRAW_TRIANGLES

    Opcode: 0x02
    Payload:
        vertex_count
        vertices...

Example:

    CMD_DRAW_TRIANGLES
    6 vertices

    v0
    v1
    v2
    v3
    v4
    v5

Interpreted as:

    Triangle 1: v0 v1 v2
    Triangle 2: v3 v4 v5

------------------------------------------------------------------------

# GPU Pipeline

The GPU hardware pipeline consists of:

    Command Reader
          |
          v
    Vertex Fetch
          |
          v
    Triangle Setup
          |
          v
    Rasterizer
          |
          v
    Framebuffer Write

------------------------------------------------------------------------

# Framebuffer

The framebuffer resides in **FPGA block RAM**.

Advantages:

-   deterministic latency
-   avoids DDR bandwidth contention
-   ideal for VGA scanout

### Example Size

#### 320×240 resolution

16‑bit color:

    320 × 240 × 2 bytes = 153,600 bytes

≈150 KB

#### 640×480 resolution

    640 × 480 × 2 bytes = 614,400 bytes

≈600 KB

------------------------------------------------------------------------

# VGA Output

The VGA controller continuously reads pixels from the framebuffer and
generates the VGA timing signals.

    Framebuffer → VGA Scanout → Monitor

GPU writes pixels while VGA reads them.

Optional improvements:

-   double buffering
-   tile rendering

------------------------------------------------------------------------

# Data Flow During Rendering

## CPU Side

1.  Write vertices to DDR
2.  Write command buffer to DDR
3.  Write GPU registers
4.  Trigger GPU

## GPU Side

1.  Read commands from DDR
2.  Fetch vertex data
3.  Rasterize triangles
4.  Write pixels into framebuffer

## Display

VGA controller reads framebuffer and outputs pixels to the monitor.

------------------------------------------------------------------------

# Key Design Properties

  Feature              Implementation
  -------------------- -----------------------------
  Command interface    Lightweight HPS→FPGA bridge
  Bulk data transfer   Full HPS→FPGA bridge
  Vertex storage       DDR memory
  Framebuffer          FPGA block RAM
  Display output       VGA controller
  Memory access        GPU reads DDR only

------------------------------------------------------------------------

# Advantages of This Architecture

-   Simple CPU--GPU interface
-   Minimal FPGA--DDR complexity
-   Deterministic framebuffer access
-   Scalable command buffer design
-   Efficient vertex reads using 64‑bit bus

------------------------------------------------------------------------

# Possible Future Extensions

-   indexed vertex buffers
-   texture support
-   double buffered framebuffers
-   DMA command streaming
-   tile‑based rasterization
