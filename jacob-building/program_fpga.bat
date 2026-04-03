@echo off
REM ============================================================
REM  program_fpga.bat — Convert .sof to .rbf and deploy to board
REM ============================================================

REM --- Configuration (edit these) ---
set BOARD_IP=192.168.137.3
set BOARD_USER=root
set QUARTUS_BIN=C:\intelFPGA_lite\18.1\quartus\bin64
set SOF_FILE=output_files\graphics_system.sof
set RBF_FILE=output_files\graphics_system.rbf
REM ----------------------------------

echo.
echo [1/3] Converting .sof to .rbf ...
"%QUARTUS_BIN%\quartus_cpf" -c "%SOF_FILE%" "%RBF_FILE%"
if %ERRORLEVEL% neq 0 (
    echo ERROR: quartus_cpf failed. Check your Quartus path and .sof file.
    pause
    exit /b 1
)
echo       Done.

echo.
echo [2/3] Mounting boot partition on board ...
ssh %BOARD_USER%@%BOARD_IP% "mkdir -p /tmp/mnt && mount /dev/mmcblk0p1 /tmp/mnt"
if %ERRORLEVEL% neq 0 (
    echo ERROR: Could not SSH into board. Is it powered on and connected?
    pause
    exit /b 1
)
echo       Done.

echo.
echo [3/3] Copying .rbf to board ...
scp "%RBF_FILE%" %BOARD_USER%@%BOARD_IP%:/tmp/mnt/
if %ERRORLEVEL% neq 0 (
    echo ERROR: scp failed.
    pause
    exit /b 1
)
echo       Done.

echo.
echo ============================================================
echo  SUCCESS: .rbf deployed to board.
echo  Reboot the board to load the new FPGA configuration.
echo ============================================================
pause