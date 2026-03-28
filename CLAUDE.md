<!-- GSD:project-start source:PROJECT.md -->
## Project

**ProffieOS SD Card Style Loader**

A runtime SD card style loader for ProffieOS that enables lightsaber blade lighting styles to be loaded from `.style` files on the SD card instead of being compiled into firmware. Users write `StyleFromSD("path/to/style.style")` in their preset arrays alongside existing `StylePtr<>()` styles — both return the same `StyleAllocator` type and are fully interchangeable. Styles are parsed lazily on first preset selection, adding no boot delay.

**Core Value:** Edit lightsaber blade styles by swapping SD card files — no firmware recompile needed.

### Constraints

- **Memory:** STM32L452RE — 512KB flash, 160KB RAM. Style objects must be freed after use, no leaks across style switches.
- **Performance:** Style run() called every frame (60fps). Parser runs once per preset selection, not per frame.
- **Compatibility:** All existing StylePtr<> usage must continue to work unchanged. No changes to Preset struct layout.
- **No boot delay:** SD card not read during setup(). Lazy loading is non-negotiable.
- **No exceptions:** C++ exceptions disabled on embedded targets. All errors via return values or null checks.
- **Recursion depth:** .style files can nest deeply (chimera reaches 21 levels). Parser must guard against stack overflow.
- **Stack size:** ~4-8KB available. Recursive descent parser must be careful.
<!-- GSD:project-end -->

<!-- GSD:stack-start source:codebase/STACK.md -->
## Technology Stack

## Languages
- C++ (C++11/C++14 features) - Core firmware for all hardware targets
- Arduino (C++ variant) - Sketch-based entry point at `ProffieOS.ino`
## Runtime
- Arduino Core (Teensyduino for Teensy boards, STM32L4 Arduino core for Proffieboard)
- Bare-metal ARM Cortex M4 microcontrollers
- GCC ARM (ARM EABI GCC compiler, invoked by Arduino CLI)
- Build system: Arduino-CLI with Makefile orchestration
- Arduino Library Manager (via Arduino IDE/CLI)
- No traditional package manager; libraries included in Arduino core
## Frameworks
- Arduino API (digitalWrite, pinMode, analogRead, etc.)
- STM32L4xx HAL (ARM-provided hardware abstraction layer) - For Proffieboard V1/V2/V3
- Teensyduino Libraries - For Teensy 3.x/4.x boards
- Custom ProffieOS DAC driver (in `sound/dac.h`, `sound/dac_esp32.h`)
- I2S Digital Audio Interface
- Audio streaming framework: ProffieOSAudioStream base class
- Dynamic mixer with compressor: `AudioDynamicMixer` (in `sound/dynamic_mixer.h`)
- WS2811/NeoPixel support (addressable RGB LED strips)
- PWM LED support (RGB stars, segmented blades)
- FastLED integration (optional)
- Custom LED drivers in `blades/` directory
- Looper framework - For background loop-based tasks
- StateMachine class - For cooperative multitasking without threads
- SaberBase event broadcasting system - Decouples sound, blade, and prop logic
## Key Dependencies
- Arduino Core Libraries (Wire.h for I2C, SPI.h, SD.h for SD card)
- SerialFlash.h - For Teensy serial flash support (optional)
- Snooze.h - For power management on Teensy (optional)
- ARM CMSIS DSP (`arm_math.h`) - For fast DSP operations (sqrt, FFT, etc.)
- stm32l4xx HAL headers - Direct STM32L4xx register access
- I2C/Wire protocol support - For motion sensors and I2C devices
- SPI protocol - For SD card and display interfaces
- Custom WAV file parser and playback engine
- HybridFont class - Polyphonic audio mixing (NEC + Plecter font support)
- DMA-driven audio output with interrupt handling
- Template-based style system (in `styles/`) - Compile-time generated blade effects
- Color system: Color8 class with RGB/HSV support
- Blade style parser: Runtime style loading from SD card (new)
## Configuration
- Arduino IDE Tools Menu (hardware selection, optimization, USB settings)
- Fully Qualified Board Name (FQBN) selection in Makefile
- CONFIG_FILE include directive - Selects hardware configuration at compile time
- Config hierarchy: `ProffieOS.ino` → CONFIG_FILE (e.g., `config/default_proffieboard_config.h`)
- `Makefile` - Primary build orchestration (Teensy and Proffieboard FQBN definitions)
- `ProffieOS.ino` - Sketch entry point with preprocessor directives
- Hardware configs in `config/` directory:
- `presets[]` array defined in CONFIG_FILE
- Blade styles defined compile-time via StylePtr<> template instantiation
- Preset includes: font name, track file, blade styles per blade
- SD card-based preset loading: `presets.ini` file support (new)
## Hardware Targets
- Proffieboard V1 (STM32L433CC) - 2MB flash, 256KB RAM
- Proffieboard V2 (STM32L433CC)
- Proffieboard V3 (STM32L452RE) - 512KB flash, 160KB RAM
- Compiler: STM32L4 Arduino core with GCC ARM
- FQBN pattern: `proffieboard:stm32l4:Proffieboard-L433CC:...`
- Teensy 3.1/3.2 (MK20DX256)
- Teensy 3.5 (MK64FX512)
- Teensy 3.6 (MK66FX1M0)
- Teensy 4.1 (IMXRT1062)
- Compiler: Teensyduino (GCC ARM variant)
- FQBN pattern: `teensy:avr:teensy31:...`, `teensy:avr:teensy41:...`
- Generic Teensy/STM32-based configurations
## Platform Constraints
- Arduino IDE 1.8.15+ or Arduino CLI
- GCC ARM toolchain (bundled with Arduino cores)
- No external build system required (Makefile wraps arduino-cli)
- STM32L433: 256KB (tight for complex styles)
- STM32L452: 512KB (comfortable)
- Teensy 3.x: 256KB-1MB depending on variant
- **Challenge:** Compiled blade styles consume flash; SD card style loading (new) mitigates this
- STM32L433: 64KB
- STM32L452: 160KB
- Teensy 4.1: 512KB
- **Challenge:** Audio buffering, preset array, style object creation all compete for heap
- Full rebuild typically 30-60 seconds
- Template-heavy codebase increases compiler load
- Test suite includes multiple device targets (test1-test9, testA-testD in Makefile)
## Special Features
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
- STM32L4: `opt=os` (size optimization)
- Teensy 3.x: `opt=o2std` (standard O2)
- Teensy 4.1: `opt=o2std` or faster
- Fast math enabled selectively for performance-critical code (see `__FAST_MATH__`)
<!-- GSD:stack-end -->

<!-- GSD:conventions-start source:CONVENTIONS.md -->
## Conventions

## Naming Patterns
- Header guards: `#ifndef [DIRECTORY]_[FILENAME]_H` / `#define [DIRECTORY]_[FILENAME]_H`
- Implementation files: `.cpp` for standalone compile units, `.ino` for Arduino sketch
- Test files: `tests.cpp` (one per module) containing all tests for a subsystem
- PascalCase for class names
- Base classes often have "Base" suffix
- Template implementations often have "Impl" suffix
- camelCase for method names
- Virtual methods marked with `override` keyword
- Getter methods typically prefix with `get_` or just return without prefix
- State machine methods: `Loop()`, `Send()`, `SendClick()`
- Private/protected members use trailing underscore (`_`) convention
- Public struct members: no trailing underscore (direct data members)
- Config constants: SCREAMING_SNAKE_CASE
- Macro functions: mix of styles depending on purpose
- Static constants: camelCase or SCREAMING_SNAKE_CASE depending on type
- Enum class names: PascalCase
- Enum values: SCREAMING_SNAKE_CASE
- Global functions: camelCase
- Arduino-like global functions: lowercase with underscores
## Code Style
- No auto-formatter detected (no .clang-format, .prettierrc, or eslint config found)
- Manual style observed:
- No linter detected in build system
- Code follows general C++ best practices but relies on manual review
## Import Organization
- Headers guard against double inclusion
- No namespace pollution (all code is in global namespace or uses classes to organize)
- Example from `ProffieOS.ino`:
- No path aliases detected (no using namespace, no import aliases)
- Relative includes used throughout: `#include "common/color.h"`, `#include "button_base.h"`
## Error Handling
- Used for debug-only assertions: `PROFFIEOS_ASSERT(X)` defined in `common/common.h`
- Disabled in release builds (`#ifndef ENABLE_DEBUG`)
- Strategy: Check condition, enable interrupts if disabled, log to STDERR, infinite loop (safe for embedded)
- Methods return `bool` to indicate success/failure
- Some methods return `nullptr` or null objects to indicate failure
- No exception handling used (typical for embedded/Arduino)
- Explicit pointer checks: `if (ptr)`, `if (!ptr)`, `if (ptr == nullptr)`
- Example: `if (tmp) free(tmp);` in `current_preset.h`
## Logging
- `STDOUT` for normal output: `STDOUT.print()`, `STDOUT.println()`, `STDOUT << value`
- `STDERR` for error output: `STDERR << "message"`
- Operator overloading via `operator<<` for fluent logging
- `common/stdout.h`: Main logging infrastructure with `ConsoleHelper` class
- `common/monitoring.h`: Monitoring/debug output control
- Battery/status output uses STDOUT: `STDOUT.print("Battery voltage: ");`
- Configuration output uses STDOUT: `STDOUT << variable_name << " " << max_arg << "\n"`
- No structured log levels (no DEBUG, INFO, WARN, ERROR explicitly)
- Logging based on code path: status updates, configuration, debug info when enabled
- Monitoring can filter output: `monitor.IsMonitoring(Monitoring::MonitorSerial)`
## Comments
- Configuration parameters: extensive comments above `#define` blocks
- Algorithms: comments above complex calculation code
- Embedded constraints: notes about memory/performance
- TODO/FIXME: inline notes for future work
- Test helper explanations: comments before test functions
- Not used in this codebase
- Method purposes documented via inline comments and class-level documentation
- Example: Block comment above class explaining purpose in `buttons/button.h`
## Function Design
- Methods generally kept short (10-30 lines)
- Complex state machines use helper methods: `Send()`, `SendClick()` in `button_base.h`
- Inline state machine expansion via `STATE_MACHINE_BEGIN()` / `YIELD()` macros
- Methods prefer const references for complex objects: `Color8 mix(const Color8& other, int x) const`
- Simple types passed by value: `int, float, uint8_t`
- Pointers used sparingly, typically for optional parameters
- Example constructor: `Button(enum BUTTON button, int pin, const char* name)`
- Boolean for success/failure: `bool Load()`, `bool DebouncedRead()`
- Value types for queries: `int fixed()`, `float battery()`
- Void for state-changing operations: `void Loop() override`
- Const return for immutability: `const char* name() override`
## Module Design
- Headers define public interfaces (classes, functions, macros)
- Implementation files (.cpp) contain standalone code (tests, main functions)
- No namespace wrapping (all in global namespace)
- Not used; each module has its own header
- Clear dependency direction: `common/` used by all, `blades/` used by core logic
- Circular dependencies avoided via forward declarations
- Configuration-driven composition: `ProffieOS.ino` pulls all necessary pieces via CONFIG_FILE
- Each subsystem (blades, buttons, styles, sound, etc.) is self-contained
- Headers in subsystem directory, tests in same directory as `tests.cpp`
- Example: `blades/blade_base.h`, `blades/simple_blade.h`, `blades/tests.cpp`
## Memory Management
- Preferred for small structures and local variables
- Examples: `Color8`, `Angle`, local state machines in `ButtonBase`
- Embedded constraint: RAM is limited (STM32L4, Teensy)
- Used for large collections and dynamic objects: `BladeStyle* new BladeStyle()` in `StyleFactoryImpl`
- Example in `style_blade.h`:
- Typically not deleted (lifetime tied to saber initialization)
- No delete calls detected in most code (rely on process termination)
- Custom reference-counted pointer using doubly-linked list (no ref count overhead)
- Located in `common/linked_ptr.h`
- Used for string management: prevents memory leaks without atomic reference counts
- Provides RAII semantics: destructor handles cleanup
- Placement new used for embedded union types: `new (&sf_file_) SerialFlashFile;`
- No custom allocator macros detected
- No `std::vector` in main code (used only in POSIX tests)
- Static arrays preferred: `uint8_t data[MAX_SIZE]`
- Pre-allocated buffers checked for overflow: `if (bufsize_ > 1) { ... }`
## Inlining and Performance
- `__attribute__((always_inline))` used for performance-critical methods
- Constexpr used for compile-time evaluation
- Careful use of `[[no_unique_address]]` for zero-cost abstractions (C++20)
- Macro wrapper: `#define PONUA [[no_unique_address]]` with fallback for older compilers
## State Machine Pattern
- `STATE_MACHINE_BEGIN()` initializes state tracking
- `YIELD()` pauses execution and returns control to scheduler
- Variables persist across yields (saved in class members)
- No external state machine library used
<!-- GSD:conventions-end -->

<!-- GSD:architecture-start source:ARCHITECTURE.md -->
## Architecture

## Pattern Overview
- Distributed event system via SaberBase class and linked-list of listeners
- Hardware abstraction through BladeBase interface with platform-specific implementations
- Template-based style composition allowing styles to be nested and combined
- Preset system that decouples blade configuration from style definitions
- Looper pattern for managing per-frame updates across all components
- Configuration via C++ template metaprogramming in config files
## Layers
- Purpose: Encapsulate LED hardware control (pin driving, WS2811 protocol, PWM, etc.)
- Location: `blades/`
- Contains: `BladeBase` abstract interface, concrete implementations (`WS2811BladePtr`, `SimpleBladePtr`, `FastLEDBladePtr`, etc.), LED interface adapters
- Depends on: Arduino hardware API, DMA drivers (platform-specific)
- Used by: Blade style system for setting LED colors
- Purpose: Calculate blade colors and patterns per LED per frame
- Location: `styles/`
- Contains: Style templates (RGB colors, transitions, effects), base `BladeStyle` class, composition helpers
- Depends on: Color representation (`color.h`), transitions system
- Used by: Blade instances to render visual effects
- Purpose: Distribute events (clash, ignition, motion, etc.) to all registered listeners
- Location: `common/saber_base.h`, `props/`
- Contains: `SaberBase` static class with event dispatcher, effect definitions, linked-list of listeners
- Depends on: Looper pattern for per-frame delivery
- Used by: Sound system, blade styles, prop-specific handlers
- Purpose: Load and manage font/sound/style combinations from files or ROM
- Location: `common/preset.h`, `common/current_preset.h`
- Contains: `Preset` struct (font, track, StyleAllocator per blade), `CurrentPreset` class (reads from disk/ROM)
- Depends on: Style allocators, file reader system
- Used by: PropBase to load user configurations
- Purpose: Detect button clicks, motion, clashes and convert to SaberBase events
- Location: `props/prop_base.h` and subclasses (`saber.h`, `blaster.h`, etc.)
- Contains: `PropBase` class (button handling, clash detection), prop-specific extensions
- Depends on: Motion detection, button input, SaberBase for event distribution
- Used by: Main event loop to handle user interaction
- Purpose: Play and mix sound effects triggered by SaberBase events
- Location: `sound/`
- Contains: `HybridFont` (sound library loader), DAC/mixer infrastructure, audio streams
- Depends on: SD card file system, effect system
- Used by: SaberBase event listeners for audio feedback
- Purpose: Initialize hardware, manage frame timing
- Location: `ProffieOS.ino`
- Contains: `setup()`, `loop()`, Looper invocation, SD card initialization
- Depends on: All subsystems above
- Used by: Arduino runtime
## Data Flow
- `SaberBase` static members: `is_on_`, `lockup_`, `color_change_mode_`, `sound_length`, `clash_strength_`
- `CurrentPreset` instance: current font/track/style/name
- PropBase instance: button state, pending clash, muted volume
- Per-BladeBase: current style pointer, color buffer
## Key Abstractions
- Purpose: Abstract interface for anything that displays color (main blade, accent LEDs, button lights, etc.)
- Examples: `WS2811BladePtr<n>`, `SimpleBladePtr<...>`, `FastLEDBladePtr<...>`, `SubBladePtr<...>`
- Pattern: Virtual interface with concrete hardware-specific implementations; styles call `set(led, color)` on blade
- Purpose: Abstract interface for color calculation algorithms
- Examples: `Rgb<R,G,B>`, `InOutHelper<...>`, `Fire<...>`, `Clash<...>`
- Pattern: Template-based composition; `run(blade)` for per-frame updates, `getColor(led)` for per-LED color
- Purpose: Static event dispatcher for lightsaber-specific events
- Examples: Listeners include HybridFont (sound), per-blade handlers, motion processor
- Pattern: Linked-list of virtual listeners; called via static `SaberBase::Do*()` methods
- Purpose: Factory that converts runtime style strings to compiled BladeStyle instances
- Examples: `style_allocator1` in each `Preset` struct
- Pattern: Macro-generated per-blade; `make()` returns `BladeStyle*` allocated on heap
- Purpose: Hook for per-frame callbacks during main event loop
- Examples: PropBase (input), HybridFont (audio), WS2811BladePtr (DMA management)
- Pattern: Linked-list of virtual listeners with `Setup()` and `Loop()` methods
- Purpose: Base for input handling and event generation
- Examples: `Saber` (button detection), `Blaster` (blaster mode), `DualProp` (dual-blade)
- Pattern: Virtual methods for `On()`, `Off()`, `Clash()`, button handling; calls SaberBase events
## Entry Points
## Error Handling
- `PROFFIEOS_ASSERT()` for debug-time checks; may call `while(true)` to freeze on errors
- `ProffieOSErrors::sd_card_not_found()`, `font_directory_not_found()` broadcast effects via `SaberBase::DoEffect()`
- Invalid style strings detected at load time via `CurrentPreset::IsValidStyleString()` with validation warnings to stdout
- Blade detection failure results in error effect and boot message
## Cross-Cutting Concerns
- `CONFIG_TOP` phase: Define NUM_BLADES, NUM_BUTTONS, enable flags
- `CONFIG_PRESETS` phase: Load ROM preset array and blade configuration
- `CONFIG_PROP` phase: Instantiate prop object
- `CONFIG_BOTTOM` phase: Final config-dependent setup
<!-- GSD:architecture-end -->

<!-- GSD:workflow-start source:GSD defaults -->
## Git Rules

**Never commit `.planning/` to git.** The `.planning/` directory is gitignored and must stay that way. It contains planning artifacts (STATE.md, ROADMAP.md, REQUIREMENTS.md, SUMMARYs) that are local-only. Do not use `git add --force` or `git add -f` on any `.planning/` file. If a gsd-tools commit command tries to include `.planning/` files in a code branch, skip those files.

## GSD Workflow Enforcement

Before using Edit, Write, or other file-changing tools, start work through a GSD command so planning artifacts and execution context stay in sync.

Use these entry points:
- `/gsd:quick` for small fixes, doc updates, and ad-hoc tasks
- `/gsd:debug` for investigation and bug fixing
- `/gsd:execute-phase` for planned phase work

Do not make direct repo edits outside a GSD workflow unless the user explicitly asks to bypass it.
<!-- GSD:workflow-end -->



<!-- GSD:profile-start -->
## Developer Profile

> Profile not yet configured. Run `/gsd:profile-user` to generate your developer profile.
> This section is managed by `generate-claude-profile` -- do not edit manually.
<!-- GSD:profile-end -->
