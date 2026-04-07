# 3D Graphics System — Windows Setup Guide

## Prerequisites

- **Quartus Prime Lite** installed (the scripts assume `C:\intelFPGA_lite\18.1\quartus\bin64` — edit the path in `program_fpga.bat` if yours differs)
- **Windows 10 or later** (ships with `ssh` and `scp` built in)
- **PuTTY** (for UART serial access) — download from https://www.putty.org
- The DE0-Nano-SoC / Atlas board, powered on and connected via ethernet

## Network Setup

The board is configured with a static IP of **192.168.137.3**. Connect an ethernet cable directly between your PC and the board.

On your Windows PC, configure the ethernet adapter:

1. Open **Settings → Network & Internet → Ethernet → adapter properties**
2. Select **Internet Protocol Version 4 (TCP/IPv4)** → Properties
3. Set a manual IP:
   - IP address: **192.168.137.1**
   - Subnet mask: **255.255.255.0**
   - Gateway: leave blank
4. Click OK

Verify connectivity by opening a terminal and running:

```
ping 192.168.137.3
```

## UART Serial Console (PuTTY)

The UART console gives you a terminal on the board that works even during boot — useful for debugging and for situations where SSH isn't available yet.

1. Connect the board's USB-UART port to your PC with a USB cable
2. Open **Device Manager** → expand **Ports (COM & LPT)** → note the port (e.g. `COM3`)
3. Open **PuTTY**
4. Select **Connection type: Serial**
5. Set **Serial line** to your COM port (e.g. `COM3`)
6. Set **Speed** to `115200`
7. Click **Open**

You should see a login prompt (or boot messages if the board is starting up). Log in as `root` (no password by default).

## Script 1: Deploy FPGA Bitstream (`program_fpga.bat`)

This script converts the Quartus `.sof` file to a `.rbf`, copies it to the board's boot partition, and then you reboot manually.

**Usage:**

1. Open a terminal in the project root directory (where `output_files/` is)
2. Run:
   ```
   program_fpga.bat
   ```
3. When it says SUCCESS, reboot the board — either type `reboot` in the UART console or power cycle it

**What it does:**

1. Runs `quartus_cpf` to convert `output_files/graphics_system.sof` → `.rbf`
2. SSHs into the board and mounts the boot partition at `/tmp/mnt`
3. Copies the `.rbf` to the boot partition

## Script 2: Build HPS Software (`build_software.bat`)

This script transfers the `software/` directory to the board and runs `make` on it.

**Usage:**

1. Open a terminal in the project root directory (where `software/` is)
2. Run:
   ```
   build_software.bat
   ```

**What it does:**

1. Copies the entire `software/` directory to `/root/software/` on the board via `scp`
2. SSHs in and runs `make clean && make`

## SSH Tips

Windows 10+ includes OpenSSH. You can SSH into the board at any time with:

```
ssh root@192.168.137.3
```

To avoid typing the IP every time, create the file `C:\Users\<you>\.ssh\config` with:

```
Host hps
    HostName 192.168.137.3
    User root
```

Then you can just type `ssh hps`.

## Troubleshooting

| Problem | Fix |
|---|---|
| `ssh: connect to host ... Connection refused` | Board isn't booted yet, or ethernet not connected. Wait for boot to finish (watch on UART) and check cable. |
| `ping` works but `ssh` doesn't | SSH server may not be running. Connect via UART and run `systemctl start sshd`. |
| `quartus_cpf` not found | Edit `QUARTUS_BIN` in `program_fpga.bat` to match your Quartus install path. |
| `scp` hangs or is very slow | First SSH connection does key exchange. Type `yes` when prompted to accept the host key. |
| UART shows no output | Check COM port number in Device Manager. Try unplugging and replugging USB. Make sure baud is 115200. |
| Board doesn't get its IP after reboot | Connect via UART and run `connmanctl services` to check network state. |