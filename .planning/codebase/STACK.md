# Technology Stack

**Analysis Date:** 2026-03-28

## Languages

**Primary:**
- C++ (C++11/C++14 features) - Core firmware for all hardware targets
- Arduino (C++ variant) - Sketch-based entry point at `ProffieOS.ino`

## Runtime

**Environment:**
- Arduino Core (Teensyduino for Teensy boards, STM32L4 Arduino core for Proffieboard)
- Bare-metal ARM Cortex M4 microcontrollers

**Compiler:**
- GCC ARM (ARM EABI GCC compiler, invoked by Arduino CLI)
- Build system: Arduino-CLI with Makefile orchestration

**Package Manager:**
- Arduino Library Manager (via Arduino IDE/CLI)
- No traditional package manager; libraries included in Arduino core

## Frameworks

**Core Hardware Abstraction:**
- Arduino API (digitalWrite, pinMode, analogRead, etc.)
- STM32L4xx HAL (ARM-provided hardware abstraction layer) - For Proffieboard V1/V2/V3
- Teensyduino Libraries - For Teensy 3.x/4.x boards

**Audio System:**
- Custom ProffieOS DAC driver (in `sound/dac.h`, `sound/dac_esp32.h`)
- I2S Digital Audio Interface
- Audio streaming framework: ProffieOSAudioStream base class
- Dynamic mixer with compressor: `AudioDynamicMixer` (in `sound/dynamic_mixer.h`)

**LED/Blade Driver:**
- WS2811/NeoPixel support (addressable RGB LED strips)
- PWM LED support (RGB stars, segmented blades)
- FastLED integration (optional)
- Custom LED drivers in `blades/` directory

**State Machine & Event System:**
- Looper framework - For background loop-based tasks
- StateMachine class - For cooperative multitasking without threads
- SaberBase event broadcasting system - Decouples sound, blade, and prop logic

## Key Dependencies

**Critical:**
- Arduino Core Libraries (Wire.h for I2C, SPI.h, SD.h for SD card)
- SerialFlash.h - For Teensy serial flash support (optional)
- Snooze.h - For power management on Teensy (optional)
- ARM CMSIS DSP (`arm_math.h`) - For fast DSP operations (sqrt, FFT, etc.)

**Hardware Drivers:**
- stm32l4xx HAL headers - Direct STM32L4xx register access
- I2C/Wire protocol support - For motion sensors and I2C devices
- SPI protocol - For SD card and display interfaces

**Sound & Audio:**
- Custom WAV file parser and playback engine
- HybridFont class - Polyphonic audio mixing (NEC + Plecter font support)
- DMA-driven audio output with interrupt handling

**Blade & Lighting:**
- Template-based style system (in `styles/`) - Compile-time generated blade effects
- Color system: Color8 class with RGB/HSV support
- Blade style parser: Runtime style loading from SD card (new)

## Configuration

**Environment:**
- Arduino IDE Tools Menu (hardware selection, optimization, USB settings)
- Fully Qualified Board Name (FQBN) selection in Makefile
- CONFIG_FILE include directive - Selects hardware configuration at compile time
- Config hierarchy: `ProffieOS.ino` → CONFIG_FILE (e.g., `config/default_proffieboard_config.h`)

**Build Configuration Files:**
- `Makefile` - Primary build orchestration (Teensy and Proffieboard FQBN definitions)
- `ProffieOS.ino` - Sketch entry point with preprocessor directives
- Hardware configs in `config/` directory:
  - `proffieboard_v1_config.h` - Proffieboard V1 (STM32L433CC)
  - `proffieboard_v3_config.h` - Proffieboard V3 (STM32L452RE)
  - `default_v3_config.h` - Teensy V3 reference
  - `v3_config.h` - Teensy V3 base configuration
  - Multiple prop-specific configs (blaster, crossguard, graflex, etc.)

**Preset Configuration:**
- `presets[]` array defined in CONFIG_FILE
- Blade styles defined compile-time via StylePtr<> template instantiation
- Preset includes: font name, track file, blade styles per blade
- SD card-based preset loading: `presets.ini` file support (new)

## Hardware Targets

**Supported Boards:**

**ProffieBoard (STM32L4 series):**
- Proffieboard V1 (STM32L433CC) - 2MB flash, 256KB RAM
- Proffieboard V2 (STM32L433CC)
- Proffieboard V3 (STM32L452RE) - 512KB flash, 160KB RAM
- Compiler: STM32L4 Arduino core with GCC ARM
- FQBN pattern: `proffieboard:stm32l4:Proffieboard-L433CC:...`

**Teensy (NXP Kinetis):**
- Teensy 3.1/3.2 (MK20DX256)
- Teensy 3.5 (MK64FX512)
- Teensy 3.6 (MK66FX1M0)
- Teensy 4.1 (IMXRT1062)
- Compiler: Teensyduino (GCC ARM variant)
- FQBN pattern: `teensy:avr:teensy31:...`, `teensy:avr:teensy41:...`

**MicroMod (optional):**
- Generic Teensy/STM32-based configurations

## Platform Constraints

**Development:**
- Arduino IDE 1.8.15+ or Arduino CLI
- GCC ARM toolchain (bundled with Arduino cores)
- No external build system required (Makefile wraps arduino-cli)

**Flash Memory:**
- STM32L433: 256KB (tight for complex styles)
- STM32L452: 512KB (comfortable)
- Teensy 3.x: 256KB-1MB depending on variant
- **Challenge:** Compiled blade styles consume flash; SD card style loading (new) mitigates this

**RAM:**
- STM32L433: 64KB
- STM32L452: 160KB
- Teensy 4.1: 512KB
- **Challenge:** Audio buffering, preset array, style object creation all compete for heap

**Compilation Time:**
- Full rebuild typically 30-60 seconds
- Template-heavy codebase increases compiler load
- Test suite includes multiple device targets (test1-test9, testA-testD in Makefile)

## Special Features

**Conditional Compilation Flags:**
- `ENABLE_AUDIO` - Enable sound system
- `ENABLE_MOTION` - Enable IMU/motion sensing
- `ENABLE_WS2811` - Enable addressable LED support
- `ENABLE_SD` - Enable SD card support
- `ENABLE_SERIALFLASH` - Enable Teensy serial flash
- `ENABLE_SNOOZE` - Enable Teensy power management
- `ENABLE_DISPLAY_CODE` - Enable OLED display support (SSD1306)
- `SAVE_STATE` - Persist volume, preset, color changes across power cycles
- `DYNAMIC_BLADE_LENGTH` - Allow runtime blade length adjustment
- `DYNAMIC_CLASH_THRESHOLD` - Allow runtime clash sensitivity tuning

**Optimization Levels:**
- STM32L4: `opt=os` (size optimization)
- Teensy 3.x: `opt=o2std` (standard O2)
- Teensy 4.1: `opt=o2std` or faster
- Fast math enabled selectively for performance-critical code (see `__FAST_MATH__`)

---

*Stack analysis: 2026-03-28*
