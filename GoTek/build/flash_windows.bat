@echo off
setlocal enabledelayedexpansion
title ESP32-S3 Virtual CD-ROM -- Flash Tool
echo ============================================================
echo   ESP32-S3 Virtual CD-ROM -- Flash Tool
echo ============================================================
echo.
echo Flashes the complete firmware image to your ESP32-S3.
echo.
echo Requirements:
echo   - Python 3.x installed  (https://www.python.org)
echo   - CH343/CP2102 USB driver installed
echo   - ESP32-S3 connected via UART USB cable  (LEFT connector)
echo.

:: Check Python
python --version >nul 2>&1
if errorlevel 1 (
    echo ERROR: Python not found.
    echo Download and install from https://www.python.org/downloads/
    echo Make sure to check "Add Python to PATH" during install.
    pause
    exit /b 1
)

:: Install / update esptool
echo Installing / updating esptool...
pip install esptool --quiet --upgrade
if errorlevel 1 (
    echo ERROR: Failed to install esptool.
    pause
    exit /b 1
)

:: Check firmware file
if not exist "firmware_merged.bin" (
    echo ERROR: firmware_merged.bin not found.
    echo Make sure it is in the same folder as this script.
    pause
    exit /b 1
)

echo.
for %%I in (firmware_merged.bin) do echo Firmware size: %%~zI bytes
echo.
echo Ready to flash. Connect the UART USB cable (LEFT connector).
echo Press any key to start...
pause >nul
echo.
echo Flashing...
echo.

:: --after no_reset: esptool exits cleanly after writing
:: (hard_reset hangs at 99.6%% waiting for DTR/RTS response on CH343/CP2102)
python -m esptool --chip esp32s3 --baud 921600 ^
  --before default_reset --after no_reset ^
  write_flash --flash_mode dio --flash_size 16MB --flash_freq 80m ^
  0x0 firmware_merged.bin

if errorlevel 1 (
    echo.
    echo ============================================================
    echo   FLASH FAILED
    echo ============================================================
    echo.
    echo Troubleshooting:
    echo   1. Make sure cable is in the LEFT connector (UART, not OTG)
    echo   2. Install CH343 driver:
    echo      https://www.wch-ic.com/products/CH343.html
    echo   3. Hold BOOT button, press RESET, release BOOT, retry
    echo   4. Try a lower baud rate: change 921600 to 460800
    echo.
    pause
    exit /b 1
)

:: Trigger boot after successful flash
echo.
echo Resetting ESP32-S3...
python -m esptool --chip esp32s3 run >nul 2>&1

echo.
echo ============================================================
echo   FLASH SUCCESSFUL
echo ============================================================
echo.
echo Next steps:
echo   1. Disconnect the UART cable
echo   2. Connect the OTG USB cable (RIGHT connector) to your PC
echo   3. Open a browser at  http://espcd.local
echo   4. Configure Wi-Fi in the Config tab
echo      (set ssid / set pass / wifi reconnect via serial if needed)
echo.
pause
