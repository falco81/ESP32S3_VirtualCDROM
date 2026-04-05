#!/bin/bash
# ESP32-S3 Virtual CD-ROM -- Flash Tool (Linux / macOS)

set -e
BOLD="\033[1m"; GREEN="\033[0;32m"; RED="\033[0;31m"; RESET="\033[0m"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

echo ""
echo -e "${BOLD}============================================================"
echo "  ESP32-S3 Virtual CD-ROM -- Flash Tool"
echo -e "============================================================${RESET}"
echo ""

# Python check
if ! command -v python3 &>/dev/null; then
    echo -e "${RED}ERROR: python3 not found.${RESET}"
    echo "  Debian/Ubuntu: sudo apt install python3 python3-pip"
    echo "  macOS:         brew install python"
    exit 1
fi

# Install esptool
echo "Installing esptool..."
python3 -m pip install esptool --quiet --upgrade 2>/dev/null \
  || python3 -m pip install esptool --quiet --upgrade --break-system-packages 2>/dev/null \
  || true

python3 -m esptool version &>/dev/null || { echo -e "${RED}ERROR: esptool unavailable.${RESET}"; exit 1; }

# Firmware check
if [ ! -f "firmware_merged.bin" ]; then
    echo -e "${RED}ERROR: firmware_merged.bin not found.${RESET}"
    echo "Place it in the same folder as this script."
    exit 1
fi

SIZE=$(wc -c < firmware_merged.bin)
echo -e "${GREEN}Firmware: firmware_merged.bin  (${SIZE} bytes)${RESET}"
echo ""

# Linux serial permissions hint
if [ -e /dev/ttyUSB0 ] && [ ! -w /dev/ttyUSB0 ]; then
    echo "Note: no write access to /dev/ttyUSB0."
    echo "      Run:  sudo usermod -aG dialout \$USER  then log out/in"
    echo "      Or:   sudo chmod a+rw /dev/ttyUSB0  (temporary)"
    echo ""
fi

echo "Connect the UART USB cable to the LEFT connector on the ESP32-S3."
echo -n "Press Enter when ready..."
read

echo ""
echo "Flashing..."
echo ""

# --after no_reset: esptool exits cleanly after writing.
# hard_reset hangs at 99.6% waiting for DTR/RTS response on CH343/CP2102
# modules that do not implement RTS-based reset. A separate 'run' command
# is issued afterwards to boot the firmware.
set +e
python3 -m esptool --chip esp32s3 --baud 921600 \
    --before default_reset --after no_reset \
    write_flash --flash_mode dio --flash_size 16MB --flash_freq 80m \
    0x0 firmware_merged.bin
RESULT=$?
set -e

if [ $RESULT -eq 0 ]; then
    # Trigger boot after successful flash
    echo ""
    echo "Resetting ESP32-S3..."
    python3 -m esptool --chip esp32s3 run &>/dev/null || true

    echo ""
    echo -e "${BOLD}${GREEN}============================================================"
    echo "  FLASH SUCCESSFUL"
    echo -e "============================================================${RESET}"
    echo ""
    echo "Next steps:"
    echo "  1. Disconnect the UART cable"
    echo "  2. Connect the OTG USB cable (RIGHT connector) to your PC"
    echo "  3. Open http://espcd.local in a browser"
    echo "  4. Configure Wi-Fi in the Config tab"
    echo ""
else
    echo ""
    echo -e "${BOLD}${RED}============================================================"
    echo "  FLASH FAILED"
    echo -e "============================================================${RESET}"
    echo ""
    echo "Troubleshooting:"
    echo "  1. Cable must be in the LEFT (UART/CH343) connector"
    echo "  2. Linux permissions: sudo chmod a+rw /dev/ttyUSB0"
    echo "  3. Hold BOOT, press RESET, release BOOT, then retry"
    echo "  4. Lower baud: edit this script, change 921600 to 460800"
    echo ""
    exit 1
fi
