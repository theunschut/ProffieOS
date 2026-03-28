# ProffieOS Testing

## Overview

ProffieOS has a custom, lightweight testing framework built entirely without a third-party test library. All tests are plain C++ programs compiled with `g++` and run as POSIX executables on Linux/macOS. There is no test runner binary or CI configuration checked into the repo; the top-level `Makefile` wires everything together.

The test suite has two layers:

1. **POSIX unit tests** — C++ programs compiled and run on the host machine (the `posixtests` target). These test pure logic by replacing all hardware dependencies with inline stubs.
2. **Firmware compile tests** — The `subtest` target extracts a copy of the source tree, injects a specific config header, and invokes `arduino-cli compile` against real board targets. These catch compilation failures and ABI issues on the actual MCU toolchain; they do not run any code.

Running `make test` (in the repo root) executes both layers.

---

## File Inventory

| File | Lines | What it tests |
|---|---|---|
| `common/tests.cpp` | 909 | Preset INI read/write, color maths, byte order, HSV/HSL conversion, extrapolator, color compositing (`RGBA`/`MixColors`), `ConfigFile`, `CommandParser`, `CyclInt`, `EffectLocation`, range/stripe intersection |
| `common/test2.cpp` | 146 | `CurrentPreset` with a different `INSTALL_TIME_EXTRA` constant; exercises the `.tmp` → `.ini` promotion path |
| `styles/tests.cpp` | 970 | `StyleParser` argument get/set/reset/copy, `Gradient`, `Mix`, `InOutHelper`/`InOutTr`, Cylon, style rendering via `MockBlade`, `SmoothStep`, `GetArgMax`, effect color spot-checks |
| `blades/tests.cpp` | 115 | LED `ColorSelector` drive logic |
| `buttons/tests.cpp` | 564 | Button event state machine — short click, double/triple/quad click, long hold, two-button combos |
| `sound/tests.cpp` | 385 | `Effect` file-scanning (flat files, numbered variants, subdirs, `.DS_Store` filtering), `PlayWav` at 44.1 kHz / 22.05 kHz (resampled) / 48 kHz (unsupported, expected failure) |
| `display/tests.cpp` | 174 | `MonoFrame` landscape/portrait pixel buffer conversion |
| `sound/filter_test.cpp` | 117 | WAV I/O tool; applies audio filter to stdin and writes stdout. Not a pass/fail test — a manual audio-inspection utility |
| `sound/talkie_test.cpp` | 115 | Renders a Talkie speech token to a WAV file (used by `sound/Makefile` to produce `zero.wav` which the main sound test links against) |
| `common/sd_test.h` | 153 | On-device SD card throughput profiler (`SDTestHelper`). Not a pass/fail unit test — an instrumented utility that measures read latency histograms when compiled into firmware |

---

## Build and Run

Each subdirectory that has a test has its own `Makefile` with a `test` target:

```
# Run all POSIX tests (six test binaries)
make posixtests

# Run a single subsystem
cd styles && make test
cd common && make test
cd blades && make test
cd sound  && make test
cd buttons && make test
cd display && make test
```

Each `Makefile` compiles with:
```
g++ -O -g -std=c++11   (or c++14 for styles)
```

The `styles/` test additionally accepts `clang++` with `-fsanitize=address` as a commented-out alternative.

Firmware compile tests run via the top-level `make test1` through `make testD` targets. Each extracts the source tree, substitutes a config header, and invokes `arduino-cli compile --fqbn=<board>`.

---

## Test Framework Conventions

There is no third-party framework. Each test file defines its own assertion macros at the top:

| Macro | Semantics |
|---|---|
| `CHECK(X)` | Aborts with `fprintf(stderr, ...)` + `exit(1)` if false |
| `CHECK_EQ(X, Y)` | Equality; prints both values on failure |
| `CHECK_NE(X, Y)` | Inequality |
| `CHECK_LT / _LE / _GT / _GE` | Ordered comparisons |
| `CHECK_NEAR(X, Y, D)` | Floating-point equality within delta |
| `CHECK_NEAR_MSG(...)` | Same with extra diagnostic output |
| `CHECK_STREQ(X, Y)` | `strcmp`-based string equality, handles null |
| `CHECK_GLOB(PAT, Y)` | Custom glob match (used in sound tests) |
| `EXPECT_EQ / EXPECT_EQ_EVENT` | Used in button tests; same semantics |

Test functions are plain `void` functions called from `main()`. There is no auto-discovery; new test functions must be added to `main()` manually.

The buttons test uses a slightly more sophisticated harness: a `TestHelper` base class backed by `StateMachine` (a `YIELD()`-based coroutine) with `SLEEP(ms)` and `EXPECT_EVENT(button, event)` macros that advance simulated time and assert on the event stream.

---

## Mocking Strategy

Hardware dependencies are eliminated by source-level replacement before any ProffieOS header is included. Each test file establishes its own stub environment at the top, then includes the headers under test.

### Time

```cpp
uint64_t micros_ = 0;          // mutable global
uint32_t micros() { return micros_; }
uint32_t millis() { return micros_ / 1000; }
```

Tests advance time by assigning to `micros_` directly or by incrementing it in loops (`micros_ += 1000`).

### Filesystem (LSFS)

`common/lsfs.h` selects its implementation at compile time. In tests that define `PROFFIE_TEST`, the POSIX LSFS backend (`lsfs.h`) is compiled in, which calls real POSIX `open`/`read`/`stat`/`opendir` against the process working directory. Tests create temporary files and directories using `mkdir()` / `fopen()` / `fclose()` and clean up with `system("rm -rvf ...")`.

The `current_directory` directory-traversal macros are replaced with simple constants:
```cpp
#define current_directory "."
#define next_current_directory(dir) nullptr
```

### Hardware Peripherals

All hardware abstractions are replaced with empty or trivially returning stubs:

```cpp
#define noInterrupts() do{}while(0)
#define interrupts()   do{}while(0)
#define LOCK_SD(X)     do{}while(0)
#define SCOPED_PROFILER() do{}while(0)
void MountSDCard() {}
```

### Motion Sensor (Fusor)

`styles/tests.cpp` defines `MockFuse` that returns constant zero values for all motion readings (angles, swing speed, gyro), eliminating the IMU/fusion dependency:

```cpp
struct MockFuse {
  float angle1() { return angle1_; }
  float swing_speed() { return swing_speed_; }
  V3 gyro() { return V3(0.0); }
  // ...
};
MockFuse fusor;
```

`common/tests.cpp` includes the real `Fusor` class under `#ifdef FUSE_SPEED` with a note that fuse tests lacking speed() still need to be written.

### Audio Mixer

```cpp
struct MockDynamicMixer {
  int32_t last_sample() const { return 4093; }
  int32_t last_sum() const { return 16384; }
  int32_t audio_volume() const { return 100000; }
};
MockDynamicMixer dynamic_mixer;
```

### Blade / LED

`styles/tests.cpp` provides `MockBlade`, a complete `BladeBase` implementation that stores `Color16` values in a `std::vector<Color16>`. Tests call `style->run(&mock_blade)` and then inspect `mock_blade.colors[i]` directly.

```cpp
class MockBlade : public BladeBase {
  std::vector<Color16> colors;
  void set(int led, Color16 c) override { colors[led] = c; }
  int num_leds() const override { return colors.size(); }
  // ...
};
```

### Print / STDOUT

Each test file instantiates a `Print` object connected to `stdout`/`stderr` and assigns it to the `default_output` / `stdout_output` globals, then creates a `ConsoleHelper STDOUT` instance.

### Buttons

`buttons/tests.cpp` provides `TestButton`, a `ButtonBase` subclass whose `Read()` returns a mutable `pressed_` field that test coroutines toggle directly.

---

## What Is Tested

### common/tests.cpp
- `CurrentPreset`: load, save, next/prev wrap-around, unterminated-file rejection, wrong-install-time rejection, `.tmp` file promotion, `SaveAt` reordering
- Color arithmetic: byte order encode/invert, HSV rotation, HSL round-trip, `Color8`/`Color16` conversions
- `RGBA`/`RGBA_um`/`RGBA_um_nod` compositing and `MixColors` across a large value set
- `ConfigFile`: read/write INI round-trip with unknown-variable tolerance
- `CommandParser`: `RunCommandAndGetSingleLine`, `RunCommandAndFindNextSortedLine` (forward and reverse)
- `CyclInt` wrapping arithmetic
- `EffectLocation` blade-mask filtering
- `Range::intersect_with_stripes` geometry
- `Extrapolator<float>` slope and prediction

### common/test2.cpp
- Same `CurrentPreset` logic as above, but compiled with a different `INSTALL_TIME_EXTRA` to test the multi-install-time upgrade path

### styles/tests.cpp
- `StyleParser::GetArgument`, `SetArgument`, `ResetArguments`, `CopyArguments`, `MaxUsedArgument`
- `RgbArg` default and overridden color values via `testGetArg`
- `Gradient`, `Mix`, `Layers`, `SmoothStep`, `InOutHelper`, `InOutTr` rendering correctness
- `Cylon` balance/coverage checks at various time offsets
- Complex multi-layer styles (styles 1–6) for basic correctness (color channel, on/off state, melt)
- `GetArgMax` compile-time template introspection
- `TestCompileStyle` — compiles a large representative style tree to catch compile errors silently (no runtime assertions)

### blades/tests.cpp
- `ColorSelector` for various Cree LED sub-pixel types

### buttons/tests.cpp
- State machine event sequences for: single short click, long click, long held (with accept), double/triple/quad click, two-button simultaneous combos, press-release with event acceptance

### sound/tests.cpp
- `Effect::ScanCurrentDirectory`: flat files, numbered files (`hum1.wav`), subdirectories, `.DS_Store` / `._` file exclusion, `alt000/` alternative sets, nested sub-file directories, `files_found()` / `expected_files()` counts
- `PlayWav`: 44.1 kHz playback, 22.05 kHz upsampled playback (2× samples), 48 kHz rejection, recovery after failed open

### display/tests.cpp
- `MonoFrame` landscape → portrait pixel rearrangement for 64, 128, and 192 pixel widths with `uint16_t`, `uint32_t`, and `uint64_t` column types; verified against three deterministic fill patterns

---

## What Is Not Tested

The following subsystems have no automated tests:

- **Hardware drivers**: SPI, SDMMC, I2S, DMA, PWM, WS2811/WS2813 LED strip timing — all are hardware-only
- **ProffieBoard HAL layer**: STM32L4 / STM32L452 register manipulation, clock setup, DOSFS integration
- **ESP32 platform code**: `esp32/` layer, atomic ops, cycle counters — recently added, no tests
- **Props and sabers**: `props/` — no unit tests; prop behavior is only exercised by the firmware compile tests
- **Modes framework**: `modes/` — no tests
- **MTP (Media Transfer Protocol)**: `mtp/` — no tests
- **IR subsystem**: `ir/` — no tests
- **Motion fusion (Fusor) without speed()**: the `#ifdef FUSE_SPEED` block is excluded from normal runs; the TODO comment at line 549 of `common/tests.cpp` notes this gap explicitly
- **Style instantiation at runtime**: `StyleParser::ParseStyle` (string → `BladeStyle*`) is not directly tested; only argument manipulation helpers are tested
- **SD card I/O performance**: `SDTestHelper` in `common/sd_test.h` is a benchmark utility, not a pass/fail test
- **Audio pipeline**: mixing, buffering, volume ramping, DMA callback timing — not tested
- **Save/load of user configuration**: only preset INI files are tested; the broader save-dir / YAML / variant-save paths are not
- **Multi-blade coordination**: `NUM_BLADES` is pinned to small values (1, 3, or 13) in each test; blade-array interaction is not exercised

---

## Gaps Relevant to an SD Card Style Parser

The runtime SD card style parser feature requires a component that:
1. Reads a style description string from a file on the SD card at boot or preset change
2. Parses that string into a `BladeStyle*` (or a `StyleAllocator` equivalent) at runtime
3. Feeds the result into the blade's `SetStyle()` pipeline

The existing test infrastructure provides good foundations but leaves specific gaps:

### What exists and is usable

- **POSIX LSFS backend** (`common/lsfs.h` with `PROFFIE_TEST`) is already used by `common/tests.cpp` and `common/test2.cpp`. A new test can create temporary files, write style strings to them, and call parsing code without any additional mocking.
- **`MockBlade`** in `styles/tests.cpp` provides a working blade target for rendering the parsed style.
- **`StyleParser` argument tests** in `styles/tests.cpp` test the string manipulation layer (`GetArgument`, `SetArgument`) that any parser will call.
- **`CurrentPreset` + INI tests** in `common/tests.cpp` demonstrate the full create-write-read round-trip pattern that a style file reader should follow.
- **`micros_` time control** is well established; a style parser that depends on time can be exercised with deterministic time sequences.

### What is missing

- **`StyleParser::ParseStyle` is untested end-to-end.** The function that maps a style name string to a `BladeStyle*` object is not exercised by any existing test. There is no test that calls `ParseStyle("standard 0,65535,65535 ...")` and verifies the resulting blade output.
- **No round-trip test for SD-sourced style strings.** There is no test that writes a style string to a file, reads it back through the full file-reading stack, parses it, runs it on a `MockBlade`, and asserts on the rendered colors.
- **No error-path tests for malformed style strings.** Bad style names, truncated argument lists, unrecognised style names, and oversized strings are not tested anywhere.
- **No test for `CurrentPreset::current_style_` population from disk.** The INI tests in `common/tests.cpp` read back style fields as raw strings (`CHECK_STREQ(preset.current_style_[0].get(), "style0:1")`), but they do not pass those strings through `StyleParser` or verify that a usable `BladeStyle*` results.
- **No test for the `builtin` style allocator path.** `BuiltinPresetAllocator` in `styles/style_parser.h` is not tested; it depends on `current_config` which is already mockable in the styles test file.
- **No test for SD-file-not-found fallback behavior.** A runtime parser must handle missing files gracefully; no test exercises this path.
- **`sound/Makefile` runs `talkie_test` to produce `zero.wav`** — this pattern shows that audio file creation in tests works, but no analogous pattern exists for style files.

### Recommended test additions

A new `styles/sd_style_test.cpp` (or an extension of `styles/tests.cpp`) should:
1. Use the existing POSIX LSFS backend to write a temp file containing a style string.
2. Call `StyleParser::ParseStyle` (or whatever the runtime parser entry point is).
3. Run the result on a `MockBlade`.
4. Assert on specific rendered colors using `CHECK_COLOR`.
5. Repeat for: valid named style, valid `builtin N M` style, missing file, malformed string, empty string, and style name not in the registry.

A new `common/` test function (or addition to `common/tests.cpp`) should test the file-read portion in isolation: write a file with a `STYLE=` line, read it back through `CurrentPreset`, and verify the raw string is preserved correctly before passing it to any parser.
