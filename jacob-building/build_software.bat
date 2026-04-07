@echo off
REM ============================================================
REM  build_software.bat — Transfer and build HPS software
REM ============================================================

REM --- Configuration (edit these) ---
set BOARD_IP=192.168.137.3
set BOARD_USER=root
set LOCAL_SOFTWARE_DIR=software
set REMOTE_SOFTWARE_DIR=/root/software
REM ----------------------------------

echo.
echo [1/2] Transferring software directory to board ...
scp -r "%LOCAL_SOFTWARE_DIR%" %BOARD_USER%@%BOARD_IP%:%REMOTE_SOFTWARE_DIR%
if %ERRORLEVEL% neq 0 (
    echo ERROR: scp failed. Is the board powered on and connected?
    pause
    exit /b 1
)
echo       Done.

echo.
echo [2/2] Building on board ...
ssh %BOARD_USER%@%BOARD_IP% "cd %REMOTE_SOFTWARE_DIR% && make clean && make"
if %ERRORLEVEL% neq 0 (
    echo ERROR: Build failed. Check the build output above.
    pause
    exit /b 1
)
echo       Done.

echo.
echo ============================================================
echo  SUCCESS: Software built on board.
echo  Binary is at %REMOTE_SOFTWARE_DIR% on the board.
echo ============================================================
pause