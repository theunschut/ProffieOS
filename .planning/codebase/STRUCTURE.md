# ProffieOS Codebase Structure

## Top-Level Directory Layout

```
ProffieOS/
├── ProffieOS.ino          # Arduino sketch entry point — the compilation root
├── Makefile               # Build targets for testing and utilities
├── README.md
├── PROJECT.md
├── LICENCE.txt
│
├── config/                # User configuration files (one per saber build)
├── common/                # Core infrastructure: base classes, utilities, OS primitives
├── blades/                # Blade driver implementations (WS2811, PWM, SPI, SubBlade, etc.)
├── styles/                # Blade style templates (color, effect, layer, transition wrappers)
├── functions/             # Numeric function templates used inside styles
├── transitions/           # Transition templates (fade, wipe, blink, etc.)
├── props/                 # Prop (button-mapping / interaction) implementations
├── sound/                 # Audio subsystem: DAC, mixing, font handling, effects
├── motion/                # IMU/gyro drivers (LSM6DS3H, MPU6050, FXOS8700, etc.)
├── buttons/               # Button and input drivers
├── modes/                 # Edit-mode menu system
├── display/               # SSD1306 / SPI display drivers
├── ir/                    # Infrared receiver/transmitter drivers
├── mtp/                   # MTP (Media Transfer Protocol) SD card access over USB
├── scripts/               # Test scripts and clash-recorder tools
├── doc/                   # Hardware diagrams and pinout images
├── pqoi/                  # PQOI image format encoder/decoder (for display frames)
├── pov_tools/             # POV (persistence-of-vision) data file utilities
├── fontconvert/           # Bitmap font conversion utilities for the display
└── videotoblc/            # Video-to-BLC frame converter for file-based blade styles
```

---

## Key File Locations

### Entry Point

| File | Role |
|------|------|
| `ProffieOS.ino` | Arduino sketch. Defines `CONFIG_FILE`, includes the chosen config, conditionally includes all subsystems, instantiates `prop`, and provides `setup()` / `loop()`. Nothing else should be changed here for typical use. |

### Configuration Files

All user configs live in `config/`. A config is a single `.h` file that is included multiple times with different `#define` guards active:

| Guard | What goes here |
|-------|---------------|
| `CONFIG_TOP` | `#include` the board header, set `NUM_BLADES`, `NUM_BUTTONS`, `VOLUME`, feature flags (`ENABLE_AUDIO`, `ENABLE_MOTION`, …) |
| `CONFIG_PROP` | `#include` the desired prop file, e.g. `"../props/saber_fett263_buttons.h"` |
| `CONFIG_STYLES` | Optional per-preset style declarations pulled out of `CONFIG_PRESETS` |
| `CONFIG_PRESETS` | Declare `Preset presets[]` and `BladeConfig blades[]` arrays |
| `CONFIG_BUTTONS` | Optional: override button wiring with `Button<>` / `TouchButton<>` instantiations |
| `CONFIG_BOTTOM` | Optional: last-resort overrides after all other includes |

Key example configs:

| File | Notes |
|------|-------|
| `config/proffieboard_v3_config.h` | Board-level pin map and peripheral assignments for V3 |
| `config/proffieboard_v2_config.h` | V2 pin map |
| `config/default_proffieboard_config.h` | Minimal two-button WS2811 + RGB star reference config |
| `config/OS6_config_example.h` | Full OS6 example with Fett263 prop and Edit Mode styles |
| `config/torro_config.h` | Per-user custom config (referenced in project memory) |

### Board/Platform Headers

Included inside `CONFIG_TOP` to set pin enums and hardware defines:

```
config/proffieboard_v3_config.h   — Proffieboard V3 (STM32L4)
config/proffieboard_v2_config.h   — Proffieboard V2
config/proffieboard_v1_config.h   — Proffieboard V1
config/teensy_audio_board_rev_d.h — Teensy 4.x + Audio Shield
```

### Core Infrastructure (`common/`)

| File | Role |
|------|------|
| `common/common.h` | Fundamental macros (`NELEM`, `PONUA`, `PROFFIEOS_ASSERT`) and C++ helpers |
| `common/saber_base.h` | `SaberBase` — event bus. Defines all `EffectType` values (`EFFECT_CLASH`, `EFFECT_BLAST`, …) and `DoEffect()` dispatch |
| `common/blade_config.h` | `BladeConfig` struct (ohm id, blade pointers, preset array) and `current_config` |
| `common/preset.h` | `Preset` struct (font path, track path, per-blade `StyleAllocator`) |
| `common/looper.h` | `Looper` base class — registers objects to receive `Loop()` and `Setup()` calls |
| `common/command_parser.h` | `CommandParser` — serial command dispatch |
| `common/config_file.h` | `ConfigFile` — reads key=value `.ini` files from SD (font config, global state) |
| `common/lsfs.h` | Filesystem abstraction layer (`LSFS::File`, `LSFS::Dir`, path helpers) |
| `common/sd_card.h` | SD card mount/unmount helpers |
| `common/color.h` | `Color8`, `Color16`, `OverDriveColor` types and blend math |
| `common/onceperblade.h` | `ONCEPERBLADE(F)` macro — expands `F(1) F(2) … F(N)` for `NUM_BLADES` |
| `common/state_machine.h` | `StateMachine` helper (coroutine-style `SLEEP`, `STATE_MACHINE_BEGIN/END`) |
| `common/current_preset.h` | `CurrentPreset` — runtime-mutable preset storage for Edit Mode |
| `common/battery_monitor.h` | Battery voltage monitor |
| `common/booster.h` | 5 V boost converter control |
| `common/resources.h` | First include in `ProffieOS.ino`; pulls in platform headers and common.h |

### Blade Drivers (`blades/`)

| File | Role |
|------|------|
| `blades/blade_base.h` | `BladeBase` abstract interface (`set()`, `num_leds()`, `is_on()`, …) |
| `blades/abstract_blade.h` | `AbstractBlade : BladeBase, SaberBase` — common `SaberBase` subscription logic |
| `blades/ws2811_blade.h` | `WS2811BladePtr<>` / `WS281XBladePtr<>` — neopixel strip driver; platform-selects STM32/Teensy/ESP32 backend |
| `blades/simple_blade.h` | `SimpleBladePtr<>` — up to 4 single-LED (PWM) channels |
| `blades/sub_blade.h` | `SubBlade()`, `SubBladeReverse()`, `SubBladeWithStride()` — virtual slices of a parent blade |
| `blades/fastled_blade.h` | FASTLed-based blade |
| `blades/saviblade.h` | Savi-style blade driver |
| `blades/dim_blade.h` | `DimBlade()` — brightness wrapper |
| `blades/blade_wrapper.h` | `BladeWrapper` passthrough base |
| `blades/blade_id.h` | Blade identification by resistor (ohm measurement) |
| `blades/drive_logic.h` | LED power-pin drive logic |
| `blades/stm32l4_ws2811.h` | STM32L4 DMA WS2811 backend |
| `blades/esp32_ws2811.h` | ESP32 RMT WS2811 backend |
| `blades/teensy4_ws2811.h` | Teensy 4 WS2811 backend |

### Blade Style System (`styles/`)

| File | Role |
|------|------|
| `styles/blade_style.h` | `BladeStyle` abstract base; `StyleFactory`; `LayerRunResult` / `FunctionRunResult` enums |
| `styles/style_ptr.h` | `StylePtr<STYLE>` — wraps a template style in a `BladeStyle`; `StyleBase`; `StyleHelper` |
| `styles/layers.h` | `Layers<BASE, L1, L2, …>` — Photoshop-style layer compositor |
| `styles/colors.h` | Named color constants (`RED`, `GREEN`, `CYAN`, …) |
| `styles/rgb.h` / `rgb_arg.h` | `Rgb<R,G,B>`, `RgbArg<ARG,DEFAULT>` — compile-time and argument-driven colors |
| `styles/legacy_styles.h` | `StyleNormalPtr<>`, `StyleFirePtr<>`, `EASYBLADE()` convenience wrappers |
| `styles/inout_helper.h` | `InOutHelper<>` / `InOutSparkTip<>` — ignition/retraction wrappers |
| `styles/transition_effect.h` | `TransitionEffectL<TR, EFFECT>` — plays a transition on a specific effect |
| `styles/lockup.h` | `LockupTrL<>` — lockup/drag/melt layer |
| `styles/blast.h` | Blast effect layer |
| `styles/clash.h` | Clash effect layer |
| `styles/fire.h` | Fire / flame effect |
| `styles/edit_mode.h` | Edit-Mode style argument injection |
| `styles/style_parser.h` | Runtime string-to-style parser (used by Edit Mode and SD style loading) |
| `styles/file.h` | `FromFileStyle` — drives blade colors from a `.blc` binary frame file |

### Numeric Functions (`functions/`)

Header-only templates that return a per-LED integer (0–32768) used as inputs to style layers.

Notable files: `int.h`, `int_arg.h`, `sin.h`, `scale.h`, `clamp.h`, `sound_level.h`, `swing_speed.h`, `clash_impact.h`, `time_since_effect.h`, `battery_level.h`, `blade_angle.h`, `variation.h`.

### Transitions (`transitions/`)

Header-only templates describing how one color state transitions to another.

Notable files: `fade.h`, `wipe.h`, `center_wipe.h`, `instant.h`, `blink.h`, `delay.h`, `wave.h`, `random.h`, `concat.h`, `loop.h`, `select.h`, `sequence.h`.

### Props (`props/`)

| File | Role |
|------|------|
| `props/prop_base.h` | `PropBase : CommandParser, Looper, SaberBase, ModeInterface` — base class for all props; global/preset state save files |
| `props/saber.h` | Default `Saber` prop (two-button saber logic) |
| `props/saber_fett263_buttons.h` | Fett263 extended saber prop; sets `PROP_TYPE SaberFett263Buttons` |
| `props/saber_BC_buttons.h` | BC-Buttons prop |
| `props/saber_sa22c_buttons.h` | SA22C prop |
| `props/saber_shtok_buttons.h` | Shtok prop |
| `props/saber_caiwyn_buttons.h` | Caiwyn prop |
| `props/saber_sabersense_buttons.h` | SaberSense prop |
| `props/blaster.h` | Blaster prop |
| `props/dual_prop.h` | `DualProp<A,B>` — combines two props via multiple inheritance |
| `props/audiofx.h` | AudioFX sound board prop |
| `props/detonator.h` | Detonator prop |
| `props/hev.h` | HEV suit prop |

### Sound Subsystem (`sound/`)

| File | Role |
|------|------|
| `sound/sound.h` | Top-level sound include; coordinates all audio components |
| `sound/hybrid_font.h` | `HybridFont` — SaberBase listener that triggers sound effects; reads `config.ini` via `FontConfigFile` |
| `sound/effect.h` | `Effect` — scans SD for numbered sound files (e.g. `clsh01.wav`) and picks one at random |
| `sound/dac.h` | DAC driver (STM32 / Teensy) |
| `sound/dac_esp32.h` | DAC driver (ESP32 / I2S) |
| `sound/dynamic_mixer.h` | `AudioDynamicMixer` — sums audio streams with auto gain |
| `sound/buffered_wav_player.h` | SD WAV file streaming player |
| `sound/playwav.h` | `PlayWav` helpers |
| `sound/smooth_swing_v2.h` | SmoothSwing V2 engine |
| `sound/amplifier.h` | Audio amplifier enable/disable |
| `sound/sound_library.h` | `SPEC` macro and sound library integration |

### Motion (`motion/`)

| File | Role |
|------|------|
| `motion/lsm6ds3h.h` | LSM6DS3H IMU driver (default for Proffieboard V2/V3) |
| `motion/mpu6050.h` | MPU-6050 IMU driver |
| `motion/fxos8700.h` | FXOS8700 accelerometer/magnetometer |
| `motion/fxas21002.h` | FXAS21002 gyroscope |
| `motion/motion_util.h` | Shared gyro/accel filtering utilities |

### Buttons (`buttons/`)

| File | Role |
|------|------|
| `buttons/button_base.h` | `ButtonBase` — abstract button |
| `buttons/button.h` | `Button<PIN>` — digital button with debounce |
| `buttons/debounced_button.h` | Debounce logic |
| `buttons/touchbutton.h` | Capacitive touch button |
| `buttons/latching_button.h` | Latching (toggle) button |
| `buttons/stm32l4_touchbutton.h` | STM32L4 hardware touch button |
| `buttons/rotary.h` | Rotary encoder input |
| `buttons/pots.h` | Potentiometer input |

### Modes / Edit Mode (`modes/`)

| File | Role |
|------|------|
| `modes/mode.h` | `ModeInterface` base; mode-push/pop stack |
| `modes/menu_base.h` | Menu navigation base |
| `modes/color_menues.h` | Color picker menus |
| `modes/preset_modes.h` | Preset selection modes |
| `modes/settings_menues.h` | Settings menus |
| `modes/default_spec.h` | `DefaultMenuSpec` — standard OS6 menu layout |
| `modes/style_argument_helpers.h` | Helpers for editing `IntArg` / `RgbArg` style parameters |
| `modes/top_menu_wrapper.h` | Top-level menu wrapper prop integration |

---

## Naming Conventions

### Files

- All source files use `.h` (header-only, template-heavy). There are no `.cpp` compilation units in the main firmware — everything is `#include`d into `ProffieOS.ino`.
- File names are `snake_case` and describe either a component (`ws2811_blade.h`), an effect (`clash.h`, `lockup.h`), or a function (`swing_speed.h`).
- Test files are `tests.cpp` or `*_test.cpp` and live alongside the headers they test.
- Config files are named `<board_or_variant>_config.h`.
- Prop files follow `saber_<author>_buttons.h` for community props.

### Classes

- `PascalCase` throughout.
- Blade drivers: `WS2811Blade`, `SimpleBlade`, `SubBlade` — instantiated via `*Ptr<>` factory functions.
- Style templates: `PascalCase` template parameters, e.g. `Layers<BASE, L1, L2>`, `LockupTrL<COLOR, BEGIN_TR, END_TR, LOCKUP_TYPE>`.
- Props: `Saber`, `SaberFett263Buttons`, `Blaster`, `DualProp<A,B>`.
- Event bus / infrastructure: `SaberBase`, `BladeBase`, `PropBase`, `Looper`, `CommandParser`.
- Style layer variants append `L` suffix: `LockupTrL`, `TransitionEffectL`, `AlphaL`, `BrownNoiseFlickerL`.
- Transition variants append `X` suffix for versions that take runtime-computed durations: `TrFadeX`, `TrWipeX`, `InOutHelperX`.

### Macros

- Feature flags: `ENABLE_AUDIO`, `ENABLE_MOTION`, `ENABLE_WS2811`, `ENABLE_SD` (positive opt-in) or `DISABLE_*` (opt-out).
- Save flags: `SAVE_STATE`, `SAVE_VOLUME`, `SAVE_PRESET`, `SAVE_COLOR_CHANGE`, `SAVE_BLADE_DIMMING`.
- Config guards: `CONFIG_TOP`, `CONFIG_PRESETS`, `CONFIG_PROP`, `CONFIG_BUTTONS`, `CONFIG_BOTTOM`, `CONFIG_STYLES`.
- Per-blade iteration: `ONCEPERBLADE(MACRO)` — expands `MACRO(1)` through `MACRO(NUM_BLADES)`.
- Named colors: `ALL_CAPS` (`RED`, `GREEN`, `WHITE`, `BLACK`, `CYAN`, …).
- Effect types: `EFFECT_CLASH`, `EFFECT_BLAST`, `EFFECT_IGNITION`, … — values of `EffectType` enum, defined via `DEFINE_ALL_EFFECTS()` X-macro in `saber_base.h`.
- Style arguments: `BASE_COLOR_ARG`, `CLASH_COLOR_ARG`, `LOCKUP_POSITION_ARG`, `STYLE_OPTION_ARG`, … — integer indices into the preset's style argument array.
- Prop type: `PROP_TYPE` — set by the chosen prop header; consumed in `ProffieOS.ino` to instantiate `prop`.
- `CONFIGARRAY(X)` — expands to `X, NELEM(X)` for initializing `BladeConfig::presets` / `num_presets`.
- Include guards: `#ifndef DIRECTORY_FILENAME_H` (e.g. `STYLES_BLADE_STYLE_H`, `COMMON_SABER_BASE_H`).

### Style Template Conventions

- Types that produce a **color** are plain `PascalCase`: `Rgb<>`, `Layers<>`, `Mix<>`, `Gradient<>`.
- Types that produce a **layer** (semi-transparent, meant for `Layers<>`) append `L`: `AlphaL<>`, `LockupTrL<>`, `TransitionEffectL<>`.
- Types that produce a **function** (integer 0–32768 per LED) are plain `PascalCase`: `Sin<>`, `Scale<>`, `ClashImpactF<>`, `SwingSpeed<>`.
- Types that produce a **transition** are prefixed `Tr`: `TrFade<>`, `TrWipe<>`, `TrConcat<>`, `TrInstant`, `TrWaveX<>`.

---

## Where to Add New Files

### New user config (new saber build)
Place in `config/` as `<descriptive_name>_config.h`. Include a board header in `CONFIG_TOP`, prop in `CONFIG_PROP`, presets/blades arrays in `CONFIG_PRESETS`. Set `CONFIG_FILE` in `ProffieOS.ino`.

### New prop (button mapping / interaction logic)
Place in `props/` as `saber_<name>_buttons.h` (or `<prop_type>.h` for non-saber props). Subclass `PropBase` (or an existing prop) with `PROP_INHERIT_PREFIX`. Add `#undef PROP_TYPE` / `#define PROP_TYPE YourClass` at the bottom so the config's `CONFIG_PROP` block can include it.

### New blade style or effect layer
Place in `styles/`. Use the `L`-suffix convention if the template produces a layer (has an alpha channel). Header-only; no registration needed — styles are referenced directly in `StylePtr<>` or `Layers<>` instantiations inside config presets.

### New numeric function (used inside styles)
Place in `functions/`. Header-only template returning an integer value per LED. Follow the existing file-per-function pattern.

### New transition
Place in `transitions/`. Header-only template prefixed `Tr`.

### New blade driver
Place in `blades/`. Subclass `AbstractBlade` (which inherits both `BladeBase` and `SaberBase`). Provide a `*BladePtr<>()` factory function. Platform-specific backends (DMA, RMT, etc.) go in the same directory with a platform suffix (`stm32l4_`, `esp32_`, `teensy4_`).

### New IMU / motion sensor
Place in `motion/`. Subclass or follow the pattern of existing drivers (provide `Setup()`, `Loop()`, populate `accel` / `gyro` globals). Enable via a `GYRO_CLASS` define in the board config header.

### New sound subsystem component
Place in `sound/`. Subclass `AudioStream` or `SaberBase` as appropriate. Register with `dynamic_mixer` or the `SaberBase` linked list.

### New serial/button input
Place in `buttons/`. Subclass `ButtonBase`. Instantiate in the config's `CONFIG_BUTTONS` block or in the prop constructor.

### New Edit Mode menu
Place in `modes/`. Implement `ModeInterface`. Wire it into the prop's button handler or into `top_menu_wrapper.h`.

### Platform-specific utilities
If a file is only needed on one MCU family, name it with a platform prefix (`stm32l4_`, `esp32_`, `teensy4_`) and `#ifdef`-guard the include site in the relevant driver header.
