# External Integrations

**Analysis Date:** 2026-03-28

## Hardware Interfaces

**Motion Sensing (IMU - Inertial Measurement Unit):**
- MPU6050 (InvenSense 6-axis accelerometer/gyroscope)
  - Driver: `motion/mpu6050.h`
  - Interface: I2C bus
  - Registers: 6-axis data (ACCEL_XOUT_H through GYRO_ZOUT_L)
  - Used for: Clash detection, swing speed, motion-based effects
  - Config: `#define GYRO_CLASS MPU6050` (conditional)

- LSM6DS3H (STMicroelectronics 6-axis MEMS sensor)
  - Driver: `motion/lsm6ds3h.h`
  - Interface: I2C bus
  - Default on Proffieboard V1/V3
  - Config: `#define GYRO_CLASS LSM6DS3H`

- FXOS8700 (NXP accelerometer + magnetometer)
  - Driver: `motion/fxos8700.h`
  - Interface: I2C bus

- FXAS21002 (NXP gyroscope)
  - Driver: `motion/fxas21002.h`
  - Interface: I2C bus

**Configuration in Hardware Headers:**
- `config/proffieboard_v1_config.h`: LSM6DS3H + I2C pins 7/30 (STM32 pins PB9/PA9)
- `config/proffieboard_v3_config.h`: LSM6DS3H + I2C pins 18/19 (STM32 pins PB7/PB8)
- Motion interrupt: Pin 30 (Proffieboard V1), Pin 39 (Proffieboard V1), Pin 30 (V3)
- Sampling rates: `GYRO_MEASUREMENTS_PER_SECOND 1600` (V1), standard 1600Hz (V3)

## Data Storage

**SD Card Storage:**
- Protocol: SPI (Proffieboard V1) or SDIO (Proffieboard V3)
- Client: DOSFS (Arduino FS library wrapper) via `common/sd_card.h`, `common/lsfs.h`
- Mounting: Dynamic mounting/unmounting to save power when not needed
- Mount control: `common/sd_card.h` SDCard class manages lifecycle
- Files accessed:
  - `presets.ini` - Runtime preset configuration
  - `*.style` - Dynamic blade style definitions (new feature)
  - `tracks/*.wav` - Sound effect files
  - Font directories with numbered sound files (NEC/Plecter fonts)

**SD Card Pins (Proffieboard V1):**
- CS (Chip Select): Pin 4 (PB14)
- MOSI (Data Out): Pin 11 (PA7)
- MISO (Data In): Pin 12 (PA6)
- CLK (Clock): Pin 15 (PA5)

**SD Card Pins (Proffieboard V3):**
- Interface: SDIO (not SPI)
- Direct STM32L4 SDIO peripheral (faster, more reliable)

**File System:**
- DOSFS on STM32, SD library on Teensy
- Path resolution: Forward slashes (Linux-style paths)
- Case-sensitive or case-insensitive depending on SD card formatting

**Serial Flash Storage (Teensy optional):**
- SerialFlash library support (optional, `#ifdef ENABLE_SERIALFLASH`)
- Driver: `sound/` directory (serial flash WAV streaming)
- Use case: Store sound effects on Teensy's onboard flash

## Audio Output

**DAC (Digital-to-Analog Converter):**
- STM32L4: I2S-based audio with STM32L4 SAI peripheral
  - Driver: `sound/dac.h` (generic)
  - Hardware: I2S pins (BCLK, LRCLK, TXD0)
  - DMA: Direct Memory Access (STM32L4 DMA2 controller)
  - Pins (Proffieboard V1): bclkPin=3 (PB13), txd0Pin=31 (PA10), lrclkPin=2 (PB12)

- Teensy: Analog DAC output via PWM or dedicated DAC pins
  - Driver: `sound/dac.h`
  - Reference voltage: Adjustable (`#define LOUD` for 3V vref on Teensy)
  - DMA channel for streaming

**Audio Processing:**
- Dynamic mixer: `AudioDynamicMixer` (template-based, supports N channels)
- Compressor/limiter built into mixer (automatic gain control)
- Click avoider: `sound/click_avoider_lin.h` (prevent audio pops on transitions)
- Buffering: DMA half-buffer interrupts trigger refilling

**Amplifier Control:**
- Amplifier enable pin: 32 (Proffieboard V1), 32 (V3) - PA/PH pin
  - Controlled by: `sound/amplifier.h` class
  - Turns off during silent periods to save power

**Booster (Battery Boost Circuit):**
- Booster enable pin: 31 (Proffieboard V1), 31 (V3)
  - Controlled by: `common/booster.h` class
  - Enables when high current draw needed (sound playing + LEDs on)

## LED/Blade Output

**Addressable RGB LEDs (WS2811/NeoPixel):**
- Protocol: Single-wire PWM at 800 kHz or 400 kHz (configurable)
- Drivers:
  - `blades/ws2811_blade.h` - WS2811BladePtr template
  - `blades/esp32_ws2811.h` - ESP32 variant
  - `blades/stm32l4_ws2811.h` - STM32L4 variant
  - `blades/teensy4_ws2811.h` - Teensy 4.x variant
- Pins: Multiple blade pins available (bladePin, blade2Pin-blade9Pin)
- Data format: 24-bit RGB (8-bit per channel)
- Max LEDs: Configurable via `maxLedsPerStrip` (typically 144 per pin)

**PWM-Driven LEDs (RGB Stars, Segmented Blades):**
- Three-pin RGB control (one pin per color channel)
- Drivers: `blades/simple_blade.h`, `blades/pwm_pin.h`
- LED templates: CreeXPE2 series, other RGB star templates
- Blade power pins (6-pin control bus for up to 3 RGB stars):
  - Pin 20-25 (Proffieboard V1)
  - Pin 20-25 (Proffieboard V3)

**Blade Identification:**
- Blade ID Pin: Pin 1 (Proffieboard V1/V3 bladeIdentifyPin)
- Analog read on blade ID triggers blade detection
- Optional: `#define BLADE_ID_SCAN_MILLIS` for periodic scanning
- Use case: Auto-detect which blade is installed (different LED configs)

**LED Driver Templates (Compile-time):**
- `blades/drive_logic.h` - LED current calculations
- `blades/leds.h` - LED color space conversions (RGB, RGBW, HSV)
- Style system selects which LEDs illuminate and to what color/brightness

## Power Management

**Battery Monitoring:**
- Analog input pin: batteryLevelPin (Pin 29 on Proffieboard V1/V3 = PC4)
- Driver: `common/battery_monitor.h` - BatteryMonitor class
- Sampling: Async analog read with exponential filtering
- Voltage range: 2.85V (empty) to 4.1V (full) for LiPo battery
- Battery percentage calculation: Energy-based (quadratic voltage relationship)
- Low battery detection: Threshold-based with hysteresis
- State persistence: Can save battery state across power cycles

**Charge Detection:**
- Charge detect pin: 27 (Proffieboard V1/V3 = PA0)
- Used to: Detect when charging (optional feature)

## Serial Communication

**UART/Serial:**
- Driver: `common/serial.h`
- Pins: rxPin (Pin 16 = PC0), txPin (Pin 17 = PC1) on Proffieboard
- Baud rate: Configurable (typically 115200)
- Purpose: Debug output, CLI access
- Macro: STDOUT (redirected to serial or USB)

**USB (CDC - Virtual COM Port):**
- CDC (Communications Device Class) serial emulation
- Interface: Arduino Serial object
- Dual-purpose: Serial debug + USB-MSC mass storage
- Config: Selectable via Makefile FQBN (`usb=cdc`)

**I2C (Two-Wire Interface):**
- Driver: `common/i2cdevice.h` - I2CDevice base class
- Used for: Motion sensors, displays, and other I2C peripherals
- Library: Arduino Wire.h (STM32 or Teensy variant)
- Pins: i2cDataPin (SDA), i2cClockPin (SCL)

**SPI (Serial Peripheral Interface):**
- Used for: SD card (Proffieboard V1), LED drivers (optional)
- Library: Arduino SPI.h
- Pins: spiClock, spiDataIn, spiDataOut, sdCardSelectPin

## IR (Infrared) Support

**IR Receiver:**
- Drivers: `ir/receiver.h`, `ir/ir.h`
- Hardware: Passive IR sensor on GPIO pin (micromod configs)
- Protocols supported:
  - NEC: `ir/nec.h` - Standard IR protocol (buttons)
  - RC6: `ir/rc6.h` - Philips RC6 protocol
  - Custom: Framework extensible for new protocols

**IR Transmitter/Blaster:**
- Driver: `ir/blaster.h`
- Use case: Blaster prop mode (shoot IR pulses)
- Configuration: `#ifdef BLASTER_SHOTS_UNTIL_EMPTY`
- State: Bullet count tracking, mode detection

**IR Integration in Props:**
- Props can define IR handlers to respond to remote commands
- Example: Detonator prop receives IR trigger signals

## Display/UI

**OLED Display (SSD1306):**
- Driver: `display/ssd1306.h`
- Protocol: I2C or SPI communication
- Resolution: 128x64 pixels typical
- Enable: `#ifdef ENABLE_SSD1306` or `#include INCLUDE_SSD1306`
- Controller: Layer-based drawing system (`display/layer_controller.h`)
- Frame buffer: RGB565 format (`display/rgb565frame.h`)
- SPI variant: `display/spidisplay.h` for faster SPI displays

**Touch Buttons (STM32L4 capacitive):**
- Driver: `buttons/stm32l4_touchbutton.h`
- Hardware: Capacitive touch pads on STM32L4
- Use case: Touch-based menu navigation (menus.h modes)

## Audio Font System

**Font File Structure:**
- Directory-based organization: `/[FontName]/[Category]/[01.wav]` through `[NN.wav]`
- Two main formats supported:

**NEC Style (Polyphonic):**
- Multiple simultaneous sounds (hum + swing + clash)
- Numbered files: hum, swing, swing-accent, etc.
- Mixer combines all playing sounds

**Plecter Style (Monophonic):**
- Single sound at a time
- Long hum files, transition sounds
- Simpler but often with longer, more complex effects

**Hybrid Font (`HybridFont` class):**
- Can mix NEC and Plecter files in same font
- Effect class scans directory for available numbered files
- Automatic detection: Does file 01.wav exist? (NEC) or just hum.wav? (Plecter)

## Dynamic Style Loading (New Feature)

**SD Card `.style` Files:**
- Path: `/[FontDir]/Blade/color/[stylename].style` (example: `/DF V6/Blade/color/calk.style`)
- Format: Custom text-based style definition language
- Parser: `styles/style_parser.h` (StyleTokenizer, StyleFactoryBuilder)
- Lazy loading: `StyleFromSD()` defers parsing to first preset selection (no boot delay)

**Preset Configuration Files:**
- File: `presets.ini` on SD card
- Format: INI-style with sections
- Variables: `styledef=path` entries for style definitions
- Runtime validation in `CurrentPreset::Read()` (logs validity, doesn't store factories)
- Compile-time factories: `StyleFromSD()` wraps in LazyStyleFactory

## USB Mass Storage

**MTP (Media Transfer Protocol):**
- Driver: `mtp/mtpd.h` - MTP daemon
- Storage backends:
  - `mtp/mtp_storage_sd.h` - SD card access via MTP
  - `mtp/mtp_storage_serialflash.h` - Serial flash access via MTP
- Purpose: File transfer without unmounting SD card
- Enabled: Conditional `#ifdef USB_CLASS_MSC`

**USB-MSC (Mass Storage Class):**
- Arduino USB support for disk access
- Allows computer to browse SD card files directly
- Conflict: Can't use USB-CDC serial at same time (board must choose)

## Monitoring & Diagnostics

**Monitoring System:**
- Class: `common/monitoring.h` - Monitoring singleton
- Categories: Battery, profiling, logging levels
- Purpose: Selective debug output without console spam
- Integration: Used throughout codebase via `monitor.ShouldPrint()`

**Profiling/Performance:**
- Cycle counter: `common/scoped_cycle_counter.h`
- DMA interrupt cycles: Measured and logged
- Loop cycle analysis: Helps identify performance bottlenecks
- Logging: `audio_dma_interrupt_cycles`, `pixel_dma_interrupt_cycles`, `motion_interrupt_cycles`

## Clock Control

**Clock Scaling:**
- Driver: `common/clock_control.h`
- Purpose: Reduce CPU clock when not needed to save power
- STM32L4: Can run at 80 MHz (full speed) or lower during idle
- Configuration: Board-specific clock options in FQBN

## External Hardware Integration Summary

| Hardware | Interface | Driver | Config |
|----------|-----------|--------|--------|
| Motion Sensor (MPU6050/LSM6DS3H) | I2C | `motion/*.h` | `GYRO_CLASS` define |
| SD Card | SPI/SDIO | DOSFS, `sd_card.h` | `ENABLE_SD` |
| Audio DAC | I2S/PWM | `sound/dac.h` | `ENABLE_AUDIO` |
| Addressable LEDs | 1-wire PWM | `blades/ws2811_blade.h` | `ENABLE_WS2811` |
| PWM RGB LEDs | PWM x3 | `blades/simple_blade.h` | Pin config |
| Battery Monitor | Analog input | `battery_monitor.h` | `batteryLevelPin` |
| Amplifier | GPIO out | `amplifier.h` | `amplifierPin` |
| Booster | GPIO out | `booster.h` | `boosterPin` |
| IR Receiver | GPIO in | `ir/receiver.h` | Prop-specific |
| OLED Display | I2C/SPI | `display/ssd1306.h` | `ENABLE_SSD1306` |
| Touch Buttons | STM32 peripheral | `buttons/stm32l4_touchbutton.h` | `ENABLE_TOUCHBUTTON` |

---

*Integration audit: 2026-03-28*
