# F2H SDRAM Read Procedure — DE0-Nano-SoC / Atlas Board

## One-time setup (per program)

`mmap` the three regions:
- LW bridge: `0xFF200000` (size `0x1000`)
- SDR controller: `0xFFC25000` (size `0x1000`)
- Reserved DDR3: `0x18000000` (size as needed)

## For each read transaction

1. **Clear start** — write `0` to `GPU_CONTROL` PIO (`LW + 0x20`)

2. **Small delay** — `usleep(100)` to let the edge detector settle

3. **Set read address** — write the **word address** to `CMD_ADDR` PIO (`LW + 0x00`). Word address = byte address ÷ 8. Your reserved region starts at byte `0x18000000`, so FPGA base address is `0x03000000`. For byte offset N within the region: FPGA addr = `0x03000000 + N/8`

4. **Set read size** — write byte count to `CMD_SIZE` PIO (`LW + 0x10`). Must be ≥ 1. Burst count will be `ceil(read_size / 8)`. Keep ≤ 255 bytes (max burst = 31 on this port, but your 8-bit field limits to 255 bytes = 31 beats)

5. **Assert start** — write `1` to `GPU_CONTROL`

6. **Configure MPFE** — in this exact order:
   - Write `0x00003FFF` to `FPGAPORTRST` (`SDR + 0x80`) — hold ports in reset
   - Write `0x00044555` to `CPORTWIDTH` (`SDR + 0x64`)
   - Write `0x2C011000` to `CPORTWMAP` (`SDR + 0x68`)
   - Write `0x00B00088` to `CPORTRMAP` (`SDR + 0x6C`)
   - Write `0x00760210` to `RFIFOCMAP` (`SDR + 0x70`)
   - Write `0x00980543` to `WFIFOCMAP` (`SDR + 0x74`)
   - Write `0x0005A56A` to `CPORTRDWR` (`SDR + 0x78`)
   - Write `0x00000000` to `FPGAPORTRST` (`SDR + 0x80`) — release reset
   - Write `0x0000000A` to `STATICCFG` (`SDR + 0x5C`) — apply config

7. **Deassert start** — write `0` to `GPU_CONTROL` immediately

8. **Wait** — `usleep(1000)` or poll

9. **Read status** — read `GPU_STATUS` PIO (`LW + 0x30`). Bit 7 = done, bits 6:0 = `result[6:0]`

## Why this order matters

Steps 5 and 6 are the critical trick. The FSM must be in `WAIT_REQ` with a pending read *before* you configure the MPFE. The MPFE registers only hold their values for a brief window before hardware reverts them. The pending transaction completes during that window. If you configure MPFE first and then trigger, the registers revert before the transaction starts.

## Address mapping cheat sheet

| HPS byte address     | FPGA word address | Offset from base |
|----------------------|-------------------|------------------|
| `0x18000000`         | `0x03000000`      | 0                |
| `0x18000008`         | `0x03000001`      | 8 bytes          |
| `0x18000010`         | `0x03000002`      | 16 bytes         |
| `0x18000000 + N`     | `0x03000000 + N/8`| N bytes          |

## What the FSM captures

`result` = `readdata[7:0]` of the first 64-bit beat. That's the least significant byte of the first 32-bit word at the target address. For `0xABCDBEEF`, that's `0xEF`. You only see 7 bits in the status register because bit 7 is shared with `done`.

## PIO Address Map

| Register    | LW Bridge Offset | Direction     |
|-------------|-------------------|---------------|
| cmd_addr    | `0x00`            | HPS → FPGA   |
| cmd_size    | `0x10`            | HPS → FPGA   |
| gpu_control | `0x20`            | HPS → FPGA   |
| gpu_status  | `0x30`            | FPGA → HPS   |

## SDR Controller Register Map

| Register    | Address        | Correct Value  |
|-------------|----------------|----------------|
| STATICCFG   | `0xFFC2505C`   | `0x0000000A`   |
| CPORTWIDTH  | `0xFFC25064`   | `0x00044555`   |
| CPORTWMAP   | `0xFFC25068`   | `0x2C011000`   |
| CPORTRMAP   | `0xFFC2506C`   | `0x00B00088`   |
| RFIFOCMAP   | `0xFFC25070`   | `0x00760210`   |
| WFIFOCMAP   | `0xFFC25074`   | `0x00980543`   |
| CPORTRDWR   | `0xFFC25078`   | `0x0005A56A`   |
| FPGAPORTRST | `0xFFC25080`   | `0x00000000`   |