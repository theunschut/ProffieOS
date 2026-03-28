# Architecture

**Analysis Date:** 2026-03-28

## Pattern Overview

**Overall:** Event-driven, component-based architecture with template-based style system

**Key Characteristics:**
- Distributed event system via SaberBase class and linked-list of listeners
- Hardware abstraction through BladeBase interface with platform-specific implementations
- Template-based style composition allowing styles to be nested and combined
- Preset system that decouples blade configuration from style definitions
- Looper pattern for managing per-frame updates across all components
- Configuration via C++ template metaprogramming in config files

## Layers

**Hardware Abstraction (Blades):**
- Purpose: Encapsulate LED hardware control (pin driving, WS2811 protocol, PWM, etc.)
- Location: `blades/`
- Contains: `BladeBase` abstract interface, concrete implementations (`WS2811BladePtr`, `SimpleBladePtr`, `FastLEDBladePtr`, etc.), LED interface adapters
- Depends on: Arduino hardware API, DMA drivers (platform-specific)
- Used by: Blade style system for setting LED colors

**Style System:**
- Purpose: Calculate blade colors and patterns per LED per frame
- Location: `styles/`
- Contains: Style templates (RGB colors, transitions, effects), base `BladeStyle` class, composition helpers
- Depends on: Color representation (`color.h`), transitions system
- Used by: Blade instances to render visual effects

**Effect and Event System:**
- Purpose: Distribute events (clash, ignition, motion, etc.) to all registered listeners
- Location: `common/saber_base.h`, `props/`
- Contains: `SaberBase` static class with event dispatcher, effect definitions, linked-list of listeners
- Depends on: Looper pattern for per-frame delivery
- Used by: Sound system, blade styles, prop-specific handlers

**Preset System:**
- Purpose: Load and manage font/sound/style combinations from files or ROM
- Location: `common/preset.h`, `common/current_preset.h`
- Contains: `Preset` struct (font, track, StyleAllocator per blade), `CurrentPreset` class (reads from disk/ROM)
- Depends on: Style allocators, file reader system
- Used by: PropBase to load user configurations

**Prop/Input Handler:**
- Purpose: Detect button clicks, motion, clashes and convert to SaberBase events
- Location: `props/prop_base.h` and subclasses (`saber.h`, `blaster.h`, etc.)
- Contains: `PropBase` class (button handling, clash detection), prop-specific extensions
- Depends on: Motion detection, button input, SaberBase for event distribution
- Used by: Main event loop to handle user interaction

**Sound System:**
- Purpose: Play and mix sound effects triggered by SaberBase events
- Location: `sound/`
- Contains: `HybridFont` (sound library loader), DAC/mixer infrastructure, audio streams
- Depends on: SD card file system, effect system
- Used by: SaberBase event listeners for audio feedback

**Boot and Main Loop:**
- Purpose: Initialize hardware, manage frame timing
- Location: `ProffieOS.ino`
- Contains: `setup()`, `loop()`, Looper invocation, SD card initialization
- Depends on: All subsystems above
- Used by: Arduino runtime

## Data Flow

**Blade Rendering (Per Frame):**

1. `loop()` calls `Looper::DoLoop()`
2. Looper iterates all registered `Looper` subclasses (BladeBase instances, sound system, etc.)
3. Each `BladeBase` calls `BladeStyle::run(blade)` to update blade state
4. Style's `run()` calls `blade->set(led, color)` for each LED
5. `BladeBase` implementation drives actual hardware (GPIO, DMA, SPI)

**Event Dispatch (Click/Motion):**

1. PropBase's `Loop()` detects input (button or motion sensor)
2. PropBase calls `SaberBase::DoEffect(effect_type, location)`
3. `SaberBase::DoEffect()` iterates linked-list of registered SaberBase subclasses
4. Each listener's `OnEffect()` method is called
5. Sound system (`HybridFont`) queues audio; Blade styles respond to effects

**Preset Loading:**

1. PropBase calls `CurrentPreset::Set(preset_num)` from ROM `Preset` array
2. `CurrentPreset` extracts font, track, and `StyleAllocator` strings
3. PropBase calls `style_allocator->make()` to instantiate `BladeStyle*`
4. Allocator compiles style string into concrete style class via template instantiation
5. `BladeBase::SetStyle()` swaps current style

**State Management:**

- `SaberBase` static members: `is_on_`, `lockup_`, `color_change_mode_`, `sound_length`, `clash_strength_`
- `CurrentPreset` instance: current font/track/style/name
- PropBase instance: button state, pending clash, muted volume
- Per-BladeBase: current style pointer, color buffer

## Key Abstractions

**BladeBase:**
- Purpose: Abstract interface for anything that displays color (main blade, accent LEDs, button lights, etc.)
- Examples: `WS2811BladePtr<n>`, `SimpleBladePtr<...>`, `FastLEDBladePtr<...>`, `SubBladePtr<...>`
- Pattern: Virtual interface with concrete hardware-specific implementations; styles call `set(led, color)` on blade

**BladeStyle:**
- Purpose: Abstract interface for color calculation algorithms
- Examples: `Rgb<R,G,B>`, `InOutHelper<...>`, `Fire<...>`, `Clash<...>`
- Pattern: Template-based composition; `run(blade)` for per-frame updates, `getColor(led)` for per-LED color

**SaberBase:**
- Purpose: Static event dispatcher for lightsaber-specific events
- Examples: Listeners include HybridFont (sound), per-blade handlers, motion processor
- Pattern: Linked-list of virtual listeners; called via static `SaberBase::Do*()` methods

**StyleAllocator:**
- Purpose: Factory that converts runtime style strings to compiled BladeStyle instances
- Examples: `style_allocator1` in each `Preset` struct
- Pattern: Macro-generated per-blade; `make()` returns `BladeStyle*` allocated on heap

**Looper:**
- Purpose: Hook for per-frame callbacks during main event loop
- Examples: PropBase (input), HybridFont (audio), WS2811BladePtr (DMA management)
- Pattern: Linked-list of virtual listeners with `Setup()` and `Loop()` methods

**PropBase:**
- Purpose: Base for input handling and event generation
- Examples: `Saber` (button detection), `Blaster` (blaster mode), `DualProp` (dual-blade)
- Pattern: Virtual methods for `On()`, `Off()`, `Clash()`, button handling; calls SaberBase events

## Entry Points

**Boot Sequence:**

1. Location: `ProffieOS.ino::setup()` (line 1601)
2. Triggers: Arduino startup
3. Responsibilities: Serial init, SD card mount, style string loading, `Looper::DoSetup()` calls all Setup() methods, `SaberBase::DoBoot()` event, blade detection
4. Key calls:
   - `LSFS::Begin()` - Mount SD card
   - `Looper::DoSetup()` - Initialize all registered loopers
   - `prop.FindBlade(true)` - Detect connected blades
   - `SaberBase::DoBoot()` - Broadcast boot event

**Main Event Loop:**

1. Location: `ProffieOS.ino::loop()` (line 1716)
2. Triggers: Arduino main loop (called repeatedly)
3. Responsibilities: Frame-by-frame updates for all components
4. Implementation: Calls `Looper::DoLoop()` which iterates registered Looper instances

**MTP Support:**

1. Location: Optional `mtpd.loop()` for USB file transfers
2. Triggers: When USB MTP endpoint is available

## Error Handling

**Strategy:** Assertion and error effect triggering

**Patterns:**
- `PROFFIEOS_ASSERT()` for debug-time checks; may call `while(true)` to freeze on errors
- `ProffieOSErrors::sd_card_not_found()`, `font_directory_not_found()` broadcast effects via `SaberBase::DoEffect()`
- Invalid style strings detected at load time via `CurrentPreset::IsValidStyleString()` with validation warnings to stdout
- Blade detection failure results in error effect and boot message

## Cross-Cutting Concerns

**Logging:** `STDOUT` and `STDERR` objects for debug output; filtered by config `ENABLE_DEBUG`

**Validation:** Style string validation at load time via `CurrentPreset::ValidateStyleString()` with optional strict checks

**Authentication:** Not applicable (embedded device with no multi-user model)

**Configuration:** Three-pass includes in `ProffieOS.ino`:
- `CONFIG_TOP` phase: Define NUM_BLADES, NUM_BUTTONS, enable flags
- `CONFIG_PRESETS` phase: Load ROM preset array and blade configuration
- `CONFIG_PROP` phase: Instantiate prop object
- `CONFIG_BOTTOM` phase: Final config-dependent setup

**State Persistence:** Optional `SavePresetStateFile` and `SaveGlobalStateFile` classes save/restore via SD card key-value format

---

*Architecture analysis: 2026-03-28*
