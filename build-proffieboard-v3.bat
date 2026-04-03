@echo off
REM Build ProffieOS for Proffieboard V3 using arduino-cli
REM Settings:
REM   Board: Proffieboard V3 (STM32L452RE)
REM   DOSFS: SDCARD (SDIO High Speed)
REM   Optimize: Smallest Code
REM   CPU Speed: 80 MHz
REM   USB Type: Serial + Mass Storage + WebUSB
REM   Peripheral Clock: Half

setlocal enabledelayedexpansion

REM Path to arduino-cli (bundled with Arduino IDE)
set ARDUINO_CLI=C:\Users\theun\AppData\Local\Programs\Arduino IDE\resources\app\lib\backend\resources\arduino-cli.exe

REM Check if arduino-cli exists
if not exist "!ARDUINO_CLI!" (
    echo Error: arduino-cli not found at: !ARDUINO_CLI!
    echo Please ensure Arduino IDE is installed.
    exit /b 1
)

REM FQBN with board options
set FQBN=proffieboard:stm32l4:ProffieboardV3-L452RE:dosfs=sdmmc1,opt=os,speed=80,usb=cdc_msc_webusb,pclk=2

REM Get script directory
set SCRIPT_DIR=%~dp0

echo Building ProffieOS for Proffieboard V3...
echo FQBN: !FQBN!
echo.

REM Run compilation
"!ARDUINO_CLI!" compile --fqbn "!FQBN!" "!SCRIPT_DIR:~0,-1!"

REM Check exit code
if %ERRORLEVEL% equ 0 (
    echo.
    echo Build successful!
) else (
    echo.
    echo Build failed
    exit /b 1
)
