#!/bin/bash
# Build ProffieOS for Proffieboard V3 using arduino-cli
# Settings:
#   Board: Proffieboard V3 (STM32L452RE)
#   DOSFS: SDCARD (SDIO High Speed)
#   Optimize: Smallest Code
#   CPU Speed: 80 MHz
#   USB Type: Serial + Mass Storage + WebUSB
#   Peripheral Clock: Half

# Path to arduino-cli (bundled with Arduino IDE)
ARDUINO_CLI="/c/Users/theun/AppData/Local/Programs/Arduino IDE/resources/app/lib/backend/resources/arduino-cli.exe"

# Check if arduino-cli exists
if [ ! -f "$ARDUINO_CLI" ]; then
    echo "Error: arduino-cli not found at: $ARDUINO_CLI"
    echo "Please ensure Arduino IDE is installed."
    exit 1
fi

# FQBN with board options
FQBN="proffieboard:stm32l4:ProffieboardV3-L452RE:dosfs=sdmmc1,opt=os,speed=80,usb=cdc_msc_webusb,pclk=2"

# Get script directory
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

echo "Building ProffieOS for Proffieboard V3..."
echo "FQBN: $FQBN"
echo ""

# Run compilation
"$ARDUINO_CLI" compile --fqbn "$FQBN" "$SCRIPT_DIR"

# Check exit code
if [ $? -eq 0 ]; then
    echo ""
    echo "✓ Build successful!"
else
    echo ""
    echo "✗ Build failed"
    exit 1
fi
